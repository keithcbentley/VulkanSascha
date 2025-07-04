/*
* Vulkan Example - Descriptor indexing (VK_EXT_descriptor_indexing)
*
* Demonstrates use of descriptor indexing to dynamically index into a variable sized array of images
*
* The sample renders multiple objects with the index of the texture (descriptor) to use passed as a vertex attribute (aka "descriptor indexing")
*
* Relevant code parts are marked with [POI]
*
* Copyright (C) 2021-2025 Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/


#include "vulkanexamplebase.h"


class VulkanExample : public VulkanExampleBase
{
public:
	// We will be dynamically indexing into an array of images
	std::vector<vks::Texture2D> textures;

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t m_indexCount{ 0 };

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};

	struct Vertex {
		float pos[3];
		float uv[2];
		int32_t textureIndex;
	};

	VulkanExample() : VulkanExampleBase()
	{
		title = "Descriptor indexing";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.0f));
		camera.setRotation(glm::vec3(-35.0f, 0.0f, 0.0f));
		camera.setPerspective(45.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);

		// [POI] Enable required extensions
		m_requestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

		// [POI] Enable required extension m_vkPhysicalDeviceFeatures
		physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
		physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

		m_deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
		// Use layer settings extension to configure MoltenVK
		m_requestedInstanceExtensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);

		// Configure MoltenVK to use Metal argument buffers (needed for descriptor indexing)
		VkLayerSettingEXT layerSetting;
		layerSetting.pLayerName = "MoltenVK";
		layerSetting.pSettingName = "MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS";
		layerSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
		layerSetting.valueCount = 1;

		// Make this static so layer setting reference remains valid after leaving constructor scope
		static const VkBool32 layerSettingOn = VK_TRUE;
		layerSetting.pValues = &layerSettingOn;
		m_requestedLayerSettings.push_back(layerSetting);
#endif
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			for (auto& texture : textures) {
				texture.destroy();
			}
			vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			vertexBuffer.destroy();
			indexBuffer.destroy();
			uniformBuffer.destroy();
		}
	}

	// Generate some random textures
	void generateTextures()
	{
		textures.resize(32);
		for (size_t i = 0; i < textures.size(); i++) {
			std::random_device rndDevice;
			std::default_random_engine rndEngine(m_benchmark.active ? 0 : rndDevice());
			std::uniform_int_distribution<> rndDist(50, UCHAR_MAX);
			const int32_t dim = 3;
			const size_t bufferSize = dim * dim * 4;
			std::vector<uint8_t> texture(bufferSize);
			for (size_t j = 0; j < dim * dim; j++) {
				texture[j * 4]     = rndDist(rndEngine);
				texture[j * 4 + 1] = rndDist(rndEngine);
				texture[j * 4 + 2] = rndDist(rndEngine);
				texture[j * 4 + 3] = 255;
			}
			textures[i].fromBuffer(texture.data(), bufferSize, VK_FORMAT_R8G8B8A8_UNORM, dim, dim, m_pVulkanDevice, m_vkQueue, VK_FILTER_NEAREST);
		}
	}

	// Generates a line of cubes with randomized per-face texture indices and uploads them to the GPU
	void generateCubes()
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// Generate random per-face texture indices
		std::random_device rndDevice;
		std::default_random_engine rndEngine(m_benchmark.active ? 0 : rndDevice());
		std::uniform_int_distribution<int32_t> rndDist(0, static_cast<uint32_t>(textures.size()) - 1);

		// Generate cubes with random per-face texture indices
		const uint32_t count = 5;
		for (uint32_t i = 0; i < count; i++) {
			// Push indices to buffer
			const std::vector<uint32_t> cubeIndices = {
				0,1,2,0,2,3,
				4,5,6,4,6,7,
				8,9,10,8,10,11,
				12,13,14,12,14,15,
				16,17,18,16,18,19,
				20,21,22,20,22,23
			};
			for (auto& index : cubeIndices) {
				indices.push_back(index + static_cast<uint32_t>(vertices.size()));
			}
			// Get random per-Face texture indices that the shader will sample from
			int32_t textureIndices[6];
			for (uint32_t j = 0; j < 6; j++) {
				textureIndices[j] = rndDist(rndEngine);
			}
			// Push vertices to buffer
			float pos = 2.5f * i - (count * 2.5f / 2.0f) + 1.25f;
			const std::vector<Vertex> cube = {
				{ { -1.0f + pos, -1.0f,  1.0f }, { 0.0f, 0.0f }, textureIndices[0] },
				{ {  1.0f + pos, -1.0f,  1.0f }, { 1.0f, 0.0f }, textureIndices[0] },
				{ {  1.0f + pos,  1.0f,  1.0f }, { 1.0f, 1.0f }, textureIndices[0] },
				{ { -1.0f + pos,  1.0f,  1.0f }, { 0.0f, 1.0f }, textureIndices[0] },

				{ {  1.0f + pos,  1.0f,  1.0f }, { 0.0f, 0.0f }, textureIndices[1] },
				{ {  1.0f + pos,  1.0f, -1.0f }, { 1.0f, 0.0f }, textureIndices[1] },
				{ {  1.0f + pos, -1.0f, -1.0f }, { 1.0f, 1.0f }, textureIndices[1] },
				{ {  1.0f + pos, -1.0f,  1.0f }, { 0.0f, 1.0f }, textureIndices[1] },

				{ { -1.0f + pos, -1.0f, -1.0f }, { 0.0f, 0.0f }, textureIndices[2] },
				{ {  1.0f + pos, -1.0f, -1.0f }, { 1.0f, 0.0f }, textureIndices[2] },
				{ {  1.0f + pos,  1.0f, -1.0f }, { 1.0f, 1.0f }, textureIndices[2] },
				{ { -1.0f + pos,  1.0f, -1.0f }, { 0.0f, 1.0f }, textureIndices[2] },

				{ { -1.0f + pos, -1.0f, -1.0f }, { 0.0f, 0.0f }, textureIndices[3] },
				{ { -1.0f + pos, -1.0f,  1.0f }, { 1.0f, 0.0f }, textureIndices[3] },
				{ { -1.0f + pos,  1.0f,  1.0f }, { 1.0f, 1.0f }, textureIndices[3] },
				{ { -1.0f + pos,  1.0f, -1.0f }, { 0.0f, 1.0f }, textureIndices[3] },

				{ {  1.0f + pos,  1.0f,  1.0f }, { 0.0f, 0.0f }, textureIndices[4] },
				{ { -1.0f + pos,  1.0f,  1.0f }, { 1.0f, 0.0f }, textureIndices[4] },
				{ { -1.0f + pos,  1.0f, -1.0f }, { 1.0f, 1.0f }, textureIndices[4] },
				{ {  1.0f + pos,  1.0f, -1.0f }, { 0.0f, 1.0f }, textureIndices[4] },

				{ { -1.0f + pos, -1.0f, -1.0f }, { 0.0f, 0.0f }, textureIndices[5] },
				{ {  1.0f + pos, -1.0f, -1.0f }, { 1.0f, 0.0f }, textureIndices[5] },
				{ {  1.0f + pos, -1.0f,  1.0f }, { 1.0f, 1.0f }, textureIndices[5] },
				{ { -1.0f + pos, -1.0f,  1.0f }, { 0.0f, 1.0f }, textureIndices[5] },
			};
			for (auto& vertex : cube) {
				vertices.push_back(vertex);
			}
		}

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

	// [POI] Set up descriptor sets and set layout
	void setupDescriptors()
	{
		// Descriptor pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(textures.size()))
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
		// Increase the per-stage descriptor samplers limit on macOS/iOS (maxPerStageDescriptorUpdateAfterBindSamplers > maxPerStageDescriptorSamplers)
		descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
#endif
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Descriptor set layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// [POI] Binding 1 contains a texture array that is dynamically non-uniform sampled from in the fragment shader:
			//	outFragColor = texture(textures[nonuniformEXT(inTexIndex)], inUV);
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, static_cast<uint32_t>(textures.size()))
		};

		// [POI] The fragment shader will be using an unsized array of samplers, which has to be marked with the VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
		setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		setLayoutBindingFlags.bindingCount = 2;
		// Binding 0 is the vertex shader uniform buffer, which does not use indexing
		// Binding 1 are the fragment shader images, which use indexing
		// In the fragment shader:
		//	layout (set = 0, binding = 1) uniform sampler2D textures[];

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
		// Disable variable descriptor count feature on macOS/iOS until MoltenVK supports this feature when using combined m_vkImage sampler textures
		// Note we are using only 1 descriptor set with a fixed descriptor count/pool size, so we can simply turn off the capability for now
		std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
			0,
			0
		};
		setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		// Increase the per-stage descriptor samplers limit on macOS/iOS (maxPerStageDescriptorUpdateAfterBindSamplers > maxPerStageDescriptorSamplers)
		descriptorSetLayoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		descriptorSetLayoutCI.pNext = &setLayoutBindingFlags;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorSetLayoutCI, nullptr, &m_vkDescriptorSetLayout));

		// [POI] Descriptor sets
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		allocInfo.pNext = nullptr;
#else
		// Enable variable descriptor count feature on platforms other than macOS/iOS
		std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
			0,
			VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
		};
		setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		descriptorSetLayoutCI.pNext = &setLayoutBindingFlags;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorSetLayoutCI, nullptr, &m_vkDescriptorSetLayout));

		// [POI] Descriptor sets
		// We need to provide the descriptor counts for bindings with variable counts using a new structure
		std::vector<uint32_t> variableDesciptorCounts = {
			static_cast<uint32_t>(textures.size())
		};

		VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo = {};
		variableDescriptorCountAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
		variableDescriptorCountAllocInfo.descriptorSetCount = static_cast<uint32_t>(variableDesciptorCounts.size());
		variableDescriptorCountAllocInfo.pDescriptorCounts  = variableDesciptorCounts.data();

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		allocInfo.pNext = &variableDescriptorCountAllocInfo;
#endif

		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets(2);

		writeDescriptorSets[0] = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);

		// Image descriptors for the texture array
		std::vector<VkDescriptorImageInfo> textureDescriptors(textures.size());
		for (size_t i = 0; i < textures.size(); i++) {
			textureDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			textureDescriptors[i].sampler = textures[i].sampler;;
			textureDescriptors[i].imageView = textures[i].view;
		}

		// [POI] Second and final descriptor is a texture array
		// Unlike an array texture, these are adressed like typical arrays
		writeDescriptorSets[1] = {};
		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].dstArrayElement = 0;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSets[1].descriptorCount = static_cast<uint32_t>(textures.size());
		writeDescriptorSets[1].pBufferInfo = 0;
		writeDescriptorSets[1].dstSet = descriptorSet;
		writeDescriptorSets[1].pImageInfo = textureDescriptors.data();

		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		// Vertex bindings and attributes
		VkVertexInputBindingDescription vertexInputBinding = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
			{ 2, 0, VK_FORMAT_R32_SINT, offsetof(Vertex, textureIndex) }
		};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// Instacing m_vkPipeline
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getShadersPath() + "descriptorindexing/descriptorindexing.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		// [POI] The fragment shader does non-uniform access into our sampler array, so we need to use nonuniformEXT: texture(textures[nonuniformEXT(inTexIndex)], inUV) in it (see descriptorindexing.frag)
		shaderStages[1] = loadShader(getShadersPath() + "descriptorindexing/descriptorindexing.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass, 0);
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
	}

	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
		VK_CHECK_RESULT(uniformBuffer.map());
		updateUniformBuffersCamera();
	}

	void updateUniformBuffersCamera()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.view = camera.matrices.view;
		uniformData.model = glm::mat4(1.0f);
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	}

	void buildCommandBuffers()
	{
		VkClearValue clearValues[2];
		clearValues[0].color = m_vkClearColorValueDefault;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = m_vkRenderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = m_drawAreaWidth;
		renderPassBeginInfo.renderArea.extent.height = m_drawAreaHeight;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawCmdBuffers[i], m_indexCount, 1, 0, 0, 0);
			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		generateTextures();
		generateCubes();
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
		updateUniformBuffersCamera();
		draw();
	}

};

VULKAN_EXAMPLE_MAIN()
