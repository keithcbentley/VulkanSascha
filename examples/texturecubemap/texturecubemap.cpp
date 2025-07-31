/*
 * Vulkan Example - Cube map texture loading and displaying
 *
 * This sample shows how to load and render a cubemap. A cubemap is a textures that contains 6 images, one per cube face.
 * The sample displays the cubemap as a skybox (background) and as a reflection on a selectable object
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
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

	struct
	{
		uint32_t	m_mipLevelsForUI = 0;
		float		m_lodBiasFromUI = 0.0;
		int32_t		m_objectIndexFromUI = 0;
		bool		m_displaySkybox = true;
	} m_uiData;

    vkcpp::Texture m_textureCubeMap;

    struct Models {
        vkglTF::Model skybox;
        // The sample lets you select different models to apply the cubemap to
        std::vector<vkglTF::Model> objects;
    } models;

    struct UBOVS {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::mat4 inverseModelview;
        float lodBias = 0.0f;
    } uboVS;

    vks::Buffer uniformBuffer;

    vkcpp::PipelineLayout m_pipelineLayout;
    vkcpp::GraphicsPipeline m_pipelineSkybox;
    vkcpp::GraphicsPipeline m_pipelineReflect;

    vkcpp::DescriptorSetLayout m_descriptorSetLayout;
    vkcpp::DescriptorSet m_descriptorSet;

    std::vector<std::string> m_objectNames;

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Cube map textures";
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -4.0f));
        camera.setRotation(glm::vec3(0.0f));
        camera.setRotationSpeed(0.25f);
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
    }

	~VulkanExample() = default;

    // Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
    virtual void getEnabledFeatures()
    {
        // if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
        //	m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
        // }
    }

    // Loads a cubemap from a file, uploads it to the m_vkDevice and create all Vulkan resources required to display it
    void loadCubemap(std::string filename, VkFormat format)
    {
        ktxResult result;
        ktxTexture* ktxTexture;

        if (!vks::tools::fileExists(filename)) {
            vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
        }
        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
        assert(result == KTX_SUCCESS);


		//	Handy shorthand.
		const uint32_t textureCubeMapWidth = ktxTexture->baseWidth;
		const uint32_t textureCubeMapHeight = ktxTexture->baseHeight;
		const uint32_t textureCubeMapMipLevelCount = ktxTexture->numLevels;
		const uint8_t* const pTextureCubeMapData = ktxTexture_GetData(ktxTexture);
		const uint64_t textureCubeMapSize = ktxTexture_GetSize(ktxTexture);
		m_uiData.m_mipLevelsForUI = textureCubeMapMipLevelCount;

		vkcpp::Buffer_DeviceMemory<> newStagingBuffer
			= vkcpp::Buffer_DeviceMemory<>::withCopyUnmap(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				vkcpp::TypedCount<>(ktxTexture_GetSize(ktxTexture)),
				0,
				vkcpp::MEMORY_PROPERTY_HOST_VISIBLE_COHERENT,
				ktxTexture_GetData(ktxTexture));



        VkImageCreateInfo imageCreateInfo {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = textureCubeMapMipLevelCount;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { textureCubeMapWidth, textureCubeMapHeight, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        // Cube faces count as array layers in Vulkan
		//	TODO: kind of a magic number.
		//	Cubes always have 6 sides, but we aren't doing any checking
		//	on the texture that we loaded.
        imageCreateInfo.arrayLayers = 6;
        // This flag is required for cube map images
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		m_textureCubeMap.takeImage(vkcpp::Image(imageCreateInfo));
		m_textureCubeMap.allocateBindImageMemory(vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);

        VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Setup buffer copy regions for each face including all of its miplevels
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;

        for (uint32_t faceLayer = 0; faceLayer < 6; faceLayer++) {
            for (uint32_t mipLevel = 0; mipLevel < textureCubeMapMipLevelCount; mipLevel++) {
                // Calculate offset into staging buffer for the current mip level and face
                ktx_size_t offset;
                KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, mipLevel, 0, faceLayer, &offset);
                assert(ret == KTX_SUCCESS);
                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = mipLevel;
                bufferCopyRegion.imageSubresource.baseArrayLayer = faceLayer;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = textureCubeMapWidth >> mipLevel;
                bufferCopyRegion.imageExtent.height = textureCubeMapHeight >> mipLevel;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;
                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        // Image barrier for optimal m_vkImage (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = textureCubeMapMipLevelCount;
        subresourceRange.layerCount = 6;

        vks::tools::setImageLayout(
            copyCmd,
            m_textureCubeMap.image(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy the cube map faces from the staging buffer to the optimal tiled m_vkImage
        vkCmdCopyBufferToImage(
            copyCmd,
            newStagingBuffer.buffer(),
            m_textureCubeMap.image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Change texture m_vkImage layout to shader read after all faces have been copied
        m_textureCubeMap.setVkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vks::tools::setImageLayout(
			copyCmd,
			m_textureCubeMap.image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_textureCubeMap.vkImageLayout(),
            subresourceRange);

        m_pVulkanDevice->flushCommandBuffer(copyCmd, m_queue, true);

        // Create sampler
		VkSamplerCreateInfo vkSamplerCreateInfo = vks::initializers::samplerCreateInfo();
		vkSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		vkSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		vkSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		vkSamplerCreateInfo.addressModeV = vkSamplerCreateInfo.addressModeU;
		vkSamplerCreateInfo.addressModeW = vkSamplerCreateInfo.addressModeU;
		vkSamplerCreateInfo.mipLodBias = 0.0f;
		vkSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		vkSamplerCreateInfo.minLod = 0.0f;
		vkSamplerCreateInfo.maxLod = static_cast<float>(textureCubeMapMipLevelCount);
		vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkSamplerCreateInfo.maxAnisotropy = 1.0f;
        // if (m_pVulkanDevice->m_vkPhysicalDeviceFeatures.samplerAnisotropy)
        //{
        //	sampler.maxAnisotropy = m_pVulkanDevice->m_vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy;
        //	sampler.anisotropyEnable = VK_TRUE;
        // }
		m_textureCubeMap.takeSampler(vkcpp::Sampler(vkSamplerCreateInfo));

		vkcpp::ImageViewCreateInfo imageViewCreateInfo(
			m_textureCubeMap.image(),
			VK_IMAGE_VIEW_TYPE_CUBE,
			format,
			VK_IMAGE_ASPECT_COLOR_BIT);

        // 6 array layers (faces)
		imageViewCreateInfo.subresourceRange.layerCount = 6;
        // Set number of mip levels
		imageViewCreateInfo.subresourceRange.levelCount = textureCubeMapMipLevelCount;
		m_textureCubeMap.takeImageView(vkcpp::ImageView(imageViewCreateInfo));

        // Clean up staging resources
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
            vkcpp::CommandBuffer commandBuffer = vkcpp::CommandBuffer::makeCopy(m_drawCommandBuffers[i]);

            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];
            commandBuffer
                .begin(cmdBufInfo)
                .cmdBeginRenderPass(renderPassBeginInfo)
                .cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight)
                .cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight)
                .cmdBindDescriptorSet(m_descriptorSet, m_pipelineLayout);

            if (m_uiData.m_displaySkybox) {
                commandBuffer.cmdBindPipeline(m_pipelineSkybox);
                models.skybox.draw(commandBuffer);
            }

            // 3D object
            commandBuffer.cmdBindPipeline(m_pipelineReflect);
            models.objects[m_uiData.m_objectIndexFromUI].draw(commandBuffer);

            drawUI(commandBuffer);

            commandBuffer
                .cmdEndRenderPass()
                .end();
        }
    }

    void loadAssets()
    {
        uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
        // Skybox
        models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
        // Objects
        std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
        m_objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };
        models.objects.resize(filenames.size());
        for (size_t i = 0; i < filenames.size(); i++) {
            models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], m_pVulkanDevice, m_queue, glTFLoadingFlags);
        }
        // Cubemap texture
        loadCubemap(getAssetPath() + "textures/cubemap_yokohama_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM);
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

        vkcpp::WriteDescriptorSetArray writeDescriptorSetArray(m_descriptorSet);
		writeDescriptorSetArray
            .addBufferWriteDescriptor(
                uniformBufferBindingIndex,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                uniformBuffer.m_vkDescriptorBufferInfo)
            .addImageWriteDescriptor(
                combinedImageSamplerBindingIndex,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                m_textureCubeMap.imageView(),
				m_textureCubeMap.vkImageLayout(),
				m_textureCubeMap.sampler());
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
        VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        VkGraphicsPipelineCreateInfo pipelineCI
            = vks::initializers::pipelineCreateInfo(m_pipelineLayout, m_renderPassOriginal, 0);
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
        shaderStages[0] = loadShader(getShadersPath() + "texturecubemap/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "texturecubemap/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		m_pipelineSkybox = vkcpp::GraphicsPipeline(pipelineCI);

        // Cube map reflect pipeline
        shaderStages[0] = loadShader(getShadersPath() + "texturecubemap/reflect.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "texturecubemap/reflect.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        // Enable depth test and write
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthTestEnable = VK_TRUE;
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		m_pipelineReflect = vkcpp::GraphicsPipeline(pipelineCI);
    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
			&uniformBuffer, sizeof(uboVS)));
        VK_CHECK_RESULT(uniformBuffer.map());
    }

    void updateUniformBuffers()
    {
        uboVS.projection = camera.matrices.perspective;
        // Note: Both the object and skybox use the same uniform data,
		// the translation part of the skybox is removed in the shader (see skybox.vert)
        uboVS.modelView = camera.matrices.view;
        uboVS.inverseModelview = glm::inverse(camera.matrices.view);
        memcpy(uniformBuffer.m_pMapped, &uboVS, sizeof(uboVS));
		uboVS.lodBias = m_uiData.m_lodBiasFromUI;
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
            if (overlay->sliderFloat("LOD bias", &m_uiData.m_lodBiasFromUI, 0.0f, (float)m_uiData.m_mipLevelsForUI)) {
                updateUniformBuffers();
            }
            if (overlay->comboBox("Object type", &m_uiData.m_objectIndexFromUI, m_objectNames)) {
                buildCommandBuffers();
            }
            if (overlay->checkBox("Skybox", &m_uiData.m_displaySkybox)) {
                buildCommandBuffers();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()