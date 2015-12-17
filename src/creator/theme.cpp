#include "theme.h"

#pragma warning(push, 0)
#include <wx/sizer.h>
#pragma warning(pop)

ThemePanel::ThemePanel(wxNotebookPage* parent, trance_pb::Session& session)
: wxPanel{parent, wxID_ANY}
, _session(session)
{
  auto sizer = new wxBoxSizer{wxVERTICAL};
  SetSizer(sizer);
}