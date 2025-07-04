/*
 * Vulkan Example - Using shader objects via VK_EXT_shader_object
 *
 * Copyright (C) 2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

class VulkanExample: public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	// Same uniform buffer layout as shader
	struct UniformData {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	VkShaderEXT shaders[2];

	VkPhysicalDeviceShaderObjectFeaturesEXT enabledShaderObjectFeaturesEXT{};
	VkPhysicalDeviceDynamicRenderingFeaturesKHR enabledDynamicRenderingFeaturesKHR{};

	PFN_vkCreateShadersEXT vkCreateShadersEXT{ VK_NULL_HANDLE };
	PFN_vkDestroyShaderEXT vkDestroyShaderEXT{ VK_NULL_HANDLE };
	PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT{ VK_NULL_HANDLE };
	PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT{ VK_NULL_HANDLE };

	// VK_EXT_shader_objects requires render passes to be dynamic
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR{ VK_NULL_HANDLE };
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR{ VK_NULL_HANDLE };

	// With VK_EXT_shader_object pipeline state must be set at command buffer creation using these functions
	PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT{ VK_NULL_HANDLE };
	PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT{ VK_NULL_HANDLE };

	// VK_EXT_vertex_input_dynamic_state
	PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Shader objects (VK_EXT_shader_object)";
		camera.type = Camera::CameraType::lookat;
		camera.setPosition(glm::vec3(0.0f, 0.0f, -10.5f));
		camera.setRotation(glm::vec3(-25.0f, 15.0f, 0.0f));
		camera.setRotationSpeed(0.5f);
		camera.setPerspective(60.0f, (float)(m_drawAreaWidth) / (float)m_drawAreaHeight, 0.1f, 256.0f);

		enabledDeviceExtensions.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);

		enabledDeviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

		// With VK_EXT_shader_object all baked pipeline state is set dynamically at command buffer creation, so we need to enable additional extensions
		enabledDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);

		// Since we are not requiring Vulkan 1.2, we need to enable some additional extensios for dynamic rendering
		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);

		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		enabledShaderObjectFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
		enabledShaderObjectFeaturesEXT.shaderObject = VK_TRUE;

		enabledDynamicRenderingFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
		enabledDynamicRenderingFeaturesKHR.dynamicRendering = VK_TRUE;
		enabledDynamicRenderingFeaturesKHR.pNext = &enabledShaderObjectFeaturesEXT;

		deviceCreatepNextChain = &enabledDynamicRenderingFeaturesKHR;
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayout, nullptr);
			uniformBuffer.destroy();
			vkDestroyShaderEXT(m_vkDevice, shaders[0], nullptr);
			vkDestroyShaderEXT(m_vkDevice, shaders[1], nullptr);
		}
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		scene.loadFromFile(getAssetPath() + "models/treasure_smooth.gltf", vulkanDevice, m_vkQueue, glTFLoadingFlags);
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
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &descriptorSetLayout));
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCI, nullptr, &m_vkPipelineLayout));
		// Sets
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0 : Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor) 
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	// Loads a binary shader file
	void _loadShader(std::string filename, char* &code, size_t &size) {
		// @todo: Android
		std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);
		if (is.is_open())
		{
			size = is.tellg();
			is.seekg(0, std::ios::beg);
			code = new char[size];
			is.read(code, size);
			is.close();
			assert(size > 0);
		}
		else
		{
			vks::tools::exitFatal("Error: Could not open shader " + filename, VK_ERROR_UNKNOWN);
		}
	}

	void createShaderObjects()
	{
		size_t shaderCodeSizes[2]{};
		char* shaderCodes[2]{};

		VkShaderCreateInfoEXT shaderCreateInfos[2]{};

		// With VK_EXT_shader_object we can generate an implementation dependent binary file that's faster to load
		// So we check if the binray files exist and if we can load it instead of the SPIR-V
		bool binaryShadersLoaded = false;

		if (vks::tools::fileExists(getShadersPath() + "shaderobjects/phong.vert.bin") && vks::tools::fileExists(getShadersPath() + "shaderobjects/phong.frag.bin")) {
			// VS
			_loadShader(getShadersPath() + "shaderobjects/phong.vert.bin", shaderCodes[0], shaderCodeSizes[0]);
			shaderCreateInfos[0].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
			shaderCreateInfos[0].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
			shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderCreateInfos[0].nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderCreateInfos[0].codeType = VK_SHADER_CODE_TYPE_BINARY_EXT;
			shaderCreateInfos[0].pCode = shaderCodes[0];
			shaderCreateInfos[0].codeSize = shaderCodeSizes[0];
			shaderCreateInfos[0].pName = "main";
			shaderCreateInfos[0].setLayoutCount = 1;
			shaderCreateInfos[0].pSetLayouts = &descriptorSetLayout;

			// FS
			_loadShader(getShadersPath() + "shaderobjects/phong.frag.bin", shaderCodes[1], shaderCodeSizes[1]);
			shaderCreateInfos[1].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
			shaderCreateInfos[1].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
			shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderCreateInfos[1].nextStage = 0;
			shaderCreateInfos[1].codeType = VK_SHADER_CODE_TYPE_BINARY_EXT;
			shaderCreateInfos[1].pCode = shaderCodes[1];
			shaderCreateInfos[1].codeSize = shaderCodeSizes[1];
			shaderCreateInfos[1].pName = "main";
			shaderCreateInfos[1].setLayoutCount = 1;
			shaderCreateInfos[1].pSetLayouts = &descriptorSetLayout;

			VkResult result = vkCreateShadersEXT(m_vkDevice, 2, shaderCreateInfos, nullptr, shaders);
			// If the function returns e.g. VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT, the binary file is no longer (or not at all) compatible with the current implementation
			if (result == VK_SUCCESS) {
				binaryShadersLoaded = true;
			} else {
				std::cout << "Could not load binary shader files (" << vks::tools::errorString(result) << ", loading SPIR - V instead\n";
			}
		}

		// If the binary files weren't present, or we could not load them, we load from SPIR-V
		if (!binaryShadersLoaded) {
			// VS
			_loadShader(getShadersPath() + "shaderobjects/phong.vert.spv", shaderCodes[0], shaderCodeSizes[0]);
			shaderCreateInfos[0].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
			shaderCreateInfos[0].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
			shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderCreateInfos[0].nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderCreateInfos[0].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
			shaderCreateInfos[0].pCode = shaderCodes[0];
			shaderCreateInfos[0].codeSize = shaderCodeSizes[0];
			shaderCreateInfos[0].pName = "main";
			shaderCreateInfos[0].setLayoutCount = 1;
			shaderCreateInfos[0].pSetLayouts = &descriptorSetLayout;

			// FS
			_loadShader(getShadersPath() + "shaderobjects/phong.frag.spv", shaderCodes[1], shaderCodeSizes[1]);
			shaderCreateInfos[1].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
			shaderCreateInfos[1].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
			shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderCreateInfos[1].nextStage = 0;
			shaderCreateInfos[1].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
			shaderCreateInfos[1].pCode = shaderCodes[1];
			shaderCreateInfos[1].codeSize = shaderCodeSizes[1];
			shaderCreateInfos[1].pName = "main";
			shaderCreateInfos[1].setLayoutCount = 1;
			shaderCreateInfos[1].pSetLayouts = &descriptorSetLayout;

			VK_CHECK_RESULT(vkCreateShadersEXT(m_vkDevice, 2, shaderCreateInfos, nullptr, shaders));

			// Store the binary shader files so we can try to load them at the next start
			size_t dataSize{ 0 };
			char* data{ nullptr };
			std::fstream is;

			vkGetShaderBinaryDataEXT(m_vkDevice, shaders[0], &dataSize, nullptr);
			data = new char[dataSize];
			vkGetShaderBinaryDataEXT(m_vkDevice, shaders[0], &dataSize, data);
			is.open(getShadersPath() + "shaderobjects/phong.vert.bin", std::ios::binary | std::ios::out);
			is.write(data, dataSize);
			is.close();
			delete[] data;

			vkGetShaderBinaryDataEXT(m_vkDevice, shaders[1], &dataSize, nullptr);
			data = new char[dataSize];
			vkGetShaderBinaryDataEXT(m_vkDevice, shaders[1], &dataSize, data);
			is.open(getShadersPath() + "shaderobjects/phong.frag.bin", std::ios::binary | std::ios::out);
			is.write(data, dataSize);
			is.close();
			delete[] data;
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			// Transition color and depth images for drawing
			vks::tools::insertImageMemoryBarrier(
				drawCmdBuffers[i],
				swapChain.images[i],
				0,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
			vks::tools::insertImageMemoryBarrier(
				drawCmdBuffers[i],
				m_defaultDepthStencil.m_vkImage,
				0,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

			// New structures are used to define the attachments used in dynamic rendering
			VkRenderingAttachmentInfoKHR colorAttachment{};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			colorAttachment.imageView = swapChain.imageViews[i];
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

			// A single depth stencil attachment info can be used, but they can also be specified separately.
			// When both are specified separately, the only requirement is that the m_vkImage m_vkImageView is identical.			
			VkRenderingAttachmentInfoKHR depthStencilAttachment{};
			depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
			depthStencilAttachment.imageView = m_defaultDepthStencil.m_vkImageView;
			depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };

			VkRenderingInfoKHR renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
			renderingInfo.renderArea = { 0, 0, m_drawAreaWidth, m_drawAreaHeight };
			renderingInfo.layerCount = 1;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;
			renderingInfo.pDepthAttachment = &depthStencilAttachment;
			renderingInfo.pStencilAttachment = &depthStencilAttachment;

			// Begin dynamic rendering
			vkCmdBeginRenderingKHR(drawCmdBuffers[i], &renderingInfo);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);

			// No more pipelines required, everything is bound at command buffer level
			// This also means that we need to explicitly set a lot of the state to be spec compliant

			vkCmdSetViewportWithCountEXT(drawCmdBuffers[i], 1, &viewport);
			vkCmdSetScissorWithCountEXT(drawCmdBuffers[i], 1, &scissor);
			vkCmdSetCullModeEXT(drawCmdBuffers[i], VK_CULL_MODE_BACK_BIT);
			vkCmdSetFrontFaceEXT(drawCmdBuffers[i], VK_FRONT_FACE_COUNTER_CLOCKWISE);
			vkCmdSetDepthTestEnableEXT(drawCmdBuffers[i], VK_TRUE);
			vkCmdSetDepthWriteEnableEXT(drawCmdBuffers[i], VK_TRUE);
			vkCmdSetDepthCompareOpEXT(drawCmdBuffers[i], VK_COMPARE_OP_LESS_OR_EQUAL);
			vkCmdSetPrimitiveTopologyEXT(drawCmdBuffers[i], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			vkCmdSetRasterizerDiscardEnableEXT(drawCmdBuffers[i], VK_FALSE);
			vkCmdSetPolygonModeEXT(drawCmdBuffers[i], VK_POLYGON_MODE_FILL);
			vkCmdSetRasterizationSamplesEXT(drawCmdBuffers[i], VK_SAMPLE_COUNT_1_BIT);
			vkCmdSetAlphaToCoverageEnableEXT(drawCmdBuffers[i], VK_FALSE);
			vkCmdSetDepthBiasEnableEXT(drawCmdBuffers[i], VK_FALSE);
			vkCmdSetStencilTestEnableEXT(drawCmdBuffers[i], VK_FALSE);
			vkCmdSetPrimitiveRestartEnableEXT(drawCmdBuffers[i], VK_FALSE);

			const uint32_t sampleMask = 0xFF;
			vkCmdSetSampleMaskEXT(drawCmdBuffers[i], VK_SAMPLE_COUNT_1_BIT, &sampleMask);

			const VkBool32 colorBlendEnables = false;
			const VkColorComponentFlags colorBlendComponentFlags = 0xf;
			const VkColorBlendEquationEXT colorBlendEquation{};
			vkCmdSetColorBlendEnableEXT(drawCmdBuffers[i], 0, 1, &colorBlendEnables);
			vkCmdSetColorWriteMaskEXT(drawCmdBuffers[i], 0, 1, &colorBlendComponentFlags);

			VkVertexInputBindingDescription2EXT vertexInputBinding{};
			vertexInputBinding.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
			vertexInputBinding.binding = 0;
			vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			vertexInputBinding.stride = sizeof(vkglTF::Vertex);
			vertexInputBinding.divisor = 1;

			std::vector<VkVertexInputAttributeDescription2EXT> vertexAttributes = {
				{ VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, nullptr, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, pos) },
				{ VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, nullptr, 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, normal) },
				{ VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, nullptr, 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vkglTF::Vertex, color) }
			};

			vkCmdSetVertexInputEXT(drawCmdBuffers[i], 1, &vertexInputBinding, 3, vertexAttributes.data());

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			scene.bindBuffers(drawCmdBuffers[i]);

			// Binding the shaders
			VkShaderStageFlagBits stages[2] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
			vkCmdBindShadersEXT(drawCmdBuffers[i], 2, stages, shaders);
			scene.draw(drawCmdBuffers[i]);

			// @todo: Currently disabled, the UI needs to be adopated to work with shader objects
			// drawUI(drawCmdBuffers[i]);

			// End dynamic rendering
			vkCmdEndRenderingKHR(drawCmdBuffers[i]);

			// Transition color m_vkImage for presentation
			vks::tools::insertImageMemoryBarrier(
				drawCmdBuffers[i],
				swapChain.images[i],
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				0,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Create the vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData)));
		VK_CHECK_RESULT(uniformBuffer.map());
		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uniformData.projection = camera.matrices.perspective;
		uniformData.modelView = camera.matrices.view;
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

		// As this is an extension, we need to explicitly load the function pointers for the shader object commands used in this sample

		vkCreateShadersEXT = reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCreateShadersEXT"));
		vkDestroyShaderEXT = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkDestroyShaderEXT"));
		vkCmdBindShadersEXT = reinterpret_cast<PFN_vkCmdBindShadersEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdBindShadersEXT"));
		vkGetShaderBinaryDataEXT = reinterpret_cast<PFN_vkGetShaderBinaryDataEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkGetShaderBinaryDataEXT"));

		vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdBeginRenderingKHR"));
		vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdEndRenderingKHR"));

		vkCmdSetAlphaToCoverageEnableEXT = reinterpret_cast<PFN_vkCmdSetAlphaToCoverageEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetAlphaToCoverageEnableEXT"));
		vkCmdSetColorBlendEnableEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetColorBlendEnableEXT"));
		vkCmdSetColorWriteMaskEXT = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetColorWriteMaskEXT"));
		vkCmdSetCullModeEXT = reinterpret_cast<PFN_vkCmdSetCullModeEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetCullModeEXT"));
		vkCmdSetDepthBiasEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthBiasEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetDepthBiasEnableEXT"));
		vkCmdSetDepthCompareOpEXT = reinterpret_cast<PFN_vkCmdSetDepthCompareOpEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetDepthCompareOpEXT"));
		vkCmdSetDepthTestEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthTestEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetDepthTestEnableEXT"));
		vkCmdSetDepthWriteEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthWriteEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetDepthWriteEnableEXT"));
		vkCmdSetFrontFaceEXT = reinterpret_cast<PFN_vkCmdSetFrontFaceEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetFrontFaceEXT"));
		vkCmdSetPolygonModeEXT = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetPolygonModeEXT"));
		vkCmdSetPrimitiveRestartEnableEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveRestartEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetPrimitiveRestartEnableEXT"));
		vkCmdSetPrimitiveTopologyEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetPrimitiveTopologyEXT"));
		vkCmdSetRasterizationSamplesEXT = reinterpret_cast<PFN_vkCmdSetRasterizationSamplesEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetRasterizationSamplesEXT"));
		vkCmdSetRasterizerDiscardEnableEXT = reinterpret_cast<PFN_vkCmdSetRasterizerDiscardEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetRasterizerDiscardEnableEXT"));
		vkCmdSetSampleMaskEXT = reinterpret_cast<PFN_vkCmdSetSampleMaskEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetSampleMaskEXT"));
		vkCmdSetScissorWithCountEXT = reinterpret_cast<PFN_vkCmdSetScissorWithCountEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetScissorWithCountEXT"));
		vkCmdSetStencilTestEnableEXT = reinterpret_cast<PFN_vkCmdSetStencilTestEnableEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetStencilTestEnableEXT"));
		vkCmdSetVertexInputEXT = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetVertexInputEXT"));
		vkCmdSetViewportWithCountEXT = reinterpret_cast<PFN_vkCmdSetViewportWithCountEXT>(vkGetDeviceProcAddr(m_vkDevice, "vkCmdSetViewportWithCountEXT"));;

		loadAssets();
		prepareUniformBuffers();
		setupDescriptors();
		createShaderObjects();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		updateUniformBuffers();
	}
};

VULKAN_EXAMPLE_MAIN()
