/*
 * Vulkan Example - Basic indexed triangle rendering
 *
 * Note:
 *	This is a "pedal to the metal" example to show off how to get Vulkan up and displaying something
 *	Contrary to the other examples, this one won't make use of helper functions or initializers
 *	Except in a few cases (swap chain setup e.g.)
 *
 * Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include <assert.h>
#include <exception>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkanexamplebase.h"
#include <VulkanCpp.hpp>
#include <vulkan/vulkan.h>

vkcpp::VulkanContext vkcpp::s_vulkanContext;

// We want to keep GPU and CPU busy. To do that we may start building a new command buffer while the previous one is still being executed
// This number defines how many frames may be worked on simultaneously at once
// Increasing this number may improve performance but will also introduce additional latency
#define MAX_CONCURRENT_FRAMES 2

class VulkanExample : public VulkanExampleBase {
public:
    // Vertex layout used in this example
    struct Vertex {
        float position[3];
        float color[3];
    };

    std::vector<Vertex> s_vertexDataBuffer {
        { { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
    };
    //        uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

    // Setup indices
    std::vector<uint32_t> s_vertexIndexBuffer { 0, 1, 2 };
    //        m_indicesCount = static_cast<uint32_t>(indexBuffer.size());
    //        uint32_t indexBufferSize = m_indicesCount * sizeof(uint32_t);

    vkcpp::Buffer_DeviceMemory<Vertex> m_vertices;
    vkcpp::Buffer_DeviceMemory<uint32_t> m_indices;

    // For simplicity we use the same uniform block layout as in the shader:
    //
    //	layout(set = 0, binding = 0) uniform UBO
    //	{
    //		mat4 projectionMatrix;
    //		mat4 modelMatrix;
    //		mat4 viewMatrix;
    //	} ubo;
    //
    // This way we can just memcopy the ubo data to the ubo
    // Note: You should use data types that align with the GPU in order to avoid manual padding (vec4, mat4)
    struct ShaderData {
        glm::mat4 projectionMatrix;
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
    };

    // Uniform buffer block object
    struct UniformBuffer {
        //        vkcpp::DeviceMemory<T> m_deviceMemoryOriginal;
        //        vkcpp::Buffer<T> m_bufferOriginal;
        // The descriptor set stores the resources bound to the binding points in a shader
        // It connects the binding points of the different shaders with the buffers and images used for those bindings
        vkcpp::DescriptorSet m_descriptorSet;
        // We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
        //        uint8_t* m_pMappedMemory { nullptr };

        vkcpp::Buffer_DeviceMemory<ShaderData> m_buffer_deviceMemory;
    };

    // We use one UBO per frame, so we can have a frame overlap and make sure that uniforms aren't updated while still in use
    std::array<UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniformBuffers;

    // The m_vkPipeline layout is used by a m_vkPipeline to access the descriptor sets
    // It defines interface (without binding any actual data) between the shader stages used by the m_vkPipeline and the shader resources
    // A m_vkPipeline layout can be shared among multiple pipelines as long as their interfaces match
    vkcpp::PipelineLayout m_pipelineLayout;

    // Pipelines (often called "m_vkPipeline state objects") are used to bake all states that affect a m_vkPipeline
    // While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) m_vkPipeline states upfront
    // So for each combination of non-dynamic m_vkPipeline states you need a new m_vkPipeline (there are a few exceptions to this not discussed here)
    // Even though this adds a new dimension of planning ahead, it's a great opportunity for performance optimizations by the driver
    vkcpp::GraphicsPipeline m_graphicsPipeline;

    // The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
    // Like the m_vkPipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
    vkcpp::DescriptorSetLayout m_descriptorSetLayout;

    // Synchronization primitives
    // Synchronization is an important concept of Vulkan that OpenGL mostly hid away. Getting this right is crucial to using Vulkan.

    // Semaphores are used to coordinate operations within the graphics m_vkQueue and ensure correct command ordering
    std::vector<VkSemaphore> m_vkPresentCompleteSemaphores {};
    std::vector<VkSemaphore> m_vkRenderCompleteSemaphores {};

    // std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> m_vkCommandBuffers {};
    std::array<VkFence, MAX_CONCURRENT_FRAMES> m_vkWaitFences {};

    // To select the correct sync and command objects, we need to keep track of the current frame
    uint32_t m_currentFrameIndex { 0 };

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "Basic indexed triangle";
        // To keep things simple, we don't use the UI overlay from the framework
        m_exampleSettings.m_showUIOverlay = false;
        // Setup a default look-at camera
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
        camera.setRotation(glm::vec3(0.0f));
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 1.0f, 256.0f);
        // Values not set here are initialized in the base class constructor
    }

    ~VulkanExample() override
    {
        // Clean up used Vulkan resources
        // Note: Inherited destructor cleans up resources stored in base class
        if (m_device) {

            for (size_t i = 0; i < m_vkPresentCompleteSemaphores.size(); i++) {
                vkDestroySemaphore(m_device, m_vkPresentCompleteSemaphores[i], nullptr);
            }
            for (size_t i = 0; i < m_vkRenderCompleteSemaphores.size(); i++) {
                vkDestroySemaphore(m_device, m_vkRenderCompleteSemaphores[i], nullptr);
            }
            for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
                vkDestroyFence(m_device, m_vkWaitFences[i], nullptr);
            }
        }
    }

    // Create the per-frame (in flight) Vulkan synchronization primitives used in this example
    void createSynchronizationPrimitives()
    {
        // Fences are used to check draw command buffer completion on the host
        for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
            VkFenceCreateInfo fenceCI {};
            fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            // Create the fences in signaled state (so we don't wait on first render of each command buffer)
            fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            // Fence used to ensure that command buffer has completed exection before using it again
            VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCI, nullptr, &m_vkWaitFences[i]));
        }
        // Semaphores are used for correct command ordering within a m_vkQueue
        // Used to ensure that m_vkImage presentation is complete before starting to submit again
        m_vkPresentCompleteSemaphores.resize(MAX_CONCURRENT_FRAMES);
        for (auto& semaphore : m_vkPresentCompleteSemaphores) {
            VkSemaphoreCreateInfo semaphoreCI { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCI, nullptr, &semaphore));
        }
        // Render completion
        // Semaphore used to ensure that all commands submitted have been finished before submitting the m_vkImage to the m_vkQueue
        m_vkRenderCompleteSemaphores.resize(m_swapChain.images.size());
        for (auto& semaphore : m_vkRenderCompleteSemaphores) {
            VkSemaphoreCreateInfo semaphoreCI { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCI, nullptr, &semaphore));
        }
    }

    // Prepare vertex and index buffers for an indexed triangle
    // Also uploads them to m_vkDevice local m_vkDeviceMemory using staging and initializes vertex input and attribute binding to match the vertex shader
    void createVertexBuffer()
    {
        //	A note on m_vkDeviceMemory management in Vulkan in general:
        //	This is a very complex topic and while it's fine for an example application to do
        //	small individual m_vkDeviceMemory allocations that is not
        //	what should be done a real-world application, where you should allocate
        //	large chunks of m_vkDeviceMemory at once instead.

        // Static data like vertex and index buffer should be stored on the m_vkDevice m_vkDeviceMemory for optimal (and fastest) access by the GPU
        //
        // To achieve this we use so-called "staging buffers" :
        // - Create a buffer that's visible to the host (and can be mapped)
        // - Copy the data to this buffer
        // - Create another buffer that's local on the m_vkDevice (VRAM) with the same size
        // - Copy the data from the host to the m_vkDevice using a command buffer
        // - Delete the host visible (staging) buffer
        // - Use the m_vkDevice local buffers for rendering
        //
        // Note: On unified m_vkDeviceMemory architectures where host (CPU) and GPU share the same m_vkDeviceMemory, staging is not necessary
        // To keep this sample easy to follow, there is no check for that in place

        // Copy vertex info to device memory. We need to make a buffer/memory that's visible
        // to the host and device to copy the data from host to device, and then buffer/memory that's device only
        // for use by the device.  That last copy step needs to be done via a command buffer and copy command.
        // We will do the same thing for the index info.

        // Create a host-visible buffer to copy the vertex data to (staging buffer)
        vkcpp::Buffer_DeviceMemory verticesStagingBuffer
            = vkcpp::Buffer_DeviceMemory<Vertex>::withCopy(
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                vkcpp::TypedCount(s_vertexDataBuffer),
                0,
                vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
                s_vertexDataBuffer.data());
        verticesStagingBuffer.unmapMemory();
        m_vertices = vkcpp::Buffer_DeviceMemory(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vkcpp::TypedCount(s_vertexDataBuffer),
            0,
            vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);

        vkcpp::Buffer_DeviceMemory indicesStagingBuffer
            = vkcpp::Buffer_DeviceMemory<uint32_t>::withCopy(
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                vkcpp::TypedCount(s_vertexIndexBuffer),
                0,
                vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
                s_vertexIndexBuffer.data());
        indicesStagingBuffer.unmapMemory();
        m_indices = vkcpp::Buffer_DeviceMemory(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vkcpp::TypedCount(s_vertexIndexBuffer),
            0,
            vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);

        // Buffer copies have to be submitted to a m_vkQueue, so we need a command buffer for them
        // Note: Some devices offer a dedicated transfer m_vkQueue (with only the transfer bit set) that may be faster when doing lots of copies
        VkCommandBuffer vkCommandBuffer;

        VkCommandBufferAllocateInfo cmdBufAllocateInfo {};
        cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufAllocateInfo.commandPool = m_commandPool;
        cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufAllocateInfo.commandBufferCount = 1;
        VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, &vkCommandBuffer));

        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        VK_CHECK_RESULT(vkBeginCommandBuffer(vkCommandBuffer, &cmdBufInfo));
        // Put buffer region copies into command buffer
        VkBufferCopy copyRegion {};
        // Vertex buffer
        copyRegion.size = vkcpp::TypedCount(s_vertexDataBuffer).vkDeviceSize();
        vkCmdCopyBuffer(vkCommandBuffer, verticesStagingBuffer.m_buffer, m_vertices.m_buffer, 1, &copyRegion);
        // Index buffer
        copyRegion.size = vkcpp::TypedCount(s_vertexIndexBuffer).vkDeviceSize();
        vkCmdCopyBuffer(vkCommandBuffer, indicesStagingBuffer.m_buffer, m_indices.m_buffer, 1, &copyRegion);
        VK_CHECK_RESULT(vkEndCommandBuffer(vkCommandBuffer));

        // Submit the command buffer to the m_vkQueue to finish the copy
        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vkCommandBuffer;

        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fenceCI {};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = 0;
        VkFence fence;
        VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCI, nullptr, &fence));

        // Submit to the m_vkQueue
        VK_CHECK_RESULT(vkQueueSubmit(m_queue, 1, &submitInfo, fence));
        // Wait for the fence to signal that command buffer has finished executing
        VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

        vkDestroyFence(m_device, fence, nullptr);

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &vkCommandBuffer);
    }

    // Descriptors are allocated from a pool, that tells the implementation how many and what types of descriptors we are going to use (at maximum)
    void createDescriptorPool()
    {
        vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
        descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_CONCURRENT_FRAMES);
        // Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
        // Our sample will create one set per uniform buffer per frame
        descriptorPoolCreateInfo.maxSets = MAX_CONCURRENT_FRAMES;
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo);
    }

    // Descriptor set layouts define the interface between our application and the shader
    // Basically connects the different shader stages to descriptors for binding uniform buffers, m_vkImage samplers, etc.
    // So every shader binding should map to one descriptor set layout binding
    void createDescriptorSetLayout()
    {
        // Binding 0: Uniform buffer (Vertex shader)
        vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
        descriptorSetLayoutCreateInfo.addDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vkcpp::SHADER_STAGE_VERTEX);
        m_descriptorSetLayout = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);
    }

    // Shaders access data using descriptor sets that "point" at our uniform buffers
    // The descriptor sets make use of the descriptor set layouts created above
    void createDescriptorSets()
    {
        // Allocate one descriptor set per frame from the global descriptor pool
        for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
            m_uniformBuffers[i].m_descriptorSet = vkcpp::DescriptorSet(m_descriptorSetLayout, m_descriptorPool);

            // Update the descriptor set determining the shader binding points
            // For every binding point used in a shader there needs to be one
            // descriptor set matching that binding point

            vkcpp::WriteDescriptorSet writeDescriptorSet(m_uniformBuffers[i].m_descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            writeDescriptorSet.addBufferInfo(
                m_uniformBuffers[i].m_buffer_deviceMemory.m_buffer, 0, sizeof(ShaderData));

            vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
        }
    }

    // Create the depth (and stencil) buffer attachments used by our framebuffers
    // Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
    void setupDepthStencil() override
    {
        // Create an optimal m_vkImage used as the depth stencil attachment
        VkImageCreateInfo vkImageCreateInfo {};
        vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        vkImageCreateInfo.format = m_vkFormatDepth;
        vkImageCreateInfo.extent = { m_drawAreaWidth, m_drawAreaHeight, 1 };
        vkImageCreateInfo.mipLevels = 1;
        vkImageCreateInfo.arrayLayers = 1;
        vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        vkImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vkImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_defaultDepthStencil.m_image = vkcpp::Image(vkImageCreateInfo);

        // Allocate m_vkDeviceMemory for the m_vkImage (m_vkDevice local) and bind it to our m_vkImage
        VkMemoryAllocateInfo vkMemoryAllocateInfo {};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements vkMemoryRequirements;
        vkGetImageMemoryRequirements(m_device, m_defaultDepthStencil.m_image, &vkMemoryRequirements);
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex
            = vkcpp::findMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);
        m_defaultDepthStencil.m_deviceMemory = vkcpp::DeviceMemory(vkMemoryAllocateInfo);

        VK_CHECK_RESULT(vkBindImageMemory(m_device, m_defaultDepthStencil.m_image, m_defaultDepthStencil.m_deviceMemory, 0));

        // Create a vkImageView for the depth stencil m_vkImage
        // Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
        // This allows for multiple views of one m_vkImage with differing ranges (e.g. for different layers)
        VkImageViewCreateInfo vkImageViewCreateInfo {};
        vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vkImageViewCreateInfo.format = m_vkFormatDepth;
        vkImageViewCreateInfo.subresourceRange = {};
        vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
        if (m_vkFormatDepth >= VK_FORMAT_D16_UNORM_S8_UINT) {
            vkImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        vkImageViewCreateInfo.subresourceRange.levelCount = 1;
        vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        vkImageViewCreateInfo.subresourceRange.layerCount = 1;
        vkImageViewCreateInfo.image = m_defaultDepthStencil.m_image;
        m_defaultDepthStencil.m_imageView = vkcpp::ImageView(vkImageViewCreateInfo);
    }

    // Create a frame buffer for each swap chain m_vkImage
    // Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
    void setupFrameBuffer() override
    {
        // Create a frame buffer for every m_vkImage in the swapchain
        m_vkFrameBuffers.resize(m_swapChain.images.size());
        for (size_t i = 0; i < m_vkFrameBuffers.size(); i++) {
            std::array<VkImageView, 2> attachments {};
            // Color attachment is the m_vkImageView of the swapchain m_vkImage
            attachments[0] = m_swapChain.imageViews[i];
            // Depth/Stencil attachment is the same for all frame buffers due to how depth works with current GPUs
            attachments[1] = m_defaultDepthStencil.m_imageView;

            VkFramebufferCreateInfo frameBufferCI {};
            frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            // All frame buffers use the same renderpass setup
            frameBufferCI.renderPass = m_renderPassOriginal;
            frameBufferCI.attachmentCount = static_cast<uint32_t>(attachments.size());
            frameBufferCI.pAttachments = attachments.data();
            frameBufferCI.width = m_drawAreaWidth;
            frameBufferCI.height = m_drawAreaHeight;
            frameBufferCI.layers = 1;
            // Create the framebuffer
            VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &frameBufferCI, nullptr, &m_vkFrameBuffers[i]));
        }
    }

    // Render pass setup
    // Render passes are a new concept in Vulkan. They describe the attachments used during rendering and may contain multiple subpasses with attachment dependencies
    // This allows the driver to know up-front what the rendering will look like and is a good opportunity to optimize especially on tile-based renderers (with multiple subpasses)
    // Using sub pass dependencies also adds implicit layout transitions for the attachment used, so we don't need to add explicit m_vkImage m_vkDeviceMemory barriers to transform them
    // Note: Override of virtual function in the base class and called from within VulkanExampleBase::prepare
    void setupRenderPass() override
    {
        // This example will use a single render pass with one subpass
        constexpr int attachmentCount = 2;
        constexpr int colorPresentAttachmentIndex = 0;
        constexpr int depthStencilAttachmentIndex = 1;

        constexpr int subpassCount = 1;
        constexpr int theOnlySubpassIndex = 0;

        vkcpp::RenderPassCreateInfo renderPassCreateInfo(subpassCount, attachmentCount);
        renderPassCreateInfo.attachmentDescription(colorPresentAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleColorPresent(m_swapChain.colorFormat);

        renderPassCreateInfo.attachmentDescription(depthStencilAttachmentIndex)
            = vkcpp::AttachmentDescription::simpleDepthStencil(m_vkFormatDepth);

        renderPassCreateInfo.subpassDescription(theOnlySubpassIndex)
            .addColorAttachmentReference(colorPresentAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setDepthStencilAttachmentReference(depthStencilAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        // Setup subpass dependencies
        // These will add the implicit attachment layout transitions specified by the attachment descriptions
        // The actual usage layout is preserved through the layout specified in the attachment reference
        // Each subpass dependency will introduce a m_vkDeviceMemory and execution dependency between the source and dest subpass described by
        // srcStageMask, dstStageMask, srcAccessMask, dstAccessMask (and dependencyFlags is set)
        // Note: VK_SUBPASS_EXTERNAL is a special constant that refers to all commands executed outside of the actual renderpass)
        renderPassCreateInfo
            .addSubpassDependency(VK_SUBPASS_EXTERNAL, theOnlySubpassIndex)
            .addSrc(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_NONE)
            .addDst(
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

        auto fragmentTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        renderPassCreateInfo
            .addSubpassDependency(VK_SUBPASS_EXTERNAL, theOnlySubpassIndex)
            .addSrc(fragmentTests, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .addDst(
                fragmentTests,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

        // Create the actual renderpass
        m_renderPassOriginal = vkcpp::RenderPass(renderPassCreateInfo);
    }

    // Vulkan loads its shaders from an immediate binary representation called SPIR-V
    // Shaders are compiled offline from e.g. GLSL using the reference glslang compiler
    // This function loads such a shader from a binary file and returns a shader module structure
    VkShaderModule loadSPIRVShader(const std::string& filename)
    {
        size_t shaderSize;
        char* shaderCode { nullptr };

        std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

        if (is.is_open()) {
            shaderSize = is.tellg();
            is.seekg(0, std::ios::beg);
            // Copy file contents into a buffer
            shaderCode = new char[shaderSize];
            is.read(shaderCode, shaderSize);
            is.close();
            assert(shaderSize > 0);
        }

        if (shaderCode) {
            // Create a new shader module that will be used for m_vkPipeline creation
            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = shaderSize;
            shaderModuleCI.pCode = (uint32_t*)shaderCode;

            VkShaderModule shaderModule;
            VK_CHECK_RESULT(vkCreateShaderModule(m_device, &shaderModuleCI, nullptr, &shaderModule));

            delete[] shaderCode;

            return shaderModule;
        } else {
            std::cerr << "Error: Could not open shader file \"" << filename << "\"" << std::endl;
            return VK_NULL_HANDLE;
        }
    }

    void createPipelines()
    {
        // Create the m_vkPipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
        // In a more complex scenario you would have different m_vkPipeline layouts for different descriptor set layouts that could be reused
        vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayout);
        m_pipelineLayout = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

        // Create the graphics m_vkPipeline used in this example
        // Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
        // A m_vkPipeline is then stored and hashed on the GPU making m_vkPipeline changes very fast
        // Note: There are still a few dynamic states that are not directly part of the m_vkPipeline (but the info that they are used is)

        VkGraphicsPipelineCreateInfo pipelineCI {};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // The layout used for this m_vkPipeline (can be shared among multiple pipelines using the same layout)
        pipelineCI.layout = m_pipelineLayout;
        // Renderpass this m_vkPipeline is attached to
        pipelineCI.renderPass = m_renderPassOriginal;

        // Construct the different states making up the m_vkPipeline

        // Input assembly state describes how primitives are assembled
        // This m_vkPipeline will assemble vertex data as a triangle lists (though we only use one triangle)
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI {};
        inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Rasterization state
        VkPipelineRasterizationStateCreateInfo rasterizationStateCI {};
        rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
        rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationStateCI.depthClampEnable = VK_FALSE;
        rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateCI.depthBiasEnable = VK_FALSE;
        rasterizationStateCI.lineWidth = 1.0f;

        // Color blend state describes how blend factors are calculated (if used)
        // We need one blend attachment state per color attachment (even if blending is not used)
        VkPipelineColorBlendAttachmentState blendAttachmentState {};
        blendAttachmentState.colorWriteMask = 0xf;
        blendAttachmentState.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo colorBlendStateCI {};
        colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCI.attachmentCount = 1;
        colorBlendStateCI.pAttachments = &blendAttachmentState;

        // Viewport state sets the number of viewports and scissor used in this m_vkPipeline
        // Note: This is actually overridden by the dynamic states (see below)
        VkPipelineViewportStateCreateInfo viewportStateCI {};
        viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCI.viewportCount = 1;
        viewportStateCI.scissorCount = 1;

        // Enable dynamic states
        // Most states are baked into the m_vkPipeline, but there are still a few dynamic states that can be changed within a command buffer
        // To be able to change these we need do specify which dynamic states will be changed using this m_vkPipeline. Their actual states are set later on in the command buffer.
        // For this example we will set the viewport and scissor using dynamic states
        std::vector<VkDynamicState> dynamicStateEnables;
        dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
        VkPipelineDynamicStateCreateInfo dynamicStateCI {};
        dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
        dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

        // Depth and stencil state containing depth and stencil compare and test operations
        // We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCI {};
        depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateCI.depthTestEnable = VK_TRUE;
        depthStencilStateCI.depthWriteEnable = VK_TRUE;
        depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateCI.back.failOp = VK_STENCIL_OP_KEEP;
        depthStencilStateCI.back.passOp = VK_STENCIL_OP_KEEP;
        depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depthStencilStateCI.stencilTestEnable = VK_FALSE;
        depthStencilStateCI.front = depthStencilStateCI.back;

        // Multi sampling state
        // This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the m_vkPipeline
        VkPipelineMultisampleStateCreateInfo multisampleStateCI {};
        multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleStateCI.pSampleMask = nullptr;

        // Vertex input descriptions
        // Specifies the vertex input parameters for a m_vkPipeline

        // Vertex input binding
        // This example uses a single vertex input binding at binding point 0 (see vkCmdBindVertexBuffers)
        VkVertexInputBindingDescription vertexInputBinding {};
        vertexInputBinding.binding = 0;
        vertexInputBinding.stride = sizeof(Vertex);
        vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Input attribute bindings describe shader attribute locations and m_vkDeviceMemory layouts
        std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributs {};
        // These match the following shader layout (see triangle.vert):
        //	layout (location = 0) in vec3 inPos;
        //	layout (location = 1) in vec3 inColor;
        // Attribute location 0: Position
        vertexInputAttributs[0].binding = 0;
        vertexInputAttributs[0].location = 0;
        // Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
        vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexInputAttributs[0].offset = offsetof(Vertex, position);
        // Attribute location 1: Color
        vertexInputAttributs[1].binding = 0;
        vertexInputAttributs[1].location = 1;
        // Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
        vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertexInputAttributs[1].offset = offsetof(Vertex, color);

        // Vertex input state used for m_vkPipeline creation
        VkPipelineVertexInputStateCreateInfo vertexInputStateCI {};
        vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCI.vertexBindingDescriptionCount = 1;
        vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
        vertexInputStateCI.vertexAttributeDescriptionCount = 2;
        vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributs.data();

        // Shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages {};

        // Vertex shader
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // Set m_vkPipeline stage for this shader
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        // Load binary SPIR-V shader
        shaderStages[0].module = loadSPIRVShader(getShadersPath() + "triangle/triangle.vert.spv");
        // Main entry point for the shader
        shaderStages[0].pName = "main";
        assert(shaderStages[0].module != VK_NULL_HANDLE);

        // Fragment shader
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // Set m_vkPipeline stage for this shader
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Load binary SPIR-V shader
        shaderStages[1].module = loadSPIRVShader(getShadersPath() + "triangle/triangle.frag.spv");
        // Main entry point for the shader
        shaderStages[1].pName = "main";
        assert(shaderStages[1].module != VK_NULL_HANDLE);

        // Set m_vkPipeline shader stage info
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();

        // Assign the m_vkPipeline states to the m_vkPipeline creation info structure
        pipelineCI.pVertexInputState = &vertexInputStateCI;
        pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
        pipelineCI.pRasterizationState = &rasterizationStateCI;
        pipelineCI.pColorBlendState = &colorBlendStateCI;
        pipelineCI.pMultisampleState = &multisampleStateCI;
        pipelineCI.pViewportState = &viewportStateCI;
        pipelineCI.pDepthStencilState = &depthStencilStateCI;
        pipelineCI.pDynamicState = &dynamicStateCI;

        // Create rendering m_vkPipeline using the specified states
        m_graphicsPipeline = vkcpp::GraphicsPipeline(pipelineCI);

        // Shader modules are no longer needed once the graphics m_vkPipeline has been created
        vkDestroyShaderModule(m_device, shaderStages[0].module, nullptr);
        vkDestroyShaderModule(m_device, shaderStages[1].module, nullptr);
    }

    void createUniformBuffers()
    {
        // Prepare and initialize the per-frame uniform buffer blocks containing shader uniforms
        // Single uniforms like in OpenGL are no longer present in Vulkan. All shader uniforms are passed via uniform buffer blocks

        // Create the buffers
        for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
            m_uniformBuffers[i].m_buffer_deviceMemory
                = vkcpp::Buffer_DeviceMemory<ShaderData>::withMap(
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    vkcpp::TypedCount<ShaderData>(1),
                    0,
                    vkcpp::MEMORY_PROPERTY_HOST_VISIBLE_COHERENT);
        }
    }

    void prepare() override
    {
        VulkanExampleBase::prepare();
        createSynchronizationPrimitives();
        createVertexBuffer();
        createUniformBuffers();
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
        createPipelines();
        m_prepared = true;
    }

    void render() override
    {
        if (!m_prepared)
            return;

        // Use a fence to wait until the command buffer has finished execution before using it again
        vkWaitForFences(m_device, 1, &m_vkWaitFences[m_currentFrameIndex], VK_TRUE, UINT64_MAX);
        VK_CHECK_RESULT(vkResetFences(m_device, 1, &m_vkWaitFences[m_currentFrameIndex]));

        // Get the next swap chain m_vkImage from the implementation
        // Note that the implementation is free to return the images in any order, so we must use the acquire function and can't just cycle through the images/imageIndex on our own
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            m_device,
            m_swapChain.swapChain,
            UINT64_MAX,
            m_vkPresentCompleteSemaphores[m_currentFrameIndex],
            VK_NULL_HANDLE,
            &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            windowResize();
            return;
        } else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR)) {
            throw "Could not acquire the next swap chain m_vkImage!";
        }

        // Update the uniform buffer for the next frame
        ShaderData shaderData {};
        shaderData.projectionMatrix = camera.matrices.perspective;
        shaderData.viewMatrix = camera.matrices.view;
        shaderData.modelMatrix = glm::mat4(1.0f);

        // Copy the current matrices to the current frame's uniform buffer
        // Note: Since we requested a host coherent m_vkDeviceMemory type for the uniform buffer, the write is instantly visible to the GPU
		m_uniformBuffers[m_currentFrameIndex].m_buffer_deviceMemory.mappedMemory() = shaderData;

        // Build the command buffer
        // Unlike in OpenGL all rendering commands are recorded into command buffers that are then submitted to the m_vkQueue
        // This allows to generate work upfront in a separate thread
        // For basic command buffers (like in this sample), recording is so fast that there is no need to offload this

        // Start the first sub pass specified in our default render pass setup by the base class
        // This will clear the color and depth attachment
        // Set clear values for all framebuffer attachments with loadOp set to clear
        // We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
        VkClearValue clearValues[2] {};
        clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassBeginInfo {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = m_renderPassOriginal;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
        renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        renderPassBeginInfo.framebuffer = m_vkFrameBuffers[imageIndex];

        vkcpp::CommandBuffer commandBuffer = m_drawCommandBuffers[m_currentFrameIndex];

        commandBuffer
            .reset()
            .begin()
            .cmdBeginRenderPass(renderPassBeginInfo)
            .cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight)
            .cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight)
            .cmdBindDescriptorSet(m_uniformBuffers[m_currentFrameIndex].m_descriptorSet, m_pipelineLayout)
            .cmdBindPipeline(m_graphicsPipeline)
            .cmdBindVertexBuffer(m_vertices.m_buffer)
            .cmdBindIndexBuffer(m_indices.m_buffer, VK_INDEX_TYPE_UINT32)
            .cmdDrawIndexed(s_vertexIndexBuffer.size())
            .cmdEndRenderPass()
            .end();

        // Submit the command buffer to the graphics m_vkQueue

        // Pipeline stage at which the m_vkQueue submission will wait (via pWaitSemaphores)

        // VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        //  The submit info structure specifies a command buffer m_vkQueue submission batch
        vkcpp::SubmitInfo submitInfo;
        submitInfo.addCommandBuffer(commandBuffer);
        submitInfo.addWaitSemaphore(m_vkPresentCompleteSemaphores[m_currentFrameIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        submitInfo.addSignalSemaphore(m_vkRenderCompleteSemaphores[imageIndex]);
        m_queue.submit(submitInfo, m_vkWaitFences[m_currentFrameIndex]);

        // Present the current frame buffer to the swap chain
        // Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
        // This ensures that the m_vkImage is not presented to the windowing system until all commands have been submitted

        vkcpp::PresentInfo presentInfo;
        presentInfo.addWaitSemaphore(m_vkRenderCompleteSemaphores[imageIndex]);
        presentInfo.addSwapchain(m_swapChain.swapChain, imageIndex);
        result = vkQueuePresentKHR(m_queue, &presentInfo);

        if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
            windowResize();
        } else if (result != VK_SUCCESS) {
            throw "Could not present the m_vkImage to the swap chain!";
        }

        // Select the next frame to render to, based on the max. no. of concurrent frames
        m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_CONCURRENT_FRAMES;
    }
};

// OS specific main entry points
// Most of the code base is shared for the different supported operating systems, but stuff like message handling differs

// Windows entry point
VulkanExample* vulkanExample;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (vulkanExample != NULL) {
        vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int)
{
    std::cout << std::format("WinMain\n");
    for (size_t i = 0; i < __argc; i++) {
        VulkanExample::args.push_back(__argv[i]);
    };

    try {
        vkcpp::VulkanContextCreateInfo appContextCreateInfo;
        vkcpp::initVulkanContext(appContextCreateInfo);

        vulkanExample = new VulkanExample();
        vulkanExample->initVulkan();
        vulkanExample->setupWindow(hInstance, WndProc);
        vulkanExample->prepare();
        vulkanExample->renderLoop();
        delete (vulkanExample);
    } catch (std::exception& e) {
        std::cout << std::format("exception: %s\n", e.what());
    }
    std::cout << "WinMain return;\n";
    return 0;
}
