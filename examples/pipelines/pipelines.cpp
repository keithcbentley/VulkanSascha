/*
 * Vulkan Example - Using different pipelines in a single renderpass
 *
 * This sample shows how to setup multiple graphics pipelines and how to use them for drawing objects with differring visuals
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanglTFModel.h"
#include "vulkanexamplebase.h"
#include <VulkanCpp.hpp>

vkcpp::VulkanContext vkcpp::s_vulkanContext;


class VulkanExample : public VulkanExampleBase {
public:
    vkglTF::Model scene;

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec4 lightPos { 0.0f, 2.0f, 1.0f, 0.0f };
    } uniformData;

    vks::Buffer uniformBuffer;

    vkcpp::PipelineLayout m_pipelineLayoutOriginal;

    VkDescriptorSet descriptorSet { VK_NULL_HANDLE };
    VkDescriptorSetLayout m_vkDescriptorSetLayout { VK_NULL_HANDLE };

    struct {
        vkcpp::GraphicsPipeline m_phongPipelineOriginal;
        vkcpp::GraphicsPipeline m_wireframePipelineOriginal;
        vkcpp::GraphicsPipeline m_toonPipelineOriginal;
    } m_pipelines;

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Pipeline state objects";
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
        camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
        camera.setRotationSpeed(0.5f);
        camera.setPerspective(60.0f, (float)(m_drawAreaWidth / 3.0f) / (float)m_drawAreaHeight, 0.1f, 256.0f);
    }

    ~VulkanExample()
    {
        if (m_device) {
            vkDestroyDescriptorSetLayout(m_device, m_vkDescriptorSetLayout, nullptr);

            uniformBuffer.destroy();
        }
    }

    // Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
    virtual void getEnabledFeatures()
    {
        // Fill mode non solid is required for wireframe display
        if (m_physicalDeviceFeatures.m_features2.features.fillModeNonSolid) {
            m_vkPhysicalDeviceFeatures10.fillModeNonSolid = VK_TRUE;
        }

        // Wide lines must be present for line m_drawAreaWidth > 1.0f
        if (m_physicalDeviceFeatures.m_features2.features.wideLines) {
            m_vkPhysicalDeviceFeatures10.wideLines = VK_TRUE;
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

        for (int32_t i = 0; i < m_drawCommandBuffers.size(); ++i) {
            renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

            VK_CHECK_RESULT(vkBeginCommandBuffer(m_drawCommandBuffers[i], &cmdBufInfo));

            vkCmdBeginRenderPass(m_drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
            vkCmdSetViewport(m_drawCommandBuffers[i], 0, 1, &viewport);

            VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
            vkCmdSetScissor(m_drawCommandBuffers[i], 0, 1, &scissor);

            vkCmdBindDescriptorSets(m_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutOriginal, 0, 1, &descriptorSet, 0, NULL);
            scene.bindBuffers(m_drawCommandBuffers[i]);

            // Left : Render the scene using the solid colored pipeline with phong shading
            viewport.width = (float)m_drawAreaWidth / 3.0f;
            vkCmdSetViewport(m_drawCommandBuffers[i], 0, 1, &viewport);
            vkCmdBindPipeline(m_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_phongPipelineOriginal);
            vkCmdSetLineWidth(m_drawCommandBuffers[i], 1.0f);
            scene.draw(m_drawCommandBuffers[i]);

            // Center : Render the scene using a toon style pipeline
            viewport.x = (float)m_drawAreaWidth / 3.0f;
            vkCmdSetViewport(m_drawCommandBuffers[i], 0, 1, &viewport);
            vkCmdBindPipeline(m_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_toonPipelineOriginal);
            // Line m_drawAreaWidth > 1.0f only if wide lines feature is supported
            if (m_vkPhysicalDeviceFeatures10.wideLines) {
                vkCmdSetLineWidth(m_drawCommandBuffers[i], 2.0f);
            }
            scene.draw(m_drawCommandBuffers[i]);

            // Right : Render the scene as wireframe (if that feature is supported by the implementation)
            if (m_vkPhysicalDeviceFeatures10.fillModeNonSolid) {
                viewport.x = (float)m_drawAreaWidth / 3.0f + (float)m_drawAreaWidth / 3.0f;
                vkCmdSetViewport(m_drawCommandBuffers[i], 0, 1, &viewport);
                vkCmdBindPipeline(m_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines.m_wireframePipelineOriginal);
                scene.draw(m_drawCommandBuffers[i]);
            }

            drawUI(m_drawCommandBuffers[i]);

            vkCmdEndRenderPass(m_drawCommandBuffers[i]);

            VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCommandBuffers[i]));
        }
    }

    void loadAssets()
    {
        const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
        scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", m_pVulkanDevice, m_queue, glTFLoadingFlags);
    }

    void setupDescriptors()
    {
        // Pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
        };
        VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		m_descriptorPool = vkcpp::DescriptorPool(vkDescriptorPoolCreateInfo);

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

        // Set
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet));

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.m_vkDescriptorBufferInfo)
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    void preparePipelines()
    {

        vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.addDescriptorSetLayout(m_vkDescriptorSetLayout);
        m_pipelineLayoutOriginal = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

        vkcpp::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
        graphicsPipelineCreateInfo.setRenderPass(m_renderPassOriginal, 0);
        graphicsPipelineCreateInfo.setPipelineLayout(m_pipelineLayoutOriginal);
        graphicsPipelineCreateInfo.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        graphicsPipelineCreateInfo.setRasterizationStateCreateInfo(vkcpp::PipelineRasterizationStateCreateInfo());

        graphicsPipelineCreateInfo.addColorBlendAttachmentState(vkcpp::PipelineColorBlendAttachmentState());
        graphicsPipelineCreateInfo.setDepthStencilStateCreateInfo(vkcpp::PipelineDepthStencilStateCreateInfo::basicDepth());
        graphicsPipelineCreateInfo.addViewport(VkViewport {});
        graphicsPipelineCreateInfo.addScissor(VkRect2D {});

        graphicsPipelineCreateInfo.setMultisampleStateCreateInfo(
            vkcpp::PipelineMultisampleStateCreateInfo::reasonableDefaults());

        graphicsPipelineCreateInfo.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        graphicsPipelineCreateInfo.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
        graphicsPipelineCreateInfo.addDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
        graphicsPipelineCreateInfo.setPipelineVertexInputStateCreateInfo(
            vkglTF::Vertex::getPipelineVertexInputStateVkcpp(
                { vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color }));
        graphicsPipelineCreateInfo.allowDerivatives(true);


        //	Shader modules can be safely destroyed after pipeline creation, so RAII
        //	for the handles is ok for this situation.
        vkcpp::ShaderModule vertexShaderModulePhong = vkcpp::ShaderModule::createShaderModuleFromFile(
            getShadersPath() + "pipelines/phong.vert.spv");
        graphicsPipelineCreateInfo.addShaderModule(vertexShaderModulePhong, VK_SHADER_STAGE_VERTEX_BIT, "main");

        vkcpp::ShaderModule fragmentShaderModulePhong = vkcpp::ShaderModule::createShaderModuleFromFile(
            getShadersPath() + "pipelines/phong.frag.spv");
        graphicsPipelineCreateInfo.addShaderModule(fragmentShaderModulePhong, VK_SHADER_STAGE_FRAGMENT_BIT, "main");

        m_pipelines.m_phongPipelineOriginal = vkcpp::GraphicsPipeline(graphicsPipelineCreateInfo);

        //	Not sure if this is necessary, but original code turned this off.
        graphicsPipelineCreateInfo.allowDerivatives(false);
        graphicsPipelineCreateInfo.setBasePipeline(m_pipelines.m_phongPipelineOriginal);

        // Toon shading pipeline
		graphicsPipelineCreateInfo.clearShaders();
		vkcpp::ShaderModule vertexShaderModuleToon = vkcpp::ShaderModule::createShaderModuleFromFile(
			getShadersPath() + "pipelines/toon.vert.spv");
		graphicsPipelineCreateInfo.addShaderModule(vertexShaderModuleToon, VK_SHADER_STAGE_VERTEX_BIT, "main");

		vkcpp::ShaderModule fragmentShaderModuleToon = vkcpp::ShaderModule::createShaderModuleFromFile(
			getShadersPath() + "pipelines/toon.frag.spv");
		graphicsPipelineCreateInfo.addShaderModule(fragmentShaderModuleToon, VK_SHADER_STAGE_FRAGMENT_BIT, "main");

        m_pipelines.m_toonPipelineOriginal = vkcpp::GraphicsPipeline(graphicsPipelineCreateInfo);

		//	Wireframe
		graphicsPipelineCreateInfo.setRasterizationPolygonMode(VK_POLYGON_MODE_LINE);

		graphicsPipelineCreateInfo.clearShaders();
		vkcpp::ShaderModule vertexShaderModuleWireframe = vkcpp::ShaderModule::createShaderModuleFromFile(
			getShadersPath() + "pipelines/wireframe.vert.spv");
		graphicsPipelineCreateInfo.addShaderModule(vertexShaderModuleWireframe, VK_SHADER_STAGE_VERTEX_BIT, "main");

		vkcpp::ShaderModule fragmentShaderModuleWireframe = vkcpp::ShaderModule::createShaderModuleFromFile(
			getShadersPath() + "pipelines/wireframe.frag.spv");
		graphicsPipelineCreateInfo.addShaderModule(fragmentShaderModuleWireframe, VK_SHADER_STAGE_FRAGMENT_BIT, "main");

		m_pipelines.m_wireframePipelineOriginal = vkcpp::GraphicsPipeline(graphicsPipelineCreateInfo);

    }

    // Prepare and initialize uniform buffer containing shader uniforms
    void prepareUniformBuffers()
    {
        // Create the vertex shader uniform buffer block
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
        // Map persistent
        VK_CHECK_RESULT(uniformBuffer.map());
    }

    void updateUniformBuffers()
    {
        // Override the base sample camera setup, since we use three viewports
        camera.setPerspective(60.0f, (float)(m_drawAreaWidth / 3.0f) / (float)m_drawAreaHeight, 0.1f, 256.0f);
        uniformData.projection = camera.matrices.perspective;
        uniformData.modelView = camera.matrices.view;
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
        if (!m_vkPhysicalDeviceFeatures10.fillModeNonSolid) {
            if (overlay->header("Info")) {
                overlay->text("Non solid fill modes not supported!");
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
