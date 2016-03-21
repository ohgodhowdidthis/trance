#include "theme.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"
#include "../image.h"
#include "../session.h"

#pragma warning(push, 0)
#include <SFML/Graphics.hpp>
#include <src/trance.pb.h>
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/timer.h>
#include <wx/treelist.h>
#pragma warning(pop)

#include <algorithm>
#include <filesystem>
#include <thread>

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

  std::string StripUserNls(const std::string& nl) {
    std::string s = nl;
    std::string r;
    bool first = true;
    while (!s.empty()) {
      auto p = s.find_first_of("\\n");
      auto q = s.substr(0, p != std::string::npos ? p : s.size());
      r += (first ? "" : "/") + q;
      first = false;
      s = s.substr(p != std::string::npos ? 2 + p : s.size());
    }
    return r;
  }

  std::string UserToNl(const std::string& user) {
    std::string s = StripUserNls(user);
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
  , _dirty{true}
  , _bitmap{0, 0}
  {
    _timer = new wxTimer(this, wxID_ANY);
    _timer->Start(20);
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    Bind(wxEVT_TIMER, [&](const wxTimerEvent&)
    {
      _dirty = true;
      ++_frame;
      Refresh();
    }, wxID_ANY);

    Bind(wxEVT_SIZE, [&](const wxSizeEvent&)
    {
      _dirty = true;
      Refresh();
    });

    Bind(wxEVT_PAINT, [&](const wxPaintEvent&)
    {
      wxPaintDC dc{this};
      int width = 0;
      int height = 0;
      dc.GetSize(&width, &height);

      if (width && height && _dirty) {
        wxImage temp_image{width, height};
        if (!_images.empty()) {
          const auto& image = _images[_frame % _images.size()];
          auto iw = image->GetWidth();
          auto ih = image->GetHeight();
          auto scale = std::min(float(height) / ih, float(width) / iw);
          auto sw = unsigned(scale * iw);
          auto sh = unsigned(scale * ih);
          temp_image.Paste(image->Scale(sw, sh, wxIMAGE_QUALITY_HIGH),
                           (width - sw) / 2, (height - sh) / 2);
        }
        _bitmap = wxBitmap{temp_image};
      }
      dc.DrawBitmap(_bitmap, 0, 0, false);
    });
  }

  ~ImagePanel() {
    _timer->Stop();
  }

  void SetAnimation(const std::vector<Image>& images) {
    _images.clear();
    for (const auto& image : images) {
      auto ptr = image.get_sf_image();
      if (!ptr) {
        continue;
      }
      auto sf_image = *ptr;
      _images.emplace_back(std::make_unique<wxImage>(
          (int) sf_image.getSize().x, (int) sf_image.getSize().y));
      for (unsigned y = 0; y < sf_image.getSize().y; ++y) {
        for (unsigned x = 0; x < sf_image.getSize().x; ++x) {
          const auto& c = sf_image.getPixel(x, y);
          _images.back()->SetRGB(x, y, c.r, c.g, c.b);
        }
      }
    }
    _dirty = true;
    _frame = 0;
    Refresh();
  }

  void SetImage(const Image& image) {
    std::vector<Image> images = {image};
    SetAnimation(images);
  }

private:
  bool _dirty;
  std::size_t _frame;
  std::vector<std::shared_ptr<wxImage>> _images;
  wxBitmap _bitmap;
  wxTimer* _timer;
};

ThemePage::ThemePage(wxNotebook* parent,
                     CreatorFrame& creator_frame,
                     trance_pb::Session& session,
                     const std::string& session_path)
: wxNotebookPage{parent, wxID_ANY}
, _creator_frame{creator_frame}
, _session{session}
, _session_path{session_path}
, _complete_theme{new trance_pb::Theme}
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
  auto right =
      new wxStaticBoxSizer{wxVERTICAL, right_panel, "Text messages"};
  auto right_buttons = new wxBoxSizer{wxVERTICAL};

  auto left_splitter = new wxSplitterWindow{
      left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  left_splitter->SetSashGravity(0.5);
  left_splitter->SetMinimumPaneSize(128);

  auto leftleft_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftleft =
      new wxStaticBoxSizer{wxVERTICAL, leftleft_panel, "Included files"};
  auto leftright_panel = new wxPanel{left_splitter, wxID_ANY};
  auto leftright =
      new wxStaticBoxSizer{wxVERTICAL, leftright_panel, "Preview"};

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
  _tree->GetView()->SetToolTip("Images, animations and fonts that are part "
                               "of the currently-selected theme.");
  _tree->AppendColumn("");

  _image_panel = new ImagePanel{leftright_panel};
  _image_panel->SetToolTip("Preview of the selected item.");

  _text_list = new wxListCtrl{
      right_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxLC_EDIT_LABELS};
  _text_list->InsertColumn(0, "Text", wxLIST_FORMAT_LEFT,
                           wxLIST_AUTOSIZE_USEHEADER);
  _text_list->SetToolTip("Text messages that are part "
                         "of the currently-selected theme.");

  _button_new = new wxButton{right_panel, ID_NEW, "New"};
  _button_edit = new wxButton{right_panel, ID_EDIT, "Edit"};
  _button_delete = new wxButton{right_panel, ID_DELETE, "Delete"};
  _button_refresh =
      new wxButton{leftleft_panel, ID_REFRESH, "Refresh directory"};

  _button_new->SetToolTip("Create a new text item.");
  _button_edit->SetToolTip("Edit the selected text item.");
  _button_delete->SetToolTip("Delete the selected text item.");
  _button_refresh->SetToolTip(
      "Scan the session directory for available images, animations and fonts.");

  leftleft->Add(_tree, 1, wxEXPAND | wxALL, DEFAULT_BORDER);
  leftleft->Add(_button_refresh, 0, wxEXPAND | wxALL, DEFAULT_BORDER);
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

  _text_list->Bind(wxEVT_LIST_ITEM_SELECTED, [&](wxListEvent& e)
  {
    std::string user = _text_list->GetItemText(e.GetIndex());
    _current_text_line = UserToNl(user);
    GenerateFontPreview();
  }, wxID_ANY);

  right_panel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
  {
    (*_session.mutable_theme_map())[_item_selected].add_text_line("NEW TEXT");
    RefreshOurData();
    _text_list->SetItemState(_text_list->GetItemCount() - 1,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    _creator_frame.MakeDirty(true);
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
    RefreshOurData();
    if (removed >= 0 && _text_list->GetItemCount()) {
      _text_list->SetItemState(
          std::min(_text_list->GetItemCount() - 1, removed),
          wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
    _creator_frame.MakeDirty(true);
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
    RefreshOurData();
    _current_text_line = new_text;
    GenerateFontPreview();
    _creator_frame.MakeDirty(true);
  }, wxID_ANY);

  leftleft_panel->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent&)
  {
    _creator_frame.RefreshDirectory();
    RefreshOurData();
  }, ID_REFRESH);

  _tree->Bind(wxEVT_TREELIST_SELECTION_CHANGED, [&](wxTreeListEvent& e)
  {
    auto data = _tree->GetItemData(e.GetItem());
    if (data != nullptr) {
      std::string path = ((const wxStringClientData*) data)->GetData();
      auto root = std::tr2::sys::path{_session_path}.parent_path().string();
      const auto& images = _complete_theme->image_path();
      const auto& anims = _complete_theme->animation_path();
      const auto& fonts = _complete_theme->font_path();
      if (std::find(images.begin(), images.end(), path) != images.end()) {
        _image_panel->SetImage(load_image(root + "/" + path));
      }
      if (std::find(anims.begin(), anims.end(), path) != anims.end()) {
        _image_panel->SetAnimation(load_animation(root + "/" + path));
      }
      if (std::find(fonts.begin(), fonts.end(), path) != fonts.end()) {
        _current_font = root + "/" + path;
        GenerateFontPreview();
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
            handle(_complete_theme->image_path(),
                    *it->second.mutable_image_path(), path);
            handle(_complete_theme->animation_path(),
                    *it->second.mutable_animation_path(), path);
            handle(_complete_theme->font_path(),
                    *it->second.mutable_font_path(), path);
          }
        };
    recurse(e.GetItem());
    _creator_frame.MakeDirty(true);
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

  int selected_text = 0;
  for (int i = 0; i < _text_list->GetItemCount(); ++i) {
    if (_text_list->GetItemState(i, wxLIST_STATE_SELECTED)) {
      selected_text = i;
      break;
    }
  }

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
  if (!_item_selected.empty() && it->second.text_line_size()) {
    _text_list->SetItemState(
        std::min(selected_text, it->second.text_line_size() - 1),
        wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
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

void ThemePage::RefreshDirectory(const std::string& directory)
{
  *_complete_theme = trance_pb::Theme{};
  search_resources(*_complete_theme, directory);

  _tree->DeleteAllItems();
  _tree_lookup.clear();
  _tree_lookup["."] = _tree->GetRootItem();

  std::vector<std::string> paths;
  for (const auto& path : _complete_theme->image_path()) {
    paths.push_back(path);
  }
  for (const auto& path : _complete_theme->animation_path()) {
    paths.push_back(path);
  }
  for (const auto& path : _complete_theme->font_path()) {
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

  const auto& c = *_complete_theme;
  std::set<std::string> image_set{c.image_path().begin(),
                                  c.image_path().end()};
  std::set<std::string> animation_set{c.animation_path().begin(),
                                      c.animation_path().end()};
  std::set<std::string> font_set{c.font_path().begin(),
                                 c.font_path().end()};
  for (auto& pair : *_session.mutable_theme_map()) {
    for (auto it = pair.second.mutable_image_path()->begin();
         it != pair.second.mutable_image_path()->end();) {
      it = image_set.count(*it) ? 1 + it :
          pair.second.mutable_image_path()->erase(it);
    }
    for (auto it = pair.second.mutable_animation_path()->begin();
         it != pair.second.mutable_animation_path()->end();) {
      it = animation_set.count(*it) ? 1 + it :
          pair.second.mutable_animation_path()->erase(it);
    }
    for (auto it = pair.second.mutable_font_path()->begin();
         it != pair.second.mutable_font_path()->end();) {
      it = font_set.count(*it) ? 1 + it :
          pair.second.mutable_font_path()->erase(it);
    }
  }
}

void ThemePage::GenerateFontPreview()
{
  if (_current_font.empty()) {
    return;
  }
  std::string text;
  if (_current_text_line.empty()) {
    text = (--std::tr2::sys::path{_current_font}.end())->string();
  } else {
    text = _current_text_line;
  }

  static const unsigned border = 32;
  static const unsigned font_size = 128;
  // Render in another thread to avoid interfering with OpenGL contexts.
  std::thread worker([&]
  {
    sf::Font font;
    font.loadFromFile(_current_font);
    sf::Text text_obj{text, font, font_size};
    auto bounds = text_obj.getLocalBounds();

    sf::RenderTexture texture;
    texture.create(border + (unsigned) bounds.width,
                   border + (unsigned) bounds.height);
    sf::Transform transform;
    transform.translate(border / 2 - bounds.left, border / 2 - bounds.top);
    texture.draw(text_obj, transform);
    texture.display();
    _image_panel->SetImage(texture.getTexture().copyToImage());
  });
  worker.join();
}