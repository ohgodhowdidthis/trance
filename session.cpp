#include "session.h"
#include <algorithm>
#include <fstream>
#include <SFML/Graphics/Color.hpp>
#include <google/protobuf/text_format.h>
#include <trance.pb.cc>

namespace {

  void set_default_program_types(trance_pb::ProgramConfiguration* program)
  {
    program->clear_type();

    auto type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_ACCELERATE);
    type->set_random_weight(1);

    type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_SLOW_FLASH);
    type->set_random_weight(1);

    type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_SUB_TEXT);
    type->set_random_weight(2);

    type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_FLASH_TEXT);
    type->set_random_weight(2);

    type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_PARALLEL);
    type->set_random_weight(2);

    type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_SUPER_PARALLEL);
    type->set_random_weight(2);

    type = program->add_type();
    type->set_type(trance_pb::ProgramConfiguration_Type_ANIMATION);
    type->set_random_weight(2);
  }

  void validate_colour(trance_pb::Colour* colour)
  {
    colour->set_r(std::max(0.f, std::min(1.f, colour->r())));
    colour->set_g(std::max(0.f, std::min(1.f, colour->g())));
    colour->set_b(std::max(0.f, std::min(1.f, colour->b())));
    colour->set_a(std::max(0.f, std::min(1.f, colour->a())));
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
  system->set_enable_oculus_rift(true);
  system->set_image_cache_size(64);
  system->set_font_cache_size(8);

  auto program = session.mutable_program();
  set_default_program_types(program);
  *program->mutable_main_text_colour() = sf2colour({255, 150, 200, 224});
  *program->mutable_shadow_text_colour() = sf2colour({0, 0, 0, 192});

  validate_session(session);
  return session;
}

void validate_session(trance_pb::Session& session)
{
  auto system = session.mutable_system();
  system->set_image_cache_size(std::max(6u, system->image_cache_size()));
  system->set_font_cache_size(std::max(2u, system->image_cache_size()));

  auto program = session.mutable_program();
  unsigned int count = 0;
  for (const auto& type : program->type()) {
    count += type.random_weight();
  }
  if (!count) {
    set_default_program_types(program);
  }
  validate_colour(program->mutable_main_text_colour());
  validate_colour(program->mutable_shadow_text_colour());
}
