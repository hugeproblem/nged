#pragma once

#include <imgui.h>

#include <stdint.h>
#include <memory>
#include <vector>

#if defined(NGED_BACKEND_DX11)
#include <d3d11.h>
#elif defined(NGED_BACKEND_DX12)
#include <d3d12.h>
#elif defined(NGED_BACKEND_VULKAN)
#include <vulkan/vulkan.hpp>
#endif


#ifndef NGED_MAX_NUM_TEXTURES
#define NGED_MAX_NUM_TEXTURES (1024-1) // 1 is reserved for font texture
#endif

namespace nged {

class Texture;
using TexturePtr = std::shared_ptr<Texture>;

class TextureResource;

class Texture final
{
  TextureResource* resource_ = nullptr;
  Texture(Texture const&) = delete;
public:
  Texture(TextureResource* resource): resource_(resource) {}
  ~Texture() { release(); }
  void release();
  ImTextureID id() const;
};

template <class TextureResourceImpl>
class TextureResourcePool
{
  TextureResourceImpl resources_[NGED_MAX_NUM_TEXTURES];
  std::vector<size_t> freeIndices_;
public:
  TextureResourcePool()
  {
    for (size_t i = 0; i < NGED_MAX_NUM_TEXTURES; ++i)
      freeIndices_.push_back(i);
  }
  TextureResourceImpl* allocate()
  {
    if (freeIndices_.empty())
      return nullptr;
    size_t index = freeIndices_.back();
    freeIndices_.pop_back();
    return &resources_[index];
  }
  void free(TextureResourceImpl* resource)
  {
    size_t index = resource - resources_;
    freeIndices_.push_back(index);
  }
  TextureResourceImpl& getResource(size_t index) { return resources_[index]; }
};


enum class AddressMode
{
  Repeat, Clamp, Border
};

enum class FilterMode
{
  Nearest, Linear
};

// upload r8g8b8a8 texture
TexturePtr uploadTexture(
  uint8_t const* data,
  int width,
  int height,
  AddressMode address = AddressMode::Repeat,
  FilterMode filter = FilterMode::Linear
);

}
