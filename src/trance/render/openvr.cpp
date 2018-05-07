#include <trance/render/openvr.h>
#include <common/util.h>
#include <algorithm>
#include <iostream>

#pragma warning(push, 0)
#include <common/trance.pb.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#pragma warning(pop)

OpenVrRenderer::OpenVrRenderer(const trance_pb::System& system)
: _initialised{false}
, _success{false}
, _width{0}
, _height{0}
, _system{nullptr}
, _compositor{nullptr}
{
  vr::HmdError error;
  _system = vr::VR_Init(&error, vr::VRApplication_Scene);
  if (!_system || error != vr::VRInitError_None) {
    std::cerr << "OpenVR initialization failed" << std::endl;
    std::cerr << vr::VR_GetVRInitErrorAsEnglishDescription(error) << std::endl;
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

  _compositor = vr::VRCompositor();
  if (!_compositor) {
    std::cerr << "OpenVR compositor failed" << std::endl;
    return;
  }

  _system->GetRecommendedRenderTargetSize(&_width, &_height);
  for (int i = 0; i < 2; ++i) {
    GLuint fbo;
    GLuint fb_tex;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fb_tex);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "framebuffer failed" << std::endl;
      return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    _fbo.push_back(fbo);
    _fb_tex.push_back(fb_tex);
  }
  _success = true;
}

OpenVrRenderer::~OpenVrRenderer()
{
  for (auto fb_tex : _fb_tex) {
    glDeleteTextures(1, &fb_tex);
  }
  for (auto fbo : _fbo) {
    glDeleteFramebuffers(1, &fbo);
  }
  if (_initialised) {
    vr::VR_Shutdown();
  }
}

bool OpenVrRenderer::success() const
{
  return _success;
}

bool OpenVrRenderer::vr_enabled() const
{
  return true;
}

bool OpenVrRenderer::is_openvr() const
{
  return true;
}

uint32_t OpenVrRenderer::view_width() const
{
  return _width;
}

uint32_t OpenVrRenderer::width() const
{
  // TODO: ???
  return _width;
}

uint32_t OpenVrRenderer::height() const
{
  return _height;
}

float OpenVrRenderer::eye_spacing_multiplier() const
{
  // TODO: ???
  return 150.f;
}

void OpenVrRenderer::init()
{
}

bool OpenVrRenderer::update()
{
  vr::VREvent_t event;
  while (_system->PollNextEvent(&event, sizeof(event))) {
    // Ignore.
  }
  return true;
}

void OpenVrRenderer::render(const std::function<void(State)>& render_fn)
{
  static vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
  auto error = vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount,
                                                nullptr, 0);
  if (error != vr::VRCompositorError_None) {
    std::cerr << "compositor wait failed: " << static_cast<uint32_t>(error) << std::endl;
  }

  for (int eye = 0; eye < 2; ++eye) {
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo[eye]);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, _width, _height);
    render_fn(eye ? State::VR_RIGHT : State::VR_LEFT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  vr::Texture_t left = {(void*) (uintptr_t) _fbo[0], vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
  vr::Texture_t right = {(void*) (uintptr_t) _fbo[1], vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
  error = vr::VRCompositor()->Submit(vr::Eye_Left, &left);
  if (error != vr::VRCompositorError_None) {
    std::cerr << "compositor submit failed: " << static_cast<uint32_t>(error) << std::endl;
  }
  error = vr::VRCompositor()->Submit(vr::Eye_Right, &right);
  if (error != vr::VRCompositorError_None) {
    std::cerr << "compositor submit failed: " << static_cast<uint32_t>(error) << std::endl;
  }
  vr::VRCompositor()->PostPresentHandoff();
}