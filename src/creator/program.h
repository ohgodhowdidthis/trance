#ifndef TRANCE_CREATOR_PROGRAM_H
#define TRANCE_CREATOR_PROGRAM_H

#pragma warning(push, 0)
#include <wx/notebook.h>
#pragma warning(pop)

#include <memory>
#include <unordered_map>

namespace trance_pb {
  class Program;
  class Session;
}
class CreatorFrame;
template<typename T>
class ItemList;
class wxSpinCtrl;
class wxCheckBox;
class wxColourPickerCtrl;
class wxSlider;
class wxTreeListCtrl;
class wxTreeListItem;

class ProgramPage : public wxNotebookPage {
public:
  ProgramPage(wxNotebook* parent,
              CreatorFrame& creator_frame,
              trance_pb::Session& session);
  ~ProgramPage();
  void RefreshOurData();
  void RefreshData();
  void RefreshThemes();

private:
  void Changed();

  CreatorFrame& _creator_frame;
  trance_pb::Session& _session;
  std::string _item_selected;

  ItemList<trance_pb::Program>* _item_list;
  std::unordered_map<std::string, wxTreeListItem> _tree_lookup;
  std::unordered_map<unsigned int, wxSpinCtrl*> _visual_lookup;
  wxTreeListCtrl* _tree;

  wxSpinCtrl* _global_fps;
  wxSlider* _zoom_intensity;
  wxCheckBox* _reverse_spiral;
  wxColourPickerCtrl* _spiral_colour_a;
  wxColourPickerCtrl* _spiral_colour_b;
  wxColourPickerCtrl* _main_text_colour;
  wxColourPickerCtrl* _shadow_text_colour;
  wxSpinCtrl* _spiral_colour_a_alpha;
  wxSpinCtrl* _spiral_colour_b_alpha;
  wxSpinCtrl* _main_text_colour_alpha;
  wxSpinCtrl* _shadow_text_colour_alpha;
};

#endif
