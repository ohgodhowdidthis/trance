#include "program.h"
#include "item_list.h"
#include "main.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#include <wx/splitter.h>
#pragma warning(pop)

ProgramPage::ProgramPage(wxNotebook* parent,
                         CreatorFrame& creator_frame,
                         trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
, _creator_frame{creator_frame}
, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto splitter = new wxSplitterWindow{
      this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxSP_THIN_SASH | wxSP_LIVE_UPDATE};
  splitter->SetSashGravity(0);
  splitter->SetMinimumPaneSize(128);

  auto bottom_panel = new wxPanel{splitter, wxID_ANY};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  _item_list = new ItemList<trance_pb::Program>{
      splitter, *session.mutable_program_map(),
      [&](const std::string& s) { _item_selected = s; },
      std::bind(&CreatorFrame::ProgramCreated, &creator_frame),
      std::bind(&CreatorFrame::ProgramDeleted, &creator_frame,
                std::placeholders::_1),
      std::bind(&CreatorFrame::ProgramRenamed, &creator_frame,
                std::placeholders::_1, std::placeholders::_2)};
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);
}

ProgramPage::~ProgramPage()
{
}

void ProgramPage::RefreshData()
{
  _item_list->RefreshData();
}