#ifndef TRANCE_SRC_CREATOR_PLAYLIST_H
#define TRANCE_SRC_CREATOR_PLAYLIST_H
#include <memory>
#include <vector>

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

namespace trance_pb
{
  class AudioEvent;
  class PlaylistItem;
  class Session;
}
class CreatorFrame;
template <typename T>
class ItemList;
class wxBoxSizer;
class wxCheckBox;
class wxChoice;
class wxPanel;
class wxRadioButton;
class wxSpinCtrl;
class wxStaticText;

class PlaylistPage : public wxNotebookPage
{
public:
  PlaylistPage(wxNotebook* parent, CreatorFrame& creator_frame, trance_pb::Session& session);
  ~PlaylistPage();
  void RefreshOurData();
  void RefreshData();
  void RefreshProgramsAndPlaylists();
  void RefreshDirectory(const std::string& directory);

private:
  void SwitchToStandard();
  void SwitchToSubroutine();
  void AddSubroutineItem(const std::string& playlist_item_name);
  void AddNextItem(const std::string& name, std::uint32_t weight_value, const std::string& variable,
                   const std::string& variable_value);
  void AddAudioEvent(const trance_pb::AudioEvent& event);

  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;
  std::string _item_selected;
  std::vector<std::string> _audio_files;
  ItemList<trance_pb::PlaylistItem>* _item_list;

  wxCheckBox* _is_first;
  wxRadioButton* _standard;
  wxRadioButton* _subroutine;
  wxChoice* _program;
  wxSpinCtrl* _play_time_seconds;

  wxPanel* _left_panel;
  wxPanel* _right_panel;
  wxPanel* _bottom_bottom_panel;
  wxBoxSizer* _playlist_item_sizer;
  wxBoxSizer* _next_items_sizer;
  wxBoxSizer* _audio_events_sizer;
  std::vector<wxChoice*> _subroutine_items;
  std::vector<wxBoxSizer*> _next_items;
  std::vector<wxBoxSizer*> _audio_events;
};

#endif
