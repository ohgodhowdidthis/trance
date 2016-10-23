#include "launch.h"
#include "../common.h"
#include "../session.h"
#include "main.h"

#pragma warning(push, 0)
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace
{
  struct TimeBounds {
    uint64_t min_seconds = -1;
    uint64_t max_seconds = 0;
  };
  struct PlayTime {
    TimeBounds initial_sequence;
    TimeBounds after_looping;
  };

  PlayTime calculate_play_time(const trance_pb::Session& session,
                               const std::unordered_map<std::string, std::string>& variables)
  {
    PlayTime result;
    struct PlayStackEntry {
      bool operator==(const PlayStackEntry& entry) const
      {
        return name == entry.name && subroutine_count == entry.subroutine_count;
      }
      std::string name;
      int subroutine_count;
    };
    struct LoopSearchEntry {
      std::vector<PlayStackEntry> playlist_stack;
      uint64_t seconds;
    };

    std::vector<std::vector<LoopSearchEntry>> queue;
    std::unordered_map<std::string, TimeBounds> loop_times;

    queue.push_back({{{{session.first_playlist_item(), 0}}, 0}});
    while (!queue.empty()) {
      auto entry = queue.front();
      queue.erase(queue.begin());

      bool looped = false;
      for (auto it = entry.begin(); it != std::prev(entry.end()); ++it) {
        if (it->playlist_stack == entry.back().playlist_stack) {
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

      auto it = session.playlist().find(entry.back().playlist_stack.back().name);
      if (it == session.playlist().end()) {
        continue;
      }

      if (it->second.has_subroutine() && entry.back().playlist_stack.size() < MAXIMUM_STACK &&
          entry.back().playlist_stack.back().subroutine_count <
              it->second.subroutine().playlist_item_name_size()) {
        queue.emplace_back(entry);
        queue.back().emplace_back(entry.back());
        ++queue.back().back().playlist_stack.back().subroutine_count;
        queue.back().back().playlist_stack.push_back(
            {it->second.subroutine().playlist_item_name(
                 entry.back().playlist_stack.back().subroutine_count),
             0});
        continue;
      }

      bool any_next = false;
      for (const auto& next : it->second.next_item()) {
        if (!is_enabled(next, variables)) {
          continue;
        }
        queue.emplace_back(entry);
        queue.back().emplace_back(entry.back());
        queue.back().back().playlist_stack.back().name = next.playlist_item_name();
        if (it->second.has_standard()) {
          queue.back().back().seconds += it->second.standard().play_time_seconds();
        }
        any_next = true;
      }
      if (!any_next && entry.back().playlist_stack.size() > 1) {
        queue.emplace_back(entry);
        queue.back().emplace_back(entry.back());
        queue.back().back().playlist_stack.pop_back();
        if (it->second.has_standard()) {
          queue.back().back().seconds += it->second.standard().play_time_seconds();
        }
      } else if (!any_next) {
        queue.emplace_back(entry);
        queue.back().emplace_back(entry.back());
      }
    }
    return result;
  }
}

VariableConfiguration::VariableConfiguration(trance_pb::System& system,
                                             const trance_pb::Session& session,
                                             const std::string& session_path,
                                             const std::function<void()>& on_change, wxPanel* panel)
: _system{system}
, _session{session}
, _session_path{session_path}
, _on_change{on_change}
, _sizer{nullptr}
{
  std::unordered_map<std::string, std::string> last_variables;
  auto it = system.last_session_map().find(session_path);
  if (it != system.last_session_map().end()) {
    last_variables.insert(it->second.variable_map().begin(), it->second.variable_map().end());
  }

  if (!session.variable_map().empty()) {
    _sizer = new wxStaticBoxSizer{wxVERTICAL, panel, "Variable configuration"};
  }
  for (const auto& pair : session.variable_map()) {
    wxStaticText* label = new wxStaticText{panel, wxID_ANY, pair.first + ":"};
    wxChoice* choice = new wxChoice{panel, wxID_ANY};
    int i = 0;
    auto jt = last_variables.find(pair.first);
    bool has_last_value = jt != last_variables.end() &&
        std::find(pair.second.value().begin(), pair.second.value().end(), jt->second) !=
            pair.second.value().end();
    std::vector<std::string> values{pair.second.value().begin(), pair.second.value().end()};
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
    _sizer->Add(label, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
    _sizer->Add(choice, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

    choice->Bind(wxEVT_CHOICE, [&](wxCommandEvent&) { _on_change(); });
  }
}

std::unordered_map<std::string, std::string> VariableConfiguration::Variables() const
{
  std::unordered_map<std::string, std::string> variables;
  for (const auto& pair : _variables) {
    variables[pair.first] = pair.second->GetString(pair.second->GetSelection());
  }
  return variables;
}

wxSizer* VariableConfiguration::Sizer() const
{
  return _sizer;
}

void VariableConfiguration::SaveToSystem(CreatorFrame* parent) const
{
  auto& last_session = (*_system.mutable_last_session_map())[_session_path];
  last_session.mutable_variable_map()->clear();
  for (const auto& pair : _variables) {
    (*last_session.mutable_variable_map())[pair.first] =
        pair.second->GetString(pair.second->GetSelection());
  }
  parent->SaveSystem(false);
}

void VariableConfiguration::ResetDefaults()
{
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
  _on_change();
}

LaunchFrame::LaunchFrame(CreatorFrame* parent, trance_pb::System& system,
                         const trance_pb::Session& session, const std::string& session_path)
: wxFrame{parent,           wxID_ANY,
          "Launch session", wxDefaultPosition,
          wxDefaultSize,    wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN}
, _parent{parent}
, _system{system}
, _session{session}
, _session_path{session_path}
{
  auto panel = new wxPanel{this};
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto top = new wxBoxSizer{wxVERTICAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};
  if (!_session.variable_map().empty()) {
    _configuration.reset(new VariableConfiguration{_system, _session, _session_path,
                                                   [&] { RefreshTimeEstimate(); }, panel});
    top->Add(_configuration->Sizer(), 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  }
  auto top_bottom = new wxStaticBoxSizer{wxVERTICAL, panel, "Estimated running time"};

  auto button_cancel = new wxButton{panel, wxID_ANY, "Cancel"};
  bottom->Add(button_cancel, 1, wxALL, DEFAULT_BORDER);
  if (_configuration) {
    auto button_defaults = new wxButton{panel, wxID_ANY, "Defaults"};
    bottom->Add(button_defaults, 1, wxALL, DEFAULT_BORDER);

    button_defaults->Bind(wxEVT_COMMAND_BUTTON_CLICKED,
                          [&](wxCommandEvent&) { _configuration->ResetDefaults(); });
  }
  auto button_launch = new wxButton{panel, wxID_ANY, "Launch"};
  bottom->Add(button_launch, 1, wxALL, DEFAULT_BORDER);

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) {
    _parent->LaunchClosed();
    Destroy();
  });

  _text = new wxStaticText{panel, wxID_ANY, ""};
  top_bottom->Add(_text, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  top->Add(top_bottom, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  RefreshTimeEstimate();

  sizer->Add(top, 1, wxEXPAND, 0);
  sizer->Add(bottom, 0, wxEXPAND, 0);

  button_launch->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
    Launch();
    Close();
  });

  button_cancel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) { Close(); });

  panel->SetSizer(sizer);
  SetClientSize(sizer->GetMinSize());
  CentreOnParent();
  Show(true);
}

void LaunchFrame::Launch()
{
  if (_configuration) {
    _configuration->SaveToSystem(_parent);
  }
  _parent->Launch();
}

void LaunchFrame::RefreshTimeEstimate()
{
  std::unordered_map<std::string, std::string> variables;
  if (_configuration) {
    variables = _configuration->Variables();
  }
  auto play_time = calculate_play_time(_session, variables);

  auto format = [](const TimeBounds& bounds) {
    if (bounds.min_seconds == bounds.max_seconds) {
      if (!bounds.min_seconds) {
        return std::string{"none"};
      }
      return format_time(bounds.min_seconds, false);
    }
    return format_time(bounds.min_seconds, false) + " to " + format_time(bounds.max_seconds, false);
  };

  auto initial_sequence = format(play_time.initial_sequence);
  auto after_looping = format(play_time.after_looping);
  _text->SetLabel("Main sequence: " + initial_sequence + ".\nLooping sequence: " + after_looping +
                  ".");
}