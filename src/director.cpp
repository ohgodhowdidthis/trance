#include "director.h"
#include <algorithm>
#include <iostream>
#include "font.h"
#include "session.h"
#include "theme.h"
#include "util.h"
#include "visual.h"
#include "visual_api.h"
#include "visual_cyclers.h"

#pragma warning(push, 0)
extern "C" {
#include <GL/glew.h>
}
#include <src/trance.pb.h>
#include <SFML/OpenGL.hpp>
#pragma warning(pop)

namespace
{
  const float max_eye_offset = 1.f / 16;
  const uint32_t spiral_type_max = 7;
}
#include "shaders.h"

Director::Director(sf::RenderWindow& window, const trance_pb::Session& session,
                   const trance_pb::System& system, ThemeBank& themes,
                   const trance_pb::Program& program, bool realtime, bool oculus_rift,
                   bool convert_to_yuv)
: _window{window}
, _session{session}
, _system{system}
, _themes{themes}
, _width{window.getSize().x}
, _height{window.getSize().y}
, _program{&program}
, _realtime{realtime}
, _convert_to_yuv{convert_to_yuv}
, _render_fbo{0}
, _render_fb_tex{0}
, _yuv_fbo{0}
, _yuv_fb_tex{0}
, _spiral_program{0}
, _quad_buffer{0}
{
  _oculus.enabled = false;
  _oculus.session = nullptr;

  GLenum ok = glewInit();
  if (ok != GLEW_OK) {
    std::cerr << "couldn't initialise GLEW: " << glewGetErrorString(ok) << std::endl;
  }

  if (!GLEW_VERSION_2_1) {
    std::cerr << "OpenGL 2.1 not available" << std::endl;
  }

  if (!GLEW_ARB_texture_non_power_of_two) {
    std::cerr << "OpenGL non-power-of-two textures not available" << std::endl;
  }

  if (!GLEW_ARB_shading_language_100 || !GLEW_ARB_shader_objects || !GLEW_ARB_vertex_shader ||
      !GLEW_ARB_fragment_shader) {
    std::cerr << "OpenGL shaders not available" << std::endl;
  }

  if (!GLEW_EXT_framebuffer_object) {
    std::cerr << "OpenGL framebuffer objects not available" << std::endl;
  }

  if (oculus_rift) {
    if (_realtime) {
      _oculus.enabled = init_oculus_rift();
    } else {
      _oculus.enabled = true;
    }
  }
  if (!_realtime) {
    init_framebuffer(_render_fbo, _render_fb_tex, _width, _height);
    init_framebuffer(_yuv_fbo, _yuv_fb_tex, _width, _height);
    _screen_data.reset(new uint8_t[4 * _width * _height]);
  }

  std::cout << "\npreloading GPU" << std::endl;
  static const std::size_t gl_preload = 1000;
  for (std::size_t i = 0; i < gl_preload; ++i) {
    themes.get_image(false);
    themes.get_image(true);
  }

  auto compile_shader = [&](GLuint shader) {
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
      GLint log_size = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);

      char* error_log = new char[log_size];
      glGetShaderInfoLog(shader, log_size, &log_size, error_log);
      std::cerr << error_log;
      delete[] error_log;
    }
  };

  auto link = [&](GLuint program) {
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
      GLint log_size = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);

      char* error_log = new char[log_size];
      glGetProgramInfoLog(program, log_size, &log_size, error_log);
      std::cerr << error_log;
      delete[] error_log;
    }
  };

  auto compile = [&](const std::string& vertex_text, const std::string& fragment_text) {
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);

    const char* v = vertex_text.data();
    const char* f = fragment_text.data();

    glShaderSource(vertex, 1, &v, nullptr);
    glShaderSource(fragment, 1, &f, nullptr);

    compile_shader(vertex);
    compile_shader(fragment);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    return program;
  };

  _new_program = compile(new_vertex, new_fragment);
  _spiral_program = compile(spiral_vertex, spiral_fragment);
  _yuv_program = compile(yuv_vertex, yuv_fragment);

  static const float quad_data[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f,
                                    1.f,  -1.f, 1.f, 1.f,  -1.f, 1.f};
  glGenBuffers(1, &_quad_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, quad_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  _visual_api.reset(new VisualApiImpl{*this, _themes, session, system, _height});
  change_visual(0);
  if (_realtime && !_oculus.enabled) {
    _window.setVisible(true);
    _window.setActive();
    _window.display();
  }
}

Director::~Director()
{
  glDeleteBuffers(1, &_quad_buffer);
  if (_oculus.session) {
    ovr_Destroy(_oculus.session);
  }
}

void Director::set_program(const trance_pb::Program& program)
{
  _program = &program;
}

bool Director::update()
{
  _visual_api->update();
  if (_old_visual) {
    _old_visual.reset(nullptr);
  }

  _visual->cycler()->advance();
  if (_visual->cycler()->complete()) {
    change_visual(_visual->cycler()->length());
  }

  bool to_oculus = _realtime && _oculus.enabled;
  if (to_oculus) {
    ovrSessionStatus status;
    auto result = ovr_GetSessionStatus(_oculus.session, &status);
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
      if (ovr_RecenterTrackingOrigin(_oculus.session) != ovrSuccess) {
        ovr_ClearShouldRecenterFlag(_oculus.session);
      }
    }
    _oculus.started = status.HmdPresent && !status.DisplayLost;
    if (!status.IsVisible && random_chance(1024)) {
      std::cerr << "Lost focus (move the HMD?)" << std::endl;
    }
  }
  return true;
}

void Director::render() const
{
  Image::delete_textures();
  bool to_window = _realtime && !_oculus.enabled;
  bool to_oculus = _realtime && _oculus.enabled;

  if (!_oculus.enabled) {
    glBindFramebuffer(GL_FRAMEBUFFER, to_window ? 0 : _render_fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    _oculus.rendering_right = false;
    _visual->render(*_visual_api);
  } else if (to_oculus) {
    if (_oculus.started) {
      auto timing = ovr_GetPredictedDisplayTime(_oculus.session, 0);
      auto sensorTime = ovr_GetTimeInSeconds();
      auto tracking = ovr_GetTrackingState(_oculus.session, timing, true);
      ovr_CalcEyePoses(tracking.HeadPose.ThePose, _oculus.eye_view_offset,
                       _oculus.layer.RenderPose);

      int index = 0;
      auto result =
          ovr_GetTextureSwapChainCurrentIndex(_oculus.session, _oculus.texture_chain, &index);
      if (result != ovrSuccess) {
        std::cerr << "Oculus texture swap chain index failed" << std::endl;
      }

      glBindFramebuffer(GL_FRAMEBUFFER, _oculus.fbo_ovr[index]);
      glClear(GL_COLOR_BUFFER_BIT);

      for (int eye = 0; eye < 2; ++eye) {
        _oculus.rendering_right = eye == ovrEye_Right;
        const auto& view = _oculus.layer.Viewport[eye];
        glViewport(view.Pos.x, view.Pos.y, view.Size.w, view.Size.h);
        _visual->render(*_visual_api);
      }

      result = ovr_CommitTextureSwapChain(_oculus.session, _oculus.texture_chain);
      if (result != ovrSuccess) {
        std::cerr << "Oculus commit texture swap chain failed" << std::endl;
      }

      _oculus.layer.SensorSampleTime = sensorTime;
      const ovrLayerHeader* layers = &_oculus.layer.Header;
      result = ovr_SubmitFrame(_oculus.session, 0, nullptr, &layers, 1);
      if (result != ovrSuccess && result != ovrSuccess_NotVisible) {
        std::cerr << "Oculus submit frame failed" << std::endl;
      }
    }
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, to_window ? 0 : _render_fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int eye = 0; eye < 2; ++eye) {
      _oculus.rendering_right = eye == ovrEye_Right;
      glViewport(_oculus.rendering_right * view_width(), 0, view_width(), _height);
      _visual->render(*_visual_api);
    }
  }

  if (!_realtime) {
    // Could do more on the GPU e.g. scaling, splitting planes, but the VP8
    // encoding is the bottleneck anyway.
    glBindFramebuffer(GL_FRAMEBUFFER, _yuv_fbo);
    glViewport(0, 0, _width, _height);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glUseProgram(_yuv_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _render_fb_tex);

    glUniform1i(glGetUniformLocation(_new_program, "source"), 0);
    glUniform1f(glGetUniformLocation(_yuv_program, "yuv_mix"), _convert_to_yuv ? 1.f : 0.f);
    glUniform2f(glGetUniformLocation(_yuv_program, "resolution"), float(_width), float(_height));
    auto loc = glGetAttribLocation(_yuv_program, "position");
    glEnableVertexAttribArray(loc);
    glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
    glVertexAttribPointer(loc, 2, GL_FLOAT, false, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (!_realtime) {
    glBindTexture(GL_TEXTURE_2D, _yuv_fb_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, _screen_data.get());
  }
  if (to_window) {
    _window.display();
  }
}

const uint8_t* Director::get_screen_data() const
{
  return _screen_data.get();
}

const trance_pb::Program& Director::program() const
{
  return *_program;
}

bool Director::vr_enabled() const
{
  return _oculus.enabled;
}

void Director::render_spiral(float spiral, uint32_t spiral_width, uint32_t spiral_type) const
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_CULL_FACE);

  auto aspect_ratio = float(view_width()) / float(_height);

  glUseProgram(_spiral_program);
  glUniform1f(glGetUniformLocation(_spiral_program, "near_plane"), 1.f);
  glUniform1f(glGetUniformLocation(_spiral_program, "far_plane"), 1.f + far_plane_distance());
  glUniform1f(glGetUniformLocation(_spiral_program, "eye_offset"), eye_offset());
  glUniform1f(glGetUniformLocation(_spiral_program, "aspect_ratio"), aspect_ratio);
  glUniform1f(glGetUniformLocation(_spiral_program, "width"), float(spiral_width));
  glUniform1f(glGetUniformLocation(_spiral_program, "spiral_type"), float(spiral_type));
  glUniform1f(glGetUniformLocation(_spiral_program, "time"), spiral);
  glUniform4f(glGetUniformLocation(_spiral_program, "acolour"), _program->spiral_colour_a().r(),
              _program->spiral_colour_a().g(), _program->spiral_colour_a().b(),
              _program->spiral_colour_a().a());
  glUniform4f(glGetUniformLocation(_spiral_program, "bcolour"), _program->spiral_colour_b().r(),
              _program->spiral_colour_b().g(), _program->spiral_colour_b().b(),
              _program->spiral_colour_b().a());

  auto position_location = glGetAttribLocation(_spiral_program, "device_position");
  glEnableVertexAttribArray(position_location);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glVertexAttribPointer(position_location, 2, GL_FLOAT, false, 0, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisableVertexAttribArray(position_location);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Director::render_image(const Image& image, float alpha, float zoom_origin, float zoom) const
{
  GLuint position_buffer;
  glGenBuffers(1, &position_buffer);
  std::vector<float> position_data;

  GLuint texture_buffer;
  glGenBuffers(1, &texture_buffer);
  std::vector<float> texture_data;

  auto x_scale = float(image.width()) / _width;
  auto y_scale = float(image.height()) / _height;
  auto x_size = std::min(1.f, x_scale / y_scale);
  auto y_size = std::min(1.f, y_scale / x_scale);
  if (_oculus.enabled) {
    x_size /= 2.5;
    y_size /= 2.5;
  }

  GLsizei vertex_count = 0;
  auto add_quad = [&](int x, int y, bool x_flip, bool y_flip) {
    auto x_offset = 2 * x * x_size;
    auto y_offset = 2 * y * y_size;
    vertex_count += 6;

    position_data.insert(position_data.end(),
                         {x_offset - x_size, y_offset - y_size, zoom, zoom_origin,
                          x_offset + x_size, y_offset - y_size, zoom, zoom_origin,
                          x_offset - x_size, y_offset + y_size, zoom, zoom_origin,
                          x_offset + x_size, y_offset - y_size, zoom, zoom_origin,
                          x_offset + x_size, y_offset + y_size, zoom, zoom_origin,
                          x_offset - x_size, y_offset + y_size, zoom, zoom_origin});

    texture_data.insert(
        texture_data.end(),
        {x_flip ? 1.f : 0.f, y_flip ? 0.f : 1.f, x_flip ? 0.f : 1.f, y_flip ? 0.f : 1.f,
         x_flip ? 1.f : 0.f, y_flip ? 1.f : 0.f, x_flip ? 0.f : 1.f, y_flip ? 0.f : 1.f,
         x_flip ? 0.f : 1.f, y_flip ? 1.f : 0.f, x_flip ? 1.f : 0.f, y_flip ? 1.f : 0.f});
  };

  for (int x = 0; x_size * (2 * x - 1) < 1 + max_eye_offset; ++x) {
    for (int y = 0; y_size * (2 * y - 1) < 1; ++y) {
      add_quad(x, y, x % 2, y % 2);
      if (x) {
        add_quad(-x, y, x % 2, y % 2);
      }
      if (y) {
        add_quad(x, -y, x % 2, y % 2);
      }
      if (x && y) {
        add_quad(-x, -y, x % 2, y % 2);
      }
    }
  }
  // Avoids the very edge of images.
  static const auto epsilon = 1.f / 256;
  for (auto& uv : texture_data) {
    uv = uv * (1 - epsilon) + epsilon / 2;
  }

  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * position_data.size(), position_data.data(),
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * texture_data.size(), texture_data.data(),
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glEnable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(_new_program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, image.texture());
  glUniform1i(glGetUniformLocation(_new_program, "texture"), 0);
  glUniform1f(glGetUniformLocation(_new_program, "near_plane"), 1.f);
  glUniform1f(glGetUniformLocation(_new_program, "far_plane"), 1.f + far_plane_distance());
  glUniform1f(glGetUniformLocation(_new_program, "eye_offset"), eye_offset());
  glUniform4f(glGetUniformLocation(_new_program, "colour"), 1.f, 1.f, 1.f, alpha);

  GLuint position_location = glGetAttribLocation(_new_program, "virtual_position");
  glEnableVertexAttribArray(position_location);
  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glVertexAttribPointer(position_location, 4, GL_FLOAT, false, 0, 0);

  GLuint texture_location = glGetAttribLocation(_new_program, "texture_coord");
  glEnableVertexAttribArray(texture_location);
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer);
  glVertexAttribPointer(texture_location, 2, GL_FLOAT, false, 0, 0);

  glDrawArrays(GL_TRIANGLES, 0, vertex_count);

  glDisableVertexAttribArray(position_location);
  glDisableVertexAttribArray(texture_location);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDeleteBuffers(1, &position_buffer);
  glDeleteBuffers(1, &texture_buffer);
}

sf::Vector2f Director::text_size(const Font& font, const std::string& text, bool large) const
{
  auto size = font.get_size(text, large);
  return {size.x / view_width(), size.y / _height};
}

void Director::render_text(const Font& font, const std::string& text, bool large,
                           const sf::Color& colour, float scale, const sf::Vector2f& offset,
                           float zoom_origin, float zoom) const
{
  auto vertices = font.get_vertices(text, large);

  GLuint position_buffer;
  glGenBuffers(1, &position_buffer);
  std::vector<float> position_data;

  GLuint texture_buffer;
  glGenBuffers(1, &texture_buffer);
  std::vector<float> texture_data;

  for (const auto& vertex : vertices) {
    position_data.insert(position_data.end(),
                         {offset.x + 2 * scale * vertex.x / view_width(),
                          offset.y - 2 * scale * vertex.y / _height, zoom, zoom_origin});
    texture_data.insert(texture_data.end(), {vertex.u, vertex.v});
  }

  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * position_data.size(), position_data.data(),
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * texture_data.size(), texture_data.data(),
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glEnable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(_new_program);

  glActiveTexture(GL_TEXTURE0);
  font.bind_texture(large);
  glUniform1i(glGetUniformLocation(_new_program, "texture"), 0);
  glUniform1f(glGetUniformLocation(_new_program, "near_plane"), 1.f);
  glUniform1f(glGetUniformLocation(_new_program, "far_plane"), 1.f + far_plane_distance());
  glUniform1f(glGetUniformLocation(_new_program, "eye_offset"), eye_offset());
  glUniform4f(glGetUniformLocation(_new_program, "colour"), colour.r / 255.f, colour.g / 255.f,
              colour.b / 255.f, colour.a / 255.f);

  GLuint position_location = glGetAttribLocation(_new_program, "virtual_position");
  glEnableVertexAttribArray(position_location);
  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glVertexAttribPointer(position_location, 4, GL_FLOAT, false, 0, 0);

  GLuint texture_location = glGetAttribLocation(_new_program, "texture_coord");
  glEnableVertexAttribArray(texture_location);
  glBindBuffer(GL_ARRAY_BUFFER, texture_buffer);
  glVertexAttribPointer(texture_location, 2, GL_FLOAT, false, 0, 0);

  glDrawArrays(GL_QUADS, 0, GLsizei(vertices.size()));

  // Font texture must be unbound.
  glActiveTexture(GL_TEXTURE0);
  sf::Texture::bind(nullptr);

  glDisableVertexAttribArray(position_location);
  glDisableVertexAttribArray(texture_location);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDeleteBuffers(1, &position_buffer);
  glDeleteBuffers(1, &texture_buffer);
}

bool Director::init_framebuffer(uint32_t& fbo, uint32_t& fb_tex, uint32_t width,
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

bool Director::init_oculus_rift()
{
  if (ovr_Create(&_oculus.session, &_oculus.luid) != ovrSuccess) {
    std::cerr << "Oculus session failed" << std::endl;
    return false;
  }
  _oculus.started = false;
  auto desc = ovr_GetHmdDesc(_oculus.session);
  ovr_SetBool(_oculus.session, "QueueAheadEnabled", ovrFalse);

  ovrSizei eye_left =
      ovr_GetFovTextureSize(_oculus.session, ovrEyeType(0), desc.DefaultEyeFov[0], 1.0);
  ovrSizei eye_right =
      ovr_GetFovTextureSize(_oculus.session, ovrEyeType(1), desc.DefaultEyeFov[0], 1.0);
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

  auto result =
      ovr_CreateTextureSwapChainGL(_oculus.session, &texture_chain_desc, &_oculus.texture_chain);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain failed" << std::endl;
    ovrErrorInfo info;
    ovr_GetLastErrorInfo(&info);
    std::cerr << info.ErrorString << std::endl;
  }
  int texture_count = 0;
  result = ovr_GetTextureSwapChainLength(_oculus.session, _oculus.texture_chain, &texture_count);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain length failed" << std::endl;
  }
  for (int i = 0; i < texture_count; ++i) {
    GLuint fbo;
    GLuint fb_tex = 0;
    result = ovr_GetTextureSwapChainBufferGL(_oculus.session, _oculus.texture_chain, i, &fb_tex);
    if (result != ovrSuccess) {
      std::cerr << "Oculus texture swap chain buffer failed" << std::endl;
    }

    glGenFramebuffers(1, &fbo);
    _oculus.fbo_ovr.push_back(fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "framebuffer failed" << std::endl;
      return false;
    }
  }

  auto erd_left = ovr_GetRenderDesc(_oculus.session, ovrEye_Left, desc.DefaultEyeFov[0]);
  auto erd_right = ovr_GetRenderDesc(_oculus.session, ovrEye_Right, desc.DefaultEyeFov[1]);
  _oculus.eye_view_offset[0] = erd_left.HmdToEyeOffset;
  _oculus.eye_view_offset[1] = erd_right.HmdToEyeOffset;

  _oculus.layer.Header.Type = ovrLayerType_EyeFov;
  _oculus.layer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
  _oculus.layer.ColorTexture[0] = _oculus.texture_chain;
  _oculus.layer.ColorTexture[1] = _oculus.texture_chain;
  _oculus.layer.Fov[0] = erd_left.Fov;
  _oculus.layer.Fov[1] = erd_right.Fov;
  _oculus.layer.Viewport[0].Pos.x = 0;
  _oculus.layer.Viewport[0].Pos.y = 0;
  _oculus.layer.Viewport[0].Size.w = fw / 2;
  _oculus.layer.Viewport[0].Size.h = fh;
  _oculus.layer.Viewport[1].Pos.x = fw / 2;
  _oculus.layer.Viewport[1].Pos.y = 0;
  _oculus.layer.Viewport[1].Size.w = fw / 2;
  _oculus.layer.Viewport[1].Size.h = fh;

  _width = fw;
  _height = fh;
  _window.setSize(sf::Vector2u(0, 0));
  return true;
}

void Director::change_visual(uint32_t length)
{
  // Like !random_chance(chance), but scaled to current speed and cycle length.
  // Roughly 1/2 chance for a cycle of length 2048.
  auto fps = program().global_fps();
  if (length && random((2 * fps * length) / 2048) >= 120) {
    return;
  }
  _visual.swap(_old_visual);

  uint32_t total = 0;
  for (const auto& type : _program->visual_type()) {
    total += type.random_weight();
  }
  auto r = random(total);
  total = 0;
  trance_pb::Program_VisualType t;
  for (const auto& type : _program->visual_type()) {
    if (r < (total += type.random_weight())) {
      t = type.type();
      break;
    }
  }

  // TODO: if it's the same as the last choice, don't reset!
  if (t == trance_pb::Program_VisualType_ACCELERATE) {
    _visual.reset(new AccelerateVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_SLOW_FLASH) {
    _visual.reset(new SlowFlashVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_SUB_TEXT) {
    _visual.reset(new SubTextVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_FLASH_TEXT) {
    _visual.reset(new FlashTextVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_PARALLEL) {
    _visual.reset(new ParallelVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_SUPER_PARALLEL) {
    _visual.reset(new SuperParallelVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_ANIMATION) {
    _visual.reset(new AnimationVisual{*_visual_api});
  }
  if (t == trance_pb::Program_VisualType_SUPER_FAST) {
    _visual.reset(new SuperFastVisual{*_visual_api});
  }
}

uint32_t Director::view_width() const
{
  return _oculus.enabled ? _width / 2 : _width;
}

float Director::far_plane_distance() const
{
  return 1.f + _system.draw_depth().draw_depth() * 256.f;
}

float Director::eye_offset() const
{
  return !_oculus.enabled ? 0 : _oculus.rendering_right ? max_eye_offset : -max_eye_offset;
}
