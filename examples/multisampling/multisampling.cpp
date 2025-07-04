/*
* Vulkan Example - Multisampling using resolve attachments (MSAA)
* 
* This sample shows how to do multisampled anti aliasing using built-in hardware via resolve attachments
* These are special attachments that a multi-sampled is resolved to using a fixed sample pattern
*
* Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample : public VulkanExampleBase
{
public:
	bool useSampleShading = false;
	VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

	vkglTF::Model model;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(5.0f, -5.0f, 5.0f, 1.0f);
	} uniformData;
	vks::Buffer uniformBuffer;

	struct {
		VkPipeline MSAA{ VK_NULL_HANDLE };
		VkPipeline MSAASampleShading{ VK_NULL_HANDLE };
	} pipelines;

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };
	VkExtent2D attachmentSize{};

	// Holds the Vulkan resources required for the final multi sample output target
	struct MultiSampleTarget {
		struct {
			VkImage image{ VK_NULL_HANDLE };
			VkImageView view{ VK_NULL_HANDLE };
			VkDeviceMemory memory{ VK_NULL_HANDLE };
		} color;
		struct {
			VkImage image{ VK_NULL_HANDLE };
			VkImageView view{ VK_NULL_HANDLE };
			VkDeviceMemory memory{ VK_NULL_HANDLE };
		} depth;
	} multisampleTarget;

	VulkanExample() : VulkanExampleBase()
	{
		title = "Multisampling";
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
		camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
		camera.setTranslation(glm::vec3(2.5f, 2.5f, -7.5f));
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipeline(m_vkDevice, pipelines.MSAA, nullptr);
			vkDestroyPipeline(m_vkDevice, pipelines.MSAASampleShading, nullptr);

			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);

			// Destroy MSAA target
			vkDestroyImage(m_vkDevice, multisampleTarget.color.image, nullptr);
			vkDestroyImageView(m_vkDevice, multisampleTarget.color.view, nullptr);
			vkFreeMemory(m_vkDevice, multisampleTarget.color.memory, nullptr);
			vkDestroyImage(m_vkDevice, multisampleTarget.depth.image, nullptr);
			vkDestroyImageView(m_vkDevice, multisampleTarget.depth.view, nullptr);
			vkFreeMemory(m_vkDevice, multisampleTarget.depth.memory, nullptr);

			uniformBuffer.destroy();
		}
	}

	// Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
	virtual void getEnabledFeatures()
	{
		// Enable sample rate shading filtering if supported
		if (m_vkPhysicalDeviceFeatures.sampleRateShading) {
			m_vkPhysicalDeviceFeatures10.sampleRateShading = VK_TRUE;
		}
		// Enable anisotropic filtering if supported
		if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
			m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
		}
	}

	// Creates a multi sample render target (m_vkImage and m_vkImageView) that is used to resolve
	// into the visible frame buffer target in the render pass
	void setupMultisampleTarget()
	{
		// Check if m_vkDevice supports requested sample count for color and depth frame buffer
		assert((m_vkPhysicalDeviceProperties.limits.framebufferColorSampleCounts & sampleCount) && (m_vkPhysicalDeviceProperties.limits.framebufferDepthSampleCounts & sampleCount));

		// Color target
		VkImageCreateInfo info = vks::initializers::imageCreateInfo();
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = m_swapChain.colorFormat;
		info.extent.width = m_drawAreaWidth;
		info.extent.height = m_drawAreaHeight;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.samples = sampleCount;
		// Image will only be used as a transient target
		info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &info, nullptr, &multisampleTarget.color.image));

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(m_vkDevice, multisampleTarget.color.image, &memReqs);
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		// We prefer a lazily allocated m_vkDeviceMemory type
		// This means that the m_vkDeviceMemory gets allocated when the implementation sees fit, e.g. when first using the images
		VkBool32 lazyMemTypePresent;
		memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &lazyMemTypePresent);
		if (!lazyMemTypePresent)
		{
			// If this is not available, fall back to m_vkDevice local m_vkDeviceMemory
			memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, &multisampleTarget.color.memory));
		vkBindImageMemory(m_vkDevice, multisampleTarget.color.image, multisampleTarget.color.memory, 0);

		// Create m_vkImage m_vkImageView for the MSAA target
		VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
		viewInfo.image = multisampleTarget.color.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_swapChain.colorFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &multisampleTarget.color.view));

		// Depth target
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = m_vkFormatDepth;
		info.extent.width = m_drawAreaWidth;
		info.extent.height = m_drawAreaHeight;
		info.extent.depth = 1;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.samples = sampleCount;
		// Image will only be used as a transient target
		info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &info, nullptr, &multisampleTarget.depth.image));

		vkGetImageMemoryRequirements(m_vkDevice, multisampleTarget.depth.image, &memReqs);
		memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;

		memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &lazyMemTypePresent);
		if (!lazyMemTypePresent)
		{
			memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}

		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, &multisampleTarget.depth.memory));
		vkBindImageMemory(m_vkDevice, multisampleTarget.depth.image, multisampleTarget.depth.memory, 0);

		// Create m_vkImage m_vkImageView for the MSAA target
		viewInfo.image = multisampleTarget.depth.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_vkFormatDepth;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (m_vkFormatDepth >= VK_FORMAT_D16_UNORM_S8_UINT)
			viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &multisampleTarget.depth.view));
	}

	// Setup a render pass for using a multi sampled attachment
	// and a resolve attachment that the msaa m_vkImage is resolved
	// to at the end of the render pass
	void setupRenderPass()
	{
		// Overrides the virtual function of the base class
		
		attachmentSize = { m_drawAreaWidth, m_drawAreaHeight };

		std::array<VkAttachmentDescription, 3> attachments = {};

		// Multisampled attachment that we render to
		attachments[0].format = m_swapChain.colorFormat;
		attachments[0].samples = sampleCount;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// This is the frame buffer attachment to where the multisampled m_vkImage
		// will be resolved to and which will be presented to the swapchain
		attachments[1].format = m_swapChain.colorFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Multisampled depth attachment we render to
		attachments[2].format = m_vkFormatDepth;
		attachments[2].samples = sampleCount;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 2;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Resolve attachment reference for the color attachment
		VkAttachmentReference resolveReference = {};
		resolveReference.attachment = 1;
		resolveReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		// Pass our resolve attachments to the sub pass
		subpass.pResolveAttachments = &resolveReference;
		subpass.pDepthStencilAttachment = &depthReference;

		std::array<VkSubpassDependency, 2> dependencies{};

		// Depth attachment
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		dependencies[0].dependencyFlags = 0;
		// Color attachment
		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = 0;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask = 0;
		dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		dependencies[1].dependencyFlags = 0;

		VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(m_vkDevice, &renderPassInfo, nullptr, &m_vkRenderPass));
	}

	// Frame buffer attachments must match with render pass setup,
	// so we need to adjust frame buffer creation to cover our
	// multisample target
	void setupFrameBuffer()
	{
		// Overrides the virtual function of the base class

		// SRS - If the m_hwnd is m_resized, the MSAA attachments need to be released and recreated
		if (attachmentSize.width != m_drawAreaWidth || attachmentSize.height != m_drawAreaHeight)
		{
			attachmentSize = { m_drawAreaWidth, m_drawAreaHeight };
			
			// Destroy MSAA target
			vkDestroyImage(m_vkDevice, multisampleTarget.color.image, nullptr);
			vkDestroyImageView(m_vkDevice, multisampleTarget.color.view, nullptr);
			vkFreeMemory(m_vkDevice, multisampleTarget.color.memory, nullptr);
			vkDestroyImage(m_vkDevice, multisampleTarget.depth.image, nullptr);
			vkDestroyImageView(m_vkDevice, multisampleTarget.depth.view, nullptr);
			vkFreeMemory(m_vkDevice, multisampleTarget.depth.memory, nullptr);
		}
		
		std::array<VkImageView, 3> attachments;

		setupMultisampleTarget();

		attachments[0] = multisampleTarget.color.view;
		// attachment[1] = swapchain m_vkImage
		attachments[2] = multisampleTarget.depth.view;

		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = NULL;
		frameBufferCreateInfo.renderPass = m_vkRenderPass;
		frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		frameBufferCreateInfo.pAttachments = attachments.data();
		frameBufferCreateInfo.width = m_drawAreaWidth;
		frameBufferCreateInfo.height = m_drawAreaHeight;
		frameBufferCreateInfo.layers = 1;

		// Create frame buffers for every swap chain m_vkImage
		m_vkFrameBuffers.resize(m_swapChain.images.size());
		for (uint32_t i = 0; i < m_vkFrameBuffers.size(); i++)
		{
			attachments[1] = m_swapChain.imageViews[i];
			VK_CHECK_RESULT(vkCreateFramebuffer(m_vkDevice, &frameBufferCreateInfo, nullptr, &m_vkFrameBuffers[i]));
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[3];
		// Clear to a white background for higher contrast
		clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		clearValues[1].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
		clearValues[2].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = m_vkRenderPass;
		renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
		renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
		renderPassBeginInfo.clearValueCount = 3;
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
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, useSampleShading ? pipelines.MSAASampleShading : pipelines.MSAA);
			model.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, m_vkPipelineLayout);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		model.loadFromFile(getAssetPath() + "models/voyager.gltf", m_pVulkanDevice, m_vkQueue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY);
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
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
		// Layout uses set 0 for passing vertex shader ubo and set 1 for fragment shader images (taken from glTF model)
		const std::vector<VkDescriptorSetLayout> setLayouts = {
			m_vkDescriptorSetLayout,
			vkglTF::descriptorSetLayoutImage,
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Setup multi sampling
		VkPipelineMultisampleStateCreateInfo multisampleState{};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		// Number of samples to use for rasterization
		multisampleState.rasterizationSamples = sampleCount;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color });

		// MSAA rendering pipeline
		shaderStages[0] = loadShader(getShadersPath() + "multisampling/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "multisampling/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.MSAA));

		if (m_pVulkanDevice->m_vkPhysicalDeviceFeatures.sampleRateShading)
		{
			// MSAA with sample shading pipeline
			// Sample shading enables per-sample shading to avoid shader aliasing and smooth out e.g. high frequency texture maps
			// Note: This will trade performance for are more stable m_vkImage
			
			// Enable per-sample shading (instead of per-fragment)
			multisampleState.sampleShadingEnable = VK_TRUE;
			// Minimum fraction for sample shading
			multisampleState.minSampleShading = 0.25f;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.MSAASampleShading));
		}
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
		// Map persistent
		VK_CHECK_RESULT(uniformBuffer.map());
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.model = camera.matrices.view;
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	}

	// Select the highest sample count usable by the platform
	// In a realworld application, this would be a user setting instead
	VkSampleCountFlagBits getMaxAvailableSampleCount()
	{
		VkSampleCountFlags supportedSampleCount = std::min(m_vkPhysicalDeviceProperties.limits.framebufferColorSampleCounts, m_vkPhysicalDeviceProperties.limits.framebufferDepthSampleCounts);
		std::vector< VkSampleCountFlagBits> possibleSampleCounts {
			VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT
		};
		for (auto& possibleSampleCount : possibleSampleCounts) {
			if (supportedSampleCount & possibleSampleCount) {
				return possibleSampleCount;
			}
		}
		return VK_SAMPLE_COUNT_1_BIT;
	}

	void prepare()
	{
		sampleCount = getMaxAvailableSampleCount();
		m_UIOverlay.rasterizationSamples = sampleCount;
		VulkanExampleBase::prepare();
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
		if (m_pVulkanDevice->m_vkPhysicalDeviceFeatures.sampleRateShading) {
			if (overlay->header("Settings")) {
				if (overlay->checkBox("Sample rate shading", &useSampleShading)) {
					buildCommandBuffers();
				}
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
