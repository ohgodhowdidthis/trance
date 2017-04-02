#ifndef TRANCE_SRC_CREATOR_EXPORT_H
#define TRANCE_SRC_CREATOR_EXPORT_H
#include <creator/launch.h>
#include <memory>

#pragma warning(push, 0)
#include <wx/frame.h>
#pragma warning(pop)

namespace trance_pb
{
  class System;
}
class CreatorFrame;
class wxCheckBox;
class wxSlider;
class wxSpinCtrl;

class ExportFrame : public wxFrame
{
public:
  ExportFrame(CreatorFrame* parent, trance_pb::System& system, const trance_pb::Session& session,
              const std::string& session_path, const std::string& executable_path);

private:
  void ResetDefaults();
  void Export();

  trance_pb::System& _system;
  std::string _path;

  CreatorFrame* _parent;
  wxCheckBox* _export_3d;
  wxSpinCtrl* _width;
  wxSpinCtrl* _height;
  wxSpinCtrl* _fps;
  wxSpinCtrl* _length;
  wxSpinCtrl* _threads;
  wxSlider* _quality;
  std::unique_ptr<VariableConfiguration> _configuration;
};

#endif