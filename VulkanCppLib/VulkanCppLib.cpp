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

PhysicalDeviceFeatures PhysicalDevice::getPhysicalDeviceFeatures2()
{
    PhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures2(m_vkPhysicalDevice, physicalDeviceFeatures);
    return physicalDeviceFeatures;
}

PhysicalDeviceProperties PhysicalDevice::getPhysicalDeviceProperties2()
{
    PhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties2(m_vkPhysicalDevice, physicalDeviceProperties);
    return physicalDeviceProperties;
}

std::vector<VkExtensionProperties> PhysicalDevice::EnumerateDeviceExtensionProperties()
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

PhysicalDeviceMemoryProperties PhysicalDevice::getPhysicalDeviceMemoryProperties()
{
    VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &vkPhysicalDeviceMemoryProperties);
    return PhysicalDeviceMemoryProperties(vkPhysicalDeviceMemoryProperties);
}

uint32_t PhysicalDevice::findMemoryTypeIndex(
    uint32_t usableMemoryIndexBits,
    MemoryPropertyFlags requiredPropertiesArg)
{
	return getPhysicalDeviceMemoryProperties().findMemoryTypeIndex(
		usableMemoryIndexBits, requiredPropertiesArg);
}

std::vector<VkQueueFamilyProperties> PhysicalDevice::getAllQueueFamilyProperties()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> allQueueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, allQueueFamilyProperties.data());
    return allQueueFamilyProperties;
}

Queue Device::getDeviceQueue(int deviceQueueFamilyIndex, int deviceQueueIndex)
{
    VkQueue vkQueue;
    vkGetDeviceQueue(m_handle, deviceQueueFamilyIndex, deviceQueueIndex, &vkQueue);
    if (vkQueue == nullptr) {
        throw Exception(VK_ERROR_UNKNOWN);
    }
    return Queue(vkQueue, deviceQueueFamilyIndex);
}

void VulkanContext::init(const VulkanContextCreateInfo& vulkanContextCreateInfo)
{
    VulkanInstanceCreateInfo vulkanInstanceCreateInfo {};
    vulkanInstanceCreateInfo.addLayer("VK_LAYER_KHRONOS_validation");

    vulkanInstanceCreateInfo.addExtension("VK_EXT_debug_utils");
    vulkanInstanceCreateInfo.addExtension("VK_KHR_surface");
    vulkanInstanceCreateInfo.addExtension("VK_KHR_win32_surface");

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = DebugUtilsMessenger::getCreateInfo();
    vulkanInstanceCreateInfo.pNext = &debugCreateInfo;

    //	Create the vulkan instance.  After this, use vulkanInstance().
    s_vulkanContext.m_vulkanInstanceOriginal = VulkanInstance(vulkanInstanceCreateInfo);

    auto allPhysicalDevices = vulkanInstance().getAllPhysicalDevices();

    for (auto p : allPhysicalDevices) {
        PhysicalDevice physicalDevice = PhysicalDevice(p);
        auto deviceProperties = physicalDevice.getPhysicalDeviceProperties2();

        std::cout << std::format("deviceName: {}\n", deviceProperties.vkPhysicalDeviceProperties().deviceName);
        std::cout << std::format("m_requestedApiVersion: {}  driverVersion: {}\n",
            VersionNumber(deviceProperties.vkPhysicalDeviceProperties().apiVersion).asString(),
            VersionNumber(deviceProperties.vkPhysicalDeviceProperties().driverVersion).asString());
        auto extensions = physicalDevice.EnumerateDeviceExtensionProperties();
        for (VkExtensionProperties extension : extensions) {
            std::cout << std::format("extension: {}\n", extension.extensionName);
        }

        std::cout << '\n';
    }

    //	Create the physical device.  After this, use physicalDevice().
    s_vulkanContext.m_physicalDeviceOriginal = PhysicalDevice(allPhysicalDevices[0]);
    m_physicalDeviceFeatures = physicalDevice().getPhysicalDeviceFeatures2();
    m_physicalDeviceProperties = physicalDevice().getPhysicalDeviceProperties2();
	m_physicalDeviceMemoryProperties = physicalDevice().getPhysicalDeviceMemoryProperties();

    DeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.addExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    deviceCreateInfo.addDeviceQueue(0, 1);
    deviceCreateInfo.addDeviceQueue(0, 1);
    deviceCreateInfo.addDeviceQueue(1, 1);
    deviceCreateInfo.addDeviceQueue(1, 1);

    deviceCreateInfo.setDeviceFeatures(m_physicalDeviceFeatures);

    //	Create (logical) device.  After this, use device().
    s_vulkanContext.m_deviceOriginal = Device(deviceCreateInfo, s_vulkanContext.m_physicalDeviceOriginal);
    s_vulkanContext.m_vkDeviceOriginal = device();

    std::cout << "AppContext::init()\n";
}

void initVulkanContext(const VulkanContextCreateInfo& vulkanContextCreateInfo)
{
    s_vulkanContext.init(vulkanContextCreateInfo);
}

VulkanInstance& vulkanInstance()
{
    return s_vulkanContext.vulkanInstanceContext();
}

PhysicalDevice& physicalDevice()
{
    return s_vulkanContext.physicalDeviceContext();
}

Device& device()
{
    return s_vulkanContext.deviceContext();
}

VkDevice vkDevice()
{
    return s_vulkanContext.deviceContext();
}

VkPhysicalDeviceProperties& vkPhysicalDeviceProperties()
{
    return s_vulkanContext.vkPhysicalDeviceProperties();
}

VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures()
{
    return s_vulkanContext.vkPhysicalDeviceFeatures();
}

uint32_t findMemoryTypeIndex(
	uint32_t usableMemoryIndexBits,
	MemoryPropertyFlags requiredProperties) {

	return s_vulkanContext.findMemoryTypeIndex(usableMemoryIndexBits, requiredProperties);
}


};
