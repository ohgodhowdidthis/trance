#include "session.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <SFML/Graphics/Color.hpp>
#include <google/protobuf/text_format.h>
#include <trance.pb.cc>

namespace {

std::string split_text_line(const std::string& text)
{
  // Split strings into two lines at the space closest to the middle. This is
  // sort of ad-hoc. There should probably be a better way that can judge length
  // and split over more than two lines.
  auto l = text.length() / 2;
  auto r = l;
  while (true) {
    if (text[r] == ' ') {
      return text.substr(0, r) + '\n' + text.substr(r + 1);
    }

    if (text[l] == ' ') {
      return text.substr(0, l) + '\n' + text.substr(l + 1);
    }

    if (l == 0 || r == text.length() - 1) {
      break;
    }
    --l;
    ++r;
  }
  return text;
}

void set_default_visual_types(trance_pb::Program* program)
{
  program->clear_visual_type();

  auto type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_ACCELERATE);
  type->set_random_weight(1);

  type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_SLOW_FLASH);
  type->set_random_weight(1);

  type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_SUB_TEXT);
  type->set_random_weight(2);

  type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_FLASH_TEXT);
  type->set_random_weight(2);

  type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_PARALLEL);
  type->set_random_weight(2);

  type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_SUPER_PARALLEL);
  type->set_random_weight(2);

  type = program->add_visual_type();
  type->set_type(trance_pb::Program_VisualType_ANIMATION);
  type->set_random_weight(2);
}

void validate_colour(trance_pb::Colour* colour)
{
  colour->set_r(std::max(0.f, std::min(1.f, colour->r())));
  colour->set_g(std::max(0.f, std::min(1.f, colour->g())));
  colour->set_b(std::max(0.f, std::min(1.f, colour->b())));
  colour->set_a(std::max(0.f, std::min(1.f, colour->a())));
}

void search_resources(trance_pb::Session& session)
{
  static const std::string wildcards = "/wildcards/";
  auto& themes = *session.mutable_theme_map();

  std::tr2::sys::path path(".");
  for (auto it = std::tr2::sys::recursive_directory_iterator(path);
       it != std::tr2::sys::recursive_directory_iterator(); ++it) {
    if (std::tr2::sys::is_regular_file(it->status())) {
      std::string ext = it->path().extension();
      for (auto& c : ext) {
        c = tolower(c);
      }

      auto jt = ++it->path().begin();
      if (jt == it->path().end()) {
        continue;
      }
      auto theme_name = jt == --it->path().end() ? wildcards : *jt;

      if (ext == ".ttf") {
        themes[theme_name].add_font_path(it->path());
      }
      else if (ext == ".txt") {
        std::ifstream f(it->path());
        std::string line;
        while (std::getline(f, line)) {
          if (!line.length()) {
            continue;
          }
          for (auto& c : line) {
            c = toupper(c);
          }
          themes[theme_name].add_text_line(split_text_line(line));
        }
      }
      // Should really check is_gif_animated(), but it takes far too long.
      else if (ext == ".webm" || ext == ".gif") {
        themes[theme_name].add_animation_path(it->path());
      }
      else if (ext == ".png" || ext == ".bmp" ||
               ext == ".jpg" || ext == ".jpeg") {
        themes[theme_name].add_image_path(it->path());
      }
    }
  }

  // Merge wildcards theme into all others.
  for (auto& pair : themes) {
    if (pair.first == wildcards) {
      continue;
    }
    for (const auto& s : themes[wildcards].image_path()) {
      pair.second.add_image_path(s);
    }
    for (const auto& s : themes[wildcards].animation_path()) {
      pair.second.add_animation_path(s);
    }
    for (const auto& s : themes[wildcards].font_path()) {
      pair.second.add_font_path(s);
    }
    for (const auto& s : themes[wildcards].text_line()) {
      pair.second.add_text_line(s);
    }
  }

  // Leave wildcards theme if there are no others.
  if (themes.size() == 1) {
    themes["default"] = themes[wildcards];
  }
  themes.erase(wildcards);
  for (auto& pair : themes) {
    session.mutable_program()->add_enabled_theme_name(pair.first);
  }
}

}

sf::Color colour2sf(const trance_pb::Colour& colour)
{
  return sf::Color(
      sf::Uint8(colour.r() * 255),
      sf::Uint8(colour.g() * 255),
      sf::Uint8(colour.b() * 255),
      sf::Uint8(colour.a() * 255));
}

trance_pb::Colour sf2colour(const sf::Color& colour)
{
  trance_pb::Colour result;
  result.set_r(colour.r / 255.f);
  result.set_g(colour.g / 255.f);
  result.set_b(colour.b / 255.f);
  result.set_a(colour.a / 255.f);
  return result;
}

trance_pb::Session load_session(const std::string& path)
{
  trance_pb::Session session;
  bool loaded_okay = false;
  std::ifstream f{path};
  if (f) {
    std::string str{std::istreambuf_iterator<char>{f},
                    std::istreambuf_iterator<char>{}};
    loaded_okay = google::protobuf::TextFormat::ParseFromString(str, &session);
  }

  if (loaded_okay) {
    validate_session(session);
    return session;
  }
  return get_default_session();
}

void save_session(const trance_pb::Session& session, const std::string& path)
{
  std::string str;
  google::protobuf::TextFormat::PrintToString(session, &str);
  std::ofstream f{path};
  f << str;
}

trance_pb::Session get_default_session()
{
  trance_pb::Session session;

  auto system = session.mutable_system();
  system->set_enable_vsync(false);
  system->set_enable_oculus_rift(true);
  system->set_oculus_image_depth(1.f);
  system->set_oculus_text_depth(1.f);
  system->set_image_cache_size(64);
  system->set_font_cache_size(8);

  auto program = session.mutable_program();
  set_default_visual_types(program);
  program->set_global_fps(120);
  program->set_zoom_intensity(.2f);
  *program->mutable_spiral_colour_a() = sf2colour({255, 150, 200, 50});
  *program->mutable_spiral_colour_b() = sf2colour({0, 0, 0, 50});
  program->set_reverse_spiral_direction(false);

  *program->mutable_main_text_colour() = sf2colour({255, 150, 200, 224});
  *program->mutable_shadow_text_colour() = sf2colour({0, 0, 0, 192});

  search_resources(session);
  validate_session(session);
  return session;
}

void validate_session(trance_pb::Session& session)
{
  auto system = session.mutable_system();
  system->set_image_cache_size(std::max(6u, system->image_cache_size()));
  system->set_font_cache_size(std::max(2u, system->image_cache_size()));

  auto program = session.mutable_program();
  uint32_t count = 0;
  for (const auto& type : program->visual_type()) {
    count += type.random_weight();
  }
  if (!count) {
    set_default_visual_types(program);
  }
  program->set_global_fps(std::max(1u, std::min(240u, program->global_fps())));
  program->set_zoom_intensity(
    std::max(0.f, std::min(1.f, program->zoom_intensity())));
  validate_colour(program->mutable_spiral_colour_a());
  validate_colour(program->mutable_spiral_colour_b());
  validate_colour(program->mutable_main_text_colour());
  validate_colour(program->mutable_shadow_text_colour());
}