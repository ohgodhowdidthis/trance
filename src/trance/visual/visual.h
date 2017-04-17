#ifndef TRANCE_SRC_TRANCE_VISUAL_VISUAL_H
#define TRANCE_SRC_TRANCE_VISUAL_VISUAL_H
#include <common/media/image.h>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

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
  virtual void reset() {}
  Cycler* cycler();
  void render(VisualRender& api) const;

protected:
  void set_cycler(Cycler* cycler);
  void set_render(const std::function<void(VisualRender& api)>& function);

private:
  std::shared_ptr<Cycler> _cycler;
  std::function<void(VisualRender& api)> _render;
};

class AccelerateVisual : public Visual
{
public:
  AccelerateVisual(VisualControl& api);

private:
  bool _alternate_animation;
  bool _text_on;
  Image _current;
};

class SubTextVisual : public Visual
{
public:
  SubTextVisual(VisualControl& api);

private:
  Image _current;
  bool _alternate;
  uint32_t _sub_speed_multiplier;
};

class SlowFlashVisual : public Visual
{
public:
  SlowFlashVisual(VisualControl& api);

private:
  Image _current;
};

class FlashTextVisual : public Visual
{
public:
  FlashTextVisual(VisualControl& api);
  void reset() override;

private:
  bool _animated;
  bool _alternate;
  Image _start;
  Image _end;
};

class ParallelVisual : public Visual
{
public:
  ParallelVisual(VisualControl& api);

private:
  uint32_t _anim_cycle;
  uint32_t _alternate_anim_cycle;
  Image _image;
  Image _alternate;
};

class SuperParallelVisual : public Visual
{
public:
  SuperParallelVisual(VisualControl& api);

private:
  bool _alternate_animation;
  std::vector<Image> _images;
};

class AnimationVisual : public Visual
{
public:
  AnimationVisual(VisualControl& api);

private:
  Image _animation_backup;
  Image _current;
};

class SuperFastVisual : public Visual
{
public:
  SuperFastVisual(VisualControl& api);

private:
  bool _alternate;
  uint32_t _cooldown_timer;
  uint32_t _animation_timer;
  Image _current;
};

#endif