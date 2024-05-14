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
#include <cstddef>
#include <array>
#include <vector>
#include <iomanip>

#include "VkRenderer.h"
#include "VkUtil.h"
#include "AndroidOut.h"

using namespace std;

struct Vector2 {
    union {
        float x;
        float r;
        float u;
    };

    union {
        float y;
        float g;
        float v;
    };
};

struct Vector3 {
    union {
        float x;
        float r;
        float u;
    };

    union {
        float y;
        float g;
        float v;
    };

    union {
        float z;
        float b;
        float w;
    };
};

struct Vertex {
    Vector3 position;
    Vector3 color;
    Vector2 uv;
};

struct Uniform {
    float position[8];
    float ratio;
};

VkRenderer::VkRenderer(ANativeWindow *nativeWindow, AAssetManager *assetManager)
    : mAssetManager{assetManager}, mFrameIndex{0} {
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
    // 3. VkPhysicalDeviceMemoryProperties 얻기
    // ================================================================================
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mPhysicalDeviceMemoryProperties);

    // ================================================================================
    // 4. VkDevice 생성
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
    // 5. VkSurface 생성
    // ================================================================================
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = nativeWindow
    };

    VK_CHECK_ERROR(vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo, nullptr, &mSurface));

    VkBool32 supported;
    VK_CHECK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice,
                                                        mQueueFamilyIndex,
                                                        mSurface,
                                                        &supported));
    assert(supported);

    // ================================================================================
    // 6. VkSwapchain 생성
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
        // 7. VkImageView 생성
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
    // 8. VkCommandPool 생성
    // ================================================================================
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = mQueueFamilyIndex
    };

    VK_CHECK_ERROR(vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool));

    // ================================================================================
    // 9. VkCommandBuffer 할당
    // ================================================================================
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = mCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = swapchainImageCount
    };

    mCommandBuffers.resize(swapchainImageCount);
    VK_CHECK_ERROR(vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, mCommandBuffers.data()));

    mFencesForSubmit.resize(swapchainImageCount);
    for (auto& fence : mFencesForSubmit) {
        // ================================================================================
        // 10. VkFence 생성
        // ================================================================================
        VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        VK_CHECK_ERROR(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &fence));
    }

    mFencesForAcquire.resize(swapchainImageCount);
    for (auto& fence : mFencesForAcquire) {
        // ================================================================================
        // 11. VkFence 생성
        // ================================================================================
        VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };

        VK_CHECK_ERROR(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &fence));
    }

    mSemaphores.resize(swapchainImageCount);
    for (auto& semaphore : mSemaphores) {
        // ================================================================================
        // 12. VkSemaphore 생성
        // ================================================================================
        VkSemaphoreCreateInfo semaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        VK_CHECK_ERROR(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &semaphore));
    }

    // ================================================================================
    // 13. VkRenderPass 생성
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
        // 14. VkFramebuffer 생성
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
    // 15. Vertex VkShaderModule 생성
    // ================================================================================
    string_view vertexShaderCode = {
        "#version 310 es                                                         \n"
        "                                                                        \n"
        "layout(location = 0) in vec3 inPosition;                                \n"
        "layout(location = 1) in vec3 inColor;                                   \n"
        "layout(location = 2) in vec2 inUv;                                      \n"
        "                                                                        \n"
        "layout(location = 0) out vec3 outColor;                                 \n"
        "layout(location = 1) out vec2 outUv;                                    \n"
        "                                                                        \n"
        "layout(set = 0, binding = 0) uniform Uniform {                          \n"
        "    float position[2];                                                  \n"
        "    float ratio;                                                        \n"
        "};                                                                      \n"
        "                                                                        \n"
        "void main() {                                                           \n"
        "    gl_Position = vec4(inPosition, 1.0);                                \n"
        "    gl_Position.x *= ratio;                                             \n"
        "    gl_Position.x += position[0];                                       \n"
        "    gl_Position.y += position[1];                                       \n"
        "    outColor = inColor;                                                 \n"
        "    outUv = inUv;                                                       \n"
        "}                                                                       \n"
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
    // 16. Fragment VkShaderModule 생성
    // ================================================================================
    string_view fragmentShaderCode = {
        "#version 310 es                                                         \n"
        "precision mediump float;                                                \n"
        "                                                                        \n"
        "layout(location = 0) in vec3 inColor;                                   \n"
        "layout(location = 1) in vec2 inUv;                                      \n"
        "                                                                        \n"
        "layout(location = 0) out vec4 outColor;                                 \n"
        "                                                                        \n"
        "layout(set = 0, binding = 1) uniform sampler2D combinedImageSampler;    \n"
        "                                                                        \n"
        "void main() {                                                           \n"
        "    vec4 texColor = texture(combinedImageSampler, inUv);                \n"
        "    outColor = vec4(inColor * texColor.rgb, 1.0);                       \n"
        "}                                                                       \n"
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
    // 17. VkDescriptorSetLayout 생성
    // ================================================================================
    array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = descriptorSetLayoutBindings.size(),
        .pBindings = descriptorSetLayoutBindings.data()
    };

    VK_CHECK_ERROR(vkCreateDescriptorSetLayout(mDevice,
                                               &descriptorSetLayoutCreateInfo,
                                               nullptr,
                                               &mDescriptorSetLayout));

    // ================================================================================
    // 18. VkPipelineLayout 생성
    // ================================================================================
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &mDescriptorSetLayout
    };

    VK_CHECK_ERROR(vkCreatePipelineLayout(mDevice,
                                          &pipelineLayoutCreateInfo,
                                          nullptr,
                                          &mPipelineLayout));

    // ================================================================================
    // 19. Graphics VkPipeline 생성
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

    VkVertexInputBindingDescription vertexInputBindingDescription{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    array<VkVertexInputAttributeDescription, 3> vertexInputAttributeDescriptions{
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position)
        },
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color)
        },
        VkVertexInputAttributeDescription{
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, uv)
        }
    };

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexInputBindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescriptions.size()),
        .pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data()
    };

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
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

    constexpr array<VkDynamicState, 2> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamicStates.size(),
        .pDynamicStates = dynamicStates.data()
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
        .pDynamicState = &pipelineDynamicStateCreateInfo,
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
    // 20. Vertex 정의
    // ================================================================================
    constexpr array<Vertex, 3> vertices{
        Vertex{
            .position{0.0, -0.5, 0.0},
            .color{1.0, 0.0, 0.0},
            .uv{0.5, 0.0}
        },
        Vertex{
            .position{0.5, 0.5, 0.0},
            .color{0.0, 1.0, 0.0},
            .uv{1.0, 1.0}
        },
        Vertex{
            .position{-0.5, 0.5, 0.0},
            .color{0.0, 0.0, 1.0},
            .uv{0.0, 1.0}
        },
    };
    constexpr VkDeviceSize vertexDataSize{vertices.size() * sizeof(Vertex)};

    // ================================================================================
    // 21. Staging VkBuffer 생성
    // ================================================================================
    VkBufferCreateInfo stagingBufferCreateInfo{
        .sType =VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertexDataSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    VkBuffer stagingBuffer;
    VK_CHECK_ERROR(vkCreateBuffer(mDevice, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

    // ================================================================================
    // 22. Staging VkBuffer의 VkMemoryRequirements 얻기
    // ================================================================================
    VkMemoryRequirements stagingMemoryRequirements;
    vkGetBufferMemoryRequirements(mDevice, stagingBuffer, &stagingMemoryRequirements);

    // ================================================================================
    // 23. Staging VkDeviceMemory를 할당 할 수 있는 메모리 타입 인덱스 얻기
    // ================================================================================
    uint32_t stagingMemoryTypeIndex;
    VK_CHECK_ERROR(vkGetMemoryTypeIndex(mPhysicalDeviceMemoryProperties,
                                        stagingMemoryRequirements,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        &stagingMemoryTypeIndex));

    // ================================================================================
    // 24. Staging VkDeviceMemory 할당
    // ================================================================================
    VkMemoryAllocateInfo stagingMemoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = stagingMemoryRequirements.size,
        .memoryTypeIndex = stagingMemoryTypeIndex
    };

    VkDeviceMemory stagingMemory;
    VK_CHECK_ERROR(vkAllocateMemory(mDevice, &stagingMemoryAllocateInfo, nullptr, &stagingMemory));

    // ================================================================================
    // 25. Staging VkBuffer와 Staging VkDeviceMemory 바인드
    // ================================================================================
    VK_CHECK_ERROR(vkBindBufferMemory(mDevice, stagingBuffer, stagingMemory, 0));

    // ================================================================================
    // 26. Vertex 데이터 복사
    // ================================================================================
    void *stagingData;
    VK_CHECK_ERROR(vkMapMemory(mDevice, stagingMemory, 0, vertexDataSize, 0, &stagingData));
    memcpy(stagingData, vertices.data(), vertexDataSize);
    vkUnmapMemory(mDevice, stagingMemory);

    // ================================================================================
    // 27. Vertex VkBuffer 생성
    // ================================================================================
    VkBufferCreateInfo vertexBufferCreateInfo{
        .sType =VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertexDataSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    };

    VK_CHECK_ERROR(vkCreateBuffer(mDevice, &vertexBufferCreateInfo, nullptr, &mVertexBuffer));

    // ================================================================================
    // 28. Vertex VkBuffer의 VkMemoryRequirements 얻기
    // ================================================================================
    VkMemoryRequirements vertexMemoryRequirements;
    vkGetBufferMemoryRequirements(mDevice, mVertexBuffer, &vertexMemoryRequirements);

    // ================================================================================
    // 29. Vertex VkDeviceMemory를 할당 할 수 있는 메모리 타입 인덱스 얻기
    // ================================================================================
    uint32_t vertexMemoryTypeIndex;
    VK_CHECK_ERROR(vkGetMemoryTypeIndex(mPhysicalDeviceMemoryProperties,
                                        vertexMemoryRequirements,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                        &vertexMemoryTypeIndex));

    // ================================================================================
    // 30. Vertex VkDeviceMemory 할당
    // ================================================================================
    VkMemoryAllocateInfo vertexMemoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = vertexMemoryRequirements.size,
        .memoryTypeIndex = vertexMemoryTypeIndex
    };

    VK_CHECK_ERROR(vkAllocateMemory(mDevice, &vertexMemoryAllocateInfo, nullptr, &mVertexMemory));

    // ================================================================================
    // 31. Vertex VkBuffer와 Vertex VkDeviceMemory 바인드
    // ================================================================================
    VK_CHECK_ERROR(vkBindBufferMemory(mDevice, mVertexBuffer, mVertexMemory, 0));

    // ================================================================================
    // 32. VkCommandBuffer 기록 시작
    // ================================================================================
    VkCommandBufferBeginInfo commandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    auto commandBuffer = mCommandBuffers[0];
    VK_CHECK_ERROR(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    // ================================================================================
    // 33. Staging VkBuffer에서 Vertex VkBuffer로 복사
    // ================================================================================
    VkBufferCopy bufferCopy{
        .size = vertexDataSize
    };

    vkCmdCopyBuffer(commandBuffer, stagingBuffer, mVertexBuffer, 1, &bufferCopy);

    mUniformBuffers.resize(swapchainImageCount);
    mUniformMemories.resize(swapchainImageCount);
    mUniformData.resize(swapchainImageCount);
    for (auto i = 0; i != swapchainImageCount; ++i) {
        auto& uniformBuffer = mUniformBuffers[i];
        auto& uniformMemory = mUniformMemories[i];
        auto& uniformData = mUniformData[i];

        // ================================================================================
        // 34. Uniform VkBuffer 생성
        // ================================================================================
        VkBufferCreateInfo uniformBufferCreateInfo{
            .sType =VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(Uniform),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        };

        VK_CHECK_ERROR(vkCreateBuffer(mDevice, &uniformBufferCreateInfo, nullptr, &uniformBuffer));

        // ================================================================================
        // 35. Uniform VkBuffer의 VkMemoryRequirements 얻기
        // ================================================================================
        VkMemoryRequirements uniformMemoryRequirements;
        vkGetBufferMemoryRequirements(mDevice, uniformBuffer, &uniformMemoryRequirements);

        // ================================================================================
        // 36. Uniform VkDeviceMemory를 할당 할 수 있는 메모리 타입 인덱스 얻기
        // ================================================================================
        uint32_t uniformMemoryTypeIndex;
        VK_CHECK_ERROR(vkGetMemoryTypeIndex(mPhysicalDeviceMemoryProperties,
                                            uniformMemoryRequirements,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            &uniformMemoryTypeIndex));

        // ================================================================================
        // 37. Uniform VkDeviceMemory 할당
        // ================================================================================
        VkMemoryAllocateInfo uniformMemoryAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = uniformMemoryRequirements.size,
            .memoryTypeIndex = uniformMemoryTypeIndex
        };

        VK_CHECK_ERROR(vkAllocateMemory(mDevice, &uniformMemoryAllocateInfo, nullptr, &uniformMemory));

        // ================================================================================
        // 38. Uniform VkBuffer와 Vertex VkDeviceMemory 바인드
        // ================================================================================
        VK_CHECK_ERROR(vkBindBufferMemory(mDevice, uniformBuffer, uniformMemory, 0));

        // ================================================================================
        // 39. Uniform 데이터 초기화
        // ================================================================================
        VK_CHECK_ERROR(vkMapMemory(mDevice, uniformMemory, 0, VK_WHOLE_SIZE, 0, &uniformData));
        memset(uniformData, 0, sizeof(Uniform));
    }

    // ================================================================================
    // 40. VkTexture 생성
    // ================================================================================
    VkTextureCreateInfo textureCreateInfo{
        .sType = VK_STRUCTURE_TYPE_TEXTURE_CREATE_INFO,
        .pAssetManager = mAssetManager,
        .pFileName = "vulkan.png"
    };

    VK_CHECK_ERROR(vkCreateTexture(mDevice, &textureCreateInfo, nullptr, &mTexture));

    // ================================================================================
    // 41. VkTexture 속성 얻기
    // ================================================================================
    VkTextureProperties textureProperties;
    vkGetTextureProperties(mTexture, &textureProperties);

    // ================================================================================
    // 42. VkImage 생성
    // ================================================================================
    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = textureProperties.format,
        .extent = textureProperties.extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    };

    VK_CHECK_ERROR(vkCreateImage(mDevice,
                                 &imageCreateInfo,
                                 nullptr,
                                 &mImage));

    // ================================================================================
    // 43. VkImage의 VkMemoryRequirements 얻기
    // ================================================================================
    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(mDevice, mImage, &imageMemoryRequirements);


    // ================================================================================
    // 44. Image VkDeviceMemory를 할당 할 수 있는 메모리 타입 인덱스 얻기
    // ================================================================================
    uint32_t imageMemoryTypeIndex;
    VK_CHECK_ERROR(vkGetMemoryTypeIndex(mPhysicalDeviceMemoryProperties,
                                        imageMemoryRequirements,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        &imageMemoryTypeIndex));

    // ================================================================================
    // 45. Image VkDeviceMemory 할당
    // ================================================================================
    VkMemoryAllocateInfo imageMemoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = imageMemoryRequirements.size,
        .memoryTypeIndex = imageMemoryTypeIndex
    };

    VK_CHECK_ERROR(vkAllocateMemory(mDevice, &imageMemoryAllocateInfo, nullptr, &mMemory));

    // ================================================================================
    // 46. VkImage와 Image VkDeviceMemory 바인드
    // ================================================================================
    VK_CHECK_ERROR(vkBindImageMemory(mDevice, mImage, mMemory, 0));

    // ================================================================================
    // 47. VkImage의 VkImageSubresource 얻기
    // ================================================================================
    VkImageSubresource imageSubresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .arrayLayer = 0
    };

    VkSubresourceLayout subresourceLayout;
    vkGetImageSubresourceLayout(mDevice,
                                mImage,
                                &imageSubresource,
                                &subresourceLayout);

    // ================================================================================
    // 48. Image 데이터 초기화
    // ================================================================================
    void* imageData;
    VK_CHECK_ERROR(vkMapMemory(mDevice, mMemory, 0, VK_WHOLE_SIZE, 0, &imageData));

    for (auto h = 0; h < textureProperties.extent.height; ++h) {
        const auto dstOffset = h * subresourceLayout.rowPitch;
        const auto srcRowPitch = textureProperties.extent.width * 4;
        const auto srcOffset = h * srcRowPitch;
        memcpy(static_cast<uint8_t *>(imageData) + dstOffset,
               static_cast<uint8_t *>(textureProperties.pData) + srcOffset,
               srcRowPitch);
    }

    vkUnmapMemory(mDevice, mMemory);

    // ================================================================================
    // 49. VkImageView 생성
    // ================================================================================
    VkImageViewCreateInfo imageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = mImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = textureProperties.format,
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

    VK_CHECK_ERROR(vkCreateImageView(mDevice, &imageViewCreateInfo, nullptr, &mImageView));

    // ================================================================================
    // 50. VkImageLayout 변환
    // ================================================================================
    VkImageMemoryBarrier imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &imageMemoryBarrier);

    // ================================================================================
    // 51. VkCommandBuffer 기록 종료
    // ================================================================================
    VK_CHECK_ERROR(vkEndCommandBuffer(commandBuffer));

    // ================================================================================
    // 52. VkCommandBuffer 제출
    // ================================================================================
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    VK_CHECK_ERROR(vkQueueSubmit(mQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK_ERROR(vkQueueWaitIdle(mQueue));

    // ================================================================================
    // 53. Staging VkBuffer 파괴
    // ================================================================================
    vkFreeMemory(mDevice, stagingMemory, nullptr);

    // ================================================================================
    // 54. VkSampler 생성
    // ================================================================================
    vkDestroyBuffer(mDevice, stagingBuffer, nullptr);

    // ================================================================================
    // 55. VkSampler 생성
    // ================================================================================
    const VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };

    VK_CHECK_ERROR(vkCreateSampler(mDevice,
                                   &samplerCreateInfo,
                                   nullptr,
                                   &mSampler));

    // ================================================================================
    // 56. VkDescriptorPool 생성
    // ================================================================================
    array<VkDescriptorPoolSize, 2> descriptorPoolSizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1 * swapchainImageCount
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1 * swapchainImageCount
        }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = swapchainImageCount,
        .poolSizeCount = descriptorPoolSizes.size(),
        .pPoolSizes = descriptorPoolSizes.data()
    };

    VK_CHECK_ERROR(vkCreateDescriptorPool(mDevice,
                                          &descriptorPoolCreateInfo,
                                          nullptr,
                                          &mDescriptorPool));

    // ================================================================================
    // 57. VkDescriptorSet 할당
    // ================================================================================
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &mDescriptorSetLayout
    };

    mDescriptorSets.resize(swapchainImageCount);
    for (auto &descriptorSet : mDescriptorSets) {
        VK_CHECK_ERROR(vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, &descriptorSet));
    }

    for (auto i = 0; i != swapchainImageCount; ++i) {
        auto uniformBuffer = mUniformBuffers[i];
        auto descriptorSet = mDescriptorSets[i];

        // ================================================================================
        // 58. VkDescriptorSet 갱신
        // ================================================================================
        VkDescriptorBufferInfo descriptorBufferInfo{
            .buffer = uniformBuffer,
            .range = VK_WHOLE_SIZE
        };

        VkDescriptorImageInfo descriptorImageInfo{
            .sampler = mSampler,
            .imageView = mImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        array<VkWriteDescriptorSet, 2> writeDescriptorSets{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &descriptorBufferInfo
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &descriptorImageInfo
            }
        };

        vkUpdateDescriptorSets(mDevice,
                               writeDescriptorSets.size(),
                               writeDescriptorSets.data(),
                               0,
                               nullptr);
    }
}

VkRenderer::~VkRenderer() {
    VK_CHECK_ERROR(vkDeviceWaitIdle(mDevice));
    vkFreeDescriptorSets(mDevice, mDescriptorPool, mDescriptorSets.size(), mDescriptorSets.data());
    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    vkDestroySampler(mDevice, mSampler, nullptr);
    vkDestroyImageView(mDevice, mImageView, nullptr);
    vkFreeMemory(mDevice, mMemory, nullptr);
    vkDestroyImage(mDevice, mImage, nullptr);
    vkDestroyTexture(mDevice, mTexture, nullptr);
    mUniformData.clear();
    for (auto uniformMemory : mUniformMemories) {
        vkUnmapMemory(mDevice, uniformMemory);
    }
    for (auto uniformMemory : mUniformMemories) {
        vkFreeMemory(mDevice, uniformMemory, nullptr);
    }
    mUniformMemories.clear();
    for (auto uniformBuffer : mUniformBuffers) {
        vkDestroyBuffer(mDevice, uniformBuffer, nullptr);
    }
    mUniformBuffers.clear();
    vkFreeMemory(mDevice, mVertexMemory, nullptr);
    vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
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
    for (auto semaphore : mSemaphores) {
        vkDestroySemaphore(mDevice, semaphore, nullptr);
    }
    mSemaphores.clear();
    for (auto fence : mFencesForAcquire) {
        vkDestroyFence(mDevice, fence, nullptr);
    }
    mFencesForAcquire.clear();
    for (auto fence : mFencesForSubmit) {
        vkDestroyFence(mDevice, fence, nullptr);
    }
    mFencesForSubmit.clear();
    vkFreeCommandBuffers(mDevice, mCommandPool, mCommandBuffers.size(), mCommandBuffers.data());
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroyInstance(mInstance, nullptr);
}

void VkRenderer::render() {
    auto fenceForSubmit = mFencesForSubmit[mFrameIndex];
    auto fenceForAcquire = mFencesForAcquire[mFrameIndex];
    auto commandBuffer = mCommandBuffers[mFrameIndex];
    auto uniformData = static_cast<Uniform *>(mUniformData[mFrameIndex]);
    auto descriptorSet = mDescriptorSets[mFrameIndex];
    auto semaphore = mSemaphores[mFrameIndex];

    // ================================================================================
    // 1. VkFence 기다린 후 초기화
    // ================================================================================
    VK_CHECK_ERROR(vkWaitForFences(mDevice, 1, &fenceForSubmit, VK_TRUE, UINT64_MAX));
    VK_CHECK_ERROR(vkResetFences(mDevice, 1, &fenceForSubmit));

    // ================================================================================
    // 2. 위치 정보 갱신
    // ================================================================================
    for (auto i = 0; i < 8; i += 4) {
        uniformData->position[i] += 0.01;
        if (uniformData->position[i] > 1.5) {
            uniformData->position[i] = -1.5;
        }
    }
    auto ratio = mSwapchainImageExtent.height / static_cast<float>(mSwapchainImageExtent.width);
    uniformData->ratio = ratio;

    // ================================================================================
    // 3. 화면에 출력할 수 있는 VkImage 얻기
    // ================================================================================
    uint32_t swapchainImageIndex;
    VK_CHECK_ERROR(vkAcquireNextImageKHR(mDevice,
                                         mSwapchain,
                                         UINT64_MAX,
                                         VK_NULL_HANDLE,
                                         fenceForAcquire,
                                         &swapchainImageIndex));
    auto framebuffer = mFramebuffers[swapchainImageIndex];

    // ================================================================================
    // 4. VkFence 기다린 후 초기화
    // ================================================================================
    VK_CHECK_ERROR(vkWaitForFences(mDevice, 1, &fenceForAcquire, VK_TRUE, UINT64_MAX));
    VK_CHECK_ERROR(vkResetFences(mDevice, 1, &fenceForAcquire));

    // ================================================================================
    // 5. VkCommandBuffer 초기화
    // ================================================================================
    vkResetCommandBuffer(commandBuffer, 0);

    // ================================================================================
    // 6. VkCommandBuffer 기록 시작
    // ================================================================================
    VkCommandBufferBeginInfo commandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK_ERROR(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    // ================================================================================
    // 7. VkRenderPass 시작
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

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ================================================================================
    // 8. Viewport 설정
    // ================================================================================
    const VkViewport viewport{
        .width = static_cast<float>(mSwapchainImageExtent.width),
        .height = static_cast<float>(mSwapchainImageExtent.height),
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // ================================================================================
    // 9. Scissor 설정
    // ================================================================================
    VkRect2D scissor{
        .extent = mSwapchainImageExtent
    };

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // ================================================================================
    // 10. Graphics VkPipeline 바인드
    // ================================================================================
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    // ================================================================================
    // 11. Vertex VkBuffer 바인드
    // ================================================================================
    VkDeviceSize vertexBufferOffset{0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVertexBuffer, &vertexBufferOffset);

    // ================================================================================
    // 12. VkDescriptorSet 바인드
    // ================================================================================
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            mPipelineLayout,
                            0,
                            1,
                            &descriptorSet,
                            0,
                            nullptr);

    // ================================================================================
    // 13. 삼각형 그리기
    // ================================================================================
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // ================================================================================
    // 14. VkRenderPass 종료
    // ================================================================================
    vkCmdEndRenderPass(commandBuffer);

    // ================================================================================
    // 15. VkCommandBuffer 기록 종료
    // ================================================================================
    VK_CHECK_ERROR(vkEndCommandBuffer(commandBuffer));

    // ================================================================================
    // 16. VkCommandBuffer 제출
    // ================================================================================
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &semaphore
    };

    VK_CHECK_ERROR(vkQueueSubmit(mQueue, 1, &submitInfo, fenceForSubmit));

    // ================================================================================
    // 17. VkImage 화면에 출력
    // ================================================================================
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &semaphore,
        .swapchainCount = 1,
        .pSwapchains = &mSwapchain,
        .pImageIndices = &swapchainImageIndex
    };

    VK_CHECK_ERROR(vkQueuePresentKHR(mQueue, &presentInfo));

    // ================================================================================
    // 18. 프레임 인덱스 갱신
    // ================================================================================
    mFrameIndex = ++mFrameIndex % mSwapchainImages.size();
}
