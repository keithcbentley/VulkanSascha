/*
 * Vulkan Example - Using input attachments
 *
 * Copyright (C) 2018-2024 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Summary:
 * Input attachments can be used to read attachment contents from a previous sub pass
 * at the same pixel position within a single render pass
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	struct UBOMatrices {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
	} uboMatrices;

	struct UBOParams {
		glm::vec2 brightnessContrast = glm::vec2(0.5f, 1.8f);
		glm::vec2 range = glm::vec2(0.6f, 1.0f);
		int32_t attachmentIndex = 1;
	} uboParams;

	struct {
		vks::Buffer matrices;
		vks::Buffer params;
	} uniformBuffers;

	struct {
		VkPipeline attachmentWrite{ VK_NULL_HANDLE };
		VkPipeline attachmentRead{ VK_NULL_HANDLE };
	} pipelines;

	struct {
		VkPipelineLayout attachmentWrite{ VK_NULL_HANDLE };
		VkPipelineLayout attachmentRead{ VK_NULL_HANDLE };
	} pipelineLayouts;

	struct {
		VkDescriptorSet attachmentWrite{ VK_NULL_HANDLE };
		std::vector<VkDescriptorSet> attachmentRead{ VK_NULL_HANDLE };
	} descriptorSets;

	struct {
		VkDescriptorSetLayout attachmentWrite{ VK_NULL_HANDLE };
		VkDescriptorSetLayout attachmentRead{ VK_NULL_HANDLE };
	} descriptorSetLayouts;

	struct FrameBufferAttachment {
		VkImage image{ VK_NULL_HANDLE };
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkImageView view{ VK_NULL_HANDLE };
		VkFormat format;
	};
	struct Attachments {
		FrameBufferAttachment color, depth;
	};
	std::vector<Attachments> attachments;
	VkExtent2D attachmentSize{};

	const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

	VulkanExample() : VulkanExampleBase()
	{
		title = "Input attachments";
		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 2.5f;
		camera.setPosition(glm::vec3(1.65f, 1.75f, -6.15f));
		camera.setRotation(glm::vec3(-12.75f, 380.0f, 0.0f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
		m_UIOverlay.subpass = 1;
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			for (uint32_t i = 0; i < attachments.size(); i++) {
				vkDestroyImageView(m_vkDevice, attachments[i].color.view, nullptr);
				vkDestroyImage(m_vkDevice, attachments[i].color.image, nullptr);
				vkFreeMemory(m_vkDevice, attachments[i].color.memory, nullptr);
				vkDestroyImageView(m_vkDevice, attachments[i].depth.view, nullptr);
				vkDestroyImage(m_vkDevice, attachments[i].depth.image, nullptr);
				vkFreeMemory(m_vkDevice, attachments[i].depth.memory, nullptr);
			}

			vkDestroyPipeline(m_vkDevice, pipelines.attachmentRead, nullptr);
			vkDestroyPipeline(m_vkDevice, pipelines.attachmentWrite, nullptr);

			vkDestroyPipelineLayout(m_vkDevice, pipelineLayouts.attachmentWrite, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, pipelineLayouts.attachmentRead, nullptr);

			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayouts.attachmentWrite, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayouts.attachmentRead, nullptr);

			uniformBuffers.matrices.destroy();
			uniformBuffers.params.destroy();
		}
	}

	void clearAttachment(FrameBufferAttachment* attachment)
	{
		vkDestroyImageView(m_vkDevice, attachment->view, nullptr);
		vkDestroyImage(m_vkDevice, attachment->image, nullptr);
		vkFreeMemory(m_vkDevice, attachment->memory, nullptr);
	}

	// Create a frame buffer attachment
	void createAttachment(VkFormat format, VkImageUsageFlags usage, FrameBufferAttachment *attachment)
	{
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = m_drawAreaWidth;
		imageCI.extent.height = m_drawAreaHeight;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		// VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT flag is required for input attachments;
		imageCI.usage = usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &imageCI, nullptr, &attachment->image));

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(m_vkDevice, attachment->image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, &attachment->memory));
		VK_CHECK_RESULT(vkBindImageMemory(m_vkDevice, attachment->image, attachment->memory, 0));

		VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.format = format;
		imageViewCI.subresourceRange = {};
		imageViewCI.subresourceRange.aspectMask = aspectMask;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.image = attachment->image;
		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &imageViewCI, nullptr, &attachment->view));
	}

	// Override framebuffer setup from base class
	void setupFrameBuffer()
	{
		// If the m_hwnd is m_resized, all the framebuffers/attachments used in our composition passes need to be recreated
		if (attachmentSize.width != m_drawAreaWidth || attachmentSize.height != m_drawAreaHeight)
		{
			attachmentSize = { m_drawAreaWidth, m_drawAreaHeight };

			for (auto i = 0; i < attachments.size(); i++) {
				clearAttachment(&attachments[i].color);
				clearAttachment(&attachments[i].depth);
			}

			// SRS - Recreate attachments and descriptors in case number of swapchain images has changed on resize
			attachments.resize(m_swapChain.images.size());
			for (auto i = 0; i < attachments.size(); i++) {
				createAttachment(colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &attachments[i].color);
				createAttachment(m_vkFormatDepth, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &attachments[i].depth);
			}

			vkDestroyPipelineLayout(m_vkDevice, pipelineLayouts.attachmentWrite, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, pipelineLayouts.attachmentRead, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayouts.attachmentWrite, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayouts.attachmentRead, nullptr);
			vkDestroyDescriptorPool(m_vkDevice, m_vkDescriptorPool, nullptr);

			// Since the framebuffers/attachments are referred in the descriptor sets, these need to be updated on resize
			setupDescriptors();
		}

		VkImageView views[3];

		VkFramebufferCreateInfo frameBufferCI{};
		frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCI.renderPass = m_vkRenderPass;
		frameBufferCI.attachmentCount = 3;
		frameBufferCI.pAttachments = views;
		frameBufferCI.width = m_drawAreaWidth;
		frameBufferCI.height = m_drawAreaHeight;
		frameBufferCI.layers = 1;

		m_vkFrameBuffers.resize(m_swapChain.images.size());
		for (uint32_t i = 0; i < m_vkFrameBuffers.size(); i++)
		{
			views[0] = m_swapChain.imageViews[i];
			views[1] = attachments[i].color.view;
			views[2] = attachments[i].depth.view;
			VK_CHECK_RESULT(vkCreateFramebuffer(m_vkDevice, &frameBufferCI, nullptr, &m_vkFrameBuffers[i]));
		}
	}

	// Override render pass setup from base class
	void setupRenderPass()
	{
		attachmentSize = { m_drawAreaWidth, m_drawAreaHeight };		

		attachments.resize(m_swapChain.images.size());
		for (auto i = 0; i < attachments.size(); i++) {
			createAttachment(colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &attachments[i].color);
			createAttachment(m_vkFormatDepth, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &attachments[i].depth);
		}

		std::array<VkAttachmentDescription, 3> attachments{};

		// Swap chain m_vkImage color attachment
		// Will be transitioned to present layout
		attachments[0].format = m_swapChain.colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Input attachments
		// These will be written in the first subpass, transitioned to input attachments
		// and then read in the secod subpass

		// Color
		attachments[1].format = colorFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// Depth
		attachments[2].format = m_vkFormatDepth;
		attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::array<VkSubpassDescription,2> subpassDescriptions{};

		/*
			First subpass
			Fill the color and depth attachments
		*/
		VkAttachmentReference colorReference = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[0].colorAttachmentCount = 1;
		subpassDescriptions[0].pColorAttachments = &colorReference;
		subpassDescriptions[0].pDepthStencilAttachment = &depthReference;

		/*
			Second subpass
			Input attachment read and swap chain color attachment write
		*/

		// Color reference (target) for this sub pass is the swap chain color attachment
		VkAttachmentReference colorReferenceSwapchain = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[1].colorAttachmentCount = 1;
		subpassDescriptions[1].pColorAttachments = &colorReferenceSwapchain;

		// Color and depth attachment written to in first sub pass will be used as input attachments to be read in the fragment shader
		VkAttachmentReference inputReferences[2];
		inputReferences[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		inputReferences[1] = { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		// Use the attachments filled in the first pass as input attachments
		subpassDescriptions[1].inputAttachmentCount = 2;
		subpassDescriptions[1].pInputAttachments = inputReferences;

		/*
			Subpass dependencies for layout transitions
		*/
		std::array<VkSubpassDependency, 3> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// This dependency transitions the input attachment from color attachment to shader read
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = 1;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[2].srcSubpass = 0;
		dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfoCI{};
		renderPassInfoCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfoCI.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfoCI.pAttachments = attachments.data();
		renderPassInfoCI.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
		renderPassInfoCI.pSubpasses = subpassDescriptions.data();
		renderPassInfoCI.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfoCI.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(m_vkDevice, &renderPassInfoCI, nullptr, &m_vkRenderPass));
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[3];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
		clearValues[2].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = m_vkRenderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
		renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
		renderPassBeginInfo.clearValueCount = 3;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			/*
				First sub pass
				Fills the attachments
			*/
			{
				vks::debugutils::cmdBeginLabel(drawCmdBuffers[i], "Subpass 0: Writing attachments", { 1.0f, 0.78f, 0.05f, 1.0f });

				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.attachmentWrite);
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.attachmentWrite, 0, 1, &descriptorSets.attachmentWrite, 0, NULL);
				scene.draw(drawCmdBuffers[i]);

				vks::debugutils::cmdEndLabel(drawCmdBuffers[i]);
			}

			/*
				Second sub pass
				Render a full screen quad, reading from the previously written attachments via input attachments
			*/
			{
				vks::debugutils::cmdBeginLabel(drawCmdBuffers[i], "Subpass 1: Reading attachments", { 0.0f, 0.5f, 1.0f, 1.0f });

				vkCmdNextSubpass(drawCmdBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.attachmentRead);
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.attachmentRead, 0, 1, &descriptorSets.attachmentRead[i], 0, NULL);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				vks::debugutils::cmdEndLabel(drawCmdBuffers[i]);
			}

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

	void updateAttachmentReadDescriptors(uint32_t index)
	{
		// Image descriptors for the input attachments read by the shader
		std::vector<VkDescriptorImageInfo> descriptors = {
			vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, attachments[index].color.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, attachments[index].depth.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: Color input attachment
			vks::initializers::writeDescriptorSet(descriptorSets.attachmentRead[index], VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0, &descriptors[0]),
			// Binding 1: Depth input attachment
			vks::initializers::writeDescriptorSet(descriptorSets.attachmentRead[index], VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, &descriptors[1]),
			// Binding 2: Display parameters uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSets.attachmentRead[index], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &uniformBuffers.params.descriptor),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void setupDescriptors()
	{
		/*
			Pool
		*/
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(attachments.size()) + 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(attachments.size()) + 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, static_cast<uint32_t>(attachments.size()) * 2 + 1),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), static_cast<uint32_t>(attachments.size()) + 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		/*
			Attachment write
		*/
		{
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
			};
			VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &descriptorSetLayouts.attachmentWrite));

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.attachmentWrite, 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.attachmentWrite));

			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &descriptorSetLayouts.attachmentWrite, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.attachmentWrite));

			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSets.attachmentWrite, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.matrices.descriptor);
			vkUpdateDescriptorSets(m_vkDevice, 1, &writeDescriptorSet, 0, nullptr);
		}

		/*
			Attachment read
		*/
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0: Color input attachment
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			// Binding 1: Depth input attachment
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			// Binding 2: Display parameters uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayouts.attachmentRead));

		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.attachmentRead, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &pipelineLayouts.attachmentRead));

		descriptorSets.attachmentRead.resize(attachments.size());
		for (auto i = 0; i < descriptorSets.attachmentRead.size(); i++) {
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &descriptorSetLayouts.attachmentRead, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.attachmentRead[i]));
			updateAttachmentReadDescriptors(i);
		}

	}

	void preparePipelines()
	{
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo();

		pipelineCI.renderPass = m_vkRenderPass;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

		/*
			Attachment write
		*/

		// Pipeline will be used in first sub pass
		pipelineCI.subpass = 0;
		pipelineCI.layout = pipelineLayouts.attachmentWrite;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal});

		shaderStages[0] = loadShader(getShadersPath() + "inputattachments/attachmentwrite.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "inputattachments/attachmentwrite.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.attachmentWrite));

		/*
			Attachment read
		*/

		// Pipeline will be used in second sub pass
		pipelineCI.subpass = 1;
		pipelineCI.layout = pipelineLayouts.attachmentRead;

		VkPipelineVertexInputStateCreateInfo emptyInputStateCI{};
		emptyInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		pipelineCI.pVertexInputState = &emptyInputStateCI;
		colorBlendStateCI.attachmentCount = 1;
		rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		depthStencilStateCI.depthWriteEnable = VK_FALSE;

		shaderStages[0] = loadShader(getShadersPath() + "inputattachments/attachmentread.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "inputattachments/attachmentread.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.attachmentRead));
	}

	void prepareUniformBuffers()
	{
		m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.matrices, sizeof(uboMatrices));
		m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.params, sizeof(uboParams));
		VK_CHECK_RESULT(uniformBuffers.matrices.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uboMatrices.projection = camera.matrices.perspective;
		uboMatrices.view = camera.matrices.view;
		uboMatrices.model = glm::mat4(1.0f);
		memcpy(uniformBuffers.matrices.mapped, &uboMatrices, sizeof(uboMatrices));
		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
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
		if (overlay->header("Settings")) {
			overlay->text("Input attachment");
			if (overlay->comboBox("##attachment", &uboParams.attachmentIndex, { "color", "depth" })) {
				updateUniformBuffers();
			}
			switch (uboParams.attachmentIndex) {
			case 0:
				overlay->text("Brightness");
				if (overlay->sliderFloat("##b", &uboParams.brightnessContrast[0], 0.0f, 2.0f)) {
					updateUniformBuffers();
				}
				overlay->text("Contrast");
				if (overlay->sliderFloat("##c", &uboParams.brightnessContrast[1], 0.0f, 4.0f)) {
					updateUniformBuffers();
				}
				break;
			case 1:
				overlay->text("Visible range");
				if (overlay->sliderFloat("min", &uboParams.range[0], 0.0f, uboParams.range[1])) {
					updateUniformBuffers();
				}
				if (overlay->sliderFloat("max", &uboParams.range[1], uboParams.range[0], 1.0f)) {
					updateUniformBuffers();
				}
				break;
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
