#ifndef TRANCE_SRC_CREATOR_THEME_H
#define TRANCE_SRC_CREATOR_THEME_H
#include <memory>
#include <unordered_map>

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

namespace trance_pb
{
  class Session;
  class Theme;
}
class CreatorFrame;
template <typename T>
class ItemList;
class ImagePanel;
class wxButton;
class wxListCtrl;
class wxTreeListCtrl;
class wxTreeListItem;

class ThemePage : public wxNotebookPage
{
public:
  ThemePage(wxNotebook* parent, CreatorFrame& creator_frame, trance_pb::Session& session,
            const std::string& session_path);
  ~ThemePage();
  void RefreshOurData();
  void RefreshData();
  void RefreshDirectory(const std::string& directory);
  void Shutdown();

private:
  void RefreshTree(wxTreeListItem item);
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
  wxButton* _button_next_unused;
  wxButton* _button_next_theme;
  wxListCtrl* _text_list;
};

#endif
