#pragma once

#include <format>
#include <string>
#include <vector>
#include <iostream>

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

}