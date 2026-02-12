#include "texture.h"
#include <d3d11.h>

namespace nged {

class TextureResource
{
public:
  ID3D11Texture2D* texture_ = nullptr;
  ID3D11ShaderResourceView* srv_ = nullptr;
  ~TextureResource();
  void release();
  ImTextureID id() const { return reinterpret_cast<ImTextureID>(srv_); }
};

static TextureResourcePool<TextureResource>& resourcePool()
{
  static TextureResourcePool<TextureResource> pool;
  return pool;
};

TextureResource::~TextureResource()
{
  release();
}

void TextureResource::release()
{
  if (texture_)
    texture_->Release();
  if (srv_)
    srv_->Release();

  if (texture_ && srv_)
    resourcePool().free(this);

  texture_ = nullptr;
  srv_ = nullptr;
}

ImTextureID Texture::id() const { return resource_->id(); }
void Texture::release()
{
  resource_->release();
  resource_ = nullptr;
}

TexturePtr uploadTextureDX11(ID3D11Device* device, uint8_t const* data, int width, int height, AddressMode addrMode, FilterMode filter)
{
  constexpr int channels = 4; // always rgba8
  TextureResource* resource = resourcePool().allocate();
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;
  D3D11_SUBRESOURCE_DATA subres;
  subres.pSysMem = data;
  subres.SysMemPitch = width * channels;
  subres.SysMemSlicePitch = 0;
  ID3D11Texture2D* texture = nullptr;
  if (FAILED(device->CreateTexture2D(&desc, &subres, &texture))) {
    delete resource;
    return nullptr;
  }
  resource->texture_ = texture;
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.Format = desc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.MipLevels = 1;
  ID3D11ShaderResourceView* srv = nullptr;
  if (FAILED(device->CreateShaderResourceView(texture, &srvDesc, &srv))) {
    delete resource;
    return nullptr;
  }
  resource->srv_ = srv;
  IM_ASSERT(texture && srv);
  return std::make_shared<Texture>(resource);
}

} // namespace nged
