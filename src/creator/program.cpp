#include "program.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace {
  const std::string ACCELERATE_TOOLTIP =
      "Accelerates and decelerates changing images with overlaid text.";
  const std::string SLOW_FLASH_TOOLTIP =
      "Alternates between slowly-changing images and animations, and "
      "rapidly-changing images, with overlaid text.";
  const std::string SUB_TEXT_TOOLTIP =
      "Overlays subtle text on changing images.";
  const std::string FLASH_TEXT_TOOLTIP =
      "Smoothly fades between images, animations and text.";
  const std::string PARALLEL_TOOLTIP =
      "Displays two images or animations at once with overlaid text.";
  const std::string SUPER_PARALLEL_TOOLTIP =
      "Displays four images or animations at once with overlaid text.";
  const std::string ANIMATION_TOOLTIP =
      "Displays an animation with overlaid text.";
  const std::string SUPER_FAST_TOOLTIP =
      "Splices rapidly-changing images and overlaid text with "
      "brief clips of animation.";
}

ProgramPage::ProgramPage(wxNotebook* parent,
                         CreatorFrame& creator_frame,
                         trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
, _creator_frame{creator_frame}
, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto splitter = new wxSplitterWindow{
      this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  splitter->SetSashGravity(0);
  splitter->SetMinimumPaneSize(128);

  auto bottom_panel = new wxPanel{splitter, wxID_ANY};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  auto bottom_splitter = new wxSplitterWindow{
      bottom_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  bottom_splitter->SetSashGravity(0.75);
  bottom_splitter->SetMinimumPaneSize(128);

  auto left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxBoxSizer{wxHORIZONTAL};
  auto right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto right =
      new wxStaticBoxSizer{wxVERTICAL, right_panel, "Program settings"};
  auto right_buttons = new wxBoxSizer{wxVERTICAL};

  auto left_splitter = new wxSplitterWindow{
      left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  left_splitter->SetSashGravity(0.5);
  left_splitter->SetMinimumPaneSize(128);

  auto leftleft_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftleft =
      new wxStaticBoxSizer{wxVERTICAL, leftleft_panel, "Enabled themes"};
  auto leftright_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftright =
      new wxStaticBoxSizer{wxVERTICAL, leftright_panel, "Visualizer weights"};

  _item_list = new ItemList<trance_pb::Program>{
      splitter, *session.mutable_program_map(), "program",
      [&](const std::string& s) { _item_selected = s; RefreshOurData(); },
      std::bind(&CreatorFrame::ProgramCreated, &_creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::ProgramDeleted, &_creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::ProgramRenamed, &_creator_frame,
                std::placeholders::_1, std::placeholders::_2)};

  _tree = new wxTreeListCtrl{
      leftleft_panel, 0, wxDefaultPosition, wxDefaultSize,
      wxTL_SINGLE | wxTL_CHECKBOX | wxTL_3STATE | wxTL_NO_HEADER};
  _tree->AppendColumn("");
  _tree->GetView()->SetToolTip(
      "Themes that are part of the currently-selected program.");

  leftleft->Add(_tree, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  leftleft_panel->SetSizer(leftleft);

  auto add_visual = [&](const std::string& name, const std::string& tooltip,
                        trance_pb::Program::VisualType type) {
    auto row_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto label = new wxStaticText{leftright_panel, wxID_ANY, name + ":"};
    auto weight = new wxSpinCtrl{leftright_panel, wxID_ANY};
    label->SetToolTip(tooltip);
    weight->SetToolTip(tooltip + " A higher value makes this "
                       "visualizer more likely to be used.");
    weight->SetRange(0, 100);
    row_sizer->Add(label, 1, wxALL, DEFAULT_BORDER);
    row_sizer->Add(weight, 0, wxALL, DEFAULT_BORDER);
    leftright->Add(row_sizer, 0, wxEXPAND);
    _visual_lookup[type] = weight;

    weight->Bind(wxEVT_SPINCTRL, [&this,weight,type](wxCommandEvent& e) {
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
  add_visual("4-parallel", SUPER_PARALLEL_TOOLTIP,
             trance_pb::Program::SUPER_PARALLEL);
  add_visual("Accelerate", ACCELERATE_TOOLTIP, trance_pb::Program::ACCELERATE);
  add_visual("Alternate", SLOW_FLASH_TOOLTIP, trance_pb::Program::SLOW_FLASH);
  add_visual("Animation", ANIMATION_TOOLTIP, trance_pb::Program::ANIMATION);
  add_visual("Fade", FLASH_TEXT_TOOLTIP, trance_pb::Program::FLASH_TEXT);
  add_visual("Rapid", SUPER_FAST_TOOLTIP, trance_pb::Program::SUPER_FAST);
  add_visual("Subtext", SUB_TEXT_TOOLTIP, trance_pb::Program::SUB_TEXT);

  leftright_panel->SetSizer(leftright);
  left->Add(left_splitter, 1, wxEXPAND, 0);
  left_splitter->SplitVertically(leftleft_panel, leftright_panel);
  left_panel->SetSizer(left);
  right_panel->SetSizer(right);
  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(left_panel, right_panel);
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  _tree->Bind(wxEVT_TREELIST_ITEM_CHECKED, [&](wxTreeListEvent& e)
  {
    auto it = _session.mutable_program_map()->find(_item_selected);
    if (it == _session.mutable_program_map()->end()) {
      e.Veto();
      return;
    }
    auto checked = _tree->GetCheckedState(e.GetItem());
    auto data = _tree->GetItemData(e.GetItem());
    std::string theme = ((const wxStringClientData*) data)->GetData();
    auto& theme_names = *it->second.mutable_enabled_theme_name();

    if (checked == wxCHK_CHECKED) {
      if (std::find(theme_names.begin(),
                    theme_names.end(), theme) == theme_names.end()) {
        it->second.add_enabled_theme_name(theme);
      }
    } else {
      auto it = theme_names.begin();
      while (it != theme_names.end()) {
        it = *it == theme ? theme_names.erase(it) : 1 + it;
      }
    }
    _creator_frame.MakeDirty(true);
  }, wxID_ANY);
}

ProgramPage::~ProgramPage()
{
}

void ProgramPage::RefreshOurData()
{
  for (auto item = _tree->GetFirstItem(); item.IsOk();
       item = _tree->GetNextItem(item)) {
    _tree->CheckItem(item, wxCHK_UNCHECKED);
  }

  for (const auto& pair : _visual_lookup) {
    pair.second->SetValue(0);
  }

  auto select = [&](const std::string& s) {
    auto it = _tree_lookup.find(s);
    if (it != _tree_lookup.end()) {
      _tree->CheckItem(it->second);
    }
  };

  auto it = _session.program_map().find(_item_selected);
  if (it != _session.program_map().end()) {
    for (const auto& theme : it->second.enabled_theme_name()) {
      select(theme);
    }
    for (const auto& visual : it->second.visual_type()) {
      auto jt = _visual_lookup.find(visual.type());
      if (jt != _visual_lookup.end()) {
        jt->second->SetValue(visual.random_weight());
      }
    }
  }
}

void ProgramPage::RefreshData()
{
  _item_list->RefreshData();
  RefreshOurData();
}

void ProgramPage::RefreshThemes()
{
  _tree->DeleteAllItems();
  _tree_lookup.clear();

  std::vector<std::string> themes;
  for (const auto& pair : _session.theme_map()) {
    themes.push_back(pair.first);
  }
  std::sort(themes.begin(), themes.end());

  for (const auto& theme : themes) {
    wxClientData* data = new wxStringClientData{theme};
    auto item = _tree->AppendItem(_tree->GetRootItem(), theme, -1, -1, data);
    _tree_lookup[theme] = item;
  }

  std::set<std::string> theme_set{themes.begin(), themes.end()};
  for (auto& pair : *_session.mutable_program_map()) {
    for (auto it = pair.second.mutable_enabled_theme_name()->begin();
         it != pair.second.mutable_enabled_theme_name()->end();) {
      it = theme_set.count(*it) ? 1 + it :
          pair.second.mutable_enabled_theme_name()->erase(it);
    }
  }
}