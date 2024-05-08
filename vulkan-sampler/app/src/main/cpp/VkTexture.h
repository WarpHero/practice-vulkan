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

#ifndef PRACTICE_VULKAN_VKTEXTURE_H
#define PRACTICE_VULKAN_VKTEXTURE_H

#include <game-activity/GameActivity.h>
#include <vulkan/vulkan.h>

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkTexture)

typedef enum VkStructureTypeEXT {
    VK_STRUCTURE_TYPE_TEXTURE_CREATE_INFO = 2000000000
} VkStructureTypeEXT;

typedef struct VkTextureCreateInfo {
    VkStructureTypeEXT    sType;
    const void*           pNext;
    VkFlags               flags;
    AAssetManager*        pAssetManager;
    const char*           pFileName;
} VkTextureCreateInfo;

typedef struct VkTextureProperties {
    VkFormat              format;
    VkExtent3D            extent;
    void*                 pData;
} VkTextureProperties;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateTexture(
    VkDevice                                    device,
    const VkTextureCreateInfo*                  pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkTexture*                                  pTexture);

VKAPI_ATTR void VKAPI_CALL vkDestroyTexture(
    VkDevice                                    device,
    VkTexture                                   texture,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkGetTextureProperties(
    VkTexture                                   texture,
    VkTextureProperties*                        pTextureProperties);

#endif //PRACTICE_VULKAN_VKTEXTURE_H
