#include <OVR_CAPI.h>
#include <SFML/Window.hpp>
#include <trance.pb.h>
#include <iostream>
#include <string>
#include <thread>
#include "director.h"
#include "images.h"
#include "session.h"

int main(int argc, char** argv)
{
  static const std::string bad_alloc =
      "OUT OF MEMORY! TRY REDUCING USAGE IN SETTINGS...";

  static const std::string session_cfg_path = "./session.cfg";
  trance_pb::Session session = load_session(session_cfg_path);
  save_session(session, session_cfg_path);
  if (session.system().enable_oculus_rift()) {
    ovr_Initialize();
  }

  sf::RenderWindow window;
  glClearDepth(1.f);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  std::vector<trance_pb::ImageSet> sets;
  for (const auto& set : session.image_set()) {
    bool enabled = false;
    for (const auto& name : session.resource().enabled_image_set_name()) {
      if (name == set.image_set_name()) {
        enabled = true;
        break;
      }
    }
    if (!enabled) {
      continue;
    }

    std::cout << "set " << set.image_set_name() << " with " <<
        set.image_path_size() << " image(s), " <<
        set.animation_path_size() << " animation(s), and " <<
        set.text_line_size() << " line(s) of text" << std::endl;
    sets.push_back(set);
  }
  ImageBank images{sets, session.system().image_cache_size()};

  auto video_mode = sf::VideoMode::getDesktopMode();
  window.create(video_mode, "Ubtrance",
                session.system().enable_oculus_rift() ?
                sf::Style::None : sf::Style::Fullscreen);
  window.setVerticalSyncEnabled(false);
  window.setFramerateLimit(60);
  window.setVisible(true);
  window.setActive();
  window.display();

  std::vector<std::string> font_paths{
      session.resource().enabled_font_path().begin(),
      session.resource().enabled_font_path().end()};
  auto director = std::make_unique<Director>(
      window, session, images, font_paths,
      video_mode.width, video_mode.height);
  bool running = true;

  // Run the asynchronous load/unload thread.
  std::thread image_load_thread([&]{
    while (running) {
      try {
        images.async_update();
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
  sf::Clock clock;
  try {
    while (running) {
      sf::Event event;
      while (window.pollEvent(event)) {
        if (event.type == event.Closed ||
            (event.type == event.KeyPressed &&
             event.key.code == sf::Keyboard::Escape)) {
          running = false;
        }
        if (event.type == sf::Event::Resized) {
          glViewport(0, 0, event.size.width, event.size.height);
        }
      }

      time += clock.getElapsedTime().asSeconds();
      clock.restart();
      bool update = false;
      while (time >= director->get_frame_time()) {
        update = true;
        time -= director->get_frame_time();
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
  window.close();
  image_load_thread.join();
  director.reset();
  ovr_Shutdown();
  return 0;
}