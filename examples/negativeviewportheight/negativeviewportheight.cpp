/*
* Vulkan Example - Using negative viewport heights for changing Vulkan's coordinate system
*
* Note: Requires a m_vkDevice that supports VK_KHR_MAINTENANCE1
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

class VulkanExample : public VulkanExampleBase
{
public:
	bool negativeViewport = true;
	float offsety = 0.0f;
	float offsetx = 0.0f;
	int32_t windingOrder = 1;
	int32_t cullMode = (int32_t)VK_CULL_MODE_BACK_BIT;
	int32_t quadType = 0;

	VkPipelineLayout m_vkPipelineLayout;
	VkPipeline m_vkPipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_vkDescriptorSetLayout;
	struct DescriptorSets {
		VkDescriptorSet CW;
		VkDescriptorSet CCW;
	} descriptorSets;

	struct Textures {
		vks::Texture2D CW;
		vks::Texture2D CCW;
	} textures;

	struct Quad {
		vks::Buffer verticesYUp;
		vks::Buffer verticesYDown;
		vks::Buffer indicesCCW;
		vks::Buffer indicesCW;
		void destroy()
		{
			verticesYUp.destroy();
			verticesYDown.destroy();
			indicesCCW.destroy();
			indicesCW.destroy();
		}
	} quad;

	VulkanExample() : VulkanExampleBase()
	{
		title = "Negative Viewport m_drawAreaHeight";
		// [POI] VK_KHR_MAINTENANCE1 is required for using negative viewport heights
		// Note: This is core as of Vulkan 1.1. So if you target 1.1 you don't have to explicitly enable this
		m_requestedDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
		vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
		textures.CW.destroy();
		textures.CCW.destroy();
		quad.destroy();
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

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);

			// [POI] Viewport setup
			VkViewport viewport{};
			if (negativeViewport) {
				viewport.x = offsetx;
				// [POI] When using a negative viewport m_drawAreaHeight, the origin needs to be adjusted too
				viewport.y = (float)m_drawAreaHeight - offsety;
				viewport.width = (float)m_drawAreaWidth;
				// [POI] Flip the sign of the viewport's m_drawAreaHeight
				viewport.height = -(float)m_drawAreaHeight;
			}
			else {
				viewport.x = offsetx;
				viewport.y = offsety;
				viewport.width = (float)m_drawAreaWidth;
				viewport.height = (float)m_drawAreaHeight;
			}
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			// Render the quad with clock wise and counter clock wise indices, visibility is determined by m_vkPipeline settings

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSets.CW, 0, nullptr);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], quad.indicesCW.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, quadType == 0 ? &quad.verticesYDown.buffer : &quad.verticesYUp.buffer, offsets);
			vkCmdDrawIndexed(drawCmdBuffers[i], 6, 1, 0, 0, 0);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSets.CCW, 0, nullptr);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], quad.indicesCCW.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawCmdBuffers[i], 6, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		textures.CW.loadFromFile(getAssetPath() + "textures/texture_orientation_cw_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_vkQueue);
		textures.CCW.loadFromFile(getAssetPath() + "textures/texture_orientation_ccw_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_vkQueue);

		// [POI] Create two quads with different Y orientations

		struct Vertex {
			float pos[3];
			float uv[2];
		};

		const float ar = (float)m_drawAreaHeight / (float)m_drawAreaWidth;

		// OpenGL style (y points upwards)
		std::vector<Vertex> verticesYPos = {
			{ -1.0f * ar,  1.0f, 1.0f, 0.0f, 1.0f },
			{ -1.0f * ar, -1.0f, 1.0f, 0.0f, 0.0f },
			{  1.0f * ar, -1.0f, 1.0f, 1.0f, 0.0f },
			{  1.0f * ar,  1.0f, 1.0f, 1.0f, 1.0f },
		};

		// Vulkan style (y points downwards)
		std::vector<Vertex> verticesYNeg = {
			{ -1.0f * ar, -1.0f, 1.0f, 0.0f, 1.0f },
			{ -1.0f * ar,  1.0f, 1.0f, 0.0f, 0.0f },
			{  1.0f * ar,  1.0f, 1.0f, 1.0f, 0.0f },
			{  1.0f * ar, -1.0f, 1.0f, 1.0f, 1.0f },
		};

		const VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryPropertyFlags, &quad.verticesYUp, sizeof(Vertex) * 4, verticesYPos.data()));
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryPropertyFlags, &quad.verticesYDown,  sizeof(Vertex) * 4, verticesYNeg.data()));

		// [POI] Create two set of indices, one for counter clock wise, and one for clock wise rendering
		std::vector<uint32_t> indices = { 2,1,0, 0,3,2 };
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryPropertyFlags, &quad.indicesCCW, indices.size() * sizeof(uint32_t), indices.data()));
		indices = { 0,1,2, 2,3,0 };
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryPropertyFlags, &quad.indicesCW, indices.size() * sizeof(uint32_t), indices.data()));
	}

	void setupDescriptors()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayoutCI, nullptr, &m_vkDescriptorSetLayout));
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &m_vkPipelineLayout));

		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(1, &poolSize, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolCI, nullptr, &m_vkDescriptorPool));

		VkDescriptorSetAllocateInfo descriptorSetAI = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAI, &descriptorSets.CW));
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAI, &descriptorSets.CCW));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.CW, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.CW.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.CCW, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.CCW.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, 2, &writeDescriptorSets[0], 0, nullptr);
	}

	void preparePipelines()
	{
		if (m_vkPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
		}

		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

		VkPipelineRasterizationStateCreateInfo rasterizationStateCI{};
		rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCI.lineWidth = 1.0f;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE + cullMode;
		rasterizationStateCI.frontFace = windingOrder == 0 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;

		// Vertex bindings and attributes
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX),
		};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),				// Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3),	// uv
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		VkGraphicsPipelineCreateInfo pipelineCreateInfoCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		//pipelineCreateInfoCI.pVertexInputState = &emptyInputState;
		pipelineCreateInfoCI.pVertexInputState = &vertexInputState;
		pipelineCreateInfoCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCreateInfoCI.pRasterizationState = &rasterizationStateCI;
		pipelineCreateInfoCI.pColorBlendState = &colorBlendStateCI;
		pipelineCreateInfoCI.pMultisampleState = &multisampleStateCI;
		pipelineCreateInfoCI.pViewportState = &viewportStateCI;
		pipelineCreateInfoCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCreateInfoCI.pDynamicState = &dynamicStateCI;

		const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
			loadShader(getShadersPath() + "negativeviewportheight/quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getShadersPath() + "negativeviewportheight/quad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		pipelineCreateInfoCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfoCI.pStages = shaderStages.data();

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCreateInfoCI, nullptr, &m_vkPipeline));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		m_vkSubmitInfo.commandBufferCount = 1;
		m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		m_prepared = true;
	}

	virtual void render()
	{
		if (!m_prepared)
			return;
		draw();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Scene")) {
			overlay->text("Quad type");
			if (overlay->comboBox("##quadtype", &quadType, { "VK (y negative)", "GL (y positive)" })) {
				buildCommandBuffers();
			}
		}

		if (overlay->header("Viewport")) {
			if (overlay->checkBox("Negative viewport m_drawAreaHeight", &negativeViewport)) {
				buildCommandBuffers();
			}
			if (overlay->sliderFloat("offset x", &offsetx, -(float)m_drawAreaWidth, (float)m_drawAreaWidth)) {
				buildCommandBuffers();
			}
			if (overlay->sliderFloat("offset y", &offsety, -(float)m_drawAreaHeight, (float)m_drawAreaHeight)) {
				buildCommandBuffers();
			}
		}
		if (overlay->header("Pipeline")) {
			overlay->text("Winding order");
			if (overlay->comboBox("##windingorder", &windingOrder, { "clock wise", "counter clock wise" })) {
				preparePipelines();
			}
			overlay->text("Cull mode");
			if (overlay->comboBox("##cullmode", &cullMode, { "none", "front face", "back face" })) {
				preparePipelines();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()