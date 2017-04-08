#include "streamer.h"
#include <algorithm>
#include <iostream>
#include "common/util.h"
#include "image.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#pragma warning(push, 0)
#include <giflib/gif_lib.h>
#include <libvpx/vp8dx.h>
#include <libvpx/vpx_decoder.h>
#include <libwebm/mkvparser.hpp>
#include <libwebm/mkvreader.hpp>
#pragma warning(pop)

GifStreamer::GifStreamer(const std::string& path) : _path{path}
{
  int error_code = 0;
  _gif = DGifOpenFileName(path.c_str(), &error_code);
  if (!_gif) {
    std::cerr << "couldn't load " << path << ": " << GifErrorString(error_code) << std::endl;
    return;
  }
  if (DGifSlurp(_gif) != GIF_OK) {
    std::cerr << "couldn't slurp " << path << ": " << GifErrorString(_gif->Error) << std::endl;
    return;
  }

  _pixels.reset(new uint32_t[_gif->SWidth * _gif->SHeight]);
  for (int i = 0; i < _gif->SWidth * _gif->SHeight; ++i) {
    _pixels[i] = _gif->SBackGroundColor;
  }
}

GifStreamer::~GifStreamer()
{
  if (_gif) {
    int error_code = 0;
    if (DGifCloseFile(_gif, &error_code) != GIF_OK) {
      std::cerr << "couldn't close " << _path << ": " << GifErrorString(error_code) << std::endl;
    }
  }
}

bool GifStreamer::success() const
{
  return _success;
}

size_t GifStreamer::length() const
{
  if (!_success || !_gif) {
    return 0;
  }
  return static_cast<size_t>(_gif->ImageCount);
}

Image GifStreamer::next_frame()
{
  const auto& frame = _gif->SavedImages[_index];
  bool transparency = false;
  uint8_t transparency_byte = 0;
  // Delay time in hundredths of a second. Ignore it; it messes with the
  // rhythm.
  int delay_time = 1;
  for (int j = 0; j < frame.ExtensionBlockCount; ++j) {
    const auto& block = frame.ExtensionBlocks[j];
    if (block.Function != GRAPHICS_EXT_FUNC_CODE) {
      continue;
    }

    char dispose = (block.Bytes[0] >> 2) & 7;
    transparency = block.Bytes[0] & 1;
    delay_time = block.Bytes[1] + (block.Bytes[2] << 8);
    transparency_byte = block.Bytes[3];

    if (dispose == 2) {
      for (int k = 0; k < _gif->SWidth * _gif->SHeight; ++k) {
        _pixels[k] = _gif->SBackGroundColor;
      }
    }
  }
  auto map = frame.ImageDesc.ColorMap ? frame.ImageDesc.ColorMap : _gif->SColorMap;

  auto fw = frame.ImageDesc.Width;
  auto fh = frame.ImageDesc.Height;
  auto fl = frame.ImageDesc.Left;
  auto ft = frame.ImageDesc.Top;

  for (int y = 0; y < std::min(_gif->SHeight, fh); ++y) {
    for (int x = 0; x < std::min(_gif->SWidth, fw); ++x) {
      uint8_t byte = frame.RasterBits[x + y * fw];
      if (transparency && byte == transparency_byte) {
        continue;
      }
      const auto& c = map->Colors[byte];
      _pixels[fl + x + (ft + y) * _gif->SWidth] =
          c.Red | (c.Green << 8) | (c.Blue << 16) | (0xff << 24);
    }
  }

  return {_gif->SWidth, _gif->SHeight, (unsigned char*) _pixels.get()};
  ++_index;
  std::cout << ";";
}

WebmStreamer::WebmStreamer(const std::string& path) : _path{path}
{
  mkvparser::MkvReader reader;
  if (reader.Open(path.c_str())) {
    std::cerr << "couldn't open " << path << std::endl;
    return {};
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebmlHeader;
  ebmlHeader.Parse(&reader, pos);

  mkvparser::Segment* segment_tmp;
  if (mkvparser::Segment::CreateInstance(&reader, pos, segment_tmp)) {
    std::cerr << "couldn't load " << path << ": segment create failed" << std::endl;
    return {};
  }

  std::unique_ptr<mkvparser::Segment> segment(segment_tmp);
  if (segment->Load() < 0) {
    std::cerr << "couldn't load " << path << ": segment load failed" << std::endl;
    return {};
  }

  const mkvparser::VideoTrack* video_track = nullptr;
  for (unsigned long i = 0; i < segment->GetTracks()->GetTracksCount(); ++i) {
    const auto& track = segment->GetTracks()->GetTrackByIndex(i);
    if (track && track->GetType() && mkvparser::Track::kVideo ||
        track->GetCodecNameAsUTF8() == std::string("VP8")) {
      video_track = (const mkvparser::VideoTrack*) track;
      break;
    }
  }

  if (!video_track) {
    std::cerr << "couldn't load " << path << ": no VP8 video track found" << std::endl;
    return {};
  }

  vpx_codec_ctx_t codec;
  auto codec_error = [&](const std::string& s) {
    auto detail = vpx_codec_error_detail(&codec);
    std::cerr << "couldn't load " << path << ": " << s << ": " << vpx_codec_error(&codec);
    if (detail) {
      std::cerr << ": " << detail;
    }
    std::cerr << std::endl;
  };

  if (vpx_codec_dec_init(&codec, vpx_codec_vp8_dx(), nullptr, 0)) {
    codec_error("initialising codec");
    return {};
  }

  std::vector<Image> result;
  for (auto cluster = segment->GetFirst(); cluster && !cluster->EOS();
       cluster = segment->GetNext(cluster)) {
    const mkvparser::BlockEntry* block;
    if (cluster->GetFirst(block) < 0) {
      std::cerr << "couldn't load " << path << ": couldn't parse first block of cluster"
                << std::endl;
      return {};
    }

    while (block && !block->EOS()) {
      const auto& b = block->GetBlock();
      if (b->GetTrackNumber() == video_track->GetNumber()) {
        for (int i = 0; i < b->GetFrameCount(); ++i) {
          const auto& frame = b->GetFrame(i);
          std::unique_ptr<uint8_t[]> data(new uint8_t[frame.len]);
          reader.Read(frame.pos, frame.len, data.get());

          if (vpx_codec_decode(&codec, data.get(), frame.len, nullptr, 0)) {
            codec_error("decoding frame");
            return {};
          }

          vpx_codec_iter_t it = nullptr;
          while (auto img = vpx_codec_get_frame(&codec, &it)) {
            // Convert I420 (YUV with NxN Y-plane and (N/2)x(N/2) U- and V-
            // planes) to RGB.
            std::unique_ptr<uint32_t[]> data(new uint32_t[img->d_w * img->d_h]);
            auto w = img->d_w;
            auto h = img->d_h;
            for (uint32_t y = 0; y < h; ++y) {
              for (uint32_t x = 0; x < w; ++x) {
                uint8_t Y = img->planes[VPX_PLANE_Y][x + y * img->stride[VPX_PLANE_Y]];
                uint8_t U = img->planes[VPX_PLANE_U][x / 2 + (y / 2) * img->stride[VPX_PLANE_U]];
                uint8_t V = img->planes[VPX_PLANE_V][x / 2 + (y / 2) * img->stride[VPX_PLANE_V]];

                auto cl = [](float f) { return (uint8_t) std::max(0, std::min(255, (int) f)); };
                auto R = cl(1.164f * (Y - 16.f) + 1.596f * (V - 128.f));
                auto G = cl(1.164f * (Y - 16.f) - 0.391f * (U - 128.f) - 0.813f * (V - 128.f));
                auto B = cl(1.164f * (Y - 16.f) + 2.017f * (U - 128.f));
                data[x + y * w] = R | (G << 8) | (B << 16) | (0xff << 24);
              }
            }

            result.emplace_back(w, h, (uint8_t*) data.get());
            std::cout << ";";
          }
        }
      }

      if (cluster->GetNext(block, block) < 0) {
        std::cerr << "couldn't load " << path << ": couldn't parse next block of cluster"
                  << std::endl;
        return {};
      }
    }
  }

  if (vpx_codec_destroy(&codec)) {
    codec_error("destroying codec");
    return {};
  }

  return result;
}

bool WebmStreamer::success() const
{
  return _success;
}

size_t WebmStreamer::length() const
{
  if (!_success) {
    return 0;
  }
}

Image WebmStreamer::next_frame()
{
}

bool is_gif_animated(const std::string& path)
{
  int error_code = 0;
  GifFileType* gif = DGifOpenFileName(path.c_str(), &error_code);
  if (!gif) {
    std::cerr << "couldn't load " << path << ": " << GifErrorString(error_code) << std::endl;
    return false;
  }
  int frames = 0;
  if (DGifSlurp(gif) != GIF_OK) {
    std::cerr << "couldn't slurp " << path << ": " << GifErrorString(gif->Error) << std::endl;
  } else {
    frames = gif->ImageCount;
  }
  if (DGifCloseFile(gif, &error_code) != GIF_OK) {
    std::cerr << "couldn't close " << path << ": " << GifErrorString(error_code) << std::endl;
  }
  return frames > 0;
}

std::unique_ptr<Streamer> load_animation(const std::string& path)
{
  if (ext_is(path, "gif")) {
    return std::unique_ptr<Streamer>{new GifStreamer(path)};
  }
  if (ext_is(path, "webm")) {
    return std::unique_ptr<Streamer>{new WebmStreamer(path)};
  }
  return {};
}