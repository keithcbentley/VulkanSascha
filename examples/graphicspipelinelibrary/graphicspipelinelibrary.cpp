/*
* Vulkan Example - Using VK_EXT_graphics_pipeline_library
*
* Copyright (C) 2022-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <thread>
#include <mutex>

class VulkanExample: public VulkanExampleBase
{
public:
	bool linkTimeOptimization = true;

	vkglTF::Model scene;

	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, -2.0f, 1.0f, 0.0f);
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphicsPipelineLibraryFeatures{};

	struct PipelineLibrary {
		VkPipeline vertexInputInterface;
		VkPipeline preRasterizationShaders;
		VkPipeline fragmentOutputInterface;
		std::vector<VkPipeline> fragmentShaders;
	} pipelineLibrary;

	std::vector<VkPipeline> pipelines{};

	struct ShaderInfo {
		uint32_t* code;
		size_t size;
	};

	std::mutex mutex;
	VkPipelineCache threadPipelineCache{ VK_NULL_HANDLE };

	bool  newPipelineCreated = false;

	uint32_t splitX{ 2 };
	uint32_t splitY{ 2 };

	std::vector<glm::vec3> colors{};
	float rotation{ 0.0f };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Graphics pipeline library";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
		camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
		camera.setRotationSpeed(0.5f);

		// Enable required extensions
		m_requestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		m_requestedDeviceExtensions.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);

		// Enable required extension m_vkPhysicalDeviceFeatures
		graphicsPipelineLibraryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
		graphicsPipelineLibraryFeatures.graphicsPipelineLibrary = VK_TRUE;
		m_deviceCreatepNextChain = &graphicsPipelineLibraryFeatures;
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			for (auto pipeline : pipelines) {
				vkDestroyPipeline(m_vkDevice, pipeline, nullptr);
			}
			for (auto pipeline : pipelineLibrary.fragmentShaders) {
				vkDestroyPipeline(m_vkDevice, pipeline, nullptr);
			}
			vkDestroyPipeline(m_vkDevice, pipelineLibrary.fragmentOutputInterface, nullptr);
			vkDestroyPipeline(m_vkDevice, pipelineLibrary.preRasterizationShaders, nullptr);
			vkDestroyPipeline(m_vkDevice, pipelineLibrary.vertexInputInterface, nullptr);
			vkDestroyPipelineCache(m_vkDevice, threadPipelineCache, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
			uniformBuffer.destroy();
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

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

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			scene.bindBuffers(drawCmdBuffers[i]);

			// Render a viewport for each pipeline
			float w = (float)m_drawAreaWidth / (float)splitX;
			float h = (float)m_drawAreaHeight / (float)splitY;
			uint32_t idx = 0;
			for (uint32_t y = 0; y < splitX; y++) {
				for (uint32_t x = 0; x < splitY; x++) {
					VkViewport viewport{};
					viewport.x = w * (float)x;
					viewport.y = h * (float)y;
					viewport.width = w;
					viewport.height = h;
					viewport.minDepth = 0.0f;
					viewport.maxDepth = 1.0f;
					vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

					VkRect2D scissor{};
					scissor.extent.width = (uint32_t)w;
					scissor.extent.height = (uint32_t)h;
					scissor.offset.x = (uint32_t)w * x;
					scissor.offset.y = (uint32_t)h * y;
					vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

					if (pipelines.size() > idx) {
						vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[idx]);
						scene.draw(drawCmdBuffers[i]);
					}

					idx++;
				}
			}

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		scene.loadFromFile(getAssetPath() + "models/color_teapot_spheres.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

		// Set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	// With VK_EXT_graphics_pipeline_library we don't need to create the shader module when loading it, but instead have the driver create it at linking time
	// So we use a custom function that only loads the required shader information without actually creating the shader module
	bool loadShaderFile(std::string fileName, ShaderInfo &shaderInfo)
	{
#if defined(__ANDROID__)
		// Load shader from compressed asset
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, fileName.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		shaderInfo.size = size;
		shaderInfo.code = new uint32_t[size / 4];
		AAsset_read(asset, reinterpret_cast<char*>(shaderInfo.code), size);
		AAsset_close(asset);
		return true;
#else
		std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

		if (is.is_open())
		{
			shaderInfo.size = is.tellg();
			is.seekg(0, std::ios::beg);
			shaderInfo.code = new uint32_t[shaderInfo.size];
			is.read(reinterpret_cast<char*>(shaderInfo.code), shaderInfo.size);
			is.close();
			return true;
		} else {
			std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
			throw std::runtime_error("Could open shader file");
			return false;
		}
#endif
	}

	// Create the shared pipeline parts up-front
	void preparePipelineLibrary()
	{
		// Shared layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Create a pipeline library for the vertex input interface
		{
			VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{};
			libraryInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
			libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

			VkPipelineVertexInputStateCreateInfo vertexInputState = *vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color });
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

			VkGraphicsPipelineCreateInfo pipelineLibraryCI{};
			pipelineLibraryCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineLibraryCI.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
			pipelineLibraryCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineLibraryCI.pNext = &libraryInfo;
			pipelineLibraryCI.pInputAssemblyState = &inputAssemblyState;
			pipelineLibraryCI.pVertexInputState = &vertexInputState;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineLibraryCI, nullptr, &pipelineLibrary.vertexInputInterface));
		}

		// Creata a pipeline library for the vertex shader stage
		{
			VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{};
			libraryInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
			libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

			VkDynamicState vertexDynamicStates[2] = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR };

			VkPipelineDynamicStateCreateInfo dynamicInfo{};
			dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicInfo.dynamicStateCount = 2;
			dynamicInfo.pDynamicStates = vertexDynamicStates;

			VkPipelineViewportStateCreateInfo viewportState = {};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

			ShaderInfo shaderInfo{};
			loadShaderFile(getShadersPath() + "graphicspipelinelibrary/shared.vert.spv", shaderInfo);

			VkShaderModuleCreateInfo shaderModuleCI{};
			shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCI.codeSize = shaderInfo.size;
			shaderModuleCI.pCode = shaderInfo.code;

			VkPipelineShaderStageCreateInfo shaderStageCI{};
			shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageCI.pNext = &shaderModuleCI;
			shaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderStageCI.pName = "main";

			VkGraphicsPipelineCreateInfo pipelineLibraryCI{};
			pipelineLibraryCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineLibraryCI.pNext = &libraryInfo;
			pipelineLibraryCI.renderPass = m_vkRenderPass;
			pipelineLibraryCI.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
			pipelineLibraryCI.stageCount = 1;
			pipelineLibraryCI.pStages = &shaderStageCI;
			pipelineLibraryCI.layout = m_vkPipelineLayout;
			pipelineLibraryCI.pDynamicState = &dynamicInfo;
			pipelineLibraryCI.pViewportState = &viewportState;
			pipelineLibraryCI.pRasterizationState = &rasterizationState;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineLibraryCI, nullptr, &pipelineLibrary.preRasterizationShaders));

			delete[] shaderInfo.code;
		}

		// Create a pipeline library for the fragment output interface
		{
			VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{};
			libraryInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
			libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

			VkPipelineColorBlendAttachmentState  blendAttachmentSstate = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
			VkPipelineColorBlendStateCreateInfo  colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentSstate);
			VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

			VkGraphicsPipelineCreateInfo pipelineLibraryCI{};
			pipelineLibraryCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineLibraryCI.pNext = &libraryInfo;
			pipelineLibraryCI.layout = m_vkPipelineLayout;
			pipelineLibraryCI.renderPass = m_vkRenderPass;
			pipelineLibraryCI.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
			pipelineLibraryCI.pColorBlendState = &colorBlendState;
			pipelineLibraryCI.pMultisampleState = &multisampleState;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineLibraryCI, nullptr, &pipelineLibrary.fragmentOutputInterface));
		}
	}

	void threadFn()
	{
		const std::lock_guard<std::mutex> lock(mutex);

		auto start = std::chrono::steady_clock::now();

		prepareNewPipeline();
		newPipelineCreated = true;

		// Change viewport/draw count
		if (pipelines.size() > splitX * splitY) {
			splitX++;
			splitY++;
		}

		auto delta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
		std::cout << "Pipeline created in " << delta.count() << " microseconds\n";
	}

	// Create a new pipeline using the pipeline library and a customized fragment shader
	// Used from a thread
	void prepareNewPipeline()
	{
		// Create the fragment shader part of the pipeline library with some random options
		VkGraphicsPipelineLibraryCreateInfoEXT libraryInfo{};
		libraryInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
		libraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineMultisampleStateCreateInfo  multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		// Using the pipeline library extension, we can skip the pipeline shader module creation and directly pass the shader code to the pipeline
		ShaderInfo shaderInfo{};
		loadShaderFile(getShadersPath() + "graphicspipelinelibrary/uber.frag.spv", shaderInfo);

		VkShaderModuleCreateInfo shaderModuleCI{};
		shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCI.codeSize = shaderInfo.size;
		shaderModuleCI.pCode = shaderInfo.code;

		VkPipelineShaderStageCreateInfo shaderStageCI{};
		shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCI.pNext = &shaderModuleCI;
		shaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStageCI.pName = "main";

		// Select lighting model using a specialization constant
		srand(m_benchmark.active ? 0 : ((unsigned int)time(NULL)));
		uint32_t lighting_model = (int)(rand() % 4);

		// Each shader constant of a shader stage corresponds to one map entry
		VkSpecializationMapEntry specializationMapEntry{};
		specializationMapEntry.constantID = 0;
		specializationMapEntry.size = sizeof(uint32_t);

		VkSpecializationInfo specializationInfo{};
		specializationInfo.mapEntryCount = 1;
		specializationInfo.pMapEntries = &specializationMapEntry;
		specializationInfo.dataSize = sizeof(uint32_t);
		specializationInfo.pData = &lighting_model;

		shaderStageCI.pSpecializationInfo = &specializationInfo;

		VkGraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.pNext = &libraryInfo;
		pipelineCI.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
		pipelineCI.stageCount = 1;
		pipelineCI.pStages = &shaderStageCI;
		pipelineCI.layout = m_vkPipelineLayout;
		pipelineCI.renderPass = m_vkRenderPass;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pMultisampleState = &multisampleState;
		VkPipeline fragmentShader = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, threadPipelineCache, 1, &pipelineCI, nullptr, &fragmentShader));

		// Create the pipeline using the pre-built pipeline library parts
		// Except for above fragment shader part all parts have been pre-built and will be re-used
		std::vector<VkPipeline> libraries = {
			pipelineLibrary.vertexInputInterface,
			pipelineLibrary.preRasterizationShaders,
			fragmentShader,
			pipelineLibrary.fragmentOutputInterface };

		// Link the library parts into a graphics pipeline
		VkPipelineLibraryCreateInfoKHR pipelineLibraryCI{};
		pipelineLibraryCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
		pipelineLibraryCI.libraryCount = static_cast<uint32_t>(libraries.size());
		pipelineLibraryCI.pLibraries = libraries.data();

		// If set to true, we pass VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT which will let the implementation do additional optimizations at link time
		// This trades in pipeline creation time for run-time performance
		bool optimized = true;

		VkGraphicsPipelineCreateInfo executablePipelineCI{};
		executablePipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		executablePipelineCI.pNext = &pipelineLibraryCI;
		executablePipelineCI.layout = m_vkPipelineLayout;
		if (linkTimeOptimization)
		{
			// If link time optimization is activated in the UI, we set the VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT flag which will let the implementation do additional optimizations at link time
			// This trades in pipeline creation time for run-time performance
			executablePipelineCI.flags = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
		}

		VkPipeline executable = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, threadPipelineCache, 1, &executablePipelineCI, nullptr, &executable));

		pipelines.push_back(executable);
		// Push fragment shader to list for deletion in the sample's destructor
		pipelineLibrary.fragmentShaders.push_back(fragmentShader);

		delete[] shaderInfo.code;
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Create the vertex shader uniform buffer block
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffer,
			sizeof(UniformData)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffer.map());

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		if (!paused) {
			rotation += m_frameTimer * 0.1f;
		}
		camera.setPerspective(45.0f, ((float)m_drawAreaWidth / (float)splitX) / ((float)m_drawAreaHeight / (float)splitY), 0.1f, 256.0f);
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view * glm::rotate(glm::mat4(1.0f), glm::radians(rotation * 360.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		m_vkSubmitInfo.commandBufferCount = 1;
		m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelineLibrary();
		buildCommandBuffers();

		// Create a separate pipeline cache for the pipeline creation thread
		VkPipelineCacheCreateInfo pipelineCachCI = {};
		pipelineCachCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		vkCreatePipelineCache(m_vkDevice, &pipelineCachCI, nullptr, &threadPipelineCache);

		// Create first pipeline using a background thread
		std::thread pipelineGenerationThread(&VulkanExample::threadFn, this);
		pipelineGenerationThread.detach();

		m_prepared = true;
	}

	virtual void render()
	{
		if (!m_prepared)
			return;
		if (newPipelineCreated)
		{
			newPipelineCreated = false;
			vkQueueWaitIdle(m_vkQueue);
			buildCommandBuffers();
		}
		updateUniformBuffers();
		draw();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		overlay->checkBox("Link time optimization", &linkTimeOptimization);
		if (overlay->button("New pipeline")) {
			// Spwan a thread to create a new pipeline in the background
			std::thread pipelineGenerationThread(&VulkanExample::threadFn, this);
			pipelineGenerationThread.detach();
		}
	}
};

VULKAN_EXAMPLE_MAIN()