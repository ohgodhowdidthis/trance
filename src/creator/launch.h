#ifndef TRANCE_SRC_CREATOR_LAUNCH_H
#define TRANCE_SRC_CREATOR_LAUNCH_H
#include <functional>
#include <memory>
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
class wxButton;
class wxChoice;
class wxPanel;
class wxSizer;
class wxStaticText;

class VariableConfiguration
{
public:
  VariableConfiguration(trance_pb::System& system, const trance_pb::Session& session,
                        const std::string& session_path, const std::function<void()>& on_change,
                        wxPanel* panel);

  std::unordered_map<std::string, std::string> Variables() const;
  wxSizer* Sizer() const;
  void SaveToSystem(CreatorFrame* parent) const;
  void ResetDefaults();

private:
  trance_pb::System& _system;
  const trance_pb::Session& _session;
  std::string _session_path;
  std::function<void()> _on_change;

  wxSizer* _sizer;
  std::unordered_map<std::string, wxChoice*> _variables;
};

class LaunchFrame : public wxFrame
{
public:
  LaunchFrame(CreatorFrame* parent, trance_pb::System& system, const trance_pb::Session& session,
              const std::string& session_path);

private:
  void Launch();
  void RefreshTimeEstimate();

  CreatorFrame* _parent;
  trance_pb::System& _system;
  const trance_pb::Session& _session;
  std::string _session_path;
  std::unique_ptr<VariableConfiguration> _configuration;
  wxStaticText* _text;
};

#endif
