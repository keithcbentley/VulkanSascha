/*
 * Vulkan Example - Cube map array texture loading and displaying
 *
 * This sample shows how load and render an cubemap array texture. A single m_vkImage contains multiple cube maps.
 * The cubemap to be displayed is selected in the fragment shader
 *
 * Copyright (C) 2020-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanglTFModel.h"
#include "vulkanexamplebase.h"
#include <ktx.h>
#include <ktxvulkan.h>

vkcpp::VulkanContext vkcpp::s_vulkanContext;

class VulkanExample : public VulkanExampleBase {
public:
    bool displaySkybox = true;

    vks::Texture cubeMapArray;

    struct Meshes {
        vkglTF::Model skybox;
        std::vector<vkglTF::Model> objects;
        int32_t objectIndex = 0;
    } models;

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::mat4 inverseModelview;
        float lodBias = 0.0f;
        // Used by the fragment shader to select the cubemap from the array cubemap
        int cubeMapIndex = 1;
    } uniformData;

    vks::Buffer uniformBuffer;

	vkcpp::PipelineLayout m_pipelineLayout;
	vkcpp::GraphicsPipeline m_pipelineSkybox;
    vkcpp::GraphicsPipeline m_pipelineReflect;

	vkcpp::DescriptorSetLayout m_descriptorSetLayout;
	vkcpp::DescriptorSet m_descriptorSet;

    std::vector<std::string> objectNames;

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Cube map texture arrays";
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -4.0f));
        camera.setRotationSpeed(0.25f);
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
    }

    ~VulkanExample()
    {
        if (m_device) {
            vkDestroyImageView(m_device, cubeMapArray.m_vkImageView, nullptr);
            vkDestroyImage(m_device, cubeMapArray.m_vkImage, nullptr);
            vkDestroySampler(m_device, cubeMapArray.m_vkSampler, nullptr);
            vkFreeMemory(m_device, cubeMapArray.m_vkDeviceMemory, nullptr);
        }
    }

    // Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
    virtual void getEnabledFeatures() {
        //// This sample requires support for cube map arrays
        // if (m_vkPhysicalDeviceFeatures.imageCubeArray) {
        //	m_vkPhysicalDeviceFeatures10.imageCubeArray = VK_TRUE;
        // } else {
        //	vks::tools::exitFatal("Selected GPU does not support cube map arrays!", VK_ERROR_FEATURE_NOT_PRESENT);
        // }
        // m_vkPhysicalDeviceFeatures10.imageCubeArray = VK_TRUE;
        // if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
        //	m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
        // }
    };

    // Loads a cubemap array from a file, uploads it to the m_vkDevice and create all Vulkan resources required to display it
    void loadCubemapArray(std::string filename, VkFormat format)
    {
        ktxResult result;
        ktxTexture* ktxTexture;

#if defined(__ANDROID__)
        // Textures are stored inside the apk on Android (compressed)
        // So they need to be loaded via the asset manager
        AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
        if (!asset) {
            vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
        }
        size_t size = AAsset_getLength(asset);
        assert(size > 0);

        ktx_uint8_t* textureData = new ktx_uint8_t[size];
        AAsset_read(asset, textureData, size);
        AAsset_close(asset);
        result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
        delete[] textureData;
#else
        if (!vks::tools::fileExists(filename)) {
            vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
        }
        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
#endif
        assert(result == KTX_SUCCESS);

        // Get m_vkPhysicalDeviceProperties required for using and upload texture data from the ktx texture object
        cubeMapArray.width = ktxTexture->baseWidth;
        cubeMapArray.height = ktxTexture->baseHeight;
        cubeMapArray.mipLevels = ktxTexture->numLevels;
        cubeMapArray.layerCount = ktxTexture->numLayers;
        ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

        vks::Buffer sourceData;

        // Create a host-visible source buffer that contains the raw m_vkImage data
        VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
        bufferCreateInfo.size = ktxTextureSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		sourceData.m_buffer = vkcpp::Buffer(bufferCreateInfo);
        //VK_CHECK_RESULT(vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &sourceData.m_vkBuffer));

        // Get m_vkDeviceMemory requirements for the source buffer (alignment, m_vkDeviceMemory type bits)
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_device, sourceData.m_buffer, &memReqs);
        VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
        memAllocInfo.allocationSize = memReqs.size;
        // Get m_vkDeviceMemory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex
			= vkcpp::findMemoryTypeIndex(
				memReqs.memoryTypeBits,
				vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT);
		sourceData.m_deviceMemory = vkcpp::DeviceMemory(memAllocInfo);
        VK_CHECK_RESULT(vkBindBufferMemory(m_device, sourceData.m_buffer, sourceData.m_deviceMemory, 0));

        // Copy the ktx m_vkImage data into the source buffer
        uint8_t* data;
        VK_CHECK_RESULT(vkMapMemory(m_device, sourceData.m_deviceMemory, 0, memReqs.size, 0, (void**)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(m_device, sourceData.m_deviceMemory);

        // Create optimal tiled target m_vkImage
        // VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        VkImageCreateInfo imageCreateInfo {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = cubeMapArray.mipLevels;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { cubeMapArray.width, cubeMapArray.height, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        // Cube faces count as array layers in Vulkan
        imageCreateInfo.arrayLayers = 6 * cubeMapArray.layerCount;
        // This flag is required for cube map images
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        VK_CHECK_RESULT(vkCreateImage(m_device, &imageCreateInfo, nullptr, &cubeMapArray.m_vkImage));

        // Allocate m_vkDeviceMemory for the cube map array m_vkImage
        vkGetImageMemoryRequirements(m_device, cubeMapArray.m_vkImage, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex
			= vkcpp::findMemoryTypeIndex(memReqs.memoryTypeBits, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &cubeMapArray.m_vkDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(m_device, cubeMapArray.m_vkImage, cubeMapArray.m_vkDeviceMemory, 0));

        /*
                We now copy the parts that make up the cube map array to our m_vkImage via a command buffer
                Cube map arrays in ktx are stored with a layout like this:
                - Mip Level 0
                        - Layer 0 (= Cube map 0)
                                - Face +X
                                - Face -X
                                - Face +Y
                                - Face -Y
                                - Face +Z
                                - Face -Z
                        - Layer 1 (= Cube map 1)
                                - Face +X
                                ...
                - Mip Level 1
                        - Layer 0 (= Cube map 0)
                                - Face +X
                                ...
                        - Layer 1 (= Cube map 1)
                                - Face +X
                                ...
        */

        VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Setup buffer copy regions for each face including all of its miplevels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;
        for (uint32_t face = 0; face < 6; face++) {
            for (uint32_t layer = 0; layer < ktxTexture->numLayers; layer++) {
                for (uint32_t level = 0; level < ktxTexture->numLevels; level++) {
                    ktx_size_t offset;
                    KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, level, layer, face, &offset);
                    assert(ret == KTX_SUCCESS);
                    VkBufferImageCopy bufferCopyRegion = {};
                    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    bufferCopyRegion.imageSubresource.mipLevel = level;
                    bufferCopyRegion.imageSubresource.baseArrayLayer = layer * 6 + face;
                    bufferCopyRegion.imageSubresource.layerCount = 1;
                    bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
                    bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
                    bufferCopyRegion.imageExtent.depth = 1;
                    bufferCopyRegion.bufferOffset = offset;
                    bufferCopyRegions.push_back(bufferCopyRegion);
                }
            }
        }

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = cubeMapArray.mipLevels;
        subresourceRange.layerCount = 6 * cubeMapArray.layerCount;

        // Transition target m_vkImage to accept the writes from our buffer to m_vkImage copies
        vks::tools::setImageLayout(copyCmd, cubeMapArray.m_vkImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

        // Copy the cube map array buffer parts from the staging buffer to the optimal tiled m_vkImage
        vkCmdCopyBufferToImage(
            copyCmd,
            sourceData.m_buffer,
            cubeMapArray.m_vkImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Transition m_vkImage to shader read layout
        cubeMapArray.m_vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vks::tools::setImageLayout(copyCmd, cubeMapArray.m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cubeMapArray.m_vkImageLayout, subresourceRange);

        m_pVulkanDevice->flushCommandBuffer(copyCmd, m_queue, true);

        // Create sampler
        VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV = sampler.addressModeU;
        sampler.addressModeW = sampler.addressModeU;
        sampler.mipLodBias = 0.0f;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        sampler.maxLod = static_cast<float>(cubeMapArray.mipLevels);
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler.maxAnisotropy = 1.0f;
        // if (m_pVulkanDevice->m_vkPhysicalDeviceFeatures.samplerAnisotropy)
        //{
        //	sampler.maxAnisotropy = m_pVulkanDevice->m_vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
        //	sampler.anisotropyEnable = VK_TRUE;
        // }
        VK_CHECK_RESULT(vkCreateSampler(m_device, &sampler, nullptr, &cubeMapArray.m_vkSampler));

        // Create the m_vkImage m_vkImageView for a cube map array
        VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
        view.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        view.format = format;
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 6 * cubeMapArray.layerCount;
        view.subresourceRange.levelCount = cubeMapArray.mipLevels;
        view.image = cubeMapArray.m_vkImage;
        VK_CHECK_RESULT(vkCreateImageView(m_device, &view, nullptr, &cubeMapArray.m_vkImageView));

        // Clean up staging resources
        //vkFreeMemory(m_device, sourceData.m_vkMemory, nullptr);
        //vkDestroyBuffer(m_device, sourceData.m_vkBuffer, nullptr);
        ktxTexture_Destroy(ktxTexture);
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
            // Set target frame buffer
            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			vkcpp::CommandBuffer commandBuffer = vkcpp::CommandBuffer::makeCopy(m_drawCommandBuffers[i]);
			commandBuffer.begin(cmdBufInfo);
			commandBuffer.cmdBeginRenderPass(renderPassBeginInfo);
			commandBuffer.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight);
			commandBuffer.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight);
			commandBuffer.cmdBindDescriptorSet(m_descriptorSet, m_pipelineLayout);

            // Skybox
            if (displaySkybox) {
				commandBuffer.cmdBindPipeline(m_pipelineSkybox);
                models.skybox.draw(commandBuffer);
            }

            // 3D object
			commandBuffer.cmdBindPipeline(m_pipelineReflect);
            models.objects[models.objectIndex].draw(commandBuffer);

            drawUI(commandBuffer);

			commandBuffer.cmdEndRenderPass();
			commandBuffer.end();
        }
    }

    void loadAssets()
    {
        uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
        // Skybox
        models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
        // Objects
        std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
        objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };
        models.objects.resize(filenames.size());
        for (size_t i = 0; i < filenames.size(); i++) {
            models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], m_pVulkanDevice, m_queue, glTFLoadingFlags);
        }
        // Load the cube map array from a ktx texture file
        loadCubemapArray(getAssetPath() + "textures/cubemap_array.ktx", VK_FORMAT_R8G8B8A8_UNORM);
    }

    void setupDescriptors()
    {
        // Descriptor Pool
		vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
		descriptorPoolCreateInfo
			.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
			.addDescriptorCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo);

        // Descriptor Set Layout
		constexpr int uniformBufferBindingIndex = 0;
		constexpr int combinedImageSamplerBindingIndex = 1;

		vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
		descriptorSetLayoutCreateInfo
			.addDescriptorSetLayoutBinding(
				uniformBufferBindingIndex,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				vkcpp::SHADER_STAGE_VERTEX | vkcpp::SHADER_STAGE_FRAGMENT)
			.addDescriptorSetLayoutBinding(
				combinedImageSamplerBindingIndex,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				vkcpp::SHADER_STAGE_FRAGMENT);
		m_descriptorSetLayout = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

        // Descriptor Set
		m_descriptorSet = vkcpp::DescriptorSet(m_descriptorSetLayout, m_descriptorPool);

		// Update Descriptor Set
		vkcpp::WriteDescriptorSetArray writeDescriptorSetArray(m_descriptorSet);
		writeDescriptorSetArray
			.addBufferWriteDescriptor(
				uniformBufferBindingIndex,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				uniformBuffer.m_vkDescriptorBufferInfo)
			.addImageWriteDescriptor(
				combinedImageSamplerBindingIndex,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				cubeMapArray.m_vkImageView,
				cubeMapArray.m_vkImageLayout,
				cubeMapArray.m_vkSampler);
		writeDescriptorSetArray.updateDescriptorSets();

    }

    void preparePipelines()
    {
        // Pipeline Layout
		vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayout);

		m_pipelineLayout = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

        // Pipelines
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_pipelineLayout, m_renderPassOriginal, 0);
        pipelineCI.pInputAssemblyState = &inputAssemblyState;
        pipelineCI.pRasterizationState = &rasterizationState;
        pipelineCI.pColorBlendState = &colorBlendState;
        pipelineCI.pMultisampleState = &multisampleState;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pDepthStencilState = &depthStencilState;
        pipelineCI.pDynamicState = &dynamicState;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal });

        // Skybox pipeline (background cube)
        shaderStages[0] = loadShader(getShadersPath() + "texturecubemaparray/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "texturecubemaparray/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		m_pipelineSkybox = vkcpp::GraphicsPipeline(pipelineCI);

        // Cube map reflect pipeline
        shaderStages[0] = loadShader(getShadersPath() + "texturecubemaparray/reflect.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "texturecubemaparray/reflect.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        // Enable depth test and write
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthTestEnable = VK_TRUE;
        // Flip cull mode
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		m_pipelineReflect = vkcpp::GraphicsPipeline(pipelineCI);
    }

    void prepareUniformBuffers()
    {
        // Object vertex shader uniform buffer
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
			&uniformBuffer, sizeof(UniformData)));
        // Map persistent
        VK_CHECK_RESULT(uniformBuffer.map());
    }

    void updateUniformBuffers()
    {
        uniformData.projection = camera.matrices.perspective;
        uniformData.modelView = camera.matrices.view;
        uniformData.inverseModelview = glm::inverse(camera.matrices.view);
        memcpy(uniformBuffer.m_pMapped, &uniformData, sizeof(UniformData));
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

    void draw()
    {
        VulkanExampleBase::prepareFrame();
        m_vkSubmitInfo.commandBufferCount = 1;
        VkCommandBuffer vkCommandBuffer = m_drawCommandBuffers[m_currentBufferIndex];
        m_vkSubmitInfo.pCommandBuffers = &vkCommandBuffer;
        VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
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
            overlay->sliderInt("Cube map", &uniformData.cubeMapIndex, 0, cubeMapArray.layerCount - 1);
            overlay->sliderFloat("LOD bias", &uniformData.lodBias, 0.0f, (float)cubeMapArray.mipLevels);
            if (overlay->comboBox("Object type", &models.objectIndex, objectNames)) {
                buildCommandBuffers();
            }
            if (overlay->checkBox("Skybox", &displaySkybox)) {
                buildCommandBuffers();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
