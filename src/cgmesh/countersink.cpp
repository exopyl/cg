#include "countersink.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "mesh.h"

namespace {

// Les CINQ anneaux de la bague, du bas vers le haut, chacun de `segments`
// sommets ranges dans le meme ordre angulaire :
//
//     E  ---------------------  Ro, zTop      \
//     |                     |                  |  paroi exterieure
//     |    C ..... bouche ..|.. R, zTop       /   (mord dans la plaque)
//     |     \               |
//     |      \  cone        |
//     |       B ....... r, zTop - coneDepth
//     |       |             |
//     |       | cylindre    |
//     D ------A------------ |  r et Ro, zBottom
//
// Chaque paroi est un ruban de deux triangles par segment, soit 10 x segments
// en tout. L'orientation de chacun est DERIVEE une fois pour toutes ci-dessous
// et verifiee par le volume signe dans les tests -- une bague retournee se
// soustrairait de la piece au lieu de s'y ajouter, et rien a l'ecran ne le
// dirait.
struct Ring
{
	float radius;
	float z;
};

} // namespace

Mesh* countersinkCollar (const CountersinkCollarOptions& opt)
{
	const float r = opt.holeRadius;
	const float R = opt.mouthRadius;
	const float t = opt.zTop - opt.zBottom;

	if (r <= 0.f || R <= r) return nullptr;
	if (t <= 0.f) return nullptr;
	// Egalite comprise : une fraisure aussi profonde que la plaque ne laisse
	// aucune portee cylindrique, donc rien pour guider la vis.
	if (opt.coneDepth <= 0.f || opt.coneDepth >= t) return nullptr;

	int n = opt.segments;
	if (n < 8) n = 8;

	const float Ro = R + ((opt.bite > 0.f) ? opt.bite : 0.f);
	const float zc = opt.zTop - opt.coneDepth;

	const Ring A { r,  opt.zBottom };
	const Ring B { r,  zc };
	const Ring C { R,  opt.zTop };
	const Ring D { Ro, opt.zBottom };
	const Ring E { Ro, opt.zTop };
	const Ring rings[5] = { A, B, C, D, E };

	// Meme convention de phase que circleContour : le sommet k est a l'angle
	// 2 pi k / n. C'est ce qui fait coincider les aretes de la bague et celles
	// du percage de la plaque, donc un recouvrement UNIFORME de `bite` tout
	// autour plutot qu'un contact par endroits.
	const float PI = 3.14159265358979323846f;
	std::vector<float> verts;
	verts.reserve ((std::size_t)n * 5 * 3);
	for (int ring = 0; ring < 5; ++ring)
	{
		for (int k = 0; k < n; ++k)
		{
			const float a = 2.f * PI * (float)k / (float)n;
			verts.push_back (opt.cx + rings[ring].radius * std::cos (a));
			verts.push_back (opt.cy + rings[ring].radius * std::sin (a));
			verts.push_back (rings[ring].z);
		}
	}

	// Indice du sommet k de l'anneau `ring`, en refermant le tour.
	const auto V = [n] (int ring, int k) -> unsigned int {
		return (unsigned int)(ring * n + (k % n));
	};
	enum { iA = 0, iB = 1, iC = 2, iD = 3, iE = 4 };

	std::vector<unsigned int> tris;
	tris.reserve ((std::size_t)n * 10 * 3);
	const auto tri = [&tris] (unsigned int a, unsigned int b, unsigned int c) {
		tris.push_back (a); tris.push_back (b); tris.push_back (c);
	};

	for (int k = 0; k < n; ++k)
	{
		const int k1 = k + 1;

		// FOND (z = zBottom), normale vers le bas : l'anneau plein r -> Ro.
		tri (V (iA, k), V (iA, k1), V (iD, k1));
		tri (V (iA, k), V (iD, k1), V (iD, k));

		// PAROI EXTERIEURE (Ro), normale vers l'exterieur.
		tri (V (iD, k), V (iD, k1), V (iE, k1));
		tri (V (iD, k), V (iE, k1), V (iE, k));

		// DESSUS (z = zTop), normale vers le haut : le mince liston R -> Ro, tout
		// ce qui reste de la face superieure une fois la bouche ouverte.
		tri (V (iC, k), V (iE, k), V (iE, k1));
		tri (V (iC, k), V (iE, k1), V (iC, k1));

		// PORTEE CYLINDRIQUE (r, de zBottom a zc), normale vers l'AXE : c'est le
		// vide du trou de vis qui est de ce cote.
		tri (V (iA, k), V (iB, k), V (iB, k1));
		tri (V (iA, k), V (iB, k1), V (iA, k1));

		// CONE (de r a R), meme sens : la matiere est a l'exterieur du cone, le
		// vide a l'interieur et au-dessus -- l'interieur d'un entonnoir.
		tri (V (iB, k), V (iC, k), V (iC, k1));
		tri (V (iB, k), V (iC, k1), V (iB, k1));
	}

	const unsigned int nVerts = (unsigned int)(verts.size () / 3);
	const std::size_t nFaces = tris.size () / 3;

	Mesh* m = new Mesh ();
	m->Init (nVerts, (unsigned int)nFaces);
	m->SetVertices (nVerts, verts.data ());
	for (std::size_t i = 0; i < nFaces; ++i)
	{
		auto f = m->FaceAt (i);
		f->SetNVertices (3);
		f->SetVertex (0, tris[3 * i + 0]);
		f->SetVertex (1, tris[3 * i + 1]);
		f->SetVertex (2, tris[3 * i + 2]);
		f->SetMaterialId (opt.materialId);
	}
	m->ComputeNormals ();
	m->IncrementRevision ();
	return m;
}
