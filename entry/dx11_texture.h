#pragma once

#include "texture.h"
#include <d3d11.h>

namespace nged {

TexturePtr uploadTextureDX11(
  ID3D11Device* device,
  uint8_t const* data, int width, int height, AddressMode addrMode, FilterMode filter);

}

