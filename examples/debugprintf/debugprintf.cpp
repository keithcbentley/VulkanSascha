/*
* Vulkan Example - Example for using printf in shaders to help debugging. Can be used in conjunction with a debugging app like RenderDoc (https://renderdoc.org)
* 
* See this whitepaper for details: https://www.lunarg.com/wp-content/uploads/2021/08/Using-Debug-Printf-02August2021.pdf
*
* Copyright (C) 2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/*
* The only change required for printf in shaders on the application is enabling the VK_KHR_shader_non_semantic_info extensions
* The actual printing is done in the shaders (see toon.vert from the glsl/hlsl) folder
* For glsl shaders that use this feature, the GL_EXT_debug_printf extension needs to be enabled
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"


class VulkanExample : public VulkanExampleBase
{
public:
	vks::Buffer uniformBuffer;
	vkglTF::Model scene;

	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(0.0f, 5.0f, 15.0f, 1.0f);
	} uboVS;

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Debug output with shader printf";
		camera.setRotation(glm::vec3(-4.35f, 16.25f, 0.0f));
		camera.setRotationSpeed(0.5f);
		camera.setPosition(glm::vec3(0.1f, 1.1f, -8.5f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);

		// Using printf requires the non semantic info extension to be enabled
		m_requestedDeviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);

#if (defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT)) && defined(VK_EXAMPLE_XCODE_GENERATED)
		// SRS - Force validation on since shader printf provided by VK_LAYER_KHRONOS_validation on macOS
		settings.validation = true;

		// Use layer settings extension to configure Validation Layer
		m_requestedInstanceExtensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);

		// SRS - Enable the Validation Layer's printf feature
		VkLayerSettingEXT layerSetting;
		layerSetting.pLayerName = "VK_LAYER_KHRONOS_validation";
		layerSetting.pSettingName = "enables";
		layerSetting.type = VK_LAYER_SETTING_TYPE_STRING_EXT;
		layerSetting.valueCount = 1;

		// Make static so layer setting reference remains valid after leaving constructor scope
		static const char *layerEnables = "VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT";
		layerSetting.pValues = &layerEnables;
		m_requestedLayerSettings.push_back(layerSetting);

		// SRS - RenderDoc not available on macOS so redirect printf output to stdout
		layerSetting.pSettingName = "printf_to_stdout";
		layerSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
		layerSetting.valueCount = 1;

		// Make static so layer setting reference remains valid after leaving constructor scope
		static const VkBool32 layerSettingOn = VK_TRUE;
		layerSetting.pValues = &layerSettingOn;
		m_requestedLayerSettings.push_back(layerSetting);

		// Enable required m_vkPhysicalDeviceFeatures and set API version for Validation Layer printf
		m_vkPhysicalDeviceFeaturesEnabled.fragmentStoresAndAtomics = VK_TRUE;
		m_vkPhysicalDeviceFeaturesEnabled.vertexPipelineStoresAndAtomics = VK_TRUE;

		m_requestedApiVersion = VK_API_VERSION_1_1;
#endif
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			uniformBuffer.destroy();
		}
	}

	void loadAssets()
	{
		scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", m_pVulkanDevice, m_vkQueue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VkClearValue clearValues[2]{};

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			clearValues[0].color = m_vkClearColorValueDefault;
			clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = m_vkRenderPass;
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];
			renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
			renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
			scene.draw(drawCmdBuffers[i]);
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
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Toon shading m_vkPipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR	};
		VkPipelineDynamicStateCreateInfo dynamicStateCI =	vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
			loadShader(getShadersPath() + "debugprintf/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getShadersPath() + "debugprintf/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color});
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uboVS)));
		VK_CHECK_RESULT(uniformBuffer.map());
	}

	void updateUniformBuffers()
	{
		uboVS.projection = camera.matrices.perspective;
		uboVS.model = camera.matrices.view;
		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
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
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		m_prepared = true;
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
		if (overlay->header("Info")) {
			overlay->text("Please run this sample with a graphics debugger attached");
		}
	}
};

VULKAN_EXAMPLE_MAIN()
