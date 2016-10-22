#include "variables.h"
#include <algorithm>
#include "../common.h"
#include "../session.h"
#include "item_list.h"
#include "main.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#pragma warning(pop)

namespace
{
  const std::string DEFAULT_VALUE_TOOLTIP = "The value to use by default.";
  const std::string DESCRIPTION_TOOLTIP =
      "Description of what this variable controls. Displayed when choosing "
      "a value for this variable on session launch.";
}

VariablePage::VariablePage(wxNotebook* parent, CreatorFrame& creator_frame,
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
  bottom_splitter->SetSashGravity(0.5);
  bottom_splitter->SetMinimumPaneSize(128);

  auto left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxStaticBoxSizer{wxVERTICAL, left_panel, "Variable values"};
  auto right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto right = new wxStaticBoxSizer{wxVERTICAL, right_panel, "Variable information"};
  auto left_buttons = new wxBoxSizer{wxVERTICAL};

  _item_list = new ItemList<trance_pb::Variable>{
      splitter, *session.mutable_variable_map(), "variable",
      [&](const std::string& s) {
        _item_selected = s;
        RefreshOurData();
      },
      std::bind(&CreatorFrame::VariableCreated, &_creator_frame, std::placeholders::_1),
      std::bind(&CreatorFrame::VariableDeleted, &_creator_frame, std::placeholders::_1),
      std::bind(&CreatorFrame::VariableRenamed, &_creator_frame, std::placeholders::_1,
                std::placeholders::_2),
      true};

  _value_list = new wxListCtrl{left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS};
  _value_list->InsertColumn(0, "Value", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
  _value_list->SetToolTip(
      "Potential values that this variable can be set to "
      "when the session is launched.");

  _button_new = new wxButton{left_panel, ID_NEW, "New"};
  _button_edit = new wxButton{left_panel, ID_EDIT, "Edit"};
  _button_delete = new wxButton{left_panel, ID_DELETE, "Delete"};

  _button_new->SetToolTip("Create a new variable value.");
  _button_edit->SetToolTip("Edit the selected variable value.");
  _button_delete->SetToolTip("Delete the selected variable value.");

  left->Add(_value_list, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  left->Add(left_buttons, 0, wxEXPAND, 0);
  left_buttons->Add(_button_new, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
  left_buttons->Add(_button_edit, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
  left_buttons->Add(_button_delete, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
  left_panel->SetSizer(left);

  wxStaticText* label = nullptr;
  label = new wxStaticText{right_panel, wxID_ANY, "Default value:"};
  label->SetToolTip(DEFAULT_VALUE_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  _default_value = new wxChoice{right_panel, wxID_ANY};
  _default_value->SetToolTip(DEFAULT_VALUE_TOOLTIP);
  right->Add(_default_value, 0, wxALL | wxEXPAND, DEFAULT_BORDER);
  label = new wxStaticText{right_panel, wxID_ANY, "Description:"};
  label->SetToolTip(DESCRIPTION_TOOLTIP);
  right->Add(label, 0, wxALL, DEFAULT_BORDER);
  _description =
      new wxTextCtrl{right_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE};
  _description->SetToolTip(DESCRIPTION_TOOLTIP);
  right->Add(_description, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  right_panel->SetSizer(right);

  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(left_panel, right_panel);
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  _value_list->Bind(
      wxEVT_SIZE, [&](wxSizeEvent&) { _value_list->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER); },
      wxID_ANY);

  _value_list->Bind(
      wxEVT_LIST_ITEM_SELECTED,
      [&](wxListEvent& e) { _current_value = _value_list->GetItemText(e.GetIndex()); }, wxID_ANY);

  _value_list->Bind(wxEVT_LIST_END_LABEL_EDIT,
                    [&](wxListEvent& e) {
                      if (e.IsEditCancelled()) {
                        return;
                      }
                      e.Veto();
                      std::string old_name = _current_value;
                      std::string new_name = e.GetLabel();
                      if (new_name.empty()) {
                        return;
                      }
                      for (int i = 0; i < _value_list->GetItemCount(); ++i) {
                        if (_value_list->GetItemText(i) == new_name && i != e.GetIndex()) {
                          return;
                        }
                      }
                      auto& data = (*_session.mutable_variable_map())[_item_selected];
                      for (auto& value : *data.mutable_value()) {
                        if (value == old_name) {
                          value = new_name;
                        }
                      }
                      if (data.default_value() == old_name) {
                        data.set_default_value(new_name);
                      }
                      _current_value = new_name;
                      _creator_frame.VariableValueRenamed(_item_selected, old_name, new_name);
                      RefreshData();
                    },
                    wxID_ANY);

  left_panel->Bind(
      wxEVT_COMMAND_BUTTON_CLICKED,
      [&](wxCommandEvent&) {
        static const std::string new_name = "New value";
        auto name = new_name;
        int number = 0;
        auto& data = (*_session.mutable_variable_map())[_item_selected];
        while (std::find(data.value().begin(), data.value().end(), name) != data.value().end()) {
          name = new_name + " (" + std::to_string(number) + ")";
          ++number;
        }
        data.add_value(name);
        _current_value = name;
        RefreshOurData();
        _creator_frame.VariableValueCreated(_item_selected, name);
        _creator_frame.MakeDirty(true);
      },
      ID_NEW);

  left_panel->Bind(wxEVT_COMMAND_BUTTON_CLICKED,
                   [&](wxCommandEvent&) {
                     for (int i = 0; i < _value_list->GetItemCount(); ++i) {
                       if (_value_list->GetItemState(i, wxLIST_STATE_SELECTED)) {
                         _value_list->EditLabel(i);
                         return;
                       }
                     }
                   },
                   ID_EDIT);

  left_panel->Bind(
      wxEVT_COMMAND_BUTTON_CLICKED,
      [&](wxCommandEvent&) {
        auto& data = (*_session.mutable_variable_map())[_item_selected];
        int removed = -1;
        for (int i = 0; i < _value_list->GetItemCount(); ++i) {
          if (_value_list->GetItemState(i, wxLIST_STATE_SELECTED)) {
            removed = i;
            break;
          }
        }
        if (removed >= 0) {
          for (auto it = data.mutable_value()->begin(); it != data.mutable_value()->end();) {
            if (*it == _current_value) {
              it = data.mutable_value()->erase(it);
            } else {
              ++it;
            }
          }
          if (data.default_value() == _current_value) {
            data.set_default_value(data.value(0));
          }
          _creator_frame.VariableValueDeleted(_item_selected, _current_value);
          _current_value.clear();
        }
        RefreshOurData();
        if (removed >= 0) {
          auto index = std::min(_value_list->GetItemCount() - 1, removed);
          _current_value = _value_list->GetItemText(index);
          _value_list->SetItemState(index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        }
        _creator_frame.MakeDirty(true);
      },
      ID_DELETE);

  _default_value->Bind(wxEVT_CHOICE, [&](wxCommandEvent&) {
    auto it = _session.mutable_variable_map()->find(_item_selected);
    if (it == _session.mutable_variable_map()->end()) {
      return;
    }
    it->second.set_default_value(_default_value->GetString(_default_value->GetSelection()));
    _creator_frame.MakeDirty(true);
  });

  _description->Bind(wxEVT_TEXT, [&](wxCommandEvent&) {
    auto it = _session.mutable_variable_map()->find(_item_selected);
    if (it == _session.mutable_variable_map()->end()) {
      return;
    }
    it->second.set_description(_description->GetValue());
    _creator_frame.MakeDirty(true);
  });
}

VariablePage::~VariablePage()
{
}

void VariablePage::RefreshOurData()
{
  _default_value->Clear();
  _description->ChangeValue("");

  auto it = _session.variable_map().find(_item_selected);
  if (it != _session.variable_map().end()) {
    std::vector<std::string> values{it->second.value().begin(), it->second.value().end()};
    std::sort(values.begin(), values.end());
    if (_current_value.empty() && !values.empty()) {
      _current_value = values.front();
    }

    while (_value_list->GetItemCount() < values.size()) {
      _value_list->InsertItem(_value_list->GetItemCount(), "");
    }
    while (_value_list->GetItemCount() > it->second.value_size()) {
      _value_list->DeleteItem(_value_list->GetItemCount() - 1);
    }

    long i = 0;
    for (const auto& value : values) {
      _value_list->SetItemText(i, value);
      if (value == _current_value) {
        _value_list->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      }
      _default_value->Append(value);
      if (value == it->second.default_value()) {
        _default_value->SetSelection(i);
      }
      ++i;
    }
    _description->ChangeValue(it->second.description());

    _button_new->Enable(true);
    _button_edit->Enable(!values.empty());
    _button_delete->Enable(values.size() > 1);
    _default_value->Enable(!values.empty());
    _description->Enable(true);
  } else {
    _value_list->DeleteAllItems();
    _button_new->Enable(false);
    _button_edit->Enable(false);
    _button_delete->Enable(false);
    _default_value->Enable(false);
    _description->Enable(false);
  }
}

void VariablePage::RefreshData()
{
  _item_list->RefreshData();
  RefreshOurData();
}