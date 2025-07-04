/*
* Vulkan Example - Host m_vkImage copy using VK_EXT_host_image_copy
* 
* This sample shows how to use host m_vkImage copies to directly upload an m_vkImage to the devic without having to use staging

* Copyright (C) 2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <ktx.h>
#include <ktxvulkan.h>

class VulkanExample : public VulkanExampleBase
{
public:
	// Pointers for functions added by the host m_vkImage copy extension;
	PFN_vkCopyMemoryToImageEXT vkCopyMemoryToImageEXT{ nullptr };
	PFN_vkTransitionImageLayoutEXT vkTransitionImageLayoutEXT{ nullptr };
	// Used to check feature m_vkImage format support for host m_vkImage copies
	PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2{ nullptr };

	VkPhysicalDeviceHostImageCopyFeaturesEXT enabledPhysicalDeviceHostImageCopyFeaturesEXT{};

	// Contains all Vulkan objects that are required to store and use a texture
	struct Texture {
		VkSampler sampler{ VK_NULL_HANDLE };
		VkImage image{ VK_NULL_HANDLE };
		VkDeviceMemory deviceMemory{ VK_NULL_HANDLE };
		VkImageView view{ VK_NULL_HANDLE };
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t mipLevels{ 0 };
	} texture;

	vkglTF::Model plane;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 viewPos;
		float lodBias = 0.0f;
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Host m_vkImage copy";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -1.5f));
		camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);

		// Enable required extensions
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);

		// Enable host m_vkImage copy feature
		enabledPhysicalDeviceHostImageCopyFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT;
		enabledPhysicalDeviceHostImageCopyFeaturesEXT.hostImageCopy = VK_TRUE;
		deviceCreatepNextChain = &enabledPhysicalDeviceHostImageCopyFeaturesEXT;
	}

	~VulkanExample() override
	{
		if (m_vkDevice) {
			destroyTextureImage(texture);
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayout, nullptr);
			uniformBuffer.destroy();
		}
	}

	// Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
	void getEnabledFeatures() override
	{
		// Enable anisotropic filtering if supported
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		};
	}

	/*
		Upload texture m_vkImage data to the GPU

		Unlike the texture(3d/array/etc) samples, this one uses the VK_EXT_host_image_copy to drasticly simplify the process 
		of uploading an m_vkImage from the host to the GPU. This new extension adds a way of directly uploading m_vkImage data from
		host m_vkDeviceMemory to an optimal tiled m_vkImage on the m_vkDevice (GPU). This no longer requires a staging buffer in between, as we can
		now directly copy data stored in host m_vkDeviceMemory to the m_vkImage. The extension also adds new functionality to simplfy m_vkImage barriers
	*/
	void loadTexture()
	{
		// We use the Khronos texture format (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/)
		std::string filename = getAssetPath() + "textures/metalplate01_rgba.ktx";

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
		texture.width = ktxTexture->baseWidth;
		texture.height = ktxTexture->baseHeight;
		texture.mipLevels = ktxTexture->numLevels;
		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);

		const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;

		// Check if the m_vkImage format supports the host m_vkImage copy flag
		// Note: All formats that support sampling are required to support this flag
		// So for the format used here (R8G8B8A8_UNORM) we could skip this check
		// The flag we need to check is an extension flag, so we need to go through VkFormatProperties3 
		VkFormatProperties3 formatProperties3{};
		formatProperties3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR;
		// Properties3 need to be chained into Properties2
		VkFormatProperties2 formatProperties2{};
		formatProperties2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		formatProperties2.pNext = &formatProperties3;
		vkGetPhysicalDeviceFormatProperties2(m_vkPhysicalDevice, imageFormat, &formatProperties2);

		if ((formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0) {
			vks::tools::exitFatal("The selected m_vkImage format does not support the required host transfer bit.", -1);
		}

		// Create optimal tiled target m_vkImage on the m_vkDevice
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = imageFormat;
		imageCreateInfo.mipLevels = texture.mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { texture.width, texture.height, 1 };
		// For images that use host m_vkImage copy we need to specify the VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT usage flag
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &texture.image));

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};
		vkGetImageMemoryRequirements(m_vkDevice, texture.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAllocInfo, nullptr, &texture.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(m_vkDevice, texture.image, texture.deviceMemory, 0));

		// With host m_vkImage copy we can directly copy from the KTX m_vkImage in host m_vkDeviceMemory to the m_vkDevice
		// This is pretty straight forward, as the KTX m_vkImage is already tightly packed, doesn't need and swizzle and as such matches
		// what the m_vkDevice expects 

		// Set up copy information for all mip levels stored in the m_vkImage
		std::vector<VkMemoryToImageCopyEXT> memoryToImageCopies{};
		for (uint32_t i = 0; i < texture.mipLevels; i++) {
			// Setup a buffer m_vkImage copy structure for the current mip level
			VkMemoryToImageCopyEXT memoryToImageCopy = {};
			memoryToImageCopy.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT;
			memoryToImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			memoryToImageCopy.imageSubresource.mipLevel = i;
			memoryToImageCopy.imageSubresource.baseArrayLayer = 0;
			memoryToImageCopy.imageSubresource.layerCount = 1;
			memoryToImageCopy.imageExtent.width = ktxTexture->baseWidth >> i;
			memoryToImageCopy.imageExtent.height = ktxTexture->baseHeight >> i;
			memoryToImageCopy.imageExtent.depth = 1;

			// This tells the implementation where to read the data from
			// As the KTX file is tightly packed, we can simply offset into that buffer for the current mip level
			ktx_size_t offset;
			KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
			assert(ret == KTX_SUCCESS);
			memoryToImageCopy.pHostPointer = ktxTextureData + offset;

			memoryToImageCopies.push_back(memoryToImageCopy);
		}

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = texture.mipLevels;
		subresourceRange.layerCount = 1;

		// VK_EXT_host_image_copy als introduces a simplified way of doing the required m_vkImage transition on the host
		// This no longer requires a dedicated command buffer to submit the barrier
		// We also no longer need multiple transitions, and only have to do one for the final layout
		VkHostImageLayoutTransitionInfoEXT hostImageLayoutTransitionInfo{};
		hostImageLayoutTransitionInfo.sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT;
		hostImageLayoutTransitionInfo.image = texture.image;
		hostImageLayoutTransitionInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		hostImageLayoutTransitionInfo.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hostImageLayoutTransitionInfo.subresourceRange = subresourceRange;

		vkTransitionImageLayoutEXT(m_vkDevice, 1, &hostImageLayoutTransitionInfo);

		// With the m_vkImage in the correct layout and copy information for all mip levels setup, we can now issue the copy to our taget m_vkImage from the host
		// The implementation will then convert this to an implementation specific optimal tiling layout
		VkCopyMemoryToImageInfoEXT copyMemoryInfo{};
		copyMemoryInfo.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT;
		copyMemoryInfo.dstImage = texture.image;
		copyMemoryInfo.dstImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		copyMemoryInfo.regionCount = static_cast<uint32_t>(memoryToImageCopies.size());
		copyMemoryInfo.pRegions = memoryToImageCopies.data();

		vkCopyMemoryToImageEXT(m_vkDevice, &copyMemoryInfo);

		ktxTexture_Destroy(ktxTexture);

		// Create a texture sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = (float)texture.mipLevels;
		sampler.maxAnisotropy = vulkanDevice->m_vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
		sampler.anisotropyEnable = VK_TRUE;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(m_vkDevice, &sampler, nullptr, &texture.sampler));

		// Create m_vkImage m_vkImageView
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = imageFormat;
		view.subresourceRange = subresourceRange;
		view.image = texture.image;
		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &view, nullptr, &texture.view));
	}

	// Free all Vulkan resources used by a texture object
	void destroyTextureImage(Texture texture)
	{
		vkDestroyImageView(m_vkDevice, texture.view, nullptr);
		vkDestroyImage(m_vkDevice, texture.image, nullptr);
		vkDestroySampler(m_vkDevice, texture.sampler, nullptr);
		vkFreeMemory(m_vkDevice, texture.deviceMemory, nullptr);
	}

	void buildCommandBuffers() override
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

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);

			plane.draw(drawCmdBuffers[i]);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
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
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));

		// Setup a descriptor m_vkImage info for the current texture to be used as a combined m_vkImage sampler
		VkDescriptorImageInfo textureDescriptor;
		textureDescriptor.imageView = texture.view;
		textureDescriptor.sampler = texture.sampler;
		textureDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
        // Shaders
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages = {
		    loadShader(getShadersPath() + "texture/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		    loadShader(getShadersPath() + "texture/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        };

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Normal });
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCreateInfo, nullptr, &m_vkPipeline));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformData), &uniformData));
		VK_CHECK_RESULT(uniformBuffer.map());
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view;
		uniformData.viewPos = camera.viewPos;
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(uniformData));
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		plane.loadFromFile(getAssetPath() + "models/plane_z.gltf", vulkanDevice, m_vkQueue, glTFLoadingFlags);
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();

		// Get the function pointers required host m_vkImage copies
		vkCopyMemoryToImageEXT = reinterpret_cast<PFN_vkCopyMemoryToImageEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCopyMemoryToImageEXT"));
		vkTransitionImageLayoutEXT = reinterpret_cast<PFN_vkTransitionImageLayoutEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkTransitionImageLayoutEXT"));
		vkGetPhysicalDeviceFormatProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(vkGetInstanceProcAddr(m_vulkanInstance, "vkGetPhysicalDeviceFormatProperties2KHR"));

		loadAssets();
		loadTexture();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		m_vkSubmitInfo.commandBufferCount = 1;
		m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void render() override
	{
		if (!prepared)
			return;
		updateUniformBuffers();
		draw();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings")) {
			if (overlay->sliderFloat("LOD bias", &uniformData.lodBias, 0.0f, (float)texture.mipLevels)) {
				updateUniformBuffers();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
