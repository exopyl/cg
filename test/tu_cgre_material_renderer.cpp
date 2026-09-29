//
// Premier test de cgre.
//
// Le module n'en avait AUCUN : il n'est bati que sous ENABLE_SINAIA, donc ni la
// CI, ni ASan, ni coverage.sh ne le compilent, et la cible TU ne le liait pas.
// Ce fichier ouvre la voie -- il se neutralise la ou cgre n'existe pas (voir
// CG_HAS_CGRE dans test/CMakeLists.txt), et reste donc sans effet sur la CI
// Linux.
//
// POURQUOI MaterialRenderer, et pourquoi CES cas. Sa gestion du cycle de vie
// vient d'etre entierement reecrite : cinq tableaux paralleles de 256 entrees
// remplaces par un vecteur et une table de hachage, avec un RemoveMaterial qui
// n'existait pas. Les trois defauts corriges etaient tous SILENCIEUX --
// saturation a 256 rendant les maillages suivants en bleu par defaut sans un
// message, textures jamais liberees, deduplication sur une adresse recyclable.
// Une regression y serait tout aussi silencieuse : elle ne se verrait qu'a
// l'ecran, et tard.
//
// AUCUN CONTEXTE OPENGL n'est requis, et c'est ce qui rend ce test possible :
// AddMaterial n'appelle glGenTextures que pour un MATERIAL_TEXTURE, et
// RemoveMaterial ne supprime un objet de texture que s'il en existe un. Sur des
// MaterialColorExt, tout le chemin de registre est du C++ pur. C'est exactement
// la part de cgre qui est testable sans echafaudage graphique.
//

#ifdef CG_HAS_CGRE

#include <gtest/gtest.h>

#include "../src/cgre/material_renderer.h"
#include "../src/cgre/mesh_renderer.h"
#include "../src/cgre/phong_extras.h"
#include "../src/cgmesh/material_pbr.h"

#include <memory>
#include <set>
#include <vector>

namespace
{

class CgreMaterialRenderer : public ::testing::Test
{
protected:
	// Le registre est un SINGLETON : son etat survit d'un test a l'autre. Chaque
	// cas retire donc ce qu'il a ajoute, et aucun n'affirme une valeur ABSOLUE
	// d'identifiant -- seulement des relations entre identifiants obtenus dans le
	// meme cas. Sans cela les tests dependraient de leur ordre d'execution.
	void TearDown () override
	{
		for (auto& m : m_owned)
			MaterialRenderer::getInstance ()->RemoveMaterial (m.get ());
		m_owned.clear ();
	}

	// Le registre ne possede pas les materiaux : il n'en garde que l'adresse.
	// C'est le test qui les fait vivre.
	Material* MakeMaterial (const char* name)
	{
		auto* mat = new MaterialColorExt ();
		mat->SetName (name);
		m_owned.emplace_back (mat);
		return mat;
	}

	std::vector<std::unique_ptr<Material>> m_owned;
};

TEST_F (CgreMaterialRenderer, RefuseUnMateriauNul)
{
	EXPECT_EQ (-1, MaterialRenderer::getInstance ()->AddMaterial (nullptr));
}

TEST_F (CgreMaterialRenderer, Deduplique)
{
	Material* mat = MakeMaterial ("acier");

	const int first  = MaterialRenderer::getInstance ()->AddMaterial (mat);
	const int second = MaterialRenderer::getInstance ()->AddMaterial (mat);

	EXPECT_GE (first, 0);
	EXPECT_EQ (first, second) << "le meme materiau doit rendre le meme emplacement";
}

TEST_F (CgreMaterialRenderer, DistingueDeuxMateriaux)
{
	Material* a = MakeMaterial ("acier");
	Material* b = MakeMaterial ("cuir");

	const int idA = MaterialRenderer::getInstance ()->AddMaterial (a);
	const int idB = MaterialRenderer::getInstance ()->AddMaterial (b);

	EXPECT_GE (idA, 0);
	EXPECT_GE (idB, 0);
	EXPECT_NE (idA, idB);
}

// LE CAS QUI COMPTE : sans RemoveMaterial, le registre ne faisait que croitre.
TEST_F (CgreMaterialRenderer, LEmplacementLibereEstReutilise)
{
	Material* a = MakeMaterial ("acier");
	const int idA = MaterialRenderer::getInstance ()->AddMaterial (a);
	ASSERT_GE (idA, 0);

	MaterialRenderer::getInstance ()->RemoveMaterial (a);

	Material* b = MakeMaterial ("cuir");
	const int idB = MaterialRenderer::getInstance ()->AddMaterial (b);

	EXPECT_EQ (idA, idB) << "l'emplacement rendu doit servir au materiau suivant";
}

TEST_F (CgreMaterialRenderer, UnEmplacementLibereNActivePlusRien)
{
	Material* a = MakeMaterial ("acier");
	const int idA = MaterialRenderer::getInstance ()->AddMaterial (a);
	ASSERT_GE (idA, 0);

	MaterialRenderer::getInstance ()->RemoveMaterial (a);

	// GetGlInfo est ce que le shader interroge pour savoir s'il doit
	// echantillonner. Sur un emplacement libere il doit rendre « rien », et non
	// les objets de texture du materiau disparu.
	const MaterialRenderer::MaterialGlInfo info =
		MaterialRenderer::getInstance ()->GetGlInfo ((unsigned int)idA);
	EXPECT_FALSE (info.hasTexture);
	EXPECT_FALSE (info.hasReflection);
}

TEST_F (CgreMaterialRenderer, RetirerDeuxFoisEstSansEffet)
{
	Material* a = MakeMaterial ("acier");
	ASSERT_GE (MaterialRenderer::getInstance ()->AddMaterial (a), 0);

	MaterialRenderer::getInstance ()->RemoveMaterial (a);
	MaterialRenderer::getInstance ()->RemoveMaterial (a);   // ne doit rien casser

	// Et le materiau peut revenir : une scene rechargee reenregistre les siens.
	EXPECT_GE (MaterialRenderer::getInstance ()->AddMaterial (a), 0);
}

TEST_F (CgreMaterialRenderer, GetGlInfoHorsBornesEstSur)
{
	const MaterialRenderer::MaterialGlInfo info =
		MaterialRenderer::getInstance ()->GetGlInfo (100000u);
	EXPECT_FALSE (info.hasTexture);
	EXPECT_FALSE (info.hasReflection);
	EXPECT_FLOAT_EQ (0.f, info.reflAmount);
}

// LE PLAFOND DE 256, qui etait la vraie bombe : au-dela, AddMaterial rendait -1,
// GetMaterialRendererIds empilait ce -1, et TOUS les maillages ouverts ensuite
// etaient rendus en bleu par defaut -- sans un message. Ouvrir successivement
// 26 fichiers a 10 materiaux suffisait.
TEST_F (CgreMaterialRenderer, PlusDeLimiteA256)
{
	const int kCount = 300;
	std::set<int> ids;

	for (int i = 0; i < kCount; ++i)
	{
		Material* mat = MakeMaterial (("mat_" + std::to_string (i)).c_str ());
		const int id = MaterialRenderer::getInstance ()->AddMaterial (mat);
		ASSERT_GE (id, 0) << "materiau " << i << " refuse : le plafond est revenu";
		ids.insert (id);
	}

	EXPECT_EQ ((size_t)kCount, ids.size ()) << "les identifiants doivent etre distincts";
}

// ---------------------------------------------------------------------------
//  Mode « Materiaux sans PBR »
// ---------------------------------------------------------------------------

// LES INDICES SONT PUBLICS : le selecteur de sinaia prend la selection pour
// l'indice. Un mode insere au milieu decalerait silencieusement les autres.
TEST (CgreShadingMode, IndicesStables)
{
	EXPECT_EQ (0, (int)CG_shading_mode::Materials);
	EXPECT_EQ (1, (int)CG_shading_mode::Neutral);
	EXPECT_EQ (2, (int)CG_shading_mode::VertexColors);
	EXPECT_EQ (3, (int)CG_shading_mode::MaterialsNoPbr);
	EXPECT_EQ (4, kShadingModeCount);
}

TEST (CgreShadingMode, SeulMaterialsAutoriseLePbr)
{
	EXPECT_TRUE  (ShadingUsesMeshMaterials (CG_shading_mode::Materials));
	EXPECT_TRUE  (ShadingUsesMeshMaterials (CG_shading_mode::MaterialsNoPbr));
	EXPECT_FALSE (ShadingUsesMeshMaterials (CG_shading_mode::Neutral));
	EXPECT_FALSE (ShadingUsesMeshMaterials (CG_shading_mode::VertexColors));

	EXPECT_TRUE  (ShadingAllowsPbr (CG_shading_mode::Materials));
	EXPECT_FALSE (ShadingAllowsPbr (CG_shading_mode::MaterialsNoPbr));
	EXPECT_FALSE (ShadingAllowsPbr (CG_shading_mode::Neutral));
	EXPECT_FALSE (ShadingAllowsPbr (CG_shading_mode::VertexColors));
}

namespace
{
	// Un MaterialPbrInfo « televerse » sans contexte GL : les noms de texture
	// sont fictifs, MakePhongExtras ne fait que les tester.
	MaterialRenderer::MaterialPbrInfo PbrInfoWithMaps ()
	{
		MaterialRenderer::MaterialPbrInfo info;
		info.isPbr = true;
		for (std::size_t i = 0; i < static_cast<std::size_t> (cgpbr::MapSlot::count); ++i)
		{
			info.mapTex[i] = (GLuint)(10 + i);
			info.hasMap[i] = true;
		}
		info.factors.emissive[0] = 1.f;
		info.factors.emissive[1] = 0.5f;
		info.factors.emissive[2] = 0.f;
		return info;
	}
}

TEST (CgrePhongExtras, UnMateriauNonPbrNObtientRien)
{
	MaterialRenderer::MaterialPbrInfo info = PbrInfoWithMaps ();
	info.isPbr = false;
	const cgre::PhongExtras x = cgre::MakePhongExtras (info, true);
	EXPECT_FALSE (x.useEmissiveMap);
	EXPECT_FALSE (x.useOcclusionMap);
	EXPECT_FALSE (x.useNormalMap);
	EXPECT_FLOAT_EQ (0.f, x.emissive[0]);
	EXPECT_LT (x.alphaCutoff, 0.f);
}

// LA PERTE QUE CE MODE CORRIGE : la projection texturee (MaterialTexture) n'a
// pas de canal d'emission. Le facteur doit arriver quand meme, carte comprise.
TEST (CgrePhongExtras, EmissionConserveeAvecSaCarte)
{
	const cgre::PhongExtras x = cgre::MakePhongExtras (PbrInfoWithMaps (), true);
	EXPECT_FLOAT_EQ (1.f,  x.emissive[0]);
	EXPECT_FLOAT_EQ (0.5f, x.emissive[1]);
	EXPECT_FLOAT_EQ (0.f,  x.emissive[2]);
	EXPECT_TRUE (x.useEmissiveMap);
	EXPECT_TRUE (x.useOcclusionMap);
	EXPECT_FLOAT_EQ (1.f, x.occlusionStrength);
	EXPECT_TRUE (x.useNormalMap);
	EXPECT_LT (x.alphaCutoff, 0.f) << "opaque : aucune decoupe";
}

TEST (CgrePhongExtras, UnFacteurEmissifNulAnnuleLaCarte)
{
	MaterialRenderer::MaterialPbrInfo info = PbrInfoWithMaps ();
	info.factors.emissive[0] = info.factors.emissive[1] = info.factors.emissive[2] = 0.f;
	EXPECT_FALSE (cgre::MakePhongExtras (info, true).useEmissiveMap);
}

TEST (CgrePhongExtras, SansUvAucuneCarte)
{
	const cgre::PhongExtras x = cgre::MakePhongExtras (PbrInfoWithMaps (), false);
	EXPECT_FALSE (x.useEmissiveMap);
	EXPECT_FALSE (x.useOcclusionMap);
	EXPECT_FALSE (x.useNormalMap);
	// Le FACTEUR, lui, ne depend d'aucune UV.
	EXPECT_FLOAT_EQ (1.f, x.emissive[0]);
}

// Une carte du jeu 1 serait echantillonnee avec les UV du jeu 0 : refusee.
TEST (CgrePhongExtras, UneCarteDuSecondJeuEstRefusee)
{
	MaterialRenderer::MaterialPbrInfo info = PbrInfoWithMaps ();
	info.uvSet[static_cast<std::size_t> (cgpbr::MapSlot::occlusion)] = 1;
	const cgre::PhongExtras x = cgre::MakePhongExtras (info, true);
	EXPECT_FALSE (x.useOcclusionMap);
	EXPECT_FLOAT_EQ (0.f, x.occlusionStrength);
	EXPECT_TRUE (x.useEmissiveMap) << "les autres cartes ne sont pas concernees";
}

TEST (CgrePhongExtras, ForceEtEchelleNullesEteignentLeursCartes)
{
	MaterialRenderer::MaterialPbrInfo info = PbrInfoWithMaps ();
	info.factors.occlusionStrength = 0.f;
	info.factors.normalScale       = 0.f;
	const cgre::PhongExtras x = cgre::MakePhongExtras (info, true);
	EXPECT_FALSE (x.useOcclusionMap);
	EXPECT_FALSE (x.useNormalMap);
}

TEST (CgrePhongExtras, SeulLeModeMaskDecoupe)
{
	MaterialRenderer::MaterialPbrInfo info = PbrInfoWithMaps ();
	info.factors.alphaCutoff = 0.3f;

	info.alphaMode = cgpbr::AlphaMode::mask;
	EXPECT_FLOAT_EQ (0.3f, cgre::MakePhongExtras (info, true).alphaCutoff);

	info.alphaMode = cgpbr::AlphaMode::blend;
	EXPECT_LT (cgre::MakePhongExtras (info, true).alphaCutoff, 0.f);
}

// DE BOUT EN BOUT, par le registre : GetPbrInfo doit transmettre ce que la
// projection de Phong jette -- emission et mode d'alpha. Sans carte, AddMaterial
// n'appelle aucune fonction GL (la projection est un MaterialColorExt).
TEST_F (CgreMaterialRenderer, GetPbrInfoTransmetEmissionEtAlpha)
{
	auto* pbr = new MaterialPbr ();
	pbr->SetName ("lanterne");
	pbr->EditFactors ().emissive[0] = 0.8f;
	pbr->EditFactors ().alphaCutoff = 0.25f;
	pbr->SetAlphaMode (cgpbr::AlphaMode::mask);
	m_owned.emplace_back (pbr);

	const int id = MaterialRenderer::getInstance ()->AddMaterial (pbr);
	ASSERT_GE (id, 0);

	const MaterialRenderer::MaterialPbrInfo info =
		MaterialRenderer::getInstance ()->GetPbrInfo ((unsigned int)id);
	ASSERT_TRUE (info.isPbr);
	EXPECT_EQ (cgpbr::AlphaMode::mask, info.alphaMode);

	const cgre::PhongExtras x = cgre::MakePhongExtras (info, true);
	EXPECT_FLOAT_EQ (0.8f,  x.emissive[0]);
	EXPECT_FALSE (x.useEmissiveMap) << "aucune carte televersee";
	EXPECT_FLOAT_EQ (0.25f, x.alphaCutoff);
}

} // namespace

#endif // CG_HAS_CGRE
