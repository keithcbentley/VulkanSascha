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

vkcpp::AppContext vkcpp::s_appContext;


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
        vkcpp::Sampler m_sampler;
        vkcpp::Image m_image;
        VkImageLayout m_vkImageLayout;
        vkcpp::DeviceMemory m_deviceMemory;
        vkcpp::ImageView m_imageView;
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
        if (m_device) {
            vkDestroyPipeline(m_device, m_vkPipeline, nullptr);
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
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &formatProperties);
            useStaging = !(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        }

//		useStaging = false;
        if (useStaging) {

			vkcpp::Buffer_DeviceMemory stagingBufferAndMemory
				= vkcpp::Buffer_DeviceMemory::withCopy(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					ktxTextureSize,
					0, //	Queue family index.  Does this matter?
					vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
					ktxTextureData);
            stagingBufferAndMemory.unmapMemory();

            //	Setup buffer copy regions for each mip level.
            //	We use this later when we finally copy the staging data
            //	to the properly layed out image memory.
            //	TODO: need to make a smart version to hold multiple
            std::vector<VkBufferImageCopy> bufferCopyRegions;
            for (uint32_t mipLevel = 0; mipLevel < m_texture.m_mipLevels; mipLevel++) {
                // Calculate offset into staging buffer for the current mip level.
                //	Note that we are using offsets.  We get the offset from the original
                //	memory, and then use it as the offset from the staging buffer base
                //	when doing the copy.  This works because the staging buffer memory
                //	is an exact copy of the original memory.
                ktx_size_t offset;
                KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, mipLevel, 0, 0, &offset);
                assert(ret == KTX_SUCCESS);
                // Setup a buffer m_vkImage copy structure for the current mip level
                vkcpp::BufferImageCopy bufferCopyRegion;
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageSubresource.mipLevel = mipLevel;
                //	TODO: this is screwy.  Is this some magic value stuff with widths and heights?
                bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> mipLevel;
                bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> mipLevel;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;
                bufferCopyRegions.emplace_back(bufferCopyRegion);
            }

            // Create optimal tiled target image on the m_vkDevice
            VkImageCreateInfo imageCreateInfo {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.format = format;
            imageCreateInfo.mipLevels = m_texture.m_mipLevels;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCreateInfo.extent = { m_texture.m_width, m_texture.m_height, 1 };
            imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            m_texture.m_image = vkcpp::Image(imageCreateInfo);

            VkMemoryRequirements vkMemoryRequirementsImage = m_texture.m_image.getMemoryRequirements();
            m_texture.m_deviceMemory = vkcpp::DeviceMemory(
                vkMemoryRequirementsImage, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);

            VK_CHECK_RESULT(vkBindImageMemory(m_device, m_texture.m_image, m_texture.m_deviceMemory, 0));

            VkCommandBuffer vkcb = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            vkcpp::CommandBuffer commandBuffer = vkcpp::CommandBuffer::makeCopy(vkcb);

            // Image barriers for the texture m_vkImage

            // The sub resource range describes the regions of the m_vkImage that will be transitioned using the m_vkDeviceMemory barriers below
            // Image only contains color data
            // Start at first mip level
            // We will transition on all mip levels
            // The 2D texture only has one layer
            VkImageSubresourceRange vkImageSubresourceRange = {};
            vkImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vkImageSubresourceRange.baseMipLevel = 0;
            vkImageSubresourceRange.levelCount = m_texture.m_mipLevels;
            vkImageSubresourceRange.layerCount = 1;

            //	Use an image memory barrier to transition the texture image layout to transfer target
            //	so we can copy our buffer data to it.
            vkcpp::ImageMemoryBarrier imageMemoryBarrier;
            imageMemoryBarrier
                .setImage(m_texture.m_image)
                .setOldNewImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                .setSubresourceRange(vkImageSubresourceRange)
                .setSrcDstAccessMask(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT)
                .setSrcDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);

            //	Transform the image layout so we can transfer to it.
            //	Note the pipeline stages are from host (memory?) to the transfer stage.
            commandBuffer.cmdPipelineBarrierImageMemory(
                imageMemoryBarrier, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

            // Now do the actual memory transfer from staging buffer memory to the image memory.
            commandBuffer.cmdCopyBufferToImage(
                stagingBufferAndMemory.m_buffer,
                m_texture.m_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(bufferCopyRegions.size()),
                bufferCopyRegions.data());

            //	Now do another another layout transition so the shader can be sample the image.
            imageMemoryBarrier
                .setSrcDstAccessMask(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
                .setOldNewImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            //	Note the pipeline stage goes from the transfer stage to the fragment shader stage.
            commandBuffer.cmdPipelineBarrierImageMemory(
                imageMemoryBarrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            // Store current layout for later reuse
            m_texture.m_vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            m_pVulkanDevice->flushCommandBuffer(commandBuffer, m_vkQueue, true);

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
            vkcpp::Image mappableImage = vkcpp::Image(imageCreateInfo);

            VkMemoryRequirements vkMemoryRequirementsImage = mappableImage.getMemoryRequirements();
            vkcpp::DeviceMemory stagingMemory = vkcpp::DeviceMemory(
                vkMemoryRequirementsImage, vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT);

            vkcpp::DeviceMemory mappableMemory = vkcpp::DeviceMemory(
                vkMemoryRequirementsImage, vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT);

            mappableImage.bindImageMemory(mappableMemory);
			mappableMemory.mapCopyUnmap(ktxTextureData, vkMemoryRequirementsImage.size);

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
        VkSamplerCreateInfo vkSamplerCreateInfo = vks::initializers::samplerCreateInfo();
        vkSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        vkSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        vkSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        vkSamplerCreateInfo.mipLodBias = 0.0f;
        vkSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        vkSamplerCreateInfo.minLod = 0.0f;
        // Set max level-of-detail to mip level count of the texture
        vkSamplerCreateInfo.maxLod = (useStaging) ? (float)m_texture.m_mipLevels : 0.0f;
        // Enable anisotropic filtering
        // This feature is optional, so we must check if it's supported on the m_vkDevice
        if (m_physicalDeviceFeatures.m_features2.features.samplerAnisotropy) {
            // Use max. level of anisotropy for this example
            vkSamplerCreateInfo.maxAnisotropy = m_physicalDeviceProperties.m_properties2.properties.limits.maxSamplerAnisotropy;
            vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
        } else {
            // The m_vkDevice does not support anisotropic filtering
            vkSamplerCreateInfo.maxAnisotropy = 1.0;
            vkSamplerCreateInfo.anisotropyEnable = VK_FALSE;
        }
        vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        m_texture.m_sampler = vkcpp::Sampler(vkSamplerCreateInfo);
        // VK_CHECK_RESULT(vkCreateSampler(m_deviceOriginal, &sampler, nullptr, &m_texture.m_vkSampler));

        // Create m_vkImage m_vkImageView
        // Textures are not directly accessed by the shaders and
        // are abstracted by m_vkImage views containing additional
        // information and sub resource ranges
        VkImageViewCreateInfo vkImageViewCreateInfo = vks::initializers::imageViewCreateInfo();
        vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vkImageViewCreateInfo.format = format;
        // The subresource range describes the set of mip levels (and array layers) that can be accessed through this m_vkImage m_vkImageView
        // It's possible to create multiple m_vkImage views for a single m_vkImage referring to different (and/or overlapping) ranges of the m_vkImage
        vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        vkImageViewCreateInfo.subresourceRange.layerCount = 1;
        // Linear tiling usually won't support mip maps
        // Only set mip map count if optimal tiling is used
        vkImageViewCreateInfo.subresourceRange.levelCount = (useStaging) ? m_texture.m_mipLevels : 1;
        // The m_vkImageView will be based on the texture's m_vkImage
        vkImageViewCreateInfo.image = m_texture.m_image;
        m_texture.m_imageView = vkcpp::ImageView(vkImageViewCreateInfo);
        // VK_CHECK_RESULT(vkCreateImageView(m_deviceOriginal, &view, nullptr, &m_texture.m_vkImageView));
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
        vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
        descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
        descriptorPoolCreateInfo.setMaxSets(2);
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo);

        // Layout
        constexpr int uniformBufferIndex = 0;
        constexpr int combinedImageSamplerIndex = 1;
        vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
        descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(
            uniformBufferIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vkcpp::SHADER_STAGE_VERTEX);
        descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(
            combinedImageSamplerIndex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vkcpp::SHADER_STAGE_FRAGMENT);
        m_descriptorSetLayout = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

        // Set
        m_descriptorSet = vkcpp::DescriptorSet(m_descriptorSetLayout, m_descriptorPool);
        // Setup a descriptor m_vkImage info for the current texture to be used as a combined m_vkImage sampler
        VkDescriptorImageInfo textureDescriptor;
        // The m_vkImage's m_vkImageView (images are never directly accessed by the shader, but rather through views defining subresources)
        textureDescriptor.imageView = m_texture.m_imageView;
        // The sampler (Telling the m_vkPipeline how to sample the texture, including repeat, border, etc.)
        textureDescriptor.sampler = m_texture.m_sampler;
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
            textureDescriptor);

        descriptorSetUpdater.updateDescriptorSets();
    }

    void preparePipelines()
    {
        // Layout
        vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayout);

        m_pipelineLayout = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

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
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_vkPipelineCache, 1, &pipelineCreateInfo, nullptr, &m_vkPipeline));
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
