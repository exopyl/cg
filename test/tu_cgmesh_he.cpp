#include <gtest/gtest.h>

#include <string>

#include "../src/cgmesh/cgmesh.h"

// ASSERT_* n'est utilisable que dans un helper a retour void : le maillage
// ressort donc par parametre. Sans cette verification, un rabbit.obj absent
// laisse un maillage vide dont les descripteurs de forme ne terminent pas.
static void load_mesh(Mesh_half_edge** out)
{
	*out = nullptr;

	std::string filename("./test/data/rabbit.obj");

	Mesh_half_edge* he = new Mesh_half_edge();
	ASSERT_EQ(he->m_pMesh->load(filename.c_str()), 0)
		<< filename << " introuvable ou illisible";
	he->create_half_edge();

	*out = he;
}

TEST(TEST_cgmesh_he, constructor)
{
	Ticker *ticker = new Ticker ();
	double dElapsedTime = 0;

	std::string filename("./test/data/rabbit.obj");
	Mesh *mesh = new Mesh ();
	ASSERT_EQ(mesh->load (filename.c_str()), 0)
		<< filename << " introuvable ou illisible";
	dElapsedTime = ticker->stop ();
	cout << mesh->GetNVertices () << " vertices" << endl;
	cout << mesh->GetNFaces () << " faces" << endl;
	cout << dElapsedTime << "ms" << endl;
	delete mesh;


	ticker->start ();
	Mesh_half_edge *hemodel = new Mesh_half_edge (filename.c_str());
	dElapsedTime = ticker->stop ();
	cout << hemodel->m_pMesh->GetNVertices () << " vertices" << endl;
	cout << hemodel->m_pMesh->GetNFaces () << " faces" << endl;
	cout << dElapsedTime << "ms" << endl;

	EXPECT_EQ(hemodel->m_pMesh->GetNVertices (), 453);
	EXPECT_EQ(hemodel->m_pMesh->GetNFaces (), 902);

	delete hemodel;
}

TEST(TEST_cgmesh_he, smoothing)
{
	Mesh_half_edge* he = nullptr;
	ASSERT_NO_FATAL_FAILURE(load_mesh(&he));

	// Laplacian
	MeshAlgoSmoothingLaplacian *pSmoothingLaplacian = new MeshAlgoSmoothingLaplacian ();
	
	pSmoothingLaplacian->Apply (he);
	he->m_pMesh->save ("export_smoothing_laplacian_x1.obj");
	
	pSmoothingLaplacian->Apply (he);
	he->m_pMesh->save ("export_smoothing_laplacian_x2.obj");
	
	pSmoothingLaplacian->Apply (he);
	he->m_pMesh->save ("export_smoothing_laplacian_x3.obj");
	
	delete pSmoothingLaplacian;

	// Taubin
	MeshAlgoSmoothingTaubin *pSmoothingTaubin = new MeshAlgoSmoothingTaubin ();
	
	pSmoothingTaubin->Apply (he);
	he->m_pMesh->save ("export_smoothing_taubin_x1.obj");
	
	pSmoothingTaubin->Apply (he);
	he->m_pMesh->save ("export_smoothing_taubin_x2.obj");
	
	pSmoothingTaubin->Apply (he);
	he->m_pMesh->save ("export_smoothing_taubin_x3.obj");
	
	delete pSmoothingTaubin;
}

TEST(TEST_cgmesh_he, subdivision)
{
	Mesh_half_edge* he = nullptr;
	ASSERT_NO_FATAL_FAILURE(load_mesh(&he));

	// Loop
	MeshAlgoSubdivisionLoop *pSsubdivisionLoop = new MeshAlgoSubdivisionLoop ();
	
	pSsubdivisionLoop->Apply (he);
	he->m_pMesh->save ("export_subdivision_loop_x1.obj");
	
	//pSsubdivisionLoop->Apply (model);
	//model->save ("export_subdivision_loop_x2.obj");
	
	delete pSsubdivisionLoop;
	//delete he;
	

	// Karbacher
	MeshAlgoSubdivisionKarbacher *pSsubdivisionKarbacher = new MeshAlgoSubdivisionKarbacher ();
#if 0 // TORESTORE
	pSsubdivisionKarbacher->Apply (he);
	he->m_pMesh->save ("export_subdivision_karbacher_x1.obj");
#endif
	delete pSsubdivisionKarbacher;
}

//
//
//
TEST(TEST_cgmesh_he, normals)
{
	Mesh_half_edge* he = nullptr;
	ASSERT_NO_FATAL_FAILURE(load_mesh(&he));

	Ticker *ticker = new Ticker ();
	double dElapsedTime = 0;

	ticker->start ();

	Normals *n = new Normals ();
	n->EvalOnVertices (he, Normals::THURMER);

	dElapsedTime = ticker->stop ();
	cout << "normals on vertices : " << dElapsedTime << "ms" << endl;

	delete n;
	delete ticker;
}

//
//
//
// `prefix` distingue les fichiers de sortie d'un appelant a l'autre. Sans lui, les
// trois tests qui appellent cette fonction ecrivaient les MEMES quatre
// histogrammes.
static void diff_common(TensorMethodId tensorMethodId, const char* prefix)
{
	Mesh_half_edge* he = nullptr;
	ASSERT_NO_FATAL_FAILURE(load_mesh(&he));

	Normals* normalsEvaluator = new Normals();
	normalsEvaluator->EvalOnVertices(he, Normals::THURMER);

	bool res;

	MeshAlgoTensorEvaluator *pDiffParamEvaluator = new MeshAlgoTensorEvaluator();
	pDiffParamEvaluator->Init (he);
	
	pDiffParamEvaluator->Evaluate (tensorMethodId);
	float fCurvature;
	//pDiffParamEvaluator->Dump ();
	pDiffParamEvaluator->GetExtremalCurvature (CurvatureType::Max, 1, &fCurvature);
	printf ("GetMaxCurvatureMaximal : %f\n", fCurvature);
	
	pDiffParamEvaluator->GetExtremalCurvature (CurvatureType::Max, 0, &fCurvature);
	printf ("GetMinCurvatureMaximal : %f\n", fCurvature);
	
	pDiffParamEvaluator->GetExtremalCurvature (CurvatureType::Min, 1, &fCurvature);
	printf ("GetMaxCurvatureMinimal : %f\n", fCurvature);
	
	pDiffParamEvaluator->GetExtremalCurvature (CurvatureType::Min, 0, &fCurvature);
	printf ("GetMinCurvatureMinimal : %f\n", fCurvature);
	
	int n;
	float *curvatures;
	pDiffParamEvaluator->GetCurvatures (CurvatureType::Max, &n, &curvatures);
	printf ("GetMaximalCurvatures : %d\n", n);

	// histogram
	float *histogram;
	int nbins = 64;
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CurvatureType::Max, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, (std::string(prefix) + "_histogram_maximal_curvatures.txt").c_str());
		SAFE_FREE (histogram);
	}
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CurvatureType::Min, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, (std::string(prefix) + "_histogram_minimal_curvatures.txt").c_str());
		SAFE_FREE (histogram);
	}
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CurvatureType::Mean, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, (std::string(prefix) + "_histogram_mean_curvatures.txt").c_str());
		SAFE_FREE (histogram);
	}
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CurvatureType::Gaussian, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, (std::string(prefix) + "_histogram_gaussian_curvatures.txt").c_str());
		SAFE_FREE (histogram);
	}
	
	delete pDiffParamEvaluator;
}

TEST(TEST_cgmesh_he, diff_hamman)
{
	diff_common(TENSOR_HAMANN, "hamman");
}

TEST(TEST_cgmesh_he, diff_taubin)
{
	diff_common(TENSOR_TAUBIN, "taubin");
}

TEST(TEST_cgmesh_he, diff_desbrun)
{
	diff_common(TENSOR_DESBRUN, "desbrun");
}

TEST(TEST_cgmesh_he, diff_steiner)
{
	diff_common(TENSOR_STEINER, "steiner");
}

TEST(TEST_cgmesh_he, diff_goldfeather)
{
	diff_common(TENSOR_GOLDFEATHER, "goldfeather");
}

TEST(TEST_cgmesh_he, clipper)
{
	Mesh_half_edge* he = nullptr;
	ASSERT_NO_FATAL_FAILURE(load_mesh(&he));
	Cmodel3d_half_edge_clipper *clipper = new Cmodel3d_half_edge_clipper (he);

	clipper->set_plane(Vector3d(0., 0., .3), Vector3d(0., 0., 1.));

	int n_intersections;
	int* n_vertices;
	float** intersections;
	clipper->get_intersections(&n_intersections, &n_vertices, &intersections);

	EXPECT_EQ(n_intersections, 5);
}

//
// Features
//
//

//#define VERTICES_DISTRIBUTION_PCA

//#define VERTICES_DISTRIBUTION_PCA_PAQUET
//#define NORMALES_DISTRIBUTION_PCA_PAQUET

#define HISTOGRAM_OSADA
//#define HISTOGRAM_OSADA_A3
//#define HISTOGRAM_OSADA_D1
#define HISTOGRAM_OSADA_D2
//#define HISTOGRAM_OSADA_D3
//#define HISTOGRAM_OSADA_D4

//#define CURVATURE_HISTOGRAM
//#define HISTOGRAM_BESL
//#define HISTOGRAM_MPEG7
//#define HISTOGRAM_GAUSSIAN_CURVATURE



/*** Evaluate shape descriptors */
static float distance_between_histograms (float *h1, float *h2, int n_bins)
{
	double d = 0.0;
	for (int i=0; i<n_bins; i++)
		d += (h1[i]-h2[i])*(h1[i]-h2[i]);
	return d;
}

static void evaluate_shape_descriptors (Mesh_half_edge *model)
{
#ifdef VERTICES_DISTRIBUTION_PCA
	Cmodel3d_features_distribution_around_axis *d0 = new Cmodel3d_features_distribution_around_axis (model);
#endif /* VERTICES_DISTRIBUTION_PCA */
	
#ifdef HISTOGRAM_OSADA
	std::vector<unsigned int> osada_tris = model->m_pMesh->GetTriangles ();
	Cshape_distribution_osada *osada;
	osada = new Cshape_distribution_osada (model->m_pMesh->GetNVertices (), model->m_pMesh->GetVertices ().data(),
					       model->m_pMesh->GetNFaces (), osada_tris.data());
#endif /* HISTOGRAM_OSADA */
	
#ifdef VERTICES_DISTRIBUTION_PCA_PAQUET
	Cmodel3d_features_distribution_around_axis *d0 = new Cmodel3d_features_distribution_around_axis (model);
#endif /* VERTICES_DISTRIBUTION_PCA_PAQUET */
	
#ifdef NORMALES_DISTRIBUTION_PCA_PAQUET
	Cmodel3d_features_distribution_around_axis *d0 = new Cmodel3d_features_distribution_around_axis (model);
#endif /* NORMALES_DISTRIBUTION_PCA_PAQUET */
	
#ifdef CURVATURE_HISTOGRAM
	Cmodel3d_half_edge *hemodel = new Cmodel3d_half_edge (model->get_n_vertices (),
							      model->get_vertices (),
							      model->get_n_faces (),
							      model->get_faces ());
	
	Cmodel3d_features_differential_parameters_distribution *d2;
	d2 = new Cmodel3d_features_differential_parameters_distribution (hemodel);
#endif /* CURVATURE_HISTOGRAM */
	
	
	printf ("%d %d\n", model->m_pMesh->GetNVertices (), model->m_pMesh->GetNFaces ());
	int n_data = 2000000; // nombre maximal de relevés sur le modèle 3D
	int istart = 200000;    // nombre de relevés de départ
	int istep  = 100000;    // pas pour le nombre de relevés
	int n_extractions = 100; // nombre d'extractions du descripteur sur le même modèle pour effectuer une moyenne
	int n_bins = 64;
	float *histo1, *histo2;
	
	float *errors = (float*)malloc(n_data*sizeof(float));
	errors = (float*)memset ((void*)errors, 0, n_data*sizeof(float));
	
	float **histos = (float**)malloc(n_extractions*sizeof(float*));
	//for (i=0; i<n_extractions; i++) histos[i] = (float*)malloc(n_bins*sizeof(float));
	
	int n_distances = n_extractions*(n_extractions-1)/2;
	float *distances = (float*)malloc(n_distances*sizeof(float));
	float *variances = (float*)malloc(n_data*sizeof(float));
	
	int i,j,k;
	
/*
  Cticker t;
  t.start();
  for (i=0; i<10000000; i++)
  {
	  v3d pt;
	  osada->select_random_point (pt);
	 //float t = rand()/(RAND_MAX+1.0);
	 }
  printf ("%lf\n", t.stop());
  exit (EXIT_SUCCESS);
*/

  for (i=istart; i<n_data; i+=istep)
  {
	  printf ("i = %d\n", i);
	  for (j=0; j<n_extractions; j++)
	  {
#ifdef HISTOGRAM_OSADA
		osada->init_random (j*10);
#ifdef HISTOGRAM_OSADA_A3
		osada->evaluate_distribution (Cshape_distribution_osada::A3, i, 64);
#endif
#ifdef HISTOGRAM_OSADA_D1
		osada->evaluate_distribution (Cshape_distribution_osada::D1, i, 64);
#endif
#ifdef HISTOGRAM_OSADA_D2
		osada->evaluate_distribution (Cshape_distribution_osada::D2, i, 64);
#endif
#ifdef HISTOGRAM_OSADA_D3
		osada->evaluate_distribution (Cshape_distribution_osada::D3, i, 64);
#endif
#ifdef HISTOGRAM_OSADA_D4
		osada->evaluate_distribution (Cshape_distribution_osada::D4, i, 64);
#endif
		osada->normalize_distribution ();
		histos[j] = osada->get_histogram ();
#endif /* HISTOGRAM_OSADA */
	  }

	  // compute all the distances between the histograms
	  int iwalk = 0;
	  for (j=0; j<n_extractions-1; j++)
		  for (k=j+1; k<n_extractions; k++)
			  distances[iwalk++] = 10000.0*distance_between_histograms (histos[j], histos[k], n_bins);
//	  assert (iwalk == n_distances);

	  // mean distance
	  float dmean = 0.0;
	  for (j=0; j<n_distances; j++)
		  dmean += distances[j];
	  dmean /= n_distances;
	  printf ("dmean = %f\n", dmean);

	  // variance
	  for (j=0; j<n_distances; j++)
	  {
		  //printf ("%f %f\n", distances[j], dmean);
		  variances[i] = variances[i] + (distances[j]-dmean)*(distances[j]-dmean);
		  //printf ("%f\n", variances[i]);
	  }
	  variances[i] /= n_distances;
	  //variances[i] = dmean;
	  printf ("variance = %lf\n", variances[i]);
  }
/*
  osada->evaluate_distribution (type, i, n_bins);
  osada->normalize_distribution ();
  osada->export_distribution ("evaluation_shape.dat");
  delete osada;
*/
  // export variances
  FILE *ptr = fopen ("D3_variances.dat", "w");
  for (i=istart; i<n_data; i+=istep)
	  fprintf (ptr, "%d %f\n", i, variances[i]);
  fclose (ptr);

  exit (EXIT_SUCCESS);
}



//
// shape distribution (Osada)
//
static void evaluate_shape_distribution_osada (Mesh_half_edge *model, int nPoints, int nBins)
{
	std::vector<unsigned int> osada_tris = model->m_pMesh->GetTriangles ();
	Cshape_distribution_osada *osada = new Cshape_distribution_osada (model->m_pMesh->GetNVertices (),
									  model->m_pMesh->GetVertices ().data(),
									  model->m_pMesh->GetNFaces (),
									  osada_tris.data());
	
	// A3
	osada->evaluate_distribution (Cshape_distribution_osada::A3, nPoints, nBins);
	osada->normalize_distribution ();
	osada->export_distribution ("features_A3.dat");
	
	// D1
	osada->evaluate_distribution (Cshape_distribution_osada::D1, nPoints, nBins);
	osada->normalize_distribution ();
	osada->export_distribution ("features_D1.dat");
	
	// D2
	osada->evaluate_distribution (Cshape_distribution_osada::D2, nPoints, nBins);
	osada->normalize_distribution ();
	osada->export_distribution ("features_D2.dat");
	
	// D3
	osada->evaluate_distribution (Cshape_distribution_osada::D3, nPoints, nBins);
	osada->normalize_distribution ();
	osada->export_distribution ("features_D3.dat");
	
	// D4
	osada->evaluate_distribution (Cshape_distribution_osada::D4, nPoints, nBins);
	osada->normalize_distribution ();
	osada->export_distribution ("features_D4.dat");

	delete osada;
	return;
}

//
// distribution around axis (Paquet, Rioux)
//
static void evaluate_distribution_around_axis (Mesh_half_edge *model)
{
	// VERTICES_DISTRIBUTION_PCA
	{
		Cdistribution_around_axis *d0 = new Cdistribution_around_axis (model);

		d0->compute_first_order_distributions (Cdistribution_around_axis::ORIENTATION_PCA);
		float *lengths    = d0->get_lengths ();
		float *dmeans     = d0->get_dmeans ();
		float *variances  = d0->get_variances ();
		float *deviations = d0->get_deviations ();
		printf ("%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n",
		  lengths[0], lengths[1], lengths[2],
		  dmeans[0], dmeans[1], dmeans[2],
		  variances[0], variances[1], variances[2],
		  deviations[0], deviations[1], deviations[2]);
		/*
		printf ("lengths    : %f %f %f\n", lengths[0], lengths[1], lengths[2]);
		printf ("dmeans     : %f %f %f\n", dmeans[0], dmeans[1], dmeans[2]);
		printf ("variances  : %f %f %f\n", variances[0], variances[1], variances[2]);
		printf ("deviations : %f %f %f\n", deviations[0], deviations[1], deviations[2]);
		*/
		
		delete d0;
	}


  // VERTICES_DISTRIBUTION_PCA_PAQUET
	{
		Cdistribution_around_axis *d0 = new Cdistribution_around_axis (model);

		d0->compute_first_order_distributions_paquet (Cdistribution_around_axis::ORIENTATION_PCA, 50000, 64);
		d0->export_histogram1k  ("paquet_vertices_pca_histo1k.dat");	// histo1k
		d0->export_histogram2k1 ("paquet_vertices_pca_histo2k1.dat");	// histo2k1
		d0->export_histogram2k2 ("paquet_vertices_pca_histo2k2.dat");	// histo2k2
		d0->export_histogram3k  ("paquet_vertices_pca_histo3k.dat");	// histo3k

		delete d0;
	}

  // NORMALES_DISTRIBUTION_PCA_PAQUET
	{
		Cdistribution_around_axis *d0 = new Cdistribution_around_axis (model);

		d0->compute_second_order_distributions_paquet (Cdistribution_around_axis::ORIENTATION_PCA, 50000, 64);
		d0->export_histogram1k  ("paquet_normales_pca_histo1k.dat");	// histo1k
		d0->export_histogram2k1 ("paquet_normales_pca_histo2k1.dat"); // histo2k1
		d0->export_histogram2k2 ("paquet_normales_pca_histo2k2.dat"); // histo2k2
		d0->export_histogram3k  ("paquet_normales_pca_histo3k.dat");	// histo3k

		delete d0;
	}
}

//
// differential parameters distribution
//
static void evaluate_differential_parameters_distribution (Mesh_half_edge *he)
{
	Cdifferential_parameters_distribution *d2;
	d2 = new Cdifferential_parameters_distribution (he);
	
	// Besl
	d2->compute_distribution (Cdifferential_parameters_distribution::BESL, 64);
	d2->normalize_distribution ();
	d2->export_distribution ("features_diff_param_besl.dat");
	
	// gaussian curvature
	d2->compute_distribution (Cdifferential_parameters_distribution::GAUSSIAN_CURVATURE, 64);
	d2->normalize_distribution ();
	d2->export_distribution ("features_diff_param_gaussian.dat");
	
	// mpeg7
	d2->compute_distribution (Cdifferential_parameters_distribution::MPEG7, 64);
	d2->normalize_distribution ();
	d2->export_distribution ("features_diff_param_mpeg7.dat");

	delete d2;
}

TEST(TEST_cgmesh_he, features)
{
	Mesh_half_edge* he = nullptr;
	ASSERT_NO_FATAL_FAILURE(load_mesh(&he));

	// shape distributions by Osada
	evaluate_shape_distribution_osada (he, 100000, 64);
	
	// distribution around axis by Paquet, Rioux
	evaluate_distribution_around_axis (he);
	
	// differential parameters distribution
	evaluate_differential_parameters_distribution (he);
}

// ============================================================================
//  Mesh_half_edge (Mesh *)
// ============================================================================
//
// Envelopper un maillage le copie en PROFONDEUR et ne le triangule pas : tout ce
// qu'il possede doit survivre, son identite de polygone comprise.
TEST (TEST_cgmesh_he, wrapping_a_mesh_keeps_everything_it_owns)
{
	Mesh src;
	src.Init (5, 2);
	float v[15] = { 0,0,0,  1,0,0,  1,1,0,  0,1,0,  2,0,0 };
	src.SetVertices (5, v);
	src.SetFace (0, 0, 1, 2, 3);          // un QUAD : son identite doit survivre
	src.SetFace (1, 1, 4, 2);
	src.SetName ("enveloppe");
	src.Material_Add (new MaterialColor (11, 22, 33));
	src.FaceAt (0)->SetMaterialId (0);
	src.FaceAt (0)->SetUsesTextureCoordinates (true);
	src.FaceAt (0)->SetTexCoord (2u, 5u);
	src.AddLine (0, 4);
	src.AddPoint (3);
	src.InitVertexColors (0.5f, 0.25f, 0.125f);

	Mesh_half_edge he (&src);
	Mesh *m = he.m_pMesh;
	ASSERT_NE (m, nullptr);

	EXPECT_EQ (m->GetName (), "enveloppe");
	EXPECT_EQ (m->GetNVertices (), 5u);

	// Le QUAD reste un quad : envelopper un maillage ne doit plus detruire son
	// identite de polygone (decision R2).
	EXPECT_EQ (m->GetNFaces (), 2u);
	EXPECT_EQ (m->GetFaceNVertices (0), 4);
	EXPECT_FALSE (m->IsTriangleMesh ());

	// Materiaux, UV, lignes, points, couleurs.
	ASSERT_EQ (m->GetNMaterials (), 1u);
	EXPECT_NE (m->GetMaterial (0u), src.GetMaterial (0u)) << "clone, pas partage";
	EXPECT_EQ (m->FaceAt (0)->GetMaterialId (), 0);
	EXPECT_TRUE (m->FaceAt (0)->UsesTextureCoordinates ());
	EXPECT_EQ (m->FaceAt (0)->GetTexCoordIndex (2), 5);
	EXPECT_EQ (m->GetLines ().size (), 2u);
	EXPECT_EQ (m->GetPoints ().size (), 1u);
	EXPECT_FLOAT_EQ (m->GetVertexColors ()[1], 0.25f);

	// Et la structure demi-arete se construit bien sur le maillage TRIANGULE,
	// paresseusement : le quad donne deux triangles, soit 3 faces au total.
	// Passer GetNFaces () au lieu de tris.size()/3 en aurait perdu une.
	Che_mesh *che = he.GetCheMesh ();
	ASSERT_NE (che, nullptr);
	EXPECT_EQ (che->m_edges_face.size (), 3u) << "2 triangles issus du quad + 1 triangle";
	EXPECT_EQ (che->m_ne, 9) << "3 aretes par triangle";
}

// ---------------------------------------------------------------------------
//  Empreinte de Che_edge
// ---------------------------------------------------------------------------
// Che_edge est instancie 3 fois par triangle : sur 2 M de triangles, chaque
// octet du type coute 5,7 Mio. Le filet epingle la disposition COMPLETE --
// taille, alignement et decalage de chaque membre -- contre un temoin qui
// declare exactement les membres attendus, dans l'ordre attendu.
//
// ⚠ Portee EXACTE de ce filet, MESUREE par sabotage (trois variantes compilees
// et executees) : reintroduire `int m_flag` avant m_data le fait ceder (40
// contre 32 sur x64). Reintroduire `bool m_visited` ou un `char` ne le fait PAS
// ceder -- il reste 3 octets de bourrage entre m_valid et m_data, sur x64
// comme sur wasm32, et jusqu'a trois membres d'un octet s'y logeraient sans
// couter un octet ni bouger un decalage. Ce filet garde donc l'empreinte, pas
// la liste des membres : un membre GRATUIT lui echappe par construction.
namespace {
struct Che_edge_membres_attendus
{
	int m_v_begin;
	int m_v_end;
	int m_pair;
	int m_face;
	int m_he_next;
	char m_valid;
	void *m_data;
};
} // namespace

TEST (TEST_cgmesh_he, che_edge_ne_porte_que_les_membres_lus)
{
	EXPECT_EQ (sizeof (Che_edge), sizeof (Che_edge_membres_attendus))
		<< "un membre a ete ajoute ou retire de Che_edge ; il se paie 3 fois par triangle";
	EXPECT_EQ (alignof (Che_edge), alignof (Che_edge_membres_attendus));

	EXPECT_EQ (offsetof (Che_edge, m_v_begin), offsetof (Che_edge_membres_attendus, m_v_begin));
	EXPECT_EQ (offsetof (Che_edge, m_v_end),   offsetof (Che_edge_membres_attendus, m_v_end));
	EXPECT_EQ (offsetof (Che_edge, m_pair),    offsetof (Che_edge_membres_attendus, m_pair));
	EXPECT_EQ (offsetof (Che_edge, m_face),    offsetof (Che_edge_membres_attendus, m_face));
	EXPECT_EQ (offsetof (Che_edge, m_he_next), offsetof (Che_edge_membres_attendus, m_he_next));
	EXPECT_EQ (offsetof (Che_edge, m_valid),   offsetof (Che_edge_membres_attendus, m_valid));
	EXPECT_EQ (offsetof (Che_edge, m_data),    offsetof (Che_edge_membres_attendus, m_data));
}

TEST (TEST_cgmesh_he, che_edge_tient_dans_le_bourrage_du_pointeur)
{
	// Temoin de la valeur REELLE, pas seulement de l'egalite au temoin : le cas
	// precedent serait satisfait par deux declarations fausses de la meme facon,
	// puisque le temoin est ecrit a la main. Ici la disposition est RECALCULEE
	// depuis les tailles des types : 5 entiers, un octet loge dans le trou
	// d'alignement du pointeur, puis le pointeur, et aucun bourrage de fin.
	// 28 octets sur wasm32, 32 sur x64.
	constexpr std::size_t alignement = alignof (void *);
	constexpr std::size_t avant_le_pointeur = 5 * sizeof (int) + sizeof (char);
	constexpr std::size_t decalage_du_pointeur =
		(avant_le_pointeur + alignement - 1) / alignement * alignement;

	EXPECT_EQ (offsetof (Che_edge, m_data), decalage_du_pointeur);
	EXPECT_EQ (sizeof (Che_edge), decalage_du_pointeur + sizeof (void *));
}

// ---------------------------------------------------------------------------
//  Cartes paresseuses
// ---------------------------------------------------------------------------
// create_half_edge ne remplit plus les trois cartes : elles se construisent au
// premier acces, depuis m_edges / m_edges_vertex / m_edges_face. Ce filet
// verifie que le CONTENU est le meme qu'avant, pas qu'il arrive plus tard --
// l'instant de la construction ne s'observe qu'a la memoire, et c'est la sonde
// WASM (tmp/memprobe) qui le mesure.
TEST (TEST_cgmesh_he, les_cartes_se_construisent_au_premier_acces_et_repondent)
{
	// Tetraedre : 4 sommets, 4 faces, ferme, chaque demi-arete appariee.
	Mesh mesh;
	float v[12] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  0.f,1.f,0.f,  0.f,0.f,1.f };
	unsigned int f[12] = { 0,2,1,  0,1,3,  0,3,2,  1,2,3 };
	mesh.SetVertices (4, v);
	mesh.SetFaces (4, 3, f);

	Mesh_half_edge he (&mesh);
	Che_mesh *che = he.GetCheMesh ();
	ASSERT_EQ (che->m_ne, 12);

	// Aucun acces aux cartes n'a eu lieu jusqu'ici. La premiere recherche doit
	// donc les batir et repondre juste sur les DOUZE demi-aretes.
	for (int i = 0; i < che->m_ne; i++)
		EXPECT_EQ (che->get_edge (che->edge (i).m_v_begin, che->edge (i).m_v_end), i)
			<< "demi-arete " << i;

	// Une arete qui n'existe pas reste introuvable : sans ce temoin, une carte
	// qui rendrait n'importe quoi passerait le cas precedent aussi.
	EXPECT_EQ (che->get_edge (0, 0), -1);
	EXPECT_EQ (che->get_edge (3, 99), -1);

	// Et la carte sommet -> arete sortante concorde avec le vecteur dont elle
	// est tiree.
	for (unsigned int i = 0; i < 4; i++)
		EXPECT_EQ (che->get_edge_from_vertex ((int)i), che->m_edges_vertex[i]);

	// Les trois accesseurs rendent des conteneurs peuplés, pas des vides.
	EXPECT_EQ (che->EdgeMap ()->size (), 12u);
	EXPECT_EQ (che->VertexEdgeMap ()->size (), 4u);
	EXPECT_EQ (che->FaceEdgeMap ()->size (), 4u);
}

TEST (TEST_cgmesh_he, une_contraction_entretient_les_cartes_construites_tardivement)
{
	// La contraction ENTRETIENT les cartes en place. Construites tard, elles
	// doivent l'etre AVANT qu'elle n'y touche, sinon la reconstruction ecraserait
	// ses mises a jour -- ou pire, partirait de vecteurs qu'elle ne met pas a jour.
	Mesh mesh;
	// Octaedre : ferme, variete, chaque sommet de degre 4.
	float v[18] = {  1.f,0.f,0.f,  -1.f,0.f,0.f,  0.f,1.f,0.f,
	                 0.f,-1.f,0.f,  0.f,0.f,1.f,   0.f,0.f,-1.f };
	unsigned int f[24] = { 0,2,4, 2,1,4, 1,3,4, 3,0,4,
	                       2,0,5, 1,2,5, 3,1,5, 0,3,5 };
	mesh.SetVertices (6, v);
	mesh.SetFaces (8, 3, f);

	Mesh_half_edge he (&mesh);
	Che_mesh *che = he.GetCheMesh ();

	const int ei = che->get_edge (0, 2);
	ASSERT_GE (ei, 0);
	ASSERT_EQ (che->edge_contract2 (ei), 0);

	// La cle contractee a disparu de la carte, et la carte sommet -> arete a
	// perdu le sommet absorbe.
	EXPECT_EQ (che->get_edge (0, 2), -1);
	EXPECT_EQ (che->get_edge_from_vertex (2), -1);
	EXPECT_GE (che->get_edge_from_vertex (0), 0);
}

// ---------------------------------------------------------------------------
//  Cession du maillage de travail
// ---------------------------------------------------------------------------
TEST (TEST_cgmesh_he, release_cede_le_maillage_et_laisse_l_enveloppe_vide)
{
	Mesh source;
	float v[12] = { 0.f,0.f,0.f,  1.f,0.f,0.f,  0.f,1.f,0.f,  0.f,0.f,1.f };
	unsigned int f[12] = { 0,2,1,  0,1,3,  0,3,2,  1,2,3 };
	source.SetVertices (4, v);
	source.SetFaces (4, 3, f);

	Mesh_half_edge he (&source);
	Mesh *before = he.m_pMesh;
	ASSERT_NE (he.GetCheMesh (), nullptr);

	Mesh *released = he.release ();

	// C'est bien le MEME objet qui sort, pas une copie : sans cette egalite de
	// pointeur, release ne ferait rien economiser.
	EXPECT_EQ (released, before);
	EXPECT_NE (released, &source) << "et ce n'est pas l'entree : elle reste a l'appelant";
	ASSERT_NE (released, nullptr);
	EXPECT_EQ (released->GetNVertices (), 4u);
	EXPECT_EQ (released->GetNFaces (), 4u);

	// L'enveloppe repart vide et UTILISABLE : ni pendante, ni porteuse de la
	// topologie du maillage parti.
	ASSERT_NE (he.m_pMesh, nullptr);
	EXPECT_NE (he.m_pMesh, released);
	EXPECT_EQ (he.m_pMesh->GetNVertices (), 0u);
	EXPECT_EQ (he.GetCheMesh ()->m_ne, 0);

	delete released;
}
