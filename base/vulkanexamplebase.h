/*
* Vulkan Example base class
*
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <ShellScalingAPI.h>
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>
#include "VulkanAndroid.h"
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
#include <directfb.h>
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#elif defined(_DIRECT2DISPLAY)
//
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include <xcb/xcb.h>
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
#include <TargetConditionals.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <numeric>
#include <array>

#include <VulkanCpp.hpp>

#include "CommandLineParser.hpp"
#include "keycodes.hpp"
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanUIOverlay.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"

#include "VulkanInitializers.hpp"
#include "camera.hpp"
#include "benchmark.hpp"

class VulkanExampleBase
{

protected:

	vkcpp::VulkanInstance	m_vulkanInstance;
	vkcpp::PhysicalDevice	m_physicalDevice;
	vkcpp::Device			m_device;

private:

	uint32_t m_destWidth {};
    uint32_t m_destHeight {};
	bool m_resizing = false;
    std::string m_shaderDir = "glsl";

    std::string getWindowTitle() const;
	void handleMouseMove(int32_t x, int32_t y);
	void nextFrame();
	void updateOverlay();
	void createPipelineCache();
	void createCommandPool();
	void createSynchronizationPrimitives();
	void createSurface();
	void createSwapChain();
	void createCommandBuffers();
	void destroyCommandBuffers();

protected:

	// Returns the path to the root of the glsl, hlsl or slang shader directory.
	std::string getShadersPath() const;


	// Frame counter to display fps
	uint32_t m_frameCounter = 0;
	uint32_t m_lastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTimestamp;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_tPrevEnd;

	std::vector<std::string> m_supportedInstanceExtensions;
   
	VkPhysicalDeviceMemoryProperties m_vkPhysicalDeviceMemoryProperties{};

	std::vector<const char*> m_requestedDeviceExtensions;

	std::vector<const char*> m_requestedInstanceExtensions;

	std::vector<VkLayerSettingEXT> m_requestedLayerSettings;

	vkcpp::Queue m_queue;

	VkFormat m_vkFormatDepth{VK_FORMAT_UNDEFINED};

	vkcpp::CommandPool m_commandPool;
	std::vector<vkcpp::CommandBuffer> m_drawCommandBuffers;

	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo m_vkSubmitInfo{};


	// Global render pass for frame buffer writes
	//VkRenderPass m_vkRenderPass{ VK_NULL_HANDLE };
	vkcpp::RenderPass m_renderPassOriginal;

	std::vector<VkFramebuffer>m_vkFrameBuffers;

	uint32_t m_currentBufferIndex = 0;

	vkcpp::DescriptorPool m_descriptorPool;

	std::vector<VkShaderModule> m_vkShaderModules;

	VkPipelineCache m_vkPipelineCache{ VK_NULL_HANDLE };

	VulkanSwapChain m_swapChain;

	// Synchronization semaphores
	struct {
		// Swap chain m_vkImage presentation
		VkSemaphore m_vkSemaphorePresentComplete;
		// Command buffer submission and execution
		VkSemaphore m_vkSemaphoreRenderComplete;
	} semaphores{};

	std::vector<VkFence> m_vkFences;

	bool m_requiresStencil{ false };

public:

	bool m_prepared = false;
	bool m_resized = false;
	bool m_viewUpdated = false;
	uint32_t m_drawAreaWidth = 1280;
	uint32_t m_drawAreaHeight = 720;

	vks::UIOverlay m_UIOverlay;
	CommandLineParser m_commandLineParser;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float m_frameTimer = 1.0f;

	vks::Benchmark m_benchmark;

	/** @brief Encapsulated physical and logical vulkan device */
	vks::VulkanDevice *m_pVulkanDevice{};

	/** @brief Example settings that can be changed e.g. by command line arguments */
	struct ExampleSettings {
		/** @brief Activates validation layers (and message output) when set to true */
		//	Always use validation layers. These are examples.
//		bool m_useValidationLayers = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool m_fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool m_forceSwapChainVsync = false;
		/** @brief Enable UI overlay */
		bool m_showUIOverlay = true;
	} m_exampleSettings;

	/** @brief State of gamepad input (only used on Android) */
	struct {
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;

	/** @brief State of mouse/touch input */
	struct {
		struct {
			bool left = false;
			bool right = false;
			bool middle = false;
		} buttons;
		glm::vec2 position;
	} mouseState;

	VkClearColorValue m_vkClearColorValueDefault = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	static std::vector<const char*> args;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	bool paused = false;

	Camera camera;

	std::string title = "Vulkan Example";
	std::string name = "vulkanExample";
	uint32_t m_requestedApiVersion = VK_API_VERSION_1_0;

	///** @brief Default depth stencil attachment used by the default render pass */
	//struct {
	//	vkcpp::Image m_image;
	//	vkcpp::DeviceMemory<> m_deviceMemory;
	//	vkcpp::ImageView m_imageView;
	//} m_defaultDepthStencil{};

	vkcpp::Image_Memory_View m_depthStencilDefault;

	// OS specific
	HWND m_hwnd;
	HINSTANCE m_hinstance;

	/** @brief Default base class constructor */
	VulkanExampleBase();
	virtual ~VulkanExampleBase();
	VulkanExampleBase(const VulkanExampleBase&) = delete;
	VulkanExampleBase& operator=(const VulkanExampleBase&) = delete;
    VulkanExampleBase(VulkanExampleBase&&) noexcept = delete;
    VulkanExampleBase& operator=(VulkanExampleBase&&) noexcept = delete;

	void setCommandLineOptions();
	/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
	bool initVulkan();


	void setupConsole(std::string title);
	void setupDPIAwareness();
	HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
	void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	/** @brief (Pure virtual) Render function to be implemented by the sample application */
	virtual void render() = 0;
	/** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
	virtual void keyPressed(uint32_t);
	/** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
	virtual void mouseMoved(double x, double y, bool &handled);
	/** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
	virtual void windowResized();
	/** @brief (Virtual) Called when resources have been recreated that require a rebuild of the command buffers (e.g. frame buffer), to be implemented by the sample application */
	virtual void buildCommandBuffers();
	/** @brief (Virtual) Setup default depth and stencil views */
	virtual void setupDepthStencil();
	/** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
	virtual void setupFrameBuffer();
	/** @brief (Virtual) Setup a default renderpass */
	virtual void setupRenderPass();
	/** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
	virtual void getEnabledFeatures();
	/** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
	virtual void getEnabledExtensions();

	/** @brief Prepares all Vulkan resources and functions required to run the sample */
	virtual void prepare();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	void windowResize();

	/** @brief Entry point for the main render loop */
	void renderLoop();

	/** @brief Adds the drawing commands for the ImGui overlay to the given command buffer */
	void drawUI(const VkCommandBuffer commandBuffer);

	/** Prepare the next frame for workload submission by acquiring the next swap chain m_vkImage */
	void prepareFrame();
	/** @brief Presents the current image to the swap chain */
	void submitFrame();
	/** @brief (Virtual) Default image acquire + submission and command buffer submission function */
	virtual void renderFrame();

	/** @brief (Virtual) Called when the UI overlay is updating, can be used to add custom elements to the overlay */
	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay);

#if defined(_WIN32)
	virtual void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
};

#include "Entrypoints.h"