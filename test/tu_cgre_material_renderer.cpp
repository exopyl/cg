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

} // namespace

#endif // CG_HAS_CGRE
