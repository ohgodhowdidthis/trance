#include "settings.h"
#include "../common.h"
#include "../session.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/sizer.h>
#pragma warning(pop)

wxBEGIN_EVENT_TABLE(SettingsFrame, wxFrame)
EVT_CLOSE(SettingsFrame::OnClose)
wxEND_EVENT_TABLE()

SettingsFrame::SettingsFrame(wxFrame* parent,
                             const std::string& executable_path)
: wxFrame{parent, wxID_ANY, "System Settings",
          wxDefaultPosition, wxDefaultSize}
, _system_path{executable_path + "/" + SYSTEM_CONFIG_PATH}
, _panel{new wxPanel{this}}
, _sizer{new wxBoxSizer{wxHORIZONTAL}}
{
  try {
    _system = load_system(_system_path);
  } catch (std::runtime_error&) {
    _system = get_default_system();
  }
  _panel->SetSizer(_sizer);
  Show(true);
}

void SettingsFrame::OnClose(wxCloseEvent& event)
{
  Destroy();
}