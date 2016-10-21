#include "export.h"
#include "main.h"
#include "../common.h"
#include "../util.h"

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#pragma warning(pop)

namespace {
  const std::string WIDTH_TOOLTIP = "Width, in pixels, of the exported video.";
  const std::string HEIGHT_TOOLTIP =
      "Height, in pixels, of the exported video.";
  const std::string FPS_TOOLTIP =
      "Number of frames per second in the exported video.";
  const std::string LENGTH_TOOLTIP =
      "Length, in seconds, of the exported video.";
  const std::string THREADS_TOOLTIP =
      "Number of threads to use for rendering the video. "
      "Increase to make use of all CPU cores.";
  const std::string QUALITY_TOOLTIP =
      "Quality of the exported video. 0 is best, 4 is worst. "
      "Better-quality videos take longer to export.";

  const std::vector<std::string> EXPORT_FILE_PATTERNS = {
      "H.264 video (*.h264)|*.h264",
      "WebM video (*.webm)|*.webm",
      "JPEG frame-by-frame (*.jpg)|*.jpg",
      "PNG frame-by-frame (*.png)|*.png",
      "BMP frame-by-frame (*.bmp)|*.bmp",
  };

  const std::vector<std::string> EXPORT_FILE_EXTENSIONS = {
      "h264", "webm", "jpg", "png", "bmp",
  };
}

ExportFrame::ExportFrame(CreatorFrame* parent, trance_pb::System& system,
                         const std::string& default_path)
: wxFrame{parent, wxID_ANY, "Export video",
          wxDefaultPosition, wxDefaultSize,
          wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN}
, _system{system}
, _parent{parent}
{
  const auto& settings = system.last_export_settings();

  std::tr2::sys::path last_path = settings.path().empty() ?
      default_path + ".h264" : settings.path();
  auto patterns = EXPORT_FILE_PATTERNS;
  for (std::size_t i = 0; i < patterns.size(); ++i) {
    for (const auto& ext : EXPORT_FILE_EXTENSIONS) {
      if (ext_is(patterns[i], ext) && ext_is(last_path.string(), ext)) {
        std::swap(patterns[0], patterns[i]);
      }
    }
  }
  std::string pattern_str;
  bool first = true;
  for (const auto& p : patterns) {
    pattern_str += (first ? "" : "|") + p;
    first = false;
  }
  wxFileDialog dialog{
      parent, "Choose export file", last_path.parent_path().string(),
      (--last_path.end())->string(), pattern_str,
      wxFD_SAVE | wxFD_OVERWRITE_PROMPT};

  Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent& event)
  {
    _parent->ExportClosed();
    Destroy();
  });

  if (dialog.ShowModal() == wxID_CANCEL) {
    Close();
    return;
  }
  _path = dialog.GetPath();
  bool frame_by_frame =
      ext_is(_path, "jpg") || ext_is(_path, "png") || ext_is(_path, "bmp");

  auto panel = new wxPanel{this};
  auto sizer = new wxBoxSizer{wxVERTICAL};
  auto top = new wxBoxSizer{wxVERTICAL};
  auto bottom = new wxBoxSizer{wxHORIZONTAL};
  auto top_inner = new wxStaticBoxSizer{
      wxVERTICAL, panel, "Export settings for " + _path};
  auto quality = new wxBoxSizer{wxHORIZONTAL};

  _width = new wxSpinCtrl{panel, wxID_ANY};
  _height = new wxSpinCtrl{panel, wxID_ANY};
  _fps = new wxSpinCtrl{panel, wxID_ANY};
  _length = new wxSpinCtrl{panel, wxID_ANY};
  _threads = new wxSpinCtrl{panel, wxID_ANY};
  _quality = new wxSlider{
      panel, wxID_ANY, (int) settings.quality(), 0, 4,
      wxDefaultPosition, wxDefaultSize,
      wxSL_HORIZONTAL | wxSL_AUTOTICKS | wxSL_VALUE_LABEL};
  auto button_export = new wxButton{panel, ID_EXPORT, "Export"};
  auto button_cancel = new wxButton{panel, ID_CANCEL, "Cancel"};

  _width->SetToolTip(WIDTH_TOOLTIP);
  _height->SetToolTip(HEIGHT_TOOLTIP);
  _fps->SetToolTip(FPS_TOOLTIP);
  _length->SetToolTip(LENGTH_TOOLTIP);
  _threads->SetToolTip(THREADS_TOOLTIP);
  _quality->SetToolTip(QUALITY_TOOLTIP);

  _width->SetRange(320, 30720);
  _height->SetRange(240, 17280);
  _fps->SetRange(1, 120);
  _length->SetRange(1, 86400);
  _threads->SetRange(1, 128);

  _width->SetValue(settings.width());
  _height->SetValue(settings.height());
  _fps->SetValue(settings.fps());
  _length->SetValue(settings.length());
  _threads->SetValue(settings.threads());

  sizer->Add(top, 1, wxEXPAND, 0);
  sizer->Add(bottom, 0, wxEXPAND, 0);
  top->Add(top_inner, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  wxStaticText* label = nullptr;

  label = new wxStaticText{panel, wxID_ANY, "Width:"};
  label->SetToolTip(WIDTH_TOOLTIP);
  top_inner->Add(label, 0, wxALL, DEFAULT_BORDER);
  top_inner->Add(_width, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  label = new wxStaticText{panel, wxID_ANY, "Height:"};
  label->SetToolTip(HEIGHT_TOOLTIP);
  top_inner->Add(label, 0, wxALL, DEFAULT_BORDER);
  top_inner->Add(_height, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  label = new wxStaticText{panel, wxID_ANY, "FPS:"};
  label->SetToolTip(FPS_TOOLTIP);
  top_inner->Add(label, 0, wxALL, DEFAULT_BORDER);
  top_inner->Add(_fps, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  label = new wxStaticText{panel, wxID_ANY, "Length:"};
  label->SetToolTip(LENGTH_TOOLTIP);
  top_inner->Add(label, 0, wxALL, DEFAULT_BORDER);
  top_inner->Add(_length, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  auto qlabel = new wxStaticText{panel, wxID_ANY, "Quality:"};
  qlabel->SetToolTip(THREADS_TOOLTIP);
  top_inner->Add(qlabel, 0, wxALL, DEFAULT_BORDER);
  auto q0_label = new wxStaticText{panel, wxID_ANY, "Best"};
  quality->Add(q0_label, 0, wxALL, DEFAULT_BORDER);
  quality->Add(_quality, 1, wxALL | wxEXPAND, DEFAULT_BORDER);
  auto q4_label = new wxStaticText{panel, wxID_ANY, "Worst"};
  quality->Add(q4_label, 0, wxALL, DEFAULT_BORDER);
  top_inner->Add(quality, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  auto tlabel = new wxStaticText{panel, wxID_ANY, "Threads:"};
  tlabel->SetToolTip(QUALITY_TOOLTIP);
  top_inner->Add(tlabel, 0, wxALL, DEFAULT_BORDER);
  top_inner->Add(_threads, 0, wxALL | wxEXPAND, DEFAULT_BORDER);

  bottom->Add(button_cancel, 1, wxALL, DEFAULT_BORDER);
  bottom->Add(button_export, 1, wxALL, DEFAULT_BORDER);

  if (frame_by_frame) {
    _threads->Enable(false);
    _quality->Enable(false);
    q0_label->Enable(false);
    q4_label->Enable(false);
    qlabel->Enable(false);
    tlabel->Enable(false);
  }

  panel->SetSizer(sizer);
  SetClientSize(sizer->GetMinSize());
  Show(true);

  Bind(wxEVT_COMMAND_BUTTON_CLICKED,
       [&](wxCommandEvent&) { Export(); Close(); }, ID_EXPORT);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED,
       [&](wxCommandEvent&) { Close(); }, ID_CANCEL);
}

void ExportFrame::Export()
{
  auto& settings = *_system.mutable_last_export_settings();
  settings.set_path(_path);
  settings.set_width(_width->GetValue());
  settings.set_height(_height->GetValue());
  settings.set_fps(_fps->GetValue());
  settings.set_length(_length->GetValue());
  settings.set_quality(_quality->GetValue());
  settings.set_threads(_threads->GetValue());
  _parent->SaveSystem(false);
  _parent->ExportVideo(_path);
}