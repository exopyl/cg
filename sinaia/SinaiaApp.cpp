// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#include "SinaiaApp.h"
#include "SinaiaFrame.h"
#include "RemoteConsole.h"
#include "sample.xpm"


bool MyApp::OnInit()
{
    // App/vendor name: lets wxConfig::Get() derive a persistent store location
    // (registry on Windows, INI in the user data dir on Linux). Must be set
    // before the frame is built, since MyFrame's ctor loads the favorites list.
    SetVendorName(wxT("Mango3D"));
    SetAppName(wxT("Sinaia"));

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
