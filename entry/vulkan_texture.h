#pragma once
#include "texture.h"
#include <vulkan/vulkan.hpp>

namespace nged {

TexturePtr uploadTextureVK(
  VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool command_pool, VkAllocationCallbacks* allocator,
  uint8_t const* data, int width, int height, AddressMode addrMode, FilterMode filter);
void releaseAllTextureResources();

}
