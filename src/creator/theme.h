#ifndef TRANCE_CREATOR_THEME_H
#define TRANCE_CREATOR_THEME_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>

template<typename T>
class ItemList;

class ThemePage : public wxNotebookPage {
public:
  ThemePage(wxNotebook* parent, trance_pb::Session& session);
  ~ThemePage();
  void RefreshData();

private:
  trance_pb::Session& _session;
  std::string _item_selected;
  std::unique_ptr<ItemList<trance_pb::Theme>> _item_list;
};

#endif
