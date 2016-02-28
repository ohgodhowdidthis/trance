#ifndef TRANCE_CREATOR_PLAYLIST_H
#define TRANCE_CREATOR_PLAYLIST_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>

template<typename T>
class ItemList;

class PlaylistPage : public wxNotebookPage {
public:
  PlaylistPage(wxNotebook* parent, trance_pb::Session& session);
  ~PlaylistPage();
  void RefreshData();

private:
  trance_pb::Session& _session;
  std::string _item_selected;
  ItemList<trance_pb::PlaylistItem>* _item_list;
};

#endif
