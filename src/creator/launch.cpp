#include "launch.h"
#include "main.h"
#include "../common.h"
#include "../session.h"

#pragma warning(push, 0)
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace {
  struct TimeBounds {
    uint64_t min_seconds = -1;
    uint64_t max_seconds = 0;
  };
  struct PlayTime {
    TimeBounds initial_sequence;
    TimeBounds after_looping;
  };

  PlayTime calculate_play_time(
      const trance_pb::Session& session,
      const std::unordered_map<std::string, std::string>& variables)
  {
    PlayTime result;
    struct LoopSearchEntry {
      std::string playlist_item;
      uint64_t seconds;
    };

    std::vector<std::vector<LoopSearchEntry>> queue;
    std::unordered_map<std::string, TimeBounds> loop_times;

    queue.push_back({{session.first_playlist_item(), 0}});
    while (!queue.empty()) {
      auto entry = queue.front();
      queue.erase(queue.begin());

      bool looped = false;
      for (auto it = entry.begin(); it != std::prev(entry.end()); ++it) {
        if (it->playlist_item == entry.back().playlist_item) {
          looped = true;
          auto initial_sequence = it->seconds;
          auto after_looping = entry.back().seconds - it->seconds;
          result.initial_sequence.max_seconds =
              std::max(result.initial_sequence.max_seconds, initial_sequence);
          result.initial_sequence.min_seconds =
              std::min(result.initial_sequence.min_seconds, initial_sequence);
          result.after_looping.max_seconds =
              std::max(result.after_looping.max_seconds, after_looping);
          result.after_looping.min_seconds =
              std::min(result.after_looping.min_seconds, after_looping);
          break;
        }
      }
      if (looped) {
        continue;
      }

      auto it = session.playlist().find(entry.back().playlist_item);
      if (it == session.playlist().end()) {
        continue;
      }
      bool any_next = false;
      for (const auto& next : it->second.next_item()) {
        if (!is_enabled(next, variables)) {
          continue;
        }
        queue.emplace_back(entry);
        queue.back().push_back({
            next.playlist_item_name(),
            entry.back().seconds + it->second.play_time_seconds()});
        any_next = true;
      }
      if (!any_next) {
        queue.emplace_back(entry);
        queue.back().emplace_back(entry.back());
      }
    }
    return result;
  }
}

LaunchFrame::LaunchFrame(
    CreatorFrame* parent, trance_pb::System& system,
    const trance_pb::Session& session, const std::string& session_path,
    const std::function<void()>& callback)
: wxFrame{parent, wxID_ANY, "Launch session",
          wxDefaultPosition, wxDefaultSize,
          wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN}
, _parent{parent}
, _system{system}
, _session{session}
, _session_path{session_path}
, _callback{callback}
{
  auto panel = new wxPanel{this};
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto top = new wxBoxSizer{wxVERTICAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};
  wxStaticBoxSizer* top_inner;
  if (!_session.variable_map().empty()) {
    top_inner = new wxStaticBoxSizer{
        wxVERTICAL, panel, "Variable configuration"};
  }
  auto top_bottom = new wxStaticBoxSizer{
      wxVERTICAL, panel, "Estimated running time"};

  std::unordered_map<std::string, std::string> last_variables;
  auto it = _system.last_session_map().find(_session_path);
  if (it != _system.last_session_map().end()) {
    last_variables.insert(it->second.variable_map().begin(),
                          it->second.variable_map().end());
  }

  for (const auto& pair : _session.variable_map()) {
    wxStaticText* label = new wxStaticText{panel, wxID_ANY, pair.first + ":"};
    wxChoice* choice = new wxChoice{panel, wxID_ANY};
    int i = 0;
    auto jt = last_variables.find(pair.first);
    bool has_last_value = jt != last_variables.end() &&
        std::find(pair.second.value().begin(), pair.second.value().end(),
                  jt->second) != pair.second.value().end();
    std::vector<std::string> values{pair.second.value().begin(),
                                    pair.second.value().end()};
    std::sort(values.begin(), values.end());
    for (const auto& value : values) {
      choice->Append(value);
      if ((has_last_value && value == jt->second) ||
          (!has_last_value && value == pair.second.default_value())) {
        choice->SetSelection(i);
      }
      ++i;
    }
    _variables[pair.first] = choice;

    label->SetToolTip(pair.second.description());
    choice->SetToolTip(pair.second.description());
    top_inner->Add(label, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
    top_inner->Add(choice, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

    choice->Bind(wxEVT_CHOICE,
                 [&](wxCommandEvent&) { RefreshTimeEstimate(); });
  }

  auto button_cancel = new wxButton{panel, ID_CANCEL, "Cancel"};
  auto button_launch = new wxButton{panel, ID_LAUNCH, "Launch"};
  bottom->Add(button_cancel, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(button_launch, 1, wxALL, DEFAULT_BORDER);

  if (!_session.variable_map().empty()) {
    auto button_defaults = new wxButton{panel, ID_DEFAULTS, "Defaults"};
    bottom->Add(button_defaults, 1, wxALL, DEFAULT_BORDER);

    Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
      for (const auto& pair : _variables) {
        auto it = _session.variable_map().find(pair.first);
        if (it == _session.variable_map().end()) {
          continue;
        }
        for (unsigned int i = 0; i < pair.second->GetCount(); ++i) {
          if (pair.second->GetString(i) == it->second.default_value()) {
            pair.second->SetSelection(i);
            break;
          }
        }
      }
      RefreshTimeEstimate();
    }, ID_DEFAULTS);
  }

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event)
  {
    _parent->LaunchClosed();
    Destroy();
  });

  if (!_session.variable_map().empty()) {
    top->Add(top_inner, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  }
  _text = new wxStaticText{panel, wxID_ANY, ""};
  top_bottom->Add(_text, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  top->Add(top_bottom, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  RefreshTimeEstimate();

  sizer->Add(top, 1, wxEXPAND, 0);
  sizer->Add(bottom, 0, wxEXPAND, 0);

  panel->SetSizer(sizer);
  SetClientSize(sizer->GetMinSize());

  Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
    Launch();
    Close();
  }, ID_LAUNCH);

  Bind(wxEVT_COMMAND_BUTTON_CLICKED,
       [&](wxCommandEvent&) { Close(); }, ID_CANCEL);

  Show(true);
}

void LaunchFrame::Launch()
{
  auto& last_session = (*_system.mutable_last_session_map())[_session_path];
  last_session.mutable_variable_map()->clear();
  for (const auto& pair : _variables) {
    (*last_session.mutable_variable_map())[pair.first] =
        pair.second->GetString(pair.second->GetSelection());
  }
  _parent->SaveSystem(false);
  _callback();
}

void LaunchFrame::RefreshTimeEstimate()
{
  std::unordered_map<std::string, std::string> variables;
  for (const auto& pair : _variables) {
    variables[pair.first] = pair.second->GetString(pair.second->GetSelection());
  }
  auto play_time = calculate_play_time(_session, variables);

  auto format = [](const TimeBounds& bounds) {
    if (bounds.min_seconds == bounds.max_seconds) {
      if (!bounds.min_seconds) {
        return std::string{"none"};
      }
      return format_time(bounds.min_seconds);
    }
    return format_time(bounds.min_seconds) +
        " to " + format_time(bounds.max_seconds);
  };

  auto initial_sequence = format(play_time.initial_sequence);
  auto after_looping = format(play_time.after_looping);
  _text->SetLabel("Main sequence: " + initial_sequence +
      ".\nLooping sequence: " + after_looping + ".");
}