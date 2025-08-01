/*
 * Vulkan Example - 3D texture loading (and generation using perlin noise) example
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"

vkcpp::VulkanContext vkcpp::s_vulkanContext;

// Vertex layout for this example
struct Vertex {
    float pos[3];
    float uv[2];
    float normal[3];
};

// Translation of Ken Perlin's JAVA implementation (http://mrl.nyu.edu/~perlin/noise/)
template <typename T>
class PerlinNoise {
private:
    uint32_t permutations[512];
    T fade(T t)
    {
        return t * t * t * (t * (t * (T)6 - (T)15) + (T)10);
    }
    T lerp(T t, T a, T b)
    {
        return a + t * (b - a);
    }
    T grad(int hash, T x, T y, T z)
    {
        // Convert LO 4 bits of hash code into 12 gradient directions
        int h = hash & 15;
        T u = h < 8 ? x : y;
        T v = h < 4 ? y : h == 12 || h == 14 ? x
                                             : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

public:
    PerlinNoise(bool applyRandomSeed)
    {
        // Generate random lookup for permutations containing all numbers from 0..255
        std::vector<uint8_t> plookup;
        plookup.resize(256);
        std::iota(plookup.begin(), plookup.end(), 0);
        std::default_random_engine rndEngine(applyRandomSeed ? std::random_device {}() : 0);
        std::shuffle(plookup.begin(), plookup.end(), rndEngine);

        for (uint32_t i = 0; i < 256; i++) {
            permutations[i] = permutations[256 + i] = plookup[i];
        }
    }
    T noise(T x, T y, T z)
    {
        // Find unit cube that contains point
        int32_t X = (int32_t)floor(x) & 255;
        int32_t Y = (int32_t)floor(y) & 255;
        int32_t Z = (int32_t)floor(z) & 255;
        // Find relative x,y,z of point in cube
        x -= floor(x);
        y -= floor(y);
        z -= floor(z);

        // Compute fade curves for each of x,y,z
        T u = fade(x);
        T v = fade(y);
        T w = fade(z);

        // Hash coordinates of the 8 cube corners
        uint32_t A = permutations[X] + Y;
        uint32_t AA = permutations[A] + Z;
        uint32_t AB = permutations[A + 1] + Z;
        uint32_t B = permutations[X + 1] + Y;
        uint32_t BA = permutations[B] + Z;
        uint32_t BB = permutations[B + 1] + Z;

        // And add blended results for 8 corners of the cube;
        T res = lerp(w, lerp(v, lerp(u, grad(permutations[AA], x, y, z), grad(permutations[BA], x - 1, y, z)), lerp(u, grad(permutations[AB], x, y - 1, z), grad(permutations[BB], x - 1, y - 1, z))),
            lerp(v, lerp(u, grad(permutations[AA + 1], x, y, z - 1), grad(permutations[BA + 1], x - 1, y, z - 1)), lerp(u, grad(permutations[AB + 1], x, y - 1, z - 1), grad(permutations[BB + 1], x - 1, y - 1, z - 1))));
        return res;
    }
};

// Fractal noise generator based on perlin noise above
template <typename T>
class FractalNoise {
private:
    PerlinNoise<T> perlinNoise;
    uint32_t octaves;
    T frequency {};
    T amplitude {};
    T persistence;

public:
    FractalNoise(const PerlinNoise<T>& perlinNoiseIn)
        : perlinNoise(perlinNoiseIn)
    {
        octaves = 6;
        persistence = (T)0.5;
    }

    T noise(T x, T y, T z)
    {
        T sum = 0;
        T frequency = (T)1;
        T amplitude = (T)1;
        T max = (T)0;
        for (uint32_t i = 0; i < octaves; i++) {
            sum += perlinNoise.noise(x * frequency, y * frequency, z * frequency) * amplitude;
            max += amplitude;
            amplitude *= persistence;
            frequency *= (T)2;
        }

        sum = sum / max;
        return (sum + (T)1.0) / (T)2.0;
    }
};

class VulkanExample : public VulkanExampleBase {
public:
    // Contains all Vulkan objects that are required to store and use a 3D texture
    vkcpp::Texture m_texture3d;

    struct Texture3DConfig {
        VkDescriptorImageInfo m_vkDescriptorImageInfo;
        VkFormat m_vkFormat;
        uint32_t m_width { 0 };
        uint32_t m_height { 0 };
        uint32_t m_depth { 0 };
        uint32_t m_mipLevels { 0 };
    } m_texture3dConfig;

    vks::Buffer vertexBuffer;
    vks::Buffer indexBuffer;
    uint32_t m_indexCount { 0 };

    struct UniformData {
        glm::mat4 projection;
        glm::mat4 modelView;
        glm::vec4 viewPos;
        // The current depth level of the texture to display
        // This is animated
        float depth = 0.0f;
    } uniformData;
    vks::Buffer uniformBuffer;

    VkPipeline m_vkPipeline { VK_NULL_HANDLE };
    VkPipelineLayout m_vkPipelineLayout { VK_NULL_HANDLE };
    VkDescriptorSet m_vkDescriptorSet { VK_NULL_HANDLE };
    VkDescriptorSetLayout m_vkDescriptorSetLayout { VK_NULL_HANDLE };

    VulkanExample()
        : VulkanExampleBase()
    {
        title = "3D textures";
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
        camera.setRotation(glm::vec3(0.0f, 15.0f, 0.0f));
        camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
        srand(m_benchmark.active ? 0 : (unsigned int)time(NULL));
    }

    ~VulkanExample()
    {
        if (m_device) {
            vkDestroyPipeline(m_device, m_vkPipeline, nullptr);
            vkDestroyPipelineLayout(m_device, m_vkPipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(m_device, m_vkDescriptorSetLayout, nullptr);
        }
    }

    // Prepare all Vulkan resources for the 3D texture (including descriptors)
    // Does not fill the texture with data
    void prepareNoiseTexture(uint32_t width, uint32_t height, uint32_t depth)
    {
        // A 3D texture is described as m_drawAreaWidth x m_drawAreaHeight x depth
        m_texture3dConfig.m_width = width;
        m_texture3dConfig.m_height = height;
        m_texture3dConfig.m_depth = depth;
        m_texture3dConfig.m_mipLevels = 1;
        m_texture3dConfig.m_vkFormat = VK_FORMAT_R8_UNORM;

        // Format support check
        // 3D texture support in Vulkan is mandatory (in contrast to OpenGL) so no need to check if it's supported
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, m_texture3dConfig.m_vkFormat, &formatProperties);
        // Check if format supports transfer
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) {
            std::cout << "Error: Device does not support flag TRANSFER_DST for selected texture format!" << std::endl;
            return;
        }
        // Check if GPU supports requested 3D texture dimensions
        // uint32_t maxImageDimension3D(m_pVulkanDevice->m_vkPhysicalDeviceProperties.limits.maxImageDimension3D);
        // if (width > maxImageDimension3D || height > maxImageDimension3D || depth > maxImageDimension3D) {
        //    std::cout << "Error: Requested texture dimensions is greater than supported 3D texture dimension!" << std::endl;
        //    return;
        //}

        // Create optimal tiled target m_vkImage
        // VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        VkImageCreateInfo vkImageCreateInfo {};
        vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        vkImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
        vkImageCreateInfo.format = m_texture3dConfig.m_vkFormat;
        vkImageCreateInfo.mipLevels = m_texture3dConfig.m_mipLevels;
        vkImageCreateInfo.arrayLayers = 1;
        vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        vkImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkImageCreateInfo.extent.width = m_texture3dConfig.m_width;
        vkImageCreateInfo.extent.height = m_texture3dConfig.m_height;
        vkImageCreateInfo.extent.depth = m_texture3dConfig.m_depth;
        // Set initial layout of the m_vkImage to undefined
        vkImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        m_texture3d.takeImage(vkcpp::Image(vkImageCreateInfo));
        m_texture3d.allocateBindImageMemory(vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL);

        // Create sampler
        VkSamplerCreateInfo vkSamplerCreateInfo = vks::initializers::samplerCreateInfo();
        vkSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        vkSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        vkSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        vkSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkSamplerCreateInfo.mipLodBias = 0.0f;
        vkSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        vkSamplerCreateInfo.minLod = 0.0f;
        vkSamplerCreateInfo.maxLod = 0.0f;
        vkSamplerCreateInfo.maxAnisotropy = 1.0;
        vkSamplerCreateInfo.anisotropyEnable = VK_FALSE;
        vkSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        m_texture3d.takeSampler(vkcpp::Sampler(vkSamplerCreateInfo));

        // Create m_vkImage m_vkImageView
        vkcpp::ImageViewCreateInfo imageViewCreateInfo(
			m_texture3d.image(),
			VK_IMAGE_VIEW_TYPE_3D,
			m_texture3dConfig.m_vkFormat,
			VK_IMAGE_ASPECT_COLOR_BIT);
        m_texture3d.takeImageView(vkcpp::ImageView(imageViewCreateInfo));

        // VK_CHECK_RESULT(vkCreateImageView(m_device, &view, nullptr, &m_textureSascha.m_vkImageView));

        // Fill m_vkImage descriptor m_vkImage info to be used descriptor set setup
        m_texture3dConfig.m_vkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_texture3dConfig.m_vkDescriptorImageInfo.imageView = m_texture3d.imageView();
        m_texture3dConfig.m_vkDescriptorImageInfo.sampler = m_texture3d.sampler();

        updateNoiseTexture();
    }

    // Generate randomized noise and upload it to the 3D texture using staging
    void updateNoiseTexture()
    {
        const uint32_t texMemSize
            = m_texture3dConfig.m_width * m_texture3dConfig.m_height * m_texture3dConfig.m_depth;

        uint8_t* data = new uint8_t[texMemSize];
        memset(data, 0, texMemSize);

        // Generate perlin based noise
        //        std::cout << "Generating " << m_textureSascha.m_width << " x " << m_textureSascha.m_height << " x " << m_textureSascha.m_depth << " noise texture..." << std::endl;

        auto tStart = std::chrono::high_resolution_clock::now();

        PerlinNoise<float> perlinNoise(!m_benchmark.active);
        FractalNoise<float> fractalNoise(perlinNoise);

        const float noiseScale = static_cast<float>(rand() % 10) + 4.0f;

#pragma omp parallel for
        for (int32_t z = 0; z < static_cast<int32_t>(m_texture3dConfig.m_depth); z++) {
            for (int32_t y = 0; y < static_cast<int32_t>(m_texture3dConfig.m_height); y++) {
                for (int32_t x = 0; x < static_cast<int32_t>(m_texture3dConfig.m_width); x++) {
                    float nx = (float)x / (float)m_texture3dConfig.m_width;
                    float ny = (float)y / (float)m_texture3dConfig.m_height;
                    float nz = (float)z / (float)m_texture3dConfig.m_depth;
                    float n = fractalNoise.noise(nx * noiseScale, ny * noiseScale, nz * noiseScale);
                    n = n - floor(n);
                    data[x
                        + y * m_texture3dConfig.m_width
                        + z * m_texture3dConfig.m_width * m_texture3dConfig.m_height]
                        = static_cast<uint8_t>(floor(n * 255));
                }
            }
        }

        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        std::cout << "Done in " << tDiff << "ms" << std::endl;

        // Create a host-visible staging buffer that contains the raw m_vkImage data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        // Buffer object
        VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
        bufferCreateInfo.size = texMemSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK_RESULT(vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Allocate host visible m_vkDeviceMemory for data upload
        VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs = {};
        vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex
            = vkcpp::findMemoryTypeIndex(
                memReqs.memoryTypeBits,
                vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT);
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t* mapped;
        VK_CHECK_RESULT(vkMapMemory(m_device, stagingMemory, 0, memReqs.size, 0, (void**)&mapped));
        memcpy(mapped, data, texMemSize);
        vkUnmapMemory(m_device, stagingMemory);

        VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // The sub resource range describes the regions of the m_vkImage we will be transitioned
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        // Optimal m_vkImage will be used as destination for the copy, so we must transfer from our
        // initial undefined m_vkImage layout to the transfer destination layout
        vks::tools::setImageLayout(
            copyCmd,
            m_texture3d.image(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy 3D noise data to texture

        // Setup buffer copy regions
        VkBufferImageCopy bufferCopyRegion {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = m_texture3dConfig.m_width;
        bufferCopyRegion.imageExtent.height = m_texture3dConfig.m_height;
        bufferCopyRegion.imageExtent.depth = m_texture3dConfig.m_depth;

        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            m_texture3d.image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion);

        // Change texture m_vkImage layout to shader read after all mip levels have been copied
        m_texture3d.setVkImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vks::tools::setImageLayout(
            copyCmd,
            m_texture3d.image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_texture3d.vkImageLayout(),
            subresourceRange);

        m_pVulkanDevice->flushCommandBuffer(copyCmd, m_queue, true);

        // Clean up staging resources
        delete[] data;
        vkFreeMemory(m_device, stagingMemory, nullptr);
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
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

            commandBuffer.begin(cmdBufInfo);
            commandBuffer.cmdBeginRenderPass(renderPassBeginInfo);
            commandBuffer.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight);
            commandBuffer.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight);
            commandBuffer.cmdBindPipeline(m_vkPipeline);
            commandBuffer.cmdBindDescriptorSet(m_vkDescriptorSet, m_vkPipelineLayout);
            commandBuffer.cmdBindVertexBuffer(vertexBuffer.m_buffer);
            commandBuffer.cmdBindIndexBuffer(indexBuffer.m_buffer, VK_INDEX_TYPE_UINT32);
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
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &stagingBuffers.vertices, vertices.size() * sizeof(Vertex), vertices.data()));
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &stagingBuffers.indices, indices.size() * sizeof(uint32_t), indices.data()));

        // Device local destination buffers
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL,
            &vertexBuffer, vertices.size() * sizeof(Vertex)));
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vkcpp::MEMORY_PROPERTY_DEVICE_LOCAL,
            &indexBuffer, indices.size() * sizeof(uint32_t)));

        // Copy from host do m_vkDevice
        m_pVulkanDevice->copyBuffer(&stagingBuffers.vertices, &vertexBuffer, m_queue);
        m_pVulkanDevice->copyBuffer(&stagingBuffers.indices, &indexBuffer, m_queue);
    }

    void setupDescriptors()
    {
        // Pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
        m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolInfo);
        // VK_CHECK_RESULT(vkCreateDescriptorPool(m_deviceOriginal, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            // Binding 1 : Fragment shader m_vkImage sampler
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

        // Set
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_descriptorPool, &m_vkDescriptorSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device, &allocInfo, &m_vkDescriptorSet));

        // Image descriptor for the 3D texture
        VkDescriptorImageInfo textureDescriptor = vks::initializers::descriptorImageInfo(
            m_texture3d.sampler(),
            m_texture3d.imageView(),
            m_texture3d.vkImageLayout());

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            // Binding 0 : Vertex shader uniform buffer
            vks::initializers::writeDescriptorSet(m_vkDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.m_vkDescriptorBufferInfo),
            // Binding 1 : Fragment shader texture sampler
            vks::initializers::writeDescriptorSet(m_vkDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textureDescriptor)
        };
        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    void preparePipelines()
    {
        // Layout
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
        VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

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
        shaderStages[0] = loadShader(getShadersPath() + "texture3d/texture3d.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "texture3d/texture3d.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_renderPassOriginal, 0);
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
        VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
            &uniformBuffer, sizeof(UniformData), &uniformData));
        VK_CHECK_RESULT(uniformBuffer.map());
    }

    void updateUniformBuffers()
    {
        uniformData.projection = camera.matrices.perspective;
        uniformData.modelView = camera.matrices.view;
        uniformData.viewPos = camera.viewPos;
        if (!paused) {
            // Animate depth
            uniformData.depth += m_frameTimer * 0.15f;
            if (uniformData.depth > 1.0f) {
                uniformData.depth = uniformData.depth - 1.0f;
            }
        }
        memcpy(uniformBuffer.m_pMapped, &uniformData, sizeof(UniformData));
    }

    void prepare()
    {
        VulkanExampleBase::prepare();
        generateQuad();
        prepareUniformBuffers();
        prepareNoiseTexture(128, 128, 128);
        setupDescriptors();
        preparePipelines();
        buildCommandBuffers();
        m_prepared = true;
    }

    void draw()
    {
		VulkanExampleBase::prepareSubmitFrameBase(m_drawCommandBuffers[m_currentBufferIndex]);
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
            if (overlay->button("Generate new texture")) {
                updateNoiseTexture();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
