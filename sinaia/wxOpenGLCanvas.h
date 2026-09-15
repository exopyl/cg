#pragma once

#include "../src/cgre/gl_wrapper.h"
#include "wx/glcanvas.h"
#include <wx/arrstr.h>
#include <wx/dataobj.h>
#include <chrono>
#include <cstddef>
#include <list>
#include "../src/cgre/cgre.h"
#include "../src/cgmesh/vmodels.h"
#include "CuttingMat.h"
#include "ImportSettings.h"


class Mesh;
class BoundingBox;

// Format de glisser-déposer INTERNE : une LISTE de chemins de modèles issus du
// panneau des fichiers de sinaia, séparés par '\n' (un seul chemin = liste à un
// élément, sans séparateur). Format privé à l'application : producteur
// (MyFrame::OnFilesCtrlBeginDrag) et consommateur (ModelDropTarget) sont les
// seuls à le connaître et doivent rester cohérents.
wxDataFormat SinaiaModelPathFormat();

//
// ref : http://wiki.wxwidgets.org/WxGLCanvas
//
class MyGLCanvas: public wxGLCanvas
{
public:
	static const int* GetDefaultAttributes();

	// CONTEXTE DE PARTAGE UNIQUE.
	//
	// Chaque onglet du notebook est un MyGLCanvas distinct, donc un contexte GL
	// distinct. Sans partage, un identifiant d'objet GL (texture, VBO, et demain
	// programme GLSL) cree dans un onglet est INVALIDE dans les autres : c'est ce
	// qui oblige CuttingMat a instancier son tapis par canvas plutot que de le
	// partager, au prix de 54 Mo de texture dupliques par onglet (voir le
	// commentaire de CuttingMat.h).
	//
	// Le contexte racine est cree une seule fois par MyFrame, AVANT tout canvas,
	// avec le MEME format de pixel que les canvas de rendu -- wglShareLists
	// l'exige. Tous les contextes de canvas sont ensuite crees en partage avec
	// lui, ce qui place textures, tampons, shaders et programmes dans un groupe
	// de partage unique.
	//
	// ATTENTION pour la suite : le groupe de partage couvre les objets de DONNEES
	// (textures, buffers, shaders, programmes, renderbuffers) mais PAS les objets
	// conteneurs -- VAO et FBO restent propres a chaque contexte et devront etre
	// crees par canvas.
	static void         SetSharedContext (wxGLContext* pContext);
	static wxGLContext* GetSharedContext ();

	wxGLContext*	m_context = nullptr;
	wxTextCtrl*		m_CtrlLog = nullptr; // TODO : replace with a lambda
	
	MyGLCanvas(wxWindow *parent, wxTextCtrl* pCtrlLog, int *args = 0);
	~MyGLCanvas() override;
	
	void LoadModel(const wxString& filename, const ImportSettings& settings = ImportSettings());
	// Ajoute un fichier à la scène COURANTE (nouveau Model) sans remplacer la vue ni
	// renormaliser (repère monde conservé) ; recadre sur la scène. nullptr si échec.
	Model* AppendModel(const wxString& filename);

	// Ajout d'un LOT de fichiers. Le recadrage est fait UNE seule fois, après le
	// dernier ajout : N appels à AppendModel feraient sauter la vue N fois. Tout
	// chemin d'ajout multiple (menu File > Add, dépôt de plusieurs fichiers) passe
	// par ici. Chaque fichier accepté ou refusé produit sa ligne de journal.
	// Renvoie le nombre de fichiers effectivement ajoutés (0 = aucun, la vue n'a
	// alors pas bougé).
	//
	// wxArrayString et non std::vector<wxString> : wxWidgets instancie explicitement
	// std::vector<wxString> en dllimport, ce qui rend inutilisables les membres que
	// la DLL n'exporte pas (construction par intervalle, assign...). wxArrayString
	// est la seule forme de liste de chaînes que ce projet peut manipuler librement.
	std::size_t AppendModels(const wxArrayString& filenames);

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
	// CIBLE des traitements et des commandes qui opèrent sur UN fichier.
	//
	// Règle : un seul Model dans la scène -> c'est lui, sans exiger de sélection ;
	// plusieurs Model -> le Model sélectionné, et nullptr si aucun ne l'est. Aucun
	// repli sur le Model 0 : dans une scène multi-fichiers sans sélection la cible
	// est ambiguë, et l'appelant doit refuser l'opération au lieu d'en désigner une.
	Model*   GetTargetModel(void) const;
	// Maillages de GetTargetModel(), nullptr s'il n'y a pas de cible.
	VMeshes* GetTargetMeshes(void);
	// Vrai quand GetTargetModel() rend nullptr PARCE QUE la scène compte plusieurs
	// Model sans sélection. Distingue ce cas de la scène vide, pour que l'appelant
	// journalise la raison qui dit quoi faire.
	bool     IsTargetAmbiguous(void) const;
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

	// ------------------------------------------------------------------
	// Reglages DETERMINISTES de la camera, pour le harnais de captures.
	// ------------------------------------------------------------------
	// Une capture de reference n'a de valeur que si elle est reproductible : il
	// faut pouvoir poser exactement le meme point de vue d'une execution a
	// l'autre, ce que la manipulation a la souris ne permet pas. D'ou ces
	// reglages, pilotables depuis la console distante.
	//
	// Convention de ce harnais : azimut = rotation autour de Y (lacet),
	// elevation = rotation autour de X appliquee ENSUITE (tangage), en degres.
	// (0, 0) laisse l'orientation neutre.
	void  SetCameraOrientation (float azimuthDeg, float elevationDeg);
	void  SetCameraZoom (float zoom);
	float GetCameraZoom () const;
	void  ResetCamera ();

	// PIVOT. C'est un ETAT, pose par ces deux actions explicites et par les
	// cadrages du chargement -- jamais derive au vol. Une bascule de visibilite
	// ne le deplace donc PAS : faire sauter la vue sous l'utilisateur parce
	// qu'il a masque un modele serait une regression, pas un service.
	//
	// SetCameraPivot ne touche pas a la distance ; FrameVisibleScene recadre sur
	// la bbox agregee des modeles visibles, base de coupe exclue.
	bool  SetCameraPivot (float x, float y, float z);
	bool  FrameVisibleScene ();

	// RECENTRER SUR UNE CIBLE. Meme geste que FrameVisibleScene -- pivot au
	// centre de la bbox, distance reglee sur SA demi-diagonale -- mais sur un
	// sous-ensemble de la scene. C'est ce que demande le cas d'usage du
	// multi-fichier (AppendModel) : superposer deux reconstructions pour les
	// comparer, donc amener B au centre puis tourner autour de LUI.
	//
	// Cadrer sur une cible ne retire rien de la plage de profondeur : les plans
	// near/far continuent de couvrir toute la scene visible, sinon cadrer sur B
	// ferait disparaitre A -- exactement ce qu'on cherchait a comparer.
	//
	// false si la cible n'existe pas ou si sa bbox est vide/degeneree ; l'appelant
	// distingue ainsi « rien a cadrer » d'un cadrage effectif.
	bool  FrameModel (Model* mdl);
	bool  FrameModelByIndex (std::size_t index);   // indice dans VModels (panneau "Models")
	bool  FrameSelectedModel ();                   // touche F
	// Cadre sur la BASE DE COUPE, a sa cote courante. Reponse au besoin « je veux
	// revoir ma reference metrique » : le tapis reste hors de la sphere de
	// cadrage (D-7), on le vise explicitement au lieu d'elargir le cadrage de
	// toutes les vues.
	bool  FrameCuttingMat ();

	// PANORAMIQUE scripte, en pixels ecran (origine en haut a gauche, comme les
	// evenements souris). Meme chemin que le glisser du bouton du milieu : le
	// critere « le point sous le curseur y reste » se mesure donc sur la loi
	// reellement utilisee, et non sur une replique.
	bool  PanCamera (float dx, float dy);

	// POSER LE PIVOT SUR LE POINT DE SURFACE SOUS UN PIXEL, sans toucher a la
	// distance. C'est le geste Alt + clic gauche (convention Blender / Fusion),
	// et `camera pivot pick PX PY` emprunte CE point d'entree : la commande
	// mesure donc le geste, pas une replique.
	//
	// Un rayon qui ne touche rien LAISSE LE PIVOT EN PLACE. Le remettre a une
	// valeur par defaut ferait sauter la vue sur un clic manque, alors que
	// l'utilisateur n'a rien demande.
	//
	// Un pixel hors du rectangle du viewport est REFUSE ici, et non dans
	// l'appelant : la conversion pixel -> NDC extrapole sans broncher, donc un
	// PX de 10 000 rendrait un rayon parfaitement calcule qui ne correspond a
	// aucun point de l'image. Le refus appartient a la loi, pas a chaque
	// appelant -- sinon la console et la souris divergeraient.
	enum class PivotPickStatus
	{
		Ok,             // pivot pose sur la surface
		NoView,         // pas de camera, ou viewport vide
		OutOfViewport,  // pixel hors du rectangle [0,w] x [0,h] : pivot INCHANGE
		NoHit           // le rayon ne touche aucun modele visible : pivot INCHANGE
	};
	struct PivotPick
	{
		PivotPickStatus status = PivotPickStatus::NoView;
		float  point[3] = { 0.f, 0.f, 0.f };   // point d'impact monde (status Ok)
		float  t        = 0.f;                 // point = origine du rayon + t * direction
		Model* model    = nullptr;             // modele touche (status Ok)
		// Publies dans TOUS les cas : une erreur « hors viewport » qui ne dit pas
		// quelles sont les bornes oblige le script a les redemander ailleurs.
		int    viewportWidth = 0, viewportHeight = 0;
	};
	PivotPick SetPivotFromPixel (float pixelX, float pixelY);

	// ------------------------------------------------------------------
	// INSTRUMENT DE MESURE de la camera
	// ------------------------------------------------------------------
	// Les criteres d'acceptation du chantier caméra s'enoncent en PIXELS (« le
	// pivot reste au centre a +-2 px sur 36 azimuts », « un glisser de H pixels
	// deplace la projection de H pixels »). Les juger sur des images ne refute
	// rien, et le harnais de captures s'est de surcroit revele non reproductible
	// d'un lancement a l'autre. D'ou ces deux lectures chiffrees.
	//
	// Elles se construisent depuis le MODELE de camera (OrbitCamera + viewport)
	// et jamais par glGetFloatv : l'etat GL n'est pas celui de la camera entre
	// un redimensionnement et la peinture suivante, ni apres un Refresh(false)
	// qui planifie sans executer.

	struct CameraInfo
	{
		float pivot[3]       = { 0.f, 0.f, 0.f };
		float eye[3]         = { 0.f, 0.f, 0.f };
		float distance       = 0.f;            // ||oeil - pivot||
		float zNear          = 0.f;            // plans EFFECTIFS de la prochaine image
		float zFar           = 0.f;
		// Sphere de PROFONDEUR : son centre n'est PAS l'origine du monde, et un
		// script qui le supposerait mesurerait faux. Il est donc publie.
		float sceneCenter[3] = { 0.f, 0.f, 0.f };
		float sceneRadius    = 0.f;
		float framingRadius  = 0.f;            // sphere de CADRAGE : borne le dolly
		float fovYDeg        = 0.f;
		int   viewportWidth  = 0;
		int   viewportHeight = 0;
	};
	bool GetCameraInfo (CameraInfo& info) const;

	// Un point hors du volume de vue n'a pas de projection utilisable : le dire
	// vaut mieux que rendre un couple de pixels arbitraire, qu'un script
	// prendrait pour une mesure.
	enum class ProjectStatus
	{
		Ok,
		OkBeyondFar,      // projete, mais au-dela du plan far : invisible
		NoCamera,
		EmptyViewport,
		BehindEye,        // w <= 0 : le point est derriere l'oeil (ou NaN)
		ClippedByNear     // devant le plan near, donc ecrete
	};

	struct ProjectedPoint
	{
		ProjectStatus status = ProjectStatus::NoCamera;
		// Pixels, origine en HAUT A GAUCHE et y vers le bas : la convention des
		// evenements souris de wx (OnMouse, ScreenToRay). Une mesure comparee a
		// un deplacement de curseur doit se lire dans le meme repere.
		float pixelX = 0.f, pixelY = 0.f;
		float ndcX = 0.f, ndcY = 0.f, ndcZ = 0.f;
		float eyeDepth = 0.f;            // profondeur le long de l'axe de vue
		int   viewportWidth = 0, viewportHeight = 0;
	};
	ProjectedPoint ProjectPoint (const float world[3]) const;

	// DEPROJECTION pixel -> rayon monde : l'operation INVERSE de ProjectPoint,
	// dans la meme convention de pixels (origine en haut a gauche, y vers le
	// bas) et sur la meme source (OrbitCamera + viewport).
	//
	// Le Model touche est rendu avec le rayon : c'est le meme chemin que le
	// survol a la souris, donc un script qui l'interroge mesure le picking reel
	// et non une replique.
	struct PickedRay
	{
		bool   valid = false;             // false : pas de camera, ou viewport vide
		float  origin[3]    = { 0.f, 0.f, 0.f };
		float  direction[3] = { 0.f, 0.f, 0.f };   // normalisee
		Model* model = nullptr;           // premier Model visible touche, ou nullptr
		int    viewportWidth = 0, viewportHeight = 0;
	};
	PickedRay UnprojectPixel (float pixelX, float pixelY);

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

	// Les deux moitiés d'un ajout, séparées pour qu'un lot ne recadre qu'une fois.
	// AppendOneModel charge le fichier et l'insère dans la scène sans TOUCHER à la
	// caméra ni redessiner ; FinishAppendBatch clôt le lot (recadrage, mode
	// d'affichage, rafraîchissement). Un ajout isolé enchaîne les deux.
	Model* AppendOneModel(const wxString& filename);
	void   FinishAppendBatch();

	// Position the camera (pivot, eye distance and clip planes) so the given
	// bounding box fits in view, whatever its native scale/position.
	//
	// `target` est la CIBLE DU CADRAGE, pas forcement la scene : elle peut etre
	// un seul modele (touche F, `camera pivot model N`) ou la base de coupe. La
	// boite que les plans de coupe doivent couvrir est relue ici depuis la scene
	// visible, et non deduite de `target` -- les deux ne coincident que lorsque
	// la cible est la scene entiere.
	void FrameCamera(const BoundingBox& target);

	// PLANS DE COUPE, decouples du CADRAGE. La distance de la camera se regle sur
	// le seul modele (c'est lui qu'on observe), mais les plans near/far doivent
	// couvrir tout ce qui est VISIBLE -- base de coupe comprise, sinon ses coins
	// passent derriere le plan far des qu'un petit modele resserre le cadrage.
	//
	// La base de coupe n'entre dans le calcul que si elle est AFFICHEE : elle
	// elargirait sinon la plage de profondeur de toutes les vues, au detriment de
	// la precision du tampon, pour un objet qu'on ne dessine pas.
	//
	// Les DEUX spheres sortent d'ici, et d'ici seulement. Calculees ensemble a
	// partir de la meme boite, elles ne peuvent pas decrire deux scenes
	// differentes -- ce qu'autoriseraient deux chemins de publication distincts.
	void UpdateSceneSpheres();

	// DEUX BOITES, parce que les deux spheres repondent a deux questions.
	//
	// CADRAGE : la derniere CIBLE visee -- la scene visible, un modele, ou le
	// tapis. Elle donne le pivot, la distance d'oeil, les bornes du dolly et la
	// maille de la grille.
	//
	// PROFONDEUR : la scene VISIBLE entiere, tapis exclu ici (il s'y unit dans
	// UpdateSceneSpheres s'il est affiche). Conservee en boite et non en rayon :
	// l'union avec celle du tapis a besoin des deux coins.
	//
	// Les confondre ferait disparaitre, en cadrant sur B, tout ce qui n'est pas B :
	// les plans near/far se resserreraient sur la seule cible.
	//
	// La bascule du tapis rejoue l'union sans retoucher au cadrage : c'est ce qui
	// garantit qu'afficher ou masquer le tapis ne deplace ni le pivot ni la
	// distance d'oeil.
	float m_framingBoundsMin[3] = { 0.f, 0.f, 0.f };
	float m_framingBoundsMax[3] = { 0.f, 0.f, 0.f };
	bool  m_hasFramingBounds    = false;
	float m_sceneBoundsMin[3] = { 0.f, 0.f, 0.f };
	float m_sceneBoundsMax[3] = { 0.f, 0.f, 0.f };
	bool  m_hasSceneBounds    = false;
	float m_framingRadius     = 0.f;   // sphere de CADRAGE : demi-diagonale autour du pivot

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

	// Construit le rayon monde (origine + direction) passant par le pixel (x,y)
	// depuis OrbitCamera et le viewport -- MEME SOURCE que ProjectPoint, dont
	// c'est l'operation inverse, et non l'etat GL courant. false si le viewport
	// est vide ou s'il n'y a pas de camera.
	//
	// Les pixels sont des FLOTTANTS : la souris n'en produit que des entiers,
	// mais l'aller-retour avec ProjectPoint -- qui rend des fractions de pixel --
	// perdrait sa precision a l'arrondi.
	bool ScreenToRay(float x, float y, float orig[3], float dir[3]);

	// Resultat du picking au niveau FICHIER : le Model touche en PREMIER par le
	// rayon souris, et le POINT d'impact.
	//
	// Le point est rendu avec le modele parce que le t du rayon est de toute
	// facon calcule pour departager les touches : ne pas le publier obligerait
	// tout appelant qui en a besoin -- Alt + clic gauche -- a relancer le meme
	// parcours de BVH.
	//
	// `point` est un point de SURFACE quand le modele a des faces. Sur un nuage
	// de points, faute de surface, c'est le point d'ENTREE dans la boite
	// englobante : le repli est le meme que celui du tri des touches.
	struct PickHit
	{
		Model* model    = nullptr;             // nullptr : le rayon ne touche rien
		float  point[3] = { 0.f, 0.f, 0.f };   // valide seulement si model != nullptr
		float  t        = 0.f;                 // point = orig + t * dir, dir normalisee
	};
	// N'inspecte que les Model visibles.
	PickHit PickModel(float x, float y);

	// Racine du groupe de partage GL (voir SetSharedContext). Appartient a
	// MyFrame, qui la cree avant tout canvas et la conserve pour la duree de
	// l'application : ce canvas n'en est jamais proprietaire.
	static wxGLContext* s_pSharedContext;

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

	// Vrai entre l'enfoncement et le relachement d'un Alt + clic gauche. Ce
	// geste pose le pivot et NE DOIT PAS engager le trackball : sans ce drapeau,
	// le relachement et les deplacements intermediaires retomberaient dans les
	// branches de rotation, et le moindre tremblement ferait tourner la vue au
	// moment ou l'utilisateur vise.
	bool     m_altPivotGesture = false;


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
