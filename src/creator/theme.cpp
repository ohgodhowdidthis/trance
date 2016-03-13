#include "theme.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"
#include "../image.h"

#pragma warning(push, 0)
#include <wx/dcclient.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#pragma warning(pop)

#include <algorithm>
#include <filesystem>

namespace {
  std::string NlToUser(const std::string& nl) {
    std::string s = nl;
    std::string r;
    bool first = true;
    while (!s.empty()) {
      auto p = s.find_first_of('\n');
      auto q = s.substr(0, p != std::string::npos ? p : s.size());
      r += (first ? "" : "  /  ") + q;
      first = false;
      s = s.substr(p != std::string::npos ? 1 + p : s.size());
    }
    return r;
  }

  std::string UserToNl(const std::string& user) {
    std::string s = NlToUser(user);
    std::string r;
    bool first = true;
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty()) {
      auto p = s.find_first_of('/');
      auto q = s.substr(0, p != std::string::npos ? p : s.size());
      while (!q.empty() && q[0] == ' ') {
        q = q.substr(1);
      }
      while (!q.empty() && q[q.size() - 1] == ' ') {
        q = q.substr(0, q.size() - 1);
      }
      if (!q.empty()) {
        r += (first ? "" : "\n") + q;
        first = false;
      }
      s = s.substr(p != std::string::npos ? 1 + p : s.size());
      p = s.find_first_of('/');
    }
    return r;
  }
}

class ImagePanel : public wxPanel {
public:
  ImagePanel(wxWindow* parent)
  : wxPanel{parent, wxID_ANY}
  , _dirty{false}
  , _image{0, 0}
  , _bitmap{0, 0}
  {
    Bind(wxEVT_SIZE, [&](const wxSizeEvent&)
    {
      Refresh();
    });

    Bind(wxEVT_PAINT, [&](const wxPaintEvent&)
    {
      if (!_image.IsOk()) {
        return;
      }
      wxPaintDC dc{this};
      int width = 0;
      int height = 0;
      dc.GetSize(&width, &height);

      auto iw = _image.GetWidth();
      auto ih = _image.GetHeight();
      auto scale = std::min(float(height) / ih, float(width) / iw);
      auto sw = unsigned(scale * iw);
      auto sh = unsigned(scale * ih);
      auto dirty =
          _dirty || _bitmap.GetWidth() != sw || _bitmap.GetHeight() != sh;
      if (iw && ih && dirty) {
        _bitmap = wxBitmap{_image.Scale(sw, sh, wxIMAGE_QUALITY_HIGH)};
      }
      dc.DrawBitmap(_bitmap, (width - sw) / 2, (height - sh) / 2, false);
    });
  }

  void SetImage(const sf::Image& image) {
    _image = wxImage{(int) image.getSize().x, (int) image.getSize().y};
    for (unsigned y = 0; y < image.getSize().y; ++y) {
      for (unsigned x = 0; x < image.getSize().x; ++x) {
        const auto& c = image.getPixel(x, y);
        _image.SetRGB(x, y, c.r, c.g, c.b);
      }
    }
    _dirty = true;
    Refresh();
  }

private:
  bool _dirty;
  wxImage _image;
  wxBitmap _bitmap;
};

ThemePage::ThemePage(wxNotebook* parent,
                     CreatorFrame& creator_frame,
                     trance_pb::Session& session,
                     const trance_pb::Theme& complete_theme,
                     const std::string& session_path)
: wxNotebookPage{parent, wxID_ANY}
, _creator_frame{creator_frame}
, _session{session}
, _complete_theme{complete_theme}
, _session_path{session_path}
, _tree{nullptr}
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
  bottom_splitter->SetSashGravity(0.75);
  bottom_splitter->SetMinimumPaneSize(128);

  auto left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxBoxSizer{wxHORIZONTAL};
  auto right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto right = new wxBoxSizer{wxHORIZONTAL};
  auto right_buttons = new wxBoxSizer{wxVERTICAL};

  auto left_splitter = new wxSplitterWindow{
      left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  left_splitter->SetSashGravity(0.5);
  left_splitter->SetMinimumPaneSize(128);

  auto leftleft_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftleft = new wxBoxSizer{wxHORIZONTAL};
  auto leftright_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftright = new wxBoxSizer{wxHORIZONTAL};

  _item_list = new ItemList<trance_pb::Theme>{
      splitter, *session.mutable_theme_map(), "theme",
      [&](const std::string& s) { _item_selected = s; RefreshOurData(); },
      std::bind(&CreatorFrame::ThemeCreated, &_creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::ThemeDeleted, &_creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::ThemeRenamed, &_creator_frame,
                std::placeholders::_1, std::placeholders::_2)};

  _tree = new wxTreeListCtrl{
      leftleft_panel, 0, wxDefaultPosition, wxDefaultSize,
      wxTL_SINGLE | wxTL_CHECKBOX | wxTL_3STATE | wxTL_NO_HEADER};
  _tree->AppendColumn("");

  _image_panel = new ImagePanel{leftright_panel};
  _image_panel->SetToolTip("Preview of the selected image or animation");

  _text_list = new wxListCtrl{
      right_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS};
  _text_list->InsertColumn(0, "Text", wxLIST_FORMAT_LEFT,
                           wxLIST_AUTOSIZE_USEHEADER);

  _button_new = new wxButton{right_panel, ID_NEW, "New"};
  _button_edit = new wxButton{right_panel, ID_EDIT, "Edit"};
  _button_delete = new wxButton{right_panel, ID_DELETE, "Delete"};

  _button_new->SetToolTip("Create a new text item");
  _button_edit->SetToolTip("Edit the selected text item");
  _button_delete->SetToolTip("Delete the selected text item");

  leftleft->Add(_tree, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  leftleft_panel->SetSizer(leftleft);
  leftright->Add(_image_panel, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  leftright_panel->SetSizer(leftright);
  left->Add(left_splitter, 1, wxEXPAND, 0);
  left_splitter->SplitVertically(leftleft_panel, leftright_panel);
  left_panel->SetSizer(left);
  right->Add(_text_list, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  right->Add(right_buttons, 0, wxEXPAND, 0);
  right_buttons->Add(_button_new, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
  right_buttons->Add(_button_edit, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
  right_buttons->Add(_button_delete, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
  right_panel->SetSizer(right);
  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(left_panel, right_panel);
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  _text_list->Bind(wxEVT_SIZE, [&](wxSizeEvent&)
  {
    _text_list->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);
  }, wxID_ANY);

  right_panel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
  {
    (*_session.mutable_theme_map())[_item_selected].add_text_line("NEW TEXT");
    RefreshData();
    _text_list->SetItemState(_text_list->GetItemCount() - 1,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
  }, ID_NEW);

  right_panel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
  {
    for (int i = 0; i < _text_list->GetItemCount(); ++i) {
      if (_text_list->GetItemState(i, wxLIST_STATE_SELECTED)) {
        _text_list->EditLabel(i);
        return;
      }
    }
  }, ID_EDIT);

  right_panel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
  {
    int removed = -1;
    for (int i = 0; i < _text_list->GetItemCount(); ++i) {
      if (_text_list->GetItemState(i, wxLIST_STATE_SELECTED)) {
        auto& theme = (*_session.mutable_theme_map())[_item_selected];
        theme.mutable_text_line()->erase(
            i + theme.mutable_text_line()->begin());
        removed = i;
        break;
      }
    }
    RefreshData();
    if (removed >= 0 && _text_list->GetItemCount()) {
      _text_list->SetItemState(
          std::min(_text_list->GetItemCount() - 1, removed),
          wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
  }, ID_DELETE);

  right_panel->Bind(wxEVT_LIST_END_LABEL_EDIT, [&](wxListEvent& e)
  {
    if (e.IsEditCancelled()) {
      return;
    }
    e.Veto();
    std::string new_text = e.GetLabel();
    new_text = UserToNl(new_text);
    if (new_text.empty()) {
      return;
    }
    for (int i = 0; i < _text_list->GetItemCount(); ++i) {
      if (_text_list->GetItemState(i, wxLIST_STATE_SELECTED)) {
        *(*_session.mutable_theme_map())[_item_selected]
            .mutable_text_line()->Mutable(i) = new_text;
      }
    }
    RefreshData();
  }, wxID_ANY);

  _tree->Bind(wxEVT_TREELIST_SELECTION_CHANGED, [&](wxTreeListEvent& e)
  {
    auto data = _tree->GetItemData(e.GetItem());
    if (data != nullptr) {
      std::string path = ((const wxStringClientData*) data)->GetData();
      auto root = std::tr2::sys::path{_session_path}.parent_path().string();
      const auto& images = _complete_theme.image_path();
      const auto& anims = _complete_theme.animation_path();
      if (std::find(images.begin(), images.end(), path) != images.end()) {
        auto image = load_image(root + "/" + path).get_sf_image();
        if (image) {
          _image_panel->SetImage(*image);
        }
      }
      if (std::find(anims.begin(), anims.end(), path) != anims.end()) {
        auto images = load_animation(root + "/" + path);
        if (!images.empty() && images[0].get_sf_image()) {
          _image_panel->SetImage(*images[0].get_sf_image());
        }
      }
    }
  }, wxID_ANY);

  _tree->Bind(wxEVT_TREELIST_ITEM_CHECKED, [&](wxTreeListEvent& e)
  {
    auto it = _session.mutable_theme_map()->find(_item_selected);
    if (it == _session.mutable_theme_map()->end()) {
      e.Veto();
      return;
    }
    auto checked = _tree->GetCheckedState(e.GetItem());
    _tree->CheckItemRecursively(e.GetItem(), checked);
    _tree->UpdateItemParentStateRecursively(e.GetItem());

    auto handle = [&](const google::protobuf::RepeatedPtrField<std::string>& c,
                      google::protobuf::RepeatedPtrField<std::string>& t,
                      const std::string& path)
    {
      if (std::find(c.begin(), c.end(), path) == c.end()) {
        return;
      }
      auto it = std::find(t.begin(), t.end(), path);
      if (checked == wxCHK_CHECKED && it == t.end()) {
        *t.Add() = path;
      }
      if (checked == wxCHK_UNCHECKED && it != t.end()) {
        t.erase(it);
      }
    };

    std::function<void(const wxTreeListItem&)> recurse =
        [&](const wxTreeListItem& item) {
          for (auto c = _tree->GetFirstChild(item); c.IsOk();
               c = _tree->GetNextSibling(c)) {
            recurse(c);
          }
          auto data = _tree->GetItemData(item);
          if (data != nullptr) {
            std::string path = ((const wxStringClientData*) data)->GetData();
            handle(_complete_theme.image_path(),
                    *it->second.mutable_image_path(), path);
            handle(_complete_theme.animation_path(),
                    *it->second.mutable_animation_path(), path);
            handle(_complete_theme.font_path(),
                    *it->second.mutable_font_path(), path);
          }
        };
    recurse(e.GetItem());
  }, wxID_ANY);
}

ThemePage::~ThemePage()
{
}

void ThemePage::RefreshOurData()
{
  for (auto item = _tree->GetFirstItem(); item.IsOk();
       item = _tree->GetNextItem(item)) {
    _tree->CheckItem(item, wxCHK_UNCHECKED);
  }

  auto select = [&](const std::string& s) {
    auto it = _tree_lookup.find(s);
    if (it != _tree_lookup.end()) {
      _tree->CheckItem(it->second);
      _tree->UpdateItemParentStateRecursively(it->second);
    }
  };

  auto it = _session.theme_map().find(_item_selected);
  if (it != _session.theme_map().end()) {
    for (const auto& path : it->second.image_path()) {
      select(path);
    }
    for (const auto& path : it->second.animation_path()) {
      select(path);
    }
    for (const auto& path : it->second.font_path()) {
      select(path);
    }

    while (_text_list->GetItemCount() < it->second.text_line_size()) {
      _text_list->InsertItem(_text_list->GetItemCount(), "");
    }
    while (_text_list->GetItemCount() > it->second.text_line_size()) {
      _text_list->DeleteItem(_text_list->GetItemCount() - 1);
    }
    for (int i = 0; i < it->second.text_line_size(); ++i) {
      _text_list->SetItemText((long) i, NlToUser(it->second.text_line(i)));
    }
  } else {
    _text_list->DeleteAllItems();
  }
  _button_new->Enable(!_item_selected.empty());
  _button_edit->Enable(!_item_selected.empty() && it->second.text_line_size());
  _button_delete->Enable(
      !_item_selected.empty() && it->second.text_line_size());
}

void ThemePage::RefreshData()
{
  _item_list->RefreshData();
  RefreshOurData();
}

void ThemePage::RefreshRoot()
{
  _tree->DeleteAllItems();
  _tree_lookup.clear();
  _tree_lookup["."] = _tree->GetRootItem();

  std::vector<std::string> paths;
  for (const auto& path : _complete_theme.image_path()) {
    paths.push_back(path);
  }
  for (const auto& path : _complete_theme.animation_path()) {
    paths.push_back(path);
  }
  for (const auto& path : _complete_theme.font_path()) {
    paths.push_back(path);
  }
  std::sort(paths.begin(), paths.end());

  for (const auto& path_str : paths) {
    std::tr2::sys::path path{path_str};
    for (auto it = ++path.begin(); it != path.end(); ++it) {
      std::tr2::sys::path component;
      std::tr2::sys::path parent;
      for (auto jt = path.begin(); jt != it; ++jt) {
        component.append(*jt);
        parent.append(*jt);
      }
      component.append(*it);
      if (_tree_lookup.find(component.string()) == _tree_lookup.end()) {
        wxClientData* data = nullptr;
        if (it == --path.end()) {
          data = new wxStringClientData{component.string()};
        }
        auto item = _tree->AppendItem(
            _tree_lookup[parent.string()], it->string(), -1, -1, data);
        _tree_lookup[component.string()] = item;
      }
    }
  }
}