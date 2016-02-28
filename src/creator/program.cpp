#include "program.h"
#include "item_list.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#pragma warning(pop)

ProgramPage::ProgramPage(wxNotebook* parent, trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  _item_list.reset(new ItemList<trance_pb::Program>{
      this, sizer, *session.mutable_program_map(),
      [&](const std::string& s) { _item_selected = s; }});
  sizer->Add(bottom, 1, wxEXPAND, 0);
  SetSizer(sizer);
}

ProgramPage::~ProgramPage()
{
}

void ProgramPage::RefreshData()
{
  _item_list->RefreshData();
}