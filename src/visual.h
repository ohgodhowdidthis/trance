#ifndef TRANCE_VISUAL_H
#define TRANCE_VISUAL_H
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "image.h"

class Cycler;
class VisualControl;
class VisualRender;

// Interface to an object which can render and control the visual state.
// These visuals are swapped out by the Director every so often for different
// styles.
class Visual
{
public:
  virtual ~Visual() = default;
  virtual void update(VisualControl& api) = 0;
  virtual void render(VisualRender& api) const = 0;
};

class AccelerateVisual : public Visual
{
public:
  AccelerateVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  static const uint32_t max_speed = 48;
  static const uint32_t min_speed = 4;
  static const uint32_t text_time = 4;

  Image _current;
  bool _text_on;
  uint32_t _image_count;
  uint32_t _change_timer;
  uint32_t _change_speed;
  uint32_t _change_speed_timer;
  uint32_t _text_timer;
};

class SubTextVisual : public Visual
{
public:
  SubTextVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  std::shared_ptr<Cycler> _cycler;
  std::function<void(VisualRender& api)> _render;
  Image _current;
  bool _alternate;
  uint32_t _sub_speed_multiplier;
};

class SlowFlashVisual : public Visual
{
public:
  SlowFlashVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  std::shared_ptr<Cycler> _cycler;
  std::function<void(VisualRender& api)> _render;
  Image _current;
};

class FlashTextVisual : public Visual
{
public:
  FlashTextVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  const bool _animated;
  bool _alternate;
  std::shared_ptr<Cycler> _cycler;
  std::function<void(VisualRender& api)> _render;
  Image _start;
  Image _end;
};

class ParallelVisual : public Visual
{
public:
  ParallelVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

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
  uint32_t _timer;
  uint32_t _cycle;
};

class SuperParallelVisual : public Visual
{
public:
  SuperParallelVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  static const std::size_t image_count = 3;
  static const uint32_t length = 32;
  static const uint32_t cycles = 32;

  std::vector<Image> _images;
  std::vector<uint32_t> _lengths;
  uint32_t _timer;
  uint32_t _font_timer;
  uint32_t _cycle;
};

class AnimationVisual : public Visual
{
public:
  AnimationVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  static const uint32_t length = 256;
  static const uint32_t cycles = 4;
  static const uint32_t image_length = 16;
  static const uint32_t animation_length = 128;

  Image _animation_backup;
  Image _current;
  uint32_t _timer;
  uint32_t _cycle;
};

class SuperFastVisual : public Visual
{
public:
  SuperFastVisual(VisualControl& api);
  void update(VisualControl& api) override;
  void render(VisualRender& api) const override;

private:
  static const uint32_t length = 1024;
  static const uint32_t anim_length = 64;
  static const uint32_t nonanim_length = 64;
  static const uint32_t image_length = 8;

  Image _current;
  uint32_t _start_timer;
  uint32_t _animation_timer;
  bool _alternate;
  uint32_t _timer;
};

#endif