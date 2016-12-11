#include "visual_api.h"
#include <iostream>
#include "director.h"
#include "image.h"
#include "theme.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#pragma warning(pop)

namespace
{
  const uint32_t spiral_type_max = 7;

  sf::Color colour2sf(const trance_pb::Colour& colour)
  {
    return sf::Color(sf::Uint8(colour.r() * 255), sf::Uint8(colour.g() * 255),
                     sf::Uint8(colour.b() * 255), sf::Uint8(colour.a() * 255));
  }
}

std::vector<std::string> SplitWords(const std::string& text, SplitType type)
{
  std::vector<std::string> result;
  if (type == SplitType::NONE) {
    result.emplace_back(text);
    return result;
  }
  std::string s = text;
  while (!s.empty()) {
    auto of = type == SplitType::LINE ? "\r\n" : " \t\r\n";
    auto p = s.find_first_of(of);
    auto q = s.substr(0, p != std::string::npos ? p : s.size());
    if (!q.empty()) {
      result.push_back(q);
    }
    s = s.substr(p != std::string::npos ? 1 + p : s.size());
  }
  if (result.empty()) {
    result.emplace_back();
  }
  return result;
}

VisualApiImpl::VisualApiImpl(Director& director, ThemeBank& themes,
                             const trance_pb::Session& session, const trance_pb::System& system)
: _director{director}
, _themes{themes}
, _font_cache{_themes.get_root_path(), system.font_cache_size()}
, _switch_themes{0}
, _spiral{0}
, _spiral_type{0}
, _spiral_width{60}
, _small_subtext_x{0}
, _small_subtext_y{0}
{
  std::cout << "\ncaching text sizes" << std::endl;
  std::vector<std::string> fonts;
  for (const auto& pair : session.theme_map()) {
    for (const auto& font : pair.second.font_path()) {
      fonts.push_back(font);
    }
  }
  for (const auto& font : fonts) {
    FontCache temp_cache{_themes.get_root_path(), 8 * system.font_cache_size()};
    auto cache_text_size = [&](const std::string& text) {
      auto cache_key = font + "/\t/\t/" + text;
      auto cache_it = _text_size_cache.find(cache_key);
      if (cache_it == _text_size_cache.end()) {
        _text_size_cache[cache_key] = get_cached_text_size(temp_cache, text, font);
        std::cout << "-";
      }
    };

    for (const auto& pair : session.theme_map()) {
      if (!std::count(pair.second.font_path().begin(), pair.second.font_path().end(), font)) {
        continue;
      }
      for (const auto& text : pair.second.text_line()) {
        for (const auto& line : SplitWords(text, SplitType::LINE)) {
          cache_text_size(line);
        }
        for (const auto& word : SplitWords(text, SplitType::WORD)) {
          cache_text_size(word);
        }
      }
    }
  }

  change_font(true);
  change_spiral();
  change_subtext();
}

void VisualApiImpl::update()
{
  ++_switch_themes;
}

Image VisualApiImpl::get_image(bool alternate) const
{
  return _themes.get_image(alternate);
}

const std::string& VisualApiImpl::get_text(bool alternate) const
{
  return _themes.get_text(alternate, true);
}

void VisualApiImpl::maybe_upload_next() const
{
  _themes.maybe_upload_next();
}

void VisualApiImpl::rotate_spiral(float amount)
{
  if (!_director.program().reverse_spiral_direction()) {
    amount *= -1;
  }
  _spiral += amount / (32 * sqrt(float(_spiral_width)));
  while (_spiral > 1.f) {
    _spiral -= 1.f;
  }
  while (_spiral < 0.f) {
    _spiral += 1.f;
  }
}

void VisualApiImpl::change_spiral()
{
  if (random_chance(4)) {
    return;
  }
  _spiral_type = random(spiral_type_max);
  _spiral_width = 360 / (1 + random(6));
}

void VisualApiImpl::change_font(bool force)
{
  if (force || random_chance(4)) {
    _current_font = _themes.get_font(false);
  }
}

void VisualApiImpl::change_subtext(bool alternate)
{
  static const uint32_t count = 16;
  _subtext.clear();
  for (uint32_t i = 0; i < 16; ++i) {
    auto s = _themes.get_text(alternate, false);
    for (auto& c : s) {
      if (c == '\n') {
        c = ' ';
      }
    }
    if (!s.empty()) {
      _subtext.push_back(s);
    }
  }
}

void VisualApiImpl::change_small_subtext(bool force, bool alternate)
{
  if (force || _small_subtext.empty()) {
    _small_subtext = _themes.get_text(alternate, false);
    std::replace(_small_subtext.begin(), _small_subtext.end(), '\n', ' ');
    float x = _small_subtext_x;
    float y = _small_subtext_y;
    while (std::abs(x - _small_subtext_x) < .25f) {
      x = (random_chance() ? 1 : -1) * (16 + random(80)) / 128.f;
    }
    while (std::abs(y - _small_subtext_y) < .25f) {
      if (_director.vr_enabled()) {
        y = (random_chance() ? 1 : -1) * (2 + random(64)) / 128.f;
      } else {
        y = (random_chance() ? 1 : -1) * (16 + random(80)) / 128.f;
      }
    }
    _small_subtext_x = x;
    _small_subtext_y = y;
  } else {
    _small_subtext.clear();
  }
}

bool VisualApiImpl::change_themes()
{
  if (_switch_themes < 2048 || random_chance(4)) {
    return false;
  }
  if (_themes.change_themes()) {
    _switch_themes = 0;
    if (_current_font.empty()) {
      change_font(true);
    }
    return true;
  }
  return false;
}

bool VisualApiImpl::change_visual(uint32_t chance)
{
  // Like !random_chance(chance), but scaled to current speed.
  if (chance && random(chance * _director.program().global_fps()) < 120 &&
      _director.change_visual()) {
    _current_subfont = _themes.get_font(false);
    return true;
  }
  return false;
}

void VisualApiImpl::render_animation_or_image(Anim type, const Image& image, float alpha,
                                              float multiplier, float zoom) const
{
  Image anim = _themes.get_animation(
      type == Anim::ANIM_ALTERNATE,
      std::size_t((120.f / _director.program().global_fps()) * _switch_themes / 8));

  if (type != Anim::NONE && anim) {
    render_image(anim, alpha, multiplier, zoom);
  } else {
    render_image(image, alpha, multiplier, zoom);
  }
}

void VisualApiImpl::render_image(const Image& image, float alpha, float multiplier,
                                 float zoom) const
{
  _director.render_image(image, alpha, multiplier, zoom);
}

void VisualApiImpl::render_text(const std::string& text, float multiplier) const
{
  if (_current_font.empty() || text.empty()) {
    return;
  }
  auto cache_key = _current_font + "/\t/\t/" + text;
  auto it = _text_size_cache.find(cache_key);
  if (it == _text_size_cache.end()) {
    return;
  }
  auto main_size = it->second;
  auto shadow_size = main_size + FontCache::char_size_lock;
  const auto& program = _director.program();
  _director.render_text(text, _font_cache.get_font(_current_font, shadow_size),
                        colour2sf(program.shadow_text_colour()),
                        _director.off3d(1.f + multiplier, true),
                        std::exp((4.f - multiplier) / 16.f));
  _director.render_text(text, _font_cache.get_font(_current_font, main_size),
                        colour2sf(program.main_text_colour()), _director.off3d(multiplier, true),
                        std::exp((4.f - multiplier) / 16.f));
}

void VisualApiImpl::render_subtext(float alpha, float multiplier) const
{
  if (_current_subfont.empty() || _subtext.empty()) {
    return;
  }

  static const uint32_t char_size = 100;
  std::size_t n = 0;
  const auto& font = _font_cache.get_font(_current_subfont, char_size);

  auto make_text = [&] {
    std::string t;
    do {
      t += " " + _subtext[n];
      n = (n + 1) % _subtext.size();
    } while (get_text_size(t, font).x < _director.resolution().x);
    return t;
  };

  float offx3d = _director.off3d(multiplier, true).x;
  auto text = make_text();
  auto d = get_text_size(text, font);
  if (d.y <= 0) {
    return;
  }
  auto colour = colour2sf(_director.program().shadow_text_colour());
  colour.a = uint8_t(colour.a * alpha);
  _director.render_text(text, font, colour, sf::Vector2f{offx3d, 0});
  auto offset = d.y + 4;
  for (int i = 1; d.y / 2 + i * offset < _director.resolution().y; ++i) {
    text = make_text();
    _director.render_text(text, font, colour, sf::Vector2f{offx3d, i * offset});

    text = make_text();
    _director.render_text(text, font, colour, -sf::Vector2f{offx3d, i * offset});
  }
}

void VisualApiImpl::render_small_subtext(float alpha, float multiplier) const
{
  if (_current_subfont.empty() || _small_subtext.empty()) {
    return;
  }
  static const uint32_t border_x = 400;
  static const uint32_t border_y = 200;
  static const uint32_t char_size = 100;

  std::size_t n = 0;
  const auto& font = _font_cache.get_font(_current_subfont, char_size);

  float offx3d = _director.off3d(multiplier, true).x;
  auto d = get_text_size(_small_subtext, font);
  if (d.y <= 0) {
    return;
  }
  auto colour = colour2sf(_director.program().shadow_text_colour());
  colour.a = uint8_t(colour.a * alpha);
  _director.render_text(
      _small_subtext, font, colour,
      sf::Vector2f{offx3d + _small_subtext_x * (_director.resolution().x - border_x) / 2,
                   _small_subtext_y * (_director.resolution().y - border_y) / 2});
}

void VisualApiImpl::render_spiral() const
{
  _director.render_spiral(_spiral, _spiral_width, _spiral_type);
}

uint32_t VisualApiImpl::get_cached_text_size(const FontCache& cache, const std::string& text,
                                             const std::string& font) const
{
  static const uint32_t minimum_size = 2 * FontCache::char_size_lock;
  static const uint32_t increment = FontCache::char_size_lock;
  static const uint32_t maximum_size = 40 * FontCache::char_size_lock;
  static const uint32_t border_x = 250;
  static const uint32_t border_y = 150;

  uint32_t size = minimum_size;
  auto res = _director.resolution();
  auto target_x = _director.view_width() - border_x;
  auto target_y = std::min(res.y / 3, res.y - border_y);
  sf::Vector2f last_result;
  while (size < maximum_size) {
    const auto& loaded_font = cache.get_font(font, size);
    auto result = get_text_size(text, loaded_font);
    if (result.x > target_x || result.y > target_y || result == last_result) {
      break;
    }
    last_result = result;
    size *= 2;
  }
  size /= 2;
  last_result = sf::Vector2f{};
  while (size < maximum_size) {
    const auto& loaded_font = cache.get_font(font, size + increment);
    auto result = get_text_size(text, loaded_font);
    if (result.x > target_x || result.y > target_y || result == last_result) {
      break;
    }
    last_result = result;
    size += increment;
  }
  return size;
};

sf::Vector2f VisualApiImpl::get_text_size(const std::string& text, const Font& font) const
{
  auto hspace = font.font->getGlyph(' ', font.key.char_size, false).advance;
  auto vspace = font.font->getLineSpacing(font.key.char_size);
  float x = 0.f;
  float y = 0.f;
  float xmin = 0.f;
  float ymin = 0.f;
  float xmax = 0.f;
  float ymax = 0.f;

  uint32_t prev = 0;
  for (std::size_t i = 0; i < text.length(); ++i) {
    uint32_t current = text[i];
    x += font.font->getKerning(prev, current, font.key.char_size);
    prev = current;

    switch (current) {
    case L' ':
      x += hspace;
      continue;
    case L'\t':
      x += hspace * 4;
      continue;
    case L'\n':
      y += vspace;
      x = 0;
      continue;
    case L'\v':
      y += vspace * 4;
      continue;
    }

    const auto& g = font.font->getGlyph(current, font.key.char_size, false);
    float x1 = x + g.bounds.left;
    float y1 = y + g.bounds.top;
    float x2 = x + g.bounds.left + g.bounds.width;
    float y2 = y + g.bounds.top + g.bounds.height;
    xmin = std::min(xmin, std::min(x1, x2));
    xmax = std::max(xmax, std::max(x1, x2));
    ymin = std::min(ymin, std::min(y1, y2));
    ymax = std::max(ymax, std::max(y1, y2));
    x += g.advance;
  }

  return {xmax - xmin, ymax - ymin};
}