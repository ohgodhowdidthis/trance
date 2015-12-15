#include "../common.h"
#include "../session.h"
#include <filesystem>

#pragma warning(push, 0)
#define _UNICODE
#include <src/trance.pb.h>
#include <wx/wx.h>
#include <wx/cmdline.h>
#include <wx/notebook.h>
#include <wx/stdpaths.h>
#pragma warning(pop)

static const std::string session_file_pattern =
    "Session files (*.session)|*.session";

class CreatorFrame : public wxFrame {
public:
  enum {
    ID_EDIT_SYSTEM_CONFIG = 1,
  };

  CreatorFrame(const std::string& executable_path, const std::string& parameter)
    : wxFrame{nullptr, wxID_ANY, "creator", wxPoint{128, 128}, wxSize{480, 640}}
    , _session_dirty{false}
    , _executable_path{executable_path}
  {
    auto menuFile = new wxMenu;
    auto menuBar = new wxMenuBar;
    menuFile->Append(wxID_OPEN);
    menuFile->Append(wxID_SAVE);
    menuFile->Append(wxID_SAVEAS);
    menuFile->AppendSeparator();
    menuFile->Append(ID_EDIT_SYSTEM_CONFIG, "&Edit system settings...\tCtrl+E",
      "Edit global system settings that apply to all sessions");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    menuBar->Append(menuFile, "&File");
    SetMenuBar(menuBar);
    CreateStatusBar();
    SetStatusText("Running in " + _executable_path);

    auto panel = new wxPanel(this);
    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    auto notebook = new wxNotebook(panel, 0);
    sizer->Add(notebook, 1, wxEXPAND | wxALL, 0);
    panel->SetSizer(sizer);
    notebook->AddPage(new wxNotebookPage(notebook, wxID_ANY), "Themes");
    notebook->AddPage(new wxNotebookPage(notebook, wxID_ANY), "Programs");
    notebook->AddPage(new wxNotebookPage(notebook, wxID_ANY), "Playlist");

    if (!parameter.empty()) {
      OpenSession(parameter);
    }
  }

private:
  trance_pb::Session _session;
  bool _session_dirty;
  std::string _session_path;
  std::string _executable_path;

  bool ConfirmDiscardChanges()
  {
    if (!_session_dirty) {
      return true;
    }
    return wxMessageBox(
        "Current session has unsaved changes! Proceed?", "",
        wxICON_QUESTION | wxYES_NO, this) == wxYES;
  }

  bool OpenSession(const std::string& path)
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

  void SetSessionPath(const std::string& path)
  {
    _session_path = path;
    SetTitle("creator - " + _session_path);
  }

  void OnOpen(wxCommandEvent& event)
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

  void OnSave(wxCommandEvent& event)
  {
    if (_session_path.empty()) {
      OnSaveAs(event);
      return;
    }
    save_session(_session, _session_path);
    _session_dirty = false;
    SetStatusText("Wrote " + _session_path);
  }

  void OnSaveAs(wxCommandEvent& event)
  {
    wxFileDialog dialog{
        this, "Save session file", _executable_path, DEFAULT_SESSION_PATH,
        session_file_pattern, wxFD_SAVE | wxFD_OVERWRITE_PROMPT};
    if (dialog.ShowModal() == wxID_CANCEL) {
      return;
    }
    SetSessionPath(std::string(dialog.GetPath()));
    OnSave(event);
  }

  void OnEditSystemConfig(wxCommandEvent& event)
  {
  }

  void OnExit(wxCommandEvent& event)
  {
    Close(false);
  }

  void OnClose(wxCloseEvent& event)
  {
    if (event.CanVeto() && !ConfirmDiscardChanges()) {
      event.Veto();
      return;
    }
    Destroy();
  }

  wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(CreatorFrame, wxFrame)
EVT_MENU(wxID_OPEN, CreatorFrame::OnOpen)
EVT_MENU(wxID_SAVE, CreatorFrame::OnSave)
EVT_MENU(wxID_SAVEAS, CreatorFrame::OnSaveAs)
EVT_MENU(CreatorFrame::ID_EDIT_SYSTEM_CONFIG, CreatorFrame::OnEditSystemConfig)
EVT_MENU(wxID_EXIT, CreatorFrame::OnExit)
EVT_CLOSE(CreatorFrame::OnClose)
wxEND_EVENT_TABLE()

class CreatorApp : public wxApp {
public:
  CreatorApp() : _frame{nullptr} {}

  bool OnInit() override {
    wxApp::OnInit();
    std::tr2::sys::path path(
        std::string(wxStandardPaths::Get().GetExecutablePath()));
    _frame = new CreatorFrame{path.parent_path().string(), _parameter};
    _frame->Show(true);
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