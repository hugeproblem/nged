#include "vulkan_texture.h"
#include "backends/imgui_impl_vulkan.h"
#include <vulkan/vulkan.hpp>

#include <stdexcept>

namespace nged {

class TextureResource final
{
public:
  VkDevice        device_;
  VkDescriptorSet descriptorSet_;
  VkImageView     imageView_;
  VkImage         image_;
  VkDeviceMemory  memory_;
  VkSampler       sampler_;
  VkBuffer        uploadBuffer_;
  VkDeviceMemory  uploadMemory_;

  TextureResource()
    : device_(VK_NULL_HANDLE)
    , descriptorSet_(VK_NULL_HANDLE)
    , imageView_(VK_NULL_HANDLE)
    , image_(VK_NULL_HANDLE)
    , memory_(VK_NULL_HANDLE)
    , sampler_(VK_NULL_HANDLE)
    , uploadBuffer_(VK_NULL_HANDLE)
    , uploadMemory_(VK_NULL_HANDLE)
  {
  }
  ~TextureResource()
  {
    release();
  }
  void release();
  ImTextureID id() const
  {
    static_assert(sizeof(ImTextureID) >= sizeof(VkDescriptorSet), "ImTextureID is too small to hold vk texture pointer");
    return reinterpret_cast<ImTextureID>(descriptorSet_);
  }
};

ImTextureID Texture::id() const
{
  return resource_->id();
}

void Texture::release()
{
  resource_->release();
  resource_ = nullptr;
}

static TextureResourcePool<TextureResource>& resourcePool()
{
  static TextureResourcePool<TextureResource> pool;
  return pool;
}

void TextureResource::release()
{
  bool hasResource = descriptorSet_ != VK_NULL_HANDLE || imageView_ != VK_NULL_HANDLE || image_ != VK_NULL_HANDLE || memory_ != VK_NULL_HANDLE || sampler_ != VK_NULL_HANDLE || uploadBuffer_ != VK_NULL_HANDLE || uploadMemory_ != VK_NULL_HANDLE;
  if (descriptorSet_ != VK_NULL_HANDLE) {
    ImGui_ImplVulkan_RemoveTexture(descriptorSet_);
    descriptorSet_ = VK_NULL_HANDLE;
  }
  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
  }
  if (imageView_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, imageView_, nullptr);
    imageView_ = VK_NULL_HANDLE;
  }
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(device_, image_, nullptr);
    image_ = VK_NULL_HANDLE;
  }
  if (uploadBuffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, uploadBuffer_, nullptr);
    uploadBuffer_ = VK_NULL_HANDLE;
  }
  if (uploadMemory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, uploadMemory_, nullptr);
    uploadMemory_ = VK_NULL_HANDLE;
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
  }
  if (hasResource)
    resourcePool().free(this);
}

// Helper function to find Vulkan memory type bits. See ImGui_ImplVulkan_MemoryType() in imgui_impl_vulkan.cpp
static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
    if ((type_filter & (1 << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) ==
          properties)
      return i;

  return 0xFFFFFFFF; // Unable to find memoryType
}

static void check_vk_result(VkResult err)
{
  if (err == 0)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0) {
    std::string msg = "vulkan error: " + std::to_string(err);
    throw std::runtime_error(msg);
  }
}

TexturePtr uploadTextureVK(
  VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool command_pool, VkAllocationCallbacks* allocator,
  uint8_t const* data, int width, int height, AddressMode addrMode, FilterMode filter)
{
  constexpr int channels = 4; // always rgba8
  auto& pool = resourcePool();
  TextureResource* resource = pool.allocate();
  if (!resource)
    return nullptr;

  resource->device_ = device;

  VkDeviceSize imageSize = width * height * 4;

  // Calculate allocation size (in number of bytes)
  size_t image_size = width * height * channels;

  VkResult err;

  // Create the Vulkan image.
  {
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    err = vkCreateImage(device, &info, allocator, &resource->image_);
    check_vk_result(err);
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, resource->image_, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex =
      findMemoryType(physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    err = vkAllocateMemory(
      device, &alloc_info, allocator, &resource->memory_);
    check_vk_result(err);
    err =
      vkBindImageMemory(device, resource->image_, resource->memory_, 0);
    check_vk_result(err);
  }

  // Create the Image View
  {
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = resource->image_;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 1;
    err = vkCreateImageView(device, &info, allocator, &resource->imageView_);
    check_vk_result(err);
  }

  // Create Sampler
  {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU =
      VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border
                                      // color
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.minLod = -1000;
    sampler_info.maxLod = 1000;
    sampler_info.maxAnisotropy = 1.0f;
    err =
      vkCreateSampler(device, &sampler_info, allocator, &resource->sampler_);
    check_vk_result(err);
  }

  // Create Descriptor Set using ImGUI's implementation
  resource->descriptorSet_ =
    ImGui_ImplVulkan_AddTexture(resource->sampler_,
                                resource->imageView_,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Create Upload Buffer
  {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = image_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(
      device, &buffer_info, allocator, &resource->uploadBuffer_);
    check_vk_result(err);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, resource->uploadBuffer_, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex =
      findMemoryType(physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    err = vkAllocateMemory(
      device, &alloc_info, allocator, &resource->uploadMemory_);
    check_vk_result(err);
    err = vkBindBufferMemory(
      device, resource->uploadBuffer_, resource->uploadMemory_, 0);
    check_vk_result(err);
  }

  // Upload to Buffer:
  {
    void* map = NULL;
    err = vkMapMemory(
      device, resource->uploadMemory_, 0, image_size, 0, &map);
    check_vk_result(err);
    memcpy(map, data, image_size);
    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = resource->uploadMemory_;
    range[0].size = image_size;
    err = vkFlushMappedMemoryRanges(device, 1, range);
    check_vk_result(err);
    vkUnmapMemory(device, resource->uploadMemory_);
  }

  // Create a command buffer that will perform following steps when hit in the
  // command queue.
  // TODO: this works in the example, but may need input if this is an
  // acceptable way to access the pool/create the command buffer.
  VkCommandBuffer command_buffer;
  {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    err = vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);
    check_vk_result(err);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    check_vk_result(err);
  }

  // Copy to Image
  {
    VkImageMemoryBarrier copy_barrier[1] = {};
    copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].image = resource->image_;
    copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier[0].subresourceRange.levelCount = 1;
    copy_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         copy_barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(command_buffer,
                           resource->uploadBuffer_,
                           resource->image_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    VkImageMemoryBarrier use_barrier[1] = {};
    use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].image = resource->image_;
    use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    use_barrier[0].subresourceRange.levelCount = 1;
    use_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         use_barrier);
  }

  // End command buffer
  {
    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    err = vkEndCommandBuffer(command_buffer);
    check_vk_result(err);
    err = vkQueueSubmit(queue, 1, &end_info, VK_NULL_HANDLE);
    check_vk_result(err);
    err = vkDeviceWaitIdle(device);
    check_vk_result(err);
  }

  return std::make_shared<Texture>(resource);
}

void releaseAllTextureResources()
{
  auto& pool = resourcePool();
  for (size_t i = 0; i < NGED_MAX_NUM_TEXTURES; ++i)
    pool.getResource(i).release();
}

}
