#include "playlist.h"
#include "item_list.h"
#include "../common.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#include <wx/splitter.h>
#pragma warning(pop)

PlaylistPage::PlaylistPage(wxNotebook* parent, trance_pb::Session& session)
: wxNotebookPage{parent, wxID_ANY}
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

  _item_list = new ItemList<trance_pb::PlaylistItem>{
      splitter, *session.mutable_playlist(),
      [&](const std::string& s) { _item_selected = s; }};
  bottom_panel->SetSizer(bottom);

  sizer->Add(splitter, 1, wxEXPAND, 0);
  splitter->SplitHorizontally(_item_list, bottom_panel);
  SetSizer(sizer);
}

PlaylistPage::~PlaylistPage()
{
}

void PlaylistPage::RefreshData()
{
  _item_list->RefreshData();
}