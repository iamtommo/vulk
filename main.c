#include <assert.h>
#include <string.h>
#include <cglm/cglm.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK(f) { \
    VkResult res = (f); \
    if (res != VK_SUCCESS) { \
        fprintf(stderr, "Fatal: %s (%d) in %s at line %d\n", \
        string_VkResult(res), res, __FILE__, __LINE__); \
        assert(res == VK_SUCCESS); \
    } \
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
VkFramebuffer *swapchainFramebuffers;
uint32_t swapchainImageCount;
VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;
VkShaderModule vertShaderModule;
VkShaderModule fragShaderModule;
VkPipelineLayout pipelineLayout;
VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkCommandPool commandPool;
VkCommandBuffer commandBuffer;
int imageIndex;
VkSemaphore imageAvailableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;

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

VkShaderModule read_shader(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = malloc(fileSize);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory for shader file\n");
        fclose(file);
        exit(1);
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    if (bytesRead != fileSize) {
        fprintf(stderr, "Failed to read file: %s\n", filename);
        fclose(file);
        free(buffer);
        exit(1);
    }
    fclose(file);

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = (const uint32_t*)buffer,
    };

    VkShaderModule shaderModule;
    VK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));
    free(buffer);
    return shaderModule;
}

void draw() {
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL
    };
    VK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = swapchainFramebuffers[imageIndex],
        .renderArea.offset = {0, 0},
        .renderArea.extent = swapChainExtent,
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = swapChainExtent.width,
        .height = swapChainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = swapChainExtent,
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    VK(vkEndCommandBuffer(commandBuffer));
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

    vertShaderModule = read_shader("../shaders/vert.spv");
    fragShaderModule = read_shader("../shaders/frag.spv");

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main"
    };

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
        .pDynamicStates = &dynamicStates[0]
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float) swapChainExtent.width,
        .height = (float) swapChainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = swapChainExtent
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = NULL,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    VK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout));

    VkAttachmentDescription colorAttachment = {
        .format = swapChainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    VK(vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass));

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f

    };

    VkPipelineShaderStageCreateInfo stages[] = {vertShaderStageInfo, fragShaderStageInfo};
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = NULL,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    VK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline));

    swapchainFramebuffers = malloc(sizeof(VkFramebuffer) * swapchainImageCount);
    for (int i = 0; i < swapchainImageCount; i++) {
        VkImageView attachments[] = {
            swapchainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapChainExtent.width,
            .height = swapChainExtent.height,
            .layers = 1
        };

        VK(vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[i]));
    }

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIdx
    };
    VK(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool));

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT // initial state signaled, first frame fence passes
    };
    VK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailableSemaphore));
    VK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinishedSemaphore));
    VK(vkCreateFence(device, &fenceInfo, NULL, &inFlightFence));

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);

        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffer, 0);

        draw();

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO
        };

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence));

        VkSwapchainKHR swapChains[] = {swapChain};
        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = swapChains,
            .pImageIndices = &imageIndex
        };
        vkQueuePresentKHR(graphicsQueue, &presentInfo);
    }

    vkDeviceWaitIdle(device);

    vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
    vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
    vkDestroyFence(device, inFlightFence, NULL);
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyPipeline(device, graphicsPipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyRenderPass(device, renderPass, NULL);
    vkDestroyShaderModule(device, vertShaderModule, NULL);
    vkDestroyShaderModule(device, fragShaderModule, NULL);
    if (destroyDebugUtilsMessenger != 0) {
        destroyDebugUtilsMessenger(vk, vkDebugMessenger, NULL);
    }
    for (int i = 0; i < swapchainImageCount; i++) {
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
        vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
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