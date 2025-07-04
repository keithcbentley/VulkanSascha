/*
* Vulkan Example - Conditional rendering
*
* Note: Requires a m_vkDevice that supports the VK_EXT_conditional_rendering extension
*
* With conditional rendering it's possible to execute certain rendering commands based on a buffer value instead of having to rebuild the command buffers.
* This example sets up a conditional buffer with one value per glTF part, that is used to toggle visibility of single model parts.
*
* Copyright (C) 2018-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample : public VulkanExampleBase
{
public:
	PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT{ VK_NULL_HANDLE };
	PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT{ VK_NULL_HANDLE };

	vkglTF::Model scene;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
	} uniformData;
	vks::Buffer uniformBuffer;

	std::vector<int32_t> conditionalVisibility{};
	vks::Buffer conditionalBuffer;

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Conditional rendering";
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(45.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(-2.25f, -52.0f, 0.0f));
		camera.setTranslation(glm::vec3(1.9f, -2.05f, -18.0f));
		camera.rotationSpeed *= 0.25f;

		/*
			[POI] Enable extension required for conditional rendering
		*/
		m_requestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			uniformBuffer.destroy();
			conditionalBuffer.destroy();
		}
	}

	void renderNode(vkglTF::Node *node, VkCommandBuffer commandBuffer) {
		if (node->mesh) {
			for (vkglTF::Primitive * primitive : node->mesh->primitives) {
				const std::vector<VkDescriptorSet> descriptorsets = {
					descriptorSet,
					node->mesh->uniformBuffer.descriptorSet
				};
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, static_cast<uint32_t>(descriptorsets.size()), descriptorsets.data(), 0, NULL);

				vkCmdPushConstants(commandBuffer, m_vkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(primitive->material.baseColorFactor), &primitive->material.baseColorFactor);

				/*
					[POI] Setup the conditional rendering
				*/
				VkConditionalRenderingBeginInfoEXT conditionalRenderingBeginInfo{};
				conditionalRenderingBeginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
				conditionalRenderingBeginInfo.buffer = conditionalBuffer.buffer;
				conditionalRenderingBeginInfo.offset = sizeof(int32_t) * node->index;

				/*
					[POI] Begin conditionally rendered section

					If the value from the conditional rendering buffer at the given offset is != 0, the draw commands will be executed
				*/
				vkCmdBeginConditionalRenderingEXT(commandBuffer, &conditionalRenderingBeginInfo);

				vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);

				vkCmdEndConditionalRenderingEXT(commandBuffer);
			}

		};
		for (auto child : node->children) {
			renderNode(child, commandBuffer);
		}
	}


	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
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

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);

			const VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &scene.vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], scene.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			for (auto node : scene.nodes) {
				renderNode(node, drawCmdBuffers[i]);
			}

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		scene.loadFromFile(getAssetPath() + "models/gltf/glTF-Embedded/Buggy.gltf", m_pVulkanDevice, m_vkQueue);
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		};
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolCI, nullptr, &m_vkDescriptorPool));

		// Layouts
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
		descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorLayoutCI.pBindings = setLayoutBindings.data();
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayoutCI, nullptr, &m_vkDescriptorSetLayout));

		// Sets
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &descriptorSetAllocateInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		std::array<VkDescriptorSetLayout, 2> setLayouts = {
			m_vkDescriptorSetLayout, vkglTF::descriptorSetLayoutUbo
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec4), 0);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables, 0);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
			loadShader(getShadersPath() + "conditionalrender/model.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getShadersPath() + "conditionalrender/model.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffer,
			sizeof(UniformData)));
		VK_CHECK_RESULT(uniformBuffer.map());
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = glm::scale(camera.matrices.view, glm::vec3(0.1f , -0.1f, 0.1f));
		uniformData.model = glm::translate(glm::mat4(1.0f), scene.dimensions.min);
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	}

	void updateConditionalBuffer()
	{
		memcpy(conditionalBuffer.mapped, conditionalVisibility.data(), sizeof(int32_t) * conditionalVisibility.size());
	}

	/*
		[POI] Extension specific setup

		Gets the function pointers required for conditional rendering
		Sets up a dedicated conditional buffer that is used to determine visibility at draw time
	*/
	void prepareConditionalRendering()
	{
		/*
			The conditional rendering functions are part of an extension so they have to be loaded manually
		*/
		vkCmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)vkGetDeviceProcAddr(m_vkDevice, "vkCmdBeginConditionalRenderingEXT");
		if (!vkCmdBeginConditionalRenderingEXT) {
			vks::tools::exitFatal("Could not get a valid function pointer for vkCmdBeginConditionalRenderingEXT", -1);
		}

		vkCmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)vkGetDeviceProcAddr(m_vkDevice, "vkCmdEndConditionalRenderingEXT");
		if (!vkCmdEndConditionalRenderingEXT) {
			vks::tools::exitFatal("Could not get a valid function pointer for vkCmdEndConditionalRenderingEXT", -1);
		}

		/*
			Create the buffer that contains the conditional rendering information

			A single conditional value is 32 bits and if it's zero the rendering commands are discarded
			This sample renders multiple rows of objects conditionally, so we setup a buffer with one value per row
		*/
		conditionalVisibility.resize(scene.linearNodes.size());
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&conditionalBuffer,
			sizeof(int32_t) *conditionalVisibility.size(),
			conditionalVisibility.data()));
		VK_CHECK_RESULT(conditionalBuffer.map());

		// By default, all parts of the glTF are visible
		for (auto i = 0; i < conditionalVisibility.size(); i++) {
			conditionalVisibility[i] = 1;
		}

		/*
			Copy visibility data
		*/
		updateConditionalBuffer();
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
		prepareConditionalRendering();
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
		if (overlay->header("Visibility")) {

			if (overlay->button("All")) {
				for (auto i = 0; i < conditionalVisibility.size(); i++) {
					conditionalVisibility[i] = 1;
				}
				updateConditionalBuffer();
			}
			ImGui::SameLine();
			if (overlay->button("None")) {
				for (auto i = 0; i < conditionalVisibility.size(); i++) {
					conditionalVisibility[i] = 0;
				}
				updateConditionalBuffer();
			}
			ImGui::NewLine();

			ImGui::BeginChild("InnerRegion", ImVec2(200.0f * overlay->scale, 400.0f * overlay->scale), false);
			for (auto node : scene.linearNodes) {
				// Add visibility toggle checkboxes for all model nodes with a mesh
				if (node->mesh) {
					if (overlay->checkBox(("[" + std::to_string(node->index) + "] " + node->mesh->name).c_str(), &conditionalVisibility[node->index])) {
						updateConditionalBuffer();
					}
				}
			}
			ImGui::EndChild();

		}
	}

};

VULKAN_EXAMPLE_MAIN()
