// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "SinaiaApp.h"
#include "SinaiaFrame.h"


bool MyApp::OnInit()
{
    wxFrame* frame = new MyFrame(NULL,
                                 wxID_ANY,
                                 wxT("Sinaia"),
                                 wxDefaultPosition,
                                 wxSize(800, 600));
    SetTopWindow(frame);
    frame->SetIcon(wxIcon(wxT("open.xpm")));

    frame->Show();

    return true;
}
