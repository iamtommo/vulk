#include <assert.h>
#include <string.h>
#include <cglm/cglm.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK(f)                                                 \
{                                                                      \
VkResult res = (f);                                                \
if (res != VK_SUCCESS) {                                           \
fprintf(stderr, "Fatal: %s (%d) in %s at line %d\n",   \
string_VkResult(res), res, __FILE__, __LINE__);                              \
assert(res == VK_SUCCESS);                                     \
}                                                                  \
}


VkInstance vk;
VkPhysicalDevice physicalDevice;
VkDevice device;
VkDebugUtilsMessengerEXT vkDebugMessenger;
VkQueue graphicsQueue;
VkSurfaceKHR surface;
VkSwapchainKHR swapChain;
VkImage *swapchainImages;
VkImageView *swapchainImageViews;
uint32_t swapchainImageCount;
VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;

PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger = NULL;
PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger = NULL;

static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan window", NULL, NULL);

    uint32_t extensionCount = 0;
    VkExtensionProperties extensions[256];
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, &extensions[0]);

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo
      };

    uint32_t enabledExtensionCount = 0;
    const char *enabledExtensions[256];

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (int i = 0; i < glfwExtensionCount; i++) {
        enabledExtensions[i] = glfwExtensions[i];
    }
    enabledExtensionCount += glfwExtensionCount;

#ifndef NDEBUG
    enabledExtensions[enabledExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

    createInfo.enabledExtensionCount = enabledExtensionCount;
    createInfo.ppEnabledExtensionNames = enabledExtensions;
    printf("enabled %d/%d extensions\n", enabledExtensionCount, extensionCount);

    createInfo.enabledLayerCount = 0;
#ifndef NDEBUG
    uint32_t layerCount;
    VK(vkEnumerateInstanceLayerProperties(&layerCount, NULL));

    VkLayerProperties availableLayers[128];
    VK(vkEnumerateInstanceLayerProperties(&layerCount, &availableLayers[0]));

    const char* validationLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    int enabledLayerCount = 0;
    const char* enabledLayerNames[128];
    for (uint32_t i = 0; i < layerCount; ++i) {
        const char *layerName = availableLayers[i].layerName;
        bool found = false;
        for (uint32_t j = 0; j < sizeof(validationLayers) / sizeof(validationLayers[0]); j++) {
            if (strcmp(layerName, validationLayers[j]) == 0) {
                found = true;
            }
        }

        if (found) {
            enabledLayerNames[enabledLayerCount] = layerName;
            enabledLayerCount++;
        }
    }

    createInfo.enabledLayerCount = enabledLayerCount;
    createInfo.ppEnabledLayerNames = enabledLayerNames;
    printf("enabled %d/%d validation layers\n", enabledLayerCount, layerCount);
#endif

    VK(vkCreateInstance(&createInfo, NULL, &vk));

#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vkDebugCallback,
        .pUserData = NULL
    };
    createDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vk, "vkCreateDebugUtilsMessengerEXT");
    destroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vk, "vkDestroyDebugUtilsMessengerEXT");

    createDebugUtilsMessenger(vk, &debugMessengerCreateInfo, NULL, &vkDebugMessenger);
#endif

    // surface
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hwnd = glfwGetWin32Window(window),
        .hinstance = GetModuleHandle(NULL)
    };
    VK(vkCreateWin32SurfaceKHR(vk, &surfaceCreateInfo, NULL, &surface));

    // devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vk, &deviceCount, NULL);
    if (deviceCount != 1) {
        fprintf(stderr, "No GPU");
        exit(1);
    }

    vkEnumeratePhysicalDevices(vk, &deviceCount, &physicalDevice);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    VkQueueFamilyProperties queueFamilies[32];
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, &queueFamilies[0]);
    int queueFamilyIdx = -1;
    for (int i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueFamilyIdx = i;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIdx,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queueCreateInfo,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &deviceFeatures
    };

    // deprecated (backwards compat)
    deviceCreateInfo.enabledLayerCount = createInfo.enabledLayerCount;
    deviceCreateInfo.ppEnabledLayerNames = createInfo.ppEnabledLayerNames;

    int enabledDeviceExtensionCount = 0;
    const char *enabledDeviceExtensions[256];
    enabledDeviceExtensions[enabledDeviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    deviceCreateInfo.enabledExtensionCount = enabledDeviceExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions;

    VK(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

    vkGetDeviceQueue(device, queueFamilyIdx, 0, &graphicsQueue);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL);
    VkSurfaceFormatKHR surfaceFormats[32];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, &surfaceFormats[0]);

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);
    VkPresentModeKHR presentModes[32];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, &presentModes[0]);

    VkSurfaceFormatKHR surfaceFormat = {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    VkExtent2D extent = {
        .width = width,
        .height = height
    };

    uint32_t minSwapchainImageCount = surfaceCapabilities.minImageCount + 1;
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = minSwapchainImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE
    };

    VK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapChain));

    vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, NULL);
    swapchainImages = malloc(sizeof(VkImage) * swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, &swapchainImages[0]);
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    swapchainImageViews = malloc(sizeof(VkImageView) * swapchainImageCount);
    for (int i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapChainImageFormat,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };

        VK(vkCreateImageView(device, &viewCreateInfo, NULL, &swapchainImageViews[i]));
    }

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    if (destroyDebugUtilsMessenger != 0) {
        destroyDebugUtilsMessenger(vk, vkDebugMessenger, NULL);
    }
    for (int i = 0; i < swapchainImageCount; i++) {
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
    }
    free(swapchainImages);
    free(swapchainImageViews);
    vkDestroySwapchainKHR(device, swapChain, NULL);
    vkDestroySurfaceKHR(vk, surface, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(vk, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}