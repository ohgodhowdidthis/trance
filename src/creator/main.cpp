#include "main.h"
#include "settings.h"
#include "../common.h"
#include "../session.h"
#include <filesystem>

#pragma warning(push, 0)
#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#pragma warning(pop)

static const std::string session_file_pattern =
    "Session files (*.session)|*.session";

wxBEGIN_EVENT_TABLE(CreatorFrame, wxFrame)
EVT_MENU(wxID_NEW, CreatorFrame::OnNew)
EVT_MENU(wxID_OPEN, CreatorFrame::OnOpen)
EVT_MENU(wxID_SAVE, CreatorFrame::OnSave)
EVT_MENU(CreatorFrame::ID_EDIT_SYSTEM_CONFIG, CreatorFrame::OnEditSystemConfig)
EVT_MENU(wxID_EXIT, CreatorFrame::OnExit)
EVT_CLOSE(CreatorFrame::OnClose)
wxEND_EVENT_TABLE()

CreatorFrame::CreatorFrame(const std::string& executable_path, 
                           const std::string& parameter)
: wxFrame{nullptr, wxID_ANY, "Creator", wxDefaultPosition, wxSize{480, 640}}
, _session_dirty{false}
, _executable_path{executable_path}
, _panel{new wxPanel{this}}
, _sizer{new wxBoxSizer{wxHORIZONTAL}}
, _notebook{new wxNotebook{_panel, 0}}
{
  auto menuFile = new wxMenu;
  auto menuBar = new wxMenuBar;
  menuFile->Append(wxID_NEW);
  menuFile->Append(wxID_OPEN);
  menuFile->Append(wxID_SAVE);
  menuFile->AppendSeparator();
  menuFile->Append(ID_EDIT_SYSTEM_CONFIG, "&Edit system settings...\tCtrl+E",
                   "Edit global system settings that apply to all sessions");
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);
  menuBar->Append(menuFile, "&File");
  SetMenuBar(menuBar);
  CreateStatusBar();
  SetStatusText("Running in " + _executable_path);

  _notebook->AddPage(new wxNotebookPage{_notebook, wxID_ANY}, "Themes");
  _notebook->AddPage(new wxNotebookPage{_notebook, wxID_ANY}, "Programs");
  _notebook->AddPage(new wxNotebookPage{_notebook, wxID_ANY}, "Playlist");
  _sizer->Add(_notebook, 1, wxEXPAND | wxALL, 0);
  _panel->SetSizer(_sizer);
  _panel->Hide();
  Show(true);

  if (!parameter.empty()) {
    OpenSession(parameter);
  }
}

void CreatorFrame::OnNew(wxCommandEvent& event)
{
  if (!ConfirmDiscardChanges()) {
    return;
  }
  wxFileDialog dialog{
      this, "Choose session location", _executable_path, DEFAULT_SESSION_PATH,
      session_file_pattern, wxFD_SAVE | wxFD_OVERWRITE_PROMPT};
  if (dialog.ShowModal() == wxID_CANCEL) {
    return;
  }
  SetSessionPath(std::string(dialog.GetPath()));
  _session = {};
  _session_dirty = true;
}

void CreatorFrame::OnOpen(wxCommandEvent& event)
{
  if (!ConfirmDiscardChanges()) {
    return;
  }
  wxFileDialog dialog{this, "Open session file", _executable_path, "",
      session_file_pattern, wxFD_OPEN | wxFD_FILE_MUST_EXIST};
  if (dialog.ShowModal() == wxID_CANCEL) {
    return;
  }
  OpenSession(std::string(dialog.GetPath()));
}

void CreatorFrame::OnSave(wxCommandEvent& event)
{
  save_session(_session, _session_path);
  _session_dirty = false;
  SetStatusText("Wrote " + _session_path);
}

void CreatorFrame::OnEditSystemConfig(wxCommandEvent& event)
{
  new SettingsFrame{this, _executable_path};
}

void CreatorFrame::OnExit(wxCommandEvent& event)
{
  Close(false);
}

void CreatorFrame::OnClose(wxCloseEvent& event)
{
  if (event.CanVeto() && !ConfirmDiscardChanges()) {
    event.Veto();
    return;
  }
  Destroy();
}

bool CreatorFrame::ConfirmDiscardChanges()
{
  if (!_session_dirty) {
    return true;
  }
  return wxMessageBox(
      "Current session has unsaved changes! Proceed?", "",
      wxICON_QUESTION | wxYES_NO, this) == wxYES;
}

bool CreatorFrame::OpenSession(const std::string& path)
{
  try {
    _session = load_session(path);
    SetSessionPath(path);
    SetStatusText("Read " + _session_path);
    _session_dirty = false;
    return true;
  } catch (const std::exception& e) {
    wxMessageBox(std::string(e.what()), "", wxICON_ERROR, this);
    return false;
  }
}

void CreatorFrame::SetSessionPath(const std::string& path)
{
  _session_path = path;
  _panel->Show();
  _panel->Layout();
  SetTitle("creator - " + _session_path);
}

class CreatorApp : public wxApp {
public:
  CreatorApp() : _frame{nullptr} {}

  bool OnInit() override {
    wxApp::OnInit();
    std::tr2::sys::path path(
        std::string(wxStandardPaths::Get().GetExecutablePath()));
    _frame = new CreatorFrame{path.parent_path().string(), _parameter};
    SetTopWindow(_frame);
    return true;
  };

  int OnExit() override {
    // Deleting _frame causes a crash. Shrug.
    return 0;
  }

  void OnInitCmdLine(wxCmdLineParser& parser) override
  {
    static const wxCmdLineEntryDesc desc[] = {
      {wxCMD_LINE_PARAM, NULL, NULL, "session file",
       wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
      {wxCMD_LINE_NONE},
    };
    parser.SetDesc(desc);
    parser.SetSwitchChars("-");
  }

  bool OnCmdLineParsed(wxCmdLineParser& parser) override
  {
    if (parser.GetParamCount()) {
      _parameter = parser.GetParam(0);
    }
    return true;
  }

private:
  std::string _parameter;
  CreatorFrame* _frame;
};

wxIMPLEMENT_APP(CreatorApp);