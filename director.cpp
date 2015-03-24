#include "director.h"
#include "images.h"
#include "program.h"
#include "session.h"
#include "util.h"
#include <iostream>
#include <GL/glew.h>
#include <SFML/OpenGL.hpp>
#include <trance.pb.h>

static const unsigned int spiral_type_max = 7;

static const std::string text_vertex = R"(
uniform vec4 colour;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 vtexcoord;
varying vec4 vcolour;

void main()
{
  gl_Position = vec4(2.0 * position.x, -2.0 * position.y, 0.0, 1.0);
  vtexcoord = texcoord;
  vcolour = colour;
}
)";
static const std::string text_fragment = R"(
uniform sampler2D texture;
varying vec2 vtexcoord;
varying vec4 vcolour;

void main()
{
  gl_FragColor = vcolour * texture2D(texture, vtexcoord);
}
)";

static const std::string image_vertex = R"(
uniform vec2 min;
uniform vec2 max;
uniform vec2 flip;
uniform float alpha;
uniform float zoom;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 vtexcoord;
varying float valpha;

void main()
{
  vec2 pos = position / 2.0 + 0.5;
  pos = pos * (max - min) + min;
  pos = (pos - 0.5) * 2.0;
  gl_Position = vec4(pos, 0.0, 1.0);
  float z = 0.25 * zoom + 0.005;
  vtexcoord = vec2(texcoord.x > 0.5 ? 1.0 - z : z,
                   texcoord.y > 0.5 ? 1.0 - z : z);
  vtexcoord = vec2(flip.x != 0.0 ? 1.0 - vtexcoord.x : vtexcoord.x,
                   flip.y != 0.0 ? 1.0 - vtexcoord.y : vtexcoord.y);
  valpha = alpha;
}
)";
static const std::string image_fragment = R"(
uniform sampler2D texture;
varying vec2 vtexcoord;
varying float valpha;

void main()
{
  gl_FragColor = vec4(texture2D(texture, vtexcoord).rgb, valpha);
}
)";

static const std::string spiral_vertex = R"(
attribute vec2 position;

void main() {
  gl_Position = vec4(position.xy, 0.0, 1.0);
}
)";
static const std::string spiral_fragment = R"(
uniform float time;
uniform vec2 resolution;
uniform float offset;
uniform vec4 acolour;
uniform vec4 bcolour;

// A divisor of 360 (determines the number of spiral arms).
uniform float width;
uniform float spiral_type;

float spiral1(float r)
{
  return log(r);
}

float spiral2(float r)
{
  return r * r;
}

float spiral3(float r)
{
  return r;
}

float spiral4(float r)
{
  return sqrt(r);
}

float spiral5(float r)
{
  return -abs(r - 1);
}

float spiral6(float r)
{
  float r1 = r * 1.2;
  float r2 = (1.5 - 0.5 * r) * 1.2;
  return r < 1 ? pow(r1, 6.0) : -pow(r2, 6.0);
}

float spiral7(float r)
{
  float m = mod(r, 0.2);
  m = m < 0.1 ? m : 0.2 - m;
  return r + m * 3.0;
}

void main(void)
{
  vec2 aspect = vec2(resolution.x / resolution.y, 1.0);
  vec2 op = gl_FragCoord.xy - vec2(offset, 0.0);
  vec2 position = -aspect.xy + 2.0 * op / resolution.xy * aspect.xy;
  float angle = 0.0;
  float radius = length(position);
  if (position.x != 0.0 && position.y != 0.0) {
    angle = degrees(atan(position.y, position.x));
  }

  float factor =
      spiral_type == 1 ? spiral1(radius) :
      spiral_type == 2 ? spiral2(radius) :
      spiral_type == 3 ? spiral3(radius) :
      spiral_type == 4 ? spiral4(radius) :
      spiral_type == 5 ? spiral5(radius) :
      spiral_type == 6 ? spiral6(radius) :
                         spiral7(radius);
  float amod = mod(angle - width * time - 2.0 * width * factor, width);
  float v = amod < width / 2 ? 0.0 : 1.0;
  float t = 0.2 + 2.0 * (1.0 - pow(min(1.0, radius), 0.4));
  if (amod > width / 2.0 - t && amod < width / 2.0 + t) {
    v = (amod - width / 2.0 + t) / (2.0 * t);
  }
  if (amod < t) {
    v = 1.0 - (amod + t) / (2.0 * t);
  }
  if (amod > width - t) {
    v = 1.0 - (amod - width + t) / (2.0 * t);
  }
  gl_FragColor = mix(acolour, bcolour, v);
}
)";

Director::Director(sf::RenderWindow& window, const trance_pb::Session& session,
                   ImageBank& images, const std::vector<std::string>& fonts,
                   unsigned int width, unsigned int height)
: _window{window}
, _session{session}
, _images{images}
, _fonts{fonts, session.system().font_cache_size()}
, _width{width}
, _height{height}
, _image_program{0}
, _spiral_program{0}
, _quad_buffer{0}
, _tex_buffer{0}
, _spiral{0}
, _spiral_type{0}
, _spiral_width{60}
, _switch_sets{0}
{
  _oculus.enabled = session.system().enable_oculus_rift();
  _oculus.hmd = nullptr;
  _oculus.fbo = 0;
  _oculus.fb_tex = 0;
  _oculus.fb_depth = 0;

  GLenum ok = glewInit();
  if (ok != GLEW_OK) {
    std::cerr << "Couldn't initialise GLEW: " <<
        glewGetErrorString(ok) << std::endl;
  }

  if (!GLEW_VERSION_2_1) {
    std::cerr << "OpenGL 2.1 not available" << std::endl;
  }

  if (!GLEW_ARB_texture_non_power_of_two) {
    std::cerr << "OpenGL non-power-of-two textures not available" << std::endl;
  }

  if (!GLEW_ARB_shading_language_100 || !GLEW_ARB_shader_objects ||
      !GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader) {
    std::cerr << "OpenGL shaders not available" << std::endl;
  }

  init_oculus_rift();

  static const std::size_t gl_preload = 1000;
  for (std::size_t i = 0; i < gl_preload; ++i) {
    images.get();
    images.get(true);
  }

  auto compile_shader = [&](GLuint shader)
  {
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

  auto link = [&](GLuint program)
  {
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

  auto compile = [&](const std::string& vertex_text,
                     const std::string& fragment_text)
  {
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

  _spiral_program = compile(spiral_vertex, spiral_fragment);
  _image_program = compile(image_vertex, image_fragment);
  _text_program = compile(text_vertex, text_fragment);

  static const float quad_data[] = {
    -1.f, -1.f,
    1.f, -1.f,
    -1.f, 1.f,
    1.f, -1.f,
    1.f, 1.f,
    -1.f, 1.f};
  glGenBuffers(1, &_quad_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, quad_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  static const float tex_data[] = {
    0.f, 1.f,
    1.f, 1.f,
    0.f, 0.f,
    1.f, 1.f,
    1.f, 0.f,
    0.f, 0.f};
  glGenBuffers(1, &_tex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, _tex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, tex_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  change_font();
  change_spiral();
  change_program();
  change_subtext();
}

Director::~Director()
{
  if (_oculus.hmd) {
    ovrHmd_Destroy(_oculus.hmd);
  }
}

float Director::get_frame_time() const
{
  return 1.f / program().global_fps();
}

void Director::update()
{
  if (_oculus.enabled) {
    ovrHSWDisplayState state;
    ovrHmd_GetHSWDisplayState(_oculus.hmd, &state);
    if (state.Displayed && ovr_GetTimeInSeconds() > state.DismissibleTime) {
      ovrHmd_DismissHSWDisplay(_oculus.hmd);
    }
  }
  ++_switch_sets;
  if (_old_program) {
    _old_program.reset(nullptr);
  }
  _program->update();
}

void Director::render() const
{
  Image::delete_textures();
  if (!_oculus.enabled) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    _oculus.rendering_right = false;
    _program->render();
    _window.display();
    return;
  }

  ovrPosef pose[2];
  ovrHmd_BeginFrame(_oculus.hmd, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, _oculus.fbo);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  for (int i = 0; i < 2; ++i) {
    auto eye = _oculus.hmd->EyeRenderOrder[i];
    pose[eye] = ovrHmd_GetHmdPosePerEye(_oculus.hmd, eye);
		glViewport((eye != ovrEye_Left) * view_width(), 0, view_width(), _height);
    _oculus.rendering_right = eye == ovrEye_Right;
    _program->render();
  }

  ovrHmd_EndFrame(_oculus.hmd, pose, &_oculus.fb_ovr_tex[0].Texture);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glUseProgram(0);
}

Image Director::get(bool alternate) const
{
  return _images.get(alternate);
}

const std::string& Director::get_text(bool alternate) const
{
  return _images.get_text(alternate);
}

void Director::maybe_upload_next() const
{
  _images.maybe_upload_next();
}

void Director::render_image(const Image& image, float alpha,
                            float multiplier, float zoom) const
{
  if (image.anim_type != Image::NONE) {
    bool alternate = image.anim_type == Image::ALTERNATE_ANIMATION;
    Image anim = _images.get_animation(
        std::size_t(120.f * get_frame_time() * _switch_sets / 8), alternate);
    if (anim.texture) {
      render_image(anim, alpha, multiplier, zoom);
      return;
    }
  }
  glEnable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(_image_program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, image.texture);
  glUniform1f(glGetUniformLocation(_image_program, "alpha"), alpha);
  glUniform1f(glGetUniformLocation(_image_program, "zoom"),
              program().zoom_intensity() * zoom);

  GLuint ploc = glGetAttribLocation(_image_program, "position");
  glEnableVertexAttribArray(ploc);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glVertexAttribPointer(ploc, 2, GL_FLOAT, false, 0, 0);

  GLuint tloc = glGetAttribLocation(_image_program, "texcoord");
  glEnableVertexAttribArray(tloc);
  glBindBuffer(GL_ARRAY_BUFFER, _tex_buffer);
  glVertexAttribPointer(tloc, 2, GL_FLOAT, false, 0, 0);

  float offx3d = off3d(multiplier).x;
  auto x = float(image.width);
  auto y = float(image.height);

  auto scale = std::min(float(_height) / y, float(_width) / x);
  if (_oculus.enabled) {
    scale *= 0.5f;
  }
  x *= scale;
  y *= scale;

  for (int i = 0; _width / 2 - i * x + x / 2 >= 0; ++i) {
    for (int j = 0; _height / 2 - j * y + y / 2 >= 0; ++j) {
      auto x1 = offx3d + _width / 2 - x / 2;
      auto x2 = offx3d + _width / 2 + x / 2;
      auto y1 = _height / 2 - y / 2;
      auto y2 = _height / 2 + y / 2;
      render_texture(x1 - i * x, y1 - j * y,
                     x2 - i * x, y2 - j * y,
                     i % 2 != 0, j % 2 != 0);
      if (i != 0) {
        render_texture(x1 + i * x, y1 - j * y,
                       x2 + i * x, y2 - j * y,
                       i % 2 != 0, j % 2 != 0);
      }
      if (j != 0) {
        render_texture(x1 - i * x, y1 + j * y,
                       x2 - i * x, y2 + j * y,
                       i % 2 != 0, j % 2 != 0);
      }
      if (i != 0 && j != 0) {
        render_texture(x1 + i * x, y1 + j * y,
                       x2 + i * x, y2 + j * y,
                       i % 2 != 0, j % 2 != 0);
      }
    }
  }

  glDisableVertexAttribArray(ploc);
  glDisableVertexAttribArray(tloc);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Director::render_text(const std::string& text, float multiplier) const
{
  static const std::size_t default_size = 200;
  static const std::size_t shadow_extra = 60;
  if (_current_font.empty()) {
    return;
  }

  unsigned int border = _oculus.enabled ? 250 : 100;
  auto fit_text = [&](unsigned int size, bool fix)
  {
    auto r = get_text_size(text, _fonts.get_font(_current_font, size));
    int new_size = size;
    while (!fix && r.x > view_width() - border) {
      new_size = new_size * (view_width() - border) / int(r.x);
      r = get_text_size(text, _fonts.get_font(_current_font, new_size));
    }
    return new_size;
  };

  auto main_size = fit_text(default_size, false);
  auto shadow_size = fit_text(default_size + shadow_extra, true);
  render_raw_text(
      text, _fonts.get_font(_current_font, shadow_size),
      colour2sf(program().shadow_text_colour()), off3d(1.f + multiplier),
      std::exp((4.f - multiplier) / 16.f));
  render_raw_text(
      text, _fonts.get_font(_current_font, main_size),
      colour2sf(program().main_text_colour()), off3d(multiplier),
      std::exp((4.f - multiplier) / 16.f));
}

void Director::render_subtext(float alpha, float multiplier) const
{
  if (_subtext.empty()) {
    return;
  }

  static const unsigned int char_size = 100;
  std::size_t n = 0;
  const auto& font = _fonts.get_font(_current_subfont, char_size);

  auto make_text = [&]
  {
    std::string t;
    do {
      t += " " + _subtext[n];
      n = (n + 1) % _subtext.size();
    }
    while (get_text_size(t, font).x < _width);
    return t;
  };

  float offx3d = off3d(multiplier).x;
  auto text = make_text();
  auto d = get_text_size(text, font);
  auto colour = sf::Color(0, 0, 0, sf::Uint8(alpha * 255));
  render_raw_text(text, font, colour, sf::Vector2f{offx3d, 0});
  auto offset = d.y + 4;
  for (int i = 1; d.y / 2 + i * offset < _height; ++i) {
    text = make_text();
    render_raw_text(text, font, colour, sf::Vector2f{offx3d, i * offset});

    text = make_text();
    render_raw_text(text, font, colour, -sf::Vector2f{offx3d, i * offset});
  }
}

void Director::render_spiral(float multiplier) const
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_CULL_FACE);

  glUseProgram(_spiral_program);
  glUniform1f(glGetUniformLocation(_spiral_program, "time"), _spiral);
  glUniform2f(glGetUniformLocation(_spiral_program, "resolution"),
              float(view_width()), float(_height));

  float offset = off3d(multiplier).x +
      (_oculus.rendering_right ? float(view_width()) : 0.f);
  glUniform1f(glGetUniformLocation(_spiral_program, "offset"),
              _oculus.enabled ? offset : 0.f);
  glUniform1f(glGetUniformLocation(_spiral_program, "width"),
              float(_spiral_width));
  glUniform1f(glGetUniformLocation(_spiral_program, "spiral_type"),
              float(_spiral_type));
  glUniform4f(glGetUniformLocation(_spiral_program, "acolour"),
              program().spiral_colour_a().r(), program().spiral_colour_a().g(),
              program().spiral_colour_a().b(), program().spiral_colour_a().a());
  glUniform4f(glGetUniformLocation(_spiral_program, "bcolour"),
              program().spiral_colour_b().r(), program().spiral_colour_b().g(),
              program().spiral_colour_b().b(), program().spiral_colour_b().a());

  auto loc = glGetAttribLocation(_spiral_program, "position");
  glEnableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glVertexAttribPointer(loc, 2, GL_FLOAT, false, 0, 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisableVertexAttribArray(loc);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Director::rotate_spiral(float amount)
{
  _spiral += amount / (32 * sqrt(float(_spiral_width)));
  while (_spiral > 1.f) {
    _spiral -= 1.f;
  }
  while (_spiral < 0.f) {
    _spiral += 1.f;
  }
}

void Director::change_spiral()
{
  if (random_chance(4)) {
    return;
  }
  _spiral_type = random(spiral_type_max);
  _spiral_width = 360 / (1 + random(6));
}

void Director::change_font(bool force)
{
  _current_font = _fonts.get_path(force);
}

void Director::change_subtext(bool alternate)
{
  static const unsigned int count = 16;
  _subtext.clear();
  for (unsigned int i = 0; i < 16; ++i) {
    auto s = _images.get_text(alternate);
    for (auto& c : s) {
      if (c == '\n') {
        c = ' ';
      }
    }
    if (!s.empty()) {
      _subtext.push_back(s);
    }
  }
}

bool Director::change_sets()
{
  if (_switch_sets < 2048 || random_chance(4)) {
    return false;
  }
  if (_images.change_sets()) {
    _switch_sets = 0;
    return true;
  }
  return false;
}

void Director::change_program()
{
  if (_old_program) {
    return;
  }
  _current_subfont = _fonts.get_path(false);
  _program.swap(_old_program);

  unsigned int total = 0;
  for (const auto& type : program().type()) {
    total += type.random_weight();
  }
  auto r = random(total);
  total = 0;
  trance_pb::ProgramConfiguration_Type t;
  for (const auto& type : program().type()) {
    if (r < (total += type.random_weight())) {
      t = type.type();
      break;
    }
  }

  if (t == trance_pb::ProgramConfiguration_Type_ACCELERATE) {
    _program.reset(new AccelerateProgram{*this, random_chance()});
  }
  if (t == trance_pb::ProgramConfiguration_Type_SLOW_FLASH) {
    _program.reset(new SlowFlashProgram{*this});
  }
  if (t == trance_pb::ProgramConfiguration_Type_SUB_TEXT) {
    _program.reset(new SubTextProgram{*this});
  }
  if (t == trance_pb::ProgramConfiguration_Type_FLASH_TEXT) {
    _program.reset(new FlashTextProgram{*this});
  }
  if (t == trance_pb::ProgramConfiguration_Type_PARALLEL) {
    _program.reset(new ParallelProgram{*this});
  }
  if (t == trance_pb::ProgramConfiguration_Type_SUPER_PARALLEL) {
    _program.reset(new SuperParallelProgram{*this});
  }
  if (t == trance_pb::ProgramConfiguration_Type_ANIMATION) {
    _program.reset(new AnimationProgram{*this});
  }
}

const trance_pb::ProgramConfiguration& Director::program() const
{
  return _session.program();
}

void Director::init_oculus_rift()
{
  if (!_oculus.enabled) {
    return;
  }

  if (!GLEW_EXT_framebuffer_object) {
    std::cerr << "OpenGL framebuffer objects not available" << std::endl;
    _oculus.enabled = false;
    return;
  }

  _oculus.hmd = ovrHmd_Create(0);
  if (!_oculus.hmd) {
    std::cerr << "Oculus HMD failed" << std::endl;
#ifndef DEBUG
    _oculus.enabled = false;
    return;
#endif
    _oculus.hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
    if (!_oculus.hmd) {
      std::cerr << "Oculus HMD debug mode failed" << std::endl;
      _oculus.enabled = false;
      return;
    }
  }

  _window.setSize(sf::Vector2u(_oculus.hmd->Resolution.w,
                               _oculus.hmd->Resolution.h));
  auto oculus_flags =
      ovrTrackingCap_Orientation |
      ovrTrackingCap_MagYawCorrection |
      ovrTrackingCap_Position;
  ovrHmd_ConfigureTracking(_oculus.hmd, oculus_flags, 0);

  ovrSizei eye_left = ovrHmd_GetFovTextureSize(
      _oculus.hmd, ovrEye_Left, _oculus.hmd->DefaultEyeFov[0], 1.0);
  ovrSizei eye_right = ovrHmd_GetFovTextureSize(
      _oculus.hmd, ovrEye_Right, _oculus.hmd->DefaultEyeFov[0], 1.0);
  int fw = eye_left.w + eye_right.w;
  int fh = std::max(eye_left.h, eye_right.h);

  glGenFramebuffers(1, &_oculus.fbo);
  glGenTextures(1, &_oculus.fb_tex);
  glGenRenderbuffers(1, &_oculus.fb_depth);

  glBindTexture(GL_TEXTURE_2D, _oculus.fb_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, _oculus.fbo);

  glBindTexture(GL_TEXTURE_2D, _oculus.fb_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fw, fh, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, _oculus.fb_tex, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, _oculus.fb_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fw, fh);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, _oculus.fb_depth);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer failed" << std::endl;
    _oculus.enabled = false;
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  for (int i = 0; i < 2; ++i) {
    _oculus.fb_ovr_tex[i].OGL.Header.API = ovrRenderAPI_OpenGL;
    _oculus.fb_ovr_tex[i].OGL.Header.TextureSize.w = fw;
    _oculus.fb_ovr_tex[i].OGL.Header.TextureSize.h = fh;
    _oculus.fb_ovr_tex[i].OGL.Header.RenderViewport.Pos.x = i * fw / 2;
    _oculus.fb_ovr_tex[i].OGL.Header.RenderViewport.Pos.y = 0;
    _oculus.fb_ovr_tex[i].OGL.Header.RenderViewport.Size.w = fw / 2;
    _oculus.fb_ovr_tex[i].OGL.Header.RenderViewport.Size.h = fh;
    _oculus.fb_ovr_tex[i].OGL.TexId = _oculus.fb_tex;
  }

  memset(&_oculus.gl_cfg, 0, sizeof(_oculus.gl_cfg));
  _oculus.gl_cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
  _oculus.gl_cfg.OGL.Header.BackBufferSize = _oculus.hmd->Resolution;
  _oculus.gl_cfg.OGL.Header.Multisample = 1;
  _oculus.gl_cfg.OGL.Window = GetActiveWindow();
  _oculus.gl_cfg.OGL.DC = wglGetCurrentDC();

  if (!(_oculus.hmd->HmdCaps & ovrHmdCap_ExtendDesktop)) {
    std::cerr << "Direct HMD access unsupported" << std::endl;
    _oculus.enabled = false;
    return;
  }

  auto hmd_caps =
      ovrHmdCap_LowPersistence |
      ovrHmdCap_DynamicPrediction;
  ovrHmd_SetEnabledCaps(_oculus.hmd, hmd_caps);

  auto distort_caps =
      ovrDistortionCap_Chromatic |
      ovrDistortionCap_Vignette |
      ovrDistortionCap_TimeWarp |
      ovrDistortionCap_Overdrive;
  if (!ovrHmd_ConfigureRendering(_oculus.hmd, &_oculus.gl_cfg.Config,
                                 distort_caps, _oculus.hmd->DefaultEyeFov,
                                 _oculus.eye_desc)) {
    std::cerr << "Oculus rendering failed" << std::endl;
    _oculus.enabled = false;
    return;
  }
  _width = fw;
  _height = fh;
  _window.setVerticalSyncEnabled(true);
  _window.setFramerateLimit(75);
#ifndef DEBUG
  _window.setPosition(sf::Vector2i(_oculus.hmd->WindowsPos.x,
                                   _oculus.hmd->WindowsPos.y));
#endif
}

sf::Vector2f Director::off3d(float multiplier) const
{
  float x = !_oculus.enabled || !multiplier ? 0.f :
      !_oculus.rendering_right ? _width / (8.f * multiplier) :
                                 _width / -(8.f * multiplier);
  return {x, 0};
}

unsigned int Director::view_width() const
{
  return _oculus.enabled ? _width / 2 : _width;
}

void Director::render_texture(float l, float t, float r, float b,
                              bool flip_h, bool flip_v) const
{
  glUniform2f(glGetUniformLocation(_image_program, "min"),
              l / _width, t / _height);
  glUniform2f(glGetUniformLocation(_image_program, "max"),
              r / _width, b / _height);
  glUniform2f(glGetUniformLocation(_image_program, "flip"),
              flip_h ? 1.f : 0.f, flip_v ? 1.f : 0.f);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Director::render_raw_text(const std::string& text, const Font& font,
                               const sf::Color& colour,
                               const sf::Vector2f& offset, float scale) const
{
  if (!text.length()) {
    return;
  }

  struct vertex {
    float x;
    float y;
    float u;
    float v;
  };
  static std::vector<vertex> vertices;
  vertices.clear();

  auto hspace = font.font->getGlyph(' ', font.key.char_size, false).advance;
  auto vspace = font.font->getLineSpacing(font.key.char_size);
  float x = 0.f;
  float y = 0.f;
  const sf::Texture& texture = font.font->getTexture(font.key.char_size);

  float xmin = 256.f;
  float ymin = 256.f;
  float xmax = -256.f;
  float ymax = -256.f;

  uint32_t prev = 0;
  for (std::size_t i = 0; i < text.length(); ++i) {
    uint32_t current = text[i];
    x += font.font->getKerning(prev, current, font.key.char_size);
    prev = current;

    switch (current) {
      case L' ':
        x += hspace;
        continue;
      case L'\t':
        x += hspace * 4;
        continue;
      case L'\n':
        y += vspace;
        x = 0;
        continue;
      case L'\v':
        y += vspace * 4;
        continue;
    }

    const auto& g = font.font->getGlyph(current, font.key.char_size, false);
    float x1 = (x + g.bounds.left) / _width;
    float y1 = (y + g.bounds.top) / _height;
    float x2 = (x + g.bounds.left + g.bounds.width) / _width;
    float y2 = (y + g.bounds.top + g.bounds.height) / _height;
    float u1 = float(g.textureRect.left) / texture.getSize().x;
    float v1 = float(g.textureRect.top) / texture.getSize().y;
    float u2 =
        float(g.textureRect.left + g.textureRect.width) / texture.getSize().x;
    float v2 =
        float(g.textureRect.top + g.textureRect.height) / texture.getSize().y;

    vertices.push_back({x1, y1, u1, v1});
    vertices.push_back({x2, y1, u2, v1});
    vertices.push_back({x2, y2, u2, v2});
    vertices.push_back({x1, y2, u1, v2});
    xmin = std::min(xmin, std::min(x1, x2));
    xmax = std::max(xmax, std::max(x1, x2));
    ymin = std::min(ymin, std::min(y1, y2));
    ymax = std::max(ymax, std::max(y1, y2));
    x += g.advance;
  }
  for (auto& v : vertices) {
    v.x -= xmin + (xmax - xmin) / 2;
    v.y -= ymin + (ymax - ymin) / 2;
    v.x *= scale;
    v.y *= scale;
    v.x += offset.x / _width;
    v.y += offset.y / _height;
  }

  glEnable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(_text_program);

  glActiveTexture(GL_TEXTURE0);
  sf::Texture::bind(&texture);
  glUniform4f(glGetUniformLocation(_text_program, "colour"),
              colour.r / 255.f, colour.g / 255.f,
              colour.b / 255.f, colour.a / 255.f);
  const char* data = reinterpret_cast<const char*>(vertices.data());

  GLuint ploc = glGetAttribLocation(_image_program, "position");
  glEnableVertexAttribArray(ploc);
  glVertexAttribPointer(ploc, 2, GL_FLOAT, false, sizeof(vertex), data);

  GLuint tloc = glGetAttribLocation(_image_program, "texcoord");
  glEnableVertexAttribArray(tloc);
  glVertexAttribPointer(tloc, 2, GL_FLOAT, false, sizeof(vertex), data + 8);
  glDrawArrays(GL_QUADS, 0, (GLsizei) vertices.size());
}

sf::Vector2f Director::get_text_size(
    const std::string& text, const Font& font) const
{
  auto hspace = font.font->getGlyph(' ', font.key.char_size, false).advance;
  auto vspace = font.font->getLineSpacing(font.key.char_size);
  float x = 0.f;
  float y = 0.f;
  float xmin = 0.f;
  float ymin = 0.f;
  float xmax = 0.f;
  float ymax = 0.f;

  uint32_t prev = 0;
  for (std::size_t i = 0; i < text.length(); ++i) {
    uint32_t current = text[i];
    x += font.font->getKerning(prev, current, font.key.char_size);
    prev = current;

    switch (current) {
      case L' ':
        x += hspace;
        continue;
      case L'\t':
        x += hspace * 4;
        continue;
      case L'\n':
        y += vspace;
        x = 0;
        continue;
      case L'\v':
        y += vspace * 4;
        continue;
    }

    const auto& g = font.font->getGlyph(current, font.key.char_size, false);
    float x1 = x + g.bounds.left;
    float y1 = y + g.bounds.top;
    float x2 = x + g.bounds.left + g.bounds.width;
    float y2 = y + g.bounds.top + g.bounds.height;
    xmin = std::min(xmin, std::min(x1, x2));
    xmax = std::max(xmax, std::max(x1, x2));
    ymin = std::min(ymin, std::min(y1, y2));
    ymax = std::max(ymax, std::max(y1, y2));
    x += g.advance;
  }

  return {xmax - xmin, ymax - ymin};
}