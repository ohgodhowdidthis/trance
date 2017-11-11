#include "playlist.h"
#include <common/common.h>
#include <common/session.h>
#include <creator/item_list.h>
#include <creator/main.h>

#pragma warning(push, 0)
#include <common/trance.pb.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/wrapsizer.h>
#pragma warning(pop)

namespace
{
  const std::string IS_FIRST_TOOLTIP =
      "Whether this is the first playlist item used when the session starts.";

  const std::string STANDARD_TOOLTIP =
      "A standard playlist item that uses a particular program for a given duration.";

  const std::string SUBROUTINE_TOOLTIP =
      "A subroutine playlist item invokes one or more other playlists in sequence. "
      "After each item in the sequence finishes (by reaching a state where there is "
      "nothing more to play), control returns to the subroutine.";

  const std::string PROGRAM_TOOLTIP = "The program used for the duration of this playlist item.";

  const std::string PLAY_TIME_SECONDS_TOOLTIP =
      "The duration (in seconds) that this playlist item lasts. "
      "After the time is up, the next playlist item is chosen randomly based "
      "on the weights assigned below.";

  const std::string SUBROUTINE_ITEM_TOOLTIP =
      "A subplaylist invoked by this subroutine. When the subplaylist finishes "
      "because there is no next playlist item available, control returns to the "
      "subroutine. When there are no more subplaylists, a new playlist item is "
      "chosen from the next playlist items of the subroutine.";

  const std::string NEXT_ITEM_CHOICE_TOOLTIP = "Which playlist item might be chosen next.";

  const std::string NEXT_ITEM_WEIGHT_TOOLTIP =
      "A higher weight makes this entry more likely to be chosen next.";

  const std::string NEXT_ITEM_VARIABLE_TOOLTIP =
      "A variable whose value controls whether this entry is available.";

  const std::string NEXT_ITEM_VARIABLE_VALUE_TOOLTIP =
      "The value that the chosen condition variable must have in order for "
      "this entry to be available.";

  const std::string AUDIO_EVENT_TYPE_TOOLTIP = "What kind of audio change to apply.";

  const std::string AUDIO_EVENT_CHANNEL_TOOLTIP =
      "Which audio channel this audio event applies to.";

  const std::string AUDIO_EVENT_PATH_TOOLTIP = "Audio file to play.";

  const std::string AUDIO_EVENT_LOOP_TOOLTIP =
      "Whether to loop the file forever (or until another event interrupts it).";

  const std::string AUDIO_EVENT_NEXT_UNUSED_CHANNEL_TOOLTIP =
      "Play audio on the next channel that isn't currently in use.";

  const std::string AUDIO_EVENT_INITIAL_VOLUME_TOOLTIP =
      "The initial volume of the audio channel used to play this file.";

  const std::string AUDIO_EVENT_FADE_VOLUME_TOOLTIP =
      "Target volume of the audio channel after this volume fade.";

  const std::string AUDIO_EVENT_FADE_TIME_TOOLTIP =
      "Time (in seconds) over which to apply the volume change.";
}

PlaylistPage::PlaylistPage(wxNotebook* parent, CreatorFrame& creator_frame,
                           trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
, _creator_frame{creator_frame}
, _session(session)
, _program{nullptr}
, _play_time_seconds{nullptr}
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto splitter = new wxSplitterWindow{this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  splitter->SetSashGravity(0);
  splitter->SetMinimumPaneSize(128);

  auto bottom_panel = new wxPanel{splitter, wxID_ANY};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  auto bottom_bottom_splitter = new wxSplitterWindow{
      bottom_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  bottom_bottom_splitter->SetSashGravity(0.5);
  bottom_bottom_splitter->SetMinimumPaneSize(128);

  _bottom_bottom_panel = new wxPanel{bottom_bottom_splitter, wxID_ANY};
  _next_items_sizer = new wxStaticBoxSizer{wxVERTICAL, _bottom_bottom_panel, "Next playlist items"};
  auto bottom_top_panel = new wxPanel{bottom_bottom_splitter, wxID_ANY};
  auto bottom_top = new wxBoxSizer{wxHORIZONTAL};

  auto bottom_splitter = new wxSplitterWindow{bottom_top_panel, wxID_ANY, wxDefaultPosition,
                                              wxDefaultSize, wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  bottom_splitter->SetSashGravity(0.5);
  bottom_splitter->SetMinimumPaneSize(128);

  _left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxStaticBoxSizer{wxVERTICAL, _left_panel, "Playlist item"};
  _right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  _audio_events_sizer = new wxStaticBoxSizer{wxVERTICAL, _right_panel, "Audio events"};

  _item_list = new ItemList<trance_pb::PlaylistItem>{
      splitter,
      *session.mutable_playlist(),
      "playlist item",
      [&](const std::string& s) {
        _item_selected = s;
        RefreshOurData();
      },
      std::bind(&CreatorFrame::PlaylistItemCreated, &_creator_frame, std::placeholders::_1),
      std::bind(&CreatorFrame::PlaylistItemDeleted, &_creator_frame, std::placeholders::_1),
      std::bind(&CreatorFrame::PlaylistItemRenamed, &_creator_frame, std::placeholders::_1,
                std::placeholders::_2)};

  auto options_sizer = new wxWrapSizer{wxHORIZONTAL};

  _is_first = new wxCheckBox{_left_panel, wxID_ANY, "First playlist item"};
  _is_first->SetToolTip(IS_FIRST_TOOLTIP);
  options_sizer->Add(_is_first, 0, wxALL, DEFAULT_BORDER);

  _standard = new wxRadioButton{_left_panel,       wxID_ANY,      "Standard",
                                wxDefaultPosition, wxDefaultSize, wxRB_GROUP};
  _standard->SetToolTip(STANDARD_TOOLTIP);
  options_sizer->Add(_standard, 0, wxALL, DEFAULT_BORDER);
  _subroutine = new wxRadioButton{_left_panel, wxID_ANY, "Subroutine"};
  _subroutine->SetToolTip(SUBROUTINE_TOOLTIP);
  options_sizer->Add(_subroutine, 0, wxALL, DEFAULT_BORDER);
  left->Add(options_sizer);

  _playlist_item_sizer = new wxBoxSizer{wxVERTICAL};
  left->Add(_playlist_item_sizer, 1, wxEXPAND);
  SwitchToStandard();

  _bottom_bottom_panel->SetSizer(_next_items_sizer);
  _left_panel->SetSizer(left);
  _right_panel->SetSizer(_audio_events_sizer);

  bottom_top->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(_left_panel, _right_panel);
  bottom_top_panel->SetSizer(bottom_top);

  bottom->Add(bottom_bottom_splitter, 1, wxEXPAND, 0);
  bottom_bottom_splitter->SplitHorizontally(bottom_top_panel, _bottom_bottom_panel);
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  _is_first->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, [&](wxCommandEvent&) {
    _session.set_first_playlist_item(_item_selected);
    _is_first->Enable(false);
    _creator_frame.MakeDirty(true);
  });

  _standard->Bind(wxEVT_RADIOBUTTON, [&](wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    auto& standard = *it->second.mutable_standard();
    if (_session.program_map().find(standard.program()) == _session.program_map().end() &&
        !_session.program_map().empty()) {
      standard.set_program(_session.program_map().begin()->first);
    }
    RefreshOurData();
    _creator_frame.MakeDirty(true);
  });

  _subroutine->Bind(wxEVT_RADIOBUTTON, [&](wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.mutable_subroutine();
    RefreshOurData();
    _creator_frame.MakeDirty(true);
  });
}

PlaylistPage::~PlaylistPage()
{
}

void PlaylistPage::RefreshOurData()
{
  for (const auto& item : _next_items) {
    item->Clear(true);
  }
  for (const auto& item : _audio_events) {
    item->Clear(true);
  }
  _next_items_sizer->Clear(true);
  _audio_events_sizer->Clear(true);
  _next_items.clear();
  _audio_events.clear();

  auto is_first = _session.first_playlist_item() == _item_selected;
  _is_first->SetValue(is_first);
  _is_first->Enable(!is_first);
  auto it = _session.playlist().find(_item_selected);
  if (it != _session.playlist().end()) {
    if (it->second.has_standard()) {
      SwitchToStandard();

      for (unsigned int i = 0; i < _program->GetCount(); ++i) {
        if (_program->GetString(i) == it->second.standard().program()) {
          _program->SetSelection(i);
          break;
        }
      }
      _play_time_seconds->SetValue(it->second.standard().play_time_seconds());
    } else if (it->second.has_subroutine()) {
      SwitchToSubroutine();

      for (const auto& subroutine_item : it->second.subroutine().playlist_item_name()) {
        AddSubroutineItem(subroutine_item);
      }
      AddSubroutineItem("");
    }

    for (const auto& item : it->second.next_item()) {
      AddNextItem(item.playlist_item_name(), item.random_weight(), item.condition_variable_name(),
                  item.condition_variable_value());
    }
    for (const auto& event : it->second.audio_event()) {
      AddAudioEvent(event);
    }
  }
  AddNextItem("", 0, "", "");
  AddAudioEvent({});
  _bottom_bottom_panel->Layout();
}

void PlaylistPage::RefreshData()
{
  _item_list->RefreshData();
  RefreshOurData();
}

void PlaylistPage::RefreshProgramsAndPlaylists()
{
  if (_program) {
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
}

void PlaylistPage::RefreshDirectory(const std::string& directory)
{
  _audio_files.clear();
  search_audio_files(_audio_files, directory);
  std::sort(_audio_files.begin(), _audio_files.end());
}

void PlaylistPage::SwitchToStandard()
{
  _standard->SetValue(true);
  _playlist_item_sizer->Clear(true);
  _program = nullptr;
  _play_time_seconds = nullptr;
  _subroutine_items.clear();
  wxStaticText* label = nullptr;

  label = new wxStaticText{_left_panel, wxID_ANY, "Program:"};
  label->SetToolTip(PROGRAM_TOOLTIP);
  _playlist_item_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);

  _program = new wxChoice{_left_panel, wxID_ANY};
  _program->SetToolTip(PROGRAM_TOOLTIP);
  _playlist_item_sizer->Add(_program, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  label = new wxStaticText{_left_panel, wxID_ANY, "Play time (seconds):"};
  label->SetToolTip(PLAY_TIME_SECONDS_TOOLTIP);
  _playlist_item_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);

  _play_time_seconds = new wxSpinCtrl{_left_panel, wxID_ANY};
  _play_time_seconds->SetToolTip(PLAY_TIME_SECONDS_TOOLTIP);
  _play_time_seconds->SetRange(0, 86400);
  _playlist_item_sizer->Add(_play_time_seconds, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  _left_panel->Layout();
  RefreshProgramsAndPlaylists();

  _program->Bind(wxEVT_CHOICE, [&](wxCommandEvent& e) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.mutable_standard()->set_program(_program->GetString(_program->GetSelection()));
    _creator_frame.MakeDirty(true);
  });

  _play_time_seconds->Bind(wxEVT_SPINCTRL, [&](wxCommandEvent& e) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.mutable_standard()->set_play_time_seconds(_play_time_seconds->GetValue());
    _creator_frame.MakeDirty(true);
  });
}

void PlaylistPage::SwitchToSubroutine()
{
  _subroutine->SetValue(true);
  _playlist_item_sizer->Clear(true);
  _program = nullptr;
  _play_time_seconds = nullptr;
  _subroutine_items.clear();

  auto label = new wxStaticText{_left_panel, wxID_ANY, "Subroutine playlist items:"};
  label->SetToolTip(SUBROUTINE_TOOLTIP);
  _playlist_item_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  RefreshProgramsAndPlaylists();
}

void PlaylistPage::AddSubroutineItem(const std::string& playlist_item_name)
{
  auto choice = new wxChoice{_left_panel, wxID_ANY};
  choice->SetToolTip(SUBROUTINE_ITEM_TOOLTIP);
  _playlist_item_sizer->Add(choice, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  std::vector<std::string> playlist_items;
  for (const auto& pair : _session.playlist()) {
    playlist_items.push_back(pair.first);
  }
  std::sort(playlist_items.begin(), playlist_items.end());
  choice->Append("");
  int i = 1;
  for (const auto& item_name : playlist_items) {
    choice->Append(item_name);
    if (item_name == playlist_item_name) {
      choice->SetSelection(i);
    }
    ++i;
  }
  if (playlist_item_name.empty()) {
    choice->SetSelection(0);
  }

  auto index = _subroutine_items.size();
  _subroutine_items.push_back(choice);
  _left_panel->Layout();

  choice->Bind(wxEVT_CHOICE, [&, index, choice](wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    std::string name = choice->GetString(choice->GetSelection());
    if (name != "") {
      while (it->second.subroutine().playlist_item_name_size() <= int(index)) {
        it->second.mutable_subroutine()->add_playlist_item_name(name);
      }
      it->second.mutable_subroutine()->set_playlist_item_name(int(index), name);
    } else if (it->second.subroutine().playlist_item_name_size() > int(index)) {
      it->second.mutable_subroutine()->mutable_playlist_item_name()->erase(
          index + it->second.mutable_subroutine()->mutable_playlist_item_name()->begin());
    }
    _creator_frame.MakeDirty(true);
    RefreshOurData();
  });
}

void PlaylistPage::AddNextItem(const std::string& name, std::uint32_t weight_value,
                               const std::string& variable, const std::string& variable_value)
{
  std::vector<std::string> playlist_items;
  for (const auto& pair : _session.playlist()) {
    playlist_items.push_back(pair.first);
  }
  std::sort(playlist_items.begin(), playlist_items.end());

  auto sizer = new wxBoxSizer{wxHORIZONTAL};
  auto choice = new wxChoice{_bottom_bottom_panel, wxID_ANY};
  choice->SetToolTip(NEXT_ITEM_CHOICE_TOOLTIP);
  choice->Append("");
  int i = 1;
  for (const auto& item : playlist_items) {
    choice->Append(item);
    if (item == name) {
      choice->SetSelection(i);
    }
    ++i;
  }
  sizer->Add(choice, 0, wxALL, DEFAULT_BORDER);
  auto wrap_sizer = new wxWrapSizer{wxHORIZONTAL};

  wxStaticText* label;
  wxSizer* box_sizer;

  box_sizer = new wxBoxSizer{wxHORIZONTAL};
  label = new wxStaticText{_bottom_bottom_panel, wxID_ANY, "Weight:"};
  label->SetToolTip(NEXT_ITEM_WEIGHT_TOOLTIP);
  box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  label->Enable(name != "");

  auto weight = new wxSpinCtrl{_bottom_bottom_panel, wxID_ANY};
  weight->SetToolTip(NEXT_ITEM_WEIGHT_TOOLTIP);
  weight->SetRange(name == "" ? 0 : 1, 100);
  weight->SetValue(weight_value);
  box_sizer->Add(weight, 0, wxALL, DEFAULT_BORDER);
  weight->Enable(name != "");
  wrap_sizer->Add(box_sizer, 0, wxEXPAND);

  box_sizer = new wxBoxSizer{wxHORIZONTAL};
  label = new wxStaticText{_bottom_bottom_panel, wxID_ANY, "Condition variable:"};
  label->SetToolTip(NEXT_ITEM_VARIABLE_TOOLTIP);
  box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  label->Enable(name != "");

  auto variable_choice = new wxChoice{_bottom_bottom_panel, wxID_ANY};
  variable_choice->SetToolTip(NEXT_ITEM_VARIABLE_TOOLTIP);
  variable_choice->Enable(name != "");
  variable_choice->Append("[None]");
  i = 1;
  std::vector<std::string> variables;
  for (const auto& pair : _session.variable_map()) {
    variables.push_back(pair.first);
  }
  std::sort(variables.begin(), variables.end());
  for (const auto& v : variables) {
    variable_choice->Append(v);
    if (v == variable) {
      variable_choice->SetSelection(i);
    }
    ++i;
  }
  box_sizer->Add(variable_choice, 0, wxALL, DEFAULT_BORDER);

  label = new wxStaticText{_bottom_bottom_panel, wxID_ANY, "Value:"};
  label->SetToolTip(NEXT_ITEM_VARIABLE_VALUE_TOOLTIP);
  box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  label->Enable(name != "" && variable != "");

  auto variable_value_choice = new wxChoice{_bottom_bottom_panel, wxID_ANY};
  variable_value_choice->SetToolTip(NEXT_ITEM_VARIABLE_VALUE_TOOLTIP);
  variable_value_choice->Enable(name != "" && variable != "");
  if (!variable.empty()) {
    auto it = _session.variable_map().find(variable);
    i = 0;
    std::vector<std::string> variable_values;
    for (const auto& value : it->second.value()) {
      variable_values.push_back(value);
    }
    std::sort(variable_values.begin(), variable_values.end());
    for (const auto& value : variable_values) {
      variable_value_choice->Append(value);
      if (value == variable_value) {
        variable_value_choice->SetSelection(i);
      }
      ++i;
    }
  }
  box_sizer->Add(variable_value_choice, 0, wxALL, DEFAULT_BORDER);
  wrap_sizer->Add(box_sizer, 0, wxEXPAND);

  sizer->Add(wrap_sizer, 1, wxEXPAND);
  _next_items_sizer->Add(sizer, 0, wxEXPAND);

  auto index = _next_items.size();
  _next_items.push_back(sizer);

  choice->Bind(wxEVT_CHOICE, [&, index, choice](const wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    std::string name = choice->GetString(choice->GetSelection());
    if (name != "") {
      while (it->second.next_item_size() <= int(index)) {
        it->second.add_next_item()->set_random_weight(1);
      }
      it->second.mutable_next_item(int(index))->set_playlist_item_name(name);
    } else if (it->second.next_item_size() > int(index)) {
      it->second.mutable_next_item()->erase(index + it->second.mutable_next_item()->begin());
    }
    _creator_frame.MakeDirty(true);
  });

  weight->Bind(wxEVT_SPINCTRL, [&, index, weight](const wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.mutable_next_item(int(index))->set_random_weight(weight->GetValue());
  });

  variable_choice->Bind(wxEVT_CHOICE, [&, index, variable_choice](const wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    auto& item = *it->second.mutable_next_item(int(index));
    if (!variable_choice->GetSelection()) {
      item.clear_condition_variable_name();
      item.clear_condition_variable_value();
    } else {
      std::string name = variable_choice->GetString(variable_choice->GetSelection());
      item.set_condition_variable_name(name);
      auto variable_it = _session.variable_map().find(name);
      item.set_condition_variable_value(variable_it->second.default_value());
    }
    _creator_frame.MakeDirty(true);
  });

  variable_value_choice->Bind(
      wxEVT_CHOICE, [&, index, variable_value_choice](const wxCommandEvent&) {
        auto it = _session.mutable_playlist()->find(_item_selected);
        if (it == _session.mutable_playlist()->end()) {
          return;
        }
        auto& item = *it->second.mutable_next_item(int(index));
        std::string value = variable_value_choice->GetString(variable_value_choice->GetSelection());
        item.set_condition_variable_value(value);
        _creator_frame.MakeDirty(true);
      });
}

void PlaylistPage::AddAudioEvent(const trance_pb::AudioEvent& event)
{
  auto sizer = new wxBoxSizer{wxHORIZONTAL};
  auto index = _audio_events.size();
  _audio_events.push_back(sizer);

  auto choice = new wxChoice{_right_panel, wxID_ANY};
  choice->SetToolTip(AUDIO_EVENT_TYPE_TOOLTIP);
  choice->Append("");
  choice->Append("Play file");
  choice->Append("Stop channel");
  choice->Append("Fade channel volume");
  choice->SetSelection(event.type());
  sizer->Add(choice, 0, wxALL, DEFAULT_BORDER);
  auto wrap_sizer = new wxWrapSizer{wxHORIZONTAL};

  choice->Bind(wxEVT_CHOICE, [&, index, choice](const wxCommandEvent&) {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    auto type = choice->GetSelection();
    if (type) {
      while (it->second.audio_event_size() <= int(index)) {
        it->second.add_audio_event();
      }
      auto& e = *it->second.mutable_audio_event(int(index));
      if (e.type() != type) {
        e.set_path("");
        e.set_loop(false);
        e.set_next_unused_channel(false);
        e.set_time_seconds(0);
      }
      e.set_type(trance_pb::AudioEvent::Type(type));
    } else if (it->second.audio_event_size() > int(index)) {
      it->second.mutable_audio_event()->erase(index + it->second.mutable_audio_event()->begin());
    }
    _creator_frame.MakeDirty(true);
    RefreshOurData();
  });

  wxSizer* box_sizer;
  if (event.type() != trance_pb::AudioEvent::NONE) {
    box_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto label = new wxStaticText{_right_panel, wxID_ANY, "Channel:"};
    label->SetToolTip(AUDIO_EVENT_CHANNEL_TOOLTIP);
    box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);

    auto channel = new wxSpinCtrl{_right_panel, wxID_ANY};
    channel->SetToolTip(AUDIO_EVENT_CHANNEL_TOOLTIP);
    channel->SetRange(0, 32);
    channel->SetValue(event.channel());
    box_sizer->Add(channel, 0, wxALL, DEFAULT_BORDER);

    channel->Bind(wxEVT_SPINCTRL, [&, index, channel](const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      it->second.mutable_audio_event(int(index))->set_channel(channel->GetValue());
      _creator_frame.MakeDirty(true);
    });
    wrap_sizer->Add(box_sizer, 0, wxEXPAND);
  }
  if (event.type() == trance_pb::AudioEvent::AUDIO_PLAY ||
      event.type() == trance_pb::AudioEvent::AUDIO_FADE) {
    box_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto tooltip = event.type() == trance_pb::AudioEvent::AUDIO_PLAY
        ? AUDIO_EVENT_INITIAL_VOLUME_TOOLTIP
        : AUDIO_EVENT_FADE_VOLUME_TOOLTIP;
    auto label = new wxStaticText{_right_panel, wxID_ANY, "Volume:"};
    label->SetToolTip(tooltip);
    box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);

    auto volume = new wxSpinCtrl{_right_panel, wxID_ANY};
    volume->SetToolTip(tooltip);
    volume->SetRange(0, 100);
    volume->SetValue(event.volume());
    box_sizer->Add(volume, 0, wxALL, DEFAULT_BORDER);

    volume->Bind(wxEVT_SPINCTRL, [&, index, volume](const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      it->second.mutable_audio_event(int(index))->set_volume(volume->GetValue());
      _creator_frame.MakeDirty(true);
    });
    wrap_sizer->Add(box_sizer, 0, wxEXPAND);
  }
  if (event.type() == trance_pb::AudioEvent::AUDIO_PLAY) {
    box_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto label = new wxStaticText{_right_panel, wxID_ANY, "File:"};
    label->SetToolTip(AUDIO_EVENT_PATH_TOOLTIP);
    box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);

    auto path_choice = new wxChoice{_right_panel, wxID_ANY};
    path_choice->SetToolTip(AUDIO_EVENT_PATH_TOOLTIP);
    int i = 0;
    for (const auto& p : _audio_files) {
      path_choice->Append(p);
      if (event.path() == p) {
        path_choice->SetSelection(i);
      }
      ++i;
    }
    box_sizer->Add(path_choice, 0, wxALL, DEFAULT_BORDER);

    auto loop = new wxCheckBox{_right_panel, wxID_ANY, "Loop"};
    loop->SetToolTip(AUDIO_EVENT_LOOP_TOOLTIP);
    loop->SetValue(event.loop());
    box_sizer->Add(loop, 0, wxALL, DEFAULT_BORDER);

    auto next_unused = new wxCheckBox{_right_panel, wxID_ANY, "Next unused channel"};
    next_unused->SetToolTip(AUDIO_EVENT_NEXT_UNUSED_CHANNEL_TOOLTIP);
    next_unused->SetValue(event.next_unused_channel());
    box_sizer->Add(next_unused, 0, wxALL, DEFAULT_BORDER);
    wrap_sizer->Add(box_sizer, 0, wxEXPAND);

    path_choice->Bind(wxEVT_CHOICE, [&, index, path_choice](const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      auto p = path_choice->GetString(path_choice->GetSelection());
      it->second.mutable_audio_event(int(index))->set_path(p);
      _creator_frame.MakeDirty(true);
    });

    loop->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, [&, index, loop](const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      it->second.mutable_audio_event(int(index))->set_loop(loop->GetValue());
      _creator_frame.MakeDirty(true);
    });

    next_unused->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED,
      [&, index, next_unused](const wxCommandEvent&) {
        auto it = _session.mutable_playlist()->find(_item_selected);
        if (it == _session.mutable_playlist()->end()) {
          return;
        }
        it->second.mutable_audio_event(int(index))->set_next_unused_channel(
            next_unused->GetValue());
        _creator_frame.MakeDirty(true);
      });
  }
  if (event.type() == trance_pb::AudioEvent::AUDIO_FADE) {
    box_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto label = new wxStaticText{_right_panel, wxID_ANY, "Time (seconds):"};
    label->SetToolTip(AUDIO_EVENT_FADE_TIME_TOOLTIP);
    box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);

    auto time = new wxSpinCtrl{_right_panel, wxID_ANY};
    time->SetToolTip(AUDIO_EVENT_FADE_TIME_TOOLTIP);
    time->SetRange(0, 3600);
    time->SetValue(event.time_seconds());
    box_sizer->Add(time, 0, wxALL, DEFAULT_BORDER);
    wrap_sizer->Add(box_sizer, 0, wxEXPAND);

    time->Bind(wxEVT_SPINCTRL, [&, index, time](const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      it->second.mutable_audio_event(int(index))->set_time_seconds(time->GetValue());
      _creator_frame.MakeDirty(true);
    });
  }

  sizer->Add(wrap_sizer, 1, wxEXPAND);
  _audio_events_sizer->Add(sizer, 0, wxEXPAND);
  _right_panel->Layout();
}