/*
 * Vulkan entry points
 *
 * Platform specific macros for the example main entry points
 * 
 * Copyright (C) 2024-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

/*
 * Windows
 */
#define VULKAN_EXAMPLE_MAIN()																		\
VulkanExample *vulkanExample;																		\
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)						\
{																									\
	if (vulkanExample != NULL)																		\
	{																								\
		vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);									\
	}																								\
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));												\
}																									\
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int) \
{																									\
	for (int32_t i = 0; i < __argc; i++) { VulkanExample::args.push_back(__argv[i]); };  			\
	vkcpp::VulkanContextCreateInfo vulkanContextCreateInfo;	\
	vkcpp::initVulkanContext(vulkanContextCreateInfo);		\
	vulkanExample = new VulkanExample();															\
	vulkanExample->initVulkan();																	\
	vulkanExample->setupWindow(hInstance, WndProc);													\
	vulkanExample->prepare();																		\
	vulkanExample->renderLoop();																	\
	delete(vulkanExample);																			\
	return 0;																						\
}

