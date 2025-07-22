/*
* Vulkan Example - Dynamic uniform buffers
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*
* Summary:
* Demonstrates the use of dynamic uniform buffers.
*
* Instead of using one uniform buffer per-object, this example allocates one big uniform buffer
* with respect to the alignment reported by the m_vkDevice via minUniformBufferOffsetAlignment that
* contains all matrices for the objects in the scene.
*
* The used descriptor type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC then allows to set a dynamic
* offset used to pass data from the single uniform buffer to the connected shader binding point.
*/

#include "vulkanexamplebase.h"

vkcpp::VulkanContext vkcpp::s_vulkanContext;

#define OBJECT_INSTANCES 125

// Vertex layout for this example
struct Vertex {
	float pos[3];
	float color[3];
};

// Wrapper functions for aligned m_vkDeviceMemory allocation
// There is currently no standard for this in C++ that works across all platforms and vendors, so we abstract this
void* alignedAlloc(size_t size, size_t alignment)
{
	void *data = nullptr;
#if defined(_MSC_VER) || defined(__MINGW32__)
	data = _aligned_malloc(size, alignment);
#else
	int res = posix_memalign(&data, alignment, size);
	if (res != 0)
		data = nullptr;
#endif
	return data;
}

void alignedFree(void* data)
{
#if	defined(_MSC_VER) || defined(__MINGW32__)
	_aligned_free(data);
#else
	free(data);
#endif
}

class VulkanExample : public VulkanExampleBase
{
public:
	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t m_indexCount{ 0 };

	struct {
		vks::Buffer view;
		vks::Buffer dynamic;
	} uniformBuffers;

	struct {
		glm::mat4 projection;
		glm::mat4 view;
	} uboVS;

	// Store random per-object rotations
	glm::vec3 rotations[OBJECT_INSTANCES];
	glm::vec3 rotationSpeeds[OBJECT_INSTANCES];

	// One big uniform buffer that contains all matrices
	// Note that we need to manually allocate the data to cope for GPU-specific uniform buffer offset alignments
	struct UboDataDynamic {
		glm::mat4* model{ nullptr };
	} uboDataDynamic;

	vkcpp::PipelineLayout m_pipelineLayout;
	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	vkcpp::DescriptorSetLayout m_descriptorSetLayout;
	vkcpp::DescriptorSet m_descriptorSet;
	
	float animationTimer{ 0.0f };

	size_t dynamicAlignment{ 0 };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Dynamic uniform buffers";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -30.0f));
		camera.setRotation(glm::vec3(0.0f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
	}

	~VulkanExample()
	{
		if (m_device) {
			if (uboDataDynamic.model) {
				alignedFree(uboDataDynamic.model);
			}
			vkDestroyPipeline(m_device, m_vkPipeline, nullptr);
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

		for (int32_t i = 0; i < m_drawCommandBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			vkcpp::CommandBuffer commandBuffer
				= vkcpp::CommandBuffer::makeCopy(m_drawCommandBuffers[i]);
			commandBuffer
				.begin(cmdBufInfo)
				.cmdBeginRenderPass(renderPassBeginInfo)
				.cmdSetViewport(m_drawAreaWidth, m_drawAreaHeight)
				.cmdSetScissor(m_drawAreaWidth, m_drawAreaHeight)
				.cmdBindPipeline(m_vkPipeline)
				.cmdBindVertexBuffer(vertexBuffer.m_buffer)
				.cmdBindIndexBuffer(indexBuffer.m_buffer, VK_INDEX_TYPE_UINT32);

			// Render multiple objects using different model matrices by dynamically offsetting into one uniform buffer
			for (uint32_t j = 0; j < OBJECT_INSTANCES; j++)
			{
				// One dynamic offset per dynamic descriptor to offset into the ubo containing all model matrices
				uint32_t dynamicOffset = j * static_cast<uint32_t>(dynamicAlignment);
				// Bind the descriptor set for rendering a mesh using the dynamic offset
				commandBuffer
					.cmdBindDescriptorSetDynamicOffset(m_descriptorSet, m_pipelineLayout, dynamicOffset)
					.cmdDrawIndexed(m_indexCount);
			}

			drawUI(commandBuffer);

			commandBuffer.cmdEndRenderPass();
			commandBuffer.end();

		}
	}

	void generateCube()
	{
		// Setup vertices indices for a colored cube
		std::vector<Vertex> vertices = {
			{ { -1.0f, -1.0f,  1.0f },{ 1.0f, 0.0f, 0.0f } },
			{ {  1.0f, -1.0f,  1.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f } },
			{ { -1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f } },
			{ {  1.0f, -1.0f, -1.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f } },
		};

		std::vector<uint32_t> indices = {
			0,1,2, 2,3,0, 1,5,6, 6,2,1, 7,6,5, 5,4,7, 4,0,3, 3,7,4, 4,5,1, 1,0,4, 3,2,6, 6,7,3,
		};

		m_indexCount = static_cast<uint32_t>(indices.size());

		// Create buffers
		// For the sake of simplicity we won't stage the vertex data to the gpu m_vkDeviceMemory
		// Vertex buffer
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
			&vertexBuffer,
			vertices.size() * sizeof(Vertex),
			vertices.data()));
		// Index buffer
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
			&indexBuffer,
			indices.size() * sizeof(uint32_t),
			indices.data()));
	}

	void setupDescriptors()
	{
		//	Descriptor Pool
		vkcpp::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
		descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		descriptorPoolCreateInfo.addDescriptorCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1);
		m_descriptorPool = vkcpp::DescriptorPool(descriptorPoolCreateInfo);


		// Descriptor Set Layout
		constexpr int uniformBufferBindingIndex = 0;
		constexpr int dynamicUniformBufferBindingIndex = 1;

		vkcpp::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
		descriptorSetLayoutCreateInfo
			.addDescriptorSetLayoutBinding(
				uniformBufferBindingIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vkcpp::SHADER_STAGE_VERTEX)
			.addDescriptorSetLayoutBinding(
				dynamicUniformBufferBindingIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, vkcpp::SHADER_STAGE_VERTEX);

		m_descriptorSetLayout = vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo);

		// Descriptor Set
		m_descriptorSet = vkcpp::DescriptorSet(m_descriptorSetLayout, m_descriptorPool);

		vkcpp::DescriptorSetUpdater descriptorSetUpdater(m_descriptorSet);
		descriptorSetUpdater
			.addBufferWriteDescriptor(
				uniformBufferBindingIndex,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				uniformBuffers.view.m_vkDescriptorBufferInfo)
			.addBufferWriteDescriptor(
				dynamicUniformBufferBindingIndex,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				uniformBuffers.dynamic.m_vkDescriptorBufferInfo);

		descriptorSetUpdater.updateDescriptorSets();
	}

	void preparePipelines()
	{
		// Layout
		vkcpp::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.addDescriptorSetLayout(m_descriptorSetLayout);
		m_pipelineLayout = vkcpp::PipelineLayout(pipelineLayoutCreateInfo);

		//VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0,  VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Vertex bindings and attributes
		VkVertexInputBindingDescription vertexInputBinding = { 
			vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),	// Location 0 : Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)),	// Location 1 : Color
		};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		shaderStages[0] = loadShader(getShadersPath() + "dynamicuniformbuffer/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "dynamicuniformbuffer/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo
			= vks::initializers::pipelineCreateInfo(m_pipelineLayout, m_renderPassOriginal, 0);
		pipelineCreateInfo.pVertexInputState = &vertexInputStateCI;
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
		// Allocate data for the dynamic uniform buffer object
		// We allocate this manually as the alignment of the offset differs between GPUs

		// Calculate required alignment based on minimum m_vkDevice offset alignment
		size_t minUboAlignment = vkcpp::vkPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
		dynamicAlignment = sizeof(glm::mat4);
		if (minUboAlignment > 0) {
			dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}

		size_t bufferSize = OBJECT_INSTANCES * dynamicAlignment;

		uboDataDynamic.model = (glm::mat4*)alignedAlloc(bufferSize, dynamicAlignment);
		assert(uboDataDynamic.model);

		std::cout << "minUniformBufferOffsetAlignment = " << minUboAlignment << std::endl;
		std::cout << "dynamicAlignment = " << dynamicAlignment << std::endl;

		// Vertex shader uniform buffer block

		// Static shared uniform buffer object with projection and m_vkImageView matrix
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT,
			&uniformBuffers.view,
			sizeof(uboVS)));

		// Uniform buffer object with per-object matrices
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			vkcpp::MEMORY_PROPERTY_HOST_VISIBLE,
			&uniformBuffers.dynamic,
			bufferSize));

		// Override descriptor range to [base, base + dynamicAlignment]
		uniformBuffers.dynamic.m_vkDescriptorBufferInfo.range = dynamicAlignment;

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.view.map());
		VK_CHECK_RESULT(uniformBuffers.dynamic.map());

		// Prepare per-object matrices with offsets and random rotations
		std::default_random_engine rndEngine(m_benchmark.active ? 0 : (unsigned)time(nullptr));
		std::normal_distribution<float> rndDist(-1.0f, 1.0f);
		for (uint32_t i = 0; i < OBJECT_INSTANCES; i++) {
			rotations[i] = glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine)) * 2.0f * (float)M_PI;
			rotationSpeeds[i] = glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine));
		}

		updateUniformBuffers();
		updateDynamicUniformBuffer();
	}

	void updateUniformBuffers()
	{
		// Fixed ubo with projection and m_vkImageView matrices
		uboVS.projection = camera.matrices.perspective;
		uboVS.view = camera.matrices.view;

		memcpy(uniformBuffers.view.m_pMapped, &uboVS, sizeof(uboVS));
	}

	void updateDynamicUniformBuffer()
	{
		// Update at max. 60 fps
		animationTimer += m_frameTimer;	
		if (animationTimer <= 1.0f / 60.0f) {
			return;
		}

		// Dynamic ubo with per-object model matrices indexed by offsets in the command buffer
		uint32_t dim = static_cast<uint32_t>(pow(OBJECT_INSTANCES, (1.0f / 3.0f)));
		glm::vec3 offset(5.0f);

		for (uint32_t x = 0; x < dim; x++)
		{
			for (uint32_t y = 0; y < dim; y++)
			{
				for (uint32_t z = 0; z < dim; z++)
				{
					uint32_t index = x * dim * dim + y * dim + z;

					// Aligned offset
					glm::mat4* modelMat = (glm::mat4*)(((uint64_t)uboDataDynamic.model + (index * dynamicAlignment)));

					// Update rotations
					rotations[index] += animationTimer * rotationSpeeds[index];

					// Update matrices
					glm::vec3 pos = glm::vec3(-((dim * offset.x) / 2.0f) + offset.x / 2.0f + x * offset.x, -((dim * offset.y) / 2.0f) + offset.y / 2.0f + y * offset.y, -((dim * offset.z) / 2.0f) + offset.z / 2.0f + z * offset.z);
					*modelMat = glm::translate(glm::mat4(1.0f), pos);
					*modelMat = glm::rotate(*modelMat, rotations[index].x, glm::vec3(1.0f, 1.0f, 0.0f));
					*modelMat = glm::rotate(*modelMat, rotations[index].y, glm::vec3(0.0f, 1.0f, 0.0f));
					*modelMat = glm::rotate(*modelMat, rotations[index].z, glm::vec3(0.0f, 0.0f, 1.0f));
				}
			}
		}

		animationTimer = 0.0f;

		memcpy(uniformBuffers.dynamic.m_pMapped, uboDataDynamic.model, uniformBuffers.dynamic.m_size);
		// Flush to make changes visible to the host
		VkMappedMemoryRange memoryRange = vks::initializers::mappedMemoryRange();
		memoryRange.memory = uniformBuffers.dynamic.m_deviceMemory;
		memoryRange.size = uniformBuffers.dynamic.m_size;
		vkFlushMappedMemoryRanges(m_device, 1, &memoryRange);
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		generateCube();
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
		updateDynamicUniformBuffer();
		draw();
	}
};

VULKAN_EXAMPLE_MAIN()
