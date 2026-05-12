// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "SinaiaApp.h"
#include "SinaiaFrame.h"
#include "RemoteConsole.h"


bool MyApp::OnInit()
{
    wxInitAllImageHandlers();
    MyFrame* frame = new MyFrame(nullptr,
                                 wxID_ANY,
                                 wxT("Sinaia"),
                                 wxDefaultPosition,
                                 wxSize(800, 600));
    SetTopWindow(frame);
    frame->SetIcon(wxIcon(wxT("open.xpm")));

    frame->Show();

    RemoteConsole::Get().Start(7777, frame);

    return true;
}

int MyApp::OnExit()
{
    RemoteConsole::Get().Stop();
    return wxApp::OnExit();
}
