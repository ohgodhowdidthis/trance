#include "visual.h"
#include <common/util.h>
#include <trance/director.h>
#include <trance/visual/api.h>
#include <trance/visual/cyclers.h>

void Visual::set_cycler(Cycler* cycler)
{
  _cycler.reset(cycler);
}

void Visual::set_render(const std::function<void(VisualRender& api)>& function)
{
  _render = function;
}

Cycler* Visual::cycler()
{
  return _cycler.get();
}

void Visual::render(VisualRender& api) const
{
  _render(api);
}

AccelerateVisual::AccelerateVisual(VisualControl& api) : _text_on{false}
{
  std::vector<Cycler*> main_image;
  std::vector<Cycler*> main_text;
  std::vector<Cycler*> main_sequence;

  for (uint32_t image_length = 48; image_length >= 4; --image_length) {
    bool last = image_length == 4;
    uint64_t d = 48 - image_length;
    auto image_count = 1 + (d * d * d * d * d * d) / (48 * 48 * 48 * 48 * 48);
    if (last) {
      image_count *= 2;
    }
    float spiral_speed = 1 + float(48 - image_length) / 16;
    bool alternate = (image_length / 12) % 2 == 0;
    bool text_word = image_length <= 8;

    auto spiral = new ActionCycler{[&, spiral_speed] { api.rotate_spiral(spiral_speed); }};
    main_image.push_back(
        new ActionCycler{image_length, [&, alternate] { _current = api.get_image(alternate); }});

    auto text_action = [&, alternate, text_word] {
      if (_text_on = !_text_on) {
        api.change_text(text_word ? VisualControl::SPLIT_WORD : VisualControl::SPLIT_LINE,
                        alternate);
      }
    };
    main_text.push_back(new ActionCycler{4, text_action});
    auto upload = image_length > 24
        ? new ActionCycler{image_length, image_length / 2, [&] { api.maybe_upload_next(); }}
        : new ActionCycler{image_length};

    auto parallel = new ParallelCycler{{main_image.back(), spiral, upload}};
    auto oneshot = new OneShotCycler{{parallel, main_text.back()}};
    main_sequence.push_back(new RepeatCycler{uint32_t(image_count), oneshot});
  }
  auto main = new SequenceCycler{main_sequence};

  auto oneshot = new ActionCycler{[&] {
    api.change_font();
    api.change_spiral();
    api.change_themes();
  }};

  set_cycler(new OneShotCycler{{oneshot, main}});
  set_render([=](VisualRender& api) {
    auto zoom_origin = .4f * main->progress();
    auto zoom = zoom_origin + .1f * main_image[main->index()]->progress();
    api.render_image(_current, 1.f, zoom_origin, zoom);
    api.render_animation_or_image(((48 - main->index()) / 12) % 2 == 0
                                      ? VisualRender::Anim::ANIM_ALTERNATE
                                      : VisualRender::Anim::ANIM,
                                  {}, .2f, 0, 0);

    api.render_spiral();
    if ((_text_on && main_text[main->index()]->active()) || 1 + main->index() == main_text.size()) {
      api.render_text(.6f + .2f * main->progress(), .6f + .2f * main->progress(), zoom, zoom);
    }
  });
}

SubTextVisual::SubTextVisual(VisualControl& api) : _alternate{false}, _sub_speed_multiplier{0}
{
  auto oneshot = new ActionCycler{[&] {
    api.change_themes();
    api.change_font();
    api.change_spiral();
    ++_sub_speed_multiplier;
  }};
  auto maybe_upload_next = new ActionCycler{48, 24, [&] { api.maybe_upload_next(); }};
  auto image = new ActionCycler{48, [&] {
                                  _current = api.get_image(_alternate);
                                  _alternate = !_alternate;
                                }};

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
  set_cycler(new ParallelCycler{{spiral, new OneShotCycler{{oneshot, main}}}});

  set_render([=](VisualRender& api) {
    api.render_animation_or_image(
        _alternate ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, {}, 1, 0, 0);
    auto image_zoom = .375f * image->progress();
    api.render_image(_current, .8f, 0, image_zoom);
    api.render_subtext(1.f / 4, image_zoom);
    api.render_spiral();
    api.render_text(.75f, .75f, image_zoom, image_zoom);
  });
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
  auto fast_repeat = new RepeatCycler{32, fast_loop};
  auto fast_main = new OneShotCycler{{fast_oneshot, fast_repeat}};
  auto fast_subtext = new ActionCycler{4, 0, [&] { api.change_small_subtext(true, true); }};
  auto fast_spiral = new ActionCycler{[&] { api.rotate_spiral(4.f); }};
  auto fast_cycler = new ParallelCycler{{fast_main, fast_subtext, fast_spiral}};

  auto oneshot = new ActionCycler{[&] { api.change_themes(); }};
  auto main_repeat = new RepeatCycler{2, new SequenceCycler{{slow_cycler, fast_cycler}}};
  set_cycler(new OneShotCycler{{oneshot, main_repeat}});

  set_render([=](VisualRender& api) {
    auto zoom_origin =
        slow_loop->active() ? .25f * slow_main->progress() : fast_repeat->index() / 48.f;
    auto zoom = slow_loop->active() ? .25f * slow_main->progress() + .5f * slow_loop->progress()
                                    : (fast_repeat->index() + 8.f * fast_loop->progress()) / 48.f;
    api.render_animation_or_image(slow_loop->active() && slow_repeat->index() % 2
                                      ? VisualRender::Anim::ANIM
                                      : VisualRender::Anim::NONE,
                                  _current, 1, zoom_origin, zoom);
    api.render_spiral();
    api.render_small_subtext(1.f / 5, .5f);
    if (slow_loop->active() && slow_loop->frame() >= slow_loop->length() / 2) {
      api.render_text(.8f, .8f, zoom, zoom);
    }
    if (fast_cycler->active() && fast_text->frame() >= fast_text->length() / 2) {
      api.render_text(7.f / 8, 1.f - fast_main->progress() / 8.f, 7.f / 8,
                      1.f - fast_main->progress() / 8.f);
    }
  });
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

  set_cycler(new OneShotCycler{{parallel, oneshot}});

  set_render([=](VisualRender& api) {
    auto progress = image->progress();
    auto anim =
        main_repeat->index() % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;

    api.render_animation_or_image(
        !_animated || !image_repeat->index() ? VisualRender::Anim::NONE : anim, _start, 1.f, 0,
        .4f * (1 + progress));

    api.render_animation_or_image(
        !_animated || image_repeat->index() ? VisualRender::Anim::NONE : anim, _end,
        image->progress(), 0, .4f * progress);

    api.render_spiral();
    api.render_small_subtext(1.f / 5, .25f);
    if ((image_repeat->index())) {
      api.render_text(.85f - .05f * progress, .9f - .1f * progress, .75f, .8f - .05f * progress);
    }
  });
}

void FlashTextVisual::reset()
{
  _animated = random_chance();
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

  auto whole = new OneShotCycler{{oneshot, parallel}};
  set_cycler(whole);

  set_render([=](VisualRender& api) {
    auto anim = _anim_cycle % 3 == 2 ? VisualRender::Anim::ANIM : VisualRender::Anim::NONE;
    api.render_animation_or_image(anim, _image, 1, 0, .5f * image->progress());

    auto alt_anim = _alternate_anim_cycle % 3 == 1 ? VisualRender::Anim::ANIM_ALTERNATE
                                                   : VisualRender::Anim::NONE;
    api.render_animation_or_image(alt_anim, _alternate, .5f, 0, .75f * image_alt->progress());

    api.render_spiral();
    api.render_small_subtext(1.f / 5, .25f);
    if (counter->index() == 1 || counter->index() == 2) {
      api.render_text(.75f, .75f, .5f * image->progress(), .5f * image->progress());
    }
  });
}

SuperParallelVisual::SuperParallelVisual(VisualControl& api)
{
  std::vector<Cycler*> main_loops;
  std::vector<Cycler*> progress;
  std::vector<Cycler*> single;

  for (uint32_t i = 0; i < 3; ++i) {
    _images.push_back(api.get_image(i >= 2));
    auto set = new ActionCycler{16, [&, i] { _images[i] = api.get_image(i >= 2); }};
    single.push_back(new ActionCycler{16});
    progress.push_back(new SequenceCycler{{set, single.back(), new ActionCycler{64}}});
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

  auto main = new OneShotCycler{{oneshot, parallel}};
  set_cycler(main);

  set_render([=](VisualRender& api) {
    bool is_single =
        std::any_of(single.begin(), single.end(), [](Cycler* cycler) { return cycler->active(); });

    for (std::size_t i = 0; i < _images.size(); ++i) {
      auto anim = i != 0
          ? VisualRender::Anim::NONE
          : repeat->index() % 2 ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;
      if (!is_single || single[i]->active()) {
        auto zoom_origin = .125f * main->progress();
        api.render_animation_or_image(anim, _images[i], is_single ? 1.f : 1.f / (1 + i),
                                      zoom_origin, zoom_origin + .875f * progress[i]->progress());
      }
    }
    api.render_spiral();
    if (text->frame() < text->length() / 2) {
      api.render_text(.875f, .875f, .75f, .75f);
    }
  });
}

AnimationVisual::AnimationVisual(VisualControl& api)
{
  auto image_timer = new ActionCycler{16};
  auto zoom_timer = new ActionCycler{128};
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

  auto parallel = new ParallelCycler{{spiral, small_subtext, upload, image, image_alt, image_timer,
                                      zoom_timer, text_both, text_counter}};
  auto repeat = new RepeatCycler{4, parallel};
  auto oneshot = new ActionCycler{[&] {
    api.change_spiral();
    api.change_font();
    api.change_themes();
  }};

  set_cycler(new OneShotCycler{{oneshot, repeat}});

  set_render([=](VisualRender& api) {
    auto which_anim =
        text_alt->active() ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM;
    auto image_zoom = 1.25f * zoom_timer->progress();
    api.render_animation_or_image(which_anim, _animation_backup, 1.f, 0, image_zoom);
    api.render_image(_current, .2f, .5f, .5f + .25f * image_timer->progress());
    api.render_spiral();
    api.render_small_subtext(1.f / 5, .5f);
    if (!text_counter->index()) {
      api.render_text(.75f, .75f, .5f, .5f);
    }
  });
}

SuperFastVisual::SuperFastVisual(VisualControl& api)
: _alternate{false}, _cooldown_timer{0}, _animation_timer{0}
{
  auto length = new ActionCycler{2048};
  auto rapid = new ActionCycler{8, [&] {
                                  if (_animation_timer) {
                                    if (--_animation_timer == 4) {
                                      api.maybe_upload_next();
                                    }
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

  set_cycler(new OneShotCycler{{oneshot, parallel}});

  set_render([=](VisualRender& api) {
    if (_animation_timer) {
      auto anim_progress = float(8 * (16 - _animation_timer) + rapid->frame()) / 128;
      api.render_animation_or_image(
          _alternate ? VisualRender::Anim::ANIM_ALTERNATE : VisualRender::Anim::ANIM, _current, 1.f,
          0.f, anim_progress);
    } else {
      auto image_zoom = .1875f * rapid->progress();
      api.render_image(_current, 1.f, 0.f, image_zoom);
      if (rapid->frame() >= rapid->length() - 2) {
        api.render_text(.75f, .75f, image_zoom, image_zoom);
      }
    }
    api.render_spiral();
  });
}