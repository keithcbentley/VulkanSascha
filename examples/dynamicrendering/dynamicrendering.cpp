/*
 * Vulkan Example - Using VK_KHR_dynamic_rendering for rendering without framebuffers and render passes
 *
 * Copyright (C) 2022-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"


class VulkanExample : public VulkanExampleBase
{
public:
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR{ VK_NULL_HANDLE };
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR{ VK_NULL_HANDLE };

	VkPhysicalDeviceDynamicRenderingFeaturesKHR enabledDynamicRenderingFeaturesKHR{};

	vkglTF::Model model;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 viewPos;
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Dynamic rendering";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.0f));
		camera.setRotation(glm::vec3(-7.5f, 72.0f, 0.0f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);

		m_requestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		// The sample uses the extension (instead of Vulkan 1.2, where dynamic rendering is core)
		m_requestedDeviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);

		// in addition to the extension, the feature needs to be explicitly enabled too by chaining the extension structure into m_vkDevice creation
		enabledDynamicRenderingFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
		enabledDynamicRenderingFeaturesKHR.dynamicRendering = VK_TRUE;

		m_deviceCreatepNextChain = &enabledDynamicRenderingFeaturesKHR;
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

	void setupRenderPass()
	{
		// With VK_KHR_dynamic_rendering we no longer need a render pass, so skip the sample base render pass setup
		m_vkRenderPass = VK_NULL_HANDLE;
	}

	void setupFrameBuffer()
	{
		// With VK_KHR_dynamic_rendering we no longer need a frame buffer, so skip the sample base framebuffer setup
	}

	// Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
	virtual void getEnabledFeatures()
	{
		// Enable anisotropic filtering if supported
		if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
			m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
		};
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		model.loadFromFile(getAssetPath() + "models/voyager.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			// With dynamic rendering there are no subpass dependencies, so we need to take care of proper layout transitions by using barriers
			// This set of barriers prepares the color and depth images for output
			vks::tools::insertImageMemoryBarrier(
				drawCmdBuffers[i],
				m_swapChain.images[i],
				0,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
			vks::tools::insertImageMemoryBarrier(
				drawCmdBuffers[i],
				m_defaultDepthStencil.m_vkImage,
				0,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

			// New structures are used to define the attachments used in dynamic rendering
			VkRenderingAttachmentInfoKHR colorAttachment{};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			colorAttachment.imageView = m_swapChain.imageViews[i];
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

			// A single depth stencil attachment info can be used, but they can also be specified separately.
			// When both are specified separately, the only requirement is that the m_vkImage m_vkImageView is identical.			
			VkRenderingAttachmentInfoKHR depthStencilAttachment{};
			depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			depthStencilAttachment.imageView = m_defaultDepthStencil.m_vkImageView;
			depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };

			VkRenderingInfoKHR renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
			renderingInfo.renderArea = { 0, 0, m_drawAreaWidth, m_drawAreaHeight };
			renderingInfo.layerCount = 1;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;
			renderingInfo.pDepthAttachment = &depthStencilAttachment;
			renderingInfo.pStencilAttachment = &depthStencilAttachment;

			// Begin dynamic rendering
			vkCmdBeginRenderingKHR(drawCmdBuffers[i], &renderingInfo);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);

			model.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, m_vkPipelineLayout);
			
			drawUI(drawCmdBuffers[i]);

			// End dynamic rendering
			vkCmdEndRenderingKHR(drawCmdBuffers[i]);

			// This set of barriers prepares the color m_vkImage for presentation, we don't need to care for the depth m_vkImage
			vks::tools::insertImageMemoryBarrier(
				drawCmdBuffers[i],
				m_swapChain.images[i],
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				0,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

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
		const std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));
		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		// Uses set 0 for passing vertex shader ubo and set 1 for fragment shader images (taken from glTF model)
		const std::vector<VkDescriptorSetLayout> setLayouts = {
			m_vkDescriptorSetLayout,
			vkglTF::descriptorSetLayoutImage,
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
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
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

		// We no longer need to set a renderpass for the m_vkPipeline create info
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo();
		pipelineCI.layout = m_vkPipelineLayout;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		// New create info to define color, depth and stencil attachments at m_vkPipeline create time
		VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
		pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		pipelineRenderingCreateInfo.colorAttachmentCount = 1;
		pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChain.colorFormat;
		pipelineRenderingCreateInfo.depthAttachmentFormat = m_vkFormatDepth;
		pipelineRenderingCreateInfo.stencilAttachmentFormat = m_vkFormatDepth;
		// Chain into the m_vkPipeline creat einfo
		pipelineCI.pNext = &pipelineRenderingCreateInfo;

		shaderStages[0] = loadShader(getShadersPath() + "dynamicrendering/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "dynamicrendering/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformData), &uniformData));
		VK_CHECK_RESULT(uniformBuffer.map());
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view;
		uniformData.viewPos = camera.viewPos;
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(uniformData));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		// Since we use an extension, we need to expliclity load the function pointers for extension related Vulkan commands
		vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdBeginRenderingKHR"));
		vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdEndRenderingKHR"));

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
};

VULKAN_EXAMPLE_MAIN()
