#ifndef TRANCE_CREATOR_THEME_H
#define TRANCE_CREATOR_THEME_H

#pragma warning(push, 0)
#include <src/trance.pb.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#pragma warning(pop)

class ThemePanel : public wxPanel {
public:
  ThemePanel(wxNotebookPage* parent, trance_pb::Session& session);

private:
  trance_pb::Session& _session;
};

#endif
