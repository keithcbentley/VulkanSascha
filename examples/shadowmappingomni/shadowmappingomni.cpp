/*
 * Vulkan Example - Omni directional shadows using a dynamic cube map
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanglTFModel.h"
#include "vulkanexamplebase.h"

vkcpp::VulkanContext vkcpp::s_vulkanContext;


class VulkanExample : public VulkanExampleBase {
public:
    bool displayCubeMap { false };

    // Defines the depth range used for the shadow maps
    // This should be kept as small as possible for precision
    float zNear { 0.1f };
    float zFar { 1024.0f };

    struct {
        vkglTF::Model scene;
        vkglTF::Model debugcube;
    } models;

    glm::vec4 lightPos = glm::vec4(0.0f, -2.5f, 0.0f, 1.0);

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 lightPos;
    };
    UniformData uniformDataScene, uniformDataOffscreen;

    struct {
        vks::Buffer scene;
        vks::Buffer offscreen;
    } uniformBuffers;

    struct {
        VkPipeline scene { VK_NULL_HANDLE };
        VkPipeline offscreen { VK_NULL_HANDLE };
        VkPipeline cubemapDisplay { VK_NULL_HANDLE };
    } pipelines;

    struct {
        VkPipelineLayout scene { VK_NULL_HANDLE };
        VkPipelineLayout offscreen { VK_NULL_HANDLE };
    } pipelineLayouts;

    struct {
        VkDescriptorSet scene { VK_NULL_HANDLE };
        VkDescriptorSet offscreen { VK_NULL_HANDLE };
    } descriptorSets;

    VkDescriptorSetLayout m_vkDescriptorSetLayout { VK_NULL_HANDLE };

    vks::Texture shadowCubeMap;
    std::array<VkImageView, 6> shadowCubeMapFaceImageViews {};

    // Framebuffer for offscreen rendering
    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    };
    struct OffscreenPass {
        int32_t width, height;
        std::array<VkFramebuffer, 6> frameBuffers;
        FrameBufferAttachment depth;
        VkRenderPass renderPass;
        VkSampler sampler;
        VkDescriptorImageInfo descriptor;
    } offscreenPass;

    // Size of the shadow map texture (per face)
    const uint32_t offscreenImageSize { 1024 };
    // We use a 32 bit float format for max. precision. Depending on the use case, lower precision may be fine and can save bandwidth
    const VkFormat offscreenImageFormat { VK_FORMAT_R32_SFLOAT };
    // The depth format is selected at runtime
    VkFormat offscreenDepthFormat { VK_FORMAT_UNDEFINED };

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Point light shadows (cubemap)";
        camera.type = Camera::CameraType::lookat;
        camera.setPerspective(45.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, zNear, zFar);
        camera.setRotation(glm::vec3(-20.5f, -673.0f, 0.0f));
        camera.setPosition(glm::vec3(0.0f, 0.5f, -15.0f));
        timerSpeed *= 0.5f;
    }

    ~VulkanExample()
    {
        if (m_device) {
            // Cube map
            for (uint32_t i = 0; i < 6; i++) {
                vkDestroyImageView(m_device, shadowCubeMapFaceImageViews[i], nullptr);
            }

            vkDestroyImageView(m_device, shadowCubeMap.m_vkImageView, nullptr);
            vkDestroyImage(m_device, shadowCubeMap.m_vkImage, nullptr);
            vkDestroySampler(m_device, shadowCubeMap.m_vkSampler, nullptr);
            vkFreeMemory(m_device, shadowCubeMap.m_vkDeviceMemory, nullptr);

            // Depth attachment
            vkDestroyImageView(m_device, offscreenPass.depth.view, nullptr);
            vkDestroyImage(m_device, offscreenPass.depth.image, nullptr);
            vkFreeMemory(m_device, offscreenPass.depth.mem, nullptr);

            for (uint32_t i = 0; i < 6; i++) {
                vkDestroyFramebuffer(m_device, offscreenPass.frameBuffers[i], nullptr);
            }

            vkDestroyRenderPass(m_device, offscreenPass.renderPass, nullptr);

            // Pipelines
            vkDestroyPipeline(m_device, pipelines.scene, nullptr);
            vkDestroyPipeline(m_device, pipelines.offscreen, nullptr);
            vkDestroyPipeline(m_device, pipelines.cubemapDisplay, nullptr);

            vkDestroyPipelineLayout(m_device, pipelineLayouts.scene, nullptr);
            vkDestroyPipelineLayout(m_device, pipelineLayouts.offscreen, nullptr);

            vkDestroyDescriptorSetLayout(m_device, m_vkDescriptorSetLayout, nullptr);
        }
    }

    void prepareCubeMap()
    {
        shadowCubeMap.width = offscreenImageSize;
        shadowCubeMap.height = offscreenImageSize;

        // Cube map m_vkImage description
        VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = offscreenImageFormat;
        imageCreateInfo.extent = { shadowCubeMap.width, shadowCubeMap.height, 1 };
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 6;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;

        VkCommandBuffer layoutCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Create cube map m_vkImage
        VK_CHECK_RESULT(vkCreateImage(m_device, &imageCreateInfo, nullptr, &shadowCubeMap.m_vkImage));

        vkGetImageMemoryRequirements(m_device, shadowCubeMap.m_vkImage, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex
            = vkcpp::findMemoryTypeIndex(memReqs.memoryTypeBits, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &shadowCubeMap.m_vkDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(m_device, shadowCubeMap.m_vkImage, shadowCubeMap.m_vkDeviceMemory, 0));

        // Image barrier for optimal m_vkImage (target)
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 6;
        vks::tools::setImageLayout(
            layoutCmd,
            shadowCubeMap.m_vkImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange);

        m_pVulkanDevice->flushCommandBuffer(layoutCmd, m_queue, true);

        // Create sampler
        VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler.addressModeV = sampler.addressModeU;
        sampler.addressModeW = sampler.addressModeU;
        sampler.mipLodBias = 0.0f;
        sampler.maxAnisotropy = 1.0f;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        sampler.maxLod = 1.0f;
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK_RESULT(vkCreateSampler(m_device, &sampler, nullptr, &shadowCubeMap.m_vkSampler));

        // Create m_vkImage m_vkImageView
        VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
        view.image = VK_NULL_HANDLE;
        view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view.format = offscreenImageFormat;
        view.components = { VK_COMPONENT_SWIZZLE_R };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 6;
        view.image = shadowCubeMap.m_vkImage;
        VK_CHECK_RESULT(vkCreateImageView(m_device, &view, nullptr, &shadowCubeMap.m_vkImageView));

        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.subresourceRange.layerCount = 1;
        view.image = shadowCubeMap.m_vkImage;

        for (uint32_t i = 0; i < 6; i++) {
            view.subresourceRange.baseArrayLayer = i;
            VK_CHECK_RESULT(vkCreateImageView(m_device, &view, nullptr, &shadowCubeMapFaceImageViews[i]));
        }
    }

    // Set up a separate render pass for the offscreen frame buffer
    // This is necessary as the offscreen frame buffer attachments
    // use formats different to the ones from the visible frame buffer
    // and at least the depth one may not be compatible
    void prepareOffscreenRenderpass()
    {
        VkAttachmentDescription osAttachments[2] = {};

        // Find a suitable depth format for
        VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(vkcpp::physicalDevice(), &offscreenDepthFormat);
        assert(validDepthFormat);

        osAttachments[0].format = offscreenImageFormat;
        osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth attachment
        osAttachments[1].format = offscreenDepthFormat;
        osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference = {};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthReference = {};
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
        subpass.pDepthStencilAttachment = &depthReference;

        VkRenderPassCreateInfo renderPassCreateInfo = vks::initializers::renderPassCreateInfo();
        renderPassCreateInfo.attachmentCount = 2;
        renderPassCreateInfo.pAttachments = osAttachments;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;

        VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &offscreenPass.renderPass));
    }

    // Prepare a new framebuffer for offscreen rendering
    // The contents of this framebuffer are then
    // copied to the different cube map faces
    void prepareOffscreenFramebuffer()
    {
        offscreenPass.width = offscreenImageSize;
        offscreenPass.height = offscreenImageSize;

        // Color attachment
        VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = offscreenImageFormat;
        imageCreateInfo.extent.width = offscreenPass.width;
        imageCreateInfo.extent.height = offscreenPass.height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // Image of the framebuffer is blit source
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = offscreenImageFormat;
        colorImageView.flags = 0;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;

        VkCommandBuffer layoutCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Depth stencil attachment
        imageCreateInfo.format = offscreenDepthFormat;
        imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
        depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthStencilView.format = offscreenDepthFormat;
        depthStencilView.flags = 0;
        depthStencilView.subresourceRange = {};
        depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (offscreenDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
            depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        depthStencilView.subresourceRange.baseMipLevel = 0;
        depthStencilView.subresourceRange.levelCount = 1;
        depthStencilView.subresourceRange.baseArrayLayer = 0;
        depthStencilView.subresourceRange.layerCount = 1;

        VK_CHECK_RESULT(vkCreateImage(m_device, &imageCreateInfo, nullptr, &offscreenPass.depth.image));

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, offscreenPass.depth.image, &memReqs);

        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex
			= vkcpp::findMemoryTypeIndex(memReqs.memoryTypeBits, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAlloc, nullptr, &offscreenPass.depth.mem));
        VK_CHECK_RESULT(vkBindImageMemory(m_device, offscreenPass.depth.image, offscreenPass.depth.mem, 0));

        vks::tools::setImageLayout(
            layoutCmd,
            offscreenPass.depth.image,
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        m_pVulkanDevice->flushCommandBuffer(layoutCmd, m_queue, true);

        depthStencilView.image = offscreenPass.depth.image;
        VK_CHECK_RESULT(vkCreateImageView(m_device, &depthStencilView, nullptr, &offscreenPass.depth.view));

        VkImageView attachments[2];
        attachments[1] = offscreenPass.depth.view;

        VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
        fbufCreateInfo.renderPass = offscreenPass.renderPass;
        fbufCreateInfo.attachmentCount = 2;
        fbufCreateInfo.pAttachments = attachments;
        fbufCreateInfo.width = offscreenPass.width;
        fbufCreateInfo.height = offscreenPass.height;
        fbufCreateInfo.layers = 1;

        for (uint32_t i = 0; i < 6; i++) {
            attachments[0] = shadowCubeMapFaceImageViews[i];
            VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &fbufCreateInfo, nullptr, &offscreenPass.frameBuffers[i]));
        }
    }

    // Updates a single cube map face
    // Renders the scene with face's m_vkImageView directly to the cubemap layer `faceIndex`
    // Uses push constants for quick update of m_vkImageView matrix for the current cube map face
    void updateCubeFace(uint32_t faceIndex, VkCommandBuffer commandBuffer)
    {
        VkClearValue clearValues[2];
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
        // Reuse render pass from example pass
        renderPassBeginInfo.renderPass = offscreenPass.renderPass;
        renderPassBeginInfo.framebuffer = offscreenPass.frameBuffers[faceIndex];
        renderPassBeginInfo.renderArea.extent.width = offscreenPass.width;
        renderPassBeginInfo.renderArea.extent.height = offscreenPass.height;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        // Update m_vkImageView matrix via push constant

        glm::mat4 viewMatrix = glm::mat4(1.0f);
        switch (faceIndex) {
        case 0: // POSITIVE_X
            viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case 1: // NEGATIVE_X
            viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case 2: // POSITIVE_Y
            viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case 3: // NEGATIVE_Y
            viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case 4: // POSITIVE_Z
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case 5: // NEGATIVE_Z
            viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            break;
        }

        // Render scene from cube face's point of m_vkImageView
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Update shader push constant block
        // Contains current face m_vkImageView matrix
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayouts.offscreen,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &viewMatrix);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.offscreen, 0, 1, &descriptorSets.offscreen, 0, NULL);
        models.scene.draw(commandBuffer);

        vkCmdEndRenderPass(commandBuffer);
    }

    void buildCommandBuffers()
    {
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

        for (int32_t i = 0; i < m_drawCommandBuffers.size(); ++i) {
            vkcpp::CommandBuffer commandBuffer
                = vkcpp::CommandBuffer::makeCopy(m_drawCommandBuffers[i]);

            commandBuffer.begin(cmdBufInfo);

            /*
                    Generate shadow cube maps using one render pass per face
            */
            {
                commandBuffer.cmdSetViewport(offscreenPass.width, offscreenPass.height);
                commandBuffer.cmdSetScissor(offscreenPass.width, offscreenPass.height);

                for (uint32_t face = 0; face < 6; face++) {
                    updateCubeFace(face, commandBuffer);
                }
            }

            /*
                    Note: Explicit synchronization is not required between the render pass, as this is done implicit via sub pass dependencies
            */

            /*
                    Scene rendering with applied shadow map
            */
            {
                VkClearValue clearValues[2];
                clearValues[0].color = m_vkClearColorValueDefault;
                clearValues[1].depthStencil = { 1.0f, 0 };

                VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
                renderPassBeginInfo.renderPass = m_renderPassOriginal;
                renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];
                renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
                renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
                renderPassBeginInfo.clearValueCount = 2;
                renderPassBeginInfo.pClearValues = clearValues;

                commandBuffer.cmdBeginRenderPass(renderPassBeginInfo);
                commandBuffer.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight);
                commandBuffer.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight);
                commandBuffer.cmdBindDescriptorSet(descriptorSets.scene, pipelineLayouts.scene);

                if (displayCubeMap) {
                    // Display all six sides of the shadow cube map
                    // Note: Visualization of the different faces is done in the fragment shader, see cubemapdisplay.frag
                    commandBuffer.cmdBindPipeline(pipelines.cubemapDisplay);
                    models.debugcube.draw(commandBuffer);
                } else {
                    commandBuffer.cmdBindPipeline(pipelines.scene);
                    models.scene.draw(commandBuffer);
                }

                drawUI(commandBuffer);

                commandBuffer.cmdEndRenderPass();
            }
            commandBuffer.end();
        }
    }

    void loadAssets()
    {
        const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
        models.debugcube.loadFromFile(getAssetPath() + "models/cube.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
        models.scene.loadFromFile(getAssetPath() + "models/shadowscene_fire.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
    }

    void setupDescriptors()
    {
        // Pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
		m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolInfo);
        //VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            // Binding 1 : Fragment shader m_vkImage sampler (cube map)
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

        // Sets
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayout, 1);

        // 3D scene
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSets.scene));
        // Image descriptor for the cube map
        VkDescriptorImageInfo texDescriptor = vks::initializers::descriptorImageInfo(
            shadowCubeMap.m_vkSampler,
            shadowCubeMap.m_vkImageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        std::vector<VkWriteDescriptorSet> sceneDescriptorSets = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::writeDescriptorSet(
				descriptorSets.scene,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.scene.m_vkDescriptorBufferInfo),
            // Binding 1 : Fragment shader shadow sampler
            vks::initializers::writeDescriptorSet(descriptorSets.scene, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptor)
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(sceneDescriptorSets.size()), sceneDescriptorSets.data(), 0, nullptr);

        // Offscreen
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSets.offscreen));
        std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::writeDescriptorSet(
				descriptorSets.offscreen,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&uniformBuffers.offscreen.m_vkDescriptorBufferInfo),
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(offScreenWriteDescriptorSets.size()), offScreenWriteDescriptorSets.data(), 0, nullptr);
    }

    void preparePipelines()
    {
        // Layouts
        // 3D scene pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
        VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.scene));

        // Offscreen pipeline layout
        // Push constants for cube map face m_vkImageView matrices
        VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
        // Push constant ranges are part of the pipeline layout
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.offscreen));

        // Pipelines
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

        // 3D scene pipeline
        // Load shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        shaderStages[0] = loadShader(getShadersPath() + "shadowmappingomni/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "shadowmappingomni/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        VkGraphicsPipelineCreateInfo pipelineCI
			= vks::initializers::pipelineCreateInfo(pipelineLayouts.scene, m_renderPassOriginal, 0);
        pipelineCI.pInputAssemblyState = &inputAssemblyState;
        pipelineCI.pRasterizationState = &rasterizationState;
        pipelineCI.pColorBlendState = &colorBlendState;
        pipelineCI.pMultisampleState = &multisampleState;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pDepthStencilState = &depthStencilState;
        pipelineCI.pDynamicState = &dynamicState;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.scene));

        // Offscreen pipeline
        shaderStages[0] = loadShader(getShadersPath() + "shadowmappingomni/offscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "shadowmappingomni/offscreen.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        pipelineCI.layout = pipelineLayouts.offscreen;
        pipelineCI.renderPass = offscreenPass.renderPass;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreen));

        // Cube map display pipeline
        shaderStages[0] = loadShader(getShadersPath() + "shadowmappingomni/cubemapdisplay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "shadowmappingomni/cubemapdisplay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
        pipelineCI.pVertexInputState = &emptyInputState;
        pipelineCI.layout = pipelineLayouts.scene;
        pipelineCI.renderPass = m_renderPassOriginal;
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.cubemapDisplay));
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        // Offscreen vertex shader uniform buffer
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE_COHERENT,
			&uniformBuffers.offscreen, sizeof(UniformData)));
        // Scene vertex shader uniform buffer
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE_COHERENT,
			&uniformBuffers.scene, sizeof(UniformData)));
        // Map persistent
        VK_CHECK_RESULT(uniformBuffers.offscreen.map());
        VK_CHECK_RESULT(uniformBuffers.scene.map());
    }

    void updateUniformBuffers()
    {
        uniformDataScene.projection = camera.matrices.perspective;
        uniformDataScene.view = camera.matrices.view;
        uniformDataScene.model = glm::mat4(1.0f);
        uniformDataScene.lightPos = lightPos;
        memcpy(uniformBuffers.scene.m_pMapped, &uniformDataScene, sizeof(UniformData));
    }

    void updateUniformBufferOffscreen()
    {
        lightPos.x = sin(glm::radians(timer * 360.0f)) * 0.15f;
        lightPos.z = cos(glm::radians(timer * 360.0f)) * 0.15f;
        uniformDataOffscreen.projection = glm::perspective((float)(M_PI / 2.0), 1.0f, zNear, zFar);
        uniformDataOffscreen.view = glm::mat4(1.0f);
        uniformDataOffscreen.model = glm::translate(glm::mat4(1.0f), glm::vec3(-lightPos.x, -lightPos.y, -lightPos.z));
        uniformDataOffscreen.lightPos = lightPos;
        memcpy(uniformBuffers.offscreen.m_pMapped, &uniformDataOffscreen, sizeof(UniformData));
    }

    void draw()
    {
        VulkanExampleBase::prepareSubmitFrameBase(m_drawCommandBuffers[m_currentBufferIndex]);
    }

    void prepare()
    {
        VulkanExampleBase::prepare();
        loadAssets();
        prepareUniformBuffers();
        prepareCubeMap();
        setupDescriptors();
        prepareOffscreenRenderpass();
        preparePipelines();
        prepareOffscreenFramebuffer();
        buildCommandBuffers();
        m_prepared = true;
    }

    virtual void render()
    {
        if (!m_prepared)
            return;
        updateUniformBuffers();
        updateUniformBufferOffscreen();
        draw();
    }

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
    {
        if (overlay->header("Settings")) {
            if (overlay->checkBox("Display shadow cube render target", &displayCubeMap)) {
                buildCommandBuffers();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
