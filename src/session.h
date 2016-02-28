#ifndef TRANCE_SESSION_H
#define TRANCE_SESSION_H

#include <string>

namespace trance_pb {
  class Colour;
  class Session;
  class System;
  class Theme;
}

bool is_image(const std::string& path);
bool is_animation(const std::string& path);
bool is_font(const std::string& path);
bool is_text_file(const std::string& path);

void search_resources(trance_pb::Session& session, const std::string& root);
void search_resources(trance_pb::Theme& theme, const std::string& root);

trance_pb::System load_system(const std::string& path);
void save_system(const trance_pb::System&, const std::string& path);
trance_pb::System get_default_system();
void validate_system(trance_pb::System& session);

trance_pb::Session load_session(const std::string& path);
void save_session(const trance_pb::Session& session, const std::string& path);
trance_pb::Session get_default_session();
void validate_session(trance_pb::Session& session);

#endif