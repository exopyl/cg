#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "../src/cgmesh/cgmesh.h"

//
// ParametricSurface::EvaluateTensor() sur ParametricTorus.
//
// Ce code calcule les courbures ANALYTIQUEMENT, depuis les deux formes
// fondamentales construites a partir des cinq derivees fournies par
// EvaluatePosition(). Il ne partage aucune ligne avec les estimateurs discrets de
// DiffParamEvaluator_*, et c'est tout l'interet : une fois valide, il fournit une
// verite de terrain SOMMET PAR SOMMET pour les mesurer.
//
// Le tore est le cas d'epreuve utile parce qu'il porte les trois regimes de
// courbure sur une seule surface fermee, sans bord ni singularite :
//   K > 0 sur la moitie exterieure, K < 0 sur la moitie interieure, K = 0 sur les
//   deux paralleles v = pi/2 et v = 3pi/2.
//
// PARAMETRISATION REELLE DU CODE (surface_parametric.cpp:688) :
//   p(U,V) = ((R + r cos v) cos u, (R + r cos v) sin u, r sin v)
//   avec u = -2*pi*U   (noter le SIGNE) et v = +2*pi*V
//
// La normale etant n = p_u x p_v, ce signe la rend SORTANTE, ce qui fixe le signe
// des courbures :
//   kappa_meridien = -1/r                        (constante, le long du tube)
//   kappa_parallele = -cos v / (R + r cos v)
// Verification aux trois points remarquables, avec R=3, r=1 :
//   v=0   (exterieur) : -1/(R+r) = -0.25 et -1/r = -1  -> K = +1/4, tous deux < 0
//   v=pi/2            :  0            et -1            -> K = 0
//   v=pi  (interieur) : +1/(R-r) = +0.5  et -1         -> K = -1/2, une selle
//
// Comme -1/(R+r) > -1/r pour tout R > 0, la courbure PARALLELE est toujours
// kappa_max et la MERIDIENNE toujours kappa_min. Les tests s'appuient sur cet
// ordre, et l'ecart le signalerait.
//

namespace
{
	const float R = 3.0f;      // grand rayon
	const float r = 1.0f;      // petit rayon (rayon du tube)

	// Indexation des sommets par Generate() : index = v * nu + u,
	// avec fU = u/nu et fV = v/nv (bCloseU et bCloseV sont vrais, sans
	// dedoublement de couture, donc nu-1+1 = nu).
	unsigned int vertexIndex (unsigned int nu, unsigned int u, unsigned int v)
	{
		return v * nu + u;
	}

	float meridianCurvature (float /*fv*/, float rr)
	{
		return -1.0f / rr;
	}

	float parallelCurvature (float fv, float RR, float rr)
	{
		return -std::cos(fv) / (RR + rr * std::cos(fv));
	}

	float angleV (unsigned int v, unsigned int nv)
	{
		return 2.0f * (float)M_PI * (float)v / (float)nv;
	}
}

// ---------------------------------------------------------------------------
//  Les courbures principales, en chaque sommet
// ---------------------------------------------------------------------------
// Le test central : balayage de la grille entiere, comparaison a la forme close.
// Un ecart ici invalide l'oracle, donc tout ce qui s'y appuie.
TEST (TEST_cgmesh_surface_parametric, torus_principal_curvatures_match_closed_form)
{
	const unsigned int nu = 24, nv = 16;
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());
	ASSERT_EQ (torus.GetNVertices (), nu * nv);

	unsigned int checked = 0;
	for (unsigned int v = 0; v < nv; ++v)
	{
		const float fv    = angleV (v, nv);
		const float kMer  = meridianCurvature (fv, r);
		const float kPar  = parallelCurvature (fv, R, r);
		SCOPED_TRACE (::testing::Message () << "v=" << v << " fv=" << fv);

		for (unsigned int u = 0; u < nu; ++u)
		{
			Tensor* t = torus.GetTensor (vertexIndex (nu, u, v));
			ASSERT_NE (t, nullptr) << "aucun tenseur au sommet (" << u << "," << v << ")";

			// parallele = kappa_max, meridien = kappa_min (cf. en-tete)
			EXPECT_NEAR (t->GetKappaMax (), kPar, 2e-4f) << "u=" << u;
			EXPECT_NEAR (t->GetKappaMin (), kMer, 2e-4f) << "u=" << u;
			++checked;
		}
	}
	EXPECT_EQ (checked, nu * nv) << "des sommets n'ont pas ete verifies";
}

// La courbure du meridien ne depend QUE du rayon du tube : c'est le cercle de
// rayon r, quel que soit l'endroit ou on le coupe. Une dependance en v
// trahirait une erreur dans la seconde forme fondamentale.
TEST (TEST_cgmesh_surface_parametric, torus_meridian_curvature_is_constant_over_the_whole_surface)
{
	const unsigned int nu = 20, nv = 20;
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());

	for (unsigned int i = 0; i < torus.GetNVertices (); ++i)
	{
		Tensor* t = torus.GetTensor (i);
		ASSERT_NE (t, nullptr) << "sommet " << i;
		EXPECT_NEAR (t->GetKappaMin (), -1.0f / r, 2e-4f) << "sommet " << i;
	}
}

// La courbure de Gauss decoupe le tore en trois regimes. C'est le test qui
// justifie le choix du tore : aucune autre surface fermee simple ne les porte
// tous les trois.
TEST (TEST_cgmesh_surface_parametric, torus_gaussian_curvature_partitions_the_surface)
{
	const unsigned int nu = 12, nv = 16;      // nv multiple de 4 : v=pi/2 et 3pi/2 sont sur la grille
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());

	for (unsigned int v = 0; v < nv; ++v)
	{
		const float fv   = angleV (v, nv);
		const float cosv = std::cos (fv);
		Tensor* t = torus.GetTensor (vertexIndex (nu, 0, v));
		ASSERT_NE (t, nullptr);
		const float K = t->GetGaussianCurvature ();
		SCOPED_TRACE (::testing::Message () << "v=" << v << " cos(v)=" << cosv << " K=" << K);

		// K = cos v / (r (R + r cos v)) : le signe est celui de cos v.
		EXPECT_NEAR (K, cosv / (r * (R + r * cosv)), 2e-4f);

		if (std::fabs (cosv) < 1e-5f)
			EXPECT_NEAR (K, 0.0f, 2e-4f) << "parabolique attendu sur v=pi/2 et v=3pi/2";
		else if (cosv > 0.0f)
			EXPECT_GT (K, 0.0f) << "elliptique attendu sur la moitie exterieure";
		else
			EXPECT_LT (K, 0.0f) << "hyperbolique attendu sur la moitie interieure";
	}
}

// Bornes exactes de K : maximum a l'equateur exterieur, minimum a l'interieur.
// Attrape une erreur de signe globale, qui echangerait les deux.
TEST (TEST_cgmesh_surface_parametric, torus_gaussian_curvature_extrema_sit_on_the_equators)
{
	const unsigned int nu = 12, nv = 16;
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());

	Tensor* outer = torus.GetTensor (vertexIndex (nu, 0, 0));         // v = 0
	Tensor* inner = torus.GetTensor (vertexIndex (nu, 0, nv / 2));    // v = pi
	ASSERT_NE (outer, nullptr);
	ASSERT_NE (inner, nullptr);

	EXPECT_NEAR (outer->GetGaussianCurvature (),  1.0f / (r * (R + r)), 2e-4f);
	EXPECT_NEAR (inner->GetGaussianCurvature (), -1.0f / (r * (R - r)), 2e-4f);

	// A l'exterieur les deux courbures sont de meme signe, a l'interieur non.
	EXPECT_GT (outer->GetKappaMax () * outer->GetKappaMin (), 0.0f);
	EXPECT_LT (inner->GetKappaMax () * inner->GetKappaMin (), 0.0f);
}

// La courbure moyenne, redondante avec les deux precedentes mais calculee par un
// chemin different dans EvaluateTensor (elle vient de H, pas de kappa1*kappa2).
TEST (TEST_cgmesh_surface_parametric, torus_mean_curvature_matches_closed_form)
{
	const unsigned int nu = 10, nv = 16;
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());

	for (unsigned int v = 0; v < nv; ++v)
	{
		const float fv = angleV (v, nv);
		Tensor* t = torus.GetTensor (vertexIndex (nu, 0, v));
		ASSERT_NE (t, nullptr);
		const float expected = 0.5f * (meridianCurvature (fv, r) + parallelCurvature (fv, R, r));
		EXPECT_NEAR (t->GetMeanCurvature (), expected, 2e-4f) << "v=" << v;
	}
}

// La courbure est une propriete de la SURFACE, pas de l'echantillonnage : deux
// grilles differentes doivent donner la meme valeur au meme (u,v). Attrape une
// formule qui melerait le pas de la grille aux derivees.
TEST (TEST_cgmesh_surface_parametric, torus_curvature_is_independent_of_the_sampling_grid)
{
	ParametricTorus coarse (8,  8,  R, r);
	ParametricTorus fine   (64, 32, R, r);
	ASSERT_TRUE (coarse.Generate ());
	ASSERT_TRUE (fine.Generate ());

	// v = 0, pi/2, pi, 3pi/2 tombent sur les deux grilles.
	for (int q = 0; q < 4; ++q)
	{
		Tensor* c = coarse.GetTensor (vertexIndex (8,  0, (unsigned)q * 8  / 4));
		Tensor* f = fine.GetTensor   (vertexIndex (64, 0, (unsigned)q * 32 / 4));
		ASSERT_NE (c, nullptr);
		ASSERT_NE (f, nullptr);
		EXPECT_NEAR (c->GetKappaMax (), f->GetKappaMax (), 2e-4f) << "quart " << q;
		EXPECT_NEAR (c->GetKappaMin (), f->GetKappaMin (), 2e-4f) << "quart " << q;
	}
}

// La courbure est l'inverse d'une longueur : doubler la taille du tore doit
// diviser toutes les courbures par deux. Attrape un facteur d'echelle manquant
// ou une normalisation en trop.
TEST (TEST_cgmesh_surface_parametric, torus_curvature_scales_inversely_with_size)
{
	const unsigned int nu = 12, nv = 16;
	ParametricTorus unit   (nu, nv, R,        r);
	ParametricTorus doubled(nu, nv, 2.0f * R, 2.0f * r);
	ASSERT_TRUE (unit.Generate ());
	ASSERT_TRUE (doubled.Generate ());

	for (unsigned int v = 0; v < nv; ++v)
	{
		Tensor* a = unit.GetTensor    (vertexIndex (nu, 0, v));
		Tensor* b = doubled.GetTensor (vertexIndex (nu, 0, v));
		ASSERT_NE (a, nullptr);
		ASSERT_NE (b, nullptr);
		EXPECT_NEAR (b->GetKappaMax (), 0.5f * a->GetKappaMax (), 2e-4f) << "v=" << v;
		EXPECT_NEAR (b->GetKappaMin (), 0.5f * a->GetKappaMin (), 2e-4f) << "v=" << v;
	}
}

// La normale doit etre SORTANTE : c'est ce qui fixe le signe des courbures, donc
// ce dont depend tout l'en-tete de ce fichier. Sur un tore, « sortant » veut dire
// s'ecarter du cercle central de rayon R.
TEST (TEST_cgmesh_surface_parametric, torus_normal_points_away_from_the_tube_axis)
{
	const unsigned int nu = 16, nv = 16;
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());

	for (unsigned int v = 0; v < nv; ++v)
		for (unsigned int u = 0; u < nu; u += 4)
		{
			const unsigned int idx = vertexIndex (nu, u, v);
			Tensor* t = torus.GetTensor (idx);
			ASSERT_NE (t, nullptr);

			float P[3];
			ASSERT_GE (torus.GetVertex (idx, P), 0);

			// Centre du tube sur ce meridien : meme direction radiale, rayon R.
			const float rho = std::sqrt (P[0]*P[0] + P[1]*P[1]);
			ASSERT_GT (rho, 1e-6f);
			const float C[3] = { R * P[0] / rho, R * P[1] / rho, 0.0f };

			float n[3];
			t->GetNormal (n);
			const float dot = n[0]*(P[0]-C[0]) + n[1]*(P[1]-C[1]) + n[2]*(P[2]-C[2]);

			// |P - C| = r, et la normale doit etre exactement radiale au tube.
			EXPECT_NEAR (dot, r, 1e-3f)
				<< "normale non sortante ou non radiale en (" << u << "," << v << ")";
		}
}

// ---------------------------------------------------------------------------
//  Directions principales : DEFECTUEUSES
// ---------------------------------------------------------------------------
// Les courbures ci-dessus sont exactes, les directions ne le sont pas.
//
// EvaluateTensor() resout le systeme propre de la matrice de forme 2x2, ce qui
// donne un vecteur propre dans l'espace des PARAMETRES (u,v). Le passage en 3D
// devrait etre  d = ev[0] * p_u + ev[1] * p_v.  Le code ecrit a la place
// (surface_parametric.cpp:277) :
//
//     Vector3d d1 (ev1[0], ev1[1], 0.0f);   // (u,v) relu comme un (x,y,0) MONDE
//     ...
//     dd1 = d1 - (d1 . n) n;                // puis projete sur le plan tangent
//
// Le resultat n'a aucune raison d'etre la direction principale, et se degrade
// jusqu'a s'annuler : au sommet (u=0, v=0) le plan tangent est engendre par y et
// z, la normale est x, donc la projection de (1,0,0) vaut ZERO.
//
// Ce test ne valide pas ce comportement, il le FIGE, pour qu'une correction soit
// un changement reconnu. Le jour ou il echoue, les directions sont probablement
// devenues justes : verifier, puis remplacer ces attentes par les vraies
// (meridien -> z, parallele -> y au sommet u=0).
TEST (TEST_cgmesh_surface_parametric, torus_principal_directions_are_currently_wrong)
{
	const unsigned int nu = 16, nv = 16;
	ParametricTorus torus (nu, nv, R, r);
	ASSERT_TRUE (torus.Generate ());

	// --- equateur exterieur, u = 0 : position (R+r, 0, 0), normale +x ---------
	// Plan tangent engendre par y (parallele) et z (meridien).
	{
		Tensor* t = torus.GetTensor (vertexIndex (nu, 0, 0));
		ASSERT_NE (t, nullptr);
		float dmax[3], dmin[3];
		t->GetDirectionMax (dmax);
		t->GetDirectionMin (dmin);

		// dmax devrait valoir +/-y (la parallele porte kappa_max). Il est NUL.
		const float lenMax = std::sqrt (dmax[0]*dmax[0] + dmax[1]*dmax[1] + dmax[2]*dmax[2]);
		EXPECT_NEAR (lenMax, 0.0f, 1e-5f)
			<< "la direction max n'est plus le vecteur nul : les directions ont "
			   "peut-etre ete corrigees, cf. le commentaire ci-dessus";

		// dmin devrait valoir +/-z (le meridien porte kappa_min). Il vaut y,
		// c'est-a-dire la direction de l'AUTRE courbure principale.
		EXPECT_NEAR (std::fabs (dmin[1]), 1.0f, 1e-5f)
			<< "la direction min n'est plus alignee sur y";
		EXPECT_NEAR (std::fabs (dmin[2]), 0.0f, 1e-5f)
			<< "la direction min s'aligne sur z : elle est peut-etre correcte "
			   "desormais";
	}

	// --- equateur interieur, u = 0 : position (R-r, 0, 0), normale -x ---------
	// Ici dmax sort non nul, mais les deux directions sont ECHANGEES : dmax
	// s'aligne sur z (le meridien, qui porte kappa_min) et dmin sur y.
	{
		Tensor* t = torus.GetTensor (vertexIndex (nu, 0, nv / 2));
		ASSERT_NE (t, nullptr);
		float dmax[3], dmin[3];
		t->GetDirectionMax (dmax);
		t->GetDirectionMin (dmin);

		EXPECT_NEAR (std::fabs (dmax[2]), 1.0f, 1e-5f)
			<< "direction max attendue sur z (le meridien) tant que le defaut est la";
		EXPECT_NEAR (std::fabs (dmin[1]), 1.0f, 1e-5f)
			<< "direction min attendue sur y (la parallele) tant que le defaut est la";
	}
}
