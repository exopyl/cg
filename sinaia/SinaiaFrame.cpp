#include <wx/artprov.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>     //for opening files from OpenFile
#include <wx/dir.h>
#include <wx/dirctrl.h>
#include <wx/listctrl.h>
#include <wx/dnd.h>
#include <wx/scrolwin.h>
#include <wx/bmpbuttn.h>
#include <wx/dcmemory.h>
#include <wx/stattext.h>
#include "wx/mimetype.h"
#include <wx/propgrid/propgrid.h>
#include <wx/busyinfo.h>
#include <vector>
#include <chrono>

#include "sample.xpm"

// https://www.flaticon.com/fr/icone-gratuite/format-de-fichier-obj_8760186
#include "format_obj.xpm"
#include "format_stl.xpm"
#include "format_3ds.xpm"
#include "open.xpm"
#include "save.xpm"
#include "icons.h"
#include "SupportedFormats.h"

#include "SinaiaFrame.h"
#include "wxOpenGLCanvas.h"
//#include "DrawingArea.h"
#include "SettingsPanel.h"
#include "SettingsDialog.h"
#include "PropertyPanel.h"
#include "CurvaturePanel.h"
#include "DecimationPanel.h"

#include "../src/cgmesh/smoothing_taubin.h"
#include "../src/cgmesh/smoothing_laplacian.h"
#include "../src/cgmesh/subdivision.h"
#include "../src/cgmesh/DiffParamEvaluator.h"
#include "../src/cgmesh/mesh_data_manager.h"
#include "../src/cgmesh/parameterized_shapes.h"
#include "../src/cgmesh/normals.h"

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
    EVT_MENU(ID_FILE_EXPORT_IMAGE, MyFrame::OnExportImage)
    EVT_MENU(ID_GEOMETRY_NEW_CUBE, MyFrame::OnNewGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_SPHERE, MyFrame::OnNewGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_CYLINDER, MyFrame::OnNewGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_TEAPOT, MyFrame::OnNewGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_KLEIN_BOTTLE, MyFrame::OnNewGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_CUBE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_SPHERE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_CYLINDER, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_CONE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_CAPSULE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_TORUS, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_KLEIN_BOTTLE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_HELICOID, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_SEASHELL, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_SEASHELL_VON_SEGGERN, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_CORKSCREW, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_MOBIUS_STRIP, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_RADIAL_WAVE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_BREATHER, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_HYPERBOLIC_PARABOLOID, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_MONKEY_SADDLE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_BLOBS, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_DROP, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_GUIMARD, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_TORUS_KNOT, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_CINQUEFOIL_KNOT, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_TREFOIL_KNOT, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_BORROMEAN_RINGS, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_MENGER_SPONGE, MyFrame::OnNewParameterizedGeometry)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_SVG,           MyFrame::OnNewParameterizedSvg)
    EVT_MENU(ID_GEOMETRY_NEW_PARAM_IMPLICIT,      MyFrame::OnNewParameterizedImplicit)
    EVT_MENU(ID_Settings, MyFrame::OnSettings)
    EVT_MENU(wxID_EXIT, MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_MENU(ID_3D_FRAME, MyFrame::On3DFrame)
    EVT_MENU(ID_3D_GRID, MyFrame::On3DGrid)
    EVT_MENU(ID_3D_FILL, MyFrame::On3DFill)
    EVT_MENU(ID_3D_WIREFRAME, MyFrame::On3DWireframe)
    EVT_MENU(ID_3D_SMOOTH, MyFrame::On3DSmooth)
    EVT_MENU(ID_3D_FLAT, MyFrame::On3DFlat)
    EVT_MENU(ID_3D_POINT, MyFrame::On3DPoint)
    EVT_MENU(ID_3D_WARNING, MyFrame::On3DWarning)
    EVT_MENU(ID_3D_LIGHTING, MyFrame::On3DLighting)
    EVT_MENU(ID_3D_CLIPPING, MyFrame::On3DClippingPlane)
    EVT_MENU(ID_RENDER_SHOW_FPS, MyFrame::OnToggleShowFps)
    EVT_MENU(ID_BUTTON_RENDERING_BGCOLOR, MyFrame::OnBgColor)

    EVT_BUTTON(ID_BUTTON_RENDERING_BGCOLOR, MyFrame::OnBgColor)

    EVT_DIRCTRL_SELECTIONCHANGED(ID_DIRCTRL, MyFrame::OnDirCtrlSelectionChanged)
    EVT_LIST_ITEM_ACTIVATED(ID_FILESCTRL, MyFrame::OnFilesCtrlListItemActivated)
    EVT_LIST_BEGIN_DRAG(ID_FILESCTRL, MyFrame::OnFilesCtrlBeginDrag)

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
    EVT_MENU(ID_TREATMENT_MAKE_TRIANGLES, MyFrame::OnTreatmentMakeTriangles)
    EVT_UPDATE_UI(ID_TREATMENT_MAKE_TRIANGLES, MyFrame::OnUpdateUITreatmentMakeTriangles)
    EVT_MENU(ID_TREATMENT_MERGE_VERTICES, MyFrame::OnTreatmentMergeVertices)
    EVT_MENU(ID_TREATMENT_NORMALIZE, MyFrame::OnTreatmentNormalize)
    EVT_MENU(ID_TREATMENT_SMOOTHING_TAUBIN, MyFrame::OnTreatmentSmoothingTaubin)
    EVT_MENU(ID_TREATMENT_SMOOTHING_LAPLACIAN, MyFrame::OnTreatmentSmoothingLaplacian)
    EVT_MENU(ID_TREATMENT_SUBDIVISION_LOOP, MyFrame::OnTreatmentSubdivisionLoop)
    EVT_MENU(ID_TREATMENT_SUBDIVISION_KARBACHER, MyFrame::OnTreatmentSubdivisionKarbacher)
    EVT_MENU(ID_TREATMENT_SUBDIVISION_SQRT3, MyFrame::OnTreatmentSubdivisionSqrt3)
    // Treatments dock: one method combo + Apply per tab. The Apply handlers
    // dispatch to the per-method handlers based on the combo selection.
    EVT_BUTTON(ID_TREATMENT_SMOOTHING_APPLY, MyFrame::OnTreatmentApplySmoothing)
    EVT_BUTTON(ID_TREATMENT_SUBDIVISION_APPLY, MyFrame::OnTreatmentApplySubdivision)
    EVT_BUTTON(ID_TREATMENT_NORMALS_APPLY, MyFrame::OnTreatmentApplyNormals)
    EVT_MENU(ID_ShowInformation, MyFrame::OnShowWindow)
    EVT_MENU(ID_ShowTreatments, MyFrame::OnShowWindow)
    EVT_UPDATE_UI(ID_ShowTreatments, MyFrame::OnUpdateUI)
    EVT_MENU(ID_ShowLogging, MyFrame::OnShowWindow)
    EVT_MENU(ID_ShowExplorer, MyFrame::OnShowWindow)
    EVT_UPDATE_UI(ID_ShowInformation, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_ShowLogging, MyFrame::OnUpdateUI)
    EVT_UPDATE_UI(ID_ShowExplorer, MyFrame::OnUpdateUI)
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
    Connect(wxEVT_DROP_FILES, wxDropFilesEventHandler(MyFrame::OnDropFiles), nullptr, this);


    // tell wxAuiManager to manage this frame
    m_mgr.SetManagedWindow(this);
    m_mgr.SetFlags(m_mgr.GetFlags() | wxAUI_MGR_LIVE_RESIZE);

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
    file_menu->Append(ID_FILE_EXPORT_IMAGE, _T("&Export image as..."), _T("Export current view to a BMP image"));
    file_menu->AppendSeparator();
    file_menu->Append(ID_Settings, _T("&Settings..."), _T("Configure import options"));
    file_menu->AppendSeparator();
    file_menu->Append(wxID_EXIT, _("Exit"));


    wxMenu* new_geometry_menu = new wxMenu;
    new_geometry_menu->Append(ID_GEOMETRY_NEW_CUBE, wxT("Cube"));
    new_geometry_menu->Append(ID_GEOMETRY_NEW_SPHERE, wxT("Sphere"));
    new_geometry_menu->Append(ID_GEOMETRY_NEW_CYLINDER, wxT("Cylinder"));
    new_geometry_menu->Append(ID_GEOMETRY_NEW_TEAPOT, wxT("Teapot"));
    new_geometry_menu->Append(ID_GEOMETRY_NEW_KLEIN_BOTTLE, wxT("Klein bottle"));

    // Parameterized shapes -- live-edited via the Parameters panel
    wxMenu* basic_shapes_menu = new wxMenu;
    basic_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_CUBE, wxT("Cube..."));
    basic_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_SPHERE, wxT("Sphere..."));
    basic_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_CYLINDER, wxT("Cylinder..."));
    basic_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_CONE, wxT("Cone..."));
    basic_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_CAPSULE, wxT("Capsule..."));
    basic_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_TORUS, wxT("Torus..."));

    wxMenu* parametric_surfaces_menu = new wxMenu;
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_KLEIN_BOTTLE, wxT("Klein Bottle..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_HELICOID, wxT("Helicoid..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_SEASHELL, wxT("Seashell..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_SEASHELL_VON_SEGGERN, wxT("Seashell (von Seggern)..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_CORKSCREW, wxT("Corkscrew..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_MOBIUS_STRIP, wxT("Mobius Strip..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_RADIAL_WAVE, wxT("Radial Wave..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_BREATHER, wxT("Breather..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_HYPERBOLIC_PARABOLOID, wxT("Hyperbolic Paraboloid..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_MONKEY_SADDLE, wxT("Monkey Saddle..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_BLOBS, wxT("Blobs..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_DROP, wxT("Drop..."));
    parametric_surfaces_menu->Append(ID_GEOMETRY_NEW_PARAM_GUIMARD, wxT("Guimard..."));

    wxMenu* knots_menu = new wxMenu;
    knots_menu->Append(ID_GEOMETRY_NEW_PARAM_TORUS_KNOT, wxT("Torus Knot..."));
    knots_menu->Append(ID_GEOMETRY_NEW_PARAM_CINQUEFOIL_KNOT, wxT("Cinquefoil Knot..."));
    knots_menu->Append(ID_GEOMETRY_NEW_PARAM_TREFOIL_KNOT, wxT("Trefoil Knot..."));
    knots_menu->Append(ID_GEOMETRY_NEW_PARAM_BORROMEAN_RINGS, wxT("Borromean Rings..."));

    wxMenu* fractal_shapes_menu = new wxMenu;
    fractal_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_MENGER_SPONGE, wxT("Menger Sponge..."));

    wxMenu* svg_shapes_menu = new wxMenu;
    svg_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_SVG, wxT("SVG extrusion..."));

    wxMenu* pointcloud_shapes_menu = new wxMenu;
    pointcloud_shapes_menu->Append(ID_GEOMETRY_NEW_PARAM_IMPLICIT, wxT("Implicit surface (PLY)..."));

    wxMenu* create_menu = new wxMenu;
    create_menu->AppendSubMenu(basic_shapes_menu, wxT("Basic Shapes"));
    create_menu->AppendSubMenu(parametric_surfaces_menu, wxT("Parametric Surfaces"));
    create_menu->AppendSubMenu(knots_menu, wxT("Knots"));
    create_menu->AppendSubMenu(fractal_shapes_menu, wxT("Fractal Shapes"));
    create_menu->AppendSubMenu(svg_shapes_menu, wxT("From SVG"));
    create_menu->AppendSubMenu(pointcloud_shapes_menu, wxT("From Point Cloud"));

    wxMenu* geometry_menu = new wxMenu;
    geometry_menu->AppendSubMenu(new_geometry_menu, wxT("New"));
    geometry_menu->AppendSubMenu(create_menu, wxT("Create"));

    wxMenu* options_menu = new wxMenu;

    // The notebook appearance options (theme, close button, alignment, tab
    // flags) have been moved to the "2D Panel" tab of the Settings dialog
    // (File > Settings...). See SettingsDialog / PanelSettings. The 3D view
    // options below stay in the menu, where they apply live.
    wxMenu* panel_menu = new wxMenu;
    panel_menu->Append(ID_BUTTON_RENDERING_BGCOLOR, _("Background"));
    panel_menu->AppendCheckItem(ID_RENDER_SHOW_FPS, _("Show FPS"));
    options_menu->AppendSubMenu(panel_menu, wxT("3D Panel"));

    wxMenu* windows_menu = new wxMenu;
    windows_menu->AppendCheckItem(ID_ShowInformation, _("Information"));
    windows_menu->AppendCheckItem(ID_ShowTreatments, _("Treatments"));
    windows_menu->AppendCheckItem(ID_ShowLogging, _("Logging Window"));
    windows_menu->AppendCheckItem(ID_ShowExplorer, _("Explorer"));
    options_menu->AppendSubMenu(windows_menu, wxT("Windows"));

    wxMenu* help_menu = new wxMenu;
    help_menu->Append(wxID_ABOUT, _("About..."));

    // Smoothing / Subdivision / Decimation now live as tabs of the "Treatments"
    // dock (Options > Windows > Treatments), each with a method combo + Apply.
    wxMenu* treatments_menu = new wxMenu;
    treatments_menu->Append(ID_TREATMENT_MAKE_TRIANGLES, _("Make triangles"));
    treatments_menu->Append(ID_TREATMENT_MERGE_VERTICES, _("Merge vertices"));
    treatments_menu->Append(ID_TREATMENT_NORMALIZE, _("Normalize"));

    mb->Append(file_menu, _("File"));
    mb->Append(geometry_menu, _("Geometry"));
    mb->Append(treatments_menu, _("Treatments"));
    mb->Append(options_menu, _("Options"));
    mb->Append(help_menu, _("Help"));

    SetMenuBar(mb);

    CreateStatusBar(2);
    int statusWidths[2] = { -1, 100 };
    GetStatusBar()->SetStatusWidths(2, statusWidths);
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
    //tb2->AddTool(wxID_OPEN, wxBitmap(open_xpm), wxNullBitmap, false, -1, -1, (wxObject *) nullptr, _("Open"));
    //tb2->AddTool(wxID_SAVEAS, wxBitmap(save_xpm), wxNullBitmap, false, -1, -1, (wxObject *) nullptr, _("Save"));
    //tb2->AddTool(ID_SampleItem+3, wxT("Test"), wxBitmap (open_xpm));
    //tb2->AddTool(ID_SampleItem+4, wxT("Test"), wxBitmap (save_xpm));
    wxBitmap tb2_open = wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_OTHER, wxSize(16,16));
    wxBitmap tb2_save = wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_OTHER, wxSize(16,16));
    wxBitmap tb2_saveas = wxArtProvider::GetBitmap(wxART_FILE_SAVE_AS, wxART_OTHER, wxSize(16,16));
    m_pToolBar2->AddTool(wxID_OPEN, wxT("Test"), tb2_open);
    //m_pToolBar2->AddTool(wxID_SAVE, wxT("Test"), tb2_save);
    m_pToolBar2->AddTool(wxID_SAVEAS, wxT("Test"), tb2_saveas);
    m_pToolBar2->AddTool(ID_3D_FRAME, wxT("Repere"), wxBitmap(repere_xpm));
    m_pToolBar2->AddTool(ID_3D_GRID, wxT("Grid"), wxBitmap(grid_xpm));
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
    m_pToolBar2->AddSeparator();
    m_pToolBar2->AddTool(ID_3D_CLIPPING, wxT("Clipping"), wxBitmap(planar_cut));
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
    // Show property help strings as tooltips — used to surface the full source
    // path when hovering the "File" row (its value is only the file basename).
    m_propertiesGrid->SetExtraStyle(m_propertiesGrid->GetExtraStyle() | wxPG_EX_HELP_AS_TOOLTIPS);
    g_properties = {
        new wxStringProperty("File"),
        new wxStringProperty("Meshes"),
        new wxStringProperty("Vertices"),
        new wxStringProperty("Faces") ,
        new wxStringProperty("Triangle mesh"),
        new wxStringProperty("Borders"),
        new wxStringProperty("Non manifold edges"),
        new wxStringProperty("Bounding box X"),
        new wxStringProperty("Bounding box Y"),
        new wxStringProperty("Bounding box Z"),
        new wxStringProperty("Size X"),
        new wxStringProperty("Size Y"),
        new wxStringProperty("Size Z"),
    };
    for (auto& property : g_properties)
    {
        m_propertiesGrid->Append(property);
        property->ChangeFlag(wxPG_PROP_READONLY, true);
    }

    // m_propertiesGrid is added below, grouped into the "Information" notebook.

    // Parameters panel: live-edits the active parameterized geometry. Hidden
    // by default; UpdateContextualPanes() reveals it only when the active tab
    // drives a parameterized object.
    m_pParamPanel = new PropertyPanel(this);
    m_pParamPanel->SetOnChanged([this]() { this->OnParameterChanged(); });
    m_mgr.AddPane(m_pParamPanel, wxAuiPaneInfo().Name(wxT("Parameters")).Caption(wxT("Parameters")).Right().BestSize(250, -1).MinSize(200, -1).Hide());

    // Curvature panel: lets the user choose which curvature to display. Hidden
    // until curvature visualization is active for the current tab. The
    // callbacks recompute (method change) or recolour (type change) the model.
    m_pCurvaturePanel = new CurvaturePanel(this);
    m_pCurvaturePanel->SetOnMethodChanged([this](TensorMethodId method) {
        MyGLCanvas* pCanvas = m_pCtrl ? (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection()) : nullptr;
        if (!pCanvas) return;
        CurvatureState& st = m_curvatureByCanvas[pCanvas];
        st.method = method;
        // Only recompute/recolour when the visualization is on; otherwise the
        // method is just remembered for when "Apply visualization" is checked.
        if (st.enabled)
            ApplyCurvature(pCanvas, method, st.type);
    });
    m_pCurvaturePanel->SetOnTypeChanged([this](CurvatureType type) {
        MyGLCanvas* pCanvas = m_pCtrl ? (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection()) : nullptr;
        if (!pCanvas) return;
        m_curvatureByCanvas[pCanvas];  // ensure an entry exists to store the type
        RecolorCurvature(pCanvas, type);
    });
    m_pCurvaturePanel->SetOnEnableChanged([this](bool enabled) {
        MyGLCanvas* pCanvas = m_pCtrl ? (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection()) : nullptr;
        if (pCanvas) SetCurvatureEnabled(pCanvas, enabled);
    });
    // m_pCurvaturePanel is added below as the "Curvature" tab of the Treatments dock.

    // Decimation parameters panel — added below as the "Decimation" tab of the
    // Treatments dock. Apply runs ApplyDecimation() on the active canvas.
    m_pDecimationPanel = new DecimationPanel(this);
    m_pDecimationPanel->SetOnApply([this]() { ApplyDecimation(); });


    m_hierarchyMeshes = CreateHierarchyMeshesTreeCtrl();
    m_hierarchyMaterials = CreateHierarchyMaterialsTreeCtrl();

    // Panneau "Models" : liste défilante, une ligne par Model (nom + œil + poubelle).
    BuildModelsIcons();
    m_modelsPanel = new wxScrolledWindow(this, ID_OPENFILESLIST);
    m_modelsPanel->SetScrollRate(0, 10);
    m_modelsPanel->SetSizer(new wxBoxSizer(wxVERTICAL));

    // Group Properties / Meshes / Materials as tabs of a single dockable
    // "Information" pane (saves dock space vs three stacked panes). The controls
    // are reparented into an AUI notebook. No close buttons / no external tab
    // move, so the frame's catch-all notebook close/DND handlers never fire on
    // it, and PAGE_CHANGED is bound to the main notebook id only.
    m_pInfoNotebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxAUI_NB_TOP | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS);
    m_propertiesGrid->Reparent(m_pInfoNotebook);
    m_hierarchyMeshes->Reparent(m_pInfoNotebook);
    m_hierarchyMaterials->Reparent(m_pInfoNotebook);
    m_pInfoNotebook->AddPage(m_propertiesGrid,    _("Properties"));
    m_pInfoNotebook->AddPage(m_hierarchyMeshes,   _("Meshes"));
    m_pInfoNotebook->AddPage(m_hierarchyMaterials, _("Materials"));
    // Empilés à droite dans la même rangée : "Models" AU-DESSUS (Position 0), puis
    // "Model information" en dessous (Position 1).
    m_mgr.AddPane(m_modelsPanel, wxAuiPaneInfo().Name(wxT("Models")).Caption(wxT("Models"))
                  .Right().Layer(1).Row(0).Position(0).BestSize(280, 110).MinSize(160, 70));
    // Name interne inchangé ("Information" : référencé par le menu Affichage et la
    // persistance de layout) ; seul le libellé visible passe à "Model information".
    m_mgr.AddPane(m_pInfoNotebook, wxAuiPaneInfo().Name(wxT("Information")).Caption(wxT("Model information"))
                  .Right().Layer(1).Row(0).Position(1).BestSize(280, 500).MinSize(220, 200));

    // "Treatments" dock: Smoothing / Subdivision / Normals / Decimation tabs.
    // Each operation tab is a method combo + Apply, applied to the active model.
    m_pTreatmentsNotebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                              wxAUI_NB_TOP | wxAUI_NB_SCROLL_BUTTONS);
    {
        wxPanel *page = new wxPanel(m_pTreatmentsNotebook, wxID_ANY);
        page->SetBackgroundColour(*wxWHITE);
        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);
        s->Add(new wxStaticText(page, wxID_ANY, _("Method")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
        wxArrayString methods;
        methods.Add(_("Taubin"));
        methods.Add(_("Laplacian"));
        m_pSmoothingMethodCombo = new wxComboBox(page, wxID_ANY, methods[0], wxDefaultPosition,
                                                 wxDefaultSize, methods, wxCB_READONLY);
        m_pSmoothingMethodCombo->SetSelection(0);
        s->Add(m_pSmoothingMethodCombo, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
        s->Add(new wxButton(page, ID_TREATMENT_SMOOTHING_APPLY, _("Apply smoothing")), 0, wxEXPAND | wxALL, 6);
        page->SetSizer(s);
        m_pTreatmentsNotebook->AddPage(page, _("Smoothing"));
    }
    {
        wxPanel *page = new wxPanel(m_pTreatmentsNotebook, wxID_ANY);
        page->SetBackgroundColour(*wxWHITE);
        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);
        s->Add(new wxStaticText(page, wxID_ANY, _("Method")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
        wxArrayString methods;
        methods.Add(_("Loop"));
        methods.Add(_("Karbacher"));
        methods.Add(_("Sqrt(3)"));
        m_pSubdivisionMethodCombo = new wxComboBox(page, wxID_ANY, methods[0], wxDefaultPosition,
                                                   wxDefaultSize, methods, wxCB_READONLY);
        m_pSubdivisionMethodCombo->SetSelection(0);
        s->Add(m_pSubdivisionMethodCombo, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
        s->Add(new wxButton(page, ID_TREATMENT_SUBDIVISION_APPLY, _("Apply subdivision")), 0, wxEXPAND | wxALL, 6);
        page->SetSizer(s);
        m_pTreatmentsNotebook->AddPage(page, _("Subdivision"));
    }
    {
        wxPanel *page = new wxPanel(m_pTreatmentsNotebook, wxID_ANY);
        page->SetBackgroundColour(*wxWHITE);
        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);
        s->Add(new wxStaticText(page, wxID_ANY, _("Method")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
        wxArrayString methods;
        methods.Add(_("Gouraud (equal weight)"));
        methods.Add(_("Thurmer (angle weighted)"));
        methods.Add(_("Max (Nelson Max)"));
        m_pNormalsMethodCombo = new wxComboBox(page, wxID_ANY, methods[1], wxDefaultPosition,
                                               wxDefaultSize, methods, wxCB_READONLY);
        m_pNormalsMethodCombo->SetSelection(1); // default: Thurmer (angle-weighted)
        s->Add(m_pNormalsMethodCombo, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
        s->Add(new wxButton(page, ID_TREATMENT_NORMALS_APPLY, _("Apply normals")), 0, wxEXPAND | wxALL, 6);
        page->SetSizer(s);
        m_pTreatmentsNotebook->AddPage(page, _("Normals"));
    }
    // Curvature tab: live curvature-visualization panel (method + type +
    // "Apply visualization"), reparented into the notebook.
    m_pCurvaturePanel->Reparent(m_pTreatmentsNotebook);
    m_pTreatmentsNotebook->AddPage(m_pCurvaturePanel, _("Curvature"));

    // Decimation tab: the full parameters panel, reparented into the notebook.
    m_pDecimationPanel->Reparent(m_pTreatmentsNotebook);
    m_pTreatmentsNotebook->AddPage(m_pDecimationPanel, _("Decimation"));

    // Empilé à droite, sous "Model information" (Position 2 dans la même rangée).
    m_mgr.AddPane(m_pTreatmentsNotebook, wxAuiPaneInfo().Name(wxT("Treatments")).Caption(wxT("Treatments"))
                  .Right().Layer(1).Row(0).Position(2).BestSize(280, 360).MinSize(220, 180));


    m_dcDirectory = new wxGenericDirCtrl(this, ID_DIRCTRL, wxT(""), wxDefaultPosition, wxSize(142, 120), wxSIMPLE_BORDER | wxDIRCTRL_DIR_ONLY, wxT("All files (*.*)|*.*"), 0);
    {
        wxTreeCtrl* tree = m_dcDirectory->GetTreeCtrl();
        wxTreeItemIdValue cookie;
        wxTreeItemId child = tree->GetFirstChild(tree->GetRootItem(), cookie);
        while (child.IsOk()) {
            tree->Expand(child);
            child = tree->GetNextChild(tree->GetRootItem(), cookie);
        }
    }
    m_mgr.AddPane(m_dcDirectory, wxAuiPaneInfo().Name(wxT("Explorer")).Caption(wxT("Explorer")).Left());

    /*m_mgr.AddPane(m_dcDirectory, wxAuiPaneInfo().
        Name(wxT("test7")).Caption(wxT("Dir Tree")).
        Bottom().Layer(1).Position(1).
        CloseButton(true).MaximizeButton(true));
 */

    m_filesCtrl = new wxListCtrl(this, ID_FILESCTRL, wxDefaultPosition, wxDefaultSize, wxLC_SINGLE_SEL | wxLC_LIST);

    wxIcon iconObj(format_obj);
    wxIcon iconStl(format_stl);
    wxIcon icon3ds(format_3ds);

    wxImageList* pImageList = new wxImageList(16, 16, false, 3);
    pImageList->Add(wxBitmap(iconObj).ConvertToImage().Rescale(16, 16, wxIMAGE_QUALITY_HIGH));
    pImageList->Add(wxBitmap(iconStl).ConvertToImage().Rescale(16, 16, wxIMAGE_QUALITY_HIGH));
    pImageList->Add(wxBitmap(icon3ds).ConvertToImage().Rescale(16, 16, wxIMAGE_QUALITY_HIGH));
    m_filesCtrl->SetImageList(pImageList, wxIMAGE_LIST_SMALL);
    m_mgr.AddPane(m_filesCtrl, wxAuiPaneInfo().Name(wxT("Files")).Caption(wxT("Files")).Bottom());

    m_pWndLogging = CreateTextCtrl(wxT("Logging...\n"));
    m_mgr.AddPane(m_pWndLogging, wxAuiPaneInfo().Name(wxT("Logging Window")).Caption(wxT("Logging Window")).Bottom());
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

        case ID_ShowInformation:
            event.Check(m_mgr.GetPane(wxT("Information")).IsShown());
            break;
        case ID_ShowTreatments:
            event.Check(m_mgr.GetPane(wxT("Treatments")).IsShown());
            break;
        case ID_ShowLogging:
            event.Check(m_mgr.GetPane(wxT("Logging Window")).IsShown());
            break;
        case ID_ShowExplorer:
            event.Check(m_mgr.GetPane(wxT("Explorer")).IsShown());
            break;
    }
}

void MyFrame::OnShowWindow(wxCommandEvent& evt)
{
    wxString paneName;
    switch (evt.GetId())
    {
        case ID_ShowInformation: paneName = wxT("Information"); break;
        case ID_ShowTreatments:  paneName = wxT("Treatments"); break;
        case ID_ShowLogging:    paneName = wxT("Logging Window"); break;
        case ID_ShowExplorer:   paneName = wxT("Explorer"); break;
        default: return;
    }

    wxAuiPaneInfo& pane = m_mgr.GetPane(paneName);
    pane.Show(!pane.IsShown());

    if (evt.GetId() == ID_ShowExplorer)
    {
        wxAuiPaneInfo& filesPane = m_mgr.GetPane(wxT("Files"));
        filesPane.Show(pane.IsShown());
    }

    m_mgr.Update();
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
    wxWindow* page = ctrl->GetPage(evt.GetSelection());
    if (page->IsKindOf(CLASSINFO(wxHtmlWindow)))
    {
        int res = wxMessageBox(wxT("Are you sure you want to close/hide this notebook page?"),
                       wxT("wxAUI"),
                       wxYES_NO,
                       this);
        if (res != wxYES)
            evt.Veto();
        return;
    }

    // Drop the per-canvas state tied to the page being closed so a future
    // canvas allocated at the same address cannot inherit stale associations.
    MyGLCanvas* pCanvas = (MyGLCanvas*)page;
    if (m_pParamPanel)
    {
        auto it = m_paramByCanvas.find(pCanvas);
        if (it != m_paramByCanvas.end())
        {
            // Unbind the panel if it is currently showing this object.
            m_pParamPanel->Bind(nullptr);
            m_paramByCanvas.erase(it);
        }
    }
    m_curvatureByCanvas.erase(pCanvas);
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
        // Le filtre et l'icône dérivent du catalogue central (SupportedFormats.h),
        // partagé avec le wildcard du wxFileDialog. -1 => format non importable.
        const int icon = sinaia::IconForExtension(filename.GetExt());
        if (icon >= 0)
            m_filesCtrl->InsertItem(index++, filename.GetName() + _T(".") + filename.GetExt(), icon);
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

void MyFrame::OnFilesCtrlBeginDrag(wxListEvent& event)
{
    const long item = event.GetIndex();
    if (item < 0)
        return;

    wxListItem info;
    info.SetId(item);
    info.SetMask(wxLIST_MASK_TEXT);
    if (!m_filesCtrl->GetItem(info))
        return;

    // Chemin complet = dossier de l'Explorer + nom du fichier (même reconstruction
    // que le double-clic). On glisse au format INTERNE SinaiaModelPathFormat (et NON
    // wxFileDataObject) : seul le canvas reconnaît ce format et appelle AppendModel.
    const wxString path = m_dcDirectory->GetPath() + wxT("\\") + info.m_text;
    const wxScopedCharBuffer utf8 = path.ToUTF8();
    wxCustomDataObject data(SinaiaModelPathFormat());
    data.SetData(utf8.length(), utf8.data());
    wxDropSource source(data, m_filesCtrl);
    source.DoDragDrop(wxDrag_CopyOnly);
}

void MyFrame::OnNotebookPageChanged(wxAuiNotebookEvent& event)
{
    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (pGLCanvas)
    {
        pGLCanvas->ResetProjectionMode();
        //pGLCanvas->DrawGL();

        // Apply the current 3D-view line/point size to this tab (covers tabs
        // freshly created, which fire PAGE_CHANGED via AddPage).
        pGLCanvas->SetLineWidth(m_lineWidth);
        pGLCanvas->SetPointSize(m_pointSize);
        pGLCanvas->SetLineColor (m_lineColor.Red()   / 255.0f,
                                 m_lineColor.Green() / 255.0f,
                                 m_lineColor.Blue()  / 255.0f);
        pGLCanvas->SetPointColor(m_pointColor.Red()   / 255.0f,
                                 m_pointColor.Green() / 255.0f,
                                 m_pointColor.Blue()  / 255.0f);

        // Synchronize toolbar button states
        wxAuiToolBarItem* p;

        p = m_pToolBar2->FindTool(ID_3D_FRAME);
        if (p) p->SetSticky(pGLCanvas->GetRepere());

        p = m_pToolBar2->FindTool(ID_3D_GRID);
        if (p) p->SetSticky(pGLCanvas->GetGrid());

        p = m_pToolBar2->FindTool(ID_3D_FILL);
        if (p) p->SetSticky(pGLCanvas->GetFill());

        p = m_pToolBar2->FindTool(ID_3D_WIREFRAME);
        if (p) p->SetSticky(pGLCanvas->GetWireframe());

        p = m_pToolBar2->FindTool(ID_3D_POINT);
        if (p) p->SetSticky(pGLCanvas->GetPoint());

        p = m_pToolBar2->FindTool(ID_3D_LIGHTING);
        if (p) p->SetSticky(pGLCanvas->GetLighting());

        p = m_pToolBar2->FindTool(ID_3D_SMOOTH);
        if (p) p->SetSticky(pGLCanvas->GetSmooth());

        p = m_pToolBar2->FindTool(ID_3D_FLAT);
        if (p) p->SetSticky(!pGLCanvas->GetSmooth());

        p = m_pToolBar2->FindTool(ID_3D_CLIPPING);
        if (p) p->SetSticky(pGLCanvas->GetClippingPlane());

        m_pToolBar2->Refresh();

        // Re-bind the Parameters panel to the parameterized object
        // associated with the newly active tab, if any.
        if (m_pParamPanel)
        {
            auto it = m_paramByCanvas.find(pGLCanvas);
            m_pParamPanel->Bind(it != m_paramByCanvas.end() ? it->second.get() : nullptr);
        }

        // Reflect this tab's stored curvature selection in the panel (or the
        // defaults if curvature was never configured for this tab).
        if (m_pCurvaturePanel)
        {
            auto it = m_curvatureByCanvas.find(pGLCanvas);
            if (it != m_curvatureByCanvas.end())
            {
                m_pCurvaturePanel->SetSelection(it->second.method, it->second.type);
                m_pCurvaturePanel->SetEnabled(it->second.enabled);
            }
            else
            {
                m_pCurvaturePanel->SetSelection(TENSOR_TAUBIN, CurvatureType::Mean);
                m_pCurvaturePanel->SetEnabled(false);
            }
        }
    }
    UpdatePropertiesGrid();
    UpdateContextualPanes();
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
    // Dérivé du catalogue central (SupportedFormats.h), partagé avec le panneau "Files".
    const wxString wildcard = sinaia::BuildOpenWildcard();

    wxFileDialog fd(this, wxT("Open 3D Model"), wxEmptyString, wxEmptyString, wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

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

    MyGLCanvas* pGLCanvas = new MyGLCanvas(m_pCtrl, m_pWndLogging, (int*)MyGLCanvas::GetDefaultAttributes());
    pGLCanvas->LoadModel(strFilename, m_importSettings);

    wxArrayString aString = wxSplit(strFilename, '\\');
    wxString title = aString.back();
    m_pCtrl->AddPage(pGLCanvas, title, true);

    // update properties grid
    UpdatePropertiesGrid();
   

    wxAuiToolBarItem* p;
    bool b;

    p = m_pToolBar2->FindTool(ID_3D_FRAME);
    b = pGLCanvas->GetRepere();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_GRID);
    b = pGLCanvas->GetGrid();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_FILL);
    b = pGLCanvas->GetFill();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_WIREFRAME);
    b = pGLCanvas->GetWireframe();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_POINT);
    b = pGLCanvas->GetPoint();
    p->SetSticky(b);

    p = m_pToolBar2->FindTool(ID_3D_CLIPPING);
    b = pGLCanvas->GetClippingPlane();
    p->SetSticky(b);

    m_pToolBar2->Refresh();

    UpdateContextualPanes();
}

void MyFrame::OnSceneChanged()
{
    UpdatePropertiesGrid();
    UpdateContextualPanes();
}

// Dessine les icônes œil / œil barré / poubelle (16x16) une seule fois. Fond blanc
// rendu transparent par un masque -> s'intègre aux boutons quel que soit le thème.
void MyFrame::BuildModelsIcons()
{
    auto make = [](void(*draw)(wxMemoryDC&)) -> wxBitmap {
        wxBitmap bmp(16, 16);
        wxMemoryDC dc(bmp);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        draw(dc);
        dc.SelectObject(wxNullBitmap);
        bmp.SetMask(new wxMask(bmp, *wxWHITE));
        return bmp;
    };
    m_iconEye = make([](wxMemoryDC& dc) {                 // œil ouvert
        dc.DrawEllipse(1, 4, 14, 8);
        dc.SetBrush(*wxBLACK_BRUSH);
        dc.DrawCircle(8, 8, 2);
    });
    m_iconEyeOff = make([](wxMemoryDC& dc) {              // œil barré (masqué)
        dc.DrawEllipse(1, 4, 14, 8);
        dc.SetBrush(*wxBLACK_BRUSH);
        dc.DrawCircle(8, 8, 2);
        dc.DrawLine(2, 13, 14, 3);
    });
    m_iconTrash = make([](wxMemoryDC& dc) {               // poubelle
        dc.DrawLine(3, 4, 13, 4);                         // couvercle
        dc.DrawLine(6, 2, 10, 2);                         // poignée
        dc.DrawRectangle(4, 5, 8, 9);                     // bac
        dc.DrawLine(7, 7, 7, 12);                         // stries
        dc.DrawLine(9, 7, 9, 12);
    });
    m_iconRefresh = make([](wxMemoryDC& dc) {             // flèche circulaire (recharger)
        dc.DrawEllipticArc(3, 3, 10, 10, 55, 340);        // cercle ouvert en haut-droite
        dc.SetBrush(*wxBLACK_BRUSH);
        wxPoint head[3] = { wxPoint(9, 2), wxPoint(14, 3), wxPoint(11, 7) };  // pointe de flèche
        dc.DrawPolygon(3, head);
    });
}

void MyFrame::UpdateModelsList()
{
    if (!m_modelsPanel)
        return;
    m_modelsPanel->DestroyChildren();
    m_modelRowLabels.clear();
    wxSizer* sizer = m_modelsPanel->GetSizer();
    sizer->Clear();

    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    VModels* scene = pGLCanvas ? pGLCanvas->GetVModels() : nullptr;
    if (scene)
    {
        // Pas d'auto-sélection : la sélection est posée à l'ouverture (SetVMeshes) et
        // par les clics ; retirer le Model sélectionné la laisse VIDE (info vidée).
        const Model* selected = pGLCanvas->GetSelectedModel();

        // Une ligne par Model (indices stables : ligne i == Model i). Les boutons
        // capturent i ; toute modif de la scène rappelle UpdateModelsList -> réindex.
        int idx = 0;
        for (auto& mdl : scene->GetModels())
        {
            const int i = idx++;
            wxString label = mdl->m_name.empty() ? wxString::Format("model %d", i)
                                                 : wxString(mdl->m_name);

            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
            wxStaticText* lbl = new wxStaticText(m_modelsPanel, wxID_ANY, label);
            m_modelRowLabels.push_back(lbl);   // pour la surbrillance au survol
            // Ligne sélectionnée en GRAS (le survol, lui, ne change que la couleur).
            if (mdl.get() == selected)
            {
                wxFont f = lbl->GetFont(); f.MakeBold(); lbl->SetFont(f);
            }
            // Clic sur le libellé -> sélectionne ce Model (met à jour Model information).
            lbl->Bind(wxEVT_LEFT_DOWN, [this, i](wxMouseEvent&){ SelectModelRow(i); });
            row->Add(lbl, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

            wxBitmapButton* eye = new wxBitmapButton(m_modelsPanel, wxID_ANY,
                mdl->m_visible ? m_iconEye : m_iconEyeOff, wxDefaultPosition,
                wxDefaultSize, wxBU_EXACTFIT | wxBORDER_NONE);
            eye->SetToolTip(_("Afficher / masquer"));
            eye->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){ ToggleModelVisibility(i); });
            row->Add(eye, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);

            // Bouton « recharger » : uniquement pour les Model issus d'un fichier
            // (m_path renseigné) ; les géométries générées n'ont rien à relire.
            if (!mdl->m_path.empty())
            {
                wxBitmapButton* refresh = new wxBitmapButton(m_modelsPanel, wxID_ANY,
                    m_iconRefresh, wxDefaultPosition, wxDefaultSize,
                    wxBU_EXACTFIT | wxBORDER_NONE);
                refresh->SetToolTip(_("Recharger depuis le fichier"));
                refresh->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){ RefreshModelAt(i); });
                row->Add(refresh, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
            }

            wxBitmapButton* trash = new wxBitmapButton(m_modelsPanel, wxID_ANY,
                m_iconTrash, wxDefaultPosition, wxDefaultSize,
                wxBU_EXACTFIT | wxBORDER_NONE);
            trash->SetToolTip(_("Supprimer"));
            trash->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){ RemoveModelAt(i); });
            row->Add(trash, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

            sizer->Add(row, 0, wxEXPAND | wxTOP, 2);
        }
    }
    m_modelsPanel->FitInside();
    m_modelsPanel->Layout();
}

void MyFrame::HighlightModelRow(int index)
{
    const wxColour normal = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const wxColour hi(200, 120, 0);   // orange = ligne survolée dans la vue 3D
    for (int i = 0; i < (int)m_modelRowLabels.size(); ++i)
    {
        wxStaticText* lbl = m_modelRowLabels[i];
        if (!lbl) continue;
        lbl->SetForegroundColour(i == index ? hi : normal);
        lbl->Refresh();
    }
}

void MyFrame::SelectModelRow(int index)
{
    MyGLCanvas* canvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (!canvas || !canvas->GetVModels())
        return;
    Model* mdl = canvas->GetVModels()->GetModel((size_t)index);
    if (!mdl)
        return;
    canvas->SetSelectedModel(mdl);
    UpdateModelsList();       // restyle (gras sur la ligne sélectionnée)
    UpdatePropertiesGrid();   // Model information suit la sélection
}

void MyFrame::ToggleModelVisibility(int index)
{
    MyGLCanvas* canvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (!canvas || !canvas->GetVModels())
        return;
    Model* mdl = canvas->GetVModels()->GetModel((size_t)index);
    if (!mdl)
        return;
    mdl->m_visible = !mdl->m_visible;
    canvas->Refresh(false);
    UpdateModelsList();   // met à jour l'icône œil / œil barré
}

void MyFrame::RemoveModelAt(int index)
{
    MyGLCanvas* canvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (!canvas || !canvas->GetVModels())
        return;

    // Détache les maillages du renderer avant destruction (le MeshRenderer indexe
    // par Mesh* : un pointeur détruit resté indexé provoquerait un accès invalide).
    Model* mdl = canvas->GetVModels()->GetModel((size_t)index);
    if (mdl)
        for (auto* mesh : mdl->m_meshes.GetMeshes())
            if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);

    canvas->ClearHoveredModel();   // le Model survolé peut être celui qu'on retire
    if (canvas->GetSelectedModel() == mdl)
        canvas->SetSelectedModel(nullptr);   // évite un pointeur pendouillant
    canvas->GetVModels()->Remove((size_t)index);
    canvas->Refresh(false);
    OnSceneChanged();     // reconstruit la liste + panneaux
}

void MyFrame::RefreshModelAt(int index)
{
    MyGLCanvas* canvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (!canvas || !canvas->GetVModels())
        return;
    Model* mdl = canvas->GetVModels()->GetModel((size_t)index);
    if (!mdl)
        return;

    // ReloadModel détache les anciens Mesh* du renderer, relit le fichier en
    // place (le Model* et donc la sélection restent valides) et recadre la
    // caméra seulement si ce Model est seul dans la scène.
    canvas->ReloadModel(mdl);

    // Reconstruit la liste + Model information (counts, bbox, etc. ont changé).
    OnSceneChanged();
}

void MyFrame::UpdatePropertiesGrid()
{
    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (!pGLCanvas)
        return;

    // Reconstruit d'abord le panneau "Models" (qui auto-sélectionne le 1er Model si
    // aucun ne l'est encore).
    UpdateModelsList();

    // Panneau "Model information" : infos du Model SÉLECTIONNÉ dans "Models". Si rien
    // n'est sélectionné (ex. le Model sélectionné vient d'être supprimé) -> infos VIDES.
    Model* selected = pGLCanvas->GetSelectedModel();
    if (!selected)
    {
        m_propertiesGrid->ChangePropertyValue("File",               wxVariant(wxString()));
        if (wxPGProperty* fileProp = m_propertiesGrid->GetProperty("File"))
            fileProp->SetHelpString(wxString());
        m_propertiesGrid->ChangePropertyValue("Vertices",           wxVariant(0));
        m_propertiesGrid->ChangePropertyValue("Faces",              wxVariant(0));
        m_propertiesGrid->ChangePropertyValue("Meshes",             wxVariant(0));
        m_propertiesGrid->ChangePropertyValue("Triangle mesh",      wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Borders",            wxVariant(0));
        m_propertiesGrid->ChangePropertyValue("Non manifold edges", wxVariant(0));
        m_propertiesGrid->ChangePropertyValue("Bounding box X",     wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Bounding box Y",     wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Bounding box Z",     wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Size X",             wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Size Y",             wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Size Z",             wxVariant(wxString()));
        if (m_hierarchyMeshes)    m_hierarchyMeshes->DeleteAllItems();
        if (m_hierarchyMaterials) m_hierarchyMaterials->DeleteAllItems();
        return;
    }
    VMeshes* pObject = &selected->m_meshes;

    // Source file: value = basename, full path shown as tooltip (help string).
    m_propertiesGrid->ChangePropertyValue("File", wxVariant(wxString::FromUTF8(selected->m_name.c_str())));
    if (wxPGProperty* fileProp = m_propertiesGrid->GetProperty("File"))
        fileProp->SetHelpString(wxString::FromUTF8(selected->m_path.c_str()));

    m_propertiesGrid->ChangePropertyValue("Vertices", wxVariant(static_cast<int>(pObject->GetNVertices())));
    m_propertiesGrid->ChangePropertyValue("Faces", wxVariant(static_cast<int>(pObject->GetNFaces())));
    m_propertiesGrid->ChangePropertyValue("Meshes", wxVariant(static_cast<int>(pObject->GetNMeshes())));
    const bool bIsTriangleMesh = pObject->IsTriangleMesh();
    m_propertiesGrid->ChangePropertyValue("Triangle mesh", wxVariant(bIsTriangleMesh? wxString("True") : wxString("False")));

    m_propertiesGrid->ChangePropertyValue("Borders", wxVariant(static_cast<int>(pGLCanvas->GetNBorders())));
    m_propertiesGrid->ChangePropertyValue("Non manifold edges", wxVariant(static_cast<int>(pGLCanvas->GetNNonManifoldEdges())));

    // Bounding box of the selected model: one "[ min , max ]" row per axis, plus
    // a separate size row per axis.
    const BoundingBox& bbox = selected->ComputeBBox();
    if (bbox.IsEmpty())
    {
        m_propertiesGrid->ChangePropertyValue("Bounding box X", wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Bounding box Y", wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Bounding box Z", wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Size X",         wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Size Y",         wxVariant(wxString()));
        m_propertiesGrid->ChangePropertyValue("Size Z",         wxVariant(wxString()));
    }
    else
    {
        float mn[3], mx[3];
        bbox.GetMinMax(mn, mx);
        auto rangeText = [](float lo, float hi) {
            return wxString::Format(wxT("[ %.4g , %.4g ]"), lo, hi);
        };
        auto sizeText = [](float lo, float hi) {
            return wxString::Format(wxT("%.4g"), hi - lo);
        };
        m_propertiesGrid->ChangePropertyValue("Bounding box X", wxVariant(rangeText(mn[0], mx[0])));
        m_propertiesGrid->ChangePropertyValue("Bounding box Y", wxVariant(rangeText(mn[1], mx[1])));
        m_propertiesGrid->ChangePropertyValue("Bounding box Z", wxVariant(rangeText(mn[2], mx[2])));
        m_propertiesGrid->ChangePropertyValue("Size X",         wxVariant(sizeText(mn[0], mx[0])));
        m_propertiesGrid->ChangePropertyValue("Size Y",         wxVariant(sizeText(mn[1], mx[1])));
        m_propertiesGrid->ChangePropertyValue("Size Z",         wxVariant(sizeText(mn[2], mx[2])));
    }

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

    if (m_hierarchyMaterials)
    {
        m_hierarchyMaterials->DeleteAllItems();
        wxTreeItemId root = m_hierarchyMaterials->AddRoot(wxT("Materials"), 0);

        for (auto& mesh : pObject->GetMeshes())
        {
            if (mesh->GetNMaterials() > 0)
            {
                wxTreeItemId meshItem = m_hierarchyMaterials->AppendItem(root, wxString(mesh->m_name), 0);
                for (unsigned int i = 0; i < mesh->GetNMaterials(); i++)
                {
                    Material* mat = mesh->GetMaterial(i);
                    if (mat)
                    {
                        wxString matName = mat->GetName().empty() ? wxString::Format("Material %u", i) : wxString(mat->GetName());
                        wxTreeItemId matItem = m_hierarchyMaterials->AppendItem(meshItem, matName, 1);
                        
                        // Add some details based on type
                        switch (mat->GetType())
                        {
                        case MATERIAL_COLOR:
                        {
                            MaterialColor* mc = dynamic_cast<MaterialColor*>(mat);
                            if (mc)
                            {
                                m_hierarchyMaterials->AppendItem(matItem, wxString::Format("Color: RGB(%.2f, %.2f, %.2f)", mc->GetFloatRed(), mc->GetFloatGreen(), mc->GetFloatBlue()), 1);
                            }
                            break;
                        }
                        case MATERIAL_TEXTURE:
                        {
                            MaterialTexture* mt = dynamic_cast<MaterialTexture*>(mat);
                            if (mt)
                            {
                                m_hierarchyMaterials->AppendItem(matItem, wxString::Format("Texture: %s", mt->GetFilename()), 1);
                            }
                            break;
                        }
                        case MATERIAL_COLOR_ADV:
                        {
                            MaterialColorExt* mce = dynamic_cast<MaterialColorExt*>(mat);
                            if (mce)
                            {
                                m_hierarchyMaterials->AppendItem(matItem, wxString::Format("Type: Advanced Color"), 1);
                                m_hierarchyMaterials->AppendItem(matItem, wxString::Format("  Diffuse: RGB(%.2f, %.2f, %.2f)", mce->m_fDiffuse[0], mce->m_fDiffuse[1], mce->m_fDiffuse[2]), 1);
                                m_hierarchyMaterials->AppendItem(matItem, wxString::Format("  Ambient: RGB(%.2f, %.2f, %.2f)", mce->m_fAmbient[0], mce->m_fAmbient[1], mce->m_fAmbient[2]), 1);
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
                m_hierarchyMaterials->Expand(meshItem);
            }
        }
        m_hierarchyMaterials->Expand(root);
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

void MyFrame::OnExportImage(wxCommandEvent& WXUNUSED(event))
{
    int sel = m_pCtrl->GetSelection();
    if (sel < 0)
        return;

    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(sel);
    if (!pGLCanvas)
        return;

    wxFileDialog fd(this, _("Export image as..."), wxEmptyString, wxEmptyString,
        _("PNG files (*.png)|*.png|BMP files (*.bmp)|*.bmp"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (fd.ShowModal() == wxID_OK)
    {
        wxString strFilename = fd.GetPath();
        *m_pWndLogging << _T("Exporting image as ") << strFilename << _T("\n");

        // Capture OpenGL buffer
        int w, h;
        pGLCanvas->GetClientSize(&w, &h);

        if (w <= 0 || h <= 0)
            return;

        pGLCanvas->SetCurrent(*pGLCanvas->m_context);
        pGLCanvas->Refresh();
        pGLCanvas->Update();

        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        std::vector<unsigned char> pixels(3 * w * h);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

        // Flip image vertically (OpenGL starts at bottom-left)
        // wxImage(int, int, unsigned char*, bool) takes ownership of the pointer 
        // and will call free() on it, so we must use malloc.
        unsigned char* flipped_pixels = (unsigned char*)malloc(3 * w * h);
        if (!flipped_pixels) {
            return;
        }

        for (int y = 0; y < h; y++)
        {
            memcpy(flipped_pixels + 3 * w * y, pixels.data() + 3 * w * (h - 1 - y), 3 * w);
        }

        wxImage img(w, h, flipped_pixels, false); 
        if (img.SaveFile(strFilename))
        {
            *m_pWndLogging << _T("Successfully exported image to ") << strFilename << _T("\n");
        }
        else
        {
            wxMessageBox(_("Failed to save the image."), _("Error"), wxOK | wxICON_ERROR, this);
            *m_pWndLogging << _T("Error: Failed to export image to ") << strFilename << _T("\n");
        }
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

void MyFrame::OnNewGeometry(wxCommandEvent& event)
{
	Mesh* pMesh = nullptr;
	wxString title;

	switch (event.GetId())
	{
	case ID_GEOMETRY_NEW_CUBE:
		pMesh = CreateCube();
		title = wxT("cube");
		break;

	case ID_GEOMETRY_NEW_SPHERE:
		pMesh = new ParametricSphere(20, 20);
		title = wxT("sphere");
		break;

	case ID_GEOMETRY_NEW_CYLINDER:
		pMesh = CreateCylinder(2.f, 1.f, 32, true);
		title = wxT("cylinder");
		break;

	case ID_GEOMETRY_NEW_TEAPOT:
		pMesh = CreateTeapot();
		title = wxT("teapot");
		break;

	case ID_GEOMETRY_NEW_KLEIN_BOTTLE:
		pMesh = CreateKleinBottle(20, 20);
		title = wxT("klein bottle");
		break;

	default:
		return;
	}

	if (!pMesh)
		return;

	MyGLCanvas* pGLCanvas = new MyGLCanvas(m_pCtrl, m_pWndLogging, (int*)MyGLCanvas::GetDefaultAttributes());
	auto pVMeshes = new VMeshes();
	pVMeshes->AddMesh(pMesh);
	pGLCanvas->SetVMeshes(pVMeshes);
	m_pCtrl->AddPage(pGLCanvas, title, true);

	UpdatePropertiesGrid();
	UpdateContextualPanes();
}

//
// Create a parameterized geometry: bind it to the Parameters panel and
// render it in a new tab. Editing a parameter in the panel triggers
// Regenerate() and refreshes the view.
//
void MyFrame::OnNewParameterizedGeometry(wxCommandEvent& event)
{
	std::unique_ptr<IParameterized> pParam;
	wxString title;

	#define CASE(ID, ClassName, Label) \
		case ID: pParam = std::make_unique<ClassName>(); title = wxT(Label); break

	switch (event.GetId())
	{
	CASE(ID_GEOMETRY_NEW_PARAM_CUBE,                  ParameterizedCube,                  "cube");
	CASE(ID_GEOMETRY_NEW_PARAM_SPHERE,                ParameterizedSphere,                "sphere");
	CASE(ID_GEOMETRY_NEW_PARAM_CYLINDER,              ParameterizedCylinder,              "cylinder");
	CASE(ID_GEOMETRY_NEW_PARAM_CONE,                  ParameterizedCone,                  "cone");
	CASE(ID_GEOMETRY_NEW_PARAM_CAPSULE,               ParameterizedCapsule,               "capsule");
	CASE(ID_GEOMETRY_NEW_PARAM_TORUS,                 ParameterizedTorus,                 "torus");
	CASE(ID_GEOMETRY_NEW_PARAM_KLEIN_BOTTLE,          ParameterizedKleinBottle,           "klein bottle");
	CASE(ID_GEOMETRY_NEW_PARAM_HELICOID,              ParameterizedHelicoid,              "helicoid");
	CASE(ID_GEOMETRY_NEW_PARAM_SEASHELL,              ParameterizedSeashell,              "seashell");
	CASE(ID_GEOMETRY_NEW_PARAM_SEASHELL_VON_SEGGERN,  ParameterizedSeashellVonSeggern,    "seashell (von Seggern)");
	CASE(ID_GEOMETRY_NEW_PARAM_CORKSCREW,             ParameterizedCorkscrew,             "corkscrew");
	CASE(ID_GEOMETRY_NEW_PARAM_MOBIUS_STRIP,          ParameterizedMobiusStrip,           "mobius strip");
	CASE(ID_GEOMETRY_NEW_PARAM_RADIAL_WAVE,           ParameterizedRadialWave,            "radial wave");
	CASE(ID_GEOMETRY_NEW_PARAM_BREATHER,              ParameterizedBreather,              "breather");
	CASE(ID_GEOMETRY_NEW_PARAM_HYPERBOLIC_PARABOLOID, ParameterizedHyperbolicParaboloid,  "hyperbolic paraboloid");
	CASE(ID_GEOMETRY_NEW_PARAM_MONKEY_SADDLE,         ParameterizedMonkeySaddle,          "monkey saddle");
	CASE(ID_GEOMETRY_NEW_PARAM_BLOBS,                 ParameterizedBlobs,                 "blobs");
	CASE(ID_GEOMETRY_NEW_PARAM_DROP,                  ParameterizedDrop,                  "drop");
	CASE(ID_GEOMETRY_NEW_PARAM_GUIMARD,               ParameterizedGuimard,               "guimard");
	CASE(ID_GEOMETRY_NEW_PARAM_TORUS_KNOT,            ParameterizedTorusKnot,             "torus knot");
	CASE(ID_GEOMETRY_NEW_PARAM_CINQUEFOIL_KNOT,       ParameterizedCinquefoilKnot,        "cinquefoil knot");
	CASE(ID_GEOMETRY_NEW_PARAM_TREFOIL_KNOT,          ParameterizedTrefoilKnot,           "trefoil knot");
	CASE(ID_GEOMETRY_NEW_PARAM_BORROMEAN_RINGS,       ParameterizedBorromeanRings,        "borromean rings");
	CASE(ID_GEOMETRY_NEW_PARAM_MENGER_SPONGE,         ParameterizedMengerSponge,          "menger sponge");
	default:
		return;
	}

	#undef CASE

	// Build the render tab: take the initial mesh (if the object produces
	// one) and install it in a fresh VMeshes on a new GL canvas.
	Mesh *pMesh = pParam->TakeMesh();

	MyGLCanvas *pCanvas = new MyGLCanvas(m_pCtrl, m_pWndLogging,
	                                     (int*)MyGLCanvas::GetDefaultAttributes());
	auto *pVMeshes = new VMeshes();
	if (pMesh)
		pVMeshes->AddMesh(pMesh);
	pCanvas->SetVMeshes(pVMeshes);
	m_pCtrl->AddPage(pCanvas, title, true);

	// Register the canvas -> param association and bind the panel.
	// Page-changed events fired by AddPage will also call Bind, but we
	// re-bind explicitly to cover the case of adding into an empty notebook.
	IParameterized *pRaw = pParam.get();
	m_paramByCanvas[pCanvas] = std::move(pParam);
	m_pParamPanel->Bind(pRaw);

	UpdatePropertiesGrid();
	UpdateContextualPanes();
}

//
// SVG extrusion: open a file dialog, instantiate ParameterizedSvgExtrusion
// with the chosen path, and bind it to the Properties panel exactly like
// the other parameterized geometries.
//
void MyFrame::OnNewParameterizedSvg(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog dlg(this, _("Select an SVG file"), wxEmptyString, wxEmptyString,
	                 wxT("SVG files (*.svg)|*.svg|All files (*.*)|*.*"),
	                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (dlg.ShowModal() != wxID_OK)
		return;

	const std::string path = std::string(dlg.GetPath().mb_str(wxConvUTF8));
	auto pParam = std::make_unique<ParameterizedSvgExtrusion>(path);

	Mesh* pMesh = pParam->TakeMesh();
	if (!pMesh)
	{
		wxMessageBox(_("Failed to import SVG (file unreadable or no fillable shapes)."),
		             _("SVG import error"), wxOK | wxICON_ERROR, this);
		return;
	}

	MyGLCanvas* pCanvas = new MyGLCanvas(m_pCtrl, m_pWndLogging,
	                                     (int*)MyGLCanvas::GetDefaultAttributes());
	auto* pVMeshes = new VMeshes();
	pVMeshes->AddMesh(pMesh);
	pCanvas->SetVMeshes(pVMeshes);
	m_pCtrl->AddPage(pCanvas, dlg.GetFilename(), true);

	IParameterized* pRaw = pParam.get();
	m_paramByCanvas[pCanvas] = std::move(pParam);
	m_pParamPanel->Bind(pRaw);

	UpdatePropertiesGrid();
	UpdateContextualPanes();
}

//
// Implicit surface from a point cloud: open a file dialog, instantiate
// ParameterizedImplicitFromPoints with the chosen PLY, and bind it to the
// Properties panel exactly like the SVG extrusion. The two editable
// parameters are the marching-cubes resolution and the iso-surface distance.
//
void MyFrame::OnNewParameterizedImplicit(wxCommandEvent& WXUNUSED(event))
{
	wxFileDialog dlg(this, _("Select a point cloud (PLY)"), wxEmptyString, wxEmptyString,
	                 wxT("PLY point clouds (*.ply)|*.ply|All files (*.*)|*.*"),
	                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (dlg.ShowModal() != wxID_OK)
		return;

	const std::string path = std::string(dlg.GetPath().mb_str(wxConvUTF8));

	// The constructor loads the cloud and runs the first Regenerate(); time it.
	auto t0 = std::chrono::high_resolution_clock::now();
	auto pParam = std::make_unique<ParameterizedImplicitFromPoints>(path);
	auto t1 = std::chrono::high_resolution_clock::now();
	const double buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

	Mesh* pMesh = pParam->TakeMesh();
	if (!pMesh)
	{
		wxMessageBox(_("Failed to load point cloud (file unreadable or no points)."),
		             _("Point cloud import error"), wxOK | wxICON_ERROR, this);
		return;
	}

	Log(wxString::Format(_T("Implicit surface from %s built in %.1f ms (%u faces, %u vertices)"),
	                     dlg.GetFilename(), buildMs, pMesh->GetNFaces(), pMesh->GetNVertices()));

	MyGLCanvas* pCanvas = new MyGLCanvas(m_pCtrl, m_pWndLogging,
	                                     (int*)MyGLCanvas::GetDefaultAttributes());
	auto* pVMeshes = new VMeshes();
	pVMeshes->AddMesh(pMesh);
	pCanvas->SetVMeshes(pVMeshes);
	m_pCtrl->AddPage(pCanvas, dlg.GetFilename(), true);

	IParameterized* pRaw = pParam.get();
	m_paramByCanvas[pCanvas] = std::move(pParam);
	m_pParamPanel->Bind(pRaw);

	UpdatePropertiesGrid();
	UpdateContextualPanes();
}

//
// Called by PropertyPanel after a parameter edit has triggered
// IParameterized::Regenerate(). Builds a fresh VMeshes around the newly
// generated mesh and installs it on the active canvas. SetVMeshes()
// deletes the previous VMeshes, normalizes the new mesh (bbox, translate,
// scale) and refreshes the GL view.
//
void MyFrame::OnParameterChanged()
{
	// Regenerate() already ran inside the panel; report how long it took.
	const double regenMs = m_pParamPanel ? m_pParamPanel->GetLastRegenMs() : 0.0;

	int sel = m_pCtrl->GetSelection();
	if (sel < 0)
		return;
	MyGLCanvas *pCanvas = (MyGLCanvas*)m_pCtrl->GetPage(sel);
	if (!pCanvas)
		return;

	auto it = m_paramByCanvas.find(pCanvas);
	if (it == m_paramByCanvas.end())
		return;

	const wxString name(it->second->GetName());

	Mesh *pNewMesh = it->second->TakeMesh();
	if (!pNewMesh)
	{
		Log(wxString::Format(_T("%s regenerated in %.1f ms"), name, regenMs));
		return;  // param object does not produce a mesh (e.g. Menger sponge)
	}

	const unsigned int nf = pNewMesh->GetNFaces();
	const unsigned int nv = pNewMesh->GetNVertices();

	auto *pNewVMeshes = new VMeshes();
	pNewVMeshes->AddMesh(pNewMesh);
	pCanvas->SetVMeshes(pNewVMeshes);  // deletes old VMeshes, normalizes, refreshes

	// The mesh was rebuilt: any curvature colouring is gone and its tensors no
	// longer apply. Drop the curvature state and reset the panel for this tab
	// (it is the active one).
	if (m_curvatureByCanvas.erase(pCanvas) > 0 && m_pCurvaturePanel)
	{
		m_pCurvaturePanel->SetSelection(TENSOR_TAUBIN, CurvatureType::Mean);
		m_pCurvaturePanel->SetEnabled(false);
	}

	Log(wxString::Format(_T("%s regenerated in %.1f ms (%u faces, %u vertices)"),
	                     name, regenMs, nf, nv));
}

//
// File > Settings: edit the operations applied to a model on import. The
// new settings take effect for subsequently imported models only; models
// already open are left untouched.
//
void MyFrame::OnSettings(wxCommandEvent& WXUNUSED(event))
{
	// Seed the dialog with the current import options and the live notebook
	// appearance options (the "2D Panel" tab).
	PanelSettings panel;
	panel.notebookStyle = m_notebook_style;
	panel.notebookTheme = static_cast<int>(m_notebook_theme);
	panel.lineWidth     = m_lineWidth;
	panel.pointSize     = m_pointSize;
	panel.lineColor     = m_lineColor;
	panel.pointColor    = m_pointColor;

	// The 2D Panel options are applied live as the user edits them; the dialog
	// calls back into ApplyPanelSettings on every change and restores this
	// initial state itself if the user cancels.
	SettingsDialog dlg(this, m_importSettings, panel,
	                   [this](const PanelSettings& p) { ApplyPanelSettings(p); });
	if (dlg.ShowModal() == wxID_OK)
	{
		m_importSettings = dlg.GetImportSettings();
		ApplyPanelSettings(dlg.GetPanelSettings());
	}
}

//
// Apply the notebook appearance options edited in the "2D Panel" tab of the
// Settings dialog: restyle every managed wxAuiNotebook with the chosen flags
// and tab art theme. (FPS overlay and 3D background colour stay in the
// "Options > 3D Panel" menu and are handled by their own live handlers.)
//
void MyFrame::ApplyPanelSettings(const PanelSettings& panel)
{
	m_notebook_style = panel.notebookStyle;
	m_notebook_theme = panel.notebookTheme;

	// 3D view line/point size: remember as the app default and push to every
	// open canvas (new tabs pick it up via OnNotebookPageChanged).
	m_lineWidth = panel.lineWidth;
	m_pointSize = panel.pointSize;
	m_lineColor  = panel.lineColor;
	m_pointColor = panel.pointColor;
	if (m_pCtrl)
	{
		for (size_t i = 0, n = m_pCtrl->GetPageCount(); i < n; ++i)
		{
			MyGLCanvas* pCanvas = (MyGLCanvas*)m_pCtrl->GetPage(i);
			if (pCanvas)
			{
				pCanvas->SetLineWidth(m_lineWidth);
				pCanvas->SetPointSize(m_pointSize);
				pCanvas->SetLineColor (m_lineColor.Red()   / 255.0f,
				                       m_lineColor.Green() / 255.0f,
				                       m_lineColor.Blue()  / 255.0f);
				pCanvas->SetPointColor(m_pointColor.Red()   / 255.0f,
				                       m_pointColor.Green() / 255.0f,
				                       m_pointColor.Blue()  / 255.0f);
			}
		}
	}

	wxAuiPaneInfoArray& all_panes = m_mgr.GetAllPanes();
	for (size_t i = 0, count = all_panes.GetCount(); i < count; ++i)
	{
		wxAuiPaneInfo& pane = all_panes.Item(i);
		if (pane.window->IsKindOf(CLASSINFO(wxAuiNotebook)))
		{
			wxAuiNotebook* nb = (wxAuiNotebook*)pane.window;
			nb->SetArtProvider(m_notebook_theme == 1
			                       ? static_cast<wxAuiTabArt*>(new wxAuiSimpleTabArt)
			                       : static_cast<wxAuiTabArt*>(new wxAuiDefaultTabArt));
			nb->SetWindowStyleFlag(m_notebook_style);
			nb->Refresh();
		}
	}
}

//
//
//
void MyFrame::On3DFrame(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Frame\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		pGLCanvas->ChangeRepere ();
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *p = m_pToolBar2->FindTool(ID_3D_FRAME);
		bool b = pGLCanvas->GetRepere ();
		p->SetSticky (b);
	}
}

//
//
//
void MyFrame::On3DGrid(wxCommandEvent& WXUNUSED(event))
{
	*m_pWndLogging << _T("Grid\n");
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection ());
	if (pGLCanvas)
	{
		pGLCanvas->ChangeGrid ();
		pGLCanvas->Refresh ();

		wxAuiToolBarItem *p = m_pToolBar2->FindTool(ID_3D_GRID);
		bool b = pGLCanvas->GetGrid ();
		p->SetSticky (b);
	}
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

void MyFrame::On3DClippingPlane(wxCommandEvent& WXUNUSED(event))
{
    *m_pWndLogging << _T("Clipping plane\n");
    MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
    if (pGLCanvas)
    {
        bool bActive = pGLCanvas->GetClippingPlane();
        pGLCanvas->SetClippingPlane(!bActive);
        pGLCanvas->Refresh();

        wxAuiToolBarItem* p = m_pToolBar2->FindTool(ID_3D_CLIPPING);
        bool b = pGLCanvas->GetClippingPlane();
        p->SetSticky(b);

        if (b)
            pGLCanvas->SetFocus();
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

void MyFrame::OnToggleShowFps(wxCommandEvent& event)
{
	m_bShowFps = event.IsChecked();
	wxStatusBar* sb = GetStatusBar();
	if (sb)
		sb->SetStatusText(m_bShowFps ? wxString(wxT("FPS: --")) : wxString(), 1);

	// Force a repaint so the next OnPaint kicks the counter into motion (or
	// clears any leftover text).
	MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (pGLCanvas)
		pGLCanvas->Refresh(false);
}

MyGLCanvas* MyFrame::GetActiveCanvas()
{
	if (!m_pCtrl) return nullptr;
	const int sel = m_pCtrl->GetSelection();
	if (sel < 0) return nullptr;
	return dynamic_cast<MyGLCanvas*>(m_pCtrl->GetPage(sel));
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
	{
		pGLCanvas->SetBackgroundColor (colNew.Red(), colNew.Green(), colNew.Blue());
		pGLCanvas->Refresh();
	}
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
	Mesh *geometry = nullptr;
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
				geometry = nullptr;
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

wxTreeCtrl* MyFrame::CreateHierarchyMaterialsTreeCtrl()
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

wxAuiNotebook* MyFrame::CreateNotebook(void)
{
   // create the notebook off-window to avoid flicker
   wxSize client_size = GetClientSize();

   m_pCtrl = new wxAuiNotebook(this, ID_NOTEBOOK_MAIN,
                                    wxPoint(client_size.x, client_size.y),
                                    wxSize(430,200),
                                    m_notebook_style);
   m_pCtrl->SetArtProvider(new wxAuiSimpleTabArt);

   m_pGLCanvas = new MyGLCanvas (m_pCtrl, m_pWndLogging, (int*)MyGLCanvas::GetDefaultAttributes());
   auto pVMeshes = new VMeshes();
   pVMeshes->AddMesh(CreateCube());
   m_pGLCanvas->SetVMeshes(pVMeshes);
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
   algo.EvaluateColors (CurvatureType::Gaussian);


   float *pHistogram = nullptr;
   algo.GetCurvaturesHistogram (CurvatureType::Max, 64, &pHistogram);
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
   algo.EvaluateColors (CurvatureType::Mean);
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

//
// Treatments
//
void MyFrame::OnUpdateUITreatmentMakeTriangles(wxUpdateUIEvent& event)
{
	if (!m_pCtrl || m_pCtrl->GetSelection() < 0)
	{
		event.Enable(false);
		return;
	}
	MyGLCanvas* pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
	{
		event.Enable(false);
		return;
	}
	VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes || pVMeshes->GetNMeshes() == 0)
	{
		event.Enable(false);
		return;
	}
	// Grey the entry when there's nothing left to triangulate.
	event.Enable(!pVMeshes->IsTriangleMesh());
}

void MyFrame::OnTreatmentMakeTriangles(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

	VMeshes * pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		const auto nFacesBefore = pMesh->GetNFaces();
		pMesh->Triangulate();
		const auto nFacesAfter  = pMesh->GetNFaces();

		*m_pWndLogging << wxString::Format(_T("Make triangles: %u -> %u faces\n"),
		                                    nFacesBefore, nFacesAfter);
	}

	pGLCanvas->UpdateTopologicIssues();
	pGLCanvas->Refresh();
	UpdatePropertiesGrid();
}

void MyFrame::OnTreatmentMergeVertices(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

    VMeshes * pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		auto nVerticesBefore = pMesh->GetNVertices();
		pMesh->MergeVertices();
		auto nVerticesAfter = pMesh->GetNVertices();

		pMesh->ComputeNormals();

		*m_pWndLogging << wxString::Format(_T("Merge vertices: %u -> %u\n"), nVerticesBefore, nVerticesAfter);
	}

	pGLCanvas->UpdateTopologicIssues();
	pGLCanvas->Refresh();
	UpdatePropertiesGrid();
}

void MyFrame::OnTreatmentNormalize(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

    VMeshes * pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	pGLCanvas->ApplyNormalization(true); // Call the new ApplyNormalization method

	pGLCanvas->Refresh();
	UpdatePropertiesGrid();
	*m_pWndLogging << _T("Model normalized\n");
}

void MyFrame::OnTreatmentSmoothingTaubin(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

	VMeshes *pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	MeshAlgoSmoothingTaubin algo;
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		Mesh_half_edge *pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
		algo.Apply(pMeshHE);

		// Copy smoothed vertices back
		for (unsigned int i = 0; i < pMesh->m_nVertices; i++)
		{
			pMesh->m_pVertices[3 * i]     = pMeshHE->m_pMesh->m_pVertices[3 * i];
			pMesh->m_pVertices[3 * i + 1] = pMeshHE->m_pMesh->m_pVertices[3 * i + 1];
			pMesh->m_pVertices[3 * i + 2] = pMeshHE->m_pMesh->m_pVertices[3 * i + 2];
		}
		pMesh->ComputeNormals();
		pMesh->IncrementRevision();
	}

	pGLCanvas->Refresh();
	*m_pWndLogging << _T("Smoothing Taubin applied\n");
}

void MyFrame::OnTreatmentSmoothingLaplacian(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

    VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	MeshAlgoSmoothingLaplacian algo;
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		Mesh_half_edge *pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
		algo.Apply(pMeshHE);

		// Copy smoothed vertices back
		for (unsigned int i = 0; i < pMesh->m_nVertices; i++)
		{
			pMesh->m_pVertices[3 * i]     = pMeshHE->m_pMesh->m_pVertices[3 * i];
			pMesh->m_pVertices[3 * i + 1] = pMeshHE->m_pMesh->m_pVertices[3 * i + 1];
			pMesh->m_pVertices[3 * i + 2] = pMeshHE->m_pMesh->m_pVertices[3 * i + 2];
		}
		pMesh->ComputeNormals();
		pMesh->IncrementRevision();
	}

	pGLCanvas->Refresh();
	*m_pWndLogging << _T("Smoothing Laplacian applied\n");
}

//
// Replace pMesh's vertices and faces with the subdivided result stored in pNewMesh.
// Used by the three subdivision handlers below : after the algorithm runs on the
// half-edge mesh, we must copy back BOTH vertices and faces (subdivision changes
// counts and topology) and recompute normals so that the renderer picks up the
// new geometry on the next IncrementRevision().
//
static void ReplaceMeshGeometry (Mesh *pMesh, Mesh *pNewMesh)
{
	// Build flat triangle index list from the new mesh's Face** array.
	std::vector<unsigned int> faces (3 * pNewMesh->m_nFaces);
	for (unsigned int f = 0; f < pNewMesh->m_nFaces; ++f)
	{
		faces[3*f+0] = (unsigned int)pNewMesh->m_pFaces[f]->GetVertex(0);
		faces[3*f+1] = (unsigned int)pNewMesh->m_pFaces[f]->GetVertex(1);
		faces[3*f+2] = (unsigned int)pNewMesh->m_pFaces[f]->GetVertex(2);
	}

	// Delete the existing per-Face objects on the destination ; SetFaces frees
	// the outer pointer array but leaks the inner Face* without this cleanup.
	if (pMesh->m_pFaces)
	{
		for (unsigned int i = 0; i < pMesh->m_nFaces; ++i)
			delete pMesh->m_pFaces[i];
	}

	pMesh->SetVertices (pNewMesh->m_nVertices, pNewMesh->m_pVertices.data());
	pMesh->SetFaces (pNewMesh->m_nFaces, 3, faces.data());
	pMesh->ComputeNormals ();

	// Carry per-vertex appearance attributes when the new mesh provides them
	// vertex-parallel (e.g. attribute-preserving decimation). Colours/UVs are
	// otherwise lost; normals are recomputed above.
	if (pNewMesh->m_pVertexColors.size() == 3u * (size_t)pNewMesh->m_nVertices)
		pMesh->m_pVertexColors = pNewMesh->m_pVertexColors;
	if (pNewMesh->m_pTextureCoordinates.size() == 2u * (size_t)pNewMesh->m_nVertices)
	{
		pMesh->m_pTextureCoordinates = pNewMesh->m_pTextureCoordinates;
		pMesh->m_nTextureCoordinates = pNewMesh->m_nTextureCoordinates;
	}

	pMesh->IncrementRevision ();
}

void MyFrame::OnTreatmentSubdivisionLoop(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

	VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	MeshAlgoSubdivisionLoop algo;
	algo.SetUseWarrenMask (true);
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		Mesh_half_edge *pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
		algo.Apply(pMeshHE);
		ReplaceMeshGeometry (pMesh, pMeshHE->m_pMesh);
	}

	pGLCanvas->Refresh();
	*m_pWndLogging << _T("Subdivision Loop (Warren) applied\n");
}

void MyFrame::OnTreatmentSubdivisionKarbacher(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

	VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	MeshAlgoSubdivisionKarbacher algo;
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		// Karbacher reads vertex normals — make sure they are up-to-date and
		// invalidate the cached half-edge so it picks up the fresh normals.
		pMesh->ComputeNormals ();
		pMesh->IncrementRevision ();

		Mesh_half_edge *pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
		algo.Apply(pMeshHE);
		ReplaceMeshGeometry (pMesh, pMeshHE->m_pMesh);
	}

	pGLCanvas->Refresh();
	*m_pWndLogging << _T("Subdivision Karbacher applied\n");
}

void MyFrame::OnTreatmentSubdivisionSqrt3(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas *pGLCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());
	if (!pGLCanvas)
		return;

	VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	MeshAlgoSubdivisionSqrt3 algo;
	algo.SetSmoothOriginal (true);
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		Mesh_half_edge *pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
		algo.Apply(pMeshHE);
		ReplaceMeshGeometry (pMesh, pMeshHE->m_pMesh);
	}

	pGLCanvas->Refresh();
	*m_pWndLogging << _T("Subdivision Sqrt(3) applied\n");
}

// Apply the smoothing method picked in the Treatments > Smoothing tab. Reuses
// the per-method handlers (which read the active canvas and refresh).
void MyFrame::OnTreatmentApplySmoothing(wxCommandEvent& WXUNUSED(event))
{
	wxCommandEvent dummy;
	const int sel = m_pSmoothingMethodCombo ? m_pSmoothingMethodCombo->GetSelection() : 0;
	if (sel == 1) OnTreatmentSmoothingLaplacian(dummy);
	else          OnTreatmentSmoothingTaubin(dummy);
}

// Apply the subdivision method picked in the Treatments > Subdivision tab.
void MyFrame::OnTreatmentApplySubdivision(wxCommandEvent& WXUNUSED(event))
{
	wxCommandEvent dummy;
	const int sel = m_pSubdivisionMethodCombo ? m_pSubdivisionMethodCombo->GetSelection() : 0;
	if (sel == 2)      OnTreatmentSubdivisionSqrt3(dummy);
	else if (sel == 1) OnTreatmentSubdivisionKarbacher(dummy);
	else               OnTreatmentSubdivisionLoop(dummy);
}

// Recompute vertex normals on the active model with the method picked in the
// Treatments > Normals tab, then refresh. Angle-weighted estimators (Thurmer/
// Max) avoid the shading discontinuities of the equal-weight average.
void MyFrame::OnTreatmentApplyNormals(wxCommandEvent& WXUNUSED(event))
{
	MyGLCanvas* pGLCanvas = GetActiveCanvas();
	if (!pGLCanvas)
	{
		*m_pWndLogging << _T("Normals: no active model\n");
		return;
	}
	VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	const int sel = m_pNormalsMethodCombo ? m_pNormalsMethodCombo->GetSelection() : 1;
	Normals::MethodId method = Normals::THURMER;
	switch (sel)
	{
		case 0: method = Normals::GOURAUD;  break;
		case 1: method = Normals::THURMER;  break;
		case 2: method = Normals::MAX;      break;
	}

	Normals algo;
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		if (sel == 0)
		{
			// Equal-weight average: robust on any face topology, no half-edge.
			pMesh->ComputeNormals();
		}
		else
		{
			Mesh_half_edge* pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
			if (!pMeshHE || !pMeshHE->m_pMesh)
				continue;
			algo.EvalOnVertices(pMeshHE, method);
			// Same vertex order/count as pMesh; copy the unit normals back.
			if (pMeshHE->m_pMesh->m_pVertexNormals.size() == pMesh->m_pVertexNormals.size())
				pMesh->m_pVertexNormals = pMeshHE->m_pMesh->m_pVertexNormals;
		}
		pMesh->IncrementRevision();
	}

	pGLCanvas->Refresh();
	*m_pWndLogging << wxString::Format(_T("Normals recomputed (method %d)\n"), sel);
}

// Open (and raise) the Decimation parameters panel.
void MyFrame::OnTreatmentDecimation(wxCommandEvent& WXUNUSED(event))
{
	wxAuiPaneInfo& pane = m_mgr.GetPane(wxT("Decimation"));
	if (!pane.IsOk())
		return;
	pane.Show(true);
	m_mgr.Update();
}

// Run QEM decimation on the active canvas's meshes with the panel parameters.
void MyFrame::ApplyDecimation(void)
{
	if (!m_pDecimationPanel)
		return;

	MyGLCanvas* pGLCanvas = GetActiveCanvas();
	if (!pGLCanvas)
	{
		*m_pWndLogging << _T("Decimation: no active model\n");
		return;
	}

	VMeshes* pVMeshes = pGLCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	// QEM edge-collapse decimation assumes a MANIFOLD mesh. On a non-manifold
	// mesh (typical of welded STLs with overlapping/internal parts) the
	// half-edge edge-pairing is ill-defined and collapses corrupt the
	// connectivity (holes, flipped triangles). Warn before producing garbage.
	size_t nonManifold = 0;
	for (auto& pMesh : pVMeshes->GetMeshes())
		nonManifold += MeshDataManager::GetInstance().GetTopologicIssues(pMesh).nonManifoldEdges.size();
	if (nonManifold > 0)
	{
		wxString msg = wxString::Format(
			_("This model has %zu non-manifold edges.\n\n"
			  "QEM edge-collapse decimation assumes a manifold mesh; on a "
			  "non-manifold one it will likely corrupt the result (holes, "
			  "flipped triangles). Repair/remesh the model to a watertight "
			  "manifold first.\n\nDecimate anyway?"), nonManifold);
		if (wxMessageBox(msg, _("Non-manifold mesh"), wxYES_NO | wxICON_WARNING, this) != wxYES)
		{
			*m_pWndLogging << wxString::Format(_T("Decimation cancelled: mesh has %zu non-manifold edges\n"), nonManifold);
			return;
		}
	}

	const float ratio = m_pDecimationPanel->GetTargetRatio();
	Mesh_half_edge::SimplifyOptions opt;
	opt.preserve_features   = m_pDecimationPanel->GetPreserveFeatures();
	opt.feature_angle_deg   = m_pDecimationPanel->GetFeatureAngleDeg();
	opt.preserve_attributes = m_pDecimationPanel->GetPreserveAttributes();
	opt.attribute_metric    = m_pDecimationPanel->GetAttributeMetric();
	opt.attribute_weight    = m_pDecimationPanel->GetAttributeWeight();
	opt.preserve_uv         = m_pDecimationPanel->GetPreserveUV();
	opt.max_error           = m_pDecimationPanel->GetMaxError();
	opt.exact_error         = m_pDecimationPanel->GetExactError();

	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		const unsigned int facesBefore = pMesh->GetNFaces();

		Mesh_half_edge* pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);
		if (!pMeshHE || !pMeshHE->m_pMesh)
			continue;

		// The half-edge copy drops colours/UV; sync the rendered mesh's
		// vertex-parallel attributes so decimation can preserve them. (Same
		// vertex order/count as pMesh at this point.)
		Mesh* src = pMeshHE->m_pMesh;
		if (pMesh->m_pVertexColors.size() == 3u * (size_t)src->m_nVertices)
			src->m_pVertexColors = pMesh->m_pVertexColors;
		if (pMesh->m_pTextureCoordinates.size() == 2u * (size_t)src->m_nVertices)
		{
			src->m_pTextureCoordinates = pMesh->m_pTextureCoordinates;
			src->m_nTextureCoordinates = pMesh->m_nTextureCoordinates;
		}

		pMeshHE->simplify(ratio, opt);
		ReplaceMeshGeometry(pMesh, pMeshHE->m_pMesh);

		*m_pWndLogging << wxString::Format(_T("Decimation: %u -> %u faces\n"),
		                                   facesBefore, pMesh->GetNFaces());
	}

	pGLCanvas->UpdateTopologicIssues();
	pGLCanvas->Refresh();
	UpdatePropertiesGrid();
}

//
// Adaptive docking & curvature visualization
//

//
// Show only the side panes relevant to the active tab. Currently this drives
// the "Parameters" pane, shown iff the active canvas drives a parameterized
// object. The "Curvature" pane is a regular tool window toggled from
// Options > Windows, not a contextual one.
//
void MyFrame::UpdateContextualPanes()
{
	MyGLCanvas* pCanvas = nullptr;
	if (m_pCtrl && m_pCtrl->GetSelection() != wxNOT_FOUND)
		pCanvas = (MyGLCanvas*)m_pCtrl->GetPage(m_pCtrl->GetSelection());

	const bool showParams = pCanvas && m_paramByCanvas.count(pCanvas) > 0;

	wxAuiPaneInfo& paramPane = m_mgr.GetPane(wxT("Parameters"));
	if (paramPane.IsOk() && paramPane.IsShown() != showParams)
	{
		paramPane.Show(showParams);
		m_mgr.Update();
	}
}

// Colour each mesh from its stored curvature tensors for `type`. The VBO only
// re-uploads its colour buffer when the mesh revision changes, so bump it;
// the geometry is untouched, so re-stamp the tensors as still valid.
static void colorizeCurvatureMeshes(VMeshes* pVMeshes, CurvatureType type)
{
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		pMesh->InitVertexColorsFromCurvatures(type);
		pMesh->IncrementRevision();
		pMesh->MarkTensorsComputed();
	}
}

// Put back the vertex colours captured before the curvature map was applied
// (empty vector => the mesh had none, so it renders with its material again).
static void restoreCurvatureColors(VMeshes* pVMeshes, const std::vector<std::vector<float>>& saved)
{
	std::vector<Mesh*>& meshes = pVMeshes->GetMeshes();
	for (size_t i = 0; i < meshes.size(); i++)
	{
		meshes[i]->m_pVertexColors = (i < saved.size()) ? saved[i] : std::vector<float>();
		meshes[i]->IncrementRevision();
		meshes[i]->MarkTensorsComputed();
	}
}

//
// (Re)compute the curvature tensor field on the active canvas with `method`,
// store the per-canvas selection, colour the mesh for `type` and reveal the
// Curvature pane. The tensors are computed on a half-edge working copy then
// adopted onto the rendered mesh, so a later type change only re-derives the
// colour (RecolorCurvature) without recomputing the field.
//
void MyFrame::ApplyCurvature(MyGLCanvas* pCanvas, TensorMethodId method, CurvatureType type)
{
	if (!pCanvas)
		return;

	VMeshes* pVMeshes = pCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	CurvatureState& state = m_curvatureByCanvas[pCanvas];

	// Snapshot the meshes' colours once, before the curvature map overwrites
	// them, so "Apply visualization" off can restore the original look.
	if (!state.savedValid)
	{
		state.savedColors.clear();
		for (auto& pMesh : pVMeshes->GetMeshes())
			state.savedColors.push_back(pMesh->m_pVertexColors);
		state.savedValid = true;
	}

	// (Re)compute the tensor field and adopt it onto the rendered meshes.
	for (auto& pMesh : pVMeshes->GetMeshes())
	{
		Mesh_half_edge* pMeshHE = MeshDataManager::GetInstance().GetHalfEdge(pMesh);

		MeshAlgoTensorEvaluator algo;
		algo.Init(pMeshHE);
		algo.Evaluate(method);

		// Bring the freshly computed tensors back onto the rendered mesh so
		// the curvature colour can be re-derived per type without recompute.
		pMesh->AdoptTensorsFrom(*pMeshHE->m_pMesh);
	}

	state.method  = method;
	state.type    = type;
	state.enabled = true;

	if (m_pCurvaturePanel)
	{
		m_pCurvaturePanel->SetSelection(method, type);
		m_pCurvaturePanel->SetEnabled(true);
	}

	colorizeCurvatureMeshes(pVMeshes, type);
	pCanvas->Refresh();

	*m_pWndLogging << _T("Curvatures applied\n");
}

//
// Re-derive the per-vertex curvature colour for a different curvature type
// from the tensors already stored on the mesh (no tensor recompute). The type
// is remembered even while the visualization is off, so it applies on re-enable.
//
void MyFrame::RecolorCurvature(MyGLCanvas* pCanvas, CurvatureType type)
{
	if (!pCanvas)
		return;

	auto it = m_curvatureByCanvas.find(pCanvas);
	if (it == m_curvatureByCanvas.end())
		return;  // curvature not active for this tab

	it->second.type = type;

	VMeshes* pVMeshes = pCanvas->GetVMeshes();
	if (!pVMeshes || !it->second.enabled)
		return;  // off: keep the new type but leave the mesh as-is

	colorizeCurvatureMeshes(pVMeshes, type);
	pCanvas->Refresh();
}

//
// "Apply visualization" toggle: re-colour from the stored tensors when turned
// on, restore the captured original colours when turned off. The Curvature
// pane stays visible either way so the user can flip it back.
//
void MyFrame::SetCurvatureEnabled(MyGLCanvas* pCanvas, bool enabled)
{
	if (!pCanvas)
		return;

	VMeshes* pVMeshes = pCanvas->GetVMeshes();
	if (!pVMeshes)
		return;

	CurvatureState& state = m_curvatureByCanvas[pCanvas];

	if (enabled)
	{
		// Compute the tensor field (if not already) and colour the mesh for
		// the stored method/type. ApplyCurvature flips state.enabled to true.
		ApplyCurvature(pCanvas, state.method, state.type);
		return;
	}

	state.enabled = false;
	if (state.savedValid)
		restoreCurvatureColors(pVMeshes, state.savedColors);
	pCanvas->Refresh();
}
