#include <SFML/Window.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "images.h"
#include "director.h"

static const std::string settings_cfg_path = "./settings.cfg";
static const std::string settings_cfg_contents = R"(\
# number of images to keep in memory at a time
# (uses up both RAM and video card memory)
image_cache_size 120

# number of font sizes to keep in memory at a time
# each character size of a single font uses another
# slot in the cache
# (uses up video card memory)
font_cache_size 10

# rgba of the main text
main_text_colour 255 150 200 224

# rgba of the drop shadow
shadow_text_color 0 0 0 192
)";

struct program_data {
  std::unordered_map<std::string, std::vector<std::string>> images;
  std::unordered_map<std::string, std::vector<std::string>> texts;
  std::vector<std::string> fonts;
};

void search_data(program_data& data)
{
  std::vector<std::string> wildcards;
  std::vector<std::string> text_wildcards;
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

      if (ext == ".ttf") {
        data.fonts.push_back(it->path());
      }
      if (ext == ".txt") {
        std::ifstream f(it->path());
        std::string line;
        while (std::getline(f, line)) {
          if (!line.length()) {
            continue;
          }
          for (auto& c : line) {
            c = toupper(c);
          }
          if (jt == --it->path().end()) {
            text_wildcards.push_back(line);
          }
          else {
            data.texts[*jt].push_back(line);
          }
        }
      }
      if (ext != ".gif" && ext != ".png" && ext != ".bmp" &&
          ext != ".jpg" && ext != ".jpeg") {
        continue;
      }

      if (jt == --it->path().end()) {
        wildcards.push_back(it->path());
      }
      else {
        data.images[*jt].push_back(it->path());
      }
    }
  }

  for (auto& pair : data.images) {
    for (const auto& s : wildcards) {
      pair.second.push_back(s);
    }
    data.texts[pair.first].begin();
  }
  for (auto& pair : data.texts) {
    for (const auto& s : text_wildcards) {
      pair.second.push_back(s);
    }
  }
}

void load_settings()
{
  std::ifstream f(settings_cfg_path);
  if (!f) {
    // Write default file.
    std::ofstream f(settings_cfg_path);
    f << settings_cfg_contents;
    f.close();
    return;
  }

  std::string line;
  while (std::getline(f, line)) {
    if (!line.length()) {
      continue;
    }

    std::stringstream ss(line);
    std::string str;
    unsigned int val;
    ss >> str;
    if (str == "image_cache_size") {
      ss >> Settings::settings.image_cache_size;
    }
    if (str == "font_cache_size") {
      ss >> Settings::settings.font_cache_size;
    }
    if (str == "main_text_colour") {
      ss >> val;
      Settings::settings.main_text_colour.r = val;
      ss >> val;
      Settings::settings.main_text_colour.g = val;
      ss >> val;
      Settings::settings.main_text_colour.b = val;
      ss >> val;
      Settings::settings.main_text_colour.a = val;
    }
    if (str == "shadow_text_colour") {
      ss >> val;
      Settings::settings.shadow_text_colour.r = val;
      ss >> val;
      Settings::settings.shadow_text_colour.g = val;
      ss >> val;
      Settings::settings.shadow_text_colour.b = val;
      ss >> val;
      Settings::settings.shadow_text_colour.a = val;
    }
  }
}

int main(int argc, char** argv)
{
  program_data data;
  search_data(data);
  load_settings();

  if (data.images.empty()) {
    std::cerr << "no images found" << std::endl;
    return 1;
  }

  sf::RenderWindow window;
  glClearDepth(1.f);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ImageBank images;
  for (const auto& p : data.images) {
    images.add_set(p.second, data.texts[p.first]);
  }
  images.initialise();

  auto video_mode = sf::VideoMode::getDesktopMode();
  window.create(video_mode, "Ubtrance", sf::Style::Fullscreen);
  window.setVerticalSyncEnabled(false);
  window.setFramerateLimit(60);
  window.setVisible(true);
  window.setActive();
  window.display();

  Director director(
      window, images, data.fonts,
      video_mode.width, video_mode.height);
  const float frame_time = 1.f / 120;
  bool running = true;

  // Run the asynchronous load/unload thread.
  std::thread image_load_thread([&]{
    while (running) {
      images.async_update();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  float time = 0.f;
  sf::Clock clock;
  while (running) {
    sf::Event event;
    while (window.pollEvent(event)) {
      if (event.type == event.Closed ||
          (event.type == event.KeyPressed && event.key.code == sf::Keyboard::Escape)) {
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
      director.update();
    }
    if (update) {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      director.render();
      window.display();
    }
  }
  window.close();
  image_load_thread.join();
  return 0;
}