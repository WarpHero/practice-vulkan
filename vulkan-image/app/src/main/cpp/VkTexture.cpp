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

#include <vector>
#include <stb_image.h>
#include "VkTexture.h"

using namespace std;

VkResult vkCreateTexture(
    VkDevice                                    device,
    const VkTextureCreateInfo*                  pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkTexture*                                  pTexture) {

    auto pAsset = AAssetManager_open(pCreateInfo->pAssetManager,
                                     pCreateInfo->pFileName,
                                     AASSET_MODE_BUFFER);
    if (!pAsset) {
        return VK_ERROR_UNKNOWN;
    }

    auto assetLength = AAsset_getLength(pAsset);
    vector<uint8_t> asset(assetLength);
    AAsset_read(pAsset, asset.data(), assetLength);
    AAsset_close(pAsset);

    auto pTextureProperties = make_unique<VkTextureProperties>();
    int width;
    int height;
    int component;
    pTextureProperties->pData = stbi_load_from_memory(asset.data(),
                                                      assetLength,
                                                      &width,
                                                      &height,
                                                      &component,
                                             4);
    if (!pTextureProperties->pData) {
        return VK_ERROR_UNKNOWN;
    }
    pTextureProperties->format = VK_FORMAT_R8G8B8A8_UNORM;
    pTextureProperties->extent.width = width;
    pTextureProperties->extent.height = height;
    pTextureProperties->extent.depth = 1;

    *pTexture = reinterpret_cast<VkTexture>(pTextureProperties.release());
    return VK_SUCCESS;
}

void vkDestroyTexture(
    VkDevice                                    device,
    VkTexture                                   texture,
    const VkAllocationCallbacks*                pAllocator) {
    auto pTextureProperties = reinterpret_cast<VkTextureProperties*>(texture);
    stbi_image_free(pTextureProperties->pData);
    delete pTextureProperties;
}

void vkGetTextureProperties(
    VkTexture                                   texture,
    VkTextureProperties*                        pTextureProperties) {
    *pTextureProperties = *reinterpret_cast<VkTextureProperties *>(texture);
}
