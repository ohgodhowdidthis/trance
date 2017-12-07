#ifndef TRANCE_SRC_TRANCE_RENDER_RENDER_H
#define TRANCE_SRC_TRANCE_RENDER_RENDER_H
#include <functional>
#include <memory>
#include <string>

#pragma warning(push, 0)
#include <GL/glew.h>
#pragma warning(pop)

namespace sf
{
  class RenderWindow;
}
namespace trance_pb
{
  class System;
}

GLuint compile(const std::string& vertex_text, const std::string& fragment_text);
void init_glew();

class Renderer
{
public:
  // TODO: could factor out actual rendering to intermediate texture(s) and add multisampling?
  enum class State {
    NONE = 0,
    VR_LEFT = 1,
    VR_RIGHT = 2,
  };

  virtual ~Renderer() = default;

  sf::RenderWindow& window();
  virtual bool vr_enabled() const = 0;
  virtual bool is_openvr() const = 0;
  virtual uint32_t view_width() const = 0;
  virtual uint32_t width() const = 0;
  virtual uint32_t height() const = 0;
  virtual float eye_spacing_multiplier() const = 0;

  virtual void init() = 0;
  virtual bool update() = 0;
  virtual void render(const std::function<void(State)>& render_fn) = 0;

protected:
  std::unique_ptr<sf::RenderWindow> _window;
};

class ScreenRenderer : public Renderer
{
public:
  ScreenRenderer(const trance_pb::System& system);

  bool vr_enabled() const override;
  bool is_openvr() const override;
  uint32_t view_width() const override;
  uint32_t width() const override;
  uint32_t height() const override;
  float eye_spacing_multiplier() const override;

  void init() override;
  bool update() override;
  void render(const std::function<void(State)>& render_fn) override;
};

#endif