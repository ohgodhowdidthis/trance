#include "audio.h"
#include <iostream>

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <SFML/Audio/Music.hpp>
#pragma warning(pop)

Audio::Audio(const std::string& root_path) : _root_path{root_path}
{
}

Audio::~Audio()
{
}

void Audio::TriggerEvents(const trance_pb::PlaylistItem& item)
{
  for (const auto& event : item.audio_event()) {
    TriggerEvent(event);
  }
}

void Audio::TriggerEvent(const trance_pb::AudioEvent& event)
{
  while (event.channel() >= _channels.size()) {
    _channels.emplace_back(channel{{}, 0, false, 0, 0, 0, {}});
    _channels.back().music.reset(new sf::Music);
  }
  auto& channel = _channels[event.channel()];

  if (event.type() == trance_pb::AudioEvent::AUDIO_PLAY) {
    if (!channel.music->openFromFile(_root_path + "/" + event.path())) {
      std::cerr << "\ncouldn't load " << event.path() << std::endl;
      return;
    }
    channel.music->setLoop(event.loop());
    channel.music->setVolume(float(event.volume()));
    channel.music->play();
    channel.volume = event.volume();
  } else if (event.type() == trance_pb::AudioEvent::AUDIO_STOP) {
    channel.music->stop();
  } else if (event.type() == trance_pb::AudioEvent::AUDIO_FADE) {
    channel.current_fade = true;
    channel.fade_initial_volume = channel.volume;
    channel.fade_target_volume = event.volume();
    channel.fade_time_seconds = event.time_seconds();
    channel.fade_start = _clock.now();
  }
}

void Audio::Update()
{
  for (auto& channel : _channels) {
    if (!channel.current_fade) {
      continue;
    }
    auto seconds = std::chrono::seconds(channel.fade_time_seconds);
    if (channel.fade_start + seconds < _clock.now()) {
      channel.volume = channel.fade_target_volume;
      channel.music->setVolume(float(channel.volume));
      channel.current_fade = false;
      continue;
    }
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(_clock.now() - channel.fade_start);
    auto r = float(elapsed_ms.count()) / (1000.f * channel.fade_time_seconds);
    r = std::max(0.f, std::min(1.f, r));
    auto volume = r * channel.fade_target_volume + (1 - r) * channel.fade_initial_volume;
    channel.volume = std::uint32_t(volume + .5f);
    channel.music->setVolume(volume);
  }
}