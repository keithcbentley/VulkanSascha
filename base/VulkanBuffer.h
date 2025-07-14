/*
* Vulkan buffer class
*
* Encapsulates a Vulkan buffer
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTools.h"

namespace vks
{	
	/**
	* @brief Encapsulates access to a Vulkan buffer backed up by device memory
	* @note To be filled by an external source like the VulkanDevice
	*/
	struct Buffer
	{
		VkDevice device;
		VkBuffer m_vkBuffer = VK_NULL_HANDLE;
		VkDeviceMemory m_vkMemory = VK_NULL_HANDLE;
		VkDescriptorBufferInfo m_vkDescriptorBufferInfo;
		VkDeviceSize m_size = 0;
		VkDeviceSize m_alignment = 0;
		void* m_pMapped = nullptr;
		/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
		VkBufferUsageFlags m_vkBufferUsageFlags;
		/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
		VkMemoryPropertyFlags m_vkMemoryPropertyFlags;


		VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void unmap();
		VkResult bind(VkDeviceSize offset = 0);
		void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void copyTo(void* data, VkDeviceSize size);
		VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void destroy();
	};
}