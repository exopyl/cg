#pragma once

#include "../src/cgre/gl_wrapper.h"
#include "wx/glcanvas.h"
#include <wx/dataobj.h>
#include <chrono>
#include <list>
#include "../src/cgre/cgre.h"
#include "../src/cgmesh/vmodels.h"
#include "ImportSettings.h"


class Mesh;
class BoundingBox;

// Format de glisser-déposer INTERNE : un chemin de modèle issu du panneau des
// fichiers de sinaia. Le canvas n'accepte QUE ce format — les dépôts de fichiers
// venus de l'OS (explorateur) sont donc ignorés, si bien que l'ajout d'un modèle
// à la vue courante ne se produit QUE via un glisser depuis ce panneau.
wxDataFormat SinaiaModelPathFormat();

//
// ref : http://wiki.wxwidgets.org/WxGLCanvas
//
class MyGLCanvas: public wxGLCanvas
{
public:
	static const int* GetDefaultAttributes();

	wxGLContext*	m_context = nullptr;
	wxTextCtrl*		m_CtrlLog = nullptr; // TODO : replace with a lambda
	
	MyGLCanvas(wxWindow *parent, wxTextCtrl* pCtrlLog, int *args = 0);
	virtual ~MyGLCanvas();
	
	void LoadModel(const wxString& filename, const ImportSettings& settings = ImportSettings());
	// Ajoute un fichier à la scène COURANTE (nouveau Model) sans remplacer la vue ni
	// renormaliser (repère monde conservé) ; recadre sur la scène. nullptr si échec.
	Model* AppendModel(const wxString& filename);
	void SaveModel(const wxString& filename);

	Mesh* GetMesh(void);
	void  SetMesh(Mesh *pMesh);

	// Scène multi-fichiers (nouveau modèle). Le canvas en est propriétaire.
	VModels* GetVModels(void) { return m_pVModels; }

	// Survol : Model actuellement sous le curseur (nullptr si aucun). Pointeur stable
	// (VModels stocke des unique_ptr). Réinitialisé quand la scène change.
	Model* GetHoveredModel(void) const { return m_hoveredModel; }
	void   ClearHoveredModel(void) { m_hoveredModel = nullptr; }

	// Sélection : Model dont les propriétés sont affichées dans "Model information"
	// (piloté par un clic dans le panneau "Models"). Pointeur stable ; réinitialisé
	// quand la scène change.
	Model* GetSelectedModel(void) const { return m_selectedModel; }
	void   SetSelectedModel(Model* m) { m_selectedModel = m; }
	// Compat : renvoie le VMeshes du fichier ACTIF (le premier Model) — la plupart
	// des traitements historiques opèrent encore sur « le » VMeshes de la vue.
	VMeshes* GetVMeshes(void);
	// normalize: when true (default), the meshes are centered and scaled to a
	// unit bounding box. File import passes the user's "Normalisation" import
	// option here; geometry-creation callers keep the default. La scène devient
	// un unique Model qui adopte les maillages de pVMeshes (qui est ensuite détruit).
	void SetVMeshes(VMeshes* pVMeshes, bool normalize = true);

	void SetBackgroundColor (unsigned char r, unsigned char g, unsigned char b);
	void GetBackgroundColor (unsigned char *r, unsigned char *g, unsigned char *b);

	void SetLighting (bool bLighting) { prop.light = bLighting; Refresh(false); };
	bool GetLighting (void) { return prop.light; };

	void SetSmooth (bool bSmooth) { m_bSmooth = bSmooth; Refresh(false); };
	bool GetSmooth (void) { return m_bSmooth; };

	// Width (in pixels) of the wireframe / edge lines drawn in the 3D view.
	void SetLineWidth (float w) { prop.linesize = w; Refresh(false); };
	float GetLineWidth (void) { return prop.linesize; };

	// Size (in pixels) of the points drawn in the 3D view (point cloud mode).
	void SetPointSize (float s) { prop.pointsize = s; Refresh(false); };
	float GetPointSize (void) { return prop.pointsize; };

	void ChangeFill (void);
	bool GetFill (void);

	void ChangeWireframe (void);
	bool GetWireframe (void);

	void ChangePoint(void);
	bool GetPoint(void);

	void ChangeRepere(void);
	bool GetRepere(void);

	void ChangeGrid(void);
	bool GetGrid(void);

	void SetClippingPlane(bool bActive) { prop.clipping_plane_active = bActive; Refresh(false); };
	bool GetClippingPlane (void) { return prop.clipping_plane_active; };
	void SetClippingPlaneZ(float z) { prop.clipping_plane_z = z; Refresh(false); };

	void ApplyNormalization(bool normalize);

	void ChangeWarning(void);	bool GetWarning(void);

	void ChangeBoundingBox (void);
	bool GetBoundingBox (void);

	unsigned int GetNNonManifoldEdges() const;
	unsigned int GetNBorders() const;

	void UpdateTopologicIssues();

	void ResetProjectionMode();
	void DrawGL();

	// Save current GL framebuffer (front buffer) to a PNG. Must be called
	// on the wx main thread (uses this canvas' GL context).
	bool SaveScreenshot(const wxString& path);

/*
	Mesh_half_edge* GetMesh (void) { return m_pMesh; };

	void NPRCompute (void);
	void SetSegments (ListNPRSegments &listSegments) { m_listSegments = listSegments; };
	void SetNPRAngleThreshold (float fValue) { m_fNPRAngleThreshold = fValue; };

	void ExportNPR (void);
*/
protected:
	void OnPaint(wxPaintEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnEraseBackground(wxEraseEvent& event);
	void OnMouse(wxMouseEvent& event);
	void OnKeyDown(wxKeyEvent& event);

private:
	void InitGL();

	// Position the camera (zoom, rotation pivot and clip planes) so the given
	// model bounding box fits in view, whatever its native scale/position.
	void FrameCamera(const BoundingBox& bbox);

	// Construit le rayon monde (origine + direction) passant par le pixel (x,y) à
	// partir des matrices GL courantes. false si non inversibles.
	bool ScreenToRay(int x, int y, float orig[3], float dir[3]);
	// Model dont l'AABB est traversée en PREMIER par le rayon souris (picking au
	// niveau FICHIER), ou nullptr. N'inspecte que les Model visibles.
	Model* PickModel(int x, int y);

	bool m_bInitialized;

	// rendering modes
	rendering_properties_s prop;
	float m_fBackgroundColor[3];
	bool m_bSmooth;
	bool m_bBoundingBox;

	Ctrackball *m_pTrackball;

	//CRenderingEngine *m_pRenderingEngine;
	int m_nId;
	std::list<int> m_listId;

	Mesh *m_pMesh = nullptr;
	//Mesh_half_edge *m_pMesh = nullptr;
	VModels *m_pVModels = nullptr;   // scène = liste de Model (un par fichier)
	Model   *m_hoveredModel = nullptr;    // Model survolé (surbrillance bbox)
	Model   *m_selectedModel = nullptr;   // Model sélectionné (-> Model information)
	int      m_pressX = 0, m_pressY = 0;  // position du clic gauche (clic vs glisser)


	// viewport
	float m_fFovy;
	float m_fWindowWidth, m_fWindowHeight;
	float m_fNear, m_fFar;

	// NPR
	//NPRManager *m_pNPRManager;

	//ListNPRSegments m_listSegments;
	float m_fNPRAngleThreshold;

	// FPS overlay
	unsigned int m_fpsFrames = 0;
	std::chrono::steady_clock::time_point m_fpsLastUpdate = std::chrono::steady_clock::now();

    DECLARE_EVENT_TABLE()
};
