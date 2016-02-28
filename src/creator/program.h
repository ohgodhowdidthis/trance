#ifndef TRANCE_CREATOR_PROGRAM_H
#define TRANCE_CREATOR_PROGRAM_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>

template<typename T>
class ItemList;

class ProgramPage : public wxNotebookPage {
public:
  ProgramPage(wxNotebook* parent, trance_pb::Session& session);
  ~ProgramPage();
  void RefreshData();

private:
  trance_pb::Session& _session;
  std::string _item_selected;
  ItemList<trance_pb::Program>* _item_list;
};

#endif
