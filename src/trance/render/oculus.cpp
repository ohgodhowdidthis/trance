#include "oculus.h"
#include <common/util.h>
#include <algorithm>
#include <iostream>

#pragma warning(push, 0)
#include <GL/glew.h>
#include <common/trance.pb.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#pragma warning(pop)

namespace
{
  void print_oculus_error()
  {
    ovrErrorInfo info;
    ovr_GetLastErrorInfo(&info);
    std::cerr << info.ErrorString << std::endl;
  }
}

OculusRenderer::OculusRenderer(const trance_pb::System& system)
: _initialised{false}, _success{false}, _started{false}, _width{0}, _height{0}, _session{nullptr}
{
  if (ovr_Initialize(nullptr) != ovrSuccess) {
    std::cerr << "Oculus initialization failed" << std::endl;
    print_oculus_error();
    return;
  }
  _initialised = true;

  _window.reset(new sf::RenderWindow);
  _window->create({}, "trance", sf::Style::None);
  _window->setVerticalSyncEnabled(system.enable_vsync());
  _window->setFramerateLimit(0);
  _window->setVisible(false);
  _window->setActive(true);

  init_glew();

  if (ovr_Create(&_session, &_luid) != ovrSuccess) {
    std::cerr << "Oculus session failed" << std::endl;
    print_oculus_error();
    return;
  }
  auto desc = ovr_GetHmdDesc(_session);
  ovr_SetBool(_session, "QueueAheadEnabled", ovrFalse);

  auto xfov = std::max({desc.DefaultEyeFov[0].LeftTan, desc.DefaultEyeFov[0].RightTan,
                        desc.DefaultEyeFov[1].LeftTan, desc.DefaultEyeFov[1].RightTan});
  auto yfov = std::max({desc.DefaultEyeFov[0].DownTan, desc.DefaultEyeFov[0].UpTan,
                        desc.DefaultEyeFov[1].DownTan, desc.DefaultEyeFov[1].UpTan});
  ovrFovPort fov;
  fov.LeftTan = fov.RightTan = xfov;
  fov.DownTan = fov.UpTan = yfov;

  ovrSizei eye_left = ovr_GetFovTextureSize(_session, ovrEye_Left, fov, 1.0);
  ovrSizei eye_right = ovr_GetFovTextureSize(_session, ovrEye_Right, fov, 1.0);
  int fw = eye_left.w + eye_right.w;
  int fh = std::max(eye_left.h, eye_right.h);

  ovrTextureSwapChainDesc texture_chain_desc;
  texture_chain_desc.Type = ovrTexture_2D;
  texture_chain_desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
  texture_chain_desc.ArraySize = 1;
  texture_chain_desc.Width = fw;
  texture_chain_desc.Height = fh;
  texture_chain_desc.MipLevels = 0;
  texture_chain_desc.SampleCount = 1;
  texture_chain_desc.StaticImage = false;
  texture_chain_desc.MiscFlags = ovrTextureMisc_None;
  texture_chain_desc.BindFlags = 0;

  auto result = ovr_CreateTextureSwapChainGL(_session, &texture_chain_desc, &_texture_chain);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain failed" << std::endl;
    print_oculus_error();
  }
  int texture_count = 0;
  result = ovr_GetTextureSwapChainLength(_session, _texture_chain, &texture_count);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain length failed" << std::endl;
    print_oculus_error();
  }
  for (int i = 0; i < texture_count; ++i) {
    GLuint fbo;
    GLuint fb_tex = 0;
    result = ovr_GetTextureSwapChainBufferGL(_session, _texture_chain, i, &fb_tex);
    if (result != ovrSuccess) {
      std::cerr << "Oculus texture swap chain buffer failed" << std::endl;
      print_oculus_error();
    }

    glGenFramebuffers(1, &fbo);
    _fbo_ovr.push_back(fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "framebuffer failed" << std::endl;
      return;
    }
  }

  auto erd_left = ovr_GetRenderDesc(_session, ovrEye_Left, desc.DefaultEyeFov[0]);
  auto erd_right = ovr_GetRenderDesc(_session, ovrEye_Right, desc.DefaultEyeFov[1]);
  _eye_view_offset[0] = erd_left.HmdToEyeOffset;
  _eye_view_offset[1] = erd_right.HmdToEyeOffset;

  _layer.Header.Type = ovrLayerType_EyeFov;
  _layer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  _layer.ColorTexture[0] = _texture_chain;
  _layer.ColorTexture[1] = _texture_chain;
  _layer.Fov[0] = erd_left.Fov;
  _layer.Fov[1] = erd_right.Fov;
  _layer.Viewport[0].Pos.x = 0;
  _layer.Viewport[0].Pos.y = 0;
  _layer.Viewport[0].Size.w = fw / 2;
  _layer.Viewport[0].Size.h = fh;
  _layer.Viewport[1].Pos.x = fw / 2;
  _layer.Viewport[1].Pos.y = 0;
  _layer.Viewport[1].Size.w = fw / 2;
  _layer.Viewport[1].Size.h = fh;

  _width = fw;
  _height = fh;
  _success = true;
}

OculusRenderer::~OculusRenderer()
{
  for (auto fbo : _fbo_ovr) {
    glDeleteFramebuffers(1, &fbo);
  }
  if (_session) {
    ovr_Destroy(_session);
  }
  if (_initialised) {
    ovr_Shutdown();
  }
}

bool OculusRenderer::success() const
{
  return _success;
}

bool OculusRenderer::vr_enabled() const
{
  return true;
}

bool OculusRenderer::is_openvr() const
{
  return false;
}

uint32_t OculusRenderer::view_width() const
{
  return _width / 2;
}

uint32_t OculusRenderer::width() const
{
  return _width;
}

uint32_t OculusRenderer::height() const
{
  return _height;
}

float OculusRenderer::eye_spacing_multiplier() const
{
  return 1.f;
}

void OculusRenderer::init()
{
}

bool OculusRenderer::update()
{
  ovrSessionStatus status;
  auto result = ovr_GetSessionStatus(_session, &status);
  if (result != ovrSuccess) {
    std::cerr << "Oculus session status failed" << std::endl;
  }
  if (status.ShouldQuit) {
    return false;
  }
  if (status.DisplayLost) {
    std::cerr << "Oculus display lost" << std::endl;
  }
  if (status.ShouldRecenter) {
    if (ovr_RecenterTrackingOrigin(_session) != ovrSuccess) {
      ovr_ClearShouldRecenterFlag(_session);
    }
  }
  _started = status.HmdPresent && !status.DisplayLost;
  if (!status.IsVisible && random_chance(1024)) {
    std::cerr << "Lost focus (move the HMD?)" << std::endl;
  }
  return true;
}

void OculusRenderer::render(const std::function<void(State)>& render_fn)
{
  if (!_started) {
    return;
  }

  auto timing = ovr_GetPredictedDisplayTime(_session, 0);
  auto sensorTime = ovr_GetTimeInSeconds();
  auto tracking = ovr_GetTrackingState(_session, timing, true);
  ovr_CalcEyePoses(tracking.HeadPose.ThePose, _eye_view_offset, _layer.RenderPose);

  int index = 0;
  auto result = ovr_GetTextureSwapChainCurrentIndex(_session, _texture_chain, &index);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain index failed" << std::endl;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, _fbo_ovr[index]);
  glClear(GL_COLOR_BUFFER_BIT);

  for (int eye = 0; eye < 2; ++eye) {
    const auto& view = _layer.Viewport[eye];
    glViewport(view.Pos.x, view.Pos.y, view.Size.w, view.Size.h);
    render_fn(eye == ovrEye_Right ? State::VR_RIGHT : State::VR_LEFT);
  }

  result = ovr_CommitTextureSwapChain(_session, _texture_chain);
  if (result != ovrSuccess) {
    std::cerr << "Oculus commit texture swap chain failed" << std::endl;
  }

  _layer.SensorSampleTime = sensorTime;
  const ovrLayerHeader* layers = &_layer.Header;
  result = ovr_SubmitFrame(_session, 0, nullptr, &layers, 1);
  if (result != ovrSuccess && result != ovrSuccess_NotVisible) {
    std::cerr << "Oculus submit frame failed" << std::endl;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}