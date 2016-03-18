#ifndef TRANCE_CREATOR_PLAYLIST_H
#define TRANCE_CREATOR_PLAYLIST_H

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>
#include <vector>

namespace trance_pb {
  class PlaylistItem;
  class Session;
}
class CreatorFrame;
template<typename T>
class ItemList;
class wxCheckBox;
class wxChoice;
class wxSpinCtrl;

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

  wxCheckBox* _is_first;
  wxChoice* _program;
  wxSpinCtrl* _play_time_seconds;

  struct next_item {
    wxSpinCtrl* weight;
  };
  std::vector<next_item> _next_items;
};

#endif
