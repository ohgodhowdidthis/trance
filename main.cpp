#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "director.h"
#include "export.h"
#include "session.h"
#include "theme.h"

#pragma warning(push, 0)
#include <gflags/gflags.h>
#include <OVR_CAPI.h>
#include <SFML/Window.hpp>
#include <trance.pb.h>
#pragma warning(pop)

std::unique_ptr<Exporter> create_exporter(const exporter_settings& settings)
{
  std::unique_ptr<Exporter> exporter;
  if (ext_is(settings.path, "jpg") || ext_is(settings.path, "png") ||
      ext_is(settings.path, "bmp")) {
    exporter = std::make_unique<FrameExporter>(settings);
  }
  if (ext_is(settings.path, "webm")) {
    exporter = std::make_unique<WebmExporter>(settings);
  }
  if (ext_is(settings.path, "h264")) {
    exporter = std::make_unique<H264Exporter>(settings);
  }
  return exporter && exporter->success() ?
      std::move(exporter) : std::unique_ptr<Exporter>{};
}

std::unique_ptr<sf::RenderWindow> create_window(
    const trance_pb::SystemConfiguration& system,
    uint32_t width, uint32_t height, bool visible)
{
  // Call ovr_Initialize() before getting an OpenGL context.
  if (system.enable_oculus_rift()) {
    ovr_Initialize();
  }
  auto window = std::make_unique<sf::RenderWindow>();
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  auto video_mode = sf::VideoMode::getDesktopMode();
  if (width && height) {
    video_mode.width = width;
    video_mode.height = height;
  }
  auto style = !visible || system.enable_oculus_rift() ?
      sf::Style::None : sf::Style::Fullscreen;
  window->create(video_mode, "trance", style);

  window->setVerticalSyncEnabled(system.enable_vsync());
  window->setFramerateLimit(60);
  window->setVisible(visible);
  window->setActive();
  if (visible) {
    window->display();
  }
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
  static std::string never_happens;
  return never_happens;
}

static const std::string bad_alloc =
    "OUT OF MEMORY! TRY REDUCING USAGE IN SETTINGS...";
static const uint32_t async_millis = 10;

std::thread run_async_thread(std::atomic<bool>& running, ThemeBank& bank)
{
  // Run the asynchronous load/unload thread.
  return std::thread{[&]{
    while (running) {
      try {
        bank.async_update();
      }
      catch (std::bad_alloc&) {
        std::cerr << bad_alloc << std::endl;
        running = false;
        throw;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(async_millis));
    }
  }};
}

void handle_events(std::atomic<bool>& running, sf::RenderWindow* window)
{
  sf::Event event;
  while (window && window->pollEvent(event)) {
    if (event.type == event.Closed ||
        (event.type == event.KeyPressed &&
          event.key.code == sf::Keyboard::Escape)) {
      running = false;
    }
    if (event.type == sf::Event::Resized) {
      glViewport(0, 0, event.size.width, event.size.height);
    }
  }
}

void print_info(const sf::Clock& clock,
                uint32_t frames, uint32_t total_frames)
{
  auto format_time = [](uint32_t seconds) {
    auto minutes = seconds / 60;
    seconds = seconds % 60;
    auto hours = minutes / 60;
    minutes = minutes % 60;

    std::string result;
    if (hours) {
      result += std::to_string(hours) + ":";
    }
    result += (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
        (seconds < 10 ? "0" : "") + std::to_string(seconds);
    return result;
  };

  float completion = float(frames) / total_frames;
  auto elapsed = uint32_t(clock.getElapsedTime().asSeconds() + .5f);
  auto eta = uint32_t(.5f + (completion ?
      clock.getElapsedTime().asSeconds() * (1.f / completion - 1.f) : 0.f));
  auto percentage = uint32_t(100 * completion);

  std::cout << std::endl <<
      "frame: " << frames << " / " << total_frames << " [" << percentage <<
      "%]; elapsed: " << format_time(elapsed) << "; eta: " <<
      format_time(eta) << std::endl;
}

void play_session(
    const trance_pb::Session& session, const exporter_settings& settings)
{
  bool realtime = settings.path.empty();
  auto exporter = create_exporter(settings);
  if (!realtime && !exporter) {
    std::cerr << "don't know how to export that format" << std::endl;
    return;
  }

  const trance_pb::PlaylistItem* item =
      &session.playlist().find(session.first_playlist_item())->second;
  std::unordered_set<std::string> enabled_themes{
      item->program().enabled_theme_name().begin(),
      item->program().enabled_theme_name().end()};

  auto theme_bank = std::make_unique<ThemeBank>(session, enabled_themes);
  auto window = create_window(
      session.system(),
      realtime ? 0 : settings.width, realtime ? 0 : settings.height, realtime);
  auto director = std::make_unique<Director>(
      *window, session, *theme_bank, item->program(),
      realtime, exporter && exporter->requires_yuv_input());

  std::thread async_thread;
  std::atomic<bool> running = true;
  if (realtime) {
    async_thread = run_async_thread(running, *theme_bank);
  }

  float update_time = 0.f;
  float async_update_time = 0.f;
  float playlist_item_time = 0.f;
  uint32_t frames = 0;
  sf::Clock clock;
  try {
    while (running) {
      handle_events(running, window.get());

      float elapsed = 0.f;
      if (realtime) {
        elapsed = clock.getElapsedTime().asSeconds();
        clock.restart();
      }
      else {
        elapsed = 1.f / settings.fps;
        uint32_t total_frames = settings.length * settings.fps;
        if (frames % 8 == 0) {
          print_info(clock, frames, total_frames);
        }
        if (frames >= total_frames) {
          running = false;
          break;
        }
        ++frames;
      }
      update_time += elapsed;
      async_update_time += elapsed;
      playlist_item_time += elapsed;

      float async_frame_time = async_millis * 1.f / 1000;
      while (!realtime && async_update_time >= async_frame_time) {
        async_update_time -= async_frame_time;
        theme_bank->async_update();
      }

      if (item->play_time_seconds() &&
          playlist_item_time >= item->play_time_seconds()) {
        playlist_item_time -= item->play_time_seconds();
        auto next = next_playlist_item(item);
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
      while (update_time >= frame_time) {
        update = true;
        update_time -= frame_time;
        director->update();
      }
      if (update || !realtime) {
        director->render();
      }

      if (!realtime) {
        exporter->encode_frame(director->get_screen_data());
      }
    }
  }
  catch (std::bad_alloc&) {
    std::cerr << bad_alloc << std::endl;
    throw;
  }

  if (realtime) {
    async_thread.join();
  }
  // Destroy oculus HMD before calling ovr_Shutdown().
  director.reset();
  close_window(*window, session.system());
}

DEFINE_string(export_path, "", "export video to this path");
DEFINE_uint64(export_width, 1280, "export video resolution width");
DEFINE_uint64(export_height, 720, "export video resolution height");
DEFINE_uint64(export_fps, 60, "export video frames per second");
DEFINE_uint64(export_length, 300, "export video length in seconds");
DEFINE_uint64(export_bitrate, 5000, "export video target bitrate");

int main(int argc, char** argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (argc > 2) {
    std::cerr << "too many command-line arguments" << std::endl;
    return 1;
  }
  exporter_settings settings{
      FLAGS_export_path,
      uint32_t(FLAGS_export_width), uint32_t(FLAGS_export_height),
      uint32_t(FLAGS_export_fps), uint32_t(FLAGS_export_length),
      uint32_t(FLAGS_export_bitrate)};

  std::string session_path{argc == 2 ? argv[1] : "./default_session.cfg"};
  trance_pb::Session session = load_session(session_path);
  save_session(session, session_path);
  play_session(session, settings);
  return 0;
}