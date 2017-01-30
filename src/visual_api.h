#ifndef TRANCE_VISUAL_API_H
#define TRANCE_VISUAL_API_H
#include <string>
#include <vector>
#include "font.h"

class Director;
class Image;
class ThemeBank;
namespace trance_pb
{
  class Session;
  class System;
}

class VisualControl
{
public:
  virtual ~VisualControl() = default;

  enum SplitType {
    SPLIT_WORD = 0,
    SPLIT_LINE = 1,
    SPLIT_WORD_GAPS = 2,
    SPLIT_LINE_GAPS = 3,
    SPLIT_ONCE_ONLY = 4,
  };

  virtual Image get_image(bool alternate = false) const = 0;
  virtual void maybe_upload_next() const = 0;

  virtual void rotate_spiral(float amount) = 0;
  virtual void change_spiral() = 0;
  virtual void change_font(bool force = false) = 0;
  virtual void change_text(SplitType split_type, bool alternate = false) = 0;
  virtual void change_subtext(bool alternate = false) = 0;
  virtual void change_small_subtext(bool force = false, bool alternate = false) = 0;
  virtual bool change_themes() = 0;
};

class VisualRender
{
public:
  virtual ~VisualRender() = default;

  enum class Anim {
    NONE,
    ANIM,
    ANIM_ALTERNATE,
  };
  virtual void render_animation_or_image(Anim type, const Image& image, float alpha,
                                         float zoom_origin, float zoom) const = 0;
  virtual void
  render_image(const Image& image, float alpha, float zoom_origin, float zoom) const = 0;
  virtual void
  render_text(float zoom_origin, float zoom, float shadow_zoom_origin, float shadow_zoom) const = 0;
  virtual void render_subtext(float alpha, float zoom_origin) const = 0;
  virtual void render_small_subtext(float alpha, float zoom_origin) const = 0;
  virtual void render_spiral() const = 0;
};

class VisualApiImpl : public VisualControl, public VisualRender
{
public:
  VisualApiImpl(Director& director, ThemeBank& themes, const trance_pb::Session& session,
                const trance_pb::System& system, uint32_t height_pixels);
  void update();

  Image get_image(bool alternate = false) const override;
  void maybe_upload_next() const override;

  void rotate_spiral(float amount) override;
  void change_spiral() override;
  void change_font(bool force = false) override;
  void change_text(SplitType split_type, bool alternate = false) override;
  void change_subtext(bool alternate = false) override;
  void change_small_subtext(bool force = false, bool alternate = false) override;
  bool change_themes() override;

  void render_animation_or_image(Anim type, const Image& image, float alpha, float zoom_origin,
                                 float zoom) const override;
  void render_image(const Image& image, float alpha, float zoom_origin, float zoom) const override;
  void render_text(float zoom_origin, float zoom, float shadow_zoom_origin,
                   float shadow_zoom) const override;
  void render_subtext(float alpha, float zoom_origin) const override;
  void render_small_subtext(float alpha, float zoom_origin) const override;
  void render_spiral() const override;

private:
  float zoom_intensity(float zoom_origin, float zoom) const;

  Director& _director;
  ThemeBank& _themes;
  FontCache _font_cache;

  uint32_t _switch_themes;
  float _spiral;
  uint32_t _spiral_type;
  uint32_t _spiral_width;
  std::string _current_font;
  std::string _current_subfont;
  std::vector<std::string> _subtext;
  std::string _small_subtext;
  float _small_subtext_x;
  float _small_subtext_y;

  std::vector<std::string> _current_text;
};

#endif