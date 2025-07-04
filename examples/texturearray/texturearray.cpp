/*
* Vulkan Example - Texture arrays and instanced rendering
* 
* This sample shows how to load and render a texture array. This is a single layered texture where each layer contains different m_vkImage data.
* The different layers are displayed on cubes using instancing, where each m_vulkanInstance selects a different layer from the texture
*
* Copyright (C) 2016-2023 Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include <ktx.h>
#include <ktxvulkan.h>

#define MAX_LAYERS 8

// Vertex layout for this example
struct Vertex {
	float pos[3];
	float uv[2];
};

class VulkanExample : public VulkanExampleBase
{
public:
	// Number of array layers in texture array
	// Also used as m_vulkanInstance count
	uint32_t layerCount{ 0 };
	vks::Texture textureArray;

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t m_indexCount{ 0 };

	// Values passed to the shader per drawn m_vulkanInstance
	struct alignas(16) PerInstanceData {
		// Model matrix
		glm::mat4 model;
		// Layer index from which this m_vulkanInstance will sample in the fragment shader
		float arrayIndex{ 0 };
	};

	struct UniformData {
		// Global matrices
		struct {
			glm::mat4 projection;
			glm::mat4 view;
		} matrices;
		// Separate data for each m_vulkanInstance
		PerInstanceData* instance{ nullptr };
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Texture arrays";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -7.5f));
		camera.setRotation(glm::vec3(-35.0f, 0.0f, 0.0f));
		camera.setPerspective(45.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyImageView(m_vkDevice, textureArray.view, nullptr);
			vkDestroyImage(m_vkDevice, textureArray.image, nullptr);
			vkDestroySampler(m_vkDevice, textureArray.sampler, nullptr);
			vkFreeMemory(m_vkDevice, textureArray.deviceMemory, nullptr);
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			vertexBuffer.destroy();
			indexBuffer.destroy();
			uniformBuffer.destroy();
			delete[] uniformData.instance;
		}
	}

	void loadTextureArray(std::string filename, VkFormat format)
	{
		ktxResult result;
		ktxTexture* ktxTexture;

#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		if (!asset) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
		}
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		ktx_uint8_t *textureData = new ktx_uint8_t[size];
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);
		result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		delete[] textureData;
#else
		if (!vks::tools::fileExists(filename)) {
			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
		}
		result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
#endif
		assert(result == KTX_SUCCESS);

		// Get m_vkPhysicalDeviceProperties required for using and upload texture data from the ktx texture object
		textureArray.width = ktxTexture->baseWidth;
		textureArray.height = ktxTexture->baseHeight;
		layerCount = ktxTexture->numLayers;
        assert(layerCount <= MAX_LAYERS);
		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create a host-visible staging buffer that contains the raw m_vkImage data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = ktxTextureSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(m_vkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get m_vkDeviceMemory requirements for the staging buffer (alignment, m_vkDeviceMemory type bits)
		vkGetBufferMemoryRequirements(m_vkDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get m_vkDeviceMemory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(m_vkDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(m_vkDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, ktxTextureData, ktxTextureSize);
		vkUnmapMemory(m_vkDevice, stagingMemory);

		// Setup buffer copy regions for array layers
		std::vector<VkBufferImageCopy> bufferCopyRegions;

		// To keep this simple, we will only load layers and no mip level
		for (uint32_t layer = 0; layer < layerCount; layer++)
		{
			// Calculate offset into staging buffer for the current array layer
			ktx_size_t offset;
			KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, 0, layer, 0, &offset);
			assert(ret == KTX_SUCCESS);
			// Setup a buffer m_vkImage copy structure for the current array layer
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = 0;
			bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth;
			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;
			bufferCopyRegions.push_back(bufferCopyRegion);
		}

		// Create optimal tiled target m_vkImage
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { textureArray.width, textureArray.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.arrayLayers = layerCount;

		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &textureArray.image));

		vkGetImageMemoryRequirements(m_vkDevice, textureArray.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAllocInfo, nullptr, &textureArray.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(m_vkDevice, textureArray.image, textureArray.deviceMemory, 0));

		VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Image barrier for optimal m_vkImage (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = layerCount;

		vks::tools::setImageLayout(
			copyCmd,
			textureArray.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled m_vkImage
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			textureArray.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data());

		// Change texture m_vkImage layout to shader read after all faces have been copied
		textureArray.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vks::tools::setImageLayout(
			copyCmd,
			textureArray.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			textureArray.imageLayout,
			subresourceRange);

		m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue, true);

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 8;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(m_vkDevice, &sampler, nullptr, &textureArray.sampler));

		// Create m_vkImage m_vkImageView
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		view.format = format;
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.subresourceRange.layerCount = layerCount;
		view.subresourceRange.levelCount = 1;
		view.image = textureArray.image;
		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &view, nullptr, &textureArray.view));

		// Clean up staging resources
		vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
		vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
		ktxTexture_Destroy(ktxTexture);
	}

	void loadAssets()
	{
		loadTextureArray(getAssetPath() + "textures/texturearray_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = m_vkClearColorValueDefault;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = m_vkRenderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
		renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], m_indexCount, layerCount, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	// Creates a vertex and index buffer for a cube
	// This is used to display the texture on
	void generateCube()
	{
		std::vector<Vertex> vertices = {
			{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f } },
			{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f } },
			{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f } },
			{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f } },

			{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
			{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
			{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },
			{ {  1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },

			{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f } },
			{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f } },
			{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f } },
			{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f } },

			{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f } },
			{ { -1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f } },
			{ { -1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f } },
			{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f } },

			{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
			{ { -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
			{ { -1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f } },
			{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f } },

			{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f } },
			{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f } },
			{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } },
			{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
		};
		std::vector<uint32_t> indices = {
			0,1,2, 0,2,3, 4,5,6,  4,6,7, 8,9,10, 8,10,11, 12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
		};

		m_indexCount = static_cast<uint32_t>(indices.size());

		// Create buffers and upload data to the GPU
		struct StagingBuffers {
			vks::Buffer vertices;
			vks::Buffer indices;
		} stagingBuffers;

		// Host visible source buffers (staging)
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffers.vertices, vertices.size() * sizeof(Vertex), vertices.data()));
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffers.indices, indices.size() * sizeof(uint32_t), indices.data()));

		// Device local destination buffers
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, vertices.size() * sizeof(Vertex)));
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, indices.size() * sizeof(uint32_t)));

		// Copy from host do m_vkDevice
		m_pVulkanDevice->copyBuffer(&stagingBuffers.vertices, &vertexBuffer, m_vkQueue);
		m_pVulkanDevice->copyBuffer(&stagingBuffers.indices, &indexBuffer, m_vkQueue);

		// Clean up
		stagingBuffers.vertices.destroy();
		stagingBuffers.indices.destroy();
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// Binding 1 : Fragment shader m_vkImage sampler
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));

		// Image descriptor for the texture array
		VkDescriptorImageInfo textureDescriptor =
			vks::initializers::descriptorImageInfo(
				textureArray.sampler,
				textureArray.view,
				textureArray.imageLayout);

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
			// Binding 1 : Fragment shader texture sampler
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textureDescriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables, 0);

		// Vertex bindings and attributes
		VkVertexInputBindingDescription vertexInputBinding = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
		};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// Instancing m_vkPipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getShadersPath() + "texturearray/instancing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "texturearray/instancing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	void prepareUniformBuffers()
	{
		uniformData.instance = new PerInstanceData[layerCount];

		uint32_t uboSize = sizeof(uniformData.matrices) + (MAX_LAYERS * sizeof(PerInstanceData));

		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, uboSize));

		// Array indices and model matrices are fixed
		float offset = -1.5f;
		float center = (layerCount*offset) / 2.0f - (offset * 0.5f);
		for (uint32_t i = 0; i < layerCount; i++) {
			// Instance model matrix
			uniformData.instance[i].model = glm::translate(glm::mat4(1.0f), glm::vec3(i * offset - center, 0.0f, 0.0f));
			uniformData.instance[i].model = glm::scale(uniformData.instance[i].model, glm::vec3(0.5f));
			// Instance texture array index
			uniformData.instance[i].arrayIndex = (float)i;
		}

		// Update instanced part of the uniform buffer
		uint8_t *pData;
		uint32_t dataOffset = sizeof(uniformData.matrices);
		uint32_t dataSize = layerCount * sizeof(PerInstanceData);
		VK_CHECK_RESULT(vkMapMemory(m_vkDevice, uniformBuffer.memory, dataOffset, dataSize, 0, (void **)&pData));
		memcpy(pData, uniformData.instance, dataSize);
		vkUnmapMemory(m_vkDevice, uniformBuffer.memory);

		// Map persistent
		VK_CHECK_RESULT(uniformBuffer.map());
	}

	void updateUniformBuffersCamera()
	{
		uniformData.matrices.projection = camera.matrices.perspective;
		uniformData.matrices.view = camera.matrices.view;
		memcpy(uniformBuffer.mapped, &uniformData.matrices, sizeof(uniformData.matrices));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		generateCube();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		m_prepared = true;
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		m_vkSubmitInfo.commandBufferCount = 1;
		m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	virtual void render()
	{
		if (!m_prepared)
			return;
		updateUniformBuffersCamera();
		draw();
	}
};

VULKAN_EXAMPLE_MAIN()
