#include <stdlib.h>
#include <stdio.h>

#include "bundle.h"

Bundle::Bundle ()
{
	cameras_n = 0;
	cameras = NULL;
}

Bundle::~Bundle ()
{
	for (int i=0; i<cameras_n; i++)
		     delete cameras[i];
	delete [] cameras;
}

int Bundle::Load (char *filename)
{
	if (!filename)
		return -1;

	printf ("bundle out file : %s\n", filename);
	FILE *file = fopen (filename, "r");
	if (!file)
	{
		printf ("unable to open %s", filename);
		return -1;
	}
	const int BUFFER_SIZE = 2048;

	char buffer[BUFFER_SIZE];
	fgets (buffer, BUFFER_SIZE, file);
	printf ("%s\n", buffer);

	int nCameras, nPoints;
	fscanf (file, "%d %d\n", &nCameras, &nPoints);
	printf ("nCameras : %d\n", nCameras);
	printf ("nPoints : %d\n", nPoints);
	
	// cameras
	cameras_n = nCameras;
	cameras = new BundleCamera*[cameras_n];
	for (int i=0; i<nCameras; i++)
	{
		BundleCamera *cam = new BundleCamera();
		fscanf (file, "%f %f %f\n", &cam->f_pxl, &cam->k1, &cam->k2);
		cam->f_mm = 5.4*cam->f_pxl/640.;

		fscanf (file, "%f %f %f\n", &cam->R[0], &cam->R[1], &cam->R[2]);
		fscanf (file, "%f %f %f\n", &cam->R[4], &cam->R[5], &cam->R[6]);
		fscanf (file, "%f %f %f\n", &cam->R[8], &cam->R[9], &cam->R[10]);
		mat4_get_inverse (cam->R, cam->R);

		fscanf (file, "%f %f %f\n", &cam->T[0], &cam->T[1], &cam->T[2]);

		//camera_dump (cam);
		cameras[i] = cam;
	}

	// points
	mesh = new Mesh (nPoints, 0);
	mesh->InitVertexColors ();
	int r, g, b;
	for (int i=0; i<nPoints; i++)
	{
		fscanf (file, "%f %f %f\n", &mesh->m_pVertices[3*i+0], &mesh->m_pVertices[3*i+1], &mesh->m_pVertices[3*i+2]);
		fscanf (file, "%d %d %d\n", &r, &g, &b);
		mesh->m_pVertexColors[3*i+0] = (float)r/255.;
		mesh->m_pVertexColors[3*i+1] = (float)g/255.;
		mesh->m_pVertexColors[3*i+2] = (float)b/255.;
		fgets (buffer, BUFFER_SIZE, file);
	}
	//models[imodel] = model;
	//imodel++;

	fclose (file);

	return 0;
}

int Bundle::Load2 (char *bundlefilename, char *imageslistfilename, char *rootpath)
{
	if (!bundlefilename || !imageslistfilename)
		return NULL;

	// open the files
	FILE *bundlefile = fopen (bundlefilename, "r");
	if (!bundlefile)
		return NULL;
	FILE *imageslistfile = fopen (imageslistfilename, "r");
	if (!imageslistfile)
	{
		fclose (bundlefile);
		return NULL;
	}

	// read the files
	char buffer[4096];
	fgets (buffer, 4096, bundlefile);

	int nCameras, nPoints;
	fscanf (bundlefile, "%d %d\n", &nCameras, &nPoints);

	cameras_n = nCameras;
	n_points = nPoints;
	cameras = new BundleCamera*[cameras_n];//(camera_t**)malloc(nCameras*sizeof(camera_t*));
	pt_visible_from_cameras = (unsigned int**)malloc(nPoints*sizeof(unsigned int));
	n_pt_visible_from_cameras = (unsigned int*)malloc(nPoints*sizeof(unsigned int));

	// cameras
	int nEffectiveCameras = 0;
	int *cmap = (int*)malloc(nCameras*sizeof(int));
	for (int i=0; i<nCameras; i++)
	{
		// create a new camera structure
		BundleCamera *camera = new BundleCamera ();

		fgets (buffer, 4096, imageslistfile);
		sscanf (buffer, "%s", buffer);
		char *imgfilename = (char*)malloc(256*sizeof(char));
		if (rootpath)
		{
			imgfilename = strcpy (imgfilename, rootpath);
			buffer[0] = '/';
			imgfilename = strcat (imgfilename, buffer);
		}
		else
			imgfilename = buffer;
		camera->filename = imgfilename;
		if (1)// allinfo)
		{
			//texture_t *texture = import_image(imgfilename);
			//camera->w = texture->width;
			//camera->h = texture->height;
			//texture_delete (texture);
		}

		fscanf (bundlefile, "%f %f %f\n", &camera->f_pxl, &camera->k1, &camera->k2);

		// get ccd width
		if (1)//allinfo)
		{
			//camera->CCDWidth_mm = get_ccd_width (imgfilename);
			//if (camera->CCDWidth_mm <= 0.)
			//	camera->CCDWidth_mm = 5.23;
			//camera->CCDHeight_mm = camera->CCDWidth_mm*camera->h/camera->w;
		}

		// focal length (mm) = CCD width (mm) * focal length (pxl) / image width (pxl)
		if (1)//allinfo)
			camera->f_mm = camera->CCDWidth_mm*camera->f_pxl/camera->w;

		fscanf (bundlefile, "%f %f %f\n", &camera->R[0], &camera->R[1], &camera->R[2]);
		fscanf (bundlefile, "%f %f %f\n", &camera->R[4], &camera->R[5], &camera->R[6]);
		fscanf (bundlefile, "%f %f %f\n", &camera->R[8], &camera->R[9], &camera->R[10]);
		//fmat4_dump (camera->R);

		// update Rinv & d with R
		if (1)//allinfo)
		{
			mat4_get_inverse (camera->Rinv, camera->R);
			vec3_init (camera->d, 0., 0., -1.);
			mat4_transform (camera->d, camera->Rinv, camera->d);
		}

		fscanf (bundlefile, "%f %f %f\n", &camera->T[0], &camera->T[1], &camera->T[2]);
		// update pos with T
		if (1)//allinfo)
		{
			vec3_scale (camera->pos, camera->T, -1.);
			mat4_transform (camera->pos, camera->Rinv, camera->pos);
		}

		if (camera->T[0] == 0. && camera->T[1] == 0. && camera->T[2] == 0.)
		{
			printf ("Skip invalid camera\n");
			delete camera;
			camera = NULL;
			cmap[i] = -1;
		}
		else
		{
			cameras[nEffectiveCameras] = camera;
			cmap[i] = nEffectiveCameras;
			nEffectiveCameras++;
		}
	}
	
	// points
	pt_visible_from_cameras = (unsigned int**) malloc (nPoints*sizeof(unsigned int*));
	memset (pt_visible_from_cameras, 0, nPoints*sizeof(unsigned int*));
	n_pt_visible_from_cameras = (unsigned int*) malloc (nPoints*sizeof(unsigned int));
	memset (n_pt_visible_from_cameras, 0, nPoints*sizeof(unsigned int));
	float x, y, z;
	int r, g, b;
	for (int i=0; i<nPoints; i++)
	{
		fscanf (bundlefile, "%f %f %f\n", &x, &y, &z);
		vec3 pt;
		vec3_init (pt, x, y, z);

		fscanf (bundlefile, "%d %d %d\n", &r, &g, &b);
		fgets (buffer, 4096, bundlefile);
		sscanf (buffer, "%d", &n_pt_visible_from_cameras[i]);
		//printf ("point %d / %d visible from %d cameras : ", i, nPoints, n_pt_visible_from_cameras[i]);
		pt_visible_from_cameras[i] = (unsigned int*)malloc(n_pt_visible_from_cameras[i]*sizeof(unsigned int));

		unsigned int j=0;
		char *pch = strtok (buffer," ");
		pch = strtok (NULL, " ");
		for (unsigned int j=0; j<n_pt_visible_from_cameras[i]; j++)
		{
			int ci = cmap[atoi (pch)];
			pt_visible_from_cameras[i][j] = ci;

			int b = atoi (pch);
			pch = strtok (NULL, " ");

			//camera_t *camera = cameras[ci];
			//fmat4_transform (pt, camera->R, pt);
			//fvec3_addition (pt, pt, camera->T);

			int a = atoi (pch);
			pch = strtok (NULL, " ");

			float imgx = atof (pch);
			pch = strtok (NULL, " ");

			float imgy = atof (pch);
			pch = strtok (NULL, " ");
		}
	}
	cameras_n = nEffectiveCameras;

	// cleaning
	free (cmap);
	fclose (imageslistfile);
	fclose (bundlefile);

	return 0;
}

int Bundle::DeleteRedundantCameras (void)
{
	return 0;
#if 0
	bundle_t *res = bundle_new ();
	if (!res)
		return res;

	res->n_cameras = 0;
	res->cameras = (camera_t**)malloc(bundle->n_cameras*sizeof(camera_t*));
/*
	res->n_points = bundle->n_points;
	res->pt_visible_from_cameras = (unsigned int**)malloc(res->n_points*sizeof(unsigned int));
	res->n_pt_visible_from_cameras = (unsigned int*)malloc(res->n_points*sizeof(unsigned int));
	memcpy (res->n_pt_visible_from_cameras, bundle->n_pt_visible_from_cameras, res->n_points*sizeof(unsigned int));
*/

	// evaluate the bbox of the cameras
	bbox_t bbox;
	bbox_init (&bbox, bundle->cameras[0]->pos, bundle->cameras[0]->pos);
	for (unsigned int i=1; i<bundle->n_cameras; i++)
		bbox_update (&bbox, bundle->cameras[i]->pos);
	float threshold = fvec3_distance_between_points (bbox.minp, bbox.maxp) / 1000.0;

	int nEffectiveCameras = 0;
	int *cmap = (int*)malloc(bundle->n_cameras*sizeof(int));
	for (unsigned int i=0; i<bundle->n_cameras; i++)
	{
		int bRedundant = 0;
		for (unsigned int j=0; j<res->n_cameras; j++)
		{
			float d = fvec3_distance_between_points (bundle->cameras[i]->pos, res->cameras[j]->pos);
			float dot = fvec3_dot_product (bundle->cameras[i]->d, res->cameras[j]->d);
			if (d < threshold && dot > 0.9)
			{
				bRedundant = 1;
				break;
			}
		}
		if (bRedundant)
			cmap[i] = -1;
		else
		{
			res->cameras[nEffectiveCameras] = camera_copy (bundle->cameras[i]);
			cmap[i] = nEffectiveCameras;
			nEffectiveCameras++;
			res->n_cameras = nEffectiveCameras;
		}
	}

/*
	// copy the points
	for (int i=0; i<bundle->n_points; i++)
	{
		res->n_pt_visible_from_cameras[i] = (unsigned int*)malloc(res->n_pt_visible_from_cameras[i]*sizeof(unsigned int));
		res->pt_visible_from_cameras[i] = (unsigned int*)malloc(res->n_pt_visible_from_cameras[i]*sizeof(unsigned int));
		for (unsigned int j=0; j<bundle->n_pt_visible_from_cameras[i]; j++)
			bundle->pt_visible_from_cameras[i][j] = cmap[bundle->pt_visible_from_cameras[i][j]];
	}
*/

	// cleaning
	free (cmap);

	return res;
#endif
}






//
// get the texture coordinates from a vertex and a camera
//
static void project_texture_coordinates_on_vertex (Mesh *mesh, unsigned int vi, BundleCamera *camera, float *u, float *v)
{
	vec3 pt;
	
	mesh->GetVertex (vi, pt);
	mat4_transform (pt, camera->R, pt);
	vec3_addition (pt, pt, camera->T);
	
	*u = .5 + pt[0]*camera->f_mm / (-pt[2]*camera->CCDWidth_mm);
	*v = .5 + pt[1]*camera->f_mm / (-pt[2]*camera->CCDHeight_mm);
}

static int is_face_in_camera_field_of_view (Mesh *mesh, unsigned int fi, BundleCamera *camera)
{
	float u, v;
	unsigned int vi;
	for (unsigned int k=0; k<mesh->GetFaceNVertices (fi); k++)
	{
		vi = mesh->GetFaceVertex (fi, k);
		project_texture_coordinates_on_vertex (mesh, vi, camera, &u, &v);
		if (u<0. || u>1. || v<0. || v>1.)
			return 0;
	}
	return 1;
}

//
// apply the texture ti onto the face fi
//
static void project_texture_on_face (Mesh *mesh, unsigned int fi, unsigned int ti, BundleCamera *camera)
{
	for (unsigned int k = 0; k < mesh->GetFaceNVertices (fi); k++)
	{
		float u, v;
		project_texture_coordinates_on_vertex (mesh, mesh->GetFaceVertex (fi, k), camera, &u, &v);

		//if (u>0. && u<1. && v>0. && v<1.)
		{
			mesh->SetFaceMaterialId (fi, ti);
			Face *face = mesh->GetFace (fi);
			face->SetTexCoord (k, u, v);
			face->m_bUseTextureCoordinates = true;
		}
		//else
		//	mesh_face_mi (mesh, fi) = bundle->n_cameras;
	}
}


int Bundle::project_textures_naive (Mesh *mesh, char *bundleoutfilename, char *imageslistfilename, char *rootpath)
{
	if (!mesh || !bundleoutfilename || !imageslistfilename)
		return -1;

	printf ("!!! compute face normals\n");
	printf ("!!! allocate memory for texcoord\n");
	printf ("!!! allocate memory for materials\n");

	// set the new textures
	for (unsigned int i=0; i<cameras_n; i++)
	{
		BundleCamera *camera = cameras[i];
		if (!camera)
			continue;

		// add new texture in the mesh
		Img *texture = new Img ();
		texture->load (camera->filename);
		camera->w = texture->m_iWidth;
		camera->h = texture->m_iHeight;
		MaterialTexture *pMaterial = new MaterialTexture (camera->filename);
		mesh->Material_Add (pMaterial);
		delete texture;
	}
	MaterialColor *pMaterialColor = new MaterialColor (255, 0, 0);
	mesh->Material_Add (pMaterialColor); // color for the faces not reached by the cameras

	for (unsigned int j=0; j<mesh->m_nFaces; j++)
	{
		vec3 fnormal;
		vec3_init (fnormal, mesh->m_pFaceNormals[3*j], mesh->m_pFaceNormals[3*j+1], mesh->m_pFaceNormals[3*j+2]);
		vec3_normalize (fnormal);
		float dot = 0.3;

		int camera_selected = -1;
		// test the current face with each camera
		for (unsigned int i=0; i<cameras_n; i++)
		{
			BundleCamera *camera = cameras[i];
			if (!camera)
				continue;

			vec3 ray, barycenter;
			mesh->GetFaceBarycenter (j, barycenter);
			vec3_subtraction (ray, camera->T, barycenter);
			vec3_normalize (ray);
			float dotcurrent = vec3_dot_product (ray, fnormal);

			//float dotcurrent = fvec3_dot_product (camera->d, fnormal);
			// offset the barycenter
			barycenter[0] += 0.001*fnormal[0];
			barycenter[1] += 0.001*fnormal[1];
			barycenter[2] += 0.001*fnormal[2];

			if (dotcurrent > dot)
			{
				dot = dotcurrent;
				camera_selected = (int)i;
			}
		}

		if (1 || camera_selected == -1)
		{
			mesh->SetFaceMaterialId (j, cameras_n);
			continue;
		}
		//printf ("camera selected : %d (%f)\n", camera_selected, dot);
		BundleCamera *camera = cameras[camera_selected];
		unsigned int texture_id = camera_selected;

		// apply texture to the face
		project_texture_on_face (mesh, j, texture_id, camera);
	}

	return 0;
}

typedef struct gc_extra_s {
	Mesh *mesh;
	Bundle *bundle;
} gc_extra_t;

static int _smooth_cost_func (int fi1, int fi2, int ti1, int ti2, void *d)
{
	gc_extra_t *gc_extra = (gc_extra_t*)d;
	Mesh *mesh = gc_extra->mesh;
	Bundle *bundle = gc_extra->bundle;
	if (!mesh || !bundle)
		return -1;

	BundleCamera *c1 = bundle->cameras[ti1];
	BundleCamera *c2 = bundle->cameras[ti2];
	if (!c1 || !c2)
		return 0;

	if (ti1 == ti2)
		return 0;

	// get the two vertices shared by the faces
	int i=0, vis[2];
	if (mesh->GetFaceVertex (fi1, 0) == mesh->GetFaceVertex (fi2, 0) ||
	    mesh->GetFaceVertex (fi1, 0) == mesh->GetFaceVertex (fi2, 1) ||
	    mesh->GetFaceVertex (fi1, 0) == mesh->GetFaceVertex (fi2, 2) )
		vis[i++] = mesh->GetFaceVertex (fi1, 0);
	if (mesh->GetFaceVertex (fi1, 1) == mesh->GetFaceVertex (fi2, 0) ||
	    mesh->GetFaceVertex (fi1, 1) == mesh->GetFaceVertex (fi2, 1) ||
	    mesh->GetFaceVertex (fi1, 1) == mesh->GetFaceVertex (fi2, 2) )
		vis[i++] = mesh->GetFaceVertex (fi1, 1);
	if (mesh->GetFaceVertex (fi1, 2) == mesh->GetFaceVertex (fi2, 0) ||
	    mesh->GetFaceVertex (fi1, 2) == mesh->GetFaceVertex (fi2, 1) ||
	    mesh->GetFaceVertex (fi1, 2) == mesh->GetFaceVertex (fi2, 2) )
		vis[i++] = mesh->GetFaceVertex (fi1, 1);

	if (i!=2)
		printf ("!!! vertices in common not found\n");
	int vi1 = vis[0], vi2 = vis[1];

	// texture coordinates in texture ti1
	float u1_t1, v1_t1, u2_t1, v2_t1;
	project_texture_coordinates_on_vertex (mesh, vi1, c1, &u1_t1, &v1_t1);
	project_texture_coordinates_on_vertex (mesh, vi2, c1, &u2_t1, &v2_t1);

	// texture coordinates in texture ti2
	float u1_t2, v1_t2, u2_t2, v2_t2;
	project_texture_coordinates_on_vertex (mesh, vi1, c1, &u1_t2, &v1_t2);
	project_texture_coordinates_on_vertex (mesh, vi2, c1, &u2_t2, &v2_t2);

	vec3 pt1, pt2;
	mesh->GetVertex (vi1, pt1);
	mesh->GetVertex (vi2, pt2);
	float l = vec3_distance (pt1, pt2);

	unsigned int n = (unsigned int)(10000.*l);
	int energy = 0;
	for (unsigned int i=0; i<n; i++)
	{
		// walking texture coordinates in texture ti1
		float u1 = u1_t1 + i*(u2_t1-u1_t1)/n;
		float v1 = v1_t1 + i*(v2_t1-v1_t1)/n;

		// walking color in ti1
		unsigned char r1, g1, b1, a1;
		MaterialTexture *texture1 = (MaterialTexture*)mesh->m_pMaterials[ti1];
		texture1->GetImage()->get_nearest_pixel (u1, v1, &r1, &g1, &b1, &a1);

		// walking texture coordinates in texture ti1
		float u2 = u1_t2 + i*(u2_t2-u1_t2)/n;
		float v2 = v1_t2 + i*(v2_t2-v1_t2)/n;

		// walking color in ti2
		unsigned char r2, g2, b2, a2;
		MaterialTexture *texture2 = (MaterialTexture*)mesh->m_pMaterials[ti2];
		texture2->GetImage()->get_nearest_pixel (u2, v2, &r2, &g2, &b2, &a2);

		energy += (int)0.5*sqrt((float)((r1-r2)*(r1-r2)+(g1-g2)*(g1-g2)+(b1-b2)*(b1-b2)));
	}

	return energy;
}

#if 0
//
// "Seamless Mosaicing of Image-Based Texture Maps"
// Victor Lempitsky & Denis Ivanov
// Computer Vision and Pattern Recognition, 2007. CVPR '07
// www.robots.ox.ac.uk/~vilem/SeamlessMosaicing.pdf
//
// implemented with GCoptimization Library (http://www.csd.uwo.ca/~olga/OldCode.html)
//
int Bundle::project_textures_lempitsky07 (Mesh *mesh, char *bundleoutfilename, char *imageslistfilename, char *rootpath)
{
	if (!mesh || !bundleoutfilename || !imageslistfilename)
		return -1;

	DeleteRedundantCameras ();
	/*
	mesh_resize (mesh, mesh->vm, mesh->fm, bundle->n_cameras);

	MESH_REQUIRE (mesh, MESH_FLAG_TRIANGULAR);
	MESH_REQUIRE (mesh, MESH_FLAG_KDTREE);
	MESH_REQUIRE (mesh, MESH_FLAG_FACE_NORMALS);
	MESH_REQUIRE (mesh, MESH_FLAG_TEXCOORD);
	MESH_REQUIRE (mesh, MESH_FLAG_EDGE_ADJACENCY);
	*/
	// set the new textures
	for (unsigned int i=0; i<cameras_n; i++)
	{
		BundleCamera *camera = cameras[i];
		if (!camera)
			continue;

		// add new texture in the mesh
		texture_t *texture = import_image (camera->filename);
		mesh_set_texture (mesh, i, texture);
		texture_delete (texture);

		//fvec3_normalize (camera->d);
		//mesh_set_color_index (mesh, i, camera->d[0], camera->d[1], camera->d[2]);
	}
	mesh_set_color_index (mesh, bundle->n_cameras, 1., 0., 0.); // color for the faces not reached by the cameras

	// create a general graph for graph cut
	void *gg = GC_GeneralGraph_create (mesh->fn, bundle->n_cameras);

	// evaluate neighborough
	for (unsigned int h = 0; h < mesh->em; h++)
	{
		corner_t c = mesh->e[h].estar;
		if (!IS_CORNER_VALID(c))
			continue;

		if (mesh->e[h].estarsize == 2)
		{
			unsigned int f1 = CORNER_FACE(c);
			c = mesh_face_estar(mesh, CORNER_FACE(c), CORNER_EDGE(c));
			unsigned int f2 = CORNER_FACE(c);
			GC_GeneralGraph_set_neighbors (gg, f1, f2);
		}
		else
		{
			//printf ("[warning] edge between %d & %d have starsize = %d\n",
			//	mesh->e[h].v[0], mesh->e[h].v[1], mesh->e[h].estarsize);
		}
	}

	// evaluate data costs
	int costmax = 50;
	int *data_cost = (int*)malloc(mesh->fn*bundle->n_cameras*sizeof(int));
	for (unsigned int i=0; i<mesh->fn; i++)
	{
		float dot = 0.3;
		vec3 fnormal;
		vec3_init (fnormal, mesh->fnormal[i][0], mesh->fnormal[i][1], mesh->fnormal[i][2]);
		vec3_normalize (fnormal);
		
		vec3 barycenter;
		mesh_face_barycenter (mesh, i, barycenter);

		int camera_selected = -1;

		// test the current face with each camera
		for (unsigned int j=0; j<bundle->n_cameras; j++)
		{
			camera_t *camera = bundle->cameras[j];
			if (!camera)
				continue;

			// if the face is not in the field of view of the camera
			if (!is_face_in_camera_field_of_view (mesh, i, camera))
			{
				data_cost[i*bundle->n_cameras+j] = costmax;
				continue;
			}

			// is the face occluded by another face ?
			vec3 ray;
			vec3_subtraction (ray, barycenter, camera->pos);
			vec3_normalize (ray);
 			float thit;
			if (mesh_kdtree_ray_intersect(mesh, camera->pos, ray, &thit) != i)
			{
				data_cost[i*bundle->n_cameras+j] = costmax;
				continue;
			}

			// evaluate the weight
			float dotcurrent = fvec3_dot_product (camera->d, fnormal);
			data_cost[i*bundle->n_cameras+j] = (dotcurrent>0.)?costmax:10;//90:100*(acos(dotcurrent));
			if (dotcurrent > dot)
			{
				dot = dotcurrent;
				camera_selected = (int)j;
			}
		}
	}
	GC_GeneralGraph_set_data_cost (gg, data_cost);

	// evaluate smooth costs
	gc_extra_t *gc_extra = (gc_extra_t*)malloc(sizeof(gc_extra_t));
	gc_extra->mesh = mesh;
	gc_extra->bundle = bundle;

	int (*SmoothCostFn)(int s1, int s2, int l1, int l2, void *d);
	SmoothCostFn = &_smooth_cost_func;

	GC_GeneralGraph_set_smooth_cost_func (gg, _smooth_cost_func, (void*)gc_extra);

	//
	int *results = (int*)malloc(mesh->fn*sizeof(int));
	printf ("energy before = %d\n", GC_GeneralGraph_get_energy (gg));
	GC_GeneralGraph_minimize (gg, results, mesh->fn);
	printf ("energy after  = %d\n", GC_GeneralGraph_get_energy (gg));

	// apply the result
	for (unsigned int i=0; i<mesh->fn; i++)
	{
		unsigned int label = results[i];
		if (data_cost[i*bundle->n_cameras+label] != costmax)
			project_texture_on_face (mesh, i, label, bundle->cameras[label]);
		else
			mesh_face_mi (mesh, i) = bundle->n_cameras;
	}

	//mesh_remove_faces_without_texture (mesh);

	// cleaning
	free (gc_extra);
	free (results);
	free (data_cost);

	return 0;
}

//
//
//
int convert_vlleafsift_to_siftlowe (char *keyfile_vlfeat_format, char *keyfile_lowe_format)
{
	int ret = 0;
	FILE *in = fopen (keyfile_vlfeat_format, "r");
	if (!in)
		return -1;

	FILE *out = fopen (keyfile_lowe_format, "w");
	if (!out)
	{
		fclose (in);
		return -1;
	}

	char buffer[1024];
	int nFeatures = 0;
	do {
		fgets (buffer, 1024, in);
		nFeatures++;
	} while (!feof(in));
	nFeatures--;
	printf ("[%d features extracted]\n", nFeatures);
	fprintf (out, "%d 128\n", nFeatures);
	rewind (in);

	do {
		fgets (buffer, 1024, in);
		unsigned int cnt = 0, istart = 0;
		for (unsigned int i=0; i<strlen(buffer); i++)
		{
			if (buffer[i] == ' ')
			{
				cnt++;
				if (cnt == 4 || cnt == 24 || cnt == 44 || cnt == 64 || cnt == 84 || cnt == 104 || cnt == 124)
					buffer[i] = '\n';
				if (cnt == 4)
					istart = i+1;
			}
		}
		float p[4];
		sscanf (buffer, "%f %f %f %f", &p[0], &p[1], &p[2], &p[3]);
		fprintf (out, "%f %f %f %f\n", p[1], p[0], p[2], p[3]);
		fprintf (out, "%s", buffer+istart);
	} while (!feof(in));
	
	fclose (out);
	fclose (in);

	return ret;
}
#endif
