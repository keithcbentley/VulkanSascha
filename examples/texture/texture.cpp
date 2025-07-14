/*
 * Vulkan Example - Texture loading (and display) example (including mip maps)
 *
 * This sample shows how to upload a 2D texture to the m_vkDevice and how to display it. In Vulkan this is done using images, views and samplers.
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"
#include <ktx.h>
#include <ktxvulkan.h>

// Vertex layout for this example
struct Vertex {
    float pos[3];
    float uv[2];
    float normal[3];
};

class VulkanExample : public VulkanExampleBase {
public:
    // Contains all Vulkan objects that are required to store and use a texture
    // Note that this repository contains a texture class (VulkanTexture.hpp) that encapsulates texture loading functionality in a class that is used in subsequent demos
    struct Texture {
        VkSampler m_vkSampler { VK_NULL_HANDLE };
        vkcpp::Image m_image;
        VkImageLayout m_vkImageLayout;
        vkcpp::DeviceMemory m_deviceMemory;
        VkImageView m_vkImageView { VK_NULL_HANDLE };
        uint32_t m_width { 0 };
        uint32_t m_height { 0 };
        uint32_t m_mipLevels { 0 };
    } m_texture;

    vks::Buffer vertexBuffer;
    vks::Buffer indexBuffer;
    uint32_t m_indexCount { 0 };

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec4 viewPos;
        // This is used to change the bias for the level-of-detail (mips) in the fragment shader
        float lodBias = 0.0f;
    } uniformData;
    vks::Buffer uniformBuffer;

    VkPipeline m_vkPipeline { VK_NULL_HANDLE };
    vkcpp::PipelineLayout m_pipelineLayout;
    vkcpp::DescriptorSet m_descriptorSet;
    vkcpp::DescriptorSetLayout m_descriptorSetLayout;

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Texture loading";
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
        camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
    }

    ~VulkanExample()
    {
        if (m_deviceOriginal) {
            destroyTextureImage(m_texture);
            vkDestroyPipeline(m_deviceOriginal, m_vkPipeline, nullptr);
            vertexBuffer.destroy();
            indexBuffer.destroy();
            uniformBuffer.destroy();
        }
    }

    // Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
    virtual void getEnabledFeatures()
    {
        // Enable anisotropic filtering if supported
        if (m_physicalDeviceFeatures.m_features2.features.samplerAnisotropy) {
            m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
        };
    }

    /*
            Upload texture m_vkImage data to the GPU

            Vulkan offers two types of m_vkImage tiling (m_vkDeviceMemory layout):

            Linear tiled images:
                    These are stored as is and can be copied directly to. But due to the linear nature they're not a good match for GPUs and format and feature support is very limited.
                    It's not advised to use linear tiled images for anything else than copying from host to GPU if buffer copies are not an option.
                    Linear tiling is thus only implemented for learning purposes, one should always prefer optimal tiled m_vkImage.

            Optimal tiled images:
                    These are stored in an implementation specific layout matching the capability of the hardware. They usually support more formats and m_vkPhysicalDeviceFeatures and are much faster.
                    Optimal tiled images are stored on the m_vkDevice and not accessible by the host. So they can't be written directly to (like liner tiled images) and always require
                    some sort of data copy, either from a buffer or	a linear tiled m_vkImage.

            In Short: Always use optimal tiled images for rendering.
    */
    void loadTexture()
    {
        // We use the Khronos texture format (https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/)
        std::string filename = getAssetPath() + "textures/metalplate01_rgba.ktx";
        // Texture data contains 4 channels (RGBA) with unnormalized 8-bit values, this is the most commonly supported format
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

        ktxResult result;
        ktxTexture* ktxTexture;

        if (!vks::tools::fileExists(filename)) {
            vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
        }
        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
        assert(result == KTX_SUCCESS);

        // Get m_vkPhysicalDeviceProperties required for using and upload texture data from the ktx texture object
        m_texture.m_width = ktxTexture->baseWidth;
        m_texture.m_height = ktxTexture->baseHeight;
        m_texture.m_mipLevels = ktxTexture->numLevels;
        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

        // We prefer using staging to copy the texture data to a m_vkDevice local optimal m_vkImage
        VkBool32 useStaging = true;

        // Only use linear tiling if forced
        bool forceLinearTiling = false;
        if (forceLinearTiling) {
            // Don't use linear if format is not supported for (linear) shader sampling
            // Get m_vkDevice m_vkPhysicalDeviceProperties for the requested texture format
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(m_physicalDeviceOriginal, format, &formatProperties);
            useStaging = !(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        }


        if (useStaging) {
            // Copy data to an optimal tiled m_vkImage
            // This loads the texture data into a host local buffer that is copied to the optimal tiled m_vkImage on the m_vkDevice

            // Create a host-visible staging buffer that contains the raw m_vkImage data
            // This buffer will be the data source for copying texture data to the optimal tiled m_vkImage on the m_vkDevice

			vkcpp::BufferCreateInfo bufferCreateInfo;
            bufferCreateInfo.size = ktxTextureSize;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			vkcpp::Buffer stagingBuffer = vkcpp::Buffer(bufferCreateInfo, m_deviceOriginal);

            // Get m_vkDeviceMemory requirements for the staging buffer (alignment, m_vkDeviceMemory type bits)
			VkMemoryRequirements vkMemoryRequirementsBuffer = stagingBuffer.getMemoryRequirements();
			vkcpp::DeviceMemory stagingMemory = vkcpp::DeviceMemory(
				vkMemoryRequirementsBuffer, vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT, m_deviceOriginal);


            VK_CHECK_RESULT(vkBindBufferMemory(m_deviceOriginal, stagingBuffer, stagingMemory, 0));

            // Copy texture data into host local staging buffer
            uint8_t* data;
            VK_CHECK_RESULT(vkMapMemory(m_deviceOriginal, stagingMemory, 0, vkMemoryRequirementsBuffer.size, 0, (void**)&data));
            memcpy(data, ktxTextureData, ktxTextureSize);
            vkUnmapMemory(m_deviceOriginal, stagingMemory);

            // Setup buffer copy regions for each mip level
            std::vector<VkBufferImageCopy> bufferCopyRegions;
            uint32_t offset = 0;

            for (uint32_t i = 0; i < m_texture.m_mipLevels; i++) {
                // Calculate offset into staging buffer for the current mip level
                ktx_size_t offset;
                KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
                assert(ret == KTX_SUCCESS);
                // Setup a buffer m_vkImage copy structure for the current mip level
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = i;
                bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> i;
                bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> i;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;
                bufferCopyRegions.push_back(bufferCopyRegion);
            }

            // Create optimal tiled target m_vkImage on the m_vkDevice
            // VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
            VkImageCreateInfo imageCreateInfo {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.format = format;
            imageCreateInfo.mipLevels = m_texture.m_mipLevels;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // Set initial layout of the m_vkImage to undefined
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCreateInfo.extent = { m_texture.m_width, m_texture.m_height, 1 };
            imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			m_texture.m_image = vkcpp::Image(imageCreateInfo, m_deviceOriginal);

			VkMemoryRequirements vkMemoryRequirementsImage = m_texture.m_image.getMemoryRequirements();
			m_texture.m_deviceMemory = vkcpp::DeviceMemory(
				vkMemoryRequirementsImage, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL, m_deviceOriginal);

            VK_CHECK_RESULT(vkBindImageMemory(m_deviceOriginal, m_texture.m_image, m_texture.m_deviceMemory, 0));

            VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

            // Image m_vkDeviceMemory barriers for the texture m_vkImage

            // The sub resource range describes the regions of the m_vkImage that will be transitioned using the m_vkDeviceMemory barriers below
            VkImageSubresourceRange subresourceRange = {};
            // Image only contains color data
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // Start at first mip level
            subresourceRange.baseMipLevel = 0;
            // We will transition on all mip levels
            subresourceRange.levelCount = m_texture.m_mipLevels;
            // The 2D texture only has one layer
            subresourceRange.layerCount = 1;

            // Transition the texture m_vkImage layout to transfer target, so we can safely copy our buffer data to it.
            VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();
            imageMemoryBarrier.image = m_texture.m_image;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            // Insert a m_vkDeviceMemory dependency at the proper m_vkPipeline stages that will execute the m_vkImage layout transition
            // Source m_vkPipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
            // Destination m_vkPipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
            vkCmdPipelineBarrier(
                copyCmd,
                VK_PIPELINE_STAGE_HOST_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);

            // Copy mip levels from staging buffer
            vkCmdCopyBufferToImage(
                copyCmd,
                stagingBuffer,
                m_texture.m_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(bufferCopyRegions.size()),
                bufferCopyRegions.data());

            // Once the data has been uploaded we transfer to the texture m_vkImage to the shader read layout, so it can be sampled from
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Insert a m_vkDeviceMemory dependency at the proper m_vkPipeline stages that will execute the m_vkImage layout transition
            // Source m_vkPipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
            // Destination m_vkPipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            vkCmdPipelineBarrier(
                copyCmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);

            // Store current layout for later reuse
            m_texture.m_vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue, true);

        } else {
            // Copy data to a linear tiled m_vkImage


            // Load mip map level 0 to linear tiling m_vkImage
			VkImageCreateInfo imageCreateInfo {};
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.format = format;
            imageCreateInfo.mipLevels = 1;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
            imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
            imageCreateInfo.extent = { m_texture.m_width, m_texture.m_height, 1 };
			vkcpp::Image mappableImage = vkcpp::Image(imageCreateInfo, m_deviceOriginal);

			VkMemoryRequirements vkMemoryRequirementsImage = mappableImage.getMemoryRequirements();
			vkcpp::DeviceMemory stagingMemory = vkcpp::DeviceMemory(
				vkMemoryRequirementsImage, vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT, m_deviceOriginal);

			vkcpp::DeviceMemory mappableMemory = vkcpp::DeviceMemory(
				vkMemoryRequirementsImage, vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT, m_deviceOriginal);

            VK_CHECK_RESULT(vkBindImageMemory(m_deviceOriginal, mappableImage, mappableMemory, 0));

            // Map m_vkImage m_vkDeviceMemory
            void* data;
            VK_CHECK_RESULT(vkMapMemory(m_deviceOriginal, mappableMemory, 0, vkMemoryRequirementsImage.size, 0, &data));
            // Copy m_vkImage data of the first mip level into m_vkDeviceMemory
            memcpy(data, ktxTextureData, vkMemoryRequirementsImage.size);
            vkUnmapMemory(m_deviceOriginal, mappableMemory);

            // Linear tiled images don't need to be staged and can be directly used as textures
            m_texture.m_image = std::move(mappableImage);
            m_texture.m_deviceMemory = std::move(mappableMemory);
            m_texture.m_vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Setup m_vkImage m_vkDeviceMemory barrier transfer m_vkImage to shader read layout
            VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

            // The sub resource range describes the regions of the m_vkImage we will be transition
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.layerCount = 1;

            // Transition the texture m_vkImage layout to shader read, so it can be sampled from
            VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();
            imageMemoryBarrier.image = m_texture.m_image;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Insert a m_vkDeviceMemory dependency at the proper m_vkPipeline stages that will execute the m_vkImage layout transition
            // Source m_vkPipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
            // Destination m_vkPipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            vkCmdPipelineBarrier(
                copyCmd,
                VK_PIPELINE_STAGE_HOST_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);

            m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue, true);
        }

        ktxTexture_Destroy(ktxTexture);

        // Create a texture sampler
        // In Vulkan textures are accessed by samplers
        // This separates all the sampling information from the texture data. This means you could have multiple sampler objects for the same texture with different settings
        // Note: Similar to the samplers available with OpenGL 3.3
        VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipLodBias = 0.0f;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        // Set max level-of-detail to mip level count of the texture
        sampler.maxLod = (useStaging) ? (float)m_texture.m_mipLevels : 0.0f;
        // Enable anisotropic filtering
        // This feature is optional, so we must check if it's supported on the m_vkDevice
        if (m_physicalDeviceFeatures.m_features2.features.samplerAnisotropy) {
            // Use max. level of anisotropy for this example
            sampler.maxAnisotropy = m_physicalDeviceProperties.m_properties2.properties.limits.maxSamplerAnisotropy;
            sampler.anisotropyEnable = VK_TRUE;
        } else {
            // The m_vkDevice does not support anisotropic filtering
            sampler.maxAnisotropy = 1.0;
            sampler.anisotropyEnable = VK_FALSE;
        }
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK_RESULT(vkCreateSampler(m_deviceOriginal, &sampler, nullptr, &m_texture.m_vkSampler));

        // Create m_vkImage m_vkImageView
        // Textures are not directly accessed by the shaders and
        // are abstracted by m_vkImage views containing additional
        // information and sub resource ranges
        VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        // The subresource range describes the set of mip levels (and array layers) that can be accessed through this m_vkImage m_vkImageView
        // It's possible to create multiple m_vkImage views for a single m_vkImage referring to different (and/or overlapping) ranges of the m_vkImage
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.baseMipLevel = 0;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.layerCount = 1;
        // Linear tiling usually won't support mip maps
        // Only set mip map count if optimal tiling is used
        view.subresourceRange.levelCount = (useStaging) ? m_texture.m_mipLevels : 1;
        // The m_vkImageView will be based on the texture's m_vkImage
        view.image = m_texture.m_image;
        VK_CHECK_RESULT(vkCreateImageView(m_deviceOriginal, &view, nullptr, &m_texture.m_vkImageView));
    }

    // Free all Vulkan resources used by a texture object
    void destroyTextureImage(Texture texture)
    {
        vkDestroyImageView(m_deviceOriginal, texture.m_vkImageView, nullptr);
        vkDestroyImage(m_deviceOriginal, texture.m_image, nullptr);
        vkDestroySampler(m_deviceOriginal, texture.m_vkSampler, nullptr);
        vkFreeMemory(m_deviceOriginal, texture.m_deviceMemory, nullptr);
    }

    void buildCommandBuffers()
    {
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

        VkClearValue clearValues[2];
        clearValues[0].color = m_vkClearColorValueDefault;
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
        renderPassBeginInfo.renderPass = m_renderPassOriginal;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
        renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {

			vkcpp::CommandBuffer commandBuffer = vkcpp::CommandBuffer::makeCopy(drawCmdBuffers[i]);

            // Set target frame buffer
            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			commandBuffer.begin(cmdBufInfo);
			commandBuffer.cmdBeginRenderPass(renderPassBeginInfo);
			commandBuffer.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight);
			commandBuffer.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight);
			commandBuffer.cmdBindPipeline(m_vkPipeline);
			commandBuffer.cmdBindDescriptorSet(m_descriptorSet, m_pipelineLayout);
			commandBuffer.cmdBindVertexBuffer(vertexBuffer.m_vkBuffer);
			commandBuffer.cmdBindIndexBuffer(indexBuffer.m_vkBuffer, VK_INDEX_TYPE_UINT32);
			commandBuffer.cmdDrawIndexed(m_indexCount);
            drawUI(commandBuffer);
			commandBuffer.cmdEndRenderPass();
			commandBuffer.end();
        }
    }

    // Creates a vertex and index buffer for a quad made of two triangles
    // This is used to display the texture on
    void generateQuad()
    {
        // Setup vertices for a single uv-mapped quad made from two triangles
        std::vector<Vertex> vertices = {
            { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
            { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
        };

        // Setup indices
        std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };
        m_indexCount = static_cast<uint32_t>(indices.size());

        // Create buffers and upload data to the GPU
        struct StagingBuffers {
            vks::Buffer vertices;
            vks::Buffer indices;
        } stagingBuffers;

        // Host visible source buffers (staging)
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffers.vertices, vertices.size() * sizeof(Vertex), vertices.data()));
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffers.indices, indices.size() * sizeof(uint32_t), indices.data()));

        // Device local destination buffers
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, vertices.size() * sizeof(Vertex)));
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, indices.size() * sizeof(uint32_t)));

        // Copy from host do m_vkDevice
        m_pVulkanDevice->copyBuffer(&stagingBuffers.vertices, &vertexBuffer, m_vkQueue);
        m_pVulkanDevice->copyBuffer(&stagingBuffers.indices, &indexBuffer, m_vkQueue);

        // Clean up
        stagingBuffers.vertices.destroy();
        stagingBuffers.indices.destroy();
    }

    void setupDescriptors()
    {
        // Pool
		vkcpp::DescriptorPoolCreateInfo	descriptorPoolCreateInfo;
		descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
		descriptorPoolCreateInfo.setMaxSets(2);
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo, m_deviceOriginal);

        // Layout
		constexpr int	uniformBufferIndex = 0;
		constexpr int	combinedImageSamplerIndex = 1;
		vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
		descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(
			uniformBufferIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vkcpp::SHADER_STAGE_VERTEX);
		descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(
			combinedImageSamplerIndex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vkcpp::SHADER_STAGE_FRAGMENT);
		m_descriptorSetLayout = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo, m_deviceOriginal);
        
		//VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_deviceOriginal, descriptorSetLayoutCreateInfo, nullptr, &m_vkDescriptorSetLayout));

        // Set
        //VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_descriptorSetLayout, 1);
        //VK_CHECK_RESULT(vkAllocateDescriptorSets(m_deviceOriginal, &allocInfo, &descriptorSet));
		m_descriptorSet = vkcpp::DescriptorSet(m_descriptorSetLayout, m_descriptorPool);
        // Setup a descriptor m_vkImage info for the current texture to be used as a combined m_vkImage sampler
        VkDescriptorImageInfo textureDescriptor;
        // The m_vkImage's m_vkImageView (images are never directly accessed by the shader, but rather through views defining subresources)
        textureDescriptor.imageView = m_texture.m_vkImageView;
        // The sampler (Telling the m_vkPipeline how to sample the texture, including repeat, border, etc.)
        textureDescriptor.sampler = m_texture.m_vkSampler;
        // The current layout of the m_vkImage(Note: Should always fit the actual use, e.g.shader read)
        textureDescriptor.imageLayout = m_texture.m_vkImageLayout;

		vkcpp::DescriptorSetUpdater descriptorSetUpdater;

		descriptorSetUpdater.addBufferWriteDescriptor(
			m_descriptorSet,
			uniformBufferIndex,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			uniformBuffer.m_vkDescriptorBufferInfo);

		descriptorSetUpdater.addImageWriteDescriptor(
			m_descriptorSet,
			combinedImageSamplerIndex,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			textureDescriptor
			);

		descriptorSetUpdater.updateDescriptorSets(m_deviceOriginal);

    }

    void preparePipelines()
    {
        // Layout
		vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayout);

		m_pipelineLayout = vkcpp::PipelineLayout(pipelineLayoutCreateInfo, m_deviceOriginal);

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

        // Shaders
        shaderStages[0] = loadShader(getShadersPath() + "texture/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "texture/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        // Vertex input state
        std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
        };
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
            vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)),
            vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)),
        };
        VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
        vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
        vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
        vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(m_pipelineLayout, m_renderPassOriginal, 0);
        pipelineCreateInfo.pVertexInputState = &vertexInputState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfo.pStages = shaderStages.data();
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_deviceOriginal, m_vkPipelineCache, 1, &pipelineCreateInfo, nullptr, &m_vkPipeline));
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        // Vertex shader uniform buffer block
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformData), &uniformData));
        VK_CHECK_RESULT(uniformBuffer.map());
    }

    void updateUniformBuffers()
    {
        uniformData.projection = camera.matrices.perspective;
        uniformData.modelView = camera.matrices.view;
        uniformData.viewPos = camera.viewPos;
        memcpy(uniformBuffer.m_pMapped, &uniformData, sizeof(uniformData));
    }

    void prepare()
    {
        VulkanExampleBase::prepare();
        loadTexture();
        generateQuad();
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

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
    {
        if (overlay->header("Settings")) {
            if (overlay->sliderFloat("LOD bias", &uniformData.lodBias, 0.0f, (float)m_texture.m_mipLevels)) {
                updateUniformBuffers();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
