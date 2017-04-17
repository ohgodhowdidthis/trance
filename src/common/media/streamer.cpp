#include "streamer.h"
#include <algorithm>
#include <iostream>
#include "common/util.h"

#pragma warning(push, 0)
#include <giflib/gif_lib.h>
#pragma warning(pop)

AsyncStreamer::AsyncStreamer(const std::function<std::unique_ptr<Streamer>()>& load_function,
                             size_t buffer_size)
: _load_function{load_function}, _buffer_size{buffer_size}
{
}

bool AsyncStreamer::is_loaded() const
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  if (_current.streamer) {
    return _current.end || _current.buffer.size() >= _buffer_size;
  }
  std::lock_guard<std::mutex> next_lock{_next_mutex};
  return _current.buffer.empty() && _next.buffer.empty();
}

void AsyncStreamer::load()
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  if (!_current.streamer) {
    _current.streamer = _load_function();
    _current.buffer.clear();
    _current.end = false;

    _update_counter = 0.f;
    _reached_end = false;
    _backwards = false;
    _index = 0;
  }
}

void AsyncStreamer::clear()
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  std::lock_guard<std::mutex> next_lock{_next_mutex};
  _current.streamer.reset();
  _next.streamer.reset();
  _current.end = false;
  _next.end = false;

  _update_counter = 0.f;
  _reached_end = false;
  _backwards = false;
  _index = 0;
}

void AsyncStreamer::maybe_upload_next(const std::function<void(const Image&)>& function)
{
  {
    std::lock_guard<std::mutex> lock{_current_mutex};
    if (!_current.buffer.empty()) {
      function(_current.buffer[random(_current.buffer.size())]);
    }
  }
  {
    std::lock_guard<std::mutex> lock{_next_mutex};
    if (!_next.buffer.empty()) {
      function(_next.buffer[random(_next.buffer.size())]);
    }
  }
}

Image AsyncStreamer::get_frame(const std::function<void(const Image&)>& function) const
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  if (_index >= _current.buffer.size()) {
    return {};
  }
  function(_current.buffer[_index]);
  Image image = _current.buffer[_index];
  return image;
}

void AsyncStreamer::advance_frame(uint32_t global_fps, bool maybe_switch)
{
  if (!_current.streamer) {
    return;
  }
  _update_counter += (120.f / global_fps) / 8.f;
  while (_update_counter > 1.f) {
    _update_counter -= 1.f;

    std::lock_guard<std::mutex> lock{_current_mutex};
    {
      std::lock_guard<std::mutex> next_lock{_next_mutex};
      if (!_old_streamer && _old_buffer.empty() && _next.streamer &&
          (!_current.streamer->success() || _current.buffer.empty() ||
           (maybe_switch && _reached_end && (_next.end || _next.buffer.size() >= _buffer_size)))) {
        _current.streamer.swap(_next.streamer);
        _current.buffer.swap(_next.buffer);
        _current.end = _next.end;

        {
          std::lock_guard<std::mutex> lock{_old_mutex};
          _next.streamer.swap(_old_streamer);
          _next.buffer.swap(_old_buffer);
        }
        _next.end = false;

        _reached_end = false;
        _backwards = false;
        _index = 0;
        return;
      }
    }
    if (_backwards) {
      if (_index > 0) {
        --_index;
      } else {
        _backwards = false;
        if (_index < _current.buffer.size() - 1) {
          ++_index;
        }
      }
    } else {
      if (_index < _current.buffer.size() - 1) {
        if (++_index == _current.buffer.size() - 1 && _current.end) {
          _reached_end = true;
        }
      } else {
        _backwards = true;
        if (_current.end) {
          _reached_end = true;
        }
        if (_index > 0) {
          --_index;
        }
      }
    }
  }
}

void AsyncStreamer::async_update(const std::function<void(const Image&)>& cleanup_function)
{
  {
    std::lock_guard<std::mutex> lock{_old_mutex};
    if (_old_streamer) {
      _old_streamer.reset();
    }
    while (_old_buffer.size() > _buffer_size) {
      cleanup_function(_old_buffer.front());
      _old_buffer.pop_front();
    }
    if (!_old_buffer.empty()) {
      cleanup_function(_old_buffer.front());
      _old_buffer.pop_front();
    }
  }

  do {
    std::lock_guard<std::mutex> lock{_current_mutex};
    if (!_current.streamer || _current.end || !_index) {
      break;
    }
    auto image = _current.streamer->next_frame();
    if (image) {
      _current.buffer.push_back(image);
      _current.buffer.pop_front();
      --_index;
    } else {
      _current.end = true;
    }
  } while (true);

  {
    std::unique_lock<std::mutex> lock{_current_mutex};
    std::unique_lock<std::mutex> next_lock{_next_mutex};
    if (_current.streamer && !_next.streamer) {
      next_lock.unlock();
      lock.unlock();
      auto next_streamer = _load_function();
      lock.lock();
      next_lock.lock();
      if (_current.streamer && !_next.streamer) {
        _next.streamer = std::move(next_streamer);
        _next.buffer.clear();
        _next.end = false;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock{_current_mutex};
    if (_current.streamer && !_current.end && _current.buffer.size() < _buffer_size) {
      auto image = _current.streamer->next_frame();
      if (image) {
        _current.buffer.push_back(image);
        if (_index > 0) {
          _current.buffer.pop_front();
          --_index;
        }
      } else {
        _current.end = true;
      }
    }
    if (!_current.streamer && !_current.buffer.empty()) {
      cleanup_function(_current.buffer.front());
      _current.buffer.pop_front();
    }
  }

  {
    std::lock_guard<std::mutex> lock{_next_mutex};
    if (_next.streamer && !_next.end && _next.buffer.size() < _buffer_size) {
      auto image = _next.streamer->next_frame();
      if (image) {
        _next.buffer.push_back(image);
      } else {
        _next.end = true;
      }
    }
    if (!_next.streamer && !_next.buffer.empty()) {
      cleanup_function(_next.buffer.front());
      _next.buffer.pop_front();
    }
  }
}

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
  _success = true;
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

void GifStreamer::reset()
{
  _index = 0;
}

Image GifStreamer::next_frame()
{
  if (!success() || _index >= _gif->ImageCount) {
    return {};
  }
  if (!_index) {
    for (int i = 0; i < _gif->SWidth * _gif->SHeight; ++i) {
      _pixels[i] = _gif->SBackGroundColor;
    }
  }

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

  _index = (_index + 1);
  std::cout << ";";
  return {static_cast<std::uint32_t>(_gif->SWidth), static_cast<std::uint32_t>(_gif->SHeight),
          (unsigned char*) _pixels.get()};
}

WebmStreamer::WebmStreamer(const std::string& path) : _path{path}, _codec{}
{
  if (_reader.Open(path.c_str())) {
    std::cerr << "couldn't open " << path << std::endl;
    return;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebmlHeader;
  ebmlHeader.Parse(&_reader, pos);

  mkvparser::Segment* segment_tmp;
  if (mkvparser::Segment::CreateInstance(&_reader, pos, segment_tmp)) {
    std::cerr << "couldn't load " << path << ": segment create failed" << std::endl;
    return;
  }

  _segment.reset(segment_tmp);
  if (_segment->Load() < 0) {
    std::cerr << "couldn't load " << path << ": segment load failed" << std::endl;
    return;
  }

  bool vp9 = false;
  for (unsigned long i = 0; i < _segment->GetTracks()->GetTracksCount(); ++i) {
    const auto& track = _segment->GetTracks()->GetTrackByIndex(i);
    if (track && track->GetType() == mkvparser::Track::kVideo) {
      std::string codec{track->GetCodecId()};
      if (codec.find("VP8") != std::string::npos) {
        _video_track = (const mkvparser::VideoTrack*) track;
        break;
      }
      if (codec.find("VP9") != std::string::npos) {
        vp9 = true;
        _video_track = (const mkvparser::VideoTrack*) track;
        break;
      }
    }
  }

  if (!_video_track) {
    std::cerr << "couldn't load " << path << ": no video track found" << std::endl;
    return;
  }

  if (vpx_codec_dec_init(&_codec, vp9 ? vpx_codec_vp9_dx() : vpx_codec_vp8_dx(), nullptr, 0)) {
    codec_error("initialising codec");
    return;
  }

  _success = true;
  return;
}

WebmStreamer::~WebmStreamer()
{
  if (_success && vpx_codec_destroy(&_codec)) {
    codec_error("destroying codec");
  }
}

bool WebmStreamer::success() const
{
  return _success;
}

void WebmStreamer::reset()
{
  _cluster = nullptr;
  _cluster_eos = false;
}

Image WebmStreamer::next_frame()
{
  if (!_success) {
    return {};
  }
  if (!_cluster_eos && !_cluster) {
    _cluster = _segment->GetFirst();
  }

  bool block_eos = false;
  while (true) {
    if (_cluster_eos || _cluster->EOS()) {
      return {};
    }

    if (!block_eos && !_block) {
      if (_cluster->GetFirst(_block) < 0) {
        std::cerr << "couldn't load " << _path << ": couldn't parse first block of cluster"
                  << std::endl;
        _success = false;
        return {};
      }
      _block_index = -1;
    }
    if (block_eos || _block->EOS()) {
      block_eos = false;
      _cluster = _segment->GetNext(_cluster);
      if (!_cluster) {
        _cluster_eos = true;
      }
      _block = nullptr;
      continue;
    }

    if (_block_index < 0) {
      _block_index = 0;
      _iterating = false;
      _it = nullptr;
    }
    if (_block->GetBlock()->GetTrackNumber() != _video_track->GetNumber() ||
        _block_index >= _block->GetBlock()->GetFrameCount()) {
      if (_cluster->GetNext(_block, _block) < 0) {
        std::cerr << "couldn't load " << _path << ": couldn't parse next block of cluster"
                  << std::endl;
        _success = false;
        return {};
      }
      if (!_block) {
        block_eos = true;
      }
      _block_index = -1;
      continue;
    }

    if (!_iterating) {
      auto& frame = _block->GetBlock()->GetFrame(_block_index);
      _data.reset(new uint8_t[frame.len]);
      _reader.Read(frame.pos, frame.len, _data.get());

      if (vpx_codec_decode(&_codec, _data.get(), frame.len, nullptr, 0)) {
        codec_error("decoding frame");
        _success = false;
        return {};
      }
      _iterating = true;
      _image = vpx_codec_get_frame(&_codec, &_it);
    }
    if (!_image) {
      ++_block_index;
      _iterating = false;
      _it = nullptr;
      continue;
    }

    break;
  }

  // Convert I420 (YUV with NxN Y-plane and (N/2)x(N/2) U- and V-planes) to RGB.
  std::unique_ptr<uint32_t[]> pixels(new uint32_t[_image->d_w * _image->d_h]);
  auto w = _image->d_w;
  auto h = _image->d_h;
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      uint8_t Y = _image->planes[VPX_PLANE_Y][x + y * _image->stride[VPX_PLANE_Y]];
      uint8_t U = _image->planes[VPX_PLANE_U][x / 2 + (y / 2) * _image->stride[VPX_PLANE_U]];
      uint8_t V = _image->planes[VPX_PLANE_V][x / 2 + (y / 2) * _image->stride[VPX_PLANE_V]];

      auto cl = [](float f) { return (uint8_t) std::max(0, std::min(255, (int) f)); };
      auto R = cl(1.164f * (Y - 16.f) + 1.596f * (V - 128.f));
      auto G = cl(1.164f * (Y - 16.f) - 0.391f * (U - 128.f) - 0.813f * (V - 128.f));
      auto B = cl(1.164f * (Y - 16.f) + 2.017f * (U - 128.f));
      pixels[x + y * w] = R | (G << 8) | (B << 16) | (0xff << 24);
    }
  }

  _image = vpx_codec_get_frame(&_codec, &_it);
  std::cout << ";";
  return {w, h, (uint8_t*) pixels.get()};
}

void WebmStreamer::codec_error(const std::string& error)
{
  auto detail = vpx_codec_error_detail(&_codec);
  std::cerr << "couldn't load " << _path << ": " << error << ": " << vpx_codec_error(&_codec);
  if (detail) {
    std::cerr << ": " << detail;
  }
  std::cerr << std::endl;
};

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