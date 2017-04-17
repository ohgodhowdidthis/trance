#include "settings.h"
#include <common/common.h>
#include <creator/main.h>

#pragma warning(push, 0)
#include <common/trance.pb.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace
{
  int f2v(float f)
  {
    return int(f * 8);
  }

  float v2f(int v)
  {
    return float(v) / 8;
  }

  const std::string VSYNC_TOOLTIP =
      "Turn on VSync to eliminate tearing. This can cause frame rate to "
      "stutter if the video card can't keep up.";

  const std::string IMAGE_CACHE_SIZE_TOOLTIP =
      "Number of images to load into memory at once. Increases variation, but "
      "uses up both RAM and video memory. Images will be swapped in and out "
      "of the cache periodically.";

  const std::string FONT_CACHE_SIZE_TOOLTIP =
      "Number of fonts to load into memory at once. Increasing the font cache "
      "size prevents pauses when loading fonts, but uses up both RAM and video "
      "memory.";

  const std::string MONITOR_TOOLTIP = "Render fullscreen 2D to primary monitor.";

  const std::string OCULUS_TOOLTIP = "Render to the Oculus rift using LibOVR.";

  const std::string OPENVR_TOOLTIP = "Render to any SteamVR-compatible device using OpenVR.";

  const std::string DRAW_DEPTH_TOOLTIP =
      "Controls the depth of the view. This affects the intensity of the zoom "
      "effect and the 3D effect in VR. The default value is 4.";

  const std::string EYE_SPACING_TOOLTIP =
      "Distance between the view for each eye in VR. Adjust this if the 3D effects are "
      "out-of-sync or difficult to focus on. The default value is .0625.";
}

SettingsFrame::SettingsFrame(CreatorFrame* parent, trance_pb::System& system)
: wxFrame{parent,
          wxID_ANY,
          "System settings",
          wxDefaultPosition,
          wxDefaultSize,
          wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN}
, _system{system}
, _parent{parent}
{
  auto panel = new wxPanel{this};
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto top = new wxBoxSizer{wxHORIZONTAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};
  auto left = new wxStaticBoxSizer{wxVERTICAL, panel, "Memory"};
  auto right = new wxStaticBoxSizer{wxVERTICAL, panel, "Rendering"};
  auto right_mode = new wxBoxSizer{wxHORIZONTAL};

  _monitor =
      new wxRadioButton{panel, wxID_ANY, "Monitor", wxDefaultPosition, wxDefaultSize, wxRB_GROUP};
  _oculus = new wxRadioButton{panel, wxID_ANY, "Oculus Rift"};
  _openvr = new wxRadioButton{panel, wxID_ANY, "SteamVR"};

  _enable_vsync = new wxCheckBox{panel, wxID_ANY, "Enable VSync"};
  _image_cache_size = new wxSpinCtrl{panel, wxID_ANY};
  _font_cache_size = new wxSpinCtrl{panel, wxID_ANY};
  _draw_depth = new wxSlider{panel,
                             wxID_ANY,
                             f2v(_system.draw_depth().draw_depth()),
                             0,
                             8,
                             wxDefaultPosition,
                             wxDefaultSize,
                             wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_VALUE_LABEL};
  _eye_spacing = new wxSpinCtrlDouble{panel, wxID_ANY};
  auto button_ok = new wxButton{panel, wxID_ANY, "OK"};
  auto button_cancel = new wxButton{panel, wxID_ANY, "Cancel"};
  _button_apply = new wxButton{panel, wxID_ANY, "Apply"};

  _monitor->SetToolTip(MONITOR_TOOLTIP);
  _oculus->SetToolTip(OCULUS_TOOLTIP);
  _openvr->SetToolTip(OPENVR_TOOLTIP);
  if (system.renderer() == trance_pb::System::OPENVR) {
    _openvr->SetValue(true);
  } else if (system.renderer() == trance_pb::System::OCULUS) {
    _oculus->SetValue(true);
  } else {
    _monitor->SetValue(true);
    _eye_spacing->Disable();
  }
  _enable_vsync->SetToolTip(VSYNC_TOOLTIP);
  _enable_vsync->SetValue(_system.enable_vsync());
  _image_cache_size->SetToolTip(IMAGE_CACHE_SIZE_TOOLTIP);
  _image_cache_size->SetRange(8, 1024);
  _image_cache_size->SetValue(_system.image_cache_size());
  _font_cache_size->SetToolTip(FONT_CACHE_SIZE_TOOLTIP);
  _font_cache_size->SetRange(2, 256);
  _font_cache_size->SetValue(_system.font_cache_size());
  _draw_depth->SetToolTip(DRAW_DEPTH_TOOLTIP);
  _eye_spacing->SetToolTip(EYE_SPACING_TOOLTIP);
  _eye_spacing->SetRange(0., 1.);
  _eye_spacing->SetIncrement(1. / 128);
  _eye_spacing->SetValue(_system.eye_spacing().eye_spacing());

  sizer->Add(top, 1, wxEXPAND, 0);
  sizer->Add(bottom, 0, wxEXPAND, 0);
  top->Add(left, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  top->Add(right, 1, wxALL | wxEXPAND, DEFAULT_BORDER);

  wxStaticText* label = nullptr;
  label = new wxStaticText{panel, wxID_ANY, "Image cache size:"};
  label->SetToolTip(IMAGE_CACHE_SIZE_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  left->Add(_image_cache_size, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Font cache size:"};
  label->SetToolTip(IMAGE_CACHE_SIZE_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  left->Add(_font_cache_size, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Rendering mode:"};
  right->Add(right_mode, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  right_mode->Add(_monitor, 1, wxALL, DEFAULT_BORDER);
  right_mode->Add(_oculus, 1, wxALL, DEFAULT_BORDER);
  right_mode->Add(_openvr, 1, wxALL, DEFAULT_BORDER);
  right->Add(_enable_vsync, 0, wxALL, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Draw depth:"};
  label->SetToolTip(DRAW_DEPTH_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  right->Add(_draw_depth, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  _eye_spacing_label = new wxStaticText{panel, wxID_ANY, "Eye spacing (in VR):"};
  _eye_spacing_label->SetToolTip(EYE_SPACING_TOOLTIP);
  right->Add(_eye_spacing_label, 0, wxALL, DEFAULT_BORDER);
  right->Add(_eye_spacing, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  bottom->Add(button_ok, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(button_cancel, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(_button_apply, 1, wxALL, DEFAULT_BORDER);

  _eye_spacing_label->Enable(!_monitor->GetValue());
  _eye_spacing->Enable(!_monitor->GetValue());
  _button_apply->Enable(false);

  panel->SetSizer(sizer);
  SetClientSize(sizer->GetMinSize());
  CentreOnParent();
  Show(true);

  auto changed = [&](wxCommandEvent&) { Changed(); };
  Bind(wxEVT_RADIOBUTTON, changed);
  Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, changed);
  Bind(wxEVT_SLIDER, changed);
  Bind(wxEVT_SPINCTRL, changed);
  Bind(wxEVT_SPINCTRLDOUBLE, changed);

  button_ok->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
    Apply();
    Close();
  });
  button_cancel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) { Close(); });
  _button_apply->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) { Apply(); });

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) {
    _parent->SettingsClosed();
    Destroy();
  });
}

void SettingsFrame::Changed()
{
  _eye_spacing_label->Enable(!_monitor->GetValue());
  _eye_spacing->Enable(!_monitor->GetValue());
  _button_apply->Enable(true);
}

void SettingsFrame::Apply()
{
  _system.set_renderer(_openvr->GetValue() ? trance_pb::System::OPENVR
                                           : _oculus->GetValue() ? trance_pb::System::OCULUS
                                                                 : trance_pb::System::MONITOR);
  _system.set_enable_vsync(_enable_vsync->GetValue());
  _system.set_image_cache_size(_image_cache_size->GetValue());
  _system.set_font_cache_size(_font_cache_size->GetValue());
  _system.mutable_draw_depth()->set_draw_depth(v2f(_draw_depth->GetValue()));
  _system.mutable_eye_spacing()->set_eye_spacing(static_cast<float>(_eye_spacing->GetValue()));
  _parent->SaveSystem(true);

  // Seems to work around a bug. No idea why. Radio buttons break if the first isn't set while
  // disabling the button.
  _monitor->SetValue(true);
  _button_apply->Enable(false);
  if (_system.renderer() == trance_pb::System::OPENVR) {
    _openvr->SetValue(true);
  } else if (_system.renderer() == trance_pb::System::OCULUS) {
    _oculus->SetValue(true);
  } else {
    _monitor->SetValue(true);
  }
}