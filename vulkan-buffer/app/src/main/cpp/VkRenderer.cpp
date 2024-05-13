// MIT License
//
// Copyright (c) 2024 Daemyung Jang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cassert>
#include <array>
#include <vector>
#include <iomanip>

#include "VkRenderer.h"
#include "VkUtil.h"
#include "AndroidOut.h"

using namespace std;

struct Vector3 {
    union {
        float x;
        float r;
    };

    union {
        float y;
        float g;
    };

    union {
        float z;
        float b;
    };
};

struct Vertex {
    Vector3 position;
    Vector3 color;
};

VkRenderer::VkRenderer(ANativeWindow *window) {
    // ================================================================================
    // 1. VkInstance 생성
    // ================================================================================
    VkApplicationInfo applicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Practice Vulkan",
        .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
    };

    uint32_t instanceLayerCount;
    VK_CHECK_ERROR(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

    vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
    VK_CHECK_ERROR(vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                                      instanceLayerProperties.data()));

    vector<const char *> instanceLayerNames;
    for (const auto &properties: instanceLayerProperties) {
        instanceLayerNames.push_back(properties.layerName);
    }

    uint32_t instanceExtensionCount;
    VK_CHECK_ERROR(vkEnumerateInstanceExtensionProperties(nullptr,
                                                          &instanceExtensionCount,
                                                          nullptr));

    vector<VkExtensionProperties> instanceExtensionProperties(instanceExtensionCount);
    VK_CHECK_ERROR(vkEnumerateInstanceExtensionProperties(nullptr,
                                                          &instanceExtensionCount,
                                                          instanceExtensionProperties.data()));

    vector<const char *> instanceExtensionNames;
    for (const auto &properties: instanceExtensionProperties) {
        if (properties.extensionName == string("VK_KHR_surface") ||
            properties.extensionName == string("VK_KHR_android_surface")) {
            instanceExtensionNames.push_back(properties.extensionName);
        }
    }
    assert(instanceExtensionNames.size() == 2);

    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = static_cast<uint32_t>(instanceLayerNames.size()),
        .ppEnabledLayerNames = instanceLayerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensionNames.size()),
        .ppEnabledExtensionNames = instanceExtensionNames.data()
    };

    VK_CHECK_ERROR(vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance));

    // ================================================================================
    // 2. VkPhysicalDevice 선택
    // ================================================================================
    uint32_t physicalDeviceCount;
    VK_CHECK_ERROR(vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, nullptr));

    vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK_ERROR(vkEnumeratePhysicalDevices(mInstance,
                                              &physicalDeviceCount,
                                              physicalDevices.data()));

    mPhysicalDevice = physicalDevices[0];

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &physicalDeviceProperties);

    aout << "Selected Physical Device Information ↓" << endl;
    aout << setw(16) << left << " - Device Name: "
         << string_view(physicalDeviceProperties.deviceName) << endl;
    aout << setw(16) << left << " - Device Type: "
         << vkToString(physicalDeviceProperties.deviceType) << endl;
    aout << std::hex;
    aout << setw(16) << left << " - Device ID: " << physicalDeviceProperties.deviceID << endl;
    aout << setw(16) << left << " - Vendor ID: " << physicalDeviceProperties.vendorID << endl;
    aout << std::dec;
    aout << setw(16) << left << " - API Version: "
         << VK_API_VERSION_MAJOR(physicalDeviceProperties.apiVersion) << "."
         << VK_API_VERSION_MINOR(physicalDeviceProperties.apiVersion) << endl;
    aout << setw(16) << left << " - Driver Version: "
         << VK_API_VERSION_MAJOR(physicalDeviceProperties.driverVersion) << "."
         << VK_API_VERSION_MINOR(physicalDeviceProperties.driverVersion) << endl;

    // ================================================================================
    // 3. VkDevice 생성
    // ================================================================================
    uint32_t queueFamilyPropertiesCount;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyPropertiesCount, nullptr);

    vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyPropertiesCount,
                                             queueFamilyProperties.data());

    for (mQueueFamilyIndex = 0;
         mQueueFamilyIndex != queueFamilyPropertiesCount; ++mQueueFamilyIndex) {
        if (queueFamilyProperties[mQueueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            break;
        }
    }

    const vector<float> queuePriorities{1.0};
    VkDeviceQueueCreateInfo deviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = mQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = queuePriorities.data()
    };

    uint32_t deviceExtensionCount;
    VK_CHECK_ERROR(vkEnumerateDeviceExtensionProperties(mPhysicalDevice,
                                                        nullptr,
                                                        &deviceExtensionCount,
                                                        nullptr));

    vector<VkExtensionProperties> deviceExtensionProperties(deviceExtensionCount);
    VK_CHECK_ERROR(vkEnumerateDeviceExtensionProperties(mPhysicalDevice,
                                                        nullptr,
                                                        &deviceExtensionCount,
                                                        deviceExtensionProperties.data()));

    vector<const char *> deviceExtensionNames;
    for (const auto &properties: deviceExtensionProperties) {
        if (properties.extensionName == string("VK_KHR_swapchain")) {
            deviceExtensionNames.push_back(properties.extensionName);
        }
    }
    assert(deviceExtensionNames.size() == 1);

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size()),
        .ppEnabledExtensionNames = deviceExtensionNames.data()
    };

    VK_CHECK_ERROR(vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice));
    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);

    // ================================================================================
    // 4. VkSurface 생성
    // ================================================================================
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = window
    };

    VK_CHECK_ERROR(vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo, nullptr, &mSurface));

    VkBool32 supported;
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice,
                                                        mQueueFamilyIndex,
                                                        mSurface,
                                                        &supported));
    assert(supported);

    // ================================================================================
    // 5. VkSwapchain 생성
    // ================================================================================
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice,
                                                             mSurface,
                                                             &surfaceCapabilities));
    mSwapchainImageExtent = surfaceCapabilities.currentExtent;

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
    for (auto i = 0; i <= 4; ++i) {
        if (auto flag = 0x1u << i; surfaceCapabilities.supportedCompositeAlpha & flag) {
            compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(flag);
            break;
        }
    }
    assert(compositeAlpha != VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR);

    VkImageUsageFlags swapchainImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    assert(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    uint32_t surfaceFormatCount = 0;
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice,
                                                        mSurface,
                                                        &surfaceFormatCount,
                                                        nullptr));

    vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice,
                                                        mSurface,
                                                        &surfaceFormatCount,
                                                        surfaceFormats.data()));

    uint32_t surfaceFormatIndex = VK_FORMAT_MAX_ENUM;
    for (auto i = 0; i != surfaceFormatCount; ++i) {
        if (surfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            surfaceFormatIndex = i;
            break;
        }
    }
    assert(surfaceFormatIndex != VK_FORMAT_MAX_ENUM);

    uint32_t presentModeCount;
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice,
                                                             mSurface,
                                                             &presentModeCount,
                                                             nullptr));

    vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice,
                                                             mSurface,
                                                             &presentModeCount,
                                                             presentModes.data()));

    uint32_t presentModeIndex = VK_PRESENT_MODE_MAX_ENUM_KHR;
    for (auto i = 0; i != presentModeCount; ++i) {
        if (presentModes[i] == VK_PRESENT_MODE_FIFO_KHR) {
            presentModeIndex = i;
            break;
        }
    }
    assert(presentModeIndex != VK_PRESENT_MODE_MAX_ENUM_KHR);

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = mSurface,
        .minImageCount = surfaceCapabilities.minImageCount,
        .imageFormat = surfaceFormats[surfaceFormatIndex].format,
        .imageColorSpace = surfaceFormats[surfaceFormatIndex].colorSpace,
        .imageExtent = mSwapchainImageExtent,
        .imageArrayLayers = 1,
        .imageUsage = swapchainImageUsage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = compositeAlpha,
        .presentMode = presentModes[presentModeIndex]
    };

    VK_CHECK_ERROR(vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, nullptr, &mSwapchain));

    uint32_t swapchainImageCount;
    VK_CHECK_ERROR(vkGetSwapchainImagesKHR(mDevice, mSwapchain, &swapchainImageCount, nullptr));

    mSwapchainImages.resize(swapchainImageCount);
    VK_CHECK_ERROR(vkGetSwapchainImagesKHR(mDevice,
                                           mSwapchain,
                                           &swapchainImageCount,
                                           mSwapchainImages.data()));

    mSwapchainImageViews.resize(swapchainImageCount);
    for (auto i = 0; i != swapchainImageCount; ++i) {
        // ================================================================================
        // 6. VkImageView 생성
        // ================================================================================
        VkImageViewCreateInfo imageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = mSwapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormats[surfaceFormatIndex].format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VK_CHECK_ERROR(vkCreateImageView(mDevice,
                                         &imageViewCreateInfo,
                                         nullptr,
                                         &mSwapchainImageViews[i]));
    }

    // ================================================================================
    // 7. VkCommandPool 생성
    // ================================================================================
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = mQueueFamilyIndex
    };

    VK_CHECK_ERROR(vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool));

    // ================================================================================
    // 8. VkCommandBuffer 할당
    // ================================================================================
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = mCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VK_CHECK_ERROR(vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &mCommandBuffer));

    // ================================================================================
    // 9. VkFence 생성
    // ================================================================================
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VK_CHECK_ERROR(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mFence));

    // ================================================================================
    // 10. VkSemaphore 생성
    // ================================================================================
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VK_CHECK_ERROR(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mSemaphore));

    // ================================================================================
    // 11. VkRenderPass 생성
    // ================================================================================
    VkAttachmentDescription attachmentDescription{
        .format = surfaceFormats[surfaceFormatIndex].format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference attachmentReference{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpassDescription{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentReference
    };

    VkRenderPassCreateInfo renderPassCreateInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachmentDescription,
        .subpassCount = 1,
        .pSubpasses = &subpassDescription
    };

    VK_CHECK_ERROR(vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mRenderPass));

    mFramebuffers.resize(swapchainImageCount);
    for (auto i = 0; i != swapchainImageCount; ++i) {
        // ================================================================================
        // 12. VkFramebuffer 생성
        // ================================================================================
        VkFramebufferCreateInfo framebufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = mRenderPass,
            .attachmentCount = 1,
            .pAttachments = &mSwapchainImageViews[i],
            .width = mSwapchainImageExtent.width,
            .height = mSwapchainImageExtent.height,
            .layers = 1
        };

        VK_CHECK_ERROR(vkCreateFramebuffer(mDevice,
                                           &framebufferCreateInfo,
                                           nullptr,
                                           &mFramebuffers[i]));
    }

    // ================================================================================
    // 13. Vertex VkShaderModule 생성
    // ================================================================================
    string_view vertexShaderCode = {
        "#version 310 es                                        \n"
        "                                                       \n"
        "void main() {                                          \n"
        "    vec2 pos[3] = vec2[3](vec2(-0.5,  0.5),            \n"
        "                          vec2( 0.5,  0.5),            \n"
        "                          vec2( 0.0, -0.5));           \n"
        "                                                       \n"
        "    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0); \n"
        "}                                                      \n"
    };

    std::vector<uint32_t> vertexShaderBinary;
    VK_CHECK_ERROR(vkCompileShader(vertexShaderCode,
                                   VK_SHADER_TYPE_VERTEX,
                                   &vertexShaderBinary));

    VkShaderModuleCreateInfo vertexShaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertexShaderBinary.size() * sizeof(uint32_t),
        .pCode = vertexShaderBinary.data()
    };

    VK_CHECK_ERROR(vkCreateShaderModule(mDevice,
                                        &vertexShaderModuleCreateInfo,
                                        nullptr,
                                        &mVertexShaderModule));

    // ================================================================================
    // 14. Fragment VkShaderModule 생성
    // ================================================================================
    string_view fragmentShaderCode = {
        "#version 310 es                                        \n"
        "precision mediump float;                               \n"
        "                                                       \n"
        "layout(location = 0) out vec4 fragmentColor;           \n"
        "                                                       \n"
        "void main() {                                          \n"
        "    fragmentColor = vec4(1.0, 0.0, 0.0, 1.0);          \n"
        "}                                                      \n"
    };

    std::vector<uint32_t> fragmentShaderBinary;
    VK_CHECK_ERROR(vkCompileShader(fragmentShaderCode,
                                   VK_SHADER_TYPE_FRAGMENT,
                                   &fragmentShaderBinary));

    VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragmentShaderBinary.size() * sizeof(uint32_t),
        .pCode = fragmentShaderBinary.data()
    };

    VK_CHECK_ERROR(vkCreateShaderModule(mDevice,
                                        &fragmentShaderModuleCreateInfo,
                                        nullptr,
                                        &mFragmentShaderModule));

    // ================================================================================
    // 15. VkPipelineLayout 생성
    // ================================================================================
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };

    VK_CHECK_ERROR(vkCreatePipelineLayout(mDevice,
                                          &pipelineLayoutCreateInfo,
                                          nullptr,
                                          &mPipelineLayout));

    // ================================================================================
    // 16. Graphics VkPipeline 생성
    // ================================================================================
    array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = mVertexShaderModule,
            .pName = "main"
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = mFragmentShaderModule,
            .pName = "main"
        }
    };

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology =VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkViewport viewport{
        .width = static_cast<float>(mSwapchainImageExtent.width),
        .height = static_cast<float>(mSwapchainImageExtent.height),
        .maxDepth = 1.0f
    };

    VkRect2D scissor{
        .extent = mSwapchainImageExtent
    };

    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };

    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                          VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &pipelineColorBlendAttachmentState
    };

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = pipelineShaderStageCreateInfos.size(),
        .pStages = pipelineShaderStageCreateInfos.data(),
        .pVertexInputState = &pipelineVertexInputStateCreateInfo,
        .pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
        .pViewportState = &pipelineViewportStateCreateInfo,
        .pRasterizationState = &pipelineRasterizationStateCreateInfo,
        .pMultisampleState = &pipelineMultisampleStateCreateInfo,
        .pDepthStencilState = &pipelineDepthStencilStateCreateInfo,
        .pColorBlendState = &pipelineColorBlendStateCreateInfo,
        .layout = mPipelineLayout,
        .renderPass = mRenderPass
    };

    VK_CHECK_ERROR(vkCreateGraphicsPipelines(mDevice,
                                             VK_NULL_HANDLE,
                                             1,
                                             &graphicsPipelineCreateInfo,
                                             nullptr,
                                             &mPipeline));

    // ================================================================================
    // 17. Vertex VkBuffer 생성
    // ================================================================================
    constexpr array<Vertex, 3> vertices{
        Vertex{
            .position{0.0, -0.5, 0.0},
            .color{1.0, 0.0, 0.0}
        },
        Vertex{
            .position{0.5, 0.5, 0.0},
            .color{0.0, 1.0, 0.0}
        },
        Vertex{
            .position{-0.5, 0.5, 0.0},
            .color{0.0, 0.0, 1.0}
        },
    };
    constexpr VkDeviceSize verticesSize{vertices.size() * sizeof(Vertex)};

    VkBufferCreateInfo bufferCreateInfo{
        .sType =VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = verticesSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    };

    VK_CHECK_ERROR(vkCreateBuffer(mDevice, &bufferCreateInfo, nullptr, &mVertexBuffer));
}

VkRenderer::~VkRenderer() {
    vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyShaderModule(mDevice, mVertexShaderModule, nullptr);
    vkDestroyShaderModule(mDevice, mFragmentShaderModule, nullptr);
    for (auto framebuffer: mFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    mFramebuffers.clear();
    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    for (auto imageView: mSwapchainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
    mSwapchainImageViews.clear();
    vkDestroySemaphore(mDevice, mSemaphore, nullptr);
    vkDestroyFence(mDevice, mFence, nullptr);
    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &mCommandBuffer);
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroyInstance(mInstance, nullptr);
}

void VkRenderer::render() {
    // ================================================================================
    // 1. 화면에 출력할 수 있는 VkImage 얻기
    // ================================================================================
    uint32_t swapchainImageIndex;
    VK_CHECK_ERROR(vkAcquireNextImageKHR(mDevice,
                                         mSwapchain,
                                         UINT64_MAX,
                                         VK_NULL_HANDLE,
                                         mFence,
                                         &swapchainImageIndex));
    auto framebuffer = mFramebuffers[swapchainImageIndex];

    // ================================================================================
    // 2. VkFence 기다린 후 초기화
    // ================================================================================
    VK_CHECK_ERROR(vkWaitForFences(mDevice, 1, &mFence, VK_TRUE, UINT64_MAX));
    VK_CHECK_ERROR(vkResetFences(mDevice, 1, &mFence));

    // ================================================================================
    // 3. VkCommandBuffer 초기화
    // ================================================================================
    vkResetCommandBuffer(mCommandBuffer, 0);

    // ================================================================================
    // 4. VkCommandBuffer 기록 시작
    // ================================================================================
    VkCommandBufferBeginInfo commandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK_ERROR(vkBeginCommandBuffer(mCommandBuffer, &commandBufferBeginInfo));

    // ================================================================================
    // 5. VkRenderPass 시작
    // ================================================================================
    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = mRenderPass,
        .framebuffer = framebuffer,
        .renderArea{
            .extent = mSwapchainImageExtent
        },
        .clearValueCount = 1,
        .pClearValues = &mClearValue
    };

    vkCmdBeginRenderPass(mCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ================================================================================
    // 6. Graphics VkPipeline 바인드
    // ================================================================================
    vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    // ================================================================================
    // 7. 삼각형 그리기
    // ================================================================================
    vkCmdDraw(mCommandBuffer, 3, 1, 0, 0);

    // ================================================================================
    // 8. VkRenderPass 종료
    // ================================================================================
    vkCmdEndRenderPass(mCommandBuffer);

    // ================================================================================
    // 9. Clear 색상 갱신
    // ================================================================================
    for (auto i = 0; i != 4; ++i) {
        mClearValue.color.float32[i] = fmodf(mClearValue.color.float32[i] + 0.01, 1.0);
    }

    // ================================================================================
    // 10. VkCommandBuffer 기록 종료
    // ================================================================================
    VK_CHECK_ERROR(vkEndCommandBuffer(mCommandBuffer));

    // ================================================================================
    // 11. VkCommandBuffer 제출
    // ================================================================================
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &mCommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &mSemaphore
    };

    VK_CHECK_ERROR(vkQueueSubmit(mQueue, 1, &submitInfo, VK_NULL_HANDLE));

    // ================================================================================
    // 12. VkImage 화면에 출력
    // ================================================================================
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &mSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &mSwapchain,
        .pImageIndices = &swapchainImageIndex
    };

    VK_CHECK_ERROR(vkQueuePresentKHR(mQueue, &presentInfo));
    VK_CHECK_ERROR(vkQueueWaitIdle(mQueue));
}
