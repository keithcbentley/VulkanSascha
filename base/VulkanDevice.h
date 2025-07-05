/*
 * Vulkan m_vkDevice class
 *
 * Encapsulates a physical Vulkan m_vkDevice and its logical representation
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <VulkanCpp.hpp>
#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "vulkan/vulkan.h"
#include <algorithm>
#include <assert.h>
#include <exception>

namespace vks {
struct VulkanDevice {
    /** @brief Physical device representation */
    vkcpp::PhysicalDevice m_physicalDevice;

    /** @brief Logical device representation (application's view of the device) */
    vkcpp::Device m_device;

    /** @brief Properties of the physical device including limits that the application can check against */
    VkPhysicalDeviceProperties m_vkPhysicalDeviceProperties;
    /** @brief Features of the physical device that an application can use to check if a feature is supported */
    VkPhysicalDeviceFeatures m_vkPhysicalDeviceFeatures;
    /** @brief Features that have been enabled for use on the physical device */
    VkPhysicalDeviceFeatures m_vkPhysicalDeviceFeaturesEnabled;
    /** @brief Memory types and heaps of the physical device */
    VkPhysicalDeviceMemoryProperties m_vkPhysicalDeviceMemoryProperties;
    /** @brief Queue family properties of the physical device */
    std::vector<VkQueueFamilyProperties> m_vkQueueFamilyProperties;
    /** @brief List of extensions supported by the device */
    std::vector<std::string> m_supportedExtensions;
    /** @brief Default command pool for the graphics queue family index */
    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
    /** @brief Contains queue family indices */
    struct
    {
        uint32_t graphics = 0;
        uint32_t compute = 0;
        uint32_t transfer = 0;
    } queueFamilyIndices;

    operator VkDevice() const
    {
        return m_device;
    }

    explicit VulkanDevice(
		const vkcpp::PhysicalDevice& physicalDevice,
		const vkcpp::Device& device
		);

    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
    uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags) const;
    VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
    VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size, void* data = nullptr);
    void copyBuffer(vks::Buffer* src, vks::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);
    VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
    void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
    void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
    bool extensionSupported(std::string extension);
    VkFormat getSupportedDepthFormat(bool checkSamplingSupport);
};
}
