#include "video_export.h"
#include <common/util.h>
#include <trance/media/export.h>
#include <trance/shaders.h>
#include <iostream>

#pragma warning(push, 0)
#include <GL/glew.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#pragma warning(pop)

VideoExportRenderer::VideoExportRenderer(const exporter_settings& settings)
: _settings{settings}
, _render_fbo{0}
, _render_fb_tex{0}
, _yuv_fbo{0}
, _yuv_fb_tex{0}
, _quad_buffer{0}
{
  _window.reset(new sf::RenderWindow);
  _window->setVisible(false);
  _window->setActive(true);
  init_glew();

  if (ext_is(settings.path, "jpg") || ext_is(settings.path, "png") ||
      ext_is(settings.path, "bmp")) {
    _exporter = std::make_unique<FrameExporter>(settings);
  }
  if (ext_is(settings.path, "webm")) {
    _exporter = std::make_unique<WebmExporter>(settings);
  }
  if (ext_is(settings.path, "h264")) {
    _exporter = std::make_unique<H264Exporter>(settings);
  }
  if (!_exporter || !_exporter->success()) {
    std::cerr << "don't know how to export that format" << std::endl;
    _exporter.reset();
  }

  static const float quad_data[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f,
                                    1.f,  -1.f, 1.f, 1.f,  -1.f, 1.f};
  glGenBuffers(1, &_quad_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, quad_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  _yuv_program = compile(yuv_vertex, yuv_fragment);

  init_framebuffer(_render_fbo, _render_fb_tex, settings.width, settings.height);
  init_framebuffer(_yuv_fbo, _yuv_fb_tex, settings.width, settings.height);
  _screen_data.reset(new uint8_t[4 * settings.width * settings.height]);
}

VideoExportRenderer::~VideoExportRenderer()
{
  glDeleteBuffers(1, &_quad_buffer);
}

bool VideoExportRenderer::vr_enabled() const
{
  return _settings.export_3d;
}

bool VideoExportRenderer::is_openvr() const
{
  return false;
}

uint32_t VideoExportRenderer::view_width() const
{
  return _settings.export_3d ? _settings.width / 2 : _settings.width;
}

uint32_t VideoExportRenderer::width() const
{
  return _settings.width;
}

uint32_t VideoExportRenderer::height() const
{
  return _settings.height;
}

float VideoExportRenderer::eye_spacing_multiplier() const
{
  return 1.f;
}

void VideoExportRenderer::init()
{
}

bool VideoExportRenderer::update()
{
  return true;
}

void VideoExportRenderer::render(const std::function<void(State)>& render_fn)
{
  if (_settings.export_3d) {
    glBindFramebuffer(GL_FRAMEBUFFER, _render_fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int eye = 0; eye < 2; ++eye) {
      glViewport(eye * view_width(), 0, view_width(), _settings.height);
      render_fn(eye ? State::VR_RIGHT : State::VR_LEFT);
    }
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, _render_fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    render_fn(State::NONE);
  }

  // Could do more on the GPU e.g. scaling, splitting planes, but the VP8
  // encoding is the bottleneck anyway.
  glBindFramebuffer(GL_FRAMEBUFFER, _yuv_fbo);
  glViewport(0, 0, width(), height());
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_TEXTURE_2D);
  glUseProgram(_yuv_program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _render_fb_tex);

  glUniform1i(glGetUniformLocation(_yuv_program, "source"), 0);
  glUniform1f(glGetUniformLocation(_yuv_program, "yuv_mix"),
              _exporter->requires_yuv_input() ? 1.f : 0.f);
  glUniform2f(glGetUniformLocation(_yuv_program, "resolution"), float(width()), float(height()));
  auto loc = glGetAttribLocation(_yuv_program, "position");
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glVertexAttribPointer(loc, 2, GL_FLOAT, false, 0, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, _yuv_fb_tex);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, _screen_data.get());

  _exporter->encode_frame(_screen_data.get());
}

bool VideoExportRenderer::init_framebuffer(uint32_t& fbo, uint32_t& fb_tex, uint32_t width,
                                           uint32_t height) const
{
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &fb_tex);

  glBindTexture(GL_TEXTURE_2D, fb_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glBindTexture(GL_TEXTURE_2D, fb_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_tex, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "framebuffer failed" << std::endl;
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}