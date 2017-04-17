#include "launch.h"
#include <common/common.h>
#include <common/session.h>
#include <creator/main.h>
#include <deque>

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
    TimeBounds main_content;
    TimeBounds loop_content;
  };

  // A stack frame of a node in the play graph.
  struct NodeEntry {
    bool operator==(const NodeEntry& node) const
    {
      return name == node.name && subroutine_count == node.subroutine_count;
    }
    std::string name;
    int subroutine_count;
  };
  // A node in the play graph.
  using Node = std::vector<NodeEntry>;
  struct NodeHash {
    size_t operator()(const Node& node) const
    {
      size_t hash = 0;
      for (const auto& entry : node) {
        hash_combine(hash, entry.name);
        hash_combine(hash, entry.subroutine_count);
      }
      return hash;
    }
  };
  // A directed edge in the play graph.
  struct Edge {
    size_t target_index;
    uint64_t time_seconds;
  };
  // A path in the final exhaustive search.
  struct SearchPath {
    size_t current_index;
    uint64_t main_content;
    uint64_t loop_content;
  };

  struct TarjanData {
    size_t index;
    size_t lowlink;
    bool on_stack;
  };

  PlayTime calculate_play_time(const trance_pb::Session& session,
                               const std::unordered_map<std::string, std::string>& variables)
  {
    Node first_node{{session.first_playlist_item(), 0}};
    std::unordered_map<Node, size_t, NodeHash> index_map;
    std::unordered_map<size_t, std::vector<Edge>> adjacencies;

    // Search starting from the first item to build the play graph.
    index_map[first_node] = 0;
    std::deque<Node> queue{first_node};

    auto handle_outgoing = [&](const Node& source, const Node& next, size_t time_seconds) {
      auto jt = index_map.find(next);
      if (jt == index_map.end()) {
        queue.push_back(next);
        jt = index_map.emplace(next, index_map.size()).first;
      }
      adjacencies[index_map[source]].push_back({jt->second, time_seconds});
    };

    while (!queue.empty()) {
      const auto node = queue.front();
      queue.pop_front();

      auto it = session.playlist().find(node.back().name);
      if (it == session.playlist().end()) {
        continue;
      }

      // Determine the outgoing edges.
      // Continuing a subplaylist.
      if (it->second.has_subroutine() && node.size() < MAXIMUM_STACK &&
          node.back().subroutine_count < it->second.subroutine().playlist_item_name_size()) {
        auto next = node;
        auto next_name = it->second.subroutine().playlist_item_name(next.back().subroutine_count);
        ++next.back().subroutine_count;

        next.push_back({next_name, 0});
        // Break infinite loops.
        for (auto it = next.begin(); it != next.end(); ++it) {
          if (*it == *std::prev(next.end())) {
            next.erase(std::next(it), next.end());
            break;
          }
        }
        handle_outgoing(node, next, 0);
        continue;
      }

      bool any_next = false;
      auto time_seconds = it->second.has_standard() ? it->second.standard().play_time_seconds() : 0;
      // Continuing a next item.
      for (const auto& next_item : it->second.next_item()) {
        if (!is_enabled(next_item, variables)) {
          continue;
        }
        auto next = node;
        next.back().name = next_item.playlist_item_name();
        next.back().subroutine_count = 0;
        handle_outgoing(node, next, time_seconds);
        any_next = true;
      }
      // Finishing a playlist.
      if (!any_next && node.size() > 1) {
        auto next = node;
        next.pop_back();
        handle_outgoing(node, next, time_seconds);
      } else if (!any_next) {
        handle_outgoing(node, node, 0);
      }
    }

    // Run Tarjan's strongly-connected components starting at the first node (guaranteed to cover
    // the whole graph since we built it from a search starting here).
    size_t tarjan_index = 0;
    std::unordered_map<size_t, TarjanData> tarjan_data;
    std::vector<size_t> tarjan_stack;
    std::vector<std::unordered_set<size_t>> strong_components;

    std::function<void(size_t)> run_tarjan = [&](size_t node_index) {
      tarjan_data[node_index] = {tarjan_index, tarjan_index, true};
      tarjan_stack.push_back(node_index);
      ++tarjan_index;

      for (const auto& edge : adjacencies[node_index]) {
        auto it = tarjan_data.find(edge.target_index);
        if (it == tarjan_data.end()) {
          run_tarjan(edge.target_index);
          tarjan_data[node_index].lowlink =
              std::min(tarjan_data[node_index].lowlink, tarjan_data[edge.target_index].lowlink);
        } else if (it->second.on_stack) {
          tarjan_data[node_index].lowlink =
              std::min(tarjan_data[node_index].lowlink, tarjan_data[edge.target_index].index);
        }
      }

      if (tarjan_data[node_index].lowlink == tarjan_data[node_index].index) {
        strong_components.emplace_back();
        auto it = tarjan_stack.end();
        while (it != tarjan_stack.begin()) {
          auto index = *--it;
          strong_components.back().insert(index);
          tarjan_data[index].on_stack = false;
          it = tarjan_stack.erase(it);
          if (index == node_index) {
            break;
          }
        }
      }
    };
    run_tarjan(0);

    // We now take a sort of modified condensation, which runs Dijkstra on each node within its
    // connected component to replace its edges with direct jumps to the sinks. This gives a DAG but
    // preserves the minimum distance across components.
    auto dijkstra = [&](size_t node, const std::unordered_set<size_t>& component) {
      std::unordered_map<size_t, uint64_t> distance_map;
      auto unvisited = component;
      distance_map[node] = 0;

      while (!unvisited.empty()) {
        // Could use priority queue.
        bool first = true;
        auto lowest_it = unvisited.begin();
        uint64_t lowest_distance = 0;
        for (auto it = lowest_it, end = unvisited.end(); it != end; ++it) {
          auto jt = distance_map.find(*it);
          if (jt != distance_map.end()) {
            if (first || jt->second < lowest_distance) {
              lowest_distance = jt->second;
              lowest_it = it;
            }
            first = false;
          }
        }
        auto current = *lowest_it;
        unvisited.erase(lowest_it);

        for (const auto& edge : adjacencies[current]) {
          auto distance = lowest_distance + edge.time_seconds;
          auto it = distance_map.find(edge.target_index);
          if (it == distance_map.end() || distance < it->second) {
            distance_map[edge.target_index] = distance;
          }
        }
      }

      std::vector<Edge> result;
      for (const auto& pair : distance_map) {
        if (component.find(pair.first) == component.end()) {
          result.push_back({pair.first, pair.second});
        }
      }
      return result;
    };

    std::unordered_map<size_t, uint64_t> loop_lengths;
    for (const auto& component : strong_components) {
      std::uint64_t loop_length = 0;
      for (size_t node : component) {
        // All edges must have the same length! Make sure it has an edge though
        // to exclude the one-element case.
        for (const auto& edge : adjacencies[node]) {
          if (component.find(edge.target_index) != component.end()) {
            loop_length += edge.time_seconds;
            break;
          }
        }
      }
      for (size_t node : component) {
        loop_lengths[node] = loop_length;
      }
      std::unordered_map<size_t, std::vector<Edge>> new_adjacencies;
      for (size_t node : component) {
        new_adjacencies[node] = dijkstra(node, component);
      }
      for (size_t node : component) {
        adjacencies[node] = new_adjacencies[node];
      }
    }

    // Finally, do an exhaustive search on the transformed graph. This bit could still be pretty
    // explosive if there are many possible paths even through the condensed graph...
    PlayTime result = {};
    std::deque<SearchPath> search_queue;
    search_queue.push_back({0, 0, loop_lengths[0]});
    while (!search_queue.empty()) {
      auto path = search_queue.front();
      search_queue.pop_front();

      const auto& edges = adjacencies[path.current_index];
      if (edges.empty()) {
        result.main_content.min_seconds =
            std::min(result.main_content.min_seconds, path.main_content);
        result.main_content.max_seconds =
            std::max(result.main_content.max_seconds, path.main_content);
        result.loop_content.min_seconds =
            std::min(result.loop_content.min_seconds, path.loop_content);
        result.loop_content.max_seconds =
            std::max(result.loop_content.max_seconds, path.loop_content);
      }
      for (const auto& edge : edges) {
        search_queue.push_back({edge.target_index, path.main_content + edge.time_seconds,
                                path.loop_content + loop_lengths[edge.target_index]});
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
  std::vector<std::string> variable_names;
  for (const auto& pair : session.variable_map()) {
    variable_names.push_back(pair.first);
  }
  std::sort(variable_names.begin(), variable_names.end());
  for (const auto& variable_name : variable_names) {
    const auto& variable = session.variable_map().find(variable_name)->second;
    wxStaticText* label = new wxStaticText{panel, wxID_ANY, variable_name + ":"};
    wxChoice* choice = new wxChoice{panel, wxID_ANY};
    int i = 0;
    auto jt = last_variables.find(variable_name);
    bool has_last_value = jt != last_variables.end() &&
        std::find(variable.value().begin(), variable.value().end(), jt->second) !=
            variable.value().end();
    std::vector<std::string> values{variable.value().begin(), variable.value().end()};
    std::sort(values.begin(), values.end());
    for (const auto& value : values) {
      choice->Append(value);
      if ((has_last_value && value == jt->second) ||
          (!has_last_value && value == variable.default_value())) {
        choice->SetSelection(i);
      }
      ++i;
    }
    _variables[variable_name] = choice;

    label->SetToolTip(variable.description());
    choice->SetToolTip(variable.description());
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

  auto main_content = format(play_time.main_content);
  auto loop_content = format(play_time.loop_content);
  _text->SetLabel("Main sequence: " + main_content + ".\nLooping content: " + loop_content + ".");
}