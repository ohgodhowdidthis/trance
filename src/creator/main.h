#ifndef TRANCE_CREATOR_MAIN_H
#define TRANCE_CREATOR_MAIN_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#pragma warning(pop)

class PlaylistPage;
class ProgramPage;
class ThemePage;
class SettingsFrame;
class CreatorFrame : public wxFrame {
public:
  CreatorFrame(const std::string& executable_path, const std::string& parameter);
  void SettingsClosed();

  void ThemeCreated();
  void ThemeDeleted(const std::string& theme_name);
  void ThemeRenamed(const std::string& old_name, const std::string& new_name);

  void ProgramCreated();
  void ProgramDeleted(const std::string& program_name);
  void ProgramRenamed(const std::string& old_name, const std::string& new_name);

  void PlaylistItemCreated();
  void PlaylistItemDeleted(const std::string& playlist_item_name);
  void PlaylistItemRenamed(const std::string& old_name,
                           const std::string& new_name);

private:
  enum {
    ID_EDIT_SYSTEM_CONFIG = 1,
    ID_LAUNCH_SESSION = 2,
  };

  trance_pb::Session _session;
  trance_pb::Theme _complete_theme;
  bool _session_dirty;
  std::string _session_path;
  std::string _executable_path;

  SettingsFrame* _settings;
  ThemePage* _theme_page;
  ProgramPage* _program_page;
  PlaylistPage* _playlist_page;
  wxPanel* _panel;
  wxMenuBar* _menu_bar;

  void RefreshData();
  bool ConfirmDiscardChanges();
  bool OpenSession(const std::string& path);
  void SetSessionPath(const std::string& path);
  std::string GetLastRootDirectory() const;
};

#endif