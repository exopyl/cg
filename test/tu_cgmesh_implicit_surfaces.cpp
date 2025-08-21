#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"


static void mc_to_obj(ImplicitSurface* mc)
{
	// get the triangulation
	int nvertices, nfaces;
	unsigned int* faces;
	float* vertices;
	mc->get_triangulation(&nvertices, &vertices, &nfaces, &faces);
	printf("---> %d\n", nfaces);

	// export the result (obj format)
	FILE* ptr = fopen("export.obj", "w");
	for (int i = 0; i < nvertices; i++)
		fprintf(ptr, "v %f %f %f\n",
			vertices[3 * i] / 1000., vertices[3 * i + 1] / 1000., vertices[3 * i + 2] / 5000.);

	for (int i = 0; i < nfaces; i++)
		fprintf(ptr, "f %d %d %d\n", 1 + faces[3 * i], 1 + faces[3 * i + 1], 1 + faces[3 * i + 2]);
	fclose(ptr);

	// cleaning
	free(vertices);
	free(faces);
	delete mc;
}

static float eval(float x, float y, float z)
{
	return sqrt(x * x + y * y + z * z);
}

static Img* img = NULL;
static float eval_image(float x, float y, float z)
{
	unsigned char r = img->get_r((int)x, (int)y);
	if (((int)x) % 2 == 0)
		return 1. - ((float)r / 255.);
	else
		return -1.;
}

TEST(TEST_cgmesh_implicit_surface, ok)
{
    // action
	ImplicitSurface* mc = new ImplicitSurface();

	mc->set_bbox(-1., -1., -1., 1., 1., 1.);
	mc->set_resolution_per_unit(20);
	mc->set_boundary(1);
	mc->set_orientation(1);
	mc->set_color_func(NULL);
	mc->set_vertex_func(NULL);
	mc->set_normal_func(NULL);
	//mc->set_eval_func (eval);
	mc->set_eval_func(fSample2);
	mc->set_value(.5);

	mc_to_obj(mc);
}

TEST(TEST_cgmesh_implicit_surface, image)
{
    // action
	ImplicitSurface* mc = new ImplicitSurface();

	img = new Img();
	img->load("./test/data/tga/ctc16.tga");

	mc->set_bbox(1., 1., 0., img->width() - 2, img->height() - 2, 1.);
	mc->set_resolution_per_unit(1);
	mc->set_boundary(1);
	mc->set_orientation(1);
	mc->set_color_func(NULL);
	mc->set_vertex_func(NULL);
	mc->set_normal_func(NULL);
	//mc->set_eval_func (eval);
	mc->set_eval_func(eval_image);
	mc->set_value(0.);

	mc_to_obj(mc);

	delete img;
}
