#ifndef TRANCE_CREATOR_EXPORT_H
#define TRANCE_CREATOR_EXPORT_H

#pragma warning(push, 0)
#include <wx/frame.h>
#pragma warning(pop)

namespace trance_pb
{
  class System;
}
class CreatorFrame;
class wxSlider;
class wxSpinCtrl;

class ExportFrame : public wxFrame
{
public:
  enum {
    ID_EXPORT = 11041,
    ID_CANCEL = 11040,
  };
  ExportFrame(CreatorFrame* parent, trance_pb::System& system, const std::string& default_path);

private:
  void Export();

  trance_pb::System& _system;
  std::string _executable_path;
  std::string _path;

  CreatorFrame* _parent;
  wxSpinCtrl* _width;
  wxSpinCtrl* _height;
  wxSpinCtrl* _fps;
  wxSpinCtrl* _length;
  wxSpinCtrl* _threads;
  wxSlider* _quality;
};

#endif