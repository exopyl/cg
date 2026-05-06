#pragma once

#include <vector>

class Che_mesh;

//
// Walk the 1-ring of vertex `v`, returning the (unique) neighbor vertices and
// flagging boundary topology.
//
// For an interior manifold vertex : `neighbors` lists the n vertices around v
// in walk order ; `is_boundary` is false ; `bnd_left`/`bnd_right` are -1.
//
// For a boundary manifold vertex : `neighbors` lists all neighbors ; `is_boundary`
// is true ; `bnd_left` and `bnd_right` are the two neighbors connected to v by
// boundary half-edges (i.e. the endpoints of v's two incident boundary edges).
//
// For an isolated vertex (no incident edge) : `neighbors` is empty.
//
// Used by Loop and √3 subdivision schemes for vertex smoothing stencils.
//
void collectVertexRing (Che_mesh *che, int v,
                        std::vector<int> &neighbors,
                        bool &is_boundary,
                        int &bnd_left, int &bnd_right);
