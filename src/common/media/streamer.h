#ifndef TRANCE_SRC_COMMON_MEDIA_STREAMER_H
#define TRANCE_SRC_COMMON_MEDIA_STREAMER_H
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "image.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#pragma warning(push, 0)
#include <libvpx/vp8dx.h>
#include <libvpx/vpx_decoder.h>
#include <libwebm/mkvparser.hpp>
#include <libwebm/mkvreader.hpp>
#pragma warning(pop)

class Image;
struct GifFileType;

class Streamer
{
public:
  virtual ~Streamer() = default;
  virtual bool success() const = 0;
  virtual void reset() = 0;
  virtual Image next_frame() = 0;
};

class AsyncStreamer
{
public:
  AsyncStreamer(const std::function<std::unique_ptr<Streamer>()>& load_function,
                size_t buffer_size);

  bool is_loaded() const;
  void load();
  void clear();

  void maybe_upload_next(const std::function<void(const Image&)>& function);
  Image get_frame(const std::function<void(const Image&)>& function) const;
  void advance_frame(uint32_t global_fps, bool maybe_switch);

  // Called from async update thread.
  void async_update(const std::function<void(const Image&)>& cleanup_function);

private:
  mutable std::mutex _old_mutex;
  mutable std::mutex _current_mutex;
  mutable std::mutex _next_mutex;
  struct Animation {
    std::unique_ptr<Streamer> streamer;
    std::deque<Image> buffer;
    bool end;
  };
  std::function<std::unique_ptr<Streamer>()> _load_function;
  const size_t _buffer_size;
  Animation _current;
  Animation _next;

  std::unique_ptr<Streamer> _old_streamer;
  std::deque<Image> _old_buffer;

  float _update_counter = 0.f;
  std::size_t _index = 0;
  bool _backwards = false;
  bool _reached_end = false;
};

class GifStreamer : public Streamer
{
public:
  GifStreamer(const std::string& path);
  ~GifStreamer() override;

  bool success() const override;
  void reset() override;
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
  ~WebmStreamer() override;

  bool success() const override;
  void reset() override;
  Image next_frame() override;

private:
  void codec_error(const std::string& error);

  const std::string _path;
  bool _success = false;

  mkvparser::MkvReader _reader;
  std::unique_ptr<mkvparser::Segment> _segment;
  vpx_codec_ctx_t _codec;

  const mkvparser::VideoTrack* _video_track = nullptr;
  const mkvparser::Cluster* _cluster = nullptr;
  const mkvparser::BlockEntry* _block = nullptr;
  bool _cluster_eos = false;

  int _block_index = -1;
  bool _iterating = false;
  vpx_codec_iter_t _it = nullptr;
  const vpx_image_t* _image = nullptr;
  std::unique_ptr<uint8_t[]> _data;
};

bool is_gif_animated(const std::string& path);
std::unique_ptr<Streamer> load_animation(const std::string& path);

#endif