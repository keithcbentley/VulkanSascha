#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "ExceptBitStruct.hpp"
#include "VulkanContext.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace vkcpp {

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

class Surface : public InteropHandle2<VkSurfaceKHR> {

    static void destroyFunc(VkSurfaceKHR vkSurface)
    {
        vkDestroySurfaceKHR(VulkanInstance(), vkSurface, nullptr);
    }

    Surface(
        VkSurfaceKHR vkSurface,
        DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkSurface, pfnDestroy)
    {
    }

public:
    Surface() = default;
    ~Surface() = default;
    Surface(const Surface& other)
        : InteropHandle2(other)
    {
    }
    Surface& operator=(const Surface& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Surface();
        new (this) Surface(other);
        return *this;
    }

    Surface(Surface&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }
    Surface& operator=(Surface&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Surface();
        new (this) Surface(std::move(other));
        return *this;
    }

    Surface(const Win32SurfaceCreateInfo& win32SurfaceCreateInfo)
    {
        VkSurfaceKHR vkSurface;
        VkResult vkResult = vkCreateWin32SurfaceKHR(VulkanInstance(), &win32SurfaceCreateInfo, nullptr, &vkSurface);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Surface(vkSurface, &destroyFunc);
    }

    SurfaceCapabilities getSurfaceCapabilities() const
    {
        SurfaceCapabilities surfaceCapabilities;
        VkResult vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            PhysicalDevice(),
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
            PhysicalDevice(), *this, &formatCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
            PhysicalDevice(), *this, &formatCount, surfaceFormats.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return surfaceFormats;
    }

    std::vector<VkPresentModeKHR> getSurfacePresentModes() const
    {
        uint32_t presentModeCount;
        VkResult vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice(), *this, &presentModeCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkPresentModeKHR> presentModes;
        vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice(), *this, &presentModeCount, presentModes.data());
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return presentModes;
    }
};

class Semaphore : public InteropHandle2<VkSemaphore> {

    Semaphore(VkSemaphore vkSemaphore, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkSemaphore, pfnDestroy)
    {
    }

    static void destroy(VkSemaphore vkSemaphore)
    {
        vkDestroySemaphore(device(), vkSemaphore, nullptr);
    }

public:
    Semaphore() = default;
    ~Semaphore() = default;

    Semaphore(const Semaphore& other)
        : InteropHandle2(other)
    {
    }
    Semaphore& operator=(const Semaphore& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Semaphore();
        new (this) Semaphore(other);
    }

    Semaphore(Semaphore&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Semaphore& operator=(Semaphore&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Semaphore();
        new (this) Semaphore(std::move(other));
    }

    Semaphore(VkSemaphoreCreateFlags vkSemaphoreCreateFlags)
    {
        //	TODO: how do the flags work with the newer type of semaphores?
        //	We are going to need a new type for them. Should we check the flags
        //	so we don't create one accidentally?
        VkSemaphoreCreateInfo vkSemaphoreCreateInfo {};
        vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkSemaphoreCreateInfo.flags = vkSemaphoreCreateFlags;
        VkSemaphore vkSemaphore;
        VkResult vkResult = vkCreateSemaphore(vkDevice(), &vkSemaphoreCreateInfo, nullptr, &vkSemaphore);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Semaphore(vkSemaphore, &destroy);
    }
};

class Fence : public InteropHandle2<VkFence> {

    Fence(VkFence vkFence, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkFence, pfnDestroy)
    {
    }

    static void destroy(VkFence vkFence)
    {
        vkDestroyFence(device(), vkFence, nullptr);
    }

public:
    Fence() = default;
    ~Fence() = default;

    Fence(const Fence& other)
        : InteropHandle2(other)
    {
    }

    Fence& operator=(const Fence& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Fence();
        new (this) Fence(other);
    }

    Fence(Fence&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Fence& operator=(Fence&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Fence();
        new (this) Fence(std::move(other));
    }

    //	Kind of an exception to the argument ordering usually used.
    //	Flags are almost always 0, so make them optional.  Only
    //	the m_vkDevice is required.

    Fence(VkFenceCreateFlags vkFenceCreateFlags)
    {
        VkFenceCreateInfo vkFenceCreateInfo {};
        vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkFenceCreateInfo.flags = vkFenceCreateFlags;
        VkFence vkFence;
        VkResult vkResult = vkCreateFence(vkDevice(), &vkFenceCreateInfo, nullptr, &vkFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Fence(vkFence, &destroy);
    }

    void wait() const
    {
        VkFence vkFence = *this;
        vkWaitForFences(vkDevice(), 1, &vkFence, VK_TRUE, UINT64_MAX);
    }
};

template <typename T = uint8_t>
class TypedCount {

public:
    uint64_t m_count;

    TypedCount()
        : m_count(1)
    {
    }

    explicit TypedCount(uint64_t count)
        : m_count(count)
    {
    }

    TypedCount(std::vector<T> v)
        : m_count(static_cast<uint64_t>(v.size()))
    {
    }

    VkDeviceSize vkDeviceSize() const
    {
        return sizeof(T) * m_count;
    }
};

class MemoryAllocateInfo : public VkMemoryAllocateInfo {

public:
    MemoryAllocateInfo()
        : VkMemoryAllocateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    }
};

template <typename T = uint8_t>
class DeviceMemory : public InteropHandle2<VkDeviceMemory> {

    static void destroy(VkDeviceMemory vkDeviceMemory)
    {
        vkFreeMemory(device(), vkDeviceMemory, nullptr);
    }

    DeviceMemory(VkDeviceMemory vkDeviceMemory, VkDeviceSize size, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkDeviceMemory, pfnDestroy)
        , m_size(size)
    {
    }

    VkDeviceSize m_size = 0;

public:
    operator VkDeviceMemory() const
    {
        return m_handle;
    }

    DeviceMemory() = default;
    ~DeviceMemory() = default;

    DeviceMemory(const DeviceMemory& other)
        : InteropHandle2(other)
        , m_size(other.m_size)
    {
    }

    DeviceMemory& operator=(const DeviceMemory& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~DeviceMemory();
        new (this) DeviceMemory(other);
        return *this;
    }

    DeviceMemory(DeviceMemory&& other) noexcept
        : InteropHandle2(std::move(other))
        , m_size(other.m_size)
    {
    }

    DeviceMemory& operator=(DeviceMemory&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~DeviceMemory();
        new (this) DeviceMemory(std::move(other));
        return *this;
    }

    DeviceMemory(const VkMemoryAllocateInfo& vkMemoryAllocateInfo)
    {
        VkDeviceMemory vkDeviceMemory;
        VkResult vkResult = vkAllocateMemory(vkDevice(), &vkMemoryAllocateInfo, nullptr, &vkDeviceMemory);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DeviceMemory(vkDeviceMemory, vkMemoryAllocateInfo.allocationSize, &destroy);
    }

    DeviceMemory(
        const VkMemoryRequirements& vkMemoryRequirements,
        MemoryPropertyFlags requiredMemoryPropertyFlags)
    {
        VkMemoryAllocateInfo vkMemoryAllocateInfo {};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex = vkcpp::findMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, requiredMemoryPropertyFlags);
        new (this) DeviceMemory(vkMemoryAllocateInfo);
    }

    void* mapMemory(VkDeviceSize vkDeviceSize)
    {
        void* pData;
        VkResult vkResult = vkMapMemory(vkDevice(), *this, 0, vkDeviceSize, 0, &pData);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return pData;
    }

    void unmapMemory()
    {
        vkUnmapMemory(vkDevice(), *this);
        return;
    }

    void mapCopyUnmap(void* pSource, VkDeviceSize vkDeviceSize)
    {
        void* pMemMapped = mapMemory(vkDeviceSize);
        memcpy(pMemMapped, pSource, vkDeviceSize);
        unmapMemory();
    }
};

class BufferCreateInfo : public VkBufferCreateInfo {

public:
    BufferCreateInfo()
        : VkBufferCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    }
};

template <typename T = uint8_t>
class BufferCreateInfoTyped : public BufferCreateInfo {
public:
    int m_count = 1;
};

template <typename T = uint8_t>
class Buffer : public InteropHandle2<VkBuffer> {

    static void destroy(VkBuffer vkBuffer)
    {
        vkDestroyBuffer(vkDevice(), vkBuffer, nullptr);
    }

    Buffer(VkBuffer vkBuffer, VkDeviceSize size, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkBuffer, pfnDestroy)
    {
    }

public:
    operator VkBuffer() const
    {
        return m_handle;
    }
    Buffer() = default;
    ~Buffer() = default;

    Buffer(const Buffer& other)
        : InteropHandle2(other)
    {
    }

    Buffer& operator=(const Buffer& other)
    {
        if (this == &other) {
            return *this;
        }

        this->~Buffer();
        new (this) Buffer(other);
        return *this;
    }

    Buffer(Buffer&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        this->~Buffer();
        new (this) Buffer(std::move(other));
        return *this;
    }

    Buffer(
        VkBufferUsageFlags vkBufferUsageFlags,
        VkDeviceSize sizeArg,
        uint32_t queueFamilyIndex)
    {
        VkBufferCreateInfo vkBufferCreateInfo {};
        vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vkBufferCreateInfo.usage = vkBufferUsageFlags;
        vkBufferCreateInfo.size = sizeArg;
        vkBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        uint32_t queueFamilyIndexLocal = queueFamilyIndex;
        vkBufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndexLocal;
        new (this) Buffer(vkBufferCreateInfo);
    }

    Buffer(
        VkBufferUsageFlags vkBufferUsageFlags,
        VkDeviceSize size)
        : Buffer(vkBufferUsageFlags, size, 0)
    {
    }

    Buffer(const VkBufferCreateInfo& vkBufferCreateInfo)
    {
        VkBuffer vkBuffer;
        VkResult vkResult = vkCreateBuffer(vkDevice(), &vkBufferCreateInfo, nullptr, &vkBuffer);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Buffer(vkBuffer, vkBufferCreateInfo.size, &destroy);
    }

    Buffer(BufferCreateInfoTyped<T>& bufferCreateInfoTyped)
    {
        bufferCreateInfoTyped.size = sizeof(T) * bufferCreateInfoTyped.m_count;
        VkBuffer vkBuffer;
        VkResult vkResult = vkCreateBuffer(vkDevice(), &bufferCreateInfoTyped, nullptr, &vkBuffer);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Buffer(vkBuffer, bufferCreateInfoTyped.size, &destroy);
    }

    VkMemoryRequirements getMemoryRequirements() const
    {
        VkMemoryRequirements vkMemoryRequirements;
        vkGetBufferMemoryRequirements(vkDevice(), *this, &vkMemoryRequirements);
        return vkMemoryRequirements;
    }

    DeviceMemory<T> allocateDeviceMemory(MemoryPropertyFlags requiredMemoryPropertyFlags) const
    {
        VkMemoryRequirements vkMemoryRequirements = getMemoryRequirements();
        return DeviceMemory<T>(vkMemoryRequirements, requiredMemoryPropertyFlags);
    }

    void bindDeviceMemory(VkDeviceMemory vkDeviceMemory)
    {
        VkResult vkResult = vkBindBufferMemory(vkDevice(), *this, vkDeviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }
};

template <typename T = uint8_t>
class Buffer_DeviceMemory {

    Buffer<T> m_buffer;
    DeviceMemory<T> m_deviceMemory;
    TypedCount<T> m_count;
    T* m_pMappedMemory = nullptr;

public:
    Buffer_DeviceMemory() = default;
    ~Buffer_DeviceMemory() = default;

    Buffer_DeviceMemory(const Buffer_DeviceMemory& other)
        : m_buffer(other.m_buffer)
        , m_deviceMemory(other.m_deviceMemory)
        , m_count(other.m_count)
        , m_pMappedMemory(other.m_pMappedMemory)
    {
    }

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
        , m_count(other.m_count)
        , m_pMappedMemory(other.m_pMappedMemory)
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

    //	Most basic constructor.  Just takes existing buffer and memory.
    //	Used by other constructors and useful for porting.
    Buffer_DeviceMemory(Buffer<T>&& buffer, DeviceMemory<T>&& deviceMemory, TypedCount<T> count)
        : m_buffer(std::move(buffer))
        , m_deviceMemory(std::move(deviceMemory))
        , m_count(count)
        , m_pMappedMemory(nullptr)
    {
    }

    //	Creates Buffer, DeviceMemory, and binds them.
    Buffer_DeviceMemory(
        VkBufferUsageFlags vkBufferUsageFlags,
        TypedCount<T> count,
        uint32_t queueFamilyIndex,
        MemoryPropertyFlags requiredMemoryPropertyFlags)
    {
        Buffer<T> buffer(vkBufferUsageFlags, count.vkDeviceSize(), queueFamilyIndex);
        DeviceMemory<T> deviceMemory = buffer.allocateDeviceMemory(requiredMemoryPropertyFlags);

        VkResult vkResult = vkBindBufferMemory(vkDevice(), buffer, deviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        new (this) Buffer_DeviceMemory(std::move(buffer), std::move(deviceMemory), count);
    }

    static Buffer_DeviceMemory withMap(
        VkBufferUsageFlags vkBufferUsageFlags,
        TypedCount<T> count,
        uint32_t queueFamilyIndex,
        MemoryPropertyFlags requiredMemoryPropertyFlags)
    {
        Buffer_DeviceMemory newbdm(vkBufferUsageFlags, count, queueFamilyIndex, requiredMemoryPropertyFlags);
        void* mappedMemory;
        VkResult vkResult = vkMapMemory(vkDevice(), newbdm.m_deviceMemory, 0, newbdm.m_count.vkDeviceSize(), 0, &mappedMemory);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        newbdm.m_pMappedMemory = static_cast<T*>(mappedMemory);
        return newbdm;
    }

    static Buffer_DeviceMemory withCopyUnmap(
        VkBufferUsageFlags vkBufferUsageFlags,
        TypedCount<T> count,
        uint32_t queueFamilyIndex,
        MemoryPropertyFlags requiredMemoryPropertyFlags,
        const T* pSrcMem)
    {
        Buffer_DeviceMemory newbdm = withMap(
            vkBufferUsageFlags,
            count,
            queueFamilyIndex,
            requiredMemoryPropertyFlags);

        memcpy(newbdm.m_pMappedMemory, pSrcMem, newbdm.m_count.vkDeviceSize());
        newbdm.unmapMemory();
        return newbdm;
    }

    Buffer<T> buffer()
    {
        return m_buffer;
    }

    //	Useful when porting existing apps.  Make the full Buffer_DeviceMemory
    //	in steps.  Make the buffer and device memory with explicit bind and map steps
    //	and then do the full conversion.
    void bind()
    {
        VkResult vkResult = vkBindBufferMemory(vkDevice(), m_buffer, m_deviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    //	See bind()
    void mapMemory()
    {
        void* mappedMemory;
        VkResult vkResult = vkMapMemory(vkDevice(), m_deviceMemory, 0, m_count.vkDeviceSize(), 0, &mappedMemory);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        m_pMappedMemory = static_cast<T*>(mappedMemory);
    }

    T& mappedMemory()
    {
        //	TODO: do a null check and barf if necessary.
        return *m_pMappedMemory;
    }

    void unmapMemory()
    {
        if (!m_pMappedMemory) {
            throw NullPointerException();
        }
        vkUnmapMemory(vkDevice(), m_deviceMemory);
        m_pMappedMemory = nullptr;
    }
};

class ShaderModule : public InteropHandle2<VkShaderModule> {

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

    static void destroy(VkShaderModule vkShaderModule)
    {
        vkDestroyShaderModule(vkDevice(), vkShaderModule, nullptr);
    }

    ShaderModule(VkShaderModule vkShaderModule, DestroyFunc_t pfnDestroy)
        : InteropHandle2<VkShaderModule>(vkShaderModule, pfnDestroy)
    {
    }

public:
    ShaderModule() = default;
    ~ShaderModule() = default;

    ShaderModule(const ShaderModule& other)
        : InteropHandle2(other)
    {
    }

    ShaderModule& operator=(const ShaderModule& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~ShaderModule();
        new (this) ShaderModule(other);
        return *this;
    }

    ShaderModule(ShaderModule&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    ShaderModule& operator=(ShaderModule&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~ShaderModule();
        new (this) ShaderModule(std::move(other));
        return *this;
    }

    static ShaderModule createShaderModuleFromFile(const std::string& fileName)
    {
        auto fragShaderCode = readFile(fileName);
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = fragShaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
        VkShaderModule vkShaderModule;
        VkResult vkResult = vkCreateShaderModule(vkDevice(), &createInfo, nullptr, &vkShaderModule);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return ShaderModule(vkShaderModule, &destroy);
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

    ~AttachmentDescription() = default;
    AttachmentDescription(const AttachmentDescription&) = default;
    AttachmentDescription& operator=(const AttachmentDescription&) = default;
    AttachmentDescription(AttachmentDescription&&) noexcept = default;
    AttachmentDescription& operator=(AttachmentDescription&&) noexcept = default;

    AttachmentDescription(const VkAttachmentDescription& other)
        : VkAttachmentDescription(other)
    {
    }

    AttachmentDescription& operator=(const VkAttachmentDescription& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~AttachmentDescription(); //	probably not needed
        new (this) AttachmentDescription(other);
        return *this;
    }

    static AttachmentDescription simpleColorPresent(
        VkFormat vkFormatArg)
    {
        AttachmentDescription attachmentDescription;
        attachmentDescription.format = vkFormatArg;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.setLoadOpStoreOp(
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
        attachmentDescription.setInitialLayoutFinalLayout(
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        return attachmentDescription;
    }

    static AttachmentDescription simpleColor(
        VkFormat vkFormatArg)
    {
        //	TODO: is store op store the best choice?
        //	Is it necessary for the usual use case?
        //	Maybe don't care is more efficient for the usual case?
        AttachmentDescription attachmentDescription;
        attachmentDescription.format = vkFormatArg;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.setLoadOpStoreOp(
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
        attachmentDescription.setInitialLayoutFinalLayout(
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        return attachmentDescription;
    }

    static AttachmentDescription simpleDepthStencil(
        VkFormat vkFormatArg)
    {
        AttachmentDescription attachmentDescription;
        //	Reasonable defaults
        attachmentDescription.format = vkFormatArg;
        attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDescription.setLoadOpStoreOp(
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
        attachmentDescription.setStencilLoadOpStoreOp(
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
        attachmentDescription.setInitialLayoutFinalLayout(
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        return attachmentDescription;
    }

    AttachmentDescription& setFormat(VkFormat vkFormat)
    {
        format = vkFormat;
        return *this;
    }

    AttachmentDescription& setSamples(VkSampleCountFlagBits vkSampleCountFlagBits)
    {
        samples = vkSampleCountFlagBits;
        return *this;
    }

    AttachmentDescription& setLoadOpStoreOp(
        VkAttachmentLoadOp loadOpArg,
        VkAttachmentStoreOp storeOpArg)
    {
        loadOp = loadOpArg;
        storeOp = storeOpArg;
        return *this;
    }

    AttachmentDescription& setStencilLoadOpStoreOp(
        VkAttachmentLoadOp loadOpArg,
        VkAttachmentStoreOp storeOpArg)
    {
        stencilLoadOp = loadOpArg;
        stencilStoreOp = storeOpArg;
        return *this;
    }

    AttachmentDescription& setInitialLayoutFinalLayout(
        VkImageLayout initialLayoutArg,
        VkImageLayout finalLayoutArg)
    {
        initialLayout = initialLayoutArg;
        finalLayout = finalLayoutArg;
        return *this;
    }
};
static_assert(sizeof(AttachmentDescription) == sizeof(VkAttachmentDescription));

class SubpassDescription {

    //	Always up to date.
    //	TODO:	need to figure out which attachments to preserve.
    // preserveAttachmentCount = 2;
    // pPreserveAttachments = s_preserve.data();

    //	Use an internal Vulkan structure rather than subclass.
    VkSubpassDescription m_vkSubpassDescription {};

    //	There is a max of one depth stencil attachment, but it's easier
    //	to save it as a vector rather than special casing it.
    std::vector<VkAttachmentReference> m_inputAttachmentReferences;
    std::vector<VkAttachmentReference> m_colorAttachmentReferences;
    std::vector<VkAttachmentReference> m_depthStencilAttachmentReferences;

    void reassemble()
    {
        m_vkSubpassDescription.inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentReferences.size());
        m_vkSubpassDescription.pInputAttachments = m_inputAttachmentReferences.data();

        m_vkSubpassDescription.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
        m_vkSubpassDescription.pColorAttachments = m_colorAttachmentReferences.data();

        if (!m_depthStencilAttachmentReferences.empty()) {
            m_vkSubpassDescription.pDepthStencilAttachment = m_depthStencilAttachmentReferences.data();
        }
    }

public:
    SubpassDescription() = default;

    ~SubpassDescription() = default;

    SubpassDescription(const SubpassDescription& other)
        : m_vkSubpassDescription(other.m_vkSubpassDescription)
        , m_inputAttachmentReferences(other.m_inputAttachmentReferences)
        , m_colorAttachmentReferences(other.m_colorAttachmentReferences)
        , m_depthStencilAttachmentReferences(other.m_depthStencilAttachmentReferences)
    {
        reassemble();
    }

    SubpassDescription& operator=(const SubpassDescription& other)
    {
        this->~SubpassDescription();
        new (this) SubpassDescription(other);
        reassemble();
        return *this;
    }

    SubpassDescription(SubpassDescription&& other) noexcept
        : m_vkSubpassDescription(std::move(other.m_vkSubpassDescription))
        , m_inputAttachmentReferences(std::move(other.m_inputAttachmentReferences))
        , m_colorAttachmentReferences(std::move(other.m_colorAttachmentReferences))
        , m_depthStencilAttachmentReferences(std::move(other.m_depthStencilAttachmentReferences))
    {
        reassemble();
    }

    SubpassDescription& operator=(SubpassDescription&& other) noexcept
    {
        this->~SubpassDescription();
        new (this) SubpassDescription(std::move(other));
        reassemble();
        return *this;
    }

    SubpassDescription& setPipelineBindPoint(VkPipelineBindPoint vkPipelineBindPoint)
    {
        m_vkSubpassDescription.pipelineBindPoint = vkPipelineBindPoint;
        return *this;
    }

    SubpassDescription& addInputAttachmentReference(const VkAttachmentReference& vkAttachmentReference)
    {
        m_inputAttachmentReferences.emplace_back(vkAttachmentReference);
        m_vkSubpassDescription.inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentReferences.size());
        m_vkSubpassDescription.pInputAttachments = m_inputAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& addInputAttachmentReference(
        uint32_t attachmentIndexArg,
        VkImageLayout vkImageLayoutArg)
    {
        VkAttachmentReference vkAttachmentReference {
            .attachment = attachmentIndexArg,
            .layout = vkImageLayoutArg
        };

        m_inputAttachmentReferences.emplace_back(vkAttachmentReference);
        m_vkSubpassDescription.inputAttachmentCount = static_cast<uint32_t>(m_inputAttachmentReferences.size());
        m_vkSubpassDescription.pInputAttachments = m_inputAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& addColorAttachmentReference(const VkAttachmentReference& vkAttachmentReference)
    {
        m_colorAttachmentReferences.emplace_back(vkAttachmentReference);
        m_vkSubpassDescription.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
        m_vkSubpassDescription.pColorAttachments = m_colorAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& addColorAttachmentReference(
        uint32_t attachmentIndexArg,
        VkImageLayout vkImageLayoutArg)
    {
        VkAttachmentReference vkAttachmentReference {
            .attachment = attachmentIndexArg,
            .layout = vkImageLayoutArg
        };

        m_colorAttachmentReferences.emplace_back(vkAttachmentReference);
        m_vkSubpassDescription.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
        m_vkSubpassDescription.pColorAttachments = m_colorAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& setDepthStencilAttachmentReference(const VkAttachmentReference& vkDepthStencilAttachmentReference)
    {
        m_depthStencilAttachmentReferences.emplace_back(vkDepthStencilAttachmentReference);
        m_vkSubpassDescription.pDepthStencilAttachment = m_depthStencilAttachmentReferences.data();
        return *this;
    }

    SubpassDescription& setDepthStencilAttachmentReference(
        uint32_t attachmentIndexArg,
        VkImageLayout vkImageLayoutArg)
    {
        VkAttachmentReference vkAttachmentReference {
            .attachment = attachmentIndexArg,
            .layout = vkImageLayoutArg
        };
        //	TODO: error check for multiple calls. Only one depth/stencil allowed.
        m_depthStencilAttachmentReferences.emplace_back(vkAttachmentReference);
        m_vkSubpassDescription.pDepthStencilAttachment = m_depthStencilAttachmentReferences.data();
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

    SubpassDependency(const VkSubpassDependency& other)
        : VkSubpassDependency(other)
    {
    }

    SubpassDependency& setSubpassDependency(
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

    SubpassDependency& setDependencyFlags(
        VkDependencyFlags dependencyFlagsArg)
    {
        dependencyFlags = dependencyFlagsArg;
        return *this;
    }
};
//	We use arrays of SubpassDependency to be arrays of VkSubpassDependency
//	in RenderPassCreateInfo, so they need to be the same size.
static_assert(sizeof(SubpassDependency) == sizeof(VkSubpassDependency));

class RenderPassCreateInfo {

    //	Attachments are referenced by an index number after being
    //	added to the render pass.  The application knows the
    //	number of attachments beforehand and uses the index number
    //	to refer to the attachments.  Because of this, we make the
    //	application tell us the number of attachments so that we can
    //	put them in the proper position in the vector.  Otherwise,
    //	we would need to keep track of the index, tell the application
    //	the index, have the application keep track of the index, etc.
    //	That get way too confusing for the application.

    VkRenderPassCreateInfo m_vkRenderPassCreateInfo {};

    std::vector<AttachmentDescription> m_attachmentDescriptions;

    std::vector<SubpassDescription> m_subpassDescriptions;
    std::vector<VkSubpassDescription> m_vkSubpassDescriptions;

    //	SubpassDependency is the same size as VkSubpassDependency,
    //	so we can get by with just a vector of SubpassDependency.
    std::vector<SubpassDependency> m_subpassDependencies;

public:
    ~RenderPassCreateInfo() = default;

    RenderPassCreateInfo(const RenderPassCreateInfo&) = delete;
    RenderPassCreateInfo& operator=(const RenderPassCreateInfo&) = delete;
    RenderPassCreateInfo(RenderPassCreateInfo&&) noexcept = delete;
    RenderPassCreateInfo& operator=(RenderPassCreateInfo&&) noexcept = delete;

    RenderPassCreateInfo(int subpassCountArg, int attachmentCountArg)
    {
        m_subpassDescriptions.resize(subpassCountArg);
        m_attachmentDescriptions.resize(attachmentCountArg);

        //	Watch out for name collisions between args and member variables.
        m_vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        m_vkRenderPassCreateInfo.attachmentCount = static_cast<uint32_t>(m_attachmentDescriptions.size());
        m_vkRenderPassCreateInfo.pAttachments = m_attachmentDescriptions.data();
    }

    const VkRenderPassCreateInfo* operator&() = delete;

    void addAttachmentDescription(
        int attachmentIndex,
        const AttachmentDescription& attachmentDescription)
    {
        m_attachmentDescriptions.at(attachmentIndex) = attachmentDescription;
    }

    AttachmentDescription& attachmentDescription(int index)
    {
        return m_attachmentDescriptions.at(index);
    }

    SubpassDescription& subpassDescription(int index)
    {
        SubpassDescription& subpassDescription = m_subpassDescriptions.at(index);
        return subpassDescription;
    }

    //	Useful for migrating existing code.  This is a shallow copy
    //	so the RenderPassCreateInfo is only valid as long as the original
    //	VkSubpassDependency is valid.
    void addSubpassDependency(const VkSubpassDependency& vkSubpassDependency)
    {
        m_subpassDependencies.emplace_back(vkSubpassDependency);
        m_vkRenderPassCreateInfo.dependencyCount = static_cast<uint32_t>(m_subpassDependencies.size());
        m_vkRenderPassCreateInfo.pDependencies = m_subpassDependencies.data();
    }

    //	Note that this returns a reference to the newly created
    //	subpass dependency, not the RenderPassCreateInfo.  This
    //	allows adding the dependency and then adding the rest of
    //	the dependency data.
    SubpassDependency& addSubpassDependency(
        uint32_t srcSubpassArg,
        uint32_t dstSubpassArg)
    {
        SubpassDependency& subpassDependency = m_subpassDependencies.emplace_back();
        m_vkRenderPassCreateInfo.dependencyCount = static_cast<uint32_t>(m_subpassDependencies.size());
        m_vkRenderPassCreateInfo.pDependencies = m_subpassDependencies.data();
        subpassDependency.setSubpassDependency(srcSubpassArg, dstSubpassArg);
        return subpassDependency;
    }

    VkRenderPassCreateInfo* assemble()
    {
        m_vkRenderPassCreateInfo.pSubpasses = nullptr;
        m_vkSubpassDescriptions.clear();
        m_vkRenderPassCreateInfo.subpassCount = static_cast<uint32_t>(m_subpassDescriptions.size());
        if (m_vkRenderPassCreateInfo.subpassCount > 0) {
            for (SubpassDescription& subpassDescription : m_subpassDescriptions) {
                //	TODO: investigate.  This is either really clever or really risky.
                //	Since the subpass description is always up to date, we just
                //  copy the vkSubpassDescription part into the array of subpass descriptions.
                m_vkSubpassDescriptions.emplace_back(subpassDescription.vkSubpassDescription());
            }
            m_vkRenderPassCreateInfo.pSubpasses = m_vkSubpassDescriptions.data();
        }

        return &m_vkRenderPassCreateInfo;
    }
};

class RenderPass : public InteropHandle2<VkRenderPass> {

    RenderPass(VkRenderPass vkRenderPass, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkRenderPass, pfnDestroy)
    {
    }

    static void destroy(VkRenderPass vkRenderPass)
    {
        vkDestroyRenderPass(vkDevice(), vkRenderPass, nullptr);
    }

public:
    RenderPass() = default;
    ~RenderPass() = default;
    RenderPass(const RenderPass& other)
        : InteropHandle2(other)
    {
    }
    RenderPass& operator=(const RenderPass& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~RenderPass();
        new (this) RenderPass(other);
        return *this;
    }
    RenderPass(RenderPass&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }
    RenderPass& operator=(RenderPass&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~RenderPass();
        new (this) RenderPass(std::move(other));
        return *this;
    }

    RenderPass(RenderPassCreateInfo& renderPassCreateInfo)
    {
        VkRenderPass vkRenderPass;
        VkResult vkResult = vkCreateRenderPass(vkDevice(), renderPassCreateInfo.assemble(), nullptr, &vkRenderPass);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) RenderPass(vkRenderPass, &destroy);
    }

    RenderPass(const VkRenderPassCreateInfo& vkRenderPassCreateInfo)
    {
        VkRenderPass vkRenderPass;
        VkResult vkResult = vkCreateRenderPass(vkDevice(), &vkRenderPassCreateInfo, nullptr, &vkRenderPass);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) RenderPass(vkRenderPass, &destroy);
    }
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

class Image : public InteropHandle2<VkImage> {

    Image(VkImage vkImage, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkImage, pfnDestroy)
    {
    }

    static void destroy(VkImage vkImage)
    {
        vkDestroyImage(vkDevice(), vkImage, nullptr);
    }

public:
    Image() = default;
    ~Image() = default;
    Image(const Image& other)
        : InteropHandle2(other)
    {
    }

    Image& operator=(const Image& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Image();
        new (this) Image(other);
        return *this;
    }

    Image(Image&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Image& operator=(Image&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Image();
        new (this) Image(std::move(other));
        return *this;
    }

    Image(const ImageCreateInfo& imageCreateInfo)
    {
        VkImage vkImage;
        VkResult vkResult = vkCreateImage(device(), &imageCreateInfo, nullptr, &vkImage);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Image(vkImage, &destroy);
    }

    Image(const VkImageCreateInfo& vkImageCreateInfo)
    {
        VkImage vkImage;
        VkResult vkResult = vkCreateImage(device(), &vkImageCreateInfo, nullptr, &vkImage);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Image(vkImage, &destroy);
    }

    VkMemoryRequirements getMemoryRequirements() const
    {
        VkMemoryRequirements vkMemoryRequirements;
        vkGetImageMemoryRequirements(vkDevice(), *this, &vkMemoryRequirements);
        return vkMemoryRequirements;
    }

    DeviceMemory<> allocateDeviceMemory(MemoryPropertyFlags requiredProperties) const
    {
        VkMemoryRequirements vkMemoryRequirements = getMemoryRequirements();
        VkMemoryAllocateInfo vkMemoryAllocateInfo {};
        vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
        vkMemoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, requiredProperties);
        return vkcpp::DeviceMemory(vkMemoryAllocateInfo);
    }

    void bindImageMemory(VkDeviceMemory vkDeviceMemory) const
    {
        VkResult vkResult = vkBindImageMemory(vkDevice(), *this, vkDeviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }
};

class ImageViewCreateInfo : public VkImageViewCreateInfo {

public:
    ImageViewCreateInfo()
        : VkImageViewCreateInfo {}
    {
    }
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

class ImageView : public InteropHandle2<VkImageView> {

    ImageView(VkImageView vkImageView, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkImageView, pfnDestroy)
    {
    }

    static void destroy(VkImageView vkImageView)
    {
        vkDestroyImageView(vkDevice(), vkImageView, nullptr);
    }

public:
    //	TODO: need to start remembering some info about the m_vkImage and how the
    //	imageview was created to make things easier to use later.
    ImageView() = default;
    ~ImageView() = default;

    ImageView(const ImageView& other)
        : InteropHandle2(other)
    {
    }

    ImageView& operator=(const ImageView& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~ImageView();
        new (this) ImageView(other);
        return *this;
    }

    ImageView(ImageView&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    ImageView& operator=(ImageView&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~ImageView();
        new (this) ImageView(std::move(other));
        return *this;
    }

    ImageView(const VkImageViewCreateInfo& vkImageViewCreateInfo)
    {
        VkImageView vkImageView;
        VkResult vkResult = vkCreateImageView(vkDevice(), &vkImageViewCreateInfo, nullptr, &vkImageView);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) ImageView(vkImageView, &destroy);
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

class Sampler : public InteropHandle2<VkSampler> {

    Sampler(VkSampler sampler, DestroyFunc_t pfnDestroy)
        : InteropHandle2(sampler, pfnDestroy)
    {
    }

    static void destroy(VkSampler sampler)
    {
        vkDestroySampler(vkDevice(), sampler, nullptr);
    }

public:
    Sampler() = default;
    ~Sampler() = default;

    Sampler(const Sampler& other)
        : InteropHandle2(other)
    {
    }

    Sampler& operator=(const Sampler& other)
    {
        this->~Sampler();
        new (this) Sampler(other);
        return *this;
    }

    Sampler(Sampler&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Sampler& operator=(Sampler&& other) noexcept
    {
        this->~Sampler();
        new (this) Sampler(std::move(other));
        return *this;
    }

    Sampler(const SamplerCreateInfo& samplerCreateInfo)
    {
        VkSampler vkSampler;
        VkResult vkResult = vkCreateSampler(device(), &samplerCreateInfo, nullptr, &vkSampler);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Sampler(vkSampler, &destroy);
    }

    Sampler(const VkSamplerCreateInfo& vkSamplerCreateInfo)
    {
        VkSampler vkSampler;
        VkResult vkResult = vkCreateSampler(device(), &vkSamplerCreateInfo, nullptr, &vkSampler);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Sampler(vkSampler, &destroy);
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

        //	If the m_vkImage m_vkDeviceMemory barrier is to be used in a sequence
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

    DependencyInfo()
        : VkDependencyInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    }

    void addImageMemoryBarrier(const ImageMemoryBarrier2& imageMemoryBarrier)
    {
        m_imageMemoryBarriers.push_back(imageMemoryBarrier);
        imageMemoryBarrierCount = static_cast<uint32_t>(m_imageMemoryBarriers.size());
        pImageMemoryBarriers = m_imageMemoryBarriers.data();
    }

    void addMemoryBarrier(const MemoryBarrier2& memoryBarrier)
    {
        m_memoryBarriers.push_back(memoryBarrier);
        memoryBarrierCount = static_cast<uint32_t>(m_memoryBarriers.size());
        pMemoryBarriers = m_memoryBarriers.data();
    }
};

class CommandPoolCreateInfo : public VkCommandPoolCreateInfo {

public:
    CommandPoolCreateInfo()
        : VkCommandPoolCreateInfo {}
    {
        VkCommandPoolCreateInfo cmdPoolInfo = {};
        sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    }

    CommandPoolCreateInfo& setFlags(VkCommandPoolCreateFlagBits flagsArg)
    {
        flags = flagsArg;
        return *this;
    }

    CommandPoolCreateInfo& setQueueFamilyIndex(uint32_t queueFamilyIndexArg)
    {
        queueFamilyIndex = queueFamilyIndexArg;
        return *this;
    }
};

class CommandBufferAllocateInfo : public VkCommandBufferAllocateInfo {
public:
    CommandBufferAllocateInfo()
        : VkCommandBufferAllocateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    }

    CommandBufferAllocateInfo& setCount(uint32_t commandBufferCountArg)
    {
        commandBufferCount = commandBufferCountArg;
        return *this;
    }

    CommandBufferAllocateInfo& setCommandPool(VkCommandPool commandPoolArg)
    {
        commandPool = commandPoolArg;
        return *this;
    }
};

//	TODO: should we make some object that has contains a m_vkQueue and command pool
//	and whatever else needed so we don't have to pass the info around as pairs?
class CommandPool : public InteropHandle2<VkCommandPool> {

    CommandPool(VkCommandPool vkCommandPool, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkCommandPool, pfnDestroy)
    {
    }

    static void destroy(VkCommandPool vkCommandPool)
    {
        vkDestroyCommandPool(vkDevice(), vkCommandPool, nullptr);
    }

public:
    CommandPool() = default;
    ~CommandPool() = default;

    CommandPool(const CommandPool& other)
        : InteropHandle2(other)
    {
    }

    CommandPool& operator=(const CommandPool& other)
    {
        if (this == &other) {
            return *this;
        }

        this->~CommandPool();
        new (this) CommandPool(other);
        return *this;
    }

    CommandPool(CommandPool&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    CommandPool& operator=(CommandPool&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        this->~CommandPool();
        new (this) CommandPool(std::move(other));
        return *this;
    }

    CommandPool(const VkCommandPoolCreateInfo& commandPoolCreateInfo)
    {
        VkCommandPool vkCommandPool;
        VkResult vkResult = vkCreateCommandPool(
            vkDevice(),
            &commandPoolCreateInfo,
            nullptr,
            &vkCommandPool);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) CommandPool(vkCommandPool, &destroy);
    }

    CommandPool(
        uint32_t queueFamilyIndex,
        VkCommandPoolCreateFlags vkCommandPoolCreateFlags)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo {};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = vkCommandPoolCreateFlags;
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        new (this) CommandPool(commandPoolCreateInfo);
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

    ImageMemoryBarrier& setSrcDstQueueFamilyIndex(
        uint32_t srcQueueFamilyIndexArg,
        uint32_t dstQueueFamilyIndexArg)
    {
        srcQueueFamilyIndex = srcQueueFamilyIndexArg;
        dstQueueFamilyIndex = dstQueueFamilyIndexArg;
        return *this;
    }

    ImageMemoryBarrier& setSrcDstAccessMask(
        VkAccessFlagBits srcAccessMaskArg,
        VkAccessFlagBits dstAccessMaskArg)
    {
        srcAccessMask = srcAccessMaskArg;
        dstAccessMask = dstAccessMaskArg;
        return *this;
    }

    ImageMemoryBarrier& setOldNewImageLayout(
        VkImageLayout oldImageLayoutArg,
        VkImageLayout newImageLayoutArg)
    {
        oldLayout = oldImageLayoutArg;
        newLayout = newImageLayoutArg;
        return *this;
    }

    ImageMemoryBarrier& setImage(VkImage imageArg)
    {
        image = imageArg;
        return *this;
    }

    ImageMemoryBarrier& setSubresourceRange(
        const VkImageSubresourceRange& vkImageSubresourceRangeArg)
    {
        subresourceRange = vkImageSubresourceRangeArg;
        return *this;
    }
};

class BufferImageCopy : public VkBufferImageCopy {

    //	TODO: need something to hold multiple of these for
    //	complicated copy operations.

public:
    BufferImageCopy()
        : VkBufferImageCopy {}
    {
    }
};
static_assert(sizeof(BufferImageCopy) == sizeof(VkBufferImageCopy));

class SubmitInfo : public VkSubmitInfo {

    //	Always up to date.
    std::vector<VkSemaphore> m_vkWaitSemaphores;
    std::vector<VkPipelineStageFlags> m_vkPipelineStateFlags;
    std::vector<VkCommandBuffer> m_vkCommandBuffers;
    std::vector<VkSemaphore> m_vkSignalSemaphores;

public:
    SubmitInfo()
        : VkSubmitInfo {}
    {
        sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    }
    ~SubmitInfo() = default;
    SubmitInfo(const SubmitInfo&) = delete;
    SubmitInfo& operator=(const SubmitInfo&) = delete;
    SubmitInfo(SubmitInfo&&) noexcept = delete;
    SubmitInfo& operator=(SubmitInfo&&) noexcept = delete;

    void addCommandBuffer(VkCommandBuffer vkCommandBuffer)
    {
        m_vkCommandBuffers.emplace_back(vkCommandBuffer);
        commandBufferCount = static_cast<uint32_t>(m_vkCommandBuffers.size());
        pCommandBuffers = m_vkCommandBuffers.data();
    }

    void addWaitSemaphore(
        VkSemaphore vkSemaphore,
        VkPipelineStageFlags vkPipelineStateFlags)
    {
        m_vkWaitSemaphores.emplace_back(vkSemaphore);
        m_vkPipelineStateFlags.emplace_back(vkPipelineStateFlags);
        waitSemaphoreCount = static_cast<uint32_t>(m_vkWaitSemaphores.size());
        pWaitSemaphores = m_vkWaitSemaphores.data();
        pWaitDstStageMask = m_vkPipelineStateFlags.data();
    }

    void addSignalSemaphore(VkSemaphore vkSemaphore)
    {
        m_vkSignalSemaphores.emplace_back(vkSemaphore);
        signalSemaphoreCount = static_cast<uint32_t>(m_vkSignalSemaphores.size());
        pSignalSemaphores = m_vkSignalSemaphores.data();
    }
};

class SubmitInfo2 : public VkSubmitInfo2 {

    //	Always up to date.
    std::vector<VkSemaphoreSubmitInfo> m_waitSemaphoreInfos;
    std::vector<VkCommandBufferSubmitInfo> m_commandBufferInfos;
    std::vector<VkSemaphoreSubmitInfo> m_signalSemaphoreInfos;

public:
    SubmitInfo2()
        : VkSubmitInfo2 {}
    {
        sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    }
    ~SubmitInfo2() = default;
    SubmitInfo2(const SubmitInfo2&) = delete;
    SubmitInfo2& operator=(const SubmitInfo2&) = delete;
    SubmitInfo2(SubmitInfo2&&) noexcept = delete;
    SubmitInfo2& operator=(SubmitInfo2&&) noexcept = delete;

    SubmitInfo2& addCommandBuffer(VkCommandBuffer vkCommandBuffer)
    {
        VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo {};
        vkCommandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        vkCommandBufferSubmitInfo.commandBuffer = vkCommandBuffer;
        m_commandBufferInfos.emplace_back(vkCommandBufferSubmitInfo);
        commandBufferInfoCount = static_cast<uint32_t>(m_commandBufferInfos.size());
        pCommandBufferInfos = m_commandBufferInfos.data();
        return *this;
    }

    SubmitInfo2& addWaitSemaphore(
        VkSemaphore vkSemaphore,
        PipelineStageFlags2 waitPipelineStateFlags2)
    {
        VkSemaphoreSubmitInfo vkSemaphoreSubmitInfo {};
        vkSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        vkSemaphoreSubmitInfo.semaphore = vkSemaphore;
        vkSemaphoreSubmitInfo.stageMask = static_cast<VkPipelineStageFlagBits2>(waitPipelineStateFlags2);
        m_waitSemaphoreInfos.emplace_back(vkSemaphoreSubmitInfo);
        waitSemaphoreInfoCount = static_cast<uint32_t>(m_waitSemaphoreInfos.size());
        pWaitSemaphoreInfos = m_waitSemaphoreInfos.data();
        return *this;
    }

    void addSignalSemaphore(VkSemaphore vkSemaphore)
    {
        VkSemaphoreSubmitInfo vkSemaphoreSubmitInfo {};
        vkSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        vkSemaphoreSubmitInfo.semaphore = vkSemaphore;
        m_signalSemaphoreInfos.emplace_back(vkSemaphoreSubmitInfo);
        signalSemaphoreInfoCount = static_cast<uint32_t>(m_signalSemaphoreInfos.size());
        pSignalSemaphoreInfos = m_signalSemaphoreInfos.data();
    }
};

class PresentInfo : public VkPresentInfoKHR {

    std::vector<VkSemaphore> m_vkSemaphoreWaits;
    std::vector<VkSwapchainKHR> m_vkSwapChains;
    std::vector<uint32_t> m_swapChainImageIndices;

public:
    PresentInfo()
        : VkPresentInfoKHR {}
    {
        sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    }

    ~PresentInfo() = default;
    PresentInfo(const PresentInfo&) = delete;
    PresentInfo& operator=(const PresentInfo&) = delete;
    PresentInfo(PresentInfo&&) noexcept = delete;
    PresentInfo& operator=(PresentInfo&&) noexcept = delete;

    void addWaitSemaphore(VkSemaphore vkSemaphore)
    {
        m_vkSemaphoreWaits.push_back(vkSemaphore);
        waitSemaphoreCount = static_cast<uint32_t>(m_vkSemaphoreWaits.size());
        pWaitSemaphores = m_vkSemaphoreWaits.data();
    }

    void addSwapchain(
        VkSwapchainKHR vkSwapChain,
        uint32_t swapChainImageIndex)
    {
        m_vkSwapChains.push_back(vkSwapChain);
        m_swapChainImageIndices.push_back(swapChainImageIndex);
        swapchainCount = static_cast<uint32_t>(m_vkSwapChains.size());
        pSwapchains = m_vkSwapChains.data();
        pImageIndices = m_swapChainImageIndices.data();
    }
};

class Queue : public InteropHandle2<VkQueue> {

public:
    uint32_t m_queueFamilyIndex = 0;

    Queue() = default;
    ~Queue() = default;

    Queue(const Queue& other)
        : InteropHandle2(other)
    {
    }

    Queue& operator=(const Queue& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Queue();
        new (this) Queue(other);
        return *this;
    }

    Queue(Queue&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    Queue& operator=(Queue&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Queue();
        new (this) Queue(std::move(other));
        return *this;
    }

    //	Queues always come from the m_vkDevice and are never actually destroyed
    Queue(VkQueue vkQueue, uint32_t queueFamilyIndex)
        : InteropHandle2(vkQueue, nullptr)
        , m_queueFamilyIndex(queueFamilyIndex)
    {
    }

    static Queue makeCopy(VkQueue vkQueue)
    {
        return Queue(vkQueue, -1);
    }

    void waitIdle() const
    {
        vkQueueWaitIdle(*this);
    }

    // void submit(const SubmitInfo& submitInfo, VkFence vkFence) const
    //{
    //     VkResult vkResult = vkQueueSubmit(*this, 1, &submitInfo, vkFence);
    //     if (vkResult != VK_SUCCESS) {
    //         throw Exception(vkResult);
    //     }
    // }

    void submit(const VkSubmitInfo& vkSubmitInfo, VkFence vkFence) const
    {
        VkResult vkResult = vkQueueSubmit(*this, 1, &vkSubmitInfo, vkFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    // void submit(const VkSubmitInfo& vkSubmitInfo, VkFence vkFence) const
    //{
    //     VkResult vkResult = vkQueueSubmit(*this, 1, &vkSubmitInfo, vkFence);
    //     if (vkResult != VK_SUCCESS) {
    //         throw Exception(vkResult);
    //     }
    // }

    void submit2(VkCommandBuffer vkCommandBuffer) const
    {
        SubmitInfo2 submitInfo2;
        submitInfo2.addCommandBuffer(vkCommandBuffer);
        VkResult vkResult = vkQueueSubmit2(*this, 1, &submitInfo2, VK_NULL_HANDLE);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2(VkCommandBuffer vkCommandBuffer, VkFence vkFence) const
    {
        SubmitInfo2 submitInfo2;
        submitInfo2.addCommandBuffer(vkCommandBuffer);
        VkResult vkResult = vkQueueSubmit2(*this, 1, &submitInfo2, vkFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2(const VkSubmitInfo2& vkSubmitInfo2, VkFence vkFence) const
    {
        VkResult vkResult = vkQueueSubmit2(*this, 1, &vkSubmitInfo2, vkFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2(const VkSubmitInfo2& vkSubmitInfo2) const
    {
        VkResult vkResult = vkQueueSubmit2(*this, 1, &vkSubmitInfo2, VK_NULL_HANDLE);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
    }

    void submit2Fenced(VkCommandBuffer vkCommandBuffer) const
    {
        SubmitInfo2 submitInfo2;
        submitInfo2.addCommandBuffer(vkCommandBuffer);
        vkcpp::Fence completedFence;
        VkResult vkResult = vkQueueSubmit2(*this, 1, &submitInfo2, completedFence);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        completedFence.wait();
    }

    VkResult present(const PresentInfo& presentInfo) const
    {
        VkResult vkResult = vkQueuePresentKHR(*this, &presentInfo);
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

    //	Always up to date.
    std::vector<VkDescriptorPoolSize> m_vkDescriptorPoolSizes;

public:
    DescriptorPoolCreateInfo()
        : VkDescriptorPoolCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        maxSets = 1;
    }

    ~DescriptorPoolCreateInfo() = default;
    DescriptorPoolCreateInfo(const DescriptorPoolCreateInfo&) = delete;
    DescriptorPoolCreateInfo& operator=(const DescriptorPoolCreateInfo&) = delete;
    DescriptorPoolCreateInfo(DescriptorPoolCreateInfo&&) noexcept = delete;
    DescriptorPoolCreateInfo& operator=(DescriptorPoolCreateInfo&&) noexcept = delete;

    DescriptorPoolCreateInfo& addDescriptorCount(VkDescriptorType vkDescriptorType, uint32_t count)
    {
        VkDescriptorPoolSize vkDescriptorPoolSize;
        vkDescriptorPoolSize.type = vkDescriptorType;
        vkDescriptorPoolSize.descriptorCount = count;
        m_vkDescriptorPoolSizes.emplace_back(vkDescriptorPoolSize);
        poolSizeCount = static_cast<uint32_t>(m_vkDescriptorPoolSizes.size());
        pPoolSizes = m_vkDescriptorPoolSizes.data();
        return *this;
    }

    DescriptorPoolCreateInfo& setMaxSets(uint32_t maxSetsArg)
    {
        maxSets = maxSetsArg;
        return *this;
    }
};

class DescriptorPool : public InteropHandle2<VkDescriptorPool> {

    DescriptorPool(VkDescriptorPool vkDescriptorPool, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkDescriptorPool, pfnDestroy)
    {
    }

    static void destroy(VkDescriptorPool vkDescriptorPool)
    {
        vkDestroyDescriptorPool(vkDevice(), vkDescriptorPool, nullptr);
    }

public:
    DescriptorPool() = default;
    ~DescriptorPool() = default;

    DescriptorPool(const DescriptorPool& other)
        : InteropHandle2(other)
    {
    }

    DescriptorPool& operator=(const DescriptorPool& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~DescriptorPool();
        new (this) DescriptorPool(other);
        return *this;
    }

    DescriptorPool(DescriptorPool&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    DescriptorPool& operator=(DescriptorPool&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~DescriptorPool();
        new (this) DescriptorPool(std::move(other));
        return *this;
    }

    DescriptorPool(const DescriptorPoolCreateInfo& descriptorPoolCreateInfo)
    {
        VkDescriptorPool vkDescriptorPool;
        VkResult vkResult = vkCreateDescriptorPool(vkDevice(), &descriptorPoolCreateInfo, nullptr, &vkDescriptorPool);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorPool(vkDescriptorPool, &destroy);
    }

    DescriptorPool(const VkDescriptorPoolCreateInfo& vkDescriptorPoolCreateInfo)
    {
        VkDescriptorPool vkDescriptorPool;
        VkResult vkResult = vkCreateDescriptorPool(vkDevice(), &vkDescriptorPoolCreateInfo, nullptr, &vkDescriptorPool);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorPool(vkDescriptorPool, &destroy);
    }
};

struct DescriptorSetLayoutBinding {
    int m_bindingIndex;
    VkDescriptorType m_vkDescriptorType;
    vkcpp::ShaderStageFlags m_shaderStageFlags;
};

class DescriptorSetLayoutCreateInfo : public VkDescriptorSetLayoutCreateInfo {

    //	Always up to date.

    //	TODO: not doing anything with VkDescriptorSetLayoutCreateFlags yet.

    std::vector<VkDescriptorSetLayoutBinding> m_bindings;

public:
    ~DescriptorSetLayoutCreateInfo() = default;
    DescriptorSetLayoutCreateInfo(const DescriptorSetLayoutCreateInfo&) = delete;
    DescriptorSetLayoutCreateInfo& operator=(const DescriptorSetLayoutCreateInfo&) = delete;
    DescriptorSetLayoutCreateInfo(DescriptorSetLayoutCreateInfo&&) noexcept = delete;
    DescriptorSetLayoutCreateInfo& operator=(DescriptorSetLayoutCreateInfo&&) noexcept = delete;

    DescriptorSetLayoutCreateInfo()
        : VkDescriptorSetLayoutCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    }

    // DescriptorSetLayoutCreateInfo(
    //     std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings)
    //     : VkDescriptorSetLayoutCreateInfo {}
    //{
    //     sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //     addDescriptorSetLayoutBindings(descriptorSetLayoutBindings);
    // }

    DescriptorSetLayoutCreateInfo& addDescriptorSetLayoutBinding(
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
        m_bindings.push_back(layoutBinding);
        bindingCount = static_cast<uint32_t>(m_bindings.size());
        pBindings = m_bindings.data();

        return *this;
    }

    // void addDescriptorSetLayoutBindings(
    //     std::vector<DescriptorSetLayoutBinding>& descriptorSetLayoutBindings)
    //{
    //     for (DescriptorSetLayoutBinding& descriptorSetLayoutBinding : descriptorSetLayoutBindings) {
    //         addDescriptorSetLayoutBinding(
    //             descriptorSetLayoutBinding.m_bindingIndex,
    //             descriptorSetLayoutBinding.m_vkDescriptorType,
    //             descriptorSetLayoutBinding.m_shaderStageFlags);
    //     }
    // }
};

class DescriptorSetLayout : public InteropHandle2<VkDescriptorSetLayout> {

    DescriptorSetLayout(
        VkDescriptorSetLayout vkDescriptorSetLayout,
        VkDevice vkDevice,
        DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkDescriptorSetLayout, pfnDestroy)
    {
    }

    static void destroy(VkDescriptorSetLayout vkDescriptorSetLayout)
    {
        vkDestroyDescriptorSetLayout(vkDevice(), vkDescriptorSetLayout, nullptr);
    }

public:
    DescriptorSetLayout() = default;
    ~DescriptorSetLayout() = default;

    DescriptorSetLayout(const DescriptorSetLayout& other)
        : InteropHandle2(other)
    {
    }

    DescriptorSetLayout& operator=(const DescriptorSetLayout& other)
    {
        this->~DescriptorSetLayout();
        new (this) DescriptorSetLayout(other);
        return *this;
    }

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept
    {
        this->~DescriptorSetLayout();
        new (this) DescriptorSetLayout(std::move(other));
        return *this;
    }

    DescriptorSetLayout(
        const DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo)
    {
        VkDescriptorSetLayout vkDescriptorSetLayout;
        VkResult vkResult = vkCreateDescriptorSetLayout(
            vkDevice(),
            &descriptorSetLayoutCreateInfo,
            nullptr,
            &vkDescriptorSetLayout);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorSetLayout(vkDescriptorSetLayout, vkDevice(), &destroy);
    }

    DescriptorSetLayout(
        const VkDescriptorSetLayoutCreateInfo& vkDescriptorSetLayoutCreateInfo)
    {
        VkDescriptorSetLayout vkDescriptorSetLayout;
        VkResult vkResult = vkCreateDescriptorSetLayout(
            vkDevice(),
            &vkDescriptorSetLayoutCreateInfo,
            nullptr,
            &vkDescriptorSetLayout);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorSetLayout(vkDescriptorSetLayout, vkDevice(), &destroy);
    }
};

//	Hold on.  This gets complicated.
//	This is the smart version of VkWriteDescriptor set.
//	The name is a bit misleading.  Updating descriptor sets and
//	push descriptors use arrays of these.  So it really a write
//	array entry. That said, it's a bit complicated itself.
//
//	The VkWriteDescriptor part require specifying the descriptor set number,
//	binding index, and an array (or partial array) of element. A single
//	resource update looks like an array of one element.
//
//	An entry can update only one descriptor type per entry.
//
//	In theory, one array of these structures could update anything
//	and everything using vkWriteDescriptorSets.  In practice, at least
//	in the demos, updates are pretty simple.  There is usually only one
//	descriptor set, and each entry updates only one resource rather than
//	using the count/array aspect of the structure.  Also, the push
//	descriptor set command can only handle one descriptor set (number)
//	at a time.
//
//	With all this in mind, the following restrictions apply.  Full
//	array functionality is not supported (yet) but it should be easy
//	to add since the data is already stored in a vector.
//
//	The structure is kept up to date, but because of the vectors,
//	it's not a drop in replacement for the raw vk structure. The
//	WriteDescriptorSetArray handles the complexities of creating
//	an array of actual VkWriteDescriptorSet structures for use
//	when writing a descriptor set, or using push descriptors.
//
//	The write descriptor set array will handle the complexities of
//	constructing the "real" VkWriteDescriptorSet array for use in
//	Vulkan functions.
//
//	The common usage (so far) seems to be to update a single descriptor
//	set at a time, and push descriptor sets use only one descriptor set
//	per call. Because of this, and in order to simplify the interface,
//	the descriptor set array will use, and remember, only one descriptor
//	set at a time. Using multiple descriptor sets in one array will require
//	a separate call on the array object to change the descriptor set before
//	adding more descriptors.
class WriteDescriptorSet : public VkWriteDescriptorSet {

    std::vector<VkDescriptorBufferInfo> m_vkDescriptorBufferInfos;
    std::vector<VkDescriptorImageInfo> m_vkDescriptorImageInfos;
    std::vector<VkBufferView> m_vkBufferViewTexels;

    void reassemble()
    {
        if (!m_vkDescriptorBufferInfos.empty()) {
            pBufferInfo = m_vkDescriptorBufferInfos.data();
            return;
        }

        if (!m_vkDescriptorImageInfos.empty()) {
            pImageInfo = m_vkDescriptorImageInfos.data();
            return;
        }

        if (!m_vkBufferViewTexels.empty()) {
            pTexelBufferView = m_vkBufferViewTexels.data();
            return;
        }
    }

public:
    ~WriteDescriptorSet() = default;
    WriteDescriptorSet(const WriteDescriptorSet&) = delete;
    WriteDescriptorSet& operator=(const WriteDescriptorSet&) = delete;

    //	Need at least the move constructor so that WriteDescriptorSetArray
    //	can put them into a vector.
    WriteDescriptorSet(WriteDescriptorSet&& other) noexcept
        : VkWriteDescriptorSet(other)
    {
        m_vkDescriptorBufferInfos = std::move(other.m_vkDescriptorBufferInfos);
        m_vkDescriptorImageInfos = std::move(other.m_vkDescriptorImageInfos);
        m_vkBufferViewTexels = std::move(other.m_vkBufferViewTexels);
        reassemble();
    }

    WriteDescriptorSet& operator=(WriteDescriptorSet&&) noexcept = delete;

    WriteDescriptorSet(
        VkDescriptorSet vkDescriptorSet,
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType)
        : VkWriteDescriptorSet {}
    {
        sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        dstSet = vkDescriptorSet;
        dstBinding = bindingIndex;
        descriptorType = vkDescriptorType;
    }

    void addBufferInfo(const VkDescriptorBufferInfo& vkDescriptorBufferInfo)
    {
        m_vkDescriptorBufferInfos.emplace_back(vkDescriptorBufferInfo);
        descriptorCount = static_cast<uint32_t>(m_vkDescriptorBufferInfos.size());
        pBufferInfo = m_vkDescriptorBufferInfos.data();
    }

    void addBufferInfo(
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        VkDescriptorBufferInfo vkDescriptorBufferInfo {};
        vkDescriptorBufferInfo.buffer = buffer;
        vkDescriptorBufferInfo.offset = offset;
        vkDescriptorBufferInfo.range = range;
        addBufferInfo(vkDescriptorBufferInfo);
    }

    void addImageInfo(const VkDescriptorImageInfo& vkDescriptorImageInfo)
    {
        m_vkDescriptorImageInfos.emplace_back(vkDescriptorImageInfo);
        descriptorCount = static_cast<uint32_t>(m_vkDescriptorImageInfos.size());
        pImageInfo = m_vkDescriptorImageInfos.data();
    }

    void addImageInfo(
        VkImageView vkImageViewArg,
        VkImageLayout vkImageLayoutArg,
        VkSampler vkSamplerArg)
    {
        VkDescriptorImageInfo vkDescriptorImageInfo {
            .sampler = vkSamplerArg,
            .imageView = vkImageViewArg,
            .imageLayout = vkImageLayoutArg,
        };
        addImageInfo(vkDescriptorImageInfo);
    }

    void addImageInfo(
        VkImageView vkImageViewArg,
        VkImageLayout vkImageLayoutArg)
    {
        VkDescriptorImageInfo vkDescriptorImageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = vkImageViewArg,
            .imageLayout = vkImageLayoutArg,
        };
        addImageInfo(vkDescriptorImageInfo);
    }
};

class WriteDescriptorSetArray {

    //	The smart data.
    std::vector<WriteDescriptorSet> m_writeDescriptorSets;

    //	For now, we assemble the write descriptor set information
    //	into an array that we keep. This array has only a limited
    //	useful lifetime, so creating a separate array seems a bit
    //	dangerous.  Instead, the "real" array is assembled internally
    //	and then data and size are given out.
    std::vector<VkWriteDescriptorSet> m_vkWriteDescriptorSets;

    VkDescriptorSet m_currentDescriptorSet = VK_NULL_HANDLE;

    bool m_assembleCalled = false;

public:
    ~WriteDescriptorSetArray() noexcept(false)
    {
        if (!m_assembleCalled) {
            throw Exception("WriteDescriptorSetArray destroyed without being used.");
        }
    }
    WriteDescriptorSetArray(const WriteDescriptorSetArray&) = delete;
    WriteDescriptorSetArray& operator=(const WriteDescriptorSetArray&) = delete;
    WriteDescriptorSetArray(WriteDescriptorSetArray&&) noexcept = delete;
    WriteDescriptorSetArray& operator=(WriteDescriptorSetArray&&) noexcept = delete;

    WriteDescriptorSetArray(VkDescriptorSet vkDescriptorSet)
        : m_currentDescriptorSet(vkDescriptorSet)
    {
    }

    // WriteDescriptorSetArray& addBufferWriteDescriptor(
    //     uint32_t bindingIndex,
    //     VkDescriptorType vkDescriptorType,
    //     VkBuffer vkBufferArg,
    //     VkDeviceSize offsetArg,
    //     VkDeviceSize rangeArg)
    //{

    //    VkWriteDescriptorSet vkWriteDescriptorSet {
    //        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    //        .dstSet = m_currentDescriptorSet,
    //        .dstBinding = bindingIndex,
    //        .dstArrayElement = 0,
    //        .descriptorCount = 1,
    //        .descriptorType = vkDescriptorType,
    //    };

    //    VkDescriptorBufferInfo vkDescriptorBufferInfo {
    //        .buffer = vkBufferArg,
    //        .offset = offsetArg,
    //        .range = rangeArg
    //    };

    //    m_vkWriteDescriptorSets.emplace_back(vkWriteDescriptorSet);
    //    return *this;
    //}

    //	Handy version for Sascha Willems demos.
    //	Descriptor buffer info is already created.
    WriteDescriptorSetArray& addBufferWriteDescriptor(
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        const VkDescriptorBufferInfo& vkDescriptorBufferInfo)
    {
        WriteDescriptorSet& writeDescriptorSet
            = m_writeDescriptorSets.emplace_back(m_currentDescriptorSet, bindingIndex, vkDescriptorType);
        writeDescriptorSet.addBufferInfo(vkDescriptorBufferInfo);
        return *this;
    }

    WriteDescriptorSetArray& addImageWriteDescriptor(
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        const VkDescriptorImageInfo& vkDescriptorImageInfo)
    {
        WriteDescriptorSet& writeDescriptorSet
            = m_writeDescriptorSets.emplace_back(m_currentDescriptorSet, bindingIndex, vkDescriptorType);
        writeDescriptorSet.addImageInfo(vkDescriptorImageInfo);
        return *this;
    }

    WriteDescriptorSetArray& addImageWriteDescriptor(
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        VkImageView vkImageView,
        VkImageLayout vkImageLayout,
        VkSampler vkSampler)
    {
        WriteDescriptorSet& writeDescriptorSet
            = m_writeDescriptorSets.emplace_back(m_currentDescriptorSet, bindingIndex, vkDescriptorType);
        writeDescriptorSet.addImageInfo(vkImageView, vkImageLayout, vkSampler);
        return *this;
    }

    WriteDescriptorSetArray& addImageWriteDescriptor(
        uint32_t bindingIndex,
        VkDescriptorType vkDescriptorType,
        VkImageView vkImageView,
        VkImageLayout vkImageLayout)
    {
        WriteDescriptorSet& writeDescriptorSet
            = m_writeDescriptorSets.emplace_back(m_currentDescriptorSet, bindingIndex, vkDescriptorType);
        writeDescriptorSet.addImageInfo(vkImageView, vkImageLayout);
        return *this;
    }

    void assemble()
    {
        //	DANGER Will Robinson!
        //	We create an array of VkWriteDescriptorSets by shallow copying the
        //	VkWriteDescriptorSet info from the smart WriteDescriptorSets.
        //	This means that the pointers in the VkWriteDescriptorSets are pointing
        //	to the data in the smart WriteDescriptorSet.  And this means that the
        //	VkWriteDescriptorSet is only valid as long as the WriteDescriptorSet
        //	is unchanged. This shouldn't be a problem in normal usage, but don't
        //	go playing clever games with the VkWriteDescriptorSet data.

        //	TODO: need to figure out what to do if there were no updates.
        //	Kind of an edge case but might avoid some debugging pain later.

        if (m_assembleCalled) {
            return;
        }
        for (const WriteDescriptorSet& writeDescriptorSet : m_writeDescriptorSets) {
            m_vkWriteDescriptorSets.emplace_back(static_cast<VkWriteDescriptorSet>(writeDescriptorSet));
        }
        m_assembleCalled = true;
    }

    VkWriteDescriptorSet* vkWriteDescriptorSetsData()
    {
        assemble();
        return m_vkWriteDescriptorSets.data();
    }

    uint32_t vkWriteDescriptorSetsSize()
    {
        assemble();
        return static_cast<uint32_t>(m_vkWriteDescriptorSets.size());
    }

    void updateDescriptorSets()
    {
        vkUpdateDescriptorSets(
            vkDevice(),
            vkWriteDescriptorSetsSize(),
            vkWriteDescriptorSetsData(),
            0, nullptr);
    }
};

class DescriptorSet : public InteropHandle3<VkDescriptorSet, VkDescriptorPool> {

    DescriptorSet(
        VkDescriptorSet vkDescriptorSet,
        VkDescriptorPool vkDescriptorPool,
        DestroyFunc_t pfnDestroy,
        const DescriptorSetLayout& descriptorSetLayout)
        : InteropHandle3(vkDescriptorSet, vkDescriptorPool, pfnDestroy)
    {
    }

    static void destroy(VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool)
    {
        vkFreeDescriptorSets(vkDevice(), vkDescriptorPool, 1, &vkDescriptorSet);
    }

public:
    DescriptorSet() = default;
    ~DescriptorSet() = default;

    DescriptorSet(const DescriptorSet& other)
        : InteropHandle3(other)
    {
    }

    DescriptorSet& operator=(const DescriptorSet& other)
    {
        this->~DescriptorSet();
        new (this) DescriptorSet(other);
        return *this;
    }

    DescriptorSet(DescriptorSet&& other) noexcept
        : InteropHandle3(std::move(other))
    {
    }

    DescriptorSet& operator=(DescriptorSet&& other) noexcept
    {
        this->~DescriptorSet();
        new (this) DescriptorSet(std::move(other));
        return *this;
    }

    DescriptorSet(DescriptorSetLayout descriptorSetLayout, VkDescriptorPool vkDescriptorPool)
    {
        VkDescriptorSetLayout vkDescriptorSetLayout = descriptorSetLayout;
        VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo {};
        vkDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        vkDescriptorSetAllocateInfo.descriptorPool = vkDescriptorPool;
        vkDescriptorSetAllocateInfo.descriptorSetCount = 1;
        vkDescriptorSetAllocateInfo.pSetLayouts = &vkDescriptorSetLayout;

        VkDescriptorSet vkDescriptorSet;
        VkResult vkResult = vkAllocateDescriptorSets(
            vkDevice(),
            &vkDescriptorSetAllocateInfo,
            &vkDescriptorSet);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) DescriptorSet(vkDescriptorSet, vkDescriptorPool, &destroy, descriptorSetLayout);
    }
};

class PipelineLayoutCreateInfo : public VkPipelineLayoutCreateInfo {

    std::vector<VkDescriptorSetLayout> m_vkDescriptorSetLayouts;
    std::vector<VkPushConstantRange> m_vkPushConstantRanges;

public:
    PipelineLayoutCreateInfo()
        : VkPipelineLayoutCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    }

    //	No copy, no move for now.  Could be changed with extra code if necessary.
    ~PipelineLayoutCreateInfo() = default;
    PipelineLayoutCreateInfo(const PipelineLayoutCreateInfo&) = delete;
    PipelineLayoutCreateInfo& operator=(const PipelineLayoutCreateInfo&) = delete;
    PipelineLayoutCreateInfo(PipelineLayoutCreateInfo&&) noexcept = delete;
    PipelineLayoutCreateInfo& operator=(PipelineLayoutCreateInfo&&) noexcept = delete;

    PipelineLayoutCreateInfo& addDescriptorSetLayout(
        VkDescriptorSetLayout vkDescriptorSetLayout)
    {
        //	Always up to date.
        m_vkDescriptorSetLayouts.push_back(vkDescriptorSetLayout);
        setLayoutCount = static_cast<uint32_t>(m_vkDescriptorSetLayouts.size());
        pSetLayouts = m_vkDescriptorSetLayouts.data();
        return *this;
    }

    PipelineLayoutCreateInfo& addPushConstantRange(
        ShaderStageFlags shaderStageFlags,
        uint32_t offsetArg,
        size_t sizeArg)
    {
        VkPushConstantRange vkPushConstantRange {};
        vkPushConstantRange.stageFlags = static_cast<VkShaderStageFlags>(shaderStageFlags);
        vkPushConstantRange.offset = offsetArg;
        vkPushConstantRange.size = static_cast<uint32_t>(sizeArg);

        m_vkPushConstantRanges.emplace_back(vkPushConstantRange);
        pushConstantRangeCount = static_cast<uint32_t>(m_vkPushConstantRanges.size());
        pPushConstantRanges = m_vkPushConstantRanges.data();
        return *this;
    }
};

class PipelineLayout : public InteropHandle2<VkPipelineLayout> {

    PipelineLayout(VkPipelineLayout vkPipelineLayout, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkPipelineLayout, pfnDestroy)
    {
    }

    static void destroy(VkPipelineLayout vkPipelineLayout)
    {
        vkDestroyPipelineLayout(vkDevice(), vkPipelineLayout, nullptr);
    }

public:
    PipelineLayout() = default;
    ~PipelineLayout() = default;

    PipelineLayout(const PipelineLayout& other)
        : InteropHandle2(other)
    {
    }

    PipelineLayout& operator=(const PipelineLayout& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~PipelineLayout();
        new (this) PipelineLayout(other);
        return *this;
    }

    PipelineLayout(PipelineLayout&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    PipelineLayout& operator=(PipelineLayout&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~PipelineLayout();
        new (this) PipelineLayout(std::move(other));
        return *this;
    }

    PipelineLayout(const PipelineLayoutCreateInfo& pipelineLayoutCreateInfo)
    {
        VkPipelineLayout vkPipelineLayout;
        VkResult vkResult = vkCreatePipelineLayout(vkDevice(), &pipelineLayoutCreateInfo, nullptr, &vkPipelineLayout);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) PipelineLayout(vkPipelineLayout, &destroy);
    }
};

class PipelineInputAssemblyStateCreateInfo : public VkPipelineInputAssemblyStateCreateInfo {

public:
    PipelineInputAssemblyStateCreateInfo()
        : VkPipelineInputAssemblyStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    }

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
        cullMode = VK_CULL_MODE_BACK_BIT;
        // cullMode = VK_CULL_MODE_NONE;
        frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
};

class PipelineMultisampleStateCreateInfo : public VkPipelineMultisampleStateCreateInfo {

public:
    PipelineMultisampleStateCreateInfo()
        : VkPipelineMultisampleStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    }

    static PipelineMultisampleStateCreateInfo reasonableDefaults()
    {
        PipelineMultisampleStateCreateInfo reasonableDefaults;
        reasonableDefaults.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        reasonableDefaults.minSampleShading = 1.0f;
        return reasonableDefaults;
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

    std::vector<PipelineColorBlendAttachmentState> m_pipelineColorBlendAttachmentStates;

public:
    ~PipelineColorBlendStateCreateInfo() = default;
    PipelineColorBlendStateCreateInfo(const PipelineColorBlendStateCreateInfo&) = delete;
    PipelineColorBlendStateCreateInfo& operator=(const PipelineColorBlendStateCreateInfo&) = delete;
    PipelineColorBlendStateCreateInfo(PipelineColorBlendStateCreateInfo&&) noexcept = delete;
    PipelineColorBlendStateCreateInfo& operator=(PipelineColorBlendStateCreateInfo&&) noexcept = delete;

    //	Default create with reasonable settings.
    PipelineColorBlendStateCreateInfo()
        : VkPipelineColorBlendStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    }

    void addColorBlendAttachmentState(
        const PipelineColorBlendAttachmentState& pipelineColorBlendAttachmentState)
    {
        m_pipelineColorBlendAttachmentStates.emplace_back(pipelineColorBlendAttachmentState);
        attachmentCount = static_cast<uint32_t>(m_pipelineColorBlendAttachmentStates.size());
        pAttachments = m_pipelineColorBlendAttachmentStates.data();
    }
};

class PipelineDepthStencilStateCreateInfo : public VkPipelineDepthStencilStateCreateInfo {

public:
    PipelineDepthStencilStateCreateInfo()
        : VkPipelineDepthStencilStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    }

    static PipelineDepthStencilStateCreateInfo basicDepth()
    {
        PipelineDepthStencilStateCreateInfo basicDepth;
        basicDepth.depthTestEnable = VK_TRUE;
        basicDepth.depthWriteEnable = VK_TRUE;
        basicDepth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        basicDepth.depthBoundsTestEnable = VK_FALSE;
        basicDepth.back.failOp = VK_STENCIL_OP_KEEP;
        basicDepth.back.passOp = VK_STENCIL_OP_KEEP;
        basicDepth.back.compareOp = VK_COMPARE_OP_ALWAYS;
        basicDepth.stencilTestEnable = VK_FALSE;
        basicDepth.front = basicDepth.back;

        return basicDepth;
    }
};
static_assert(sizeof(PipelineDepthStencilStateCreateInfo) == sizeof(VkPipelineDepthStencilStateCreateInfo));

class PipelineDynamicStateCreateInfo : public VkPipelineDynamicStateCreateInfo {

    std::vector<VkDynamicState> m_dynamicStates;

public:
    PipelineDynamicStateCreateInfo()
        : VkPipelineDynamicStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    }

    void addDynamicState(VkDynamicState vkDynamicState)
    {
        //	Always up to date
        m_dynamicStates.emplace_back(vkDynamicState);
        dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
        pDynamicStates = m_dynamicStates.data();
    }
};

class PipelineVertexInputStateCreateInfo : public VkPipelineVertexInputStateCreateInfo {

    std::vector<VkVertexInputBindingDescription> m_vertexInputBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> m_vertexInputAttributeDescriptions;

    void reassemble()
    {
        vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindingDescriptions.size());
        pVertexBindingDescriptions = m_vertexInputBindingDescriptions.data();
        vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributeDescriptions.size());
        pVertexAttributeDescriptions = m_vertexInputAttributeDescriptions.data();
    }

public:
    ~PipelineVertexInputStateCreateInfo() = default;

    //	Doesn't copy pNext.
    PipelineVertexInputStateCreateInfo(const PipelineVertexInputStateCreateInfo& other)
        : VkPipelineVertexInputStateCreateInfo(other)
    {
        pNext = nullptr;
        m_vertexInputBindingDescriptions = other.m_vertexInputBindingDescriptions;
        m_vertexInputAttributeDescriptions = other.m_vertexInputAttributeDescriptions;
        reassemble();
    }

    PipelineVertexInputStateCreateInfo& operator=(const PipelineVertexInputStateCreateInfo& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~PipelineVertexInputStateCreateInfo();
        new (this) PipelineVertexInputStateCreateInfo(other);
        return *this;
    }

    PipelineVertexInputStateCreateInfo(PipelineVertexInputStateCreateInfo&& other) noexcept
        : VkPipelineVertexInputStateCreateInfo(std::move(other))
    {
        pNext = nullptr;
        m_vertexInputBindingDescriptions = std::move(other.m_vertexInputBindingDescriptions);
        m_vertexInputAttributeDescriptions = std::move(other.m_vertexInputAttributeDescriptions);
        reassemble();
    }

    PipelineVertexInputStateCreateInfo& operator=(PipelineVertexInputStateCreateInfo&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~PipelineVertexInputStateCreateInfo();
        new (this) PipelineVertexInputStateCreateInfo(std::move(other));
    }

    PipelineVertexInputStateCreateInfo()
        : VkPipelineVertexInputStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    }

    void addVertexInputBindingDescription(
        const VkVertexInputBindingDescription& vertexInputBindingDescription)
    {
        m_vertexInputBindingDescriptions.emplace_back(vertexInputBindingDescription);
        vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindingDescriptions.size());
        pVertexBindingDescriptions = m_vertexInputBindingDescriptions.data();
    }

    void addVertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate vkVertexInputRate)
    {
        VkVertexInputBindingDescription vkVertexInputBindingDescription {
            .binding = binding,
            .stride = stride,
            .inputRate = vkVertexInputRate
        };

        m_vertexInputBindingDescriptions.emplace_back(vkVertexInputBindingDescription);
        vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexInputBindingDescriptions.size());
        pVertexBindingDescriptions = m_vertexInputBindingDescriptions.data();
    }

    void addVertexInputAttributeDescription(
        const VkVertexInputAttributeDescription& vertexInputAttributeDescription)
    {
        m_vertexInputAttributeDescriptions.emplace_back(vertexInputAttributeDescription);
        vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributeDescriptions.size());
        pVertexAttributeDescriptions = m_vertexInputAttributeDescriptions.data();
    }

    void addVertexInputAttributeDescription(
        uint32_t binding, uint32_t location, VkFormat vkFormat, uint32_t offset)
    {
        VkVertexInputAttributeDescription vkVertexInputAttributeDescription {
            .location = location,
            .binding = binding,
            .format = vkFormat,
            .offset = offset
        };
        m_vertexInputAttributeDescriptions.emplace_back(vkVertexInputAttributeDescription);
        vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexInputAttributeDescriptions.size());
        pVertexAttributeDescriptions = m_vertexInputAttributeDescriptions.data();
    }
};

class PipelineViewportStateCreateInfo : public VkPipelineViewportStateCreateInfo {
    //	TODO: are the flags used at all?

    std::vector<VkViewport> m_viewports;
    std::vector<VkRect2D> m_scissors;

public:
    PipelineViewportStateCreateInfo()
        : VkPipelineViewportStateCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    }

    ~PipelineViewportStateCreateInfo() = default;
    PipelineViewportStateCreateInfo(const PipelineViewportStateCreateInfo&) = delete;
    PipelineViewportStateCreateInfo& operator=(const PipelineViewportStateCreateInfo&) = delete;
    PipelineViewportStateCreateInfo(PipelineViewportStateCreateInfo&&) noexcept = delete;
    PipelineViewportStateCreateInfo& operator=(PipelineViewportStateCreateInfo&&) noexcept = delete;

    void addViewport(const VkViewport& viewport)
    {
        m_viewports.emplace_back(viewport);
        viewportCount = static_cast<uint32_t>(m_viewports.size());
        pViewports = m_viewports.data();
    }

    void addScissor(const VkRect2D scissor)
    {
        m_scissors.emplace_back(scissor);
        scissorCount = static_cast<uint32_t>(m_scissors.size());
        pScissors = m_scissors.data();
    }
};

class PipelineRenderingCreateInfo : public VkPipelineRenderingCreateInfo {
    //	TODO: is viewMask used?
    std::vector<VkFormat> m_colorAttachmentFormats;

public:
    PipelineRenderingCreateInfo()
        : VkPipelineRenderingCreateInfo {}
    {
        sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    }

    void setDepthAttachmentFormat(VkFormat vkFormat)
    {
        depthAttachmentFormat = vkFormat;
    }

    void setStencilAttachmentFormat(VkFormat vkFormat)
    {
        stencilAttachmentFormat = vkFormat;
    }

    void addColorAttachmentFormat(VkFormat vkFormat)
    {
        m_colorAttachmentFormats.emplace_back(vkFormat);
        colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentFormats.size());
        pColorAttachmentFormats = m_colorAttachmentFormats.data();
    }
};

class GraphicsPipeline;
class GraphicsPipelineCreateInfo {

    PipelineInputAssemblyStateCreateInfo m_pipelineInputAssemblyStateCreateInfo;
    PipelineVertexInputStateCreateInfo m_pipelineVertexInputStateCreateInfo;
    std::vector<VkPipelineShaderStageCreateInfo> m_vkPipelineShaderStageCreateInfos;
    PipelineDynamicStateCreateInfo m_pipelineDynamicStateCreateInfo;
    PipelineViewportStateCreateInfo m_viewportStateCreateInfo;
    PipelineRasterizationStateCreateInfo m_pipelineRasterizationStateCreateInfo;
    PipelineMultisampleStateCreateInfo m_pipelineMultisampleStateCreateInfo;
    PipelineColorBlendStateCreateInfo m_pipelineColorBlendStateCreateInfo;
    PipelineDepthStencilStateCreateInfo m_pipelineDepthStencilStateCreateInfo;

    //	TODO: just handling pipeline rendering create info extensions for now.
    PipelineRenderingCreateInfo m_pipelineRenderingCreateInfo;

    //	Contains rather than inherits from the Vulkan structure.
    //	Not sure if it makes any difference.  It's a tiny bit
    //	more understandable when assembling since there is so much
    //	going on during the assembly.  It's clearer what's going
    //	into the Vulkan create info structure.
    VkGraphicsPipelineCreateInfo m_vkGraphicsPipelineCreateInfo {};

    void assemblePartial()
    {
        m_vkGraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        m_vkGraphicsPipelineCreateInfo.pVertexInputState = &m_pipelineVertexInputStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pInputAssemblyState = &m_pipelineInputAssemblyStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pViewportState = &m_viewportStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pRasterizationState = &m_pipelineRasterizationStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pMultisampleState = &m_pipelineMultisampleStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pColorBlendState = &m_pipelineColorBlendStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pDynamicState = &m_pipelineDynamicStateCreateInfo;
        m_vkGraphicsPipelineCreateInfo.pDepthStencilState = &m_pipelineDepthStencilStateCreateInfo;

        m_vkGraphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        m_vkGraphicsPipelineCreateInfo.basePipelineIndex = -1; // Optional
    }

public:
    GraphicsPipelineCreateInfo()
    {
        assemblePartial();
    }

    //	No move, no copy.  Maybe update code if necessary for multiple pipelines.
    ~GraphicsPipelineCreateInfo() = default;
    GraphicsPipelineCreateInfo(const GraphicsPipelineCreateInfo&) = delete;
    GraphicsPipelineCreateInfo& operator=(const GraphicsPipelineCreateInfo&) = delete;
    GraphicsPipelineCreateInfo(GraphicsPipelineCreateInfo&&) noexcept = delete;
    GraphicsPipelineCreateInfo& operator=(GraphicsPipelineCreateInfo&&) noexcept = delete;

    VkGraphicsPipelineCreateInfo* operator&()
    {
        return &m_vkGraphicsPipelineCreateInfo;
    }

    void usePipelineRenderingCreateInfo()
    {
        m_vkGraphicsPipelineCreateInfo.pNext = &m_pipelineRenderingCreateInfo;
    }

    void setInputAssemblyState(VkPrimitiveTopology vkPrimitiveTopology)
    {
        m_pipelineInputAssemblyStateCreateInfo = PipelineInputAssemblyStateCreateInfo(vkPrimitiveTopology);
    }

    void setRasterizationStateCreateInfo(const PipelineRasterizationStateCreateInfo& pipelineRasterizationStateCreateInfo)
    {
        m_pipelineRasterizationStateCreateInfo = pipelineRasterizationStateCreateInfo;
    }

    void setRasterizationPolygonMode(VkPolygonMode vkPolygonMode)
    {
        m_pipelineRasterizationStateCreateInfo.polygonMode = vkPolygonMode;
    }

    void setMultisampleStateCreateInfo(const PipelineMultisampleStateCreateInfo& pipelineMultisampleStateCreateInfo)
    {
        m_pipelineMultisampleStateCreateInfo = pipelineMultisampleStateCreateInfo;
    }

    void setDepthStencilStateCreateInfo(PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo)
    {
        m_pipelineDepthStencilStateCreateInfo = pipelineDepthStencilStateCreateInfo;
    }

    void setDepthAttachmentFormat(VkFormat vkFormat)
    {
        m_pipelineRenderingCreateInfo.setDepthAttachmentFormat(vkFormat);
    }

    void setStencilAttachmentFormat(VkFormat vkFormat)
    {
        m_pipelineRenderingCreateInfo.setStencilAttachmentFormat(vkFormat);
    }

    void addColorAttachmentFormat(VkFormat vkFormat)
    {
        m_pipelineRenderingCreateInfo.addColorAttachmentFormat(vkFormat);
    }

    void addColorBlendAttachmentState(
        const PipelineColorBlendAttachmentState& pipelineColorBlendAttachmentState)
    {
        m_pipelineColorBlendStateCreateInfo.addColorBlendAttachmentState(pipelineColorBlendAttachmentState);
    }

    void addShaderModule(
        const ShaderModule& shaderModule,
        VkShaderStageFlagBits vkShaderStageFlagBits,
        const char* entryPointName)
    {
        VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo {};
        vkPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vkPipelineShaderStageCreateInfo.stage = vkShaderStageFlagBits;
        vkPipelineShaderStageCreateInfo.module = shaderModule;
        vkPipelineShaderStageCreateInfo.pName = entryPointName;
        m_vkPipelineShaderStageCreateInfos.emplace_back(vkPipelineShaderStageCreateInfo);
        m_vkGraphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(m_vkPipelineShaderStageCreateInfos.size());
        m_vkGraphicsPipelineCreateInfo.pStages = m_vkPipelineShaderStageCreateInfos.data();
    }

    void clearShaders()
    {
        m_vkPipelineShaderStageCreateInfos.clear();
        m_vkGraphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(m_vkPipelineShaderStageCreateInfos.size());
        m_vkGraphicsPipelineCreateInfo.pStages = m_vkPipelineShaderStageCreateInfos.data();
    }

    void addVertexInputBindingDescription(
        const VkVertexInputBindingDescription& vertexInputBindingDescription)
    {
        m_pipelineVertexInputStateCreateInfo.addVertexInputBindingDescription(vertexInputBindingDescription);
    }

    void addVertexInputBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate vkVertexInputRate)
    {
        m_pipelineVertexInputStateCreateInfo.addVertexInputBindingDescription(binding, stride, vkVertexInputRate);
    }

    void addVertexInputAttributeDescription(
        const VkVertexInputAttributeDescription& vertexInputAttributeDescription)
    {
        m_pipelineVertexInputStateCreateInfo.addVertexInputAttributeDescription(vertexInputAttributeDescription);
    }

    void addVertexInputAttributeDescription(
        uint32_t binding, uint32_t location, VkFormat vkFormat, uint32_t offset)
    {
        m_pipelineVertexInputStateCreateInfo
            .addVertexInputAttributeDescription(binding, location, vkFormat, offset);
    }

    void addDynamicState(VkDynamicState vkDynamicState)
    {
        m_pipelineDynamicStateCreateInfo.addDynamicState(vkDynamicState);
    }

    //	Useful for porting.
    void setPipelineVertexInputStateCreateInfo(const PipelineVertexInputStateCreateInfo& pipelineVertexInputStateCreateInfo)
    {
        m_pipelineVertexInputStateCreateInfo = pipelineVertexInputStateCreateInfo;
    }

    void allowDerivatives(bool torf)
    {
        if (torf) {
            m_vkGraphicsPipelineCreateInfo.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
            return;
        }
        m_vkGraphicsPipelineCreateInfo.flags &= ~VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    }

    void setBasePipeline(VkPipeline basePipeline)
    {
        m_vkGraphicsPipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
        m_vkGraphicsPipelineCreateInfo.basePipelineHandle = basePipeline;
        m_vkGraphicsPipelineCreateInfo.basePipelineIndex = -1;
    }

    void addViewport(const VkViewport& viewport)
    {
        m_viewportStateCreateInfo.addViewport(viewport);
    }

    void addScissor(const VkRect2D scissor)
    {
        m_viewportStateCreateInfo.addScissor(scissor);
    }

    void setPipelineLayout(const PipelineLayout& pipelineLayout)
    {
        m_vkGraphicsPipelineCreateInfo.layout = pipelineLayout;
    }

    void setRenderPass(const RenderPass& renderPass, int subpassNumber)
    {
        m_vkGraphicsPipelineCreateInfo.renderPass = renderPass;
        m_vkGraphicsPipelineCreateInfo.subpass = subpassNumber;
    }
};

class GraphicsPipeline : public InteropHandle2<VkPipeline> {

    GraphicsPipeline(VkPipeline vkPipeline, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkPipeline, pfnDestroy)
    {
    }

    static void destroy(VkPipeline vkPipeline)
    {
        vkDestroyPipeline(vkDevice(), vkPipeline, nullptr);
    }

public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() = default;

    GraphicsPipeline(const GraphicsPipeline& other)
        : InteropHandle2(other)
    {
    }

    GraphicsPipeline& operator=(const GraphicsPipeline& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~GraphicsPipeline();
        new (this) GraphicsPipeline(other);
        return *this;
    }

    GraphicsPipeline(GraphicsPipeline&& other) noexcept
        : InteropHandle2(std::move(other))
    {
    }

    GraphicsPipeline& operator=(GraphicsPipeline&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~GraphicsPipeline();
        new (this) GraphicsPipeline(std::move(other));
        return *this;
    }

    GraphicsPipeline(GraphicsPipelineCreateInfo& pipelineCreateInfo)
    {
        VkPipeline vkPipeline;
        VkResult vkResult = vkCreateGraphicsPipelines(vkDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &vkPipeline);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) GraphicsPipeline(vkPipeline, &destroy);
    }

    GraphicsPipeline(VkGraphicsPipelineCreateInfo& vkGraphicsPipelineCreateInfo)
    {
        VkPipeline vkPipeline;
        VkResult vkResult = vkCreateGraphicsPipelines(vkDevice(), VK_NULL_HANDLE, 1, &vkGraphicsPipelineCreateInfo, nullptr, &vkPipeline);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) GraphicsPipeline(vkPipeline, &destroy);
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

class Swapchain : public InteropHandle2<VkSwapchainKHR> {

    Swapchain(VkSwapchainKHR vkSwapchain, DestroyFunc_t pfnDestroy, VkExtent2D vkSwapchainImageExtent)
        : InteropHandle2(vkSwapchain, pfnDestroy)
        , m_vkSwapchainImageExtent(vkSwapchainImageExtent)
    {
    }

    static void destroy(VkSwapchainKHR vkSwapchain)
    {
        vkDestroySwapchainKHR(vkDevice(), vkSwapchain, nullptr);
    }

    VkExtent2D m_vkSwapchainImageExtent = { .width = 0, .height = 0 };

public:
    Swapchain() = default;
    ~Swapchain() = default;

    Swapchain(const Swapchain& other)
        : InteropHandle2(other)
        , m_vkSwapchainImageExtent(other.m_vkSwapchainImageExtent)
    {
    }

    Swapchain& operator=(const Swapchain& other)
    {
        if (this == &other) {
            return *this;
        }
        this->~Swapchain();
        new (this) Swapchain(other);
        return *this;
    }

    Swapchain(Swapchain&& other) noexcept
        : InteropHandle2(std::move(other))
        , m_vkSwapchainImageExtent(other.m_vkSwapchainImageExtent)
    {
    }

    Swapchain& operator=(Swapchain&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Swapchain();
        new (this) Swapchain(std::move(other));
        return *this;
    }

    Swapchain(const VkSwapchainCreateInfoKHR& vkSwapchainCreateInfo)
    {
        VkSwapchainKHR vkSwapchain;
        VkResult vkResult = vkCreateSwapchainKHR(device(), &vkSwapchainCreateInfo, nullptr, &vkSwapchain);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Swapchain(vkSwapchain, &destroy, vkSwapchainCreateInfo.imageExtent);
    }

    VkExtent2D imageExtent() const { return m_vkSwapchainImageExtent; }

    std::vector<VkImage> getImages() const
    {
        uint32_t swapchainImageCount;
        VkResult vkResult = vkGetSwapchainImagesKHR(vkDevice(), *this, &swapchainImageCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        vkResult = vkGetSwapchainImagesKHR(vkDevice(), *this, &swapchainImageCount, swapchainImages.data());
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

class Image_DeviceMemory {

    Image_DeviceMemory(Image&& image, DeviceMemory<>&& deviceMemory)
        : m_image(std::move(image))
        , m_deviceMemory(std::move(deviceMemory))
    {
    }

public:
    Image m_image;
    DeviceMemory<> m_deviceMemory;

    Image_DeviceMemory() = default;
    ~Image_DeviceMemory() = default;

    //	TODO: not clear why copying would be needed.
    Image_DeviceMemory(const Image_DeviceMemory&) = delete;
    Image_DeviceMemory& operator=(const Image_DeviceMemory&) = delete;

    Image_DeviceMemory(Image_DeviceMemory&& other) noexcept
        : m_image(std::move(other.m_image))
        , m_deviceMemory(std::move(other.m_deviceMemory))
    {
    }

    Image_DeviceMemory& operator=(Image_DeviceMemory&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Image_DeviceMemory();
        new (this) Image_DeviceMemory(std::move(other));
        return *this;
    }

    Image_DeviceMemory(
        const ImageCreateInfo& imageCreateInfo,
        MemoryPropertyFlags properties)
    {
        Image image(imageCreateInfo);
        DeviceMemory deviceMemory = image.allocateDeviceMemory(properties);

        VkResult vkResult = vkBindImageMemory(vkDevice(), image, deviceMemory, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }

        new (this) Image_DeviceMemory(std::move(image), std::move(deviceMemory));
    }

    Image_DeviceMemory(
        VkExtent2D vkExtent2D,
        VkFormat format,
        VkImageUsageFlags usage,
        MemoryPropertyFlags properties)
    {
        ImageCreateInfo imageCreateInfo(format, usage);
        imageCreateInfo.setExtent(vkExtent2D);
        new (this) Image_DeviceMemory(imageCreateInfo, properties);
    }
};

class Image_Memory_View {

public:
    Image m_image;
    DeviceMemory<> m_deviceMemory;
    ImageView m_imageView;

    Image_Memory_View() = default;
    ~Image_Memory_View() = default;

    Image_Memory_View(const Image_Memory_View&) = delete;
    Image_Memory_View& operator=(const Image_Memory_View&) = delete;

    Image_Memory_View(Image_Memory_View&& other) noexcept
        : m_image(std::move(other.m_image))
        , m_deviceMemory(std::move(other.m_deviceMemory))
        , m_imageView(std::move(other.m_imageView))
    {
    }

    Image_Memory_View& operator=(Image_Memory_View&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Image_Memory_View();
        new (this) Image_Memory_View(std::move(other));
    }

    //	Handy move constructor.
    Image_Memory_View(
        Image&& image,
        DeviceMemory<>&& deviceMemory,
        ImageView&& imageView)
        : m_image(std::move(image))
        , m_deviceMemory(std::move(deviceMemory))
        , m_imageView(std::move(imageView))
    {
    }
};

class Framebuffer : public InteropHandle2<VkFramebuffer> {

    Framebuffer(VkFramebuffer vkFramebuffer, DestroyFunc_t pfnDestroy)
        : InteropHandle2(vkFramebuffer, pfnDestroy)
    {
    }

    static void destroy(VkFramebuffer vkFramebuffer)
    {
        vkDestroyFramebuffer(vkDevice(), vkFramebuffer, nullptr);
    }

    //	Right now, framebuffer holds these just to control lifetime.
    //	TODO: is there ever going to be more than one of these?
    std::vector<Image_Memory_View> m_image_memory_views;

    std::vector<ImageView> m_imageViews;

public:
    Framebuffer() = default;
    ~Framebuffer() = default;
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept
        : InteropHandle2(std::move(other))
        , m_image_memory_views(std::move(other.m_image_memory_views))
        , m_imageViews(std::move(other.m_imageViews))
    {
    }

    Framebuffer& operator=(Framebuffer&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        this->~Framebuffer();
        new (this) Framebuffer(std::move(other));
        return *this;
    }

    Framebuffer(FramebufferCreateInfo& framebufferCreateInfo)
    {
        VkFramebuffer vkFramebuffer;
        VkResult vkResult = vkCreateFramebuffer(device(), framebufferCreateInfo.assemble(), nullptr, &vkFramebuffer);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        new (this) Framebuffer(vkFramebuffer, &destroy);
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
    //	Just save the create info since we need parts of it later.
    SwapchainCreateInfo m_swapchainCreateInfo;

    //	Save the "smart" m_vkSurface since the create info only has the base m_vkBuffer.
    Surface m_surface;
    RenderPass m_renderPass;
    Swapchain m_swapchain;
    std::vector<Framebuffer> m_swapchainFrameBuffers;

    bool m_swapchainUpToDate = false;

private:
    // void makeEmpty()
    //{
    //     //	TODO: Need to review the whole move thing to make sure this all makes sense.
    //     m_swapchainFrameBuffers.clear();
    // }

    void destroyFrameBuffers()
    {
        m_swapchainFrameBuffers.clear();
    }

    void destroy()
    {
        if (m_swapchain) {
            vkDeviceWaitIdle(vkDevice());
            destroyFrameBuffers();
        }
    }

    void createSwapchainFrameBuffers()
    {

        std::vector<VkImage> m_swapchainImages = m_swapchain.getImages();

        for (VkImage vkImage : m_swapchainImages) {
            //	The renderpass vkImage vkImageView and the depth buffer are "passed"
            //	to the renderpass via attachments to the framebuffer.  This
            //	means that was don't need to actually pass them through code.
            //	All we need to do is create them and make sure they don't disappear.
            //	We just have the framebuffer take ownership of the m_vkImage m_vkImageView and
            //	depth buffer.
            Image_Memory_View depthBuffer(createDepthBuffer(m_swapchain.imageExtent()));

            ImageViewCreateInfo imageViewCreateInfo(
                vkImage,
                VK_IMAGE_VIEW_TYPE_2D,
                m_swapchainCreateInfo.imageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT);
            ImageView imageView(imageViewCreateInfo);

            //	Note that attachments here are m_vkImage views.  When a
            //	Renderpass adds attachments, the attachments are descriptions
            //	of these attachments.
            FramebufferCreateInfo framebufferCreateInfo(m_renderPass, m_swapchain.imageExtent());
            framebufferCreateInfo
                .addAttachment(imageView)
                .addAttachment(depthBuffer.m_imageView);

            Framebuffer framebuffer(framebufferCreateInfo);
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
        //	the m_hwnd is minimized.  Return a "null" m_swapChain if
        //	this occurs.
        if (surfaceExtent.width == 0 || surfaceExtent.height == 0) {
            return Swapchain {};
        }
        //	TODO: should probably sanity check some other m_vkSurface capabilities.
        swapChainCreateInfo.surface = surface;
        swapChainCreateInfo.imageExtent = surfaceExtent;
        swapChainCreateInfo.preTransform = vkSurfaceCapabilities.currentTransform;

        return Swapchain(swapChainCreateInfo);
    }

public:
    //	TODO: this needs an entire review to see what's actually needed.
    Swapchain_FrameBuffers() = default;
    ~Swapchain_FrameBuffers() = default;

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

        vkDeviceWaitIdle(vkDevice());
        destroyFrameBuffers();
        //	TODO: can we set the old swapchain to avoid this?
        //	Explicitly destroy the old swapchain for now.
        m_swapchain = Swapchain();

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
        VkExtent2D vkExtent2D)
    {
        constexpr VkFormat depthBufferFormat = VK_FORMAT_D32_SFLOAT;

        ImageCreateInfo imageCreateInfo(
            depthBufferFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.setExtent(vkExtent2D);
        Image_DeviceMemory image_memory(imageCreateInfo, MEMORY_PROPERTY_DEVICE_LOCAL);

        ImageViewCreateInfo imageViewCreateInfo(
            image_memory.m_image,
            VK_IMAGE_VIEW_TYPE_2D,
            depthBufferFormat,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        ImageView imageView(imageViewCreateInfo);

        return Image_Memory_View(
            std::move(image_memory.m_image),
            std::move(image_memory.m_deviceMemory),
            std::move(imageView));
    }
};

class Texture {
    Image m_image;
    DeviceMemory<> m_imageDeviceMemory;
    ImageView m_imageView;
    Sampler m_sampler;

    VkImageLayout m_vkImageLayout;

public:
    Texture() = default;
    ~Texture() = default;

    //	No move, no copy for now.
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept = delete;
    Texture& operator=(Texture&&) noexcept = delete;

    Image image()
    {
        return m_image;
    }

    //	Useful when porting.
    void takeImage(Image&& image)
    {
        m_image = std::move(image);
    }

    Sampler sampler()
    {
        return m_sampler;
    }

    void takeSampler(Sampler&& sampler)
    {
        m_sampler = std::move(sampler);
    }

    ImageView imageView()
    {
        return m_imageView;
    }

    void takeImageView(ImageView&& imageView)
    {
        m_imageView = std::move(imageView);
    }

    void allocateBindImageMemory(MemoryPropertyFlags requiredProperties)
    {
        m_imageDeviceMemory = m_image.allocateDeviceMemory(requiredProperties);
        m_image.bindImageMemory(m_imageDeviceMemory);
    }

    void setVkImageLayout(VkImageLayout vkImageLayout)
    {
        m_vkImageLayout = vkImageLayout;
    }

    VkImageLayout vkImageLayout()
    {
        return m_vkImageLayout;
    }
};

class CommandBuffer : public InteropHandle3<VkCommandBuffer, VkCommandPool> {

    static void destroy(VkCommandBuffer vkCommandBuffer, VkCommandPool vkCommandPool)
    {
        vkFreeCommandBuffers(vkDevice(), vkCommandPool, 1, &vkCommandBuffer);
    }

    CommandBuffer(
        VkCommandBuffer vkCommandBuffer,
        VkCommandPool vkCommandPool,
        DestroyFunc_t pfnDestroy)
        : InteropHandle3(vkCommandBuffer, vkCommandPool, pfnDestroy)
    {
    }

public:
    CommandBuffer() = default;

    CommandBuffer(VkCommandPool vkCommandPool)
    {
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = vkCommandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer vkCommandBuffer;
        vkAllocateCommandBuffers(vkDevice(), &allocInfo, &vkCommandBuffer);
        new (this) CommandBuffer(vkCommandBuffer, vkCommandPool, &destroy);
    }

    //	Handy way to make simple copy from an existing vkCommandBuffer.
    //	Note that we don't have a real command pool.
    static CommandBuffer makeCopy(VkCommandBuffer vkCommandBuffer)
    {
        return CommandBuffer(vkCommandBuffer, VK_NULL_HANDLE, nullptr);
    }

    CommandBuffer& reset()
    {
        VkResult vkResult = vkResetCommandBuffer(*this, 0);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return *this;
    }

    CommandBuffer& begin()
    {
        VkCommandBufferBeginInfo vkCommandBufferBeginInfo {};
        vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VkResult vkResult = vkBeginCommandBuffer(*this, &vkCommandBufferBeginInfo);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return *this;
    }

    CommandBuffer& begin(const VkCommandBufferBeginInfo& vkCommandBufferBeginInfo)
    {
        VkResult vkResult = vkBeginCommandBuffer(*this, &vkCommandBufferBeginInfo);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return *this;
    }

    CommandBuffer& beginOneTimeSubmit()
    {
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(*this, &beginInfo);
        return *this;
    }

    CommandBuffer& end()
    {
        VkResult vkResult = vkEndCommandBuffer(*this);
        if (vkResult != VK_SUCCESS) {
            throw Exception(vkResult);
        }
        return *this;
    }

    CommandBuffer& cmdBeginRendering(const VkRenderingInfo& vkRenderingInfo)
    {
        vkCmdBeginRendering(*this, &vkRenderingInfo);
        return *this;
    }

    CommandBuffer& cmdEndRendering()
    {
        vkCmdEndRendering(*this);
        return *this;
    }

    CommandBuffer& cmdCopyBufferToImage(
        Buffer<> buffer,
        Image image,
        uint32_t width,
        uint32_t height)
    {
        BufferImageCopy bufferImageCopy;
        bufferImageCopy.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(*this, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
        return *this;
    }

    CommandBuffer& cmdCopyBufferToImage(
        Buffer<> buffer,
        Image image,
        VkImageLayout vkImageLayout,
        uint32_t regionCount,
        const VkBufferImageCopy* pRegions)
    {
        vkCmdCopyBufferToImage(*this, buffer, image, vkImageLayout, regionCount, pRegions);
        return *this;
    }

    //	TODO: do we need to implement cmdCopyBuffer2?
    CommandBuffer& cmdCopyBuffer(
        Buffer<> srcBuffer,
        Buffer<> dstBuffer,
        VkDeviceSize size)
    {
        //	TODO: maybe do some size checking on the destination to avoid overwriting.
        VkBufferCopy vkBufferCopy { .srcOffset = 0, .dstOffset = 0, .size = size };

        vkCmdCopyBuffer(*this, srcBuffer, dstBuffer, 1, &vkBufferCopy);
        return *this;
    }

    CommandBuffer& cmdPipelineBarrier2(DependencyInfo& dependencyInfo)
    {
        vkCmdPipelineBarrier2(*this, &dependencyInfo);
        return *this;
    }

    CommandBuffer& cmdBeginRenderPass(const VkRenderPassBeginInfo& vkRenderPassBeginInfo)
    {
        vkCmdBeginRenderPass(*this, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    CommandBuffer& cmdNextSubpass()
    {
        vkCmdNextSubpass(*this, VK_SUBPASS_CONTENTS_INLINE);
        return *this;
    }

    CommandBuffer& cmdEndRenderPass()
    {
        vkCmdEndRenderPass(*this);
        return *this;
    }

    // CommandBuffer& cmdSetViewport(VkExtent2D vkExtent2D)
    //{
    //     Viewport viewport;
    //     viewport.width = static_cast<float>(vkExtent2D.width);
    //     viewport.height = static_cast<float>(vkExtent2D.height);
    //     viewport.minDepth = 0.0f;
    //     viewport.maxDepth = 1.0f;
    //     vkCmdSetViewport(*this, 0, 1, &viewport);
    //     return *this;
    // }

    template <typename ArgT>
    CommandBuffer& cmdSetViewport(ArgT width, ArgT height)
    {
        Viewport viewport;
        viewport
            .setWidthHeight(width, height)
            .setMinMaxDepth(0.0, 1.0);
        vkCmdSetViewport(*this, 0, 1, &viewport);
        return *this;
    }

    template <typename ArgT>
    CommandBuffer& cmdSetViewport(
        ArgT x, ArgT y,
        ArgT width, ArgT height)
    {
        Viewport viewport;
        viewport
            .setXY(x, y)
            .setWidthHeight(width, height)
            .setMinMaxDepth(0.0, 1.0);
        vkCmdSetViewport(*this, 0, 1, &viewport);
        return *this;
    }

    CommandBuffer& cmdSetViewport(const VkViewport& vkViewport)
    {
        vkCmdSetViewport(*this, 0, 1, &vkViewport);
        return *this;
    }

    CommandBuffer& cmdSetScissor(VkExtent2D vkExtent2D)
    {
        VkRect2D scissor {};
        scissor.extent = vkExtent2D;
        vkCmdSetScissor(*this, 0, 1, &scissor);
        return *this;
    }

    CommandBuffer& cmdSetScissor(const VkRect2D& scissor)
    {
        vkCmdSetScissor(*this, 0, 1, &scissor);
        return *this;
    }

    CommandBuffer& cmdSetScissor(uint32_t width, uint32_t height)
    {
        VkRect2D scissor {};
        scissor.extent.width = width;
        scissor.extent.height = height;
        vkCmdSetScissor(*this, 0, 1, &scissor);
        return *this;
    }

    CommandBuffer& cmdBindPipeline(VkPipeline vkPipeline)
    {
        vkCmdBindPipeline(*this, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
        return *this;
    }

    CommandBuffer& cmdBindComputePipeline(VkPipeline vkPipeline)
    {
        vkCmdBindPipeline(*this, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);
        return *this;
    }

    CommandBuffer& cmdBindDescriptorSet(
        VkDescriptorSet vkDescriptorSet,
        VkPipelineLayout vkPipelineLayout)
    {
        vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipelineLayout, 0, 1, &vkDescriptorSet, 0, nullptr);
        return *this;
    }

    CommandBuffer& cmdBindComputeDescriptorSet(
        VkDescriptorSet vkDescriptorSet,
        VkPipelineLayout vkPipelineLayout)
    {
        vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_COMPUTE,
            vkPipelineLayout, 0, 1, &vkDescriptorSet, 0, nullptr);
        return *this;
    }

    CommandBuffer& cmdBindDescriptorSetDynamicOffset(
        VkDescriptorSet vkDescriptorSet,
        VkPipelineLayout vkPipelineLayout,
        uint32_t dynamicOffset)
    {
        vkCmdBindDescriptorSets(*this, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipelineLayout, 0, 1, &vkDescriptorSet, 1, &dynamicOffset);
        return *this;
    }

    CommandBuffer& cmdBindVertexBuffer(VkBuffer vkBuffer)
    {
        VkDeviceSize offsets[1] { 0 };
        vkCmdBindVertexBuffers(*this, 0, 1, &vkBuffer, offsets);
        return *this;
    }

    CommandBuffer& cmdBindIndexBuffer(VkBuffer vkBuffer, VkIndexType vkIndexType)
    {
        vkCmdBindIndexBuffer(*this, vkBuffer, 0, vkIndexType);
        return *this;
    }

    //	std::vector.size() is used as index count a lot, so use size_t as the parameter type.
    CommandBuffer& cmdDrawIndexed(size_t indexCount)
    {
        vkCmdDrawIndexed(*this, static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
        return *this;
    }

    CommandBuffer& cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount)
    {
        vkCmdDrawIndexed(*this, indexCount, instanceCount, 0, 0, 0);
        return *this;
    }

    CommandBuffer& cmdDraw(uint32_t vertexCount, uint32_t indexCount)
    {
        vkCmdDraw(*this, vertexCount, indexCount, 0, 0);
        return *this;
    }

    CommandBuffer& cmdPushConstant(
        void* pData,
        uint32_t size,
        VkPipelineLayout vkPipelineLayout,
        VkShaderStageFlagBits vkShaderStage)
    {
        vkCmdPushConstants(
            *this,
            vkPipelineLayout,
            vkShaderStage,
            0,
            size,
            pData);

        return *this;
    }

    CommandBuffer& cmdPipelineBarrierImageMemory(
        VkImage vkImage,
        VkAccessFlags vkSrcAccessMask,
        VkAccessFlags vkDstAccessMask,
        VkImageLayout vkImageLayoutOld,
        VkImageLayout vkImageLayoutNew,
        VkPipelineStageFlags vkSrcStageMask,
        VkPipelineStageFlags vkDstStageMask,
        const ImageSubresourceRange& imageSubresourceRange)
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
        return *this;
    }

    CommandBuffer& cmdPipelineBarrierImageMemory(
        const ImageMemoryBarrier& imageMemoryBarrier,
        VkPipelineStageFlags vkSrcStageMask,
        VkPipelineStageFlags vkDstStageMask)
    {
        vkCmdPipelineBarrier(
            *this,
            vkSrcStageMask,
            vkDstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);

        return *this;
    }

    CommandBuffer& cmdPushDescriptorSet(
        WriteDescriptorSetArray& writeDescriptorSetArray,
        uint32_t descriptorSetNumber,
        VkPipelineLayout vkPipelineLayout)
    {
        vkCmdPushDescriptorSet(
            *this,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipelineLayout,
            descriptorSetNumber,
            writeDescriptorSetArray.vkWriteDescriptorSetsSize(),
            writeDescriptorSetArray.vkWriteDescriptorSetsData());
        return *this;
    }

    CommandBuffer& cmdDispatch(
        uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ)
    {
        vkCmdDispatch(*this, groupCountX, groupCountY, groupCountZ);
        return *this;
    }
};

}
