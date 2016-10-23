#include "main.h"
#include <filesystem>
#include "../common.h"
#include "../session.h"
#include "../util.h"
#include "export.h"
#include "launch.h"
#include "playlist.h"
#include "program.h"
#include "settings.h"
#include "theme.h"
#include "variables.h"

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

static const std::string session_file_pattern = "Session files (*.session)|*.session";

CreatorFrame::CreatorFrame(const std::string& executable_path, const std::string& parameter)
: wxFrame{nullptr, wxID_ANY, "Creator", wxDefaultPosition, wxSize{640, 640}}
, _session_dirty{false}
, _executable_path{executable_path}
, _settings{nullptr}
, _theme_page{nullptr}
, _program_page{nullptr}
, _playlist_page{nullptr}
, _variable_page{nullptr}
, _panel{new wxPanel{this}}
, _menu_bar{new wxMenuBar}
{
  auto menu_file = new wxMenu;
  menu_file->Append(wxID_NEW);
  menu_file->Append(wxID_OPEN);
  menu_file->Append(wxID_SAVE);
  menu_file->AppendSeparator();
  menu_file->Append(ID_LAUNCH_SESSION, "&Launch session\tCtrl+L", "Launch the current session");
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

  _notebook = new wxNotebook{_panel, wxID_ANY};
  _theme_page = new ThemePage{_notebook, *this, _session, _session_path};
  _program_page = new ProgramPage{_notebook, *this, _session};
  _playlist_page = new PlaylistPage{_notebook, *this, _session};
  _variable_page = new VariablePage{_notebook, *this, _session};

  _notebook->AddPage(_theme_page, "Themes");
  _notebook->AddPage(_program_page, "Programs");
  _notebook->AddPage(_playlist_page, "Playlist");
  _notebook->AddPage(_variable_page, "Variables");

  auto sizer = new wxBoxSizer{wxHORIZONTAL};
  sizer->Add(_notebook, 1, wxEXPAND, 0);
  _panel->SetSizer(sizer);
  _panel->Hide();
  Show(true);

  if (!parameter.empty()) {
    OpenSession(parameter);
  }

  Bind(wxEVT_MENU,
       [&](wxCommandEvent& event) {
         if (!ConfirmDiscardChanges()) {
           return;
         }
         wxFileDialog dialog{this,
                             "Choose session location",
                             _system.last_root_directory(),
                             DEFAULT_SESSION_PATH,
                             session_file_pattern,
                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT};
         if (dialog.ShowModal() == wxID_CANCEL) {
           return;
         }
         _session = get_default_session();
         std::tr2::sys::path path{std::string(dialog.GetPath())};
         search_resources(_session, path.parent_path().string());
         SetSessionPath(std::string(dialog.GetPath()));
         SetStatusText("Generated default session for " + path.parent_path().string());
         MakeDirty(true);
       },
       wxID_NEW);

  Bind(wxEVT_MENU,
       [&](wxCommandEvent& event) {
         if (!ConfirmDiscardChanges()) {
           return;
         }
         wxFileDialog dialog{this, "Open session file",  _system.last_root_directory(),
                             "",   session_file_pattern, wxFD_OPEN | wxFD_FILE_MUST_EXIST};
         if (dialog.ShowModal() == wxID_CANCEL) {
           return;
         }
         OpenSession(std::string(dialog.GetPath()));
       },
       wxID_OPEN);

  Bind(wxEVT_MENU,
       [&](wxCommandEvent& event) {
         save_session(_session, _session_path);
         MakeDirty(false);
         SetStatusText("Wrote " + _session_path);
         _menu_bar->Enable(ID_LAUNCH_SESSION, true);
         _menu_bar->Enable(ID_EXPORT_VIDEO, true);
       },
       wxID_SAVE);

  Bind(wxEVT_MENU,
       [&](wxCommandEvent& event) {
         if (!ConfirmDiscardChanges()) {
           return;
         }
         Disable();
         _notebook->Disable();
         auto frame = new LaunchFrame{this, _system, _session, _session_path};
       },
       ID_LAUNCH_SESSION);

  Bind(wxEVT_MENU,
       [&](wxCommandEvent& event) {
         if (!ConfirmDiscardChanges()) {
           return;
         }
         Disable();
         _notebook->Disable();
         auto frame = new ExportFrame{this, _system, _session, _session_path, _executable_path};
       },
       ID_EXPORT_VIDEO);

  Bind(wxEVT_MENU,
       [&](wxCommandEvent& event) {
         if (_settings) {
           _settings->Raise();
           return;
         }
         _settings = new SettingsFrame{this, _system};
       },
       ID_EDIT_SYSTEM_CONFIG);

  Bind(wxEVT_MENU, [&](wxCommandEvent& event) { Close(false); }, wxID_EXIT);

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event) {
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

void CreatorFrame::Launch()
{
  auto trance_exe_path = get_trance_exe_path(_executable_path);
  auto system_config_path = get_system_config_path(_executable_path);
  auto command_line = trance_exe_path + " \"" + _session_path + "\" \"" + system_config_path + "\"";
  if (!_session.variable_map().empty()) {
    command_line += " \"--variables=" + EncodeVariables() + "\"";
  }
  SetStatusText("Running " + command_line);
  wxExecute(command_line, wxEXEC_ASYNC | wxEXEC_SHOW_CONSOLE);
}

void CreatorFrame::ExportVideo(const std::string& path)
{
  bool frame_by_frame = ext_is(path, "jpg") || ext_is(path, "png") || ext_is(path, "bmp");
  const auto& settings = _system.last_export_settings();

  auto trance_exe_path = get_trance_exe_path(_executable_path);
  auto system_config_path = get_system_config_path(_executable_path);
  auto command_line = trance_exe_path + " \"" + _session_path + "\" \"" + system_config_path +
      "\" --export_path=\"" + path + "\" --export_width=" + std::to_string(settings.width()) +
      " --export_height=" + std::to_string(settings.height()) + " --export_fps=" +
      std::to_string(settings.fps()) + " --export_length=" + std::to_string(settings.length());
  if (!frame_by_frame) {
    command_line += " --export_quality=" + std::to_string(settings.quality()) +
        " --export_threads=" + std::to_string(settings.threads());
  }
  if (!_session.variable_map().empty()) {
    command_line += " \"--variables=" + EncodeVariables() + "\"";
  }
  SetStatusText("Running " + command_line);
  wxExecute(command_line, wxEXEC_ASYNC | wxEXEC_SHOW_CONSOLE);
}

void CreatorFrame::RefreshDirectory()
{
  auto parent = std::tr2::sys::path{_session_path}.parent_path().string();
  _theme_page->RefreshDirectory(parent);
  _playlist_page->RefreshDirectory(parent);
  _theme_page->RefreshData();
  _program_page->RefreshThemes();
  _program_page->RefreshData();
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshData();
  _variable_page->RefreshData();
  _system.set_last_root_directory(parent);
  SaveSystem(false);
}

void CreatorFrame::SettingsClosed()
{
  _settings = nullptr;
}

void CreatorFrame::ExportClosed()
{
  _notebook->Enable();
  Enable();
}

void CreatorFrame::LaunchClosed()
{
  _notebook->Enable();
  Enable();
}

void CreatorFrame::ThemeCreated(const std::string& theme_name)
{
  _program_page->RefreshThemes();
  _program_page->RefreshOurData();
  SetStatusText("Created theme '" + theme_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ThemeDeleted(const std::string& theme_name)
{
  for (auto& pair : *_session.mutable_program_map()) {
    auto it = pair.second.mutable_enabled_theme()->begin();
    while (it != pair.second.mutable_enabled_theme()->end()) {
      if (it->theme_name() == theme_name) {
        it = pair.second.mutable_enabled_theme()->erase(it);
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

void CreatorFrame::ThemeRenamed(const std::string& old_name, const std::string& new_name)
{
  for (auto& pair : *_session.mutable_program_map()) {
    for (auto& theme : *pair.second.mutable_enabled_theme()) {
      if (theme.theme_name() == old_name) {
        theme.set_theme_name(new_name);
      }
    }
  }
  _program_page->RefreshThemes();
  _program_page->RefreshOurData();
  SetStatusText("Renamed theme '" + old_name + "' to '" + new_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ProgramCreated(const std::string& program_name)
{
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Created program '" + program_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ProgramDeleted(const std::string& program_name)
{
  for (auto& pair : *_session.mutable_playlist()) {
    if (pair.second.has_standard() && pair.second.standard().program() == program_name) {
      if (_session.program_map().empty()) {
        pair.second.mutable_standard()->set_program("");
      } else {
        pair.second.mutable_standard()->set_program(_session.program_map().begin()->first);
      }
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Deleted program '" + program_name + "'");
  MakeDirty(true);
}

void CreatorFrame::ProgramRenamed(const std::string& old_name, const std::string& new_name)
{
  for (auto& pair : *_session.mutable_playlist()) {
    if (pair.second.has_standard() && pair.second.standard().program() == old_name) {
      pair.second.mutable_standard()->set_program(new_name);
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Renamed program '" + old_name + "' to '" + new_name + "'");
  MakeDirty(true);
}

void CreatorFrame::PlaylistItemCreated(const std::string& playlist_item_name)
{
  auto& playlist_item = (*_session.mutable_playlist())[playlist_item_name];
  if (!_session.program_map().empty() && !playlist_item.has_standard() &&
      !playlist_item.has_subroutine()) {
    playlist_item.mutable_standard()->set_program(_session.program_map().begin()->first);
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Created playlist item '" + playlist_item_name + "'");
  MakeDirty(true);
}

void CreatorFrame::PlaylistItemDeleted(const std::string& playlist_item_name)
{
  if (_session.first_playlist_item() == playlist_item_name) {
    _session.set_first_playlist_item(
        _session.playlist().empty() ? "" : _session.playlist().begin()->first);
  }
  for (auto& pair : *_session.mutable_playlist()) {
    auto it = pair.second.mutable_next_item()->begin();
    while (it != pair.second.mutable_next_item()->end()) {
      it = it->playlist_item_name() == playlist_item_name
          ? pair.second.mutable_next_item()->erase(it)
          : 1 + it;
    }
    if (pair.second.has_subroutine()) {
      auto& subroutine = *pair.second.mutable_subroutine();
      for (auto it = subroutine.mutable_playlist_item_name()->begin();
           it != subroutine.mutable_playlist_item_name()->end();) {
        it =
            *it == playlist_item_name ? subroutine.mutable_playlist_item_name()->erase(it) : 1 + it;
      }
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Deleted playlist item '" + playlist_item_name + "'");
  MakeDirty(true);
}

void CreatorFrame::PlaylistItemRenamed(const std::string& old_name, const std::string& new_name)
{
  if (_session.first_playlist_item() == old_name) {
    _session.set_first_playlist_item(new_name);
  }
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.playlist_item_name() == old_name) {
        next_item.set_playlist_item_name(new_name);
      }
    }
    if (pair.second.has_subroutine()) {
      for (auto& name : *pair.second.mutable_subroutine()->mutable_playlist_item_name()) {
        if (name == old_name) {
          name = new_name;
        }
      }
    }
  }
  _playlist_page->RefreshProgramsAndPlaylists();
  _playlist_page->RefreshOurData();
  SetStatusText("Renamed playlist item '" + old_name + "' to '" + new_name + "'");
  MakeDirty(true);
}

void CreatorFrame::VariableCreated(const std::string& variable_name)
{
  static const std::string new_value_name = "Default";
  auto& variable = (*_session.mutable_variable_map())[variable_name];
  variable.add_value(new_value_name);
  variable.set_default_value(new_value_name);
  _variable_page->RefreshOurData();
  _playlist_page->RefreshOurData();
  MakeDirty(true);
}

void CreatorFrame::VariableDeleted(const std::string& variable_name)
{
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.condition_variable_name() == variable_name) {
        next_item.clear_condition_variable_name();
        next_item.clear_condition_variable_value();
      }
    }
  }
  _playlist_page->RefreshOurData();
  MakeDirty(true);
}

void CreatorFrame::VariableRenamed(const std::string& old_name, const std::string& new_name)
{
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.condition_variable_name() == old_name) {
        next_item.set_condition_variable_name(new_name);
      }
    }
  }
  _playlist_page->RefreshOurData();
  MakeDirty(true);
}

void CreatorFrame::VariableValueCreated(const std::string& variable_name,
                                        const std::string& value_name)
{
  _playlist_page->RefreshOurData();
  MakeDirty(true);
}

void CreatorFrame::VariableValueDeleted(const std::string& variable_name,
                                        const std::string& value_name)
{
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.condition_variable_name() == variable_name &&
          next_item.condition_variable_value() == value_name) {
        next_item.clear_condition_variable_name();
        next_item.clear_condition_variable_value();
      }
    }
  }
  _playlist_page->RefreshOurData();
  MakeDirty(true);
}

void CreatorFrame::VariableValueRenamed(const std::string& variable_name,
                                        const std::string& old_name, const std::string& new_name)
{
  for (auto& pair : *_session.mutable_playlist()) {
    for (auto& next_item : *pair.second.mutable_next_item()) {
      if (next_item.condition_variable_name() == variable_name &&
          next_item.condition_variable_value() == old_name) {
        next_item.set_condition_variable_value(new_name);
      }
    }
  }
  _playlist_page->RefreshOurData();
  MakeDirty(true);
}

bool CreatorFrame::ConfirmDiscardChanges()
{
  if (!_session_dirty) {
    return true;
  }
  return wxMessageBox("Current session has unsaved changes! Proceed?", "",
                      wxICON_QUESTION | wxYES_NO, this) == wxYES;
}

bool CreatorFrame::OpenSession(const std::string& path)
{
  try {
    _session = load_session(path);
    SetStatusText("Read " + _session_path);
    SetSessionPath(path);
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
  RefreshDirectory();
  _panel->Layout();
  _panel->Show();
}

std::string CreatorFrame::EncodeVariables()
{
  auto encode = [](const std::string& s) {
    std::string r;
    for (char c : s) {
      if (c == '\\') {
        r += "\\\\";
      } else if (c == ';') {
        r += "\\;";
      } else if (c == '=') {
        r += "\\=";
      } else if (c == '"') {
        r += "\\\"";
      } else {
        r += c;
      }
    }
    return r;
  };

  std::string result;
  bool first = true;
  auto it = _system.last_session_map().find(_session_path);
  for (const auto& pair : _session.variable_map()) {
    if (first) {
      first = false;
    } else {
      result += ";";
    }
    result += encode(pair.first) + "=";

    bool found = false;
    if (it != _system.last_session_map().end()) {
      auto jt = it->second.variable_map().find(pair.first);
      if (jt != it->second.variable_map().end()) {
        result += encode(jt->second);
        found = true;
      }
    }
    if (!found) {
      result += encode(pair.second.default_value());
    }
  }
  return result;
}

class CreatorApp : public wxApp
{
public:
  CreatorApp() : _frame{nullptr}
  {
  }

  bool OnInit() override
  {
    wxApp::OnInit();
    std::tr2::sys::path path{std::string(wxStandardPaths::Get().GetExecutablePath())};
    _frame = new CreatorFrame{path.parent_path().string(), _parameter};
    SetTopWindow(_frame);
    return true;
  };

  int OnExit() override
  {
    // Deleting _frame causes a crash. Shrug.
    return 0;
  }

  void OnInitCmdLine(wxCmdLineParser& parser) override
  {
    static const wxCmdLineEntryDesc desc[] = {
        {wxCMD_LINE_PARAM, NULL, NULL, "session file", wxCMD_LINE_VAL_STRING,
         wxCMD_LINE_PARAM_OPTIONAL},
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