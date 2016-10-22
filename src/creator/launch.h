#ifndef TRANCE_CREATOR_LAUNCH_H
#define TRANCE_CREATOR_LAUNCH_H
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(push, 0)
#include <wx/frame.h>
#pragma warning(pop)

namespace trance_pb
{
class Session;
class System;
}
class CreatorFrame;
class wxChoice;
class wxStaticText;

class LaunchFrame : public wxFrame
{
public:
  enum {
    ID_LAUNCH = 10301,
    ID_DEFAULTS = 10302,
    ID_CANCEL = 10303,
  };
  LaunchFrame(CreatorFrame* parent, trance_pb::System& system, const trance_pb::Session& session,
              const std::string& session_path, const std::function<void()>& callback);

private:
  void Launch();
  void RefreshTimeEstimate();

  CreatorFrame* _parent;
  trance_pb::System& _system;
  const trance_pb::Session& _session;
  std::string _session_path;
  std::function<void()> _callback;
  std::unordered_map<std::string, wxChoice*> _variables;
  wxStaticText* _text;
};

#endif
