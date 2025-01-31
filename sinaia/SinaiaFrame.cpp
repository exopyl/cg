#include <wx/artprov.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>     //for opening files from OpenFile
#include <wx/dir.h>
#include <wx/dirctrl.h>
#include <wx/listctrl.h>
#include "wx/mimetype.h"
#include <wx/propgrid/propgrid.h>
#include <wx/busyinfo.h>

#include "sample.xpm"

// https://www.flaticon.com/fr/icone-gratuite/format-de-fichier-obj_8760186
#include "format_obj.xpm"
#include "format_stl.xpm"
#include "format_3ds.xpm"
#include "open.xpm"
#include "save.xpm"
#include "icons.h"

#include "SinaiaFrame.h"
#include "wxOpenGLCanvas.h"
//#include "DrawingArea.h"
#include "SettingsPanel.h"

// control ids
enum
{
    SliderPage_Reset = wxID_HIGHEST,
    SliderPage_Clear,
    SliderPage_SetValue,
    SliderPage_SetMinAndMax,
    SliderPage_SetLineSize,
    SliderPage_SetPageSize,
    SliderPage_SetTickFreq,
    SliderPage_SetThumbLen,
    SliderPage_CurValueText,
    SliderPage_ValueText,
    SliderPage_MinText,
    SliderPage_MaxText,
    SliderPage_LineSizeText,
    SliderPage_PageSizeText,
    SliderPage_TickFreqText,
    SliderPage_ThumbLenText,
    SliderPage_RadioSides,
    SliderPage_BothSides,
    SliderPage_Slider
};


BEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_ERASE_BACKGROUND(MyFrame::OnEraseBackground)
    EVT_SIZE(MyFrame::OnSize)
    EVT_MENU(ID_NotebookTabFixedWidth, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookNoCloseButton, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookCloseButton, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookCloseButtonAll, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookCloseButtonActive, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookAllowTabMove, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookAllowTabExternalMove, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookAllowTabSplit, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookScrollButtons, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookWindowList, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookArtGloss, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookArtSimple, MyFrame::OnNotebookFlag)
    EVT_MENU(ID_NotebookAlignTop, MyFrame::OnTabAlignment)
    EVT_MENU(ID_NotebookAlignBottom, MyFrame::OnTabAlignment)
    EVT_MENU(ID_CustomizeToolbar, MyFrame::OnCustomizeToolbar)
    EVT_MENU(wxID_OPEN, MyFrame::OnOpen)
    EVT_MENU(wxID_SAVE, MyFrame::OnSave)
    EVT_MENU(wxID_SAVEAS, MyFrame::OnSaveAs)
    EVT_MENU(wxID_EXIT, MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_MENU(ID_3D_FILL, MyFrame::On3DFill)
    EVT_MENU(ID_3D_WIREFRAME, MyFrame::On3DWireframe)
    EVT_MENU(ID_3D_SMOOTH, MyFrame::On3DSmooth)
    EVT_MENU(ID_3D_FLAT, MyFrame::On3DFlat)
    EVT_MENU(ID_3D_POINT, MyFrame::On3DPoint)
    EVT_MENU(ID_3D_WARNING, MyFrame::On3DWarning)
    EVT_MENU(ID_3D_LIGHTING, MyFrame::On3DLighting)

    EVT_BUTTON(ID_BUTTON_RENDERING_BGCOLOR, MyFrame::OnBgColor)

    EVT_DIRCTRL_SELECTIONCHANGED(ID_DIRCTRL, MyFrame::OnDirCtrlSelectionChanged)
    EVT_LIST_ITEM_ACTIVATED(ID_FILESCTRL, MyFrame::OnFilesCtrlListItemActivated)

    EVT_AUINOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK_MAIN, MyFrame::OnNotebookPageChanged)

    EVT_RADIOBUTTON(ID_GEOMETRY_CUBE, MyFrame::OnSelectGeometry)
    EVT_RADIOBUTTON(ID_GEOMETRY_SPHERE, MyFrame::OnSelectGeometry)
    EVT_SPINCTRL(ID_GEOMETRY_SPHERE_LAT, MyFrame::OnSelectGeometrySpinEvent)
    EVT_SPINCTRL(ID_GEOMETRY_SPHERE_LNG, MyFrame::OnSelectGeometrySpinEvent)
    EVT_RADIOBUTTON(ID_GEOMETRY_CYLINDER, MyFrame::OnSelectGeometry)
    EVT_RADIOBUTTON(ID_GEOMETRY_KLEIN_BOTTLE, MyFrame::OnSelectGeometry)
    EVT_SPINCTRL(ID_GEOMETRY_KLEIN_BOTTLE_THETA, MyFrame::OnSelectGeometrySpinEvent)
    EVT_SPINCTRL(ID_GEOMETRY_KLEIN_BOTTLE_PHI, MyFrame::OnSelectGeometrySpinEvent)

	EVT_BUTTON(ID_BUTTON_SMOOTHING_TAUBIN, MyFrame::OnButtonSmoothingTaubin)
	EVT_BUTTON(ID_BUTTON_SMOOTHING_LAPLACIAN, MyFrame::OnButtonSmoothingLaplacian)

	EVT_BUTTON(ID_BUTTON_CURVATURES_TAUBIN, MyFrame::OnButtonCurvaturesTaubin)
	EVT_BUTTON(ID_BUTTON_CURVATURES_DESBRUN, MyFrame::OnButtonCurvaturesDesbrun)

	EVT_BUTTON(ID_BUTTON_NPR_COMPUTE, MyFrame::OnButtonNPRCompute)
	EVT_COMMAND_SCROLL(SliderPage_Slider, MyFrame::OnSlider)
	EVT_BUTTON(ID_BUTTON_NPR_EXPORT, MyFrame::OnButtonNPRExport)

	EVT_UPDATE_UI(ID_NotebookTabFixedWidth, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookNoCloseButton, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookCloseButton, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookCloseButtonAll, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookCloseButtonActive, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookAllowTabMove, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookAllowTabExternalMove, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookAllowTabSplit, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookScrollButtons, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_NotebookWindowList, MyFrame::OnUpdateUI)
    EVT_MENU_RANGE(MyFrame::ID_FirstPerspective, MyFrame::ID_FirstPerspective+1000,
                   MyFrame::OnRestorePerspective)
    EVT_AUITOOLBAR_TOOL_DROPDOWN(ID_DropDownToolbarItem, MyFrame::OnDropDownToolbarItem)
    EVT_AUI_PANE_CLOSE(MyFrame::OnPaneClose)
    EVT_AUINOTEBOOK_ALLOW_DND(wxID_ANY, MyFrame::OnAllowNotebookDnD)
    EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, MyFrame::OnNotebookPageClose)
END_EVENT_TABLE()


std::vector<wxStringProperty*> g_properties;
int PropertiesSortFunction(wxPropertyGrid* propGrid, wxPGProperty* p1, wxPGProperty* p2)
{
    auto it1 = find(g_properties.begin(), g_properties.end(), p1);
    auto it2 = find(g_properties.begin(), g_properties.end(), p2);

    // If element was found
    if (it1 != g_properties.end() && it2 != g_properties.end())
    {
        int index1 = it1 - g_properties.begin();
        int index2 = it2 - g_properties.begin();
        return index1 - index2;
    }

    return -1;
    //return p1->GetBaseName().compare(p2->GetBaseName());
}

//
// drag & drop management
//
void MyFrame::OnDropFiles(wxDropFilesEvent& event)
{
    if (event.GetNumberOfFiles() > 0) {

        wxString* dropped = event.GetFiles();
        if (dropped)
        {
            for (int i = 0; i < event.GetNumberOfFiles(); i++)
            {
                wxString name = dropped[i];

                if (wxFileExists(name))
                    OpenDocument(name);
            }
        }
    }
}


//
//
//
MyFrame::MyFrame(wxWindow* parent,
                 wxWindowID id,
                 const wxString& title,
                 const wxPoint& pos,
                 const wxSize& size,
                 long style)
        : wxFrame(parent, id, title, pos, size, style)
{
    // drag & drop management
    DragAcceptFiles(true);
    Connect(wxEVT_DROP_FILES, wxDropFilesEventHandler(MyFrame::OnDropFiles), NULL, this);


    // tell wxAuiManager to manage this frame
    m_mgr.SetManagedWindow(this);

    // set frame icon
    SetIcon(wxIcon(sample_xpm));

    // set up default notebook style
    m_notebook_style = wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_TAB_EXTERNAL_MOVE | wxNO_BORDER;
    m_notebook_theme = 0;

    // create menu
    wxMenuBar* mb = new wxMenuBar;

    wxMenu* file_menu = new wxMenu;
    file_menu->Append(wxID_OPEN, _T("&Open\tCtrl+O"), _T("Open a file"));
    file_menu->Append(wxID_SAVE, _T("&Save\tCtrl+S"), _T("Save a file"));
    file_menu->Append(wxID_SAVEAS, _T("&Save As...\tF12"), _T("Save to a new file"));
    file_menu->AppendSeparator();
    file_menu->Append(wxID_EXIT, _("Exit"));


    wxMenu* new_geometry_menu = new wxMenu;
    new_geometry_menu->Append(wxID_ANY, wxT("Cube"));
    new_geometry_menu->Append(wxID_ANY, wxT("Sphere"));

    wxMenu* geometry_menu = new wxMenu;
    geometry_menu->AppendSubMenu(new_geometry_menu, wxT("New"));

    wxMenu* options_menu = new wxMenu;

    wxMenu* panel_menu = new wxMenu;
    panel_menu->AppendRadioItem(ID_NotebookArtGloss, _("Glossy Theme (Default)"));
    panel_menu->AppendRadioItem(ID_NotebookArtSimple, _("Simple Theme"));
    panel_menu->AppendSeparator();
    panel_menu->AppendRadioItem(ID_NotebookNoCloseButton, _("No Close Button"));
    panel_menu->AppendRadioItem(ID_NotebookCloseButton, _("Close Button at Right"));
    panel_menu->AppendRadioItem(ID_NotebookCloseButtonAll, _("Close Button on All Tabs"));
    panel_menu->AppendRadioItem(ID_NotebookCloseButtonActive, _("Close Button on Active Tab"));
    panel_menu->AppendSeparator();
    panel_menu->AppendRadioItem(ID_NotebookAlignTop, _("Tab Top Alignment"));
    panel_menu->AppendRadioItem(ID_NotebookAlignBottom, _("Tab Bottom Alignment"));
    panel_menu->AppendSeparator();
    panel_menu->AppendCheckItem(ID_NotebookAllowTabMove, _("Allow Tab Move"));
    panel_menu->AppendCheckItem(ID_NotebookAllowTabExternalMove, _("Allow External Tab Move"));
    panel_menu->AppendCheckItem(ID_NotebookAllowTabSplit, _("Allow Notebook Split"));
    panel_menu->AppendCheckItem(ID_NotebookScrollButtons, _("Scroll Buttons Visible"));
    panel_menu->AppendCheckItem(ID_NotebookWindowList, _("Window List Button Visible"));
    panel_menu->AppendCheckItem(ID_NotebookTabFixedWidth, _("Fixed-width Tabs"));

    options_menu->AppendSubMenu(panel_menu, wxT("3D Panel"));

    wxMenu* help_menu = new wxMenu;
    help_menu->Append(wxID_ABOUT, _("About..."));

    mb->Append(file_menu, _("File"));
    mb->Append(geometry_menu, _("Geometry"));
    mb->Append(options_menu, _("Options"));
    mb->Append(help_menu, _("Help"));

    SetMenuBar(mb);

    CreateStatusBar();
    GetStatusBar()->SetStatusText(_("Ready"));


    // min size for the frame itself isn't completely done.
    // see the end up wxAuiManager::Update() for the test
    // code. For now, just hard code a frame minimum size
    SetMinSize(wxSize(400,300));

    // prepare a few custom overflow elements for the toolbars' overflow buttons
    
    wxAuiToolBarItemArray prepend_items;
    wxAuiToolBarItemArray append_items;
    wxAuiToolBarItem item;
    item.SetKind(wxITEM_SEPARATOR);
    append_items.Add(item);
    item.SetKind(wxITEM_NORMAL);
    item.SetId(ID_CustomizeToolbar);
    item.SetLabel(_("Customize..."));
    append_items.Add(item);


    // create some toolbars

    //
    m_pToolBar2 = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxAUI_TB_HORZ_LAYOUT | wxAUI_TB_GRIPPER);
    m_pToolBar2->SetToolBitmapSize(wxSize(16,16));

    wxBitmap tb2_bmp1 = wxArtProvider::GetBitmap(wxART_QUESTION, wxART_OTHER, wxSize(16,16));
    //tb2->AddTool(wxID_OPEN, wxBitmap(open_xpm), wxNullBitmap, false, -1, -1, (wxObject *) NULL, _("Open"));
    //tb2->AddTool(wxID_SAVEAS, wxBitmap(save_xpm), wxNullBitmap, false, -1, -1, (wxObject *) NULL, _("Save"));
    //tb2->AddTool(ID_SampleItem+3, wxT("Test"), wxBitmap (open_xpm));
    //tb2->AddTool(ID_SampleItem+4, wxT("Test"), wxBitmap (save_xpm));
    wxBitmap tb2_open = wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_OTHER, wxSize(16,16));
    wxBitmap tb2_save = wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_OTHER, wxSize(16,16));
    wxBitmap tb2_saveas = wxArtProvider::GetBitmap(wxART_FILE_SAVE_AS, wxART_OTHER, wxSize(16,16));
    m_pToolBar2->AddTool(wxID_OPEN, wxT("Test"), tb2_open);
    //m_pToolBar2->AddTool(wxID_SAVE, wxT("Test"), tb2_save);
    m_pToolBar2->AddTool(wxID_SAVEAS, wxT("Test"), tb2_saveas);
    m_pToolBar2->AddTool(ID_3D_FILL, wxT("Test"), wxBitmap (fill_xpm));
    m_pToolBar2->AddTool(ID_3D_WIREFRAME, wxT("Test"), wxBitmap (wireframe_xpm));
    m_pToolBar2->AddTool(ID_3D_POINT, wxT("Test"), wxBitmap (cloud_xpm));
    m_pToolBar2->AddSeparator();
    m_pToolBar2->AddTool(ID_3D_WARNING, wxT("Warning"), wxBitmap(warning_xpm));
    m_pToolBar2->AddSeparator();
    m_pToolBar2->AddTool(ID_3D_SMOOTH, wxT("Test"), wxBitmap (smooth_xpm));
    m_pToolBar2->AddTool(ID_3D_FLAT, wxT("Test"), wxBitmap (flat_xpm));
    m_pToolBar2->AddSeparator();
    m_pToolBar2->AddTool(ID_3D_LIGHTING, wxT("Test"), wxBitmap (light_xpm));
    //m_pToolBar2->SetCustomOverflowItems(prepend_items, append_items);
    m_pToolBar2->Realize();

/*
    wxAuiToolBar* tb3 = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                         wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_OVERFLOW);
    tb3->SetToolBitmapSize(wxSize(16,16));
    wxBitmap tb3_bmp1 = wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16,16));
    tb3->AddTool(ID_SampleItem+16, wxT("Test2"), tb3_bmp1);
    tb3->AddTool(ID_SampleItem+17, wxT("Test"), tb3_bmp1);
    tb3->AddTool(ID_SampleItem+18, wxT("Test"), tb3_bmp1);
    tb3->AddTool(ID_SampleItem+19, wxT("Test"), tb3_bmp1);
    tb3->AddSeparator();
    tb3->AddTool(ID_SampleItem+20, wxT("Test"), tb3_bmp1);
    tb3->AddTool(ID_SampleItem+21, wxT("Test"), tb3_bmp1);
    tb3->SetCustomOverflowItems(prepend_items, append_items);
    tb3->Realize();
*/

/*
    wxAuiToolBar* tb4 = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                         wxAUI_TB_DEFAULT_STYLE | 
                                         wxAUI_TB_OVERFLOW |
                                         wxAUI_TB_TEXT |
                                         wxAUI_TB_HORZ_TEXT);
    tb4->SetToolBitmapSize(wxSize(16,16));
    wxBitmap tb4_bmp1 = wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16,16));
    tb4->AddTool(ID_DropDownToolbarItem, wxT("Item 1"), tb4_bmp1);
    tb4->AddTool(ID_SampleItem+23, wxT("Item 2"), tb4_bmp1);
    tb4->AddTool(ID_SampleItem+24, wxT("Item 3"), tb4_bmp1);
    tb4->AddTool(ID_SampleItem+25, wxT("Item 4"), tb4_bmp1);
    tb4->AddSeparator();
    tb4->AddTool(ID_SampleItem+26, wxT("Item 5"), tb4_bmp1);
    tb4->AddTool(ID_SampleItem+27, wxT("Item 6"), tb4_bmp1);
    tb4->AddTool(ID_SampleItem+28, wxT("Item 7"), tb4_bmp1);
    tb4->AddTool(ID_SampleItem+29, wxT("Item 8"), tb4_bmp1);
    tb4->SetToolDropDown(ID_DropDownToolbarItem, true);
    tb4->SetCustomOverflowItems(prepend_items, append_items);
    tb4->Realize();
*/

/*
    wxAuiToolBar* tb5 = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                         wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_OVERFLOW | wxAUI_TB_VERTICAL);
    tb5->SetToolBitmapSize(wxSize(48,48));
    tb5->AddTool(ID_SampleItem+30, wxT("Test"), wxArtProvider::GetBitmap(wxART_ERROR));
    tb5->AddSeparator();
    tb5->AddTool(ID_SampleItem+31, wxT("Test"), wxArtProvider::GetBitmap(wxART_QUESTION));
    tb5->AddTool(ID_SampleItem+32, wxT("Test"), wxArtProvider::GetBitmap(wxART_INFORMATION));
    tb5->AddTool(ID_SampleItem+33, wxT("Test"), wxArtProvider::GetBitmap(wxART_WARNING));
    tb5->AddTool(ID_SampleItem+34, wxT("Test"), wxArtProvider::GetBitmap(wxART_MISSING_IMAGE));
    tb5->SetCustomOverflowItems(prepend_items, append_items);
    tb5->Realize();
*/

    // add a bunch of panes
/*
    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test1")).Caption(wxT("Pane Caption")).
                  Top());

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test2")).Caption(wxT("Client Size Reporter")).
                  Bottom().Position(1).
                  CloseButton(true).MaximizeButton(true));

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test3")).Caption(wxT("Client Size Reporter")).
                  Bottom().
                  CloseButton(true).MaximizeButton(true));

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test4")).Caption(wxT("Pane Caption")).
                  Left());

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test5")).Caption(wxT("No Close Button")).
                  Right().CloseButton(false));

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test6")).Caption(wxT("Client Size Reporter")).
                  Right().Row(1).
                  CloseButton(true).MaximizeButton(true));

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test7")).Caption(wxT("Client Size Reporter")).
                  Left().Layer(1).
                  CloseButton(true).MaximizeButton(true));
*/


    m_propertiesGrid = new wxPropertyGrid(
        this, // parent
        ID_PROPERTIESCTRL, // id
        wxDefaultPosition, // position
        wxDefaultSize, // size
        // Here are just some of the supported window styles
        wxPG_AUTO_SORT | // Automatic sorting after items added
        wxPG_SPLITTER_AUTO_CENTER | // Automatically center splitter until user manually adjusts it
        // Default style
        wxPG_DEFAULT_STYLE);

    m_propertiesGrid->SetSortFunction(PropertiesSortFunction);
    g_properties = {
        new wxStringProperty("Meshes"),
        new wxStringProperty("Vertices"),
        new wxStringProperty("Faces") ,
        new wxStringProperty("Triangle mesh"),
        new wxStringProperty("Borders"),
        new wxStringProperty("Non manifold edges"),
    };
    for (auto& property : g_properties)
    {
        m_propertiesGrid->Append(property);
        property->ChangeFlag(wxPG_PROP_READONLY, true);
    }

    m_mgr.AddPane(m_propertiesGrid, wxRIGHT, wxT("Properties"));


    m_hierarchyMeshes = CreateHierarchyMeshesTreeCtrl();
    m_mgr.AddPane(m_hierarchyMeshes, wxRIGHT, wxT("Meshes"));


    m_dcDirectory = new wxGenericDirCtrl(this, ID_DIRCTRL, wxT(""), wxDefaultPosition, wxSize(142, 120), wxSIMPLE_BORDER | wxDIRCTRL_DIR_ONLY, wxT("All files (*.*)|*.*"), 0);
    m_mgr.AddPane(m_dcDirectory, wxLEFT, wxT("Explorer"));

    /*m_mgr.AddPane(m_dcDirectory, wxAuiPaneInfo().
        Name(wxT("test7")).Caption(wxT("Dir Tree")).
        Bottom().Layer(1).Position(1).
        CloseButton(true).MaximizeButton(true));
 */

    m_filesCtrl = new wxListCtrl(this, ID_FILESCTRL, wxDefaultPosition, wxDefaultSize, wxLC_SINGLE_SEL | wxLC_ICON);



    /*
    wxMimeTypesManager mtmgr;
    wxFileType* pFileType= mtmgr.GetFileTypeFromExtension(wxT("doc"));
    wxIconLocation iconLoc;
    bool bRes = pFileType->GetIcon(&iconLoc);
    const wxIcon iconTxt(iconLoc);
    wxSize size2 = iconTxt.GetSize();
    */
    wxIcon iconObj(format_obj);
    wxIcon iconStl(format_stl);
    wxIcon icon3ds(format_3ds);

    wxImageList* pImageList = new wxImageList(32, 32, false, 1);
    int toto = pImageList->GetImageCount();
    int indexObj = pImageList->Add(iconObj);
    int indexStl = pImageList->Add(iconStl);
    int index3ds = pImageList->Add(icon3ds);
    pImageList->GetImageCount();
    m_filesCtrl->SetImageList(pImageList, wxIMAGE_LIST_NORMAL);
    m_mgr.AddPane(m_filesCtrl, wxBOTTOM, wxT("Files"));

    /*
    m_mgr.AddPane(CreateNotebookActions(), wxAuiPaneInfo().
        Name(wxT("test8")).Caption(wxT("Notebook Actions")).
        Bottom().Layer(1).Position(1).
        CloseButton(true).MaximizeButton(true));
 
    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test9")).Caption(wxT("Min Size 200x100")).
                  BestSize(wxSize(200,100)).MinSize(wxSize(200,100)).
                  Bottom().Layer(1).Position(1).
                  CloseButton(true).MaximizeButton(true));
    */

    m_pWndLogging = CreateTextCtrl(wxT("Logging...\n"));
    m_mgr.AddPane(m_pWndLogging, wxBOTTOM, wxT("Logging Window"));
    /*
    m_mgr.AddPane(m_pWndLogging, wxAuiPaneInfo().
                  Name(wxT("test10")).Caption(wxT("Logging Window")).
                  Bottom().Layer(1).Position(1));
                  */
    
    /*
    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().
                  Name(wxT("test11")).Caption(wxT("Fixed Pane")).
                  Bottom().Layer(1).Position(2).Fixed());


    m_mgr.AddPane(new SettingsPanel(this,this), wxAuiPaneInfo().
                  Name(wxT("settings")).Caption(wxT("Dock Manager Settings")).
                  Dockable(false).Float().Hide());
*/
    // create some center panes
/*
    m_mgr.AddPane(CreateGrid(), wxAuiPaneInfo().Name(wxT("grid_content")).
                  CenterPane().Hide());

    m_mgr.AddPane(CreateSizeReportCtrl(), wxAuiPaneInfo().Name(wxT("sizereport_content")).
                  CenterPane().Hide());

    m_mgr.AddPane(CreateTextCtrl(), wxAuiPaneInfo().Name(wxT("text_content")).
                  CenterPane().Hide());

    m_mgr.AddPane(CreateHTMLCtrl(), wxAuiPaneInfo().Name(wxT("html_content")).
                  CenterPane().Hide());
*/




    m_mgr.AddPane(CreateNotebook(), wxAuiPaneInfo().Name(wxT("notebook_content")).CenterPane().PaneBorder(false));

    // add the toolbars to the manager

    m_mgr.AddPane(m_pToolBar2, wxAuiPaneInfo().
                  Name(wxT("tb2")).Caption(wxT("Toolbar 2")).
                  ToolbarPane().Top().Row(1).
                  LeftDockable(false).RightDockable(false));

/*
    m_mgr.AddPane(tb3, wxAuiPaneInfo().
                  Name(wxT("tb3")).Caption(wxT("Toolbar 3")).
                  ToolbarPane().Top().Row(1).Position(1).
                  LeftDockable(false).RightDockable(false));

    m_mgr.AddPane(tb4, wxAuiPaneInfo().
                  Name(wxT("tb4")).Caption(wxT("Sample Bookmark Toolbar")).
                  ToolbarPane().Top().Row(2).
                  LeftDockable(false).RightDockable(false));
*/
/*
    m_mgr.AddPane(tb5, wxAuiPaneInfo().
                  Name(wxT("tb5")).Caption(wxT("Sample Vertical Toolbar")).
                  ToolbarPane().Left().
                  GripperTop().
                  TopDockable(false).BottomDockable(false));

    m_mgr.AddPane(new wxButton(this, wxID_ANY, _("Test Button")),
                  wxAuiPaneInfo().Name(wxT("tb6")).
                  ToolbarPane().Top().Row(2).Position(1).
                  LeftDockable(false).RightDockable(false));
*/
    // make some default perspectives

    wxString perspective_all = m_mgr.SavePerspective();

    /*
    int i, count;
    wxAuiPaneInfoArray& all_panes = m_mgr.GetAllPanes();
    for (i = 0, count = all_panes.GetCount(); i < count; ++i)
        if (!all_panes.Item(i).IsToolbar())
            all_panes.Item(i).Hide();
    m_mgr.GetPane(wxT("tb1")).Hide();
    m_mgr.GetPane(wxT("tb6")).Hide();
    m_mgr.GetPane(wxT("test8")).Show().Left().Layer(0).Row(0).Position(0);
    m_mgr.GetPane(wxT("test10")).Show().Bottom().Layer(0).Row(0).Position(0);
    m_mgr.GetPane(wxT("notebook_content")).Show();
    wxString perspective_default = m_mgr.SavePerspective();

    m_perspectives.Add(perspective_default);
    m_perspectives.Add(perspective_all);
        */
    // "commit" all changes made to wxAuiManager
    m_mgr.Update();



    auto glCanvas = new wxGLCanvas(this);
    auto glContext = new wxGLContext(glCanvas);
    wglMakeCurrent(glCanvas->GetHDC(), glContext->GetGLRC());

    // 
    int version = gladLoaderLoadWGL(glCanvas->GetHDC());
    int versionGL = gladLoaderLoadGL();
    if (version == 0) {
        printf("Failed to initialize OpenGL context\n");
        return;
    }

    float fversion = CapabilitiesManager::getInstance()->GetVersion();
    std::string str;
    CapabilitiesManager::getInstance()->GetCardInfo(str);
    Log(str);
    ShadersManager::getInstance()->Initialize();

}

MyFrame::~MyFrame()
{
    m_mgr.UnInit();
}

wxAuiDockArt* MyFrame::GetDockArt()
{
    return m_mgr.GetArtProvider();
}

void MyFrame::DoUpdate()
{
    m_mgr.Update();
}

void MyFrame::OnEraseBackground(wxEraseEvent& event)
{
    event.Skip();
}

void MyFrame::OnSize(wxSizeEvent& event)
{
    event.Skip();
}

void MyFrame::OnCustomizeToolbar(wxCommandEvent& WXUNUSED(evt))
{
    wxMessageBox(_("Customize Toolbar clicked"));
}

void MyFrame::OnNotebookFlag(wxCommandEvent& event)
{
    int id = event.GetId();

    if (id == ID_NotebookNoCloseButton ||
        id == ID_NotebookCloseButton ||
        id == ID_NotebookCloseButtonAll ||
        id == ID_NotebookCloseButtonActive)
    {
        m_notebook_style &= ~(wxAUI_NB_CLOSE_BUTTON |
                              wxAUI_NB_CLOSE_ON_ACTIVE_TAB |
                              wxAUI_NB_CLOSE_ON_ALL_TABS);

        switch (id)
        {
            case ID_NotebookNoCloseButton: break;
            case ID_NotebookCloseButton: m_notebook_style |= wxAUI_NB_CLOSE_BUTTON; break;
            case ID_NotebookCloseButtonAll: m_notebook_style |= wxAUI_NB_CLOSE_ON_ALL_TABS; break;
            case ID_NotebookCloseButtonActive: m_notebook_style |= wxAUI_NB_CLOSE_ON_ACTIVE_TAB; break;
        }
    }

    if (id == ID_NotebookAllowTabMove)
    {
        m_notebook_style ^= wxAUI_NB_TAB_MOVE;
    }
    if (id == ID_NotebookAllowTabExternalMove)
    {
        m_notebook_style ^= wxAUI_NB_TAB_EXTERNAL_MOVE;
    }
     else if (id == ID_NotebookAllowTabSplit)
    {
        m_notebook_style ^= wxAUI_NB_TAB_SPLIT;
    }
     else if (id == ID_NotebookWindowList)
    {
        m_notebook_style ^= wxAUI_NB_WINDOWLIST_BUTTON;
    }
     else if (id == ID_NotebookScrollButtons)
    {
        m_notebook_style ^= wxAUI_NB_SCROLL_BUTTONS;
    }
     else if (id == ID_NotebookTabFixedWidth)
    {
        m_notebook_style ^= wxAUI_NB_TAB_FIXED_WIDTH;
    }


    size_t i, count;
    wxAuiPaneInfoArray& all_panes = m_mgr.GetAllPanes();
    for (i = 0, count = all_panes.GetCount(); i < count; ++i)
    {
        wxAuiPaneInfo& pane = all_panes.Item(i);
        if (pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
        {
            wxAuiNotebook* nb = (wxAuiNotebook*)pane.window;

            if (id == ID_NotebookArtGloss)
            {
                nb->SetArtProvider(new wxAuiDefaultTabArt);
                m_notebook_theme = 0;
            }
             else if (id == ID_NotebookArtSimple)
            {
                nb->SetArtProvider(new wxAuiSimpleTabArt);
                m_notebook_theme = 1;
            }


            nb->SetWindowStyleFlag(m_notebook_style);
            nb->Refresh();
        }
    }


}

//
void MyFrame::OnUpdateUI(wxUpdateUIEvent& event)
{
    unsigned int flags = m_mgr.GetFlags();

    switch (event.GetId())
    {
        case ID_NotebookNoCloseButton:
            event.Check((m_notebook_style & (wxAUI_NB_CLOSE_BUTTON|wxAUI_NB_CLOSE_ON_ALL_TABS|wxAUI_NB_CLOSE_ON_ACTIVE_TAB)) != 0);
            break;
        case ID_NotebookCloseButton:
            event.Check((m_notebook_style & wxAUI_NB_CLOSE_BUTTON) != 0);
            break;
        case ID_NotebookCloseButtonAll:
            event.Check((m_notebook_style & wxAUI_NB_CLOSE_ON_ALL_TABS) != 0);
            break;
        case ID_NotebookCloseButtonActive:
            event.Check((m_notebook_style & wxAUI_NB_CLOSE_ON_ACTIVE_TAB) != 0);
            break;
        case ID_NotebookAllowTabSplit:
            event.Check((m_notebook_style & wxAUI_NB_TAB_SPLIT) != 0);
            break;
        case ID_NotebookAllowTabMove:
            event.Check((m_notebook_style & wxAUI_NB_TAB_MOVE) != 0);
            break;
        case ID_NotebookAllowTabExternalMove:
            event.Check((m_notebook_style & wxAUI_NB_TAB_EXTERNAL_MOVE) != 0);
            break;
        case ID_NotebookScrollButtons:
            event.Check((m_notebook_style & wxAUI_NB_SCROLL_BUTTONS) != 0);
            break;
        case ID_NotebookWindowList:
            event.Check((m_notebook_style & wxAUI_NB_WINDOWLIST_BUTTON) != 0);
            break;
        case ID_NotebookTabFixedWidth:
            event.Check((m_notebook_style & wxAUI_NB_TAB_FIXED_WIDTH) != 0);
            break;
        case ID_NotebookArtGloss:
            event.Check(m_notebook_style == 0);
            break;
        case ID_NotebookArtSimple:
            event.Check(m_notebook_style == 1);
            break;

    }
}

void MyFrame::OnPaneClose(wxAuiManagerEvent& evt)
{
    if (evt.pane->name == wxT("test10"))
    {
        int res = wxMessageBox(wxT("Are you sure you want to close/hide this pane?"),
                               wxT("wxAUI"),
                               wxYES_NO,
                               this);
        if (res != wxYES)
            evt.Veto();
    }
}

void MyFrame::OnRestorePerspective(wxCommandEvent& evt)
{
    m_mgr.LoadPerspective(m_perspectives.Item(evt.GetId() - ID_FirstPerspective));
}

void MyFrame::OnNotebookPageClose(wxAuiNotebookEvent& evt)
{
    wxAuiNotebook* ctrl = (wxAuiNotebook*)evt.GetEventObject();
    if (ctrl->GetPage(evt.GetSelection())->IsKindOf(CLASSINFO(wxHtmlWindow)))
    {
        int res = wxMessageBox(wxT("Are you sure you want to close/hide this notebook page?"),
                       wxT("wxAUI"),
                       wxYES_NO,
                       this);
        if (res != wxYES)
            evt.Veto();
    }
}

void MyFrame::OnAllowNotebookDnD(wxAuiNotebookEvent& evt)
{
    // for the purpose of this test application, explicitly
    // allow all noteboko drag and drop events
    evt.Allow();
}

wxPoint MyFrame::GetStartPosition()
{
    static int x = 0;
    x += 20;
    wxPoint pt = ClientToScreen(wxPoint(0,0));
    return wxPoint(pt.x + x, pt.y + x);
}

void MyFrame::OnDropDownToolbarItem(wxAuiToolBarEvent& evt)
{
    if (evt.IsDropDownClicked())
    {
        wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(evt.GetEventObject());
        
        tb->SetToolSticky(evt.GetId(), true);
        
        // create the popup menu
        wxMenu menuPopup;
        
        wxBitmap bmp = wxArtProvider::GetBitmap(wxART_QUESTION, wxART_OTHER, wxSize(16,16));
        
        wxMenuItem* m1 =  new wxMenuItem(&menuPopup, 101, _("Drop Down Item 1"));
        m1->SetBitmap(bmp);
        menuPopup.Append(m1);
        
        wxMenuItem* m2 =  new wxMenuItem(&menuPopup, 101, _("Drop Down Item 2"));
        m2->SetBitmap(bmp);
        menuPopup.Append(m2);
        
        wxMenuItem* m3 =  new wxMenuItem(&menuPopup, 101, _("Drop Down Item 3"));
        m3->SetBitmap(bmp);
        menuPopup.Append(m3);
        
        wxMenuItem* m4 =  new wxMenuItem(&menuPopup, 101, _("Drop Down Item 4"));
        m4->SetBitmap(bmp);
        menuPopup.Append(m4);
        
        // line up our menu with the button
        wxRect rect = tb->GetToolRect(evt.GetId());
        wxPoint pt = tb->ClientToScreen(rect.GetBottomLeft());
        pt = ScreenToClient(pt);
        

        PopupMenu(&menuPopup, pt);


        // make sure the button is "un-stuck"
        tb->SetToolSticky(evt.GetId(), false);
    }
}

void MyFrame::OnDirCtrlSelectionChanged(wxTreeEvent& WXUNUSED(event))
{
    const wxString& path = m_dcDirectory->GetPath();
    

    wxArrayString files;
    const size_t n = wxDir::GetAllFiles(path, &files, wxEmptyString, wxDIR_FILES);
    int index = 0;
    m_filesCtrl->ClearAll();
    for (auto& file : files)
    {
        wxFileName filename(file);// ::GetName()
        wxString ext = filename.GetExt();
        if (ext == _T("obj"))
            m_filesCtrl->InsertItem(index++, filename.GetName()+_T(".")+ filename.GetExt(), 0);
        else if (ext == _T("stl"))
            m_filesCtrl->InsertItem(index++, filename.GetName() + _T(".") + filename.GetExt(), 1);
        else if (ext == _T("3ds"))
            m_filesCtrl->InsertItem(index++, filename.GetName() + _T(".") + filename.GetExt(), 2);
    }
}

void MyFrame::OnFilesCtrlListItemActivated(wxListEvent& WXUNUSED(event))
{
    long item = -1;
    for (;;)
    {
        item = m_filesCtrl->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (item == -1)
            break;

        wxListItem info;
        info.SetId(item);
        bool bRes = m_filesCtrl->GetItem(info);
        wxString strFilename = m_dcDirectory->GetPath() + "\\" + info.m_text;
        Log(strFilename);
        OpenDocument(strFilename);
    }
}

void MyFrame::OnNotebookPageChanged(wxAuiNotebookEvent& event)
{
    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (pGLCanvas)
    {
        pGLCanvas->ResetProjectionMode();
        //pGLCanvas->DrawGL();
    }
    UpdatePropertiesGrid();
}

void MyFrame::OnTabAlignment(wxCommandEvent &evt)
{
   size_t i, count;
    wxAuiPaneInfoArray& all_panes = m_mgr.GetAllPanes();
    for (i = 0, count = all_panes.GetCount(); i < count; ++i)
    {
        wxAuiPaneInfo& pane = all_panes.Item(i);
        if (pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
        {
            wxAuiNotebook* nb = (wxAuiNotebook*)pane.window;

            if (evt.GetId() == ID_NotebookAlignTop)
                nb->SetWindowStyleFlag(nb->GetWindowStyleFlag()^wxAUI_NB_BOTTOM|wxAUI_NB_TOP);
           else if (evt.GetId() == ID_NotebookAlignBottom)
               nb->SetWindowStyleFlag(nb->GetWindowStyleFlag()^wxAUI_NB_TOP|wxAUI_NB_BOTTOM);
            nb->Refresh();
        }
    }
}

//
//
//
void MyFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
{
    wxFileDialog fd(this);

    if(fd.ShowModal() == wxID_OK)
    {
	    wxString strFilename = fd.GetPath();
        OpenDocument(strFilename);
    }
}

void MyFrame::OpenDocument(const wxString& strFilename)
{
    wxBusyCursor busyCursor;
    wxWindowDisabler disabler;
    wxBusyInfo busyInfo(_("Loading file, wait please..."));


    *m_pWndLogging << _T("Opening ") << strFilename << _T("\n");

    int args[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 24, 0 };
    MyGLCanvas* pGLCanvas = new MyGLCanvas(m_pCtrl, m_pWndLogging, args);
    pGLCanvas->LoadModel(strFilename);

    wxArrayString aString = wxSplit(strFilename, '\\');
    wxString title = aString.back();
    m_pCtrl->AddPage(pGLCanvas, title, true);

    // update properties grid
    UpdatePropertiesGrid();
   

    wxAuiToolBarItem* p;
    bool b;

    p = m_pToolBar2->FindTool(ID_3D_FILL);
    b = pGLCanvas->GetFill();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_WIREFRAME);
    b = pGLCanvas->GetWireframe();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_POINT);
    b = pGLCanvas->GetPoint();
    p->SetSticky(b);

    m_pToolBar2->Refresh();
}

void MyFrame::UpdatePropertiesGrid()
{
    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (!pGLCanvas)
        return;

    auto pObject = pGLCanvas->GetObject3D();
    m_propertiesGrid->ChangePropertyValue("Vertices", wxVariant(static_cast<int>(pObject->GetNVertices())));
    m_propertiesGrid->ChangePropertyValue("Faces", wxVariant(static_cast<int>(pObject->GetNFaces())));
    m_propertiesGrid->ChangePropertyValue("Meshes", wxVariant(static_cast<int>(pObject->GetNMeshes())));
    const bool bIsTriangleMesh = pObject->IsTriangleMesh();
    m_propertiesGrid->ChangePropertyValue("Triangle mesh", wxVariant(bIsTriangleMesh? wxString("True") : wxString("False")));

    m_propertiesGrid->ChangePropertyValue("Borders", wxVariant(static_cast<int>(pGLCanvas->GetNBorders())));
    m_propertiesGrid->ChangePropertyValue("Non manifold edges", wxVariant(static_cast<int>(pGLCanvas->GetNNonManifoldEdges())));


    char n[64];
    if (m_hierarchyMeshes)
    {
        m_hierarchyMeshes->DeleteAllItems();

        wxTreeItemId root = m_hierarchyMeshes->AddRoot(wxT("Meshes"), 0);
        for (auto& mesh : pObject->GetMeshes())
        {
            wxTreeItemId currentItem = m_hierarchyMeshes->AppendItem(root, wxString(mesh->m_name), 0);
            sprintf(n, "%d vertices\n", mesh->m_nVertices);
            m_hierarchyMeshes->AppendItem(currentItem, wxString(n), 1);
            sprintf(n, "%d faces\n", mesh->m_nFaces);
            m_hierarchyMeshes->AppendItem(currentItem, wxString(n), 1);
        }

        m_hierarchyMeshes->Expand(root);
    }
}

void MyFrame::Log(const wxString& text) const
{
    *m_pWndLogging << text << _T("\n");
}

void MyFrame::OnSave(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("OnSave\n");
}

void MyFrame::OnSaveAs(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (!pGLCanvas)
		return;

	wxFileDialog fd(this);

    if(fd.ShowModal() == wxID_OK)
    {
	    wxString strFilename = fd.GetPath();
	    *m_pWndLogging << _T("Saving as ") << strFilename << _T("\n");
	    
	    pGLCanvas->SaveModel(strFilename);
    }
}

void MyFrame::OnExit(wxCommandEvent& WXUNUSED(event))
{
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("OnAbout\n");
	wxMessageBox(_("Sinaia\nAn interface for computational geometry\n(c) Copyright 2023, lsd"), _("About Sinaia"), wxOK, this);
}

//
//
//
void MyFrame::On3DFill(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Fill\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		pGLCanvas->ChangeFill ();
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *p = m_pToolBar2->FindTool(ID_3D_FILL);
		bool b = pGLCanvas->GetFill ();
		p->SetSticky (b);
	}
}

//
//
//
void MyFrame::On3DWireframe(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Wireframe\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		pGLCanvas->ChangeWireframe ();
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *p = m_pToolBar2->FindTool(ID_3D_WIREFRAME);
		bool b = pGLCanvas->GetWireframe ();
		p->SetSticky (b);
	}
}

//
//
//
void MyFrame::On3DPoint(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Point\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		pGLCanvas->ChangePoint ();
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *p = m_pToolBar2->FindTool(ID_3D_POINT);
		bool b = pGLCanvas->GetPoint ();
		p->SetSticky (b);
	}
}

//
//
//
void MyFrame::On3DWarning(wxCommandEvent& WXUNUSED(event))
{
    *m_pWndLogging << _T("Warning\n");
    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (pGLCanvas)
    {
        pGLCanvas->ChangeWarning();
        pGLCanvas->Refresh();

        wxAuiToolBarItem* p = m_pToolBar2->FindTool(ID_3D_WARNING);
        bool b = pGLCanvas->GetWarning();
        p->SetSticky(b);
    }
}

//
//
//
void MyFrame::On3DSmooth(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Smooth\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		bool bSmooth = pGLCanvas->GetSmooth  ();
		if (bSmooth == true)
			return;

		pGLCanvas->SetSmooth (!bSmooth);
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *pSmooth = m_pToolBar2->FindTool(ID_3D_SMOOTH);
		wxAuiToolBarItem *pFlat = m_pToolBar2->FindTool(ID_3D_FLAT);
		bool b = pGLCanvas->GetSmooth ();
		if (b == true)
		{
			pSmooth->SetSticky (true);
			pFlat->SetSticky (false);
		}
		else
		{
			pSmooth->SetSticky (false);
			pFlat->SetSticky (true);
		}
	}
}

void MyFrame::On3DFlat(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Flat\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		bool bSmooth = pGLCanvas->GetSmooth  ();
		if (bSmooth == false)
			return;

		pGLCanvas->SetSmooth (!bSmooth);
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *pSmooth = m_pToolBar2->FindTool(ID_3D_SMOOTH);
		wxAuiToolBarItem *pFlat = m_pToolBar2->FindTool(ID_3D_FLAT);
		bool b = pGLCanvas->GetSmooth ();
		if (b == true)
		{
			pSmooth->SetSticky (true);
			pFlat->SetSticky (false);
		}
		else
		{
			pSmooth->SetSticky (false);
			pFlat->SetSticky (true);
		}
	}
}

//
// Light
//
void MyFrame::On3DLighting(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Lighting\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		bool bLighting = pGLCanvas->GetLighting ();
		pGLCanvas->SetLighting (!bLighting);
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *p = m_pToolBar2->FindTool(ID_3D_LIGHTING);
		bool b = pGLCanvas->GetLighting ();
		p->SetSticky (b);
	}
}


//
// Rendering
//
void MyFrame::OnBgColor(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Change background color\n");

	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (!pGLCanvas)
		return;

	unsigned char r, g, b;
	pGLCanvas->GetBackgroundColor (&r, &g, &b);
	wxColour colInit (r, g, b);
	wxColour colNew = ::wxGetColourFromUser (this, colInit);
	if (colNew.IsOk ())
		pGLCanvas->SetBackgroundColor (colNew.Red(), colNew.Green(), colNew.Blue());
}

//
// Geometry
//
void MyFrame::UpdateGeometry (void)
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (!pGLCanvas)
		return;

	// get the geometry
	Mesh *geometry = NULL;
	for (int i=0; i<m_nRadioGeometries; i++)
		if (m_pRadioGeometries[i]->GetValue())
		{
			switch (i)
			{
			case 0:
				*m_pWndLogging << _T("Cube\n");

				geometry = CreateCube ();
				break;
			case 1:
				*m_pWndLogging << _T("Sphere\n");
				{
					int nLat = m_pGeometrySphereLat->GetValue ();
					int nLng = m_pGeometrySphereLng->GetValue ();
					geometry = new ParametricSphere (nLat, nLng);
				}
				break;
			case 2:
				*m_pWndLogging << _T("Cylinder\n");
				geometry = NULL;
				break;
			case 3:
				*m_pWndLogging << _T("Klein bottle\n");
				{
					int ThetaResolution = m_pGeometryKleinBottleTheta->GetValue ();
					int PhiResolution = m_pGeometryKleinBottlePhi->GetValue ();
					geometry= CreateKleinBottle (ThetaResolution, PhiResolution);
				}
				break;
			default:
				break;
			}
			break;
		}
	if (geometry)
	{
		printf ("%d %d\n", geometry->m_nVertices, geometry->m_nFaces);
		geometry->ComputeNormals ();
		pGLCanvas->SetMesh (geometry);
	}
}

void MyFrame::OnSelectGeometrySpinEvent (wxSpinEvent & ev)
{
	UpdateGeometry ();
}

void MyFrame::OnSelectGeometry (wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Select a geometry : ");
	UpdateGeometry ();
}

wxTextCtrl* MyFrame::CreateTextCtrl(const wxString& ctrl_text)
{
    static int n = 0;

    wxString text;
    if (ctrl_text.Length() > 0)
        text = ctrl_text;
    else
        text.Printf(wxT("This is text box %d"), ++n);

    return new wxTextCtrl(this,wxID_ANY, text,
                          wxPoint(0,0), wxSize(150,90),
                          wxNO_BORDER | wxTE_MULTILINE | wxTE_READONLY);
}


wxGrid* MyFrame::CreateGrid()
{
    wxGrid* grid = new wxGrid(this, wxID_ANY,
                              wxPoint(0,0),
                              wxSize(150,250),
                              wxNO_BORDER | wxWANTS_CHARS);
    grid->CreateGrid(50, 20);
    return grid;
}

wxTreeCtrl* MyFrame::CreateHierarchyMeshesTreeCtrl()
{
    wxTreeCtrl* tree = new wxTreeCtrl(this, wxID_ANY,
                                      wxPoint(0,0), wxSize(160,250),
                                      wxTR_DEFAULT_STYLE | wxNO_BORDER);

    wxImageList* imglist = new wxImageList(16, 16, true, 2);
    imglist->Add(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, wxSize(16,16)));
    imglist->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16,16)));
    tree->AssignImageList(imglist);

    return tree;
}

wxSizeReportCtrl* MyFrame::CreateSizeReportCtrl(int width, int height)
{
    wxSizeReportCtrl* ctrl = new wxSizeReportCtrl(this, wxID_ANY,
                                   wxDefaultPosition,
                                   wxSize(width, height), &m_mgr);
    return ctrl;
}

wxHtmlWindow* MyFrame::CreateHTMLCtrl(wxWindow* parent)
{
    if (!parent)
        parent = this;

    wxHtmlWindow* ctrl = new wxHtmlWindow(parent, wxID_ANY,
					  wxDefaultPosition,
					  wxSize(400,300));
    ctrl->SetPage(GetIntroText());
    return ctrl;
}

//
// Notebook Actions
//
wxAuiNotebook* MyFrame::CreateNotebookActions(void)
{
   // create the notebook off-window to avoid flicker
   wxSize client_size = GetClientSize();

   m_pNotebookActions = new wxAuiNotebook(this, wxID_ANY,
					  wxPoint(client_size.x, client_size.y),
					  wxSize(430,200),
					  m_notebook_style	);

   wxBitmap page_bmp = wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16,16));
   
   //m_pNotebookActions->AddPage(CreateHTMLCtrl(m_pNotebookActions), wxT("Welcome to wxAUI") , false, page_bmp);

   // Rendering
   wxPanel *panelRendering = new wxPanel( m_pNotebookActions, wxID_ANY );

   new wxStaticText( panelRendering, -1, wxT("Background color"));
   new wxButton( panelRendering, ID_BUTTON_RENDERING_BGCOLOR, wxT("Background color"),  wxPoint(20, 20));

   m_pNotebookActions->AddPage( panelRendering, wxT("Rendering"), false, page_bmp );

   // Geometry
   wxPanel *panelGeometry = new wxPanel( m_pNotebookActions, wxID_ANY );

   m_nRadioGeometries = 4;
   m_pRadioGeometries = (wxRadioButton**)malloc(m_nRadioGeometries*sizeof(wxRadioButton*));
   m_pRadioGeometries[0] = new wxRadioButton(panelGeometry,ID_GEOMETRY_CUBE,wxT("Cube"), wxPoint(5,5), wxSize(100,30), wxRB_GROUP);
   m_pRadioGeometries[1] = new wxRadioButton(panelGeometry,ID_GEOMETRY_SPHERE,wxT("Sphere"), wxPoint(5,5+30), wxSize(100,30));
   m_pGeometrySphereLat = new wxSpinCtrl( panelGeometry, ID_GEOMETRY_SPHERE_LAT, wxT("5"), wxPoint(5,5+60), wxSize(50,30) );
   m_pGeometrySphereLng = new wxSpinCtrl( panelGeometry, ID_GEOMETRY_SPHERE_LNG, wxT("5"), wxPoint(105,5+60), wxSize(50,30) );
   m_pRadioGeometries[2] = new wxRadioButton(panelGeometry,ID_GEOMETRY_CYLINDER,wxT("Cylinder"), wxPoint(5,5+90), wxSize(100,30));
   m_pRadioGeometries[3] = new wxRadioButton(panelGeometry,ID_GEOMETRY_KLEIN_BOTTLE,wxT("Klein bottle"), wxPoint(5,5+120), wxSize(100,30));
   m_pGeometryKleinBottleTheta = new wxSpinCtrl( panelGeometry, ID_GEOMETRY_KLEIN_BOTTLE_THETA, wxT("5"), wxPoint(5,5+150), wxSize(50,30) );
   m_pGeometryKleinBottlePhi = new wxSpinCtrl( panelGeometry, ID_GEOMETRY_KLEIN_BOTTLE_PHI, wxT("5"), wxPoint(105,5+150), wxSize(50,30) );


   m_pNotebookActions->AddPage( panelGeometry, wxT("Geometry"), false, page_bmp );


   // Smoothing
   wxPanel *panelSmoothing = new wxPanel( m_pNotebookActions, wxID_ANY );

   wxFlexGridSizer *flexSmoothing = new wxFlexGridSizer( 2 );
   flexSmoothing->AddGrowableRow( 0 );
   flexSmoothing->AddGrowableRow( 3 );
   flexSmoothing->AddGrowableCol( 1 );
   flexSmoothing->Add( 5,5 );
   flexSmoothing->Add( 5,5 );

   flexSmoothing->Add( new wxStaticText( panelSmoothing, -1, wxT("Taubin") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexSmoothing->Add( new wxButton( panelSmoothing, ID_BUTTON_SMOOTHING_TAUBIN, wxT("Taubin") ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexSmoothing->Add( new wxStaticText( panelSmoothing, -1, wxT("Laplacian") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexSmoothing->Add( new wxButton( panelSmoothing, ID_BUTTON_SMOOTHING_LAPLACIAN, wxT("Laplacian") ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexSmoothing->Add( new wxStaticText( panelSmoothing, -1, wxT("wxTextCtrl:") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexSmoothing->Add( new wxTextCtrl( panelSmoothing, -1, wxT(""), wxDefaultPosition, wxSize(100,-1)), 1, wxALL|wxALIGN_CENTRE, 5 );

   flexSmoothing->Add( new wxStaticText( panelSmoothing, -1, wxT("wxSpinCtrl:") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexSmoothing->Add( new wxSpinCtrl( panelSmoothing, -1, wxT("5"), wxDefaultPosition, wxSize(100,-1), wxSP_ARROW_KEYS, 5, 50, 5 ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexSmoothing->Add( 5,5 );   flexSmoothing->Add( 5,5 );
   panelSmoothing->SetSizer( flexSmoothing );
   m_pNotebookActions->AddPage( panelSmoothing, wxT("Smoothing"), false, page_bmp );


   // Curvatures
   wxPanel *panelCurvatures = new wxPanel( m_pNotebookActions, wxID_ANY );

   //wxDrawingArea* m_pDrawingAreaCurvatureHistogram = new wxDrawingArea (panelCurvatures);
   //m_pDrawingAreaCurvatureHistogram->SetSize (300, 200);
	
   wxFlexGridSizer *flexCurvatures = new wxFlexGridSizer( 2 );
   flexCurvatures->AddGrowableRow( 0 );
   flexCurvatures->AddGrowableRow( 3 );
   flexCurvatures->AddGrowableCol( 1 );
   flexCurvatures->Add( 5,5 );   flexCurvatures->Add( 5,5 );

   flexCurvatures->Add( new wxStaticText( panelCurvatures, -1, wxT("Desbrun") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexCurvatures->Add( new wxButton( panelCurvatures, ID_BUTTON_CURVATURES_DESBRUN, wxT("Desbrun") ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexCurvatures->Add( new wxStaticText( panelCurvatures, -1, wxT("Taubin") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexCurvatures->Add( new wxButton( panelCurvatures, ID_BUTTON_CURVATURES_TAUBIN, wxT("Taubin") ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexCurvatures->Add( 5,5 );   flexCurvatures->Add( 5,5 );
   panelCurvatures->SetSizer( flexCurvatures );
   m_pNotebookActions->AddPage( panelCurvatures, wxT("Curvatures"), false, page_bmp );

   // NPR
/*
   wxPanel *panelNPR = new wxPanel( m_pNotebookActions, wxID_ANY );

   wxFlexGridSizer *flexNPR = new wxFlexGridSizer( 2 );
   flexNPR->AddGrowableRow( 0 );
   flexNPR->AddGrowableRow( 3 );
   flexNPR->AddGrowableCol( 1 );
   flexNPR->Add( 5,5 );   flexNPR->Add( 5,5 );

   flexNPR->Add( new wxStaticText( panelNPR, -1, wxT("ALl") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexNPR->Add( new wxButton( panelNPR, ID_BUTTON_NPR_COMPUTE, wxT("Compute") ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexNPR->Add( new wxStaticText( panelNPR, -1, wxT("Angle ") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   m_pNPRSliderMinimalAngle = new wxSlider(panelNPR, SliderPage_Slider,
                            0, 0, 100,
                            wxDefaultPosition, wxDefaultSize);
   flexNPR->Add( m_pNPRSliderMinimalAngle, 0, wxALL|wxALIGN_CENTRE, 5 );

   flexNPR->Add( new wxStaticText( panelNPR, -1, wxT("Export") ), 0, wxALL|wxALIGN_CENTRE, 5 );
   flexNPR->Add( new wxButton( panelNPR, ID_BUTTON_NPR_EXPORT, wxT("Export") ), 0, wxALL|wxALIGN_CENTRE, 5 );

   flexNPR->Add( 5,5 );   flexNPR->Add( 5,5 );
   panelNPR->SetSizer( flexNPR );
   m_pNotebookActions->AddPage( panelNPR, wxT("NPR"), false, page_bmp );
*/


/*
   m_pNotebookActions->AddPage( new wxTextCtrl( m_pNotebookActions, wxID_ANY, wxT("Some text"),
                wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxNO_BORDER) , wxT("wxTextCtrl 1"), false, page_bmp );

   m_pNotebookActions->AddPage( new wxTextCtrl( m_pNotebookActions, wxID_ANY, wxT("Some more text"),
                wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxNO_BORDER) , wxT("wxTextCtrl 2") );

   m_pNotebookActions->AddPage( new wxTextCtrl( m_pNotebookActions, wxID_ANY, wxT("Some more text"),
                wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxNO_BORDER) , wxT("wxTextCtrl 7 (longer title)") );
*/
   //m_pNotebookActions->AddPage(CreateHTMLCtrl(m_pNotebookActions), wxT("Welcome to wxAUI") , false, page_bmp);
//   m_pNotebookActions->AddPage( new wxTextCtrl( m_pNotebookActions, wxID_ANY, wxT("Some text"),
//                wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxNO_BORDER) , wxT("wxTextCtrl 1"), false, page_bmp );

   return m_pNotebookActions;
}

//
//
//
wxAuiNotebook* MyFrame::CreateNotebook(void)
{
   // create the notebook off-window to avoid flicker
   wxSize client_size = GetClientSize();

   m_pCtrl = new wxAuiNotebook(this, ID_NOTEBOOK_MAIN,
                                    wxPoint(client_size.x, client_size.y),
                                    wxSize(430,200),
                                    m_notebook_style);
   m_pCtrl->SetArtProvider(new wxAuiSimpleTabArt);

   int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 24, 0};
   m_pGLCanvas = new MyGLCanvas (m_pCtrl, m_pWndLogging, args);
   auto pObject = new Object3D();
   pObject->AddMesh(CreateCube());
   m_pGLCanvas->SetObject3D(pObject);
   m_pCtrl->AddPage( m_pGLCanvas , wxT("cube") );
   UpdatePropertiesGrid();

   return m_pCtrl;
}

wxString MyFrame::GetIntroText()
{
    const char* text =
        "<html><body>"
        "<h3>Welcome to wxAUI</h3>"
        "<br/><b>Overview</b><br/>"
        "<p>wxAUI is an Advanced User Interface library for the wxWidgets toolkit "
        "that allows developers to create high-quality, cross-platform user "
        "interfaces quickly and easily.</p>"
        "<p><b>Features</b></p>"
        "<p>With wxAUI, developers can create application frameworks with:</p>"
        "<ul>"
        "<li>Native, dockable floating frames</li>"
        "<li>Perspective saving and loading</li>"
        "<li>Native toolbars incorporating real-time, &quot;spring-loaded&quot; dragging</li>"
        "<li>Customizable floating/docking behavior</li>"
        "<li>Completely customizable look-and-feel</li>"
        "<li>Optional transparent window effects (while dragging or docking)</li>"
        "<li>Splittable notebook control</li>"
        "</ul>"
        "<p><b>What's new in 0.9.4?</b></p>"
        "<p>wxAUI 0.9.4, which is bundled with wxWidgets, adds the following features:"
        "<ul>"
        "<li>New wxAuiToolBar class, a toolbar control which integrates more "
        "cleanly with wxAuiFrameManager.</li>"
        "<li>Lots of bug fixes</li>"
        "</ul>"
        "<p><b>What's new in 0.9.3?</b></p>"
        "<p>wxAUI 0.9.3, which is now bundled with wxWidgets, adds the following features:"
        "<ul>"
        "<li>New wxAuiNotebook class, a dynamic splittable notebook control</li>"
        "<li>New wxAuiMDI* classes, a tab-based MDI and drop-in replacement for classic MDI</li>"
        "<li>Maximize/Restore buttons implemented</li>"
        "<li>Better hinting with wxGTK</li>"
        "<li>Class rename.  'wxAui' is now the standard class prefix for all wxAUI classes</li>"
        "<li>Lots of bug fixes</li>"
        "</ul>"
        "<p><b>What's new in 0.9.2?</b></p>"
        "<p>The following features/fixes have been added since the last version of wxAUI:</p>"
        "<ul>"
        "<li>Support for wxMac</li>"
        "<li>Updates for wxWidgets 2.6.3</li>"
        "<li>Fix to pass more unused events through</li>"
        "<li>Fix to allow floating windows to receive idle events</li>"
        "<li>Fix for minimizing/maximizing problem with transparent hint pane</li>"
        "<li>Fix to not paint empty hint rectangles</li>"
        "<li>Fix for 64-bit compilation</li>"
        "</ul>"
        "<p><b>What changed in 0.9.1?</b></p>"
        "<p>The following features/fixes were added in wxAUI 0.9.1:</p>"
        "<ul>"
        "<li>Support for MDI frames</li>"
        "<li>Gradient captions option</li>"
        "<li>Active/Inactive panes option</li>"
        "<li>Fix for screen artifacts/paint problems</li>"
        "<li>Fix for hiding/showing floated window problem</li>"
        "<li>Fix for floating pane sizing problem</li>"
        "<li>Fix for drop position problem when dragging around center pane margins</li>"
        "<li>LF-only text file formatting for source code</li>"
        "</ul>"
        "<p>See README.txt for more information.</p>"
        "</body></html>";

    return wxString::FromAscii(text);
}

//
// Smoothing Events
//
void MyFrame::OnButtonSmoothingTaubin(wxCommandEvent& WXUNUSED(event))
{
 	*m_pWndLogging << _T("Smoothing Taubin\n");

	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());

/*
  Mesh_half_edge* pMeshHE = pGLCanvas->GetMesh ();

   MeshAlgoSmoothingTaubin algo;
   algo.Apply (pMeshHE);
*/
	pGLCanvas->Refresh ();
}

void MyFrame::OnButtonSmoothingLaplacian(wxCommandEvent& WXUNUSED(event))
{
 	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
/*
  Mesh_half_edge* pMeshHE = pGLCanvas->GetMesh ();

   MeshAlgoSmoothingLaplacian algo;
   algo.Apply (pMeshHE);
*/
	pGLCanvas->Refresh ();
}

//
// Curvatures Events
//
void MyFrame::OnButtonCurvaturesTaubin (wxCommandEvent& WXUNUSED(event))
{
 	*m_pWndLogging << _T("Smoothing Laplacian\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());

/*
  Mesh_half_edge* pMeshHE = pGLCanvas->GetMesh ();

   MeshAlgoTensorEvaluator algo;
   algo.Init (pMeshHE);
   algo.Evaluate (TENSOR_TAUBIN);
   algo.EvaluateColors (CURVATURE_GAUSSIAN);


   float *pHistogram = NULL;
   algo.GetCurvaturesHistogram (CURVATURE_MAX, 64, &pHistogram);
   for (int i=0; i<64; i++)
		*m_pWndLogging << pHistogram[i] << " ";
	*m_pWndLogging << "\n";
	*/
	pGLCanvas->Refresh ();
}

void MyFrame::OnButtonCurvaturesDesbrun (wxCommandEvent& WXUNUSED(event))
{
 	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
/*
  Mesh_half_edge* pMeshHE = pGLCanvas->GetMesh ();

   MeshAlgoTensorEvaluator algo;
   algo.Init (pMeshHE);
   algo.Evaluate (TENSOR_DESBRUN);
   algo.EvaluateColors (CURVATURE_MEAN);
*/
	pGLCanvas->Refresh ();

	   wxPaintDC pdc(this);
///////
/*
#if wxUSE_GRAPHICS_CONTEXT
     wxGCDC gdc( pdc ) ;
    wxDC &dc = m_useContext ? (wxDC&) gdc : (wxDC&) pdc ;
#else
    wxDC &dc = pdc ;
#endif
*/
    wxDC &dc = pdc ;



	PrepareDC(dc);
	//m_owner->PrepareDC(dc);
	dc.Clear();
        dc.SetPen(*wxMEDIUM_GREY_PEN);
        for ( int i = 0; i < 200; i++ )
            dc.DrawLine(0, i*10, i*10, 0);


}

//
// NPR
//
void MyFrame::OnButtonNPRCompute (wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	//pGLCanvas->NPRCompute ();
}

void MyFrame::OnButtonNPRExport (wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	//pGLCanvas->ExportNPR ();
}

void MyFrame::OnSlider(wxScrollEvent& event)
{
    wxEventType eventType = event.GetEventType();

    //m_pNPRSliderMinimalAngle->GetValue ();

    //*m_pWndLogging << m_pNPRSliderMinimalAngle->GetValue () << "\n";

	// into the interval [ 0 ; 100 ]
	//int iValue = m_pNPRSliderMinimalAngle->GetValue ();
	
	// convert into the interval [ 0 ; 1 ]
	//float fValue = iValue/100.;

	//MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	//pGLCanvas->SetNPRAngleThreshold (fValue);

	//pGLCanvas->Refresh ();
}
