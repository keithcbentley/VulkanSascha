/*
* Vulkan Example - Text overlay rendering on-top of an existing scene using a separate render pass
*
* This sample renders a basic text overlay on top of a 3D scene that can be used e.g. for debug purposes
* For a more complete GUI sample see the ImGui sample
* 
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <sstream>
#include <iomanip>
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "../external/stb/stb_font_consolas_24_latin1.inl"

// Max. number of chars the text overlay buffer can hold
#define TEXTOVERLAY_MAX_CHAR_COUNT 2048

/*
	Mostly self-contained text overlay class
	This class contains all Vulkan resources for drawing the text overlay
	It can be plugged into an existing renderpass/command buffer
*/
class TextOverlay
{
private:
	// Created by this class
	// Font m_vkImage
	VkSampler sampler;
	VkImage image;
	VkImageView view;
	VkDeviceMemory imageMemory;
	// Character vertex buffer
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipelineCache pipelineCache;
	VkPipeline pipeline;

	// Passed from the sample
	VkRenderPass renderPass;
	VkQueue queue;
	vks::VulkanDevice* vulkanDevice;
	uint32_t* frameBufferWidth;
	uint32_t* frameBufferHeight;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	float scale;

	// Pointer to mapped vertex buffer
	glm::vec4 *mapped = nullptr;

	stb_fontchar stbFontData[STB_FONT_consolas_24_latin1_NUM_CHARS];
public:
	enum TextAlign { alignLeft, alignCenter, alignRight };

	uint32_t numLetters;
	bool visible = true;

	TextOverlay(
		vks::VulkanDevice *vulkanDevice,
		VkQueue queue,
		VkRenderPass renderPass,
		uint32_t *framebufferwidth,
		uint32_t *framebufferheight,
		float scale,
		std::vector<VkPipelineShaderStageCreateInfo> shaderstages)
	{
		this->vulkanDevice = vulkanDevice;
		this->queue = queue;
		this->shaderStages = shaderstages;
		this->frameBufferWidth = framebufferwidth;
		this->frameBufferHeight = framebufferheight;
		this->scale = scale;
		this->renderPass = renderPass;

		prepareResources();
		preparePipeline();
	}

	~TextOverlay()
	{
		// Free up all Vulkan resources requested by the text overlay
		vkDestroySampler(vulkanDevice->m_vkDevice, sampler, nullptr);
		vkDestroyImage(vulkanDevice->m_vkDevice, image, nullptr);
		vkDestroyImageView(vulkanDevice->m_vkDevice, view, nullptr);
		vkDestroyBuffer(vulkanDevice->m_vkDevice, buffer, nullptr);
		vkFreeMemory(vulkanDevice->m_vkDevice, memory, nullptr);
		vkFreeMemory(vulkanDevice->m_vkDevice, imageMemory, nullptr);
		vkDestroyDescriptorSetLayout(vulkanDevice->m_vkDevice, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(vulkanDevice->m_vkDevice, descriptorPool, nullptr);
		vkDestroyPipelineLayout(vulkanDevice->m_vkDevice, pipelineLayout, nullptr);
		vkDestroyPipelineCache(vulkanDevice->m_vkDevice, pipelineCache, nullptr);
		vkDestroyPipeline(vulkanDevice->m_vkDevice, pipeline, nullptr);
	}

	// Prepare all vulkan resources required to render the font
	// The text overlay uses separate resources for descriptors (pool, sets, layouts), pipelines and command buffers
	void prepareResources()
	{
		const uint32_t fontWidth = STB_FONT_consolas_24_latin1_BITMAP_WIDTH;
		const uint32_t fontHeight = STB_FONT_consolas_24_latin1_BITMAP_HEIGHT;

		static unsigned char font24pixels[fontHeight][fontWidth];
		stb_font_consolas_24_latin1(stbFontData, font24pixels, fontHeight);

		// Vertex buffer
		VkDeviceSize bufferSize = TEXTOVERLAY_MAX_CHAR_COUNT * sizeof(glm::vec4);

		VkBufferCreateInfo bufferInfo = vks::initializers::bufferCreateInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, bufferSize);
		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->m_vkDevice, &bufferInfo, nullptr, &buffer));

		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();

		vkGetBufferMemoryRequirements(vulkanDevice->m_vkDevice, buffer, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->m_vkDevice, &allocInfo, nullptr, &memory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->m_vkDevice, buffer, memory, 0));

		// Font texture
		VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R8_UNORM;
		imageInfo.extent.width = fontWidth;
		imageInfo.extent.height = fontHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK_RESULT(vkCreateImage(vulkanDevice->m_vkDevice, &imageInfo, nullptr, &image));

		vkGetImageMemoryRequirements(vulkanDevice->m_vkDevice, image, &memReqs);
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->m_vkDevice, &allocInfo, nullptr, &imageMemory));
		VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->m_vkDevice, image, imageMemory, 0));

		// Staging

		struct {
			VkDeviceMemory memory;
			VkBuffer buffer;
		} stagingBuffer;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = allocInfo.allocationSize;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->m_vkDevice, &bufferCreateInfo, nullptr, &stagingBuffer.buffer));

		// Get m_vkDeviceMemory requirements for the staging buffer (alignment, m_vkDeviceMemory type bits)
		vkGetBufferMemoryRequirements(vulkanDevice->m_vkDevice, stagingBuffer.buffer, &memReqs);

		allocInfo.allocationSize = memReqs.size;
		// Get m_vkDeviceMemory type index for a host visible buffer
		allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->m_vkDevice, &allocInfo, nullptr, &stagingBuffer.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->m_vkDevice, stagingBuffer.buffer, stagingBuffer.memory, 0));

		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(vulkanDevice->m_vkDevice, stagingBuffer.memory, 0, allocInfo.allocationSize, 0, (void **)&data));
		// Size of the font texture is WIDTH * HEIGHT * 1 byte (only one channel)
		memcpy(data, &font24pixels[0][0], fontWidth * fontHeight);
		vkUnmapMemory(vulkanDevice->m_vkDevice, stagingBuffer.memory);

		// Copy to m_vkImage

		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Prepare for transfer
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = fontWidth;
		bufferCopyRegion.imageExtent.height = fontHeight;
		bufferCopyRegion.imageExtent.depth = 1;

		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer.buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion
			);

		// Prepare for shader read
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vulkanDevice->flushCommandBuffer(copyCmd, queue);

		vkFreeMemory(vulkanDevice->m_vkDevice, stagingBuffer.memory, nullptr);
		vkDestroyBuffer(vulkanDevice->m_vkDevice, stagingBuffer.buffer, nullptr);

		VkImageViewCreateInfo imageViewInfo = vks::initializers::imageViewCreateInfo();
		imageViewInfo.image = image;
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = imageInfo.format;
		imageViewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,	VK_COMPONENT_SWIZZLE_A };
		imageViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VK_CHECK_RESULT(vkCreateImageView(vulkanDevice->m_vkDevice, &imageViewInfo, nullptr, &view));

		// Sampler
		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(vulkanDevice->m_vkDevice, &samplerInfo, nullptr, &sampler));

		// Descriptor
		// Font uses a separate descriptor pool
		std::array<VkDescriptorPoolSize, 1> poolSizes;
		poolSizes[0] = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

		VkDescriptorPoolCreateInfo descriptorPoolInfo =
			vks::initializers::descriptorPoolCreateInfo(
				static_cast<uint32_t>(poolSizes.size()),
				poolSizes.data(),
				1);

		VK_CHECK_RESULT(vkCreateDescriptorPool(vulkanDevice->m_vkDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Descriptor set layout
		std::array<VkDescriptorSetLayoutBinding, 1> setLayoutBindings;
		setLayoutBindings[0] = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->m_vkDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

		// Descriptor set
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->m_vkDevice, &descriptorSetAllocInfo, &descriptorSet));

		// Descriptor for the font m_vkImage
		VkDescriptorImageInfo texDescriptor = vks::initializers::descriptorImageInfo(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
		std::array<VkWriteDescriptorSet, 1> writeDescriptorSets;
		writeDescriptorSets[0] = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &texDescriptor);
		vkUpdateDescriptorSets(vulkanDevice->m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	// Prepare a separate m_vkPipeline for the font rendering decoupled from the main application
	void preparePipeline()
	{
		// Pipeline cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(vulkanDevice->m_vkDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->m_vkDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout));

		// Enable blending, using alpha from red channel of the font texture (see text.frag)
		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		std::array<VkVertexInputBindingDescription, 2> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX),
			vks::initializers::vertexInputBindingDescription(1, sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, 0),					// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(1, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec2)),	// Location 1: UV
		};

		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCreateInfo.pVertexInputState = &vertexInputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanDevice->m_vkDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
	}

	// Map buffer
	void beginTextUpdate()
	{
		VK_CHECK_RESULT(vkMapMemory(vulkanDevice->m_vkDevice, memory, 0, VK_WHOLE_SIZE, 0, (void **)&mapped));
		numLetters = 0;
	}

	// Add text to the current buffer
	void addText(std::string text, float x, float y, TextAlign align)
	{
		const uint32_t firstChar = STB_FONT_consolas_24_latin1_FIRST_CHAR;

		assert(mapped != nullptr);

		const float charW = 1.5f * scale / *frameBufferWidth;
		const float charH = 1.5f * scale / *frameBufferHeight;

		float fbW = (float)*frameBufferWidth;
		float fbH = (float)*frameBufferHeight;
		x = (x / fbW * 2.0f) - 1.0f;
		y = (y / fbH * 2.0f) - 1.0f;

		// Calculate text m_drawAreaWidth
		float textWidth = 0;
		for (auto letter : text)
		{
			stb_fontchar *charData = &stbFontData[(uint32_t)letter - firstChar];
			textWidth += charData->advance * charW;
		}

		switch (align)
		{
			case alignRight:
				x -= textWidth;
				break;
			case alignCenter:
				x -= textWidth / 2.0f;
				break;
			case alignLeft:
				break;
		}

		// Generate a uv mapped quad per char in the new text
		for (auto letter : text)
		{
			stb_fontchar *charData = &stbFontData[(uint32_t)letter - firstChar];

			mapped->x = (x + (float)charData->x0 * charW);
			mapped->y = (y + (float)charData->y0 * charH);
			mapped->z = charData->s0;
			mapped->w = charData->t0;
			mapped++;

			mapped->x = (x + (float)charData->x1 * charW);
			mapped->y = (y + (float)charData->y0 * charH);
			mapped->z = charData->s1;
			mapped->w = charData->t0;
			mapped++;

			mapped->x = (x + (float)charData->x0 * charW);
			mapped->y = (y + (float)charData->y1 * charH);
			mapped->z = charData->s0;
			mapped->w = charData->t1;
			mapped++;

			mapped->x = (x + (float)charData->x1 * charW);
			mapped->y = (y + (float)charData->y1 * charH);
			mapped->z = charData->s1;
			mapped->w = charData->t1;
			mapped++;

			x += charData->advance * charW;

			numLetters++;
		}
	}

	// Unmap buffer and update command buffers
	void endTextUpdate()
	{
		vkUnmapMemory(vulkanDevice->m_vkDevice, memory);
		mapped = nullptr;
		//updateCommandBuffers();
	}

	// Issue the draw commands for the characters of the overlay
	void draw(VkCommandBuffer cmdBuffer)
	{
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

		VkDeviceSize offsets = 0;
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offsets);
		vkCmdBindVertexBuffers(cmdBuffer, 1, 1, &buffer, &offsets);
		// One draw command for every character. This is okay for a debug overlay, but not optimal
		// In a real-world application one would try to batch draw commands
		for (uint32_t j = 0; j < numLetters; j++) {
			vkCmdDraw(cmdBuffer, 4, 1, j * 4, 0);
		}
	}
};

/*
	Vulkan example main class
*/
class VulkanExample : public VulkanExampleBase
{
public:
	TextOverlay* textOverlay{ nullptr };

	vkglTF::Model model;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Text overlay";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
		camera.setRotation(glm::vec3(-25.0f, -0.0f, 0.0f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
                m_exampleSettings.m_showUIOverlay = false;
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			uniformBuffer.destroy();
			delete(textOverlay);
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[3];

		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = m_vkRenderPass;
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

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			model.draw(drawCmdBuffers[i]);

			if (textOverlay->visible) {
				textOverlay->draw(drawCmdBuffers[i]);
			}

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}

		vkQueueWaitIdle(m_vkQueue);
	}

	// Update the text buffer displayed by the text overlay
	void updateTextOverlay(void)
	{
		uint32_t lastNumLetters = textOverlay->numLetters;

		textOverlay->beginTextUpdate();

		textOverlay->addText(title, 5.0f * m_UIOverlay.scale, 5.0f * m_UIOverlay.scale, TextOverlay::alignLeft);

		std::stringstream ss;
		ss << std::fixed << std::setprecision(2) << (m_frameTimer * 1000.0f) << "ms (" << m_lastFPS << " fps)";
		textOverlay->addText(ss.str(), 5.0f * m_UIOverlay.scale, 25.0f * m_UIOverlay.scale, TextOverlay::alignLeft);

		textOverlay->addText(m_vkPhysicalDeviceProperties.deviceName, 5.0f * m_UIOverlay.scale, 45.0f * m_UIOverlay.scale, TextOverlay::alignLeft);

		// Display current model m_vkImageView matrix
		textOverlay->addText("model m_vkImageView matrix", (float)m_drawAreaWidth - 5.0f * m_UIOverlay.scale, 5.0f * m_UIOverlay.scale, TextOverlay::alignRight);

		for (uint32_t i = 0; i < 4; i++)
		{
			ss.str("");
			ss << std::fixed << std::setprecision(2) << std::showpos;
			ss << uniformData.modelView[0][i] << " " << uniformData.modelView[1][i] << " " << uniformData.modelView[2][i] << " " << uniformData.modelView[3][i];
			textOverlay->addText(ss.str(), (float)m_drawAreaWidth - 5.0f * m_UIOverlay.scale, (25.0f + (float)i * 20.0f) * m_UIOverlay.scale, TextOverlay::alignRight);
		}

		glm::vec3 projected = glm::project(glm::vec3(0.0f), uniformData.modelView, uniformData.projection, glm::vec4(0, 0, (float)m_drawAreaWidth, (float)m_drawAreaHeight));
		textOverlay->addText("A torus knot", projected.x, projected.y, TextOverlay::alignCenter);

#if defined(__ANDROID__)
#else
		textOverlay->addText("Press \"space\" to toggle text overlay", 5.0f * m_UIOverlay.scale, 65.0f * m_UIOverlay.scale, TextOverlay::alignLeft);
		textOverlay->addText("Hold middle mouse button and drag to move", 5.0f * m_UIOverlay.scale, 85.0f * m_UIOverlay.scale, TextOverlay::alignLeft);
#endif
		textOverlay->endTextUpdate();

		// If the no. of letters changed, the no. of draw commands also changes which requires a rebuild of the command buffers
		if (lastNumLetters != textOverlay->numLetters) {
			std::cout << "rebuild cb\n";
			buildCommandBuffers();
		}
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		model.loadFromFile(getAssetPath() + "models/torusknot.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0: Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));
		
		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV});

		shaderStages[0] = loadShader(getShadersPath() + "textoverlay/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "textoverlay/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
		VK_CHECK_RESULT(uniformBuffer.map());
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view;
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	}

	void prepareTextOverlay()
	{
		// Load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(loadShader(getShadersPath() + "textoverlay/text.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderStages.push_back(loadShader(getShadersPath() + "textoverlay/text.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

		textOverlay = new TextOverlay(
			m_pVulkanDevice,
			m_vkQueue,
			m_vkRenderPass,
			&m_drawAreaWidth,
			&m_drawAreaHeight,
			m_UIOverlay.scale,
			shaderStages
			);
		updateTextOverlay();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		prepareTextOverlay();
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
		updateUniformBuffers();
		if (camera.updated) {
			updateTextOverlay();
		}
		draw();
	}

	virtual void keyPressed(uint32_t keyCode)
	{
		switch (keyCode)
		{
		case KEY_KPADD:
		case KEY_SPACE:
			textOverlay->visible = !textOverlay->visible;
			buildCommandBuffers();
		}
	}
};

VULKAN_EXAMPLE_MAIN()
