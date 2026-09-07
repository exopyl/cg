#pragma once

#include "../src/cgre/gl_wrapper.h"
#include "wx/glcanvas.h"
#include <wx/dataobj.h>
#include <chrono>
#include <list>
#include "../src/cgre/cgre.h"
#include "../src/cgmesh/vmodels.h"
#include "CuttingMat.h"
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
	~MyGLCanvas() override;
	
	void LoadModel(const wxString& filename, const ImportSettings& settings = ImportSettings());
	// Ajoute un fichier à la scène COURANTE (nouveau Model) sans remplacer la vue ni
	// renormaliser (repère monde conservé) ; recadre sur la scène. nullptr si échec.
	Model* AppendModel(const wxString& filename);

	// Recharge un Model depuis son fichier d'origine (Model::m_path), en place.
	// Détache les anciens maillages du renderer, relit le fichier, recalcule
	// normales / BVH / bbox. Recadre la caméra UNIQUEMENT si ce Model est le seul
	// de la scène ; s'il coexiste avec d'autres, la vue courante est préservée.
	// false si m_path est vide, le fichier est introuvable, ou le rechargement échoue.
	bool ReloadModel(Model* mdl);
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
	// normalize: when true (default), the meshes are centered and scaled so their
	// largest bbox dimension becomes VMeshes::kNormalizedSize (100 mm = 10 cm, ten
	// squares of the cutting mat). File import passes the user's "Normalisation" import
	// option here; geometry-creation callers keep the default. La scène devient
	// un unique Model qui adopte les maillages de pVMeshes (qui est ensuite détruit).
	void SetVMeshes(VMeshes* pVMeshes, bool normalize = true);

	// Remplace la geometrie en laissant la camera ET les positions intactes : ni
	// normalisation, ni recadrage. Pour la regeneration pendant l edition d un
	// parametre, ou tout deplacement de la forme rendrait le reglage illisible.
	void UpdateGeometryKeepingView(VMeshes* pVMeshes);

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

	// Colours (0..1 RGB) of the mesh's line ('l') and point ('p') primitives.
	void SetLineColor (float r, float g, float b)
		{ prop.line_color[0] = r; prop.line_color[1] = g; prop.line_color[2] = b; Refresh(false); };
	void SetPointColor (float r, float g, float b)
		{ prop.point_color[0] = r; prop.point_color[1] = g; prop.point_color[2] = b; Refresh(false); };

	// MODE D'OMBRAGE : d'ou vient la couleur des faces. Un selecteur et non une
	// bascule -- voir CG_shading_mode (cgre/mesh_renderer.h).
	void SetShadingMode (CG_shading_mode mode) { prop.shading = mode; Refresh(false); };
	CG_shading_mode GetShadingMode (void) const { return prop.shading; };

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

	// BASE DE COUPE : tapis quadrille de reference, affiche sous le modele pour
	// lire ses dimensions a vue. Widget du canvas et NON Model de la scene (motifs
	// dans CuttingMat.h). Le tapis est charge paresseusement au premier affichage.
	void ChangeCuttingMat(void);
	bool GetCuttingMat(void);
	// Cote a laquelle la face utile du tapis est posee, recalculee au besoin.
	// Publique pour la console distante, ou elle sert d'oracle : elle doit valoir
	// le minimum Z de la scene visible.
	float GetCuttingMatLevel(void);

	// RAMENE un Model sur le plan Z = 0 : cuit une translation en Z dans ses
	// maillages, de sorte que le minimum Z de sa bbox devienne zero. Le modele est
	// DEPLACE pour de bon (le bouton « recharger » du panneau Models revient au
	// fichier).
	//
	// Cette action n'a rien a voir avec la base de coupe : celle-ci se pose d'elle-
	// meme sur le minimum Z du modele, qui repose donc dessus quoi qu'il arrive.
	// Elle sert a mettre un modele a la cote qu'attend une chaine de FABRICATION --
	// une piece imprimee part de Z = 0, plateau de la machine.
	//
	// Renvoie le deplacement appliqué (0 si le modele reposait deja sur zero, ou
	// s'il est vide).
	float MoveModelToZeroLevel(Model* mdl);

	void SetClippingPlane(bool bActive) { prop.clipping_plane_active = bActive; Refresh(false); };
	bool GetClippingPlane (void) { return prop.clipping_plane_active; };
	void SetClippingPlaneZ(float z) { prop.clipping_plane_z = z; Refresh(false); };

	void ApplyNormalization(bool normalize);
	void AdoptScene(VMeshes* pVMeshes);
	void RefreshGeometryState();

	// Transformation de normalisation FIGEE au chargement (centre puis echelle du
	// modele initial). UpdateGeometryKeepingView la reapplique telle quelle a chaque
	// regeneration, au lieu de renormaliser : la forme reste a sa place et a son
	// echelle de depart, donc un parametre qui la deplace ou l agrandit se VOIT.
	bool  m_hasNormalization = false;
	float m_normCenter[3] = { 0.f, 0.f, 0.f };
	float m_normScale = 1.f;

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

	// PLANS DE COUPE, decouples du CADRAGE. La distance de la camera se regle sur
	// le seul modele (c'est lui qu'on observe), mais les plans near/far doivent
	// couvrir tout ce qui est VISIBLE -- base de coupe comprise, sinon ses coins
	// passent derriere le plan far des qu'un petit modele resserre le cadrage.
	//
	// La base de coupe n'entre dans le calcul que si elle est AFFICHEE : son rayon
	// elargirait sinon la plage de profondeur de toutes les vues, au detriment de
	// la precision du tampon, pour un objet qu'on ne dessine pas.
	void UpdateSceneRadius();
	float m_sceneRadiusModel = 0.f;   // rayon du seul modele, pose par FrameCamera

	// NIVEAU DE LA BASE DE COUPE. Sa face utile se pose sur le minimum Z de la
	// scene VISIBLE : le modele repose ainsi toujours dessus sans etre deplace.
	//
	// Le recalcul est declenche par une SIGNATURE de scene -- nombre de Model,
	// drapeaux de visibilite, revisions de geometrie -- et non par des appels
	// d'invalidation semes aux endroits qui modifient la scene. Il y en a une
	// dizaine (chargement, ajout, retrait, rechargement, normalisation,
	// regeneration parametrique, traitements, visibilite, deplacement) et en
	// oublier UN poserait le tapis de travers, sans rien pour le signaler.
	//
	// La revision de geometrie est deja le signal « les sommets ont bouge » du
	// depot : c'est ce que VBOManager surveille pour reteleverser ses tampons.
	void     UpdateCuttingMatLevel();
	uint64_t SceneSignature() const;
	float    m_cuttingMatZ      = 0.f;
	uint64_t m_matLevelSignature = (uint64_t)-1;

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

	// Possede par le canvas : sa duree de vie est celle de la VUE, pas celle du
	// modele affiche. Basculer d'un modele a l'autre ne doit pas relire l'asset.
	CuttingMat m_cuttingMat;

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
