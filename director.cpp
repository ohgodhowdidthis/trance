#include "director.h"
#include "images.h"
#include "program.h"
#include "util.h"
#include "glew/include/GL/glew.h"
#include <SFML/OpenGL.hpp>
#include <iostream>

Settings Settings::settings;
static const unsigned int spiral_type_max = 7;

static const std::string image_vertex = R"(
uniform vec2 min;
uniform vec2 max;
uniform vec2 flip;
uniform float alpha;
attribute vec2 position;
attribute vec2 texcoord;
noperspective varying vec2 vtexcoord;
flat varying float valpha;

void main()
{
  vec2 pos = position.xy / 2 + 0.5;
  pos = pos * (max - min) + min;
  pos = (pos - 0.5) * 2;
  gl_Position = vec4(pos, 0.0, 1.0);
  vtexcoord = vec2(flip.x != 0 ? 1.0 - texcoord.x : texcoord.x,
                   flip.y != 0 ? 1.0 - texcoord.y : texcoord.y);
  valpha = alpha;
}
)";
static const std::string image_fragment = R"(
uniform sampler2D texture;
noperspective varying vec2 vtexcoord;
flat varying float valpha;

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
  return r < 1 ? pow(r1, 6) : -pow(r2, 6);
}

float spiral7(float r)
{
  float m = mod(r, 0.2);
  m = m < 0.1 ? m : 0.2 - m;
  return r + m * 3;
}

void main(void)
{
  vec2 aspect = vec2(resolution.x / resolution.y, 1.0);
	vec2 position =
      -aspect.xy + 2.0 * gl_FragCoord.xy / resolution.xy * aspect.xy;
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
  float amod = mod(angle - width * time - 2 * width * factor, width);
  float v = amod < width / 2 ? 0.0 : 1.0;
  float t = 0.2 + 2.0 * (1.0 - pow(min(1.0, radius), 0.4));
  if (amod > width / 2 - t && amod < width / 2 + t) {
    v = (amod - width / 2 + t) / (2 * t);
  }
  if (amod < t) {
    v = 1.0 - (amod + t) / (2 * t);
  }
  if (amod > width - t) {
    v = 1.0 - (amod - width + t) / (2 * t);
  }
  gl_FragColor = vec4(v, v, v,
                      0.2 * min(1.0, radius * 2));
}
)";

Director::Director(sf::RenderWindow& window,
                   ImageBank& images, const std::vector<std::string>& fonts,
                   std::size_t width, std::size_t height)
: _window{window}
, _images{images}
, _fonts{fonts}
, _width{width}
, _height{height}
, _image_program{0}
, _spiral_program{0}
, _quad_buffer{0}
, _tex_buffer{0}
, _spiral{0}
, _spiral_type{0}
, _spiral_width{0}
, _switch_sets{0}
{
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

  glewInit();
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
    0.005f, 0.995f,
    0.995f, 0.995f,
    0.005f, 0.005f,
    0.995f, 0.995f,
    0.995f, 0.005f,
    0.005f, 0.005f};
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
}

void Director::update()
{
  ++_switch_sets;
  if (_old_program) {
    _old_program.reset(nullptr);
  }
  _program->update();
}

void Director::render() const
{
  _program->render();
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

void Director::render_image(const Image& image, float alpha) const
{
  glEnable(GL_BLEND);
  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(_image_program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, image.texture);
  glUniform1ui(glGetUniformLocation(_image_program, "texture"), 0);
  glUniform1f(glGetUniformLocation(_image_program, "alpha"), alpha);

  GLuint ploc = glGetAttribLocation(_image_program, "position");
  glEnableVertexAttribArray(ploc);
	glBindBuffer(GL_ARRAY_BUFFER, _quad_buffer);
	glVertexAttribPointer(ploc, 2, GL_FLOAT, false, 0, 0);

  GLuint tloc = glGetAttribLocation(_image_program, "texcoord");
  glEnableVertexAttribArray(tloc);
	glBindBuffer(GL_ARRAY_BUFFER, _tex_buffer);
	glVertexAttribPointer(tloc, 2, GL_FLOAT, false, 0, 0);

  auto x = float(image.width);
  auto y = float(image.height);
  if (y * _width / x > _height) {
    auto scale = float(_height) / y;
    y = float(_height);
    x *= scale;

    render_texture(_width / 2 - x / 2, 0.f,
                   _width / 2 + x / 2, float(_height),
                   false, false);
    int i = 1;
    while (_width / 2 - i * x + x / 2 >= 0) {
      render_texture(_width / 2 - i * x - x / 2, 0.f,
                     _width / 2 - i * x + x / 2, float(_height),
                     i % 2 != 0, false);
      render_texture(_width / 2 + i * x - x / 2, 0.f,
                     _width / 2 + i * x + x / 2, float(_height),
                     i % 2 != 0, false);
      ++i;
    }
  }
  else {
    auto scale = float(_width) / x;
    x = float(_width);
    y *= scale;

    render_texture(0.f, _height / 2 - y / 2,
                   float(_width), _height / 2 + y / 2,
                   false, false);
    int i = 1;
    while (_height / 2 - i * y + y / 2 >= 0) {
      render_texture(0.f, _height / 2 - i * y - y / 2,
                     float(_width), _height / 2 - i * y + y / 2,
                     false, i % 2 != 0);
      render_texture(0.f, _height / 2 + i * y - y / 2,
                     float(_width), _height / 2 + i * y + y / 2,
                     false, i % 2 != 0);
      ++i;
    }
  }

  glDisableVertexAttribArray(ploc);
	glDisableVertexAttribArray(tloc);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Director::render_text(const std::string& text) const
{
  static const std::size_t default_size = 200;
  static const std::size_t border = 100;
  static const std::size_t shadow_extra = 60;
  if (_current_font.empty()) {
    return;
  }

  auto make_text = [&](const sf::Color& colour, std::size_t size)
  {
    auto t = _fonts.get_text(text, _current_font, size);
    t.setColor(colour);
    return t;
  };

  auto fit_text = [&](const sf::Color& colour, std::size_t size, bool fix)
  {
    auto t = make_text(colour, size);
    auto r = t.getLocalBounds();

    while (!fix && r.width > _width - border) {
      int new_size = size * (_width - border);
      new_size /= int(r.width);
      t = make_text(colour, new_size);
      r = t.getLocalBounds();
    }

    t.setOrigin(r.left + r.width / 2, r.top + r.height / 2);
    t.setPosition(_width / 2.f, _height / 2.f);
    return t;
  };

  auto main = fit_text(
      Settings::settings.main_text_colour, default_size, false);
  auto shadow = fit_text(
      Settings::settings.shadow_text_colour,
      default_size + shadow_extra, true);

  _window.pushGLStates();
  _window.draw(shadow);
  _window.draw(main);
  _window.popGLStates();
}

void Director::render_subtext(float alpha) const
{
  static const unsigned int char_size = 100;
  std::size_t n = 0;

  auto make_text = [&]
  {
    auto t = _fonts.get_text("", _current_subfont, char_size);
    t.setColor({0, 0, 0, sf::Uint8(alpha * 255)});

    sf::FloatRect r;
    do {
      t.setString(t.getString() + " " + _subtext[n]);
      n = (n + 1) % _subtext.size();
      r = t.getLocalBounds();
    }
    while (r.width < _width);

    t.setOrigin(r.left + r.width / 2, r.top + r.height / 2);
    t.setPosition(_width / 2.f, _height / 2.f);
    return t;
  };

  auto text = make_text();
  auto r = text.getLocalBounds();
  r.left += text.getPosition().x;
  r.top += text.getPosition().y;

  _window.pushGLStates();
  _window.draw(text);
  int i = 1;
  auto offset = r.height + 4;
  for (; r.top + i * offset < _height; ++i) {
    text = make_text();
    text.setPosition(text.getPosition().x, text.getPosition().y + i * offset);
    _window.draw(text);

    text = make_text();
    text.setPosition(text.getPosition().x, text.getPosition().y - i * offset);
    _window.draw(text);
  }
  _window.popGLStates();
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
              float(_width), float(_height));
  glUniform1f(glGetUniformLocation(_spiral_program, "width"),
              float(_spiral_width));
  glUniform1f(glGetUniformLocation(_spiral_program, "spiral_type"),
              float(_spiral_type));

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
  if (_subtext.empty()) {
    return;
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

  auto r = random(10);
  if (r == 0) {
    _program.reset(new AccelerateProgram{*this, random_chance()});
  }
  else if (r == 1) {
    _program.reset(new SlowFlashProgram{*this});
  }
  else if (r == 2 || r == 3) {
    _program.reset(new SubTextProgram{*this});
  }
  else if (r == 4 || r == 5) {
    _program.reset(new ParallelProgram{*this});
  }
  else if (r == 6 || r == 7) {
    _program.reset(new SuperParallelProgram{*this});
  }
  else {
    _program.reset(new FlashTextProgram{*this});
  }
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