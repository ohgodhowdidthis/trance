#ifndef TRANCE_CREATOR_MAIN_H
#define TRANCE_CREATOR_MAIN_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#pragma warning(pop)

class SettingsFrame;
class CreatorFrame : public wxFrame {
public:
  enum {
    ID_EDIT_SYSTEM_CONFIG = 1,
    ID_LAUNCH_SESSION = 2,
  };
  CreatorFrame(const std::string& executable_path, const std::string& parameter);
  void SettingsClosed();

private:
  trance_pb::Session _session;
  bool _session_dirty;
  std::string _session_path;
  std::string _executable_path;

  SettingsFrame* _settings;
  wxPanel* _panel;
  wxMenuBar* _menu_bar;

  bool ConfirmDiscardChanges();
  bool OpenSession(const std::string& path);
  void SetSessionPath(const std::string& path);
  std::string GetLastRootDirectory() const;
};

#endif