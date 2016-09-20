#include "director.h"
#include "session.h"
#include "theme.h"
#include "util.h"
#include "visual.h"
#include <algorithm>
#include <iostream>

#pragma warning(push, 0)
extern "C" {
#include <GL/glew.h>
}
#include <SFML/OpenGL.hpp>
#include <src/trance.pb.h>
#pragma warning(pop)

static const uint32_t spiral_type_max = 7;

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

static const std::string new_vertex = R"(
// Distance to near plane. Controls the effect of zoom.
uniform float near_plane;
// Distance to far plane. Controls the effect of zoom.
uniform float far_plane;
// Unitless eye offset.
uniform float eye_offset;
// Alpha value.
uniform float alpha;

// Virtual position of vertex (in [-1 - |eye|, 1 + |eye|] X [-1, 1] X [0, 1]).
// The third coordinate is the zoom amount.
attribute vec3 virtual_position;
// Texture coordinate for this vertex.
attribute vec2 texture_coord;

// Output texture coordinate.
varying vec2 out_texture_coord;
// Output alpha value.
varying float out_alpha;

// Applies perspective projection onto unit square.
mat4 m_perspective = mat4(
    near_plane, 0., 0., 0.,
    0., near_plane, 0., 0.,
    0., 0., (near_plane + far_plane) / (near_plane - far_plane), -1.,
    0., 0., 2. * (near_plane * far_plane) / (near_plane - far_plane), 0.);

// Applies the zoom coordinate.
mat4 m_virtual = mat4(
    far_plane / near_plane, 0., 0., -(far_plane / near_plane) * eye_offset,
    0., far_plane / near_plane, 0., 0.,
    0., 0., far_plane - near_plane, 0.,
    0., 0., -far_plane, 1.);

// Avoids the very edge of images.
const float texture_epsilon = 1. / 256;

void main()
{
  gl_Position = m_perspective * m_virtual * vec4(virtual_position, 1.);
  out_texture_coord =
      texture_coord * (1. - texture_epsilon) + texture_epsilon / 2.;
  out_alpha = alpha;
}
)";

static const std::string new_fragment = R"(
// Active texture for this draw.
uniform sampler2D texture;
// Input texture coordinate.
varying vec2 out_texture_coord;
// Input alpha value.
varying float out_alpha;

void main()
{
  gl_FragColor = vec4(texture2D(texture, out_texture_coord).rgb, out_alpha);
}
)";

static const std::string image_vertex = R"(
uniform vec2 min_coord;
uniform vec2 max_coord;
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
  pos = pos * (max_coord - min_coord) + min_coord;
  pos = (pos - 0.5) * 2.0;
  gl_Position = vec4(pos, 0.0, 1.0);
  float z = min(0.5, 0.1 * zoom + 0.005);
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

static const std::string yuv_vertex = R"(
attribute vec2 position;

void main() {
  gl_Position = vec4(position.xy, 0.0, 1.0);
}
)";
static const std::string yuv_fragment = R"(
uniform sampler2D source;
uniform vec2 resolution;
uniform float yuv_mix;

const mat3 map = mat3(
    0.257, -0.148, 0.439,
    0.504, -0.291, -0.368,
    0.098, 0.439, -0.071);
const vec3 offset = vec3(16.0, 128.0, 128.0) / 255.0;

void main(void)
{
  vec2 coord = gl_FragCoord.xy / resolution;
  coord.y *= -1;
  vec3 rgb = texture2D(source, coord).rgb;
  vec3 yuv = clamp(offset + map * rgb, 0.0, 1.0);
  gl_FragColor = vec4(mix(rgb, yuv, yuv_mix), 1.0);
}
)";

static sf::Color colour2sf(const trance_pb::Colour& colour)
{
  return sf::Color(
    sf::Uint8(colour.r() * 255),
    sf::Uint8(colour.g() * 255),
    sf::Uint8(colour.b() * 255),
    sf::Uint8(colour.a() * 255));
}

Director::Director(sf::RenderWindow& window,
                   const trance_pb::Session& session,
                   const trance_pb::System& system,
                   ThemeBank& themes, const trance_pb::Program& program,
                   bool realtime, bool oculus_rift, bool convert_to_yuv)
: _window{window}
, _session{session}
, _system{system}
, _themes{themes}
, _fonts{_themes.get_root_path(), system.font_cache_size()}
, _width{window.getSize().x}
, _height{window.getSize().y}
, _program{&program}
, _realtime{realtime}
, _convert_to_yuv{convert_to_yuv}
, _render_fbo{0}
, _render_fb_tex{0}
, _yuv_fbo{0}
, _yuv_fb_tex{0}
, _image_program{0}
, _spiral_program{0}
, _quad_buffer{0}
, _tex_buffer{0}
, _spiral{0}
, _spiral_type{0}
, _spiral_width{60}
, _switch_themes{0}
{
  _oculus.enabled = false;
  _oculus.session = nullptr;

  GLenum ok = glewInit();
  if (ok != GLEW_OK) {
    std::cerr << "couldn't initialise GLEW: " <<
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

  if (!GLEW_EXT_framebuffer_object) {
    std::cerr << "OpenGL framebuffer objects not available" << std::endl;
  }

  if (oculus_rift) {
    if (_realtime) {
      _oculus.enabled = init_oculus_rift();
    }
    else {
      _oculus.enabled = true;
    }
  }
  if (!_realtime) {
    init_framebuffer(_render_fbo, _render_fb_tex, _width, _height);
    init_framebuffer(_yuv_fbo, _yuv_fb_tex, _width, _height);
    _screen_data.reset(new uint8_t[4 * _width * _height]);
  }

  static const std::size_t gl_preload = 1000;
  for (std::size_t i = 0; i < gl_preload; ++i) {
    themes.get().get_image();
    themes.get(true).get_image();
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
  _yuv_program = compile(yuv_vertex, yuv_fragment);

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

  std::cout << "\ncaching text sizes" << std::endl;
  std::vector<std::string> fonts;
  for (const auto& pair : session.theme_map()) {
    for (const auto& font : pair.second.font_path()) {
      fonts.push_back(font);
    }
  }
  for (const auto& font : fonts) {
    FontCache temp_cache{_themes.get_root_path(), 8 * system.font_cache_size()};
    auto cache_text_size = [&](const std::string& text) {
      auto cache_key = font + "/\t/\t/" + text;
      auto cache_it = _text_size_cache.find(cache_key);
      if (cache_it == _text_size_cache.end()) {
        _text_size_cache[cache_key] = get_cached_text_size(temp_cache, text, font);
        std::cout << "-";
      }
    };

    for (const auto& pair : session.theme_map()) {
      if (!std::count(pair.second.font_path().begin(),
                      pair.second.font_path().end(), font)) {
        continue;
      }
      for (const auto& text : pair.second.text_line()) {
        for (const auto& line : SplitWords(text, SplitType::LINE)) {
          cache_text_size(line);
        }
        for (const auto& word : SplitWords(text, SplitType::WORD)) {
          cache_text_size(word);
        }
      }
    }
  }

  change_font(true);
  change_spiral();
  change_visual(0);
  change_subtext();
  if (_realtime && !_oculus.enabled) {
    _window.setVisible(true);
    _window.setActive();
    _window.display();
  }
}

Director::~Director()
{
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
  ++_switch_themes;
  if (_old_visual) {
    _old_visual.reset(nullptr);
  }
  _visual->update();

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
    if (!status.IsVisible && _switch_themes % 1000 == 0) {
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
    _visual->render();
  }
  else if (to_oculus) {
    if (_oculus.started) {
      auto timing = ovr_GetPredictedDisplayTime(_oculus.session, 0);
      auto sensorTime = ovr_GetTimeInSeconds();
      auto tracking = ovr_GetTrackingState(_oculus.session, timing, true);
      ovr_CalcEyePoses(tracking.HeadPose.ThePose,
                        _oculus.eye_view_offset, _oculus.layer.RenderPose);

      int index = 0;
      auto result = ovr_GetTextureSwapChainCurrentIndex(
          _oculus.session, _oculus.texture_chain, &index);
      if (result != ovrSuccess) {
        std::cerr << "Oculus texture swap chain index failed" << std::endl;
      }

      glBindFramebuffer(GL_FRAMEBUFFER, _oculus.fbo_ovr[index]);
      glClear(GL_COLOR_BUFFER_BIT);

      for (int eye = 0; eye < 2; ++eye) {
        _oculus.rendering_right = eye == ovrEye_Right;
        const auto& view = _oculus.layer.Viewport[eye];
        glViewport(view.Pos.x, view.Pos.y, view.Size.w, view.Size.h);
        _visual->render();
      }

      result =
          ovr_CommitTextureSwapChain(_oculus.session, _oculus.texture_chain);
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
  }
  else {
    glBindFramebuffer(GL_FRAMEBUFFER, to_window ? 0 : _render_fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int eye = 0; eye < 2; ++eye) {
      _oculus.rendering_right = eye == ovrEye_Right;
      glViewport(_oculus.rendering_right * view_width(), 0,
                 view_width(), _height);
      _visual->render();
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

    glUniform1f(glGetUniformLocation(_yuv_program, "yuv_mix"),
                _convert_to_yuv ? 1.f : 0.f);
    glUniform2f(glGetUniformLocation(_yuv_program, "resolution"),
                float(_width), float(_height));
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
    glGetTexImage(
        GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, _screen_data.get());
  }
  if (to_window) {
    _window.display();
  }
}

const uint8_t* Director::get_screen_data() const
{
  return _screen_data.get();
}

Image Director::get_image(bool alternate) const
{
  static const uint32_t recent_images = 8;

  // Fail-safe: try to avoid showing the same image twice. This doesn't handle
  // duplicate images in different sets.
  Image image;
  while (true) {
    image = _themes.get(alternate).get_image();

    bool found = false;
    for (const auto& p : _recent_images) {
      if (p == image.texture()) {
        found = true;
        break;
      }
    }
    if (!found) {
      break;
    }
    if (!_recent_images.empty()) {
      _recent_images.pop_back();
    }
  }

  if (_recent_images.size() >= recent_images) {
    _recent_images.erase(--_recent_images.end());
  }
  _recent_images.insert(_recent_images.begin(), image.texture());
  return image;
}

const std::string& Director::get_text(bool alternate) const
{
  return _themes.get(alternate).get_text();
}

void Director::maybe_upload_next() const
{
  _themes.maybe_upload_next();
}

void Director::render_animation_or_image(
    Anim type, const Image& image,
    float alpha, float multiplier, float zoom) const
{
  Image anim = _themes.get(type == Anim::ANIM_ALTERNATE).get_animation(
      std::size_t((120.f / _program->global_fps()) * _switch_themes / 8));

  if (type != Anim::NONE && anim) {
    render_image(anim, alpha, multiplier, zoom);
  }
  else {
    render_image(image, alpha, multiplier, zoom);
  }
}

void Director::render_image(const Image& image, float alpha,
                            float multiplier, float zoom) const
{
  glEnable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(_image_program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, image.texture());
  glUniform1f(glGetUniformLocation(_image_program, "alpha"), alpha);
  glUniform1f(glGetUniformLocation(_image_program, "zoom"),
              _program->zoom_intensity() * zoom);

  GLuint ploc = glGetAttribLocation(_image_program, "position");
  glEnableVertexAttribArray(ploc);
  glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
  glVertexAttribPointer(ploc, 2, GL_FLOAT, false, 0, 0);

  GLuint tloc = glGetAttribLocation(_image_program, "texcoord");
  glEnableVertexAttribArray(tloc);
  glBindBuffer(GL_ARRAY_BUFFER, _tex_buffer);
  glVertexAttribPointer(tloc, 2, GL_FLOAT, false, 0, 0);

  float offx3d = off3d(multiplier, false).x;
  auto x = float(image.width());
  auto y = float(image.height());

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
  if (_current_font.empty() || text.empty()) {
    return;
  }
  auto cache_key = _current_font + "/\t/\t/" + text;
  auto it = _text_size_cache.find(cache_key);
  if (it == _text_size_cache.end()) {
    return;
  }
  auto main_size = it->second;
  auto shadow_size = main_size + FontCache::char_size_lock;
  render_raw_text(
      text, _fonts.get_font(_current_font, shadow_size),
      colour2sf(_program->shadow_text_colour()), off3d(1.f + multiplier, true),
      std::exp((4.f - multiplier) / 16.f));
  render_raw_text(
      text, _fonts.get_font(_current_font, main_size),
      colour2sf(_program->main_text_colour()), off3d(multiplier, true),
      std::exp((4.f - multiplier) / 16.f));
}

void Director::render_subtext(float alpha, float multiplier) const
{
  if (_current_subfont.empty() || _subtext.empty()) {
    return;
  }

  static const uint32_t char_size = 100;
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

  float offx3d = off3d(multiplier, true).x;
  auto text = make_text();
  auto d = get_text_size(text, font);
  if (d.y <= 0) {
    return;
  }
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

void Director::render_spiral() const
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

  float offset = off3d(0.f, false).x +
      (_oculus.rendering_right ? float(view_width()) : 0.f);
  glUniform1f(glGetUniformLocation(_spiral_program, "offset"),
              _oculus.enabled ? offset : 0.f);
  glUniform1f(glGetUniformLocation(_spiral_program, "width"),
              float(_spiral_width));
  glUniform1f(glGetUniformLocation(_spiral_program, "spiral_type"),
              float(_spiral_type));
  glUniform4f(glGetUniformLocation(_spiral_program, "acolour"),
              _program->spiral_colour_a().r(), _program->spiral_colour_a().g(),
              _program->spiral_colour_a().b(), _program->spiral_colour_a().a());
  glUniform4f(glGetUniformLocation(_spiral_program, "bcolour"),
              _program->spiral_colour_b().r(), _program->spiral_colour_b().g(),
              _program->spiral_colour_b().b(), _program->spiral_colour_b().a());

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
  if (!_program->reverse_spiral_direction()) {
    amount *= -1;
  }
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
  if (force || random_chance(4)) {
    _current_font = _themes.get().get_font();
  }
}

void Director::change_subtext(bool alternate)
{
  static const uint32_t count = 16;
  _subtext.clear();
  for (uint32_t i = 0; i < 16; ++i) {
    auto s = _themes.get(alternate).get_text();
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

bool Director::change_themes()
{
  if (_switch_themes < 2048 || random_chance(4)) {
    return false;
  }
  if (_themes.change_themes()) {
    _switch_themes = 0;
    if (_current_font.empty()) {
      change_font(true);
    }
    return true;
  }
  return false;
}

bool Director::change_visual(uint32_t chance)
{
  // Like !random_chance(chance), but scaled to speed.
  if (_old_visual ||
      (chance && random(chance * _program->global_fps()) >= 120)) {
    return false;
  }
  _current_subfont = _themes.get().get_font();
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

  if (t == trance_pb::Program_VisualType_ACCELERATE) {
    _visual.reset(new AccelerateVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_SLOW_FLASH) {
    _visual.reset(new SlowFlashVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_SUB_TEXT) {
    _visual.reset(new SubTextVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_FLASH_TEXT) {
    _visual.reset(new FlashTextVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_PARALLEL) {
    _visual.reset(new ParallelVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_SUPER_PARALLEL) {
    _visual.reset(new SuperParallelVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_ANIMATION) {
    _visual.reset(new AnimationVisual{*this});
  }
  if (t == trance_pb::Program_VisualType_SUPER_FAST) {
    _visual.reset(new SuperFastVisual{*this});
  }
  return true;
}

bool Director::init_framebuffer(uint32_t& fbo, uint32_t& fb_tex,
                                uint32_t width, uint32_t height) const
{
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &fb_tex);

  glBindTexture(GL_TEXTURE_2D, fb_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glBindTexture(GL_TEXTURE_2D, fb_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, fb_tex, 0);

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

  ovrSizei eye_left = ovr_GetFovTextureSize(
      _oculus.session, ovrEyeType(0), desc.DefaultEyeFov[0], 1.0);
  ovrSizei eye_right = ovr_GetFovTextureSize(
      _oculus.session, ovrEyeType(1), desc.DefaultEyeFov[0], 1.0);
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

  auto result = ovr_CreateTextureSwapChainGL(
      _oculus.session, &texture_chain_desc, &_oculus.texture_chain);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain failed" << std::endl;
      ovrErrorInfo info;
  ovr_GetLastErrorInfo(&info);
  std::cerr << info.ErrorString << std::endl;
  }
  int texture_count = 0;
  result = ovr_GetTextureSwapChainLength(
      _oculus.session, _oculus.texture_chain, &texture_count);
  if (result != ovrSuccess) {
    std::cerr << "Oculus texture swap chain length failed" << std::endl;
  }
  for (int i = 0; i < texture_count; ++i) {
    GLuint fbo;
    GLuint fb_tex = 0;
    result = ovr_GetTextureSwapChainBufferGL(
        _oculus.session, _oculus.texture_chain, i, &fb_tex);
    if (result != ovrSuccess) {
      std::cerr << "Oculus texture swap chain buffer failed" << std::endl;
    }

    glGenFramebuffers(1, &fbo);
    _oculus.fbo_ovr.push_back(fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, fb_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      std::cerr << "framebuffer failed" << std::endl;
      return false;
    }
  }

  auto erd_left = ovr_GetRenderDesc(
      _oculus.session, ovrEye_Left, desc.DefaultEyeFov[0]);
  auto erd_right = ovr_GetRenderDesc(
      _oculus.session, ovrEye_Right, desc.DefaultEyeFov[1]);
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

sf::Vector2f Director::off3d(float multiplier, bool text) const
{
  float x = !_oculus.enabled || !multiplier ? 0.f :
      !_oculus.rendering_right ? _width / (8.f * multiplier) :
                                 _width / -(8.f * multiplier);
  x *= (text ? _system.oculus_text_depth() :
               _system.oculus_image_depth());
  return {x, 0};
}

uint32_t Director::view_width() const
{
  return _oculus.enabled ? _width / 2 : _width;
}

void Director::render_texture(float l, float t, float r, float b,
                              bool flip_h, bool flip_v) const
{
  glUniform2f(glGetUniformLocation(_image_program, "min_coord"),
              l / _width, t / _height);
  glUniform2f(glGetUniformLocation(_image_program, "max_coord"),
              r / _width, b / _height);
  glUniform2f(glGetUniformLocation(_image_program, "flip"),
              flip_h ? 1.f : 0.f, flip_v ? 1.f : 0.f);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Director::render_raw_text(const std::string& text, const Font& font,
                               const sf::Color& colour,
                               const sf::Vector2f& offset, float scale) const
{
  if (text.empty()) {
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

uint32_t Director::get_cached_text_size(const FontCache& cache,
                                        const std::string& text,
                                        const std::string& font) const
{
  static const uint32_t minimum_size = 2 * FontCache::char_size_lock;
  static const uint32_t increment = FontCache::char_size_lock;
  static const uint32_t maximum_size = 40 * FontCache::char_size_lock;
  static const uint32_t border_x = 250;
  static const uint32_t border_y = 150;

  uint32_t size = minimum_size;
  auto target_x = view_width() - border_x;
  auto target_y = std::min(_height / 3, _height - border_y);
  sf::Vector2f last_result;
  while (size < maximum_size) {
    const auto& loaded_font = cache.get_font(font, size);
    auto result = get_text_size(text, loaded_font);
    if (result.x > target_x || result.y > target_y || result == last_result) {
      break;
    }
    last_result = result;
    size *= 2;
  }
  size /= 2;
  last_result = sf::Vector2f{};
  while (size < maximum_size) {
    const auto& loaded_font = cache.get_font(font, size + increment);
    auto result = get_text_size(text, loaded_font);
    if (result.x > target_x || result.y > target_y || result == last_result) {
      break;
    }
    last_result = result;
    size += increment;
  }
  return size;
};

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