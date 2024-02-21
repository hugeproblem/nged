
#pragma once

#include "texture.h"
#include <d3d12.h>

namespace nged {

TexturePtr uploadTextureDX12(
  ID3D12Device* device,
  uint8_t const* data, int width, int height, AddressMode addrMode, FilterMode filter);

void initializeTextureResourcePool(ID3D12Device* device, ID3D12DescriptorHeap* srvDescHeap);
void releaseAllTextureResources();

}
