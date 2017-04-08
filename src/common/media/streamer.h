#ifndef TRANCE_SRC_COMMON_MEDIA_STREAMER_H
#define TRANCE_SRC_COMMON_MEDIA_STREAMER_H
#include <cstddef>
#include <memory>
#include <string>

class Image;
struct GifFileType;

class Streamer
{
public:
  virtual bool success() const = 0;
  virtual size_t length() const = 0;
  virtual Image next_frame() = 0;
};

class GifStreamer : public Streamer
{
public:
  GifStreamer(const std::string& path);
  ~GifStreamer();

  bool success() const override;
  size_t length() const override;
  Image next_frame() override;

private:
  const std::string _path;
  bool _success = false;
  GifFileType* _gif = nullptr;
  std::size_t _index = 0;
  std::unique_ptr<uint32_t[]> _pixels;
};

class WebmStreamer : public Streamer
{
public:
  WebmStreamer(const std::string& path);

  bool success() const override;
  size_t length() const override;
  Image next_frame() override;

private:
  const std::string _path;
  bool _success = false;
};

bool is_gif_animated(const std::string& path);
std::unique_ptr<Streamer> load_animation(const std::string& path);

#endif