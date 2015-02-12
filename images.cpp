#include "images.h"
#include "director.h"
#include "util.h"
#include <algorithm>
#include <iostream>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <gif_lib.h>
#include <jpgd.h>

namespace {
  std::size_t image_cache_size()
  {
    return std::max(6u, Settings::settings.image_cache_size);
  }
}

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

ImageSet::ImageSet(const std::vector<std::string>& images,
                   const std::vector<std::string>& texts,
                   const std::vector<std::string>& animations)
: _paths{images}
, _animation_paths{animations}
, _target_load{0}
, _last_id{0}
, _last_text_id{0}
, _animation_id{0}
{
  // Split strings into two lines at the space closest to the middle. This is
  // sort of ad-hoc. There should probably be a better way that can judge length
  // and split over more than two lines.
  for (const auto& t : texts) {
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

ImageSet::ImageSet(const ImageSet& images)
: _paths(images._paths)
, _animation_paths(images._animation_paths)
, _texts(images._texts)
, _target_load(images._target_load)
, _last_id(images._last_id)
, _animation_id(images._animation_id)
{
  for (const auto& t : images._images) {
    _paths.emplace_back(t.path);
  }
}

Image ImageSet::get() const
{
  // Lock the mutex so we don't interfere with the thread calling
  // ImageBank::async_update().
  _mutex.lock();
  _last_id = random_excluding(_images.size(), _last_id);
  return get_internal(_images, _last_id, _mutex);
}

Image ImageSet::get_animation(std::size_t frame) const
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

const std::string& ImageSet::get_text() const
{
  static const std::string empty;
  if (_texts.empty()) {
    return empty;
  }
  return _texts[
      _last_text_id = random_excluding(_texts.size(), _last_text_id)];
}

void ImageSet::set_target_load(std::size_t target_load)
{
  _target_load = target_load;
}

void ImageSet::perform_swap()
{
  if (_animation_paths.size() > 2 && random_chance(8)) {
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

void ImageSet::perform_load()
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

void ImageSet::perform_all_loads()
{
  while (!all_loaded()) {
    perform_load();
  }
}

bool ImageSet::all_loaded() const
{
  return (_images.size() == _target_load || _paths.empty()) &&
      (_animation_images.empty() == !_target_load || _animation_paths.empty());
}

std::size_t ImageSet::loaded() const
{
  return _images.size();
}

bool ImageSet::load_animation_internal(std::vector<Image>& images,
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

    for (int y = 0; y < fh; ++y) {
      for (int x = 0; x < fw; ++x) {
        unsigned char byte = frame.RasterBits[x + y * fw];
        if (transparency && byte == transparency_byte) {
          continue;
        }
        const auto& c = map->Colors[byte];
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

bool ImageSet::load_internal(Image* image, const std::string& path) const
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

void ImageSet::load_animation_internal()
{
  auto id = random_excluding(_animation_paths.size(), _animation_id);
  std::vector<Image> images;
  bool loaded = load_animation_internal(images, _animation_paths[id]);
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

void ImageSet::unload_animation_internal()
{
  _animation_mutex.lock();
  _animation_images.clear();
  _animation_mutex.unlock();
}

void ImageSet::load_internal()
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

void ImageSet::unload_internal()
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

Image ImageSet::get_internal(const std::vector<Image>& list, std::size_t index,
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

void ImageBank::add_set(const std::vector<std::string>& images,
                        const std::vector<std::string>& texts,
                        const std::vector<std::string>& animations)
{
  _sets.emplace_back(images, texts, animations);
}

void ImageBank::initialise()
{
  _updates = 0;
  _cooldown = switch_cooldown;
  if (_sets.size() == 0) {
    // Nothing to do.
    return;
  }
  if (_sets.size() == 1) {
    // Only have one active set and no switching.
    _a = 0;
    _b = 0;
    _sets[0].set_target_load(image_cache_size());
    _sets[0].perform_all_loads();
    return;
  }
  if (_sets.size() == 2) {
    // Two active sets and switching just swaps them.
    _a = 0;
    _b = 1;
    _sets[0].set_target_load(image_cache_size() / 2);
    _sets[1].set_target_load(image_cache_size() / 2);
    _sets[0].perform_all_loads();
    _sets[1].perform_all_loads();
    return;
  }

  // For three sets, we keep every set loaded at all times but swap the two
  // active ones.
  //
  // Four four or more sets, we have:
  // - 2 active sets (_a, _b)
  // - 1 loading in (_next)
  // - 1 being unloaded (_prev)
  // - some others
  _a = random(_sets.size());
  _b = random_excluding(_sets.size(), _a);
  do {
    _next = random_excluding(_sets.size(), _a);
  }
  while (_next == _b);

  _sets[_a].set_target_load(image_cache_size() / 3);
  _sets[_b].set_target_load(image_cache_size() / 3);
  _sets[_next].set_target_load(image_cache_size() / 3);
  _sets[_a].perform_all_loads();
  _sets[_b].perform_all_loads();

  if (_sets.size() == 3) {
    _sets[_next].perform_all_loads();
  }
  else {
    // _prev just needs to be some unused index.
    _prev = 0;
    while (_prev == _a || _prev == _b || _prev == _next) {
      ++_prev;
    }
  }
}

Image ImageBank::get(bool alternate) const
{
  return alternate ? _sets[_a].get() : _sets[_b].get();
}

const std::string& ImageBank::get_text(bool alternate) const
{
  return alternate ? _sets[_a].get_text() : _sets[_b].get_text();
}

Image ImageBank::get_animation(std::size_t frame, bool alternate) const
{
  return alternate ?
      _sets[_a].get_animation(frame) : _sets[_b].get_animation(frame);
}

void ImageBank::maybe_upload_next()
{
  if (_sets.size() > 3 && _sets[_next].loaded() > 0) {
    _sets[_next].get();
  }
}

bool ImageBank::change_sets()
{
  _cooldown = switch_cooldown;
  if (_sets.size() < 3) {
    // Only indexes need to be swapped.
    std::swap(_a, _b);
    return true;
  }
  if (_sets.size() == 3) {
    // Indexes need to be cycled.
    std::size_t t = _a;
    _a = _b;
    _b = _next;
    _next = _a;
    return true;
  }

  // For four or more sets, we need to wait until the next one has loaded in
  // sufficiently.
  if (!_sets[_prev].all_loaded() || !_sets[_next].all_loaded()) {
    return false;
  }

  _prev = _a;
  _a = _b;
  _b = _next;
  do {
    _next = random_excluding(_sets.size(), _prev);
  }
  while (_next == _a || _next == _b);

  // Update target loads.
  _sets[_prev].set_target_load(0);
  _sets[_next].set_target_load(image_cache_size() / 3);
  return true;
}

void ImageBank::async_update()
{
  if (_cooldown) {
    --_cooldown;
    return;
  }

  ++_updates;
  // Swap some images from the active sets in and out every so often.
  if (_updates > 128) {
    _sets[_a].perform_swap();
    _sets[_b].perform_swap();
    _updates = 0;
  }
  if (_sets.size() == 3) {
    _sets[_next].perform_swap();
  }
  else if (_sets.size() >= 4) {
    _sets[_prev].perform_load();
    _sets[_next].perform_load();
  }
}