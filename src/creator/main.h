#ifndef TRANCE_CREATOR_MAIN_H
#define TRANCE_CREATOR_MAIN_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#pragma warning(pop)

class CreatorFrame : public wxFrame {
public:
  enum {
    ID_EDIT_SYSTEM_CONFIG = 1,
  };

  CreatorFrame(const std::string& executable_path, const std::string& parameter);

private:
  trance_pb::Session _session;
  bool _session_dirty;
  std::string _session_path;
  std::string _executable_path;

  wxPanel* _panel;
  wxSizer* _sizer;
  wxNotebook* _notebook;

  void OnNew(wxCommandEvent& event);
  void OnOpen(wxCommandEvent& event);
  void OnSave(wxCommandEvent& event);
  void OnEditSystemConfig(wxCommandEvent& event);
  void OnExit(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  bool ConfirmDiscardChanges();
  bool OpenSession(const std::string& path);
  void SetSessionPath(const std::string& path);

  wxDECLARE_EVENT_TABLE();
};

#endif