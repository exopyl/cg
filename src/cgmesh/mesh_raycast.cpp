#include <vector>

#include "mesh_raycast.h"
#include "mesh.h"
#include "octree.h"

namespace {

// Teste le rayon contre le triangle (a, b, c), indices de sommets du maillage.
// Met a jour l'intersection la plus proche si elle est meilleure que fT.
//
// `tri` est passe en parametre pour que l'appelant le reutilise d'une iteration a
// l'autre : Triangle::Init alloue une AABox, donc un new/delete par triangle et
// par rayon. C'est le cout dominant de ce fichier.
void TestTriangle (Triangle &tri, const Mesh &mesh,
                   unsigned int a, unsigned int b, unsigned int c,
                   const Vector3f &vOrig, const Vector3f &vDirection,
                   float &fT, Vector3f &vIntersection, Vector3f &vNormal)
{
	float va[3], vb[3], vc[3];
	if (mesh.GetVertex (a, va) != 0) return;
	if (mesh.GetVertex (b, vb) != 0) return;
	if (mesh.GetVertex (c, vc) != 0) return;

	tri.Init (va[0], va[1], va[2],
	          vb[0], vb[1], vb[2],
	          vc[0], vc[1], vc[2]);

	float fTCurrent;
	Vector3f vIntersectionCurrent, vNormalCurrent;
	if (!tri.GetIntersectionWithRay (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent))
		return;

	if (fT < 0. || fTCurrent < fT)
	{
		fT = fTCurrent;
		vIntersection = vIntersectionCurrent;
		vNormal = vNormalCurrent;
	}
}

// Parcours recursif de l'octree. Une feuille teste ses triangles ; un noeud
// interne descend dans les enfants dont la boite englobante est touchee.
int RaycastNode (const Mesh &mesh, const Octree &node,
                 const Vector3f &vOrig, const Vector3f &vDirection,
                 float *_t, Vector3f &vIntersection, Vector3f &vNormal)
{
	if (node.IsLeaf ())
	{
		const unsigned int nTriangles = node.GetNTriangles ();
		const unsigned int *pTriangles = node.GetTriangles ();
		float fT = -1.;
		Triangle tri;
		for (unsigned int i=0; i<nTriangles; i++)
			TestTriangle (tri, mesh,
			              pTriangles[3*i], pTriangles[3*i+1], pTriangles[3*i+2],
			              vOrig, vDirection, fT, vIntersection, vNormal);

		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}

	// Noeud interne : descendre dans les enfants dont la boite est touchee.
	Ray ray (vOrig[0], vOrig[1], vOrig[2], vDirection[0], vDirection[1], vDirection[2]);
	float fT = -1.;
	const Octree* const* pChildren = node.GetChildren ();
	for (int i=0; i<8; i++)
	{
		const Octree *pChild = pChildren[i];
		if (!pChild)
			continue;

		float vecMin[3], vecMax[3];
		pChild->GetMinMax (vecMin, vecMax);
		AABox box (vecMin[0], vecMin[1], vecMin[2]);
		box.AddVertex (vecMax[0], vecMax[1], vecMax[2]);
		if (!box.intersection (ray, 0., 10000.))
			continue;

		float fTCurrent;
		Vector3f vIntersectionCurrent, vNormalCurrent;
		if (RaycastNode (mesh, *pChild, vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent))
		{
			if (fT < 0. || fTCurrent < fT)
			{
				fT = fTCurrent;
				vIntersection = vIntersectionCurrent;
				vNormal = vNormalCurrent;
			}
		}
	}

	*_t = fT;
	return (fT < 0.)? 0 : 1;
}

} // namespace

std::unique_ptr<Octree> BuildRaycastOctree (Mesh &mesh)
{
	auto pOctree = std::make_unique<Octree> ();

	std::vector<unsigned int> tris = mesh.GetTriangles ();

	// ⚠ tris.size()/3, et SURTOUT PAS mesh.GetNFaces(). GetTriangles() triangule :
	// un maillage a n-gons rend strictement plus de triangles que de faces. Sur un
	// compte trop petit, l'octree n'indexe qu'une partie des triangles et les
	// rayons manquent des faces existantes, sans rien signaler. Un maillage
	// tout-triangles ne fait pas la difference -- c'est ce qui rend l'erreur
	// facile a commettre et invisible. Couvert par tu_cgmesh_raycast.cpp.
	const int nTriangles = (int)(tris.size () / 3);

	pOctree->BuildForTriangles (mesh.GetVertices ().data (), (int)mesh.GetNVertices (),
	                            100, // maxTriangles par feuille
	                            5,   // maxDepth
	                            tris.data (), nTriangles);
	return pOctree;
}

int GetIntersectionWithRay (const Mesh &mesh, const Octree &octree,
                            const Vector3f &vOrig, const Vector3f &vDirection,
                            float *_t, Vector3f &vIntersection, Vector3f &vNormal)
{
	return RaycastNode (mesh, octree, vOrig, vDirection, _t, vIntersection, vNormal);
}

int GetIntersectionWithRayBruteForce (const Mesh &mesh,
                                      const Vector3f &vOrig, const Vector3f &vDirection,
                                      float *_t, Vector3f &vIntersection, Vector3f &vNormal)
{
	float fT = -1.;
	Triangle tri;

	const unsigned int nFaces = mesh.GetNFaces ();
	for (unsigned int fi=0; fi<nFaces; fi++)
	{
		const int nCorners = mesh.GetFaceNVertices (fi);
		if (nCorners < 3)
			continue;

		// Eventail depuis le coin 0 : (0,1,2), (0,2,3), ...
		for (int k=1; k+1<nCorners; k++)
		{
			const int a = mesh.GetFaceVertex (fi, 0);
			const int b = mesh.GetFaceVertex (fi, k);
			const int c = mesh.GetFaceVertex (fi, k+1);
			if (a < 0 || b < 0 || c < 0)
				continue;

			TestTriangle (tri, mesh, (unsigned int)a, (unsigned int)b, (unsigned int)c,
			              vOrig, vDirection, fT, vIntersection, vNormal);
		}
	}

	*_t = fT;
	return (fT < 0.)? 0 : 1;
}
