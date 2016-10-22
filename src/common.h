#ifndef TRANCE_COMMON_H
#define TRANCE_COMMON_H

#include <filesystem>
#include <string>

static const std::string DEFAULT_SESSION_PATH = "default.session";
static const std::string SYSTEM_CONFIG_PATH = "system.cfg";
static const std::string TRANCE_EXE_PATH = "trance.exe";
static const uint32_t DEFAULT_BORDER = 2;

inline std::string get_system_config_path(const std::string& directory)
{
  return (std::tr2::sys::path{directory} / SYSTEM_CONFIG_PATH).string();
}

inline std::string get_trance_exe_path(const std::string& directory)
{
  return (std::tr2::sys::path{directory} / TRANCE_EXE_PATH).string();
}

inline std::string format_time(uint64_t seconds)
{
  auto minutes = seconds / 60;
  seconds = seconds % 60;
  auto hours = minutes / 60;
  minutes = minutes % 60;

  std::string result;
  if (hours) {
    result += std::to_string(hours) + ":";
  }
  result += (minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") +
      std::to_string(seconds);
  return result;
};

#endif