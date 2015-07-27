#ifndef TRANCE_VISUAL_H
#define TRANCE_VISUAL_H

#include <cstddef>
#include "theme.h"

class Director;

// Interface to an object which can render and control the visual state.
// These visuals are swapped out by the Director every so often for different
// styles.
// TODO: fix missing images after speed-up change (maybe only in export mode?).
class Visual {
public:

  Visual(Director& director);
  virtual ~Visual() {}

  virtual void update() = 0;
  virtual void render() const = 0;

protected:

  const Director& director() const;
  Director& director();

private:

  Director& _director;

};

class AccelerateVisual : public Visual {
public:

  AccelerateVisual(Director& director, bool start_fast);
  void update() override;
  void render() const override;

private:

  static const uint32_t max_speed = 48;
  static const uint32_t min_speed = 0;
  static const uint32_t text_time = 4;

  Image _current;
  std::string _current_text;

  bool _text_on;
  uint32_t _change_timer;
  uint32_t _change_speed;
  uint32_t _change_speed_timer;
  uint32_t _text_timer;
  bool _change_faster;

};

class SubTextVisual : public Visual {
public:

  SubTextVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const uint32_t speed = 48;
  static const uint32_t sub_speed = 12;
  static const uint32_t cycles = 32;

  Image _current;
  std::string _current_text;
  bool _text_on;
  uint32_t _change_timer;
  uint32_t _sub_timer;
  uint32_t _cycle;
  uint32_t _sub_speed_multiplier;

};

class SlowFlashVisual : public Visual {
public:

  SlowFlashVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const uint32_t max_speed = 64;
  static const uint32_t min_speed = 4;
  static const uint32_t cycle_length = 16;
  static const uint32_t set_length = 4;

  Image _current;
  std::string _current_text;
  uint32_t _change_timer;

  bool _flash;
  bool _anim;
  uint32_t _image_count;
  uint32_t _cycle_count;

};

class FlashTextVisual : public Visual {
public:

  FlashTextVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const uint32_t length = 64;
  static const uint32_t font_length = 64;
  static const uint32_t cycles = 8;

  bool _animated;
  Image _start;
  Image _end;
  std::string _current_text;
  uint32_t _timer;
  uint32_t _cycle;
  uint32_t _font_timer;

};

class ParallelVisual : public Visual {
public:

  ParallelVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const uint32_t length = 32;
  static const uint32_t cycles = 64;

  Image _image;
  Image _alternate;
  uint32_t _anim_cycle;
  uint32_t _alternate_anim_cycle;
  uint32_t _length;
  uint32_t _alternate_length;
  bool _switch_alt;
  bool _text_on;
  std::string _current_text;
  uint32_t _timer;
  uint32_t _cycle;

};

class SuperParallelVisual : public Visual {
public:

  SuperParallelVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const std::size_t image_count = 4;
  static const uint32_t font_length = 64;
  static const uint32_t length = 2;
  static const uint32_t cycles = 512;

  std::vector<Image> _images;
  std::vector<uint32_t> _lengths;
  std::size_t _index;
  std::string _current_text;
  uint32_t _timer;
  uint32_t _font_timer;
  uint32_t _cycle;

};

class AnimationVisual : public Visual {
public:

  AnimationVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const uint32_t length = 256;
  static const uint32_t cycles = 4;

  Image _animation_backup;
  Image _current;
  std::string _current_text;
  uint32_t _timer;
  uint32_t _cycle;

};

class SuperFastVisual : public Visual {
public:

  SuperFastVisual(Director& director);
  void update() override;
  void render() const override;

private:

  static const uint32_t length = 1024;
  static const uint32_t anim_length = 128;
  static const uint32_t nonanim_lenth = 16;

  Image _current;
  std::string _current_text;
  uint32_t _start_timer;
  uint32_t _animation_timer;
  bool _animation_alt;
  uint32_t _timer;

};

#endif