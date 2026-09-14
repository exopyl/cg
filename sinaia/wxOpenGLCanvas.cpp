#include <wx/textctrl.h>
#include <wx/dcclient.h>

#define _WINSOCKAPI_
#include "../src/cgre/gl_wrapper.h"
#include "../src/cgmath/cgmath.h"
#include "../src/cgmesh/bounding_box.h"
#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/mesh_data_manager.h"

#include "wxOpenGLCanvas.h"
#include "SinaiaFrame.h"
#include <wx/statusbr.h>
#include <wx/image.h>
#include <wx/dnd.h>
#include <wx/filename.h>
#include <cstring>
#include <cmath>
#include <algorithm>

// Cible de glisser-déposer du canvas, deux formats :
//  - format INTERNE SinaiaModelPathFormat (glisser depuis le panneau des fichiers)
//    -> AJOUT à la vue courante (AppendModel) ;
//  - format FICHIER de l'OS (glisser depuis l'explorateur Windows)
//    -> OUVERTURE dans une NOUVELLE vue (onglet), via MyFrame::LoadModelFile.
// GetReceivedFormat() indique lequel a été déposé.
namespace {
class ModelDropTarget : public wxDropTarget
{
public:
	explicit ModelDropTarget(MyGLCanvas* canvas) : m_canvas(canvas)
	{
		wxDataObjectComposite* composite = new wxDataObjectComposite();
		m_internal = new wxCustomDataObject(SinaiaModelPathFormat());
		m_files    = new wxFileDataObject();
		composite->Add(m_internal, true);   // format préféré
		composite->Add(m_files);
		m_composite = composite;
		SetDataObject(composite);           // le drop target en prend possession
	}

	wxDragResult OnData(wxCoord, wxCoord, wxDragResult def) override
	{
		if (!GetData())
			return wxDragNone;

		MyFrame* frame = dynamic_cast<MyFrame*>(wxGetTopLevelParent(m_canvas));

		if (m_composite->GetReceivedFormat() == SinaiaModelPathFormat())
		{
			// Glisser interne (panneau des fichiers) -> ajout à la vue courante.
			const wxString path = wxString::FromUTF8(
				static_cast<const char*>(m_internal->GetData()), m_internal->GetSize());
			if (path.empty() || !m_canvas->AppendModel(path))
				return wxDragNone;
			if (frame)
				frame->OnSceneChanged();
			return def;
		}

		// Dépôt de fichiers de l'OS -> chaque fichier ouvert dans une nouvelle vue.
		const wxArrayString files = m_files->GetFilenames();
		if (!frame || files.empty())
			return wxDragNone;
		for (const auto& f : files)
			frame->LoadModelFile(f);
		return def;
	}

private:
	MyGLCanvas*            m_canvas;
	wxDataObjectComposite* m_composite;
	wxCustomDataObject*    m_internal;
	wxFileDataObject*      m_files;
};
} // namespace

wxDataFormat SinaiaModelPathFormat()
{
	return wxDataFormat(wxT("sinaia/model-path"));
}

BEGIN_EVENT_TABLE(MyGLCanvas, wxGLCanvas)
    EVT_SIZE(MyGLCanvas::OnSize)
    EVT_PAINT(MyGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(MyGLCanvas::OnEraseBackground)
    EVT_MOUSE_EVENTS(MyGLCanvas::OnMouse)
    EVT_KEY_DOWN(MyGLCanvas::OnKeyDown)
END_EVENT_TABLE()

const int* MyGLCanvas::GetDefaultAttributes()
{
	static const int attributes[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 24, WX_GL_SAMPLE_BUFFERS, 1, WX_GL_SAMPLES, 4, 0 };
	return attributes;
}

wxGLContext* MyGLCanvas::s_pSharedContext = nullptr;

void MyGLCanvas::SetSharedContext (wxGLContext* pContext)
{
	s_pSharedContext = pContext;
}

wxGLContext* MyGLCanvas::GetSharedContext ()
{
	return s_pSharedContext;
}

//
//
//
MyGLCanvas::MyGLCanvas(wxWindow *parent, wxTextCtrl* pCtrlLog, int *args)
	: wxGLCanvas(parent, wxID_ANY, args ? args : GetDefaultAttributes(), wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS)
{
	m_CtrlLog = pCtrlLog;
	// Cree en PARTAGE avec le contexte racine de MyFrame (cf. SetSharedContext) :
	// textures, VBO et programmes GLSL sont alors communs a tous les onglets. Si
	// la racine n'a pas ete posee, wx recoit nullptr et l'on retombe sur
	// l'ancien comportement -- un groupe de partage par canvas.
	m_context = new wxGLContext(this, s_pSharedContext);
	//m_context->SetCurrent(static_cast<wxGLCanvas>(this));

	wglMakeCurrent(this->GetHDC(), m_context->GetGLRC());

	// 
	int version = gladLoaderLoadWGL(this->GetHDC());
	int versionGL = gladLoaderLoadGL();
	if (version == 0) {
		printf("Failed to initialize OpenGL context\n");
		return;
	}

	int argc = 1;
	char* argv[1] = { wxString((wxTheApp->argv)[0]).char_str() };
	//glutInit (&argc, argv);

	m_bInitialized = false;

	m_fBackgroundColor[0] = 1.;
	m_fBackgroundColor[1] = 1.;
	m_fBackgroundColor[2] = 1.;

	rendering_properties_init (prop);
	m_bBoundingBox = false;

	m_pTrackball = new Ctrackball ();
	m_pTrackball->set_zoom (-5.f);

	m_pMesh = nullptr;

	m_fFovy = 45.f;
	m_fWindowWidth = 0.f;
	m_fWindowHeight = 0.f;
	m_fNear = 0.001f;
	m_fFar = 100.0f;

	// Accepte le glisser-déposer de fichiers : chaque fichier lâché est ajouté à la scène.
	SetDropTarget(new ModelDropTarget(this));
}

//
//
//
MyGLCanvas::~MyGLCanvas()
{
	if (m_pVModels)
	{
		// RemoveMesh libere desormais les textures des materiaux du maillage : il
		// faut donc un contexte courant, sinon glDeleteTextures ne fait rien et
		// elles survivent jusqu'a la fin du processus. On ne le pose que si la
		// fenetre est encore a l'ecran -- a la fermeture de l'application elle ne
		// l'est plus, et le pilote reprend tout de toute facon.
		if (IsShownOnScreen())
			SetCurrent(*m_context);

		for (auto& mdl : m_pVModels->GetModels())
			for (auto* mesh : mdl->m_meshes.GetMeshes())
				if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);
	}

	delete m_pMesh;
	m_pMesh = nullptr;

	delete m_pVModels;
	m_pVModels = nullptr;

	delete m_context;
/*

	if (m_pNPRManager) delete m_pNPRManager;
	m_listId.clear ();
	m_listSegments.clear ();
*/
}

Mesh* MyGLCanvas::GetMesh(void)
{
	return m_pMesh;
};

void MyGLCanvas::SetMesh(Mesh *pMesh)
{
	delete m_pMesh;

	m_pMesh = pMesh;
	m_pMesh->centerize();
	m_pMesh->computebbox();
	float fLargestLength = m_pMesh->GetLargestLength();
	m_pMesh->scale(1.f / fLargestLength);
	m_pMesh->ComputeNormals();

	Refresh(false);
};


VMeshes* MyGLCanvas::GetVMeshes(void)
{
	// Fichier actif = premier Model de la scène (comportement mono-fichier conservé).
	if (m_pVModels && m_pVModels->GetNModels() > 0)
		return &m_pVModels->GetModels()[0]->m_meshes;
	return nullptr;
};

void MyGLCanvas::SetVMeshes(VMeshes* pObject, bool normalize)
{
	AdoptScene(pObject);
	ApplyNormalization(normalize);   // géométrie finale + BVH picking (voir ApplyNormalization)
}

// Remplace la géométrie SANS toucher à la caméra ni aux positions : ni
// normalisation, ni recadrage, ni remise à zéro du trackball. C'est ce qu'exige
// l'édition d'un paramètre, où l'on veut voir l'effet du réglage sur la forme --
// renormaliser la recentrerait et la remettrait à l'échelle à chaque cran de
// curseur, ce qui masque précisément ce que le paramètre fait.
void MyGLCanvas::UpdateGeometryKeepingView(VMeshes* pObject)
{
	AdoptScene(pObject);

	VMeshes* vm = GetVMeshes();
	if (!vm) return;

	// La geometrie regeneree arrive dans les unites du generateur, alors que la
	// scene affichee est celle du chargement, normalisee. On lui applique donc la
	// MEME transformation qu alors, figee : sans elle le modele sauterait d un coup
	// a son echelle brute, hors du cadrage etabli.
	if (m_hasNormalization)
	{
		for (const auto& mesh : vm->GetMeshes())
		{
			if (!mesh) continue;
			mesh->translate(-m_normCenter[0], -m_normCenter[1], -m_normCenter[2]);
			mesh->scale(m_normScale);
		}
	}

	for (const auto& mesh : vm->GetMeshes())
	{
		mesh->ComputeNormals();
		mesh->computebbox();
	}
	RefreshGeometryState();
}

// Adopte `pObject` comme scène courante, l'ancienne étant détachée du renderer et
// détruite. Ne touche ni à la caméra ni à la géométrie : c'est aux appelants de
// décider ce qu'ils en font.
void MyGLCanvas::AdoptScene(VMeshes* pObject)
{
	// Détache l'ancienne scène du renderer puis la détruit.
	if (m_pVModels)
	{
		for (auto& mdl : m_pVModels->GetModels())
			for (auto* mesh : mdl->m_meshes.GetMeshes())
				if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);
	}
	delete m_pVModels;
	m_hoveredModel = nullptr;    // l'ancienne scène est détruite -> plus de survol valide
	m_selectedModel = nullptr;   // idem sélection

	// Nouvelle scène : un unique Model qui ADOPTE les maillages de pObject
	// (échange de vecteurs), après quoi pObject vidé est détruit.
	m_pVModels = new VModels();
	Model* mdl = m_pVModels->Add("");
	if (pObject)
	{
		mdl->m_meshes.GetMeshes().swap(pObject->GetMeshes());
		delete pObject;
	}
	m_selectedModel = mdl;   // sélection initiale = le modèle chargé
}

void MyGLCanvas::ApplyNormalization(bool normalize)
{
	if (!m_pVModels) return;

	// TOUTE LA SCENE, pas le premier fichier.
	//
	// Cette fonction passait par GetVMeshes(), qui ne rend que le Model n° 0
	// (« comportement mono-fichier conservé », wxOpenGLCanvas.cpp:219). Depuis que
	// la vue accepte plusieurs fichiers, Treatments > Normalize ne touchait donc
	// que le premier : les suivants gardaient leur echelle d'origine.
	//
	// Et la transformation est COMMUNE a tous les maillages -- un seul centre, un
	// seul facteur, derives de la boite englobante agregee. Normaliser chaque
	// Model separement, en appelant VMeshes::Normalize par modele, les ramenerait
	// tous a la meme taille et les empilerait a l'origine : la comparaison de
	// plusieurs reconstructions, qui est l'usage meme du multi-fichier, n'aurait
	// plus de sens.
	//
	// Les modeles masques sont inclus : les laisser de cote ferait reapparaitre
	// un modele a une echelle sans rapport le jour ou on le reaffiche.
	std::vector<Mesh*> allMeshes;
	for (const auto& mdl : m_pVModels->GetModels())
		for (auto* mesh : mdl->m_meshes.GetMeshes())
			if (mesh) allMeshes.push_back(mesh);

	if (allMeshes.empty()) return;

	for (auto* mesh : allMeshes)
		mesh->ComputeNormals();

	if (normalize)
	{
		// On retient la transformation avant de l appliquer : les regenerations
		// ulterieures la rejouent a l identique au lieu de renormaliser. Meme calcul
		// que VMeshes::Normalize -- translate(-centre) puis mise a l echelle pour
		// amener la plus grande dimension a VMeshes::kNormalizedSize -- dont on a
		// besoin de la VALEUR, pas seulement de l effet. Le facteur DOIT rester
		// aligne sur celui de Normalize, sinon une regeneration parametrique
		// changerait la taille du modele sous le curseur.
		BoundingBox raw;
		for (auto* mesh : allMeshes)
		{
			mesh->computebbox();
			raw.AddBoundingBox(mesh->bbox());
		}
		raw.GetCenter(m_normCenter);
		const float largest = raw.GetLargestLength();
		m_normScale = (largest > 0.f) ? VMeshes::kNormalizedSize / largest : 1.f;
		m_hasNormalization = true;

		// Applique a la main plutot que par VMeshes::Normalize, qui recalcule sa
		// propre boite par VMeshes et ne saurait donc pas partager un centre et un
		// facteur entre plusieurs Model.
		for (auto* mesh : allMeshes)
		{
			mesh->translate(-m_normCenter[0], -m_normCenter[1], -m_normCenter[2]);
			mesh->scale(m_normScale);
			mesh->IncrementRevision();   // sans quoi le VBO garde l'ancienne geometrie
		}
	}

	// Always compute the aggregate bounding box for all meshes AFTER potential normalization
	BoundingBox aggregateBbox;
	for (auto* mesh : allMeshes)
	{
		mesh->computebbox(); // Ensure individual mesh bboxes are up-to-date
		aggregateBbox.AddBoundingBox(mesh->bbox());
	}

	// Frame the camera on the resulting model.
	m_pTrackball->ResetTransformations(); // orientation neutre ; le cadrage suit
	FrameCamera(aggregateBbox);

	RefreshGeometryState();
}

// Ce que TOUT changement de géométrie doit refaire, normalisation ou pas :
// diagnostic topologique, mode d'affichage d'un nuage sans faces, BVH de picking,
// et le rafraîchissement. Partagé avec UpdateGeometryKeepingView.
void MyGLCanvas::RefreshGeometryState()
{
	VMeshes* vm = GetVMeshes();
	if (!vm) return;

	UpdateTopologicIssues();

	// A face-less model (point cloud: .ply/.pset/.pts/.asc with only vertices)
	// has no surface to fill, so the default fill/VBO path draws nothing — the
	// model loads invisible. Switch to point display so it is visible on import.
	if (vm->GetNVertices() > 0 && vm->GetNFaces() == 0)
	{
		// If the model carries explicit line ('l') / point ('p') primitives,
		// they draw themselves (with their configurable line/point colours). The
		// all-vertices point overlay would then draw duplicate points on top and,
		// winning the equal-depth test, mask the point colour — so only enable it
		// for a genuine vertex-only cloud.
		bool hasPrimitives = false;
		for (auto* m : vm->GetMeshes())
			if (m && (m->GetNLines() > 0 || m->GetNPoints() > 0)) { hasPrimitives = true; break; }
		prop.display_points = !hasPrimitives;
		prop.display_fill   = false;
	}

	// (Re)construit le BVH de picking sur la géométrie DÉFINITIVE. Indispensable
	// après une normalisation (recentrage/mise à l'échelle) : sans ce rebuild, le
	// BVH référencerait les positions d'avant, RayNearestSurface raterait tous les
	// triangles et le survol (bbox jaune) disparaîtrait. Couvre aussi le chargement.
	//
	// TOUS les modèles, pas seulement le premier : la normalisation déplace
	// désormais la scène entière, donc tous les BVH sont périmés, pas un seul.
	if (m_pVModels)
		for (const auto& mdl : m_pVModels->GetModels())
			if (mdl) mdl->BuildBVH();

	Refresh(false);
}

//
// Set the camera distance and clip planes. The camera orbits the WORLD ORIGIN
// (it is not recentred on the model), so each model is shown at its own
// coordinate position — a normalized model sits at the origin and appears
// centred, a non-normalized one appears offset by its native coordinates.
// We fit the sphere *centred on the origin* that contains the model, which
// just guarantees it stays visible and inside the clip planes.
//
// ---------------------------------------------------------------------------
//  Reglages deterministes de la camera (harnais de captures)
// ---------------------------------------------------------------------------
// L'orientation est POSEE, pas accumulee : c'est ce qui rend un point de vue
// reproductible d'une execution a l'autre. Elle est transmise en
// COLONNE-MAJEUR, m[4*colonne + ligne], et vaut R = Rx(elevation) * Ry(azimut).
void MyGLCanvas::SetCameraOrientation (float azimuthDeg, float elevationDeg)
{
	if (!m_pTrackball) return;

	const float kDegToRad = 3.14159265358979f / 180.f;
	const float ca = std::cos (azimuthDeg   * kDegToRad);
	const float sa = std::sin (azimuthDeg   * kDegToRad);
	const float ce = std::cos (elevationDeg * kDegToRad);
	const float se = std::sin (elevationDeg * kDegToRad);

	const GLfloat m[16] = {
		 ca,      se * sa, -ce * sa, 0.f,   // colonne 0
		0.f,      ce,       se,      0.f,   // colonne 1
		 sa,     -se * ca,  ce * ca, 0.f,   // colonne 2
		0.f,     0.f,      0.f,      1.f    // colonne 3
	};
	m_pTrackball->set_rotation (m);

	// La rotation seule : ni le pivot ni la distance ne bougent, sinon
	// `camera azel` deplacerait aussi le cadrage sans le dire.
	Refresh (false);
}

void MyGLCanvas::SetCameraZoom (float zoom)
{
	if (!m_pTrackball) return;
	m_pTrackball->set_zoom (zoom);
	UpdateSceneSpheres ();
	Refresh (false);
}

float MyGLCanvas::GetCameraZoom () const
{
	return m_pTrackball ? m_pTrackball->get_zoom () : 0.f;
}

bool MyGLCanvas::GetCameraInfo (CameraInfo& info) const
{
	if (!m_pTrackball) return false;

	const OrbitCamera& cam = m_pTrackball->camera ();
	const TVector3<float>& pivot = cam.GetPivot ();
	const TVector3<float>  eye   = cam.GetEyePosition ();

	info.pivot[0] = pivot.x; info.pivot[1] = pivot.y; info.pivot[2] = pivot.z;
	info.eye[0]   = eye.x;   info.eye[1]   = eye.y;   info.eye[2]   = eye.z;
	info.distance = cam.GetDistance ();
	m_pTrackball->get_clip_planes (&info.zNear, &info.zFar);
	m_pTrackball->get_depth_center (info.sceneCenter);
	info.sceneRadius   = m_pTrackball->get_depth_radius ();
	info.framingRadius = m_pTrackball->get_framing_radius ();
	info.fovYDeg       = m_pTrackball->get_fov_y_deg ();

	int w = 0, h = 0;
	GetClientSize (&w, &h);
	info.viewportWidth  = w;
	info.viewportHeight = h;
	return true;
}

namespace {

// REPERE ECRAN <-> NDC, ecrit UNE SEULE FOIS pour les deux sens.
//
// Origine en HAUT A GAUCHE et y vers le BAS -- la convention des evenements
// souris de wx, et non celle d'OpenGL. ProjectPoint et ScreenToRay etant des
// operations inverses, la seule erreur que ce changement de repere puisse
// porter est un signe sur y ; deux formulations distantes de trois cents
// lignes ne le montreraient qu'au picking.
//
// Les bornes sont les BORDS du viewport : le pixel 0 est en ndc -1, le pixel w
// en ndc +1. Un decalage d'un demi-pixel ici casserait l'aller-retour.
void PixelFromNdc (float ndcX, float ndcY, int width, int height,
                   float* pixelX, float* pixelY)
{
	*pixelX = (0.5f * ndcX + 0.5f) * (float)width;
	*pixelY = (0.5f - 0.5f * ndcY) * (float)height;
}

void NdcFromPixel (float pixelX, float pixelY, int width, int height,
                   float* ndcX, float* ndcY)
{
	*ndcX = 2.f * pixelX / (float)width  - 1.f;
	*ndcY = 1.f - 2.f * pixelY / (float)height;
}

} // namespace

// Projection monde -> pixels par les matrices que set_camera enverra a GL, et
// non par celles qui s'y trouvent : voir le commentaire de la declaration.
MyGLCanvas::ProjectedPoint MyGLCanvas::ProjectPoint (const float world[3]) const
{
	ProjectedPoint out;
	if (!m_pTrackball)
		return out;                       // status = NoCamera

	int w = 0, h = 0;
	GetClientSize (&w, &h);
	out.viewportWidth  = w;
	out.viewportHeight = h;
	if (w <= 0 || h <= 0)
	{
		out.status = ProjectStatus::EmptyViewport;
		return out;
	}

	float mv[16], pr[16];
	m_pTrackball->get_modelview_matrix (mv);
	m_pTrackball->get_projection_matrix (pr);

	// eye = MV . (x,y,z,1), puis clip = P . eye. Colonne-majeur : l'element
	// (ligne, colonne) est en [4*colonne + ligne].
	float eye[4], clip[4];
	for (int r = 0; r < 4; r++)
		eye[r] = mv[0*4+r]*world[0] + mv[1*4+r]*world[1] + mv[2*4+r]*world[2] + mv[3*4+r];
	for (int r = 0; r < 4; r++)
		clip[r] = pr[0*4+r]*eye[0] + pr[1*4+r]*eye[1] + pr[2*4+r]*eye[2] + pr[3*4+r]*eye[3];

	// w vaut -z_oeil pour ce frustum : c'est la profondeur le long de l'axe de
	// vue. La negation du test attrape aussi NaN.
	out.eyeDepth = clip[3];
	if (!(clip[3] > 0.f))
	{
		out.status = ProjectStatus::BehindEye;
		return out;
	}

	out.ndcX = clip[0] / clip[3];
	out.ndcY = clip[1] / clip[3];
	out.ndcZ = clip[2] / clip[3];
	if (out.ndcZ < -1.f)
	{
		out.status = ProjectStatus::ClippedByNear;
		return out;
	}

	PixelFromNdc (out.ndcX, out.ndcY, w, h, &out.pixelX, &out.pixelY);
	out.status = (out.ndcZ > 1.f) ? ProjectStatus::OkBeyondFar : ProjectStatus::Ok;
	return out;
}

// Remet l'orientation a zero, puis recadre sur la scene visible -- ce qui
// repose pivot et distance. C'est le point de depart de toute capture de
// reference.
void MyGLCanvas::ResetCamera ()
{
	if (!m_pTrackball) return;
	m_pTrackball->ResetTransformations ();
	FrameVisibleScene ();
	Refresh (false);
}

// Recadre sur la bbox agregee des modeles VISIBLES : pivot au centre, distance
// telle que la scene tienne dans le champ. La grille suit -- elle est derivee du
// cadrage, elle n'a plus a le contraindre.
//
// La base de coupe est EXCLUE : c'est un decor metrique, pas le sujet observe.
// Elle n'entre que dans la sphere de profondeur (UpdateSceneSpheres).
bool MyGLCanvas::FrameVisibleScene ()
{
	if (!m_pVModels)
		return false;
	const BoundingBox bb = m_pVModels->AggregateBBox (true);
	if (bb.IsEmpty ())
		return false;
	FrameCamera (bb);
	return true;
}

// RECENTRER SUR UN MODELE. Le pivot va au centre de SA bbox et la distance se
// regle sur SA demi-diagonale : c'est ce qui distingue ce geste de
// FrameVisibleScene, qui cadre l'agregat. Sur deux reconstructions superposees,
// l'agregat cadre le vide entre les deux ; la cible, elle, remplit l'image.
bool MyGLCanvas::FrameModel (Model* mdl)
{
	if (!mdl)
		return false;

	// La bbox est recalculee et non relue : le champ m_bbox date du dernier
	// cadrage ou du dernier survol, et la geometrie a pu bouger depuis (drop,
	// normalisation, regeneration d'une forme parametree).
	//
	// COPIE et non reference : FrameCamera relit la scene visible par
	// AggregateBBox, qui reecrit m_bbox de chaque modele -- y compris celui-ci.
	const BoundingBox bb = mdl->ComputeBBox ();
	if (bb.IsEmpty ())
		return false;

	float mn[3], mx[3];
	bb.GetMinMax (mn, mx);
	if (BoundingSphereOfBox (mn, mx).radius <= 0.f)
		return false;   // modele reduit a un point : aucune distance a en deduire

	FrameCamera (bb);
	Refresh (false);
	return true;
}

bool MyGLCanvas::FrameModelByIndex (std::size_t index)
{
	return m_pVModels ? FrameModel (m_pVModels->GetModel (index)) : false;
}

bool MyGLCanvas::FrameSelectedModel ()
{
	return FrameModel (m_selectedModel);
}

// CADRER SUR LA BASE DE COUPE. Le tapis reste exclu de la sphere de cadrage
// (D-7) -- l'y faire entrer couterait le cadrage de toutes les autres vues. On
// le VISE donc explicitement quand on veut relire la reference metrique, ce qui
// est une cible de plus et non une regle changee.
bool MyGLCanvas::FrameCuttingMat ()
{
	float mn[3], mx[3];
	CuttingMat::Bounds (m_cuttingMatZ, mn, mx);

	BoundingBox bb;
	bb.AddPoint (mn[0], mn[1], mn[2]);
	bb.AddPoint (mx[0], mx[1], mx[2]);

	FrameCamera (bb);
	Refresh (false);
	return true;
}

// Pose le pivot sans toucher a la distance : l'utilisateur choisit ce autour de
// quoi il tourne, pas de combien il recule.
bool MyGLCanvas::SetCameraPivot (float x, float y, float z)
{
	if (!m_pTrackball)
		return false;
	m_pTrackball->set_pivot (x, y, z);
	Refresh (false);
	return true;
}

// Panoramique scripte. Il passe par le meme point d'entree que le glisser du
// bouton du milieu, sinon le critere mesurerait une replique de la loi au lieu
// de la loi.
bool MyGLCanvas::PanCamera (float dx, float dy)
{
	if (!m_pTrackball)
		return false;
	m_pTrackball->pan_screen (dx, dy);
	Refresh (false);
	return true;
}

// DEUX SPHERES, et non une. Elles repondent a deux questions differentes et se
// confondre leur ferait donner la mauvaise reponse a l'une des deux :
//
//   - CADRAGE (centre de bbox, demi-diagonale) : ou placer l'oeil pour que le
//     sujet remplisse le champ. Elle suit le sujet, donc elle est SERREE, et
//     elle exclut la base de coupe -- un decor ne doit reculer ni la camera ni
//     les bornes du dolly.
//   - PROFONDEUR (centre de la scene visible UNIE a la base de coupe si elle est
//     affichee, demi-diagonale de cette union) : que doivent encadrer les plans
//     de coupe.
//
// L'ancienne forme n'en avait qu'une, `||(max|x|, max|y|, max|z|)||`, la plus
// petite sphere centree sur l'ORIGINE. Pour un modele normalise mono-fichier
// elle coincide exactement avec la demi-diagonale -- une bbox centree sur
// l'origine verifie max|x| = ex/2 -- ce qui explique que le defaut ait survecu :
// le cas le plus frequent est le seul ou les deux formules s'accordent.
void MyGLCanvas::FrameCamera(const BoundingBox& target)
{
	if (target.IsEmpty())
		return;

	float mn[3], mx[3];
	target.GetMinMax(mn, mx);

	// --- sphere de CADRAGE : centre de bbox, demi-diagonale ---------------
	const BoundingSphere framing = BoundingSphereOfBox(mn, mx);
	if (framing.radius <= 0.f)
		return;   // bbox degeneree : un point, aucune distance a en deduire

	for (int i = 0; i < 3; ++i)
	{
		m_framingBoundsMin[i] = mn[i];
		m_framingBoundsMax[i] = mx[i];
	}
	m_hasFramingBounds = true;

	// La boite de PROFONDEUR se relit sur la scene visible et NON sur la cible.
	// Cadrer sur un modele resserre la distance d'oeil, jamais la plage de
	// profondeur : sinon viser B ferait sortir A de l'intervalle near/far, et le
	// cas d'usage du multi-fichier est precisement de les voir tous les deux.
	//
	// Cout : un AggregateBBox par CADRAGE (chargement, reset, touche F), jamais
	// par image -- c'est la frontiere posee par E-6.
	const BoundingBox visible = m_pVModels ? m_pVModels->AggregateBBox(true) : BoundingBox();
	if (!visible.IsEmpty())
	{
		visible.GetMinMax(m_sceneBoundsMin, m_sceneBoundsMax);
		m_hasSceneBounds = true;
	}
	else
	{
		// Aucun modele visible (scene vide, ou cadrage sur le tapis seul) : la
		// cible fait office de plage de profondeur, faute de mieux.
		for (int i = 0; i < 3; ++i)
		{
			m_sceneBoundsMin[i] = mn[i];
			m_sceneBoundsMax[i] = mx[i];
		}
		m_hasSceneBounds = true;
	}

	const float halfFovy = (m_fFovy * 0.5f) * 3.14159265f / 180.f;
	const float sinHalf  = sinf(halfFovy);
	const float distance = (sinHalf > 1e-4f ? framing.radius / sinHalf : framing.radius * 3.f) * 1.2f;

	// Les deux spheres partent EN PREMIER : set_zoom borne la distance d'oeil
	// contre le rayon de cadrage, donc les publier apres le zoom bornerait le
	// cadrage avec la scene PRECEDENTE.
	// C'est aussi ce qui laisse le trackball rederiver near/far de la distance
	// d'oeil courante a chaque image, si bien que les plans suivent le zoom.
	UpdateSceneSpheres();
	m_pTrackball->set_pivot(framing.center[0], framing.center[1], framing.center[2]);
	m_pTrackball->set_zoom(-distance);
}

// Recalcule et publie les deux spheres. Appelee au cadrage, et a chaque
// evenement qui change la plage de profondeur SANS changer le cadrage --
// bascule du tapis, tapis repose a une autre cote.
void MyGLCanvas::UpdateSceneSpheres()
{
	if (!m_pTrackball)
		return;

	// --- CADRAGE : la CIBLE du dernier cadrage SEULE, base de coupe exclue --
	// La grille s'en derive, elle ne le contraint plus : c'est ce qui autorise
	// le retrait du rembourrage +-(2,2,1) chez les appelants de FrameCamera.
	m_framingRadius = m_hasFramingBounds
	                ? BoundingSphereOfBox(m_framingBoundsMin, m_framingBoundsMax).radius
	                : 0.f;

	// --- PROFONDEUR : scene visible UNIE a la base de coupe si affichee -----
	float mn[3], mx[3];
	bool  hasBox = m_hasSceneBounds;
	for (int i = 0; i < 3; ++i)
	{
		mn[i] = m_sceneBoundsMin[i];
		mx[i] = m_sceneBoundsMax[i];
	}

	if (prop.display_cutting_mat)
	{
		float matMn[3], matMx[3];
		CuttingMat::Bounds(m_cuttingMatZ, matMn, matMx);
		for (int i = 0; i < 3; ++i)
		{
			mn[i] = hasBox ? std::min(mn[i], matMn[i]) : matMn[i];
			mx[i] = hasBox ? std::max(mx[i], matMx[i]) : matMx[i];
		}
		hasBox = true;
	}

	const BoundingSphere depth = hasBox ? BoundingSphereOfBox(mn, mx) : BoundingSphere();
	m_pTrackball->set_scene_spheres(depth.center, depth.radius, m_framingRadius);
}

// Signature bon marche de la scene. Tout ce qui peut deplacer le minimum Z
// change au moins l'un des trois termes : le nombre de Model, leur visibilite,
// la revision de geometrie de leurs maillages.
uint64_t MyGLCanvas::SceneSignature() const
{
	uint64_t h = 1469598103934665603ull;   // FNV-1a 64 bits
	auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };

	if (!m_pVModels)
		return h;

	mix(m_pVModels->GetNModels());
	for (const auto& mdl : m_pVModels->GetModels())
	{
		if (!mdl) continue;
		mix(mdl->m_visible ? 1ull : 2ull);
		for (const auto* mesh : mdl->m_meshes.GetMeshes())
			if (mesh) mix(mesh->GetRevision());
	}
	return h;
}

void MyGLCanvas::UpdateCuttingMatLevel()
{
	const uint64_t sig = SceneSignature();
	if (sig == m_matLevelSignature)
		return;
	m_matLevelSignature = sig;

	// AggregateBBox recalcule les bboxes des maillages : c'est le calcul cher, et
	// c'est justement pourquoi il est garde par la signature plutot que refait a
	// chaque image.
	float z = 0.f;
	if (m_pVModels)
	{
		const BoundingBox bb = m_pVModels->AggregateBBox(true);
		if (!bb.IsEmpty())
		{
			float mn[3], mx[3];
			bb.GetMinMax(mn, mx);
			z = mn[2];
		}
	}
	if (z == m_cuttingMatZ)
		return;

	m_cuttingMatZ = z;
	// Le tapis a change de cote : les plans de coupe doivent le couvrir la.
	UpdateSceneSpheres();
}

namespace {

// Intersection rayon (o + t*d) / AABB [mn,mx] (slab). Renvoie true et t d'entrée
// (>= 0 ; 0 si l'origine est dans la boîte) si intersection devant l'origine.
bool rayAabb(const float o[3], const float d[3], const float mn[3], const float mx[3], float& tHit)
{
	float t0 = -1e30f, t1 = 1e30f;
	for (int a = 0; a < 3; a++)
	{
		if (std::fabs(d[a]) < 1e-12f)
		{
			if (o[a] < mn[a] || o[a] > mx[a]) return false;   // parallèle et hors slab
		}
		else
		{
			float inv = 1.0f / d[a];
			float ta = (mn[a] - o[a]) * inv;
			float tb = (mx[a] - o[a]) * inv;
			if (ta > tb) std::swap(ta, tb);
			if (ta > t0) t0 = ta;
			if (tb < t1) t1 = tb;
			if (t0 > t1) return false;
		}
	}
	if (t1 < 0.0f) return false;      // boîte entièrement derrière
	tHit = (t0 >= 0.0f) ? t0 : 0.0f;  // origine dans la boîte -> 0
	return true;
}

} // namespace

// Rayon monde du pixel (x, y), construit depuis OrbitCamera et le viewport --
// et NON par inversion des matrices lues dans GL.
//
// Deux etats GL sont perimes au moment ou le survol arrive :
//   - ResetProjectionMode remet GL_MODELVIEW a l'IDENTITE et reecrit
//     GL_PROJECTION ; il s'execute sur OnSize, donc un survol qui tombe entre
//     un redimensionnement et la peinture suivante desprojetait avec
//     l'identite ;
//   - les commandes de camera appellent Refresh(false), qui PLANIFIE une
//     peinture sans l'executer : un picking scripte lisait les matrices de
//     l'image precedente.
//
// Meme source que ProjectPoint, dont cette fonction est l'inverse : le meme
// convertisseur de repere, puis la camera pour le reste.
bool MyGLCanvas::ScreenToRay(float x, float y, float orig[3], float dir[3])
{
	if (!m_pTrackball)
		return false;

	int w = 0, h = 0;
	GetClientSize(&w, &h);
	if (w <= 0 || h <= 0)
		return false;

	float ndcX = 0.f, ndcY = 0.f;
	NdcFromPixel(x, y, w, h, &ndcX, &ndcY);
	m_pTrackball->get_pick_ray(ndcX, ndcY, orig, dir);
	return true;
}

MyGLCanvas::PickHit MyGLCanvas::PickModel(float x, float y)
{
	PickHit hit;
	if (!m_pVModels)
		return hit;
	float o[3], d[3];
	if (!ScreenToRay(x, y, o, d))
		return hit;

	Model* best = nullptr;
	float  bestT = 1e30f;
	for (auto& mdl : m_pVModels->GetModels())
	{
		if (!mdl->m_visible)
			continue;
		mdl->ComputeBBox();
		if (mdl->m_bbox.IsEmpty())
			continue;

		// 1) Filtre AABB : rejet rapide si le rayon manque la boîte englobante.
		float mn[3], mx[3], tBox;
		mdl->m_bbox.GetMinMax(mn, mx);
		if (!rayAabb(o, d, mn, mx, tBox))
			continue;

		// 2) Départage par la SURFACE (BVH triangles) si le modèle en a une ;
		//    sinon (nuage de points sans faces) repli sur la distance d'entrée AABB.
		float t;
		if (mdl->HasSurface())
		{
			t = mdl->RayNearestSurface(o, d);
			if (t < 0.f)
				continue;   // boîte touchée mais aucune face -> pas un survol réel
		}
		else
		{
			t = tBox;
		}

		if (t < bestT)
		{
			bestT = t;
			best = mdl.get();
		}
	}

	if (!best)
		return hit;

	// La direction rendue par ScreenToRay est normalisee (OrbitCamera::RayFromNdc
	// divise par sa norme), donc bestT est une distance monde le long du rayon et
	// non un parametre d'echelle arbitraire.
	hit.model = best;
	hit.t     = bestT;
	for (int i = 0; i < 3; ++i)
		hit.point[i] = o[i] + bestT * d[i];
	return hit;
}

// POSER LE PIVOT SUR LA SURFACE SOUS UN PIXEL. Point d'entree unique du geste
// Alt + clic gauche et de `camera pivot pick` : les deux exercent la meme loi,
// et non deux copies qui derivent.
//
// La distance oeil-pivot n'est pas touchee -- choisir autour de quoi on tourne
// n'est pas choisir de combien on recule. Et un rayon qui ne touche rien laisse
// le pivot EN PLACE : un clic manque ne doit pas faire sauter la vue.
MyGLCanvas::PivotPick MyGLCanvas::SetPivotFromPixel (float pixelX, float pixelY)
{
	PivotPick out;
	if (!m_pTrackball)
		return out;                       // NoView

	GetClientSize (&out.viewportWidth, &out.viewportHeight);
	if (out.viewportWidth <= 0 || out.viewportHeight <= 0)
		return out;                       // NoView

	// Bornes INCLUSIVES : NdcFromPixel envoie 0 sur -1 et w sur +1, donc le bord
	// droit du viewport est le bord du frustum et reste un pixel legitime.
	if (pixelX < 0.f || pixelX > (float)out.viewportWidth ||
	    pixelY < 0.f || pixelY > (float)out.viewportHeight)
	{
		out.status = PivotPickStatus::OutOfViewport;
		return out;                       // pivot INCHANGE
	}

	const PickHit hit = PickModel (pixelX, pixelY);
	if (!hit.model)
	{
		out.status = PivotPickStatus::NoHit;
		return out;                       // pivot INCHANGE
	}

	if (!SetCameraPivot (hit.point[0], hit.point[1], hit.point[2]))
		return out;                       // NoView

	out.status = PivotPickStatus::Ok;
	out.model  = hit.model;
	out.t      = hit.t;
	for (int i = 0; i < 3; ++i)
		out.point[i] = hit.point[i];
	return out;
}

// Instrument de l'aller-retour : meme rayon que le survol, rendu au script.
MyGLCanvas::PickedRay MyGLCanvas::UnprojectPixel (float pixelX, float pixelY)
{
	PickedRay out;
	GetClientSize (&out.viewportWidth, &out.viewportHeight);
	if (!ScreenToRay (pixelX, pixelY, out.origin, out.direction))
		return out;                       // valid reste false
	out.valid = true;
	out.model = PickModel (pixelX, pixelY).model;
	return out;
}

//
//
//
void MyGLCanvas::LoadModel(const wxString& filename, const ImportSettings& settings)
{
#ifdef LINUX
	locale_t loc;
	loc = newlocale(LC_NUMERIC, "C", nullptr);
	uselocale(loc);
#endif // LINUX

	//cout << ((filename).mb_str(wxConvUTF8)) << endl;
	//printf ("%s\n", (char*) ((filename).mb_str(wxConvUTF8)).data());
	auto meshes = new VMeshes();
	std::string loaded_filename = filename.ToUTF8().data();
	VMeshesIO::load(*meshes, const_cast<char*>(loaded_filename.c_str()));

	wxString msg = wxString::Format(wxT("%zu meshes imported"), meshes->GetNMeshes());
	*m_CtrlLog << msg << _T("\n");

	// Echo the import options so the effect of each operation is traceable.
	*m_CtrlLog << wxString::Format(
		_T("Import options: Normalisation=%s, Triangulate=%s, Merge vertices=%s\n"),
		settings.normalize     ? _T("on") : _T("off"),
		settings.triangulate   ? _T("on") : _T("off"),
		settings.mergeVertices ? _T("on") : _T("off"));

	// Normalise (if enabled), compute normals, frame the camera and show the
	// model. Import operations then run on the resulting mesh — the same state
	// the Treatments menu operates on — so Merge vertices is effective at unit
	// scale rather than a no-op on raw (large) coordinates.
	SetVMeshes(meshes, settings.normalize);   // adopte les maillages de `meshes` et le détruit

	// Nomme le Model actif d'après le fichier (affiché dans l'arbre d'info).
	if (VModels* scene = GetVModels())
		if (scene->GetNModels() > 0)
		{
			scene->GetModels()[0]->m_name = std::string(wxFileName(filename).GetFullName().ToUTF8().data());
			scene->GetModels()[0]->m_path = std::string(filename.ToUTF8().data());
		}

	if (settings.triangulate || settings.mergeVertices)
	{
		for (auto& pMesh : GetVMeshes()->GetMeshes())   // `meshes` a été consommé -> fichier actif
		{
			if (settings.triangulate)
			{
				const auto nBefore = pMesh->GetNFaces();
				pMesh->Triangulate();
				*m_CtrlLog << wxString::Format(_T("  Triangulate: %u -> %u faces\n"),
				                               nBefore, pMesh->GetNFaces());
			}
			if (settings.mergeVertices)
			{
				const auto nBefore = pMesh->GetNVertices();
				pMesh->MergeVertices();
				*m_CtrlLog << wxString::Format(_T("  Merge vertices: %u -> %u vertices\n"),
				                               nBefore, pMesh->GetNVertices());
			}
			pMesh->ComputeNormals();
		}
		UpdateTopologicIssues();
		Refresh(false);
	}

/*
	m_listId.clear ();
	std::list<Mesh_half_edge*> list = m_pObject->GetMeshes();
	for (std::list<Mesh_half_edge*>::iterator it = list.begin (); it != list.end (); it++)
	{
		m_pMesh = (*it);
		m_pMesh->center ();
		m_pMesh->compute_normales ();
		//m_pMesh->init_colors (239.0/255.0, 235.0/255.0, 160.0/255.0);
		m_pMesh->init_colors (1., 1., 1.);
		//m_pMesh->init_colors (.3f, 0.4f, 0.6f);

		m_nId = m_pRenderingEngine->GetRenderer3DManager()->addMesh (m_pMesh, GR3D_VERTEX_BUFFER);
		m_listId.push_back (m_nId);
	}
*/
}

Model* MyGLCanvas::AppendModel(const wxString& filename)
{
	if (!m_pVModels)
		m_pVModels = new VModels();

	const std::string path = filename.ToUTF8().data();
	const std::string base = std::string(wxFileName(filename).GetFullName().ToUTF8().data());

	Model* mdl = m_pVModels->Add(base);
	mdl->m_path = path;
	if (!VMeshesIO::load(mdl->m_meshes, path.c_str()) || mdl->m_meshes.GetNMeshes() == 0)
	{
		m_pVModels->Remove(m_pVModels->GetNModels() - 1);   // retire le Model vide
		if (m_CtrlLog) *m_CtrlLog << wxString::Format(_T("Echec de chargement: %s\n"), filename);
		return nullptr;
	}

	for (auto* mesh : mdl->m_meshes.GetMeshes())
		if (mesh) mesh->ComputeNormals();

	mdl->BuildBVH();   // picking surface (géométrie statique après chargement)
	if (!m_selectedModel)   // 1er contenu d'un canvas vide -> sélection par défaut
		m_selectedModel = mdl;

	// PAS de normalisation : on garde le repère monde pour que les fichiers ajoutés
	// se superposent correctement (comparaison de reconstructions). On recadre juste
	// la caméra sur la bbox agrégée de la scène visible.
	m_pTrackball->ResetTransformations();
	FrameVisibleScene();

	// Nuage de points pur (sommets mais aucune face, ex. fused.ply) : le mode « fill »
	// ne dessine rien -> bascule en affichage points pour qu'il soit visible. On ne le
	// fait que si TOUTE la scène est sans faces (prop est global au canvas ; ne pas
	// forcer le mode points quand un maillage est déjà affiché).
	if (m_pVModels->GetNVertices() > 0 && m_pVModels->GetNFaces() == 0)
	{
		// Explicit line/point primitives render themselves (with their own
		// colours); only fall back to the all-vertices point overlay for a pure
		// vertex cloud, else it draws duplicate points that mask point_color.
		bool hasPrimitives = false;
		for (const auto& mdl : m_pVModels->GetModels())
		{
			for (const auto& m : mdl->m_meshes.GetMeshes())
				if (m && (m->GetNLines() > 0 || m->GetNPoints() > 0)) { hasPrimitives = true; break; }
			if (hasPrimitives) break;
		}
		prop.display_points = !hasPrimitives;
		prop.display_fill   = false;
	}

	UpdateTopologicIssues();
	Refresh(false);

	if (m_CtrlLog)
		*m_CtrlLog << wxString::Format(_T("Ajoute: %s (%zu maillages)\n"),
		                               filename, mdl->m_meshes.GetNMeshes());
	return mdl;
}

bool MyGLCanvas::ReloadModel(Model* mdl)
{
	if (!mdl || mdl->m_path.empty() || !m_pVModels)
		return false;

	const wxString path = wxString::FromUTF8(mdl->m_path.c_str());
	if (!wxFileName::FileExists(path))
	{
		if (m_CtrlLog)
			*m_CtrlLog << wxString::Format(_T("Rafraichissement impossible, fichier introuvable: %s\n"), path);
		return false;
	}

	// Détache les anciens maillages du renderer AVANT de les détruire (le
	// MeshRenderer indexe par Mesh* : un pointeur détruit resté indexé
	// provoquerait un accès invalide au prochain rendu).
	for (auto* mesh : mdl->m_meshes.GetMeshes())
		if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);

	// Le Model survolé peut être celui qu'on recharge.
	if (m_hoveredModel == mdl)
		ClearHoveredModel();

	// Cas à distinguer : seul dans la scène -> on recadre comme à un chargement ;
	// superposé à d'autres -> on préserve la vue pour ne pas perturber les autres.
	const bool sole = (m_pVModels->GetNModels() == 1);

	// Recharge en place depuis le fichier d'origine.
	mdl->m_meshes.clean();
	if (!VMeshesIO::load(mdl->m_meshes, mdl->m_path.c_str()) || mdl->m_meshes.GetNMeshes() == 0)
	{
		if (m_CtrlLog)
			*m_CtrlLog << wxString::Format(_T("Echec du rafraichissement: %s\n"), path);
		return false;   // le Model est désormais vide
	}

	for (auto* mesh : mdl->m_meshes.GetMeshes())
		if (mesh) mesh->ComputeNormals();
	mdl->BuildBVH();     // picking surface (géométrie statique après rechargement)
	mdl->ComputeBBox();

	if (sole)
	{
		m_pTrackball->ResetTransformations();
		FrameVisibleScene();
	}

	UpdateTopologicIssues();
	Refresh(false);

	if (m_CtrlLog)
		*m_CtrlLog << wxString::Format(_T("Rafraichi: %s (%zu maillages)\n"),
		                               path, mdl->m_meshes.GetNMeshes());
	return true;
}

void MyGLCanvas::SaveModel(const wxString& filename)
{
#ifdef LINUX
	locale_t loc;
	loc = newlocale(LC_NUMERIC, "C", nullptr);
	uselocale(loc);
#endif // LINUX

	//cout << ((filename).mb_str(wxConvUTF8)) << endl;
	//printf ("%s\n", (char*) ((filename).mb_str(wxConvUTF8)).data());

	if (VMeshes* vm = GetVMeshes())
		VMeshesIO::save(*vm, (char*)((filename).mb_str(wxConvUTF8)).data());
}

//
//
//
void MyGLCanvas::DrawGL()
{

    // Initialize OpenGL
    if (!m_bInitialized)
    {
        InitGL();
        ResetProjectionMode();
        m_bInitialized = true;
    }

    // Clear
    //glClearColor( 0.3f, 0.4f, 0.6f, 1.0f );
    glClearColor( m_fBackgroundColor[0], m_fBackgroundColor[1], m_fBackgroundColor[2], 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // Transformations
    glLoadIdentity();

    //glScalef (0.01f, 0.01f, 0.01f);
	glEnable (GL_NORMALIZE);

	m_pTrackball->set_camera ();

	if (prop.light)
		glEnable (GL_LIGHTING);
	else
		glDisable (GL_LIGHTING);

	if (m_bSmooth)
		glShadeModel (GL_SMOOTH);
	else
		glShadeModel (GL_FLAT);
	prop.smooth = m_bSmooth;

	if (prop.display_repere)
		repere_draw ();
	if (prop.display_grid)
	{
		// Derivee du cadrage courant et du pivot VIVANT : `camera pivot` deplace
		// la grille sans recadrer. Cout O(1) -- aucun parcours de la scene, donc
		// aucun AggregateBBox par image.
		const TVector3<float>& pivot = m_pTrackball->camera ().GetPivot ();
		const GridLayout g = grid_layout (m_framingRadius, pivot.x, pivot.y);
		draw_grid (g.size, g.steps, g.centerX, g.centerY);
	}
	// AVANT les modeles : le tapis est un fond. Le premier appel lit l'asset,
	// d'ou l'appel ici et non au chargement d'un modele -- le televersement des
	// textures exige un contexte GL courant, ce qui n'est garanti qu'a la peinture.
	if (prop.display_cutting_mat)
	{
		UpdateCuttingMatLevel();
		m_cuttingMat.Draw(prop.light != 0, m_cuttingMatZ);
	}
	// Parcourt les fichiers (Model) VISIBLES, puis leurs maillages. Le renderer
	// indexe par Mesh* (GetMeshId), donc rien d'autre à gérer côté ids.
	if (m_pVModels)
	for (const auto& mdl : m_pVModels->GetModels())
	{
		if (!mdl->m_visible) continue;
		for (const auto& mesh : mdl->m_meshes.GetMeshes())
		{
			// CG_RENDERING_VBO: server-side buffers (positions, normals, UVs,
			// colors, indices) bound once and re-drawn from VRAM. Avoids the
			// per-frame CPU->GPU push of CG_RENDERING_VERTEX_ARRAY. VBOManager
			// re-uploads automatically when Mesh::GetRevision() changes, so
			// RemoteConsole edits like `flip` stay visible.
			// The overlays (wireframe, vertex normals, points) are still drawn
			// by mesh_draw inside MeshRenderer::Draw.
			int id = MeshRenderer::getInstance()->GetMeshId(mesh, CG_RENDERING_VBO);

			// Update rendering properties from the canvas prop
			MeshRenderer::getInstance()->SetProperties(id, prop);

			MeshRenderer::getInstance()->Draw(id);
		}
	}

	// Surbrillance du Model survolé : arêtes de son AABB en jaune (12 arêtes).
	if (m_hoveredModel && m_hoveredModel->m_visible)
	{
		m_hoveredModel->ComputeBBox();
		if (!m_hoveredModel->m_bbox.IsEmpty())
		{
			float mn[3], mx[3];
			m_hoveredModel->m_bbox.GetMinMax(mn, mx);
			// Sauve l'état GL : sinon glColor(jaune)/glLineWidth « fuient » vers les
			// frames suivantes et le mesh (rendu sans matériau) apparaît jaune.
			glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT);
			glDisable(GL_LIGHTING);
			// Sur un modèle texturé, la texture encore liée modulerait la couleur des
			// arêtes (-> jaune assombri/teinté) ; le blending la mélangerait au fond.
			// On les désactive pour un jaune pur et constant quel que soit le modèle.
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
			glColor3f(1.0f, 1.0f, 0.0f);
			glLineWidth(2.0f);
			const float x0=mn[0], y0=mn[1], z0=mn[2], x1=mx[0], y1=mx[1], z1=mx[2];
			glBegin(GL_LINES);
			// bas
			glVertex3f(x0,y0,z0); glVertex3f(x1,y0,z0);
			glVertex3f(x1,y0,z0); glVertex3f(x1,y1,z0);
			glVertex3f(x1,y1,z0); glVertex3f(x0,y1,z0);
			glVertex3f(x0,y1,z0); glVertex3f(x0,y0,z0);
			// haut
			glVertex3f(x0,y0,z1); glVertex3f(x1,y0,z1);
			glVertex3f(x1,y0,z1); glVertex3f(x1,y1,z1);
			glVertex3f(x1,y1,z1); glVertex3f(x0,y1,z1);
			glVertex3f(x0,y1,z1); glVertex3f(x0,y0,z1);
			// montants
			glVertex3f(x0,y0,z0); glVertex3f(x0,y0,z1);
			glVertex3f(x1,y0,z0); glVertex3f(x1,y0,z1);
			glVertex3f(x1,y1,z0); glVertex3f(x1,y1,z1);
			glVertex3f(x0,y1,z0); glVertex3f(x0,y1,z1);
			glEnd();
			glPopAttrib();   // restaure couleur / épaisseur / lighting
		}
	}
/*
	else
	{
		if (m_bWireframe)
			glutWireTeapot (0.5);
		else
			glutSolidTeapot (0.5);
	}
*/
/*
	if (m_pVMeshes)
	{
		// draw model
		list<int>::iterator itId;
		for (itId = m_listId.begin (); itId != m_listId.end (); itId++)
		{
			m_nId = (*itId);
			m_pRenderingEngine->GetRenderer3DManager()->Draw (m_nId);
		}

		// test
		if(0)
		{
		float x, y, z;
		m_pTrackball->getCameraPosition (&x, &y, &z);
		Vector3f vPos (x, y, z);
		vPos.Normalize ();

		std::list<Mesh_half_edge*> list = m_pObject->GetMeshes();
		for (std::list<Mesh_half_edge*>::iterator it = list.begin (); it != list.end (); it++)
		{
			Mesh_half_edge *pMesh = (*it);

			float *pVertices = pMesh->get_vertices ();
			int nVertices = pMesh->get_n_vertices ();

			glBegin (GL_LINES);
			for (int i=0; i<nVertices; i++)
			{
				glVertex3f (vPos.x, vPos.y, vPos.z);
				glVertex3f (pVertices[3*i], pVertices[3*i+1], pVertices[3*i+2]);
			}
			glEnd ();
		}
		}


		// draw segments
		if (m_pNPRManager)
		{
			glPushAttrib (GL_ALL_ATTRIB_BITS);
			glEnable (GL_POLYGON_OFFSET_FILL); 
			glPolygonOffset (1., 2.);
			glEnable (GL_LINE_SMOOTH);
			
			//glEnable (GL_BLEND);
			//glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			//glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
			//glDisable (GL_LIGHTING);
			
			glDisableClientState (GL_COLOR_ARRAY);
			glLineWidth (2.f);
			glColor3f (0.f, 0.f, 0.f);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			glBegin (GL_LINES);

			m_listSegments = m_pNPRManager->GetSegments (NPR_SEGMENT_ANGLE);
			if (m_listSegments.size() != 0)
			{

				float *v = m_pMesh->get_vertices ();
				for (ListNPRSegmentsIt it=m_listSegments.begin(); it!=m_listSegments.end(); it++)
				{
					NPRSegment seg = (*it);
					if (seg.fData > m_fNPRAngleThreshold)
						break;
					Vector3 e1 = seg.e1;
					Vector3 e2 = seg.e2;
					glVertex3f (e1[0], e1[1], e1[2]);
					glVertex3f (e2[0], e2[1], e2[2]);
				}
			}

			m_listSegments = m_pNPRManager->GetSegments (NPR_SEGMENT_SILHOUETTE);
			if (m_listSegments.size() != 0)
			{
				glColor3f (0.f, 0.f, 0.f);

				float *v = m_pMesh->get_vertices ();
				for (ListNPRSegmentsIt it=m_listSegments.begin(); it!=m_listSegments.end(); it++)
				{
					NPRSegment seg = (*it);

					Vector3 e1 = seg.e1;
					Vector3 e2 = seg.e2;
					glVertex3f (e1[0], e1[1], e1[2]);
					glVertex3f (e2[0], e2[1], e2[2]);
				}

			}

			glEnd ();
			glPopAttrib ();
		}
	}
	else
	{
		if (m_bWireframe)
			glutWireTeapot (0.5);
		else
			glutSolidTeapot (0.5);
	}
*/

    glFlush();
    wxGLCanvas::SwapBuffers();
}

void MyGLCanvas::OnPaint(wxPaintEvent& WXUNUSED(event))
{
	wxGLCanvas::SetCurrent(*m_context);
	wxPaintDC dc(this);

	ResetProjectionMode();
	DrawGL();

	// FPS overlay: count frames on natural paint events (mouse drag, resize,
	// programmatic Refresh, etc.) and push to the status bar every ~250 ms.
	// We deliberately do NOT call Refresh() from here to avoid a self-feeding
	// repaint loop that would starve modal dialogs and pump events forever.
	// If you need a steady measurement, hold the left mouse button and drag.
	MyFrame* frame = dynamic_cast<MyFrame*>(wxGetTopLevelParent(this));
	if (frame && frame->IsShowFps())
	{
		++m_fpsFrames;
		const auto now = std::chrono::steady_clock::now();
		const long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - m_fpsLastUpdate).count();
		if (elapsedMs >= 250)
		{
			const double fps = (m_fpsFrames * 1000.0) / static_cast<double>(elapsedMs);
			if (wxStatusBar* sb = frame->GetStatusBar())
				sb->SetStatusText(wxString::Format(wxT("FPS: %.0f"), fps), 1);
			m_fpsFrames = 0;
			m_fpsLastUpdate = now;
		}
	}
}

bool MyGLCanvas::SaveScreenshot(const wxString& path)
{
	if (!m_context) return false;
	wxGLCanvas::SetCurrent(*m_context);

	int w = 0, h = 0;
	GetClientSize(&w, &h);
	if (w <= 0 || h <= 0) return false;

	// ON REDESSINE, puis on lit le tampon ARRIERE -- et non le tampon avant.
	//
	// Lire GL_FRONT rend le contenu de la FENETRE telle qu'elle est a l'ecran :
	// occultee par une autre fenetre, ou simplement pas encore repeinte, elle
	// donne une image noire, sans erreur ni indice. C'est exactement le piege
	// pour l'usage principal de cette fonction -- la console distante, qui pilote
	// sinaia depuis un outil externe et n'a aucune raison d'avoir la fenetre au
	// premier plan. Un rendu a la demande dans le tampon arriere, jamais presente,
	// ne depend plus de ce qui recouvre la fenetre.
	ResetProjectionMode();
	DrawGL();
	glFinish();

	std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// glReadPixels returns rows bottom-up; PNG wants top-down — flip Y.
	std::vector<unsigned char> flipped(static_cast<size_t>(w) * h * 3);
	for (int y = 0; y < h; ++y)
	{
		std::memcpy(&flipped[static_cast<size_t>(h - 1 - y) * w * 3],
		            &pixels [static_cast<size_t>(y)         * w * 3],
		            static_cast<size_t>(w) * 3);
	}

	// wxImage with static_data=true keeps ownership in our vector; SaveFile
	// reads from it while the vector is still in scope. wxPNGHandler is
	// registered by wxInitAllImageHandlers() in SinaiaApp::OnInit.
	wxImage img(w, h, flipped.data(), true);
	return img.SaveFile(path, wxBITMAP_TYPE_PNG);
}

void MyGLCanvas::OnSize(wxSizeEvent& event)
{
    if (IsShownOnScreen()) {
        wxGLCanvas::SetCurrent(*m_context);
        ResetProjectionMode();
        Refresh(false);
    }
}

void MyGLCanvas::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // Do nothing, to avoid flashing on MSW
}

//
//
//
void MyGLCanvas::OnMouse(wxMouseEvent& event)
{
	if (event.ButtonDown())
	{
		SetFocus();
		// Début d'une interaction caméra (rotation/zoom) : on annule la surbrillance
		// de survol, sinon la bbox jaune resterait affichée pendant tout le drag.
		if (m_hoveredModel)
		{
			m_hoveredModel = nullptr;
			if (auto* frame = dynamic_cast<MyFrame*>(wxGetTopLevelParent(this)))
				frame->HighlightModelRow(-1);
			Refresh(false);
		}
	}

	if (event.GetEventType() == wxEVT_MOUSEWHEEL)
	{
		// EVT_MOUSE_EVENTS capte aussi la molette : sans branche dediee
		// l'evenement serait consomme et perdu.
		const int delta = event.GetWheelDelta();
		if (m_pTrackball && delta != 0 && event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
		{
			// Une molette haute resolution envoie des fractions d'encoche, et un
			// pilote exotique peut en envoyer beaucoup d'un coup : on borne le
			// nombre d'encoches par evenement.
			float notches = (float)event.GetWheelRotation() / (float)delta;
			if (notches >  5.f) notches =  5.f;
			if (notches < -5.f) notches = -5.f;
			m_pTrackball->zoom_step(notches);
			Refresh(false);
		}
		return;
	}

	if (event.LeftDown())
	{
		// ALT + CLIC GAUCHE : poser le pivot sur le point de surface sous le
		// curseur (convention de Blender et de Fusion), sans toucher a la
		// distance.
		//
		// Le trackball n'est PAS engage. Appeler mouse_press ici amorcerait une
		// rotation : le moindre tremblement entre l'enfoncement et le
		// relachement ferait tourner la vue au moment meme ou l'utilisateur
		// vise. Le drapeau tient jusqu'au relachement, pour que ni le glisser ni
		// le LeftUp ne retombent dans les branches de rotation et de selection.
		if (event.AltDown())
		{
			m_altPivotGesture = true;
			const PivotPick r = SetPivotFromPixel((float)event.GetX(), (float)event.GetY());
			if (r.status == PivotPickStatus::NoHit && m_CtrlLog)
				*m_CtrlLog << _T("Alt+clic : aucune surface sous le curseur, pivot inchange.\n");
			return;
		}

		m_pressX = event.GetX(); m_pressY = event.GetY();   // pour distinguer clic / glisser
		m_pTrackball->mouse_press (LEFT_BUTTON, PRESSED, event.GetX(), event.GetY());
	}
	else if (event.LeftUp())
	{
		// Fin d'un Alt + clic : le trackball n'a jamais ete engage, et le geste
		// ne selectionne pas non plus -- il ne change que le pivot.
		if (m_altPivotGesture)
		{
			m_altPivotGesture = false;
			return;
		}

		m_pTrackball->mouse_press (LEFT_BUTTON, RELEASED, event.GetX(), event.GetY());

		// Clic (déplacement négligeable depuis l'enfoncement) sur un modèle -> le
		// sélectionner ; un glisser (rotation de la vue) ne sélectionne pas.
		const int dx = event.GetX() - m_pressX, dy = event.GetY() - m_pressY;
		if (dx*dx + dy*dy <= 9)   // seuil 3 px
		{
			Model* hit = PickModel(event.GetX(), event.GetY()).model;
			if (hit && hit != m_selectedModel)
			{
				m_selectedModel = hit;
				if (auto* frame = dynamic_cast<MyFrame*>(wxGetTopLevelParent(this)))
					frame->RefreshModelSelection();
			}
		}
	}
	else if (event.RightDown())
	{
		m_pTrackball->mouse_press (RIGHT_BUTTON, PRESSED, event.GetX(), event.GetY());
	}
	else if (event.RightUp())
	{
		m_pTrackball->mouse_press (RIGHT_BUTTON, RELEASED, event.GetX(), event.GetY());
	}
	else if (event.MiddleDown())
	{
		m_pTrackball->mouse_press (MIDDLE_BUTTON, PRESSED, event.GetX(), event.GetY());
	}
	else if (event.MiddleUp())
	{
		m_pTrackball->mouse_press (MIDDLE_BUTTON, RELEASED, event.GetX(), event.GetY());
	}
	else if (event.Dragging())
	{
		if (m_altPivotGesture)
			return;   // Alt + clic gauche vise, il ne fait pas tourner la vue

		wxSize sz(GetClientSize());
		m_pTrackball->mouse_move (event.GetX(), event.GetY());

		// orientation has changed, redraw mesh
		Refresh(false);
	}
	else if (event.Moving())   // déplacement SANS bouton -> survol (picking fichier)
	{
		Model* hit = PickModel(event.GetX(), event.GetY()).model;
		if (hit != m_hoveredModel)
		{
			m_hoveredModel = hit;
			// Synchronise la surbrillance de la ligne correspondante dans "Models".
			if (auto* frame = dynamic_cast<MyFrame*>(wxGetTopLevelParent(this)))
			{
				int idx = -1;
				if (hit && m_pVModels)
				{
					int k = 0;
					for (auto& mdl : m_pVModels->GetModels())
					{ if (mdl.get() == hit) { idx = k; break; } ++k; }
				}
				frame->HighlightModelRow(idx);
			}
			Refresh(false);
		}
	}
}

void MyGLCanvas::OnKeyDown(wxKeyEvent& event)
{
	// Le pas du plan de coupe se cale sur l'ECHELLE DU SUJET CADRE : clipping_plane_z
	// part dans l'equation d'un GL_CLIP_PLANE en unites MONDE, donc un pas fixe est
	// soit imperceptible, soit traversant, selon la taille du modele charge.
	//
	// Ratio : 1/(10*racine(3)). Un appui vaut 1/20 de la plus grande dimension d'un
	// sujet cubique -- pour lequel le rayon de cadrage, demi-diagonale, vaut
	// racine(3)/2 fois cette dimension. Soit une vingtaine d'appuis pour traverser
	// le sujet, quelle que soit son echelle.
	//
	// Le repli couvre le cas ou aucun cadrage n'a encore ete fait : sans rayon de
	// cadrage, il n'y a aucune echelle a lire, et un pas nul bloquerait la touche.
	constexpr float kClipStepRatio = 0.0577350f;
	const float step = (m_framingRadius > 0.f) ? m_framingRadius * kClipStepRatio : 0.05f;

	switch (event.GetKeyCode())
	{
	case WXK_UP:
	case WXK_DOWN:
		if (!prop.clipping_plane_active)
		{
			if (m_CtrlLog) *m_CtrlLog << _T("Warning: Clipping plane is not active. Check the box in Rendering panel.\n");
			break;
		}
		if (event.GetKeyCode() == WXK_UP)
			prop.clipping_plane_z += step;
		else
			prop.clipping_plane_z -= step;

		if (m_CtrlLog) *m_CtrlLog << wxString::Format(wxT("Clipping Z: %.2f\n"), prop.clipping_plane_z);
		Refresh(false);
		Update();
		break;

	// F comme « frame » : recentrer sur le modele SELECTIONNE. Sans selection on
	// ne cadre PAS la scene entiere -- ce serait rendre la touche inoffensive au
	// lieu de dire qu'il n'y a rien a viser, et l'utilisateur croirait avoir vise
	// un modele.
	//
	// wx rend les codes de lettre en MAJUSCULE dans un evenement KEY_DOWN, quel
	// que soit l'etat de la touche majuscule : un case 'f' ne se declencherait
	// jamais.
	case 'F':
		if (!FrameSelectedModel())
		{
			if (m_CtrlLog) *m_CtrlLog << _T("Aucun modele selectionne : rien a recentrer.\n");
		}
		break;

	default:
		event.Skip();
		break;
	}
}

//
//
//
void MyGLCanvas::InitGL()
{
    static const GLfloat light0_pos[4]   = { -50.0f, 50.0f, 0.0f, 0.0f };

    // white light
    static const GLfloat light0_color[4] = { 0.6f, 0.6f, 0.6f, 1.0f };

    static const GLfloat light1_pos[4]   = {  50.0f, 50.0f, 0.0f, 0.0f };

    // cold blue light
    //static const GLfloat light1_color[4] = { 0.4f, 0.4f, 1.0f, 1.0f };
	static const GLfloat light1_color[4] = { 0.4f, 0.4f, 0.4f, 1.0f };

    // remove back faces
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // speedups
    glEnable(GL_DITHER);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);

/*
    // light
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light0_color);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1_color);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHTING);
*/

	float white[4] = {1.0, 1.0, 1.0, 1.0};
	float black[4] = {0.0, 0.0, 0.0, 1.0};

	// Lighting. A single full-intensity headlight plus the default global
	// ambient (0.2) flattens the model: lit faces clip toward white and
	// recesses stay bright, washing out the texture relief. Instead use a
	// low global ambient, a slightly-under-full key light from the upper
	// front-left, and a dim fill from the opposite side. Both are directional
	// (w=0) so the shading is independent of this very large model's scale and
	// off-origin position.
	GLfloat globalAmbient[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
	glLightModelfv (GL_LIGHT_MODEL_AMBIENT, globalAmbient);

	// Two-sided lighting: light back faces with a flipped normal. Imported
	// meshes (e.g. OBJ from 3D Warehouse) often have inconsistent face winding,
	// which leaves some faces with inward-pointing normals; without this they
	// render near-black ("black reflections" that move with the view).
	glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	glEnable (GL_LIGHTING);

	// Key light
	GLfloat keyDiffuse[4]  = { 0.75f, 0.75f, 0.75f, 1.0f };
	GLfloat keyDir[4]      = { -0.4f, 0.6f, 1.0f, 0.0f };
	glEnable (GL_LIGHT0);
	glLightfv (GL_LIGHT0, GL_AMBIENT,  black);
	glLightfv (GL_LIGHT0, GL_DIFFUSE,  keyDiffuse);
	glLightfv (GL_LIGHT0, GL_SPECULAR, white);
	glLightfv (GL_LIGHT0, GL_POSITION, keyDir);

	// Fill light: dimmer, opposite side, no specular — lifts shadows just
	// enough to keep them readable without flattening the relief.
	GLfloat fillDiffuse[4] = { 0.28f, 0.28f, 0.30f, 1.0f };
	GLfloat fillDir[4]     = { 0.7f, 0.25f, 0.4f, 0.0f };
	glEnable (GL_LIGHT1);
	glLightfv (GL_LIGHT1, GL_AMBIENT,  black);
	glLightfv (GL_LIGHT1, GL_DIFFUSE,  fillDiffuse);
	glLightfv (GL_LIGHT1, GL_SPECULAR, black);
	glLightfv (GL_LIGHT1, GL_POSITION, fillDir);

	// Materiau par defaut. Les valeurs ne sont plus ici : elles vivent dans
	// MaterialRenderer, qui les repose a chaque maillage sans materiau. Les
	// laisser en dur des deux cotes les aurait fait diverger au premier
	// ajustement.
	MaterialRenderer::ActivateDefaultMaterial ();

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
}

void MyGLCanvas::ResetProjectionMode()
{
    int w, h;
    GetClientSize(&w, &h);
	m_fWindowWidth = w;
	m_fWindowHeight = h;

#ifndef __WXMOTIF__
    //if ( GetContext() )
#endif
    {
        //SetCurrent(*m_context);
        glViewport(0, 0, (GLsizei) m_fWindowWidth, (GLsizei) m_fWindowHeight);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
		//Matrix4f mat;
		//mat.SetPerspective(m_fFovy, m_fWindowWidth / m_fWindowHeight, m_fNear, m_fFar);
		//glMultMatrixf((GLfloat*)mat.m_Mat);
        // Une fenetre reduite rapporte une hauteur nulle : sans garde, le ratio
        // vaut NaN et empoisonne la matrice de projection.
        const float aspect = (m_fWindowHeight > 0.f)? m_fWindowWidth/m_fWindowHeight : 1.f;
        gluPerspective(m_fFovy, aspect, m_fNear, m_fFar);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

		m_pTrackball->set_dimensions (m_fWindowWidth, m_fWindowHeight);
    }
}

void MyGLCanvas::SetBackgroundColor (unsigned char r, unsigned char g, unsigned char b)
{
	m_fBackgroundColor[0] = r/255.;
	m_fBackgroundColor[1] = g/255.;
	m_fBackgroundColor[2] = b/255.;
}

void MyGLCanvas::GetBackgroundColor (unsigned char *r, unsigned char *g, unsigned char *b)
{
	*r = (unsigned char)(m_fBackgroundColor[0] * 255.);
	*g = (unsigned char)(m_fBackgroundColor[1] * 255.);
	*b = (unsigned char)(m_fBackgroundColor[2] * 255.);
}

void MyGLCanvas::ChangeFill (void)
{
	prop.display_fill = !prop.display_fill;
/*
	for (list<int>::iterator itId = m_listId.begin (); itId != m_listId.end (); itId++)
	{
		m_nId = (*itId);
		if (m_nId != -1)
			m_pRenderingEngine->GetRenderer3DManager()->SetFill (m_nId, m_bFill);
	}
*/
	Refresh(false);
}

bool MyGLCanvas::GetFill (void)
{
	return prop.display_fill;
}

void MyGLCanvas::ChangeWireframe (void)
{
	prop.display_wireframe = !prop.display_wireframe;
/*
	for (list<int>::iterator itId = m_listId.begin (); itId != m_listId.end (); itId++)
	{
		m_nId = (*itId);
		if (m_nId != -1)
			m_pRenderingEngine->GetRenderer3DManager()->SetWireframe (m_nId, m_bWireframe);
	}
*/
	Refresh(false);
}

bool MyGLCanvas::GetWireframe (void)
{
	return prop.display_wireframe;
}

void MyGLCanvas::ChangePoint (void)
{
	prop.display_points = !prop.display_points;
/*
	for (list<int>::iterator itId = m_listId.begin (); itId != m_listId.end (); itId++)
	{
		m_nId = (*itId);
		if (m_nId != -1)
			m_pRenderingEngine->GetRenderer3DManager()->SetPoint (m_nId, m_bPoint);
	}
*/
	Refresh(false);
}

bool MyGLCanvas::GetWarning (void)
{
	return prop.display_warning;
}


void MyGLCanvas::ChangeWarning(void)
{
	prop.display_warning = !prop.display_warning;
	Refresh(false);
}

bool MyGLCanvas::GetPoint(void)
{
	return prop.display_points;
}

void MyGLCanvas::ChangeRepere(void)
{
	prop.display_repere = !prop.display_repere;
	Refresh(false);
}

bool MyGLCanvas::GetRepere(void)
{
	return prop.display_repere;
}

void MyGLCanvas::ChangeGrid(void)
{
	prop.display_grid = !prop.display_grid;
	Refresh(false);
}

bool MyGLCanvas::GetGrid(void)
{
	return prop.display_grid;
}

float MyGLCanvas::MoveModelToZeroLevel(Model* mdl)
{
	if (!mdl) return 0.f;

	const BoundingBox& bb = mdl->ComputeBBox();
	if (bb.IsEmpty()) return 0.f;

	float mn[3], mx[3];
	bb.GetMinMax(mn, mx);
	const float dz = -mn[2];
	if (dz == 0.f) return 0.f;

	for (auto* mesh : mdl->m_meshes.GetMeshes())
		if (mesh) mesh->translate(0.f, 0.f, dz);   // incremente la revision -> VBO reteleverse

	mdl->ComputeBBox();
	// Le BVH de picking indexe les POSITIONS et suppose la geometrie statique
	// (cf. Model::BuildBVH) : sans reconstruction il designerait l'ancien
	// emplacement, et le survol repondrait a cote du modele.
	mdl->BuildBVH();

	Refresh(false);
	return dz;
}

void MyGLCanvas::ChangeCuttingMat(void)
{
	prop.display_cutting_mat = !prop.display_cutting_mat;
	UpdateSceneSpheres();   // le tapis entre ou sort de la plage de profondeur
	Refresh(false);
}

bool MyGLCanvas::GetCuttingMat(void)
{
	return prop.display_cutting_mat;
}

float MyGLCanvas::GetCuttingMatLevel(void)
{
	UpdateCuttingMatLevel();
	return m_cuttingMatZ;
}

void MyGLCanvas::ChangeBoundingBox (void)
{
	m_bBoundingBox = !m_bBoundingBox;
/*
	if (m_nId != -1)
		m_pRenderingEngine->GetRenderer3DManager()->SetBoundingBox (m_nId, m_bBoundingBox);
*/
	Refresh(false);
}

bool MyGLCanvas::GetBoundingBox (void)
{
	return m_bBoundingBox;
}

// Les compteurs interrogent le cache par revision du MeshDataManager plutot
// qu'une copie conservee dans prop. Ils sont appelés à l'affichage du panneau,
// pas par image : le parcours de la scène y est sans conséquence.
unsigned int MyGLCanvas::GetNNonManifoldEdges() const
{
	unsigned int n = 0;
	if (m_pVModels)
		for (const auto& mdl : m_pVModels->GetModels())
			for (const auto& mesh : mdl->m_meshes.GetMeshes())
				if (mesh)
					n += (unsigned int)MeshDataManager::GetInstance()
					         .GetTopologicIssues(mesh).nonManifoldEdges.size() / 2;
	return n;
}
unsigned int MyGLCanvas::GetNBorders() const
{
	unsigned int n = 0;
	if (m_pVModels)
		for (const auto& mdl : m_pVModels->GetModels())
			for (const auto& mesh : mdl->m_meshes.GetMeshes())
				if (mesh)
					n += (unsigned int)MeshDataManager::GetInstance()
					         .GetTopologicIssues(mesh).borders.size() / 2;
	return n;
}

// N'ALIMENTE PLUS prop. Cette fonction recopiait, pour CHAQUE maillage de la
// scène, ses arêtes non-manifold et ses bords dans deux std::map portées par
// rendering_properties_s — laquelle est copiée par valeur à chaque maillage et à
// chaque image. Le dessin devenait quadratique en nombre de maillages, au point
// de figer l'application dès qu'on ouvrait deux fichiers ordinaires ensemble.
//
// mesh_draw lit maintenant directement le cache pour le seul maillage qu'il
// dessine. Il reste utile de préchauffer ce cache ici : le calcul topologique
// d'un gros maillage prend du temps, autant le payer au chargement plutôt qu'à
// la première image où l'utilisateur coche « warning ».
void MyGLCanvas::UpdateTopologicIssues()
{
	if (m_pVModels)
	{
		for (const auto& mdl : m_pVModels->GetModels())
			for (const auto& mesh : mdl->m_meshes.GetMeshes())
				if (mesh)
					(void)MeshDataManager::GetInstance().GetTopologicIssues(mesh);
	}
}

//
// NPR
//
/*
void MyGLCanvas::NPRCompute (void)
{
	if (m_pNPRManager)
		delete m_pNPRManager;

	m_pNPRManager = new NPRManager ();
	if (m_pNPRManager == nullptr)
		return;

	//m_pNPRManager->SetMesh (m_pMesh);
	m_pNPRManager->SetObject3D (m_pObject);

	float x, y, z;
	m_pTrackball->getCameraPosition (&x, &y, &z);
	Vector3f vCameraPosition (x, y, z);
	m_pNPRManager->SetCameraPosition (vCameraPosition);
	m_pNPRManager->ComputeSegments ();
 
	Refresh ();
}

#include <common/WriterSVG.h>
void MyGLCanvas::ExportNPR (void)
{
	GLfloat m[16];
	m_pTrackball->get_inverse_matrix (m);
	Matrix4f mat (m);

	float n = m_fNear;
	float f = m_fFar;
	float t = m_fNear * tan (m_fFovy/2.f);
	float b = -t;
	float r = t * m_fWindowWidth / m_fWindowHeight;
	float l = -r;
	Matrix4f matPerspective (	2.*n/(r-l), 0,			(r+l)/(r-l),	0.,
					0.,			2.*n/(t-b),	(t+b)/(t-b),	0.,
					0.,			0.,			-(f+n)/(f-n),	-2.*f*n/(f-n),
					0.,			0.,			-1.,			0.);
	
	if (m_listSegments.size() == 0)
		return;


	float *v = m_pMesh->get_vertices ();

	WriterSVG writerSVG;
	bool bOK = writerSVG.InitFile("d:\/\/toto.svg");
	if (!bOK)
	{
		printf ("berk\n");
	}
	writerSVG.WriteHeader();

	std::list<point2D> listPoints;
	for (ListNPRSegmentsIt it=m_listSegments.begin(); it!=m_listSegments.end(); it++)
	{
		listPoints.clear ();

		NPRSegment seg = (*it);
		int a = seg.i1;
		int b = seg.i2;

		Vector4f vec4A, vec4B;
		vec4A.Set (v[3*a], v[3*a+1], v[3*a+2], 1.f);
		vec4B.Set (v[3*b], v[3*b+1], v[3*b+2], 1.f);

		vec4A = mat * vec4A;
		vec4B = mat * vec4B;

		
		vec4A = matPerspective * vec4A;
		vec4B = matPerspective * vec4B;

		point2D ptA, ptB;
		float fScale = 300.;

		ptA.x = 100+fScale*vec4A.x;
		ptA.y = 400 - (100+fScale*vec4A.y);
		ptB.x = 100+fScale*vec4B.x;
		ptB.y = 400 - (100+fScale*vec4B.y);

		//ptA.x = 100+fScale*v[3*a];
		//ptA.y = 100+fScale*v[3*a+1];
		//ptB.x = 100+fScale*v[3*b];
		//ptB.y = 100+fScale*v[3*b+1];

		listPoints.push_back(ptA);
		listPoints.push_back(ptB);

		writerSVG.WritePolyline ("Id1", listPoints);

		listPoints.clear ();
	}

	writerSVG.WriteFooter ();

}
*/
