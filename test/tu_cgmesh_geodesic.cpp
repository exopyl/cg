#include <iostream>
#include <fstream>

#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

static int _normalize (unsigned int n, float *data)
{
	float max = 0.;
	for (unsigned int i=0; i<n; ++i)
		if (max < data[i])
			max = data[i];
	for (unsigned int i=0; i<n; ++i)
		data[i] /= max;

	return 0;
}

static int _export_obj (geodesic::Mesh mesh, float *distances)
{
	_normalize (mesh.vertices().size(), distances);

	//
	// export mesh
	//
	FILE *mtl = fopen ("output.mtl", "w");
	fprintf (mtl, "newmtl material_0\n");
	fprintf (mtl, "map_Kd data/color_jet.ppm\n");
	fclose (mtl);
	
	FILE *ptr = fopen ("output.obj", "w");
	
	float distance_max = 1.;

	fprintf (ptr, "mtllib output.mtl\n");
	for (unsigned int i=0; i<mesh.vertices().size(); i++)
	{
		geodesic::SurfacePoint p(&mesh.vertices()[i]);
		fprintf (ptr, "v %f %f %f\n", p.x(), p.y(), p.z());
	}
	for (unsigned int i=0; i<mesh.vertices().size(); i++)
	{
		fprintf (ptr, "vt %f 0.\n", distances[i]);
	}
	fprintf (ptr, "usemtl material_0\n");
	for (unsigned int i=0; i<mesh.faces().size(); i++)
	{
		fprintf (ptr, "f %d/%d %d/%d %d/%d\n",
			 1+mesh.faces()[i].adjacent_vertices()[0]->id(),1+mesh.faces()[i].adjacent_vertices()[0]->id(),
			 1+mesh.faces()[i].adjacent_vertices()[1]->id(),1+mesh.faces()[i].adjacent_vertices()[1]->id(),
			 1+mesh.faces()[i].adjacent_vertices()[2]->id(),1+mesh.faces()[i].adjacent_vertices()[2]->id());
	}
	fclose (ptr);

	return true;
}

TEST(TEST_cgmesh_geodesic, geodesic_no_target)
{
	// context
	GeodesicWrapper* geodesic = new GeodesicWrapper();

	// read the data
	Mesh* pMesh = new Mesh();
	pMesh->load("./test/data/rabbit.obj");

	//bool success = geodesic->input (argv[1]);
	bool success = geodesic->SetMesh(pMesh->GetNVertices (), pMesh->GetVertices ().data(), pMesh->GetNFaces (), pMesh->GetTriangles().data());
	ASSERT_TRUE(success);

	// set the algorithm
	//geodesic->SetAlgorithm (geodesic::GeodesicAlgorithmBase::EXACT);

	unsigned source_vertex_index = 0;
	geodesic->AddSource(source_vertex_index);
	//geodesic->AddSource(300);

	// action : target vertex is not specified, print distances to all vertices
	float* distances = (float*)malloc(geodesic->GetNVertices() * sizeof(float));
	//geodesic::GeodesicAlgorithmBase::AlgorithmType algorithmType = geodesic::GeodesicAlgorithmBase::EXACT;
	//geodesic::GeodesicAlgorithmBase::AlgorithmType algorithmType = geodesic::GeodesicAlgorithmBase::DIJKSTRA;
	geodesic->ComputeDistances(distances, geodesic::GeodesicAlgorithmBase::DIJKSTRA);

	// export
	_export_obj(geodesic->mesh, distances);

	// clean distances
	free(distances);
}

// ---------------------------------------------------------------------------
//  Le test « avec cible » n'est PAS un test : c'est une ebauche
// ---------------------------------------------------------------------------
// Le bloc `#if 0` ci-dessous portait un TEST() invisible -- ni compte, ni
// couverture, ni colonne DISABLED. La recommandation de `debt_cgmesh.md` (§ 4)
// est de convertir tout `#if 0` de test en `DISABLED_` motive, ce qui a ete fait
// pour tu_cgmesh_set_lines.cpp.
//
// ICI, C'EST IMPOSSIBLE EN L'ETAT, et je l'ai verifie plutot que suppose : le
// bloc ne COMPILE pas. Il appelle `algorithm`, `source` et `all_sources`, trois
// identificateurs qui n'existent nulle part, et construit un
// `geodesic::SurfacePoint` avec une signature qui n'existe pas. Ce n'est pas un
// test desactive, c'est un brouillon jamais termine.
//
// Un `DISABLED_` exige que le corps compile ; le rendre compilable demande
// d'ecrire le test, pas de deplacer une garde. Le bloc reste donc sous `#if 0`,
// et le talon DISABLED_ ci-dessous rend son existence VISIBLE au decompte : c'est
// la moitie du gain, obtenue sans reecrire ce qu'on ne sait pas encore ecrire.
TEST(TEST_cgmesh_geodesic, DISABLED_geodesic_with_target_is_an_unfinished_sketch)
{
	FAIL() << "ebauche non compilable, conservee sous #if 0 dans ce fichier : "
	          "elle appelle algorithm/source/all_sources, identificateurs "
	          "inexistants, et un constructeur de SurfacePoint qui n'existe pas.";
}

#if 0
TEST(TEST_cgmesh_geodesic, geodesic_with_target)
{
	// context
	GeodesicWrapper* geodesic = new GeodesicWrapper();

	// read the data
	Mesh* pMesh = new Mesh();
	pMesh->load("./test/data/rabbit.obj");

	//bool success = geodesic->input (argv[1]);
	bool success = geodesic->SetMesh(pMesh->GetNVertices (), pMesh->GetVertices ().data(), pMesh->GetNFaces (), pMesh->GetTriangles().data());
	ASSERT_TRUE(success);

	// set the algorithm
	//geodesic->SetAlgorithm (geodesic::GeodesicAlgorithmBase::EXACT);

	unsigned source_vertex_index = 0;
	geodesic->AddSource(source_vertex_index);
	//geodesic->AddSource(300);

	// action : target vertex specified, compute single path
	unsigned target_vertex_index = 22;
	geodesic::SurfacePoint target(pMesh->GetVertices ()[target_vertex_index]);		//create source

	std::vector<geodesic::SurfacePoint> path;	//geodesic path is a sequence of SurfacePoints

	bool const lazy_people_flag = false;		//there are two ways to do exactly the same
	if(lazy_people_flag)
	{
		algorithm.geodesic(source, target, path); //find a single source-target path
	}
	else		//doing the same thing explicitly for educational reasons
	{
		double const distance_limit = geodesic::GEODESIC_INF;			// no limit for propagation
		std::vector<geodesic::SurfacePoint> stop_points(1, target);	//stop propagation when the target is covered
		algorithm.propagate(all_sources, distance_limit, &stop_points);	//"propagate(all_sources)" is also fine, but take more time because covers the whole mesh

		algorithm.trace_back(target, path);		//trace back a single path
	}

	print_info_about_path(path);
	for(unsigned i = 0; i<path.size(); ++i)
	{
		geodesic::SurfacePoint& s = path[i];
		std::cout << s.x() << "\t" << s.y() << "\t" << s.z() << std::endl;
	}
}
#endif
