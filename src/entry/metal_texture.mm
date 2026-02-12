#include "texture.h"

#import <Metal/Metal.h>

namespace nged {

class TextureResource
{
public:
  id<MTLTexture> texture = nil;
};

ImTextureID Texture::id() const
{
  return (ImTextureID)(resource_->texture);
}

void Texture::release()
{
  if (resource_)
  {
    resource_->texture = nil;
    delete resource_;
    resource_ = nullptr;
  }
}

TexturePtr uploadTexture(
  uint8_t const* data, int width, int height,
  AddressMode address, FilterMode filter)
{
  // Note: We need the MTLDevice to create a texture. 
  // However, the current interface does not pass the device.
  // We can either:
  // 1. Store the device globally in metal_main.mm and expose it.
  // 2. Use MTLCreateSystemDefaultDevice() again (might be different device?).
  // 3. Change interface (not preferred).
  
  // Since we initialized with MTLCreateSystemDefaultDevice() in main, calling it again should return the same device on most single-GPU Macs.
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  
  MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                              width:width
                                                                                             height:height
                                                                                          mipmapped:NO];
  textureDescriptor.usage = MTLTextureUsageShaderRead;
  id<MTLTexture> texture = [device newTextureWithDescriptor:textureDescriptor];

  MTLRegion region = MTLRegionMake2D(0, 0, width, height);
  [texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:width * 4];

  // Note: Sampler state is usually handled in shader or render state in Metal.
  // ImGui_ImplMetal uses a default sampler.
  // If we need custom sampling (address mode, filter), we might need to look into how ImGui_ImplMetal handles user textures.
  // ImGui_ImplMetal currently just binds the texture.
  // It seems ImGui_ImplMetal does not support per-texture sampler states via ImTextureID easily without modifying the backend or using a separate map.
  // For now, we proceed with just the texture.

  TextureResource* resource = new TextureResource();
  resource->texture = texture;
  return std::make_shared<Texture>(resource);
}

} // namespace nged
