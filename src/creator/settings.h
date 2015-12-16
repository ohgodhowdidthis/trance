#ifndef TRANCE_CREATOR_SETTINGS_H
#define TRANCE_CREATOR_SETTINGS_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/panel.h>
#pragma warning(pop)

class SettingsFrame : public wxFrame {
public:
  SettingsFrame(wxFrame* parent, const std::string& executable_path);

private:
  const std::string _system_path;
  trance_pb::System _system;
  bool _system_dirty;

  wxPanel* _panel;
  wxSizer* _sizer;

  void OnClose(wxCloseEvent& event);

  wxDECLARE_EVENT_TABLE();
};

#endif