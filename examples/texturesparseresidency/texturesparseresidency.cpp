/*
* Vulkan Example - Sparse texture residency example
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/*
* Important note : This sample is work-in-progress and works basically, but it's not finished
*/

#include "texturesparseresidency.h"

/*
	Virtual texture page
	Contains all functions and objects for a single page of a virtual texture
 */

VirtualTexturePage::VirtualTexturePage()
{
	// Pages are initially not backed up by m_vkDeviceMemory (non-resident)
	imageMemoryBind.memory = VK_NULL_HANDLE;
}

bool VirtualTexturePage::resident()
{
	return (imageMemoryBind.memory != VK_NULL_HANDLE);
}

// Allocate Vulkan m_vkDeviceMemory for the virtual page
bool VirtualTexturePage::allocate(VkDevice device, uint32_t memoryTypeIndex)
{
	if (imageMemoryBind.memory != VK_NULL_HANDLE)
	{
		return false;
	};

	imageMemoryBind = {};

	VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
	allocInfo.allocationSize = size;
	allocInfo.memoryTypeIndex = memoryTypeIndex;
	VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemoryBind.memory));

	VkImageSubresource subResource{};
	subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResource.mipLevel = mipLevel;
	subResource.arrayLayer = layer;

	// Sparse m_vkImage m_vkDeviceMemory binding
	imageMemoryBind.subresource = subResource;
	imageMemoryBind.extent = extent;
	imageMemoryBind.offset = offset;
	return true;
}

// Release Vulkan m_vkDeviceMemory allocated for this page
bool VirtualTexturePage::release(VkDevice device)
{
	del= false;
	if (imageMemoryBind.memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, imageMemoryBind.memory, nullptr);
		imageMemoryBind.memory = VK_NULL_HANDLE;
		return true;
	}
	return false;
}

/*
	Virtual texture
	Contains the virtual pages and m_vkDeviceMemory binding information for a whole virtual texture
 */

VirtualTexturePage* VirtualTexture::addPage(VkOffset3D offset, VkExtent3D extent, const VkDeviceSize size, const uint32_t mipLevel, uint32_t layer)
{
	VirtualTexturePage newPage{};
	newPage.offset = offset;
	newPage.extent = extent;
	newPage.size = size;
	newPage.mipLevel = mipLevel;
	newPage.layer = layer;
	newPage.index = static_cast<uint32_t>(pages.size());
	newPage.imageMemoryBind = {};
	newPage.imageMemoryBind.offset = offset;
	newPage.imageMemoryBind.extent = extent;
	newPage.del = false;
	pages.push_back(newPage);
	return &pages.back();
}

// Call before sparse binding to update m_vkDeviceMemory bind list etc.
void VirtualTexture::updateSparseBindInfo(std::vector<VirtualTexturePage> &bindingChangedPages, bool del)
{
	// Update list of m_vkDeviceMemory-backed sparse m_vkImage m_vkDeviceMemory binds
	//sparseImageMemoryBinds.resize(pages.size());
	sparseImageMemoryBinds.clear();
	for (auto page : bindingChangedPages)
	{
		sparseImageMemoryBinds.push_back(page.imageMemoryBind);
		if (del)
		{
			sparseImageMemoryBinds[sparseImageMemoryBinds.size() - 1].memory = VK_NULL_HANDLE;
		}
	}
	// Update sparse bind info
	bindSparseInfo = vks::initializers::bindSparseInfo();
	// todo: Semaphore for m_vkQueue submission
	// bindSparseInfo.signalSemaphoreCount = 1;
	// bindSparseInfo.pSignalSemaphores = &bindSparseSemaphore;

	// Image m_vkDeviceMemory binds
	imageMemoryBindInfo = {};
	imageMemoryBindInfo.image = image;
	imageMemoryBindInfo.bindCount = static_cast<uint32_t>(sparseImageMemoryBinds.size());
	imageMemoryBindInfo.pBinds = sparseImageMemoryBinds.data();
	bindSparseInfo.imageBindCount = (imageMemoryBindInfo.bindCount > 0) ? 1 : 0;
	bindSparseInfo.pImageBinds = &imageMemoryBindInfo;

	// Opaque m_vkImage m_vkDeviceMemory binds for the mip tail
	opaqueMemoryBindInfo.image = image;
	opaqueMemoryBindInfo.bindCount = static_cast<uint32_t>(opaqueMemoryBinds.size());
	opaqueMemoryBindInfo.pBinds = opaqueMemoryBinds.data();
	bindSparseInfo.imageOpaqueBindCount = (opaqueMemoryBindInfo.bindCount > 0) ? 1 : 0;
	bindSparseInfo.pImageOpaqueBinds = &opaqueMemoryBindInfo;
}

// Release all Vulkan resources
void VirtualTexture::destroy()
{
	for (auto page : pages)
	{
		page.release(device);
	}
	for (auto bind : opaqueMemoryBinds)
	{
		vkFreeMemory(device, bind.memory, nullptr);
	}
	// Clean up mip tail
	if (mipTailimageMemoryBind.memory != VK_NULL_HANDLE) {
		vkFreeMemory(device, mipTailimageMemoryBind.memory, nullptr);
	}
}

/*
	Vulkan Example class
*/
VulkanExample::VulkanExample() : VulkanExampleBase()
{
	title = "Sparse texture residency";
	std::cout.imbue(std::locale(""));
	camera.type = Camera::CameraType::lookat;
	camera.setPosition(glm::vec3(0.0f, 0.0f, -12.0f));
	camera.setRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
	camera.setPerspective(60.0f, (float)m_drawAreaWidth / (float)m_drawAreaHeight, 0.1f, 256.0f);
}

VulkanExample::~VulkanExample()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class
	destroyTextureImage(texture);
	vkDestroySemaphore(m_vkDevice, bindSparseSemaphore, nullptr);
	vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
	vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDescriptorSetLayout, nullptr);
	uniformBuffer.destroy();
}

void VulkanExample::getEnabledFeatures()
{
	if (m_vkPhysicalDeviceFeatures.sparseBinding && m_vkPhysicalDeviceFeatures.sparseResidencyImage2D) {
		m_vkPhysicalDeviceFeatures10.shaderResourceResidency = VK_TRUE;
		m_vkPhysicalDeviceFeatures10.sparseBinding = VK_TRUE;
		m_vkPhysicalDeviceFeatures10.sparseResidencyImage2D = VK_TRUE;
	}
	else {
		std::cout << "Sparse binding not supported" << std::endl;
	}
}

glm::uvec3 VulkanExample::alignedDivision(const VkExtent3D& extent, const VkExtent3D& granularity)
{
	glm::uvec3 res;
	res.x = extent.width / granularity.width + ((extent.width % granularity.width) ? 1u : 0u);
	res.y = extent.height / granularity.height + ((extent.height % granularity.height) ? 1u : 0u);
	res.z = extent.depth / granularity.depth + ((extent.depth % granularity.depth) ? 1u : 0u);
	return res;
}

void VulkanExample::prepareSparseTexture(uint32_t width, uint32_t height, uint32_t layerCount, VkFormat format)
{
	texture.device = m_pVulkanDevice->m_vkDevice;
	texture.width = width;
	texture.height = height;
	texture.mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1);
	texture.layerCount = layerCount;
	texture.format = format;

	texture.subRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mipLevels, 0, 1 };
	// Get m_vkDevice m_vkPhysicalDeviceProperties for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_vkPhysicalDevice, format, &formatProperties);

	const VkImageType imageType = VK_IMAGE_TYPE_2D;
	const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
	const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	const VkImageTiling imageTiling = VK_IMAGE_TILING_OPTIMAL;

	// Get sparse m_vkImage m_vkPhysicalDeviceProperties
	std::vector<VkSparseImageFormatProperties> sparseProperties;
	// Sparse m_vkPhysicalDeviceProperties count for the desired format
	uint32_t sparsePropertiesCount;
	vkGetPhysicalDeviceSparseImageFormatProperties(m_vkPhysicalDevice, format, imageType, sampleCount, imageUsage, imageTiling, &sparsePropertiesCount, nullptr);
	// Check if sparse is supported for this format
	if (sparsePropertiesCount == 0)
	{
		std::cout << "Error: Requested format does not support sparse m_vkPhysicalDeviceFeatures!" << std::endl;
		return;
	}

	// Get actual m_vkImage format m_vkPhysicalDeviceProperties
	sparseProperties.resize(sparsePropertiesCount);
	vkGetPhysicalDeviceSparseImageFormatProperties(m_vkPhysicalDevice, format, imageType, sampleCount, imageUsage, imageTiling, &sparsePropertiesCount, sparseProperties.data());

	std::cout << "Sparse m_vkImage format m_vkPhysicalDeviceProperties: " << sparsePropertiesCount << std::endl;
	for (auto props : sparseProperties)
	{
		std::cout << "\t Image granularity: w = " << props.imageGranularity.width << " h = " << props.imageGranularity.height << " d = " << props.imageGranularity.depth << std::endl;
		std::cout << "\t Aspect mask: " << props.aspectMask << std::endl;
		std::cout << "\t Flags: " << props.flags << std::endl;
	}

	// Create sparse m_vkImage
	VkImageCreateInfo sparseImageCreateInfo = vks::initializers::imageCreateInfo();
	sparseImageCreateInfo.imageType = imageType;
	sparseImageCreateInfo.format = texture.format;
	sparseImageCreateInfo.mipLevels = texture.mipLevels;
	sparseImageCreateInfo.arrayLayers = texture.layerCount;
	sparseImageCreateInfo.samples = sampleCount;
	sparseImageCreateInfo.tiling = imageTiling;
	sparseImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sparseImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	sparseImageCreateInfo.extent = { texture.width, texture.height, 1 };
	sparseImageCreateInfo.usage = imageUsage;
	sparseImageCreateInfo.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	VK_CHECK_RESULT(vkCreateImage(m_vkDevice, &sparseImageCreateInfo, nullptr, &texture.image));

	VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vks::tools::setImageLayout(copyCmd, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.subRange);
	m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue);

	// Get m_vkDeviceMemory requirements
	VkMemoryRequirements sparseImageMemoryReqs;
	// Sparse m_vkImage m_vkDeviceMemory requirement counts
	vkGetImageMemoryRequirements(m_vkDevice, texture.image, &sparseImageMemoryReqs);

	std::cout << "Image m_vkDeviceMemory requirements:" << std::endl;
	std::cout << "\t Size: " << sparseImageMemoryReqs.size << std::endl;
	std::cout << "\t Alignment: " << sparseImageMemoryReqs.alignment << std::endl;

	// Check requested m_vkImage size against hardware sparse limit
	if (sparseImageMemoryReqs.size > m_pVulkanDevice->m_vkPhysicalDeviceProperties.limits.sparseAddressSpaceSize)
	{
		std::cout << "Error: Requested sparse m_vkImage size exceeds supports sparse address space size!" << std::endl;
		return;
	};

	// Get sparse m_vkDeviceMemory requirements
	// Count
	uint32_t sparseMemoryReqsCount = 32;
	std::vector<VkSparseImageMemoryRequirements> sparseMemoryReqs(sparseMemoryReqsCount);
	vkGetImageSparseMemoryRequirements(m_vkDevice, texture.image, &sparseMemoryReqsCount, sparseMemoryReqs.data());
	if (sparseMemoryReqsCount == 0)
	{
		std::cout << "Error: No m_vkDeviceMemory requirements for the sparse m_vkImage!" << std::endl;
		return;
	}
	sparseMemoryReqs.resize(sparseMemoryReqsCount);
	// Get actual requirements
	vkGetImageSparseMemoryRequirements(m_vkDevice, texture.image, &sparseMemoryReqsCount, sparseMemoryReqs.data());

	std::cout << "Sparse m_vkImage m_vkDeviceMemory requirements: " << sparseMemoryReqsCount << std::endl;
	for (auto reqs : sparseMemoryReqs)
	{
		std::cout << "\t Image granularity: w = " << reqs.formatProperties.imageGranularity.width << " h = " << reqs.formatProperties.imageGranularity.height << " d = " << reqs.formatProperties.imageGranularity.depth << std::endl;
		std::cout << "\t Mip tail first LOD: " << reqs.imageMipTailFirstLod << std::endl;
		std::cout << "\t Mip tail size: " << reqs.imageMipTailSize << std::endl;
		std::cout << "\t Mip tail offset: " << reqs.imageMipTailOffset << std::endl;
		std::cout << "\t Mip tail stride: " << reqs.imageMipTailStride << std::endl;
		//todo:multiple reqs
		texture.mipTailStart = reqs.imageMipTailFirstLod;
	}

	// Get sparse m_vkImage requirements for the color aspect
	VkSparseImageMemoryRequirements sparseMemoryReq;
	bool colorAspectFound = false;
	for (auto reqs : sparseMemoryReqs)
	{
		if (reqs.formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
		{
			sparseMemoryReq = reqs;
			colorAspectFound = true;
			break;
		}
	}
	if (!colorAspectFound)
	{
		std::cout << "Error: Could not find sparse m_vkImage m_vkDeviceMemory requirements for color aspect bit!" << std::endl;
		return;
	}

	// @todo: proper comment
	// Calculate number of required sparse m_vkDeviceMemory bindings by alignment
	assert((sparseImageMemoryReqs.size % sparseImageMemoryReqs.alignment) == 0);
	texture.memoryTypeIndex = m_pVulkanDevice->getMemoryType(sparseImageMemoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	texture.sparseImageMemoryRequirements = sparseMemoryReq;

	// The mip tail contains all mip levels > sparseMemoryReq.imageMipTailFirstLod
	// Check if the format has a single mip tail for all layers or one mip tail for each layer
	// @todo: Comment
	texture.mipTailInfo.singleMipTail = sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;
	texture.mipTailInfo.alingedMipSize = sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_ALIGNED_MIP_SIZE_BIT;

	// Sparse bindings for each mip level of all layers outside of the mip tail
	for (uint32_t layer = 0; layer < texture.layerCount; layer++)
	{
		// sparseMemoryReq.imageMipTailFirstLod is the first mip level that's stored inside the mip tail
		for (uint32_t mipLevel = 0; mipLevel < sparseMemoryReq.imageMipTailFirstLod; mipLevel++)
		{
			VkExtent3D extent;
			extent.width = std::max(sparseImageCreateInfo.extent.width >> mipLevel, 1u);
			extent.height = std::max(sparseImageCreateInfo.extent.height >> mipLevel, 1u);
			extent.depth = std::max(sparseImageCreateInfo.extent.depth >> mipLevel, 1u);

			VkImageSubresource subResource{};
			subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subResource.mipLevel = mipLevel;
			subResource.arrayLayer = layer;

			// Aligned sizes by m_vkImage granularity
			VkExtent3D imageGranularity = sparseMemoryReq.formatProperties.imageGranularity;
			glm::uvec3 sparseBindCounts = alignedDivision(extent, imageGranularity);
			glm::uvec3 lastBlockExtent;
			lastBlockExtent.x = (extent.width % imageGranularity.width) ? extent.width % imageGranularity.width : imageGranularity.width;
			lastBlockExtent.y = (extent.height % imageGranularity.height) ? extent.height % imageGranularity.height : imageGranularity.height;
			lastBlockExtent.z = (extent.depth % imageGranularity.depth) ? extent.depth % imageGranularity.depth : imageGranularity.depth;

			// @todo: Comment
			uint32_t index = 0;
			for (uint32_t z = 0; z < sparseBindCounts.z; z++)
			{
				for (uint32_t y = 0; y < sparseBindCounts.y; y++)
				{
					for (uint32_t x = 0; x < sparseBindCounts.x; x++)
					{
						// Offset
						VkOffset3D offset;
						offset.x = x * imageGranularity.width;
						offset.y = y * imageGranularity.height;
						offset.z = z * imageGranularity.depth;
						// Size of the page
						VkExtent3D extent;
						extent.width = (x == sparseBindCounts.x - 1) ? lastBlockExtent.x : imageGranularity.width;
						extent.height = (y == sparseBindCounts.y - 1) ? lastBlockExtent.y : imageGranularity.height;
						extent.depth = (z == sparseBindCounts.z - 1) ? lastBlockExtent.z : imageGranularity.depth;

						// Add new virtual page
						VirtualTexturePage* newPage = texture.addPage(offset, extent, sparseImageMemoryReqs.alignment, mipLevel, layer);
						newPage->imageMemoryBind.subresource = subResource;

						index++;
					}
				}
			}
		}

		// @todo: proper comment
		// @todo: store in mip tail and properly release
		// @todo: Only one block for single mip tail
		if ((!texture.mipTailInfo.singleMipTail) && (sparseMemoryReq.imageMipTailFirstLod < texture.mipLevels))
		{
			// Allocate m_vkDeviceMemory for the mip tail
			VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
			allocInfo.allocationSize = sparseMemoryReq.imageMipTailSize;
			allocInfo.memoryTypeIndex = texture.memoryTypeIndex;

			VkDeviceMemory deviceMemory;
			VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &deviceMemory));

			// (Opaque) sparse m_vkDeviceMemory binding
			VkSparseMemoryBind sparseMemoryBind{};
			sparseMemoryBind.resourceOffset = sparseMemoryReq.imageMipTailOffset + layer * sparseMemoryReq.imageMipTailStride;
			sparseMemoryBind.size = sparseMemoryReq.imageMipTailSize;
			sparseMemoryBind.memory = deviceMemory;

			texture.opaqueMemoryBinds.push_back(sparseMemoryBind);
		}
	} // end layers and mips

	std::cout << "Texture info:" << std::endl;
	std::cout << "\tDim: " << texture.width << " x " << texture.height << std::endl;
	std::cout << "\tVirtual pages: " << texture.pages.size() << std::endl;

	// Check if format has one mip tail for all layers
	if ((sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) && (sparseMemoryReq.imageMipTailFirstLod < texture.mipLevels))
	{
		// Allocate m_vkDeviceMemory for the mip tail
		VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
		allocInfo.allocationSize = sparseMemoryReq.imageMipTailSize;
		allocInfo.memoryTypeIndex = texture.memoryTypeIndex;

		VkDeviceMemory deviceMemory;
		VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &deviceMemory));

		// (Opaque) sparse m_vkDeviceMemory binding
		VkSparseMemoryBind sparseMemoryBind{};
		sparseMemoryBind.resourceOffset = sparseMemoryReq.imageMipTailOffset;
		sparseMemoryBind.size = sparseMemoryReq.imageMipTailSize;
		sparseMemoryBind.memory = deviceMemory;

		texture.opaqueMemoryBinds.push_back(sparseMemoryBind);
	}

	// Create signal semaphore for sparse binding
	VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
	VK_CHECK_RESULT(vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &bindSparseSemaphore));

	// Prepare bind sparse info for reuse in m_vkQueue submission
	texture.updateSparseBindInfo(texture.pages);

	// Bind to m_vkQueue
	// todo: in draw?
	vkQueueBindSparse(m_vkQueue, 1, &texture.bindSparseInfo, VK_NULL_HANDLE);
	//todo: use sparse bind semaphore
	vkQueueWaitIdle(m_vkQueue);

	// Create sampler
	VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.mipLodBias = 0.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = static_cast<float>(texture.mipLevels);
	sampler.maxAnisotropy = m_pVulkanDevice->m_vkPhysicalDeviceFeatures.samplerAnisotropy ? m_pVulkanDevice->m_vkPhysicalDeviceProperties.limits.maxSamplerAnisotropy : 1.0f;
	sampler.anisotropyEnable = false;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	VK_CHECK_RESULT(vkCreateSampler(m_vkDevice, &sampler, nullptr, &texture.sampler));

	// Create m_vkImage m_vkImageView
	VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
	view.image = VK_NULL_HANDLE;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	view.subresourceRange.levelCount = texture.mipLevels;
	view.image = texture.image;
	VK_CHECK_RESULT(vkCreateImageView(m_vkDevice, &view, nullptr, &texture.view));

	// Fill m_vkImage descriptor m_vkImage info that can be used during the descriptor set setup
	texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	texture.descriptor.imageView = texture.view;
	texture.descriptor.sampler = texture.sampler;
}

// Free all Vulkan resources used a texture object
void VulkanExample::destroyTextureImage(SparseTexture texture)
{
	vkDestroyImageView(m_vkDevice, texture.view, nullptr);
	vkDestroyImage(m_vkDevice, texture.image, nullptr);
	vkDestroySampler(m_vkDevice, texture.sampler, nullptr);
	texture.destroy();
}

void VulkanExample::buildCommandBuffers()
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

		VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipelineLayout, 0, 1, &descriptorSet, 0, NULL);
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);
		plane.draw(drawCmdBuffers[i]);

		drawUI(drawCmdBuffers[i]);

		vkCmdEndRenderPass(drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanExample::loadAssets()
{
	const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
	plane.loadFromFile(getAssetPath() + "models/plane.gltf", m_pVulkanDevice, m_vkQueue, glTFLoadingFlags);
}

void VulkanExample::setupDescriptors()
{
	// Pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
	VK_CHECK_RESULT(vkCreateDescriptorPool(m_vkDevice, &descriptorPoolInfo, nullptr, &m_vkDescriptorPool));

	// Layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0 : Vertex shader uniform buffer
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			0),
		// Binding 1 : Fragment shader m_vkImage sampler
		vks::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1)
	};
	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_vkDevice, &descriptorLayout, nullptr, &m_vkDescriptorSetLayout));

	// Sets
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(m_vkDescriptorPool, &m_vkDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &descriptorSet));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0 : Vertex shader uniform buffer
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
		// Binding 1 : Fragment shader texture sampler
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &texture.descriptor)
	};
	vkUpdateDescriptorSets(m_vkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void VulkanExample::preparePipelines()
{
	// Layout
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&m_vkDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &m_vkPipelineLayout));

	// Pipeline
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

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo( m_vkPipelineLayout, m_vkRenderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

	shaderStages[0] = loadShader(getShadersPath() + "texturesparseresidency/sparseresidency.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getShadersPath() + "texturesparseresidency/sparseresidency.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_vkDevice, m_vkPipelineCache, 1, &pipelineCI, nullptr, &m_vkPipeline));
}

// Prepare and initialize uniform buffer containing shader uniforms
void VulkanExample::prepareUniformBuffers()
{
	// Vertex shader uniform buffer block
	VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(UniformData), &uniformData));
	updateUniformBuffers();
}

void VulkanExample::updateUniformBuffers()
{
	uniformData.projection = camera.matrices.perspective;
	uniformData.model = camera.matrices.view;
	uniformData.viewPos = camera.viewPos;

	VK_CHECK_RESULT(uniformBuffer.map());
	memcpy(uniformBuffer.mapped, &uniformData, sizeof(UniformData));
	uniformBuffer.unmap();
}

void VulkanExample::prepare()
{
	VulkanExampleBase::prepare();
	// Check if the GPU supports sparse residency for 2D images
	if (!m_pVulkanDevice->m_vkPhysicalDeviceFeatures.sparseResidencyImage2D) {
		vks::tools::exitFatal("Device does not support sparse residency for 2D images!", VK_ERROR_FEATURE_NOT_PRESENT);
	}
	loadAssets();
	prepareUniformBuffers();
	// Create a virtual texture with max. possible dimension (does not take up any VRAM yet)
	prepareSparseTexture(4096, 4096, 1, VK_FORMAT_R8G8B8A8_UNORM);
	setupDescriptors();
	preparePipelines();
	buildCommandBuffers();
	m_prepared = true;
}

void VulkanExample::draw()
{
	VulkanExampleBase::prepareFrame();
	m_vkSubmitInfo.commandBufferCount = 1;
	m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
	VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
	VulkanExampleBase::submitFrame();
}

void VulkanExample::render()
{
	if (!m_prepared)
		return;
	updateUniformBuffers();
	draw();
}

// Fills a buffer with random colors
void VulkanExample::randomPattern(uint8_t* buffer, uint32_t width, uint32_t height)
{
	std::random_device rd;
	std::mt19937 rndEngine(rd());
	std::uniform_int_distribution<uint32_t> rndDist(0, 255);
	uint8_t rndVal[4] = { 0, 0, 0, 0 };
	while (rndVal[0] + rndVal[1] + rndVal[2] < 10) {
		rndVal[0] = (uint8_t)rndDist(rndEngine);
		rndVal[1] = (uint8_t)rndDist(rndEngine);
		rndVal[2] = (uint8_t)rndDist(rndEngine);
	}
	rndVal[3] = 255;
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			for (uint32_t c = 0; c < 4; c++, ++buffer) {
				*buffer = rndVal[c];
			}
		}
	}
}

void VulkanExample::uploadContent(VirtualTexturePage page, VkImage image)
{
	// Generate some random m_vkImage data and upload as a buffer
	const size_t bufferSize = 4 * page.extent.width * page.extent.height;

	vks::Buffer imageBuffer;
	VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&imageBuffer,
		bufferSize));
	imageBuffer.map();

	uint8_t* data = (uint8_t*)imageBuffer.mapped;
	randomPattern(data, page.extent.height, page.extent.width);

	VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vks::tools::setImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.subRange, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = page.mipLevel;
	region.imageOffset = page.offset;
	region.imageExtent = page.extent;
	vkCmdCopyBufferToImage(copyCmd, imageBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	vks::tools::setImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.subRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue);

	imageBuffer.destroy();
}

void VulkanExample::fillRandomPages()
{
	vkDeviceWaitIdle(m_vkDevice);

	std::default_random_engine rndEngine(m_benchmark.active ? 0 : std::random_device{}());
	std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

	std::vector<VirtualTexturePage> updatedPages;
	std::vector<VirtualTexturePage> bindingChangedPages;
	for (auto& page : texture.pages) {
		if (rndDist(rndEngine) < 0.5f) {
			continue;
		}
		if (page.allocate(m_vkDevice, texture.memoryTypeIndex))
		{
			bindingChangedPages.push_back(page);
		}
		updatedPages.push_back(page);
	}

	// Update sparse m_vkQueue binding
	texture.updateSparseBindInfo(bindingChangedPages);
	VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &fence));
	vkQueueBindSparse(m_vkQueue, 1, &texture.bindSparseInfo, fence);
	vkWaitForFences(m_vkDevice, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(m_vkDevice, fence, nullptr);

	for (auto &page: updatedPages) {
		uploadContent(page, texture.image);
	}
}

void VulkanExample::fillMipTail()
{
	// Clean up previous mip tail m_vkDeviceMemory allocation
	if (texture.mipTailimageMemoryBind.memory != VK_NULL_HANDLE) {
		vkFreeMemory(m_vkDevice, texture.mipTailimageMemoryBind.memory, nullptr);
	}

	//@todo: WIP
	VkDeviceSize imageMipTailSize = texture.sparseImageMemoryRequirements.imageMipTailSize;
	VkDeviceSize imageMipTailOffset = texture.sparseImageMemoryRequirements.imageMipTailOffset;
	// Stride between m_vkDeviceMemory bindings for each mip level if not single mip tail (VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT not set)
	VkDeviceSize imageMipTailStride = texture.sparseImageMemoryRequirements.imageMipTailStride;

	VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
	allocInfo.allocationSize = imageMipTailSize;
	allocInfo.memoryTypeIndex = texture.memoryTypeIndex;
	VK_CHECK_RESULT(vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &texture.mipTailimageMemoryBind.memory));

	uint32_t mipLevel = texture.sparseImageMemoryRequirements.imageMipTailFirstLod;
	uint32_t width = std::max(texture.width >> texture.sparseImageMemoryRequirements.imageMipTailFirstLod, 1u);
	uint32_t height = std::max(texture.height >> texture.sparseImageMemoryRequirements.imageMipTailFirstLod, 1u);
	uint32_t depth = 1;

	for (uint32_t i = texture.mipTailStart; i < texture.mipLevels; i++) {

		const uint32_t width = std::max(texture.width >> i, 1u);
		const uint32_t height = std::max(texture.height >> i, 1u);

		// Generate some random m_vkImage data and upload as a buffer
		const size_t bufferSize = 4 * width * height;

		vks::Buffer imageBuffer;
		VK_CHECK_RESULT(m_pVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&imageBuffer,
			bufferSize));
		imageBuffer.map();

		// Fill buffer with random colors
		std::random_device rd;
		std::mt19937 rndEngine(rd());
		std::uniform_int_distribution<uint32_t> rndDist(0, 255);
		uint8_t* data = (uint8_t*)imageBuffer.mapped;
		randomPattern(data, width, height);

		VkCommandBuffer copyCmd = m_pVulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vks::tools::setImageLayout(copyCmd, texture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.subRange, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = i;
		region.imageOffset = {};
		region.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(copyCmd, imageBuffer.buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		vks::tools::setImageLayout(copyCmd, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.subRange, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		m_pVulkanDevice->flushCommandBuffer(copyCmd, m_vkQueue);

		imageBuffer.destroy();
	}
}

void VulkanExample::flushRandomPages()
{
	vkDeviceWaitIdle(m_vkDevice);

	std::default_random_engine rndEngine(m_benchmark.active ? 0 : std::random_device{}());
	std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

	std::vector<VirtualTexturePage> updatedPages;
	std::vector<VirtualTexturePage> bindingChangedPages;
	for (auto& page : texture.pages)
	{
		if (rndDist(rndEngine) < 0.5f) {
			continue;
		}
		if (page.imageMemoryBind.memory != VK_NULL_HANDLE){
			page.del = true;
			bindingChangedPages.push_back(page);
		}
	}

	// Update sparse m_vkQueue binding
	texture.updateSparseBindInfo(bindingChangedPages, true);
	VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &fence));
	vkQueueBindSparse(m_vkQueue, 1, &texture.bindSparseInfo, fence);
	vkWaitForFences(m_vkDevice, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(m_vkDevice, fence, nullptr);
	for (auto& page : texture.pages)
	{
		if (page.del)
		{
			page.release(m_vkDevice);
		}
	}
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
	if (overlay->header("Settings")) {
		if (overlay->sliderFloat("LOD bias", &uniformData.lodBias, -(float)texture.mipLevels, (float)texture.mipLevels)) {
			updateUniformBuffers();
		}
		if (overlay->button("Fill random pages")) {
			fillRandomPages();
		}
		if (overlay->button("Flush random pages")) {
			flushRandomPages();
		}
		if (overlay->button("Fill mip tail")) {
			fillMipTail();
		}
	}
	if (overlay->header("Statistics")) {
		uint32_t respages = 0;
		std::for_each(texture.pages.begin(), texture.pages.end(), [&respages](VirtualTexturePage page) { respages += (page.resident()) ? 1 : 0; });
		overlay->text("Resident pages: %d of %d", respages, static_cast<uint32_t>(texture.pages.size()));
		overlay->text("Mip tail starts at: %d", texture.mipTailStart);
	}

}

VULKAN_EXAMPLE_MAIN()
