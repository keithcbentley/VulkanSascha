#pragma once

#include <array>
#include <format>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace vkcpp {

class VersionNumber {

    uint32_t m_vkVersionNumber {};

public:
    VersionNumber()
        : m_vkVersionNumber(0)
    {
    }
    VersionNumber(uint32_t vkVersionNumber)
        : m_vkVersionNumber(vkVersionNumber)
    {
    }
    VersionNumber(uint32_t major, uint32_t minor, uint32_t patch)
        : m_vkVersionNumber(VK_MAKE_API_VERSION(0, major, minor, patch))
    {
    }

    uint32_t major() const
    {
        return VK_API_VERSION_MAJOR(m_vkVersionNumber);
    }
    uint32_t minor() const
    {
        return VK_API_VERSION_MINOR(m_vkVersionNumber);
    }
    uint32_t patch() const
    {
        return VK_API_VERSION_PATCH(m_vkVersionNumber);
    }
    uint32_t variant() const
    {
        return VK_API_VERSION_VARIANT(m_vkVersionNumber);
    }

    operator uint32_t() const
    {
        return m_vkVersionNumber;
    }

    operator bool() const
    {
        return m_vkVersionNumber != 0;
    }

    auto operator<=>(const VersionNumber& other) const
    {
        if (auto cmp = major() <=> other.major(); cmp != 0) {
            return cmp;
        }
        if (auto cmp = minor() <=> other.minor(); cmp != 0) {
            return cmp;
        }
        return patch() <=> other.patch();
    }

    std::string asString() const
    {
        return std::format("{}.{}.{} ({})", major(), minor(), patch(), variant());
    }

    static VersionNumber getVersionNumber();
};

class LayerProperties {

public:
    LayerProperties() = delete;
    ~LayerProperties() = delete;
    LayerProperties(const LayerProperties&) = delete;
    LayerProperties& operator=(const LayerProperties&) = delete;
    LayerProperties(LayerProperties&&) noexcept = delete;
    LayerProperties& operator=(LayerProperties&&) noexcept = delete;

    static std::vector<VkLayerProperties> getAllInstanceLayerProperties()
    {
        uint32_t instanceLayerCount;
        VkResult vkResult = vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkLayerProperties> allInstanceLayerProperties(instanceLayerCount);
        vkResult = vkEnumerateInstanceLayerProperties(&instanceLayerCount, allInstanceLayerProperties.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return allInstanceLayerProperties;
    }
};

class InstanceExtensionProperties {

public:
    InstanceExtensionProperties() = delete;
    ~InstanceExtensionProperties() = delete;
    InstanceExtensionProperties(const InstanceExtensionProperties&) = delete;
    InstanceExtensionProperties& operator=(const InstanceExtensionProperties&) = delete;
    InstanceExtensionProperties(InstanceExtensionProperties&&) noexcept = delete;
    InstanceExtensionProperties& operator=(InstanceExtensionProperties&&) noexcept = delete;

    static std::vector<VkExtensionProperties> getAllInstanceExtensionProperties()
    {
        uint32_t instanceExtensionCount;
        VkResult vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkExtensionProperties> allInstanceExtensionProperties = std::vector<VkExtensionProperties>(instanceExtensionCount);
        vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, allInstanceExtensionProperties.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return allInstanceExtensionProperties;
    }
};

class PhysicalDeviceFeatures {

    VkPhysicalDeviceFeatures2 m_features2 {};
    VkPhysicalDeviceVulkan11Features m_featuresV11 {};
    VkPhysicalDeviceVulkan12Features m_featuresV12 {};
    VkPhysicalDeviceVulkan13Features m_featuresV13 {};
    VkPhysicalDeviceVulkan14Features m_featuresV14 {};

    void reassemble()
    {
        m_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        m_featuresV11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        m_featuresV12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        m_featuresV13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        m_featuresV14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

        m_features2.pNext = &m_featuresV11;
        m_featuresV11.pNext = &m_featuresV12;
        m_featuresV12.pNext = &m_featuresV13;
        m_featuresV13.pNext = &m_featuresV14;
        m_featuresV14.pNext = nullptr;
    }

public:
    PhysicalDeviceFeatures()
    {
        reassemble();
    }

    ~PhysicalDeviceFeatures() = default;

    PhysicalDeviceFeatures(const PhysicalDeviceFeatures& other)
        : m_features2(other.m_features2)
        , m_featuresV11(other.m_featuresV11)
        , m_featuresV12(other.m_featuresV12)
        , m_featuresV13(other.m_featuresV13)
        , m_featuresV14(other.m_featuresV14)
    {
        reassemble();
    }

    PhysicalDeviceFeatures& operator=(const PhysicalDeviceFeatures& other)
    {
        m_features2 = other.m_features2;
        m_featuresV11 = other.m_featuresV11;
        m_featuresV12 = other.m_featuresV12;
        m_featuresV13 = other.m_featuresV13;
        m_featuresV14 = other.m_featuresV14;
        reassemble();
        return *this;
    }

    PhysicalDeviceFeatures(PhysicalDeviceFeatures&& other) noexcept
        : m_features2(other.m_features2)
        , m_featuresV11(other.m_featuresV11)
        , m_featuresV12(other.m_featuresV12)
        , m_featuresV13(other.m_featuresV13)
        , m_featuresV14(other.m_featuresV14)
    {
        reassemble();
    }

    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures&& other) noexcept
    {
        m_features2 = std::move(other.m_features2);
        m_featuresV11 = std::move(other.m_featuresV11);
        m_featuresV12 = std::move(other.m_featuresV12);
        m_featuresV13 = std::move(other.m_featuresV13);
        m_featuresV14 = std::move(other.m_featuresV14);
        reassemble();
        return *this;
    }

    const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures()
    {
        return m_features2.features;
    }

    const VkPhysicalDeviceVulkan11Features& featuresV11()
    {
        return m_featuresV11;
    }

    const VkPhysicalDeviceVulkan12Features& featuresV12()
    {
        return m_featuresV12;
    }

    const VkPhysicalDeviceVulkan13Features& featuresV13()
    {
        m_featuresV13;
    }

    const VkPhysicalDeviceVulkan14Features& featuresV14()
    {
        return m_featuresV14;
    }

    operator VkPhysicalDeviceFeatures2*()
    {
        return &m_features2;
    }
};

class PhysicalDeviceProperties {

    VkPhysicalDeviceProperties2 m_properties2 {};
    VkPhysicalDeviceVulkan11Properties m_propertiesV11 {};
    VkPhysicalDeviceVulkan12Properties m_propertiesV12 {};
    VkPhysicalDeviceVulkan13Properties m_propertiesV13 {};
    VkPhysicalDeviceVulkan14Properties m_propertiesV14 {};

    void reassemble()
    {
        m_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        m_propertiesV11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
        m_propertiesV12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
        m_propertiesV13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
        m_propertiesV14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;

        m_properties2.pNext = &m_propertiesV11;
        m_propertiesV11.pNext = &m_propertiesV12;
        m_propertiesV12.pNext = &m_propertiesV13;
        m_propertiesV13.pNext = &m_propertiesV14;
        m_propertiesV14.pNext = nullptr;
    }

public:
    PhysicalDeviceProperties()
    {
        reassemble();
    }

    ~PhysicalDeviceProperties() = default;

    PhysicalDeviceProperties(const PhysicalDeviceProperties& other)
        : m_properties2(other.m_properties2)
        , m_propertiesV11(other.m_propertiesV11)
        , m_propertiesV12(other.m_propertiesV12)
        , m_propertiesV13(other.m_propertiesV13)
        , m_propertiesV14(other.m_propertiesV14)
    {
        reassemble();
    }

    PhysicalDeviceProperties& operator=(const PhysicalDeviceProperties& other)
    {
        m_properties2 = other.m_properties2;
        m_propertiesV11 = other.m_propertiesV11;
        m_propertiesV12 = other.m_propertiesV12;
        m_propertiesV13 = other.m_propertiesV13;
        m_propertiesV14 = other.m_propertiesV14;
        reassemble();
        return *this;
    }

    //	Not sure if std::move really helps here.
    //	The structures are all flat.
    PhysicalDeviceProperties(PhysicalDeviceProperties&& other) noexcept
        : m_properties2(std::move(other.m_properties2))
        , m_propertiesV11(std::move(other.m_propertiesV11))
        , m_propertiesV12(std::move(other.m_propertiesV12))
        , m_propertiesV13(std::move(other.m_propertiesV13))
        , m_propertiesV14(std::move(other.m_propertiesV14))
    {
        reassemble();
    }

    PhysicalDeviceProperties& operator=(PhysicalDeviceProperties&& other) noexcept
    {
        m_properties2 = std::move(other.m_properties2);
        m_propertiesV11 = std::move(other.m_propertiesV11);
        m_propertiesV12 = std::move(other.m_propertiesV12);
        m_propertiesV13 = std::move(other.m_propertiesV13);
        m_propertiesV14 = std::move(other.m_propertiesV14);
        reassemble();
        return *this;
    }

    const VkPhysicalDeviceProperties& vkPhysicalDeviceProperties()
    {
        return m_properties2.properties;
    }

    const VkPhysicalDeviceVulkan11Properties& vkPhysicalDeviceVulkan11Properties()
    {
        return m_propertiesV11;
    }

    const VkPhysicalDeviceVulkan12Properties& vkPhysicalDeviceVulkan12Properties()
    {
        return m_propertiesV12;
    }

    const VkPhysicalDeviceVulkan13Properties& vkPhysicalDeviceVulkan13Properties()
    {
        return m_propertiesV13;
    }

    const VkPhysicalDeviceVulkan14Properties& vkPhysicalDeviceVulkan14Properties()
    {
        return m_propertiesV14;
    }

    operator VkPhysicalDeviceProperties2*()
    {
        return &m_properties2;
    }
};

class PhysicalDeviceMemoryProperties : public VkPhysicalDeviceMemoryProperties {

    //	VkPhysicalDeviceMemoryProperties and VkMemoryRequirements are screwy data types
    //	and the Vulkan terminology makes things more confusing.
    //	Conceptually, there are a few memory heaps, and each heap supports
    //	some combination of memory properties.  The Vulkan Device Capability Viewer
    //	program included with the Vulkan SDK shows this view of memory.
    //	In practice, things are done backwards.  Not all heaps support all memory
    //	properties.  In particular, some heaps (memory) are on the GPU device.
    //	When memory is needed, instead of searching through every heap for the
    //	required features, the existing feature/heap combinations are predetermined
    //	and made available in the VkPhysicalDeviceMemoryProperties structure.
    //	This is the VkMemoryTypes array in the structure.  The memory type contains
    //	the combination of memory properties, and the corresponding heap that supports it.
    //	Each VkMemoryType is a single combination of properties and heap.  If there
    //	are multiple heaps that support the same properties, there will be an entry
    //	for each heap.  In practice, there aren't that many combinations that are
    //	necessary or even make sense.  The biggest split you'll see is whether the
    //	memory is on the device (GPU), aka DEVICE_LOCAL_BIT, whether the memory is (also)
    //	visible on the host, aka HOST_VISIBLE_BIT, and whether the host visible memory
    //	is coherent, aka HOST_COHERENT_BIT.  Vulkan seems to expose host memory
    //	as a heap, but I'm not sure that is available for allocation using Vulkan.
    //
    //	The other part of the process is figuring out the memory required for a given
    //	use, for example, a buffer.  Vulkan objects like buffers and images will tell
    //	you what kind of memory is necessary for their use.  For example,
    //	vkGetImageMemoryRequirements will tell you the memory requirements for an
    //	image created for a particular use.  In addition to the size information,
    //	the VkMemoryRequirements structure gives you an index, or multiple indices,
    //  of the physical device memory properties that can support the requirement.
    //  The screwy part is that this information is encoded in a bit field.  Since
    //	it's possible that multiple memory/heap combinations might work, rather
    //  than have an arbitrary list of indices, the indices are encoded in a bit field.
    //	Bit 0 corresponds to memoryTypes[0], bit 1 to memoryTypes[1], and so on.
    //	If the bit is set, then the memory index could work for the allocation.
    //	The final step is to determine if there are any additional requirements from
    //	the application.  This is done by checking the application requirements
    //	against the physical memory type properties.  This requires shifting through
    //	the bit field to find the memory index, and then checking the required properites
    //	against the available properties.
    //
    //	In practice, the whole process proceeds "backwards."  For example, suppose we
    //	want a staging buffer to transfer information from host memory to device memory,
    //	so that we can later transfer it to another "better" part of device memory.
    //	We first create a buffer with VK_BUFFER_USAGE_TRANSFER_SRC_BIT since we are
    //	going to use it to transfer data to the "better" memory.  Once we have the
    //	buffer object, we ask for its memory requirements.  In addition to the size,
    //	the memory requirements tell us which memory indices can be used.  This is
    //	encoded in the bit field.  Since we want to use it as a staging buffer, we
    //  have additional requirements.  The memory must be host visible, and for convenience,
    //	we want the memory to be host coherent so we don't have to worry about explicitly
    //	flushing it.  Now we shift through the available bits in the bit field and
    //	check the full memory properties against the required memory properties.
    //	If the memory doesn't support the additional requirements, we move on to the
    //  next potential memory index.

public:
    PhysicalDeviceMemoryProperties()
        : VkPhysicalDeviceMemoryProperties {}
    {
    }

    PhysicalDeviceMemoryProperties(
        const VkPhysicalDeviceMemoryProperties& vkPhysicalDeviceMemoryProperties)
        : VkPhysicalDeviceMemoryProperties(vkPhysicalDeviceMemoryProperties)
    {
    }

    uint32_t findMemoryTypeIndex(
        uint32_t usableMemoryIndexBits,
        MemoryPropertyFlags requiredProperties)
    {

        for (uint32_t index = 0; index < memoryTypeCount; index++) {
            if ((usableMemoryIndexBits & (1 << index))
                && bitsSet(memoryTypes[index].propertyFlags, requiredProperties)) {
                return index;
            }
        }
        throw Exception("PhysicalDeviceMemoryProperties::findMemoryTypeIndex(): failed to find suitable memory type!");
    }
};

class DebugUtilsMessenger {

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* /*pUserData*/)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            std::cerr << "VERBOSE:\n";
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            std::cerr << "INFO:\n";
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            std::cerr << "WARNING:\n";
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            std::cerr << "ERROR:\n";
        } else {
            std::cerr << "OTHER: " << messageSeverity << "\n";
        }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            std::cerr << "MESSAGE_TYPE_GENERAL\n";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            std::cerr << "MESSAGE_TYPE_VALIDATION\n";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            std::cerr << "MESSAGE_TYPE_PERFORMANCE\n";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) {
            std::cerr << "MESSAGE_TYPE_DEVICE_ADDRESS_BINDING\n";
        } else {
            std::cerr << "OTHER: " << messageType << '\n';
        }

        std::cerr << "  " << pCallbackData->pMessage << '\n';
        std::cerr << "<<<<<<<<\n";
        std::flush(std::cerr);
        return VK_FALSE;
    }

public:
    DebugUtilsMessenger() = delete;
    ~DebugUtilsMessenger() = delete;
    DebugUtilsMessenger(const DebugUtilsMessenger&) = delete;
    DebugUtilsMessenger& operator=(const DebugUtilsMessenger&) = delete;
    DebugUtilsMessenger(DebugUtilsMessenger&&) noexcept = delete;
    DebugUtilsMessenger& operator=(DebugUtilsMessenger&&) noexcept = delete;

    static VkDebugUtilsMessengerCreateInfoEXT getCreateInfo()
    {
        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {};
        debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerCreateInfo.pNext = nullptr;
        debugMessengerCreateInfo.flags = 0;
        debugMessengerCreateInfo.messageSeverity = 0
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        //  Not sure if this is a reasonable selection of messages.
        //  It's just what some of the other example code uses.
        debugMessengerCreateInfo.messageType = 0
            //| VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        //  Not sure what's going on here.  This should be available from 1.1 onwards,
        //  but validation layer is complaining.
        //    | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
        debugMessengerCreateInfo.pfnUserCallback = debugCallback;
        debugMessengerCreateInfo.pUserData = nullptr;
        return debugMessengerCreateInfo;
    }
};

class PhysicalDevice {

    VkPhysicalDevice m_vkPhysicalDevice = nullptr;

public:
    //  Vulkan physical devices aren't created or destroyed like other Vulkan
    //  objects.  We just get it from the Vulkan m_vulkanInstance and use it everywhere.
    //  Since it's so simple, we just default the basic delete, copy, and assignment.
    //  Just spelling it out explicitly here so there's no confusion.
    PhysicalDevice() = default;
    ~PhysicalDevice() = default;
    PhysicalDevice(const PhysicalDevice& other) = default;
    PhysicalDevice& operator=(const PhysicalDevice& other) = default;
    PhysicalDevice(PhysicalDevice&& other) noexcept = default;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = default;

    PhysicalDevice(VkPhysicalDevice vkPhysicalDevice)
        : m_vkPhysicalDevice(vkPhysicalDevice)
    {
    }

    operator VkPhysicalDevice() const
    {
        return m_vkPhysicalDevice;
    }

    PhysicalDeviceFeatures getPhysicalDeviceFeatures2();
    PhysicalDeviceProperties getPhysicalDeviceProperties2();

    std::vector<VkExtensionProperties> EnumerateDeviceExtensionProperties();

    PhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties();

    uint32_t findMemoryTypeIndex(
        uint32_t usableMemoryIndexBits,
        MemoryPropertyFlags requiredProperties);

    std::vector<VkQueueFamilyProperties> getAllQueueFamilyProperties();
};

//	InteropHandles are the basis for almost all smart Vulkan objects.
template <typename Handle_t>
class InteropHandle2 {

public:
    using DestroyFunc_t = void (*)(Handle_t);

protected:
    Handle_t m_handle {};
    DestroyFunc_t m_pfnDestroy = nullptr;

    InteropHandle2() = default;

    ~InteropHandle2()
    {
        //  Call the handle specific destroy function.
        if (m_pfnDestroy) {
            (*m_pfnDestroy)(m_handle);
        }
        m_handle = Handle_t {};
    }

    //	TODO: is std::move really necessary?
    //	Creates an original.
    InteropHandle2(Handle_t handle, DestroyFunc_t pfnDestroy)
        : m_handle(std::move(handle))
        , m_pfnDestroy(pfnDestroy)
    {
    }

    //	Creates a copy.
    InteropHandle2(Handle_t handle)
        : m_handle(handle)
        , m_pfnDestroy(nullptr)
    {
    }

    //	Creates a copy.
    InteropHandle2(const InteropHandle2& other)
        : m_handle(other.m_handle)
        , m_pfnDestroy(nullptr)
    {
    }

    //	Note that if other is an original, this becomes the original
    InteropHandle2(InteropHandle2&& other) noexcept
        : m_handle(std::move(other.m_handle))
        , m_pfnDestroy(other.m_pfnDestroy)
    {
        other.m_handle = Handle_t {};
        other.m_pfnDestroy = nullptr;
    }

public:
    operator bool() const
    {
        return !!m_handle;
    }

    //  Handy interoperability type conversion.
    //  We can pass our objects to Vulkan functions
    //  and have the Vulkan m_vkBuffer extracted automagically.
    //  Keeps the code tidier.
    operator Handle_t() const
    {
        if (!m_handle) {
            throw NullHandleException();
        }
        return m_handle;
    }
};

template <typename Handle_t, typename Manager_t = VkDevice>
class InteropHandle3 {

public:
    using DestroyFunc_t = void (*)(Handle_t, Manager_t);

protected:
    Handle_t m_handle {};
    Manager_t m_manager {};
    DestroyFunc_t m_pfnDestroy = nullptr;

    InteropHandle3() = default;

    ~InteropHandle3()
    {
        //  Call the handled specific destroy function.
        if (m_pfnDestroy && m_handle) {
            (*m_pfnDestroy)(m_handle, m_manager);
        }
        m_handle = Handle_t {};
    }

    InteropHandle3(Handle_t handle, Manager_t manager, DestroyFunc_t pfnDestroy)
        : m_handle(std::move(handle))
        , m_manager(std::move(manager))
        , m_pfnDestroy(pfnDestroy)
    {
    }

    InteropHandle3(const InteropHandle3& other)
        : m_handle(other.m_handle)
        , m_manager(other.m_manager)
        , m_pfnDestroy(nullptr)
    {
    }

    InteropHandle3(InteropHandle3&& other) noexcept
        : m_handle(std::move(other.m_handle))
        , m_manager(std::move(other.m_manager))
        , m_pfnDestroy(std::move(other.m_pfnDestroy))
    {
        other.m_handle = Handle_t {};
        other.m_manager = Manager_t {};
        other.m_pfnDestroy = nullptr;
    }

public:
    operator bool() const
    {
        return !!m_handle;
    }

    //  Handy interoperability type conversion.
    //  We can pass our objects to Vulkan functions
    //  and have the Vulkan m_vkBuffer extracted automagically.
    //  Keeps the code tidier.
    operator Handle_t() const
    {
        if (!m_handle) {
            throw NullHandleException();
        }
        return m_handle;
    }

    Manager_t getManager() const
    {
        if (!m_manager) {
            throw NullHandleException();
        }
        return m_manager;
    }
};

class VulkanInstanceCreateInfo : public VkInstanceCreateInfo {

    //  We use sets to collect the layer names and extension names
    //  in order to avoid duplicates.  The info from the sets
    //  will then be used to assemble the vectors (count, data)
    //  used to call the create m_vulkanInstance function.
    std::unordered_set<std::string> m_layerNames {};
    std::unordered_set<std::string> m_extensionNames {};

    std::vector<const char*> m_layerNamesVector {};
    std::vector<const char*> m_extensionNamesVector {};

    //  We aren't doing anything fancy with application names, etc., so just use the raw structure.
    //  If more functionality is needed, we can beef up VkApplicationInfo.
    VkApplicationInfo m_vkApplicationInfo {};

public:
    //	Somewhat arbitrary to make this no-move, no-copy.
    ~VulkanInstanceCreateInfo() = default;
    VulkanInstanceCreateInfo(const VulkanInstanceCreateInfo&) = delete;
    VulkanInstanceCreateInfo& operator=(const VulkanInstanceCreateInfo&) = delete;
    VulkanInstanceCreateInfo(VulkanInstanceCreateInfo&&) noexcept = delete;
    VulkanInstanceCreateInfo& operator=(VulkanInstanceCreateInfo&&) noexcept = delete;

    //  Don't allow just grabbing a pointer since the data
    //  may not have been assembled into the vectors for the
    //  create m_vulkanInstance call.
    const VkInstanceCreateInfo* operator&() = delete;

    VulkanInstanceCreateInfo(VersionNumber versionNumber)
        : VkInstanceCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        m_vkApplicationInfo.apiVersion = versionNumber;
    }

    VulkanInstanceCreateInfo()
        : VulkanInstanceCreateInfo(VersionNumber::getVersionNumber())
    {
    }

    VkInstanceCreateInfo* assemble()
    {
        //	Need to convert from sets of layer names and extensions
        //	to vectors so that they can be used by the Vulkan api.
        m_layerNamesVector.clear(); // hygiene in case we are called twice
        ppEnabledLayerNames = nullptr;
        for (const std::string& layerName : m_layerNames) {
            m_layerNamesVector.push_back(layerName.c_str());
        }
        enabledLayerCount = static_cast<uint32_t>(m_layerNames.size());
        if (enabledLayerCount > 0) {
            ppEnabledLayerNames = m_layerNamesVector.data();
        }

        m_extensionNamesVector.clear(); // hygiene in case we are called twice
        ppEnabledExtensionNames = nullptr;
        for (const std::string& extensionName : m_extensionNames) {
            m_extensionNamesVector.push_back(extensionName.c_str());
        }
        enabledExtensionCount = static_cast<uint32_t>(m_extensionNames.size());
        if (enabledExtensionCount > 0) {
            ppEnabledExtensionNames = m_extensionNamesVector.data();
        }

        pApplicationInfo = &m_vkApplicationInfo;

        return this;
    }

    void addLayer(const char* layerName)
    {
        m_layerNames.insert(layerName);
    }

    void addExtension(const char* extensionName)
    {
        m_extensionNames.insert(extensionName);
    }
};

class VulkanInstance : public InteropHandle2<VkInstance> {

    VkDebugUtilsMessengerEXT m_messenger = nullptr;

    VulkanInstance(VkInstance vkInstance, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkInstance, pfnDestroy)
    {
    }

    static void destroy(VkInstance vkInstance)
    {
        vkDestroyInstance(vkInstance, nullptr);
    }

public:
    VulkanInstance() = default;

    //  Don't copy the messenger if making a copy.
    VulkanInstance(const VulkanInstance& other)
        : InteropHandle2(other)
    {
    }

    VulkanInstance& operator=(const VulkanInstance& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~VulkanInstance();
        new (this) VulkanInstance(other);
        return *this;
    }

    //	In the move constructor, we do move the messenger.
    VulkanInstance(VulkanInstance&& other) noexcept
        : InteropHandle2(std::move(other))
        , m_messenger(other.m_messenger)
    {
        other.m_messenger = nullptr;
    }

    VulkanInstance& operator=(VulkanInstance&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~VulkanInstance();
        new (this) VulkanInstance(std::move(other));
        return *this;
    }

    VulkanInstance(VulkanInstanceCreateInfo& vulkanInstanceCreateInfo)
    {
        VkInstance vkInstance;
        VkResult vkResult = vkCreateInstance(vulkanInstanceCreateInfo.assemble(), nullptr, &vkInstance);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) VulkanInstance(vkInstance, &destroy);
    }

    ~VulkanInstance()
    {
        if (m_messenger) {
            //	Compiler doesn't seem to mind since both function types return void.
            PFN_vkDestroyDebugUtilsMessengerEXT pDestroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(*this, "vkDestroyDebugUtilsMessengerEXT"));
            if (pDestroyFunc) {
                pDestroyFunc(*this, m_messenger, nullptr);
            }
            m_messenger = nullptr;
        }
    }

    void createDebugMessenger()
    {
        //	There's something odd here.  I think the compiler complains here because vkGetInstanceProcAddr returns a pointer to a function that
        //	returns a void, and that is being cast to a function pointer that has a non-void return type.  If the return function is cast to a
        //	function that also has a void return type, the compiler doesn't seem to mind.  It appears to ignore any differences in the parameter
        //	types of the functions.
#pragma warning(suppress : 4191)
        PFN_vkCreateDebugUtilsMessengerEXT pCreateFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(*this, "vkCreateDebugUtilsMessengerEXT"));
        if (pCreateFunc == nullptr) {
            throw Exception(VK_ERROR_EXTENSION_NOT_PRESENT);
        }

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = DebugUtilsMessenger::getCreateInfo();
        VkResult vkResult = (*pCreateFunc)(*this, &debugMessengerCreateInfo, nullptr, &m_messenger);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    std::vector<VkPhysicalDevice> getAllPhysicalDevices() const
    {
        uint32_t physicalDeviceCount = 0;
        VkResult vkResult = vkEnumeratePhysicalDevices(*this, &physicalDeviceCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkPhysicalDevice> vkPhysicalDevices(physicalDeviceCount);
        vkResult = vkEnumeratePhysicalDevices(*this, &physicalDeviceCount, vkPhysicalDevices.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        //	TODO: is this still happening?
        //	IMPORTANT: this is crazy.  If we don't call
        //	vkGetPhysicalDeviceQueueFamilyProperties to get the number
        //	of m_vkQueue families, we can't create m_vkDevice queues later without
        //	the validation layer spitting out an error.  Just calling the function
        //	(and throwing away the value) seems to be sufficient.
        for (VkPhysicalDevice vkPhysicalDevice : vkPhysicalDevices) {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, nullptr);
        }

        return vkPhysicalDevices;
    }

    PhysicalDevice getPhysicalDevice(int physicalDeviceIndex) const
    {
        std::vector<VkPhysicalDevice> vkPhysicalDevices = getAllPhysicalDevices();
        return { vkPhysicalDevices.at(physicalDeviceIndex) };
    }
};

class DeviceQueueCreateInfo : public VkDeviceQueueCreateInfo {
    //  TODO: doesn't m_vkBuffer flags or priorities other than 1.0f.

    static inline constexpr int MAX_DEVICE_QUEUES = 16;

    static inline constexpr std::array<float, MAX_DEVICE_QUEUES>
        s_queuePriorities {
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
        };

public:
    ~DeviceQueueCreateInfo() = default;
    DeviceQueueCreateInfo(const DeviceQueueCreateInfo&) = default;
    DeviceQueueCreateInfo& operator=(const DeviceQueueCreateInfo&) = default;
    DeviceQueueCreateInfo(DeviceQueueCreateInfo&&) noexcept = default;
    DeviceQueueCreateInfo& operator=(DeviceQueueCreateInfo&&) noexcept = default;

    DeviceQueueCreateInfo(uint32_t queueFamilyIndexArg, int queueCountArg)
        : VkDeviceQueueCreateInfo {}
    {
        //  TODO: need to range check the m_vkQueue count arg.
        //  TODO: increase m_vkQueue count to 16.
        sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueFamilyIndex = queueFamilyIndexArg;
        queueCount = static_cast<uint32_t>(queueCountArg);
        pQueuePriorities = s_queuePriorities.data();
    }
};
static_assert(sizeof(DeviceQueueCreateInfo) == sizeof(VkDeviceQueueCreateInfo));

class DeviceCreateInfo {

    VkDeviceCreateInfo m_vkDeviceCreateInfo {};

    std::vector<std::string> m_extensionNames {};
    std::vector<const char*> m_extensionStringPtrs {};

    //  TODO: doesn't m_vkBuffer the full m_vkQueue create options.
    //  Maybe make that info into a separate structure.
    // static constexpr int MAX_DEVICE_QUEUE_FAMILIES = 8;
    // std::array<int, MAX_DEVICE_QUEUE_FAMILIES> m_deviceQueueCounts {};

    std::vector<DeviceQueueCreateInfo> m_deviceQueueCreateInfos {};

public:
    ~DeviceCreateInfo() = default;
    DeviceCreateInfo(const DeviceCreateInfo&) = delete;
    DeviceCreateInfo& operator=(const DeviceCreateInfo&) = delete;
    DeviceCreateInfo(DeviceCreateInfo&&) noexcept = delete;
    DeviceCreateInfo& operator=(DeviceCreateInfo&&) noexcept = delete;

    DeviceCreateInfo()
    {
        m_vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    }

    operator VkDeviceCreateInfo*()
    {
        return &m_vkDeviceCreateInfo;
    }

    void setDeviceFeatures(PhysicalDeviceFeatures& physicalDeviceFeatures)
    {
        m_vkDeviceCreateInfo.pNext = physicalDeviceFeatures;
    }

    void addExtension(const char* extensionName)
    {
        //  Always up to date.
        m_extensionNames.emplace_back(extensionName);
        m_extensionStringPtrs.push_back(
            m_extensionNames.back().c_str());
        m_vkDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_extensionNames.size());
        m_vkDeviceCreateInfo.ppEnabledExtensionNames = m_extensionStringPtrs.data();
    }

    void addDeviceQueue(uint32_t deviceQueueFamilyIndex, int numberOfQueues)
    {
        //  Always up to date.
        //  Vulkan doesn't allow multiple requests for the same
        //  m_vkQueue family.  If we get a request for more queues
        //  in the same family, we need to add it to any existing request.
        //  TODO: need to check args.
        bool foundEntry = false;
        for (DeviceQueueCreateInfo& deviceQueueCreateInfo : m_deviceQueueCreateInfos) {
            if (deviceQueueCreateInfo.queueFamilyIndex == deviceQueueFamilyIndex) {
                deviceQueueCreateInfo.queueCount += numberOfQueues;
                foundEntry = true;
            }
        }
        if (!foundEntry) {
            m_deviceQueueCreateInfos.emplace_back(deviceQueueFamilyIndex, numberOfQueues);
        }
        m_vkDeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(m_deviceQueueCreateInfos.size());
        m_vkDeviceCreateInfo.pQueueCreateInfos = m_deviceQueueCreateInfos.data();
    }
};

class Queue;
class Device : public InteropHandle2<VkDevice> {

    Device(VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkDevice vkDevice)
    {
        vkDestroyDevice(vkDevice, nullptr);
    }

public:
    Device() = default;
    ~Device() = default;

    Device(const Device& other)
        : InteropHandle2(other)
    {
    }

    Device& operator=(const Device& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Device();
        new (this) Device(other);
        return *this;
    }

    Device(Device&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Device& operator=(Device&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Device();
        new (this) Device(std::move(other));
        return *this;
    }

    Device(DeviceCreateInfo& deviceCreateInfo, PhysicalDevice physicalDevice)
    {
        VkDevice vkDevice;
        VkResult vkResult = vkCreateDevice(physicalDevice, deviceCreateInfo, nullptr, &vkDevice);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Device(vkDevice, &destroy);
    }

    Queue getDeviceQueue(int deviceQueueFamily, int deviceQueueIndex);

    void waitIdle()
    {
        vkDeviceWaitIdle(*this);
    }
};

class VulkanContextCreateInfo {

public:
    VulkanContextCreateInfo() = default;
};

class VulkanContext {

    VulkanInstance m_vulkanInstanceOriginal;
    PhysicalDevice m_physicalDeviceOriginal;
    VkPhysicalDevice m_vkPhysicalDeviceOriginal;
    Device m_deviceOriginal;
    VkDevice m_vkDeviceOriginal {};

    PhysicalDeviceFeatures m_physicalDeviceFeatures;
    PhysicalDeviceProperties m_physicalDeviceProperties;
    PhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;

public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    VulkanInstance vulkanInstanceFromContext()
    {
        return m_vulkanInstanceOriginal;
    }

    PhysicalDevice physicalDeviceFromContext()
    {
        return m_physicalDeviceOriginal;
    }

    VkPhysicalDevice vkPhysicalDeviceFromContext()
    {
        return m_vkPhysicalDeviceOriginal;
    }

    Device deviceFromContext()
    {
        return m_deviceOriginal;
    }

    VkDevice vkDeviceFromContext()
    {
        return m_vkDeviceOriginal;
    }

    const VkPhysicalDeviceProperties& vkPhysicalDeviceProperties()
    {
        return m_physicalDeviceProperties.vkPhysicalDeviceProperties();
    }

    const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures()
    {
        return m_physicalDeviceFeatures.vkPhysicalDeviceFeatures();
    }

    PhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties()
    {
        return m_physicalDeviceMemoryProperties;
    }

    uint32_t findMemoryTypeIndex(
        uint32_t usableMemoryIndexBits,
        MemoryPropertyFlags requiredProperties)
    {
        return m_physicalDeviceMemoryProperties.findMemoryTypeIndex(usableMemoryIndexBits, requiredProperties);
    }

    void init(const VulkanContextCreateInfo& vulkanContextCreateInfo);
};

extern VulkanContext s_vulkanContext;
//	Free functions just to make things easier to use.
//  The free functions call the corresponding function on s_vulkanContext.
//	Clients can call
//	vkcpp::vulkanInstance(),
//	vkcpp::physicalDevice,
//	vkcpp::device,
//	etc
void initVulkanContext(const VulkanContextCreateInfo& vulkanContextCreateInfo);
VulkanInstance vulkanInstance();
PhysicalDevice physicalDevice();
VkPhysicalDevice vkPhysicalDevice();
Device device();
VkDevice vkDevice();
const VkPhysicalDeviceProperties& vkPhysicalDeviceProperties();
const VkPhysicalDeviceFeatures& vkPhysicalDeviceFeatures();
PhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties();
uint32_t findMemoryTypeIndex(uint32_t usableMemoryIndexBits, MemoryPropertyFlags requiredPropertiesArg);

}