#ifndef TRANCE_SRC_TRANCE_DIRECTOR_H
#define TRANCE_SRC_TRANCE_DIRECTOR_H
#include <trance/render/render.h>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#pragma warning(push, 0)
#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#pragma warning(pop)

namespace trance_pb
{
  class Program;
  class Session;
  class System;
}

class Font;
class Image;
class ThemeBank;
class Visual;
class VisualApiImpl;
class Director
{
public:
  Director(const trance_pb::Session& session, const trance_pb::System& system, ThemeBank& themes,
           const trance_pb::Program& program, Renderer& renderer);
  ~Director();

  // Called from play_session() in main.cpp.
  void set_program(const trance_pb::Program& program);
  bool update();
  void render() const;

  const trance_pb::Program& program() const;
  bool vr_enabled() const;

  void render_spiral(float spiral, uint32_t spiral_width, uint32_t spiral_type) const;
  void render_image(const Image& image, float alpha, float zoom_origin, float zoom) const;

  sf::Vector2f text_size(const Font& font, const std::string& text, bool large) const;
  void render_text(const Font& font, const std::string& text, bool large, const sf::Color& colour,
                   float scale, const sf::Vector2f& offset, float zoom_origin, float zoom) const;

private:
  void change_visual(uint32_t length);
  float far_plane_distance() const;
  float eye_offset() const;

  const trance_pb::Session& _session;
  const trance_pb::System& _system;
  ThemeBank& _themes;
  const trance_pb::Program* _program;

  GLuint _new_program;
  GLuint _spiral_program;
  GLuint _quad_buffer;

  mutable Renderer::State _render_state;
  Renderer& _renderer;
  std::unique_ptr<VisualApiImpl> _visual_api;
  std::unique_ptr<Visual> _visual;
  std::unique_ptr<Visual> _old_visual;
};

#endif
