#include "visual.h"
#include "director.h"
#include "util.h"
#include "visual_api.h"
#include "visual_cyclers.h"

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

SubTextVisual::SubTextVisual(VisualControl& api) : _alternate{false}, _sub_speed_multiplier{0}
{
  api.change_text(VisualControl::SPLIT_WORD);

  auto oneshot = new ActionCycler{[&] {
    if (!api.change_themes()) {
      _alternate = !_alternate;
    }
    api.change_font();
    api.change_spiral();
    ++_sub_speed_multiplier;
  }};
  auto maybe_upload_next = new ActionCycler{48, 24, [&] { api.maybe_upload_next(); }};
  auto image = new ActionCycler{48, [&] { _current = api.get_image(_alternate); }};

  auto text_reset =
      new ActionCycler{4, [&] { api.change_text(VisualControl::SPLIT_WORD, _alternate); }};
  auto text = new ActionCycler{4, [&] { api.change_text(VisualControl::SPLIT_ONCE_ONLY); }};
  auto text_loop = new SequenceCycler{{text_reset, new RepeatCycler{23, text}}};

  auto sub0 = new ActionCycler{12, [&] {
                                 if (_sub_speed_multiplier == 1) {
                                   api.change_subtext(_alternate);
                                 }
                               }};
  auto sub1 = new ActionCycler{24, [&] {
                                 if (_sub_speed_multiplier == 2) {
                                   api.change_subtext(_alternate);
                                 }
                               }};
  auto sub2 = new ActionCycler{48, [&] {
                                 if (_sub_speed_multiplier >= 3) {
                                   api.change_subtext(_alternate);
                                 }
                               }};

  auto loop = new ParallelCycler{{image, maybe_upload_next, text_loop, sub0, sub1, sub2}};
  auto main = new RepeatCycler{16, loop};

  auto spiral = new ActionCycler{[&] { api.rotate_spiral(4.f); }};
  _cycler.reset(new ParallelCycler{{spiral, new OneShotCycler{{oneshot, main}}}});

  _render = [=](VisualRender& api) {
    api.render_animation_or_image(
        _alternate ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, {}, 1, 10.f);
    api.render_image(_current, .8f, 8.f, 1.f + image->progress());
    api.render_subtext(1.f / 4);
    api.render_spiral();
    api.render_text();
  };
}

void SubTextVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/3 chance after 32 * 48 = 1536 frames.
    // Average length 3 * 1536 = 4608 frames.
    api.change_visual(3);
  }
}

void SubTextVisual::render(VisualRender& api) const
{
  _render(api);
}

SlowFlashVisual::SlowFlashVisual(VisualControl& api)
{
  auto slow_loop = new ActionCycler{64, [&] {
                                      _current = api.get_image();
                                      api.change_text(VisualControl::SPLIT_LINE);
                                    }};
  auto slow_oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
  }};
  auto slow_repeat = new RepeatCycler{16, slow_loop};
  auto slow_main = new OneShotCycler{{slow_oneshot, slow_repeat}};
  auto slow_subtext = new ActionCycler{16, [&] { api.change_small_subtext(true); }};
  auto slow_spiral = new ActionCycler{[&] { api.rotate_spiral(2.f); }};
  auto slow_upload = new ActionCycler{64, 32, [&] { api.maybe_upload_next(); }};
  auto slow_cycler = new ParallelCycler{{slow_main, slow_subtext, slow_spiral, slow_upload}};

  auto fast_image = new ActionCycler{8, [&] { _current = api.get_image(true); }};
  auto fast_text =
      new ActionCycler{16, 8, [&] { api.change_text(VisualControl::SPLIT_WORD, true); }};
  auto fast_loop = new ParallelCycler{{fast_image, fast_text}};
  auto fast_oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
  }};
  auto fast_main = new OneShotCycler{{fast_oneshot, new RepeatCycler{32, fast_loop}}};
  auto fast_subtext = new ActionCycler{4, 0, [&] { api.change_small_subtext(true, true); }};
  auto fast_spiral = new ActionCycler{[&] { api.rotate_spiral(4.f); }};
  auto fast_cycler = new ParallelCycler{{fast_main, fast_subtext, fast_spiral}};

  auto oneshot = new ActionCycler{[&] { api.change_themes(); }};
  auto main_repeat = new RepeatCycler{2, new SequenceCycler{{slow_cycler, fast_cycler}}};
  _cycler.reset(new OneShotCycler{{oneshot, main_repeat}});

  _render = [=](VisualRender& api) {
    auto extra =
        slow_loop->active() ? 6.f + 2.f * slow_main->progress() : 4.f + 4.f * fast_main->progress();
    auto zoom =
        slow_loop->active() ? 1.5f * slow_loop->progress() : 1.5f * .25f * fast_image->progress();
    api.render_animation_or_image(slow_loop->active() && slow_repeat->index() % 2
                                      ? VisualRender::Anim::ANIM
                                      : VisualRender::Anim::NONE,
                                  _current, 1, 8.f + extra, zoom);
    api.render_spiral();
    api.render_small_subtext(1.f / 5);
    if (slow_loop->active() && slow_loop->frame() >= slow_loop->length() / 2) {
      api.render_text(4.f);
    }
    if (fast_cycler->active() && fast_text->frame() >= fast_text->length() / 2) {
      api.render_text(11.f - 4.f * fast_main->progress());
    }
  };
}

void SlowFlashVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/2 chance after 2 * (16 * 64 + 32 * 8) = 2560 frames.
    // Average length 2 * 2560 = 5120 frames.
    api.change_visual(2);
  }
}

void SlowFlashVisual::render(VisualRender& api) const
{
  _render(api);
}

FlashTextVisual::FlashTextVisual(VisualControl& api)
: _animated{random_chance()}, _end{api.get_image(true)}, _alternate{true}
{
  auto image = new ActionCycler{64, [&] {
                                  _start = _end;
                                  _end = api.get_image(_alternate);
                                }};

  auto alternate = new ActionCycler{[&] {
    _alternate = !_alternate;
    api.change_text(VisualControl::SPLIT_LINE, _alternate);
  }};
  auto image_repeat = new RepeatCycler{2, image};
  auto main_oneshot = new OneShotCycler{{alternate, image_repeat}};
  auto main_repeat = new RepeatCycler{8, main_oneshot};

  auto spiral = new ActionCycler{[&] { api.rotate_spiral(2.5f); }};
  auto font = new ActionCycler{64, [&] { api.change_font(true); }};
  auto small_subtext = new ActionCycler{16, [&] {}};
  auto upload = new ActionCycler{64, 32, [&] { api.maybe_upload_next(); }};

  auto parallel = new ParallelCycler{{spiral, font, small_subtext, upload, main_repeat}};
  auto oneshot = new ActionCycler{[&] {
    api.change_themes();
    api.change_spiral();
  }};

  _cycler.reset(new OneShotCycler{{parallel, oneshot}});

  _render = [=](VisualRender& api) {
    auto progress = image->progress();
    auto anim =
        main_repeat->index() % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;

    api.render_animation_or_image(
        !_animated || !image_repeat->index() ? VisualRender::Anim::NONE : anim, _start, 1.f,
        40.f - 32.f * progress, (_animated ? 1.5f : 1.f) * (1.f + progress));

    api.render_animation_or_image(
        !_animated || image_repeat->index() ? VisualRender::Anim::NONE : anim, _end,
        image->progress(), 8.f + 32.f * progress, (_animated ? 1.5f : 1.f) * progress);

    api.render_spiral();
    api.render_small_subtext(1.f / 5);
    if ((image_repeat->index())) {
      api.render_text(7.f - 4.f * image->progress());
    }
  };
}

void FlashTextVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/4 chance after 64 * 16 = 1024 frames.
    // Average length 4 * 1024 = 4096 frames.
    api.change_visual(3);
  }
}

void FlashTextVisual::render(VisualRender& api) const
{
  _render(api);
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