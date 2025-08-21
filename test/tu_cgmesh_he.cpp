#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

static Mesh_half_edge* load_mesh()
{
	std::string filename("./test/data/rabbit.obj");

	Mesh_half_edge* he = new Mesh_half_edge();
	he->load(filename.c_str());
	he->create_half_edge();

	return he;
}

TEST(TEST_cgmesh_he, constructor)
{
	Ticker *ticker = new Ticker ();
	double dElapsedTime = 0;

	std::string filename("./test/data/rabbit.obj");
	Mesh *mesh = new Mesh ();
	mesh->load (filename.c_str());
	dElapsedTime = ticker->stop ();
	cout << mesh->m_nVertices << " vertices" << endl;
	cout << mesh->m_nFaces << " faces" << endl;
	cout << dElapsedTime << "ms" << endl;
	delete mesh;


	ticker->start ();
	Mesh_half_edge *hemodel = new Mesh_half_edge (filename.c_str());
	dElapsedTime = ticker->stop ();
	cout << hemodel->m_nVertices << " vertices" << endl;
	cout << hemodel->m_nFaces << " faces" << endl;
	cout << dElapsedTime << "ms" << endl;

	EXPECT_EQ(hemodel->m_nVertices, 453);
	EXPECT_EQ(hemodel->m_nFaces, 902);

	delete hemodel;
}

TEST(TEST_cgmesh_he, smoothing)
{
	Mesh_half_edge* he = load_mesh();

	// Laplacian
	MeshAlgoSmoothingLaplacian *pSmoothingLaplacian = new MeshAlgoSmoothingLaplacian ();
	
	pSmoothingLaplacian->Apply (he);
	he->save ("export_smoothing_laplacian_x1.obj");
	
	pSmoothingLaplacian->Apply (he);
	he->save ("export_smoothing_laplacian_x2.obj");
	
	pSmoothingLaplacian->Apply (he);
	he->save ("export_smoothing_laplacian_x3.obj");
	
	delete pSmoothingLaplacian;

	// Taubin
	MeshAlgoSmoothingTaubin *pSmoothingTaubin = new MeshAlgoSmoothingTaubin ();
	
	pSmoothingTaubin->Apply (he);
	he->save ("export_smoothing_taubin_x1.obj");
	
	pSmoothingTaubin->Apply (he);
	he->save ("export_smoothing_taubin_x2.obj");
	
	pSmoothingTaubin->Apply (he);
	he->save ("export_smoothing_taubin_x3.obj");
	
	delete pSmoothingTaubin;
}

TEST(TEST_cgmesh_he, subdivision)
{
	Mesh_half_edge* he = load_mesh();

	// Loop
	MeshAlgoSubdivisionLoop *pSsubdivisionLoop = new MeshAlgoSubdivisionLoop ();
	
	pSsubdivisionLoop->Apply (he);
	he->save ("export_subdivision_loop_x1.obj");
	
	//pSsubdivisionLoop->Apply (model);
	//model->save ("export_subdivision_loop_x2.obj");
	
	delete pSsubdivisionLoop;
	//delete he;
	

	// Karbacher
	MeshAlgoSubdivisionKarbacher *pSsubdivisionKarbacher = new MeshAlgoSubdivisionKarbacher ();
	
	pSsubdivisionKarbacher->Apply (he);
	he->save ("export_subdivision_karbacher_x1.obj");
	
	delete pSsubdivisionKarbacher;
}

//
//
//
TEST(TEST_cgmesh_he, normals)
{
	Mesh_half_edge* he = load_mesh();

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
static void diff_common(TensorMethodId tensorMethodId)
{
	Mesh_half_edge* he = load_mesh();

	Normals* normalsEvaluator = new Normals();
	normalsEvaluator->EvalOnVertices(he, Normals::THURMER);

	bool res;

	MeshAlgoTensorEvaluator *pDiffParamEvaluator = new MeshAlgoTensorEvaluator();
	pDiffParamEvaluator->Init (he);
	
	pDiffParamEvaluator->Evaluate (tensorMethodId);
	float fCurvature;
	//pDiffParamEvaluator->Dump ();
	pDiffParamEvaluator->GetExtremalCurvature (CURVATURE_MAX, 1, &fCurvature);
	printf ("GetMaxCurvatureMaximal : %f\n", fCurvature);
	
	pDiffParamEvaluator->GetExtremalCurvature (CURVATURE_MAX, 0, &fCurvature);
	printf ("GetMinCurvatureMaximal : %f\n", fCurvature);
	
	pDiffParamEvaluator->GetExtremalCurvature (CURVATURE_MIN, 1, &fCurvature);
	printf ("GetMaxCurvatureMinimal : %f\n", fCurvature);
	
	pDiffParamEvaluator->GetExtremalCurvature (CURVATURE_MIN, 0, &fCurvature);
	printf ("GetMinCurvatureMinimal : %f\n", fCurvature);
	
	int n;
	float *curvatures;
	pDiffParamEvaluator->GetCurvatures (CURVATURE_MAX, &n, &curvatures);
	printf ("GetMaximalCurvatures : %d\n", n);

	// histogram
	float *histogram;
	int nbins = 64;
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CURVATURE_MAX, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, "histogram_maximal_curvatures.txt");
		SAFE_FREE (histogram);
	}
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CURVATURE_MIN, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, "histogram_minimal_curvatures.txt");
		SAFE_FREE (histogram);
	}
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CURVATURE_MEAN, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, "histogram_mean_curvatures.txt");
		SAFE_FREE (histogram);
	}
	res = pDiffParamEvaluator->GetCurvaturesHistogram (CURVATURE_GAUSSIAN, nbins, &histogram);
	if (res == true)
	{
		output_1array (histogram, nbins, "histogram_gaussian_curvatures.txt");
		SAFE_FREE (histogram);
	}
	
	delete pDiffParamEvaluator;
}

TEST(TEST_cgmesh_he, diff_hamman)
{
	diff_common(TENSOR_HAMANN);
}

TEST(TEST_cgmesh_he, diff_taubin)
{
	diff_common(TENSOR_TAUBIN);
}

TEST(TEST_cgmesh_he, diff_desbrun)
{
	diff_common(TENSOR_DESBRUN);
}

TEST(TEST_cgmesh_he, diff_steiner)
{
	//diff_common(TENSOR_STEINER);
}

TEST(TEST_cgmesh_he, diff_goldfeather)
{
	//diff_common(TENSOR_GOLDFEATHER);
}

TEST(TEST_cgmesh_he, clipper)
{
	Mesh_half_edge* he = load_mesh();
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
	Cshape_distribution_osada *osada;
	osada = new Cshape_distribution_osada (model->m_nVertices, model->m_pVertices,
					       model->m_nFaces, model->GetTriangles ());
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
	
	
	printf ("%d %d\n", model->m_nVertices, model->m_nFaces);
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
	Cshape_distribution_osada *osada = new Cshape_distribution_osada (model->m_nVertices,
									  model->m_pVertices,
									  model->m_nFaces,
									  model->GetTriangles ());
	
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
	Mesh_half_edge* he = load_mesh();

	// shape distributions by Osada
	evaluate_shape_distribution_osada (he, 100000, 64);
	
	// distribution around axis by Paquet, Rioux
	evaluate_distribution_around_axis (he);
	
	// differential parameters distribution
	evaluate_differential_parameters_distribution (he);
}
