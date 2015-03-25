#ifndef TRANCE_IMAGE_H
#define TRANCE_IMAGE_H

#include <memory>
#include <mutex>
#include <vector>
#include <SFML/Graphics.hpp>

// In-memory image with load-on-request OpenGL texture which is ref-counted
// and automatically unloaded once no longer used.
class Image {
public:

  Image();
  Image(const std::string& path, uint32_t width, uint32_t height,
        unsigned char* data);
  Image(const std::string& path, const sf::Image& image);
  explicit operator bool() const;

  uint32_t width() const;
  uint32_t height() const;
  uint32_t texture() const;

  // Call from OpenGL context thread only!
  void ensure_texture_uploaded() const;
  static void delete_textures();

  std::string _path; // TODO: only needed while image
                     // loading/unloading uses dumb method.

private:

  // In order to ensure textures are deleted from the rendering thread, we
  // use a separate set.
  static std::vector<uint32_t> textures_to_delete;
  static std::mutex textures_to_delete_mutex;

  struct texture_deleter {
    texture_deleter(uint32_t texture)
    : texture{texture} {}
    ~texture_deleter();
    uint32_t texture;
  };

  uint32_t _width;
  uint32_t _height;
  std::shared_ptr<sf::Image> _sf_image;

  mutable uint32_t _texture;
  mutable std::shared_ptr<texture_deleter> _deleter;
};

bool is_gif_animated(const std::string& path);
Image load_image(const std::string& path);
std::vector<Image> load_animation(const std::string& path);

#endif