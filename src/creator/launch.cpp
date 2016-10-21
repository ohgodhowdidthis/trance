#include "launch.h"
#include "main.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#pragma warning(pop)

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
  auto top_inner = new wxStaticBoxSizer{
      wxVERTICAL, panel, "Variable configuration"};

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
    for (const auto& value : pair.second.value()) {
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
  }

  auto button_cancel = new wxButton{panel, ID_CANCEL, "Cancel"};
  auto button_defaults = new wxButton{panel, ID_DEFAULTS, "Defaults"};
  auto button_launch = new wxButton{panel, ID_LAUNCH, "Launch"};
  bottom->Add(button_cancel, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(button_defaults, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(button_launch, 1, wxALL, DEFAULT_BORDER);

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event)
  {
    _parent->LaunchClosed();
    Destroy();
  });

  top->Add(top_inner, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  sizer->Add(top, 1, wxEXPAND, 0);
  sizer->Add(bottom, 0, wxEXPAND, 0);

  panel->SetSizer(sizer);
  SetClientSize(sizer->GetMinSize());

  Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
    Launch();
    Close();
  }, ID_LAUNCH);
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
  }, ID_DEFAULTS);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED,
       [&](wxCommandEvent&) { Close(); }, ID_CANCEL);

  if (session.variable_map().empty()) {
    Launch();
    Close();
  } else {
    Show(true);
  }
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