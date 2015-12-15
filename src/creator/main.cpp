#include "../session.h"
#define _UNICODE
#include <wx/wx.h>

class CreatorFrame : public wxFrame {
public:
  CreatorFrame()
  : wxFrame{nullptr, wxID_ANY, "creator", wxPoint{128, 128}, wxSize{480, 640} }
  , _menuFile{new wxMenu}
  , _menuBar{new wxMenuBar}
  {
    _menuFile->Append(wxID_EXIT);
    _menuBar->Append(_menuFile, "&File");
    SetMenuBar(_menuBar);
    CreateStatusBar();
    SetStatusText("");
  }

private:
  wxMenu* _menuFile;
  wxMenuBar* _menuBar;

  void OnExit(wxCommandEvent& event)
  {
    Close(true);
  }

  void OnClose(wxCommandEvent& event)
  {
    delete _menuFile;
    delete _menuBar;
  }

  wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(CreatorFrame, wxFrame)
EVT_MENU(wxID_EXIT, CreatorFrame::OnExit)
EVT_MENU(wxID_CLOSE, CreatorFrame::OnClose)
wxEND_EVENT_TABLE()

class CreatorApp : public wxApp {
public:
  CreatorApp() : _frame{ nullptr } {}

  bool OnInit() override {
    _frame = new CreatorFrame;
    _frame->Show(true);
    return true;
  };

  int OnExit() override {
    // Deleting _frame causes a crash. Shrug.
    return 0;
  }

private:
  CreatorFrame* _frame;
};

wxIMPLEMENT_APP(CreatorApp);