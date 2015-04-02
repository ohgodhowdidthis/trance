#include "theme.h"
#include "director.h"
#include "util.h"
#include <iostream>
#include <trance.pb.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

Theme::Theme()
: Theme{trance_pb::Theme{}}
{
}

Theme::Theme(const trance_pb::Theme& proto)
: _image_paths{proto.image_path()}
, _animation_paths{proto.animation_path()}
, _font_paths{proto.font_path()}
, _text_lines{proto.text_line()}
, _target_load{0}
{
}

// TODO: get rid of this somehow. Copying needs mutexes, technically.
Theme::Theme(const Theme& theme)
: _image_paths{theme._image_paths}
, _animation_paths{theme._animation_paths}
, _font_paths{theme._font_paths}
, _text_lines{theme._text_lines}
, _target_load(theme._target_load)
{
}

Image Theme::get_image() const
{
  // Lock the mutex so we don't interfere with the thread calling
  // ThemeBank::async_update().
  _image_mutex.lock();
  if (_images.empty()) {
    _image_mutex.unlock();
    return get_animation(random(2 << 16));
  }
  std::size_t index = _image_paths.next_index(false);
  auto it = _images.find(index);
  it->second.ensure_texture_uploaded();
  Image image = it->second;
  _image_mutex.unlock();
  return image;
}

Image Theme::get_animation(std::size_t frame) const
{
  _animation_mutex.lock();
  if (_animation_images.empty()) {
    _animation_mutex.unlock();
    return {};
  }
  auto len = _animation_images.size();
  auto f = frame % (2 * len - 2);
  f = f < len ? f : 2 * len - 2 - f;
  _animation_images[f].ensure_texture_uploaded();
  Image image = _animation_images[f];
  _animation_mutex.unlock();
  return image;
}

const std::string& Theme::get_text() const
{
  return _text_lines.next();
}

const std::string& Theme::get_font() const
{
  return _font_paths.next();
}

void Theme::set_target_load(std::size_t target_load)
{
  _target_load = target_load;
}

void Theme::perform_swap()
{
  if (_animation_paths.enabled_count() >= 2 && random_chance(4)) {
    load_animation_internal();
    return;
  }
  // Swap if there's definitely an image to load.
  if (_image_paths.enabled_count()) {
    unload_image_internal();
    load_image_internal();
  }
}

void Theme::perform_load()
{
  if (_animation_paths.enabled_count()) {
    if (_target_load && _animation_images.empty()) {
      load_animation_internal();
    }
    else if (!_target_load && !_animation_images.empty()) {
      unload_animation_internal();
    }
  }

  if (_images.size() < _target_load && _image_paths.enabled_count()) {
    load_image_internal();
  }
  else if (_images.size() > _target_load) {
    unload_image_internal();
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
  return (_images.size() == _target_load || !_image_paths.enabled_count()) &&
      (_animation_images.empty() == !_target_load ||
       !_animation_paths.enabled_count());
}

std::size_t Theme::loaded() const
{
  return _images.size();
}

void Theme::load_image_internal()
{
  // Take a random still-enabled image, disable it and load the image.
  _image_mutex.lock();
  std::size_t index = _image_paths.next_index(true);
  _image_mutex.unlock();

  auto path = _image_paths.get(index);
  Image image = load_image(path);
  _image_mutex.lock();
  _image_paths.set_enabled(index, false);
  _images.emplace(index, image ? image : Image{});
  _image_mutex.unlock();
}

void Theme::unload_image_internal()
{
  // Opposite of load_internal(): pick a disabled image at random, unload it,
  // and re-enable it.
  _image_mutex.lock();
  std::size_t index = _image_paths.next_index(false);
  _images.erase(index);
  _image_paths.set_enabled(index, true);
  _image_mutex.unlock();
}

void Theme::load_animation_internal()
{
  auto index = _animation_paths.next_index();
  std::vector<Image> images = load_animation(_animation_paths.get(index));
  if (images.empty()) {
    // Don't try to load again.
    _animation_paths.set_enabled(index, false);
    return;
  }

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

ThemeBank::ThemeBank(const trance_pb::Session& session)
: _theme_map{session.theme_map().begin(), session.theme_map().end()}
, _themes{[&]{
  std::vector<Theme> result;
  // Need to store copies - proto map entries seem to be unstable.
  for (const auto& pair : _theme_map) {
    result.emplace_back(pair.second);
  }
  if (result.empty()) {
    result.push_back(Theme({}));
  }
  // Always have at least two themes.
  if (result.size() == 1) {
    result.emplace_back(result.back());
  }
  return result;
}()}
, _theme_shuffler{_themes}
, _prev{&_theme_shuffler.next()}
, _main{&_theme_shuffler.next()}
, _alt{&_theme_shuffler.next()}
, _next{&_theme_shuffler.next()}
, _image_cache_size{session.system().image_cache_size()}
, _updates{0}
, _cooldown{switch_cooldown}
{
  uint32_t cache_per_theme = _themes.size() == 2 ?
      _image_cache_size / 2 : _image_cache_size / 3;
  _main->set_target_load(cache_per_theme);
  _alt->set_target_load(cache_per_theme);
  _next->set_target_load(cache_per_theme);

  _main->perform_all_loads();
  _alt->perform_all_loads();
  if (_themes.size() == 3) {
    _next->perform_all_loads();
  }
}

const Theme& ThemeBank::get(bool alternate) const
{
  return alternate ? *_alt : *_main;
}

void ThemeBank::maybe_upload_next()
{
  if (_next->loaded() > 0) {
    _next->get_image();
  }
}

bool ThemeBank::change_themes()
{
  if (!_prev->all_loaded() || !_next->all_loaded()) {
    return false;
  }
  _cooldown = switch_cooldown;
  _prev = _main;
  _main = _alt;
  _alt = _next;
  _next = &_theme_shuffler.next();

  // Update target loads.
  if (_prev != _next && _prev != _alt && _prev != _main) {
    _prev->set_target_load(0);
  }
  uint32_t cache_per_theme = _themes.size() == 2 ?
      _image_cache_size / 2 : _image_cache_size / 3;
  _next->set_target_load(cache_per_theme);
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
  if (_updates > 64) {
    _main->perform_swap();
    _alt->perform_swap();
    if (_themes.size() == 3) {
      _next->perform_swap();
    }
    _updates = 0;
  }
  else {
    _prev->perform_load();
    _next->perform_load();
  }
}