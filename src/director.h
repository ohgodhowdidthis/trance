#ifndef TRANCE_DIRECTOR_H
#define TRANCE_DIRECTOR_H

#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>
#include "font.h"

#pragma warning(push, 0)
#include <GL/glew.h>
#include <libovr/OVR_CAPI.h>
#include <libovr/OVR_CAPI_GL.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#pragma warning(pop)

namespace trance_pb
{
  class Program;
  class Session;
  class System;
}

struct Font;
class Image;
class ThemeBank;
class Visual;
class Director
{
public:
  Director(sf::RenderWindow& window, const trance_pb::Session& session,
           const trance_pb::System& system, ThemeBank& themes, const trance_pb::Program& program,
           bool realtime, bool oculus_rift, bool convert_to_yuv);
  ~Director();

  // Called from play_session() in main.cpp.
  void set_program(const trance_pb::Program& program);
  bool update();
  void render() const;
  // Returns screen data only in non-realtime mode.
  const uint8_t* get_screen_data() const;

  // Visual API: called from Visual objects to render and control the
  // various elements.
  Image get_image(bool alternate = false) const;
  const std::string& get_text(bool alternate = false) const;
  void maybe_upload_next() const;

  enum class Anim {
    NONE,
    ANIM,
    ANIM_ALTERNATE,
  };
  void render_animation_or_image(Anim type, const Image& image, float alpha, float multiplier = 8.f,
                                 float zoom = 0.f) const;
  void
  render_image(const Image& image, float alpha, float multiplier = 8.f, float zoom = 0.f) const;
  void render_text(const std::string& text, float multiplier = 4.f) const;
  void render_subtext(float alpha, float multiplier = 6.f) const;
  void render_small_subtext(float alpha, float multiplier = 6.f) const;
  void render_spiral() const;

  void rotate_spiral(float amount);
  void change_spiral();
  void change_font(bool force = false);
  void change_subtext(bool alternate = false);
  void change_small_subtext(bool force = false, bool alternate = false);
  bool change_themes();
  bool change_visual(uint32_t chance);

private:
  bool init_framebuffer(uint32_t& fbo, uint32_t& fb_tex, uint32_t width, uint32_t height) const;
  bool init_oculus_rift();
  sf::Vector2f off3d(float multiplier, bool text) const;
  uint32_t view_width() const;

  void render_texture(float l, float t, float r, float b, bool flip_h, bool flip_v) const;
  void render_raw_text(const std::string& text, const Font& font, const sf::Color& colour,
                       const sf::Vector2f& offset = {}, float scale = 1.f) const;
  uint32_t get_cached_text_size(const FontCache& cache, const std::string& text,
                                const std::string& font) const;
  sf::Vector2f get_text_size(const std::string& text, const Font& font) const;

  sf::RenderWindow& _window;
  const trance_pb::Session& _session;
  const trance_pb::System& _system;
  ThemeBank& _themes;
  FontCache _fonts;
  uint32_t _width;
  uint32_t _height;
  const trance_pb::Program* _program;

  bool _realtime;
  bool _convert_to_yuv;
  uint32_t _render_fbo;
  uint32_t _render_fb_tex;
  uint32_t _yuv_fbo;
  uint32_t _yuv_fb_tex;
  std::unique_ptr<uint8_t[]> _screen_data;

  struct {
    bool enabled;
    bool started;
    ovrSession session;
    ovrGraphicsLuid luid;
    ovrTextureSwapChain texture_chain;
    std::vector<uint32_t> fbo_ovr;
    ovrVector3f eye_view_offset[2];

    mutable ovrLayerEyeFov layer;
    mutable bool rendering_right;
  } _oculus;

  GLuint _image_program;
  GLuint _spiral_program;
  GLuint _text_program;
  GLuint _yuv_program;
  GLuint _quad_buffer;
  GLuint _tex_buffer;

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
  mutable std::vector<uint32_t> _recent_images;

  uint32_t _switch_themes;
  std::unique_ptr<Visual> _visual;
  std::unique_ptr<Visual> _old_visual;
};

#endif