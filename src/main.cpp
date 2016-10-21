#include "audio.h"
#include "common.h"
#include "director.h"
#include "export.h"
#include "session.h"
#include "theme.h"

#pragma warning(push, 0)
#include <gflags/gflags.h>
#include <libovr/OVR_CAPI.h>
#include <SFML/Window.hpp>
#include <src/trance.pb.h>
#pragma warning(pop)

#include <chrono>
#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>

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
    const trance_pb::System& system,
    uint32_t width, uint32_t height, bool visible, bool oculus_rift)
{
  auto window = std::make_unique<sf::RenderWindow>();
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  auto video_mode = sf::VideoMode::getDesktopMode();
  if (width && height) {
    video_mode.width = width;
    video_mode.height = height;
  }
  auto style =
      !visible || oculus_rift ? sf::Style::None :
      system.windowed() ? sf::Style::Default : sf::Style::Fullscreen;
  window->create(video_mode, "trance", style);

  window->setVerticalSyncEnabled(system.enable_vsync());
  window->setFramerateLimit(0);
  window->setVisible(false);
  window->setActive(true);
  return window;
}

std::string next_playlist_item(
    const std::unordered_map<std::string, std::string>& variables,
    const trance_pb::PlaylistItem* item)
{
  auto is_enabled = [&](const trance_pb::PlaylistItem::NextItem next) {
    if (next.condition_variable_name().empty()) {
      return true;
    }
    std::string value;
    auto it = variables.find(next.condition_variable_name());
    if (it != variables.end()) {
      value = it->second;
    }
    return value == next.condition_variable_value();
  };

  uint32_t total = 0;
  for (const auto& next : item->next_item()) {
    total += (is_enabled(next) ? next.random_weight() : 0);
  }
  if (!total) {
    return {};
  }
  auto r = random(total);
  total = 0;
  for (const auto& next : item->next_item()) {
    if (r < (total += (is_enabled(next) ? next.random_weight() : 0))) {
      return next.playlist_item_name();
    }
  }
  return {};
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

void print_info(double elapsed_seconds,
                uint64_t frames, uint64_t total_frames)
{
  auto format_time = [](uint64_t seconds) {
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
  auto elapsed = uint64_t(elapsed_seconds + .5);
  auto eta = uint64_t(
      .5 + (completion ? elapsed_seconds * (1. / completion - 1.) : 0.));
  auto percentage = uint64_t(100 * completion);

  std::cout << std::endl <<
      "frame: " << frames << " / " << total_frames << " [" << percentage <<
      "%]; elapsed: " << format_time(elapsed) << "; eta: " <<
      format_time(eta) << std::endl;
}

void play_session(
    const std::string& root_path, const trance_pb::Session& session,
    const trance_pb::System& system,
    const std::unordered_map<std::string, std::string> variables,
    const exporter_settings& settings)
{
  bool realtime = settings.path.empty();
  auto exporter = create_exporter(settings);
  if (!realtime && !exporter) {
    std::cerr << "don't know how to export that format" << std::endl;
    return;
  }
  // Call ovr_Initialize() before getting an OpenGL context.
  bool oculus_rift = system.enable_oculus_rift();
  if (oculus_rift) {
    if (ovr_Initialize(nullptr) != ovrSuccess) {
      std::cerr << "Oculus initialization failed" << std::endl;
      oculus_rift = false;
    }
  }

  const trance_pb::PlaylistItem* item =
      &session.playlist().find(session.first_playlist_item())->second;
  auto program = [&]() -> const trance_pb::Program&
  {
    return session.program_map().find(item->program())->second;
  };

  std::cout << "loading themes" << std::endl;
  auto theme_bank =
      std::make_unique<ThemeBank>(root_path, session, system, program());
  std::cout << "\nloaded themes" << std::endl;
  auto window = create_window(
      system, realtime ? 0 : settings.width, realtime ? 0 : settings.height,
      realtime, oculus_rift);
  std::cout << "\nloading session" << std::endl;
  auto director = std::make_unique<Director>(
      *window, session, system, *theme_bank, program(),
      realtime, oculus_rift, exporter && exporter->requires_yuv_input());
  std::cout << "\nloaded session" << std::endl;

  std::thread async_thread;
  std::atomic<bool> running = true;
  std::unique_ptr<Audio> audio;
  if (realtime) {
    async_thread = run_async_thread(running, *theme_bank);
    audio.reset(new Audio{root_path});
    audio->TriggerEvents(*item);
  }
  std::cout << std::endl << "-> " << session.first_playlist_item() << std::endl;

  try {
    float update_time = 0.f;
    float playlist_item_time = 0.f;

    uint64_t elapsed_frames = 0;
    uint64_t async_update_residual = 0;
    double elapsed_frames_residual = 0;
    std::chrono::high_resolution_clock clock;
    auto clock_time = [&]
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(
          clock.now().time_since_epoch()).count();
    };
    const auto clock_start = clock_time();
    auto last_clock_time = clock_start;
    auto last_playlist_switch = clock_start;

    while (running) {
      handle_events(running, window.get());

      uint32_t frames_this_loop = 0;
      if (realtime) {
        auto t = clock_time();
        auto elapsed_ms = t - last_clock_time;
        last_clock_time = t;
        elapsed_frames_residual +=
            double(program().global_fps()) * double(elapsed_ms) / 1000.;
        while (elapsed_frames_residual >= 1.) {
          --elapsed_frames_residual;
          ++frames_this_loop;
        }
      } else {
        frames_this_loop = 1;
      }

      elapsed_frames += frames_this_loop;
      if (!realtime) {
        auto total_frames = uint64_t(settings.length) * uint64_t(settings.fps);
        if (elapsed_frames % 8 == 0) {
          auto elapsed_seconds = double(clock_time() - clock_start) / 1000.;
          print_info(elapsed_seconds, elapsed_frames, total_frames);
        }
        if (elapsed_frames >= total_frames) {
          running = false;
          break;
        }
      }

      async_update_residual +=
          uint64_t(1000. * frames_this_loop / double(program().global_fps()));
      while (!realtime && async_update_residual >= async_millis) {
        async_update_residual -= async_millis;
        theme_bank->async_update();
      }

      auto time_since_switch = clock_time() - last_playlist_switch;
      std::string next;
      while (time_since_switch >= 1000 * item->play_time_seconds() &&
             !(next = next_playlist_item(variables, item)).empty()) {
        last_playlist_switch = clock_time();
        item = &session.playlist().find(next)->second;
        if (realtime) {
          audio->TriggerEvents(*item);
        }
        std::cout << "\n-> " << next << std::endl;
        theme_bank->set_program(program());
        director->set_program(program());
      }
      if (theme_bank->swaps_to_match_theme()) {
        theme_bank->change_themes();
      }

      bool update = false;
      bool continue_playing = true;
      while (frames_this_loop > 0) {
        update = true;
        --frames_this_loop;
        continue_playing &= director->update();
      }
      if (!continue_playing) {
        break;
      }
      if (update || !realtime) {
        director->render();
      }

      if (realtime) {
        audio->Update();
      } else {
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
  window->close();
  if (oculus_rift) {
    ovr_Shutdown();
  }
}

std::unordered_map<std::string, std::string>
parse_variables(const std::string& variables)
{
  std::unordered_map<std::string, std::string> result;
  std::vector<std::string> current;
  current.emplace_back();
  bool escaped = false;
  for (char c : variables) {
    if (c == '\\' && !escaped) {
      escaped = true;
      continue;
    }
    if (escaped) {
      if (c == '\\') {
        current.back() += '\\';
      } else if (c == ';') {
        current.back() += ';';
      } else if (c == '=') {
        current.back() += '=';
      } else {
        std::cerr << "couldn't parse variables: " << variables << std::endl;
        return {};
      }
      escaped = false;
      continue;
    }
    if (c == '=' && current.size() == 1 && !current.back().empty()) {
      current.emplace_back();
      continue;
    }
    if (c == ';' && current.size() == 2 && !current.back().empty()) {
      result[current.front()] = current.back();
      current.clear();
      current.emplace_back();
      continue;
    }
    if (c != '=' && c != ';') {
      current.back() += c;
      continue;
    }
    std::cerr << "couldn't parse variables: " << variables << std::endl;
    return {};
  }
  if (current.size() == 2 && !current.back().empty()) {
    result[current.front()] = current.back();
    current.clear();
    current.emplace_back();
  }
  if (current.size() == 1 && current.back().empty()) {
    return result;
  }
  std::cerr << "couldn't parse variables: " << variables << std::endl;
  return {};
}

DEFINE_string(variables, "",
              "semicolon-separated list of key=value variable assignments");
DEFINE_string(export_path, "", "export video to this path");
DEFINE_uint64(export_width, 1280, "export video resolution width");
DEFINE_uint64(export_height, 720, "export video resolution height");
DEFINE_uint64(export_fps, 60, "export video frames per second");
DEFINE_uint64(export_length, 300, "export video length in seconds");
DEFINE_uint64(export_quality, 2, "export video quality (0 to 4, 0 is best)");
DEFINE_uint64(export_threads, 4, "export video threads");

int main(int argc, char** argv)
{
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (argc > 3) {
    std::cerr << "usage: " << argv[0] << " [session.cfg [system.cfg]]" << std::endl;
    return 1;
  }
  auto variables = parse_variables(FLAGS_variables);
  for (const auto& pair : variables) {
    std::cout << "variable " << pair.first << " = " << pair.second << std::endl;
  }

  exporter_settings settings{
      FLAGS_export_path,
      uint32_t(FLAGS_export_width), uint32_t(FLAGS_export_height),
      uint32_t(FLAGS_export_fps), uint32_t(FLAGS_export_length),
      std::min(uint32_t(4), uint32_t(FLAGS_export_quality)),
      uint32_t(FLAGS_export_threads)};

  std::string session_path{argc >= 2 ? argv[1] : "./" + DEFAULT_SESSION_PATH};
  trance_pb::Session session;
  try {
    session = load_session(session_path);
  }
  catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    session = get_default_session();
    search_resources(session, ".");
    save_session(session, session_path);
  }

  std::string system_path{argc >= 3 ? argv[2] : "./" + SYSTEM_CONFIG_PATH};
  trance_pb::System system;
  try {
    system = load_system(system_path);
  }
  catch (std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    system = get_default_system();
    save_system(system, system_path);
  }

  auto root_path = std::tr2::sys::path{session_path}.parent_path().string();
  play_session(root_path, session, system, variables, settings);
  return 0;
}