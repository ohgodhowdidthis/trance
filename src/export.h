#ifndef TRANCE_EXPORT_H
#define TRANCE_EXPORT_H

#include <cstdint>
#include <fstream>
#include <string>

#pragma warning(push, 0)
#include <libvpx/vpx_codec.h>
#include <libwebm/mkvwriter.hpp>
extern "C" {
#include <x264/x264.h>
}
#pragma warning(pop)

struct exporter_settings {
  std::string path;
  uint32_t width;
  uint32_t height;
  uint32_t fps;
  uint32_t length;
  uint32_t quality;
  uint32_t threads;
};

class Exporter
{
public:
  virtual ~Exporter()
  {
  }
  virtual bool success() const = 0;
  virtual bool requires_yuv_input() const = 0;
  virtual void encode_frame(const uint8_t* data) = 0;
};

class FrameExporter : public Exporter
{
public:
  FrameExporter(const exporter_settings& settings);

  bool success() const override;
  bool requires_yuv_input() const override;
  void encode_frame(const uint8_t* data) override;

private:
  exporter_settings _settings;
  uint32_t _frame;
};

class WebmExporter : public Exporter
{
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

class H264Exporter : public Exporter
{
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