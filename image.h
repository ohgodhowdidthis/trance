#ifndef TRANCE_IMAGE_H
#define TRANCE_IMAGE_H

#include <fstream>
#include <memory>
#include <mutex>
#include <vector>

#pragma warning(push, 0)
#include <mkvwriter.hpp>
#include <SFML/Graphics.hpp>
#include <vpx/vpx_codec.h>
extern "C" {
#include <x264.h>
}
#pragma warning(pop)

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

struct exporter_settings {
  std::string path;
  uint32_t width;
  uint32_t height;
  uint32_t fps;
  uint32_t length;
  uint32_t bitrate;
};

class Exporter {
public:

  virtual bool success() const = 0;
  virtual bool requires_yuv_input() const = 0;
  virtual void encode_frame(const uint8_t* data) = 0;

};

class FrameExporter : public Exporter {
public:

  FrameExporter(const exporter_settings& settings);

  bool success() const override;
  bool requires_yuv_input() const override;
  void encode_frame(const uint8_t* data) override;

private:

  exporter_settings _settings;  
  uint32_t _frame;

};

class WebmExporter : public Exporter {
public:

  WebmExporter(const exporter_settings& settings);
  ~WebmExporter();

  bool success() const override;
  bool requires_yuv_input() const override;
  void encode_frame(const uint8_t* data) override;

private:

  void codec_error(const std::string& error);
  bool add_frame(const vpx_image* data);

  bool _success;
  exporter_settings _settings;
  uint64_t _video_track;

  mkvmuxer::MkvWriter _writer;
  mkvmuxer::Segment _segment;

  vpx_codec_ctx_t _codec;
  vpx_image* _img;
  uint32_t _frame_index;

};

class H264Exporter : public Exporter {
public:

  H264Exporter(const exporter_settings& settings);
  ~H264Exporter();

  bool success() const override;
  bool requires_yuv_input() const override;
  void encode_frame(const uint8_t* data) override;

private:

  bool add_frame(x264_picture_t* pic);

  bool _success;
  exporter_settings _settings;
  std::ofstream _file;

  uint32_t _frame;
  x264_t* _encoder;
  x264_picture_t _pic;
  x264_picture_t _pic_out;

};

#endif