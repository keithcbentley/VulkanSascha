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

vkcpp::VulkanContext vkcpp::s_vulkanContext;

// Vertex layout for this example
struct Vertex {
    float pos[3];
    float uv[2];
    float normal[3];
};

class VulkanExample : public VulkanExampleBase {
public:
    struct
    {
        float m_mipLevelsForUI = 0.0;
        float m_lodBiasFromUI = 0.0;
    } m_uiData;


	vkcpp::Texture m_texture;

    vks::Buffer m_vertexBuffer;
    vks::Buffer m_indexBuffer;
    uint32_t m_indexCount { 0 };

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec4 viewPos;
        // This is used to change the bias for the level-of-detail (mips) in the fragment shader
        float lodBias = 0.0f;
    } m_uniformData;

    vks::Buffer m_uniformBuffer;

    vkcpp::GraphicsPipeline m_pipeline;
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

    ~VulkanExample() = default;

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

        //	Handy shorthand.
        const uint32_t textureWidth = ktxTexture->baseWidth;
        const uint32_t textureHeight = ktxTexture->baseHeight;
        const uint32_t textureMipLevelCount = ktxTexture->numLevels;
        const uint8_t* const pTextureData = ktxTexture_GetData(ktxTexture);
        const uint64_t textureSize = ktxTexture_GetSize(ktxTexture);
		m_uiData.m_mipLevelsForUI = ktxTexture->numLevels;



        vkcpp::Buffer_DeviceMemory<> stagingBufferAndMemory
            = vkcpp::Buffer_DeviceMemory<>::withCopyUnmap(
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                vkcpp::TypedCount<>(textureSize),
                0, //	Queue family index.  Does this matter?
                vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
                pTextureData);

        //	Setup buffer copy regions for each mip level.
        //	We use this later when we finally copy the staging data
        //	to the properly layed out image memory.
        //	TODO: need to make a smart version to hold multiple
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t mipLevel = 0; mipLevel < textureMipLevelCount; mipLevel++) {
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

            //	Image size shrinks by a factor of 2 for each mip level.
            bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> mipLevel;
            bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> mipLevel;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;
            bufferCopyRegions.emplace_back(bufferCopyRegion);
        }

        // Create optimal tiled target image on the m_vkDevice
		//	TODO: maybe have the texture just take the create info.
        VkImageCreateInfo imageCreateInfo {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = textureMipLevelCount;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { textureWidth, textureHeight, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        m_texture.takeImage(vkcpp::Image(imageCreateInfo));
		m_texture.allocateBindImageMemory(vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);


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
        vkImageSubresourceRange.levelCount = textureMipLevelCount;
        vkImageSubresourceRange.layerCount = 1;

        //	Use an image memory barrier to transition the texture image layout to transfer target
        //	so we can copy our buffer data to it.
        vkcpp::ImageMemoryBarrier imageMemoryBarrier;
        imageMemoryBarrier
            .setImage(m_texture.image())
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
            stagingBufferAndMemory.buffer(),
            m_texture.image(),
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
        m_texture.setVkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_pVulkanDevice->flushCommandBuffer(commandBuffer, m_queue, true);

        ktxTexture_Destroy(ktxTexture);

        // Create a texture sampler
        // In Vulkan textures are accessed by samplers
        // This separates all the sampling information from the texture data.
        // This means you could have multiple sampler objects for the same texture with different settings
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
        vkSamplerCreateInfo.maxLod = (float)textureMipLevelCount;
        // Enable anisotropic filtering
        // This feature is optional, so we must check if it's supported on the m_vkDevice
        if (vkcpp::vkPhysicalDeviceFeatures().samplerAnisotropy) {
            // Use max. level of anisotropy for this example
            vkSamplerCreateInfo.maxAnisotropy = vkcpp::vkPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
            vkSamplerCreateInfo.anisotropyEnable = VK_TRUE;
        } else {
            // The m_vkDevice does not support anisotropic filtering
            vkSamplerCreateInfo.maxAnisotropy = 1.0;
            vkSamplerCreateInfo.anisotropyEnable = VK_FALSE;
        }
        vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		m_texture.takeSampler(vkcpp::Sampler(vkSamplerCreateInfo));

        // Create Image ImageView
        // Textures are not directly accessed by the shaders and
        // are abstracted by m_vkImage views containing additional
        // information and sub resource ranges
		vkcpp::ImageViewCreateInfo imageViewCreateInfo(m_texture.image(), VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT);
        // The subresource range describes the set of mip levels (and array layers) that can be accessed through this m_vkImage m_vkImageView
        // It's possible to create multiple m_vkImage views for a single m_vkImage referring to different (and/or overlapping) ranges of the m_vkImage
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        // Linear tiling usually won't support mip maps
        // Only set mip map count if optimal tiling is used
        imageViewCreateInfo.subresourceRange.levelCount = textureMipLevelCount;
		m_texture.takeImageView(vkcpp::ImageView(imageViewCreateInfo));
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

        for (int32_t i = 0; i < m_drawCommandBuffers.size(); ++i) {

            vkcpp::CommandBuffer commandBuffer = vkcpp::CommandBuffer::makeCopy(m_drawCommandBuffers[i]);

            // Set target frame buffer
            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

            commandBuffer
                .begin(cmdBufInfo)
                .cmdBeginRenderPass(renderPassBeginInfo)
                .cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight)
                .cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight)
                .cmdBindPipeline(m_pipeline)
                .cmdBindDescriptorSet(m_descriptorSet, m_pipelineLayout)
                .cmdBindVertexBuffer(m_vertexBuffer.m_buffer)
                .cmdBindIndexBuffer(m_indexBuffer.m_buffer, VK_INDEX_TYPE_UINT32)
                .cmdDrawIndexed(m_indexCount);
            drawUI(commandBuffer);
            commandBuffer
                .cmdEndRenderPass()
                .end();
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
            vks::Buffer m_vertices;
            vks::Buffer m_indices;
        } stagingBuffers;

        // Host visible source buffers (staging)
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &stagingBuffers.m_vertices, vertices.size() * sizeof(Vertex), vertices.data()));
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &stagingBuffers.m_indices, indices.size() * sizeof(uint32_t), indices.data()));

        // Device local destination buffers
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL,
            &m_vertexBuffer, vertices.size() * sizeof(Vertex)));
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL,
            &m_indexBuffer, indices.size() * sizeof(uint32_t)));

        // Copy from host do m_vkDevice
        m_pVulkanDevice->copyBuffer(&stagingBuffers.m_vertices, &m_vertexBuffer, m_queue);
        m_pVulkanDevice->copyBuffer(&stagingBuffers.m_indices, &m_indexBuffer, m_queue);
    }

    void setupDescriptors()
    {
        // Pool
        vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
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
        textureDescriptor.imageView = m_texture.imageView();
        // The sampler (Telling the m_vkPipeline how to sample the texture, including repeat, border, etc.)
        textureDescriptor.sampler = m_texture.sampler();
        // The current layout of the m_vkImage(Note: Should always fit the actual use, e.g.shader read)
        textureDescriptor.imageLayout = m_texture.vkImageLayout();

        vkcpp::WriteDescriptorSetArray writeDescriptorSetArray(m_descriptorSet);

		writeDescriptorSetArray.addBufferWriteDescriptor(
            uniformBufferIndex,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            m_uniformBuffer.m_vkDescriptorBufferInfo);

		writeDescriptorSetArray.addImageWriteDescriptor(
            combinedImageSamplerIndex,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            textureDescriptor);

		writeDescriptorSetArray.updateDescriptorSets();
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
        m_pipeline = vkcpp::GraphicsPipeline(pipelineCreateInfo);
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        // Vertex shader uniform buffer block
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &m_uniformBuffer, sizeof(m_uniformData), &m_uniformData));
        VK_CHECK_RESULT(m_uniformBuffer.map());
    }

    void updateUniformBuffers()
    {
        m_uniformData.projection = camera.matrices.perspective;
        m_uniformData.modelView = camera.matrices.view;
        m_uniformData.viewPos = camera.viewPos;
        memcpy(m_uniformBuffer.m_pMapped, &m_uniformData, sizeof(m_uniformData));
		m_uniformData.lodBias = m_uiData.m_lodBiasFromUI;
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
        VkCommandBuffer vkCommandBuffer = m_drawCommandBuffers[m_currentBufferIndex];
        m_vkSubmitInfo.pCommandBuffers = &vkCommandBuffer;
        VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
        VulkanExampleBase::submitFrame();
    }

    void render()
    {
        if (!m_prepared)
            return;
        updateUniformBuffers();
        draw();
    }

    void OnUpdateUIOverlay(vks::UIOverlay* overlay)
    {
        if (overlay->header("Settings")) {
            if (overlay->sliderFloat("LOD bias", &m_uiData.m_lodBiasFromUI, 0.0f, m_uiData.m_mipLevelsForUI)) {
                updateUniformBuffers();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
