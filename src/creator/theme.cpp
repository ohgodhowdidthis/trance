#include "theme.h"
#include "item_list.h"
#include "../common.h"
#include "../image.h"

#pragma warning(push, 0)
#include <wx/dcclient.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#pragma warning(pop)

#include <filesystem>

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

ThemePage::ThemePage(wxNotebook* parent, trance_pb::Session& session,
                     const trance_pb::Theme& complete_theme,
                     const std::string& session_path)
: wxNotebookPage{parent, wxID_ANY}
, _session{session}
, _complete_theme{complete_theme}
, _session_path{session_path}
, _tree{nullptr}
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto splitter = new wxSplitterWindow{
      this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  splitter->SetSashGravity(0.5);
  splitter->SetMinimumPaneSize(64);

  auto bottom_panel = new wxPanel{splitter, wxID_ANY};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  auto bottom_splitter = new wxSplitterWindow{
      bottom_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  bottom_splitter->SetSashGravity(0.5);
  bottom_splitter->SetMinimumPaneSize(64);

  auto left_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto left = new wxBoxSizer{wxHORIZONTAL};
  auto right_panel = new wxPanel{bottom_splitter, wxID_ANY};
  auto right = new wxBoxSizer{wxHORIZONTAL};

  _item_list = new ItemList<trance_pb::Theme>{
      splitter, *session.mutable_theme_map(),
      [&](const std::string& s) { _item_selected = s; RefreshOurData(); }};

  _tree = new wxTreeListCtrl{
      left_panel, 0, wxDefaultPosition, wxDefaultSize,
      wxTL_SINGLE | wxTL_CHECKBOX | wxTL_3STATE | wxTL_NO_HEADER};

  _image_panel = new ImagePanel{right_panel};

  _tree->AppendColumn("");
  left->Add(_tree, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  left_panel->SetSizer(left);
  right->Add(_image_panel, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  right_panel->SetSizer(right);
  bottom_panel->SetSizer(bottom);
  bottom->Add(bottom_splitter, 1, wxEXPAND, 0);
  bottom_splitter->SplitVertically(left_panel, right_panel);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);

  Bind(wxEVT_TREELIST_SELECTION_CHANGED, [&](wxTreeListEvent& e)
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

  Bind(wxEVT_TREELIST_ITEM_CHECKED, [&](wxTreeListEvent& e)
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
  }
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