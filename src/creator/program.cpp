#include "program.h"
#include "../common.h"
#include "item_list.h"
#include "main.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace
{
  int f2v(float f)
  {
    return int(f * 10);
  }

  float v2f(int v)
  {
    return float(v) / 10;
  }

  void c2v(const trance_pb::Colour& c, wxColourPickerCtrl* colour, wxSpinCtrl* alpha)
  {
    colour->SetColour(wxColour{unsigned char(255 * c.r()), unsigned char(255 * c.g()),
                               unsigned char(255 * c.b()), unsigned char(alpha->GetValue())});
    alpha->SetValue(unsigned char(255 * c.a()));
  }

  trance_pb::Colour v2c(wxColourPickerCtrl* colour, wxSpinCtrl* alpha)
  {
    auto v = colour->GetColour();
    trance_pb::Colour c;
    c.set_r(v.Red() / 255.f);
    c.set_g(v.Green() / 255.f);
    c.set_b(v.Blue() / 255.f);
    c.set_a(alpha->GetValue() / 255.f);
    c2v(c, colour, alpha);
    return c;
  }

  const std::string ACCELERATE_TOOLTIP =
      "Accelerates and decelerates changing images with overlaid text.";
  const std::string SLOW_FLASH_TOOLTIP =
      "Alternates between slowly-changing images and animations, and "
      "rapidly-changing images, with overlaid text.";
  const std::string SUB_TEXT_TOOLTIP = "Overlays subtle text on changing images.";
  const std::string FLASH_TEXT_TOOLTIP = "Smoothly fades between images, animations and text.";
  const std::string PARALLEL_TOOLTIP =
      "Displays two images or animations at once with overlaid text.";
  const std::string SUPER_PARALLEL_TOOLTIP =
      "Displays three images or animations at once with overlaid text.";
  const std::string ANIMATION_TOOLTIP = "Displays an animation with overlaid text.";
  const std::string SUPER_FAST_TOOLTIP =
      "Splices rapidly-changing images and overlaid text with "
      "brief clips of animation.";

  const std::string GLOBAL_FPS_TOOLTIP =
      "Global speed (in ticks per second) while this program is active. "
      "Higher is faster. The default is 120.";
  const std::string ZOOM_INTENSITY_TOOLTIP =
      "Intensity of the zoom effect on static images "
      "(zero to disable entirely).";
  const std::string REVERSE_SPIRAL_TOOLTIP = "Reverse the spin direction of the spiral.";
  const std::string SPIRAL_COLOUR_TOOLTIP = "Two colours for the spiral.";
  const std::string TEXT_COLOUR_TOOLTIP = "Main colour and shadow effect colour for text messages.";
  const std::string PINNED_TOOLTIP =
      "A pinned theme is always one of the two active themes while the "
      "program is running. If a theme is pinned, the random weights are "
      "used only for selecting the second theme.";
}

ProgramPage::ProgramPage(wxNotebook* parent, CreatorFrame& creator_frame,
                         trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}, _creator_frame{creator_frame}, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto splitter = new wxSplitterWindow{this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  splitter->SetSashGravity(0);
  splitter->SetMinimumPaneSize(128);

  auto bottom_panel = new wxPanel{splitter, wxID_ANY};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  auto bottom_splitter = new wxSplitterWindow{bottom_panel, wxID_ANY, wxDefaultPosition,
                                              wxDefaultSize, wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  bottom_splitter->SetSashGravity(0.75);
  bottom_splitter->SetMinimumPaneSize(128);

  auto left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxBoxSizer{wxHORIZONTAL};
  auto right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto right = new wxStaticBoxSizer{wxVERTICAL, right_panel, "Program settings"};
  auto right_buttons = new wxBoxSizer{wxVERTICAL};

  auto left_splitter = new wxSplitterWindow{left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                            wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  left_splitter->SetSashGravity(0.5);
  left_splitter->SetMinimumPaneSize(128);

  _leftleft_panel = new wxPanel{left_splitter, wxID_ANY};
  _themes_sizer = new wxStaticBoxSizer{wxVERTICAL, _leftleft_panel, "Theme weights"};
  auto leftright_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftright = new wxStaticBoxSizer{wxVERTICAL, leftright_panel, "Visualizer weights"};

  _item_list = new ItemList<trance_pb::Program>{
      splitter, *session.mutable_program_map(), "program",
      [&](const std::string& s) {
        _item_selected = s;
        RefreshOurData();
      },
      std::bind(&CreatorFrame::ProgramCreated, &_creator_frame, std::placeholders::_1),
      std::bind(&CreatorFrame::ProgramDeleted, &_creator_frame, std::placeholders::_1),
      std::bind(&CreatorFrame::ProgramRenamed, &_creator_frame, std::placeholders::_1,
                std::placeholders::_2)};

  _leftleft_panel->SetSizer(_themes_sizer);

  auto add_visual = [&](const std::string& name, const std::string& tooltip,
                        trance_pb::Program::VisualType type) {
    auto row_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto label = new wxStaticText{leftright_panel, wxID_ANY, name + ":"};
    auto weight = new wxSpinCtrl{leftright_panel, wxID_ANY};
    label->SetToolTip(tooltip);
    weight->SetToolTip(tooltip +
                       " A higher weight makes this "
                       "visualizer more likely to be chosen.");
    weight->SetRange(0, 100);
    row_sizer->Add(label, 1, wxALL, DEFAULT_BORDER);
    row_sizer->Add(weight, 0, wxALL, DEFAULT_BORDER);
    leftright->Add(row_sizer, 0, wxEXPAND);
    _visual_lookup[type] = weight;

    weight->Bind(wxEVT_SPINCTRL, [&this, weight, type](wxCommandEvent & e) {
      auto it = _session.mutable_program_map()->find(_item_selected);
      if (it == _session.mutable_program_map()->end()) {
        return;
      }
      bool found = false;
      for (auto& visual : *it->second.mutable_visual_type()) {
        if (visual.type() == type) {
          visual.set_random_weight(weight->GetValue());
          found = true;
        }
      }
      if (!found) {
        auto& visual = *it->second.add_visual_type();
        visual.set_type(type);
        visual.set_random_weight(weight->GetValue());
      }
      _creator_frame.MakeDirty(true);
    });
  };

  add_visual("2-parallel", PARALLEL_TOOLTIP, trance_pb::Program::PARALLEL);
  add_visual("3-parallel", SUPER_PARALLEL_TOOLTIP, trance_pb::Program::SUPER_PARALLEL);
  add_visual("Accelerate", ACCELERATE_TOOLTIP, trance_pb::Program::ACCELERATE);
  add_visual("Alternate", SLOW_FLASH_TOOLTIP, trance_pb::Program::SLOW_FLASH);
  add_visual("Animation", ANIMATION_TOOLTIP, trance_pb::Program::ANIMATION);
  add_visual("Fade", FLASH_TEXT_TOOLTIP, trance_pb::Program::FLASH_TEXT);
  add_visual("Rapid", SUPER_FAST_TOOLTIP, trance_pb::Program::SUPER_FAST);
  add_visual("Subtext", SUB_TEXT_TOOLTIP, trance_pb::Program::SUB_TEXT);

  leftright_panel->SetSizer(leftright);
  left->Add(left_splitter, 1, wxEXPAND, 0);
  left_splitter->SplitVertically(_leftleft_panel, leftright_panel);
  left_panel->SetSizer(left);

  wxStaticText* label = nullptr;

  _global_fps = new wxSpinCtrl{right_panel, wxID_ANY};
  _global_fps->SetToolTip(GLOBAL_FPS_TOOLTIP);
  _global_fps->SetRange(1, 960);
  label = new wxStaticText{right_panel, wxID_ANY, "Global speed:"};
  label->SetToolTip(GLOBAL_FPS_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  right->Add(_global_fps, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  _zoom_intensity = new wxSlider{right_panel,
                                 wxID_ANY,
                                 0,
                                 0,
                                 10,
                                 wxDefaultPosition,
                                 wxDefaultSize,
                                 wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_VALUE_LABEL};
  _zoom_intensity->SetToolTip(ZOOM_INTENSITY_TOOLTIP);
  label = new wxStaticText{right_panel, wxID_ANY, "Zoom intensity:"};
  label->SetToolTip(ZOOM_INTENSITY_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  right->Add(_zoom_intensity, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  _reverse_spiral = new wxCheckBox{right_panel, wxID_ANY, "Reverse spiral direction"};
  _reverse_spiral->SetToolTip(REVERSE_SPIRAL_TOOLTIP);
  right->Add(_reverse_spiral, 0, wxALL, DEFAULT_BORDER);

  auto setup_colour = [&](wxColourPickerCtrl*& picker, wxSpinCtrl*& alpha, const std::string& label,
                          const std::string& tooltip) {
    auto label_obj = new wxStaticText{right_panel, wxID_ANY, label};
    label_obj->SetToolTip(tooltip);
    right->Add(label_obj, 0, wxALL, DEFAULT_BORDER);

    picker = new wxColourPickerCtrl{right_panel, wxID_ANY};
    picker->SetToolTip(tooltip);
    label_obj = new wxStaticText{right_panel, wxID_ANY, "Alpha:"};
    alpha = new wxSpinCtrl{right_panel, wxID_ANY};
    alpha->SetRange(0, 255);
    alpha->SetToolTip(tooltip);

    auto sizer = new wxBoxSizer{wxHORIZONTAL};
    sizer->Add(picker, 0, wxALL, DEFAULT_BORDER);
    sizer->Add(label_obj, 0, wxALL, DEFAULT_BORDER);
    sizer->Add(alpha, 1, wxALL, DEFAULT_BORDER);
    right->Add(sizer, 0, wxEXPAND, 0);
  };

  setup_colour(_spiral_colour_a, _spiral_colour_a_alpha, "Spiral colour A:", SPIRAL_COLOUR_TOOLTIP);
  setup_colour(_spiral_colour_b, _spiral_colour_b_alpha, "Spiral colour B:", SPIRAL_COLOUR_TOOLTIP);
  setup_colour(_main_text_colour, _main_text_colour_alpha, "Text main colour:",
               TEXT_COLOUR_TOOLTIP);
  setup_colour(_shadow_text_colour, _shadow_text_colour_alpha, "Text shadow colour:",
               TEXT_COLOUR_TOOLTIP);

  right_panel->SetSizer(right);
  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(left_panel, right_panel);
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  auto changed = [&](wxCommandEvent&) { Changed(); };
  right_panel->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, changed);
  right_panel->Bind(wxEVT_SLIDER, changed);
  right_panel->Bind(wxEVT_SPINCTRL, changed);
  right_panel->Bind(wxEVT_COLOURPICKER_CHANGED, changed);
}

ProgramPage::~ProgramPage()
{
}

void ProgramPage::RefreshOurData()
{
  for (const auto& pair : _theme_lookup) {
    pair.second.first->SetValue(0);
    pair.second.second->SetValue(false);
  }

  auto it = _session.program_map().find(_item_selected);
  if (it != _session.program_map().end()) {
    for (const auto& theme : it->second.enabled_theme()) {
      auto it = _theme_lookup.find(theme.theme_name());
      if (it != _theme_lookup.end()) {
        it->second.first->SetValue(theme.random_weight());
        it->second.second->SetValue(theme.pinned());
      }
    }
    for (const auto& visual : it->second.visual_type()) {
      auto jt = _visual_lookup.find(visual.type());
      if (jt != _visual_lookup.end()) {
        jt->second->SetValue(visual.random_weight());
      }
    }

    _global_fps->SetValue(it->second.global_fps());
    _zoom_intensity->SetValue(f2v(it->second.zoom_intensity()));
    _reverse_spiral->SetValue(it->second.reverse_spiral_direction());
    c2v(it->second.spiral_colour_a(), _spiral_colour_a, _spiral_colour_a_alpha);
    c2v(it->second.spiral_colour_b(), _spiral_colour_b, _spiral_colour_b_alpha);
    c2v(it->second.main_text_colour(), _main_text_colour, _main_text_colour_alpha);
    c2v(it->second.shadow_text_colour(), _shadow_text_colour, _shadow_text_colour_alpha);
  }
}

void ProgramPage::RefreshData()
{
  _item_list->RefreshData();
  RefreshOurData();
}

void ProgramPage::RefreshThemes()
{
  _themes_sizer->Clear(true);
  _theme_lookup.clear();

  std::vector<std::string> themes;
  for (const auto& pair : _session.theme_map()) {
    themes.push_back(pair.first);
  }
  std::sort(themes.begin(), themes.end());

  for (const auto& theme_name : themes) {
    auto row_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto label = new wxStaticText{_leftleft_panel, wxID_ANY, theme_name + ":"};
    auto pinned = new wxCheckBox{_leftleft_panel, wxID_ANY, "Pinned"};
    auto weight = new wxSpinCtrl{_leftleft_panel, wxID_ANY};

    label->SetToolTip("Themes that can be enabled in this program.");
    pinned->SetToolTip(PINNED_TOOLTIP);
    weight->SetToolTip(
        "A higher weight makes this "
        "theme more likely to be chosen.");
    weight->SetRange(0, 100);
    row_sizer->Add(label, 1, wxALL, DEFAULT_BORDER);
    row_sizer->Add(pinned, 0, wxALL, DEFAULT_BORDER);
    row_sizer->Add(weight, 0, wxALL, DEFAULT_BORDER);
    _themes_sizer->Add(row_sizer, 0, wxEXPAND);

    _theme_lookup.emplace(theme_name, std::make_pair(weight, pinned));

    weight->Bind(wxEVT_SPINCTRL, [this, weight, theme_name](wxCommandEvent& e) {
      auto it = _session.mutable_program_map()->find(_item_selected);
      if (it == _session.mutable_program_map()->end()) {
        return;
      }
      bool found = false;
      for (auto& theme : *it->second.mutable_enabled_theme()) {
        if (theme.theme_name() == theme_name) {
          theme.set_random_weight(weight->GetValue());
          found = true;
        }
      }
      if (!found) {
        auto& theme = *it->second.add_enabled_theme();
        theme.set_theme_name(theme_name);
        theme.set_random_weight(weight->GetValue());
      }
      _creator_frame.MakeDirty(true);
    });

    pinned->Bind(wxEVT_CHECKBOX, [this, pinned, theme_name](wxCommandEvent& e) {
      auto it = _session.mutable_program_map()->find(_item_selected);
      if (it == _session.mutable_program_map()->end()) {
        return;
      }
      for (const auto& pair : _theme_lookup) {
        if (pair.first != theme_name) {
          pair.second.second->SetValue(false);
        }
      }
      bool found = false;
      for (auto& theme : *it->second.mutable_enabled_theme()) {
        if (theme.theme_name() == theme_name) {
          theme.set_pinned(pinned->GetValue());
          found = true;
        } else {
          theme.set_pinned(false);
        }
      }
      if (!found) {
        auto& theme = *it->second.add_enabled_theme();
        theme.set_theme_name(theme_name);
        theme.set_pinned(pinned->GetValue());
      }
      _creator_frame.MakeDirty(true);
    });
  }
  _leftleft_panel->Layout();

  std::set<std::string> theme_set{themes.begin(), themes.end()};
  for (auto& pair : *_session.mutable_program_map()) {
    for (auto it = pair.second.mutable_enabled_theme()->begin();
         it != pair.second.mutable_enabled_theme()->end();) {
      it = theme_set.count(it->theme_name()) ? 1 + it
                                             : pair.second.mutable_enabled_theme()->erase(it);
    }
  }
}

void ProgramPage::Changed()
{
  auto it = _session.mutable_program_map()->find(_item_selected);
  if (it == _session.mutable_program_map()->end()) {
    return;
  }
  it->second.set_global_fps(_global_fps->GetValue());
  it->second.set_zoom_intensity(v2f(_zoom_intensity->GetValue()));
  it->second.set_reverse_spiral_direction(_reverse_spiral->GetValue());
  *it->second.mutable_spiral_colour_a() = v2c(_spiral_colour_a, _spiral_colour_a_alpha);
  *it->second.mutable_spiral_colour_b() = v2c(_spiral_colour_b, _spiral_colour_b_alpha);
  *it->second.mutable_main_text_colour() = v2c(_main_text_colour, _main_text_colour_alpha);
  *it->second.mutable_shadow_text_colour() = v2c(_shadow_text_colour, _shadow_text_colour_alpha);
  _creator_frame.MakeDirty(true);
}