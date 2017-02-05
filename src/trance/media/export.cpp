#include "export.h"
#include <iostream>

#pragma warning(push, 0)
#define VPX_CODEC_DISABLE_COMPAT 1
#include <libvpx/vp8cx.h>
#include <libvpx/vpx_encoder.h>
#include <libvpx/vpx_image.h>
#include <SFML/Graphics.hpp>
#pragma warning(pop)

FrameExporter::FrameExporter(const exporter_settings& settings) : _settings(settings), _frame{0}
{
}

bool FrameExporter::success() const
{
  return true;
}

bool FrameExporter::requires_yuv_input() const
{
  return false;
}

void FrameExporter::encode_frame(const uint8_t* data)
{
  auto counter_str = std::to_string(_frame);
  std::size_t padding =
      std::to_string(_settings.fps * _settings.length).length() - counter_str.length();

  std::size_t index = _settings.path.find_last_of('.');
  auto frame_path = _settings.path.substr(0, index) + '_' + std::string(padding, '0') +
      counter_str + _settings.path.substr(index);

  sf::Image image;
  image.create(_settings.width, _settings.height, data);
  image.saveToFile(frame_path);
  ++_frame;
}

WebmExporter::WebmExporter(const exporter_settings& settings)
: _success{false}, _settings(settings), _video_track{0}, _img{nullptr}, _frame_index{0}
{
  if (!_writer.Open(settings.path.c_str())) {
    std::cerr << "couldn't open " << settings.path << " for writing" << std::endl;
    return;
  }

  if (!_segment.Init(&_writer)) {
    std::cerr << "couldn't initialise muxer segment" << std::endl;
    return;
  }
  _segment.set_mode(mkvmuxer::Segment::kFile);
  _segment.OutputCues(true);
  _segment.GetSegmentInfo()->set_writing_app("trance");

  _video_track = _segment.AddVideoTrack(settings.width, settings.height, 0);
  if (!_video_track) {
    std::cerr << "couldn't add video track" << std::endl;
    return;
  }

  auto video = (mkvmuxer::VideoTrack*) _segment.GetTrackByNumber(_video_track);
  if (!video) {
    std::cerr << "couldn't get video track" << std::endl;
    return;
  }
  video->set_frame_rate(settings.fps);
  _segment.GetCues()->set_output_block_number(true);
  _segment.CuesTrack(_video_track);

  // See http://www.webmproject.org/docs/encoder-parameters.
  vpx_codec_enc_cfg_t cfg;
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &cfg, 0)) {
    std::cerr << "couldn't get default codec config" << std::endl;
    return;
  }
  auto area = _settings.width * _settings.height;
  auto bitrate = (1 + 2 * (4 - _settings.quality)) * area / 1024;

  cfg.g_w = _settings.width;
  cfg.g_h = _settings.height;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = _settings.fps;
  cfg.g_lag_in_frames = 24;
  cfg.g_threads = settings.threads;
  cfg.kf_min_dist = 0;
  cfg.kf_max_dist = 256;
  cfg.rc_target_bitrate = bitrate;

  if (vpx_codec_enc_init(&_codec, vpx_codec_vp8_cx(), &cfg, 0)) {
    codec_error("couldn't initialise encoder");
    return;
  }

  _img = vpx_img_alloc(nullptr, VPX_IMG_FMT_I420, _settings.width, _settings.height, 16);
  if (!_img) {
    std::cerr << "couldn't allocate image for encoding" << std::endl;
    return;
  }
  _success = true;
}

WebmExporter::~WebmExporter()
{
  if (_img) {
    vpx_img_free(_img);
  }
  // Flush encoder.
  while (add_frame(nullptr))
    ;

  if (vpx_codec_destroy(&_codec)) {
    codec_error("failed to destroy codec");
    return;
  }
  if (!_segment.Finalize()) {
    std::cerr << "couldn't finalise muxer segment" << std::endl;
    return;
  }
  _writer.Close();
}

bool WebmExporter::success() const
{
  return _success;
}

bool WebmExporter::requires_yuv_input() const
{
  return true;
}

void WebmExporter::encode_frame(const uint8_t* data)
{
  // Convert YUV to YUV420.
  for (uint32_t y = 0; y < _settings.height; ++y) {
    for (uint32_t x = 0; x < _settings.width; ++x) {
      _img->planes[VPX_PLANE_Y][x + y * _img->stride[VPX_PLANE_Y]] =
          data[4 * (x + y * _settings.width)];
    }
  }
  for (uint32_t y = 0; y < _settings.height / 2; ++y) {
    for (uint32_t x = 0; x < _settings.width / 2; ++x) {
      auto c00 = 4 * (2 * x + 2 * y * _settings.width);
      auto c01 = 4 * (2 * x + (1 + 2 * y) * _settings.width);
      auto c10 = 4 * (1 + 2 * x + 2 * y * _settings.width);
      auto c11 = 4 * (1 + 2 * x + (1 + 2 * y) * _settings.width);

      _img->planes[VPX_PLANE_U][x + y * _img->stride[VPX_PLANE_U]] =
          (data[1 + c00] + data[1 + c01] + data[1 + c10] + data[1 + c11]) / 4;
      _img->planes[VPX_PLANE_V][x + y * _img->stride[VPX_PLANE_V]] =
          (data[2 + c00] + data[2 + c01] + data[2 + c10] + data[2 + c11]) / 4;
    }
  }
  add_frame(_img);
}

void WebmExporter::codec_error(const std::string& s)
{
  auto detail = vpx_codec_error_detail(&_codec);
  std::cerr << s << ": " << vpx_codec_error(&_codec);
  if (detail) {
    std::cerr << ": " << detail;
  }
  std::cerr << std::endl;
}

bool WebmExporter::add_frame(const vpx_image* data)
{
  auto result =
      vpx_codec_encode(&_codec, data, _frame_index++, 1, 0,
                       _settings.quality <= 1 ? VPX_DL_BEST_QUALITY : VPX_DL_GOOD_QUALITY);
  if (result != VPX_CODEC_OK) {
    codec_error("couldn't encode frame");
    return false;
  }

  vpx_codec_iter_t iter = nullptr;
  const vpx_codec_cx_pkt_t* packet = nullptr;
  bool found_packet = false;
  while (packet = vpx_codec_get_cx_data(&_codec, &iter)) {
    found_packet = true;
    if (packet->kind != VPX_CODEC_CX_FRAME_PKT) {
      continue;
    }
    auto timestamp_ns = 1000000000 * packet->data.frame.pts / _settings.fps;
    bool result =
        _segment.AddFrame((uint8_t*) packet->data.frame.buf, packet->data.frame.sz, _video_track,
                          timestamp_ns, packet->data.frame.flags & VPX_FRAME_IS_KEY);
    if (!result) {
      std::cerr << "couldn't add frame" << std::endl;
      return false;
    }
  }

  return found_packet;
};

H264Exporter::H264Exporter(const exporter_settings& settings)
: _success{false}
, _settings(settings)
, _file{settings.path, std::ios::binary}
, _frame{0}
, _encoder{nullptr}
{
  x264_param_t param;
  // Best quality (0) -> "veryslow" (8); worst quality (4) -> "ultrafast" (0).
  auto quality = std::to_string(2 * (4 - settings.quality));
  if (x264_param_default_preset(&param, quality.c_str(), "film") < 0) {
    std::cerr << "couldn't get default preset" << std::endl;
    return;
  }
  param.i_threads = settings.threads > 1 ? settings.threads - 1 : 1;
  param.i_lookahead_threads = settings.threads > 1 ? 1 : 0;

  param.i_width = settings.width;
  param.i_height = settings.height;
  param.i_fps_num = settings.fps;
  param.i_fps_den = 1;
  param.i_frame_total = settings.fps * settings.length;
  param.i_keyint_min = 0;
  param.i_keyint_max = settings.fps;
  if (x264_param_apply_profile(&param, "high") < 0) {
    std::cerr << "couldn't get apply profile" << std::endl;
    return;
  }

  _encoder = x264_encoder_open(&param);
  if (!_encoder) {
    std::cerr << "couldn't create encoder" << std::endl;
    return;
  }
  if (x264_picture_alloc(&_pic, X264_CSP_I420, _settings.width, _settings.height) < 0) {
    std::cerr << "couldn't allocate picture" << std::endl;
    return;
  }
  _success = true;
}

H264Exporter::~H264Exporter()
{
  // Flush encoder.
  while (x264_encoder_delayed_frames(_encoder)) {
    add_frame(nullptr);
  }
  x264_picture_clean(&_pic);
  x264_encoder_close(_encoder);
  _file.close();
}

bool H264Exporter::add_frame(x264_picture_t* pic)
{
  x264_nal_t* nal;
  int nal_size;
  int frame_size = x264_encoder_encode(_encoder, &nal, &nal_size, pic, &_pic_out);
  if (frame_size < 0) {
    std::cerr << "couldn't encode frame" << std::endl;
    return false;
  }
  if (frame_size) {
    _file.write((const char*) nal->p_payload, frame_size);
  }
  return true;
}

bool H264Exporter::success() const
{
  return _success;
}

bool H264Exporter::requires_yuv_input() const
{
  return true;
}

void H264Exporter::encode_frame(const uint8_t* data)
{
  // Convert YUV to YUV420.
  for (uint32_t y = 0; y < _settings.height; ++y) {
    for (uint32_t x = 0; x < _settings.width; ++x) {
      _pic.img.plane[0][x + y * _settings.width] = data[4 * (x + y * _settings.width)];
    }
  }
  for (uint32_t y = 0; y < _settings.height / 2; ++y) {
    for (uint32_t x = 0; x < _settings.width / 2; ++x) {
      auto c00 = 4 * (2 * x + 2 * y * _settings.width);
      auto c01 = 4 * (2 * x + (1 + 2 * y) * _settings.width);
      auto c10 = 4 * (1 + 2 * x + 2 * y * _settings.width);
      auto c11 = 4 * (1 + 2 * x + (1 + 2 * y) * _settings.width);

      _pic.img.plane[1][x + y * _settings.width / 2] =
          (data[1 + c00] + data[1 + c01] + data[1 + c10] + data[1 + c11]) / 4;
      _pic.img.plane[2][x + y * _settings.width / 2] =
          (data[2 + c00] + data[2 + c01] + data[2 + c10] + data[2 + c11]) / 4;
    }
  }
  _pic.i_pts = _frame;
  add_frame(&_pic);
  _frame++;
}