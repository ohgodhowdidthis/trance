#ifndef TRANCE_CREATOR_PLAYLIST_H
#define TRANCE_CREATOR_PLAYLIST_H

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

class PlaylistPage : public wxNotebookPage {
public:
  PlaylistPage(wxNotebook* parent,
               CreatorFrame& creator_frame,
               trance_pb::Session& session);
  ~PlaylistPage();
  void RefreshOurData();
  void RefreshData();
  void RefreshProgramsAndPlaylists();

private:
  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;
  std::string _item_selected;
  ItemList<trance_pb::PlaylistItem>* _item_list;
};

#endif
