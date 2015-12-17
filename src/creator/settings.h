#ifndef TRANCE_CREATOR_SETTINGS_H
#define TRANCE_CREATOR_SETTINGS_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#pragma warning(pop)

class CreatorFrame;
class SettingsFrame : public wxFrame {
public:
  enum {
    ID_OK = 1,
    ID_CANCEL = 2,
    ID_APPLY = 3,
  };
  SettingsFrame(CreatorFrame* parent, const std::string& executable_path);

private:
  const std::string _system_path;
  trance_pb::System _system;
  bool _system_dirty;

  CreatorFrame* _parent;
  wxCheckBox* _enable_vsync;
  wxCheckBox* _enable_oculus_rift;
  wxSpinCtrl* _image_cache_size;
  wxSpinCtrl* _font_cache_size;
  wxSlider* _image_depth;
  wxSlider* _text_depth;
  wxButton* _button_apply;

  void Changed();
  void Apply();
};

#endif