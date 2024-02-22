#include "texture.h"

// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>            // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

namespace nged {

class TextureResource
{
public:
  GLuint id_ = 0;
};

ImTextureID Texture::id() const
{
  return reinterpret_cast<ImTextureID>(size_t(resource_->id_));
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
    addressFlag = GL_CLAMP_TO_EDGE;
  else if (address == AddressMode::Border)
    addressFlag = GL_CLAMP_TO_BORDER;
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
