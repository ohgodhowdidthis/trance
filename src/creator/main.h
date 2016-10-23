#ifndef TRANCE_CREATOR_MAIN_H
#define TRANCE_CREATOR_MAIN_H
#include <string>
#include <unordered_map>

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#pragma warning(pop)

class ExportFrame;
class LaunchFrame;
class PlaylistPage;
class ProgramPage;
class SettingsFrame;
class ThemePage;
class VariablePage;
class wxNotebook;
class wxPanel;

class CreatorFrame : public wxFrame
{
public:
  CreatorFrame(const std::string& executable_path, const std::string& parameter);
  void MakeDirty(bool dirty);
  void SaveSystem(bool show_status);

  void Launch();
  void ExportVideo(const std::string& path);

  void RefreshDirectory();
  void SettingsClosed();
  void ExportClosed();
  void LaunchClosed();

  void ThemeCreated(const std::string& theme_name);
  void ThemeDeleted(const std::string& theme_name);
  void ThemeRenamed(const std::string& old_name, const std::string& new_name);

  void ProgramCreated(const std::string& program_name);
  void ProgramDeleted(const std::string& program_name);
  void ProgramRenamed(const std::string& old_name, const std::string& new_name);

  void PlaylistItemCreated(const std::string& playlist_item_name);
  void PlaylistItemDeleted(const std::string& playlist_item_name);
  void PlaylistItemRenamed(const std::string& old_name, const std::string& new_name);

  void VariableCreated(const std::string& variable_name);
  void VariableDeleted(const std::string& variable_name);
  void VariableRenamed(const std::string& old_name, const std::string& new_name);

  void VariableValueCreated(const std::string& variable_name, const std::string& value_name);
  void VariableValueDeleted(const std::string& variable_name, const std::string& value_name);
  void VariableValueRenamed(const std::string& variable_name, const std::string& old_name,
                            const std::string& new_name);

private:
  enum {
    ID_EDIT_SYSTEM_CONFIG = 10101,
    ID_LAUNCH_SESSION = 10102,
    ID_EXPORT_VIDEO = 10103,
  };

  trance_pb::System _system;
  trance_pb::Session _session;
  bool _session_dirty;
  std::string _session_path;
  std::string _executable_path;

  SettingsFrame* _settings;
  ThemePage* _theme_page;
  wxNotebook* _notebook;
  ProgramPage* _program_page;
  PlaylistPage* _playlist_page;
  VariablePage* _variable_page;
  wxPanel* _panel;
  wxMenuBar* _menu_bar;

  bool ConfirmDiscardChanges();
  bool OpenSession(const std::string& path);
  void SetSessionPath(const std::string& path);
  std::string EncodeVariables();
};

#endif