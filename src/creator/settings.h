#ifndef TRANCE_CREATOR_SETTINGS_H
#define TRANCE_CREATOR_SETTINGS_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/checkbox.h>
#include <wx/frame.h>
#include <wx/panel.h>
#pragma warning(pop)

class CreatorFrame;
class SettingsFrame : public wxFrame {
public:
  SettingsFrame(CreatorFrame* parent, const std::string& executable_path);

private:
  const std::string _system_path;
  trance_pb::System _system;
  bool _system_dirty;

  CreatorFrame* _parent;
  wxPanel* _panel;
  wxCheckBox* _enable_vsync;
  wxCheckBox* _enable_oculus_rift;
};

#endif