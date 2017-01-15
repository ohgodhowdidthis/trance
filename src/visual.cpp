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
  auto small_subtext = new ActionCycler{16, [&] { api.change_small_subtext(); }};
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
: _anim_cycle{2}, _alternate_anim_cycle{0}, _image{api.get_image()}, _alternate{api.get_image(true)}
{
  auto counter = new RepeatCycler{4, new ActionCycler{32}};
  auto image = new ActionCycler{64, [&] {
                                  _image = api.get_image(false);
                                  ++_anim_cycle;
                                }};
  auto image_alt = new ActionCycler{64, [&] {
                                      _alternate = api.get_image(true);
                                      ++_alternate_anim_cycle;
                                    }};
  auto image_alt_off = new OffsetCycler{32, image_alt};

  auto text =
      new ActionCycler{128, [&] { api.change_text(VisualControl::SPLIT_LINE, random_chance()); }};

  auto main = new ParallelCycler{{counter, image, image_alt_off, text}};
  auto repeat = new RepeatCycler{16, main};

  auto spiral = new ActionCycler{[&] { api.rotate_spiral(3.f); }};
  auto small_subtext = new ActionCycler{8, [&] { api.change_small_subtext(); }};
  auto upload = new ActionCycler{32, 16, [&] { api.maybe_upload_next(); }};

  auto parallel = new ParallelCycler{{spiral, small_subtext, upload, repeat}};
  auto oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
    api.change_themes();
  }};

  _cycler.reset(new OneShotCycler{{oneshot, parallel}});

  _render = [=](VisualRender& api) {
    auto anim = _anim_cycle % 3 == 2 ? VisualRender::Anim::ANIM : VisualRender::Anim::NONE;
    api.render_animation_or_image(anim, _image, 1, 40.f - 32.f * _cycler->progress(),
                                  image->progress());

    auto alt_anim = _alternate_anim_cycle % 3 == 1 ? VisualRender::Anim::ANIM_ALTERNATE
                                                   : VisualRender::Anim::NONE;
    api.render_animation_or_image(alt_anim, _alternate, .5f, 8 + 32.f * _cycler->progress(),
                                  1.5f * image_alt->progress());

    api.render_spiral();
    api.render_small_subtext(1.f / 5);
    if (counter->index() == 1 || counter->index() == 2) {
      api.render_text();
    }
  };
}

void ParallelVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/2 chance after 32 * 64 = 2048 frames.
    // Average length 2 * 2048 = 4096 frames.
    api.change_visual(2);
  }
}

void ParallelVisual::render(VisualRender& api) const
{
  _render(api);
}

SuperParallelVisual::SuperParallelVisual(VisualControl& api)
{
  std::vector<Cycler*> main_loops;
  std::vector<Cycler*> progress;
  std::vector<Cycler*> single;

  for (uint32_t i = 0; i < 3; ++i) {
    _images.push_back(api.get_image(i >= 2));
    auto set = new ActionCycler{80, [&, i] { _images[i] = api.get_image(i >= 2); }};
    single.push_back(new ActionCycler{16});
    progress.push_back(new SequenceCycler{{set, single.back()}});
    main_loops.push_back(new OffsetCycler{i * 32, progress.back()});
  }

  auto repeat = new RepeatCycler{12, new ParallelCycler{main_loops}};
  auto text =
      new ActionCycler{32, [&] { api.change_text(VisualControl::SPLIT_WORD, random_chance()); }};
  auto spiral = new ActionCycler{[&] { api.rotate_spiral(3.5f); }};
  auto upload = new ActionCycler{32, 16, [&] { api.maybe_upload_next(); }};

  auto parallel = new ParallelCycler{{spiral, upload, text, repeat}};
  auto oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
    api.change_themes();
  }};

  _cycler.reset(new OneShotCycler{{oneshot, parallel}});

  _render = [=](VisualRender& api) {
    bool is_single =
        std::any_of(single.begin(), single.end(), [](Cycler* cycler) { return cycler->active(); });

    for (std::size_t i = 0; i < _images.size(); ++i) {
      auto anim = i != 0
          ? VisualRender::Anim::NONE
          : repeat->index() % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;
      if (!is_single || single[i]->active()) {
        api.render_animation_or_image(anim, _images[i], is_single ? 1.f : 1.f / (1 + i),
                                      8.f + 4.f * i + 16.f * _cycler->progress(),
                                      4.f * progress[i]->progress());
      }
    }
    api.render_spiral();
    if (text->frame() < text->length() / 2) {
      api.render_text(5.f);
    }
  };
}

void SuperParallelVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/4 chance after 12 * 96 = 1152 frames.
    // Average length 4 * 1152 = 4608 frames.
    api.change_visual(4);
  }
}

void SuperParallelVisual::render(VisualRender& api) const
{
  _render(api);
}

AnimationVisual::AnimationVisual(VisualControl& api)
{
  auto image_timer = new ActionCycler{16};
  auto image = new ActionCycler{32, [&] {
                                  _animation_backup = api.get_image();
                                  _current = api.get_image();
                                }};
  auto image_alt = new ActionCycler{32, 16, [&] { _current = api.get_image(true); }};
  auto text = new ActionCycler{128, 0, [&] { api.change_text(VisualControl::SPLIT_LINE); }};
  auto text_alt =
      new ActionCycler{128, 0, [&] { api.change_text(VisualControl::SPLIT_LINE, true); }};
  auto text_both = new SequenceCycler{{text, text_alt}};
  auto text_counter = new RepeatCycler{2, new ActionCycler{64}};

  auto spiral = new ActionCycler{[&] { api.rotate_spiral(3.5f); }};
  auto small_subtext =
      new ActionCycler{16, [&] { api.change_small_subtext(true, random_chance()); }};
  auto upload = new ActionCycler{32, 24, [&] { api.maybe_upload_next(); }};

  auto parallel = new ParallelCycler{
      {spiral, small_subtext, upload, image, image_alt, image_timer, text_both, text_counter}};
  auto repeat = new RepeatCycler{4, parallel};
  auto oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
    api.change_themes();
  }};

  _cycler.reset(new OneShotCycler{{oneshot, repeat}});

  _render = [=](VisualRender& api) {
    auto which_anim =
        text_alt->active() ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;
    api.render_animation_or_image(which_anim, _animation_backup, 1.f,
                                  12.f + 8.f * parallel->progress(), 4.f * parallel->progress());
    api.render_image(_current, .2f, 12.f + 8.f * image_timer->progress(), image_timer->progress());
    api.render_spiral();
    api.render_small_subtext(1.f / 5);
    if (!text_counter->index()) {
      api.render_text(5.f);
    }
  };
}

void AnimationVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/4 chance after 128 * 8 = 1024 frames.
    // Average length 4 * 1024 = 4096 frames.
    api.change_visual(4);
  }
}

void AnimationVisual::render(VisualRender& api) const
{
  _render(api);
}

SuperFastVisual::SuperFastVisual(VisualControl& api)
: _alternate{false}, _cooldown_timer{0}, _animation_timer{0}
{
  auto length = new ActionCycler{2048};
  auto rapid = new ActionCycler{8, [&] {
                                  if (_animation_timer) {
                                    --_animation_timer;
                                  } else if (_cooldown_timer) {
                                    --_cooldown_timer;
                                  }
                                  if (!_animation_timer && !_cooldown_timer && random_chance(16)) {
                                    _alternate = !_alternate;
                                    _animation_timer = 8 + random(9);
                                    _cooldown_timer = 8;
                                  }
                                  if (!_animation_timer) {
                                    _current = api.get_image(_alternate);
                                    api.change_text(VisualControl::SPLIT_WORD, _alternate);
                                  }
                                }};
  auto spiral = new ActionCycler{[&] { api.rotate_spiral(3.f); }};

  auto parallel = new ParallelCycler{{length, spiral, rapid}};
  auto oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
    api.change_themes();
  }};

  _cycler.reset(new OneShotCycler{{oneshot, parallel}});

  _render = [=](VisualRender& api) {
    if (_animation_timer) {
      auto anim_progress = float(8 * (16 - _animation_timer) + rapid->frame()) / 128;
      api.render_animation_or_image(
          _alternate ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, _current, 1.f,
          4.f + 16.f * anim_progress, 4.f * anim_progress);
    } else {
      api.render_image(_current, 1.f, 8.f + 12.f * rapid->progress(), rapid->progress());
      if (rapid->frame() >= rapid->length() - 2) {
        api.render_text(5.f);
      }
    }
    api.render_spiral();
  };
}

void SuperFastVisual::update(VisualControl& api)
{
  _cycler->advance();
  if (_cycler->complete()) {
    // 1/2 chance after 2048 frames.
    // Average length 2 * 2048 = 4096 frames.
    api.change_visual(2);
  }
}

void SuperFastVisual::render(VisualRender& api) const
{
  _render(api);
}