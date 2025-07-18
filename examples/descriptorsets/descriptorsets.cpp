/*
 * Vulkan Example - Using descriptor sets for passing data to shader stages
 *
 * Relevant code parts are marked with [POI]
 *
 * Copyright (C) 2018-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanglTFModel.h"
#include "vulkanexamplebase.h"

vkcpp::VulkanContext vkcpp::s_vulkanContext;


class VulkanExample : public VulkanExampleBase {
public:
    bool animate = true;

    struct Cube {
        struct Matrices {
            glm::mat4 projection;
            glm::mat4 view;
            glm::mat4 model;
        } matrices;
        vkcpp::DescriptorSet m_descriptorSet;
        vks::Texture2D texture;
        vks::Buffer uniformBuffer;
        glm::vec3 rotation { 0.0f };
    };
    std::array<Cube, 2> m_cubes;

    vkglTF::Model model;

    VkPipeline m_vkPipeline { VK_NULL_HANDLE };
    vkcpp::PipelineLayout m_pipelineLayout;
    vkcpp::DescriptorSetLayout m_descriptorSetLayout;

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Using descriptor Sets";
        camera.type = Camera::CameraType::lookat;
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 512.0f);
        camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
        camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
    }

    ~VulkanExample()
    {
        for (auto cube : m_cubes) {
            cube.uniformBuffer.destroy();
            cube.texture.destroy();
        }
    }

    virtual void getEnabledFeatures()
    {
        ////if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
        ////	m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
        ////};
    }

    void buildCommandBuffers()
    {
        VkCommandBufferBeginInfo vkCommandBufferBeginInfo = vks::initializers::commandBufferBeginInfo();

        VkClearValue clearValues[2];
        clearValues[0].color = m_vkClearColorValueDefault;
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo vkRenderPassBeginInfo = vks::initializers::renderPassBeginInfo();
        vkRenderPassBeginInfo.renderPass = m_renderPassOriginal;
        vkRenderPassBeginInfo.renderArea.offset.x = 0;
        vkRenderPassBeginInfo.renderArea.offset.y = 0;
        vkRenderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
        vkRenderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
        vkRenderPassBeginInfo.clearValueCount = 2;
        vkRenderPassBeginInfo.pClearValues = clearValues;

        for (int32_t i = 0; i < m_drawCommandBuffers.size(); ++i) {
			vkRenderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

            vkcpp::CommandBuffer commandBuffer = m_drawCommandBuffers[i];
            commandBuffer
				.begin(vkCommandBufferBeginInfo)
				.cmdBeginRenderPass(vkRenderPassBeginInfo)
				.cmdBindPipeline(m_vkPipeline)
				.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight)
				.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight);

            model.bindBuffers(commandBuffer);

            /*
                    [POI] Render cubes with separate descriptor sets
            */
            for (auto cube : m_cubes) {
                // Bind the cube's descriptor set.
				// This tells the command buffer to use the uniform buffer and m_vkImage set for this cube
                commandBuffer.cmdBindDescriptorSet(cube.m_descriptorSet, m_pipelineLayout);
                model.draw(commandBuffer);
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
        model.loadFromFile(getAssetPath() + "models/cube.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
        m_cubes[0].texture.loadFromFile(
			getAssetPath() + "textures/crate01_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_queue);
        m_cubes[1].texture.loadFromFile(
			getAssetPath() + "textures/crate02_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_queue);
    }

    /*
            [POI] Set up descriptor sets and set layout
    */
    void setupDescriptors()
    {
        /*
                Descriptor set layout

                The layout describes the shader bindings and types used
				for a certain descriptor layout and as such must match the shader bindings

                Shader bindings used in this example:
                VS:
                        layout (set = 0, binding = 0) uniform UBOMatrices ...
                FS :
                        layout (set = 0, binding = 1) uniform sampler2D ...;
        */

        //	Binding 0: Uniform buffers (used to pass matrices)
        //	Binding 1: Combined m_vkImage sampler (used to pass per object texture information)
        constexpr int uniformBufferBindingIndex = 0;
        constexpr int combinedImageSamplerBindingIndex = 1;

		//	Note that descriptor set layouts are not tied to a particular descriptor pool.
        vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
        descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(
            uniformBufferBindingIndex,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            vkcpp::SHADER_STAGE_VERTEX);
        descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(
            combinedImageSamplerBindingIndex,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            vkcpp::SHADER_STAGE_FRAGMENT);

        m_descriptorSetLayout = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

        /*
                Descriptor pool

                Actual descriptors are allocated from a descriptor pool telling the driver what types and how many
                descriptors this application will use

                An application can have multiple pools (e.g. for multiple threads) with any number of descriptor types
                as long as m_vkDevice limits are not surpassed

                It's good practice to allocate pools with actually required descriptor types and counts
        */

        //	We need a set for each cube.
		//	Each set will have one uniform buffer and one combined image sampler.
        vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_cubes.size());
        descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_cubes.size());
        descriptorPoolCreateInfo.setMaxSets(m_cubes.size());

        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo);

        /*

                Descriptor sets

                Using the shared descriptor set layout and the descriptor pool,
				we will now allocate the descriptor sets.

                Descriptor sets contain the actual descriptor
				for the objects (buffers, images) used at render time.

        */

        for (auto& cube : m_cubes) {

            //	Make a descriptor set.
			//	Remember that the descriptor set is still just a framework.
			//	It's not usable until we connect the actual device resources
			//	to the descriptor set.  Vulkan calls this "updating" the descriptor set.
            cube.m_descriptorSet = vkcpp::DescriptorSet(m_descriptorSetLayout, m_descriptorPool);

            //	Write (update) the actual resources into the descriptor set.
			//	TODO: could we make a sort of "prebound" updater that is bound
			//	to a particular descriptor set at creation.  It would make adding
			//	the descriptors a bit tidier.
            vkcpp::DescriptorSetUpdater descriptorSetUpdater(cube.m_descriptorSet);
            descriptorSetUpdater.addBufferWriteDescriptor(
                uniformBufferBindingIndex,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                cube.uniformBuffer.m_vkDescriptorBufferInfo);

            descriptorSetUpdater.addImageWriteDescriptor(
                combinedImageSamplerBindingIndex,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                cube.texture.m_vkDescriptorImageInfo);

            descriptorSetUpdater.updateDescriptorSets();
        }
    }

    void preparePipelines()
    {
        /*
                [POI] Create a m_vkPipeline layout used for our graphics m_vkPipeline
        */
        vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayout);
        m_pipelineLayout = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

        const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_pipelineLayout, m_renderPassOriginal, 0);
        pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
        pipelineCI.pRasterizationState = &rasterizationStateCI;
        pipelineCI.pColorBlendState = &colorBlendStateCI;
        pipelineCI.pMultisampleState = &multisampleStateCI;
        pipelineCI.pViewportState = &viewportStateCI;
        pipelineCI.pDepthStencilState = &depthStencilStateCI;
        pipelineCI.pDynamicState = &dynamicStateCI;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color });

        shaderStages[0] = loadShader(getShadersPath() + "descriptorsets/cube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "descriptorsets/cube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
    }

    void prepareUniformBuffers()
    {
        // Vertex shader matrix uniform buffer block
        for (auto& cube : m_cubes) {
            VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &cube.uniformBuffer,
                sizeof(Cube::Matrices)));
            VK_CHECK_RESULT(cube.uniformBuffer.map());
        }

        updateUniformBuffers();
    }

    void updateUniformBuffers()
    {
        m_cubes[0].matrices.model = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
        m_cubes[1].matrices.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.5f, 0.0f));

        for (auto& cube : m_cubes) {
            cube.matrices.projection = camera.matrices.perspective;
            cube.matrices.view = camera.matrices.view;
            cube.matrices.model = glm::rotate(cube.matrices.model, glm::radians(cube.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            cube.matrices.model = glm::rotate(cube.matrices.model, glm::radians(cube.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            cube.matrices.model = glm::rotate(cube.matrices.model, glm::radians(cube.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            cube.matrices.model = glm::scale(cube.matrices.model, glm::vec3(0.25f));
            memcpy(cube.uniformBuffer.m_pMapped, &cube.matrices, sizeof(cube.matrices));
        }
    }

    void draw()
    {
        VulkanExampleBase::prepareFrame();
		m_queue.submit2(m_drawCommandBuffers[m_currentBufferIndex]);
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
        draw();
        if (animate && !paused) {
            m_cubes[0].rotation.x += 2.5f * m_frameTimer;
            if (m_cubes[0].rotation.x > 360.0f)
                m_cubes[0].rotation.x -= 360.0f;
            m_cubes[1].rotation.y += 2.0f * m_frameTimer;
            if (m_cubes[1].rotation.y > 360.0f)
                m_cubes[1].rotation.y -= 360.0f;
        }
        if ((camera.updated) || (animate && !paused)) {
            updateUniformBuffers();
        }
    }

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
    {
        if (overlay->header("Settings")) {
            overlay->checkBox("Animate", &animate);
        }
    }
};

VULKAN_EXAMPLE_MAIN()
