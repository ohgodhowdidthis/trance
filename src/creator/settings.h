#ifndef TRANCE_SRC_CREATOR_SETTINGS_H
#define TRANCE_SRC_CREATOR_SETTINGS_H

#pragma warning(push, 0)
#include <wx/frame.h>
#pragma warning(pop)

namespace trance_pb
{
  class System;
}
class CreatorFrame;
class wxButton;
class wxCheckBox;
class wxRadioButton;
class wxSlider;
class wxSpinCtrl;

class SettingsFrame : public wxFrame
{
public:
  SettingsFrame(CreatorFrame* parent, trance_pb::System& system);

private:
  void Changed();
  void Apply();

  trance_pb::System& _system;

  CreatorFrame* _parent;
  wxRadioButton* _monitor;
  wxRadioButton* _oculus;
  wxRadioButton* _openvr;
  wxCheckBox* _enable_vsync;
  wxSpinCtrl* _image_cache_size;
  wxSpinCtrl* _font_cache_size;
  wxSlider* _draw_depth;
  wxButton* _button_apply;
};

#endif