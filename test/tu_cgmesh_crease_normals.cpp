#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_crease_normals.h"

// ============================================================================
//  Normales a seuil d'angle (mesh_crease_normals.h)
// ============================================================================
//
// Les normales sont comparees avec une tolerance : elles sortent d'un calcul en
// double puis d'un arrondi en float, et la CI Linux (GCC -O2) contracte les
// produits en FMA. Aucune egalite au bit pres n'est attendue ici, sauf pour la
// recopie a l'identique du maillage source.

namespace {

constexpr float kTol = 1e-5f;
constexpr double kPi = 3.14159265358979323846;

struct V3
{
	double x, y, z;
};

V3 Normalized (V3 v)
{
	const double l = std::sqrt (v.x * v.x + v.y * v.y + v.z * v.z);
	return { v.x / l, v.y / l, v.z / l };
}

double AngleDeg (V3 a, V3 b)
{
	a = Normalized (a);
	b = Normalized (b);
	double d = a.x * b.x + a.y * b.y + a.z * b.z;
	d = d > 1.0 ? 1.0 : (d < -1.0 ? -1.0 : d);
	return std::acos (d) * 180.0 / kPi;
}

V3 Position (const Mesh &m, unsigned int v)
{
	const std::vector<float> &p = m.GetVertices ();
	return { p[3 * v], p[3 * v + 1], p[3 * v + 2] };
}

// Normale geometrique d'un triangle du maillage.
V3 TriangleNormal (const Mesh &m, unsigned int f)
{
	const V3 a = Position (m, (unsigned int)m.GetFaceVertex (f, 0));
	const V3 b = Position (m, (unsigned int)m.GetFaceVertex (f, 1));
	const V3 c = Position (m, (unsigned int)m.GetFaceVertex (f, 2));
	const V3 u { b.x - a.x, b.y - a.y, b.z - a.z };
	const V3 w { c.x - a.x, c.y - a.y, c.z - a.z };
	return Normalized ({ u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x });
}

V3 CornerNormal (const CreaseNormals &cn, unsigned int corner)
{
	return { cn.cornerNormals[3 * corner], cn.cornerNormals[3 * corner + 1], cn.cornerNormals[3 * corner + 2] };
}

// Cube unite, 12 triangles orientes vers l'exterieur.
void MakeCube (Mesh &m)
{
	m.Init (8, 12);
	const float v[24] = {
		0.f, 0.f, 0.f,  1.f, 0.f, 0.f,  1.f, 1.f, 0.f,  0.f, 1.f, 0.f,
		0.f, 0.f, 1.f,  1.f, 0.f, 1.f,  1.f, 1.f, 1.f,  0.f, 1.f, 1.f,
	};
	m.SetVertices (8, v);
	const unsigned int tris[36] = {
		0,2,1, 0,3,2,   // z-
		4,5,6, 4,6,7,   // z+
		0,1,5, 0,5,4,   // y-
		2,3,7, 2,7,6,   // y+
		1,2,6, 1,6,5,   // x+
		0,4,7, 0,7,3,   // x-
	};
	for (unsigned int i = 0; i < 12; i++)
		m.SetFace (i, tris[3 * i], tris[3 * i + 1], tris[3 * i + 2]);
}

// Cylindre facette d'axe Z, de rayon 1 et de hauteur 2, a `segments` facettes.
// Chaque facette du flanc est un quad coupe en deux triangles par la MEME
// diagonale : un sommet du bord touche deux triangles d'une facette et un seul
// de l'autre, ce qui distingue une moyenne ponderee par les angles d'une
// moyenne simple. Les disques sont des eventails autour d'un centre.
//
// Sommets : 0..s-1 bas, s..2s-1 haut, 2s centre bas, 2s+1 centre haut.
void MakeCylinder (Mesh &m, unsigned int segments)
{
	const unsigned int s = segments;
	m.Init (2 * s + 2, 4 * s);
	for (unsigned int i = 0; i < s; ++i)
	{
		const double a = 2.0 * kPi * i / s;
		m.SetVertex (i,     (float)std::cos (a), (float)std::sin (a), 0.f);
		m.SetVertex (s + i, (float)std::cos (a), (float)std::sin (a), 2.f);
	}
	m.SetVertex (2 * s,     0.f, 0.f, 0.f);
	m.SetVertex (2 * s + 1, 0.f, 0.f, 2.f);

	unsigned int f = 0;
	for (unsigned int i = 0; i < s; ++i)
	{
		const unsigned int j = (i + 1) % s;
		m.SetFace (f++, i, j, s + j);           // flanc
		m.SetFace (f++, i, s + j, s + i);
		m.SetFace (f++, 2 * s, j, i);           // disque bas, normale -Z
		m.SetFace (f++, 2 * s + 1, s + i, s + j); // disque haut, normale +Z
	}
}

// Prisme triangulaire du comble de l'Hephaisteion : deux pans de pente
// 1 / 4,0846 qui se rejoignent au faitage, un fond horizontal, deux abouts
// verticaux. Section dans le plan XZ, extrusion le long de Y.
//   faitage / pans : 2 * atan (1 / 4,0846) = 27,5 degres
//   pan / about    : 90 degres
//   pan / fond     : 166,2 degres
void MakeComblePrism (Mesh &m)
{
	const float halfWidth = 7.2365f;
	const float height = halfWidth / 4.0846f;
	const float length = 10.f;
	m.Init (6, 8);
	m.SetVertex (0, -halfWidth, 0.f, 0.f);
	m.SetVertex (1,  halfWidth, 0.f, 0.f);
	m.SetVertex (2,  0.f,       0.f, height);
	m.SetVertex (3, -halfWidth, length, 0.f);
	m.SetVertex (4,  halfWidth, length, 0.f);
	m.SetVertex (5,  0.f,       length, height);
	m.SetFace (0, 0, 1, 2);   // about y = 0, normale -Y
	m.SetFace (1, 3, 5, 4);   // about y = L, normale +Y
	m.SetFace (2, 1, 4, 5);   // pan +X
	m.SetFace (3, 1, 5, 2);
	m.SetFace (4, 0, 2, 5);   // pan -X
	m.SetFace (5, 0, 5, 3);
	m.SetFace (6, 0, 3, 4);   // fond, normale -Z
	m.SetFace (7, 0, 4, 1);
}

bool IsAxial (V3 n)
{
	const double ax = std::fabs (n.x), ay = std::fabs (n.y), az = std::fabs (n.z);
	const int ones = (std::fabs (ax - 1.0) < kTol) + (std::fabs (ay - 1.0) < kTol) + (std::fabs (az - 1.0) < kTol);
	const int zeros = (ax < kTol) + (ay < kTol) + (az < kTol);
	return ones == 1 && zeros == 2;
}

// Sphere UV de rayon 1 : nu meridiens, nv bandes, deux poles en eventail. A
// 18 x 9 (pas de 20 degres), l'angle entre faces voisines plafonne a 20,2
// degres, sous le seuil par defaut, alors que les faces d'un meme eventail
// font entre elles jusqu'a 40 degres.
void MakeUVSphere (Mesh &m, int nu, int nv)
{
	m.Init ((unsigned int)(nu * (nv - 1) + 2), (unsigned int)(2 * nu * (nv - 2) + 2 * nu));
	unsigned int k = 0;
	for (int j = 1; j < nv; ++j)
		for (int i = 0; i < nu; ++i)
		{
			const double t = kPi * j / nv, p = 2 * kPi * i / nu;
			m.SetVertex (k++, (float)(std::sin (t) * std::cos (p)), (float)(std::sin (t) * std::sin (p)), (float)std::cos (t));
		}
	const unsigned int north = k, south = k + 1;
	m.SetVertex (north, 0.f, 0.f, 1.f);
	m.SetVertex (south, 0.f, 0.f, -1.f);
	auto id = [nu] (int j, int i) { return (unsigned int)((j - 1) * nu + (i % nu)); };
	unsigned int f = 0;
	for (int i = 0; i < nu; ++i)
	{
		m.SetFace (f++, north, id (1, i), id (1, i + 1));
		m.SetFace (f++, south, id (nv - 1, i + 1), id (nv - 1, i));
	}
	for (int j = 1; j < nv - 1; ++j)
		for (int i = 0; i < nu; ++i)
		{
			m.SetFace (f++, id (j, i), id (j + 1, i), id (j + 1, i + 1));
			m.SetFace (f++, id (j, i), id (j + 1, i + 1), id (j, i + 1));
		}
}

// Cylindre facette en QUADS (flanc) et polygones (disques), sans triangle.
// Sommets : 0..s-1 bas, s..2s-1 haut.
void MakeQuadCylinder (Mesh &m, unsigned int s)
{
	m.Init (2 * s, s + 2);
	for (unsigned int i = 0; i < s; ++i)
	{
		const double a = 2.0 * kPi * i / s;
		m.SetVertex (i,     (float)std::cos (a), (float)std::sin (a), 0.f);
		m.SetVertex (s + i, (float)std::cos (a), (float)std::sin (a), 2.f);
	}
	for (unsigned int i = 0; i < s; ++i)
	{
		const unsigned int j = (i + 1) % s;
		auto face = m.FaceAt (i);
		face->SetNVertices (4);
		face->SetVertex (0, i);
		face->SetVertex (1, j);
		face->SetVertex (2, s + j);
		face->SetVertex (3, s + i);
	}
	auto bottom = m.FaceAt (s);
	auto top = m.FaceAt (s + 1);
	bottom->SetNVertices (s);
	top->SetNVertices (s);
	for (unsigned int i = 0; i < s; ++i)
	{
		bottom->SetVertex (i, s - 1 - i);   // normale -Z
		top->SetVertex (i, s + i);          // normale +Z
	}
}

V3 RenderNormal (const Mesh::PolygonRenderData &rd, size_t k)
{
	return { rd.normals[3 * k], rd.normals[3 * k + 1], rd.normals[3 * k + 2] };
}

} // namespace

// ----------------------------------------------------------------------------
//  Cube : toutes les aretes a 90 degres, donc 24 normales de coin axiales
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, cube_corners_take_their_face_normal)
{
	Mesh m;
	MakeCube (m);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	ASSERT_EQ (cn.cornerOffsets.size (), 13u);
	ASSERT_EQ (cn.GetNCorners (), 36u);

	for (unsigned int f = 0; f < 12; ++f)
	{
		const V3 fn = TriangleNormal (m, f);
		for (unsigned int i = 0; i < 3; ++i)
		{
			const V3 n = CornerNormal (cn, cn.cornerOffsets[f] + i);
			EXPECT_TRUE (IsAxial (n)) << "face " << f << " coin " << i;
			EXPECT_LT (AngleDeg (n, fn), 1e-3) << "face " << f << " coin " << i;
		}
	}
}

TEST (TEST_cgmesh_crease_normals, cube_render_data_has_24_axial_vertices)
{
	Mesh m;
	MakeCube (m);

	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	ASSERT_EQ (rd.positions.size (), 24u * 3u) << "8 sommets x 3 faces";
	ASSERT_EQ (rd.normals.size (), rd.positions.size ());
	EXPECT_EQ (rd.indices.size (), 36u);
	for (unsigned int idx : rd.indices)
		EXPECT_LT (idx, 24u);

	for (unsigned int k = 0; k < 24; ++k)
		EXPECT_TRUE (IsAxial ({ rd.normals[3 * k], rd.normals[3 * k + 1], rd.normals[3 * k + 2] }))
			<< "sommet de rendu " << k;
}

TEST (TEST_cgmesh_crease_normals, cube_split_mesh_has_24_vertices_with_axial_normals)
{
	Mesh m;
	MakeCube (m);

	const Mesh s = BuildCreaseSplitMesh (m, kDefaultCreaseAngleDeg);
	ASSERT_EQ (s.GetNVertices (), 24u);
	ASSERT_EQ (s.GetNFaces (), 12u);
	ASSERT_EQ (s.GetVertexNormals ().size (), 24u * 3u);

	// Les 8 premiers sommets sont ceux de la source, dans le meme ordre.
	for (unsigned int k = 0; k < 8 * 3; ++k)
		EXPECT_EQ (s.GetVertices ()[k], m.GetVertices ()[k]);

	const std::vector<float> &n = s.GetVertexNormals ();
	for (unsigned int v = 0; v < 24; ++v)
		EXPECT_TRUE (IsAxial ({ n[3 * v], n[3 * v + 1], n[3 * v + 2] })) << "sommet " << v;

	// Chaque coin pointe un sommet dont la normale est celle de sa face.
	for (unsigned int f = 0; f < 12; ++f)
	{
		const V3 fn = TriangleNormal (s, f);
		for (unsigned int i = 0; i < 3; ++i)
		{
			const unsigned int v = (unsigned int)s.GetFaceVertex (f, i);
			EXPECT_LT (AngleDeg ({ n[3 * v], n[3 * v + 1], n[3 * v + 2] }, fn), 1e-3)
				<< "face " << f << " coin " << i;
		}
	}
}

// ----------------------------------------------------------------------------
//  Cylindre facette : flanc lisse, bord des disques vif
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, cylinder_flank_is_smooth_and_disc_rims_are_sharp)
{
	// 24 facettes : 15 degres entre facettes voisines, sous le seuil par defaut.
	const unsigned int s = 24;
	Mesh m;
	MakeCylinder (m, s);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	ASSERT_EQ (cn.GetNCorners (), 3u * 4u * s);

	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		const bool flank = (f % 4) < 2;
		for (unsigned int i = 0; i < 3; ++i)
		{
			const unsigned int v = (unsigned int)m.GetFaceVertex (f, i);
			const V3 n = CornerNormal (cn, cn.cornerOffsets[f] + i);
			if (flank)
			{
				// Normale radiale exacte : ponderation par les angles de coin.
				const V3 p = Position (m, v);
				const V3 radial = Normalized ({ p.x, p.y, 0.0 });
				EXPECT_NEAR (n.x, radial.x, kTol) << "face " << f << " coin " << i;
				EXPECT_NEAR (n.y, radial.y, kTol) << "face " << f << " coin " << i;
				EXPECT_NEAR (n.z, 0.0, kTol) << "face " << f << " coin " << i;
			}
			else
			{
				const double z = (f % 4) == 2 ? -1.0 : 1.0;
				EXPECT_NEAR (n.x, 0.0, kTol) << "face " << f << " coin " << i;
				EXPECT_NEAR (n.y, 0.0, kTol) << "face " << f << " coin " << i;
				EXPECT_NEAR (n.z, z, kTol) << "face " << f << " coin " << i;
			}
		}
	}
}

TEST (TEST_cgmesh_crease_normals, cylinder_render_data_shares_flank_vertices)
{
	const unsigned int s = 24;
	Mesh m;
	MakeCylinder (m, s);

	// Flanc lisse : un sommet de rendu par sommet du bord. Disques : un par
	// sommet du bord, plus le centre. Soit 2s + 2s + 2.
	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	EXPECT_EQ (rd.positions.size () / 3, (size_t)(4 * s + 2));
	EXPECT_EQ (rd.normals.size (), rd.positions.size ());
	EXPECT_EQ (rd.indices.size (), (size_t)(3 * 4 * s));
}

TEST (TEST_cgmesh_crease_normals, coarse_cylinder_is_faceted_below_its_facet_angle)
{
	// 12 facettes : 30 degres entre facettes, au-dessus du seuil par defaut --
	// chaque coin du flanc garde la normale de sa facette.
	const unsigned int s = 12;
	Mesh m;
	MakeCylinder (m, s);

	const CreaseNormals sharp = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	const CreaseNormals smooth = ComputeCreaseNormals (m, 45.f);
	for (unsigned int f = 0; f < m.GetNFaces (); f += 4)
	{
		const V3 fn = TriangleNormal (m, f);
		for (unsigned int i = 0; i < 3; ++i)
		{
			EXPECT_LT (AngleDeg (CornerNormal (sharp, sharp.cornerOffsets[f] + i), fn), 1e-3);
			// Au-dessus de 30 degres, le flanc se lisse : 15 degres d'ecart a la
			// facette, la moitie de l'angle entre facettes.
			EXPECT_NEAR (AngleDeg (CornerNormal (smooth, smooth.cornerOffsets[f] + i), fn), 15.0, 1e-3);
		}
	}
}

// ----------------------------------------------------------------------------
//  Comble : un prisme triangulaire, aucune normale de coin au-dela du seuil
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, comble_prism_has_no_corner_normal_beyond_the_threshold)
{
	Mesh m;
	MakeComblePrism (m);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	ASSERT_EQ (cn.GetNCorners (), 24u);
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		const V3 fn = TriangleNormal (m, f);
		for (unsigned int i = 0; i < 3; ++i)
			EXPECT_LT (AngleDeg (CornerNormal (cn, cn.cornerOffsets[f] + i), fn), kDefaultCreaseAngleDeg)
				<< "face " << f << " coin " << i;
	}
}

TEST (TEST_cgmesh_crease_normals, comble_prism_ridge_and_ends_stay_sharp)
{
	// Toutes les aretes du prisme depassent le seuil, faitage compris (27,5
	// degres) : chaque coin porte exactement la normale de sa face, et un pan
	// est ombre uniformement.
	Mesh m;
	MakeComblePrism (m);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		const V3 fn = TriangleNormal (m, f);
		for (unsigned int i = 0; i < 3; ++i)
			EXPECT_LT (AngleDeg (CornerNormal (cn, cn.cornerOffsets[f] + i), fn), 1e-3)
				<< "face " << f << " coin " << i;
	}

	// Rendu : 5 faces planes, 3 + 3 + 4 + 4 + 4 sommets de rendu.
	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	EXPECT_EQ (rd.positions.size () / 3, 18u);
}

// ----------------------------------------------------------------------------
//  Le maillage source n'est pas modifie
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, source_mesh_is_unchanged)
{
	Mesh m;
	MakeCube (m);
	m.ComputeNormals ();

	const std::vector<float> vertices = m.GetVertices ();
	const std::vector<float> normals = m.GetVertexNormals ();
	const std::vector<float> faceNormals = m.GetFaceNormals ();
	std::vector<int> faces;
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		for (unsigned int i = 0; i < 3; ++i)
			faces.push_back (m.GetFaceVertex (f, i));
	const unsigned int nv = m.GetNVertices ();
	const unsigned int nf = m.GetNFaces ();
	const uint64_t revision = m.GetRevision ();

	(void)ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	(void)m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	const Mesh split = BuildCreaseSplitMesh (m, kDefaultCreaseAngleDeg);
	ASSERT_EQ (split.GetNVertices (), 24u);

	EXPECT_EQ (m.GetNVertices (), nv);
	EXPECT_EQ (m.GetNFaces (), nf);
	EXPECT_EQ (m.GetRevision (), revision);
	EXPECT_EQ (m.GetVertices (), vertices);
	EXPECT_EQ (m.GetVertexNormals (), normals);
	EXPECT_EQ (m.GetFaceNormals (), faceNormals);
	for (unsigned int f = 0, k = 0; f < nf; ++f)
		for (unsigned int i = 0; i < 3; ++i, ++k)
			EXPECT_EQ (m.GetFaceVertex (f, i), faces[k]);
}

// ----------------------------------------------------------------------------
//  Dedoublement : contrat de SplitVerticesByCornerNormals
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, split_rejects_a_corner_array_of_the_wrong_size)
{
	Mesh m;
	MakeCube (m);
	const unsigned int nv = m.GetNVertices ();
	const uint64_t revision = m.GetRevision ();

	EXPECT_EQ (m.SplitVerticesByCornerNormals (std::vector<float> (3 * 35, 0.f)), -1);
	EXPECT_EQ (m.GetNVertices (), nv);
	EXPECT_EQ (m.GetRevision (), revision);
}

TEST (TEST_cgmesh_crease_normals, split_keeps_an_isolated_vertex_at_its_index)
{
	// Un sommet sans face (indice 8) garde son indice : les points et les
	// lignes, qui designent des sommets par indice, restent valides.
	Mesh m;
	MakeCube (m);
	Mesh withPoint;
	withPoint.Init (9, 12);
	std::vector<float> v (m.GetVertices ());
	v.insert (v.end (), { 5.f, 5.f, 5.f });
	withPoint.SetVertices (9, v.data ());
	for (unsigned int f = 0; f < 12; ++f)
		withPoint.SetFace (f, (unsigned int)m.GetFaceVertex (f, 0), (unsigned int)m.GetFaceVertex (f, 1),
		                   (unsigned int)m.GetFaceVertex (f, 2));

	const Mesh s = BuildCreaseSplitMesh (withPoint, kDefaultCreaseAngleDeg);
	ASSERT_EQ (s.GetNVertices (), 25u);
	EXPECT_EQ (s.GetVertices ()[3 * 8], 5.f);
	EXPECT_EQ (s.GetVertices ()[3 * 8 + 1], 5.f);
	EXPECT_EQ (s.GetVertices ()[3 * 8 + 2], 5.f);
	// Sommet sans coin, de normale source nulle (Init les met a zero) : (0, 0, 1).
	EXPECT_NEAR (s.GetVertexNormals ()[3 * 8 + 2], 1.f, kTol);
}

// ----------------------------------------------------------------------------
//  Regroupement par aretes : une surface lisse garde une normale par sommet
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, uv_sphere_keeps_one_normal_per_vertex)
{
	// Chaque arete est sous le seuil, mais les faces d'un eventail font entre
	// elles jusqu'a 40 degres : un critere par paire de faces (et non par
	// arete) donnerait plusieurs normales a un meme sommet.
	Mesh m;
	MakeUVSphere (m, 18, 9);
	ASSERT_EQ (m.GetNVertices (), 146u);

	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	EXPECT_EQ (rd.positions.size () / 3, 146u);

	const Mesh s = BuildCreaseSplitMesh (m, kDefaultCreaseAngleDeg);
	EXPECT_EQ (s.GetNVertices (), 146u);

	// Normale proche du rayon : la triangulation brise la symetrie, d'ou la
	// marge, mais un ecart de plusieurs degres trahirait un regroupement faux.
	const std::vector<float> &n = s.GetVertexNormals ();
	for (unsigned int v = 0; v < s.GetNVertices (); ++v)
		EXPECT_LT (AngleDeg ({ n[3 * v], n[3 * v + 1], n[3 * v + 2] }, Position (s, v)), 2.0) << "sommet " << v;
}

TEST (TEST_cgmesh_crease_normals, uv_sphere_is_faceted_below_its_edge_angle)
{
	// A 1 degre, seules restent reliees les deux moities planes de chaque quad
	// (trapeze isocele) : chaque coin porte la normale de sa face.
	Mesh m;
	MakeUVSphere (m, 18, 9);
	const CreaseNormals cn = ComputeCreaseNormals (m, 1.f);
	double worst = 0.0;
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
		for (unsigned int i = 0; i < 3; ++i)
			worst = std::max (worst, AngleDeg (CornerNormal (cn, cn.cornerOffsets[f] + i), TriangleNormal (m, f)));
	EXPECT_LT (worst, 1e-3);
}

// ----------------------------------------------------------------------------
//  Polygones : le maillage dedouble est triangule
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, split_mesh_of_quads_is_triangulated_and_keeps_smooth_normals)
{
	// Flanc en quads, 24 facettes (15 degres) : lisse. Les polygones sont rendus
	// a plat par BuildPolygonRenderData -- donc par l'export glTF -- : seule une
	// triangulation prealable laisse passer les normales par sommet.
	const unsigned int s = 24;
	Mesh m;
	MakeQuadCylinder (m, s);

	const Mesh split = BuildCreaseSplitMesh (m, kDefaultCreaseAngleDeg);
	ASSERT_TRUE (split.IsTriangleMesh ());
	EXPECT_EQ (split.GetNFaces (), 2 * s + 2 * (s - 2));
	// Flanc : un sommet par sommet du bord ; disques : un de plus chacun.
	EXPECT_EQ (split.GetNVertices (), 4 * s);

	const Mesh::PolygonRenderData rd = split.BuildPolygonRenderData (false);
	for (unsigned int f = 0; f < split.GetNFaces (); ++f)
	{
		const V3 fn = TriangleNormal (split, f);
		if (std::fabs (fn.z) > 0.5)
			continue;   // disque
		for (unsigned int i = 0; i < 3; ++i)
		{
			const unsigned int v = (unsigned int)split.GetFaceVertex (f, i);
			const V3 p = Position (split, v);
			const V3 radial = Normalized ({ p.x, p.y, 0.0 });
			const V3 n = RenderNormal (rd, v);
			EXPECT_NEAR (n.x, radial.x, kTol) << "face " << f << " coin " << i;
			EXPECT_NEAR (n.y, radial.y, kTol) << "face " << f << " coin " << i;
			EXPECT_NEAR (n.z, 0.0, kTol) << "face " << f << " coin " << i;
		}
	}
}

TEST (TEST_cgmesh_crease_normals, reflex_corner_of_a_concave_polygon_weighs_its_interior_angle)
{
	// Hexagone en L dans le plan z = 0, normale +Z, de coin rentrant (1, 1)
	// d'angle interieur 270 degres. Un triangle incline de 10 degres partage
	// avec lui l'arete (1, 1)-(2, 1) : ils forment un groupe.
	Mesh m;
	m.Init (7, 2);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 2.f, 0.f, 0.f);
	m.SetVertex (2, 2.f, 1.f, 0.f);
	m.SetVertex (3, 1.f, 1.f, 0.f);
	m.SetVertex (4, 1.f, 2.f, 0.f);
	m.SetVertex (5, 0.f, 2.f, 0.f);
	const float tilt = (float)std::tan (10.0 * kPi / 180.0);
	m.SetVertex (6, 1.5f, 2.f, tilt);
	auto hex = m.FaceAt (0);
	hex->SetNVertices (6);
	for (unsigned int i = 0; i < 6; ++i)
		hex->SetVertex (i, i);
	m.SetFace (1, 3, 2, 6);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	const V3 nHex { 0.0, 0.0, 1.0 };
	const V3 nTri = TriangleNormal (m, 1);
	// Angle du triangle en (1, 1), entre (2, 1, 0) et (1,5, 2, tilt).
	const V3 a = Normalized ({ 1.0, 0.0, 0.0 });
	const V3 b = Normalized ({ 0.5, 1.0, tilt });
	const double wTri = std::acos (a.x * b.x + a.y * b.y + a.z * b.z);
	const double wHex = 1.5 * kPi;
	const V3 expected = Normalized ({ wHex * nHex.x + wTri * nTri.x, wHex * nHex.y + wTri * nTri.y,
	                                  wHex * nHex.z + wTri * nTri.z });
	const V3 got = CornerNormal (cn, cn.cornerOffsets[0] + 3);
	EXPECT_NEAR (got.x, expected.x, kTol);
	EXPECT_NEAR (got.y, expected.y, kTol);
	EXPECT_NEAR (got.z, expected.z, kTol);
}

// ----------------------------------------------------------------------------
//  Cas degeneres
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, degenerate_face_takes_the_mean_of_its_valid_neighbours)
{
	// Carre plat de deux triangles, plus un triangle en aiguille (sommets
	// presque alignes : hauteur 1e-7 pour une base de 2,8) sur sa diagonale.
	Mesh m;
	m.Init (5, 3);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 1.f, 1.f, 0.f);
	m.SetVertex (3, 0.f, 1.f, 0.f);
	m.SetVertex (4, 2.f, 2.f, 1e-7f);   // presque aligne avec 0 et 2
	m.SetFace (0, 0, 1, 2);
	m.SetFace (1, 0, 2, 3);
	m.SetFace (2, 0, 2, 4);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	for (unsigned int c = 0; c < cn.GetNCorners (); ++c)
	{
		const V3 n = CornerNormal (cn, c);
		EXPECT_TRUE (std::isfinite (n.x) && std::isfinite (n.y) && std::isfinite (n.z)) << "coin " << c;
		// Faces valides : +Z. Face degeneree : moyenne des faces valides du
		// sommet (+Z), ou (0, 0, 1) au sommet 4, qui n'en a aucune.
		EXPECT_NEAR (n.z, 1.0, kTol) << "coin " << c;
	}
	// Le triangle degenere ne coupe pas le carre : ses deux triangles gardent
	// une normale par sommet.
	EXPECT_EQ (m.BuildCreaseRenderData (kDefaultCreaseAngleDeg).positions.size () / 3, 5u);
}

TEST (TEST_cgmesh_crease_normals, removed_face_has_no_corner)
{
	Mesh m;
	MakeCube (m);
	m.RemoveFace (0);

	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	EXPECT_EQ (cn.cornerOffsets[0], cn.cornerOffsets[1]);
	EXPECT_EQ (cn.GetNCorners (), 33u);
	for (unsigned int f = 1; f < 12; ++f)
		for (unsigned int i = 0; i < 3; ++i)
			EXPECT_TRUE (IsAxial (CornerNormal (cn, cn.cornerOffsets[f] + i))) << "face " << f;

	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	EXPECT_EQ (rd.indices.size (), 33u);
	EXPECT_EQ (rd.positions.size () / 3, 24u - 1u) << "le coin (0, 0, 0) de la face -Z a disparu";
}

TEST (TEST_cgmesh_crease_normals, non_planar_quad_gets_unit_corner_normals)
{
	Mesh m;
	m.Init (4, 1);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 1.f, 1.f, 0.3f);
	m.SetVertex (3, 0.f, 1.f, 0.f);
	auto q = m.FaceAt (0);
	q->SetNVertices (4);
	for (unsigned int i = 0; i < 4; ++i)
		q->SetVertex (i, i);

	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	ASSERT_EQ (rd.positions.size () / 3, 4u);
	EXPECT_EQ (rd.indices.size (), 6u);
	for (size_t k = 0; k < 4; ++k)
	{
		const V3 n = RenderNormal (rd, k);
		EXPECT_NEAR (std::sqrt (n.x * n.x + n.y * n.y + n.z * n.z), 1.0, kTol);
		EXPECT_GT (n.z, 0.9);
	}

	const Mesh s = BuildCreaseSplitMesh (m, kDefaultCreaseAngleDeg);
	EXPECT_EQ (s.GetNFaces (), 2u);
	EXPECT_TRUE (s.IsTriangleMesh ());
}

// ----------------------------------------------------------------------------
//  Attributs
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, render_data_separates_corners_with_different_uv_indices)
{
	// Carre plat de deux triangles. Les coins de la diagonale portent, dans la
	// seconde face, d'autres indices d'UV : une couture.
	Mesh m;
	m.Init (4, 2);
	m.SetVertex (0, 0.f, 0.f, 0.f);
	m.SetVertex (1, 1.f, 0.f, 0.f);
	m.SetVertex (2, 1.f, 1.f, 0.f);
	m.SetVertex (3, 0.f, 1.f, 0.f);
	m.SetFace (0, 0, 1, 2);
	m.SetFace (1, 0, 2, 3);
	m.SetTextureCoordinates ({ 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f, 0.5f, 0.f, 0.5f, 0.5f }, 6);
	const unsigned int uv[2][3] = { { 0, 1, 2 }, { 4, 5, 3 } };
	for (unsigned int f = 0; f < 2; ++f)
	{
		auto face = m.FaceAt (f);
		face->ActivateTextureCoordinatesIndices ();
		face->SetUsesTextureCoordinates (true);
		for (unsigned int i = 0; i < 3; ++i)
			face->SetTexCoord (i, uv[f][i]);
	}

	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	EXPECT_EQ (rd.positions.size () / 3, 6u) << "4 sommets, dont 2 coupes par la couture";
	ASSERT_EQ (rd.texCoords.size (), 12u);
	// Le coin 0 de la face 1 porte l'UV 4 : (0,5, 0).
	const unsigned int k = rd.indices[3];
	EXPECT_EQ (rd.texCoords[2 * k], 0.5f);
	EXPECT_EQ (rd.texCoords[2 * k + 1], 0.f);
}

TEST (TEST_cgmesh_crease_normals, split_copies_vertex_attributes_to_the_duplicates)
{
	Mesh m;
	MakeCube (m);
	const unsigned int nv = 8;
	std::vector<float> uv0, uv1, tangents;
	m.InitTensors ();
	for (unsigned int v = 0; v < nv; ++v)
	{
		m.SetVertexColor (v, 0.1f * v, 0.2f, 0.3f);
		uv0.insert (uv0.end (), { 0.1f * v, 0.5f });
		uv1.insert (uv1.end (), { 0.5f, 0.1f * v });
		const V3 t = Normalized ({ 1.0, 2.0, 3.0 });
		tangents.insert (tangents.end (), { (float)t.x, (float)t.y, (float)t.z, v % 2 ? -1.f : 1.f });
		Tensor *tensor = new Tensor ();
		tensor->SetKappaMax ((float)v);
		m.SetTensor (v, tensor);
	}
	ASSERT_EQ (m.SetTextureCoordinates (uv0, nv), 0);
	ASSERT_EQ (m.SetTextureCoordinates1 (uv1), 0);
	ASSERT_EQ (m.SetVertexTangents (tangents), 0);
	for (unsigned int f = 0; f < 12; ++f)
		m.FaceAt (f)->InitTexCoord ();   // indice d'UV de coin = indice de sommet

	const Mesh src (m);
	const CreaseNormals cn = ComputeCreaseNormals (m, kDefaultCreaseAngleDeg);
	ASSERT_EQ (m.SplitVerticesByCornerNormals (cn.cornerNormals), 16);
	ASSERT_EQ (m.GetNVertices (), 24u);
	ASSERT_EQ (m.GetVertexColors ().size (), 72u);
	ASSERT_EQ (m.GetTextureCoordinates ().size (), 48u);
	ASSERT_EQ (m.GetTextureCoordinates1 ().size (), 48u);
	ASSERT_EQ (m.GetVertexTangents ().size (), 96u);
	ASSERT_EQ (m.GetNTensors (), 24u);

	for (unsigned int f = 0; f < 12; ++f)
		for (unsigned int i = 0; i < 3; ++i)
		{
			const unsigned int o = (unsigned int)src.GetFaceVertex (f, i);
			const unsigned int d = (unsigned int)m.GetFaceVertex (f, i);
			for (int k = 0; k < 3; ++k)
				EXPECT_EQ (m.GetVertexColors ()[3 * d + k], src.GetVertexColors ()[3 * o + k]);
			for (int k = 0; k < 2; ++k)
			{
				EXPECT_EQ (m.GetTextureCoordinates ()[2 * d + k], src.GetTextureCoordinates ()[2 * o + k]);
				EXPECT_EQ (m.GetTextureCoordinates1 ()[2 * d + k], src.GetTextureCoordinates1 ()[2 * o + k]);
			}
			EXPECT_EQ (m.FaceAt (f)->GetTexCoordIndex (i), (int)d) << "l'indice d'UV suit le doublon";
			ASSERT_NE (m.GetTensor (d), nullptr);
			EXPECT_EQ (m.GetTensor (d)->GetKappaMax (), (float)o);

			// Tangente : w conserve, orthogonale a la normale du doublon.
			const float *t = &m.GetVertexTangents ()[4 * d];
			const float *n = &m.GetVertexNormals ()[3 * d];
			EXPECT_EQ (t[3], src.GetVertexTangents ()[4 * o + 3]);
			EXPECT_NEAR (t[0] * n[0] + t[1] * n[1] + t[2] * n[2], 0.0, kTol);
			EXPECT_NEAR (std::sqrt (t[0] * t[0] + t[1] * t[1] + t[2] * t[2]), 1.0, kTol);
		}
}

TEST (TEST_cgmesh_crease_normals, render_tangents_are_orthogonal_to_the_corner_normal)
{
	Mesh m;
	MakeCube (m);
	std::vector<float> tangents;
	const V3 t = Normalized ({ 1.0, 2.0, 3.0 });
	for (unsigned int v = 0; v < 8; ++v)
		tangents.insert (tangents.end (), { (float)t.x, (float)t.y, (float)t.z, 1.f });
	ASSERT_EQ (m.SetVertexTangents (tangents), 0);

	const Mesh::PolygonRenderData rd = m.BuildCreaseRenderData (kDefaultCreaseAngleDeg);
	ASSERT_EQ (rd.tangents.size (), 4 * (rd.positions.size () / 3));
	for (size_t k = 0; k < rd.positions.size () / 3; ++k)
	{
		const float *tk = &rd.tangents[4 * k];
		const V3 n = RenderNormal (rd, k);
		EXPECT_NEAR (tk[0] * n.x + tk[1] * n.y + tk[2] * n.z, 0.0, kTol) << "sommet de rendu " << k;
		EXPECT_NEAR (std::sqrt (tk[0] * tk[0] + tk[1] * tk[1] + tk[2] * tk[2]), 1.0, kTol);
		EXPECT_EQ (tk[3], 1.f);
	}
}

// ----------------------------------------------------------------------------
//  Seuil desactive, cache
// ----------------------------------------------------------------------------

TEST (TEST_cgmesh_crease_normals, negative_threshold_gives_the_mesh_normals)
{
	Mesh m;
	MakeCube (m);
	m.ComputeNormals ();

	const Mesh::PolygonRenderData a = m.BuildCreaseRenderData (kCreaseAngleMeshNormals);
	const Mesh::PolygonRenderData b = m.BuildPolygonRenderData (false);
	EXPECT_EQ (a.positions, b.positions);
	EXPECT_EQ (a.normals, b.normals);
	EXPECT_EQ (a.indices, b.indices);
	EXPECT_EQ (a.positions.size () / 3, 8u);
}

TEST (TEST_cgmesh_crease_normals, crease_normals_are_cached_by_revision_and_threshold)
{
	Mesh m;
	MakeCube (m);

	const auto a = m.GetCreaseNormals (kDefaultCreaseAngleDeg);
	EXPECT_EQ (m.GetCreaseNormals (kDefaultCreaseAngleDeg), a) << "meme revision, meme seuil";
	const auto b = m.GetCreaseNormals (45.f);
	EXPECT_NE (b, a) << "autre seuil";
	m.SetVertex (6, 1.f, 1.f, 2.f);
	EXPECT_NE (m.GetCreaseNormals (45.f), b) << "autre revision";
}

// Init remet la revision a zero : un maillage reconstruit avec le meme nombre
// d'ecritures retrouve la revision du cache, qui doit pourtant etre recalcule.
TEST (TEST_cgmesh_crease_normals, cache_is_invalidated_by_init)
{
	const auto fill = [] (Mesh &m, bool vertical)
	{
		m.Init (3, 1);
		m.SetVertex (0, 0.f, 0.f, 0.f);
		m.SetVertex (1, 1.f, 0.f, 0.f);
		m.SetVertex (2, 0.f, vertical ? 0.f : 1.f, vertical ? 1.f : 0.f);
		m.SetFace (0, 0, 1, 2);
	};

	Mesh m;
	fill (m, false);   // plan XY : normale +Z
	const uint64_t revision = m.GetRevision ();
	const auto a = m.GetCreaseNormals (kDefaultCreaseAngleDeg);
	ASSERT_EQ (a->GetNCorners (), 3u);
	EXPECT_NEAR (a->cornerNormals[2], 1.f, kTol);

	fill (m, true);    // plan XZ : normale -Y
	ASSERT_EQ (m.GetRevision (), revision) << "le cas suppose une revision identique";
	const V3 expected = TriangleNormal (m, 0);
	const auto b = m.GetCreaseNormals (kDefaultCreaseAngleDeg);
	ASSERT_EQ (b->GetNCorners (), 3u);
	for (unsigned int c = 0; c < 3; ++c)
	{
		const V3 n { b->cornerNormals[3 * c], b->cornerNormals[3 * c + 1], b->cornerNormals[3 * c + 2] };
		EXPECT_LT (AngleDeg (n, expected), 1e-3) << "coin " << c;
	}
}
