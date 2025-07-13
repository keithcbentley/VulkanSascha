/*
 * Vulkan Example - Push constants example (small shader block accessed outside of uniforms for fast updates)
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * Summary:
 * Using push constants it's possible to pass a small bit of static data to a shader, which is stored in the command buffer stat
 * This is perfect for passing e.g. static per-object data or parameters without the need for descriptor sets
 * The sample uses these to push different static parameters for rendering multiple objects
 */

#include "VulkanglTFModel.h"
#include "vulkanexamplebase.h"

class VulkanExample : public VulkanExampleBase {
public:
    vkglTF::Model model;

    // Color and position data for each sphere is uploaded using push constants
    struct SpherePushConstantData {
        glm::vec4 color;
        glm::vec4 position;
    };

    static constexpr int s_sphereCount = 16;

    std::array<SpherePushConstantData, s_sphereCount> m_spherePushConstantDatas;

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 model;
        glm::mat4 view;
    } uniformData;
    vks::Buffer uniformBuffer;

    VkPipelineLayout m_vkPipelineLayout { VK_NULL_HANDLE };
    VkPipeline m_vkPipeline { VK_NULL_HANDLE };
    VkDescriptorSetLayout m_vkDescriptorSetLayout { VK_NULL_HANDLE };
    VkDescriptorSet m_vkDescriptorSet { VK_NULL_HANDLE };

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Push constants";
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -10.0f));
        camera.setRotation(glm::vec3(0.0, 0.0f, 0.0f));
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
        camera.setRotationSpeed(0.5f);
    }

    ~VulkanExample()
    {
        if (m_deviceOriginal) {
            vkDestroyPipeline(m_deviceOriginal, m_vkPipeline, nullptr);
            vkDestroyPipelineLayout(m_deviceOriginal, m_vkPipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(m_deviceOriginal, m_vkDescriptorSetLayout, nullptr);
            uniformBuffer.destroy();
        }
    }

    void setupSpheres()
    {
        // Setup random colors and fixed positions for every sphere in the scene
        std::random_device rndDevice;
        std::default_random_engine rndEngine(m_benchmark.active ? 0 : rndDevice());
        std::uniform_real_distribution<float> rndDist(0.1f, 1.0f);

        for (uint32_t i = 0; i < m_spherePushConstantDatas.size(); i++) {
            m_spherePushConstantDatas[i].color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
            const float rad = glm::radians(i * 360.0f / static_cast<uint32_t>(m_spherePushConstantDatas.size()));
            m_spherePushConstantDatas[i].position = glm::vec4(glm::vec3(sin(rad), cos(rad), 0.0f) * 3.5f, 1.0f);
        }
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

            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

            commandBuffer
				.begin(cmdBufInfo)
				.cmdBeginRenderPass(renderPassBeginInfo)
				.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight)
				.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight)
				.cmdBindPipeline(m_vkPipeline)
				.cmdBindDescriptorSet(m_vkDescriptorSet, m_vkPipelineLayout);

            // [POI] Render the spheres passing color and position via push constants
            uint32_t spherecount = static_cast<uint32_t>(m_spherePushConstantDatas.size());
            for (uint32_t j = 0; j < spherecount; j++) {
                // [POI] Pass static sphere data as push constants
                commandBuffer.cmdPushConstant(
                    &m_spherePushConstantDatas[j],
                    sizeof(SpherePushConstantData),
                    m_vkPipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT);
                model.draw(commandBuffer);
            }

            drawUI(commandBuffer);

            commandBuffer.cmdEndRenderPass();
            commandBuffer.end();
        }
    }

    void loadAssets()
    {
        const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
        model.loadFromFile(getAssetPath() + "models/sphere.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
    }

    void setupDescriptors()
    {
        // Pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolInfo, m_deviceOriginal);

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_deviceOriginal, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

        // Set
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_deviceOriginal, &allocInfo, &m_vkDescriptorSet));

        VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(m_vkDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);
        vkUpdateDescriptorSets(m_deviceOriginal, 1, &writeDescriptorSet, 0, nullptr);
    }

    void preparePipelines()
    {
        // Layout
        // [POI] Define the push constant range used by the m_vkPipeline layout
        // Note that the spec only requires a minimum of 128 bytes, so for passing larger blocks of data you'd use UBOs or SSBOs
        VkPushConstantRange pushConstantRange {};
        // Push constants will only be accessible at the selected m_vkPipeline stages, for this sample it's the vertex shader that reads them
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SpherePushConstantData);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        VK_CHECK_RESULT(vkCreatePipelineLayout(m_deviceOriginal, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

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

        VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_renderPassOriginal, 0);
        pipelineCI.pInputAssemblyState = &inputAssemblyState;
        pipelineCI.pRasterizationState = &rasterizationState;
        pipelineCI.pColorBlendState = &colorBlendState;
        pipelineCI.pMultisampleState = &multisampleState;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pDepthStencilState = &depthStencilState;
        pipelineCI.pDynamicState = &dynamicState;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });
        shaderStages[0] = loadShader(getShadersPath() + "pushconstants/pushconstants.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "pushconstants/pushconstants.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_deviceOriginal, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
    }

    void prepareUniformBuffers()
    {
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
        VK_CHECK_RESULT(uniformBuffer.map());
        updateUniformBuffers();
    }

    void updateUniformBuffers()
    {
        uniformData.projection = camera.matrices.perspective;
        uniformData.view = camera.matrices.view;
        uniformData.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
        memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
    }

    void prepare()
    {
        VulkanExampleBase::prepare();
        loadAssets();
        setupSpheres();
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
};

VULKAN_EXAMPLE_MAIN()
