#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "../src/cgmesh/cgmesh.h"
#include "../src/cgimg/color.h"

//
//  Coloriage des courbures -- MeshAlgoTensorEvaluator::EvaluateColors.
//
// EvaluateColors n'avait aucune couverture, et il portait sa PROPRE rampe
// « jet » : six tableaux `static float` NON const, dupliques de celle de cgimg.
// La rampe qui fait foi est celle de cgimg (color_jet, color.cpp) ; cgmesh
// depend deja de cgimg, la copie locale n'achetait rien.
//
// DEUX FILETS, ET AUCUN NE TRAVERSE LA CHAINE DE COURBURE.
//
// 1. La rampe est pinnee bit a bit sur des indices EXACTS. color_jet n'est que
//    des soustractions, des multiplications et une division : sur des entrees
//    exactes, sa sortie ne depend pas de la chaine d'outils. C'est ce pin qui
//    porte le signe du zero -- color_jet (1.f) rend (0.5, ±0.f, ±0.f), et un
//    condensat des bits distingue les deux.
//
// 2. Le mappage est verifie par COHERENCE, dans le processus courant : chaque
//    couleur produite doit valoir EXACTEMENT color_jet (array[i] / max), l'index
//    etant recalcule ici par le meme parcours qu'EvaluateColors. Une rampe
//    locale qui reviendrait, une normalisation par (max - min) au lieu du max,
//    un sommet sans tenseur qui ne serait plus noir : les trois se voient. Cette
//    verification compare deux valeurs calculees par le MEME binaire, elle est
//    donc vraie sur toute chaine d'outils.
//
// ⚠ CE QUI A ETE RETIRE, ET POURQUOI. Le fichier pinnait un condensat des bits
// de TOUT le tableau de couleurs, valeur relevee sous MSVC seulement. Ce tableau
// est la sortie de « normales Thurmer -> tenseur Taubin -> |courbure| -> division
// par le max -> color_jet » : une telle chaine n'est pas reproductible bit a bit
// d'un compilateur a l'autre. Le premier passage en CI Linux l'a montre le
// 2026-08-30 -- condensat different, alors que min et max concordent a 1e-6 et
// que le sommet de courbure maximale rendait toujours exactement (0.5, 0, 0).
// L'ecart etait du bruit flottant, pas un changement de couleur. Un instrument
// qui accuse le code qu'il mesure se remplace, il ne se desactive pas.
//
// Ce que ce fichier ne pretend plus tenir : la derive lente de la COURBURE. Elle
// n'est surveillee ici que par ses bornes, a 1e-6 ; c'est aux suites
// d'estimateurs de tenseurs de la tenir de pres.
//

static const float TORUS_R = 3.0f;
static const float TORUS_r = 1.0f;

static Mesh_half_edge* build_small_torus (std::unique_ptr<ParametricTorus>& oracle)
{
	const unsigned int nu = 40, nv = 20;
	oracle = std::make_unique<ParametricTorus> (nu, nv, TORUS_R, TORUS_r);
	if (!oracle->Generate ())
		return nullptr;

	const unsigned int nVerts = oracle->GetNVertices ();
	std::vector<float> verts (3 * (size_t)nVerts);
	for (unsigned int i = 0; i < nVerts; i++)
		oracle->GetVertex (i, &verts[3 * (size_t)i]);

	std::vector<unsigned int> faces;
	for (unsigned int f = 0; f < oracle->GetNFaces (); f++)
	{
		if (oracle->GetFaceNVertices (f) != 3)
			continue;
		for (int k = 0; k < 3; k++)
			faces.push_back ((unsigned int)oracle->GetFaceVertex (f, k));
	}

	Mesh_half_edge* he = new Mesh_half_edge ();
	he->m_pMesh->SetVertices (nVerts, verts.data ());
	he->m_pMesh->SetFaces ((unsigned int)(faces.size () / 3), 3, faces.data ());
	he->create_half_edge ();
	he->m_pMesh->InitVertexNormals ();

	Normals normals;
	normals.EvalOnVertices (he, Normals::THURMER);
	return he;
}

// FNV-1a sur la representation binaire : aucune tolerance, contrairement a une
// comparaison a epsilon qui laisserait passer une derive lente.
static uint64_t digest (const std::vector<float>& v)
{
	uint64_t h = 1469598103934665603ull;
	for (float f : v)
	{
		uint32_t bits;
		std::memcpy (&bits, &f, sizeof (bits));
		for (int b = 0; b < 4; b++)
		{
			h ^= (uint64_t)((bits >> (8 * b)) & 0xffu);
			h *= 1099511628211ull;
		}
	}
	return h;
}

// Les bornes des six tables de color_jet, plus quelques points interieurs. Les
// bornes sont les seuls endroits ou la branche de recherche change de segment.
static const float JET_INDICES[] = {
	0.f,   0.11f, 0.125f, 0.25f, 0.34f, 0.35f, 0.375f, 0.5f,
	0.64f, 0.65f, 0.66f,  0.75f, 0.89f, 0.91f, 1.f
};

static const size_t JET_INDEX_COUNT = sizeof (JET_INDICES) / sizeof (JET_INDICES[0]);

static std::vector<float> jet_ramp_samples ()
{
	std::vector<float> out;
	out.reserve (3 * JET_INDEX_COUNT);
	for (size_t i = 0; i < JET_INDEX_COUNT; i++)
	{
		float r, g, b;
		color_jet (JET_INDICES[i], &r, &g, &b);
		out.push_back (r);
		out.push_back (g);
		out.push_back (b);
	}
	return out;
}

static bool same_bits (float a, float b)
{
	return std::memcmp (&a, &b, sizeof (float)) == 0;
}

// ---------------------------------------------------------------------------
//  FILET 1 -- la rampe elle-meme, sur des entrees exactes.
// ---------------------------------------------------------------------------
TEST (TEST_cgmesh_diffparam_colors, jet_ramp_is_pinned_bit_for_bit)
{
	const std::vector<float> ramp = jet_ramp_samples ();
	ASSERT_EQ (ramp.size (), 3u * JET_INDEX_COUNT);

	EXPECT_EQ (digest (ramp), 0x36f5054c0ca8d99bull)
		<< "la rampe jet a change";

	// Reperes lisibles : un condensat seul ne dit pas OU la rampe a bouge.
	float r, g, b;
	color_jet (0.f, &r, &g, &b);
	EXPECT_FLOAT_EQ (r, 0.f);
	EXPECT_FLOAT_EQ (g, 0.f);
	EXPECT_FLOAT_EQ (b, 0.5f);

	color_jet (1.f, &r, &g, &b);
	EXPECT_FLOAT_EQ (r, 0.5f);
	EXPECT_FLOAT_EQ (g, 0.f);
	EXPECT_FLOAT_EQ (b, 0.f);

	// L'entree est bornee : au-dela de 1, la sortie doit etre celle de 1 -- et
	// au BIT pres, le signe du zero compris.
	float r2, g2, b2;
	color_jet (1.5f, &r2, &g2, &b2);
	EXPECT_TRUE (same_bits (r, r2));
	EXPECT_TRUE (same_bits (g, g2));
	EXPECT_TRUE (same_bits (b, b2));

	color_jet (0.f, &r, &g, &b);
	color_jet (-0.5f, &r2, &g2, &b2);
	EXPECT_TRUE (same_bits (r, r2));
	EXPECT_TRUE (same_bits (g, g2));
	EXPECT_TRUE (same_bits (b, b2));
}

// ---------------------------------------------------------------------------
//  FILET 2 -- le mappage, verifie par coherence dans le processus courant.
// ---------------------------------------------------------------------------
TEST (TEST_cgmesh_diffparam_colors, curvature_colors_are_exactly_the_jet_of_the_normalised_curvature)
{
	std::unique_ptr<ParametricTorus> oracle;
	Mesh_half_edge* he = build_small_torus (oracle);
	ASSERT_NE (he, nullptr);

	MeshAlgoTensorEvaluator algo;
	ASSERT_TRUE (algo.Init (he));
	ASSERT_TRUE (algo.Evaluate (TENSOR_TAUBIN));
	algo.EvaluateColors (CurvatureType::Gaussian);

	const std::vector<float>& colors = he->m_pMesh->GetVertexColors ();
	const int nv = (int)he->m_pMesh->GetNVertices ();
	ASSERT_EQ (colors.size (), 3u * (size_t)nv);

	// MEME parcours qu'EvaluateColors : par indice de sommet, tenseurs nuls
	// exclus, normalisation par le MAX seul -- et non par (max - min).
	std::vector<float> array ((size_t)nv, 0.f);
	std::vector<int>   defined ((size_t)nv, 0);
	float min_value = 0.f, max_value = 0.f;
	bool found = false;
	int argmax = -1;
	for (int i = 0; i < nv; i++)
	{
		Tensor* t = algo.GetDiffParam (i);
		if (t == nullptr)
			continue;
		defined[i] = 1;
		array[i] = std::fabs (t->GetCurvature (CurvatureType::Gaussian));
		if (!found)
		{
			min_value = max_value = array[i];
			argmax = i;
			found = true;
		}
		else
		{
			if (array[i] < min_value) min_value = array[i];
			if (array[i] > max_value) { max_value = array[i]; argmax = i; }
		}
	}
	ASSERT_TRUE (found);
	ASSERT_GT (max_value, 0.f);
	ASSERT_GE (argmax, 0);

	int nDefined = 0;
	for (int i = 0; i < nv; i++)
	{
		const size_t k = 3 * (size_t)i;
		if (defined[i])
		{
			nDefined++;
			float r, g, b;
			color_jet (array[i] / max_value, &r, &g, &b);
			// Egalite BINAIRE : la couleur doit sortir de color_jet, et non
			// d'une rampe qui lui ressemble.
			ASSERT_TRUE (same_bits (colors[k],     r)) << "sommet " << i << ", canal rouge";
			ASSERT_TRUE (same_bits (colors[k + 1], g)) << "sommet " << i << ", canal vert";
			ASSERT_TRUE (same_bits (colors[k + 2], b)) << "sommet " << i << ", canal bleu";
		}
		else
		{
			EXPECT_FLOAT_EQ (colors[k],     0.f) << "sommet sans tenseur " << i;
			EXPECT_FLOAT_EQ (colors[k + 1], 0.f) << "sommet sans tenseur " << i;
			EXPECT_FLOAT_EQ (colors[k + 2], 0.f) << "sommet sans tenseur " << i;
		}
	}
	// Un filet qui n'a rien eu a verifier ne protege rien.
	EXPECT_GT (nDefined, 0);

	// Haut de la rampe : le sommet de courbure |maximale| est le seul alimente
	// avec un index valant exactement 1.0, donc rouge sombre (0.5, 0, 0).
	EXPECT_FLOAT_EQ (colors[3 * (size_t)argmax],     0.5f);
	EXPECT_FLOAT_EQ (colors[3 * (size_t)argmax + 1], 0.f);
	EXPECT_FLOAT_EQ (colors[3 * (size_t)argmax + 2], 0.f);

	// Derive lente de la COURBURE : surveillee par ses bornes seulement, a 1e-6.
	// Valeurs concordantes sous MSVC et sous GCC, relevees le 2026-08-30.
	EXPECT_NEAR (min_value, 0.000784f, 1e-6f);
	EXPECT_NEAR (max_value, 0.953061f, 1e-6f);

	delete he;
}
