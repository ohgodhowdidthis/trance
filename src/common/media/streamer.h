#ifndef TRANCE_SRC_COMMON_MEDIA_STREAMER_H
#define TRANCE_SRC_COMMON_MEDIA_STREAMER_H
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

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
  std::atomic<bool> _success = false;

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