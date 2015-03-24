#include <OVR_CAPI.h>
#include <SFML/Window.hpp>
#include <trance.pb.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "director.h"
#include "images.h"
#include "session.h"

struct program_set {
  std::vector<std::string> images;
  std::vector<std::string> texts;
  std::vector<std::string> animations;
};

struct program_data {
  std::unordered_map<std::string, program_set> sets;
  std::vector<std::string> fonts;
};

void search_data(program_data& data)
{
  static const std::string wildcards = "/wildcards/";
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
      auto set_name = jt == --it->path().end() ? wildcards : *jt;

      if (ext == ".ttf") {
        data.fonts.push_back(it->path());
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
          data.sets[set_name].texts.push_back(line);
        }
      }
      else if (ext == ".gif" || ext == ".webm") {
        data.sets[set_name].animations.push_back(it->path());
      }
      else if (ext == ".png" || ext == ".bmp" ||
               ext == ".jpg" || ext == ".jpeg") {
        data.sets[set_name].images.push_back(it->path());
      }
    }
  }

  // Merge wildcards set into all others.
  for (auto& pair : data.sets) {
    if (pair.first == wildcards) {
      continue;
    }
    for (const auto& s : data.sets[wildcards].images) {
      pair.second.images.push_back(s);
    }
    for (const auto& s : data.sets[wildcards].texts) {
      pair.second.texts.push_back(s);
    }
    for (const auto& s : data.sets[wildcards].animations) {
      pair.second.animations.push_back(s);
    }
  }
  // Erase any sets with no images.
  for (auto it = data.sets.begin(); it != data.sets.end();) {
    if (it->second.images.empty()) {
      it = data.sets.erase(it);
    }
    else {
      ++it;
    }
  }
  // Leave wildcards set if there are no others.
  if (data.sets.size() == 1) {
    data.sets["default"] = data.sets[wildcards];
  }
  data.sets.erase(wildcards);
}

int main(int argc, char** argv)
{
  static const std::string bad_alloc =
      "OUT OF MEMORY! TRY REDUCING USAGE IN SETTINGS...";
  program_data data;
  search_data(data);
  // Currently there must be at least one set.
  if (data.sets.empty()) {
    std::cerr << "NO IMAGES FOUND!" << std::endl;
    return 1;
  }

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

  ImageBank images{session.system().image_cache_size()};
  for (const auto& p : data.sets) {
    std::cout << "set " << p.first << " with " <<
        p.second.images.size() << " image(s), " <<
        p.second.animations.size() << " animation(s), and " <<
        p.second.texts.size() << " line(s) of text" << std::endl;
    images.add_set(p.second.images, p.second.texts, p.second.animations);
  }
  images.initialise();

  auto video_mode = sf::VideoMode::getDesktopMode();
  window.create(video_mode, "Ubtrance",
                session.system().enable_oculus_rift() ?
                sf::Style::None : sf::Style::Fullscreen);
  window.setVerticalSyncEnabled(false);
  window.setFramerateLimit(60);
  window.setVisible(true);
  window.setActive();
  window.display();

  auto director = std::make_unique<Director>(
      window, session, images, data.fonts,
      video_mode.width, video_mode.height);
  const float frame_time = 1.f / 120;
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
  window.close();
  image_load_thread.join();
  director.reset();
  ovr_Shutdown();
  return 0;
}