/*
* Vulkan Example - Using dynamic state
* 
* This sample demonstrates the use of some of the VK_EXT_dynamic_state extensions
* These allow an application to set some m_vkPipeline related state dynamically at drawtime
* instead of having to pre-bake the state into a m_vkPipeline
* This can help reduce the number of pipelines required
*
* Copyright (C) 2022-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample: public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos{ 0.0f, 2.0f, 1.0f, 0.0f };
	} uniformData;
	vks::Buffer uniformBuffer;

	float clearColor[4] = { 0.0f, 0.0f, 0.2f, 1.0f };

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	// This sample demonstrates different dynamic states, so we check and store what extension is available
	bool hasDynamicState{ false };
	bool hasDynamicState2{ false };
	bool hasDynamicState3{ false };
	bool hasDynamicVertexState{ false };

	VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeaturesEXT{};
	VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extendedDynamicState2FeaturesEXT{};
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynamicState3FeaturesEXT{};

	// Function pointers for dynamic states used in this sample
	// VK_EXT_dynamic_stte
	PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT{ nullptr };
	PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT{ nullptr };
	PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT{ nullptr };
	PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT{ nullptr };
	// VK_EXT_dynamic_state_2
	PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnableEXT{ nullptr };
	// VK_EXT_dynamic_state_3
	PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT{ nullptr };
	PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT{ nullptr };

	// Dynamic state UI toggles
	struct DynamicState {
		int32_t cullMode = VK_CULL_MODE_BACK_BIT;
		int32_t frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		bool depthTest = true;
		bool depthWrite = true;
	} dynamicState;
	struct DynamicState2 {
		bool rasterizerDiscardEnable = false;
	} dynamicState2;
	struct DynamicState3 {
		bool colorBlendEnable = false;
	} dynamicState3;

	VulkanExample() : VulkanExampleBase()
	{
		title = "Dynamic state";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
		camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
		camera.setRotationSpeed(0.5f);
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
		
		// Note: We enable the dynamic state extensions dynamically, based on which ones the m_vkDevice supports see getEnabledExtensions
		m_requestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			uniformBuffer.destroy();
		}
	}

	void getEnabledExtensions()
	{
		// Get the full list of extended dynamic state m_vkPhysicalDeviceFeatures supported by the m_vkDevice
		extendedDynamicStateFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
		extendedDynamicStateFeaturesEXT.pNext = &extendedDynamicState2FeaturesEXT;
		extendedDynamicState2FeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;
		extendedDynamicState2FeaturesEXT.pNext = &extendedDynamicState3FeaturesEXT;
		extendedDynamicState3FeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
		extendedDynamicState3FeaturesEXT.pNext = nullptr;

		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2;
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.pNext = &extendedDynamicStateFeaturesEXT;
		vkGetPhysicalDeviceFeatures2(m_vkPhysicalDevice, &physicalDeviceFeatures2);

		// Check what dynamic states are supported by the current implementation
		// Checking for available m_vkPhysicalDeviceFeatures is probably sufficient, but retained redundant extension checks for clarity and consistency
		hasDynamicState = m_pVulkanDevice->extensionSupported(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) && extendedDynamicStateFeaturesEXT.extendedDynamicState;
		hasDynamicState2 = m_pVulkanDevice->extensionSupported(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME) && extendedDynamicState2FeaturesEXT.extendedDynamicState2;
		hasDynamicState3 = m_pVulkanDevice->extensionSupported(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME) && extendedDynamicState3FeaturesEXT.extendedDynamicState3ColorBlendEnable && extendedDynamicState3FeaturesEXT.extendedDynamicState3ColorBlendEquation;
		hasDynamicVertexState = m_pVulkanDevice->extensionSupported(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);

		// Enable dynamic state extensions if present. This function is called after physical and before logical m_vkDevice creation, so we can enabled extensions based on a list of supported extensions
		if (hasDynamicState) {
			m_requestedDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
			extendedDynamicStateFeaturesEXT.pNext = nullptr;
			m_deviceCreatepNextChain = &extendedDynamicStateFeaturesEXT;
		}
		if (hasDynamicState2) {
			m_requestedDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
			extendedDynamicState2FeaturesEXT.pNext = nullptr;
			if (hasDynamicState) {
				extendedDynamicStateFeaturesEXT.pNext = &extendedDynamicState2FeaturesEXT;
			}
			else {
				m_deviceCreatepNextChain = &extendedDynamicState2FeaturesEXT;
			}
		}
		if (hasDynamicState3) {
			m_requestedDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
			if (hasDynamicState2) {
				extendedDynamicState2FeaturesEXT.pNext = &extendedDynamicState3FeaturesEXT;
			}
			else {
				m_deviceCreatepNextChain = &extendedDynamicState3FeaturesEXT;
			}

		}
		if (hasDynamicVertexState) {
			m_requestedDeviceExtensions.push_back(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { clearColor[0], clearColor[1], clearColor[2], clearColor[3] } };
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

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight,	0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// Apply dynamic states

			if (vkCmdSetCullModeEXT) {
				vkCmdSetCullModeEXT(drawCmdBuffers[i], VkCullModeFlagBits(dynamicState.cullMode));
			}
			if (vkCmdSetFrontFaceEXT) {
				vkCmdSetFrontFaceEXT(drawCmdBuffers[i], VkFrontFace(dynamicState.frontFace));
			}
			if (vkCmdSetDepthTestEnableEXT) {
				vkCmdSetDepthTestEnableEXT(drawCmdBuffers[i], VkFrontFace(dynamicState.depthTest));
			}
			if (vkCmdSetDepthWriteEnableEXT) {
				vkCmdSetDepthWriteEnableEXT(drawCmdBuffers[i], VkFrontFace(dynamicState.depthWrite));
			}

			if (vkCmdSetRasterizerDiscardEnableEXT) {
				vkCmdSetRasterizerDiscardEnableEXT(drawCmdBuffers[i], VkBool32(dynamicState2.rasterizerDiscardEnable));
			}

			if (vkCmdSetColorBlendEnableEXT) {
				const std::vector<VkBool32> blendEnables = { dynamicState3.colorBlendEnable };
				vkCmdSetColorBlendEnableEXT(drawCmdBuffers[i], 0, 1, blendEnables.data());

				VkColorBlendEquationEXT colorBlendEquation{};

				if (dynamicState3.colorBlendEnable) {
					colorBlendEquation.colorBlendOp = VK_BLEND_OP_ADD;
					colorBlendEquation.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
					colorBlendEquation.dstColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
					colorBlendEquation.alphaBlendOp = VK_BLEND_OP_ADD;
					colorBlendEquation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					colorBlendEquation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				}

				vkCmdSetColorBlendEquationEXT(drawCmdBuffers[i], 0, 1, &colorBlendEquation);
			}

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			scene.bindBuffers(drawCmdBuffers[i]);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
			scene.draw(drawCmdBuffers[i]);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		// Instead of having to create a m_vkPipeline for each state combination, we only create one m_vkPipeline and toggle the new dynamic states during command buffer creation
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// All dynamic states we want to use need to be enabled at m_vkPipeline creation
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, };
		if (hasDynamicState) {
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_CULL_MODE_EXT);
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_FRONT_FACE_EXT);
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT);
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT);
		}
		if (hasDynamicState2) {
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT);
		}
		if (hasDynamicState3) {
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
		}

		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

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
		pipelineCI.pVertexInputState  = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});

		// Create the graphics m_vkPipeline state objects

		shaderStages[0] = loadShader(getShadersPath() + "pipelines/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pipelines/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Create the vertex shader uniform buffer block
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
		VK_CHECK_RESULT(uniformBuffer.map());
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view;
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(uniformData));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		// Dynamic states are set with vkCmd* calls in the command buffer, so we need to load the function pointers depending on extension supports
		if (hasDynamicState) {
			vkCmdSetCullModeEXT = reinterpret_cast<PFN_vkCmdSetCullModeEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetCullModeEXT"));
			vkCmdSetFrontFaceEXT = reinterpret_cast<PFN_vkCmdSetFrontFaceEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetFrontFaceEXT"));
			vkCmdSetDepthWriteEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthWriteEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetDepthWriteEnableEXT"));
			vkCmdSetDepthTestEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthTestEnable>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetDepthTestEnableEXT"));
		}

		if (hasDynamicState2) {
			vkCmdSetRasterizerDiscardEnableEXT = reinterpret_cast<PFN_vkCmdSetRasterizerDiscardEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetRasterizerDiscardEnableEXT"));
		}

		if (hasDynamicState3) {
			vkCmdSetColorBlendEnableEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetColorBlendEnableEXT"));
			vkCmdSetColorBlendEquationEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetColorBlendEquationEXT"));
		}

		loadAssets();
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
		updateUniformBuffers();
		draw();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		bool rebuildCB = false;		
		if (overlay->header("Dynamic state")) {
			if (hasDynamicState) {
				rebuildCB = overlay->comboBox("Cull mode", &dynamicState.cullMode, { "none", "front", "back" });
				rebuildCB |= overlay->comboBox("Front face", &dynamicState.frontFace, { "Counter clockwise", "Clockwise" });
				rebuildCB |= overlay->checkBox("Depth test", &dynamicState.depthTest);
				rebuildCB |= overlay->checkBox("Depth write", &dynamicState.depthWrite);
			} else {
				overlay->text("Extension or m_vkPhysicalDeviceFeatures not supported");
			}
		}
		if (overlay->header("Dynamic state 2")) {
			if (hasDynamicState2) {
				rebuildCB |= overlay->checkBox("Rasterizer discard", &dynamicState2.rasterizerDiscardEnable);
			}
			else {
				overlay->text("Extension or m_vkPhysicalDeviceFeatures not supported");
			}
		}
		if (overlay->header("Dynamic state 3")) {
			if (hasDynamicState3) {
				rebuildCB |= overlay->checkBox("Color blend", &dynamicState3.colorBlendEnable);
				rebuildCB |= overlay->colorPicker("Clear color", clearColor);
			}
			else {
				overlay->text("Extension or m_vkPhysicalDeviceFeatures not supported");
			}
		}
		if (rebuildCB) {
			buildCommandBuffers();
		}
	}
};

VULKAN_EXAMPLE_MAIN()
