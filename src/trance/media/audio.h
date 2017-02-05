#ifndef TRANCE_SRC_TRANCE_MEDIA_AUDIO_H
#define TRANCE_SRC_TRANCE_MEDIA_AUDIO_H
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace trance_pb
{
  class AudioEvent;
  class PlaylistItem;
}
namespace sf
{
  class Music;
}

class Audio
{
public:
  Audio(const std::string& root_path);
  ~Audio();
  void TriggerEvents(const trance_pb::PlaylistItem& item);
  void TriggerEvent(const trance_pb::AudioEvent& event);
  void Update();

private:
  std::string _root_path;
  std::chrono::steady_clock _clock;
  struct channel {
    std::unique_ptr<sf::Music> music;
    std::uint32_t volume;

    bool current_fade;
    std::uint32_t fade_initial_volume;
    std::uint32_t fade_target_volume;
    std::uint32_t fade_time_seconds;
    std::chrono::steady_clock::time_point fade_start;
  };
  std::vector<channel> _channels;
};

#endif