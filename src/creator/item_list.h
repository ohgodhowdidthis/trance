#ifndef TRANCE_SRC_CREATOR_ITEM_LIST_H
#define TRANCE_SRC_CREATOR_ITEM_LIST_H
#include <algorithm>
#include <functional>

#pragma warning(push, 0)
#include <google/protobuf/map.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/window.h>
#pragma warning(pop)

template <typename T>
class ItemList : public wxPanel
{
public:
  using map_type = google::protobuf::Map<std::string, T>;

  ~ItemList() override
  {
  }
  ItemList(wxWindow* parent, map_type& data, const std::string& type_name,
           const std::function<void(const std::string&)>& on_change,
           const std::function<void(const std::string&)>& on_create,
           const std::function<void(const std::string&)>& on_delete,
           const std::function<void(const std::string&, const std::string&)>& on_rename,
           bool allow_empty = false)
  : wxPanel{parent, wxID_ANY}
  , _data{data}
  , _type_name{type_name}
  , _list{nullptr}
  , _on_change{on_change}
  , _on_create{on_create}
  , _on_delete{on_delete}
  , _on_rename{on_rename}
  , _allow_empty{allow_empty}
  {
    auto sizer = new wxBoxSizer{wxHORIZONTAL};
    auto right = new wxBoxSizer{wxVERTICAL};

    _list = new wxListCtrl{this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS};
    _list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, wxLIST_AUTOSIZE_USEHEADER);
    _list->SetToolTip("Available " + _type_name + "s.");

    _button_new = new wxButton{this, wxID_ANY, "New"};
    _button_rename = new wxButton{this, wxID_ANY, "Rename"};
    _button_duplicate = new wxButton{this, wxID_ANY, "Duplicate"};
    _button_delete = new wxButton{this, wxID_ANY, "Delete"};

    _button_new->SetToolTip("Create a new, blank " + _type_name + ".");
    _button_rename->SetToolTip("Rename the selected " + _type_name + ".");
    _button_duplicate->SetToolTip("Duplicate the selected " + _type_name + ".");
    _button_delete->SetToolTip("Delete the selected " + _type_name + ".");

    sizer->Add(_list, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
    sizer->Add(right, 0, wxEXPAND, 0);
    right->Add(_button_new, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(_button_rename, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(_button_duplicate, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(_button_delete, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    SetSizer(sizer);

    _list->Bind(wxEVT_SIZE,
                [&](wxSizeEvent&) { _list->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER); },
                wxID_ANY);

    _button_new->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
      static const std::string new_name = "New " + _type_name;
      auto name = new_name;
      int number = 0;
      while (_data.find(name) != _data.end()) {
        name = new_name + " (" + std::to_string(number) + ")";
        ++number;
      }
      _data[name] = {};
      SetSelection(name);
      RefreshData();
      _on_create(name);
    });

    _button_rename->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
      for (int i = 0; i < _list->GetItemCount(); ++i) {
        if (_list->GetItemText(i) == _selection) {
          _list->EditLabel(i);
        }
      }
    });

    _button_duplicate->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
      auto name = _selection + " copy";
      int number = 0;
      while (_data.find(name) != _data.end()) {
        name = _selection + " copy (" + std::to_string(number) + ")";
        ++number;
      }
      _data[name] = _data[_selection];
      SetSelection(name);
      RefreshData();
      _on_create(name);
    });

    _button_delete->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&) {
      if (wxMessageBox("Really delete " + _type_name + " '" + _selection + "'?", "",
                       wxICON_QUESTION | wxYES_NO, this) == wxYES) {
        auto name = _selection;
        _data.erase(name);
        RefreshData();
        _on_delete(name);
      }
    });

    Bind(wxEVT_LIST_ITEM_SELECTED,
         [&](wxListEvent& e) {
           std::string text = _list->GetItemText(e.GetIndex());
           SetSelection(text);
         },
         wxID_ANY);

    Bind(wxEVT_LIST_END_LABEL_EDIT,
         [&](wxListEvent& e) {
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
           _on_rename(old_name, new_name);
         },
         wxID_ANY);
  }

  void AddOnDoubleClick(const std::function<void(const std::string&)>& on_double_click)
  {
    Bind(wxEVT_LIST_ITEM_ACTIVATED,
         [this, on_double_click](wxListEvent&) { on_double_click(_selection); }, wxID_ANY);
  }

  void RefreshData()
  {
    std::vector<std::string> items;
    for (const auto& pair : _data) {
      items.push_back(pair.first);
    }
    std::sort(items.begin(), items.end());
    while (std::size_t(_list->GetItemCount()) < items.size()) {
      _list->InsertItem(_list->GetItemCount(), "");
    }
    while (std::size_t(_list->GetItemCount()) > items.size()) {
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
        _list->SetItemState((long) i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      }
    }
    _button_rename->Enable(!items.empty());
    _button_duplicate->Enable(!items.empty());
    _button_delete->Enable((_allow_empty && !items.empty()) || items.size() > 1);
  }

  void ClearHighlights()
  {
    for (std::size_t i = 0; i < std::size_t(_list->GetItemCount()); ++i) {
      _list->SetItemBackgroundColour((long) i, *wxWHITE);
    }
  }

  void AddHighlight(const std::string& item)
  {
    for (std::size_t i = 0; i < std::size_t(_list->GetItemCount()); ++i) {
      if (_list->GetItemText((long) i) == item) {
        _list->SetItemBackgroundColour((long) i, *wxLIGHT_GREY);
      }
    }
  }

private:
  void SetSelection(const std::string& selection)
  {
    _selection = selection;
    _on_change(selection);
  }

  map_type& _data;
  std::string _type_name;
  std::string _selection;
  std::function<void(const std::string&)> _on_change;
  std::function<void(const std::string&)> _on_create;
  std::function<void(const std::string&)> _on_delete;
  std::function<void(const std::string&, const std::string&)> _on_rename;
  bool _allow_empty;

  wxButton* _button_new;
  wxButton* _button_rename;
  wxButton* _button_duplicate;
  wxButton* _button_delete;
  wxListCtrl* _list;
};

#endif