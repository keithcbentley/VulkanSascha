/*
* Vulkan Example - Compute shader based ray tracing
* 
* This samples implements a basic ray tracer with materials and reflections using a compute shader
* Shader storage buffers are used to pass geometry information for spheres and planes to the computer shader
* The compute shader then uses these as the scene geometry for ray tracing and outputs the results to a storage m_vkImage
* The graphics part of the sample then displays that m_vkImage full screen
* Not to be confused with actual hardware accelerated ray tracing
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

class VulkanExample : public VulkanExampleBase
{
public:
	// The compute shader will store the ray traced output to a storage m_vkImage
	vks::Texture storageImage{};

	// Resources for the graphics part of the example. The graphics pipeline simply displays the compute shader output
	struct Graphics {
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };	
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	} graphics;

	// Resources for the compute part of the example
	struct Compute {
		// Object m_vkPhysicalDeviceProperties for planes and spheres are passed via a shade storage buffer
		// There is no vertex data, the compute shader calculates the primitives on the fly
		vks::Buffer objectStorageBuffer;
		vks::Buffer uniformBuffer;										// Uniform buffer object containing scene parameters
		VkQueue queue{ VK_NULL_HANDLE };								// Separate m_vkQueue for compute commands (m_vkQueue family may differ from the one used for graphics)
		VkCommandPool commandPool{ VK_NULL_HANDLE };					// Use a separate command pool (m_vkQueue family may differ from the one used for graphics)
		VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };				// Command buffer storing the dispatch commands and barriers
		VkFence fence{ VK_NULL_HANDLE };								// Synchronization fence to avoid rewriting compute CB if still in use
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };	// Compute shader binding layout
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };				// Compute shader bindings
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };				// Layout of the compute pipeline
		VkPipeline pipeline{ VK_NULL_HANDLE };							// Compute raytracing pipeline
		struct UniformDataCompute {										// Compute shader uniform block object
			glm::vec3 lightPos;
			float aspectRatio{ 1.0f };
			glm::vec4 fogColor = glm::vec4(0.0f);
			struct {
				glm::vec3 pos = glm::vec3(0.0f, 0.0f, 4.0f);
				glm::vec3 lookat = glm::vec3(0.0f, 0.5f, 0.0f);
				float fov = 10.0f;
			} camera;
            glm::mat4 _pad;
		} uniformData;
	} compute;

	// Definitions for scene objects
	// The sample uses spheres and planes that are passed to the compute shader via a shader storage buffer
	// The computer shader uses the object type to select different calculations
	enum class SceneObjectType { Sphere = 0, Plane = 1 };
	// Spheres and planes are described by different m_vkPhysicalDeviceProperties, we use a union for this
	union SceneObjectProperty {
		glm::vec4 positionAndRadius;
		glm::vec4 normalAndDistance;
	};
	struct SceneObject {
		SceneObjectProperty objectProperties{};
		glm::vec3 diffuse;
		float specular{ 1.0f };
		uint32_t id{ 0 };
		uint32_t objectType{ 0 };
		// Due to alignment rules we need to pad to make the element align at 16-bytes
		glm::ivec2 _pad;
	};

	VulkanExample() : VulkanExampleBase()
	{
		title = "Compute shader ray tracing";
		timerSpeed *= 0.25f;

		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setTranslation(glm::vec3(0.0f, 0.0f, -4.0f));
		camera.rotationSpeed = 0.0f;
		camera.movementSpeed = 2.5f;
	}

	~VulkanExample()
	{
		if (m_vkDevice) {
			// Graphics
			vkDestroyPipeline(m_vkDevice, graphics.pipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, graphics.pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, graphics.descriptorSetLayout, nullptr);

			// Compute
			vkDestroyPipeline(m_vkDevice, compute.pipeline, nullptr);
			vkDestroyPipelineLayout(m_vkDevice, compute.pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_vkDevice, compute.descriptorSetLayout, nullptr);
			vkDestroyFence(m_vkDevice, compute.fence, nullptr);
			vkDestroyCommandPool(m_vkDevice, compute.commandPool, nullptr);
			compute.uniformBuffer.destroy();
			compute.objectStorageBuffer.destroy();

			storageImage.destroy();
		}
	}

	// Prepare a storage m_vkImage that is used to store the compute shader ray tracing output
	void prepareStorageImage()
	{		
#if defined(__ANDROID__)
		// Use a smaller m_vkImage on Android for performance reasons
		const uint32_t textureSize = 1024;
#else
		const uint32_t textureSize = 2048;
#endif

		const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

		// Get m_vkDevice m_vkPhysicalDeviceProperties for the requested texture format
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(m_vkPhysicalDevice, format, &formatProperties);
		// Check if requested m_vkImage format supports m_vkImage storage operations required for storing pixel from the compute shader
		assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		// Prepare blit target texture
		storageImage.width = textureSize;
		storageImage.height = textureSize;

		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { textureSize, textureSize, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// Image will be sampled in the fragment shader and used as storage target in the compute shader
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.flags = 0;

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &imageCreateInfo, nullptr, &storageImage.image));
		vkGetImageMemoryRequirements(m_vkDevice, storageImage.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &memAllocInfo, nullptr, &storageImage.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(m_vkDevice, storageImage.image, storageImage.deviceMemory, 0));

		VkCommandBuffer layoutCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		storageImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		vks::tools::setImageLayout(layoutCmd, storageImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, storageImage.imageLayout);
		// Add an initial release barrier to the graphics m_vkQueue,
		// so that when the compute command buffer executes for the first time
		// it doesn't complain about a lack of a corresponding "release" to its "acquire"
		if (m_pVulkanDevice->queueFamilyIndices.graphics != m_pVulkanDevice->queueFamilyIndices.compute)
		{
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.image = storageImage.image;
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = 0;
			imageMemoryBarrier.srcQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.graphics;
			imageMemoryBarrier.dstQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
			vkCmdPipelineBarrier(
				layoutCmd,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}
		m_pVulkanDevice->flushCommandBuffer(layoutCmd, m_vkQueue, true);

		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 0.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(m_vkDevice, &sampler, nullptr, &storageImage.sampler));

		// Create m_vkImage m_vkImageView
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = format;
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.image = storageImage.image;
		VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &view, nullptr, &storageImage.view));

		// Initialize a descriptor for later use
		storageImage.descriptor.imageLayout = storageImage.imageLayout;
		storageImage.descriptor.imageView = storageImage.view;
		storageImage.descriptor.sampler = storageImage.sampler;
		storageImage.device = m_pVulkanDevice;
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
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = m_vkFrameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			// Image m_vkDeviceMemory barrier to make sure that compute shader writes are finished before sampling from the texture
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.image = storageImage.image;
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			if (m_pVulkanDevice->queueFamilyIndices.graphics != m_pVulkanDevice->queueFamilyIndices.compute)
			{
				// Acquire barrier for graphics m_vkQueue
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarrier.srcQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
				imageMemoryBarrier.dstQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.graphics;
				vkCmdPipelineBarrier(
					drawCmdBuffers[i],
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_FLAGS_NONE,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}
			else
			{
				// Combined barrier on single m_vkQueue family
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				vkCmdPipelineBarrier(
					drawCmdBuffers[i],
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_FLAGS_NONE,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// Display ray traced m_vkImage generated by compute shader as a full screen quad
			// Quad vertices are generated in the vertex shader
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, &graphics.descriptorSet, 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);
			vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			if (m_pVulkanDevice->queueFamilyIndices.graphics != m_pVulkanDevice->queueFamilyIndices.compute)
			{
				// Release barrier from graphics m_vkQueue
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = 0;
				imageMemoryBarrier.srcQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.graphics;
				imageMemoryBarrier.dstQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
				vkCmdPipelineBarrier(
					drawCmdBuffers[i],
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					VK_FLAGS_NONE,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}
			
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}

	}

	void buildComputeCommandBuffer()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));
		
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.image = storageImage.image;
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		if (m_pVulkanDevice->queueFamilyIndices.graphics != m_pVulkanDevice->queueFamilyIndices.compute)
		{
			// Acquire barrier for compute m_vkQueue
			imageMemoryBarrier.srcAccessMask = 0;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarrier.srcQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.graphics;
			imageMemoryBarrier.dstQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipeline);
		vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0, 0);

		vkCmdDispatch(compute.commandBuffer, storageImage.width / 16, storageImage.height / 16, 1);

		if (m_pVulkanDevice->queueFamilyIndices.graphics != m_pVulkanDevice->queueFamilyIndices.compute)
		{
			// Release barrier from compute m_vkQueue
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = 0;
			imageMemoryBarrier.srcQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
			imageMemoryBarrier.dstQueueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.graphics;
			vkCmdPipelineBarrier(
				compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		vkEndCommandBuffer(compute.commandBuffer);
	}

	// Setup and fill the compute shader storage buffes containing object definitions for the raytraced scene
	void prepareStorageBuffers()
	{
		// Id used to identify objects by the ray tracing shader
		uint32_t currentId = 0;

		std::vector<SceneObject> sceneObjects{};

		// Add some spheres to the scene
		//std::vector<Sphere> spheres;
		// Lambda to simplify object creation
		auto addSphere = [&sceneObjects, &currentId](glm::vec3 pos, float radius, glm::vec3 diffuse, float specular) {
			SceneObject sphere{};
			sphere.id = currentId++;
			sphere.objectProperties.positionAndRadius = glm::vec4(pos, radius);
			sphere.diffuse = diffuse;
			sphere.specular = specular;
			sphere.objectType = (uint32_t)SceneObjectType::Sphere;
			sceneObjects.push_back(sphere);
			};
		
		auto addPlane = [&sceneObjects, &currentId](glm::vec3 normal, float distance, glm::vec3 diffuse, float specular) {
			SceneObject plane{};
			plane.id = currentId++;
			plane.objectProperties.normalAndDistance = glm::vec4(normal, distance);
			plane.diffuse = diffuse;
			plane.specular = specular;
			plane.objectType = (uint32_t)SceneObjectType::Plane;
			sceneObjects.push_back(plane);
			};	
		
		addSphere(glm::vec3(1.75f, -0.5f, 0.0f), 1.0f, glm::vec3(0.0f, 1.0f, 0.0f), 32.0f);
		addSphere(glm::vec3(0.0f, 1.0f, -0.5f), 1.0f, glm::vec3(0.65f, 0.77f, 0.97f), 32.0f);
		addSphere(glm::vec3(-1.75f, -0.75f, -0.5f), 1.25f, glm::vec3(0.9f, 0.76f, 0.46f), 32.0f);

		const float roomDim = 4.0f;
		addPlane(glm::vec3(0.0f, 1.0f, 0.0f), roomDim, glm::vec3(1.0f), 32.0f);
		addPlane(glm::vec3(0.0f, -1.0f, 0.0f), roomDim, glm::vec3(1.0f), 32.0f);
		addPlane(glm::vec3(0.0f, 0.0f, 1.0f), roomDim, glm::vec3(1.0f), 32.0f);
		addPlane(glm::vec3(0.0f, 0.0f, -1.0f), roomDim, glm::vec3(0.0f), 32.0f);
		addPlane(glm::vec3(-1.0f, 0.0f, 0.0f), roomDim, glm::vec3(1.0f, 0.0f, 0.0f), 32.0f);
		addPlane(glm::vec3(1.0f, 0.0f, 0.0f), roomDim, glm::vec3(0.0f, 1.0f, 0.0f), 32.0f);

		VkDeviceSize storageBufferSize = sceneObjects.size() * sizeof(SceneObject);

		// Copy the data to the m_vkDevice
		vks::Buffer stagingBuffer;
		m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, storageBufferSize, sceneObjects.data());
		m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &compute.objectStorageBuffer, storageBufferSize);
		VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = { 0, 0, storageBufferSize};
		vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, compute.objectStorageBuffer.buffer, 1, &copyRegion);
		m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue, true);

		stagingBuffer.destroy();
	}

	// The descriptor pool will be shared between graphics and compute
	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));
	}

	// Prepare the graphics resources used to display the ray traced output of the compute shader
	void prepareGraphics()
	{
		// Setup descriptors

		// The graphics pipeline uses one set and one binding
		// Binding 0: Storage m_vkImage with raytraced output as a sampled m_vkImage for displaying it
		
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &graphics.descriptorSetLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &graphics.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &graphics.descriptorSet));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(graphics.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &storageImage.descriptor)
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// Layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&graphics.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout));

		// Pipeline 
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo,2> shaderStages;

		shaderStages[0] = loadShader(getShadersPath() + "computeraytracing/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "computeraytracing/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VkPipelineVertexInputStateCreateInfo emptyInputState{};
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(graphics.pipelineLayout, m_vkRenderPass, 0);
		pipelineCreateInfo.pVertexInputState = &emptyInputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.renderPass = m_vkRenderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline));
	}

	// Prepare the compute resources that generates the ray traced m_vkImage
	void prepareCompute()
	{
		// Create a compute capable m_vkDevice m_vkQueue
		// The VulkanDevice::createLogicalDevice functions finds a compute capable m_vkQueue and prefers m_vkQueue families that only support compute
		// Depending on the implementation this may result in different m_vkQueue family indices for graphics and computes,
		// requiring proper synchronization (see the m_vkDeviceMemory barriers in buildComputeCommandBuffer)
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.pNext = NULL;
		queueCreateInfo.queueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
		queueCreateInfo.queueCount = 1;
		vkGetDeviceQueue(m_vkDevice, m_pVulkanDevice->queueFamilyIndices.compute, 0, &compute.queue);

		// Setup descriptors

		// The compute pipeline uses one set and four bindings
		// Binding 0: Storage m_vkImage for raytraced output
		// Binding 1: Uniform buffer with parameters
		// Binding 2: Shader storage buffer with scene object definitions

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr,	&compute.descriptorSetLayout));

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &compute.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &compute.descriptorSet));
		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImage.descriptor),
			vks::initializers::writeDescriptorSet(compute.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &compute.uniformBuffer.descriptor),
			vks::initializers::writeDescriptorSet(compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &compute.objectStorageBuffer.descriptor),
		};
		vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);

		// Create the compute shader pipeline
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&compute.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayout));

		VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(compute.pipelineLayout, 0);
		computePipelineCreateInfo.stage = loadShader(getShadersPath() + "computeraytracing/raytracing.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(m_vkDevice, m_vkPipelineCache, 1, &computePipelineCreateInfo, nullptr, &compute.pipeline));

		// Separate command pool as m_vkQueue family for compute may be different from the graphics one
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = m_pVulkanDevice->queueFamilyIndices.compute;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &compute.commandPool));

		// Create a command buffer for compute operations
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(compute.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(m_vkDevice, &cmdBufAllocateInfo, &compute.commandBuffer));

		// Fence for compute CB sync
		VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo();
		VK_CHECK_RESULT(vkCreateFence(m_vkDevice, &fenceCreateInfo, nullptr, &compute.fence));

		// Build a single command buffer containing the compute dispatch commands
		buildComputeCommandBuffer();
	}

	void prepareUniformBuffers()
	{
		// Compute shader parameter uniform buffer block
		m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &compute.uniformBuffer, sizeof(Compute::UniformDataCompute));
	}

	void updateUniformBuffers()
	{
		compute.uniformData.aspectRatio = (float)m_drawAreaWidth / (float)m_drawAreaHeight;
		compute.uniformData.lightPos.x = 0.0f + sin(glm::radians(timer * 360.0f)) * cos(glm::radians(timer * 360.0f)) * 2.0f;
		compute.uniformData.lightPos.y = 0.0f + sin(glm::radians(timer * 360.0f)) * 2.0f;
		compute.uniformData.lightPos.z = 0.0f + cos(glm::radians(timer * 360.0f)) * 2.0f;
		compute.uniformData.camera.pos = camera.position * -1.0f;
		VK_CHECK_RESULT(compute.uniformBuffer.map());
		memcpy(compute.uniformBuffer.mapped, &compute.uniformData, sizeof(Compute::UniformDataCompute));
		compute.uniformBuffer.unmap();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		prepareStorageImage();
		prepareStorageBuffers();
		prepareUniformBuffers();
		setupDescriptorPool();
		prepareGraphics();
		prepareCompute();
		buildCommandBuffers();
		m_prepared = true;
	}

	void draw()
	{
		// Submit compute commands
		// Use a fence to ensure that compute command buffer has finished executing before using it again
		VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;

		VK_CHECK_RESULT(vkQueueSubmit(compute.queue, 1, &computeSubmitInfo, compute.fence));
		
		vkWaitForFences(m_vkDevice, 1, &compute.fence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_vkDevice, 1, &compute.fence);

		VulkanExampleBase::prepareFrame();

		// Command buffer to be submitted to the m_vkQueue
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
