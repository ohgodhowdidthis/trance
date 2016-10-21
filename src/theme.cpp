#include "theme.h"
#include "director.h"
#include "util.h"
#include <iostream>

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#pragma warning(pop)

ThemeBank::ThemeBank(
    const std::string& root_path, const trance_pb::Session& session,
    const trance_pb::System& system, const trance_pb::Program& program)
: _root_path{root_path}
, _animation_index{0}
, _image_cache_size{system.image_cache_size()}
, _swaps_to_match_theme{0}
, _updates{0}
, _cooldown{switch_cooldown}
{
  // Find all images in all themese and set up data for each.
  std::unordered_set<std::string> all_image_paths;
  std::unordered_set<std::string> all_animation_paths;
  for (const auto& pair : session.theme_map()) {
    const auto& theme = pair.second;
    all_image_paths.insert(theme.image_path().begin(),
                           theme.image_path().end());
    all_animation_paths.insert(theme.animation_path().begin(),
                               theme.animation_path().end());
  }
  for (const auto& path : all_image_paths) {
    _all_images.push_back({path, 0, {}});
  }
  _all_animations.insert(_all_animations.begin(),
                         all_animation_paths.begin(),
                         all_animation_paths.end());

  // Set up data for each theme.
  for (const auto& pair : session.theme_map()) {
    // Index lookup.
    _theme_map[pair.first] = _themes.size();
    const auto& theme = pair.second;

    std::unordered_set<std::string> images{theme.image_path().begin(),
                                           theme.image_path().end()};
    std::unordered_set<std::string> animations{theme.animation_path().begin(),
                                               theme.animation_path().end()};
    _themes.emplace_back(new ThemeInfo{
        images.size(), false, {}, 0, {},
        {_all_images.size()}, {_all_images.size()}, {_all_animations.size()},
        {theme.font_path().begin(), theme.font_path().end()},
        {theme.text_line().begin(), theme.text_line().end()}, {},
        {static_cast<std::size_t>(theme.text_line().size())}});

    // Disable images not in this theme in both shufflers so that they can
    // never be chosen.
    for (std::size_t i = 0; i < _all_images.size(); ++i) {
      if (images.count(_all_images[i].path)) {
        _themes.back()->load_shuffler.modify(i, last_image_count);
        _themes.back()->image_shuffler.modify(i, last_image_count);
      }
    }
    for (std::size_t i = 0; i < _all_animations.size(); ++i) {
      if (animations.count(_all_animations[i])) {
        _themes.back()->animation_shuffler
            .modify(i, 1 + (int32_t) _loaded_animations.size());
      }
    }
    for (std::size_t i = 0; i < _themes.back()->text_lines.size(); ++i) {
      _themes.back()->text_lookup[_themes.back()->text_lines[i]].push_back(i);
    }
  }

  // Set the initially-enabled themes.
  for (std::size_t i = 0; i < _active_themes.size(); ++i) {
    _active_themes[i] = nullptr;
  }
  set_program(program);
  // Choose the initial active themes and load them up.
  for (std::size_t i = 0; i < _active_themes.size(); ++i) {
    advance_theme();
    if (i) {
      while (!all_loaded()) {
        do_reconcile(*_active_themes.back().load());
      }
    }
  }
  for (std::size_t i = 1; i < _active_themes.size(); ++i) {
    do_load_animation(*_active_themes[i].load(), animation(i), false);
  }
}

const std::string& ThemeBank::get_root_path() const
{
  return _root_path;
}

void ThemeBank::set_program(const trance_pb::Program& program)
{
  _enabled_theme_weights.clear();
  _pinned_theme.clear();
  for (auto& theme : _themes) {
    theme->enabled = false;
  }
  for (const auto& theme : program.enabled_theme()) {
    if (theme.random_weight()) {
      _enabled_theme_weights[theme.theme_name()] = theme.random_weight();
    }
    if (theme.pinned()) {
      _pinned_theme = theme.theme_name();
    }
    auto index = _theme_map[theme.theme_name()];
    _themes[index]->enabled = true;
  }
  for (uint32_t i = 1; i < _active_themes.size(); ++i) {
    if (_active_themes[i].load() && !_active_themes[i].load()->enabled) {
      _swaps_to_match_theme = std::max(_swaps_to_match_theme, i);
    }
  }
  if (!_pinned_theme.empty()) {
    auto pinned_index = _theme_map[_pinned_theme];
    uint32_t count = 0;
    for (uint32_t i = 1; i < _active_themes.size(); ++i) {
      if (_themes[pinned_index].get() == _active_themes[i]) {
        ++count;
      }
    }
    if (count < 2) {
      _swaps_to_match_theme = std::max(_swaps_to_match_theme, 3u);
    }
  }
}

Image ThemeBank::get_image(bool alternate)
{
  auto& theme = *_active_themes[alternate ? 2 : 1].load();
  if (!theme.size) {
    return get_animation(alternate, random(2 << 16));
  }
  std::size_t index;
  Image image;
  {
    std::lock_guard<std::mutex> lock{theme.load_mutex};
    index = theme.image_shuffler.next();
    if (!_all_images[index].image) {
      return {};
    }
    do_video_upload(*_all_images[index].image);
    image = *_all_images[index].image;
  }
  _last_images.push_back(index);
  for (auto& other_theme : _themes) {
    std::lock_guard<std::mutex> lock{other_theme->load_mutex};
    other_theme->image_shuffler.decrease(_last_images.back());
    if (_last_images.size() > last_image_count) {
      other_theme->image_shuffler.increase(_last_images.front());
    }
  }
  if (_last_images.size() > last_image_count) {
    _last_images.erase(_last_images.begin());
  }
  return image;
}

Image ThemeBank::get_animation(bool alternate, std::size_t frame)
{
  auto& a = animation(alternate ? 2 : 1);
  std::lock_guard<std::mutex> lock{a.mutex};
  if (a.frames.empty()) {
    return {};
  }
  auto len = a.frames.size();
  auto f = frame % (2 * len - 2);
  f = f < len ? f : 2 * len - 2 - f;
  do_video_upload(a.frames[f]);
  return a.frames[f];
}

const std::string& ThemeBank::get_text(bool alternate, bool exclusive)
{
  auto& theme = *_active_themes[alternate ? 2 : 1].load();
  if (theme.text_lines.empty()) {
    const static std::string none;
    return none;
  }
  auto text = theme.text_lines[theme.text_shuffler.next()];
  if (!exclusive) {
    return text;
  }
  for (auto& other_theme : _themes) {
    auto it = other_theme->text_lookup.find(text);
    if (it != other_theme->text_lookup.end()) {
      for (auto index : it->second) {
        other_theme->text_shuffler.decrease(index);
      }
    }
    it = other_theme->text_lookup.find(_last_text);
    if (!_last_text.empty() && it != other_theme->text_lookup.end()) {
      for (auto index : it->second) {
        other_theme->text_shuffler.increase(index);
      }
    }
  }
  _last_text = text;
  return _last_text;
}

const std::string& ThemeBank::get_font(bool alternate)
{
  auto& theme = *_active_themes[alternate ? 2 : 1].load();
  if (theme.font_paths.empty()) {
    const static std::string none;
    return none;
  }
  auto r = random(theme.font_paths.size());
  return theme.font_paths[r];
}

void ThemeBank::maybe_upload_next()
{
  auto& theme = *_active_themes.back().load();
  if (theme.size) {
    std::lock_guard<std::mutex> lock{theme.load_mutex};
    auto index = theme.image_shuffler.next();
    if (_all_images[index].image) {
      do_video_upload(*_all_images[index].image);
    }
  }
  auto& a = animation(3);
  if (!a.frames.empty()) {
    do_video_upload(a.frames[random(a.frames.size())]);
  }
}

bool ThemeBank::change_themes()
{
  if (!all_loaded() || !all_unloaded() ||
      animation(0).loaded || !animation(3).loaded) {
    return false;
  }
  _cooldown = switch_cooldown;
  advance_theme();
  if (_swaps_to_match_theme) {
    --_swaps_to_match_theme;
  }
  return true;
}

bool ThemeBank::swaps_to_match_theme() const
{
  return _swaps_to_match_theme;
}

uint32_t ThemeBank::cache_per_theme() const
{
  std::size_t enabled_themes = 0;
  for (const auto& theme : _themes) {
    if (theme->enabled) {
      ++enabled_themes;
    }
  }
  return enabled_themes == 0 ? 0 : _image_cache_size /
      uint32_t(std::min<std::size_t>(3, enabled_themes));
}

void ThemeBank::async_update()
{
  do_purge();
  if (_cooldown) {
    --_cooldown;
    return;
  }

  ++_updates;
  // Swap some images from the active themes in and out every so often.
  if (_updates == 128) {
    do_swap(1);
    do_swap(2);
    _updates = 0;
  } else {
    do_reconcile(*_active_themes[1].load());
    do_reconcile(*_active_themes[2].load());
    if (animation(0).loaded) {
      do_load_animation(*_active_themes[0].load(), animation(0), true);
    } else if (!all_unloaded()) {
      do_unload(*_active_themes.front().load());
    }
    if (!animation(3).loaded) {
      do_load_animation(*_active_themes[3].load(), animation(3), false);
    } else if (!all_loaded()) {
      do_load(*_active_themes.back().load());
    }
  }
}

void ThemeBank::advance_theme()
{
  std::size_t random_theme_index = 0;
  if (_enabled_theme_weights.empty()) {
    random_theme_index = random(_themes.size());
  } else {
    uint32_t total = 0;
    for (const auto& pair : _enabled_theme_weights) {
      total += pair.second;
    }
    auto r = random(total);
    total = 0;
    for (const auto& pair : _enabled_theme_weights) {
      total += pair.second;
      if (r < total) {
        random_theme_index = _theme_map[pair.first];
        break;
      };
    }
  }
  if (!_pinned_theme.empty()) {
    auto pinned_index = _theme_map[_pinned_theme];
    if (_themes[pinned_index].get() != _active_themes.back()) {
      random_theme_index = pinned_index;
    }
  }

  for (std::size_t i = 0; 1 + i < _active_themes.size(); ++i) {
    _active_themes[i].store(_active_themes[1 + i].load());
  }
  _active_themes.back() = _themes[random_theme_index].get();
  _animation_index = (1 + _animation_index) % _loaded_animations.size();
}

bool ThemeBank::all_loaded() const
{
  const auto& next_theme = *_active_themes.back().load();
  return next_theme.loaded_size >= next_theme.size ||
      next_theme.loaded_size >= cache_per_theme();
}

bool ThemeBank::all_unloaded() const
{
  const auto& prev_theme = *_active_themes.front().load();
  std::size_t count = 0;
  for (const auto& active_theme : _active_themes) {
    if (active_theme.load() == &prev_theme) {
      ++count;
    }
  }
  return !prev_theme.loaded_size || count > 1;
}

ThemeBank::AnimationInfo&
ThemeBank::animation(std::size_t active_theme_index)
{
  return _loaded_animations[
      (_animation_index + active_theme_index) % _loaded_animations.size()];
}

void ThemeBank::do_swap(std::size_t active_theme_index)
{
  auto& theme = *_active_themes[active_theme_index].load();
  if (random_chance(4)) {
    do_load_animation(theme, animation(active_theme_index), false);
    return;
  }

  if (!theme.loaded_size || theme.loaded_size == theme.size) {
    return;
  }
  do_unload(theme);
  do_load(theme);
}

void ThemeBank::do_reconcile(ThemeInfo& theme)
{
  if (theme.loaded_size < cache_per_theme()) {
    do_load(theme);
  }
  if (theme.loaded_size > cache_per_theme()) {
    do_unload(theme);
  }
}

void ThemeBank::do_load(ThemeInfo& theme)
{
  if (theme.loaded_size >= theme.size) {
    return;
  }
  auto index = theme.load_shuffler.next();
  theme.load_shuffler.decrease(index);
  theme.loaded_index.emplace_back(index);

  auto& image = _all_images[index];
  // Could store spare capacity due to duplicated images and load more. Might
  // get a bit confusing though.
  if (!image.use_count++) {
    image.image.reset(new Image{load_image(_root_path + "/" + image.path)});
  }
  // Don't try to load again if it failed.
  if (!*image.image) {
    for (auto& other_theme : _themes) {
      other_theme->load_shuffler.modify(
          index, -static_cast<int32_t>(last_image_count));
    }
  }
  {
    std::lock_guard<std::mutex> lock{theme.load_mutex};
    theme.image_shuffler.increase(index);
  }
  ++theme.loaded_size;
}

void ThemeBank::do_unload(ThemeInfo& theme)
{
  if (!theme.loaded_size) {
    return;
  }
  auto index = theme.loaded_index.front();
  theme.load_shuffler.increase(index);
  theme.loaded_index.erase(theme.loaded_index.begin());

  {
    std::lock_guard<std::mutex> lock{theme.load_mutex};
    theme.image_shuffler.decrease(index);
  }
  auto& image = _all_images[index];
  if (!--image.use_count) {
    _purge_mutex.lock();
    _purgeable_images.push_back(image.image->get_sf_image());
    _purge_mutex.unlock();
    image.image.reset();
  }
  --theme.loaded_size;
}

void ThemeBank::do_load_animation(ThemeInfo& theme,
                                  AnimationInfo& animation, bool only_unload)
{
  if (animation.loaded) {
    _purge_mutex.lock();
    for (auto& image : animation.frames) {
      _purgeable_images.push_back(image.get_sf_image());
    }
    _purge_mutex.unlock();

    if (animation.index < _all_animations.size()) {
      for (auto& other_theme : _themes) {
        other_theme->animation_shuffler.increase(animation.index);
      }
    }
  }
  if (only_unload) {
    std::lock_guard<std::mutex> lock{animation.mutex};
    animation.frames.clear();
    animation.loaded = false;
    return;
  }

  animation.index = theme.animation_shuffler.next();
  if (animation.index >= _all_animations.size()) {
    animation.loaded = true;
    return;
  }


  auto new_frames =
      load_animation(_root_path + "/" + _all_animations[animation.index]);
  int32_t amount = -1;
  if (new_frames.empty()) {
    // Don't try to load again if it failed.
    amount = -static_cast<int32_t>(_loaded_animations.size());
  }

  {
    std::lock_guard<std::mutex> lock{animation.mutex};
    animation.frames = std::move(new_frames);
  }

  for (auto& other_theme : _themes) {
    other_theme->animation_shuffler.modify(animation.index, amount);
  }
  animation.loaded = true;
}

void ThemeBank::do_video_upload(const Image& image) const
{
  if (image.ensure_texture_uploaded()) {
    // Swap the sf::Image pointer so we can delete it on the async thread (see
    // do_purge() below).
    _purge_mutex.lock();
    _purgeable_images.push_back(image.get_sf_image());
    _purge_mutex.unlock();
    image.clear_sf_image();
  }
}

void ThemeBank::do_purge()
{
  _purge_mutex.lock();
  _purgeable_images.clear();
  _purge_mutex.unlock();
}