#ifndef TRANCE_COMMON_H
#define TRANCE_COMMON_H

#include <string>
static const std::string DEFAULT_SESSION_PATH = "default.session";
static const std::string SYSTEM_CONFIG_PATH = "system.cfg";
static const std::string TRANCE_EXE_PATH = "trance.exe";
static const uint32_t DEFAULT_BORDER = 2;

inline std::string get_system_config_path(const std::string& directory)
{
  return directory + "/" + SYSTEM_CONFIG_PATH;
}

inline std::string get_trance_exe_path(const std::string& directory)
{
  return directory + "/" + TRANCE_EXE_PATH;
}

#endif