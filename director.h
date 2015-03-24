#ifndef TRANCE_DIRECTOR_H
#define TRANCE_DIRECTOR_H

#include <windows.h>
#undef min
#undef max

#include <cstddef>
#include <memory>
#include <vector>
#include <GL/glew.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include "fonts.h"

namespace trance_pb {
  class Session;
  class Program;
}

struct Image;
class ThemeBank;
class Visual;
class Director {
public:

  Director(sf::RenderWindow& window, const trance_pb::Session& session,
           ThemeBank& themes, const std::vector<std::string>& fonts,
           unsigned int width, unsigned int height);
  ~Director();

  // Called from main().
  float get_frame_time() const;
  void update();
  void render() const;

  // Visual API: called from Visual objects to render and control the
  // various elements.
  Image get(bool alternate = false) const;
  const std::string& get_text(bool alternate = false) const;
  void maybe_upload_next() const;

  void render_image(const Image& image, float alpha,
                    float multiplier = 8.f, float zoom = 0.f) const;
  void render_text(const std::string& text, float multiplier = 4.f) const;
  void render_subtext(float alpha, float multiplier = 6.f) const;
  void render_spiral(float multiplier = 0.f) const;

  void rotate_spiral(float amount);
  void change_spiral();
  void change_font(bool force = false);
  void change_subtext(bool alternate = false);
  bool change_themes();
  void change_visual();

private:

  const trance_pb::Program& program() const;
  void init_oculus_rift();
  sf::Vector2f off3d(float multiplier) const;
  unsigned int view_width() const;

  void render_texture(float l, float t, float r, float b,
                      bool flip_h, bool flip_v) const;
  void render_raw_text(const std::string& text, const Font& font,
                       const sf::Color& colour, const sf::Vector2f& offset = {},
                       float scale = 1.f) const;
  sf::Vector2f get_text_size(const std::string& text, const Font& font) const;

  sf::RenderWindow& _window;
  const trance_pb::Session& _session;
  ThemeBank& _themes;
  FontCache _fonts;
  unsigned int _width;
  unsigned int _height;

  struct {
    bool enabled;
    ovrHmd hmd;

    unsigned int fbo;
    unsigned int fb_tex;
    unsigned int fb_depth;

    union ovrGLConfig gl_cfg;
    ovrGLTexture fb_ovr_tex[2];
    ovrEyeRenderDesc eye_desc[2];

    mutable bool rendering_right;
  } _oculus;

  GLuint _image_program;
  GLuint _spiral_program;
  GLuint _text_program;
  GLuint _quad_buffer;
  GLuint _tex_buffer;

  float _spiral;
  unsigned int _spiral_type;
  unsigned int _spiral_width;
  std::string _current_font;
  std::string _current_subfont;
  std::vector<std::string> _subtext;

  unsigned int _switch_themes;
  std::unique_ptr<Visual> _visual;
  std::unique_ptr<Visual> _old_visual;

};

#endif