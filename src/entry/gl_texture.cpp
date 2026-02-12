#include "texture.h"

#include <GLFW/glfw3.h>

namespace nged {

class TextureResource
{
public:
  GLuint id_ = 0;
};

ImTextureID Texture::id() const
{
  return (ImTextureID)(size_t(resource_->id_));
}

void Texture::release()
{
  if (resource_)
  {
    glDeleteTextures(1, &resource_->id_);
    delete resource_;
    resource_ = nullptr;
  }
}

TexturePtr uploadTexture(
  uint8_t const* data, int width, int height,
  AddressMode address, FilterMode filter)
{
  TextureResource* resource = new TextureResource();
  glGenTextures(1, &resource->id_);
  glBindTexture(GL_TEXTURE_2D, resource->id_);

  GLuint addressFlag = GL_REPEAT;
  GLuint filterFlag = GL_LINEAR;
  if (address == AddressMode::Clamp)
    addressFlag = 0x812F/*GL_CLAMP_TO_EDGE*/;
  else if (address == AddressMode::Border)
    addressFlag = 0x812D/*GL_CLAMP_TO_BORDER*/;
  if (filter == FilterMode::Nearest)
    filterFlag = GL_NEAREST;

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addressFlag);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressFlag);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterFlag);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterFlag);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glBindTexture(GL_TEXTURE_2D, 0);
  return std::make_shared<Texture>(resource);
}

} // namespace nged
