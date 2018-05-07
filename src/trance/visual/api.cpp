#include <trance/visual/api.h>
#include <common/media/image.h>
#include <common/util.h>
#include <trance/director.h>
#include <trance/theme_bank.h>
#include <iostream>

#pragma warning(push, 0)
#include <common/trance.pb.h>
#pragma warning(pop)

namespace
{
  const uint32_t spiral_type_max = 7;

  sf::Color colour2sf(const trance_pb::Colour& colour)
  {
    return sf::Color(sf::Uint8(colour.r() * 255), sf::Uint8(colour.g() * 255),
                     sf::Uint8(colour.b() * 255), sf::Uint8(colour.a() * 255));
  }

  std::vector<std::string> SplitText(const std::string& text, bool split_words)
  {
    std::vector<std::string> result;
    std::string s = text;
    while (!s.empty()) {
      auto of = split_words ? " \t\r\n" : "\r\n";
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
}

VisualApiImpl::VisualApiImpl(Director& director, ThemeBank& themes,
                             const trance_pb::Session& session, const trance_pb::System& system,
                             uint32_t height_pixels)
: _director{director}
, _themes{themes}
, _font_cache{_themes.get_root_path(), session, height_pixels / 3, height_pixels / 12,
              system.font_cache_size()}
, _switch_themes{0}
, _spiral{0}
, _spiral_type{0}
, _spiral_width{60}
, _small_subtext_x{0}
, _small_subtext_y{0}
{
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

void VisualApiImpl::change_animation(bool alternate)
{
  _themes.change_animation(alternate);
}

void VisualApiImpl::change_font(bool force)
{
  if (force || random_chance(4)) {
    _current_font = _themes.get_font(false);
  }
  if (force || random_chance(4)) {
    _current_subfont = _themes.get_font(false);
  }
}

void VisualApiImpl::change_text(SplitType split_type, bool alternate)
{
  bool gaps = split_type == SPLIT_LINE_GAPS || split_type == SPLIT_WORD_GAPS;
  bool split_word = split_type == SPLIT_WORD || split_type == SPLIT_WORD_GAPS;
  bool once_only = split_type == SPLIT_ONCE_ONLY;

  if (_current_text.empty() && once_only) {
    return;
  }
  if (!_current_text.empty()) {
    _current_text.erase(_current_text.begin());
    if (!_current_text.empty() || gaps || once_only) {
      return;
    }
  }
  _current_text = SplitText(_themes.get_text(alternate, true), split_word);
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
    while (std::abs(x - _small_subtext_x) < 1.f / 8) {
      x = (random_chance() ? 1 : -1) * random(64) / 128.f;
    }
    while (std::abs(y - _small_subtext_y) < 1.f / 4) {
      y = (random_chance() ? 1 : -1) * (16 + random(112)) / 128.f;
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

void VisualApiImpl::render_animation_or_image(Anim type, const Image& image, float alpha,
                                              float zoom_origin, float zoom) const
{
  Image anim;
  if (type != Anim::NONE) {
    anim = _themes.get_animation(type == Anim::ANIM_ALTERNATE);
  }

  if (anim) {
    render_image(anim, alpha, zoom_origin, zoom);
  } else {
    render_image(image, alpha, zoom_origin, zoom);
  }
}

void VisualApiImpl::render_image(const Image& image, float alpha, float zoom_origin,
                                 float zoom) const
{
  _director.render_image(image, alpha, zoom_origin, zoom_intensity(zoom_origin, zoom));
}

void VisualApiImpl::render_text(float zoom_origin, float zoom, float shadow_zoom_origin,
                                float shadow_zoom) const
{
  if (_current_font.empty() || _current_text.empty() || _current_text.front().empty()) {
    return;
  }
  const auto& text = _current_text.front();
  const auto& program = _director.program();
  const auto& font = _font_cache.get_font(_current_font);

  auto size = _director.text_size(font, text, true);
  if (!size.x || !size.y) {
    return;
  }
  auto target_x = _director.vr_enabled() ? 3.f / 8.f : 5.f / 8.f;
  auto target_y = 1.f / 3.f;
  auto scale = std::min(target_x / size.x, target_y / size.y);

  auto shadow_colour = colour2sf(_director.program().shadow_text_colour());
  auto main_colour = colour2sf(_director.program().main_text_colour());
  _director.render_text(font, text, true, shadow_colour, 1.2f * scale, {}, shadow_zoom_origin,
                        zoom_intensity(shadow_zoom_origin, shadow_zoom));
  _director.render_text(font, text, true, main_colour, scale, {}, zoom_origin,
                        zoom_intensity(zoom_origin, zoom));
}

void VisualApiImpl::render_subtext(float alpha, float zoom_origin) const
{
  if (_current_subfont.empty() || _subtext.empty()) {
    return;
  }
  const auto& program = _director.program();
  const auto& font = _font_cache.get_font(_current_subfont);
  auto target_y = _director.vr_enabled() ? 1.f / 32.f : 1.f / 16.f;

  sf::Vector2f size;
  std::string text;
  size_t n = 0;
  auto make_text = [&] {
    text.clear();
    size_t iterations = 0;
    do {
      text += " " + _subtext[n];
      n = (n + 1) % _subtext.size();
      size = _director.text_size(font, text, false);
      ++iterations;
    } while (size.x * target_y / size.y < 1.f && iterations < 64);
  };

  make_text();
  if (!size.x || !size.y) {
    return;
  }
  auto scale = target_y / size.y;

  auto colour = colour2sf(_director.program().shadow_text_colour());
  colour.a = uint8_t(colour.a * alpha);
  _director.render_text(font, text, false, colour, scale, {}, 0, 0);

  auto offset = 2 * target_y + 1.f / 512;
  for (int i = 1; (i - 1) * 2 * target_y < 1.f; ++i) {
    make_text();
    if (size.x && size.y) {
      _director.render_text(font, text, false, colour, scale, sf::Vector2f{0, i * offset},
                            zoom_origin, zoom_intensity(zoom_origin, zoom_origin));
    }

    make_text();
    if (size.x && size.y) {
      _director.render_text(font, text, false, colour, scale, -sf::Vector2f{0, i * offset},
                            zoom_origin, zoom_intensity(zoom_origin, zoom_origin));
    }
  }
}

void VisualApiImpl::render_small_subtext(float alpha, float zoom_origin) const
{
  if (_current_subfont.empty() || _small_subtext.empty()) {
    return;
  }
  const auto& program = _director.program();
  const auto& font = _font_cache.get_font(_current_subfont);

  auto size = _director.text_size(font, _small_subtext, false);
  if (!size.x || !size.y) {
    return;
  }
  auto target_y = _director.vr_enabled() ? 1.f / 24.f : 1.f / 8.f;
  auto scale = target_y / size.y;

  auto colour = colour2sf(_director.program().shadow_text_colour());
  colour.a = uint8_t(colour.a * alpha);
  _director.render_text(font, _small_subtext, false, colour, scale,
                        {_small_subtext_x / 2, _small_subtext_y / 2}, zoom_origin,
                        zoom_intensity(zoom_origin, zoom_origin));
}

void VisualApiImpl::render_spiral() const
{
  _director.render_spiral(_spiral, _spiral_width, _spiral_type);
}

float VisualApiImpl::zoom_intensity(float zoom_origin, float zoom) const
{
  return zoom_origin + (zoom - zoom_origin) * _director.program().zoom_intensity();
}