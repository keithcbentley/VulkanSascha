/*
 * Vulkan Example base class
 *
 * Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "vulkanexamplebase.h"

std::vector<const char*> VulkanExampleBase::args;

void VulkanExampleBase::renderFrame()
{
    VulkanExampleBase::prepareFrame();
    m_vkSubmitInfo.commandBufferCount = 1;
    m_vkSubmitInfo.pCommandBuffers = &drawCmdBuffers[m_currentBufferIndex];
    VK_CHECK_RESULT(vkQueueSubmit(m_vkQueue, 1, &m_vkSubmitInfo, VK_NULL_HANDLE));
    VulkanExampleBase::submitFrame();
}

std::string VulkanExampleBase::getWindowTitle() const
{
    std::string windowTitle { title + " - " + m_physicalDeviceProperties.m_properties2.properties.deviceName };
    if (!m_exampleSettings.m_showUIOverlay) {
        windowTitle += " - " + std::to_string(m_frameCounter) + " fps";
    }
    return windowTitle;
}

void VulkanExampleBase::createCommandBuffers()
{
    // Create one command buffer for each swap chain m_vkImage
    drawCmdBuffers.resize(m_swapChain.images.size());
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(m_vkCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(drawCmdBuffers.size()));
    VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VulkanExampleBase::destroyCommandBuffers()
{
    vkFreeCommandBuffers(
        m_device,
        m_vkCommandPool,
        static_cast<uint32_t>(drawCmdBuffers.size()),
        drawCmdBuffers.data());
}

std::string VulkanExampleBase::getShadersPath() const
{
    return getShaderBasePath() + m_shaderDir + "/";
}

void VulkanExampleBase::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK_RESULT(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_vkPipelineCache));
}

void VulkanExampleBase::prepare()
{
    createSurface();
    createCommandPool();
    createSwapChain();
    createCommandBuffers();
    createSynchronizationPrimitives();
    setupDepthStencil();
    setupRenderPass();
    createPipelineCache();
    setupFrameBuffer();
    m_exampleSettings.m_showUIOverlay = m_exampleSettings.m_showUIOverlay && (!m_benchmark.active);
    if (m_exampleSettings.m_showUIOverlay) {
        m_UIOverlay.device = m_pVulkanDevice;
        m_UIOverlay.queue = m_vkQueue;
        m_UIOverlay.shaders = {
            loadShader(getShadersPath() + "base/uioverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
            loadShader(getShadersPath() + "base/uioverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
        };
        m_UIOverlay.prepareResources();
        m_UIOverlay.preparePipeline(m_vkPipelineCache, m_renderPassOriginal, m_swapChain.colorFormat, m_vkFormatDepth);
    }
}

VkPipelineShaderStageCreateInfo VulkanExampleBase::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), m_vkDevice);
#else
    shaderStage.module = vks::tools::loadShader(fileName.c_str(), m_device);
#endif
    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    m_vkShaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void VulkanExampleBase::nextFrame()
{
    auto tStart = std::chrono::high_resolution_clock::now();
    if (m_viewUpdated) {
        m_viewUpdated = false;
    }

    render();
    m_frameCounter++;
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    m_frameTimer = (float)tDiff / 1000.0f;
    camera.update(m_frameTimer);
    if (camera.moving()) {
        m_viewUpdated = true;
    }
    // Convert to clamped timer value
    if (!paused) {
        timer += timerSpeed * m_frameTimer;
        if (timer > 1.0) {
            timer -= 1.0f;
        }
    }
    float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - m_lastTimestamp).count());
    if (fpsTimer > 1000.0f) {
        m_lastFPS = static_cast<uint32_t>((float)m_frameCounter * (1000.0f / fpsTimer));
        if (!m_exampleSettings.m_showUIOverlay) {
            std::string windowTitle = getWindowTitle();
            SetWindowText(m_hwnd, windowTitle.c_str());
        }
        m_frameCounter = 0;
        m_lastTimestamp = tEnd;
    }
    m_tPrevEnd = tEnd;

    updateOverlay();
}

void VulkanExampleBase::renderLoop()
{

    m_destWidth = m_drawAreaWidth;
    m_destHeight = m_drawAreaHeight;
    m_lastTimestamp = std::chrono::high_resolution_clock::now();
    m_tPrevEnd = m_lastTimestamp;

    MSG msg;
    bool quitMessageReceived = false;
    while (!quitMessageReceived) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                quitMessageReceived = true;
                break;
            }
        }
        if (m_prepared && !IsIconic(m_hwnd)) {
            nextFrame();
        }
    }
    // Flush m_vkDevice to make sure all resources can be freed
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

void VulkanExampleBase::updateOverlay()
{
    if (!m_exampleSettings.m_showUIOverlay)
        return;

    // The overlay does not need to be updated with each frame, so we limit the update rate
    // Not only does this save performance but it also makes display of fast changig values like fps more stable
    m_UIOverlay.updateTimer -= m_frameTimer;
    if (m_UIOverlay.updateTimer >= 0.0f) {
        return;
    }
    // Update at max. rate of 30 fps
    m_UIOverlay.updateTimer = 1.0f / 30.0f;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)m_drawAreaWidth, (float)m_drawAreaHeight);
    io.DeltaTime = m_frameTimer;

    io.MousePos = ImVec2(mouseState.position.x, mouseState.position.y);
    io.MouseDown[0] = mouseState.buttons.left && m_UIOverlay.visible;
    io.MouseDown[1] = mouseState.buttons.right && m_UIOverlay.visible;
    io.MouseDown[2] = mouseState.buttons.middle && m_UIOverlay.visible;

    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10 * m_UIOverlay.scale, 10 * m_UIOverlay.scale));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Vulkan Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(title.c_str());
    ImGui::TextUnformatted(m_physicalDeviceProperties.m_properties2.properties.deviceName);
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / m_lastFPS), m_lastFPS);

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 5.0f * m_UIOverlay.scale));
#endif
    ImGui::PushItemWidth(110.0f * m_UIOverlay.scale);
    OnUpdateUIOverlay(&m_UIOverlay);
    ImGui::PopItemWidth();
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    ImGui::PopStyleVar();
#endif

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();

    if (m_UIOverlay.update() || m_UIOverlay.updated) {
        buildCommandBuffers();
        m_UIOverlay.updated = false;
    }

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    if (mouseState.buttons.left) {
        mouseState.buttons.left = false;
    }
#endif
}

void VulkanExampleBase::drawUI(const VkCommandBuffer commandBuffer)
{
    if (m_exampleSettings.m_showUIOverlay && m_UIOverlay.visible) {
        const VkViewport viewport = vks::initializers::viewport((float)m_drawAreaWidth, (float)m_drawAreaHeight, 0.0f, 1.0f);
        const VkRect2D scissor = vks::initializers::rect2D(m_drawAreaWidth, m_drawAreaHeight, 0, 0);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        m_UIOverlay.draw(commandBuffer);
    }
}

void VulkanExampleBase::prepareFrame()
{
    // Acquire the next m_vkImage from the swap chain
    VkResult result = m_swapChain.acquireNextImage(semaphores.m_vkSemaphorePresentComplete, m_currentBufferIndex);
    // Recreate the swapchain if it's no longer compatible with the m_vkSurface (OUT_OF_DATE)
    // SRS - If no longer optimal (VK_SUBOPTIMAL_KHR), wait until submitFrame() in case number of swapchain images will change on resize
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            windowResize();
        }
        return;
    } else {
        VK_CHECK_RESULT(result);
    }
}

void VulkanExampleBase::submitFrame()
{
    VkResult result = m_swapChain.queuePresent(m_vkQueue, m_currentBufferIndex, semaphores.m_vkSemaphoreRenderComplete);
    // Recreate the swapchain if it's no longer compatible with the m_vkSurface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
        windowResize();
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return;
        }
    } else {
        VK_CHECK_RESULT(result);
    }
    VK_CHECK_RESULT(vkQueueWaitIdle(m_vkQueue));
}

void VulkanExampleBase::setCommandLineOptions()
{
    m_commandLineParser.add("help", { "--help" }, 0, "Show help");
    m_commandLineParser.add("validation", { "-v", "--validation" }, 0, "Enable validation layers");
    m_commandLineParser.add("validationlogfile", { "-vl", "--validationlogfile" }, 0, "Log validation messages to a textfile");
    m_commandLineParser.add("vsync", { "-vs", "--vsync" }, 0, "Enable V-Sync");
    m_commandLineParser.add("fullscreen", { "-f", "--fullscreen" }, 0, "Start in fullscreen mode");
    m_commandLineParser.add("m_drawAreaWidth", { "-w", "--m_drawAreaWidth" }, 1, "Set m_hwnd m_drawAreaWidth");
    m_commandLineParser.add("m_drawAreaHeight", { "-h", "--m_drawAreaHeight" }, 1, "Set m_hwnd m_drawAreaHeight");
    m_commandLineParser.add("shaders", { "-s", "--shaders" }, 1, "Select shader type to use (gls, hlsl or slang)");
    m_commandLineParser.add("gpuselection", { "-g", "--gpu" }, 1, "Select GPU to run on");
    m_commandLineParser.add("gpulist", { "-gl", "--listgpus" }, 0, "Display a list of available Vulkan devices");
    m_commandLineParser.add("m_benchmark", { "-b", "--m_benchmark" }, 0, "Run example in m_benchmark mode");
    m_commandLineParser.add("benchmarkwarmup", { "-bw", "--benchwarmup" }, 1, "Set warmup time for m_benchmark mode in seconds");
    m_commandLineParser.add("benchmarkruntime", { "-br", "--benchruntime" }, 1, "Set duration time for m_benchmark mode in seconds");
    m_commandLineParser.add("benchmarkresultfile", { "-bf", "--benchfilename" }, 1, "Set file name for m_benchmark results");
    m_commandLineParser.add("benchmarkresultframes", { "-bt", "--benchframetimes" }, 0, "Save frame times to m_benchmark results file");
    m_commandLineParser.add("benchmarkframes", { "-bfs", "--benchmarkframes" }, 1, "Only render the given number of frames");
#if (!(defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT)))
    m_commandLineParser.add("resourcepath", { "-rp", "--resourcepath" }, 1, "Set path for dir where assets and shaders folder is present");
#endif
    m_commandLineParser.parse(args);
    if (m_commandLineParser.isSet("help")) {
#if defined(_WIN32)
        setupConsole("Vulkan example");
#endif
        m_commandLineParser.printHelp();
        std::cin.get();
        exit(0);
    }
    // if (m_commandLineParser.isSet("validation")) {
    //     m_exampleSettings.m_useValidationLayers = true;
    // }
    // if (m_commandLineParser.isSet("validationlogfile")) {
    //     vks::debug::logToFile = true;
    // }
    if (m_commandLineParser.isSet("vsync")) {
        m_exampleSettings.m_forceSwapChainVsync = true;
    }
    if (m_commandLineParser.isSet("m_drawAreaHeight")) {
        m_drawAreaHeight = m_commandLineParser.getValueAsInt("m_drawAreaHeight", m_drawAreaHeight);
    }
    if (m_commandLineParser.isSet("m_drawAreaWidth")) {
        m_drawAreaWidth = m_commandLineParser.getValueAsInt("m_drawAreaWidth", m_drawAreaWidth);
    }
    if (m_commandLineParser.isSet("fullscreen")) {
        m_exampleSettings.m_fullscreen = true;
    }
    if (m_commandLineParser.isSet("shaders")) {
        std::string value = m_commandLineParser.getValueAsString("shaders", "glsl");
        if ((value != "glsl") && (value != "hlsl") && (value != "slang")) {
            std::cerr << "Shader type must be one of 'glsl', 'hlsl' or 'slang'\n";
        } else {
            m_shaderDir = value;
        }
    }
    if (m_commandLineParser.isSet("m_benchmark")) {
        m_benchmark.active = true;
        vks::tools::errorModeSilent = true;
    }
    if (m_commandLineParser.isSet("benchmarkwarmup")) {
        m_benchmark.warmup = m_commandLineParser.getValueAsInt("benchmarkwarmup", 0);
    }
    if (m_commandLineParser.isSet("benchmarkruntime")) {
        m_benchmark.duration = m_commandLineParser.getValueAsInt("benchmarkruntime", m_benchmark.duration);
    }
    if (m_commandLineParser.isSet("benchmarkresultfile")) {
        m_benchmark.filename = m_commandLineParser.getValueAsString("benchmarkresultfile", m_benchmark.filename);
    }
    if (m_commandLineParser.isSet("benchmarkresultframes")) {
        m_benchmark.outputFrameTimes = true;
    }
    if (m_commandLineParser.isSet("benchmarkframes")) {
        m_benchmark.outputFrames = m_commandLineParser.getValueAsInt("benchmarkframes", m_benchmark.outputFrames);
    }
    if (m_commandLineParser.isSet("resourcepath")) {
        vks::tools::resourcePath = m_commandLineParser.getValueAsString("resourcepath", "");
    }
}

VulkanExampleBase::VulkanExampleBase()
{
    // Command line arguments
    setCommandLineOptions();

    // Check for a valid asset path
    struct stat info;
    if (stat(getAssetPath().c_str(), &info) != 0) {
        std::string msg = "Could not locate asset path in \"" + getAssetPath() + "\" !";
        MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
        exit(-1);
    }

    // Validation for all samples can be forced at compile time using the FORCE_VALIDATION define
#if defined(FORCE_VALIDATION)
    settings.validation = true;
#endif

#if defined(_WIN32)
    // Enable console if validation is active, debug message callback will output to it
    // if (this->m_exampleSettings.m_useValidationLayers) {
    //    setupConsole("Vulkan example");
    //}
    setupConsole("Vulkan example");

    setupDPIAwareness();
#endif
}

VulkanExampleBase::~VulkanExampleBase()
{
    // Clean up Vulkan resources
    m_swapChain.cleanup();
    destroyCommandBuffers();

    for (auto& frameBuffer : m_vkFrameBuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }

    for (auto& shaderModule : m_vkShaderModules) {
        vkDestroyShaderModule(m_device, shaderModule, nullptr);
    }
    //    vkDestroyImageView(m_deviceOriginal, m_defaultDepthStencil.m_vkImageView, nullptr);
    //    vkDestroyImage(m_deviceOriginal, m_defaultDepthStencil.m_vkImage, nullptr);
    //    vkFreeMemory(m_deviceOriginal, m_defaultDepthStencil.m_vkDeviceMemory, nullptr);

    vkDestroyPipelineCache(m_device, m_vkPipelineCache, nullptr);

    vkDestroyCommandPool(m_device, m_vkCommandPool, nullptr);

    vkDestroySemaphore(m_device, semaphores.m_vkSemaphorePresentComplete, nullptr);
    vkDestroySemaphore(m_device, semaphores.m_vkSemaphoreRenderComplete, nullptr);
    for (auto& fence : m_vkFences) {
        vkDestroyFence(m_device, fence, nullptr);
    }

    if (m_exampleSettings.m_showUIOverlay) {
        m_UIOverlay.freeResources();
    }

    delete m_pVulkanDevice;

    // if (m_exampleSettings.m_useValidationLayers) {
    //     vks::debug::freeDebugCallback(m_vulkanInstance);
    // }

    // vkDestroyInstance(m_vulkanInstance, nullptr);
}

bool VulkanExampleBase::initVulkan()
{
    m_vulkanInstance = vkcpp::vulkanInstance();
    m_physicalDevice = vkcpp::physicalDevice();
    m_device = vkcpp::device();

    // Store m_vkPhysicalDeviceProperties (including limits),
    // m_vkPhysicalDeviceFeatures and m_vkDeviceMemory m_vkPhysicalDeviceProperties
    // of the physical m_vkDevice (so that examples can check against them)

    m_physicalDeviceFeatures = m_physicalDevice.getPhysicalDeviceFeatures2();
    m_physicalDeviceProperties = m_physicalDevice.getPhysicalDeviceProperties2();
    m_vkPhysicalDeviceMemoryProperties = m_physicalDevice.getPhysicalDeviceMemoryProperties();

    // Derived examples can override this to set actual m_vkPhysicalDeviceFeatures (based on above readings) to enable for logical m_vkDevice creation
    getEnabledFeatures();

    // Vulkan m_vkDevice creation
    // This is handled by a separate class that gets a logical m_vkDevice representation
    // and encapsulates functions related to a m_vkDevice
    m_pVulkanDevice = new vks::VulkanDevice(m_physicalDevice, m_device);

    // Derived examples can enable extensions based on the list of supported extensions read from the physical m_vkDevice
    getEnabledExtensions();

    // Get a graphics m_vkQueue from the m_vkDevice
    vkGetDeviceQueue(m_device, m_pVulkanDevice->m_queueFamilyIndices.m_graphics, 0, &m_vkQueue);

    // Find a suitable depth and/or stencil format
    VkBool32 validFormat { false };
    // Samples that make use of stencil will require a depth + stencil format, so we select from a different list
    if (m_requiresStencil) {
        validFormat = vks::tools::getSupportedDepthStencilFormat(m_physicalDevice, &m_vkFormatDepth);
    } else {
        validFormat = vks::tools::getSupportedDepthFormat(m_physicalDevice, &m_vkFormatDepth);
    }
    assert(validFormat);

    m_swapChain.setContext(m_vulkanInstance, m_physicalDevice, m_device);

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
    // Create a semaphore used to synchronize m_vkImage presentation
    // Ensures that the m_vkImage is displayed before we start submitting new commands to the m_vkQueue
    VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &semaphores.m_vkSemaphorePresentComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the m_vkImage is not presented until all commands have been submitted and executed
    VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &semaphores.m_vkSemaphoreRenderComplete));

    // Set up submit info structure
    // Semaphores will stay the same during application lifetime
    // Command buffer submission info is set by each example
    m_vkSubmitInfo = vks::initializers::submitInfo();
    m_vkSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
    m_vkSubmitInfo.waitSemaphoreCount = 1;
    m_vkSubmitInfo.pWaitSemaphores = &semaphores.m_vkSemaphorePresentComplete;
    m_vkSubmitInfo.signalSemaphoreCount = 1;
    m_vkSubmitInfo.pSignalSemaphores = &semaphores.m_vkSemaphoreRenderComplete;

    return true;
}

// Win32 : Sets up a console m_hwnd and redirects standard output to it
void VulkanExampleBase::setupConsole(std::string title)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
    // Enable flags so we can color the output
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(consoleHandle, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(consoleHandle, dwMode);
    SetConsoleTitle(TEXT(title.c_str()));
}

void VulkanExampleBase::setupDPIAwareness()
{
    typedef HRESULT*(__stdcall * SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if (shCore) {
        SetProcessDpiAwarenessFunc setProcessDpiAwareness = (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

        if (setProcessDpiAwareness != nullptr) {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }
}

HWND VulkanExampleBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
    this->m_hinstance = hinstance;

    WNDCLASSEX wndClass {};

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = wndproc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hinstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = name.c_str();
    wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

    if (!RegisterClassEx(&wndClass)) {
        std::cout << "Could not register m_hwnd class!\n";
        fflush(stdout);
        exit(1);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (m_exampleSettings.m_fullscreen) {
        if ((m_drawAreaWidth != (uint32_t)screenWidth) && (m_drawAreaHeight != (uint32_t)screenHeight)) {
            DEVMODE dmScreenSettings;
            memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
            dmScreenSettings.dmSize = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth = m_drawAreaWidth;
            dmScreenSettings.dmPelsHeight = m_drawAreaHeight;
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
                if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to m_hwnd mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES) {
                    m_exampleSettings.m_fullscreen = false;
                } else {
                    return nullptr;
                }
            }
            screenWidth = m_drawAreaWidth;
            screenHeight = m_drawAreaHeight;
        }
    }

    DWORD dwExStyle;
    DWORD dwStyle;

    if (m_exampleSettings.m_fullscreen) {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    } else {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    RECT windowRect = {
        0L,
        0L,
        m_exampleSettings.m_fullscreen ? (long)screenWidth : (long)m_drawAreaWidth,
        m_exampleSettings.m_fullscreen ? (long)screenHeight : (long)m_drawAreaHeight
    };

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    std::string windowTitle = getWindowTitle();
    m_hwnd = CreateWindowEx(0,
        name.c_str(),
        windowTitle.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        hinstance,
        NULL);

    if (!m_hwnd) {
        std::cerr << "Could not create m_hwnd!\n";
        fflush(stdout);
        return nullptr;
    }

    if (!m_exampleSettings.m_fullscreen) {
        // Center on screen
        uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
        uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
        SetWindowPos(m_hwnd, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);

    return m_hwnd;
}

void VulkanExampleBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CLOSE:
        m_prepared = false;
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        ValidateRect(m_hwnd, NULL);
        break;
    case WM_KEYDOWN:
        switch (wParam) {
        case KEY_P:
            paused = !paused;
            break;
        case KEY_F1:
            m_UIOverlay.visible = !m_UIOverlay.visible;
            m_UIOverlay.updated = true;
            break;
        case KEY_F2:
            if (camera.type == Camera::CameraType::lookat) {
                camera.type = Camera::CameraType::firstperson;
            } else {
                camera.type = Camera::CameraType::lookat;
            }
            break;
        case KEY_ESCAPE:
            PostQuitMessage(0);
            break;
        }

        if (camera.type == Camera::firstperson) {
            switch (wParam) {
            case KEY_W:
                camera.keys.up = true;
                break;
            case KEY_S:
                camera.keys.down = true;
                break;
            case KEY_A:
                camera.keys.left = true;
                break;
            case KEY_D:
                camera.keys.right = true;
                break;
            }
        }

        keyPressed((uint32_t)wParam);
        break;
    case WM_KEYUP:
        if (camera.type == Camera::firstperson) {
            switch (wParam) {
            case KEY_W:
                camera.keys.up = false;
                break;
            case KEY_S:
                camera.keys.down = false;
                break;
            case KEY_A:
                camera.keys.left = false;
                break;
            case KEY_D:
                camera.keys.right = false;
                break;
            }
        }
        break;
    case WM_LBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.left = true;
        break;
    case WM_RBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.right = true;
        break;
    case WM_MBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.middle = true;
        break;
    case WM_LBUTTONUP:
        mouseState.buttons.left = false;
        break;
    case WM_RBUTTONUP:
        mouseState.buttons.right = false;
        break;
    case WM_MBUTTONUP:
        mouseState.buttons.middle = false;
        break;
    case WM_MOUSEWHEEL: {
        short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
        m_viewUpdated = true;
        break;
    }
    case WM_MOUSEMOVE: {
        handleMouseMove(LOWORD(lParam), HIWORD(lParam));
        break;
    }
    case WM_SIZE:
        if ((m_prepared) && (wParam != SIZE_MINIMIZED)) {
            if ((m_resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED))) {
                m_destWidth = LOWORD(lParam);
                m_destHeight = HIWORD(lParam);
                windowResize();
            }
        }
        break;
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
        minMaxInfo->ptMinTrackSize.x = 64;
        minMaxInfo->ptMinTrackSize.y = 64;
        break;
    }
    case WM_ENTERSIZEMOVE:
        m_resizing = true;
        break;
    case WM_EXITSIZEMOVE:
        m_resizing = false;
        break;
    }

    OnHandleMessage(hWnd, uMsg, wParam, lParam);
}

void VulkanExampleBase::keyPressed(uint32_t) { }

void VulkanExampleBase::mouseMoved(double x, double y, bool& handled) { }

void VulkanExampleBase::buildCommandBuffers() { }

void VulkanExampleBase::createSynchronizationPrimitives()
{
    // Wait fences to sync command buffer access
    VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    m_vkFences.resize(drawCmdBuffers.size());
    for (auto& fence : m_vkFences) {
        VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence));
    }
}

void VulkanExampleBase::createCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_swapChain.queueNodeIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK_RESULT(vkCreateCommandPool(m_device, &cmdPoolInfo, nullptr, &m_vkCommandPool));
}

void VulkanExampleBase::setupDepthStencil()
{
    VkImageCreateInfo vkImageCreateInfo {};
    vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    vkImageCreateInfo.format = m_vkFormatDepth;
    vkImageCreateInfo.extent = { m_drawAreaWidth, m_drawAreaHeight, 1 };
    vkImageCreateInfo.mipLevels = 1;
    vkImageCreateInfo.arrayLayers = 1;
    vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    vkImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    m_defaultDepthStencil.m_image = vkcpp::Image(vkImageCreateInfo);
    // VK_CHECK_RESULT(vkCreateImage(m_deviceOriginal, &vkImageCreateInfo, nullptr, &m_defaultDepthStencil.m_vkImage));

    VkMemoryRequirements memReqs {};
    vkGetImageMemoryRequirements(m_device, m_defaultDepthStencil.m_image, &memReqs);

    VkMemoryAllocateInfo memAlloc {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_pVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_defaultDepthStencil.m_deviceMemory = vkcpp::DeviceMemory(memAlloc);

    // VK_CHECK_RESULT(vkAllocateMemory(m_deviceOriginal, &memAllloc, nullptr, &m_defaultDepthStencil.m_deviceMemory));

    VK_CHECK_RESULT(vkBindImageMemory(m_device, m_defaultDepthStencil.m_image, m_defaultDepthStencil.m_deviceMemory, 0));

    VkImageViewCreateInfo vkImageViewCreateInfo {};
    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vkImageViewCreateInfo.image = m_defaultDepthStencil.m_image;
    vkImageViewCreateInfo.format = m_vkFormatDepth;
    vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    vkImageViewCreateInfo.subresourceRange.levelCount = 1;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount = 1;
    vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
    if (m_vkFormatDepth >= VK_FORMAT_D16_UNORM_S8_UINT) {
        vkImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    m_defaultDepthStencil.m_imageView = vkcpp::ImageView(vkImageViewCreateInfo);
    // VK_CHECK_RESULT(vkCreateImageView(m_deviceOriginal, &imageViewCI, nullptr, &m_defaultDepthStencil.m_vkImageView));
}

void VulkanExampleBase::setupFrameBuffer()
{
    // Create frame buffers for every swap chain m_vkImage
    m_vkFrameBuffers.resize(m_swapChain.images.size());
    for (uint32_t i = 0; i < m_vkFrameBuffers.size(); i++) {
        const VkImageView attachments[2] = {
            m_swapChain.imageViews[i],
            // Depth/Stencil attachment is the same for all frame buffers
            m_defaultDepthStencil.m_imageView
        };
        VkFramebufferCreateInfo frameBufferCreateInfo {};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = m_renderPassOriginal;
        frameBufferCreateInfo.attachmentCount = 2;
        frameBufferCreateInfo.pAttachments = attachments;
        frameBufferCreateInfo.width = m_drawAreaWidth;
        frameBufferCreateInfo.height = m_drawAreaHeight;
        frameBufferCreateInfo.layers = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &frameBufferCreateInfo, nullptr, &m_vkFrameBuffers[i]));
    }
}

void VulkanExampleBase::setupRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = m_swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth attachment
    attachments[1].format = m_vkFormatDepth;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies {};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = 0;

    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = 0;
    dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = 0;

    VkRenderPassCreateInfo vkRenderPassCreateInfo = {};
    vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vkRenderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    vkRenderPassCreateInfo.pAttachments = attachments.data();
    vkRenderPassCreateInfo.subpassCount = 1;
    vkRenderPassCreateInfo.pSubpasses = &subpassDescription;
    vkRenderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    vkRenderPassCreateInfo.pDependencies = dependencies.data();

    m_renderPassOriginal = vkcpp::RenderPass(vkRenderPassCreateInfo);
}

void VulkanExampleBase::getEnabledFeatures() { }

void VulkanExampleBase::getEnabledExtensions() { }

void VulkanExampleBase::windowResize()
{
    if (!m_prepared) {
        return;
    }
    m_prepared = false;
    m_resized = true;

    // Ensure all operations on the m_vkDevice have been finished before destroying resources
    vkDeviceWaitIdle(m_device);

    // Recreate swap chain
    m_drawAreaWidth = m_destWidth;
    m_drawAreaHeight = m_destHeight;
    createSwapChain();

    // Recreate the frame buffers
    //    vkDestroyImageView(m_deviceOriginal, m_defaultDepthStencil.m_vkImageView, nullptr);
    //    vkDestroyImage(m_deviceOriginal, m_defaultDepthStencil.m_vkImage, nullptr);
    //    vkFreeMemory(m_deviceOriginal, m_defaultDepthStencil.m_vkDeviceMemory, nullptr);
    setupDepthStencil();
    for (auto& frameBuffer : m_vkFrameBuffers) {
        vkDestroyFramebuffer(m_device, frameBuffer, nullptr);
    }
    setupFrameBuffer();

    if ((m_drawAreaWidth > 0.0f) && (m_drawAreaHeight > 0.0f)) {
        if (m_exampleSettings.m_showUIOverlay) {
            m_UIOverlay.resize(m_drawAreaWidth, m_drawAreaHeight);
        }
    }

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroyCommandBuffers();
    createCommandBuffers();
    buildCommandBuffers();

    // SRS - Recreate fences in case number of swapchain images has changed on resize
    for (auto& fence : m_vkFences) {
        vkDestroyFence(m_device, fence, nullptr);
    }
    createSynchronizationPrimitives();

    vkDeviceWaitIdle(m_device);

    if ((m_drawAreaWidth > 0.0f) && (m_drawAreaHeight > 0.0f)) {
        camera.updateAspectRatio((float)m_drawAreaWidth / (float)m_drawAreaHeight);
    }

    // Notify derived class
    windowResized();

    m_prepared = true;
}

void VulkanExampleBase::handleMouseMove(int32_t x, int32_t y)
{
    int32_t dx = (int32_t)mouseState.position.x - x;
    int32_t dy = (int32_t)mouseState.position.y - y;

    bool handled = false;

    if (m_exampleSettings.m_showUIOverlay) {
        ImGuiIO& io = ImGui::GetIO();
        handled = io.WantCaptureMouse && m_UIOverlay.visible;
    }
    mouseMoved((float)x, (float)y, handled);

    if (handled) {
        mouseState.position = glm::vec2((float)x, (float)y);
        return;
    }

    if (mouseState.buttons.left) {
        camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
        m_viewUpdated = true;
    }
    if (mouseState.buttons.right) {
        camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
        m_viewUpdated = true;
    }
    if (mouseState.buttons.middle) {
        camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
        m_viewUpdated = true;
    }
    mouseState.position = glm::vec2((float)x, (float)y);
}

void VulkanExampleBase::windowResized() { }

void VulkanExampleBase::createSurface()
{
    m_swapChain.initSurface(m_hinstance, m_hwnd);
}

void VulkanExampleBase::createSwapChain()
{
    m_swapChain.create(m_drawAreaWidth, m_drawAreaHeight, m_exampleSettings.m_forceSwapChainVsync, m_exampleSettings.m_fullscreen);
}

void VulkanExampleBase::OnUpdateUIOverlay(vks::UIOverlay* overlay) { }

#if defined(_WIN32)
void VulkanExampleBase::OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { };
#endif
