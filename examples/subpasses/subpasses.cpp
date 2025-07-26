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

    vkcpp::GraphicsPipeline m_pipelineOffscreen;
    vkcpp::GraphicsPipeline m_pipelineComposition;
    vkcpp::GraphicsPipeline m_pipelineTransparent;

    vkcpp::DescriptorSetLayout m_descriptorSetLayoutScene;
    vkcpp::DescriptorSetLayout m_descriptorSetLayoutComposition;
    vkcpp::DescriptorSetLayout m_descriptorSetLayoutTransparent;

    vkcpp::DescriptorSet m_descriptorSetScene;
    vkcpp::DescriptorSet m_descriptorSetComposition;
    vkcpp::DescriptorSet m_descriptorSetTransparent;

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
        int32_t width = 0;
        int32_t height = 0;
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

        clearAttachment(&m_attachments.m_position);
        clearAttachment(&m_attachments.m_normal);
        clearAttachment(&m_attachments.m_albedo);

        m_textureGlass.destroy();
    }

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
        memAlloc.memoryTypeIndex
			= vkcpp::findMemoryTypeIndex(
				memReqs.memoryTypeBits, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);
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

    // Override framebuffer setup from base class,
    // will automatically be called upon setup and if a m_hwnd is m_resized
    void setupFrameBuffer() override
    {
        //	If the m_hwnd is resized, all the framebuffers/attachments used
        //	in our composition passes need to be recreated
        //	TODO: this is poor practice.  It replicates some of the code that
        //	is used to create some of the descriptors initially. If these sizes,
        //	or whatever, are incorrect, the bindings should be updated using
        //	the initial code, and then this function called again.
        if (m_attachments.width != m_drawAreaWidth || m_attachments.height != m_drawAreaHeight) {
            m_attachments.width = m_drawAreaWidth;
            m_attachments.height = m_drawAreaHeight;
            createGBufferAttachments();

            // Since the framebuffers/attachments are referred in the descriptor sets, these need to be updated also

            constexpr int positionBindingIndex = 0;
            constexpr int normalBindingIndex = 1;
            constexpr int albedoBindingIndex = 2;

            // Composition pass
            {
                vkcpp::DescriptorSetUpdater descriptorSetUpdater(m_descriptorSetComposition);
                descriptorSetUpdater
                    .addImageWriteDescriptor(
                        positionBindingIndex,
                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                        m_attachments.m_position.m_vkImageView,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_NULL_HANDLE)
                    .addImageWriteDescriptor(
                        normalBindingIndex,
                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                        m_attachments.m_normal.m_vkImageView,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_NULL_HANDLE)
                    .addImageWriteDescriptor(
                        albedoBindingIndex,
                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                        m_attachments.m_albedo.m_vkImageView,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_NULL_HANDLE);
                descriptorSetUpdater.updateDescriptorSets();
            }
            {
                // Forward pass
                vkcpp::DescriptorSetUpdater descriptorSetUpdater(m_descriptorSetTransparent);
                descriptorSetUpdater
                    .addImageWriteDescriptor(
                        positionBindingIndex,
                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                        m_attachments.m_position.m_vkImageView,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_NULL_HANDLE);
                descriptorSetUpdater.updateDescriptorSets();
            }
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
            attachments[1] = m_attachments.m_position.m_vkImageView;
            attachments[2] = m_attachments.m_normal.m_vkImageView;
            attachments[3] = m_attachments.m_albedo.m_vkImageView;
            attachments[4] = m_defaultDepthStencil.m_imageView;
            VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_vkFrameBuffers[i]));
        }
    }

    // Override render pass setup from base class
    void setupRenderPass() override
    {
        m_attachments.width = m_drawAreaWidth;
        m_attachments.height = m_drawAreaHeight;

        createGBufferAttachments();

        constexpr int subpassCount = 3;
        constexpr int subpassFillG = 0;
        constexpr int subpassFinalComposition = 1;
        constexpr int subpassForwardTransparency = 2;

        constexpr int attachmentCount = 5;
        constexpr int colorPresentAttachmentIndex = 0;
        constexpr int positionAttachmentIndex = 1;
        constexpr int normalAttachmentIndex = 2;
        constexpr int albedoAttachmentIndex = 3;
        constexpr int depthAttachmentIndex = 4;

        vkcpp::RenderPassCreateInfo renderPassCreateInfo(subpassCount, attachmentCount);

        //	Attach attachments.
        renderPassCreateInfo.attachmentDescription(colorPresentAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleColorPresent(m_swapChain.colorFormat);

        renderPassCreateInfo.attachmentDescription(positionAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleColor(m_attachments.m_position.m_vkFormat);

        renderPassCreateInfo.attachmentDescription(normalAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleColor(m_attachments.m_normal.m_vkFormat);

        renderPassCreateInfo.attachmentDescription(albedoAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleColor(m_attachments.m_albedo.m_vkFormat);

        renderPassCreateInfo.attachmentDescription(depthAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleDepthStencil(m_vkFormatDepth);

        constexpr VkAttachmentReference colorPresentAttachmentReference
            = { colorPresentAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        constexpr VkAttachmentReference depthAttachmentReference
            = { depthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        // First subpass: Fill G-Buffer components
        // ----------------------------------------------------------------------------------------
        renderPassCreateInfo.subpassDescription(subpassFillG)
            .addColorAttachmentReference(colorPresentAttachmentReference)
            .setDepthStencilAttachmentReference(depthAttachmentReference)
            .addColorAttachmentReference(positionAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentReference(normalAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentReference(albedoAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Second subpass: Final composition (using G-Buffer components)
        // ----------------------------------------------------------------------------------------
        //	Note that the "extra" attachments now become input attachments.
        renderPassCreateInfo.subpassDescription(subpassFinalComposition)
            .addColorAttachmentReference(colorPresentAttachmentReference)
            .setDepthStencilAttachmentReference(depthAttachmentReference)
            .addInputAttachmentReference(positionAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .addInputAttachmentReference(normalAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .addInputAttachmentReference(albedoAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Third subpass: Forward transparency
        // ----------------------------------------------------------------------------------------
        renderPassCreateInfo.subpassDescription(subpassForwardTransparency)
            .addColorAttachmentReference(colorPresentAttachmentReference)
            .setDepthStencilAttachmentReference(depthAttachmentReference)
            .setDepthStencilAttachmentReference(depthAttachmentReference)
            .addInputAttachmentReference(positionAttachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Subpass dependencies for layout transitions
        auto fragmentTests
            = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

        renderPassCreateInfo.addSubpassDependency(VK_SUBPASS_EXTERNAL, subpassFillG)
            .setSrc(fragmentTests, 0)
            .setDst(fragmentTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        renderPassCreateInfo.addSubpassDependency(VK_SUBPASS_EXTERNAL, subpassFillG)
            .setSrc(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0)
            .setDst(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        renderPassCreateInfo.addSubpassDependency(subpassFillG, subpassFinalComposition)
            .setSrc(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .setDst(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
            .setDependencyFlags(VK_DEPENDENCY_BY_REGION_BIT);

        renderPassCreateInfo.addSubpassDependency(subpassFinalComposition, subpassForwardTransparency)
            .setSrc(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .setDst(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
            .setDependencyFlags(VK_DEPENDENCY_BY_REGION_BIT);

        renderPassCreateInfo.addSubpassDependency(subpassForwardTransparency, VK_SUBPASS_EXTERNAL)
            .setSrc(
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .setDst(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_MEMORY_READ_BIT)
            .setDependencyFlags(VK_DEPENDENCY_BY_REGION_BIT);

        m_renderPassOriginal = vkcpp::RenderPass(renderPassCreateInfo);
    }

    void buildCommandBuffers() override
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
                commandBuffer.cmdBindPipeline(m_pipelineOffscreen);
                commandBuffer.cmdBindDescriptorSet(m_descriptorSetScene, m_pipelineLayoutOffscreen);
                m_modelScene.draw(commandBuffer);
                vks::debugutils::cmdEndLabel(commandBuffer);
            }

            // Second sub pass
            // This subpass will use the G-Buffer components that have been filled in the first subpass as input attachment for the final compositing
            {
                vks::debugutils::cmdBeginLabel(commandBuffer, "Subpass 1: Deferred composition", { 0.0f, 0.5f, 1.0f, 1.0f });
                commandBuffer
                    .cmdNextSubpass()
                    .cmdBindPipeline(m_pipelineComposition)
                    .cmdBindDescriptorSet(m_descriptorSetComposition, m_pipelineLayoutComposition);

                commandBuffer.cmdDraw(3, 1); //	TODO: magic numbers	vertexCount, indexCount
                vks::debugutils::cmdEndLabel(commandBuffer);
            }

            // Third subpass
            // Render transparent geometry using a forward pass that compares against depth generated during G-Buffer fill
            {
                vks::debugutils::cmdBeginLabel(commandBuffer, "Subpass 2: Forward transparency", { 0.5f, 0.76f, 0.34f, 1.0f });
                commandBuffer
                    .cmdNextSubpass()
                    .cmdBindPipeline(m_pipelineTransparent)
                    .cmdBindDescriptorSet(m_descriptorSetTransparent, m_pipelineLayoutTransparent);
                m_modelTransparent.draw(commandBuffer);
                vks::debugutils::cmdEndLabel(commandBuffer);
            }

            drawUI(commandBuffer);

            commandBuffer
                .cmdEndRenderPass()
                .end();
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
        //	Descriptor Pool
        vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo
            .addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4)
            .addDescriptorCount(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
            .addDescriptorCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4)
            .addDescriptorCount(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4)
            .setMaxSets(4);

        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo);

        //	Descriptor Set Layout
        constexpr int vertexShaderUniformBufferDescriptorIndex = 0;
        vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
        descriptorSetLayoutCreateInfo
            .addDescriptorSetLayoutBinding(
                vertexShaderUniformBufferDescriptorIndex,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                vkcpp::SHADER_STAGE_VERTEX);
        m_descriptorSetLayoutScene = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

        //	Descriptor Sets
        m_descriptorSetScene = vkcpp::DescriptorSet(m_descriptorSetLayoutScene, m_descriptorPool);

        //	Descriptor Set Update
        vkcpp::DescriptorSetUpdater descriptorSetUpdater(m_descriptorSetScene);
        descriptorSetUpdater
            .addBufferWriteDescriptor(
                vertexShaderUniformBufferDescriptorIndex,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                m_bufferGBuffer.m_vkDescriptorBufferInfo);
        descriptorSetUpdater.updateDescriptorSets();
    }

    void preparePipelines()
    {
        // Pipeline Layout
        vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayoutScene);
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
        m_pipelineOffscreen = vkcpp::GraphicsPipeline(pipelineCI);
    }

    // Create the Vulkan objects used in the composition pass (descriptor sets, pipelines, etc.)
    void prepareCompositionPass()
    {
        //	Descriptor Set Layout
        constexpr int positionInputAttachmentBindingIndex = 0;
        constexpr int normalInputAttachmentBindingIndex = 1;
        constexpr int albedoInputAttachmentBindingIndex = 2;
        constexpr int lightPositionsBindingIndex = 3;

        {
            vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
            descriptorSetLayoutCreateInfo
                .addDescriptorSetLayoutBinding(
                    positionInputAttachmentBindingIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    vkcpp::SHADER_STAGE_FRAGMENT)
                .addDescriptorSetLayoutBinding(
                    normalInputAttachmentBindingIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    vkcpp::SHADER_STAGE_FRAGMENT)
                .addDescriptorSetLayoutBinding(
                    albedoInputAttachmentBindingIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    vkcpp::SHADER_STAGE_FRAGMENT)
                .addDescriptorSetLayoutBinding(
                    lightPositionsBindingIndex,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    vkcpp::SHADER_STAGE_FRAGMENT);
            m_descriptorSetLayoutComposition = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

            // Descriptor Sets
            m_descriptorSetComposition = vkcpp::DescriptorSet(m_descriptorSetLayoutComposition, m_descriptorPool);

            //	Update Descriptor Sets
            vkcpp::DescriptorSetUpdater descriptorSetUpdater(m_descriptorSetComposition);
            descriptorSetUpdater
                .addImageWriteDescriptor(
                    positionInputAttachmentBindingIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    m_attachments.m_position.m_vkImageView,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_NULL_HANDLE)
                .addImageWriteDescriptor(
                    normalInputAttachmentBindingIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    m_attachments.m_normal.m_vkImageView,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_NULL_HANDLE)
                .addImageWriteDescriptor(
                    albedoInputAttachmentBindingIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    m_attachments.m_albedo.m_vkImageView,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_NULL_HANDLE)
                .addBufferWriteDescriptor(
                    lightPositionsBindingIndex,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    m_bufferLights.m_vkDescriptorBufferInfo);
            descriptorSetUpdater.updateDescriptorSets();
        }
        // Pipeline Layout
        {
            vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
            pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayoutComposition);
            m_pipelineLayoutComposition = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);
        }

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
        m_pipelineComposition = vkcpp::GraphicsPipeline(pipelineCI);

        // Descriptor Set layout
        constexpr int uniformBufferDescriptorIndexG = 0;
        constexpr int inputAttachmentPositionDescriptorIndex = 1;
        constexpr int combinedImageDescriptorIndex = 2;

        {
            vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
            descriptorSetLayoutCreateInfo
                .addDescriptorSetLayoutBinding(
                    uniformBufferDescriptorIndexG,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    vkcpp::SHADER_STAGE_VERTEX)
                .addDescriptorSetLayoutBinding(
                    inputAttachmentPositionDescriptorIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    vkcpp::SHADER_STAGE_FRAGMENT)
                .addDescriptorSetLayoutBinding(
                    combinedImageDescriptorIndex,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    vkcpp::SHADER_STAGE_FRAGMENT);

            m_descriptorSetLayoutTransparent = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

            // Descriptor Set
            m_descriptorSetTransparent = vkcpp::DescriptorSet(m_descriptorSetLayoutTransparent, m_descriptorPool);

            //	Update Descriptor Set
            vkcpp::DescriptorSetUpdater descriptorSetUpdater(m_descriptorSetTransparent);
            descriptorSetUpdater
                .addBufferWriteDescriptor(
                    uniformBufferDescriptorIndexG,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    m_bufferGBuffer.m_vkDescriptorBufferInfo)
                .addImageWriteDescriptor(
                    inputAttachmentPositionDescriptorIndex,
                    VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    m_attachments.m_position.m_vkImageView,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_NULL_HANDLE)
                .addImageWriteDescriptor(
                    combinedImageDescriptorIndex,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    m_textureGlass.m_vkDescriptorImageInfo);
            descriptorSetUpdater.updateDescriptorSets();
        }

        // Pipeline Layout
        {
            vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
            pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayoutTransparent);
            m_pipelineLayoutTransparent = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);
        }

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
        m_pipelineTransparent = vkcpp::GraphicsPipeline(pipelineCI);
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        // Matrices
        m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &m_bufferGBuffer,
            sizeof(m_uboGBuffer));
        VK_CHECK_RESULT(m_bufferGBuffer.map());

        // Lights
        m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
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
