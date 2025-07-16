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
    MemoryPropertyFlags requiredPropertiesArg) const
{
	MemoryPropertyFlags requiredProperties(requiredPropertiesArg);
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

void AppContext::init(const AppContextCreateInfo& appContextCreateInfo) {
	VulkanInstanceCreateInfo vulkanInstanceCreateInfo{};
	vulkanInstanceCreateInfo.addLayer("VK_LAYER_KHRONOS_validation");

	vulkanInstanceCreateInfo.addExtension("VK_EXT_debug_utils");
	vulkanInstanceCreateInfo.addExtension("VK_KHR_surface");
	vulkanInstanceCreateInfo.addExtension("VK_KHR_win32_surface");

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = DebugUtilsMessenger::getCreateInfo();
	vulkanInstanceCreateInfo.pNext = &debugCreateInfo;

	//	Create the vulkan instance.  After this, use vulkanInstance().
	s_appContext.m_vulkanInstanceOriginal = VulkanInstance(vulkanInstanceCreateInfo);

	auto allPhysicalDevices = vulkanInstance().getAllPhysicalDevices();

	for (auto p : allPhysicalDevices) {
		PhysicalDevice physicalDevice = PhysicalDevice(p);
		auto deviceProperties = physicalDevice.getPhysicalDeviceProperties2();

		std::cout << std::format("deviceName: {}\n", deviceProperties.m_properties2.properties.deviceName);
		std::cout << std::format("m_requestedApiVersion: {}  driverVersion: {}\n",
								 VersionNumber(deviceProperties.m_properties2.properties.apiVersion).asString(),
								 VersionNumber(deviceProperties.m_properties2.properties.driverVersion).asString());
		auto extensions = physicalDevice.EnumerateDeviceExtensionProperties();
		for (VkExtensionProperties extension : extensions) {
			std::cout << std::format("extension: {}\n", extension.extensionName);
		}

		std::cout << '\n';
	}

	//	Create the physical device.  After this, use physicalDevice().
	s_appContext.m_physicalDeviceOriginal = PhysicalDevice(allPhysicalDevices[0]);

	DeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.addExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	deviceCreateInfo.addDeviceQueue(0, 1);
	deviceCreateInfo.addDeviceQueue(0, 1);
	deviceCreateInfo.addDeviceQueue(1, 1);
	deviceCreateInfo.addDeviceQueue(1, 1);

	DeviceFeatures deviceFeatures = physicalDevice().getPhysicalDeviceFeatures2();
	deviceCreateInfo.setDeviceFeatures(deviceFeatures);

	//	Create (logical) device.  After this, use device().
	s_appContext.m_deviceOriginal = Device(deviceCreateInfo, s_appContext.m_physicalDeviceOriginal);
	s_appContext.m_vkDeviceOriginal = device();

	std::cout << "AppContext::init()\n";

}


const VulkanInstance& vulkanInstance() {
	return s_appContext.vulkanInstanceAppContext();
}

const PhysicalDevice& physicalDevice() {
	return s_appContext.physicalDeviceAppContext();
}

const Device& device() {
	return s_appContext.deviceAppContext();
}

VkDevice vkDevice() {
	return s_appContext.deviceAppContext();
}



};
