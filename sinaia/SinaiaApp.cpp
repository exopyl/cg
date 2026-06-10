// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "SinaiaApp.h"
#include "SinaiaFrame.h"
#include "RemoteConsole.h"
#include "sample.xpm"


bool MyApp::OnInit()
{
    wxInitAllImageHandlers();
    MyFrame* frame = new MyFrame(nullptr,
                                 wxID_ANY,
                                 wxT("Sinaia"),
                                 wxDefaultPosition,
                                 wxSize(800, 600));
    SetTopWindow(frame);
    wxIcon icon;
    icon.CopyFromBitmap(wxBitmap((const char**)sample_xpm));
    frame->SetIcon(icon);

    frame->Show();

    RemoteConsole::Get().Start(7777, frame);

    return true;
}

int MyApp::OnExit()
{
    RemoteConsole::Get().Stop();
    return wxApp::OnExit();
}
