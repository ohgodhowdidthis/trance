#include "theme.h"
#include "director.h"
#include "util.h"
#include <iostream>
#include <trance.pb.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <gif_lib.h>
#include <jpgd.h>
#include <mkvreader.hpp>
#include <mkvparser.hpp>

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

std::vector<GLuint> Image::textures_to_delete;
std::mutex Image::textures_to_delete_mutex;

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

Theme::Theme(const trance_pb::Theme& proto)
: _target_load{0}
, _last_id{0}
, _last_text_id{0}
, _animation_id{0}
{
  for (const auto& p : proto.image_path()) {
    _paths.emplace_back(p);
  }
  for (const auto& p : proto.animation_path()) {
    _animation_paths.emplace_back(p);
  }

  // Split strings into two lines at the space closest to the middle. This is
  // sort of ad-hoc. There should probably be a better way that can judge length
  // and split over more than two lines.
  for (const auto& t : proto.text_line()) {
    auto mid = t.length() / 2;
    auto l = mid;
    auto r = mid;
    bool found = false;
    while (true) {
      if (t[r] == ' ') {
        found = true;
        _texts.push_back(t.substr(0, r) + '\n' + t.substr(r + 1));
        break;
      }

      if (t[l] == ' ') {
        found = true;
        _texts.push_back(t.substr(0, l) + '\n' + t.substr(l + 1));
        break;
      }

      if (l == 0 || r == t.length() - 1) {
        break;
      }
      --l;
      ++r;
    }

    if (!found) {
      _texts.push_back(t);
    }
  }
}

Theme::Theme(const Theme& theme)
: _paths(theme._paths)
, _texts(theme._texts)
, _target_load(theme._target_load)
, _last_id(theme._last_id)
, _last_text_id(theme._last_text_id)
, _animation_id(theme._animation_id)
, _animation_paths(theme._animation_paths)
{
  for (const auto& t : theme._images) {
    _paths.emplace_back(t.path);
  }
}

Image Theme::get() const
{
  // Lock the mutex so we don't interfere with the thread calling
  // ThemeBank::async_update().
  _mutex.lock();
  if (_images.empty()) {
    _mutex.unlock();
    return get_animation(random(2 << 16));
  }
  _last_id = random_excluding(_images.size(), _last_id);
  return get_internal(_images, _last_id, _mutex);
}

Image Theme::get_animation(std::size_t frame) const
{
  _animation_mutex.lock();
  if (_animation_images.empty()) {
    _animation_mutex.unlock();
    return Image("");
  }
  auto len = _animation_images.size();
  auto f = frame % (2 * len - 2);
  f = f < len ? f : 2 * len - 2 - f;
  return get_internal(_animation_images, f, _animation_mutex);
}

const std::string& Theme::get_text() const
{
  static const std::string empty;
  if (_texts.empty()) {
    return empty;
  }
  return _texts[
      _last_text_id = random_excluding(_texts.size(), _last_text_id)];
}

void Theme::set_target_load(std::size_t target_load)
{
  _target_load = target_load;
}

void Theme::perform_swap()
{
  if (_animation_paths.size() > 2 && random_chance(4)) {
    load_animation_internal();
    return;
  }
  // Swap if there's definitely an image loaded beyond the one currently
  // displayed.
  if (_images.size() > 2 && !_paths.empty()) {
    unload_internal();
    load_internal();
  }
}

void Theme::perform_load()
{
  if (!_animation_paths.empty()) {
    if (_target_load && _animation_images.empty()) {
      load_animation_internal();
    }
    else if (!_target_load && !_animation_images.empty()) {
      unload_animation_internal();
    }
  }

  if (_images.size() < _target_load && !_paths.empty()) {
    load_internal();
  }
  else if (_images.size() > _target_load) {
    unload_internal();
  }
}

void Theme::perform_all_loads()
{
  while (!all_loaded()) {
    perform_load();
  }
}

bool Theme::all_loaded() const
{
  return (_images.size() == _target_load || _paths.empty()) &&
      (_animation_images.empty() == !_target_load || _animation_paths.empty());
}

std::size_t Theme::loaded() const
{
  return _images.size();
}

bool Theme::load_animation_gif_internal(std::vector<Image>& images,
                                        const std::string& path) const
{
  int error_code = 0;
  GifFileType* gif = DGifOpenFileName(path.c_str(), &error_code);
  if (!gif) {
    std::cerr << "couldn't load " << path <<
        ": " << GifErrorString(error_code) << std::endl;
    return false;
  }
  if (DGifSlurp(gif) != GIF_OK) {
    std::cerr << "couldn't slurp " << path <<
        ": " << GifErrorString(gif->Error) << std::endl;
    if (DGifCloseFile(gif, &error_code) != GIF_OK) {
      std::cerr << "couldn't close " << path <<
          ": " << GifErrorString(error_code) << std::endl;
    }
    return false;
  }

  auto width = gif->SWidth;
  auto height = gif->SHeight;
  std::unique_ptr<uint32_t[]> pixels(new uint32_t[width * height]);
  for (int i = 0; i < width * height; ++i) {
    pixels[i] = gif->SBackGroundColor;
  }

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

    images.emplace_back(path);
    auto& image = images.back();
    image.sf_image.reset(new sf::Image);
    image.deleter.reset(
        new Image::texture_deleter(image.texture));
    image.width = width;
    image.height = height;
    image.sf_image->create(width, height, (unsigned char*) pixels.get());
    std::cout << ";";
  }

  if (DGifCloseFile(gif, &error_code) != GIF_OK) {
    std::cerr << "couldn't close " << path <<
        ": " << GifErrorString(error_code) << std::endl;
  }
  return true;
}

bool Theme::load_animation_webm_internal(std::vector<Image>& images,
                                         const std::string& path) const
{
  mkvparser::MkvReader reader;
  if (reader.Open(path.c_str())) {
    std::cerr << "couldn't open " << path << std::endl;
    return false;
  }

  long long pos = 0;
  mkvparser::EBMLHeader ebmlHeader;
  ebmlHeader.Parse(&reader, pos);

  mkvparser::Segment* segment_tmp;
  if (mkvparser::Segment::CreateInstance(&reader, pos, segment_tmp)) {
    std::cerr << "couldn't load " << path <<
        ": segment create failed" << std::endl;
    return false;
  }

  std::unique_ptr<mkvparser::Segment> segment(segment_tmp);
  if (segment->Load() < 0) {
    std::cerr << "couldn't load " << path <<
        ": segment load failed" << std::endl;
    return false;
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
    return false;
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
    return false;
  }

  for (auto cluster = segment->GetFirst(); cluster && !cluster->EOS();
       cluster = segment->GetNext(cluster)) {
    const mkvparser::BlockEntry* block;
    if (cluster->GetFirst(block) < 0) {
      std::cerr << "couldn't load " << path <<
          ": couldn't parse first block of cluster" << std::endl;
      return false;
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
            return false;
          }

          vpx_codec_iter_t it = nullptr;
          while (auto img = vpx_codec_get_frame(&codec, &it)) {
            // Convert I420 (YUV with NxN Y-plane and (N/2)x(N/2) U- and V-
            // planes) to RGB.
            std::unique_ptr<uint32_t[]> data(
                new uint32_t[32 * img->d_w * img->d_h]);
            auto w = img->d_w;
            auto h = img->d_h;
            for (unsigned int y = 0; y < h; ++y) {
              for (unsigned int x = 0; x < w; ++x) {
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

            images.emplace_back(path);
            auto& image = images.back();
            image.sf_image.reset(new sf::Image);
            image.deleter.reset(
                new Image::texture_deleter(image.texture));
            image.width = w;
            image.height = h;
            image.sf_image->create(w, h, (unsigned char*) data.get());
            std::cout << ";";
          }
        }
      }

      if (cluster->GetNext(block, block) < 0) {
        std::cerr << "couldn't load " << path <<
            ": couldn't parse next block of cluster" << std::endl;
        return false;
      }
    }
  }

  if (vpx_codec_destroy(&codec)) {
    codec_error("destroying codec");
    return false;
  }

  return true;
}

bool Theme::load_internal(Image* image, const std::string& path) const
{
  image->sf_image.reset(new sf::Image);
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
      return false;
    }

    image->width = width;
    image->height = height;
    image->sf_image->create(width, height, data);

    free(data);
    std::cout << ".";
    return true;
  }

  if (!image->sf_image->loadFromFile(path)) {
    std::cerr << "\ncouldn't load " << path << std::endl;
    return false;
  }

  image->width = image->sf_image->getSize().x;
  image->height = image->sf_image->getSize().y;

  std::cout << ".";
  return true;
}

void Theme::load_animation_internal()
{
  auto id = random_excluding(_animation_paths.size(), _animation_id);
  std::vector<Image> images;
  const auto& path = _animation_paths[id];
  bool loaded = path.substr(path.length() - 4) == ".gif" ?
      load_animation_gif_internal(images, _animation_paths[id]) :
      load_animation_webm_internal(images, _animation_paths[id]);
  if (!loaded) {
    _animation_paths.erase(_animation_paths.begin() + id);
    if (_animation_id >= id) {
      --_animation_id;
    }
    return;
  }
  _animation_id = id;

  _animation_mutex.lock();
  std::swap(images, _animation_images);
  _animation_mutex.unlock();
  images.clear();
}

void Theme::unload_animation_internal()
{
  _animation_mutex.lock();
  _animation_images.clear();
  _animation_mutex.unlock();
}

void Theme::load_internal()
{
  // Take a random image path from the vector of paths, remove it and load it
  // into the Images vector instead.
  auto index = random(_paths.size());
  auto path = _paths[index];
  Image image(path);
  bool loaded = load_internal(&image, path);

  std::swap(_paths[index], _paths[_paths.size() - 1]);
  _paths.erase(--_paths.end());
  _mutex.lock();
  // If it didn't load successfully, the path will be discarded.
  if (loaded) {
    _images.emplace_back(image);
  }
  _mutex.unlock();
}

void Theme::unload_internal()
{
  // Opposite of load_internal(): pick a loaded image at random, unload it,
  // and add its path back to the pool of unloaded paths.
  _mutex.lock();
  auto index = random_excluding(_images.size(), _last_id);
  auto path = _images[index].path;
  std::swap(_images[index], _images[_images.size() - 1]);
  _paths.emplace_back(path);
  _images.erase(--_images.end());
  _mutex.unlock();
}

Image Theme::get_internal(const std::vector<Image>& list, std::size_t index,
                          std::mutex& unlock) const
{
  // Use a temporary object rather than reference into the vector so the mutex
  // can be unlocked earlier.
  Image image = list[index];
  if (image.texture) {
    // If the image has already been loaded into video memory, nothing needs
    // to be done.
    unlock.unlock();
  }
  else {
    // Upload the texture to video memory and set its texture_deleter so that
    // it's cleaned up when there are no more Image objects referencing it.
    glGenTextures(1, &image.texture);
    image.deleter.reset(
        new Image::texture_deleter(image.texture));

    list[index].deleter = image.deleter;
    list[index].texture = image.texture;
    unlock.unlock();

    glBindTexture(GL_TEXTURE_2D, image.texture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, image.sf_image->getPixelsPtr());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    std::cout << ":";
  }

  return image;
}

ThemeBank::ThemeBank(const std::vector<trance_pb::Theme>& themes,
                     unsigned int image_cache_size)
: _image_cache_size{image_cache_size}
, _updates{0}
, _cooldown{switch_cooldown}
{
  for (const auto& theme : themes) {
    _themes.emplace_back(theme);
  }
  if (themes.empty()) {
    _themes.push_back(Theme({}));
  }

  if (_themes.size() == 1) {
    // Always have at least two themes.
    Theme copy = _themes.back();
    _themes.emplace_back(copy);
  }
  if (_themes.size() == 2) {
    // Two active themes and switching just swaps them.
    _a = 0;
    _b = 1;
    _themes[0].set_target_load(_image_cache_size / 2);
    _themes[1].set_target_load(_image_cache_size / 2);
    _themes[0].perform_all_loads();
    _themes[1].perform_all_loads();
    return;
  }

  // For three themes, we keep every theme loaded at all times but swap the two
  // active ones.
  //
  // Four four or more themes, we have:
  // - 2 active themes (_a, _b)
  // - 1 loading in (_next)
  // - 1 being unloaded (_prev)
  // - some others
  _a = random(_themes.size());
  _b = random_excluding(_themes.size(), _a);
  do {
    _next = random_excluding(_themes.size(), _a);
  }
  while (_next == _b);

  _themes[_a].set_target_load(_image_cache_size / 3);
  _themes[_b].set_target_load(_image_cache_size / 3);
  _themes[_next].set_target_load(_image_cache_size / 3);
  _themes[_a].perform_all_loads();
  _themes[_b].perform_all_loads();

  if (_themes.size() == 3) {
    _themes[_next].perform_all_loads();
  }
  else {
    // _prev just needs to be some unused index.
    _prev = 0;
    while (_prev == _a || _prev == _b || _prev == _next) {
      ++_prev;
    }
  }
}

Image ThemeBank::get(bool alternate) const
{
  return alternate ? _themes[_a].get() : _themes[_b].get();
}

const std::string& ThemeBank::get_text(bool alternate) const
{
  return alternate ? _themes[_a].get_text() : _themes[_b].get_text();
}

Image ThemeBank::get_animation(std::size_t frame, bool alternate) const
{
  return alternate ?
      _themes[_a].get_animation(frame) : _themes[_b].get_animation(frame);
}

void ThemeBank::maybe_upload_next()
{
  if (_themes.size() > 3 && _themes[_next].loaded() > 0) {
    _themes[_next].get();
  }
}

bool ThemeBank::change_themes()
{
  _cooldown = switch_cooldown;
  if (_themes.size() < 3) {
    // Only indexes need to be swapped.
    std::swap(_a, _b);
    return true;
  }
  if (_themes.size() == 3) {
    // Indexes need to be cycled.
    std::size_t t = _a;
    _a = _b;
    _b = _next;
    _next = _a;
    return true;
  }

  // For four or more themes, we need to wait until the next one has loaded in
  // sufficiently.
  if (!_themes[_prev].all_loaded() || !_themes[_next].all_loaded()) {
    return false;
  }

  _prev = _a;
  _a = _b;
  _b = _next;
  do {
    _next = random_excluding(_themes.size(), _prev);
  }
  while (_next == _a || _next == _b);

  // Update target loads.
  _themes[_prev].set_target_load(0);
  _themes[_next].set_target_load(_image_cache_size / 3);
  return true;
}

void ThemeBank::async_update()
{
  if (_cooldown) {
    --_cooldown;
    return;
  }

  ++_updates;
  // Swap some images from the active themes in and out every so often.
  if (_updates > 128) {
    _themes[_a].perform_swap();
    _themes[_b].perform_swap();
    _updates = 0;
  }
  if (_themes.size() == 3) {
    _themes[_next].perform_swap();
  }
  else if (_themes.size() >= 4) {
    _themes[_prev].perform_load();
    _themes[_next].perform_load();
  }
}