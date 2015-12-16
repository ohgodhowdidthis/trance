#include "settings.h"
#include "main.h"
#include "../common.h"
#include "../session.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/sizer.h>
#pragma warning(pop)

SettingsFrame::SettingsFrame(CreatorFrame* parent,
                             const std::string& executable_path)
: wxFrame{parent, wxID_ANY, "System Settings",
          wxDefaultPosition, wxDefaultSize}
, _system_path{executable_path + "/" + SYSTEM_CONFIG_PATH}
, _system_dirty{false}
, _parent{parent}
, _panel{new wxPanel{this}}
, _enable_vsync{new wxCheckBox{_panel, wxID_ANY, "Enable VSync"}}
, _enable_oculus_rift{new wxCheckBox{_panel, wxID_ANY, "Enable Oculus Rift"}}
{
  try {
    _system = load_system(_system_path);
  } catch (std::runtime_error&) {
    _system = get_default_system();
  }

  _enable_vsync->SetValue(_system.enable_vsync());
  _enable_oculus_rift->SetValue(_system.enable_oculus_rift());

  auto sizer = new wxBoxSizer{wxHORIZONTAL};
  sizer->Add(_enable_vsync, 1, wxALL, DEFAULT_BORDER);
  sizer->Add(_enable_oculus_rift, 1, wxALL, DEFAULT_BORDER);
  _panel->SetSizer(sizer);
  Show(true);

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event)
  {
    _parent->SettingsClosed();
    Destroy();
  });
}