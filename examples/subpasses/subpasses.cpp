/*
 * Vulkan Example - Using subpasses for G-Buffer compositing
 *
 * Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Summary:
 * Implements a deferred rendering setup with a forward transparency pass using sub passes
 *
 * Sub passes allow reading from the previous framebuffer (in the same render pass) at
 * the same pixel position.
 *
 * This is a feature that was especially designed for tile-based-renderers
 * (mostly mobile GPUs) and is a new optimization feature in Vulkan for those GPU types.
 *
 */

#include "VulkanglTFModel.h"
#include "vulkanexamplebase.h"

vkcpp::VulkanContext vkcpp::s_vulkanContext;

class VulkanExample : public VulkanExampleBase {
public:
    vks::Texture2D m_textureGlass;

    vkglTF::Model m_modelScene;
    vkglTF::Model m_modelTransparent;

    struct {
        glm::mat4 projection;
        glm::mat4 model;
        glm::mat4 view;
    } m_uboGBuffer;

    struct Light {
        glm::vec4 position;
        glm::vec3 color;
        float radius;
    };

    std::array<Light, 64> m_lights;

    vks::Buffer m_bufferGBuffer;
    vks::Buffer m_bufferLights;

    vkcpp::PipelineLayout m_pipelineLayoutOffscreen;
    vkcpp::PipelineLayout m_pipelineLayoutComposition;
    vkcpp::PipelineLayout m_pipelineLayoutTransparent;

    VkPipeline m_vkPipelineOffscreen;
    VkPipeline m_vkPipelineComposition;
    VkPipeline m_vkPipelineTransparent;

    VkDescriptorSetLayout m_vkDescriptorSetLayoutScene;
    VkDescriptorSetLayout m_vkDescriptorSetLayoutComposition;
    VkDescriptorSetLayout m_vkDescriptorSetLayoutTransparent;

    VkDescriptorSet m_vkDescriptorSetScene;
    VkDescriptorSet m_vkDescriptorSetComposition;
    VkDescriptorSet m_vkDescriptorSetTransparent;

    // G-Buffer framebuffer attachments
    struct FrameBufferAttachment {
        VkImage m_vkImage = VK_NULL_HANDLE;
        VkDeviceMemory m_vkDeviceMemory = VK_NULL_HANDLE;
        VkImageView m_vkImageView = VK_NULL_HANDLE;
        VkFormat m_vkFormat;
    };

    struct Attachments {
        FrameBufferAttachment m_position;
        FrameBufferAttachment m_normal;
        FrameBufferAttachment m_albedo;
        int32_t width=0;
        int32_t height=0;
    } m_attachments;

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Subpasses";
        camera.type = Camera::CameraType::firstperson;
        camera.movementSpeed = 5.0f;
#ifndef __ANDROID__
        camera.rotationSpeed = 0.25f;
#endif
        camera.setPosition(glm::vec3(-3.2f, 1.0f, 5.9f));
        camera.setRotation(glm::vec3(0.5f, 210.05f, 0.0f));
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
        m_UIOverlay.subpass = 2;

        //	TODO: need to check if this is available.
        // m_vkPhysicalDeviceFeatures10.fragmentStoresAndAtomics = VK_TRUE;
    }

    ~VulkanExample()
    {
        // Clean up used Vulkan resources
        // Note : Inherited destructor cleans up resources stored in base class
        vkDestroyPipeline(m_device, m_vkPipelineOffscreen, nullptr);
        vkDestroyPipeline(m_device, m_vkPipelineComposition, nullptr);
        vkDestroyPipeline(m_device, m_vkPipelineTransparent, nullptr);

//        vkDestroyPipelineLayout(m_device, m_vkPipelineLayoutOffscreen, nullptr);
//        vkDestroyPipelineLayout(m_device, m_vkPipelineLayoutComposition, nullptr);
//        vkDestroyPipelineLayout(m_device, m_vkPipelineLayoutTransparent, nullptr);

        vkDestroyDescriptorSetLayout(m_device, m_vkDescriptorSetLayoutScene, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_vkDescriptorSetLayoutComposition, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_vkDescriptorSetLayoutTransparent, nullptr);

        clearAttachment(&m_attachments.m_position);
        clearAttachment(&m_attachments.m_normal);
        clearAttachment(&m_attachments.m_albedo);

        m_textureGlass.destroy();
        m_bufferGBuffer.destroy();
        m_bufferLights.destroy();
    }

    // Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
    virtual void getEnabledFeatures() {
        //// Enable anisotropic filtering if supported
        // if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
        //	m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
        // }
    };

    void clearAttachment(FrameBufferAttachment* attachment)
    {
        vkDestroyImageView(m_device, attachment->m_vkImageView, nullptr);
        vkDestroyImage(m_device, attachment->m_vkImage, nullptr);
        vkFreeMemory(m_device, attachment->m_vkDeviceMemory, nullptr);
    }

    // Create a frame buffer attachment
    void createAttachment(VkFormat format, VkImageUsageFlags usage, FrameBufferAttachment* attachment)
    {
        if (attachment->m_vkImage != VK_NULL_HANDLE) {
            clearAttachment(attachment);
        }

        VkImageAspectFlags aspectMask = 0;

        attachment->m_vkFormat = format;

        if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        assert(aspectMask > 0);

        VkImageCreateInfo image = vks::initializers::imageCreateInfo();
        image.imageType = VK_IMAGE_TYPE_2D;
        image.format = format;
        image.extent.width = m_attachments.width;
        image.extent.height = m_attachments.height;
        image.extent.depth = 1;
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        // VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT flag is required for input attachments
        image.usage = usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;

        VK_CHECK_RESULT(vkCreateImage(m_device, &image, nullptr, &attachment->m_vkImage));
        vkGetImageMemoryRequirements(m_device, attachment->m_vkImage, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &attachment->m_vkDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(m_device, attachment->m_vkImage, attachment->m_vkDeviceMemory, 0));

        VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
        imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageView.format = format;
        imageView.subresourceRange = {};
        imageView.subresourceRange.aspectMask = aspectMask;
        imageView.subresourceRange.baseMipLevel = 0;
        imageView.subresourceRange.levelCount = 1;
        imageView.subresourceRange.baseArrayLayer = 0;
        imageView.subresourceRange.layerCount = 1;
        imageView.image = attachment->m_vkImage;
        VK_CHECK_RESULT(vkCreateImageView(m_device, &imageView, nullptr, &attachment->m_vkImageView));
    }

    // Create color attachments for the G-Buffer components
    void createGBufferAttachments()
    {
        createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &m_attachments.m_position); // (World space) Positions
        createAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &m_attachments.m_normal); // (World space) Normals
        createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &m_attachments.m_albedo); // Albedo (color)
    }

    // Override framebuffer setup from base class, will automatically be called upon setup and if a m_hwnd is m_resized
    void setupFrameBuffer()
    {
        //	If the m_hwnd is m_resized, all the framebuffers/attachments used in our composition passes need to be recreated
        if (m_attachments.width != m_drawAreaWidth || m_attachments.height != m_drawAreaHeight) {
            m_attachments.width = m_drawAreaWidth;
            m_attachments.height = m_drawAreaHeight;
            createGBufferAttachments();
            // Since the framebuffers/attachments are referred in the descriptor sets, these need to be updated too
            // Composition pass
            std::vector<VkDescriptorImageInfo> descriptorImageInfos = {
                vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, m_attachments.m_position.m_vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, m_attachments.m_normal.m_vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, m_attachments.m_albedo.m_vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            };
            std::vector<VkWriteDescriptorSet> writeDescriptorSets;
            for (size_t i = 0; i < descriptorImageInfos.size(); i++) {
                writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(m_vkDescriptorSetComposition, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, static_cast<uint32_t>(i), &descriptorImageInfos[i]));
            }
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
            // Forward pass
            writeDescriptorSets = {
                vks::initializers::writeDescriptorSet(m_vkDescriptorSetTransparent, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, &descriptorImageInfos[0]),
            };
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
        }

        VkImageView attachments[5];

        VkFramebufferCreateInfo frameBufferCreateInfo = {};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = m_renderPassOriginal;
        frameBufferCreateInfo.attachmentCount = 5;
        frameBufferCreateInfo.pAttachments = attachments;
        frameBufferCreateInfo.width = m_drawAreaWidth;
        frameBufferCreateInfo.height = m_drawAreaHeight;
        frameBufferCreateInfo.layers = 1;

        // Create frame buffers for every swap chain m_vkImage
        m_vkFrameBuffers.resize(m_swapChain.images.size());
        for (uint32_t i = 0; i < m_vkFrameBuffers.size(); i++) {
            attachments[0] = m_swapChain.imageViews[i];
            attachments[1] = this->m_attachments.m_position.m_vkImageView;
            attachments[2] = this->m_attachments.m_normal.m_vkImageView;
            attachments[3] = this->m_attachments.m_albedo.m_vkImageView;
            attachments[4] = m_defaultDepthStencil.m_imageView;
            VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_vkFrameBuffers[i]));
        }
    }

    // Override render pass setup from base class
    void setupRenderPass()
    {
        m_attachments.width = m_drawAreaWidth;
        m_attachments.height = m_drawAreaHeight;

        createGBufferAttachments();

        std::array<VkAttachmentDescription, 5> attachments {};
        // Color attachment
        attachments[0].format = m_swapChain.colorFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Deferred attachments
        // Position
        attachments[1].format = this->m_attachments.m_position.m_vkFormat;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // Normals
        attachments[2].format = this->m_attachments.m_normal.m_vkFormat;
        attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // Albedo
        attachments[3].format = this->m_attachments.m_albedo.m_vkFormat;
        attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // Depth attachment
        attachments[4].format = m_vkFormatDepth;
        attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Three subpasses
        std::array<VkSubpassDescription, 3> subpassDescriptions {};

        // First subpass: Fill G-Buffer components
        // ----------------------------------------------------------------------------------------

        VkAttachmentReference colorReferences[4];
        colorReferences[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        colorReferences[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        colorReferences[2] = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        colorReferences[3] = { 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthReference = { 4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescriptions[0].colorAttachmentCount = 4;
        subpassDescriptions[0].pColorAttachments = colorReferences;
        subpassDescriptions[0].pDepthStencilAttachment = &depthReference;

        // Second subpass: Final composition (using G-Buffer components)
        // ----------------------------------------------------------------------------------------

        VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkAttachmentReference inputReferences[3];
        inputReferences[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        inputReferences[1] = { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        inputReferences[2] = { 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescriptions[1].colorAttachmentCount = 1;
        subpassDescriptions[1].pColorAttachments = &colorReference;
        subpassDescriptions[1].pDepthStencilAttachment = &depthReference;
        // Use the color attachments filled in the first pass as input attachments
        subpassDescriptions[1].inputAttachmentCount = 3;
        subpassDescriptions[1].pInputAttachments = inputReferences;

        // Third subpass: Forward transparency
        // ----------------------------------------------------------------------------------------
        colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        inputReferences[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        subpassDescriptions[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescriptions[2].colorAttachmentCount = 1;
        subpassDescriptions[2].pColorAttachments = &colorReference;
        subpassDescriptions[2].pDepthStencilAttachment = &depthReference;
        // Use the color/depth attachments filled in the first pass as input attachments
        subpassDescriptions[2].inputAttachmentCount = 1;
        subpassDescriptions[2].pInputAttachments = inputReferences;

        // Subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 5> dependencies;

        // This makes sure that writes to the depth m_vkImage are done before we try to write to it again
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        ;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        ;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = 0;

        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].dstSubpass = 0;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].srcAccessMask = 0;
        dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dependencyFlags = 0;

        // This dependency transitions the input attachment from color attachment to input attachment read
        dependencies[2].srcSubpass = 0;
        dependencies[2].dstSubpass = 1;
        dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[2].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[3].srcSubpass = 1;
        dependencies[3].dstSubpass = 2;
        dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[3].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[3].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[4].srcSubpass = 2;
        dependencies[4].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[4].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[4].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[4].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[4].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[4].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
        renderPassInfo.pSubpasses = subpassDescriptions.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        m_renderPassOriginal = vkcpp::RenderPass(renderPassInfo);
        // VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_vkRenderPass));
    }

    void buildCommandBuffers()
    {
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

        VkClearValue clearValues[5];
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[4].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
        renderPassBeginInfo.renderPass = m_renderPassOriginal;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
        renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
        renderPassBeginInfo.clearValueCount = 5;
        renderPassBeginInfo.pClearValues = clearValues;

        for (int32_t i = 0; i < m_drawCommandBuffers.size(); ++i) {
			vkcpp::CommandBuffer commandBuffer = vkcpp::CommandBuffer::makeCopy(m_drawCommandBuffers[i]);
            // Set target frame buffer
            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			commandBuffer.begin();
			commandBuffer.cmdBeginRenderPass(renderPassBeginInfo);
			commandBuffer.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight);
			commandBuffer.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight);

            // First sub pass
            // Renders the components of the scene to the G-Buffer attachments
            {
                vks::debugutils::cmdBeginLabel(commandBuffer, "Subpass 0: Deferred G-Buffer creation", { 1.0f, 0.78f, 0.05f, 1.0f });
				commandBuffer.cmdBindPipeline(m_vkPipelineOffscreen);
				commandBuffer.cmdBindDescriptorSet(m_vkDescriptorSetScene, m_pipelineLayoutOffscreen);
                m_modelScene.draw(commandBuffer);
                vks::debugutils::cmdEndLabel(commandBuffer);
            }

            // Second sub pass
            // This subpass will use the G-Buffer components that have been filled in the first subpass as input attachment for the final compositing
            {
                vks::debugutils::cmdBeginLabel(commandBuffer, "Subpass 1: Deferred composition", { 0.0f, 0.5f, 1.0f, 1.0f });
				commandBuffer.cmdNextSubpass();
				commandBuffer.cmdBindPipeline(m_vkPipelineComposition);
				commandBuffer.cmdBindDescriptorSet(m_vkDescriptorSetComposition, m_pipelineLayoutComposition);
				//	TODO: magic numbers
				commandBuffer.cmdDraw(3, 1);	//	vertexCount, indexCount
                vks::debugutils::cmdEndLabel(commandBuffer);
            }

            // Third subpass
            // Render transparent geometry using a forward pass that compares against depth generated during G-Buffer fill
            {
                vks::debugutils::cmdBeginLabel(m_drawCommandBuffers[i], "Subpass 2: Forward transparency", { 0.5f, 0.76f, 0.34f, 1.0f });
				commandBuffer.cmdNextSubpass();
				commandBuffer.cmdBindPipeline(m_vkPipelineTransparent);
				commandBuffer.cmdBindDescriptorSet(m_vkDescriptorSetTransparent, m_pipelineLayoutTransparent);
                m_modelTransparent.draw(commandBuffer);
                vks::debugutils::cmdEndLabel(commandBuffer);
            }

            drawUI(commandBuffer);

			commandBuffer.cmdEndRenderPass();
			commandBuffer.end();
        }
    }

    void loadAssets()
    {
        const uint32_t glTFLoadingFlags
            = vkglTF::FileLoadingFlags::PreTransformVertices
            | vkglTF::FileLoadingFlags::PreMultiplyVertexColors
            | vkglTF::FileLoadingFlags::FlipY;
        m_modelScene.loadFromFile(
            getAssetPath() + "models/samplebuilding.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
        m_modelTransparent.loadFromFile(
            getAssetPath() + "models/samplebuilding_glass.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
        m_textureGlass.loadFromFile(
            getAssetPath() + "textures/colored_glass_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_queue);
    }

    void setupDescriptors()
    {
        // Pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4),
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4),
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo
            = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 4);
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolInfo);
        // VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_vkDescriptorSetLayoutScene));

        // Sets
        VkDescriptorSetAllocateInfo allocInfo
            = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayoutScene, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_vkDescriptorSetScene));
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // Binding 0: Vertex shader uniform buffer
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetScene, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_bufferGBuffer.m_vkDescriptorBufferInfo)
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    }

    void preparePipelines()
    {
        // Layout
        //VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
        //    = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayoutScene, 1);
        //VK_CHECK_RESULT(vkCreatePipelineLayout(
        //    m_device, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayoutOffscreen));

		vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.addDescriptorSetLayout(m_vkDescriptorSetLayoutScene);
		m_pipelineLayoutOffscreen = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

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

        // Final fullscreen pass pipeline
        VkGraphicsPipelineCreateInfo pipelineCI
            = vks::initializers::pipelineCreateInfo(m_pipelineLayoutOffscreen, m_renderPassOriginal, 0);
        pipelineCI.pInputAssemblyState = &inputAssemblyState;
        pipelineCI.pRasterizationState = &rasterizationState;
        pipelineCI.pColorBlendState = &colorBlendState;
        pipelineCI.pMultisampleState = &multisampleState;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pDepthStencilState = &depthStencilState;
        pipelineCI.pDynamicState = &dynamicState;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.subpass = 0;
        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

        std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachmentStates = {
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };

        colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
        colorBlendState.pAttachments = blendAttachmentStates.data();

        // Offscreen scene rendering pipeline
        shaderStages[0] = loadShader(getShadersPath() + "subpasses/gbuffer.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "subpasses/gbuffer.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(
            vkCreateGraphicsPipelines(m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipelineOffscreen));
    }

    // Create the Vulkan objects used in the composition pass (descriptor sets, pipelines, etc.)
    void prepareCompositionPass()
    {
        // Descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0: Position input attachment
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            // Binding 1: Normal input attachment
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            // Binding 2: Albedo input attachment
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
            // Binding 3: Light positions
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        };

        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(
            setLayoutBindings.data(),
            static_cast<uint32_t>(setLayoutBindings.size()));

        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            m_device, &descriptorLayout, nullptr, &m_vkDescriptorSetLayoutComposition));

        // Pipeline Layout
		{
			vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
			pipelineLayoutCreateInfo.addDescriptorSetLayout(m_vkDescriptorSetLayoutComposition);
			m_pipelineLayoutComposition = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);
		}

        // Descriptor sets
        VkDescriptorSetAllocateInfo allocInfo
            = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayoutComposition, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_vkDescriptorSetComposition));

        // Image descriptors for the offscreen color attachments
        VkDescriptorImageInfo texDescriptorPosition
            = vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, m_attachments.m_position.m_vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageInfo texDescriptorNormal
            = vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, m_attachments.m_normal.m_vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageInfo texDescriptorAlbedo
            = vks::initializers::descriptorImageInfo(VK_NULL_HANDLE, m_attachments.m_albedo.m_vkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // Binding 0: Position texture target
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetComposition, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0, &texDescriptorPosition),
            // Binding 1: Normals texture target
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetComposition, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, &texDescriptorNormal),
            // Binding 2: Albedo texture target
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetComposition, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, &texDescriptorAlbedo),
            // Binding 4: Fragment shader lights
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetComposition, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &m_bufferLights.m_vkDescriptorBufferInfo),
        };

        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

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
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        shaderStages[0] = loadShader(getShadersPath() + "subpasses/composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "subpasses/composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        VkGraphicsPipelineCreateInfo pipelineCI
            = vks::initializers::pipelineCreateInfo(m_pipelineLayoutComposition, m_renderPassOriginal, 0);

        VkPipelineVertexInputStateCreateInfo emptyInputState {};
        emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        pipelineCI.pVertexInputState = &emptyInputState;
        pipelineCI.pInputAssemblyState = &inputAssemblyState;
        pipelineCI.pRasterizationState = &rasterizationState;
        pipelineCI.pColorBlendState = &colorBlendState;
        pipelineCI.pMultisampleState = &multisampleState;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pDepthStencilState = &depthStencilState;
        pipelineCI.pDynamicState = &dynamicState;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        // Index of the subpass that this pipeline will be used in
        pipelineCI.subpass = 1;

        depthStencilState.depthWriteEnable = VK_FALSE;

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(
            m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipelineComposition));

        // Transparent (forward) pipeline

        // Descriptor set layout
        setLayoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        };

        descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            m_device, &descriptorLayout, nullptr, &m_vkDescriptorSetLayoutTransparent));

        // Pipeline Layout
		{
			vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
			pipelineLayoutCreateInfo.addDescriptorSetLayout(m_vkDescriptorSetLayoutTransparent);
			m_pipelineLayoutTransparent = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);
		}

        // Descriptor sets
        allocInfo = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayoutTransparent, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_vkDescriptorSetTransparent));

        writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetTransparent, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &m_bufferGBuffer.m_vkDescriptorBufferInfo),
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetTransparent, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, &texDescriptorPosition),
            vks::initializers::writeDescriptorSet(
				m_vkDescriptorSetTransparent, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &m_textureGlass.m_vkDescriptorImageInfo),
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

        // Enable blending
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });
        pipelineCI.layout = m_pipelineLayoutTransparent;
        pipelineCI.subpass = 2;

        shaderStages[0] = loadShader(getShadersPath() + "subpasses/transparent.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "subpasses/transparent.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(
            m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipelineTransparent));
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        // Matrices
        m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &m_bufferGBuffer,
            sizeof(m_uboGBuffer));
        VK_CHECK_RESULT(m_bufferGBuffer.map());

        // Lights
        m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &m_bufferLights,
            m_lights.size() * sizeof(Light));
        VK_CHECK_RESULT(m_bufferLights.map());

        // Update
        updateUniformBufferDeferredMatrices();
    }

    void updateUniformBufferDeferredMatrices()
    {
        m_uboGBuffer.projection = camera.matrices.perspective;
        m_uboGBuffer.view = camera.matrices.view;
        m_uboGBuffer.model = glm::mat4(1.0f);
        memcpy(m_bufferGBuffer.m_pMapped, &m_uboGBuffer, sizeof(m_uboGBuffer));
    }

    void initLights()
    {
        std::vector<glm::vec3> colors = {
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(1.0f, 1.0f, 0.0f),
        };

        std::random_device rndDevice;
        std::default_random_engine rndGen(m_benchmark.active ? 0 : rndDevice());
        std::uniform_real_distribution<float> rndDist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> rndCol(0.0f, 0.5f);

        for (auto& light : m_lights) {
            light.position = glm::vec4(rndDist(rndGen) * 8.0f, 0.25f + std::abs(rndDist(rndGen)) * 4.0f, rndDist(rndGen) * 8.0f, 1.0f);
            // light.color = colors[rndCol(rndGen)];
            light.color = glm::vec3(rndCol(rndGen), rndCol(rndGen), rndCol(rndGen)) * 2.0f;
            light.radius = 1.0f + std::abs(rndDist(rndGen));
        }

        memcpy(m_bufferLights.m_pMapped, m_lights.data(), m_lights.size() * sizeof(Light));
    }

    void draw()
    {
        VulkanExampleBase::prepareFrame();

        // Command buffer to be submitted to the m_vkQueue
        m_vkSubmitInfo.commandBufferCount = 1;
        VkCommandBuffer vkCommandBuffer = m_drawCommandBuffers[m_currentBufferIndex];
        m_vkSubmitInfo.pCommandBuffers = &vkCommandBuffer;

        // Submit to m_vkQueue
        VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));

        VulkanExampleBase::submitFrame();
    }

    void prepare()
    {
        VulkanExampleBase::prepare();
        loadAssets();
        prepareUniformBuffers();
        initLights();
        setupDescriptors();
        preparePipelines();
        prepareCompositionPass();
        buildCommandBuffers();
        m_prepared = true;
    }

    virtual void render()
    {
        if (!m_prepared)
            return;
        if (camera.updated) {
            updateUniformBufferDeferredMatrices();
        }
        draw();
    }

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
    {
        if (overlay->header("Subpasses")) {
            overlay->text("0: Deferred G-Buffer creation");
            overlay->text("1: Deferred composition");
            overlay->text("2: Forward transparency");
        }
        if (overlay->header("Settings")) {
            if (overlay->button("Randomize lights")) {
                initLights();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
