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
class wxBoxSizer;
class wxCheckBox;
class wxChoice;
class wxPanel;
class wxSpinCtrl;
class wxStaticText;

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
  void AddNextItem(const std::string& name, std::uint32_t weight_value);

  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;
  std::string _item_selected;
  ItemList<trance_pb::PlaylistItem>* _item_list;

  wxCheckBox* _is_first;
  wxChoice* _program;
  wxSpinCtrl* _play_time_seconds;

  wxPanel* _left_panel;
  wxBoxSizer* _next_items_sizer;
  struct next_item {
    wxBoxSizer* sizer;
    wxStaticText* label;
    wxSpinCtrl* weight;
  };
  std::vector<next_item> _next_items;
};

#endif
