#include "visual.h"
#include "director.h"
#include "util.h"

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

AccelerateVisual::AccelerateVisual(Director& director, bool start_fast)
: Visual{director}
, _current{director.get_image()}
, _current_text{director.get_text()}
, _text_on{true}
, _change_timer{start_fast ? min_speed : max_speed}
, _change_speed{start_fast ? min_speed : max_speed}
, _change_speed_timer{0}
, _text_timer{text_time}
, _change_faster{!start_fast}
{
}

void AccelerateVisual::update()
{
  unsigned long d = max_speed - _change_speed;
  unsigned long m = max_speed;

  float spiral_d = 1 + float(d) / 8;
  director().rotate_spiral(_change_faster ? spiral_d : -spiral_d);

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
  if (_change_speed > 4 || _change_faster) {
    _current_text = director().get_text();
    _text_timer = text_time;
  }

  if (_change_speed_timer) {
    _change_speed_timer -= _change_faster || _change_speed_timer == 1 ? 1 : 2;
    return;
  }

  bool changed = false;
  _change_speed_timer = d * d * d * d * d * d / (m * m * m * m * m);
  if (_change_faster) {
    if (_change_speed == min_speed) {
      _change_faster = false;
      changed = true;
    }
    else {
      --_change_speed;
    }
  }
  else {
    if (_change_speed == max_speed) {
      _change_faster = true;
      changed = true;
    }
    else {
      ++_change_speed;
    }
  }

  if (changed) {
    if (!_change_faster) {
      director().change_spiral();
      if (director().change_themes()) {
        _current_text = director().get_text();
      }
    }
    director().change_font();
    if (random_chance()) {
      director().change_visual();
    }
  }
}

void AccelerateVisual::render() const
{
  director().render_image(
      _current, 1, 8.f + 48.f - _change_speed,
      _change_faster ? .5f - float(_change_timer) / (2 * _change_speed) :
      float(_change_timer) / (2 * _change_speed));
  director().render_animation_or_image(Director::Anim::ANIM, {}, .2f, 6.f);
  director().render_spiral();
  if (_change_faster && (_change_speed <= 4 || (_text_on && _text_timer)) ||
      !_change_faster && (_change_speed <= 16 || _text_on)) {
    director().render_text(_current_text);
  }
}

SubTextVisual::SubTextVisual(Director& director)
: Visual{director}
, _current{director.get_image()}
, _current_text{director.get_text()}
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

  if (_change_timer) {
    --_change_timer;
    if (_change_timer == speed / 2) {
      director().maybe_upload_next();
    }
    return;
  }

  _change_timer = speed;
  _current = director().get_image();
  _text_on = !_text_on;
  if (_text_on) {
    _current_text = director().get_text();
  }

  if (!--_cycle) {
    _cycle = cycles;
    if (director().change_themes()) {
      _current_text = director().get_text();
    }
    director().change_font();
    if (random_chance()) {
      director().change_visual();
      director().change_spiral();
    }
    ++_sub_speed_multiplier;
  }
}

void SubTextVisual::render() const
{
  director().render_animation_or_image(Director::Anim::ANIM, {}, 1, 10.f);
  director().render_image(_current, .8f, 8.f,
                          1.f - float(_change_timer) / speed);
  director().render_subtext(1.f / 4);
  director().render_spiral();
  if (_text_on && _change_timer >= speed - 3) {
    director().render_text(_current_text);
  }
}

SlowFlashVisual::SlowFlashVisual(Director& director)
: Visual{director}
, _current{director.get_image()}
, _current_text{director.get_text()}
, _change_timer{max_speed}
, _flash{false}
, _anim{false}
, _image_count{cycle_length}
, _cycle_count{set_length}
{
}

void SlowFlashVisual::update()
{
  director().rotate_spiral(_flash ? -2.f : 2.f);

  if (--_change_timer) {
    if (!_flash && _change_timer == max_speed / 2) {
      director().maybe_upload_next();
    }
    return;
  }

  if (!--_image_count) {
    _flash = !_flash;
    _image_count = _flash ? 4 * cycle_length : cycle_length;
    if (_change_timer < max_speed / 2 || _flash) {
      _current_text = director().get_text(_flash);
    }
    director().change_spiral();
    director().change_font();
    if (!--_cycle_count) {
      _cycle_count = set_length;
      director().change_themes();
      if (random_chance()) {
        director().change_visual();
      }
    }
  }

  _change_timer = _flash ? min_speed : max_speed;
  _anim = !_anim;
  _current = director().get_image(_flash);
  if (!_flash) {
    _current_text = director().get_text(false);
  }
}

void SlowFlashVisual::render() const
{
  float extra = 8.f - 8.f * _image_count / (4 * cycle_length);
  auto zoom = _flash ? float(_change_timer) / 2 * min_speed :
      .5f - float(_change_timer) / (2 * max_speed);
  director().render_animation_or_image(
      _anim && !_flash ? Director::Anim::ANIM : Director::Anim::NONE,
      _current, 1, 8.f + extra, zoom);
  director().render_spiral();
  if (_change_timer < max_speed / 2 || _flash) {
    director().render_text(
        _current_text,
        _flash ? 3.f + 8.f * (_image_count / (4.f * cycle_length)) : 4.f);
  }
}

FlashTextVisual::FlashTextVisual(Director& director)
: Visual{director}
, _animated{random_chance()}
, _start{director.get_image()}
, _end{director.get_image()}
, _current_text{director.get_text()}
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

  if (!--_timer) {
    if (!--_cycle) {
      _cycle = cycles;
      director().change_themes();
      _current_text = director().get_text();
      if (random_chance(4)) {
        director().change_visual();
      }
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
  director().render_animation_or_image(
      !_animated ? Director::Anim::NONE :
      _cycle % 2 ? Director::Anim::ANIM : Director::Anim::ANIM_ALTERNATE,
      _start, 1, 8.f + extra, 1.f - .5f * float(_timer) / length);

  director().render_animation_or_image(
      !_animated ? Director::Anim::NONE :
      _cycle % 2 ? Director::Anim::ANIM_ALTERNATE : Director::Anim::ANIM,
      _end, 1.f - float(_timer) / length, 40.f - extra,
      .5f - .5f * float(_timer) / length);

  director().render_spiral();
  if (_cycle % 2) {
    director().render_text(_current_text, 3.f + 4.f * _timer / length);
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
, _current_text{director.get_text(random_chance())}
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
    if (random_chance()) {
      director().change_visual();
    }
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
    _current_text = director().get_text(random_chance());
  }
}

void ParallelVisual::render() const
{
  float extra = 32.f * _cycle / cycles;
  auto anim = _anim_cycle % 3 == 2 ?
      Director::Anim::ANIM : Director::Anim::NONE;
  director().render_animation_or_image(
      anim, _image, 1, 8.f + extra, float(_length) / (2 * length));

  auto alt_anim = _alternate_anim_cycle % 3 == 1 ?
      Director::Anim::ANIM_ALTERNATE : Director::Anim::NONE;
  director().render_animation_or_image(
      alt_anim, _alternate, .5f, 8 + 32.f - extra,
      float(_alternate_length) / (2 * length));

  director().render_spiral();
  if (_cycle % 4 == 1 || _cycle % 4 == 2) {
    director().render_text(_current_text);
  }
}

SuperParallelVisual::SuperParallelVisual(Director& director)
: Visual{director}
, _index{0}
, _current_text{director.get_text(random_chance())}
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
    _current_text = _current_text.empty() ?
        director().get_text(random_chance()) : "";
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
    if (random_chance()) {
      director().change_visual();
    }
  }

  if (_cycle % 16 == 0) {
    director().maybe_upload_next();
  }

  _index = (_index + 1) % _images.size();
  _images[_index] = director().get_image(_index % 2 == 0);
  _lengths[_index] = 0;
}

void SuperParallelVisual::render() const
{
  float extra = 16.f - 16.f * (_cycle % 128) / (cycles / 4);
  for (std::size_t i = 0; i < _images.size(); ++i) {
    auto anim =
        i >= _images.size() / 2 ? Director::Anim::NONE :
        i % 2 ? Director::Anim::ANIM : Director::Anim::ANIM_ALTERNATE;
    director().render_animation_or_image(
        anim, _images[i], 1.f / (1 + i), 8.f + 4 * i + extra,
        i < _images.size() / 2 ? 0.f :
        float(_lengths[i]) / (4 * image_count * length));
  }
  director().render_spiral();
  director().render_text(_current_text, 5.f);
}

AnimationVisual::AnimationVisual(Director& director)
: Visual{director}
, _animation_backup{director.get_image()}
, _current{director.get_image(true)}
, _current_text{director.get_text()}
, _timer{length}
, _cycle{cycles}
{
}

void AnimationVisual::update()
{
  director().rotate_spiral(3.5f);
  _current = director().get_image(true);

  if (--_timer) {
    return;
  }
  _timer = length;
  _animation_backup = director().get_image();

  if (!--_cycle) {
    director().change_spiral();
    director().change_font();
    director().change_themes();
    _cycle = cycles;
    _current_text = director().get_text();
    if (random_chance(3)) {
      director().change_visual();
    }
  }

  if (_timer % 128 == 0) {
    director().maybe_upload_next();
  }
}

void AnimationVisual::render() const
{
  director().render_animation_or_image(
      Director::Anim::ANIM, _animation_backup, 1.f);
  director().render_image(_current, .2f, 12.f);
  director().render_spiral();
  if (_timer % 128 < 64) {
    director().render_text(_current_text, 5.f);
  }
}

SuperFastVisual::SuperFastVisual(Director& director)
: Visual{director}
, _current{director.get_image()}
, _start_timer{0}
, _animation_timer{0}
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
  if (_timer % 2 == 0) {
    _current = director().get_image();
    _current_text = director().get_text();
  }
  if (!_start_timer && random_chance(256)) {
    _animation_alt = !_animation_alt;
    _animation_timer = anim_length + random(anim_length);
    _start_timer = nonanim_lenth;
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
  if (random_chance(3)) {
    director().change_visual();
  }
}

void SuperFastVisual::render() const
{
  if (_animation_timer) {
    director().render_animation_or_image(
        _animation_alt ? Director::Anim::ANIM_ALTERNATE : Director::Anim::ANIM,
        _current, 1.f);
  }
  else {
    director().render_image(_current, 1.f, 8.f, _timer % 2 ? 0.f : 0.1f);
    if (_timer % 8 < 2) {
      director().render_text(_current_text, 5.f);
    }
  }
  director().render_spiral();
}