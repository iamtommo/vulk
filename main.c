#define GLFW_INCLUDE_VULKAN
#include <assert.h>
#include <string.h>
#include <GLFW/glfw3.h>

#include <cglm/cglm.h>

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
VkDebugUtilsMessengerEXT vkDebugMessenger;

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
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
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

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    if (destroyDebugUtilsMessenger != 0) {
        destroyDebugUtilsMessenger(vk, vkDebugMessenger, NULL);
    }
    vkDestroyInstance(vk, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}