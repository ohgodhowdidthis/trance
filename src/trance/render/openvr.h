#ifndef TRANCE_SRC_TRANCE_RENDER_OPENVR_H
#define TRANCE_SRC_TRANCE_RENDER_OPENVR_H
#include <trance/render/render.h>
#include <vector>

#pragma warning(push, 0)
#include <GL/glew.h>
#include <openvr/openvr.h>
#pragma warning(pop)

class OpenVrRenderer : public Renderer
{
public:
  OpenVrRenderer(const trance_pb::System& system);
  ~OpenVrRenderer() override;
  bool success() const;

  bool vr_enabled() const override;
  uint32_t view_width() const override;
  uint32_t width() const override;
  uint32_t height() const override;

  void init() override;
  bool update() override;
  void render(const std::function<void(State)>& render_fn) override;

private:
  bool _initialised;
  bool _success;
  uint32_t _width;
  uint32_t _height;

  vr::IVRSystem* _system;
  vr::IVRCompositor* _compositor;
  std::vector<uint32_t> _fbo;
  std::vector<uint32_t> _fb_tex;
};

#endif