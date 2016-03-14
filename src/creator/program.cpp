#include "program.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#include <wx/splitter.h>
#pragma warning(pop)

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
  auto right = new wxBoxSizer{wxHORIZONTAL};
  auto right_buttons = new wxBoxSizer{wxVERTICAL};

  auto left_splitter = new wxSplitterWindow{
      left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  left_splitter->SetSashGravity(0.5);
  left_splitter->SetMinimumPaneSize(128);

  auto leftleft_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftleft = new wxBoxSizer{wxVERTICAL};
  auto leftright_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftright = new wxBoxSizer{wxVERTICAL};

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

  leftleft->Add(_tree, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  leftleft_panel->SetSizer(leftleft);
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