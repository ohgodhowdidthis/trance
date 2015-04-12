#ifndef TRANCE_IMAGE_H
#define TRANCE_IMAGE_H

#include <memory>
#include <mutex>
#include <vector>
#include <mkvwriter.hpp>
#include <SFML/Graphics.hpp>
#include <vpx/vpx_codec.h>

struct vpx_image;

// In-memory image with load-on-request OpenGL texture which is ref-counted
// and automatically unloaded once no longer used.
class Image {
public:

  Image();
  Image(uint32_t width, uint32_t height, unsigned char* data);
  Image(const sf::Image& image);
  explicit operator bool() const;

  uint32_t width() const;
  uint32_t height() const;
  uint32_t texture() const;

  // Double-indirection for easy purging.
  typedef std::shared_ptr<std::shared_ptr<sf::Image>> sf_image_ptr;
  // Call from OpenGL context thread only!
  bool ensure_texture_uploaded() const;
  sf_image_ptr& get_sf_image() const;
  static void delete_textures();

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

  mutable uint32_t _texture;
  mutable sf_image_ptr _sf_image;
  mutable std::shared_ptr<texture_deleter> _deleter;
};

bool is_gif_animated(const std::string& path);
Image load_image(const std::string& path);
std::vector<Image> load_animation(const std::string& path);

class WebmExporter {
public:

  WebmExporter(const std::string& path, uint32_t width, uint32_t height,
               uint32_t fps, uint32_t bitrate);
  ~WebmExporter();

  void encode_frame(const sf::Image& image);

private:

  void codec_error(const std::string& error);
  bool add_frame(const vpx_image* data);

  bool _success;
  uint32_t _width;
  uint32_t _height;
  uint32_t _fps;
  uint64_t _video_track;

  mkvmuxer::MkvWriter _writer;
  mkvmuxer::Segment _segment;

  vpx_codec_ctx_t _codec;
  vpx_image* _img;
  uint32_t _frame_index;

};

#endif