#ifndef TRANCE_IMAGE_H
#define TRANCE_IMAGE_H

#include <memory>
#include <mutex>
#include <vector>

namespace sf
{
class Image;
}
struct vpx_image;

// In-memory image with load-on-request OpenGL texture which is ref-counted
// and automatically unloaded once no longer used.
class Image
{
public:
  Image();
  Image(uint32_t width, uint32_t height, unsigned char* data);
  Image(const sf::Image& image);
  explicit operator bool() const;

  uint32_t width() const;
  uint32_t height() const;
  uint32_t texture() const;

  // Call from OpenGL context thread only!
  bool ensure_texture_uploaded() const;
  const std::shared_ptr<sf::Image>& get_sf_image() const;
  void clear_sf_image() const;
  static void delete_textures();

private:
  // In order to ensure textures are deleted from the rendering thread, we
  // use a separate set.
  static std::vector<uint32_t> textures_to_delete;
  static std::mutex textures_to_delete_mutex;

  struct texture_deleter {
    texture_deleter(uint32_t texture) : texture{texture}
    {
    }
    ~texture_deleter();
    uint32_t texture;
  };

  uint32_t _width;
  uint32_t _height;

  mutable uint32_t _texture;
  mutable std::shared_ptr<sf::Image> _sf_image;
  mutable std::shared_ptr<texture_deleter> _deleter;
};

bool is_gif_animated(const std::string& path);
Image load_image(const std::string& path);
std::vector<Image> load_animation(const std::string& path);

#endif