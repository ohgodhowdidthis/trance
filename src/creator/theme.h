#ifndef TRANCE_CREATOR_THEME_H
#define TRANCE_CREATOR_THEME_H

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>
#include <unordered_map>

namespace trance_pb {
  class Session;
  class Theme;
}
class CreatorFrame;
template<typename T>
class ItemList;
class ImagePanel;
class wxButton;
class wxListCtrl;
class wxTreeListCtrl;
class wxTreeListItem;

class ThemePage : public wxNotebookPage {
public:
  ThemePage(wxNotebook* parent,
            CreatorFrame& creator_frame,
            trance_pb::Session& session,
            const std::string& session_path);
  ~ThemePage();
  void RefreshOurData();
  void RefreshData();
  void RefreshDirectory(const std::string& directory);

private:
  enum {
    ID_NEW = 10320,
    ID_EDIT = 10321,
    ID_DELETE = 10322,
    ID_OPEN = 10323,
    ID_RENAME = 10324,
    ID_REFRESH = 10325,
  };

  void RefreshHighlights();
  void GenerateFontPreview();

  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;
  const std::string& _session_path;
  std::string _item_selected;
  std::string _path_selected;
  std::unique_ptr<trance_pb::Theme> _complete_theme;

  std::string _current_font;
  std::string _current_text_line;

  ItemList<trance_pb::Theme>* _item_list;
  std::unordered_map<std::string, wxTreeListItem> _tree_lookup;
  wxTreeListCtrl* _tree;
  ImagePanel* _image_panel;
  wxButton* _button_new;
  wxButton* _button_edit;
  wxButton* _button_delete;
  wxButton* _button_open;
  wxButton* _button_rename;
  wxButton* _button_refresh;
  wxListCtrl* _text_list;
};

#endif
