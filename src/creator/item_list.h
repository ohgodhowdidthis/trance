#ifndef TRANCE_CREATOR_ITEM_LIST_H
#define TRANCE_CREATOR_ITEM_LIST_H

#pragma warning(push, 0)
#include <google/protobuf/map.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/window.h>
#pragma warning(pop)

#include <algorithm>
#include <functional>

template<typename T>
class ItemList : public wxPanel {
public:
  using map_type = google::protobuf::Map<std::string, T>;

  ~ItemList() override {}
  ItemList(wxWindow* parent, map_type& data,
           std::function<void(const std::string&)> on_change)
  : wxPanel{parent, wxID_ANY}
  , _data{data}
  , _list{nullptr}
  , _on_change{on_change}
  {
    auto sizer = new wxBoxSizer{wxHORIZONTAL};
    auto right = new wxBoxSizer{wxVERTICAL};

    _list = new wxListCtrl{
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS};
    _list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT,
                        wxLIST_AUTOSIZE_USEHEADER);

    _button_new = new wxButton{this, ID_NEW, "New"};
    _button_rename = new wxButton{this, ID_RENAME, "Rename"};
    _button_duplicate = new wxButton{this, ID_DUPLICATE, "Duplicate"};
    _button_delete = new wxButton{this, ID_DELETE, "Delete"};

    sizer->Add(_list, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
    sizer->Add(right, 0, wxEXPAND, 0);
    right->Add(_button_new, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(_button_rename, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(_button_duplicate, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(_button_delete, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    SetSizer(sizer);

    _list->Bind(wxEVT_SIZE, [&](wxSizeEvent&)
    {
      _list->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
    }, wxID_ANY);

    Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
    {
      static const std::string new_name = "New session";
      auto name = new_name;
      int number = 0;
      while (_data.find(name) != _data.end()) {
        name = new_name + " (" + std::to_string(number) + ")";
        ++number;
      }
      _data[name] = {};
      SetSelection(name);
      RefreshData();
    }, ID_NEW);

    Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
    {
      for (int i = 0; i < _list->GetItemCount(); ++i) {
        if (_list->GetItemText(i) == _selection) {
          _list->EditLabel(i);
        }
      }
    }, ID_RENAME);

    Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
    {
      auto name = _selection + " copy";
      int number = 0;
      while (_data.find(name) != _data.end()) {
        name = _selection + " copy (" + std::to_string(number) + ")";
        ++number;
      }
      _data[name] = _data[_selection];
      SetSelection(name);
      RefreshData();
    }, ID_DUPLICATE);

    Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
    {
      if (wxMessageBox(
        "Really delete session '" + _selection + "'?", "",
        wxICON_QUESTION | wxYES_NO, this) == wxYES) {
        _data.erase(_selection);
        RefreshData();
      }
    }, ID_DELETE);

    Bind(wxEVT_LIST_ITEM_SELECTED, [&](wxListEvent& e)
    {
      std::string text = _list->GetItemText(e.GetIndex());
      SetSelection(text);
    }, wxID_ANY);

    Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent& e)
    {
      if (e.IsEditCancelled()) {
        return;
      }
      e.Veto();
      std::string old_name = _selection;
      std::string new_name = e.GetLabel();
      if (new_name.empty()) {
        return;
      }
      for (int i = 0; i < _list->GetItemCount(); ++i) {
        if (_list->GetItemText(i) == new_name && i != e.GetIndex()) {
          return;
        }
      }
      _data[new_name] = _data[old_name];
      _data.erase(old_name);
      SetSelection(new_name);
      RefreshData();
    }, wxID_ANY);
  }

  void RefreshData()
  {
    std::vector<std::string> items;
    for (const auto& pair : _data) {
      items.push_back(pair.first);
    }
    std::sort(items.begin(), items.end());
    while (_list->GetItemCount() < items.size()) {
      _list->InsertItem(_list->GetItemCount(), "");
    }
    while (_list->GetItemCount() > items.size()) {
      _list->DeleteItem(_list->GetItemCount() - 1);
    }
    if (std::find(items.begin(), items.end(), _selection) == items.end()) {
      if (items.empty()) {
        SetSelection("");
      } else {
        std::size_t i = 0;
        while (1 + i < items.size() && _selection > items[i]) {
          ++i;
        }
        SetSelection(items[i]);
      }
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
      _list->SetItemText((long) i, items[i]);
      if (items[i] == _selection) {
        _list->SetItemState(
            (long) i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      }
    }
    _button_rename->Enable(!items.empty());
    _button_duplicate->Enable(!items.empty());
    _button_delete->Enable(!items.empty());
  }

private:
  void SetSelection(const std::string& selection) {
    _selection = selection;
    _on_change(selection);
  }

  enum {
    ID_NEW = 10,
    ID_RENAME = 11,
    ID_DUPLICATE = 12,
    ID_DELETE = 13,
  };
  map_type& _data;
  std::string _selection;
  std::function<void(const std::string&)> _on_change;

  wxButton* _button_new;
  wxButton* _button_rename;
  wxButton* _button_duplicate;
  wxButton* _button_delete;
  wxListCtrl* _list;
};

#endif