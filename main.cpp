#include <OVR_CAPI.h>
#include <SFML/Window.hpp>
#include <trance.pb.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "director.h"
#include "session.h"
#include "theme.h"

std::unique_ptr<sf::RenderWindow> create_window(
    const trance_pb::SystemConfiguration& system)
{
  // Call ovr_Initialize() before getting an OpenGL context.
  if (system.enable_oculus_rift()) {
    ovr_Initialize();
  }
  auto window = std::make_unique<sf::RenderWindow>();
  glClearDepth(1.f);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  auto video_mode = sf::VideoMode::getDesktopMode();
  window->create(
      video_mode, "trance",
      system.enable_oculus_rift() ? sf::Style::None : sf::Style::Fullscreen);

  window->setVerticalSyncEnabled(system.enable_vsync());
  window->setFramerateLimit(60);
  window->setVisible(true);
  window->setActive();
  window->display();
  return window;
}

void close_window(sf::RenderWindow& window,
                  const trance_pb::SystemConfiguration& system)
{
  window.close();
  if (system.enable_oculus_rift()) {
    ovr_Shutdown();
  }
}

const std::string& next_playlist_item(
    const trance_pb::PlaylistItem* item)
{
  uint32_t total = 0;
  for (const auto& next : item->next_item()) {
    total += next.random_weight();
  }
  auto r = random(total);
  total = 0;
  for (const auto& next : item->next_item()) {
    if (r < (total += next.random_weight())) {
      return next.playlist_item_name();
    }
  }
}

void play_session(const trance_pb::Session& session)
{
  static const std::string bad_alloc =
      "OUT OF MEMORY! TRY REDUCING USAGE IN SETTINGS...";

  const trance_pb::PlaylistItem* item =
      &session.playlist().find(session.first_playlist_item())->second;
  std::unordered_set<std::string> enabled_themes{
      item->program().enabled_theme_name().begin(),
      item->program().enabled_theme_name().end()};

  auto theme_bank = std::make_unique<ThemeBank>(session, enabled_themes);
  auto window = create_window(session.system());
  auto director = std::make_unique<Director>(
      *window, session, *theme_bank, item->program());

  std::atomic<bool> running = true;
  // Run the asynchronous load/unload thread.
  std::thread image_load_thread([&]{
    while (running) {
      try {
        theme_bank->async_update();
      }
      catch (std::bad_alloc&) {
        std::cerr << bad_alloc << std::endl;
        running = false;
        throw;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  float time = 0.f;
  float playlist_item_time = 0.f;
  sf::Clock clock;
  try {
    while (running) {
      sf::Event event;
      while (window->pollEvent(event)) {
        if (event.type == event.Closed ||
            (event.type == event.KeyPressed &&
             event.key.code == sf::Keyboard::Escape)) {
          running = false;
        }
        if (event.type == sf::Event::Resized) {
          glViewport(0, 0, event.size.width, event.size.height);
        }
      }

      playlist_item_time += clock.getElapsedTime().asSeconds();
      time += clock.getElapsedTime().asSeconds();
      clock.restart();

      if (item->play_time_seconds() &&
          playlist_item_time >= item->play_time_seconds()) {
        playlist_item_time -= item->play_time_seconds();
        auto next = next_playlist_item(item);
        std::cout << "changing to playlist item: " << next << std::endl;
        item = &session.playlist().find(next)->second;
        theme_bank->set_enabled_themes({
            item->program().enabled_theme_name().begin(),
            item->program().enabled_theme_name().end()});
        director->set_program(item->program());
      }
      if (theme_bank->swaps_to_match_theme()) {
        theme_bank->change_themes();
      }

      bool update = false;
      float frame_time = 1.f / item->program().global_fps();
      while (time >= frame_time) {
        update = true;
        time -= frame_time;
        director->update();
      }
      if (update) {
        director->render();
      }
    }
  }
  catch (std::bad_alloc&) {
    std::cerr << bad_alloc << std::endl;
    throw;
  }

  image_load_thread.join();
  // Destroy oculus HMD before calling ovr_Shutdown().
  director.reset();
  close_window(*window, session.system());
}

int main(int argc, char** argv)
{
  static const std::string session_cfg_path = "./session.cfg";
  trance_pb::Session session = load_session(session_cfg_path);
  save_session(session, session_cfg_path);
  play_session(session);
  return 0;
}