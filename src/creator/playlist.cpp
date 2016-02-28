#include "playlist.h"
#include "item_list.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#pragma warning(pop)

PlaylistPage::PlaylistPage(wxNotebook* parent, trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};

  _item_list.reset(new ItemList<trance_pb::PlaylistItem>{
      this, sizer, *session.mutable_playlist(),
      [&](const std::string& s) { _item_selected = s; }});
  sizer->Add(bottom, 1, wxEXPAND, 0);
  SetSizer(sizer);
}

PlaylistPage::~PlaylistPage()
{
}

void PlaylistPage::RefreshData()
{
  _item_list->RefreshData();
}