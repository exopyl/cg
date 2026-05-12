#pragma once

#include "wxSizeReportCtrl.h"
#include "wx/grid.h"
#include "wx/wxhtml.h"
#include "wxOpenGLCanvas.h"
#include "wx/radiobut.h"
#include "wx/spinctrl.h"
#include "wx/listbase.h"

#include <memory>
#include <unordered_map>

class wxPropertyGrid;
class wxGenericDirCtrl;
class wxListCtrl;
class PropertyPanel;
class IParameterized;

//
//
//
class MyFrame : public wxFrame
{
    enum
    {
        ID_CreateTree = wxID_HIGHEST+1,
        ID_CreateGrid,
        ID_CreateText,
        ID_CreateHTML,
        ID_CreateNotebook,
        ID_CreateSizeReport,
        ID_GridContent,
        ID_TextContent,
        ID_TreeContent,
        ID_HTMLContent,
        ID_NotebookContent,
        ID_SizeReportContent,
        ID_CreatePerspective,
        ID_CopyPerspectiveCode,
        ID_AllowFloating,
        ID_AllowActivePane,
        ID_TransparentHint,
        ID_VenetianBlindsHint,
        ID_RectangleHint,
        ID_NoHint,
        ID_HintFade,
        ID_NoVenetianFade,
        ID_TransparentDrag,
        ID_NoGradient,
        ID_VerticalGradient,
        ID_HorizontalGradient,
        ID_Settings,
        ID_CustomizeToolbar,
        ID_DropDownToolbarItem,
        ID_NotebookNoCloseButton,
        ID_NotebookCloseButton,
        ID_NotebookCloseButtonAll,
        ID_NotebookCloseButtonActive,
        ID_NotebookAllowTabMove,
        ID_NotebookAllowTabExternalMove,
        ID_NotebookAllowTabSplit,
        ID_NotebookWindowList,
        ID_NotebookScrollButtons,
        ID_NotebookTabFixedWidth,
        ID_NotebookArtGloss,
        ID_NotebookArtSimple,
        ID_NotebookAlignTop,
        ID_NotebookAlignBottom,
        
        ID_SampleItem,

        ID_3D_FRAME,
        ID_3D_FILL,
        ID_3D_WIREFRAME,		ID_3D_POINT,
		ID_3D_SMOOTH,
		ID_3D_FLAT,
		ID_3D_LIGHTING,
        ID_3D_CLIPPING,
        ID_3D_WARNING,
        ID_RENDER_SHOW_FPS,

	ID_BUTTON_RENDERING_BGCOLOR,

    ID_NOTEBOOK_MAIN,
    ID_PROPERTIESCTRL,
    ID_DIRCTRL,
	ID_FILESCTRL,

    ID_FILE_EXPORT_IMAGE,

	ID_GEOMETRY_NEW_CUBE,
	ID_GEOMETRY_NEW_SPHERE,
	ID_GEOMETRY_NEW_CYLINDER,
	ID_GEOMETRY_NEW_TEAPOT,
	ID_GEOMETRY_NEW_KLEIN_BOTTLE,
	ID_GEOMETRY_NEW_PARAM_CUBE,
	ID_GEOMETRY_NEW_PARAM_SPHERE,
	ID_GEOMETRY_NEW_PARAM_CYLINDER,
	ID_GEOMETRY_NEW_PARAM_CONE,
	ID_GEOMETRY_NEW_PARAM_CAPSULE,
	ID_GEOMETRY_NEW_PARAM_TORUS,
	ID_GEOMETRY_NEW_PARAM_KLEIN_BOTTLE,
	ID_GEOMETRY_NEW_PARAM_HELICOID,
	ID_GEOMETRY_NEW_PARAM_SEASHELL,
	ID_GEOMETRY_NEW_PARAM_SEASHELL_VON_SEGGERN,
	ID_GEOMETRY_NEW_PARAM_CORKSCREW,
	ID_GEOMETRY_NEW_PARAM_MOBIUS_STRIP,
	ID_GEOMETRY_NEW_PARAM_RADIAL_WAVE,
	ID_GEOMETRY_NEW_PARAM_BREATHER,
	ID_GEOMETRY_NEW_PARAM_HYPERBOLIC_PARABOLOID,
	ID_GEOMETRY_NEW_PARAM_MONKEY_SADDLE,
	ID_GEOMETRY_NEW_PARAM_BLOBS,
	ID_GEOMETRY_NEW_PARAM_DROP,
	ID_GEOMETRY_NEW_PARAM_GUIMARD,
	ID_GEOMETRY_NEW_PARAM_TORUS_KNOT,
	ID_GEOMETRY_NEW_PARAM_CINQUEFOIL_KNOT,
	ID_GEOMETRY_NEW_PARAM_TREFOIL_KNOT,
	ID_GEOMETRY_NEW_PARAM_BORROMEAN_RINGS,
	ID_GEOMETRY_NEW_PARAM_MENGER_SPONGE,
	ID_GEOMETRY_CUBE,
	ID_GEOMETRY_SPHERE,
	ID_GEOMETRY_SPHERE_LAT,
	ID_GEOMETRY_SPHERE_LNG,
	ID_GEOMETRY_CYLINDER,
	ID_GEOMETRY_KLEIN_BOTTLE,
	ID_GEOMETRY_KLEIN_BOTTLE_THETA,
	ID_GEOMETRY_KLEIN_BOTTLE_PHI,

		ID_BUTTON_SMOOTHING_TAUBIN,
		ID_BUTTON_SMOOTHING_LAPLACIAN,

		ID_BUTTON_CURVATURES_TAUBIN,
		ID_BUTTON_CURVATURES_DESBRUN,

		ID_BUTTON_NPR_COMPUTE,
		ID_BUTTON_NPR_EXPORT,

	ID_TREATMENT_MERGE_VERTICES,
	ID_TREATMENT_SMOOTHING_TAUBIN,
	ID_TREATMENT_SMOOTHING_LAPLACIAN,
	ID_TREATMENT_SUBDIVISION_LOOP,
	ID_TREATMENT_SUBDIVISION_KARBACHER,
	ID_TREATMENT_SUBDIVISION_SQRT3,
	ID_TREATMENT_CURVATURES_TAUBIN,
	ID_TREATMENT_CURVATURES_DESBRUN,
	ID_TREATMENT_CURVATURES_HAMANN,

	ID_ShowProperties,
	ID_ShowMeshes,
	ID_ShowMaterials,
	ID_ShowLogging,
	ID_ShowExplorer,

        ID_FirstPerspective = ID_CreatePerspective+1000
    };

public:
    MyFrame(wxWindow* parent,
            wxWindowID id,
            const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE | wxSUNKEN_BORDER);

    ~MyFrame();

    wxAuiDockArt* GetDockArt();
    void DoUpdate();

    bool IsShowFps() const { return m_bShowFps; }

    // Active 3D canvas of the foreground notebook page, or nullptr if no
    // model is open. Used by the remote console.
    MyGLCanvas* GetActiveCanvas();

private:
    wxTextCtrl* CreateTextCtrl(const wxString& text = wxEmptyString);
    wxGrid* CreateGrid();
    wxSizeReportCtrl* CreateSizeReportCtrl(int width = 80, int height = 80);
    wxPoint GetStartPosition();
    wxHtmlWindow* CreateHTMLCtrl(wxWindow* parent = nullptr);

	wxAuiNotebook* CreateNotebook(void);

    wxString GetIntroText();

    void OnDropFiles(wxDropFilesEvent& event);

private:

    void OnEraseBackground(wxEraseEvent& evt);
    void OnSize(wxSizeEvent& evt);

    void OnChangeContentPane(wxCommandEvent& evt);
    void OnDropDownToolbarItem(wxAuiToolBarEvent& evt);
    void OnCreatePerspective(wxCommandEvent& evt);
    void OnCopyPerspectiveCode(wxCommandEvent& evt);
    void OnRestorePerspective(wxCommandEvent& evt);
    void OnCustomizeToolbar(wxCommandEvent& evt);
    void OnAllowNotebookDnD(wxAuiNotebookEvent& evt);
    void OnNotebookPageClose(wxAuiNotebookEvent& evt);
    void OnOpen(wxCommandEvent& evt);
    void OnSave(wxCommandEvent& evt);
    void OnSaveAs(wxCommandEvent& evt);
    void OnExportImage(wxCommandEvent& evt);
    void OnNewGeometry(wxCommandEvent& evt);
    void OnNewParameterizedGeometry(wxCommandEvent& evt);
    void OnParameterChanged();
    void OnExit(wxCommandEvent& evt);
    void OnAbout(wxCommandEvent& evt);
    void OnTabAlignment(wxCommandEvent &evt);

    void OnDirCtrlSelectionChanged(wxTreeEvent& evt);
    void OnFilesCtrlListItemActivated(wxListEvent& evt);

    void OnNotebookPageChanged(wxAuiNotebookEvent& event);

    void On3DFrame(wxCommandEvent& evt);
    void On3DFill(wxCommandEvent& evt);	void On3DWireframe(wxCommandEvent& evt);
	void On3DPoint(wxCommandEvent& evt);
	void On3DSmooth(wxCommandEvent& evt);
	void On3DFlat(wxCommandEvent& evt);
	void On3DLighting(wxCommandEvent& evt);

	void On3DWarning(wxCommandEvent& evt);
	void On3DClippingPlane(wxCommandEvent& evt);

	void OnToggleShowFps(wxCommandEvent& evt);

	void OnBgColor(wxCommandEvent& evt);
	void UpdateGeometry (void);
	void OnSelectGeometrySpinEvent (wxSpinEvent & ev);
	void OnSelectGeometry(wxCommandEvent& evt);

	void OnButtonSmoothingTaubin(wxCommandEvent& evt);
	void OnButtonSmoothingLaplacian(wxCommandEvent& evt);

	void OnButtonCurvaturesTaubin(wxCommandEvent& evt);
	void OnButtonCurvaturesDesbrun(wxCommandEvent& evt);

	void OnButtonNPRCompute(wxCommandEvent& evt);
	void OnButtonNPRExport(wxCommandEvent& evt);
	void OnSlider(wxScrollEvent& event);

	void OnTreatmentMergeVertices(wxCommandEvent& evt);
	void OnTreatmentSmoothingTaubin(wxCommandEvent& evt);
	void OnTreatmentSmoothingLaplacian(wxCommandEvent& evt);
	void OnTreatmentSubdivisionLoop(wxCommandEvent& evt);
	void OnTreatmentSubdivisionKarbacher(wxCommandEvent& evt);
	void OnTreatmentSubdivisionSqrt3(wxCommandEvent& evt);
	void OnTreatmentCurvaturesTaubin(wxCommandEvent& evt);
	void OnTreatmentCurvaturesDesbrun(wxCommandEvent& evt);
	void OnTreatmentCurvaturesHamann(wxCommandEvent& evt);

    void OnGradient(wxCommandEvent& evt);
    void OnManagerFlag(wxCommandEvent& evt);
    void OnNotebookFlag(wxCommandEvent& evt);
    void OnUpdateUI(wxUpdateUIEvent& evt);

    void OnPaneClose(wxAuiManagerEvent& evt);
    void OnShowWindow(wxCommandEvent& evt);

private:
    void OpenDocument(const wxString& filename);
    void UpdatePropertiesGrid();

    void Log(const wxString& text) const;

	int m_nRadioGeometries;
	wxRadioButton **m_pRadioGeometries;
	wxSpinCtrl *m_pGeometrySphereLat, *m_pGeometrySphereLng;
	wxSpinCtrl *m_pGeometryKleinBottleTheta, *m_pGeometryKleinBottlePhi;
    wxCheckBox* m_pClippingPlane;

	wxAuiToolBar* m_pToolBar2;

    wxAuiManager m_mgr;
    wxArrayString m_perspectives;
    wxMenu* m_perspectives_menu;

	wxTextCtrl* m_pWndLogging;

	MyGLCanvas* m_pGLCanvas;

	wxAuiNotebook* m_pCtrl;
	// NPR
	//wxSlider *m_pNPRSliderMinimalAngle;

    long m_notebook_style;
    long m_notebook_theme;

    bool m_bShowFps = false;

    // Parameterized geometry: the panel shows the params of the object
    // associated with the currently active tab. One entry per tab.
    PropertyPanel* m_pParamPanel = nullptr;
    std::unordered_map<MyGLCanvas*, std::unique_ptr<IParameterized>> m_paramByCanvas;

    //
    wxPropertyGrid* m_propertiesGrid = nullptr;
    wxGenericDirCtrl* m_dcDirectory = nullptr;
    wxListCtrl* m_filesCtrl = nullptr;

    wxTreeCtrl* CreateHierarchyMeshesTreeCtrl();
    wxTreeCtrl* m_hierarchyMeshes = nullptr;

    wxTreeCtrl* CreateHierarchyMaterialsTreeCtrl();
    wxTreeCtrl* m_hierarchyMaterials = nullptr;

    DECLARE_EVENT_TABLE()
};
