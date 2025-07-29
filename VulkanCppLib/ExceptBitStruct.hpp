#pragma once

#include <exception>

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

    VkResult vkResult() const
    {
        return m_vkResult;
    }
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

class NullPointerException : public Exception {

public:
    NullPointerException()
        : Exception(VK_INCOMPLETE)
    {
    }
};

//	Yikes! Vulkan uses enums for bit values but uses
//	non-typesafe uints for the combination of flags.
//	The newer flags use 64 bit uints for values and combinations
//	but again, not typesafe.
//
//	Vulkan 32 bit flags use enums for the bit values and uint32_t
//	for the combination, or flags values.  Since the flags type, uint32_t,
//	is not distinguishable, incorrect flags can be passed in to functions.
//	(Ask me how I know!) For example, VkMemoryPropertyFlags is the
//	same type as VkShaderStageFlags since both are uint32_t.
//
//	The enums for the bit values are unique however.  This means that
//	the pair <BitType, CombinationType> is unique and can be used to
//	differentiate flags values for different bit types.
//
//	Vulkan 64 bit flags pose a different problem. Both the bit values
//	and the combination flag values are uint64_t.  This means
//	that any <BitType, CombinationType> is indinguishable from
//	and other.
//
//	To fix this, a third type is added to the template type parameters.
//	This third type (class) doesn't need to do anything.  It just needs to be
//  unique for each 64 bit flag usage.  <BitType, CombinationType, IdType>
//  is now unique.
//
//	Since 32 bit flags don't need the IdType for uniqueness, a single
//	default 32 bit class id is used for them to simplify things a bit.
//
//	using declarations are used to simplify the names of the templated bit types.
//
//	Once the unique types are available, the "new" bit values are created
//	as static const values in the vkcpp namespace, similar to what Vulkan
//	does with 64 bit values.
//
//	Macro games are used where possible to reduce the noise of creating the
//	new bit values.
//
//	Not all Vulkan bit values have necessarily been converted.  So far,
//	conversions have been driven by need and ease of conversion.

//	TODO: this is really just the bare minimum bit operations.
class Default32BitClassId {
};

template <
    typename VkBit_t,
    typename VkCombination_t,
    typename Id_t = Default32BitClassId>
class Bitset {

public:
    VkCombination_t m_value;

    explicit Bitset(VkBit_t value)
        : m_value(value)
    {
    }

    explicit operator VkCombination_t()
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

    friend bool bitsSet(VkCombination_t allBits, Bitset requiredBits)
    {
        return (allBits & requiredBits.m_value) == requiredBits.m_value;
    }

    bool bitsSet(Bitset requiredBits)
    {
        return m_value & requiredBits.m_value;
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

    friend Bitset operator&(const Bitset a, const VkCombination_t b)
    {
        Bitset val = a;
        val.m_value &= b;
        return val;
    }
};

using MemoryPropertyFlags = Bitset<VkMemoryPropertyFlagBits, VkMemoryPropertyFlags>;
#define MemoryPropertyFlagsValue(VKCPP_NAME) \
    static const MemoryPropertyFlags VKCPP_NAME(VK_##VKCPP_NAME##_BIT)

MemoryPropertyFlagsValue(MEMORY_PROPERTY_DEVICE_LOCAL);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_HOST_VISIBLE);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_HOST_COHERENT);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_HOST_CACHED);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_LAZILY_ALLOCATED);
MemoryPropertyFlagsValue(MEMORY_PROPERTY_PROTECTED);
//	Handy value used all the time for staging buffers.
static const MemoryPropertyFlags MEMORY_PROPERTY_HOST_VISIBLE_COHERENT
    = vkcpp::MEMORY_PROPERTY_HOST_VISIBLE | vkcpp::MEMORY_PROPERTY_HOST_COHERENT;

using ShaderStageFlags = Bitset<VkShaderStageFlagBits, VkShaderStageFlags>;
#define ShaderStageFlagsValue(VKCPP_NAME) \
    static const ShaderStageFlags VKCPP_NAME(VK_##VKCPP_NAME##_BIT)

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


class PipelineStageFlags2Id
{
};
using PipelineStageFlags2 = Bitset<VkPipelineStageFlagBits2, VkPipelineStageFlagBits2, PipelineStageFlags2Id>;

#define PipelineStageFlags2Value(VKCPP_NAME) \
    static const PipelineStageFlags2 VKCPP_NAME(VK_##VKCPP_NAME##_BIT)

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

//	Some Vulkan structures are embedded in other structures
//	rather than being standalone.  We would like to have
//	the advantages of the smarter structure, but can't redefine
//  the embedded structure.  This template allows us to take
//	an existing structure/object and make it look like the
//	smarter structure.  Operations on the smarter variable
//	actually modify the existing object.  This is a type of
//	downcast.  Of course, this only works when the smarter object
//	is just a simple extension.  If the smarter object added
//	any member data, we might trash memory beyond the existing
//	object.
//	TODO: is there some way to use templatey stuff to guarantee equal sizes?
//
//	TODO: This did not turn out as useful as expected.
//	Using smart objects with useful functions has been
//	pretty effective in reducing the need for standalone
//	structure littering the code.
template <typename Real_t, typename ActsLike_t>
	requires(sizeof(Real_t) == sizeof(ActsLike_t))
ActsLike_t& smartenUp(Real_t& real) {
	ActsLike_t* p = static_cast<ActsLike_t*>(&real);
	return *p;
}

class Extent2D : public VkExtent2D
{

public:
	Extent2D()
		: VkExtent2D{} {
	}

	Extent2D(uint32_t widthArg, uint32_t heightArg) {
		width = widthArg;
		height = heightArg;
	}

	template <typename Arg_t>
	Extent2D setWidthHeight(
		Arg_t widthArg,
		Arg_t heightArg) {
		width = static_cast<Arg_t>(widthArg);
		height = static_cast<Arg_t>(heightArg);
		return *this;
	}

	template <typename Arg_t>
	Extent2D& setWidth(Arg_t widthArg) {
		width = static_cast<Arg_t>(widthArg);
		return *this;
	}

	template <typename Arg_t>
	Extent2D& setHeight(Arg_t heightArg) {
		height = static_cast<Arg_t>(heightArg);
		return *this;
	}
};
static_assert(sizeof(Extent2D) == sizeof(VkExtent2D));
template Extent2D& smartenUp<VkExtent2D, Extent2D>(VkExtent2D&);

// class Extent3D : public VkExtent3D { };
//
// class Offset2D : public VkOffset2D { };
//
// class Offset3D : public VkOffset3D { };

// class Rect2D : public VkRect2D {
//
// public:
//     Rect2D()
//         : VkRect2D {}
//     {
//     }
//
//     Rect2D(VkOffset2D vkOffset2D, VkExtent2D vkExtent2D)
//     {
//         offset = vkOffset2D;
//         extent = vkExtent2D;
//     }
//
//     Rect2D(VkExtent2D vkExtent2D)
//     {
//         offset.x = 0;
//         offset.y = 0;
//         extent = vkExtent2D;
//     }
// };
// static_assert(sizeof(Rect2D) == sizeof(VkRect2D));
// template Rect2D& smartenUp<VkRect2D, Rect2D>(VkRect2D&);

class Viewport : public VkViewport
{

public:
	Viewport()
		: VkViewport{} {
		maxDepth = 1.0;
	}

	Viewport& setWidthHeight(
		float widthArg,
		float heightArg) {
		width = widthArg;
		height = heightArg;
		return *this;
	}

	Viewport& setX(float xArg) {
		x = xArg;
		return *this;
	}
};



}
