#include "program.h"
#include "director.h"
#include "util.h"

Program::Program(Director& director)
: _director{director}
{
}

const Director& Program::director() const
{
  return _director;
}

Director& Program::director()
{
  return _director;
}

AccelerateProgram::AccelerateProgram(Director& director, bool start_fast)
: Program{director}
, _current{director.get()}
, _current_text{director.get_text()}
, _text_on{true}
, _change_timer{start_fast ? min_speed : max_speed}
, _change_speed{start_fast ? min_speed : max_speed}
, _change_speed_timer{0}
, _change_faster{!start_fast}
{
}

void AccelerateProgram::update()
{
  unsigned long d = max_speed - _change_speed;
  unsigned long m = max_speed;

  float spiral_d = 1 + float(d) / 8;
  director().rotate_spiral(_change_faster ? spiral_d : -spiral_d);

  if (_change_timer) {
    --_change_timer;
    if (_change_timer == _change_speed / 2 && _change_speed > max_speed / 2) {
      director().maybe_upload_next();
    }
    return;
  }

  _change_timer = _change_speed;
  _current = director().get();
  _text_on = !_text_on;
  if (_change_speed > 4) {
    _current_text = director().get_text();
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
      if (director().change_sets()) {
        _current_text = director().get_text();
      }
    }
    director().change_font();
    if (random_chance()) {
      director().change_program();
    }
  }
}

void AccelerateProgram::render() const
{
  director().render_image(_current, 1, 8.f + 48.f - _change_speed);
  Image anim("");
  anim.anim_type = Image::ANIMATION;
  director().render_image(anim, .2f, 6.f);
  director().render_spiral();
  if (_change_speed <= 16 || _text_on) {
    director().render_text(_current_text);
  }
}

SubTextProgram::SubTextProgram(Director& director)
: Program{director}
, _current{director.get()}
, _current_text{director.get_text()}
, _text_on{true}
, _change_timer{speed}
, _sub_timer{sub_speed}
, _cycle{cycles}
, _sub_speed_multiplier{1}
{
}

void SubTextProgram::update()
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
  _current = director().get();
  _text_on = !_text_on;
  if (_text_on) {
    _current_text = director().get_text();
  }

  if (!--_cycle) {
    _cycle = cycles;
    if (director().change_sets()) {
      _current_text = director().get_text();
    }
    director().change_font();
    if (random_chance()) {
      director().change_program();
      director().change_spiral();
    }
    ++_sub_speed_multiplier;
  }
}

void SubTextProgram::render() const
{
  Image anim("");
  anim.anim_type = Image::ANIMATION;
  director().render_image(anim, 1, 10.f);
  director().render_image(_current, .8f);
  director().render_subtext(1.f / 4);
  director().render_spiral();
  if (_text_on) {
    director().render_text(_current_text);
  }
}

SlowFlashProgram::SlowFlashProgram(Director& director)
: Program{director}
, _current{director.get()}
, _current_text{director.get_text()}
, _change_timer{max_speed}
, _flash{false}
, _image_count{cycle_length}
, _cycle_count{set_length}
{
}

void SlowFlashProgram::update()
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
      director().change_sets();
      if (random_chance()) {
        director().change_program();
      }
    }
  }

  _change_timer = _flash ? min_speed : max_speed;
  bool anim = _current.anim_type != Image::NONE;
  _current = director().get(_flash);
  if (!_flash && !anim) {
    _current.anim_type = Image::ANIMATION;
  }
  if (!_flash) {
    _current_text = director().get_text(false);
  }
}

void SlowFlashProgram::render() const
{
  float extra = 32.f - 32.f * _image_count / (4 * cycle_length);
  director().render_image(_current, 1, 8.f + extra);
  director().render_spiral();
  if (_change_timer < max_speed / 2 || _flash) {
    director().render_text(
        _current_text,
        _flash ? 3.f + 8.f * (_image_count / (4.f * cycle_length)) : 4.f);
  }
}

FlashTextProgram::FlashTextProgram(Director& director)
: Program{director}
, _animated{random_chance()}
, _start{director.get()}
, _end{director.get()}
, _current_text{director.get_text()}
, _timer{length}
, _font_timer{font_length}
, _cycle{cycles}
{
  if (_animated) {
    _start.anim_type = Image::ANIMATION;
    _end.anim_type = Image::ALTERNATE_ANIMATION;
  }
}

void FlashTextProgram::update()
{
  director().rotate_spiral(2.5f);

  if (!--_font_timer) {
    director().change_font(true);
    _font_timer = font_length;
  }

  if (!--_timer) {
    if (!--_cycle) {
      _cycle = cycles;
      director().change_sets();
      _current_text = director().get_text();
      if (random_chance(4)) {
        director().change_program();
      }
    }
    _start = _end;
    _end = director().get();
    if (_animated) {
      _end.anim_type = _start.anim_type == Image::ANIMATION ?
          Image::ALTERNATE_ANIMATION : Image::ANIMATION;
    }
    _timer = length;
  }

  if (_timer == length / 2) {
    director().maybe_upload_next();
  }
}

void FlashTextProgram::render() const
{
  float extra = 32.f - 32.f * _timer / length;
  director().render_image(_start, 1, (_cycle % 2 ? 10.f : 8.f) + extra);
  director().render_image(_end, 1.f - float(_timer) / length,
                          (_cycle % 2 ? 8.f : 10.f) + extra);
  director().render_spiral();
  director().render_text(_current_text, 3.f + 8.f * _timer / length);
}

ParallelProgram::ParallelProgram(Director& director)
: Program{director}
, _image{director.get()}
, _alternate{director.get(true)}
, _anim_cycle{0}
, _alternate_anim_cycle{0}
, _switch_alt{false}
, _text_on{true}
, _current_text{director.get_text(random_chance())}
, _timer{length}
, _cycle{cycles}
{
}

void ParallelProgram::update()
{
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
    director().change_sets();
    _cycle = cycles;
    if (random_chance()) {
      director().change_program();
    }
  }

  _switch_alt = !_switch_alt;
  if (_switch_alt) {
    _alternate = director().get(true);
    ++_alternate_anim_cycle;
    if (_alternate_anim_cycle % 3 == 1) {
      _alternate.anim_type = Image::ALTERNATE_ANIMATION;
    }
  }
  else {
    _image = director().get(false);
    ++_anim_cycle;
    if (_anim_cycle % 3 == 2) {
      _image.anim_type = Image::ANIMATION;
    }
  }
  _text_on = !_text_on;
  if (_text_on) {
    _current_text = director().get_text(random_chance());
  }
}

void ParallelProgram::render() const
{
  float extra = 32.f * _cycle / cycles;
  director().render_image(_image, 1, 8 + extra);
  director().render_image(_alternate, .5f, 8 + 32.f - extra);
  director().render_spiral();
  if (!_text_on) {
    director().render_text(_current_text, 5.f);
  }
}

SuperParallelProgram::SuperParallelProgram(Director& director)
: Program{director}
, _index{0}
, _current_text{director.get_text(random_chance())}
, _timer{length}
, _font_timer{font_length}
, _cycle{cycles}
{
  for (std::size_t i = 0; i < image_count; ++i) {
    _images.push_back(director.get(i % 2 == 0));
  }
  _images.back().anim_type = Image::ANIMATION;
}

void SuperParallelProgram::update()
{
  director().rotate_spiral(3.5f);
  if (!--_font_timer) {
    _current_text = director().get_text(random_chance());
    _font_timer = font_length;
  }

  if (--_timer) {
    return;
  }
  _timer = length;

  if (!--_cycle) {
    director().change_spiral();
    director().change_font();
    director().change_sets();
    _cycle = cycles;
    if (random_chance()) {
      director().change_program();
    }
  }

  if (_cycle % 16 == 0) {
    director().maybe_upload_next();
  }

  _images[_index] = director().get(_index % 2 == 0);
  if (_index < _images.size() / 2) {
    _images[_index].anim_type = _index % 2 == 0 ?
        Image::ALTERNATE_ANIMATION : Image::ANIMATION;
  }
  _index = (_index + 1) % _images.size();
}

void SuperParallelProgram::render() const
{
  float extra = 16.f - 16.f * (_cycle % 128) / (cycles / 4);
  for (std::size_t i = 0; i < _images.size(); ++i) {
    director().render_image(_images[i], 1.f / (1 + i), 8.f + 4 * i + extra);
  }
  director().render_spiral();
  director().render_text(_current_text, 5.f);
}

AnimationProgram::AnimationProgram(Director& director)
: Program{director}
, _animation{director.get()}
, _current{director.get(true)}
, _current_text{director.get_text()}
, _timer{length}
, _cycle{cycles}
{
  _animation.anim_type = Image::ANIMATION;
}

void AnimationProgram::update()
{
  director().rotate_spiral(3.5f);
  _current = director().get(true);

  if (--_timer) {
    return;
  }
  _timer = length;

  if (!--_cycle) {
    director().change_spiral();
    director().change_font();
    director().change_sets();
    _cycle = cycles;
    _current_text = director().get_text();
    if (random_chance()) {
      director().change_program();
    }
  }

  if (_timer % 128 == 0) {
    director().maybe_upload_next();
  }
}

void AnimationProgram::render() const
{
  director().render_image(_animation, 1.f);
  director().render_image(_current, .2f, 12.f);
  director().render_spiral();
  if (_timer % 128 < 64) {
    director().render_text(_current_text, 5.f);
  }
}