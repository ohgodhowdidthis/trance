#include "main.h"
#include "export.h"
#include "playlist.h"
#include "program.h"
#include "settings.h"
#include "theme.h"
#include "../common.h"
#include "../session.h"
#include "../util.h"
#include <filesystem>

#pragma warning(push, 0)
#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#pragma warning(pop)

static const std::string session_file_pattern =
    "Session files (*.session)|*.session";

CreatorFrame::CreatorFrame(const std::string& executable_path, 
                           const std::string& parameter)
: wxFrame{nullptr, wxID_ANY, "Creator", wxDefaultPosition, wxSize{640, 640}}
, _session_dirty{false}
, _executable_path{executable_path}
, _settings{nullptr}
, _export{nullptr}
, _theme_page{nullptr}
, _program_page{nullptr}
, _playlist_page{nullptr}
, _panel{new wxPanel{this}}
, _menu_bar{new wxMenuBar}
{
  auto menu_file = new wxMenu;
  menu_file->Append(wxID_NEW);
  menu_file->Append(wxID_OPEN);
  menu_file->Append(wxID_SAVE);
  menu_file->AppendSeparator();
  menu_file->Append(ID_LAUNCH_SESSION, "&Launch session\tCtrl+L",
                   "Launch the current session");
  menu_file->Append(ID_EXPORT_VIDEO, "Export &video...\tCtrl+V",
                   "Export the current session as a video");
  menu_file->AppendSeparator();
  menu_file->Append(ID_EDIT_SYSTEM_CONFIG, "&Edit system settings...\tCtrl+E",
                   "Edit global system settings that apply to all sessions");
  menu_file->AppendSeparator();
  menu_file->Append(wxID_EXIT);
  _menu_bar->Append(menu_file, "&File");
  _menu_bar->Enable(wxID_SAVE, false);
  _menu_bar->Enable(ID_LAUNCH_SESSION, false);
  _menu_bar->Enable(ID_EXPORT_VIDEO, false);
  SetMenuBar(_menu_bar);
  CreateStatusBar();

  std::string status = "Running in " + _executable_path;
  try {
    auto system_path = get_system_config_path(executable_path);
    _system = load_system(system_path);
    status += "; read " + system_path;
  } catch (std::runtime_error&) {
    _system = get_default_system();
  }
  if (_system.last_root_directory().empty()) {
    _system.set_last_root_directory(executable_path);
  }
  SetStatusText(status);

  auto notebook = new wxNotebook{_panel, wxID_ANY};
  _theme_page =
      new ThemePage{notebook, *this, _session, _session_path};
  _program_page = new ProgramPage{notebook, *this, _session};
  _playlist_page = new PlaylistPage{notebook, *this, _session};

  notebook->AddPage(_theme_page, "Themes");
  notebook->AddPage(_program_page, "Programs");
  notebook->AddPage(_playlist_page, "Playlist");

  auto sizer = new wxBoxSizer{wxHORIZONTAL};
  sizer->Add(notebook, 1, wxEXPAND, 0);
  _panel->SetSizer(sizer);
  _panel->Hide();
  Show(true);

  if (!parameter.empty()) {
    OpenSession(parameter);
  }

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    if (!ConfirmDiscardChanges()) {
      return;
    }
    wxFileDialog dialog{
        this, "Choose session location", _system.last_root_directory(),
        DEFAULT_SESSION_PATH, session_file_pattern,
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT};
    if (dialog.ShowModal() == wxID_CANCEL) {
      return;
    }
    _session = get_default_session();
    std::tr2::sys::path path{std::string(dialog.GetPath())};
    search_resources(_session, path.parent_path().string());
    SetSessionPath(std::string(dialog.GetPath()));
    SetStatusText("Generated default session for " +
                  path.parent_path().string());
    MakeDirty(true);
  }, wxID_NEW);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    if (!ConfirmDiscardChanges()) {
      return;
    }
    wxFileDialog dialog{
        this, "Open session file", _system.last_root_directory(),
        "", session_file_pattern, wxFD_OPEN | wxFD_FILE_MUST_EXIST};
    if (dialog.ShowModal() == wxID_CANCEL) {
      return;
    }
    OpenSession(std::string(dialog.GetPath()));
  }, wxID_OPEN);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    save_session(_session, _session_path);
    MakeDirty(false);
    SetStatusText("Wrote " + _session_path);
    _menu_bar->Enable(ID_LAUNCH_SESSION, true);
    _menu_bar->Enable(ID_EXPORT_VIDEO, true);
  }, wxID_SAVE);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    auto trance_exe_path = get_trance_exe_path(_executable_path);
    auto system_config_path = get_system_config_path(_executable_path);
    auto command_line = trance_exe_path +
        " \"" + _session_path + "\" \"" + system_config_path + "\"";
    SetStatusText("Running " + command_line);
    wxExecute(command_line, wxEXEC_ASYNC | wxEXEC_SHOW_CONSOLE);
  }, ID_LAUNCH_SESSION);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    if (_export) {
      _export->Raise();
      return;
    }
    auto default_path = (_executable_path /
        *--std::tr2::sys::path{_session_path}.end()).string();
    _export = new ExportFrame{this, _system, _executable_path, default_path};
    if (_export->Cancelled()) {
      _export->Close();
    }
  }, ID_EXPORT_VIDEO);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    if (_settings) {
      _settings->Raise();
      return;
    }
    _settings = new SettingsFrame{this, _system};
  }, ID_EDIT_SYSTEM_CONFIG);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event)
  {
    Close(false);
  }, wxID_EXIT);

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event)
  {
    if (event.CanVeto() && !ConfirmDiscardChanges()) {
      event.Veto();
      return;
    }
    Destroy();
  });
}

void CreatorFrame::MakeDirty(bool dirty)
{
  _session_dirty = dirty;
  SetTitle((_session_dirty ? " [*] Creator - " : "Creator - ") + _session_path);
}

void CreatorFrame::SaveSystem(bool show_status)
{
  auto system_path = get_system_config_path(_executable_path);
  save_system(_system, system_path);
  if (show_status) {
    SetStatusText("Wrote " + system_path);
  }
}

void CreatorFrame::ExportVideo(const std::string& path) {
  bool frame_by_frame =
      ext_is(path, "jpg") || ext_is(path, "png") || ext_is(path, "bmp");
  const auto& settings = _system.last_export_settings();

  auto trance_exe_path = get_trance_exe_path(_executable_path);
  auto system_config_path = get_system_config_path(_executable_path);
  auto command_line = trance_exe_path +
      " \"" + _session_path + "\" \"" + system_config_path +
      "\" --export_path=\"" + path +
      "\" --export_width=" + std::to_string(settings.width()) +
      " --export_height=" + std::to_string(settings.height()) +
      " --export_fps=" + std::to_string(settings.fps()) +
      " --export_length=" + std::to_string(settings.length());
  if (!frame_by_frame) {
    command_line +=
        " --export_quality=" + std::to_string(settings.quality()) +
        " --export_threads=" + std::to_string(settings.threads());
  }
  SetStatusText("Running " + command_line);
  wxExecute(command_line, wxEXEC_ASYNC | wxEXEC_SHOW_CONSOLE);
}

void CreatorFrame::SettingsClosed()
{
  _settings = nullptr;
}

void CreatorFrame::ExportClosed()
{
  _export = nullptr;
}

void CreatorFrame::ThemeCreated(const std::string& theme_name) {
  _program_page->RefreshThemes();
  _program_page->RefreshOurData();
  SetStatusText("Created theme '" + theme_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ThemeDeleted(const std::string& theme_name) {
  for (auto& pair : *_session.mutable_program_map()) {
    auto it = pair.second.mutable_enabled_theme_name()->begin();
    while (it != pair.second.mutable_enabled_theme_name()->end()) {
      if (*it == theme_name) {
        it = pair.second.mutable_enabled_theme_name()->erase(it);
      } else {
        ++it;
      }
    }
  }
  _program_page->RefreshThemes();
  _program_page->RefreshOurData();
  SetStatusText("Deleted theme '" + theme_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ThemeRenamed(const std::string& old_name,
                                const std::string& new_name) {
  for (auto& pair : *_session.mutable_program_map()) {
    for (auto& theme_name : *pair.second.mutable_enabled_theme_name()) {
      if (theme_name == old_name) {
        theme_name = new_name;
      }
    }
  }
  _program_page->RefreshThemes();
  _program_page->RefreshOurData();
  SetStatusText("Renamed theme '" + old_name + "' to '" + new_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ProgramCreated(const std::string& program_name) {
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Created program '" + program_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ProgramDeleted(const std::string& program_name) {
  for (auto& pair : *_session.mutable_playlist()) {
    if (pair.second.program() == program_name) {
      if (_session.program_map().empty()) {
        pair.second.set_program("");
      } else {
        pair.second.set_program(_session.program_map().begin()->first);
      }
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Deleted program '" + program_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ProgramRenamed(const std::string& old_name,
                                  const std::string& new_name) {
  for (auto& pair : *_session.mutable_playlist()) {
    if (pair.second.program() == old_name) {
      pair.second.set_program(new_name);
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Renamed program '" + old_name + "' to '" + new_name + "'");
  MakeDirty(true);
}

void CreatorFrame::PlaylistItemCreated(const std::string& playlist_item_name) {
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Created playlist item '" + playlist_item_name + "'");
  MakeDirty(true);
}

void CreatorFrame::PlaylistItemDeleted(const std::string& playlist_item_name) {
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.playlist_item_name() == playlist_item_name) {
        next_item.set_playlist_item_name(_session.playlist().begin()->first);
      }
    }
  }
  if (_session.first_playlist_item() == playlist_item_name) {
    _session.set_first_playlist_item(
        _session.playlist().empty() ? "" : _session.playlist().begin()->first);
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Deleted playlist item '" + playlist_item_name + "'");
  MakeDirty(true);
}

void CreatorFrame::PlaylistItemRenamed(const std::string& old_name,
                                       const std::string& new_name) {
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.playlist_item_name() == old_name) {
        next_item.set_playlist_item_name(new_name);
      }
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText(
      "Renamed playlist item '" + old_name + "' to '" + new_name + "'");
  MakeDirty(true);
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
    _menu_bar->Enable(ID_LAUNCH_SESSION, true);
    _menu_bar->Enable(ID_EXPORT_VIDEO, true);
    MakeDirty(false);
    return true;
  } catch (const std::exception& e) {
    wxMessageBox(std::string(e.what()), "", wxICON_ERROR, this);
    return false;
  }
}

void CreatorFrame::SetSessionPath(const std::string& path)
{
  _session_path = path;
  _menu_bar->Enable(wxID_SAVE, true);
  _panel->Show();
  _panel->Layout();
  auto parent = std::tr2::sys::path{_session_path}.parent_path().string();
  _theme_page->RefreshDirectory(parent);
  _theme_page->RefreshData();
  _program_page->RefreshThemes();
  _program_page->RefreshData();
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshData();

  _system.set_last_root_directory(parent);
  SaveSystem(false);
}

class CreatorApp : public wxApp {
public:
  CreatorApp() : _frame{nullptr} {}

  bool OnInit() override {
    wxApp::OnInit();
    std::tr2::sys::path path{
        std::string(wxStandardPaths::Get().GetExecutablePath())};
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