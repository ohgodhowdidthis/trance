#ifndef TRANCE_CREATOR_MAIN_H
#define TRANCE_CREATOR_MAIN_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#pragma warning(pop)

class ExportFrame;
class PlaylistPage;
class ProgramPage;
class ThemePage;
class SettingsFrame;
class wxPanel;

class CreatorFrame : public wxFrame {
public:
  CreatorFrame(const std::string& executable_path, const std::string& parameter);
  void MakeDirty(bool dirty);
  void SaveSystem(bool show_status);
  void ExportVideo(const std::string& path);
  void RefreshDirectory();
  void SettingsClosed();
  void ExportClosed();

  void ThemeCreated(const std::string& theme_name);
  void ThemeDeleted(const std::string& theme_name);
  void ThemeRenamed(const std::string& old_name, const std::string& new_name);

  void ProgramCreated(const std::string& program_name);
  void ProgramDeleted(const std::string& program_name);
  void ProgramRenamed(const std::string& old_name, const std::string& new_name);

  void PlaylistItemCreated(const std::string& playlist_item_name);
  void PlaylistItemDeleted(const std::string& playlist_item_name);
  void PlaylistItemRenamed(const std::string& old_name,
                           const std::string& new_name);

private:
  enum {
    ID_EDIT_SYSTEM_CONFIG = 10101,
    ID_LAUNCH_SESSION = 10102,
    ID_EXPORT_VIDEO = 10101,
  };

  trance_pb::System _system;
  trance_pb::Session _session;
  bool _session_dirty;
  std::string _session_path;
  std::string _executable_path;

  SettingsFrame* _settings;
  ExportFrame* _export;
  ThemePage* _theme_page;
  ProgramPage* _program_page;
  PlaylistPage* _playlist_page;
  wxPanel* _panel;
  wxMenuBar* _menu_bar;

  bool ConfirmDiscardChanges();
  bool OpenSession(const std::string& path);
  void SetSessionPath(const std::string& path);
};

#endif