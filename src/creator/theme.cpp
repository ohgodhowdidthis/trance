#include "theme.h"
#include "item_list.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#pragma warning(pop)

ThemePage::ThemePage(wxNotebook* parent, trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  _item_list.reset(new ItemList<trance_pb::Theme>{
      this, sizer, *session.mutable_theme_map()});
  sizer->Add(bottom, 1, wxEXPAND, 0);
  SetSizer(sizer);
}

ThemePage::~ThemePage()
{
}

void ThemePage::RefreshData()
{
  _item_list->RefreshData();
}