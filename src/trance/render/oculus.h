#ifndef TRANCE_SRC_TRANCE_RENDER_OCULUS_H
#define TRANCE_SRC_TRANCE_RENDER_OCULUS_H
#include <trance/render/render.h>
#include <vector>

#pragma warning(push, 0)
#include <GL/glew.h>
#include <libovr/OVR_CAPI.h>
#include <libovr/OVR_CAPI_GL.h>
#pragma warning(pop)

class OculusRenderer : public Renderer
{
public:
  OculusRenderer(const trance_pb::System& system);
  ~OculusRenderer() override;
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
  bool _started;
  uint32_t _width;
  uint32_t _height;

  ovrSession _session;
  ovrGraphicsLuid _luid;
  ovrTextureSwapChain _texture_chain;
  std::vector<uint32_t> _fbo_ovr;
  ovrVector3f _eye_view_offset[2];
  ovrLayerEyeFov _layer;
};

#endif