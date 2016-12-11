#ifndef TRANCE_VISUAL_API_H
#define TRANCE_VISUAL_API_H
#include <string>
#include <vector>
#include "font.h"

enum class SplitType {
  NONE = 0,
  WORD = 1,
  LINE = 2,
};
std::vector<std::string> SplitWords(const std::string& text, SplitType type);

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
  virtual Image get_image(bool alternate = false) const = 0;
  virtual const std::string& get_text(bool alternate = false) const = 0;
  virtual void maybe_upload_next() const = 0;

  virtual void rotate_spiral(float amount) = 0;
  virtual void change_spiral() = 0;
  virtual void change_font(bool force = false) = 0;
  virtual void change_subtext(bool alternate = false) = 0;
  virtual void change_small_subtext(bool force = false, bool alternate = false) = 0;
  virtual bool change_themes() = 0;
  virtual bool change_visual(uint32_t chance) = 0;
};

class VisualRender
{
public:
  enum class Anim {
    NONE,
    ANIM,
    ANIM_ALTERNATE,
  };
  virtual void render_animation_or_image(Anim type, const Image& image, float alpha,
                                         float multiplier = 8.f, float zoom = 0.f) const = 0;
  virtual void
  render_image(const Image& image, float alpha, float multiplier = 8.f, float zoom = 0.f) const = 0;
  virtual void render_text(const std::string& text, float multiplier = 4.f) const = 0;
  virtual void render_subtext(float alpha, float multiplier = 6.f) const = 0;
  virtual void render_small_subtext(float alpha, float multiplier = 6.f) const = 0;
  virtual void render_spiral() const = 0;
};

class VisualApiImpl : public VisualControl, public VisualRender
{
public:
  VisualApiImpl(Director& director, ThemeBank& themes, const trance_pb::Session& session,
                const trance_pb::System& system);
  void update();

  Image get_image(bool alternate = false) const override;
  const std::string& get_text(bool alternate = false) const override;
  void maybe_upload_next() const override;

  void rotate_spiral(float amount) override;
  void change_spiral() override;
  void change_font(bool force = false) override;
  void change_subtext(bool alternate = false) override;
  void change_small_subtext(bool force = false, bool alternate = false) override;
  bool change_themes() override;
  bool change_visual(uint32_t chance) override;

  void render_animation_or_image(Anim type, const Image& image, float alpha, float multiplier = 8.f,
                                 float zoom = 0.f) const override;
  void render_image(const Image& image, float alpha, float multiplier = 8.f,
                    float zoom = 0.f) const override;
  void render_text(const std::string& text, float multiplier = 4.f) const override;
  void render_subtext(float alpha, float multiplier = 6.f) const override;
  void render_small_subtext(float alpha, float multiplier = 6.f) const override;
  void render_spiral() const override;

private:
  uint32_t get_cached_text_size(const FontCache& cache, const std::string& text,
                                const std::string& font) const;
  sf::Vector2f get_text_size(const std::string& text, const Font& font) const;

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

  std::unordered_map<std::string, uint32_t> _text_size_cache;
};

#endif