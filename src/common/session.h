#ifndef TRANCE_SRC_COMMON_SESSION_H
#define TRANCE_SRC_COMMON_SESSION_H
#include <string>
#include <unordered_map>
#include <vector>

std::string make_relative(const std::string& from, const std::string& to);

namespace trance_pb
{
  class Colour;
  class PlaylistItem_NextItem;
  class Session;
  class System;
  class Theme;
}

bool is_image(const std::string& path);
bool is_animation(const std::string& path);
bool is_font(const std::string& path);
bool is_text_file(const std::string& path);
bool is_audio_file(const std::string& path);

bool is_enabled(const trance_pb::PlaylistItem_NextItem& next_item,
                const std::unordered_map<std::string, std::string>& variables);

void search_resources(trance_pb::Session& session, const std::string& root);
void search_resources(trance_pb::Theme& theme, const std::string& root);
void search_audio_files(std::vector<std::string>& files, const std::string& root);

trance_pb::System load_system(const std::string& path);
void save_system(const trance_pb::System&, const std::string& path);
trance_pb::System get_default_system();
void validate_system(trance_pb::System& session);

trance_pb::Session load_session(const std::string& path);
void save_session(const trance_pb::Session& session, const std::string& path);
trance_pb::Session get_default_session();
void validate_session(trance_pb::Session& session);

#endif