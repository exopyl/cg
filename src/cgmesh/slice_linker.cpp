#include "slice_linker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

// Chainage et reparations d'une couche de segments (voir slice_linker.h). Les
// limites connues sont signalees "(D...)" la ou elles naissent ; leur
// definition est dans docs/mesh_slicing.md.

namespace cgmesh {
namespace slicing {

namespace
{
const float kFltEps = std::numeric_limits<float>::epsilon();

// Les combinaisons d'orientation de deux paires d'aretes : les deux segments
// designent-ils le MEME couple d'aretes, dans un sens ou dans l'autre ?
bool sameEdgePair (int v11, int v12, int v21, int v22,
                   int vv11, int vv12, int vv21, int vv22)
{
	return
		(v11 == vv11 && v12 == vv12 && v21 == vv21 && v22 == vv22) ||
		(v11 == vv11 && v12 == vv12 && v21 == vv22 && v22 == vv21) ||
		(v11 == vv12 && v12 == vv11 && v21 == vv21 && v22 == vv22) ||
		(v11 == vv12 && v12 == vv11 && v21 == vv22 && v22 == vv21) ||

		(v21 == vv11 && v22 == vv12 && v11 == vv21 && v12 == vv22) ||
		(v21 == vv11 && v22 == vv12 && v11 == vv22 && v12 == vv21) ||
		(v21 == vv12 && v22 == vv11 && v11 == vv21 && v12 == vv22) ||
		(v21 == vv12 && v22 == vv11 && v11 == vv22 && v12 == vv21);
}

bool sameEdge (int a, int b, int aa, int bb)
{
	return (a == aa && b == bb) || (a == bb && b == aa);
}

// Angle polaire de (x2,y2) vu de (x1,y1), dans ]-pi, pi], en float. NaN si
// les deux points sont confondus.
float polarAngle (float x1, float y1, float x2, float y2)
{
	const float dx = x2 - x1;
	const float dy = y2 - y1;
	const float norm = std::sqrt (dx * dx + dy * dy);
	float angle = std::acos (dx / norm);
	if (dy < 0.f)
		angle = -angle;
	return angle;
}
} // namespace

SliceLinker::SliceLinker (std::vector<SliceSegment>& segments)
	: m_segments (segments)
{
	buildMap ();
}

uint64_t SliceLinker::edgeKey (int v1, int v2)
{
	const auto lo = static_cast<uint32_t> (std::min (v1, v2));
	const auto hi = static_cast<uint32_t> (std::max (v1, v2));
	return (static_cast<uint64_t> (lo) << 32) | hi;
}

void SliceLinker::insertIntoMap (uint32_t index)
{
	const SliceSegment& s = m_segments[index];
	m_map[edgeKey (s.edges[0][0], s.edges[0][1])].insert (index);
	m_map[edgeKey (s.edges[1][0], s.edges[1][1])].insert (index);
}

// Seule la cle de l'extremite 0 est retiree avant reinsertion. Une
// cle perimee de l'extremite 1 peut donc subsister ; elle est sans effet, les
// comparaisons relisant les aretes du segment lui-meme.
void SliceLinker::updateIntoMap (uint32_t index)
{
	const SliceSegment& s = m_segments[index];
	m_map[edgeKey (s.edges[0][0], s.edges[0][1])].erase (index);
	insertIntoMap (index);
}

void SliceLinker::buildMap ()
{
	for (uint32_t index = 0; index < (uint32_t)m_segments.size (); ++index)
		insertIntoMap (index);
}

bool SliceLinker::execute (bool useTopology)
{
	if (useTopology)
	{
		// (D2) Chemin NOMINAL : couche variete sur indices, chainage par cle
		// d'arete, SANS retrait des segments de longueur nulle. Le seuil absolu
		// de ce retrait (length2 < FLT_EPSILON, soit 3,45e-4 de long)
		// effondrerait tout contour fait de segments courts -- un cylindre fin
		// ou tres facette rendrait une aire nulle --, pour un cout en O(k^2).
		// Il n'est pas necessaire au chainage : un segment sans direction est
		// oriente par la regle combinatoire (D6). Les points confondus sont
		// elimines ensuite par cleanContour.
		if (isManifoldOnIndices ())
		{
			linkByComparingIndices ();
			return true;
		}

		// Chemin de REPARATION : le retrait est fait AVANT les heuristiques,
		// qui supposent des couches sans segment nul : removeNonManifoldVertices
		// calcule des angles acos (dx / norm), NaN sur un voisin nul, et
		// removeNonManifoldVerticesFromNullEdge ou linkToFillHoles comparent des
		// extremites confondues. Les rendre robustes une a une changerait leur
		// comportement sur toutes les couches reparees (T13) sans cas de test
		// pour le valider : garder leur precondition est le choix le plus sur.
		// Si le retrait suffit a rendre la couche variete, on chaine sans
		// reparer : la variete est testee de nouveau APRES le retrait.
		removeZeroLengthIntersections ();   // (D2) peut retirer un connecteur porteur
		if (isManifoldOnIndices ())
		{
			linkByComparingIndices ();
		}
		else
		{
			removeDuplicateEdges ();
			removeHairs ();
			removeNonManifoldVerticesFromNullEdge ();
			removeNonManifoldVertices ();
			linkByComparingIndices ();
			linkToFillHoles ();
		}
	}
	else
	{
		removeZeroLengthIntersections ();   // chainage par points : inchange
		linkByComparingPoints ();
	}
	return true;
}

bool SliceLinker::checkManifold (unsigned int* nonManifoldEdges, unsigned int* borders,
                                 unsigned int* nonManifoldVertices) const
{
	unsigned int nEdges = 0, nBorders = 0, nVertices = 0;
	for (const SliceSegment& s : m_segments)
	{
		if (s.visited) continue;
		if (howManySimilarEdge (s) > 1) nEdges++;
	}
	for (const SliceSegment& s : m_segments)
	{
		if (s.visited) continue;
		for (int k = 0; k < 2; ++k)
		{
			const uint32_t n = howManySimilarVertex (s, k == 0);
			if (n == 0) nBorders++;
			else if (n != 1) nVertices++;
		}
	}
	if (nonManifoldEdges) *nonManifoldEdges = nEdges;
	if (borders) *borders = nBorders;
	if (nonManifoldVertices) *nonManifoldVertices = nVertices;
	return nEdges == 0 && nBorders == 0 && nVertices == 0;
}

uint32_t SliceLinker::howManySimilarEdge (const SliceSegment& s) const
{
	uint32_t n = 0;
	const int v11 = s.edges[0][0], v12 = s.edges[0][1];
	const int v21 = s.edges[1][0], v22 = s.edges[1][1];

	const std::array<uint64_t, 2> keys = { edgeKey (v11, v12), edgeKey (v21, v22) };
	for (const uint64_t key : keys)
	{
		const auto it = m_map.find (key);
		if (it == m_map.end ()) continue;
		for (const uint32_t index : it->second)
		{
			const SliceSegment* o = &m_segments[index];
			if (o == &s || o->visited) continue;
			if (sameEdgePair (v11, v12, v21, v22,
			                  o->edges[0][0], o->edges[0][1], o->edges[1][0], o->edges[1][1]))
				n++;
		}
	}
	return n;
}

template <bool Collect>
uint32_t SliceLinker::howManySimilarVertexImpl (const SliceSegment& s,
                                                std::vector<SliceSegment*>* found,
                                                bool startPoint) const
{
	if (Collect) found->clear ();

	uint32_t n = 0;
	const int v1 = s.edges[startPoint ? 0 : 1][0];
	const int v2 = s.edges[startPoint ? 0 : 1][1];

	const auto it = m_map.find (edgeKey (v1, v2));
	if (it == m_map.end ()) return 0;

	for (const uint32_t index : it->second)
	{
		// m_segments est une reference sur un vecteur NON constant : la
		// constance de la methode ne s'y propage pas.
		SliceSegment* o = &m_segments[index];
		if (o == &s || o->visited) continue;

		if (sameEdge (v1, v2, o->edges[0][0], o->edges[0][1]))
		{
			if (Collect) found->push_back (o);
			n++;
		}
		if (sameEdge (v1, v2, o->edges[1][0], o->edges[1][1]))
		{
			if (Collect) found->push_back (o);
			n++;
		}
	}
	return n;
}

uint32_t SliceLinker::howManySimilarVertex (const SliceSegment& s, bool startPoint) const
{
	return howManySimilarVertexImpl<false> (s, nullptr, startPoint);
}

uint32_t SliceLinker::howManySimilarVertex (const SliceSegment& s,
                                            std::vector<SliceSegment*>& found, bool startPoint)
{
	return howManySimilarVertexImpl<true> (s, &found, startPoint);
}

bool SliceLinker::isManifoldOnIndices () const
{
	for (const SliceSegment& s : m_segments)
	{
		if (s.visited) continue;
		if (howManySimilarEdge (s) > 0) return false;
		if (howManySimilarVertex (s, true) != 1) return false;
		if (howManySimilarVertex (s, false) != 1) return false;
	}
	return true;
}

bool SliceLinker::removeDuplicateEdges ()
{
	for (SliceSegment& s1 : m_segments)
	{
		if (s1.visited) continue;

		const int v11 = s1.edges[0][0], v12 = s1.edges[0][1];
		const int v21 = s1.edges[1][0], v22 = s1.edges[1][1];

		const std::array<uint64_t, 2> keys = { edgeKey (v11, v12), edgeKey (v21, v22) };
		for (const uint64_t key : keys)
		{
			const auto it = m_map.find (key);
			if (it == m_map.end ()) continue;
			for (const uint32_t index : it->second)
			{
				SliceSegment* s2 = &m_segments[index];
				if (s2 == &s1 || s2->visited) continue;
				if (sameEdgePair (v11, v12, v21, v22,
				                  s2->edges[0][0], s2->edges[0][1], s2->edges[1][0], s2->edges[1][1]))
					s2->visited = true;
			}
		}
	}
	return true;
}

bool SliceLinker::removeHairs ()
{
	for (SliceSegment& s : m_segments)
	{
		if (s.visited) continue;
		if ((howManySimilarVertex (s, true) == 2 && howManySimilarVertex (s, false) == 0) ||
		    (howManySimilarVertex (s, false) == 2 && howManySimilarVertex (s, true) == 0))
			s.visited = true;
	}
	return true;
}

bool SliceLinker::removeNonManifoldVerticesFromNullEdge ()
{
	for (SliceSegment& s : m_segments)
	{
		if (s.visited) continue;
		if (howManySimilarVertex (s, true) == 2 && howManySimilarVertex (s, false) == 2 &&
		    s.length2 () < kFltEps)
			s.visited = true;
	}
	return true;
}

bool SliceLinker::removeNonManifoldVertices ()
{
	for (SliceSegment& s1 : m_segments)
	{
		if (s1.visited) continue;

		std::vector<SliceSegment*> edges;
		const uint32_t n = howManySimilarVertex (s1, edges, true);
		if (n == 2)
		{
			// un segment parmi { s1, edges[0], edges[1] } sera ecarte

			// extremites de l'arete partagee
			const int v1 = s1.edges[0][0];
			const int v2 = s1.edges[0][1];

			// coordonnees du sommet non-variete
			const float xCenter = s1.points[0].x;
			const float yCenter = s1.points[0].y;

			// les trois voisins
			bool starts[3] = { true, false, false };
			const float x1 = s1.points[1].x;
			const float y1 = s1.points[1].y;

			float x2 = 0.f, y2 = 0.f;
			if (sameEdge (v1, v2, edges[0]->edges[0][0], edges[0]->edges[0][1]))
			{
				x2 = edges[0]->points[1].x; y2 = edges[0]->points[1].y; starts[1] = true;
			}
			else
			{
				x2 = edges[0]->points[0].x; y2 = edges[0]->points[0].y; starts[1] = false;
			}

			float x3 = 0.f, y3 = 0.f;
			if (sameEdge (v1, v2, edges[1]->edges[0][0], edges[1]->edges[0][1]))
			{
				x3 = edges[1]->points[1].x; y3 = edges[1]->points[1].y; starts[2] = true;
			}
			else
			{
				x3 = edges[1]->points[0].x; y3 = edges[1]->points[0].y; starts[2] = false;
			}

			const float angles[3] = {
				polarAngle (xCenter, yCenter, x1, y1),
				polarAngle (xCenter, yCenter, x2, y2),
				polarAngle (xCenter, yCenter, x3, y3)
			};

			// cas ou l'on ne peut pas decider
			if ((!starts[0] && !starts[1] && !starts[2]) ||
			    (starts[0] && starts[1] && starts[2]))
				continue;

			// Quel segment doit partir ? Celui qui est considere comme
			// interieur : deux segments de meme sens (deux arrivees, ou deux
			// departs) encadrent le troisieme, qui est ecarte.
			const float minAngle = std::min (angles[0], std::min (angles[1], angles[2]));
			int iEdgeToRemove = -1;
			for (int i = 0; i < 3; i++)
			{
				const int i0 = i, i1 = (i + 1) % 3, i2 = (i + 2) % 3;
				const float a0 = angles[i0] - minAngle;
				const float a1 = angles[i1] - minAngle;
				const float a2 = angles[i2] - minAngle;
				if (!starts[i0] && !starts[i1])
				{
					if (a0 < a1)
						iEdgeToRemove = (a2 < a1 && a2 > a0) ? i0 : i1;
					else
						iEdgeToRemove = (a2 < a0 && a2 > a1) ? i1 : i0;
				}
				else if (starts[i0] && starts[i1])
				{
					if (a0 < a1)
						iEdgeToRemove = (a2 < a1 && a2 > a0) ? i1 : i0;
					else
						iEdgeToRemove = (a2 < a0 && a2 > a1) ? i0 : i1;
				}
				if (iEdgeToRemove != -1)
					break;
			}

			SliceSegment* edgeToRemove = nullptr;
			if (iEdgeToRemove == 0) edgeToRemove = &s1;
			else if (iEdgeToRemove == 1) edgeToRemove = edges[0];
			else if (iEdgeToRemove == 2) edgeToRemove = edges[1];

			if (edgeToRemove == nullptr)
				continue;

			edgeToRemove->visited = true;

			// (D1) suit le segment ecarte et ecarte tout ce qu'il rencontre
			// jusqu'au premier sommet non-variete -- marche NON bornee, qui peut
			// effacer un contour entier. On suppose les segments de meme sens.
			// Elle termine : chaque tour consomme un segment non encore visite.
			const bool nextExtremityStart = !starts[iEdgeToRemove];
			while (edgeToRemove && howManySimilarVertex (*edgeToRemove, edges, nextExtremityStart) == 1)
			{
				edgeToRemove->visited = true;
				edgeToRemove = edges[0];
			}
			if (!nextExtremityStart && edgeToRemove)
				edgeToRemove->visited = true;
		}
		else if (n == 3 && s1.linked[0] == nullptr && s1.linked[1] == nullptr)
		{
			const int v1 = s1.edges[0][0];
			const int v2 = s1.edges[0][1];

			const float xCenter = s1.points[0].x;
			const float yCenter = s1.points[0].y;

			// les quatre voisins
			bool starts[4] = { true, false, false, false };
			float xs[4] = { s1.points[1].x, 0.f, 0.f, 0.f };
			float ys[4] = { s1.points[1].y, 0.f, 0.f, 0.f };
			for (int k = 0; k < 3; ++k)
			{
				const SliceSegment* e = edges[k];
				if (sameEdge (v1, v2, e->edges[0][0], e->edges[0][1]))
				{
					xs[k + 1] = e->points[1].x; ys[k + 1] = e->points[1].y; starts[k + 1] = true;
				}
				else
				{
					xs[k + 1] = e->points[0].x; ys[k + 1] = e->points[0].y; starts[k + 1] = false;
				}
			}

			// cas ou l'on ne peut pas decider
			int nStartsTrue = 0;
			for (int i = 0; i < 4; i++)
				if (starts[i]) nStartsTrue++;
			if (nStartsTrue != 2)
				continue;

			float angles[4];
			for (int i = 0; i < 4; ++i)
				angles[i] = polarAngle (xCenter, yCenter, xs[i], ys[i]);

			// Le premier segment est un depart. On le relie a l'arrivee qui
			// n'a aucun segment entre elle et lui.
			const int iStart0 = 0;
			int iStart1 = -1;
			int iEnds[2] = { -1, -1 };
			for (int i = 1; i < 4; i++)
			{
				if (starts[i]) iStart1 = i;
				else if (iEnds[0] == -1) iEnds[0] = i;
				else iEnds[1] = i;
			}

			const float minAngle = std::min (std::min (angles[iStart0], angles[iStart1]),
			                                 std::min (angles[iEnds[0]], angles[iEnds[1]]));

			// Indices non signes initialises a -1 (UINT_MAX) : "pas trouve".
			unsigned int iEnd0 = (unsigned int)-1;
			unsigned int iEnd1 = (unsigned int)-1;
			for (int i = 0; i < 2; i++)
			{
				const int iPotentialEnd0 = iEnds[i];
				const int iOther = iEnds[(i + 1) % 2];
				const float aS0 = angles[iStart0] - minAngle;
				const float aS1 = angles[iStart1] - minAngle;
				const float aE  = angles[iPotentialEnd0] - minAngle;
				const float aO  = angles[iOther] - minAngle;
				if (aE > aS0 && (aS1 < aS0 || aS1 > aE) && (aO < aS0 || aO > aE))
				{
					iEnd0 = (unsigned int)iPotentialEnd0;
					iEnd1 = (unsigned int)iOther;
					break;
				}
			}

			if (iEnd0 != (unsigned int)-1 && iEnd1 != (unsigned int)-1)
			{
				// on relie le segment...
				edges[iEnd0 - 1]->linked[1] = &s1;
				s1.linked[0] = edges[iEnd0 - 1];

				// ...et les deux autres
				edges[iEnd1 - 1]->linked[1] = edges[iStart1 - 1];
				edges[iStart1 - 1]->linked[0] = edges[iEnd1 - 1];
			}
		}
	}
	return true;
}

bool SliceLinker::removeZeroLengthIntersections ()
{
	// tous les segments de longueur nulle
	std::vector<uint32_t> zeros;
	for (uint32_t index = 0; index < (uint32_t)m_segments.size (); ++index)
		if (m_segments[index].length2 () < kFltEps)
			zeros.push_back (index);

	if (zeros.empty ())
		return true;

	// fusion des segments nuls entre eux
	for (size_t i = 0; i + 1 < zeros.size (); i++)
	{
		SliceSegment* s1 = &m_segments[zeros[i]];
		const int a1 = s1->edges[0][0], b1 = s1->edges[0][1];
		const int a2 = s1->edges[1][0], b2 = s1->edges[1][1];

		bool mergeDone = false;
		for (size_t j = i + 1; j < zeros.size (); j++)
		{
			SliceSegment* s2 = &m_segments[zeros[j]];
			if (s2->visited) continue;

			const int aa1 = s2->edges[0][0], bb1 = s2->edges[0][1];
			const int aa2 = s2->edges[1][0], bb2 = s2->edges[1][1];

			if (sameEdge (a1, b1, aa1, bb1) && howManySimilarVertex (*s1, true) == 1 &&
			    howManySimilarVertex (*s2, true) == 1)
			{
				s2->visited = true;
				s1->edges[0][0] = aa2; s1->edges[0][1] = bb2;
				mergeDone = true;
			}
			if (sameEdge (a1, b1, aa2, bb2) && howManySimilarVertex (*s1, true) == 1 &&
			    howManySimilarVertex (*s2, false) == 1)
			{
				s2->visited = true;
				s1->edges[0][0] = aa1; s1->edges[0][1] = bb1;
				mergeDone = true;
			}
			if (sameEdge (a2, b2, aa1, bb1) && howManySimilarVertex (*s1, false) == 1 &&
			    howManySimilarVertex (*s2, true) == 1)
			{
				s2->visited = true;
				s1->edges[1][0] = aa2; s1->edges[1][1] = bb2;
				mergeDone = true;
			}
			if (sameEdge (a2, b2, aa2, bb2) && howManySimilarVertex (*s1, false) == 1 &&
			    howManySimilarVertex (*s2, false) == 1)
			{
				s2->visited = true;
				s1->edges[1][0] = aa1; s1->edges[1][1] = bb1;
				mergeDone = true;
			}

			if (mergeDone)
			{
				// s1 a change : sa place dans la table doit suivre
				updateIntoMap (zeros[i]);
				break;
			}
		}
		// Apres une fusion, on repasse sur le MEME s1. A i == 0 le
		// decrement boucle sur SIZE_MAX et l'increment de boucle le ramene a 0
		// (arithmetique non signee, bien definie). La boucle termine : chaque
		// fusion consomme un segment.
		if (mergeDone)
			i--;
	}

	// fusion des segments nuls restants avec les segments non nuls
	for (uint32_t index = 0; index < (uint32_t)m_segments.size (); ++index)
	{
		SliceSegment& s = m_segments[index];
		const int a1 = s.edges[0][0], b1 = s.edges[0][1];
		const int a2 = s.edges[1][0], b2 = s.edges[1][1];

		for (const uint32_t zIndex : zeros)
		{
			SliceSegment* s0 = &m_segments[zIndex];
			if (s0 == &s || s0->visited) continue;

			const int aa1 = s0->edges[0][0], bb1 = s0->edges[0][1];
			const int aa2 = s0->edges[1][0], bb2 = s0->edges[1][1];

			if (sameEdge (a1, b1, aa1, bb1) && howManySimilarVertex (s, true) == 1 &&
			    howManySimilarVertex (*s0, true) == 1)
			{
				s0->visited = true;
				s.edges[0][0] = aa2; s.edges[0][1] = bb2;
				updateIntoMap (index);
			}
			if (sameEdge (a1, b1, aa2, bb2) && howManySimilarVertex (s, true) == 1 &&
			    howManySimilarVertex (*s0, false) == 1)
			{
				s0->visited = true;
				s.edges[0][0] = aa1; s.edges[0][1] = bb1;
				updateIntoMap (index);
			}
			if (sameEdge (a2, b2, aa1, bb1) && howManySimilarVertex (s, false) == 1 &&
			    howManySimilarVertex (*s0, true) == 1)
			{
				s0->visited = true;
				s.edges[1][0] = aa2; s.edges[1][1] = bb2;
				updateIntoMap (index);
			}
			if (sameEdge (a2, b2, aa2, bb2) && howManySimilarVertex (s, false) == 1 &&
			    howManySimilarVertex (*s0, false) == 1)
			{
				s0->visited = true;
				s.edges[1][0] = aa1; s.edges[1][1] = bb1;
				updateIntoMap (index);
			}
		}
	}
	return true;
}

bool SliceLinker::linkByComparingIndices ()
{
	for (SliceSegment& s : m_segments)
	{
		if (s.visited) continue;
		if (s.linked[0] && s.linked[1]) continue;

		const int a1 = s.edges[0][0], b1 = s.edges[0][1];
		const int a2 = s.edges[1][0], b2 = s.edges[1][1];

		const std::array<uint64_t, 2> keys = { edgeKey (a1, b1), edgeKey (a2, b2) };
		for (const uint64_t key : keys)
		{
			const auto bucket = m_map.find (key);
			if (bucket == m_map.end ()) continue;

			for (const uint32_t index : bucket->second)
			{
				SliceSegment* o = &m_segments[index];
				if (o == &s || o->visited) continue;
				if (o->linked[0] && o->linked[1]) continue;

				const int aa1 = o->edges[0][0], bb1 = o->edges[0][1];
				const int aa2 = o->edges[1][0], bb2 = o->edges[1][1];

				// meme segment (deux sommets dans le plan) : pas de lien avec lui-meme
				if ((a1 == b1 && a2 == b2 && aa1 == bb1 && aa2 == bb2) &&
				    ((a1 == aa1 && a2 == aa2) || (a1 == aa2 && a2 == aa1)))
					continue;

				if (sameEdge (a1, b1, aa1, bb1) && s.linked[0] == nullptr && o->linked[0] == nullptr)
				{
					s.linked[0] = o;
					o->linked[0] = &s;
				}
				if (sameEdge (a1, b1, aa2, bb2) && s.linked[0] == nullptr && o->linked[1] == nullptr)
				{
					s.linked[0] = o;
					o->linked[1] = &s;
				}
				if (sameEdge (a2, b2, aa1, bb1) && s.linked[1] == nullptr && o->linked[0] == nullptr)
				{
					s.linked[1] = o;
					o->linked[0] = &s;
				}
				if (sameEdge (a2, b2, aa2, bb2) && s.linked[1] == nullptr && o->linked[1] == nullptr)
				{
					s.linked[1] = o;
					o->linked[1] = &s;
				}
			}
		}
	}
	return true;
}

bool SliceLinker::linkToFillHoles ()
{
	const auto almostEqual = [] (float f1, float f2) { return std::fabs (f1 - f2) < 0.005f; };

	for (auto it = m_segments.begin (); it != m_segments.end (); ++it)
	{
		SliceSegment& s = *it;
		if (s.visited) continue;
		if (s.linked[0] && s.linked[1]) continue;

		const float xStart1 = s.points[0].x, yStart1 = s.points[0].y;
		const float xEnd1 = s.points[1].x, yEnd1 = s.points[1].y;

		for (auto it2 = it + 1; it2 != m_segments.end (); ++it2)
		{
			SliceSegment& o = *it2;
			if (o.visited) continue;
			if (o.linked[0] && o.linked[1]) continue;

			const float xStart2 = o.points[0].x, yStart2 = o.points[0].y;
			const float xEnd2 = o.points[1].x, yEnd2 = o.points[1].y;

			if (s.linked[1] == nullptr && o.linked[0] == nullptr &&
			    almostEqual (xEnd1, xStart2) && almostEqual (yEnd1, yStart2))
			{
				s.linked[1] = &o;
				o.linked[0] = &s;
				s.edges[1][0] = o.edges[0][0];
				s.edges[1][1] = o.edges[0][1];
			}
			if (s.linked[0] == nullptr && o.linked[1] == nullptr &&
			    almostEqual (xStart1, xEnd2) && almostEqual (yStart1, yEnd2))
			{
				s.linked[0] = &o;
				o.linked[1] = &s;
				s.edges[0][0] = o.edges[1][0];
				s.edges[0][1] = o.edges[1][1];
			}
			if (s.linked[0] == nullptr && o.linked[0] == nullptr &&
			    almostEqual (xStart1, xStart2) && almostEqual (yStart1, yStart2))
			{
				s.linked[0] = &o;
				o.linked[0] = &s;
				s.edges[0][0] = o.edges[0][0];
				s.edges[0][1] = o.edges[0][1];
			}
			if (s.linked[1] == nullptr && o.linked[1] == nullptr &&
			    almostEqual (xEnd1, xEnd2) && almostEqual (yEnd1, yEnd2))
			{
				s.linked[1] = &o;
				o.linked[1] = &s;
				s.edges[1][0] = o.edges[1][0];
				s.edges[1][1] = o.edges[1][1];
			}

			if (s.linked[0] && s.linked[1])
				break;
		}
	}
	return true;
}

bool SliceLinker::linkByComparingPoints ()
{
	const auto almostEqual = [] (float f1, float f2) { return std::fabs (f1 - f2) < 0.000001f; };

	for (auto it = m_segments.begin (); it != m_segments.end (); ++it)
	{
		SliceSegment& s = *it;
		if (s.linked[0] && s.linked[1]) continue;

		const float xStart1 = s.points[0].x, yStart1 = s.points[0].y;
		const float xEnd1 = s.points[1].x, yEnd1 = s.points[1].y;

		for (auto it2 = it + 1; it2 != m_segments.end (); ++it2)
		{
			SliceSegment& o = *it2;
			if (o.linked[0] && o.linked[1]) continue;

			const float xStart2 = o.points[0].x, yStart2 = o.points[0].y;
			const float xEnd2 = o.points[1].x, yEnd2 = o.points[1].y;

			if (almostEqual (xEnd1, xStart2) && almostEqual (yEnd1, yStart2))
			{
				s.linked[1] = &o;
				o.linked[0] = &s;
			}
			if (almostEqual (xStart1, xEnd2) && almostEqual (yStart1, yEnd2))
			{
				s.linked[0] = &o;
				o.linked[1] = &s;
			}
			if (almostEqual (xStart1, xStart2) && almostEqual (yStart1, yStart2))
			{
				s.linked[0] = &o;
				o.linked[0] = &s;
			}
			if (almostEqual (xEnd1, xEnd2) && almostEqual (yEnd1, yEnd2))
			{
				s.linked[1] = &o;
				o.linked[1] = &s;
			}

			if (s.linked[0] && s.linked[1])
				break;
		}
	}
	return true;
}

bool traceChains (std::vector<SliceSegment>& segments,
                    std::vector<std::vector<Vector3f>>& paths,
                    std::vector<std::vector<int>>& faces,
                    std::vector<bool>* closed)
{
	for (SliceSegment& start : segments)
	{
		if (start.visited || !start.valid)
			continue;

		std::vector<Vector3f> path;
		path.push_back (start.points[0]);
		path.push_back (start.points[1]);
		std::vector<int> currentFaces;
		currentFaces.push_back (start.faceId);
		start.visited = true;

		// d'un cote...
		SliceSegment* s = start.linked[1];
		while (s)
		{
			// Teste start.valid, et non s->valid (voir l'en-tete).
			if (s->visited || !start.valid) break;
			if (!s->linked[0]) break;
			if (!s->linked[1]) break;

			const int next = (!s->linked[0]->visited ||
			                  (s->linked[1]->visited && s->linked[0] == &start)) ? 0 : 1;

			path.push_back (s->points[next]);
			currentFaces.push_back (s->faceId);
			s->visited = true;
			s = s->linked[next];
		}

		// (D8) Fermeture TOPOLOGIQUE : la marche avant est revenue sur le
		// segment de depart, ET la marche arriere n'ajoute rien (verifie plus
		// bas). La seconde condition n'est pas redondante : avec des liens
		// ASYMETRIQUES -- la branche n == 3 de removeNonManifoldVertices en
		// ecrase --, un segment peut pointer vers start sans que
		// start.linked[0] pointe vers lui, et la marche arriere prolonge alors
		// le chemin par sa tete.
		const bool returnedToStart = (s == &start);
		const size_t sizeBeforeBackward = path.size ();

		// ...puis de l'autre
		s = start.linked[0];
		while (s)
		{
			if (s->visited || !start.valid) break;
			if (!s->linked[0]) break;

			const int next = (s->linked[0]->visited) ? 1 : 0;
			path.insert (path.begin (), s->points[next]);
			currentFaces.push_back (s->faceId);   // en queue, non en tete (voir l'en-tete)
			s->visited = true;
			s = s->linked[next];
		}

		const bool isClosed = returnedToStart && path.size () == sizeBeforeBackward;
		paths.push_back (std::move (path));
		faces.push_back (std::move (currentFaces));
		if (closed) closed->push_back (isClosed);
	}
	return true;
}

} // namespace slicing
} // namespace cgmesh
