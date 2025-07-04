/*
* Vulkan Example - Order Independent Transparency rendering using linked lists
*
* Copyright by Sascha Willems - www.saschawillems.de
* Copyright by Daemyung Jang  - dm86.jang@gmail.com
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define NODE_COUNT 20

class VulkanExample : public VulkanExampleBase
{
public:
	struct {
		vkglTF::Model sphere;
		vkglTF::Model cube;
	} models;

	struct Node {
		glm::vec4 color;
		float depth{ 0.0f };
		uint32_t next{ 0 };
	};

	struct {
		uint32_t count{ 0 };
		uint32_t maxNodeCount{ 0 };
	} geometrySBO;

	struct GeometryPass {
		VkRenderPass renderPass{ VK_NULL_HANDLE };
		VkFramebuffer framebuffer{ VK_NULL_HANDLE };
		vks::Buffer geometry;
		vks::Texture headIndex;
		vks::Buffer linkedList;
	} geometryPass;

	struct RenderPassUniformData {
		glm::mat4 projection;
		glm::mat4 view;
	} renderPassUniformData;
	vks::Buffer renderPassUniformBuffer;

	struct ObjectData {
		glm::mat4 model;
		glm::vec4 color;
	};

	struct {
		VkDescriptorSetLayout geometry{ VK_NULL_HANDLE };
		VkDescriptorSetLayout color{ VK_NULL_HANDLE };
	} descriptorSetLayouts;

	struct {
		VkPipelineLayout geometry{ VK_NULL_HANDLE };
		VkPipelineLayout color{ VK_NULL_HANDLE };
	} pipelineLayouts;

	struct {
		VkPipeline geometry{ VK_NULL_HANDLE };
		VkPipeline color{ VK_NULL_HANDLE };
	} pipelines;

	struct {
		VkDescriptorSet geometry{ VK_NULL_HANDLE };
		VkDescriptorSet color{ VK_NULL_HANDLE };
	} descriptorSets;

	VkDeviceSize objectUniformBufferSize{ 0 };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Order independent transparency rendering";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -6.0f));
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setPerspective(60.0f, (float) m_drawAreaWidth / (float) m_drawAreaHeight, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipeline(m_vkDevice, pipelines.geometry, nullptr);
			vkDestroyPipeline(m_vkDevice, pipelines.color, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, pipelineLayouts.geometry, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, pipelineLayouts.color, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayouts.geometry, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayouts.color, nullptr);
			destroyGeometryPass();
			renderPassUniformBuffer.destroy();
		}
	}

	void getEnabledFeatures() override
	{
		// The linked lists are built in a fragment shader using atomic stores, so the sample won't work without that feature available
		if (m_vkPhysicalDeviceFeatures.fragmentStoresAndAtomics) {
			m_vkPhysicalDeviceFeatures10.fragmentStoresAndAtomics = VK_TRUE;
		} else {
			vks::tools::exitFatal("Selected GPU does not support stores and atomic operations in the fragment stage", VK_ERROR_FEATURE_NOT_PRESENT);
		}
	};

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
		models.sphere.loadFromFile(getAssetPath() + "models/sphere.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
		models.cube.loadFromFile(getAssetPath() + "models/cube.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
	}

	void prepareUniformBuffers()
	{
		// Create an uniform buffer for a render pass.
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &renderPassUniformBuffer, sizeof(RenderPassUniformData)));
		VK_CHECK_RESULT(renderPassUniformBuffer.map());
	}

	void prepareGeometryPass()
	{
		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		// Geometry render pass doesn't need any output attachment.
		VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = 0;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;

		VK_CHECK_RESULT(vkCreateRenderPass(m_vkDevice, &renderPassInfo, nullptr, &geometryPass.renderPass));

		// Geometry frame buffer doesn't need any output attachment.
		VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
		fbufCreateInfo.renderPass = geometryPass.renderPass;
		fbufCreateInfo.attachmentCount = 0;
		fbufCreateInfo.width = m_drawAreaWidth;
		fbufCreateInfo.height = m_drawAreaHeight;
		fbufCreateInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(m_vkDevice, &fbufCreateInfo, nullptr, &geometryPass.framebuffer));

		// Create a buffer for GeometrySBO
		vks::Buffer stagingBuffer;
	
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stagingBuffer,
			sizeof(geometrySBO)));
		VK_CHECK_RESULT(stagingBuffer.map());

		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&geometryPass.geometry,
			sizeof(geometrySBO)));

		// Set up GeometrySBO data.
		geometrySBO.count = 0;
		geometrySBO.maxNodeCount = NODE_COUNT * m_drawAreaWidth * m_drawAreaHeight;
		memcpy(stagingBuffer.mapped, &geometrySBO, sizeof(geometrySBO));

		// Copy data to m_vkDevice
		VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};
		copyRegion.size = sizeof(geometrySBO);
		vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, geometryPass.geometry.buffer, 1, &copyRegion);
		m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue, true);

		stagingBuffer.destroy();
		
		// Create a texture for HeadIndex.
		// This m_vkImage will track the head index of each fragment.
		geometryPass.headIndex.device = m_pVulkanDevice;

		VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R32_UINT;
		imageInfo.extent.width = m_drawAreaWidth;
		imageInfo.extent.height = m_drawAreaHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
		// SRS - On macOS/iOS use linear tiling for atomic m_vkImage access, see https://github.com/KhronosGroup/MoltenVK/issues/1027
		imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
#else
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
#endif
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &imageInfo, nullptr, &geometryPass.headIndex.image));

		geometryPass.headIndex.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(m_vkDevice, geometryPass.headIndex.image, &memReqs);

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, &geometryPass.headIndex.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(m_vkDevice, geometryPass.headIndex.image, geometryPass.headIndex.deviceMemory, 0));

		VkImageViewCreateInfo imageViewInfo = vks::initializers::imageViewCreateInfo();
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = VK_FORMAT_R32_UINT;
		imageViewInfo.flags = 0;
		imageViewInfo.image = geometryPass.headIndex.image;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &imageViewInfo, nullptr, &geometryPass.headIndex.view));

		geometryPass.headIndex.width = m_drawAreaWidth;
		geometryPass.headIndex.height = m_drawAreaHeight;
		geometryPass.headIndex.mipLevels = 1;
		geometryPass.headIndex.layerCount = 1;
		geometryPass.headIndex.descriptor.imageView = geometryPass.headIndex.view;
		geometryPass.headIndex.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		geometryPass.headIndex.sampler = VK_NULL_HANDLE;

		// Create a buffer for LinkedListSBO
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&geometryPass.linkedList,
			sizeof(Node) * geometrySBO.maxNodeCount));

		// Change HeadIndex m_vkImage's layout from UNDEFINED to GENERAL
		VkCommandBufferAllocateInfo cmdBufAllocInfo = vks::initializers::commandBufferAllocateInfo(m_vkCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

		VkCommandBuffer cmdBuf;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(m_vkDevice, &cmdBufAllocInfo, &cmdBuf));

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &cmdBufInfo));

		VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.image = geometryPass.headIndex.image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));

		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(m_vkQueue));
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layouts

		// Create a geometry descriptor set layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// renderPassUniformData
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			// AtomicSBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			// headIndexImage
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			// LinkedListSBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayouts.geometry));

		// Create a color descriptor set layout
		setLayoutBindings = {
			// headIndexImage
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			// LinkedListSBO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayouts.color));

		updateDescriptors();
	}

	void updateDescriptors()
	{
		// Images and linked buffers are recreated on resize and part of the descriptors, so we need to update those at runtime
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &descriptorSetLayouts.geometry, 1);

		// Update a geometry descriptor set

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.geometry));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: renderPassUniformData
			vks::initializers::writeDescriptorSet(descriptorSets.geometry, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &renderPassUniformBuffer.descriptor),
			// Binding 2: GeometrySBO
			vks::initializers::writeDescriptorSet(descriptorSets.geometry, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &geometryPass.geometry.descriptor),
			// Binding 3: headIndexImage
			vks::initializers::writeDescriptorSet(descriptorSets.geometry, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &geometryPass.headIndex.descriptor),
			// Binding 4: LinkedListSBO
			vks::initializers::writeDescriptorSet(descriptorSets.geometry, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &geometryPass.linkedList.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// Update a color descriptor set
		allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &descriptorSetLayouts.color, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.color));

		writeDescriptorSets = {
			// Binding 0: headIndexImage
			vks::initializers::writeDescriptorSet(descriptorSets.color, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &geometryPass.headIndex.descriptor),
			// Binding 1: LinkedListSBO
			vks::initializers::writeDescriptorSet(descriptorSets.color, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &geometryPass.linkedList.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layouts

		// Create a geometry pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.geometry, 1);
		// Static object data passed using push constants
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ObjectData), 0);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &pipelineLayouts.geometry));

		// Create a color pipeline layout
		pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.color, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &pipelineLayouts.color));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(0, nullptr);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayouts.geometry, geometryPass.renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position });

		// Create a geometry pipeline
		shaderStages[0] = loadShader(getShadersPath() + "oit/geometry.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "oit/geometry.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.geometry));

		// Create a color pipeline
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayouts.color, m_vkRenderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = &vertexInputInfo;

		shaderStages[0] = loadShader(getShadersPath() + "oit/color.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "oit/color.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.color));
	}

	void buildCommandBuffers() override
	{
		if (m_resized)
			return;

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = m_vkClearColorValueDefault;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
		renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
		
		VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			// Update dynamic viewport state
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			// Update dynamic scissor state
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkClearColorValue clearColor;
			clearColor.uint32[0] = 0xffffffff;

			VkImageSubresourceRange subresRange = {};

			subresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresRange.levelCount = 1;
			subresRange.layerCount = 1;

			vkCmdClearColorImage(drawCmdBuffers[i], geometryPass.headIndex.image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresRange);

			// Clear previous geometry pass data
			vkCmdFillBuffer(drawCmdBuffers[i], geometryPass.geometry.buffer, 0, sizeof(uint32_t), 0);

			// We need a barrier to make sure all writes are finished before starting to write again
			VkMemoryBarrier memoryBarrier = vks::initializers::memoryBarrier();
			memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

			// Begin the geometry render pass
			renderPassBeginInfo.renderPass = geometryPass.renderPass;
			renderPassBeginInfo.framebuffer = geometryPass.framebuffer;
			renderPassBeginInfo.clearValueCount = 0;
			renderPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.geometry);
			uint32_t dynamicOffset = 0;
			models.sphere.bindBuffers(drawCmdBuffers[i]);

			// Render the scene
			ObjectData objectData;

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.geometry, 0, 1, &descriptorSets.geometry, 0, nullptr);
			objectData.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
			for (int32_t x = 0; x < 5; x++)
			{
				for (int32_t y = 0; y < 5; y++)
				{
					for (int32_t z = 0; z < 5; z++)
					{
						glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x - 2, y - 2, z - 2));
						glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));
						objectData.model = T * S;
						vkCmdPushConstants(drawCmdBuffers[i], pipelineLayouts.geometry, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ObjectData), &objectData);
						models.sphere.draw(drawCmdBuffers[i]);
					}
				}
			}

			models.cube.bindBuffers(drawCmdBuffers[i]);
			objectData.color = glm::vec4(0.0f, 0.0f, 1.0f, 0.5f);
			for (uint32_t x = 0; x < 2; x++)
			{
				glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f * x - 1.5f, 0.0f, 0.0f));
				glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f));
				objectData.model = T * S;
				vkCmdPushConstants(drawCmdBuffers[i], pipelineLayouts.geometry, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ObjectData), &objectData);
				models.cube.draw(drawCmdBuffers[i]);
			}

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			// Make a pipeline barrier to guarantee the geometry pass is done
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

			// We need a barrier to make sure all writes are finished before starting to write again
			memoryBarrier = vks::initializers::memoryBarrier();
			memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

			// Begin the color render pass
			renderPassBeginInfo.renderPass = m_vkRenderPass;
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.color);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.color, 0, 1, &descriptorSets.color, 0, nullptr);
			vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);
			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void updateUniformBuffers()
	{
		renderPassUniformData.projection = camera.matrices.perspective;
		renderPassUniformData.view = camera.matrices.view;
		memcpy(renderPassUniformBuffer.mapped, &renderPassUniformData, sizeof(RenderPassUniformData));
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers();
		prepareGeometryPass();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		updateUniformBuffers();
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

	void render() override
	{
		if (!m_prepared)
			return;
		updateUniformBuffers();
		draw();
	}

	void windowResized() override
	{
		destroyGeometryPass();
		prepareGeometryPass();
		vkResetDescriptorPool(m_vkDevice, m_vkDescriptorPool, 0);
		updateDescriptors();
		m_resized = false;
		buildCommandBuffers();
	}

	void destroyGeometryPass()
	{
		vkDestroyRenderPass(m_vkDevice, geometryPass.renderPass, nullptr);
		vkDestroyFramebuffer(m_vkDevice, geometryPass.framebuffer, nullptr);
		geometryPass.geometry.destroy();
		geometryPass.headIndex.destroy();
		geometryPass.linkedList.destroy();
	}
};

VULKAN_EXAMPLE_MAIN()
