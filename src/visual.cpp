#include "visual.h"
#include "director.h"
#include "util.h"
#include "visual_api.h"
// TODO: some sort of unification of this logic, especially timers, calls to
// maybe_upload_next, etc.

AccelerateVisual::AccelerateVisual(VisualControl& api)
: _current{api.get_image()}
, _text_on{true}
, _image_count{0}
, _change_timer{max_speed}
, _change_speed{max_speed}
, _change_speed_timer{0}
, _text_timer{text_time}
{
  api.change_text(VisualControl::SPLIT_LINE);
}

void AccelerateVisual::update(VisualControl& api)
{
  unsigned long long d = max_speed - _change_speed;
  unsigned long long m = max_speed;

  float spiral_d = 1 + float(d) / 8;
  api.rotate_spiral(spiral_d);

  if (_text_timer) {
    --_text_timer;
  }
  if (_change_timer) {
    --_change_timer;
    if (_change_timer == _change_speed / 2 && _change_speed > max_speed / 2) {
      api.maybe_upload_next();
    }
    return;
  }

  ++_image_count;
  _change_timer = _change_speed;
  _current = api.get_image((_image_count / 32) % 2);
  _text_on = !_text_on;
  if (_text_on) {
    api.change_text(_change_speed < 8 ? VisualControl::SPLIT_WORD : VisualControl::SPLIT_LINE,
                    (_image_count / 32) % 2);
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

  api.change_spiral();
  api.change_font();
  // Frames is something like:
  // 46 + sum(3 <= k < 48, (1 + floor(k^6/48^5))(48 - k)).
  // 1/2 chance after ~2850.
  // 1/2 random weight.
  // Average length 2 * 2850 = 5700 frames.
  api.change_visual(2);
}

void AccelerateVisual::render(VisualRender& api) const
{
  auto z = float(_change_timer) / (2 * _change_speed);
  api.render_image(_current, 1, 8.f + 48.f - _change_speed, .5f - z);
  api.render_animation_or_image(
      (_image_count / 32) % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, {},
      .2f, 6.f);
  api.render_spiral();
  if (_change_speed == min_speed || (_text_on && _text_timer)) {
    api.render_text();
  }
}

SubTextVisual::SubTextVisual(VisualControl& api)
: _current{api.get_image()}
, _text_on{true}
, _alternate{false}
, _change_timer{speed}
, _sub_timer{sub_speed}
, _cycle{cycles}
, _sub_speed_multiplier{1}
{
  api.change_text(VisualControl::SPLIT_WORD);
}

void SubTextVisual::update(VisualControl& api)
{
  api.rotate_spiral(4.f);

  if (!--_sub_timer) {
    _sub_timer = sub_speed * _sub_speed_multiplier;
    api.change_subtext(_alternate);
  }

  if (--_change_timer) {
    if (_change_timer % 4 == 0) {
      api.change_text(VisualControl::SPLIT_ONCE_ONLY);
    }
    if (_change_timer == speed / 2) {
      api.maybe_upload_next();
    }
    return;
  }
  _change_timer = speed;

  if (!--_cycle) {
    _cycle = cycles;
    if (!api.change_themes()) {
      _alternate = !_alternate;
    }
    api.change_font();
    // 1/3 chance after 32 * 48 = 1536 frames.
    // Average length 3 * 1536 = 4608 frames.
    if (api.change_visual(3)) {
      api.change_spiral();
    }
    ++_sub_speed_multiplier;
  }

  _current = api.get_image(_alternate);
  _text_on = !_text_on;
  if (_text_on) {
    api.change_text(VisualControl::SPLIT_WORD, _alternate);
  }
}

void SubTextVisual::render(VisualRender& api) const
{
  api.render_animation_or_image(
      _alternate ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, {}, 1, 10.f);
  api.render_image(_current, .8f, 8.f, 2.f - float(_change_timer) / speed);
  api.render_subtext(1.f / 4);
  api.render_spiral();
  if (_text_on) {
    api.render_text();
  }
}

SlowFlashVisual::SlowFlashVisual(VisualControl& api)
: _flash{false}
, _anim{false}
, _current{api.get_image()}
, _change_timer{max_speed}
, _image_count{cycle_length}
, _cycle_count{set_length}
{
  api.change_text(VisualControl::SPLIT_LINE);
}

void SlowFlashVisual::update(VisualControl& api)
{
  api.rotate_spiral(_flash ? 4.f : 2.f);

  if (_change_timer % 16 == 0 || (_flash && _change_timer % 4 == 0)) {
    api.change_small_subtext(true, _flash);
  }
  if (--_change_timer) {
    if (!_flash && _change_timer == max_speed / 2) {
      api.maybe_upload_next();
    }
    return;
  }

  if (!--_image_count) {
    _flash = !_flash;
    _image_count = _flash ? 2 * cycle_length : cycle_length;
    api.change_spiral();
    api.change_font();
    if (!--_cycle_count) {
      _cycle_count = set_length;
      api.change_themes();
      // 1/2 chance after 2 * (16 * 64 + 64 * 4) = 2560 frames.
      // Average length 2 * 2560 = 5120 frames.
      api.change_visual(2);
    }
  }

  _change_timer = _flash ? min_speed : max_speed;
  _anim = !_anim;
  _current = api.get_image(_flash);
  if (!_flash || _image_count % 2) {
    api.change_text(_flash ? VisualControl::SPLIT_WORD : VisualControl::SPLIT_LINE, _flash);
  }
}

void SlowFlashVisual::render(VisualRender& api) const
{
  float extra = 8.f - 8.f * _image_count / (4 * cycle_length);
  auto zoom = 1.5f *
      (1.f -
       (_flash ? float(max_speed - min_speed + _change_timer) : float(_change_timer)) / max_speed);
  api.render_animation_or_image(
      _anim && !_flash ? VisualRender::Anim::ANIM : VisualRender::Anim::NONE, _current, 1,
      8.f + extra, zoom);
  api.render_spiral();
  api.render_small_subtext(1.f / 5);
  if ((!_flash && _change_timer < max_speed / 2) || (_flash && _image_count % 2)) {
    api.render_text(_flash ? 3.f + 8.f * (_image_count / (4.f * cycle_length)) : 4.f);
  }
}

FlashTextVisual::FlashTextVisual(VisualControl& api)
: _animated{random_chance()}
, _start{api.get_image()}
, _end{api.get_image()}
, _timer{length}
, _font_timer{font_length}
, _cycle{cycles}
{
}

void FlashTextVisual::update(VisualControl& api)
{
  api.rotate_spiral(2.5f);

  if (!--_font_timer) {
    api.change_font(true);
    _font_timer = font_length;
  }

  if (_timer == length / 2 && _cycle % 2 == 0) {
    api.change_text(VisualControl::SPLIT_LINE, (_cycle / 2) % 2);
  }

  if (!--_timer) {
    if (!--_cycle) {
      _cycle = cycles;
      api.change_themes();
      // 1/8 chance after 64 * 8 = 512 frames.
      // Average length 8 * 512 = 4096 frames.
      api.change_visual(8);
    }

    _start = _end;
    _end = api.get_image((_cycle / 2) % 2);
    _timer = length;
  }
  if (_timer % 16 == 0) {
    api.change_small_subtext();
  }

  if (_timer == length / 2) {
    api.maybe_upload_next();
  }
}

void FlashTextVisual::render(VisualRender& api) const
{
  float extra = 32.f * _timer / length;
  float zoom = float(_timer) / length;
  api.render_animation_or_image(
      !_animated || _cycle % 2
          ? VisualRender::Anim::NONE
          : (_cycle / 2) % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM,
      _start, 1, 8.f + extra, (_animated ? 1.5f : 1.f) * (2.f - zoom));

  api.render_animation_or_image(
      !_animated || _cycle % 2 == 0
          ? VisualRender::Anim::NONE
          : (_cycle / 2) % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM,
      _end, 1.f - float(_timer) / length, 40.f - extra, (_animated ? 1.5f : 1.f) * (1.f - zoom));

  api.render_spiral();
  api.render_small_subtext(1.f / 5);
  if (_cycle % 2) {
    api.render_text(3.f + 4.f * _timer / length);
  }
}

ParallelVisual::ParallelVisual(VisualControl& api)
: _image{api.get_image()}
, _alternate{api.get_image(true)}
, _anim_cycle{0}
, _alternate_anim_cycle{0}
, _length{0}
, _alternate_length{length / 2}
, _switch_alt{false}
, _text_on{true}
, _timer{length}
, _cycle{cycles}
{
  api.change_text(VisualControl::SPLIT_LINE, random_chance());
}

void ParallelVisual::update(VisualControl& api)
{
  ++_length;
  ++_alternate_length;
  api.rotate_spiral(3.f);
  if (_timer % 8 == 0) {
    api.change_small_subtext();
  }
  if (--_timer) {
    if (_timer == length / 2) {
      api.maybe_upload_next();
    }
    return;
  }
  _timer = length;

  if (!--_cycle) {
    api.change_spiral();
    api.change_font();
    api.change_themes();
    _cycle = cycles;
    // 1/2 chance after 32 * 64 = 2048 frames.
    // Average length 2 * 2048 = 4096 frames.
    api.change_visual(2);
  }

  _switch_alt = !_switch_alt;
  if (_switch_alt) {
    _alternate = api.get_image(true);
    ++_alternate_anim_cycle;
    _alternate_length = 0;
  } else {
    _image = api.get_image(false);
    ++_anim_cycle;
    _length = 0;
  }
  if (_cycle % 4 == 2) {
    api.change_text(VisualControl::SPLIT_LINE, random_chance());
  }
}

void ParallelVisual::render(VisualRender& api) const
{
  float extra = 32.f * _cycle / cycles;
  auto anim = _anim_cycle % 3 == 2 ? VisualRender::Anim::ANIM : VisualRender::Anim::NONE;
  api.render_animation_or_image(anim, _image, 1, 8.f + extra, float(_length) / length);

  auto alt_anim = _alternate_anim_cycle % 3 == 1 ? VisualRender::Anim::ANIM_ALTERNATE
                                                 : VisualRender::Anim::NONE;
  api.render_animation_or_image(alt_anim, _alternate, .5f, 8 + 32.f - extra,
                                1.5f * float(_alternate_length) / length);

  api.render_spiral();
  api.render_small_subtext(1.f / 5);
  if (_cycle % 4 == 1 || _cycle % 4 == 2) {
    api.render_text();
  }
}

SuperParallelVisual::SuperParallelVisual(VisualControl& api) : _timer{length}, _cycle{cycles}
{
  api.change_text(VisualControl::SPLIT_WORD, random_chance());
  for (std::size_t i = 0; i < image_count; ++i) {
    _images.push_back(api.get_image(i % 2 == 0));
    _lengths.push_back(((image_count * length) - i * length) % (image_count * length));
  }
}

void SuperParallelVisual::update(VisualControl& api)
{
  for (std::size_t i = 0; i < image_count; ++i) {
    ++_lengths[i];
    if (_lengths[i] == image_count * length) {
      _images[i] = api.get_image(i % 2 == 0);
      _lengths[i] = 0;
    }
  }
  api.rotate_spiral(3.5f);

  if (--_timer) {
    return;
  }
  _timer = length;
  api.change_text(VisualControl::SPLIT_WORD, random_chance());

  if (!--_cycle) {
    api.change_spiral();
    api.change_font();
    api.change_themes();
    _cycle = cycles;
    // 1/4 chance after 32 * 32 = 1024 frames.
    // Average length 4 * 1024 = 4096 frames.
    api.change_visual(4);
  }

  if (_cycle % 16 == 0) {
    api.maybe_upload_next();
  }
}

void SuperParallelVisual::render(VisualRender& api) const
{
  float extra = 16.f - 16.f * (_cycle % 128) / (cycles / 4);
  bool single = false;
  for (uint32_t l : _lengths) {
    if (l >= image_count * length - length / 2) {
      single = true;
    }
  }
  for (std::size_t i = 0; i < image_count; ++i) {
    auto anim = i >= _images.size() / 2
        ? VisualRender::Anim::NONE
        : i % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;
    if (single && _lengths[i] < image_count * length - length / 2) {
      continue;
    }
    api.render_animation_or_image(anim, _images[i], single ? 1.f : 1.f / (1 + i),
                                  8.f + 4 * i + extra,
                                  4.f * float(_lengths[i]) / (image_count * length));
  }
  api.render_spiral();
  if (_timer > length / 2) {
    api.render_text(5.f);
  }
}

AnimationVisual::AnimationVisual(VisualControl& api)
: _animation_backup{api.get_image()}, _current{api.get_image(true)}, _timer{length}, _cycle{cycles}
{
  api.change_text(VisualControl::SPLIT_LINE);
}

void AnimationVisual::update(VisualControl& api)
{
  api.rotate_spiral(3.5f);
  if (_timer % image_length == 0) {
    _current = api.get_image((_timer / image_length) % 2 == 0);
  }
  if (_timer % (animation_length / 4) == 0) {
    _animation_backup = api.get_image();
  }
  if (_timer % 16 == 0) {
    api.change_small_subtext(true, random_chance());
  }

  if (--_timer) {
    if (_timer % 128 == 0) {
      api.maybe_upload_next();
    }
    if (_timer % 128 == 63) {
      api.change_text(VisualControl::SPLIT_LINE);
    }
    return;
  }
  _timer = length;

  if (!--_cycle) {
    api.change_spiral();
    api.change_font();
    api.change_themes();
    _cycle = cycles;
    // 1/4 chance after 256 * 4 = 1024 frames.
    // Average length 4 * 1024 = 4096 frames.
    api.change_visual(4);
  }
}

void AnimationVisual::render(VisualRender& api) const
{
  auto which_anim = (_timer / animation_length) % 2 ? VisualRender::Anim::ANIM
                                                    : VisualRender::Anim::ANIM_ALTERNATE;
  api.render_animation_or_image(which_anim, _animation_backup, 1.f,
                                20.f - 8.f * float(_timer % animation_length) / animation_length,
                                4.f - 4.f * float(_timer % animation_length) / animation_length);
  api.render_image(_current, .2f, 20.f - 8.f * float(_timer % image_length) / image_length,
                   1.f - float(_timer % image_length) / image_length);
  api.render_spiral();
  api.render_small_subtext(1.f / 5);
  if (_timer % 128 >= 64) {
    api.render_text(5.f);
  }
}

SuperFastVisual::SuperFastVisual(VisualControl& api)
: _current{api.get_image()}
, _start_timer{0}
, _animation_timer{anim_length + random(anim_length)}
, _alternate{false}
, _timer{length}
{
  api.change_text(VisualControl::SPLIT_WORD);
}

void SuperFastVisual::update(VisualControl& api)
{
  api.rotate_spiral(3.f);
  if (_animation_timer) {
    --_animation_timer;
    return;
  }
  if (_timer % image_length == 0) {
    _current = api.get_image(_alternate);
    api.change_text(VisualControl::SPLIT_WORD, _alternate);
  }
  if (!_start_timer && random_chance(128)) {
    _alternate = !_alternate;
    _animation_timer = anim_length + random(anim_length);
    _start_timer = nonanim_length;
  }
  if (_start_timer) {
    --_start_timer;
  }
  if (--_timer) {
    return;
  }

  api.change_spiral();
  api.change_font();
  api.change_themes();
  _timer = length;
  // Roughly 1024 + ~ 1024 * (128 + 128 / 2) / 256 (ignoring the _start_timer).
  // 1/2 chance after ~1792 frames.
  // Average length 2 * 1792 = 3584 frames.
  api.change_visual(2);
}

void SuperFastVisual::render(VisualRender& api) const
{
  if (_animation_timer) {
    api.render_animation_or_image(
        _alternate ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, _current, 1.f,
        20.f - 8.f * float(_animation_timer) / anim_length,
        4.f - 2.f * float(_animation_timer) / anim_length);
  } else {
    api.render_image(_current, 1.f, 20.f - 12.f * float(_timer % image_length) / image_length,
                     1.f - float(_timer % image_length) / image_length);
    if (_timer % image_length < 2) {
      api.render_text(5.f);
    }
  }
  api.render_spiral();
}