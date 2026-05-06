#include "subdivision_common.h"

#include "half_edge.h"

void
collectVertexRing (Che_mesh *che, int v,
                   std::vector<int> &neighbors,
                   bool &is_boundary,
                   int &bnd_left, int &bnd_right)
{
	neighbors.clear ();
	is_boundary = false;
	bnd_left = -1;
	bnd_right = -1;

	int e0 = che->m_edges_vertex[v];
	if (e0 < 0) return;

	// Forward walk : e -> next.next.pair
	int e = e0;
	bool hit_forward = false;
	int forward_third = -1;
	do
	{
		neighbors.push_back (che->edge(e).m_v_end);
		int n1 = che->edge(e).m_he_next;
		int n2 = che->edge(n1).m_he_next;   // n2 ends at v
		int p  = che->edge(n2).m_pair;
		if (p < 0)
		{
			hit_forward = true;
			forward_third = che->edge(n2).m_v_begin;
			break;
		}
		e = p;
	} while (e != e0);

	if (!hit_forward)
		return;

	is_boundary = true;
	neighbors.push_back (forward_third);
	bnd_right = forward_third;

	// Backward walk : e -> pair(e).next
	int e_back = e0;
	while (true)
	{
		int p_back = che->edge(e_back).m_pair;
		if (p_back < 0)
		{
			bnd_left = che->edge(e_back).m_v_end;
			break;
		}
		int prev_e = che->edge(p_back).m_he_next;
		int prev_neighbor = che->edge(prev_e).m_v_end;
		neighbors.insert (neighbors.begin(), prev_neighbor);
		e_back = prev_e;
	}
}
