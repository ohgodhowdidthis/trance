#ifndef TRANCE_CREATOR_VARIABLES_H
#define TRANCE_CREATOR_VARIABLES_H

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

namespace trance_pb {
  class Session;
  class Variable;
}
class CreatorFrame;
template<typename T>
class ItemList;

class VariablePage : public wxNotebookPage {
public:
  VariablePage(wxNotebook* parent,
               CreatorFrame& creator_frame,
               trance_pb::Session& session);
  ~VariablePage();
  void RefreshOurData();
  void RefreshData();

private:
  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;

  std::string _item_selected;
  ItemList<trance_pb::Variable>* _item_list;
};

#endif
