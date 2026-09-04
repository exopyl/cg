#include "mesh_slab.h"

#include <algorithm>

#include "mesh.h"

bool meshSlabExtentX (const Mesh& mesh, float yLo, float yHi, float& xMin, float& xMax)
{
	if (yHi < yLo) std::swap (yLo, yHi);

	// Les accesseurs de Mesh ne sont pas const ; la lecture, elle, l'est. Le
	// const_cast est cantonne ici plutot que d'elargir une signature partagee --
	// meme choix que graphExportObj et que mesh.merge.
	Mesh& m = const_cast<Mesh&> (mesh);

	bool found = false;
	float lo = 0.f, hi = 0.f;
	const auto note = [&] (float x) {
		if (!found) { lo = hi = x; found = true; return; }
		if (x < lo) lo = x;
		if (x > hi) hi = x;
	};

	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		const int n = m.GetFaceNVertices (f);
		if (n < 2) continue;

		for (int k = 0; k < n; ++k)
		{
			float a[3], b[3];
			m.GetVertex ((unsigned int)m.GetFaceVertex (f, k), a);
			m.GetVertex ((unsigned int)m.GetFaceVertex (f, (k + 1) % n), b);

			// Le sommet lui-meme, quand il est dans la tranche. Parcourir les
			// ARETES les visite tous, donc rien n'est oublie.
			if (a[1] >= yLo && a[1] <= yHi)
				note (a[0]);

			// Puis les deux traversees de plan. Une arete entierement dans un plan
			// est ignoree ici : ses deux sommets viennent d'etre pris ci-dessus, et
			// l'interpolation y diviserait par zero.
			const float planes[2] = { yLo, yHi };
			for (int p = 0; p < 2; ++p)
			{
				const float d0 = a[1] - planes[p];
				const float d1 = b[1] - planes[p];
				if ((d0 < 0.f && d1 < 0.f) || (d0 > 0.f && d1 > 0.f)) continue;
				if (d0 == d1) continue;
				const float t = d0 / (d0 - d1);
				if (t < 0.f || t > 1.f) continue;
				note (a[0] + t * (b[0] - a[0]));
			}
		}
	}

	if (!found) return false;
	xMin = lo;
	xMax = hi;
	return true;
}
