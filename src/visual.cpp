#include "visual.h"
#include "director.h"
#include "util.h"

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

// TODO: some sort of unification of this logic, especially timers, calls to
// maybe_upload_next, etc.
Visual::Visual(Director& director)
: _director{director}
{
}

const Director& Visual::director() const
{
  return _director;
}

Director& Visual::director()
{
  return _director;
}

AccelerateVisual::AccelerateVisual(Director& director)
: Visual{director}
, _current{director.get_image()}
, _current_text{SplitWords(director.get_text(), SplitType::LINE)}
, _text_on{true}
, _change_timer{max_speed}
, _change_speed{max_speed}
, _change_speed_timer{0}
, _text_timer{text_time}
{
}

void AccelerateVisual::update()
{
  unsigned long long d = max_speed - _change_speed;
  unsigned long long m = max_speed;

  float spiral_d = 1 + float(d) / 8;
  director().rotate_spiral(spiral_d);

  if (_text_timer) {
    --_text_timer;
  }
  if (_change_timer) {
    --_change_timer;
    if (_change_timer == _change_speed / 2 && _change_speed > max_speed / 2) {
      director().maybe_upload_next();
    }
    return;
  }

  _change_timer = _change_speed;
  _current = director().get_image();
  _text_on = !_text_on;
  if (!_text_on) {
    _current_text.erase(_current_text.begin());
    if (_current_text.empty()) {
      _current_text = SplitWords(
          director().get_text(),
          _change_speed < 8 ? SplitType::WORD : SplitType::LINE);
    }
  }
  _text_timer = text_time;

  if (_change_speed_timer) {
    _change_speed_timer -= 1;
    return;
  }

  bool changed = false;
  _change_speed_timer = uint32_t(d * d * d * d * d * d / (m * m * m * m * m));
  if (_change_speed != min_speed) {
    --_change_speed;
    return;
  }

  director().change_spiral();
  if (director().change_themes()) {
    _current_text = SplitWords(director().get_text(), SplitType::LINE);
  }
  director().change_font();
  // Frames is something like:
  // 46 + sum(3 <= k < 48, (1 + floor(k^6/48^5))(48 - k)).
  // 1/2 chance after ~2850.
  // 1/2 random weight.
  // Average length 2 * 2850 = 5700 frames.
  director().change_visual(2);
}

void AccelerateVisual::render() const
{
  auto z = float(_change_timer) / (2 * _change_speed);
  director().render_image(
      _current, 1, 8.f + 48.f - _change_speed, .5f - z);
  director().render_animation_or_image(Director::Anim::ANIM, {}, .2f, 6.f);
  director().render_spiral();
  if (_change_speed == min_speed || (_text_on && _text_timer)) {
    director().render_text(_current_text[0]);
  }
}

SubTextVisual::SubTextVisual(Director& director)
: Visual{director}
, _current{director.get_image()}
, _current_text{SplitWords(director.get_text(), SplitType::LINE)}
, _text_on{true}
, _change_timer{speed}
, _sub_timer{sub_speed}
, _cycle{cycles}
, _sub_speed_multiplier{1}
{
}

void SubTextVisual::update()
{
  director().rotate_spiral(4.f);

  if (!--_sub_timer) {
    _sub_timer = sub_speed * _sub_speed_multiplier;
    director().change_subtext();
  }

  if (--_change_timer) {
    if (_change_timer == speed / 2) {
      director().maybe_upload_next();
    }
    return;
  }

  _change_timer = speed;
  _current = director().get_image();
  _text_on = !_text_on;
  if (_text_on) {
    _current_text = SplitWords(director().get_text(), SplitType::LINE);
  }

  if (!--_cycle) {
    _cycle = cycles;
    if (director().change_themes()) {
      _current_text = SplitWords(director().get_text(), SplitType::LINE);
    }
    director().change_font();
    // 1/3 chance after 32 * 48 = 1536 frames.
    // Average length 3 * 1536 = 4608 frames.
    if (director().change_visual(3)) {
      director().change_spiral();
    }
    ++_sub_speed_multiplier;
  }
}

void SubTextVisual::render() const
{
  director().render_animation_or_image(Director::Anim::ANIM, {}, 1, 10.f);
  director().render_image(_current, .8f, 8.f,
                          2.f - float(_change_timer) / speed);
  director().render_subtext(1.f / 4);
  director().render_spiral();
  if (_text_on) {
    auto index = (speed - _change_timer) / 4;
    if (index < _current_text.size()) {
      director().render_text(_current_text[index]);
    }
  }
}

SlowFlashVisual::SlowFlashVisual(Director& director)
: Visual{director}
, _flash{false}
, _anim{false}
, _current{director.get_image()}
, _current_text{SplitWords(director.get_text(),
                           _flash ? SplitType::WORD : SplitType::LINE)}
, _change_timer{max_speed}
, _image_count{cycle_length}
, _cycle_count{set_length}
{
}

void SlowFlashVisual::update()
{
  director().rotate_spiral(_flash ? 4.f : 2.f);

  if (--_change_timer) {
    if (!_flash && _change_timer == max_speed / 2) {
      director().maybe_upload_next();
    }
    return;
  }

  bool changed_text = false;
  if (!--_image_count) {
    _flash = !_flash;
    _image_count = _flash ? 2 * cycle_length : cycle_length;
    if (_change_timer < max_speed / 2 || _flash) {
      changed_text = true;
      _current_text.erase(_current_text.begin());
      if (_current_text.empty()) {
        _current_text = SplitWords(director().get_text(_flash),
                                   _flash ? SplitType::WORD : SplitType::LINE);
      }
    }
    director().change_spiral();
    director().change_font();
    if (!--_cycle_count) {
      _cycle_count = set_length;
      director().change_themes();
      // 1/2 chance after 2 * (16 * 64 + 64 * 4) = 2560 frames.
      // Average length 2 * 2560 = 5120 frames.
      director().change_visual(2);
    }
  }

  _change_timer = _flash ? min_speed : max_speed;
  _anim = !_anim;
  _current = director().get_image(_flash);
  if (!changed_text) {
    _current_text.erase(_current_text.begin());
    if (_current_text.empty()) {
      _current_text = SplitWords(director().get_text(_flash),
                                 _flash ? SplitType::WORD : SplitType::LINE);
    }
  }
}

void SlowFlashVisual::render() const
{
  float extra = 8.f - 8.f * _image_count / (4 * cycle_length);
  auto zoom = 1.5f * (1.f - (_flash ?
      float(max_speed - min_speed + _change_timer) :
      float(_change_timer)) / max_speed);
  director().render_animation_or_image(
      _anim && !_flash ? Director::Anim::ANIM : Director::Anim::NONE,
      _current, 1, 8.f + extra, zoom);
  director().render_spiral();
  if ((!_flash && _change_timer < max_speed / 2) ||
      (_flash && _image_count % 2)) {
    director().render_text(
        _current_text[0],
        _flash ? 3.f + 8.f * (_image_count / (4.f * cycle_length)) : 4.f);
  }
}

FlashTextVisual::FlashTextVisual(Director& director)
: Visual{director}
, _animated{random_chance()}
, _start{director.get_image()}
, _end{director.get_image()}
, _current_text{SplitWords(director.get_text(), SplitType::LINE)}
, _timer{length}
, _font_timer{font_length}
, _cycle{cycles}
{
}

void FlashTextVisual::update()
{
  director().rotate_spiral(2.5f);

  if (!--_font_timer) {
    director().change_font(true);
    _font_timer = font_length;
  }

  if (_timer == length / 2 && _cycle % 2 == 0) {
    _current_text.erase(_current_text.begin());
    if (_current_text.empty()) {
      _current_text = SplitWords(director().get_text(), SplitType::LINE);
    }
  }

  if (!--_timer) {
    if (!--_cycle) {
      _cycle = cycles;
      director().change_themes();
      // 1/8 chance after 64 * 8 = 512 frames.
      // Average length 8 * 512 = 4096 frames.
      director().change_visual(8);
    }

    _start = _end;
    _end = director().get_image();
    _timer = length;
  }

  if (_timer == length / 2) {
    director().maybe_upload_next();
  }
}

void FlashTextVisual::render() const
{
  float extra = 32.f * _timer / length;
  float zoom = float(_timer) / length;
  director().render_animation_or_image(
      !_animated ? Director::Anim::NONE :
      _cycle % 2 ? Director::Anim::ANIM : Director::Anim::ANIM_ALTERNATE,
      _start, 1, 8.f + extra, (_animated ? 1.5f : 1.f) * (2.f - zoom));

  director().render_animation_or_image(
      !_animated ? Director::Anim::NONE :
      _cycle % 2 ? Director::Anim::ANIM_ALTERNATE : Director::Anim::ANIM,
      _end, 1.f - float(_timer) / length, 40.f - extra,
      (_animated ? 1.5f : 1.f) * (1.f - zoom));

  director().render_spiral();
  if (_cycle % 2) {
    director().render_text(_current_text[0], 3.f + 4.f * _timer / length);
  }
}

ParallelVisual::ParallelVisual(Director& director)
: Visual{director}
, _image{director.get_image()}
, _alternate{director.get_image(true)}
, _anim_cycle{0}
, _alternate_anim_cycle{0}
, _length{0}
, _alternate_length{length / 2}
, _switch_alt{false}
, _text_on{true}
, _current_text{SplitWords(director.get_text(random_chance()), SplitType::LINE)}
, _timer{length}
, _cycle{cycles}
{
}

void ParallelVisual::update()
{
  ++_length;
  ++_alternate_length;
  director().rotate_spiral(3.f);
  if (--_timer) {
    if (_timer == length / 2) {
      director().maybe_upload_next();
    }
    return;
  }
  _timer = length;

  if (!--_cycle) {
    director().change_spiral();
    director().change_font();
    director().change_themes();
    _cycle = cycles;
    // 1/2 chance after 32 * 64 = 2048 frames.
    // Average length 2 * 2048 = 4096 frames.
    director().change_visual(2);
  }

  _switch_alt = !_switch_alt;
  if (_switch_alt) {
    _alternate = director().get_image(true);
    ++_alternate_anim_cycle;
    _alternate_length = 0;
  }
  else {
    _image = director().get_image(false);
    ++_anim_cycle;
    _length = 0;
  }
  if (_cycle % 4 == 2) {
    _current_text.erase(_current_text.begin());
    if (_current_text.empty()) {
      _current_text =
          SplitWords(director().get_text(random_chance()), SplitType::LINE);
    }
  }
}

void ParallelVisual::render() const
{
  float extra = 32.f * _cycle / cycles;
  auto anim = _anim_cycle % 3 == 2 ?
      Director::Anim::ANIM : Director::Anim::NONE;
  director().render_animation_or_image(
      anim, _image, 1, 8.f + extra, float(_length) / length);

  auto alt_anim = _alternate_anim_cycle % 3 == 1 ?
      Director::Anim::ANIM_ALTERNATE : Director::Anim::NONE;
  director().render_animation_or_image(
      alt_anim, _alternate, .5f, 8 + 32.f - extra,
      1.5f * float(_alternate_length) / length);

  director().render_spiral();
  if (_cycle % 4 == 1 || _cycle % 4 == 2) {
    director().render_text(_current_text[0]);
  }
}

SuperParallelVisual::SuperParallelVisual(Director& director)
: Visual{director}
, _index{0}
, _current_text{SplitWords(director.get_text(random_chance()), SplitType::WORD)}
, _timer{length}
, _font_timer{font_length}
, _cycle{cycles}
{
  for (std::size_t i = 0; i < image_count; ++i) {
    _images.push_back(director.get_image(i % 2 == 0));
    _lengths.push_back(
        ((image_count * length) - i * length) % (image_count * length));
  }
}

void SuperParallelVisual::update()
{
  for (std::size_t i = 0; i < image_count; ++i) {
    ++_lengths[i];
  }
  director().rotate_spiral(3.5f);
  if (!--_font_timer) {
    if (_current_text.empty()) {
      _current_text = SplitWords(director().get_text(random_chance()), SplitType::WORD);
    } else {
      _current_text.erase(_current_text.begin());
    }
    _font_timer = font_length;
  }

  if (--_timer) {
    return;
  }
  _timer = length;

  if (!--_cycle) {
    director().change_spiral();
    director().change_font();
    director().change_themes();
    _cycle = cycles;
    // 1/4 chance after 2 * 512 = 1024 frames.
    // Average length 4 * 1024 = 4096 frames.
    director().change_visual(4);
  }

  if (_cycle % 16 == 0) {
    director().maybe_upload_next();
  }

  _index = (_index + 1) % _images.size();
  while (_index < _images.size() / 2) {
    _index = (_index + 1) % _images.size();
  }
  _images[_index] = director().get_image(_index % 2 == 0);
  _lengths[_index] = 0;
}

void SuperParallelVisual::render() const
{
  float extra = 16.f - 16.f * (_cycle % 128) / (cycles / 4);
  for (std::size_t i = 0; i < _images.size(); ++i) {
    auto anim =
        i >= _images.size() / 2 ? Director::Anim::NONE :
        i % 2 ? Director::Anim::ANIM_ALTERNATE : Director::Anim::ANIM;
    director().render_animation_or_image(
        anim, _images[i], 1.f / (1 + i), 8.f + 4 * i + extra,
        i < _images.size() / 2 ? 0.f :
        float(_lengths[i]) / (image_count * length));
  }
  director().render_spiral();
  director().render_text(_current_text.empty() ? "" : _current_text[0], 5.f);
}

AnimationVisual::AnimationVisual(Director& director)
: Visual{director}
, _animation_backup{director.get_image()}
, _current{director.get_image(true)}
, _current_text_base{director.get_text()}
, _current_text{SplitWords(_current_text_base, SplitType::WORD)}
, _timer{length}
, _cycle{cycles}
{
}

void AnimationVisual::update()
{
  director().rotate_spiral(3.5f);
  if (_timer % image_length == 0) {
    _current = director().get_image(true);
  }

  if (--_timer) {
    if (_timer % 128 == 0) {
      director().maybe_upload_next();
    }
    if (_timer % 128 == 63) {
      _current_text.erase(_current_text.begin());
      if (_current_text.empty()) {
        _current_text = SplitWords(_current_text_base, SplitType::WORD);
      }
    }
    return;
  }
  _timer = length;
  _animation_backup = director().get_image();

  if (!--_cycle) {
    director().change_spiral();
    director().change_font();
    director().change_themes();
    _cycle = cycles;
    _current_text_base = director().get_text();
    _current_text.erase(_current_text.begin());
    _current_text = SplitWords(_current_text_base, SplitType::WORD);
    // 1/4 chance after 256 * 4 = 1024 frames.
    // Average length 4 * 1024 = 4096 frames.
    director().change_visual(4);
  }
}

void AnimationVisual::render() const
{
  auto which_anim = (_timer / animation_length) % 2 ?
      Director::Anim::ANIM : Director::Anim::ANIM_ALTERNATE;
  director().render_animation_or_image(
      which_anim, _animation_backup, 1.f,
      20.f - 8.f * float(_timer % animation_length) / animation_length,
      4.f - 4.f * float(_timer % animation_length) / animation_length);
  director().render_image(
      _current, .2f,
      20.f - 8.f * float(_timer % image_length) / image_length,
      1.f - float(_timer % image_length) / image_length);
  director().render_spiral();
  if (_timer % 128 >= 64) {
    director().render_text(_current_text[0], 5.f);
  }
}

SuperFastVisual::SuperFastVisual(Director& director)
: Visual{director}
, _current{director.get_image()}
, _current_text{SplitWords(director.get_text(), SplitType::WORD)}
, _start_timer{0}
, _animation_timer{anim_length + random(anim_length)}
, _animation_alt{false}
, _timer{length}
{
}

void SuperFastVisual::update()
{
  director().rotate_spiral(3.f);
  if (_animation_timer) {
    --_animation_timer;
    return;
  }
  if (_timer % image_length == 0) {
    _current = director().get_image();
    _current_text.erase(_current_text.begin());
    if (_current_text.empty()) {
      _current_text = SplitWords(director().get_text(), SplitType::WORD);
    }
  }
  if (!_start_timer && random_chance(128)) {
    _animation_alt = !_animation_alt;
    _animation_timer = anim_length + random(anim_length);
    _start_timer = nonanim_length;
  }
  if (_start_timer) {
    --_start_timer;
  }
  if (--_timer) {
    return;
  }

  director().change_spiral();
  director().change_font();
  director().change_themes();
  _timer = length;
  // Roughly 1024 + ~ 1024 * (128 + 128 / 2) / 256 (ignoring the _start_timer).
  // 1/2 chance after ~1792 frames.
  // Average length 2 * 1792 = 3584 frames.
  director().change_visual(2);
}

void SuperFastVisual::render() const
{
  if (_animation_timer) {
    director().render_animation_or_image(
        _animation_alt ? Director::Anim::ANIM_ALTERNATE : Director::Anim::ANIM,
        _current, 1.f, 20.f - 8.f * float(_animation_timer) / anim_length,
        4.f - 2.f * float(_animation_timer) / anim_length);
  }
  else {
    director().render_image(
        _current, 1.f,
        20.f - 12.f * float(_timer % image_length) / image_length,
        1.f - float(_timer % image_length) / image_length);
    if (_timer % image_length < 2) {
      director().render_text(_current_text[0], 5.f);
    }
  }
  director().render_spiral();
}