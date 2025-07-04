/*
* Vulkan Example - Deferred shading with shadows from multiple light sources using geometry shader instancing
*
* This sample adds dynamic shadows (using shadow maps) to a deferred rendering setup
* 
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanFrameBuffer.hpp"
#include "VulkanglTFModel.h"

// Must match the LIGHT_COUNT define in the shadow and deferred shaders
#define LIGHT_COUNT 3

class VulkanExample : public VulkanExampleBase
{
public:
	int32_t debugDisplayTarget = 0;
	bool enableShadows = true;

	// Keep depth range as small as possible
	// for better shadow map precision
	float zNear = 0.1f;
	float zFar = 64.0f;
	float lightFOV = 100.0f;

	// Depth bias (and slope) are used to avoid shadowing artifacts
	float depthBiasConstant = 1.25f;
	float depthBiasSlope = 1.75f;

	struct {
		struct {
			vks::Texture2D colorMap;
			vks::Texture2D normalMap;
		} model;
		struct {
			vks::Texture2D colorMap;
			vks::Texture2D normalMap;
		} background;
	} textures;

	struct {
		vkglTF::Model model;
		vkglTF::Model background;
	} models;

	struct UniformDataOffscreen {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
		glm::vec4 instancePos[3];
		int layer{ 0 };
	} uniformDataOffscreen;

	// This UBO stores the shadow matrices for all of the light sources
	// The matrices are indexed using geometry shader instancing
	// The instancePos is used to place the models using instanced draws
	struct UniformDataShadows {
		glm::mat4 mvp[LIGHT_COUNT];
		glm::vec4 instancePos[3];
	} uniformDataShadows;

	struct Light {
		glm::vec4 position;
		glm::vec4 target;
		glm::vec4 color;
		glm::mat4 viewMatrix;
	};

	struct UniformDataComposition {
		glm::vec4 viewPos;
		Light lights[LIGHT_COUNT];
		uint32_t useShadows = 1;
		int32_t debugDisplayTarget = 0;
	} uniformDataComposition;

	struct {
		vks::Buffer offscreen;
		vks::Buffer composition;
		vks::Buffer shadowGeometryShader;
	} uniformBuffers;

	struct {
		VkPipeline deferred{ VK_NULL_HANDLE };
		VkPipeline offscreen{ VK_NULL_HANDLE };
		VkPipeline shadowpass{ VK_NULL_HANDLE };
	} pipelines;
	VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };

	struct {
		VkDescriptorSet model{ VK_NULL_HANDLE };
		VkDescriptorSet background{ VK_NULL_HANDLE };
		VkDescriptorSet shadow{ VK_NULL_HANDLE };
		VkDescriptorSet composition{ VK_NULL_HANDLE };
	} descriptorSets;

	VkDescriptorSetLayout m_vkDescriptorSetLayout{ VK_NULL_HANDLE };

	struct {
		// Framebuffer resources for the deferred pass
		vks::Framebuffer *deferred;
		// Framebuffer resources for the shadow pass
		vks::Framebuffer *shadow;
	} frameBuffers{};

	VkCommandBuffer offScreenCmdBuffer{ VK_NULL_HANDLE };

	// Semaphore used to synchronize between offscreen and final scene rendering
	VkSemaphore offscreenSemaphore{ VK_NULL_HANDLE };

	VulkanExample() : VulkanExampleBase()
	{
		title = "Deferred shading with shadows";
		camera.type = Camera::CameraType::firstperson;
#if defined(__ANDROID__)
		camera.movementSpeed = 2.5f;
#else
		camera.movementSpeed = 5.0f;
		camera.rotationSpeed = 0.25f;
#endif
		camera.position = { 2.15f, 0.3f, -8.75f };
		camera.setRotation(glm::vec3(-0.75f, 12.5f, 0.0f));
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, zNear, zFar);
		timerSpeed *= 0.25f;
	}

	~VulkanExample()
	{
		// Frame buffers
		if (frameBuffers.deferred)
		{
			delete frameBuffers.deferred;
		}
		if (frameBuffers.shadow)
		{
			delete frameBuffers.shadow;
		}

		vkDestroyPipeline(m_vkDevice, pipelines.deferred, nullptr);
		vkDestroyPipeline(m_vkDevice, pipelines.offscreen, nullptr);
		vkDestroyPipeline(m_vkDevice, pipelines.shadowpass, nullptr);

		vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);

		vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);

		// Uniform buffers
		uniformBuffers.composition.destroy();
		uniformBuffers.offscreen.destroy();
		uniformBuffers.shadowGeometryShader.destroy();

		// Textures
		textures.model.colorMap.destroy();
		textures.model.normalMap.destroy();
		textures.background.colorMap.destroy();
		textures.background.normalMap.destroy();

		vkDestroySemaphore(m_vkDevice, offscreenSemaphore, nullptr);
	}

	// Enable physical m_vkDevice m_vkPhysicalDeviceFeatures required for this example
	virtual void getEnabledFeatures()
	{
		// Geometry shader support is required for writing to multiple shadow map layers in one single pass
		if (m_vkPhysicalDeviceFeatures.geometryShader) {
			m_vkPhysicalDeviceFeatures10.geometryShader = VK_TRUE;
		}
		else {
			vks::tools::exitFatal("Selected GPU does not support geometry shaders!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		// Enable anisotropic filtering if supported
		if (m_vkPhysicalDeviceFeatures.samplerAnisotropy) {
			m_vkPhysicalDeviceFeatures10.samplerAnisotropy = VK_TRUE;
		}
		// Enable texture compression
		if (m_vkPhysicalDeviceFeatures.textureCompressionBC) {
			m_vkPhysicalDeviceFeatures10.textureCompressionBC = VK_TRUE;
		}
		else if (m_vkPhysicalDeviceFeatures.textureCompressionASTC_LDR) {
			m_vkPhysicalDeviceFeatures10.textureCompressionASTC_LDR = VK_TRUE;
		}
		else if (m_vkPhysicalDeviceFeatures.textureCompressionETC2) {
			m_vkPhysicalDeviceFeatures10.textureCompressionETC2 = VK_TRUE;
		}
	}

	// Prepare a layered shadow map with each layer containing depth from a light's point of m_vkImageView
	// The shadow mapping pass uses geometry shader instancing to output the scene from the different
	// light sources' point of m_vkImageView to the layers of the depth attachment in one single pass
	void shadowSetup()
	{
		frameBuffers.shadow = new vks::Framebuffer(m_pVulkanDevice);

		// Shadowmap m_vkPhysicalDeviceProperties
#if defined(__ANDROID__)
		// Use smaller shadow maps on mobile due to performance reasons
		m_vkFrameBuffers.shadow->m_drawAreaWidth = 1024;
		m_vkFrameBuffers.shadow->m_drawAreaHeight = 1024;
#else
		frameBuffers.shadow->width = 2048;
		frameBuffers.shadow->height = 2048;
#endif

		// Find a suitable depth format
		VkFormat shadowMapFormat;
		VkBool32 validShadowMapFormat = vks::tools::getSupportedDepthFormat(m_vkPhysicalDevice, &shadowMapFormat);
		assert(validShadowMapFormat);

		// Create a layered depth attachment for rendering the depth maps from the lights' point of m_vkImageView
		// Each layer corresponds to one of the lights
		// The actual output to the separate layers is done in the geometry shader using shader instancing
		// We will pass the matrices of the lights to the GS that selects the layer by the current invocation
		vks::AttachmentCreateInfo attachmentInfo = {};
		attachmentInfo.format = shadowMapFormat;
		attachmentInfo.width = frameBuffers.shadow->width;
		attachmentInfo.height = frameBuffers.shadow->height;
		attachmentInfo.layerCount = LIGHT_COUNT;
		attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		frameBuffers.shadow->addAttachment(attachmentInfo);

		// Create sampler to sample from to depth attachment
		// Used to sample in the fragment shader for shadowed rendering
		VK_CHECK_RESULT(frameBuffers.shadow->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

		// Create default renderpass for the framebuffer
		VK_CHECK_RESULT(frameBuffers.shadow->createRenderPass());
	}

	// Prepare the framebuffer for offscreen rendering with multiple attachments used as render targets inside the fragment shaders
	void deferredSetup()
	{
		frameBuffers.deferred = new vks::Framebuffer(m_pVulkanDevice);

#if defined(__ANDROID__)
		// Use max. screen dimension as deferred framebuffer size
		m_vkFrameBuffers.deferred->m_drawAreaWidth = std::max(m_drawAreaWidth, m_drawAreaHeight);
		m_vkFrameBuffers.deferred->m_drawAreaHeight = std::max(m_drawAreaWidth, m_drawAreaHeight);
#else
		frameBuffers.deferred->width = 2048;
		frameBuffers.deferred->height = 2048;
#endif

		// Four attachments (3 color, 1 depth)
		vks::AttachmentCreateInfo attachmentInfo = {};
		attachmentInfo.width = frameBuffers.deferred->width;
		attachmentInfo.height = frameBuffers.deferred->height;
		attachmentInfo.layerCount = 1;
		attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Color attachments
		// Attachment 0: (World space) Positions
		attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Attachment 1: (World space) Normals
		attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Attachment 2: Albedo (color)
		attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Depth attachment
		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(m_vkPhysicalDevice, &attDepthFormat);
		assert(validDepthFormat);

		attachmentInfo.format = attDepthFormat;
		attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		frameBuffers.deferred->addAttachment(attachmentInfo);

		// Create sampler to sample from the color attachments
		VK_CHECK_RESULT(frameBuffers.deferred->createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

		// Create default renderpass for the framebuffer
		VK_CHECK_RESULT(frameBuffers.deferred->createRenderPass());
	}

	// Put render commands for the scene into the given command buffer
	void renderScene(VkCommandBuffer cmdBuffer, bool shadow)
	{
		// Background
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, shadow ? &descriptorSets.shadow : &descriptorSets.background, 0, NULL);
		models.background.draw(cmdBuffer);

		// Objects
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, shadow ? &descriptorSets.shadow : &descriptorSets.model, 0, NULL);
		models.model.bindBuffers(cmdBuffer);
		vkCmdDrawIndexed(cmdBuffer, models.model.indices.count, 3, 0, 0, 0);
	}

	// Build a secondary command buffer for rendering the scene values to the offscreen frame buffer attachments
	void buildDeferredCommandBuffer()
	{
		if (offScreenCmdBuffer == VK_NULL_HANDLE) {
			offScreenCmdBuffer = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}

		// Create a semaphore used to synchronize offscreen rendering and usage
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &offscreenSemaphore));

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		std::array<VkClearValue, 4> clearValues = {};
		VkViewport viewport;
		VkRect2D scissor;

		// First pass: Shadow map generation
		// -------------------------------------------------------------------------------------------------------

		clearValues[0].depthStencil = { 1.0f, 0 };

		renderPassBeginInfo.renderPass = frameBuffers.shadow->renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers.shadow->framebuffer;
		renderPassBeginInfo.renderArea.extent.width = frameBuffers.shadow->width;
		renderPassBeginInfo.renderArea.extent.height = frameBuffers.shadow->height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues.data();

		VK_CHECK_RESULT(vkBeginCommandBuffer(offScreenCmdBuffer, &cmdBufInfo));

		viewport = vks::initializers::viewport((float)frameBuffers.shadow->width, (float)frameBuffers.shadow->height, 0.0f, 1.0f);
		vkCmdSetViewport(offScreenCmdBuffer, 0, 1, &viewport);

		scissor = vks::initializers::rect2D(frameBuffers.shadow->width, frameBuffers.shadow->height, 0, 0);
		vkCmdSetScissor(offScreenCmdBuffer, 0, 1, &scissor);

		// Set depth bias (aka "Polygon offset")
		vkCmdSetDepthBias(
			offScreenCmdBuffer,
			depthBiasConstant,
			0.0f,
			depthBiasSlope);

		vkCmdBeginRenderPass(offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadowpass);
		renderScene(offScreenCmdBuffer, true);
		vkCmdEndRenderPass(offScreenCmdBuffer);

		// Second pass: Deferred calculations
		// -------------------------------------------------------------------------------------------------------

		// Clear values for all attachments written in the fragment shader
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].depthStencil = { 1.0f, 0 };

		renderPassBeginInfo.renderPass = frameBuffers.deferred->renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers.deferred->framebuffer;
		renderPassBeginInfo.renderArea.extent.width = frameBuffers.deferred->width;
		renderPassBeginInfo.renderArea.extent.height = frameBuffers.deferred->height;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		viewport = vks::initializers::viewport((float)frameBuffers.deferred->width, (float)frameBuffers.deferred->height, 0.0f, 1.0f);
		vkCmdSetViewport(offScreenCmdBuffer, 0, 1, &viewport);

		scissor = vks::initializers::rect2D(frameBuffers.deferred->width, frameBuffers.deferred->height, 0, 0);
		vkCmdSetScissor(offScreenCmdBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);
		renderScene(offScreenCmdBuffer, false);
		vkCmdEndRenderPass(offScreenCmdBuffer);

		VK_CHECK_RESULT(vkEndCommandBuffer(offScreenCmdBuffer));
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.model.loadFromFile(getAssetPath() + "models/armor/armor.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
		models.background.loadFromFile(getAssetPath() + "models/deferred_box.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
		textures.model.colorMap.loadFromFile(getAssetPath() + "models/armor/colormap_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_vkQueue);
		textures.model.normalMap.loadFromFile(getAssetPath() + "models/armor/normalmap_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_vkQueue);
		textures.background.colorMap.loadFromFile(getAssetPath() + "textures/stonefloor02_color_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_vkQueue);
		textures.background.normalMap.loadFromFile(getAssetPath() + "textures/stonefloor02_normal_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_pVulkanDevice, m_vkQueue);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
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
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = VulkanExampleBase::m_vkFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSets.composition, 0, nullptr);

			// Final composition as full screen quad
			// Note: Also used for debug display if debugDisplayTarget > 0
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.deferred);
			vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 12),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo =vks::initializers::descriptorPoolCreateInfo(poolSizes, 4);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

		// Layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// Binding 0: Vertex shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 0),
			// Binding 1: Position texture
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			// Binding 2: Normals texture
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			// Binding 3: Albedo texture
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			// Binding 4: Fragment shader uniform buffer
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
			// Binding 5: Shadow map
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

		// Sets
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);

		// Image descriptors for the offscreen color attachments
		VkDescriptorImageInfo texDescriptorPosition =
			vks::initializers::descriptorImageInfo(
				frameBuffers.deferred->sampler,
				frameBuffers.deferred->attachments[0].view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		VkDescriptorImageInfo texDescriptorNormal =
			vks::initializers::descriptorImageInfo(
				frameBuffers.deferred->sampler,
				frameBuffers.deferred->attachments[1].view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		VkDescriptorImageInfo texDescriptorAlbedo =
			vks::initializers::descriptorImageInfo(
				frameBuffers.deferred->sampler,
				frameBuffers.deferred->attachments[2].view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		VkDescriptorImageInfo texDescriptorShadowMap =
			vks::initializers::descriptorImageInfo(
				frameBuffers.shadow->sampler,
				frameBuffers.shadow->attachments[0].view,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

		// Deferred composition
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.composition));
		writeDescriptorSets = {
			// Binding 1: World space position texture
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texDescriptorPosition),
			// Binding 2: World space normals texture
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &texDescriptorNormal),
			// Binding 3: Albedo texture
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &texDescriptorAlbedo),
			// Binding 4: Fragment shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &uniformBuffers.composition.descriptor),
			// Binding 5: Shadow map
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &texDescriptorShadowMap),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// Offscreen (scene)

		// Model
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.model));
		writeDescriptorSets = {
			// Binding 0: Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSets.model, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.offscreen.descriptor),
			// Binding 1: Color map
			vks::initializers::writeDescriptorSet(descriptorSets.model, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.model.colorMap.descriptor),
			// Binding 2: Normal map
			vks::initializers::writeDescriptorSet(descriptorSets.model, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.model.normalMap.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// Background
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.background));
		writeDescriptorSets = {
			// Binding 0: Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSets.background, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.offscreen.descriptor),
			// Binding 1: Color map
			vks::initializers::writeDescriptorSet(descriptorSets.background, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.background.colorMap.descriptor),
			// Binding 2: Normal map
			vks::initializers::writeDescriptorSet(descriptorSets.background, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.background.normalMap.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// Shadow mapping
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSets.shadow));
		writeDescriptorSets = {
			// Binding 0: Vertex shader uniform buffer
			vks::initializers::writeDescriptorSet(descriptorSets.shadow, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.shadowGeometryShader.descriptor),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(m_vkPipelineLayout, m_vkRenderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

		// Final fullscreen composition pass pipeline
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		shaderStages[0] = loadShader(getShadersPath() + "deferredshadows/deferred.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "deferredshadows/deferred.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Empty vertex input state, vertices are generated by the vertex shader
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCI.pVertexInputState = &emptyInputState;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.deferred));

		// Vertex input state from glTF model for pipeline rendering models
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Tangent });
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

		// Offscreen pipeline
		// Separate render pass
		pipelineCI.renderPass = frameBuffers.deferred->renderPass;

		// Blend attachment states required for all color attachments
		// This is important, as color write mask will otherwise be 0x0 and you
		// won't see anything rendered to the attachment
		std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates =
		{
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
		};
		colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
		colorBlendState.pAttachments = blendAttachmentStates.data();

		shaderStages[0] = loadShader(getShadersPath() + "deferredshadows/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "deferredshadows/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreen));

		// Shadow mapping pipeline
		// The shadow mapping pipeline uses geometry shader instancing (invocations layout modifier) to output
		// shadow maps for multiple lights sources into the different shadow map layers in one single render pass
		std::array<VkPipelineShaderStageCreateInfo, 2> shadowStages;
		shadowStages[0] = loadShader(getShadersPath() + "deferredshadows/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shadowStages[1] = loadShader(getShadersPath() + "deferredshadows/shadow.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

		pipelineCI.pStages = shadowStages.data();
		pipelineCI.stageCount = static_cast<uint32_t>(shadowStages.size());

		// Shadow pass doesn't use any color attachments
		colorBlendState.attachmentCount = 0;
		colorBlendState.pAttachments = nullptr;
		// Cull front faces
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		// Enable depth bias
		rasterizationState.depthBiasEnable = VK_TRUE;
		// Add depth bias to dynamic state, so we can change it at runtime
		dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
		dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		// Reset blend attachment state
		pipelineCI.renderPass = frameBuffers.shadow->renderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &pipelines.shadowpass));
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Offscreen vertex shader
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.offscreen, sizeof(UniformDataOffscreen)));

		// Deferred fragment shader
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.composition, sizeof(UniformDataComposition)));

		// Shadow map vertex shader (matrices from shadow's pov)
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.shadowGeometryShader, sizeof(UniformDataShadows)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.offscreen.map());
		VK_CHECK_RESULT(uniformBuffers.composition.map());
		VK_CHECK_RESULT(uniformBuffers.shadowGeometryShader.map());

		// Setup instanced model positions
		uniformDataOffscreen.instancePos[0] = glm::vec4(0.0f);
		uniformDataOffscreen.instancePos[1] = glm::vec4(-7.0f, 0.0, -4.0f, 0.0f);
		uniformDataOffscreen.instancePos[2] = glm::vec4(4.0f, 0.0, -6.0f, 0.0f);
	}

	void updateUniformBufferOffscreen()
	{
		uniformDataOffscreen.projection = camera.matrices.perspective;
		uniformDataOffscreen.view = camera.matrices.view;
		uniformDataOffscreen.model = glm::mat4(1.0f);
		memcpy(uniformBuffers.offscreen.mapped, &uniformDataOffscreen, sizeof(uniformDataOffscreen));
	}

	Light initLight(glm::vec3 pos, glm::vec3 target, glm::vec3 color)
	{
		Light light;
		light.position = glm::vec4(pos, 1.0f);
		light.target = glm::vec4(target, 0.0f);
		light.color = glm::vec4(color, 0.0f);
		return light;
	}

	void initLights()
	{
		uniformDataComposition.lights[0] = initLight(glm::vec3(-14.0f, -0.5f, 15.0f), glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.5f, 0.5f));
		uniformDataComposition.lights[1] = initLight(glm::vec3(14.0f, -4.0f, 12.0f), glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		uniformDataComposition.lights[2] = initLight(glm::vec3(0.0f, -10.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	}

	// Update deferred composition fragment shader light position and parameters uniform block
	void updateUniformBufferDeferred()
	{
		// Animate
		uniformDataComposition.lights[0].position.x = -14.0f + std::abs(sin(glm::radians(timer * 360.0f)) * 20.0f);
		uniformDataComposition.lights[0].position.z = 15.0f + cos(glm::radians(timer *360.0f)) * 1.0f;

		uniformDataComposition.lights[1].position.x = 14.0f - std::abs(sin(glm::radians(timer * 360.0f)) * 2.5f);
		uniformDataComposition.lights[1].position.z = 13.0f + cos(glm::radians(timer *360.0f)) * 4.0f;

		uniformDataComposition.lights[2].position.x = 0.0f + sin(glm::radians(timer *360.0f)) * 4.0f;
		uniformDataComposition.lights[2].position.z = 4.0f + cos(glm::radians(timer *360.0f)) * 2.0f;

		for (uint32_t i = 0; i < LIGHT_COUNT; i++) {
			// mvp from light's pov (for shadows)
			glm::mat4 shadowProj = glm::perspective(glm::radians(lightFOV), 1.0f, zNear, zFar);
			glm::mat4 shadowView = glm::lookAt(glm::vec3(uniformDataComposition.lights[i].position), glm::vec3(uniformDataComposition.lights[i].target), glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 shadowModel = glm::mat4(1.0f);

			uniformDataShadows.mvp[i] = shadowProj * shadowView * shadowModel;
			uniformDataComposition.lights[i].viewMatrix = uniformDataShadows.mvp[i];
		}

		memcpy(uniformDataShadows.instancePos, uniformDataOffscreen.instancePos, sizeof(UniformDataOffscreen::instancePos));
		memcpy(uniformBuffers.shadowGeometryShader.mapped, &uniformDataShadows, sizeof(UniformDataShadows));

		uniformDataComposition.viewPos = glm::vec4(camera.position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);;
		uniformDataComposition.debugDisplayTarget = debugDisplayTarget;

		memcpy(uniformBuffers.composition.mapped, &uniformDataComposition, sizeof(uniformDataComposition));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		deferredSetup();
		shadowSetup();
		initLights();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		buildDeferredCommandBuffer();
		m_prepared = true;
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		// Offscreen rendering

		// Wait for swap chain presentation to finish
		m_vkSubmitInfo.pWaitSemaphores = &semaphores.m_vkSemaphorePresentComplete;
		// Signal ready with offscreen semaphore
		m_vkSubmitInfo.pSignalSemaphores = &offscreenSemaphore;

		// Submit work

		// Shadow map pass
		m_vkSubmitInfo.commandBufferCount = 1;
		m_vkSubmitInfo.pCommandBuffers = &offScreenCmdBuffer;
		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));

		// Scene rendering

		// Wait for offscreen semaphore
		m_vkSubmitInfo.pWaitSemaphores = &offscreenSemaphore;
		// Signal ready with render complete semaphore
		m_vkSubmitInfo.pSignalSemaphores = &semaphores.m_vkSemaphoreRenderComplete;

		// Submit work
		m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
		VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	virtual void render()
	{
		if (!m_prepared)
			return;
		updateUniformBufferDeferred();
		updateUniformBufferOffscreen();
		draw();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->comboBox("Display", &debugDisplayTarget, { "Final composition", "Shadows", "Position", "Normals", "Albedo", "Specular" });
			bool shadows = (uniformDataComposition.useShadows == 1);
			if (overlay->checkBox("Shadows", &shadows)) {
				uniformDataComposition.useShadows = shadows;
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
