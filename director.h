#ifndef TRANCE_DIRECTOR_H
#define TRANCE_DIRECTOR_H

#include <cstddef>
#include <memory>
#include <vector>
#include <SFML/Graphics.hpp>
#include "fonts.h"

struct Settings {
  Settings()
  : main_text_colour(255, 150, 200, 224)
  , shadow_text_colour(0, 0, 0, 192)
  , image_cache_size(150)
  , font_cache_size(10)
  {}
  sf::Color main_text_colour;
  sf::Color shadow_text_colour;
  std::size_t image_cache_size;
  std::size_t font_cache_size;

  static Settings settings;
};

struct Image;
class ImageBank;
class Program;
class Director {
public:

  Director(sf::RenderWindow& window,
           ImageBank& images, const std::vector<std::string>& fonts,
           std::size_t width, std::size_t height);
  ~Director();

  // Called from main().
  void update();
  void render() const;

  // Program API: called from Program objects to render and control the
  // various elements.
  Image get(bool alternate = false) const;
  const std::string& get_text(bool alternate = false) const;
  void maybe_upload_next() const;

  void render_image(const Image& image, float alpha) const;
  void render_text(const std::string& text) const;
  void render_subtext(float alpha) const;
  void render_spiral() const;

  void rotate_spiral(float amount);
  void change_spiral();
  void change_font(bool force = false);
  void change_subtext(bool alternate = false);
  bool change_sets();
  void change_program();

private:

  void render_texture(float l, float t, float r, float b,
                      bool flip_h, bool flip_v) const;

  sf::RenderWindow& _window;
  ImageBank& _images;
  FontCache _fonts;
  unsigned int _width;
  unsigned int _height;

  std::size_t _image_program;
  std::size_t _spiral_program;
  std::size_t _quad_buffer;
  std::size_t _tex_buffer;

  float _spiral;
  unsigned int _spiral_type;
  unsigned int _spiral_width;
  std::string _current_font;
  std::string _current_subfont;
  std::vector<std::string> _subtext;

  unsigned int _switch_sets;
  std::unique_ptr<Program> _program;
  std::unique_ptr<Program> _old_program;

};

#endif