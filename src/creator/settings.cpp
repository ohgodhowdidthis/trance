#include "settings.h"
#include "../common.h"
#include "main.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace
{
  int f2v(float f)
  {
    return int(f * 4);
  }

  float v2f(int v)
  {
    return float(v) / 4;
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
      "memory. Each size of each font counts as a separate entry in the cache.";

  const std::string OCULUS_RIFT_TOOLTIP =
      "Attempt to enable the Oculus Rift support. Requires the Oculus Runtime "
      "0.8.0.0-beta or compatible version.";

  const std::string IMAGE_DEPTH_TOOLTIP =
      "How intense depth-based effects are on images in the Oculus Rift. The "
      "default value is 4.";

  const std::string TEXT_DEPTH_TOOLTIP =
      "How intense depth-based effects are on text in the Oculus Rift. The "
      "default value is 4.";
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
  auto left = new wxStaticBoxSizer{wxVERTICAL, panel, "General"};
  auto right = new wxStaticBoxSizer{wxVERTICAL, panel, "Oculus Rift"};

  _enable_vsync = new wxCheckBox{panel, wxID_ANY, "Enable VSync"};
  _image_cache_size = new wxSpinCtrl{panel, wxID_ANY};
  _font_cache_size = new wxSpinCtrl{panel, wxID_ANY};
  _enable_oculus_rift = new wxCheckBox{panel, wxID_ANY, "Enable Oculus Rift"};
  _image_depth = new wxSlider{panel,
                              wxID_ANY,
                              f2v(_system.oculus_image_depth()),
                              0,
                              8,
                              wxDefaultPosition,
                              wxDefaultSize,
                              wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_VALUE_LABEL};
  _text_depth = new wxSlider{panel,
                             wxID_ANY,
                             f2v(_system.oculus_text_depth()),
                             0,
                             8,
                             wxDefaultPosition,
                             wxDefaultSize,
                             wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_VALUE_LABEL};
  auto button_ok = new wxButton{panel, ID_OK, "OK"};
  auto button_cancel = new wxButton{panel, ID_CANCEL, "Cancel"};
  _button_apply = new wxButton{panel, ID_APPLY, "Apply"};

  _enable_vsync->SetToolTip(VSYNC_TOOLTIP);
  _enable_vsync->SetValue(_system.enable_vsync());
  _image_cache_size->SetToolTip(IMAGE_CACHE_SIZE_TOOLTIP);
  _image_cache_size->SetRange(8, 1024);
  _image_cache_size->SetValue(_system.image_cache_size());
  _font_cache_size->SetToolTip(FONT_CACHE_SIZE_TOOLTIP);
  _font_cache_size->SetRange(2, 256);
  _font_cache_size->SetValue(_system.font_cache_size());
  _enable_oculus_rift->SetToolTip(OCULUS_RIFT_TOOLTIP);
  _enable_oculus_rift->SetValue(_system.enable_oculus_rift());
  _image_depth->SetToolTip(IMAGE_DEPTH_TOOLTIP);
  _image_depth->Enable(_system.enable_oculus_rift());
  _text_depth->SetToolTip(TEXT_DEPTH_TOOLTIP);
  _text_depth->Enable(_system.enable_oculus_rift());
  _button_apply->Enable(false);

  sizer->Add(top, 1, wxEXPAND, 0);
  sizer->Add(bottom, 0, wxEXPAND, 0);
  top->Add(left, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  top->Add(right, 1, wxALL | wxEXPAND, DEFAULT_BORDER);

  wxStaticText* label = nullptr;
  left->Add(_enable_vsync, 0, wxALL, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Image cache size:"};
  label->SetToolTip(IMAGE_CACHE_SIZE_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  left->Add(_image_cache_size, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Font cache size:"};
  label->SetToolTip(IMAGE_CACHE_SIZE_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  left->Add(_font_cache_size, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  right->Add(_enable_oculus_rift, 0, wxALL, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Image depth intensity:"};
  label->SetToolTip(IMAGE_DEPTH_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  right->Add(_image_depth, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{panel, wxID_ANY, "Text depth intensity:"};
  label->SetToolTip(TEXT_DEPTH_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  right->Add(_text_depth, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  bottom->Add(button_ok, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(button_cancel, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(_button_apply, 1, wxALL, DEFAULT_BORDER);

  panel->SetSizer(sizer);
  SetClientSize(sizer->GetMinSize());
  Show(true);

  auto changed = [&](wxCommandEvent&) { Changed(); };
  Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, changed);
  Bind(wxEVT_SLIDER, changed);
  Bind(wxEVT_SPINCTRL, changed);

  Bind(wxEVT_COMMAND_BUTTON_CLICKED,
       [&](wxCommandEvent&) {
         Apply();
         Close();
       },
       ID_OK);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) { Close(); }, ID_CANCEL);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) { Apply(); }, ID_APPLY);

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) {
    _parent->SettingsClosed();
    Destroy();
  });
}

void SettingsFrame::Changed()
{
  _image_depth->Enable(_enable_oculus_rift->GetValue());
  _text_depth->Enable(_enable_oculus_rift->GetValue());
  _button_apply->Enable(true);
}

void SettingsFrame::Apply()
{
  _system.set_enable_vsync(_enable_vsync->GetValue());
  _system.set_enable_oculus_rift(_enable_oculus_rift->GetValue());
  _system.set_image_cache_size(_image_cache_size->GetValue());
  _system.set_font_cache_size(_font_cache_size->GetValue());
  _system.set_oculus_image_depth(v2f(_image_depth->GetValue()));
  _system.set_oculus_text_depth(v2f(_text_depth->GetValue()));
  _parent->SaveSystem(true);
  _button_apply->Enable(false);
}