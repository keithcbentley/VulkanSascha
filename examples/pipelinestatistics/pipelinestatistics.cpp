/*
* Vulkan Example - Retrieving m_vkPipeline statistics
*
* Copyright (C) 2017-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample : public VulkanExampleBase
{
public:
	// This sample lets you select between different models to display
	struct Models {
		std::vector<vkglTF::Model> objects;
		int32_t objectIndex{ 3 };
		std::vector<std::string> names;
	} models;
	// Size for the two-dimensional grid of objects (e.g. 3 = draws 3x3 objects)
	int32_t gridSize{ 3 };

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelview;
		glm::vec4 lightPos{ -10.0f, -10.0f, 10.0f, 1.0f };
	} uniformData;
	vks::Buffer uniformBuffer;

	int32_t cullMode{ VK_CULL_MODE_BACK_BIT };
	bool blending{ false };
	bool discard{ false };
	bool wireframe{ false };
	bool tessellation{ false };

	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	VkQueryPool queryPool{ VK_NULL_HANDLE };

	// Vector for storing m_vkPipeline statistics results
	std::vector<uint64_t> pipelineStats{};
	std::vector<std::string> pipelineStatNames{};

	VulkanExample() : VulkanExampleBase()
	{
		title = "Pipeline statistics";
		camera.type = Camera::CameraType::firstperson;
		camera.setPosition(glm::vec3(-3.0f, 1.0f, -2.75f));
		camera.setRotation(glm::vec3(-15.25f, -46.5f, 0.0f));
		camera.movementSpeed = 4.0f;
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
		camera.rotationSpeed = 0.25f;
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			vkDestroyQueryPool(m_vkDevice, queryPool, nullptr);
			uniformBuffer.destroy();
		}
	}

	virtual void getEnabledFeatures()
	{
		// Support for m_vkPipeline statistics is optional
		if (m_vkPhysicalDeviceFeatures.pipelineStatisticsQuery) {
			m_vkPhysicalDeviceFeatures10.pipelineStatisticsQuery = VK_TRUE;
		}
		else {
			vks::tools::exitFatal("Selected GPU does not support m_vkPipeline statistics!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		if (m_vkPhysicalDeviceFeatures.fillModeNonSolid) {
			m_vkPhysicalDeviceFeatures10.fillModeNonSolid = VK_TRUE;
		}
		if (m_vkPhysicalDeviceFeatures.tessellationShader) {
			m_vkPhysicalDeviceFeatures10.tessellationShader = VK_TRUE;
		}
	}

	// Setup a query pool for storing m_vkPipeline statistics
	void setupQueryPool()
	{
		pipelineStatNames = {
			"Input assembly vertex count        ",
			"Input assembly primitives count    ",
			"Vertex shader invocations          ",
			"Clipping stage primitives processed",
			"Clipping stage primitives output    ",
			"Fragment shader invocations        "
		};
		if (m_vkPhysicalDeviceFeatures.tessellationShader) {
			pipelineStatNames.push_back("Tess. control shader patches       ");
			pipelineStatNames.push_back("Tess. eval. shader invocations     ");
		}
		pipelineStats.resize(pipelineStatNames.size());

		VkQueryPoolCreateInfo queryPoolInfo = {};
		queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		// This query pool will store m_vkPipeline statistics
		queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		// Pipeline counters to be returned for this pool
		queryPoolInfo.pipelineStatistics =
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
			VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
			VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;
		if (m_vkPhysicalDeviceFeatures.tessellationShader) {
			queryPoolInfo.pipelineStatistics |=
				VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
				VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT;
		}
		queryPoolInfo.queryCount = 1;
		VK_CHECK_RESULT(vkCreateQueryPool(m_vkDevice, &queryPoolInfo, NULL, &queryPool));
	}

	// Retrieves the results of the m_vkPipeline statistics query submitted to the command buffer
	void getQueryResults()
	{
		// The size of the data we want to fetch ist based on the count of statistics values
		uint32_t dataSize = static_cast<uint32_t>(pipelineStats.size()) * sizeof(uint64_t);
		// The stride between queries is the no. of unique value entries
		uint32_t stride = static_cast<uint32_t>(pipelineStatNames.size()) * sizeof(uint64_t);
		// Note: for one query both values have the same size, but to make it easier to expand this sample these are properly calculated
		vkGetQueryPoolResults(
			m_vkDevice,
			queryPool,
			0,
			1,
			dataSize,
			pipelineStats.data(),
			stride,
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
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

			// Reset timestamp query pool
			vkCmdResetQueryPool(drawCmdBuffers[i], queryPool, 0, 1);

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth,	(float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offsets[1] = { 0 };

			// Start capture of m_vkPipeline statistics
			vkCmdBeginQuery(drawCmdBuffers[i], queryPool, 0, 0);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.objects[models.objectIndex].vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.objects[models.objectIndex].indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			for (int32_t y = 0; y < gridSize; y++) {
				for (int32_t x = 0; x < gridSize; x++) {
					glm::vec3 pos = glm::vec3(float(x - (gridSize / 2.0f)) * 2.5f, 0.0f, float(y - (gridSize / 2.0f)) * 2.5f);
					vkCmdPushConstants(drawCmdBuffers[i], m_vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
					models.objects[models.objectIndex].draw(drawCmdBuffers[i]);
				}
			}

			// End capture of m_vkPipeline statistics
			vkCmdEndQuery(drawCmdBuffers[i], queryPool, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		// Objects
		std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
		models.names = { "Sphere", "Teapot", "Torusknot", "Venus" };
		models.objects.resize(filenames.size());
		for (size_t i = 0; i < filenames.size(); i++) {
			models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], m_pVulkanDevice, m_vkQueue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY);
		}
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		// Layout
		if (m_vkPipelineLayout == VK_NULL_HANDLE) {
			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
			VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0);
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
			VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));
		}

		// Pipeline
		if (m_vkPipeline != VK_NULL_HANDLE) {
			// Destroy old m_vkPipeline if we're going to recreate it
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
		}

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, cullMode, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);		
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE,	VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
		VkPipelineTessellationStateCreateInfo tessellationState = vks::initializers::pipelineTessellationStateCreateInfo(3);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });

		if (blending) {
			blendAttachmentState.blendEnable = VK_TRUE;
			blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
			depthStencilState.depthWriteEnable = VK_FALSE;
		}

		if (discard) {
			rasterizationState.rasterizerDiscardEnable = VK_TRUE;
		}

		if (wireframe) {
			rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		}

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
		shaderStages.push_back(loadShader(getShadersPath() + "pipelinestatistics/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		if (!discard) {
			// When discard is enabled a m_vkPipeline must not contain a fragment shader
			shaderStages.push_back(loadShader(getShadersPath() + "pipelinestatistics/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		}

		if (tessellation) {
			inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			pipelineCI.pTessellationState = &tessellationState;
			shaderStages.push_back(loadShader(getShadersPath() + "pipelinestatistics/scene.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
			shaderStages.push_back(loadShader(getShadersPath() + "pipelinestatistics/scene.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		}

		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
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
		uniformData.modelview = camera.matrices.view;
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		setupQueryPool();
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

		// Read query results for displaying in next frame
		getQueryResults();
	}

	virtual void render()
	{
		if (!m_prepared)
			return;
		updateUniformBuffers();
		draw();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			if (overlay->comboBox("Object type", &models.objectIndex, models.names)) {
				updateUniformBuffers();
				buildCommandBuffers();
			}
			if (overlay->sliderInt("Grid size", &gridSize, 1, 10)) {
				buildCommandBuffers();
			}
			// To avoid having to create pipelines for all the settings up front, we recreate a single pipelin with different settings instead
			bool recreatePipeline{ false };
			std::vector<std::string> cullModeNames = { "None", "Front", "Back", "Back and front" };
			recreatePipeline |= overlay->comboBox("Cull mode", &cullMode, cullModeNames);
			recreatePipeline |= overlay->checkBox("Blending", &blending);
			recreatePipeline |= overlay->checkBox("Discard", &discard);
			// These m_vkPhysicalDeviceFeatures may not be supported by all implementations
			if (m_vkPhysicalDeviceFeatures.fillModeNonSolid) {
				recreatePipeline |= overlay->checkBox("Wireframe", &wireframe);
			}
			if (m_vkPhysicalDeviceFeatures.tessellationShader) {
				recreatePipeline |= overlay->checkBox("Tessellation", &tessellation);
			}
			if (recreatePipeline) {
				preparePipelines();
				buildCommandBuffers();
			}
		}
		if (!pipelineStats.empty()) {
			if (overlay->header("Pipeline statistics")) {
				for (auto i = 0; i < pipelineStats.size(); i++) {
					std::string caption = pipelineStatNames[i] + ": %d";
					overlay->text(caption.c_str(), pipelineStats[i]);
				}
			}
		}
	}

};

VULKAN_EXAMPLE_MAIN()