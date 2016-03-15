#ifndef TRANCE_CREATOR_PROGRAM_H
#define TRANCE_CREATOR_PROGRAM_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/treelist.h>
#pragma warning(pop)

#include <memory>

class CreatorFrame;
template<typename T>
class ItemList;
class wxSpinCtrl;

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
  std::unordered_map<trance_pb::Program::VisualType,
                     wxSpinCtrl*> _visual_lookup;
  wxTreeListCtrl* _tree;
};

#endif
