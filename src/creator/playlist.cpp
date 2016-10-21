#include "playlist.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"
#include "../session.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/wrapsizer.h>
#pragma warning(pop)

namespace {
  const std::string IS_FIRST_TOOLTIP =
      "Whether this is the first playlist item used when the session starts.";

  const std::string PROGRAM_TOOLTIP =
      "The program used for the duration of this playlist item.";

  const std::string PLAY_TIME_SECONDS_TOOLTIP =
      "The duration (in seconds) that this playlist item lasts. "
      "After the time is up, the next playlist item is chosen randomly based "
      "on the weights assigned below.";

  const std::string NEXT_ITEMS_TOOLTIP =
      "After the time is up, the next playlist item is chosen randomly based "
      "on the weights assigned below.";

  const std::string NEXT_ITEM_CHOICE_TOOLTIP =
      "Which playlist item might be chosen next.";

  const std::string NEXT_ITEM_WEIGHT_TOOLTIP =
      "A higher weight makes this entry more likely to be chosen next.";

  const std::string NEXT_ITEM_VARIABLE_TOOLTIP =
      "A variable whose value controls whether this entry is available.";

  const std::string NEXT_ITEM_VARIABLE_VALUE_TOOLTIP =
      "The value that the chosen condition variable must have in order for "
      "this entry to be available.";

  const std::string AUDIO_EVENT_TYPE_TOOLTIP =
      "What kind of audio change to apply.";

  const std::string AUDIO_EVENT_CHANNEL_TOOLTIP =
      "Which audio channel this audio event applies to.";

  const std::string AUDIO_EVENT_PATH_TOOLTIP =
      "Audio file to play.";

  const std::string AUDIO_EVENT_LOOP_TOOLTIP =
      "Whether to loop the file forever (or until another event interrupts it).";

  const std::string AUDIO_EVENT_INITIAL_VOLUME_TOOLTIP =
      "The initial volume of the audio channel used to play this file.";

  const std::string AUDIO_EVENT_FADE_VOLUME_TOOLTIP =
      "Target volume of the audio channel after this volume fade.";

  const std::string AUDIO_EVENT_FADE_TIME_TOOLTIP =
      "Time (in seconds) over which to apply the volume change.";
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
  bottom_splitter->SetSashGravity(0.5);
  bottom_splitter->SetMinimumPaneSize(128);

  _left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxStaticBoxSizer{wxVERTICAL, _left_panel, "Playlist"};
  _right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  _audio_events_sizer =
      new wxStaticBoxSizer{wxVERTICAL, _right_panel, "Audio events"};

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
  _is_first = new wxCheckBox{_left_panel, wxID_ANY, "First playlist item"};
  _is_first->SetToolTip(IS_FIRST_TOOLTIP);
  left->Add(_is_first, 0, wxALL, DEFAULT_BORDER);
  label = new wxStaticText{_left_panel, wxID_ANY, "Program:"};
  label->SetToolTip(PROGRAM_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  _program = new wxChoice{_left_panel, wxID_ANY};
  _program->SetToolTip(PROGRAM_TOOLTIP);
  left->Add(_program, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{
      _left_panel, wxID_ANY, "Play time (seconds):"};
  label->SetToolTip(PLAY_TIME_SECONDS_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  _play_time_seconds = new wxSpinCtrl{_left_panel, wxID_ANY};
  _play_time_seconds->SetToolTip(PLAY_TIME_SECONDS_TOOLTIP);
  _play_time_seconds->SetRange(0, 86400);
  left->Add(_play_time_seconds, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{_left_panel, wxID_ANY, "Next playlist items:"};
  label->SetToolTip(NEXT_ITEMS_TOOLTIP);
  left->Add(label, 0, wxALL, DEFAULT_BORDER);
  _next_items_sizer = new wxBoxSizer{wxVERTICAL};
  left->Add(_next_items_sizer, 0, wxEXPAND);

  _left_panel->SetSizer(left);
  _right_panel->SetSizer(_audio_events_sizer);
  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(_left_panel, _right_panel);
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
    for (unsigned int i = 0; i < _program->GetCount(); ++i) {
      if (_program->GetString(i) == it->second.program()) {
        _program->SetSelection(i);
        break;
      }
    }
    _play_time_seconds->SetValue(it->second.play_time_seconds());
    for (const auto& item : it->second.next_item()) {
      AddNextItem(item.playlist_item_name(), item.random_weight(),
                  item.condition_variable_name(),
                  item.condition_variable_value());
    }
    for (const auto& event : it->second.audio_event()) {
      AddAudioEvent(event);
    }
  }
  AddNextItem("", 0, "", "");
  AddAudioEvent({});
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

void PlaylistPage::RefreshDirectory(const std::string& directory)
{
  _audio_files.clear();
  search_audio_files(_audio_files, directory);
  std::sort(_audio_files.begin(), _audio_files.end());
}

void PlaylistPage::AddNextItem(const std::string& name,
                               std::uint32_t weight_value,
                               const std::string& variable,
                               const std::string& variable_value)
{
  std::vector<std::string> playlist_items;
  for (const auto& pair : _session.playlist()) {
    playlist_items.push_back(pair.first);
  }
  std::sort(playlist_items.begin(), playlist_items.end());

  auto sizer = new wxBoxSizer{wxHORIZONTAL};
  auto choice = new wxChoice{_left_panel, wxID_ANY};
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
  label = new wxStaticText{_left_panel, wxID_ANY, "Weight:"};
  label->SetToolTip(NEXT_ITEM_WEIGHT_TOOLTIP);
  box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  label->Enable(name != "");

  auto weight = new wxSpinCtrl{_left_panel, wxID_ANY};
  weight->SetToolTip(NEXT_ITEM_WEIGHT_TOOLTIP);
  weight->SetRange(name == "" ? 0 : 1, 100);
  weight->SetValue(weight_value);
  box_sizer->Add(weight, 0, wxALL, DEFAULT_BORDER);
  weight->Enable(name != "");
  wrap_sizer->Add(box_sizer, 0, wxEXPAND);

  box_sizer = new wxBoxSizer{wxHORIZONTAL};
  label = new wxStaticText{_left_panel, wxID_ANY, "Condition variable:"};
  label->SetToolTip(NEXT_ITEM_VARIABLE_TOOLTIP);
  box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  label->Enable(name != "");

  auto variable_choice = new wxChoice{_left_panel, wxID_ANY};
  variable_choice->SetToolTip(NEXT_ITEM_VARIABLE_TOOLTIP);
  variable_choice->Enable(name != "");
  variable_choice->Append("[None]");
  i = 1;
  for (const auto& pair : _session.variable_map()) {
    variable_choice->Append(pair.first);
    if (pair.first == variable) {
      variable_choice->SetSelection(i);
    }
    ++i;
  }
  box_sizer->Add(variable_choice, 0, wxALL, DEFAULT_BORDER);

  label = new wxStaticText{_left_panel, wxID_ANY, "Value:"};
  label->SetToolTip(NEXT_ITEM_VARIABLE_VALUE_TOOLTIP);
  box_sizer->Add(label, 0, wxALL, DEFAULT_BORDER);
  label->Enable(name != "" && variable != "");

  auto variable_value_choice = new wxChoice{_left_panel, wxID_ANY};
  variable_value_choice->SetToolTip(NEXT_ITEM_VARIABLE_VALUE_TOOLTIP);
  variable_value_choice->Enable(name != "" && variable != "");
  if (!variable.empty()) {
    auto it = _session.variable_map().find(variable);
    i = 0;
    for (const auto& value : it->second.value()) {
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
  _left_panel->Layout();

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
      it->second.mutable_next_item()->erase(
          index + it->second.mutable_next_item()->begin());
    }
    _creator_frame.MakeDirty(true);
    RefreshOurData();
  });

  weight->Bind(wxEVT_SPINCTRL, [&, index, weight](const wxCommandEvent&)
  {
    auto it = _session.mutable_playlist()->find(_item_selected);
    if (it == _session.mutable_playlist()->end()) {
      return;
    }
    it->second.mutable_next_item(int(index))->set_random_weight(
        weight->GetValue());
  });

  variable_choice->Bind(
      wxEVT_CHOICE,
      [&, index, variable_choice](const wxCommandEvent&) {
        auto it = _session.mutable_playlist()->find(_item_selected);
        if (it == _session.mutable_playlist()->end()) {
          return;
        }
        auto& item = *it->second.mutable_next_item(int(index));
        if (!variable_choice->GetSelection()) {
          item.clear_condition_variable_name();
          item.clear_condition_variable_value();
        } else {
          std::string name =
              variable_choice->GetString(variable_choice->GetSelection());
          item.set_condition_variable_name(name);
          auto variable_it = _session.variable_map().find(name);
          item.set_condition_variable_value(
              variable_it->second.default_value());
        }
        _creator_frame.MakeDirty(true);
        RefreshOurData();
      });

  variable_value_choice->Bind(
      wxEVT_CHOICE,
      [&, index, variable_value_choice](const wxCommandEvent&) {
        auto it = _session.mutable_playlist()->find(_item_selected);
        if (it == _session.mutable_playlist()->end()) {
          return;
        }
        auto& item = *it->second.mutable_next_item(int(index));
        std::string value = variable_value_choice->GetString(
            variable_value_choice->GetSelection());
        item.set_condition_variable_value(value);
        _creator_frame.MakeDirty(true);
        RefreshOurData();
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
        e.set_time_seconds(0);
      }
      e.set_type(trance_pb::AudioEvent::Type(type));
    } else if (it->second.audio_event_size() > int(index)) {
      it->second.mutable_audio_event()->erase(
          index + it->second.mutable_audio_event()->begin());
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
      it->second.mutable_audio_event(int(index))->set_channel(
          channel->GetValue());
      _creator_frame.MakeDirty(true);
    });
    wrap_sizer->Add(box_sizer, 0, wxEXPAND);
  }
  if (event.type() == trance_pb::AudioEvent::AUDIO_PLAY ||
      event.type() == trance_pb::AudioEvent::AUDIO_FADE) {
    box_sizer = new wxBoxSizer{wxHORIZONTAL};
    auto tooltip = event.type() == trance_pb::AudioEvent::AUDIO_PLAY ?
        AUDIO_EVENT_INITIAL_VOLUME_TOOLTIP : AUDIO_EVENT_FADE_VOLUME_TOOLTIP;
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
      it->second.mutable_audio_event(int(index))->set_volume(
          volume->GetValue());
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
    wrap_sizer->Add(box_sizer, 0, wxEXPAND);

    path_choice->Bind(wxEVT_CHOICE, [&, index, path_choice]
                                    (const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      auto p = path_choice->GetString(path_choice->GetSelection());
      it->second.mutable_audio_event(int(index))->set_path(p);
      _creator_frame.MakeDirty(true);
    });

    loop->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, [&, index, loop]
                                               (const wxCommandEvent&) {
      auto it = _session.mutable_playlist()->find(_item_selected);
      if (it == _session.mutable_playlist()->end()) {
        return;
      }
      it->second.mutable_audio_event(int(index))->set_loop(loop->GetValue());
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
      it->second.mutable_audio_event(int(index))->set_time_seconds(
          time->GetValue());
      _creator_frame.MakeDirty(true);
    });
  }

  sizer->Add(wrap_sizer, 1, wxEXPAND);
  _audio_events_sizer->Add(sizer, 0, wxEXPAND);
  _right_panel->Layout();
}