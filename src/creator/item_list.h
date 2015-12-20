#ifndef TRANCE_CREATOR_ITEM_LIST_H
#define TRANCE_CREATOR_ITEM_LIST_H

#pragma warning(push, 0)
#include <google/protobuf/map.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/window.h>
#pragma warning(pop)

#include <algorithm>

template<typename T>
class ItemList {
public:
  using map_type = google::protobuf::Map<std::string, T>;

  ItemList(wxWindow* parent, wxSizer* parent_sizer, map_type& data)
  : _data{data}
  , _parent{parent}
  , _list{nullptr}
  {
    auto sizer = new wxBoxSizer{wxHORIZONTAL};
    auto right = new wxBoxSizer{wxVERTICAL};

    _list = new wxListCtrl{parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL};
    _list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT,
                        wxLIST_AUTOSIZE_USEHEADER);

    auto button_new = new wxButton{parent, ID_NEW, "New"};
    auto button_duplicate = new wxButton{parent, ID_DUPLICATE, "Duplicate"};
    auto button_delete = new wxButton{parent, ID_DELETE, "Delete"};

    parent_sizer->Add(sizer, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
    sizer->Add(_list, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
    sizer->Add(right, 0, wxEXPAND, 0);
    right->Add(button_new, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(button_duplicate, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
    right->Add(button_delete, 0, wxEXPAND | wxALL, DEFAULT_BORDER);

    _list->Bind(wxEVT_SIZE, [&](wxSizeEvent& event)
    {
      _list->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
      _list->SetColumnWidth(0, _list->GetColumnWidth(0) - 1);
    }, wxID_ANY);

    parent->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
    {
      static const std::string new_name = "new";
      auto name = new_name;
      int number = 0;
      while (_data.find(name) != _data.end()) {
        name = new_name + std::to_string(number);
        ++number;
      }
      _data[name] = {};
      RefreshData();
    }, ID_NEW);
  }

  void RefreshData()
  {
    _list->DeleteAllItems();
    std::vector<std::string> items;
    for (const auto& pair : _data) {
      items.push_back(pair.first);
    }
    std::sort(items.begin(), items.end());
    for (std::size_t i = 0; i < items.size(); ++i) {
      _list->InsertItem((long) i, items[i]);
    }
  }

private:
  enum {
    ID_NEW = 10,
    ID_DUPLICATE = 11,
    ID_DELETE = 12,
  };
  map_type& _data;

  wxWindow* _parent;
  wxListCtrl* _list;
};

#endif