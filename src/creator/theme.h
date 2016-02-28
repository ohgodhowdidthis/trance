#ifndef TRANCE_CREATOR_THEME_H
#define TRANCE_CREATOR_THEME_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/treelist.h>
#pragma warning(pop)

#include <memory>
#include <unordered_map>
#include <vector>

template<typename T>
class ItemList;
class ImagePanel;
namespace sf {
  class Image;
}

class ThemePage : public wxNotebookPage {
public:
  ThemePage(wxNotebook* parent, trance_pb::Session& session,
            const trance_pb::Theme& complete_theme,
            const std::string& session_path);
  ~ThemePage();
  void RefreshOurData();
  void RefreshData();
  void RefreshRoot();

private:
  trance_pb::Session& _session;
  const trance_pb::Theme& _complete_theme;
  const std::string& _session_path;
  std::string _item_selected;

  ItemList<trance_pb::Theme>* _item_list;
  std::unordered_map<std::string, wxTreeListItem> _tree_lookup;
  wxTreeListCtrl* _tree;
  ImagePanel* _image_panel;
};

#endif
