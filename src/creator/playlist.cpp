#include "playlist.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace {
  const std::string IS_FIRST_TOOLTIP =
      "Whether this is the first playlist item when the session starts.";

  const std::string PROGRAM_TOOLTIP =
      "The program used for the duration of this playlist item.";

  const std::string PLAY_TIME_SECONDS_TOOLTIP =
      "The duration that this playlist item is played for. "
      "After the time is up, the next playlist item is chosen randomly based "
      "on the weights assigned below.";
}

PlaylistPage::PlaylistPage(wxNotebook* parent,
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
  auto left =
      new wxStaticBoxSizer{wxVERTICAL, left_panel, "Playlist"};
  auto right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto right =
      new wxStaticBoxSizer{wxVERTICAL, right_panel, "Events"};

  _item_list = new ItemList<trance_pb::PlaylistItem>{
      splitter, *session.mutable_playlist(), "playlist item",
      [&](const std::string& s) { _item_selected = s; RefreshOurData(); },
      std::bind(&CreatorFrame::PlaylistItemCreated, &_creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::PlaylistItemDeleted, &_creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::PlaylistItemRenamed, &_creator_frame,
                std::placeholders::_1, std::placeholders::_2)};

  wxStaticText* label = nullptr;
  _is_first = new wxCheckBox{left_panel, wxID_ANY, "First playlist item"};
  _is_first->SetToolTip(IS_FIRST_TOOLTIP);
  left->Add(_is_first, 0, wxALL, DEFAULT_BORDER);
  label = new wxStaticText{left_panel, wxID_ANY, "Program:"};
  label->SetToolTip(PROGRAM_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  _program = new wxChoice{left_panel, wxID_ANY};
  _program->SetToolTip(PROGRAM_TOOLTIP);
  left->Add(_program, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{
      left_panel, wxID_ANY, "Play time (seconds, 0 is forever):"};
  label->SetToolTip(PLAY_TIME_SECONDS_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  _play_time_seconds = new wxSpinCtrl{left_panel, wxID_ANY};
  _play_time_seconds->SetToolTip(PLAY_TIME_SECONDS_TOOLTIP);
  _play_time_seconds->SetRange(0, 86400);
  left->Add(_play_time_seconds, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  left_panel->SetSizer(left);
  right_panel->SetSizer(right);
  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(left_panel, right_panel);
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  _is_first->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, [&](wxCommandEvent&) {
    _session.set_first_playlist_item(_item_selected);
    _is_first->Enable(false);
    _creator_frame.MakeDirty(true);
  });

  _program->Bind(wxEVT_CHOICE, [&](wxCommandEvent& e) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.set_program(_program->GetString(_program->GetSelection()));
    _creator_frame.MakeDirty(true);
  });

  _play_time_seconds->Bind(wxEVT_SPINCTRL, [&](wxCommandEvent& e) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.set_play_time_seconds(_play_time_seconds->GetValue());
    _creator_frame.MakeDirty(true);
  });
}

PlaylistPage::~PlaylistPage()
{
}

void PlaylistPage::RefreshOurData()
{
  auto is_first = _session.first_playlist_item() == _item_selected;
  _is_first->SetValue(is_first);
  _is_first->Enable(!is_first);
  auto it = _session.playlist().find(_item_selected);
  if (it != _session.playlist().end()) {
    for (unsigned int i = 0; i < _program->GetCount(); ++i) {
      if (_program->GetString(i) == it->second.program()) {
        _program->SetSelection(i);
        break;
      }
    }
    _play_time_seconds->SetValue(it->second.play_time_seconds());
  }
}

void PlaylistPage::RefreshData()
{
  _item_list->RefreshData();
  RefreshOurData();
}

void PlaylistPage::RefreshProgramsAndPlaylists()
{
  _program->Clear();
  std::vector<std::string> programs;
  for (const auto& pair : _session.program_map()) {
    programs.push_back(pair.first);
  }
  std::sort(programs.begin(), programs.end());
  for (const auto& program : programs) {
    _program->Append(program);
  }
}