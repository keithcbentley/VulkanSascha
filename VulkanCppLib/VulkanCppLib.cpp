// VulkanCppLib.cpp : Defines the functions for the static library.
//

#include "VulkanCpp.hpp"

namespace vkcpp {

VersionNumber VersionNumber::getVersionNumber()
{
    uint32_t vkVersionNumber;
    vkEnumerateInstanceVersion(&vkVersionNumber);
    return { vkVersionNumber };
}


DeviceFeatures PhysicalDevice::getPhysicalDeviceFeatures2() const
{
    DeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures2(m_vkPhysicalDevice, deviceFeatures);
    return deviceFeatures;
}

DeviceProperties PhysicalDevice::getPhysicalDeviceProperties2() const
{
    DeviceProperties deviceProperties {};
    vkGetPhysicalDeviceProperties2(m_vkPhysicalDevice, deviceProperties);
    return deviceProperties;
}

std::vector<VkExtensionProperties> PhysicalDevice::EnumerateDeviceExtensionProperties() const
{
    uint32_t propertyCount;
    vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &propertyCount, nullptr);
    std::vector<VkExtensionProperties> extensions(propertyCount);
    vkEnumerateDeviceExtensionProperties(m_vkPhysicalDevice, nullptr, &propertyCount, extensions.data());
    // for (VkExtensionProperties extension : extensions) {
    //     std::cout << std::format("extension: {}\n", extension.extensionName);
    // }
    return extensions;
}

VkPhysicalDeviceMemoryProperties PhysicalDevice::getPhysicalDeviceMemoryProperties() const
{
    VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &vkPhysicalDeviceMemoryProperties);
    return vkPhysicalDeviceMemoryProperties;
}

uint32_t PhysicalDevice::findMemoryTypeIndex(
    uint32_t usableMemoryIndexBits,
    MemoryPropertyFlags requiredProperties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &memProperties);

    for (uint32_t index = 0; index < memProperties.memoryTypeCount; index++) {
        if ((usableMemoryIndexBits & (1 << index))
            && bitsSet(memProperties.memoryTypes[index].propertyFlags, requiredProperties)) {
            return index;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

std::vector<VkQueueFamilyProperties> PhysicalDevice::getAllQueueFamilyProperties() const
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> allQueueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, allQueueFamilyProperties.data());
    return allQueueFamilyProperties;
}

Queue Device::getDeviceQueue(int deviceQueueFamilyIndex, int deviceQueueIndex) const
{
    VkQueue vkQueue;
    vkGetDeviceQueue(m_handle, deviceQueueFamilyIndex, deviceQueueIndex, &vkQueue);
    if (vkQueue == nullptr) {
        throw Exception(VK_ERROR_UNKNOWN);
    }
    return Queue(vkQueue, deviceQueueFamilyIndex, *this);
}

};
