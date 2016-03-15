#ifndef TRANCE_CREATOR_PROGRAM_H
#define TRANCE_CREATOR_PROGRAM_H

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>
#include <unordered_map>

namespace trance_pb {
  class Program;
  class Session;
}
class CreatorFrame;
template<typename T>
class ItemList;
class wxSpinCtrl;
class wxTreeListCtrl;
class wxTreeListItem;

class ProgramPage : public wxNotebookPage {
public:
  ProgramPage(wxNotebook* parent,
              CreatorFrame& creator_frame,
              trance_pb::Session& session);
  ~ProgramPage();
  void RefreshOurData();
  void RefreshData();
  void RefreshThemes();

private:
  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;
  std::string _item_selected;

  ItemList<trance_pb::Program>* _item_list;
  std::unordered_map<std::string, wxTreeListItem> _tree_lookup;
  std::unordered_map<unsigned int, wxSpinCtrl*> _visual_lookup;
  wxTreeListCtrl* _tree;
};

#endif
