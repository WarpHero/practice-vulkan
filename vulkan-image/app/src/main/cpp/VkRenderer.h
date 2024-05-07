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

#include <game-activity/GameActivity.h>
#include <vulkan/vulkan.h>

#include "VkTexture.h"

class VkRenderer {
public:
    explicit VkRenderer(ANativeWindow* window, AAssetManager *assetManager);
    ~VkRenderer();

    void render();

private:
    AAssetManager *mAssetManager;
    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceMemoryProperties mPhysicalDeviceMemoryProperties;
    uint32_t mQueueFamilyIndex;
    VkDevice mDevice;
    VkQueue mQueue;
    VkSurfaceKHR mSurface;
    VkSwapchainKHR mSwapchain;
    std::vector<VkImage> mSwapchainImages;
    VkExtent2D mSwapchainImageExtent;
    VkCommandPool mCommandPool;
    VkCommandBuffer mCommandBuffer;
    VkFence mFence;
    VkClearValue mClearValue{.color{.float32{0.15, 0.15, 0.15, 1.0}}};
    VkSemaphore mSemaphore;
    std::vector<VkImageView> mSwapchainImageViews;
    VkRenderPass mRenderPass;
    std::vector<VkFramebuffer> mFramebuffers;
    VkShaderModule mVertexShaderModule;
    VkShaderModule mFragmentShaderModule;
    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mPipeline;
    VkBuffer mVertexBuffer;
    VkDeviceMemory mVertexMemory;
    VkBuffer mUniformBuffer;
    VkDeviceMemory mUniformMemory;
    void* mUniformData;
    VkDescriptorPool mDescriptorPool;
    VkDescriptorSet mDescriptorSet;
    VkTexture mTexture;
    VkImage mImage;
    VkDeviceMemory mMemory;
};