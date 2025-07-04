#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <array>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace vkcpp {

class Exception : public std::exception {
    VkResult m_vkResult = VK_ERROR_UNKNOWN;

public:
    Exception(VkResult vkResult)
        : m_vkResult(vkResult)
    {
    }

    Exception(const char* msg)
        : std::exception(msg)
    {
    }

    VkResult vkResult() const { return m_vkResult; }
};

class ShutdownException : public Exception {

public:
    ShutdownException()
        : Exception(VK_NOT_READY)
    {
    }
};

class NullHandleException : public Exception {

public:
    NullHandleException()
        : Exception(VK_INCOMPLETE)
    {
    }
};

//	Use this to make pre-existing type act like
//	another (smarter) type.
template <typename Real_t, typename ActsLike_t>
    requires(sizeof(Real_t) == sizeof(ActsLike_t))
ActsLike_t& wrapToRef(Real_t& real)
{
    ActsLike_t* p = static_cast<ActsLike_t*>(&real);
    return *p;
}

//	Yikes! Vulkan uses enums for bit values but uses
//	non-typesafe uints for the combination of flags.
//	The newer flags use 64 bit uints for values and combinations
//	but again, not typesafe.  So, we need different types
//	for the bits, the combination of bits, and something
//	to differentiate the 64 bit uints.  For the 64 bit case,
//	the bit type and the combination type will be the same.
//	In all cases, the combination type will be the type/value
//	passed to the Vulkan functions or used in the Vulkan structures.
class DefaultBitsetClassId { };
template <typename Bit_t, typename Combination_t, typename IdType_t = DefaultBitsetClassId>
class Bitset {

public:
    Combination_t m_value;

    explicit Bitset(Bit_t value)
        : m_value(value)
    {
    }

    explicit operator Combination_t()
    {
        return m_value;
    }

    Bitset& operator|=(const Bitset& rhs)
    {
        m_value |= rhs.m_value;
        return *this;
    }

    friend Bitset operator|(const Bitset a, const Bitset b)
    {
        Bitset val = a;
        val |= b;
        return val;
    }

    friend bool bitsSet(Combination_t allBits, Bitset requiredBits)
    {
        return (allBits & requiredBits.m_value) == requiredBits.m_value;
    }

    Bitset& operator&=(const Bitset& rhs)
    {
        m_value &= rhs.m_value;
        return *this;
    }

    friend Bitset operator&(const Bitset a, const Bitset b)
    {
        Bitset val = a;
        val &= b;
        return val;
    }

    friend Bitset operator&(const Bitset a, const Combination_t b)
    {
        Bitset val = a;
        val.m_value &= b;
        return val;
    }
};

class Extent2D : public VkExtent2D {

public:
    Extent2D(int widthArg, int heightArg)
    {
        width = static_cast<uint32_t>(widthArg);
        height = static_cast<uint32_t>(heightArg);
    }
};

class Extent3D : public VkExtent3D { };
//	uint32_t    m_drawAreaWidth;
//	uint32_t    m_drawAreaHeight;
//	uint32_t    depth;
//} VkExtent3D;

class Offset2D : public VkOffset2D { };
//	int32_t    x;
//	int32_t    y;
//} VkOffset2D;

class Offset3D : public VkOffset3D { };
//	int32_t    x;
//	int32_t    y;
//	int32_t    z;
//} VkOffset3D;

class Rect2D : public VkRect2D {

public:
    Rect2D()
        : VkRect2D {}
    {
    }

    Rect2D(VkOffset2D vkOffset2D, VkExtent2D vkExtent2D)
    {
        offset = vkOffset2D;
        extent = vkExtent2D;
    }

    Rect2D(VkExtent2D vkExtent2D)
    {
        offset.x = 0;
        offset.y = 0;
        extent = vkExtent2D;
    }
};
static_assert(sizeof(Rect2D) == sizeof(VkRect2D));
template Rect2D& wrapToRef<VkRect2D, Rect2D>(VkRect2D&);

class PipelineStageFlags2Id { };

using PipelineStageFlags2 = Bitset<VkPipelineStageFlagBits2, VkPipelineStageFlagBits2, PipelineStageFlags2Id>;

#define PipelineStageFlags2Value(BARE_VK_VALUE) \
    static const PipelineStageFlags2 BARE_VK_VALUE(VK_##BARE_VK_VALUE##_BIT)

static const PipelineStageFlags2 PIPELINE_STAGE_2_NONE(VK_PIPELINE_STAGE_2_NONE);
PipelineStageFlags2Value(PIPELINE_STAGE_2_TOP_OF_PIPE);
PipelineStageFlags2Value(PIPELINE_STAGE_2_DRAW_INDIRECT);
PipelineStageFlags2Value(PIPELINE_STAGE_2_VERTEX_INPUT);
PipelineStageFlags2Value(PIPELINE_STAGE_2_VERTEX_SHADER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_GEOMETRY_SHADER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_FRAGMENT_SHADER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS);
PipelineStageFlags2Value(PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS);
PipelineStageFlags2Value(PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT);
PipelineStageFlags2Value(PIPELINE_STAGE_2_COMPUTE_SHADER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_ALL_TRANSFER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_TRANSFER);
PipelineStageFlags2Value(PIPELINE_STAGE_2_BOTTOM_OF_PIPE);
PipelineStageFlags2Value(PIPELINE_STAGE_2_HOST);
PipelineStageFlags2Value(PIPELINE_STAGE_2_ALL_GRAPHICS);
PipelineStageFlags2Value(PIPELINE_STAGE_2_ALL_COMMANDS);
PipelineStageFlags2Value(PIPELINE_STAGE_2_COPY);
PipelineStageFlags2Value(PIPELINE_STAGE_2_RESOLVE);
PipelineStageFlags2Value(PIPELINE_STAGE_2_BLIT);
PipelineStageFlags2Value(PIPELINE_STAGE_2_CLEAR);
PipelineStageFlags2Value(PIPELINE_STAGE_2_INDEX_INPUT);
PipelineStageFlags2Value(PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT);
PipelineStageFlags2Value(PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS);

using MemoryPropertyFlags = Bitset<VkMemoryPropertyFlagBits, VkMemoryPropertyFlags>;

#define MemoryPropertyFlagsValue(BARE_VK_VALUE) \
    static const MemoryPropertyFlags BARE_VK_VALUE(VK_##BARE_VK_VALUE##_BIT)

MemoryPropertyFlagsValue(MEMORY_PROPERTY_DEVICE_LOCAL);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_HOST_VISIBLE);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_HOST_COHERENT);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_HOST_CACHED);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_LAZILY_ALLOCATED);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_PROTECTED);

using ShaderStageFlags = Bitset<VkShaderStageFlagBits, VkShaderStageFlags>;
#define ShaderStageFlagsValue(BARE_VK_VALUE) \
    static const ShaderStageFlags BARE_VK_VALUE(VK_##BARE_VK_VALUE##_BIT)

ShaderStageFlagsValue(SHADER_STAGE_VERTEX);
ShaderStageFlagsValue(SHADER_STAGE_TESSELLATION_CONTROL);
ShaderStageFlagsValue(SHADER_STAGE_TESSELLATION_EVALUATION);
ShaderStageFlagsValue(SHADER_STAGE_GEOMETRY);
ShaderStageFlagsValue(SHADER_STAGE_FRAGMENT);
ShaderStageFlagsValue(SHADER_STAGE_COMPUTE);
static const ShaderStageFlags SHADER_STAGE_ALL_GRAPHICS(VK_SHADER_STAGE_ALL_GRAPHICS);

// VK_SHADER_STAGE_ALL = 0x7FFFFFFF,
// VK_SHADER_STAGE_RAYGEN_BIT_KHR = 0x00000100,
// VK_SHADER_STAGE_ANY_HIT_BIT_KHR = 0x00000200,
// VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR = 0x00000400,
// VK_SHADER_STAGE_MISS_BIT_KHR = 0x00000800,
// VK_SHADER_STAGE_INTERSECTION_BIT_KHR = 0x00001000,
// VK_SHADER_STAGE_CALLABLE_BIT_KHR = 0x00002000,
// VK_SHADER_STAGE_TASK_BIT_EXT = 0x00000040,
// VK_SHADER_STAGE_MESH_BIT_EXT = 0x00000080,
// VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI = 0x00004000,
// VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI = 0x00080000,
// VK_SHADER_STAGE_RAYGEN_BIT_NV = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
// VK_SHADER_STAGE_ANY_HIT_BIT_NV = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
// VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
// VK_SHADER_STAGE_MISS_BIT_NV = VK_SHADER_STAGE_MISS_BIT_KHR,
// VK_SHADER_STAGE_INTERSECTION_BIT_NV = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
// VK_SHADER_STAGE_CALLABLE_BIT_NV = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
// VK_SHADER_STAGE_TASK_BIT_NV = VK_SHADER_STAGE_TASK_BIT_EXT,
// VK_SHADER_STAGE_MESH_BIT_NV = VK_SHADER_STAGE_MESH_BIT_EXT,
// VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF

class Device;

template <typename Handle_t, typename Owner_t = VkDevice>
class HandleWithOwner {

public:
    using DestroyFunc_t = void (*)(Handle_t, Owner_t);

protected:
    Handle_t m_handle {};
    Owner_t m_owner {};
    DestroyFunc_t m_pfnDestroy = nullptr;

    HandleWithOwner() = default;

    ~HandleWithOwner()
    {
        //  Call the handled specific destroy function.
        if (m_pfnDestroy && m_handle) {
            (*m_pfnDestroy)(m_handle, m_owner);
        }
    }

    HandleWithOwner(Handle_t handle, Owner_t owner, DestroyFunc_t pfnDestroy)
        : m_handle(std::move(handle))
        , m_owner(std::move(owner))
        , m_pfnDestroy(pfnDestroy)
    {
    }

    HandleWithOwner(Handle_t handle, Owner_t owner)
        : m_handle(std::move(handle))
        , m_owner(std::move(owner))
        , m_pfnDestroy(nullptr)
    {
    }

    HandleWithOwner(const HandleWithOwner& other)
        : m_handle(other.m_handle)
        , m_owner(other.m_owner)
        , m_pfnDestroy(nullptr)
    {
    }

    HandleWithOwner& operator=(const HandleWithOwner& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~HandleWithOwner();
        new (this) HandleWithOwner(other);
        return *this;
    }

    HandleWithOwner(HandleWithOwner&& other) noexcept
        : m_handle(std::move(other.m_handle))
        , m_owner(std::move(other.m_owner))
        , m_pfnDestroy(std::move(other.m_pfnDestroy))
    {
        other.m_handle = Handle_t {};
        other.m_owner = Owner_t {};
        other.m_pfnDestroy = nullptr;
    }

    HandleWithOwner& operator=(HandleWithOwner&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~HandleWithOwner();
        new (this) HandleWithOwner(std::move(other));
        return *this;
    }

public:
    operator bool() const { return !!m_handle; }

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

    Owner_t getOwner() const
    {
        if (!m_owner) {
            throw NullHandleException();
        }
        return m_owner;
    }

    //  Most of the time, the owner is a VkDevice or Device.
    //  In those cases, we can have a more descriptive, type-checked name.
    VkDevice getVkDevice() const
        requires std::same_as<Owner_t, VkDevice>
        || std::same_as<Owner_t, Device>
    {
        if (!m_owner) {
            throw NullHandleException();
        }
        return m_owner;
    }
};

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

    uint32_t major() const { return VK_API_VERSION_MAJOR(m_vkVersionNumber); }
    uint32_t minor() const { return VK_API_VERSION_MINOR(m_vkVersionNumber); }
    uint32_t patch() const { return VK_API_VERSION_PATCH(m_vkVersionNumber); }
    uint32_t variant() const { return VK_API_VERSION_VARIANT(m_vkVersionNumber); }

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

class DeviceFeatures {

    void assemble()
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
    VkPhysicalDeviceFeatures2 m_features2 {};
    VkPhysicalDeviceVulkan11Features m_featuresV11 {};
    VkPhysicalDeviceVulkan12Features m_featuresV12 {};
    VkPhysicalDeviceVulkan13Features m_featuresV13 {};
    VkPhysicalDeviceVulkan14Features m_featuresV14 {};

    DeviceFeatures()
    {
        assemble();
    }

    ~DeviceFeatures() = default;

    DeviceFeatures(const DeviceFeatures& other)
        : m_features2(other.m_features2)
        , m_featuresV11(other.m_featuresV11)
        , m_featuresV12(other.m_featuresV12)
        , m_featuresV13(other.m_featuresV13)
        , m_featuresV14(other.m_featuresV14)
    {
        assemble();
    }

    DeviceFeatures& operator=(const DeviceFeatures& other)
    {
        m_features2 = other.m_features2;
        m_featuresV11 = other.m_featuresV11;
        m_featuresV12 = other.m_featuresV12;
        m_featuresV13 = other.m_featuresV13;
        m_featuresV14 = other.m_featuresV14;
        assemble();
        return *this;
    }

    DeviceFeatures(DeviceFeatures&& other) noexcept
        : m_features2(other.m_features2)
        , m_featuresV11(other.m_featuresV11)
        , m_featuresV12(other.m_featuresV12)
        , m_featuresV13(other.m_featuresV13)
        , m_featuresV14(other.m_featuresV14)
    {
        assemble();
    }

    DeviceFeatures& operator=(DeviceFeatures&& other) noexcept
    {
        m_features2 = other.m_features2;
        m_featuresV11 = other.m_featuresV11;
        m_featuresV12 = other.m_featuresV12;
        m_featuresV13 = other.m_featuresV13;
        m_featuresV14 = other.m_featuresV14;
        assemble();
        return *this;
    }

    operator VkPhysicalDeviceFeatures2*()
    {
        return &m_features2;
    }
};

class DeviceProperties {

    void assemble()
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
    VkPhysicalDeviceProperties2 m_properties2 {};
    VkPhysicalDeviceVulkan11Properties m_propertiesV11 {};
    VkPhysicalDeviceVulkan12Properties m_propertiesV12 {};
    VkPhysicalDeviceVulkan13Properties m_propertiesV13 {};
    VkPhysicalDeviceVulkan14Properties m_propertiesV14 {};

    DeviceProperties()
    {
        assemble();
    }

    ~DeviceProperties() = default;

    DeviceProperties(const DeviceProperties& other)
        : m_properties2(other.m_properties2)
        , m_propertiesV11(other.m_propertiesV11)
        , m_propertiesV12(other.m_propertiesV12)
        , m_propertiesV13(other.m_propertiesV13)
        , m_propertiesV14(other.m_propertiesV14)
    {
        assemble();
    }

    DeviceProperties& operator=(const DeviceProperties& other)
    {
        m_properties2 = other.m_properties2;
        m_propertiesV11 = other.m_propertiesV11;
        m_propertiesV12 = other.m_propertiesV12;
        m_propertiesV13 = other.m_propertiesV13;
        m_propertiesV14 = other.m_propertiesV14;
        assemble();
        return *this;
    }

    DeviceProperties(DeviceProperties&& other) noexcept
        : m_properties2(other.m_properties2)
        , m_propertiesV11(other.m_propertiesV11)
        , m_propertiesV12(other.m_propertiesV12)
        , m_propertiesV13(other.m_propertiesV13)
        , m_propertiesV14(other.m_propertiesV14)
    {
        assemble();
    }

    DeviceProperties& operator=(DeviceProperties&& other) noexcept
    {
        m_properties2 = other.m_properties2;
        m_propertiesV11 = other.m_propertiesV11;
        m_propertiesV12 = other.m_propertiesV12;
        m_propertiesV13 = other.m_propertiesV13;
        m_propertiesV14 = other.m_propertiesV14;
        assemble();
        return *this;
    }

    operator VkPhysicalDeviceProperties2*()
    {
        return &m_properties2;
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

    operator VkPhysicalDevice() const { return m_vkPhysicalDevice; }

    DeviceFeatures getPhysicalDeviceFeatures2() const;

    DeviceProperties getPhysicalDeviceProperties2() const;

    std::vector<VkExtensionProperties> EnumerateDeviceExtensionProperties() const;

    VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties() const;

    uint32_t findMemoryTypeIndex(
        uint32_t usableMemoryIndexBits,
        MemoryPropertyFlags requiredProperties) const;

    std::vector<VkQueueFamilyProperties> getAllQueueFamilyProperties() const;
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
    VulkanInstanceCreateInfo(VulkanInstanceCreateInfo&&) = delete;
    VulkanInstanceCreateInfo& operator=(VulkanInstanceCreateInfo&&) = delete;

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

class VulkanInstance : public HandleWithOwner<VkInstance, VkInstance> {

    VkDebugUtilsMessengerEXT m_messenger = nullptr;

    VulkanInstance(VkInstance vkInstance, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkInstance, vkInstance, pfnDestroy)
    {
    }

    static void destroy(VkInstance vkInstance, VkInstance)
    {
        vkDestroyInstance(vkInstance, nullptr);
    }

public:
    VulkanInstance() = default;

    //  Don't copy the messenger if making a copy.
    VulkanInstance(const VulkanInstance& other)
        : HandleWithOwner(other)
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
        : HandleWithOwner(std::move(other))
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
        //	IMPORTANT: this is crazy.  If we don't call
        //	vkGetPhysicalDeviceQueueFamilyProperties to get the number
        //	of queue families, we can't create m_vkDevice queues later without
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

class Win32SurfaceCreateInfo : public VkWin32SurfaceCreateInfoKHR {

public:
    Win32SurfaceCreateInfo(HWND hWndArg, HINSTANCE hInstanceArg)
        : VkWin32SurfaceCreateInfoKHR {}
    {
        sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        hwnd = hWndArg;
        hinstance = hInstanceArg;
    }
};

class SurfaceCapabilities : public VkSurfaceCapabilitiesKHR {

public:
    SurfaceCapabilities()
        : VkSurfaceCapabilitiesKHR {}
    {
    }
};

class Surface : public HandleWithOwner<VkSurfaceKHR, VkInstance> {

    PhysicalDevice m_physicalDevice;

    static void destroyFunc(VkSurfaceKHR vkSurface, VkInstance vkInstance)
    {
        vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
    }

    Surface(
        VkSurfaceKHR vkSurface,
        VkInstance vkInstance,
        PhysicalDevice physicalDevice,
        DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkSurface, vkInstance, pfnDestroy)
        , m_physicalDevice(physicalDevice)
    {
    }

public:
    Surface() = default;

    Surface(
        const Win32SurfaceCreateInfo& win32SurfaceCreateInfo,
        const VulkanInstance& vulkanInstance,
        PhysicalDevice physicalDevice)
    {
        VkSurfaceKHR vkSurface;
        VkResult vkResult = vkCreateWin32SurfaceKHR(vulkanInstance, &win32SurfaceCreateInfo, nullptr, &vkSurface);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Surface(vkSurface, vulkanInstance, physicalDevice, &destroyFunc);
    }

    SurfaceCapabilities getSurfaceCapabilities() const
    {
        SurfaceCapabilities surfaceCapabilities;
        VkResult vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_physicalDevice,
            m_handle,
            &surfaceCapabilities);

        if (vkResult == VK_ERROR_UNKNOWN) {
            throw ShutdownException();
        }

        if (vkResult == VK_ERROR_SURFACE_LOST_KHR) {
            throw ShutdownException();
        }

        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        return surfaceCapabilities;
    }

    std::vector<VkSurfaceFormatKHR> getSurfaceFormats() const
    {
        uint32_t formatCount;
        VkResult vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_physicalDevice, *this, &formatCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_physicalDevice, *this, &formatCount, surfaceFormats.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return surfaceFormats;
    }

    std::vector<VkPresentModeKHR> getSurfacePresentModes() const
    {
        uint32_t presentModeCount;
        VkResult vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, *this, &presentModeCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkPresentModeKHR> presentModes;
        vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, *this, &presentModeCount, presentModes.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return presentModes;
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
        //  TODO: need to range check the queue count arg.
        //  TODO: increase queue count to 16.
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

    //  TODO: doesn't m_vkBuffer the full queue create options.
    //  Maybe make that info into a separate structure.
    // static constexpr int MAX_DEVICE_QUEUE_FAMILIES = 8;
    // std::array<int, MAX_DEVICE_QUEUE_FAMILIES> m_deviceQueueCounts {};

    std::vector<DeviceQueueCreateInfo> m_deviceQueueCreateInfos {};

public:
    ~DeviceCreateInfo() = default;
    DeviceCreateInfo(const DeviceCreateInfo&) = delete;
    DeviceCreateInfo& operator=(const DeviceCreateInfo&) = delete;
    DeviceCreateInfo(DeviceCreateInfo&&) = delete;
    DeviceCreateInfo& operator=(DeviceCreateInfo&&) = delete;

    DeviceCreateInfo()
    {
        m_vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    }

    operator VkDeviceCreateInfo*()
    {
        return &m_vkDeviceCreateInfo;
    }

    void setDeviceFeatures(DeviceFeatures& deviceFeatures)
    {
        m_vkDeviceCreateInfo.pNext = deviceFeatures;
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
        //  queue family.  If we get a request for more queues
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
class Device : public HandleWithOwner<VkDevice, VkPhysicalDevice> {

    Device(VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkDevice, vkPhysicalDevice, pfnDestroy)
    {
    }

    static void destroy(VkDevice vkDevice, VkPhysicalDevice)
    {
        vkDestroyDevice(vkDevice, nullptr);
    }

public:
    Device() = default;

    Device(DeviceCreateInfo& deviceCreateInfo, PhysicalDevice physicalDevice)
    {
        VkDevice vkDevice;
        VkResult vkResult = vkCreateDevice(physicalDevice, deviceCreateInfo, nullptr, &vkDevice);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Device(vkDevice, physicalDevice, &destroy);
    }

    PhysicalDevice getPhysicalDevice() const
    {
        return PhysicalDevice(getOwner());
    }

    Queue getDeviceQueue(int deviceQueueFamily, int deviceQueueIndex) const;

    uint32_t findMemoryTypeIndex(uint32_t usableMemoryIndexBits, MemoryPropertyFlags requiredProperties) const
    {
        return getPhysicalDevice().findMemoryTypeIndex(usableMemoryIndexBits, requiredProperties);
    }

    void waitIdle() const
    {
        vkDeviceWaitIdle(*this);
    }
};

class Semaphore : public HandleWithOwner<VkSemaphore> {

    Semaphore(VkSemaphore vkSemaphore, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkSemaphore, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkSemaphore vkSemaphore, VkDevice vkDevice)
    {
        vkDestroySemaphore(vkDevice, vkSemaphore, nullptr);
    }

public:
    Semaphore() = default;

    Semaphore(const VkSemaphoreCreateInfo& vkSemaphoreCreateInfo, VkDevice vkDevice)
    {
        VkSemaphore vkSemaphore;
        VkResult vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, nullptr, &vkSemaphore);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Semaphore(vkSemaphore, vkDevice, &destroy);
    }

    Semaphore(VkDevice vkDevice)
    {
        VkSemaphoreCreateInfo vkSemaphoreCreateInfo {};
        vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        new (this) Semaphore(vkSemaphoreCreateInfo, vkDevice);
    }
};

class Fence : public HandleWithOwner<VkFence> {
    //	Vulkan set/reset/signaled names are not really clear when it comes to fences.
    //	Use better open/closed terminology.
#define VKCPP_FENCE_CREATE_OPENED VK_FENCE_CREATE_SIGNALED_BIT

    Fence(VkFence vkFence, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkFence, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkFence vkFence, VkDevice vkDevice)
    {
        vkDestroyFence(vkDevice, vkFence, nullptr);
    }

public:
    Fence() = default;

    //	Kind of an exception to the argument ordering usually used.
    //	Flags are almost always 0, so make them optional.  Only
    //	the m_vkDevice is required.
    Fence(VkDevice vkDevice, VkFenceCreateFlags vkFenceCreateFlags = 0)
    {
        VkFenceCreateInfo vkFenceCreateInfo {};
        vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkFenceCreateInfo.flags = vkFenceCreateFlags;
        VkFence vkFence;
        VkResult vkResult = vkCreateFence(vkDevice, &vkFenceCreateInfo, nullptr, &vkFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Fence(vkFence, vkDevice, &destroy);
    }

    void close() const
    {
        VkFence vkFence = *this;
        vkResetFences(m_owner, 1, &vkFence);
    }

    void wait() const
    {
        VkFence vkFence = *this;
        vkWaitForFences(m_owner, 1, &vkFence, VK_TRUE, UINT64_MAX);
    }
};

class DeviceMemory : public HandleWithOwner<VkDeviceMemory> {

    static void destroy(VkDeviceMemory vkDeviceMemory, VkDevice vkDevice)
    {
        vkFreeMemory(vkDevice, vkDeviceMemory, nullptr);
    }

    DeviceMemory(VkDeviceMemory vkDeviceMemory, VkDevice vkDevice, VkDeviceSize size, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkDeviceMemory, vkDevice, pfnDestroy)
        , m_size(size)
    {
    }

    VkDeviceSize m_size = 0;

public:
    DeviceMemory() = default;

    DeviceMemory(const VkMemoryAllocateInfo& vkMemoryAllocateInfo, VkDevice vkDevice)
    {
        VkDeviceMemory vkDeviceMemory;
        VkResult vkResult = vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, nullptr, &vkDeviceMemory);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DeviceMemory(vkDeviceMemory, vkDevice, vkMemoryAllocateInfo.allocationSize, &destroy);
    }

    DeviceMemory(
        const VkMemoryRequirements& vkMemoryRequirements,
        MemoryPropertyFlags requiredMemoryPropertyFlags,
        const Device& device)
    {
        VkMemoryAllocateInfo vkMemoryAllocateInfo {};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex = device.findMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, requiredMemoryPropertyFlags);
        new (this) DeviceMemory(vkMemoryAllocateInfo, device);
    }
};

class Buffer : public HandleWithOwner<VkBuffer, Device> {

    static void destroy(VkBuffer vkBuffer, Device device)
    {
        vkDestroyBuffer(device, vkBuffer, nullptr);
    }

    Buffer(VkBuffer vkBuffer, const Device& device, VkDeviceSize size, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkBuffer, device, pfnDestroy)
        , m_size(size)
    {
    }

    Buffer(const VkBufferCreateInfo& vkBufferCreateInfo, const Device& device)
    {
        VkBuffer vkBuffer;
        VkResult vkResult = vkCreateBuffer(device, &vkBufferCreateInfo, nullptr, &vkBuffer);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Buffer(vkBuffer, device, vkBufferCreateInfo.size, &destroy);
    }

    VkDeviceSize m_size = 0;

public:
    Buffer() = default;

    Buffer(
        VkBufferUsageFlags vkBufferUsageFlags,
        VkDeviceSize size,
        uint32_t queueFamilyIndex,
        const Device& device)
    {
        VkBufferCreateInfo vkBufferCreateInfo {};
        vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vkBufferCreateInfo.usage = vkBufferUsageFlags;
        vkBufferCreateInfo.size = size;
        vkBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        uint32_t queueFamilyIndexLocal = queueFamilyIndex;
        vkBufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndexLocal;
        new (this) Buffer(vkBufferCreateInfo, device);
    }

    VkDeviceSize size() const
    {
        return m_size;
    }

    VkMemoryRequirements getMemoryRequirements() const
    {
        VkMemoryRequirements vkMemoryRequirements;
        vkGetBufferMemoryRequirements(getVkDevice(), *this, &vkMemoryRequirements);
        return vkMemoryRequirements;
    }

    DeviceMemory allocateDeviceMemory(MemoryPropertyFlags requiredMemoryPropertyFlags) const
    {
        VkMemoryRequirements vkMemoryRequirements = getMemoryRequirements();
        return DeviceMemory(vkMemoryRequirements, requiredMemoryPropertyFlags, getOwner());
    }
};

class Buffer_DeviceMemory {

    Buffer_DeviceMemory(Buffer&& buffer, DeviceMemory&& deviceMemory, void* mappedMemory)
        : m_buffer(std::move(buffer))
        , m_deviceMemory(std::move(deviceMemory))
        , m_mappedMemory(mappedMemory)
    {
    }

public:
    Buffer m_buffer;
    DeviceMemory m_deviceMemory;
    void* m_mappedMemory = nullptr;

    Buffer_DeviceMemory() = default;
    ~Buffer_DeviceMemory() = default;
    Buffer_DeviceMemory(const Buffer_DeviceMemory& other) = default;

    Buffer_DeviceMemory& operator=(const Buffer_DeviceMemory& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Buffer_DeviceMemory();
        new (this) Buffer_DeviceMemory(other);
        return *this;
    }

    Buffer_DeviceMemory(Buffer_DeviceMemory&& other) noexcept
        : m_buffer(std::move(other.m_buffer))
        , m_deviceMemory(std::move(other.m_deviceMemory))
        , m_mappedMemory(other.m_mappedMemory)
    {
    }

    Buffer_DeviceMemory& operator=(Buffer_DeviceMemory&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Buffer_DeviceMemory();
        new (this) Buffer_DeviceMemory(std::move(other));
        return *this;
    }

    Buffer_DeviceMemory(
        VkBufferUsageFlags vkBufferUsageFlags,
        VkDeviceSize size,
        uint32_t queueFamilyIndex,
        MemoryPropertyFlags memoryPropertyFlags,
        const Device& device)
    {
        Buffer buffer(vkBufferUsageFlags, size, queueFamilyIndex, device);

        DeviceMemory deviceMemory = buffer.allocateDeviceMemory(memoryPropertyFlags);

        VkResult vkResult = vkBindBufferMemory(device, buffer, deviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        // TODO: should this be a method on DeviceMemory?
        void* mappedMemory;
        vkResult = vkMapMemory(device, deviceMemory, 0, size, 0, &mappedMemory);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        new (this) Buffer_DeviceMemory(std::move(buffer), std::move(deviceMemory), mappedMemory);
    }

    Buffer_DeviceMemory(
        VkBufferUsageFlags vkBufferUsageFlags,
        int64_t size,
        uint32_t queueFamilyIndex,
        MemoryPropertyFlags requiredMemoryPropertyFlags,
        const void* pSrcMem,
        const Device& device)
    {
        new (this) Buffer_DeviceMemory(
            vkBufferUsageFlags,
            size,
            queueFamilyIndex,
            requiredMemoryPropertyFlags,
            device);
        memcpy(m_mappedMemory, pSrcMem, size);
        // unmapMemory();
    }

    void unmapMemory()
    {
        vkUnmapMemory(m_deviceMemory.getVkDevice(), m_deviceMemory);
        m_mappedMemory = nullptr;
    }
};

class ShaderModule : public HandleWithOwner<VkShaderModule> {

    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    static void destroy(VkShaderModule vkShaderModule, VkDevice vkDevice)
    {
        vkDestroyShaderModule(vkDevice, vkShaderModule, nullptr);
    }

    ShaderModule(VkShaderModule vkShaderModule, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner<VkShaderModule>(vkShaderModule, vkDevice, pfnDestroy)
    {
    }

public:
    ShaderModule() = default;

    static ShaderModule createShaderModuleFromFile(const char* fileName, VkDevice vkDevice)
    {
        auto fragShaderCode = readFile(fileName);
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = fragShaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
        VkShaderModule vkShaderModule;
        VkResult vkResult = vkCreateShaderModule(vkDevice, &createInfo, nullptr, &vkShaderModule);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return ShaderModule(vkShaderModule, vkDevice, &destroy);
    }
};

class RenderingAttachmentInfo : public VkRenderingAttachmentInfo {

public:
    RenderingAttachmentInfo()
        : VkRenderingAttachmentInfo {}
    {
        sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    }

    ~RenderingAttachmentInfo() = default;
    RenderingAttachmentInfo(const RenderingAttachmentInfo&) = default;
    RenderingAttachmentInfo& operator=(const RenderingAttachmentInfo&) = default;
    RenderingAttachmentInfo(RenderingAttachmentInfo&&) noexcept = default;
    RenderingAttachmentInfo& operator=(RenderingAttachmentInfo&&) noexcept = default;
};

class RenderingInfo : public VkRenderingInfo {

public:
    RenderingInfo()
        : VkRenderingInfo {}
    {
        sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    }
};

class AttachmentDescription : public VkAttachmentDescription {

public:
    AttachmentDescription()
        : VkAttachmentDescription {}
    {
    }

    static AttachmentDescription simpleColorAttachmentPresentDescription(
        VkFormat colorAttachmentVkFormat)
    {
        AttachmentDescription colorAttachmentDescription;
        //	Reasonable defaults
        colorAttachmentDescription.format = colorAttachmentVkFormat;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        return colorAttachmentDescription;
    }

    static AttachmentDescription simpleColorAttachmentDescription(
        VkFormat colorAttachmentVkFormat)
    {
        AttachmentDescription colorAttachmentDescription;
        //	Reasonable defaults
        colorAttachmentDescription.format = colorAttachmentVkFormat;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        return colorAttachmentDescription;
    }

    static AttachmentDescription simpleDepthAttachmentDescription()
    {
        AttachmentDescription depthAttachmentDescription;
        //	Reasonable defaults
        depthAttachmentDescription.format = VK_FORMAT_D32_SFLOAT;
        depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        return depthAttachmentDescription;
    }
};
static_assert(sizeof(AttachmentDescription) == sizeof(VkAttachmentDescription));

class SubpassDescription {

    //	TODO:	need to figure out which attachments to preserve.
    // preserveAttachmentCount = 2;
    // pPreserveAttachments = s_preserve.data();

    VkSubpassDescription m_vkSubpassDescription {};

    std::vector<VkAttachmentReference> m_inputAttachmentReferences;
    std::vector<VkAttachmentReference> m_colorAttachmentReferences;
    VkAttachmentReference m_depthStencilAttachmentReference {};

    void assemble(const SubpassDescription& other)
    {
        m_vkSubpassDescription.inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentReferences.size());
        m_vkSubpassDescription.pInputAttachments = m_inputAttachmentReferences.data();

        m_vkSubpassDescription.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
        m_vkSubpassDescription.pColorAttachments = m_colorAttachmentReferences.data();

        m_vkSubpassDescription.pDepthStencilAttachment
            = other.m_vkSubpassDescription.pDepthStencilAttachment ? &m_depthStencilAttachmentReference : nullptr;
    }

public:
    SubpassDescription() = default;

    ~SubpassDescription() = default;

    SubpassDescription(const SubpassDescription& other)
        : m_vkSubpassDescription(other.m_vkSubpassDescription)
        , m_inputAttachmentReferences(other.m_inputAttachmentReferences)
        , m_colorAttachmentReferences(other.m_colorAttachmentReferences)
        , m_depthStencilAttachmentReference(other.m_depthStencilAttachmentReference)
    {
        assemble(other);
    }

    SubpassDescription& operator=(const SubpassDescription& other)
    {
        this->~SubpassDescription();
        new (this) SubpassDescription(other);
        assemble(other);
        return *this;
    }

    SubpassDescription(SubpassDescription&& other) noexcept
        : m_vkSubpassDescription(std::move(other.m_vkSubpassDescription))
        , m_inputAttachmentReferences(std::move(other.m_inputAttachmentReferences))
        , m_colorAttachmentReferences(std::move(other.m_colorAttachmentReferences))
        , m_depthStencilAttachmentReference(std::move(other.m_depthStencilAttachmentReference))
    {
        assemble(other);
    }

    SubpassDescription& operator=(SubpassDescription&& other) noexcept
    {
        this->~SubpassDescription();
        new (this) SubpassDescription(std::move(other));
        assemble(other);
        return *this;
    }

    SubpassDescription& setPipelineBindPoint(VkPipelineBindPoint vkPipelineBindPoint)
    {
        m_vkSubpassDescription.pipelineBindPoint = vkPipelineBindPoint;
        return *this;
    }

    SubpassDescription& addInputAttachmentReference(const VkAttachmentReference& vkAttachmentReference)
    {
        m_inputAttachmentReferences.push_back(vkAttachmentReference);
        m_vkSubpassDescription.inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentReferences.size());
        m_vkSubpassDescription.pInputAttachments = m_inputAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& addColorAttachmentReference(const VkAttachmentReference& vkAttachmentReference)
    {
        m_colorAttachmentReferences.push_back(vkAttachmentReference);
        m_vkSubpassDescription.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
        m_vkSubpassDescription.pColorAttachments = m_colorAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& setDepthStencilAttachmentReference(const VkAttachmentReference& vkDepthStencilAttachmentReference)
    {
        m_depthStencilAttachmentReference = vkDepthStencilAttachmentReference;
        m_vkSubpassDescription.pDepthStencilAttachment = &m_depthStencilAttachmentReference;
        return *this;
    }

    VkSubpassDescription& vkSubpassDescription()
    {
        return m_vkSubpassDescription;
    }
};

class SubpassDependency : public VkSubpassDependency {

public:
    SubpassDependency()
        : VkSubpassDependency {}
    {
    }

    ~SubpassDependency() = default;
    SubpassDependency(const SubpassDependency&) = default;
    SubpassDependency& operator=(const SubpassDependency&) = default;
    SubpassDependency(SubpassDependency&&) noexcept = default;
    SubpassDependency& operator=(SubpassDependency&&) noexcept = default;

    SubpassDependency& setDependency(
        uint32_t srcSubpassArg,
        uint32_t dstSubpassArg)
    {
        srcSubpass = srcSubpassArg;
        dstSubpass = dstSubpassArg;
        return *this;
    }

    SubpassDependency& setSrc(
        VkPipelineStageFlags srcStageMaskArg,
        VkAccessFlags srcAccessMaskArg)
    {
        srcStageMask = srcStageMaskArg;
        srcAccessMask = srcAccessMaskArg;
        return *this;
    }

    SubpassDependency& addSrc(
        VkPipelineStageFlags srcStageMaskArg,
        VkAccessFlags srcAccessMaskArg)
    {
        srcStageMask |= srcStageMaskArg;
        srcAccessMask |= srcAccessMaskArg;
        return *this;
    }

    SubpassDependency& setDst(
        VkPipelineStageFlags dstStageMaskArg,
        VkAccessFlags dstAccessMaskArg)
    {
        dstStageMask = dstStageMaskArg;
        dstAccessMask = dstAccessMaskArg;
        return *this;
    }

    SubpassDependency& addDst(
        VkPipelineStageFlags dstStageMaskArg,
        VkAccessFlags dstAccessMaskArg)
    {
        dstStageMask |= dstStageMaskArg;
        dstAccessMask |= dstAccessMaskArg;
        return *this;
    }
};
static_assert(sizeof(SubpassDependency) == sizeof(VkSubpassDependency));

class RenderPassCreateInfo : public VkRenderPassCreateInfo {

    std::vector<VkAttachmentDescription> m_attachmentDescriptions;

    std::vector<SubpassDescription> m_subpassDescriptions;
    std::vector<VkSubpassDescription> m_vkSubpassDescriptions;

    std::vector<SubpassDependency> m_subpassDependencies;
    std::vector<VkSubpassDependency> m_vkSubpassDependencies;

public:
    RenderPassCreateInfo()
        : VkRenderPassCreateInfo {}
    {
    }

    const VkRenderPassCreateInfo* operator&() = delete;

    VkAttachmentReference addAttachment(
        const AttachmentDescription& attachmentDescription,
        VkImageLayout imageLayout)
    {
        m_attachmentDescriptions.emplace_back(attachmentDescription);
        //	"attachment" really means "attachment index in the array of attachments"
        VkAttachmentReference attachmentReference {};
        attachmentReference.attachment = static_cast<uint32_t>(m_attachmentDescriptions.size() - 1);
        attachmentReference.layout = imageLayout;
        return attachmentReference;
    }

    SubpassDescription& addSubpass()
    {
        SubpassDescription& subpassDescription = m_subpassDescriptions.emplace_back();
        subpassDescription.setPipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        return subpassDescription;
    }

    SubpassDependency& addSubpassDependency(
        uint32_t srcSubpass,
        uint32_t dstSubpass)
    {
        SubpassDependency& subpassDependency = m_subpassDependencies.emplace_back();
        subpassDependency.setDependency(srcSubpass, dstSubpass);
        return subpassDependency;
    }

    VkRenderPassCreateInfo* assemble()
    {
        sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        pAttachments = nullptr;
        attachmentCount = static_cast<uint32_t>(m_attachmentDescriptions.size());
        if (attachmentCount > 0) {
            pAttachments = m_attachmentDescriptions.data();
        }

        pSubpasses = nullptr;
        m_vkSubpassDescriptions.clear();
        subpassCount = static_cast<uint32_t>(m_subpassDescriptions.size());
        if (subpassCount > 0) {
            for (SubpassDescription& subpassDescription : m_subpassDescriptions) {
                //	TODO: investigate.  This is either really clever or really risky.
                //	We assemble the subpass description and then push back
                //	a copy of the VkSubpassDescription part of our structure.
                m_vkSubpassDescriptions.emplace_back(subpassDescription.vkSubpassDescription());
            }
            pSubpasses = m_vkSubpassDescriptions.data();
        }

        pDependencies = nullptr;
        m_vkSubpassDependencies.clear();
        dependencyCount = static_cast<uint32_t>(m_subpassDependencies.size());
        if (dependencyCount > 0) {
            for (SubpassDependency& subpassDependency : m_subpassDependencies) {
                m_vkSubpassDependencies.push_back(subpassDependency);
            }
            pDependencies = m_vkSubpassDependencies.data();
        }

        return this;
    }
};

//	TODO: do we really need this yet?
class RenderPassCreateInfo2 : public VkRenderPassCreateInfo2 {

public:
    VkRenderPassCreateInfo2* operator&() = delete;

    VkRenderPassCreateInfo2* assemble()
    {
        return this;
    }
};

class RenderPass : public HandleWithOwner<VkRenderPass> {

    RenderPass(VkRenderPass vkRenderPass, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkRenderPass, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkRenderPass vkRenderPass, VkDevice vkDevice)
    {
        vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);
    }

public:
    RenderPass() = default;

    RenderPass(RenderPassCreateInfo& renderPassCreateInfo, VkDevice vkDevice)
    {
        VkRenderPass vkRenderPass;
        VkResult vkResult = vkCreateRenderPass(vkDevice, renderPassCreateInfo.assemble(), nullptr, &vkRenderPass);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) RenderPass(vkRenderPass, vkDevice, &destroy);
    }

    //	TODO: do we really need this yet?
    // RenderPass(RenderPassCreateInfo2& renderPassCreateInfo2, VkDevice vkDevice) {
    //	VkRenderPass	vkRenderPass;
    //	VkResult vkResult = vkCreateRenderPass2(vkDevice, renderPassCreateInfo2.assemble(), nullptr, &vkRenderPass);
    //	if (vkResult != VK_SUCCESS) {
    //		throw Exception(vkResult);
    //	}
    //	new(this)RenderPass(vkRenderPass, vkDevice, &destroy);
    //}
};

class ImageCreateInfo : public VkImageCreateInfo {

public:
    ImageCreateInfo(
        VkFormat vkFormat,
        VkImageUsageFlags vkUsage)
        : VkImageCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        format = vkFormat;
        usage = vkUsage;

        imageType = VK_IMAGE_TYPE_2D;

        extent.depth = 1;
        mipLevels = 1;
        arrayLayers = 1;
        initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        samples = VK_SAMPLE_COUNT_1_BIT;
        sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ImageCreateInfo& setExtent(VkExtent2D vkExtent2D)
    {
        extent.width = vkExtent2D.width;
        extent.height = vkExtent2D.height;
        return *this;
    }
};

class Image : public HandleWithOwner<VkImage, Device> {

    Image(VkImage vkImage, Device device, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkImage, device, pfnDestroy)
    {
    }

    static void destroy(VkImage vkImage, Device device)
    {
        vkDestroyImage(device, vkImage, nullptr);
    }

public:
    Image() = default;

    Image(const ImageCreateInfo& imageCreateInfo, Device device)
    {
        VkImage vkImage;
        VkResult vkResult = vkCreateImage(device, &imageCreateInfo, nullptr, &vkImage);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Image(vkImage, device, &destroy);
    }

    // static Image fromExisting(VkImage vkImage, Device m_vkDevice) {
    //	return Image(vkImage, m_vkDevice, nullptr);
    // }

    // static std::vector<Image> fromExisting(std::vector<VkImage>& vkImages, Device m_vkDevice) {
    //	std::vector<Image> images;
    //	for (VkImage vkImage : vkImages) {
    //		images.push_back(fromExisting(vkImage, m_vkDevice));
    //	}
    //	return images;
    // }

    VkMemoryRequirements getMemoryRequirements() const
    {
        VkMemoryRequirements vkMemoryRequirements;
        vkGetImageMemoryRequirements(getVkDevice(), *this, &vkMemoryRequirements);
        return vkMemoryRequirements;
    }

    DeviceMemory allocateDeviceMemory(MemoryPropertyFlags requiredProperties) const
    {
        VkMemoryRequirements vkMemoryRequirements = getMemoryRequirements();
        VkMemoryAllocateInfo vkMemoryAllocateInfo {};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex = getOwner().findMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, requiredProperties);
        return vkcpp::DeviceMemory(vkMemoryAllocateInfo, getOwner());
    }
};

class ImageViewCreateInfo : public VkImageViewCreateInfo {

public:
    ImageViewCreateInfo(
        VkImage vkImage,
        VkImageViewType vkImageViewType,
        VkFormat vkFormat,
        VkImageAspectFlags aspectFlags)
        : VkImageViewCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image = vkImage;
        viewType = vkImageViewType;
        format = vkFormat;
        subresourceRange.aspectMask = aspectFlags;
        components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
    }
};

class ImageView : public HandleWithOwner<VkImageView> {

    ImageView(VkImageView vkImageView, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkImageView, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkImageView vkImageView, VkDevice vkDevice)
    {
        vkDestroyImageView(vkDevice, vkImageView, nullptr);
    }

public:
    //	TODO: need to start remembering some info about the image and how the
    //	imageview was created to make things easier to use later.
    ImageView() = default;

    ImageView(const VkImageViewCreateInfo& vkImageViewCreateInfo, VkDevice vkDevice)
    {
        VkImageView vkImageView;
        VkResult vkResult = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, nullptr, &vkImageView);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) ImageView(vkImageView, vkDevice, &destroy);
    }
};

class MemoryBarrier2 : public VkMemoryBarrier2 {

public:
    MemoryBarrier2(
        VkPipelineStageFlags2 srcStageMaskArg,
        VkAccessFlags2 srcAccessMaskArg,
        VkPipelineStageFlags2 dstStageMaskArg,
        VkAccessFlags2 dstAccessMaskArg)
        : VkMemoryBarrier2 {}
    {
        sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;

        srcStageMask = srcStageMaskArg;
        srcAccessMask = srcAccessMaskArg;
        dstStageMask = dstStageMaskArg;
        dstAccessMask = dstAccessMaskArg;
    }
};

class SamplerCreateInfo : public VkSamplerCreateInfo {

public:
    SamplerCreateInfo()
        : VkSamplerCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        magFilter = VK_FILTER_LINEAR;
        minFilter = VK_FILTER_LINEAR;
        mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        anisotropyEnable = VK_FALSE;
        maxAnisotropy = 1.0f;
        borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        unnormalizedCoordinates = VK_FALSE;
        compareEnable = VK_FALSE;
        compareOp = VK_COMPARE_OP_ALWAYS;
    }
};

class Sampler : public HandleWithOwner<VkSampler, Device> {

    Sampler(VkSampler sampler, Device device, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(sampler, device, pfnDestroy)
    {
    }

    static void destroy(VkSampler sampler, Device device)
    {
        vkDestroySampler(device, sampler, nullptr);
    }

public:
    Sampler() = default;

    Sampler(const SamplerCreateInfo& samplerCreateInfo, Device device)
    {
        VkSampler vkSampler;
        VkResult vkResult = vkCreateSampler(device, &samplerCreateInfo, nullptr, &vkSampler);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Sampler(vkSampler, device, &destroy);
    }
};

class ImageMemoryBarrier2 : public VkImageMemoryBarrier2 {

public:
    ImageMemoryBarrier2(VkImageLayout oldLayoutArg, VkImageLayout newLayoutArg, VkImage vkImage)
        : VkImageMemoryBarrier2 {}
    {
        sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

        oldLayout = oldLayoutArg;
        newLayout = newLayoutArg;
        srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image = vkImage;
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;

        //	If the image m_vkDeviceMemory barrier is to be used in a sequence
        //	of command buffer commands, then the access masks will need
        //	to be set according to usage.  (To the best of my current knowledge.)
    }
};

static_assert(sizeof(ImageMemoryBarrier2) == sizeof(VkImageMemoryBarrier2));

class DependencyInfo : public VkDependencyInfo {

    //	TODO: need to add the other dependency types.
    std::vector<MemoryBarrier2> m_memoryBarriers;
    std::vector<ImageMemoryBarrier2> m_imageMemoryBarriers;

public:
    ~DependencyInfo() = default;
    DependencyInfo(const DependencyInfo&) = delete;
    DependencyInfo& operator=(const DependencyInfo&) = delete;
    DependencyInfo(DependencyInfo&&) noexcept = delete;
    DependencyInfo& operator=(DependencyInfo&&) noexcept = delete;

    VkDependencyInfo* operator&() = delete;

    DependencyInfo()
        : VkDependencyInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    }

    void addImageMemoryBarrier(const ImageMemoryBarrier2& imageMemoryBarrier)
    {
        m_imageMemoryBarriers.push_back(imageMemoryBarrier);
    }

    void addMemoryBarrier(const MemoryBarrier2& memoryBarrier)
    {
        m_memoryBarriers.push_back(memoryBarrier);
    }

    VkDependencyInfo* assemble()
    {

        pMemoryBarriers = nullptr;
        memoryBarrierCount = static_cast<uint32_t>(m_memoryBarriers.size());
        if (memoryBarrierCount > 0) {
            pMemoryBarriers = m_memoryBarriers.data();
        }

        pImageMemoryBarriers = nullptr;
        imageMemoryBarrierCount = static_cast<uint32_t>(m_imageMemoryBarriers.size());
        if (imageMemoryBarrierCount > 0) {
            pImageMemoryBarriers = m_imageMemoryBarriers.data();
        }

        return this;
    }
};

//	TODO: should we make some object that has contains a queue and command pool
//	and whatever else needed so we don't have to pass the info around as pairs?
class CommandPool : public HandleWithOwner<VkCommandPool> {

    CommandPool(VkCommandPool vkCommandPool, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkCommandPool, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkCommandPool vkCommandPool, VkDevice vkDevice)
    {
        vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);
    }

public:
    CommandPool() = default;

    CommandPool(const VkCommandPoolCreateInfo& commandPoolCreateInfo, VkDevice vkDevice)
    {
        VkCommandPool vkCommandPool;
        VkResult vkResult = vkCreateCommandPool(
            vkDevice,
            &commandPoolCreateInfo,
            nullptr,
            &vkCommandPool);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) CommandPool(vkCommandPool, vkDevice, &destroy);
    }

    CommandPool(
        VkCommandPoolCreateFlags vkCommandPoolCreateFlags,
        uint32_t queueFamilyIndex,
        VkDevice vkDevice)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = vkCommandPoolCreateFlags;
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        new (this) CommandPool(commandPoolCreateInfo, vkDevice);
    }
};

class ImageSubresourceRange : public VkImageSubresourceRange {

public:
    ImageSubresourceRange(VkImageAspectFlags aspectMaskArg)
        : VkImageSubresourceRange {}
    {
        aspectMask = aspectMaskArg;
        levelCount = 1;
        layerCount = 1;
    }
};

class ImageMemoryBarrier : public VkImageMemoryBarrier {
public:
    ImageMemoryBarrier()
        : VkImageMemoryBarrier {}
    {
        sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    }
};

class CommandBuffer : public HandleWithOwner<VkCommandBuffer, CommandPool> {

    static void destroy(VkCommandBuffer vkCommandBuffer, CommandPool commandPool)
    {
        vkFreeCommandBuffers(commandPool.getVkDevice(), commandPool, 1, &vkCommandBuffer);
    }

    CommandBuffer(
        VkCommandBuffer vkCommandBuffer,
        const CommandPool& commandPool,
        DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkCommandBuffer, commandPool, pfnDestroy)
    {
    }

public:
    CommandBuffer() = default;

    CommandBuffer(const CommandPool& commandPool)
    {
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer vkCommandBuffer;
        vkAllocateCommandBuffers(commandPool.getVkDevice(), &allocInfo, &vkCommandBuffer);
        new (this) CommandBuffer(vkCommandBuffer, commandPool, &destroy);
    }

    static CommandBuffer makeCopy(VkCommandBuffer vkCommandBuffer)
    {
        return CommandBuffer(vkCommandBuffer, CommandPool(), nullptr);
    }

    void reset() const
    {
        VkResult vkResult = vkResetCommandBuffer(*this, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void begin() const
    {
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VkResult vkResult = vkBeginCommandBuffer(*this, &beginInfo);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void beginOneTimeSubmit() const
    {
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(*this, &beginInfo);
    }

    void end() const
    {
        VkResult vkResult = vkEndCommandBuffer(*this);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void cmdBeginRendering(const VkRenderingInfo& vkRenderingInfo) const
    {
        vkCmdBeginRendering(*this, &vkRenderingInfo);
    }

    void cmdEndRendering() const
    {
        vkCmdEndRendering(*this);
    }

    void cmdCopyBufferToImage(
        Buffer buffer,
        Image image,
        uint32_t width,
        uint32_t height) const
    {
        VkBufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(*this, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    //	TODO: do we need to implement cmdCopyBuffer2?
    void cmdCopyBuffer(
        Buffer srcBuffer,
        Buffer dstBuffer,
        VkDeviceSize size) const
    {
        //	TODO: maybe do some size checking on the destination to avoid overwriting.
        VkBufferCopy vkBufferCopy { .srcOffset = 0, .dstOffset = 0, .size = size };

        vkCmdCopyBuffer(*this, srcBuffer, dstBuffer, 1, &vkBufferCopy);
    }

    void cmdCopyBuffer(
        Buffer srcBuffer,
        Buffer dstBuffer) const
    {
        cmdCopyBuffer(srcBuffer, dstBuffer, srcBuffer.size());
    }

    void cmdPipelineBarrier2(
        DependencyInfo& dependencyInfo) const
    {
        vkCmdPipelineBarrier2(*this, dependencyInfo.assemble());
    }

    void cmdBeginRenderPass(const VkRenderPassBeginInfo& vkRenderPassBeginInfo) const
    {
        vkCmdBeginRenderPass(*this, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void cmdEndRenderPass() const
    {
        vkCmdEndRenderPass(*this);
    }

    void cmdSetViewport(
        VkExtent2D vkExtent2D) const
    {
        VkViewport viewport {};
        viewport.width = static_cast<float>(vkExtent2D.width);
        viewport.height = static_cast<float>(vkExtent2D.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(*this, 0, 1, &viewport);
    }

    void cmdSetViewport(uint32_t width, uint32_t height) const
    {
        VkViewport viewport {};
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(*this, 0, 1, &viewport);
    }

    void cmdSetScissor(
        VkExtent2D vkExtent2D) const
    {
        VkRect2D scissor {};
        scissor.extent = vkExtent2D;
        vkCmdSetScissor(*this, 0, 1, &scissor);
    }

    void cmdSetScissor(uint32_t width, uint32_t height) const
    {
        VkRect2D scissor {};
        scissor.extent.width = width;
        scissor.extent.height = height;
        vkCmdSetScissor(*this, 0, 1, &scissor);
    }

    void cmdBindPipeline(
        VkPipeline vkPipeline) const
    {
        vkCmdBindPipeline(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
    }

    void cmdBindDescriptorSet(
        VkPipelineLayout vkPipelineLayout,
        VkDescriptorSet vkDescriptorSet) const
    {
        vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipelineLayout, 0, 1, &vkDescriptorSet, 0, nullptr);
    }

    void cmdBindVertexBuffer(VkBuffer vkBuffer) const
    {
        VkDeviceSize offsets[1] { 0 };
        vkCmdBindVertexBuffers(*this, 0, 1, &vkBuffer, offsets);
    }

    void cmdBindIndexBuffer(VkBuffer vkBuffer, VkIndexType vkIndexType) const
    {
        vkCmdBindIndexBuffer(*this, vkBuffer, 0, vkIndexType);
    }

    void cmdDrawIndexed(uint32_t indexCount) const
    {
        vkCmdDrawIndexed(*this, indexCount, 1, 0, 0, 0);
    }

    void cmdInsertImageMemoryBarrier(
        VkImage vkImage,
        VkAccessFlags vkSrcAccessMask,
        VkAccessFlags vkDstAccessMask,
        VkImageLayout vkImageLayoutOld,
        VkImageLayout vkImageLayoutNew,
        VkPipelineStageFlags vkSrcStageMask,
        VkPipelineStageFlags vkDstStageMask,
        const ImageSubresourceRange& imageSubresourceRange) const
    {
        ImageMemoryBarrier imageMemoryBarrier;
        imageMemoryBarrier.srcAccessMask = vkSrcAccessMask;
        imageMemoryBarrier.dstAccessMask = vkDstAccessMask;
        imageMemoryBarrier.oldLayout = vkImageLayoutOld;
        imageMemoryBarrier.newLayout = vkImageLayoutNew;
        imageMemoryBarrier.image = vkImage;
        imageMemoryBarrier.subresourceRange = imageSubresourceRange;

        vkCmdPipelineBarrier(
            *this,
            vkSrcStageMask,
            vkDstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);
    }
};

class SubmitInfo2 : public VkSubmitInfo2 {

    std::vector<VkSemaphoreSubmitInfo> m_waitSemaphoreInfos;
    std::vector<VkCommandBufferSubmitInfo> m_commandBufferInfos;
    std::vector<VkSemaphoreSubmitInfo> m_signalSemaphoreInfos;

public:
    SubmitInfo2()
        : VkSubmitInfo2 {}
    {
        sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    }

    SubmitInfo2* operator&() = delete;

    void addCommandBuffer(CommandBuffer commandBuffer)
    {
        VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo {};
        vkCommandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        vkCommandBufferSubmitInfo.commandBuffer = commandBuffer;
        m_commandBufferInfos.push_back(vkCommandBufferSubmitInfo);
    }

    void addWaitSemaphore(
        Semaphore semaphore,
        PipelineStageFlags2 waitPipelineStateFlags2

        //			VkPipelineStageFlags2 waitPipelineStateFlags2
    )
    {
        VkSemaphoreSubmitInfo vkSemaphoreSubmitInfo {};
        vkSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        vkSemaphoreSubmitInfo.semaphore = semaphore;
        vkSemaphoreSubmitInfo.stageMask = static_cast<VkPipelineStageFlagBits2>(waitPipelineStateFlags2);
        m_waitSemaphoreInfos.push_back(vkSemaphoreSubmitInfo);
    }

    void addSignalSemaphore(Semaphore semaphore)
    {
        VkSemaphoreSubmitInfo vkSemaphoreSubmitInfo {};
        vkSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        vkSemaphoreSubmitInfo.semaphore = semaphore;
        m_signalSemaphoreInfos.push_back(vkSemaphoreSubmitInfo);
    }

    SubmitInfo2* assemble()
    {
        pWaitSemaphoreInfos = nullptr;
        waitSemaphoreInfoCount = static_cast<uint32_t>(m_waitSemaphoreInfos.size());
        if (waitSemaphoreInfoCount > 0) {
            pWaitSemaphoreInfos = m_waitSemaphoreInfos.data();
        }

        pCommandBufferInfos = nullptr;
        commandBufferInfoCount = static_cast<uint32_t>(m_commandBufferInfos.size());
        if (commandBufferInfoCount > 0) {
            pCommandBufferInfos = m_commandBufferInfos.data();
        }

        pSignalSemaphoreInfos = nullptr;
        signalSemaphoreInfoCount = static_cast<uint32_t>(m_signalSemaphoreInfos.size());
        if (signalSemaphoreInfoCount > 0) {
            pSignalSemaphoreInfos = m_signalSemaphoreInfos.data();
        }

        return this;
    }
};

class PresentInfo : public VkPresentInfoKHR {

    //	TODO: modify to m_vkBuffer more semaphores?
    VkSemaphore m_vkSemaphoreWait = nullptr;
    VkSwapchainKHR m_vkSwapchain = nullptr;
    uint32_t m_swapchainImageIndex = 0;

public:
    PresentInfo()
        : VkPresentInfoKHR {}
    {
        sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    }

    PresentInfo* operator&() = delete;

    void addWaitSemaphore(vkcpp::Semaphore semaphore)
    {
        m_vkSemaphoreWait = semaphore;
    }

    void addSwapchain(
        VkSwapchainKHR vkSwapchain,
        int swapchainImageIndex)
    {
        m_vkSwapchain = vkSwapchain;
        m_swapchainImageIndex = static_cast<uint32_t>(swapchainImageIndex);
    }

    VkPresentInfoKHR* assemble()
    {
        pWaitSemaphores = nullptr; //	Hygiene
        if (m_vkSemaphoreWait) {
            waitSemaphoreCount = 1;
            pWaitSemaphores = &m_vkSemaphoreWait;
        }

        pSwapchains = nullptr;
        pImageIndices = nullptr;
        if (m_vkSwapchain) {
            swapchainCount = 1;
            pSwapchains = &m_vkSwapchain;
            pImageIndices = &m_swapchainImageIndex;
        }

        pResults = nullptr;

        return this;
    }
};

class Queue : public HandleWithOwner<VkQueue, Device> {

public:
    uint32_t m_queueFamilyIndex = 0;

    Queue() = default;

    //	Queues always come from the m_vkDevice and are never (explicitly) destroyed
    Queue(VkQueue vkQueue, uint32_t queueFamilyIndex, Device device)
        : HandleWithOwner(vkQueue, device, nullptr)
        , m_queueFamilyIndex(queueFamilyIndex)
    {
    }

    Queue(const Queue& other)
        : HandleWithOwner(other)
        , m_queueFamilyIndex(other.m_queueFamilyIndex)
    {
    }

    Queue& operator=(const Queue& other)
    {
        HandleWithOwner::operator=(other);
        m_queueFamilyIndex = other.m_queueFamilyIndex;
        return *this;
    }

    void waitIdle() const
    {
        vkQueueWaitIdle(*this);
    }

    void submit2(CommandBuffer commandBuffer) const
    {
        SubmitInfo2 submitInfo2;
        submitInfo2.addCommandBuffer(commandBuffer);
        VkResult vkResult = vkQueueSubmit2(*this, 1, submitInfo2.assemble(), VK_NULL_HANDLE);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2(CommandBuffer commandBuffer, Fence fence) const
    {
        SubmitInfo2 submitInfo2;
        submitInfo2.addCommandBuffer(commandBuffer);
        VkResult vkResult = vkQueueSubmit2(*this, 1, submitInfo2.assemble(), fence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2(SubmitInfo2& submitInfo2, Fence fence) const
    {
        VkResult vkResult = vkQueueSubmit2(*this, 1, submitInfo2.assemble(), fence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2Fenced(CommandBuffer commandBuffer) const
    {
        SubmitInfo2 submitInfo2;
        submitInfo2.addCommandBuffer(commandBuffer);
        vkcpp::Fence completedFence(getVkDevice());
        VkResult vkResult = vkQueueSubmit2(*this, 1, submitInfo2.assemble(), completedFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        completedFence.wait();
    }

    VkResult present(PresentInfo& presentInfo) const
    {
        VkResult vkResult = vkQueuePresentKHR(*this, presentInfo.assemble());
        if (vkResult == VK_SUCCESS) {
            return vkResult;
        }
        if (vkResult == VK_SUBOPTIMAL_KHR) {
            return vkResult;
        }

        throw Exception(vkResult);
    }
};

class DescriptorPoolCreateInfo : public VkDescriptorPoolCreateInfo {

    //	The map allows us to collect the size info in any order.
    //	When it's time to assemble, we'll put the collected info
    //	into the vector so that it's ready for the create call.
    std::map<VkDescriptorType, int> m_poolSizesMap;
    std::vector<VkDescriptorPoolSize> m_poolSizes;

public:
    //	Don't allow getting the pointer to the raw create info structure
    //	since it may not be assembled.  Must call assemble to get the pointer.
    VkDescriptorPoolCreateInfo* operator&() = delete;

    DescriptorPoolCreateInfo()
        : VkDescriptorPoolCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    }

    void addDescriptorCount(VkDescriptorType vkDescriptorType, int count)
    {
        m_poolSizesMap[vkDescriptorType] += count;
    }

    VkDescriptorPoolCreateInfo* assemble()
    {
        m_poolSizes.clear();
        VkDescriptorPoolSize vkDescriptorPoolSize;
        for (std::pair<VkDescriptorType, int> kv : m_poolSizesMap) {
            vkDescriptorPoolSize.type = kv.first;
            vkDescriptorPoolSize.descriptorCount = kv.second;
            m_poolSizes.push_back(vkDescriptorPoolSize);
        }
        poolSizeCount = static_cast<uint32_t>(m_poolSizes.size());
        pPoolSizes = nullptr;
        if (poolSizeCount > 0) {
            pPoolSizes = m_poolSizes.data();
        }

        return this;
    }
};

class DescriptorPool : public HandleWithOwner<VkDescriptorPool> {

    DescriptorPool(VkDescriptorPool vkDescriptorPool, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkDescriptorPool, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkDescriptorPool vkDescriptorPool, VkDevice vkDevice)
    {
        vkDestroyDescriptorPool(vkDevice, vkDescriptorPool, nullptr);
    }

public:
    DescriptorPool() = default;

    DescriptorPool(DescriptorPoolCreateInfo& poolCreateInfo, VkDevice vkDevice)
    {
        VkDescriptorPool vkDescriptorPool;
        VkResult vkResult = vkCreateDescriptorPool(vkDevice, poolCreateInfo.assemble(), nullptr, &vkDescriptorPool);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorPool(vkDescriptorPool, vkDevice, &destroy);
    }
};

struct DescriptorSetLayoutBinding {
    int m_bindingIndex;
    VkDescriptorType m_vkDescriptorType;
    vkcpp::ShaderStageFlags m_shaderStage;
};

class DescriptorSetLayoutCreateInfo : public VkDescriptorSetLayoutCreateInfo {

    std::vector<VkDescriptorSetLayoutBinding> m_bindings;

public:
    VkDescriptorSetLayoutCreateInfo* operator&() = delete;

    DescriptorSetLayoutCreateInfo()
        : VkDescriptorSetLayoutCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    }

    DescriptorSetLayoutCreateInfo(
        std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings)
        : VkDescriptorSetLayoutCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        addDescriptorSetLayoutBindings(descriptorSetLayoutBindings);
    }

    DescriptorSetLayoutCreateInfo& addBinding(
        int bindingIndex,
        VkDescriptorType vkDescriptorType,
        ShaderStageFlags shaderStageFlags)
    {
        //	TODO: add check for binding index already used?
        VkDescriptorSetLayoutBinding layoutBinding {};
        layoutBinding.binding = bindingIndex;
        layoutBinding.descriptorType = vkDescriptorType;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = static_cast<VkShaderStageFlags>(shaderStageFlags);
        layoutBinding.pImmutableSamplers = nullptr;

        m_bindings.push_back(layoutBinding);
        return *this;
    }

    void addDescriptorSetLayoutBindings(
        std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings)
    {
        for (DescriptorSetLayoutBinding& descriptorSetLayoutBinding : descriptorSetLayoutBindings) {
            addBinding(
                descriptorSetLayoutBinding.m_bindingIndex,
                descriptorSetLayoutBinding.m_vkDescriptorType,
                descriptorSetLayoutBinding.m_shaderStage);
        }
    }

    VkDescriptorSetLayoutCreateInfo* assemble()
    {
        pBindings = nullptr;
        bindingCount = static_cast<uint32_t>(m_bindings.size());
        if (bindingCount > 0) {
            pBindings = m_bindings.data();
        }
        return this;
    }
};

class DescriptorSetLayout : public HandleWithOwner<VkDescriptorSetLayout> {

    DescriptorSetLayoutCreateInfo m_descriptorSetLayoutCreateInfo;

    DescriptorSetLayout(
        VkDescriptorSetLayout vkDescriptorSetLayout,
        VkDevice vkDevice,
        DestroyFunc_t pfnDestroy,
        const DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo)
        : HandleWithOwner(vkDescriptorSetLayout, vkDevice, pfnDestroy)
        , m_descriptorSetLayoutCreateInfo(descriptorSetLayoutCreateInfo)
    {
    }

    static void destroy(VkDescriptorSetLayout vkDescriptorSetLayout, VkDevice vkDevice)
    {
        vkDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout, nullptr);
    }

public:
    DescriptorSetLayout() = default;

    DescriptorSetLayout(
        DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo,
        VkDevice vkDevice)
    {
        VkDescriptorSetLayout vkDescriptorSetLayout;
        VkResult vkResult = vkCreateDescriptorSetLayout(
            vkDevice,
            descriptorSetLayoutCreateInfo.assemble(),
            nullptr,
            &vkDescriptorSetLayout);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorSetLayout(vkDescriptorSetLayout, vkDevice, &destroy, descriptorSetLayoutCreateInfo);
    }

    static DescriptorSetLayout create(
        std::vector<vkcpp::DescriptorSetLayoutBinding>& descriptorSetLayoutBindings,
        Device device)
    {
        DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(descriptorSetLayoutBindings);
        return vkcpp::DescriptorSetLayout(descriptorSetLayoutCreateInfo, device);
    }
};

class DescriptorSetUpdater {

    //	TODO: Need to add VkBufferView.  Looks like
    //	image, buffer, and bufferview are the only kinds of
    //	data that are described by descriptors and that
    //	can need to be written/updated.

    //	Union to hold each type of info that can be updated/written.
    union WriteDescriptorInfo {
        VkDescriptorBufferInfo m_vkDescriptorBufferInfo;
        VkDescriptorImageInfo m_vkDescriptorImageInfo;

        WriteDescriptorInfo(const VkDescriptorBufferInfo& vkDescriptorBufferInfo)
            : m_vkDescriptorBufferInfo(vkDescriptorBufferInfo)
        {
        }

        WriteDescriptorInfo(const VkDescriptorImageInfo& vkDescriptorImageInfo)
            : m_vkDescriptorImageInfo(vkDescriptorImageInfo)
        {
        }
    };

    //	Each write descriptor has a parallel piece of data.
    //	The write descriptor takes a pointer to the data so
    //	we need to assemble it when it is needed.  To tell
    //	which pointer field of the write descriptor is being used,
    //	we write a marker into the appropriate field, and then
    //	replace the marker with the real pointer when assembled
    //	for use.
    std::vector<VkWriteDescriptorSet> m_vkWriteDescriptorSets;
    std::vector<WriteDescriptorInfo> m_writeDescriptorInfos;

public:
    void addWriteDescriptor(
        VkDescriptorSet vkDescriptorSet,
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        vkcpp::Buffer buffer,
        VkDeviceSize size)
    {
        const VkDescriptorBufferInfo* marker = reinterpret_cast<VkDescriptorBufferInfo*>(-1);

        VkWriteDescriptorSet vkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vkDescriptorSet,
            .dstBinding = bindingIndex,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vkDescriptorType,
            .pBufferInfo = marker
        };

        VkDescriptorBufferInfo vkDescriptorBufferInfo {
            .buffer = buffer,
            .offset = 0,
            .range = size
        };

        m_vkWriteDescriptorSets.push_back(vkWriteDescriptorSet);
        m_writeDescriptorInfos.push_back(vkDescriptorBufferInfo);
    }

    void addWriteDescriptor(
        VkDescriptorSet vkDescriptorSet,
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        ImageView imageView,
        Sampler sampler)
    {
        const VkDescriptorImageInfo* marker = reinterpret_cast<VkDescriptorImageInfo*>(-1);

        VkWriteDescriptorSet vkWriteDescriptorSet {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vkDescriptorSet,
            .dstBinding = bindingIndex,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vkDescriptorType,
            .pImageInfo = marker
        };

        //	TODO: image layout should probably be a parameter.
        VkDescriptorImageInfo vkDescriptorImageInfo {
            .sampler = sampler,
            .imageView = imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        m_vkWriteDescriptorSets.push_back(vkWriteDescriptorSet);
        m_writeDescriptorInfos.emplace_back(vkDescriptorImageInfo);
    }

    void assemble()
    {
        int index = 0;
        //	Update marked pointers to the proper address from the info array.

        for (VkWriteDescriptorSet& vkWriteDescriptorSet : m_vkWriteDescriptorSets) {
            if (vkWriteDescriptorSet.pBufferInfo) {
                vkWriteDescriptorSet.pBufferInfo = &(m_writeDescriptorInfos.at(index).m_vkDescriptorBufferInfo);
            }
            if (vkWriteDescriptorSet.pImageInfo) {
                vkWriteDescriptorSet.pImageInfo = &(m_writeDescriptorInfos.at(index).m_vkDescriptorImageInfo);
            }
            ++index;
        }
    }

    void updateDescriptorSets(VkDevice vkDevice)
    {
        assemble();
        //	TODO: check for no updates before calling.
        vkUpdateDescriptorSets(
            vkDevice,
            static_cast<uint32_t>(m_vkWriteDescriptorSets.size()),
            m_vkWriteDescriptorSets.data(),
            0, nullptr);
        //	TODO: should the update info be cleared after this?
        //	is it ever reused?
    }
};

class DescriptorSet : public HandleWithOwner<VkDescriptorSet, DescriptorPool> {

    DescriptorSetLayout m_descriptorSetLayout;
    DescriptorSetUpdater m_descriptorSetUpdater;

    DescriptorSet(
        VkDescriptorSet vkDescriptorSet,
        DescriptorPool descriptorPool,
        DestroyFunc_t pfnDestroy,
        const DescriptorSetLayout& descriptorSetLayout)
        : HandleWithOwner(vkDescriptorSet, descriptorPool, pfnDestroy)
        , m_descriptorSetLayout(descriptorSetLayout)
    {
    }

    static void destroy(VkDescriptorSet vkDescriptorSet, DescriptorPool descriptorPool)
    {
        vkFreeDescriptorSets(descriptorPool.getVkDevice(), descriptorPool, 1, &vkDescriptorSet);
    }

public:
    DescriptorSet() = default;

    DescriptorSet(DescriptorSetLayout descriptorSetLayout, DescriptorPool descriptorPool)
    {

        VkDescriptorSetLayout vkDescriptorSetLayout = descriptorSetLayout;
        VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo {};
        vkDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        vkDescriptorSetAllocateInfo.descriptorPool = descriptorPool;
        vkDescriptorSetAllocateInfo.descriptorSetCount = 1;
        vkDescriptorSetAllocateInfo.pSetLayouts = &vkDescriptorSetLayout;

        VkDescriptorSet vkDescriptorSet;
        VkResult vkResult = vkAllocateDescriptorSets(
            descriptorPool.getVkDevice(),
            &vkDescriptorSetAllocateInfo,
            &vkDescriptorSet);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorSet(vkDescriptorSet, descriptorPool, &destroy, descriptorSetLayout);
    }

    //	TODO: should the descriptor type be checked against
    //	the descriptor type in the descriptor set layout info?
    void addWriteDescriptor(
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        vkcpp::Buffer buffer,
        VkDeviceSize size)
    {
        m_descriptorSetUpdater.addWriteDescriptor(
            *this,
            bindingIndex,
            vkDescriptorType,
            buffer,
            size);
    }

    void addWriteDescriptor(
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        ImageView imageView,
        Sampler sampler)
    {
        m_descriptorSetUpdater.addWriteDescriptor(
            *this,
            bindingIndex,
            vkDescriptorType,
            imageView,
            sampler);
    }

    void updateDescriptors()
    {
        m_descriptorSetUpdater.updateDescriptorSets(getOwner().getVkDevice());
    }
};

class PipelineLayoutCreateInfo : public VkPipelineLayoutCreateInfo {

    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

public:
    VkPipelineLayoutCreateInfo* operator&() = delete;

    PipelineLayoutCreateInfo()
        : VkPipelineLayoutCreateInfo()
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    }

    void addDescriptorSetLayout(
        vkcpp::DescriptorSetLayout descriptorSetLayout)
    {
        m_descriptorSetLayouts.push_back(descriptorSetLayout);
    }

    VkPipelineLayoutCreateInfo* assemble()
    {

        setLayoutCount = (uint32_t)m_descriptorSetLayouts.size();
        pSetLayouts = nullptr;
        if (setLayoutCount > 0) {
            pSetLayouts = m_descriptorSetLayouts.data();
        }
        return this;
    }
};

class PipelineLayout : public HandleWithOwner<VkPipelineLayout> {

    PipelineLayout(VkPipelineLayout vkPipelineLayout, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkPipelineLayout, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkPipelineLayout vkPipelineLayout, VkDevice vkDevice)
    {
        vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, nullptr);
    }

public:
    PipelineLayout() = default;

    PipelineLayout(PipelineLayoutCreateInfo& pipelineLayoutCreateInfo, VkDevice vkDevice)
    {
        VkPipelineLayout vkPipelineLayout;
        VkResult vkResult = vkCreatePipelineLayout(vkDevice, pipelineLayoutCreateInfo.assemble(), nullptr, &vkPipelineLayout);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) PipelineLayout(vkPipelineLayout, vkDevice, &destroy);
    }
};

class PipelineInputAssemblyStateCreateInfo : public VkPipelineInputAssemblyStateCreateInfo {

public:
    PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology vkPrimitiveTopology)
        : VkPipelineInputAssemblyStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        topology = vkPrimitiveTopology;
    }
};

class PipelineRasterizationStateCreateInfo : public VkPipelineRasterizationStateCreateInfo {

public:
    //	Default create with reasonable settings.
    PipelineRasterizationStateCreateInfo()
        : VkPipelineRasterizationStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        polygonMode = VK_POLYGON_MODE_FILL;
        lineWidth = 1.0f;
        // cullMode = VK_CULL_MODE_BACK_BIT;
        cullMode = VK_CULL_MODE_NONE;
        frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
};

class PipelineMultisampleStateCreateInfo : public VkPipelineMultisampleStateCreateInfo {

public:
    //	Default create with reasonable settings.
    PipelineMultisampleStateCreateInfo()
        : VkPipelineMultisampleStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        minSampleShading = 1.0f;
    }
};

class PipelineColorBlendAttachmentState : public VkPipelineColorBlendAttachmentState {

public:
    //	Default create with reasonable settings.
    PipelineColorBlendAttachmentState()
        : VkPipelineColorBlendAttachmentState {}
    {
        colorWriteMask
            = VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        blendEnable = VK_FALSE;
        srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendOp = VK_BLEND_OP_ADD;
        srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        alphaBlendOp = VK_BLEND_OP_ADD;
    }
};

class PipelineColorBlendStateCreateInfo : public VkPipelineColorBlendStateCreateInfo {

public:
    //	Default create with reasonable settings.
    PipelineColorBlendStateCreateInfo()
        : VkPipelineColorBlendStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        logicOpEnable = VK_FALSE;
        logicOp = VK_LOGIC_OP_COPY;
    }
};

class VertexBinding {
    //	This contains the vertex binding and the corresponding
    //	attribute descriptions for the binding.

public:
    VkVertexInputBindingDescription m_vkVertexInputBindingDescription {};
    std::vector<VkVertexInputAttributeDescription> m_vkVertexInputAttributeDescriptions;

    VertexBinding(
        uint32_t binding,
        uint32_t stride,
        VkVertexInputRate inputRate)
    {
        m_vkVertexInputBindingDescription.binding = binding;
        m_vkVertexInputBindingDescription.stride = stride;
        m_vkVertexInputBindingDescription.inputRate = inputRate;
    }

    void addVertexInputAttributeDescription(
        int bindingIndex,
        int location,
        VkFormat vkFormat,
        uint32_t offset)
    {
        VkVertexInputAttributeDescription vkVertexInputAttributeDescription {};
        vkVertexInputAttributeDescription.binding = static_cast<uint32_t>(bindingIndex);
        vkVertexInputAttributeDescription.location = static_cast<uint32_t>(location);
        vkVertexInputAttributeDescription.format = vkFormat;
        vkVertexInputAttributeDescription.offset = static_cast<uint32_t>(offset);
        m_vkVertexInputAttributeDescriptions.push_back(vkVertexInputAttributeDescription);
    }
};

class PipelineDepthStencilStateCreateInfo : public VkPipelineDepthStencilStateCreateInfo {

public:
    PipelineDepthStencilStateCreateInfo()
        : VkPipelineDepthStencilStateCreateInfo {}
    {
        //	Reasonable defaults
        sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthTestEnable = VK_TRUE;
        depthWriteEnable = VK_TRUE;
        depthCompareOp = VK_COMPARE_OP_LESS; // Less depth means in front of
        depthBoundsTestEnable = VK_FALSE;
        stencilTestEnable = VK_FALSE;
    }
};
static_assert(sizeof(PipelineDepthStencilStateCreateInfo) == sizeof(VkPipelineDepthStencilStateCreateInfo));

class PipelineDynamicStateCreateInfo : public VkPipelineDynamicStateCreateInfo {

    std::vector<VkDynamicState> m_dynamicStates;

public:
    VkPipelineDynamicStateCreateInfo* operator&() = delete;

    PipelineDynamicStateCreateInfo()
        : VkPipelineDynamicStateCreateInfo {}
    {
    }

    void addDynamicState(VkDynamicState vkDynamicState)
    {
        m_dynamicStates.push_back(vkDynamicState);
    }

    VkPipelineDynamicStateCreateInfo* assemble()
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
        pDynamicStates = nullptr;
        if (dynamicStateCount > 0) {
            pDynamicStates = m_dynamicStates.data();
        }
        return this;
    }
};

class GraphicsPipelineCreateInfo {

    PipelineInputAssemblyStateCreateInfo m_inputAssemblyStateCreateInfo { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

    std::vector<VkVertexInputBindingDescription> m_vertexInputBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributeDescriptions;
    VkPipelineVertexInputStateCreateInfo m_vkPipelineVertexInputStateCreateInfo {};

    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStageCreateInfos;

    PipelineDynamicStateCreateInfo m_pipelineDynamicStateCreateInfo;

    VkViewport m_viewport {};
    VkRect2D m_scissor {};

    VkPipelineViewportStateCreateInfo m_viewportState {};

    PipelineRasterizationStateCreateInfo m_pipelineRasterizationStateCreateInfo;
    PipelineMultisampleStateCreateInfo m_pipelineMultisampleStateCreateInfo;
    PipelineColorBlendAttachmentState m_pipelineColorBlendAttachmentState;
    PipelineColorBlendStateCreateInfo m_pipelineColorBlendStateCreateInfo;
    PipelineDepthStencilStateCreateInfo m_vkPipelineDepthStencilStateCreateInfo;

    PipelineLayout m_pipelineLayout;
    RenderPass m_renderPass;
    int m_subpassNumber;

    //	Contains rather than inherits from the Vulkan structure.
    //	Not sure if it makes any difference.  It's a tiny bit
    //	more understandable when assembling since there is so much
    //	going on during the assembly.  It's clearer what's going
    //	into the Vulkan create info structure.
    VkGraphicsPipelineCreateInfo m_vkGraphicsPipelineCreateInfo {};

public:
    void addShaderModule(
        vkcpp::ShaderModule shaderModule,
        VkShaderStageFlagBits vkShaderStageFlagBits,
        const char* entryPointName)
    {
        VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo {};
        vkPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vkPipelineShaderStageCreateInfo.stage = vkShaderStageFlagBits;
        vkPipelineShaderStageCreateInfo.module = shaderModule;
        vkPipelineShaderStageCreateInfo.pName = entryPointName;
        m_shaderStageCreateInfos.push_back(vkPipelineShaderStageCreateInfo);
    }

    void addVertexBinding(const VertexBinding& vertexBinding)
    {
        //	Take the binding and split it to the binding description
        //	and the corresponding attribute description since they are
        //	passed separately when creating the m_vkPipeline.
        m_vertexInputBindingDescriptions.push_back(vertexBinding.m_vkVertexInputBindingDescription);
        for (const VkVertexInputAttributeDescription& vkVertexInputAttributeDescription : vertexBinding.m_vkVertexInputAttributeDescriptions) {
            m_vertexInputAttributeDescriptions.push_back(vkVertexInputAttributeDescription);
        }
    }

    void addDynamicState(VkDynamicState vkDynamicState)
    {
        m_pipelineDynamicStateCreateInfo.addDynamicState(vkDynamicState);
    }

    void setViewportExtent(VkExtent2D extent)
    {
        m_viewport.width = static_cast<float>(extent.width);
        m_viewport.height = static_cast<float>(extent.height);
    }

    void setScissorExtent(VkExtent2D extent)
    {
        m_scissor.extent = extent;
    }

    void setPipelineLayout(PipelineLayout pipelineLayout)
    {
        m_pipelineLayout = pipelineLayout;
    }

    void setRenderPass(RenderPass renderPass, int subpassNumber)
    {
        m_renderPass = renderPass;
        m_subpassNumber = subpassNumber;
    }

    //	A bit dodgy.  Returning pointer to internal member.
    //	Should just be used to create m_vkPipeline.
    VkGraphicsPipelineCreateInfo* assemble()
    {

        //	TODO: need to clear out create info in case we are called twice.

        //	Assemble m_vkPipeline create info
        m_vkGraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        //	Shaders
        m_vkGraphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(m_shaderStageCreateInfos.size());
        if (m_vkGraphicsPipelineCreateInfo.stageCount > 0) {
            m_vkGraphicsPipelineCreateInfo.pStages = m_shaderStageCreateInfos.data();
        }

        //	Just assemble this create structure in place.
        //	Doesn't seem to be a need to make the create structure independent right now.
        //	TODO: maybe make independent when more vertex info?  This is getting messy.
        m_vkPipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        m_vkPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindingDescriptions.size());
        if (m_vkPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount) {
            m_vkPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = m_vertexInputBindingDescriptions.data();
        }

        m_vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributeDescriptions.size());
        if (m_vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount) {
            m_vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = m_vertexInputAttributeDescriptions.data();
        }

        m_vkGraphicsPipelineCreateInfo.pVertexInputState = &m_vkPipelineVertexInputStateCreateInfo;

        m_vkGraphicsPipelineCreateInfo.pInputAssemblyState = &m_inputAssemblyStateCreateInfo;

        m_viewport.maxDepth = 1.0f;
        // TODO make the viewportState smarter?
        m_viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_viewportState.viewportCount = 1;
        m_viewportState.pViewports = &m_viewport;
        m_viewportState.scissorCount = 1;
        m_viewportState.pScissors = &m_scissor;
        m_vkGraphicsPipelineCreateInfo.pViewportState = &m_viewportState;

        m_vkGraphicsPipelineCreateInfo.pRasterizationState = &m_pipelineRasterizationStateCreateInfo;

        m_vkGraphicsPipelineCreateInfo.pMultisampleState = &m_pipelineMultisampleStateCreateInfo;

        m_vkGraphicsPipelineCreateInfo.pDepthStencilState = nullptr;

        m_pipelineColorBlendStateCreateInfo.attachmentCount = 1;
        m_pipelineColorBlendStateCreateInfo.pAttachments = &m_pipelineColorBlendAttachmentState;
        m_vkGraphicsPipelineCreateInfo.pColorBlendState = &m_pipelineColorBlendStateCreateInfo;

        m_vkGraphicsPipelineCreateInfo.pDynamicState = m_pipelineDynamicStateCreateInfo.assemble();

        m_vkGraphicsPipelineCreateInfo.pDepthStencilState = &m_vkPipelineDepthStencilStateCreateInfo;

        m_vkGraphicsPipelineCreateInfo.layout = m_pipelineLayout;
        m_vkGraphicsPipelineCreateInfo.renderPass = m_renderPass;
        m_vkGraphicsPipelineCreateInfo.subpass = m_subpassNumber;
        m_vkGraphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        m_vkGraphicsPipelineCreateInfo.basePipelineIndex = -1; // Optional

        return &m_vkGraphicsPipelineCreateInfo;
    }
};

class GraphicsPipeline : public HandleWithOwner<VkPipeline> {

    GraphicsPipeline(VkPipeline vkPipeline, VkDevice vkDevice, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkPipeline, vkDevice, pfnDestroy)
    {
    }

    static void destroy(VkPipeline vkPipeline, VkDevice vkDevice)
    {
        vkDestroyPipeline(vkDevice, vkPipeline, nullptr);
    }

public:
    GraphicsPipeline() = default;

    GraphicsPipeline(GraphicsPipelineCreateInfo& pipelineCreateInfo, VkDevice vkDevice)
    {
        VkPipeline vkPipeline;
        VkResult vkResult = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, pipelineCreateInfo.assemble(), nullptr, &vkPipeline);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) GraphicsPipeline(vkPipeline, vkDevice, &destroy);
    }
};

class SwapchainCreateInfo : public VkSwapchainCreateInfoKHR {

public:
    SwapchainCreateInfo()
        : VkSwapchainCreateInfoKHR {}
    {
    }

    SwapchainCreateInfo(
        Surface surfaceArg,
        uint32_t minImageCountArg,
        VkFormat imageFormatArg,
        VkColorSpaceKHR imageColorSpaceArg,
        VkPresentModeKHR presentModeArg)
        : VkSwapchainCreateInfoKHR {}
    {
        sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        surface = surfaceArg;
        minImageCount = minImageCountArg;
        imageFormat = imageFormatArg;
        imageColorSpace = imageColorSpaceArg;
        imageArrayLayers = 1;
        imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        queueFamilyIndexCount = 0;
        pQueueFamilyIndices = nullptr;
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        presentMode = presentModeArg;
        clipped = VK_TRUE;
        oldSwapchain = VK_NULL_HANDLE;
    }
};

class Swapchain : public HandleWithOwner<VkSwapchainKHR, Device> {

    Swapchain(VkSwapchainKHR vkSwapchain, Device device, DestroyFunc_t pfnDestroy, VkExtent2D vkSwapchainImageExtent)
        : HandleWithOwner(vkSwapchain, device, pfnDestroy)
        , m_vkSwapchainImageExtent(vkSwapchainImageExtent)
    {
    }

    static void destroy(VkSwapchainKHR vkSwapchain, Device device)
    {
        vkDestroySwapchainKHR(device, vkSwapchain, nullptr);
    }

    VkExtent2D m_vkSwapchainImageExtent = { .width = 0, .height = 0 };

public:
    Swapchain() = default;

    Swapchain(const VkSwapchainCreateInfoKHR& vkSwapchainCreateInfo, Device device)
    {
        VkSwapchainKHR vkSwapchain;
        VkResult vkResult = vkCreateSwapchainKHR(device, &vkSwapchainCreateInfo, nullptr, &vkSwapchain);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Swapchain(vkSwapchain, device, &destroy, vkSwapchainCreateInfo.imageExtent);
    }

    VkExtent2D imageExtent() const { return m_vkSwapchainImageExtent; }

    std::vector<VkImage> getImages() const
    {
        uint32_t swapchainImageCount;
        VkResult vkResult = vkGetSwapchainImagesKHR(getVkDevice(), *this, &swapchainImageCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        vkResult = vkGetSwapchainImagesKHR(getVkDevice(), *this, &swapchainImageCount, swapchainImages.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return swapchainImages;
    }
};

class FramebufferCreateInfo : public VkFramebufferCreateInfo {

    std::vector<VkImageView> m_attachments;

public:
    VkFramebufferCreateInfo* operator&() = delete;

    FramebufferCreateInfo(
        RenderPass renderPassArg,
        VkExtent2D vkExtent2D)
        : VkFramebufferCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        renderPass = renderPassArg;
        width = vkExtent2D.width;
        height = vkExtent2D.height;
        layers = 1;
    }

    FramebufferCreateInfo& addAttachment(ImageView imageView)
    {
        m_attachments.push_back(imageView);
        return *this;
    }

    VkFramebufferCreateInfo* assemble()
    {
        attachmentCount = static_cast<uint32_t>(m_attachments.size());
        pAttachments = m_attachments.data();
        return this;
    }
};

class Image_Memory {

    Image_Memory(Image&& image, DeviceMemory&& deviceMemory)
        : m_image(std::move(image))
        , m_deviceMemory(std::move(deviceMemory))
    {
    }

public:
    Image m_image;
    DeviceMemory m_deviceMemory;

    Image_Memory() = default;

    Image_Memory(const Image_Memory&) = delete;
    Image_Memory& operator=(const Image_Memory&) = delete;

    Image_Memory(Image_Memory&& other) noexcept
        : m_image(std::move(other.m_image))
        , m_deviceMemory(std::move(other.m_deviceMemory))
    {
    }

    Image_Memory& operator=(Image_Memory&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Image_Memory();
        new (this) Image_Memory(std::move(other));
        return *this;
    }

    Image_Memory(
        const ImageCreateInfo& imageCreateInfo,
        MemoryPropertyFlags properties,
        Device device)
    {
        Image image(imageCreateInfo, device);
        DeviceMemory deviceMemory = image.allocateDeviceMemory(properties);

        VkResult vkResult = vkBindImageMemory(device, image, deviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        new (this) Image_Memory(std::move(image), std::move(deviceMemory));
    }

    Image_Memory(
        VkExtent2D vkExtent2D,
        VkFormat format,
        VkImageUsageFlags usage,
        MemoryPropertyFlags properties,
        Device device)
    {
        ImageCreateInfo imageCreateInfo(format, usage);
        imageCreateInfo.setExtent(vkExtent2D);
        new (this) Image_Memory(imageCreateInfo, properties, device);
    }
};

class Image_Memory_View {

public:
    Image m_image;
    DeviceMemory m_deviceMemory;
    ImageView m_imageView;

    Image_Memory_View() = default;

    Image_Memory_View(
        Image&& image,
        DeviceMemory&& deviceMemory,
        ImageView&& imageView)
        : m_image(std::move(image))
        , m_deviceMemory(std::move(deviceMemory))
        , m_imageView(std::move(imageView))
    {
    }
};

class Framebuffer : public HandleWithOwner<VkFramebuffer, Device> {

    Framebuffer(VkFramebuffer vkFramebuffer, Device device, DestroyFunc_t pfnDestroy)
        : HandleWithOwner(vkFramebuffer, device, pfnDestroy)
    {
    }

    static void destroy(VkFramebuffer vkFramebuffer, Device device)
    {
        vkDestroyFramebuffer(device, vkFramebuffer, nullptr);
    }

    //	Right now, framebuffer holds these just to control lifetime.
    //	TODO: is there ever going to be more than one of these?
    std::vector<Image_Memory_View> m_image_memory_views;

    std::vector<ImageView> m_imageViews;

public:
    Framebuffer() = default;

    Framebuffer(FramebufferCreateInfo& framebufferCreateInfo, Device device)
    {
        VkFramebuffer vkFramebuffer;
        VkResult vkResult = vkCreateFramebuffer(device, framebufferCreateInfo.assemble(), nullptr, &vkFramebuffer);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Framebuffer(vkFramebuffer, device, &destroy);
    }

    void take(Image_Memory_View&& image_memory_view)
    {
        m_image_memory_views.push_back(std::move(image_memory_view));
    }

    void take(ImageView&& imageView)
    {
        m_imageViews.push_back(std::move(imageView));
    }
};

class Swapchain_FrameBuffers {

public:
    static inline Device s_device;

    //	Just save the create info since we need parts of it later.
    SwapchainCreateInfo m_swapchainCreateInfo;

    //	Save the "smart" m_vkSurface since the create info only has the base m_vkBuffer.
    Surface m_surface;

    RenderPass m_renderPass;

    Swapchain m_swapchain;
    std::vector<Framebuffer> m_swapchainFrameBuffers;

    bool m_swapchainUpToDate = false;

private:
    void makeEmpty()
    {
        //	TODO: Need to review the whole move thing to make sure this all makes sense.
        m_swapchainFrameBuffers.clear();
    }

    void destroyFrameBuffers()
    {
        m_swapchainFrameBuffers.clear();
    }

    void destroy()
    {
        if (!s_device) {
            return;
        }
        if (m_swapchain) {
            vkDeviceWaitIdle(s_device);
            destroyFrameBuffers();
        }
    }

    void createSwapchainFrameBuffers()
    {

        std::vector<VkImage> m_swapchainImages = m_swapchain.getImages();

        for (VkImage vkImage : m_swapchainImages) {
            //	The renderpass image view and the depth buffer are "passed"
            //	to the renderpass via attachments to the framebuffer.  This
            //	means that was don't need to actually pass them through code.
            //	All we need to do is create them and make sure they don't disappear.
            //	We just have the framebuffer take ownership of the image view and
            //	depth buffer.
            Image_Memory_View depthBuffer(createDepthBuffer(m_swapchain.imageExtent(), s_device));

            ImageViewCreateInfo imageViewCreateInfo(
                vkImage,
                VK_IMAGE_VIEW_TYPE_2D,
                m_swapchainCreateInfo.imageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT);
            ImageView imageView(imageViewCreateInfo, m_swapchain.getOwner());

            //	Note that attachments here are image views.  When a
            //	Renderpass adds attachments, the attachments are descriptions
            //	of these attachments.
            FramebufferCreateInfo framebufferCreateInfo(m_renderPass, m_swapchain.imageExtent());
            framebufferCreateInfo
                .addAttachment(imageView)
                .addAttachment(depthBuffer.m_imageView);

            Framebuffer framebuffer(framebufferCreateInfo, s_device);
            framebuffer.take(std::move(depthBuffer));
            framebuffer.take(std::move(imageView));
            m_swapchainFrameBuffers.push_back(std::move(framebuffer));
        }
    }

    static Swapchain createSwapchain(
        SwapchainCreateInfo& swapChainCreateInfo,
        Surface surface)
    {
        const VkSurfaceCapabilitiesKHR vkSurfaceCapabilities = surface.getSurfaceCapabilities();
        const VkExtent2D surfaceExtent = vkSurfaceCapabilities.currentExtent;
        //	Can't make "real" swapchains with 0 m_drawAreaWidth or m_drawAreaHeight, e.g.,
        //	the window is minimized.  Return a "null" swapChain if
        //	this occurs.
        if (surfaceExtent.width == 0 || surfaceExtent.height == 0) {
            return Swapchain {};
        }
        //	TODO: should probably sanity check some other m_vkSurface capabilities.
        swapChainCreateInfo.surface = surface;
        swapChainCreateInfo.imageExtent = surfaceExtent;
        swapChainCreateInfo.preTransform = vkSurfaceCapabilities.currentTransform;

        return Swapchain(swapChainCreateInfo, s_device);
    }

public:
    Swapchain_FrameBuffers() { }
    ~Swapchain_FrameBuffers()
    {
        destroy();
    }

    Swapchain_FrameBuffers(
        const SwapchainCreateInfo& swapchainCreateInfo,
        Surface surface)
        : m_swapchainCreateInfo(swapchainCreateInfo)
        , m_surface(surface)
    {
    }

    Swapchain_FrameBuffers(const Swapchain_FrameBuffers&) = delete;
    Swapchain_FrameBuffers& operator=(const Swapchain_FrameBuffers&) = delete;

    Swapchain_FrameBuffers(Swapchain_FrameBuffers&& other) noexcept
        : m_swapchainCreateInfo(std::move(other.m_swapchainCreateInfo))
        , m_surface(std::move(other.m_surface))
        , m_renderPass(std::move(other.m_renderPass))
        , m_swapchain(std::move(other.m_swapchain))
        , m_swapchainFrameBuffers(std::move(other.m_swapchainFrameBuffers))
    {
        other.makeEmpty();
    }

    Swapchain_FrameBuffers& operator=(Swapchain_FrameBuffers&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Swapchain_FrameBuffers();
        new (this) Swapchain_FrameBuffers(std::move(other));
        return *this;
    }

    static void setDevice(Device device)
    {
        s_device = device;
    }

    VkSwapchainKHR vkSwapchain()
    {
        VkSwapchainKHR vkSwapchain = m_swapchain; // Need to force a type conversion for return value.
        return vkSwapchain;
    }

    bool canDraw()
    {
        if (m_swapchain && m_swapchainUpToDate) {
            return true;
        }
        recreateFullSwapchain();
        return m_swapchain;
    }

    VkExtent2D getImageExtent() const
    {
        return m_swapchain.imageExtent();
    }

    const Framebuffer& getFrameBuffer(int index)
    {
        return m_swapchainFrameBuffers.at(index);
    }

    void setRenderPass(RenderPass renderPass)
    {
        m_renderPass = renderPass;
    }

    void recreateFullSwapchain()
    {
        m_swapchainUpToDate = false;
        if (!s_device) {
            return;
        }

        vkDeviceWaitIdle(s_device);
        destroyFrameBuffers();
        //	TODO: can we set the old swapchain to avoid this?
        //	Explicitly destroy the old swapchain for now.
        m_swapchain = std::move(Swapchain());

        m_swapchain = std::move(createSwapchain(m_swapchainCreateInfo, m_surface));
        if (!m_swapchain) {
            return;
        }
        createSwapchainFrameBuffers();
        m_swapchainUpToDate = true;
    }

    void stale()
    {
        m_swapchainUpToDate = false;
    }

    Image_Memory_View createDepthBuffer(
        VkExtent2D vkExtent2D,
        vkcpp::Device device)
    {
        constexpr VkFormat depthBufferFormat = VK_FORMAT_D32_SFLOAT;

        ImageCreateInfo imageCreateInfo(
            depthBufferFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.setExtent(vkExtent2D);
        Image_Memory image_memory(imageCreateInfo, MEMORY_PROPERTY_DEVICE_LOCAL, device);

        ImageViewCreateInfo imageViewCreateInfo(
            image_memory.m_image,
            VK_IMAGE_VIEW_TYPE_2D,
            depthBufferFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        ImageView imageView(imageViewCreateInfo, device);

        return Image_Memory_View(
            std::move(image_memory.m_image),
            std::move(image_memory.m_deviceMemory),
            std::move(imageView));
    }
};

}
