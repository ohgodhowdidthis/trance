#include "image.h"
#include <iostream>
#include "common/util.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#pragma warning(push, 0)
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include "jpgd/jpgd.h"
#pragma warning(pop)

std::vector<GLuint> Image::textures_to_delete;
std::mutex Image::textures_to_delete_mutex;

Image::Image() : _width{0}, _height{0}, _texture{0}
{
}

Image::Image(uint32_t width, uint32_t height, unsigned char* data)
: _width{width}, _height{height}, _texture{0}, _sf_image{new sf::Image}
{
  _sf_image->create(width, height, data);
}

Image::Image(const sf::Image& image)
: _width{image.getSize().x}
, _height{image.getSize().y}
, _texture{0}
, _sf_image{new sf::Image{image}}
{
}

Image::operator bool() const
{
  return _width && _height;
}

uint32_t Image::width() const
{
  return _width;
}

uint32_t Image::height() const
{
  return _height;
}

uint32_t Image::texture() const
{
  return _texture;
}

bool Image::ensure_texture_uploaded() const
{
  if (_texture || !(*this)) {
    return false;
  }

  // Upload the texture to video memory and set its texture_deleter so that
  // it's cleaned up when there are no more Image objects referencing it.
  glGenTextures(1, &_texture);
  _deleter.reset(new texture_deleter{_texture});

  // Could be split out to a separate call so that Theme doesn't have to hold on
  // to mutex while uploading. This probably doesn't actually block though so no
  // worries.
  glBindTexture(GL_TEXTURE_2D, _texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               _sf_image->getPixelsPtr());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // Return true for purging on the async thread.
  std::cout << ":";
  return true;
}

const std::shared_ptr<sf::Image>& Image::get_sf_image() const
{
  return _sf_image;
}

void Image::clear_sf_image() const
{
  _sf_image.reset();
}

void Image::delete_textures()
{
  textures_to_delete_mutex.lock();
  for (const auto& texture : textures_to_delete) {
    glDeleteTextures(1, &texture);
  }
  textures_to_delete.clear();
  textures_to_delete_mutex.unlock();
}

Image::texture_deleter::~texture_deleter()
{
  textures_to_delete_mutex.lock();
  textures_to_delete.push_back(texture);
  textures_to_delete_mutex.unlock();
}

Image load_image(const std::string& path)
{
  // Load JPEGs with the jpgd library since SFML does not support progressive
  // JPEGs.
  if (ext_is(path, "jpg") || ext_is(path, "jpeg")) {
    int width = 0;
    int height = 0;
    int reqs = 0;
    unsigned char* data =
        jpgd::decompress_jpeg_image_from_file(path.c_str(), &width, &height, &reqs, 4);
    if (!data) {
      std::cerr << "\ncouldn't load " << path << std::endl;
      return {};
    }

    Image image{uint32_t(width), uint32_t(height), data};
    free(data);
    std::cout << ".";
    return image;
  }

  sf::Image sf_image;
  if (!sf_image.loadFromFile(path)) {
    std::cerr << "\ncouldn't load " << path << std::endl;
    return {};
  }

  Image image{sf_image};
  std::cout << ".";
  return image;
}