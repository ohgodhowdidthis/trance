#include "image.h"
#include <iostream>
#include <gif_lib.h>
#include <jpgd.h>
#include <mkvreader.hpp>
#include <mkvparser.hpp>
#include <SFML/OpenGL.hpp>

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

namespace {

std::vector<Image> load_animation_gif(const std::string& path)
{
  int error_code = 0;
  GifFileType* gif = DGifOpenFileName(path.c_str(), &error_code);
  if (!gif) {
    std::cerr << "couldn't load " << path <<
        ": " << GifErrorString(error_code) << std::endl;
    return {};
  }
  if (DGifSlurp(gif) != GIF_OK) {
    std::cerr << "couldn't slurp " << path <<
        ": " << GifErrorString(gif->Error) << std::endl;
    if (DGifCloseFile(gif, &error_code) != GIF_OK) {
      std::cerr << "couldn't close " << path <<
          ": " << GifErrorString(error_code) << std::endl;
    }
    return {};
  }

  auto width = gif->SWidth;
  auto height = gif->SHeight;
  std::unique_ptr<uint32_t[]> pixels(new uint32_t[width * height]);
  for (int i = 0; i < width * height; ++i) {
    pixels[i] = gif->SBackGroundColor;
  }

  std::vector<Image> result;
  for (int i = 0; i < gif->ImageCount; ++i) {
    const auto& frame = gif->SavedImages[i];
    bool transparency = false;
    unsigned char transparency_byte = 0;
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
        for (int k = 0; k < width * height; ++k) {
          pixels[k] = gif->SBackGroundColor;
        }
      }
    }
    auto map = frame.ImageDesc.ColorMap ?
        frame.ImageDesc.ColorMap : gif->SColorMap;

    auto fw = frame.ImageDesc.Width;
    auto fh = frame.ImageDesc.Height;
    auto fl = frame.ImageDesc.Left;
    auto ft = frame.ImageDesc.Top;

    for (int y = 0; y < std::min(height, fh); ++y) {
      for (int x = 0; x < std::min(width, fw); ++x) {
        unsigned char byte = frame.RasterBits[x + y * fw];
        if (transparency && byte == transparency_byte) {
          continue;
        }
        const auto& c = map->Colors[byte];
        // Still get segfaults here sometimes...
        pixels[fl + x + (ft + y) * width] =
            c.Red | (c.Green << 8) | (c.Blue << 16) | (0xff << 24);
      }
    }

    result.emplace_back(path, width, height, (unsigned char*) pixels.get());
    std::cout << ";";
  }

  if (DGifCloseFile(gif, &error_code) != GIF_OK) {
    std::cerr << "couldn't close " << path <<
        ": " << GifErrorString(error_code) << std::endl;
  }
  return result;
}

std::vector<Image> load_animation_webm(const std::string& path)
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
    std::cerr << "couldn't load " << path <<
        ": segment create failed" << std::endl;
    return {};
  }

  std::unique_ptr<mkvparser::Segment> segment(segment_tmp);
  if (segment->Load() < 0) {
    std::cerr << "couldn't load " << path <<
        ": segment load failed" << std::endl;
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
    std::cerr << "couldn't load " << path <<
        ": no VP8 video track found" << std::endl;
    return {};
  }

  vpx_codec_ctx_t codec;
  auto codec_error = [&](const std::string& s) {
    auto detail = vpx_codec_error_detail(&codec);
    std::cerr << "couldn't load " << path <<
        ": " << s << ": " << vpx_codec_error(&codec);
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
      std::cerr << "couldn't load " << path <<
          ": couldn't parse first block of cluster" << std::endl;
      return {};
    }

    while (block && !block->EOS()) {
      const auto& b = block->GetBlock();
      if (b->GetTrackNumber() == video_track->GetNumber()) {
        for (int i = 0; i < b->GetFrameCount(); ++i) {
          const auto& frame = b->GetFrame(i);
          std::unique_ptr<unsigned char[]> data(new unsigned char[frame.len]);
          reader.Read(frame.pos, frame.len, data.get());

          if (vpx_codec_decode(&codec, data.get(), frame.len, nullptr, 0)) {
            codec_error("decoding frame");
            return {};
          }

          vpx_codec_iter_t it = nullptr;
          while (auto img = vpx_codec_get_frame(&codec, &it)) {
            // Convert I420 (YUV with NxN Y-plane and (N/2)x(N/2) U- and V-
            // planes) to RGB.
            std::unique_ptr<uint32_t[]> data(
                new uint32_t[32 * img->d_w * img->d_h]);
            auto w = img->d_w;
            auto h = img->d_h;
            for (uint32_t y = 0; y < h; ++y) {
              for (uint32_t x = 0; x < w; ++x) {
                int Y = img->planes[0][x + y * img->stride[0]];
                int U = img->planes[1][x / 2 + (y / 2) * img->stride[1]];
                int V = img->planes[2][x / 2 + (y / 2) * img->stride[2]];

                auto cl = [](float f) {
                  return (unsigned char) std::max(0, std::min(255, (int) f));
                };
                auto R = cl(1.164f * (Y - 16.f) + 1.596f * (V - 128.f));
                auto G = cl(1.164f * (Y - 16.f) -
                    0.813f * (V - 128.f) - 0.391f * (U - 128.f));
                auto B = cl(1.164f * (Y - 16.f) + 2.018f * (U - 128.f));
                data[x + y * w] =
                    R | (G << 8) | (B << 16) | (0xff << 24);
              }
            }

            result.emplace_back(path, w, h, (unsigned char*) data.get());
            std::cout << ";";
          }
        }
      }

      if (cluster->GetNext(block, block) < 0) {
        std::cerr << "couldn't load " << path <<
            ": couldn't parse next block of cluster" << std::endl;
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

}

std::vector<GLuint> Image::textures_to_delete;
std::mutex Image::textures_to_delete_mutex;

Image::Image()
: _width{0}
, _height{0}
, _texture{0}
{
}

Image::Image(const std::string& path, uint32_t width, uint32_t height,
             unsigned char* data)
: _path{path}
, _width{width}
, _height{height}
, _texture{0}
, _sf_image{new sf::Image}
{
  _sf_image->create(width, height, data);
}

Image::Image(const std::string& path, const sf::Image& image)
: _path{path}
, _width{image.getSize().x}
, _height{image.getSize().y}
, _texture{0}
, _sf_image{new sf::Image{image}}
{
}

Image::operator bool() const
{
  return _width && _height;
}

uint32_t Image::width() const
{
  return _width;
}

uint32_t Image::height() const
{
  return _height;
}

uint32_t Image::texture() const
{
  return _texture;
}

void Image::ensure_texture_uploaded() const
{
  if (_texture || !(*this)) {
    return;
  }

  // Upload the texture to video memory and set its texture_deleter so that
  // it's cleaned up when there are no more Image objects referencing it.
  glGenTextures(1, &_texture);
  _deleter.reset(new texture_deleter{_texture});

  // Could be split out to a separate call so that Theme doesn't have to hold on
  // to mutex while uploading. This probably doesn't actually block though so no
  // worries.
  glBindTexture(GL_TEXTURE_2D, _texture);
  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, _width, _height,
      0, GL_RGBA, GL_UNSIGNED_BYTE, _sf_image->getPixelsPtr());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // Could unload sf_image now, but don't, to avoid interrupting the rendering
  // thread.
  std::cout << ":";
}

void Image::delete_textures()
{
  textures_to_delete_mutex.lock();
  for (const auto& texture : textures_to_delete) {
    glDeleteTextures(1, &texture);
  }
  textures_to_delete.clear();
  textures_to_delete_mutex.unlock();
}

Image::texture_deleter::~texture_deleter()
{
  textures_to_delete_mutex.lock();
  textures_to_delete.push_back(texture);
  textures_to_delete_mutex.unlock();
}

bool is_gif_animated(const std::string& path)
{
  int error_code = 0;
  GifFileType* gif = DGifOpenFileName(path.c_str(), &error_code);
  if (!gif) {
    std::cerr << "couldn't load " << path <<
        ": " << GifErrorString(error_code) << std::endl;
    return false;
  }
  int frames = 0;
  if (DGifSlurp(gif) != GIF_OK) {
    std::cerr << "couldn't slurp " << path <<
        ": " << GifErrorString(gif->Error) << std::endl;
  }
  else {
    frames = gif->ImageCount;
  }
  if (DGifCloseFile(gif, &error_code) != GIF_OK) {
    std::cerr << "couldn't close " << path <<
        ": " << GifErrorString(error_code) << std::endl;
  }
  return frames > 0;
}

Image load_image(const std::string& path)
{
  std::string lower = path;
  for (auto& c : lower) {
    c = tolower(c);
  }

  // Load JPEGs with the jpgd library since SFML does not support progressive
  // JPEGs.
  if (lower.substr(lower.size() - 4) == ".jpg" ||
      lower.substr(lower.size() - 5) == ".jpeg") {
    int width = 0;
    int height = 0;
    int reqs = 0;
    unsigned char* data = jpgd::decompress_jpeg_image_from_file(
        path.c_str(), &width, &height, &reqs, 4);
    if (!data) {
      std::cerr << "\ncouldn't load " << path << std::endl;
      return {};
    }

    Image image{path, uint32_t(width), uint32_t(height), data};
    free(data);
    std::cout << ".";
    return image;
  }

  sf::Image sf_image;
  if (!sf_image.loadFromFile(path)) {
    std::cerr << "\ncouldn't load " << path << std::endl;
    return {};
  }

  Image image{path, sf_image};
  std::cout << ".";
  return image;
}

std::vector<Image> load_animation(const std::string& path)
{
  std::string lower = path;
  for (auto& c : lower) {
    c = tolower(c);
  }
  if (lower.substr(lower.size() - 4) == ".gif") {
    return load_animation_gif(path);
  }
  if (lower.substr(lower.size() - 5) == ".webm") {
    return load_animation_webm(path);
  }
  return {};
}