#include "CuttingMat.h"

#include "../src/cgre/gl_wrapper.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/vmeshes_io.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <cmath>
#include <cstdio>

namespace {

// GEOMETRIE DU FICHIER, telle qu'ecrite dans base_de_coupe.obj et son script
// generateur. Ces valeurs ne decrivent pas un choix d'affichage : elles LISENT
// l'asset, et c'est pourquoi elles sont nommees plutot que fondues dans une
// translation opaque. Si le tapis est regenere avec d'autres cotes, ce sont ces
// six lignes qu'il faut mettre a jour, et rien d'autre.
constexpr float kPlateLength   = 450.f;   // mm, en X
constexpr float kPlateWidth    = 300.f;   // mm, en Y
constexpr float kPlateThickness =  3.f;   // mm, face utile en Z = +kPlateThickness
constexpr int   kGradX         = 42;      // graduations du quadrillage
constexpr int   kGradY         = 27;
constexpr float kGradPitch     = 10.f;    // mm entre deux graduations (1 cm)

// Coin du quadrillage : il est CENTRE sur la plaque (cf. base_coupe_texture.py).
constexpr float kGridOriginX = (kPlateLength - kGradX * kGradPitch) * .5f;   // 15 mm
constexpr float kGridOriginY = (kPlateWidth  - kGradY * kGradPitch) * .5f;   // 15 mm

// GRADUATION QUI RECOIT L'ORIGINE DU MONDE.
//
// En X, le centre de la plaque tombe sur la graduation 21 (entiere). En Y, avec
// 27 graduations — un compte IMPAIR —, il tombe sur la 13,5 : ENTRE deux traits.
// On retient donc la 13, ce qui rend la plaque asymetrique de 10 mm en Y et
// garde en echange l'origine sur une intersection du quadrillage. C'est ce
// compromis qui est voulu : 10 mm de decentrage ne se voient pas, une origine
// entre deux traits se lit faux.
constexpr int kOriginGradX = 21;
constexpr int kOriginGradY = 13;

// Translation cuite au chargement. Elle met l'origine du monde sur l'intersection
// (kOriginGradX, kOriginGradY) en X et Y, et la face utile en Z = 0 dans le repere
// LOCAL. La cote monde de cette face, elle, n'est pas cuite : Draw la recoit et
// l'applique par la matrice de vue -- cf. l'en-tete.
constexpr float kOffsetX = -(kGridOriginX + kOriginGradX * kGradPitch);   // -225 mm
constexpr float kOffsetY = -(kGridOriginY + kOriginGradY * kGradPitch);   // -145 mm
constexpr float kOffsetZ = -kPlateThickness;                             //   -3 mm

// Resolution du chemin de l'asset. Deux candidats, dans cet ordre :
//   1. a cote de l'executable, ou le POST_BUILD de CMake copie le dossier ;
//   2. dans l'arbre SOURCE, pour une execution depuis un IDE dont le repertoire
//      de sortie n'aurait pas recu la copie.
// Le .mtl et le .png sont references par un nom NU depuis le .obj, donc ils sont
// resolus par l'importeur relativement au dossier trouve ici.
wxString ResolveMatPath()
{
	const wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
	wxFileName candidate(exe.GetPath(), "base_de_coupe.obj");
	candidate.AppendDir("base_de_coupe");
	if (candidate.FileExists())
		return candidate.GetFullPath();

#ifdef SINAIA_ASSETS_DIR
	wxFileName source(SINAIA_ASSETS_DIR "/base_de_coupe/base_de_coupe.obj");
	if (source.FileExists())
		return source.GetFullPath();
#endif

	return wxString();
}

} // namespace

float CuttingMat::BoundingRadius(float z)
{
	// Repere local apres translation : x dans [-225, 225], y dans [-145, 155],
	// z dans [-3, 0] ; pose a la cote `z`, l'etendue verticale devient
	// [z - 3, z], dont le plus grand ecart a l'origine est |z| + 3.
	//
	// Pas de std::max ici : gl_wrapper tire windows.h, dont la macro `max` casse
	// l'appel qualifie. La lambda dit d'ailleurs mieux ce qu'on cherche -- la
	// plus grande distance a l'origine sur un axe, quel que soit le signe.
	const auto extent = [](float a, float b) {
		const float fa = std::fabs(a), fb = std::fabs(b);
		return (fa > fb) ? fa : fb;
	};
	const float ax = extent(kOffsetX, kOffsetX + kPlateLength);
	const float ay = extent(kOffsetY, kOffsetY + kPlateWidth);
	const float az = std::fabs(z) + kPlateThickness;
	return std::sqrt(ax * ax + ay * ay + az * az);
}

CuttingMat::~CuttingMat()
{
	// Le MeshRenderer indexe par Mesh* : un pointeur detruit reste indexe
	// provoquerait un acces invalide au prochain Draw d'un autre element.
	if (m_meshes)
	{
		for (auto* mesh : m_meshes->GetMeshes())
			if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);
		delete m_meshes;
	}
}

bool CuttingMat::Load()
{
	const wxString path = ResolveMatPath();
	if (path.IsEmpty())
		return false;

	auto* meshes = new VMeshes();
	const std::string utf8 = std::string(path.ToUTF8().data());
	if (!VMeshesIO::load(*meshes, utf8.c_str()) || meshes->GetNMeshes() == 0)
	{
		delete meshes;
		return false;
	}

	for (auto* mesh : meshes->GetMeshes())
	{
		if (!mesh) continue;
		mesh->translate(kOffsetX, kOffsetY, kOffsetZ);
		mesh->ComputeNormals();   // normales de FACE : le rendu du tapis est a plat
		mesh->computebbox();
	}

	m_meshes = meshes;
	return true;
}

void CuttingMat::Draw(bool light, float z)
{
	if (!m_loaded)
	{
		if (m_loadFailed)
			return;                 // echec deja constate : ne pas relire a chaque image
		m_loaded = Load();
		m_loadFailed = !m_loaded;
		if (!m_loaded)
		{
			// L'echec n'est PAS silencieux. Sans cette trace, une base de coupe
			// absente du repertoire de sortie donne un bouton qui ne fait rien --
			// et rien pour dire pourquoi.
			fprintf (stderr, "CuttingMat: base_de_coupe/base_de_coupe.obj introuvable ou illisible.\n");
			return;
		}
	}

	// PROPRIETES PROPRES AU TAPIS, et non celles du canvas : c'est tout l'objet
	// de la classe. Les materiaux sont imposes (sans eux, plus de texture donc
	// plus de quadrillage), le remplissage aussi, et les surcouches — fil de fer,
	// points, normales, avertissements topologiques — sont eteintes : elles
	// decriraient la boite du tapis, pas le modele qu'on observe.
	rendering_properties_s p;
	rendering_properties_init(p);
	p.shading                = CG_shading_mode::Materials;
	p.light                  = light ? 1 : 0;
	p.smooth                 = 0;        // cf. l'en-tete : normales de face
	p.display_fill           = 1;
	p.display_wireframe      = 0;
	p.display_points         = 0;
	p.display_vertex_normals = 0;
	p.display_face_normals   = 0;
	p.display_warning        = 0;
	p.normalized             = 0;
	// Le plan de coupe appartient a l'examen du MODELE. Le laisser mordre le
	// tapis retirerait la reference au moment ou l'on en a le plus besoin.
	p.clipping_plane_active  = 0;

	// glPushAttrib et non un retablissement a la main : ActivateMaterial allume
	// GL_TEXTURE_2D et lie une texture, et le modele dessine ENSUITE peut n'avoir
	// aucun materiau — il echantillonnerait alors le quadrillage.
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	// La face utile est posee EXACTEMENT au minimum Z du modele, ce qui est le but ;
	// le modele y est donc coplanaire par construction, et le test de profondeur
	// trancherait au hasard. On biaise la PROFONDEUR du tapis plutot que sa
	// geometrie : le plan de reference reste exact au flottant pres, et perd les
	// egalites.
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.f, 1.f);

	// La cote de pose passe par la MATRICE DE VUE, jamais par les sommets : le
	// niveau suit le modele a chaque changement de scene, et cuire ce Z ferait
	// reteleverser le VBO du tapis a chacun.
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(0.f, 0.f, z);

	for (auto* mesh : m_meshes->GetMeshes())
	{
		if (!mesh) continue;
		const int id = MeshRenderer::getInstance()->GetMeshId(mesh, CG_RENDERING_VBO);
		MeshRenderer::getInstance()->SetProperties(id, p);
		MeshRenderer::getInstance()->Draw(id);
	}

	glPopMatrix();
	glPopAttrib();
}
