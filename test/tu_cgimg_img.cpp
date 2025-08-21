#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

TEST(TEST_cgimg_img, generations)
{
	Img *img = new Img ();

	img->init_test_grayscale1 (100);
	img->save ((char*)"./img_grayscale1.pgm");

	img->init_test_grayscale2 (50);
	img->save ((char*)"./img_grayscale2.pgm");

	img->init_test_color_jet (256, 100);
	img->save ((char*)"./img_color_jet.ppm");
}

TEST(TEST_cgimg_img, binarization)
{
	Img* img = new Img();
	img->init_test_grayscale2(50);

	img->convert_to_grayscale ();
	// threshold
	{
		Img *imgc = new Img (*img);

		unsigned char t = imgc->get_mean_value ();
		imgc->bin_threshold (t);
		imgc->save ((char*)"./img_bin_threshold.pgm");

		delete imgc;
	}

	// otsu
	{
		Img *imgc = new Img (*img);

		imgc->bin_otsu ();
		imgc->save ((char*)"./img_bin_otsu.pgm");

		delete imgc;
	}

	// floyd steinberg
	{
		Img *imgc = new Img (*img);

		imgc->bin_floyd_steinberg ();
		imgc->save ((char*)"./img_bin_floyd_steinberg.pgm");

		delete imgc;
	}

	// dithering
	{
		unsigned char pattern[9] = {6, 8, 4,
					    1, 0, 3,
					    5, 2, 7};
		int psize = 3;
/*
  unsigned char pattern[64] = {24, 10, 12, 26, 35, 47, 49, 37,
  8, 0, 2, 14, 45, 59, 61, 51,
  22, 6, 4, 16, 43, 57, 63, 53,
  30, 20, 18, 28, 33, 41, 55, 39,
  34, 46, 48, 36, 25, 11, 13, 27,
  44, 58, 60, 50, 9, 1, 3, 15,
  42, 56, 62, 23, 7, 5, 17,
  32, 40, 54, 38, 31, 21, 19, 29};
  int psize = 8;
*/

		Img *imgc = new Img ();
		imgc->crop (img, 0, 0, img->m_iWidth - img->m_iWidth%psize, img->m_iHeight - img->m_iHeight%psize);
		
		imgc->bin_dithering (pattern, psize);
		imgc->save ((char*)"./img_bin_dithering.pgm");

		delete imgc;
	}

	// screening
	{
		Img *imgc = new Img (*img);
		Img *imgPattern = new Img ();
		imgPattern->load ((char*)"./test/data/halftone.pgm");

		imgc->bin_screening (imgPattern);
		imgc->save ((char*)"./img_bin_screening.pgm");

		delete imgPattern;
		delete imgc;
	}
}

TEST(TEST_cgimg_img, quantization)
{
	Img* img = new Img();
	img->init_test_grayscale2(50);

	// heckbert
	{
		Img *imgc = new Img (*img);
		imgc->quant_heckbert (16);
		imgc->save ((char*)"./img_quant_heckbert.ppm");
		delete imgc;
	}

	// wu
	{
		Img *imgc = new Img (*img);
		imgc->quant_wu (16);
		imgc->save ((char*)"./img_quant_wu.ppm");
		delete imgc;
	}

	// kmean
	if (0) {
		Img *imgc = new Img (*img);
		imgc->quant_kmean (0.05);
		imgc->save ((char*)"./img_quant_kmean.ppm");
		delete imgc;
	}
}

TEST(TEST_cgimg_img, filter)
{
	Img* img = new Img();
	img->init_test_grayscale2(50);

	mat3 filter;
	mat3_init (filter,
		   1., 1., 1.,
		   1., 1., 1.,
		   1., 1., 1.);

	// passe haut
	{
		mat3_init (filter,
			   0., -1., 0.,
			   -1., 5., -1.,
			   0., -1., 0.);
		Img *imgc = new Img (*img);
		imgc->filter (filter);
		imgc->save ((char*)"./img_filter_passe_haut.ppm");
		delete imgc;
	}

	// passe bas
	{
		mat3_init (filter,
			   1., 1., 1.,
			   1., 4., 1.,
			   1., 1., 1.);
		Img *imgc = new Img (*img);
		imgc->filter (filter);
		imgc->save ((char*)"./img_filter_passe_bas.ppm");
		delete imgc;
	}

	// laplacian
	{
		mat3_init (filter,
			   -1., -1., -1.,
			   -1.,  8., -1.,
			   -1., -1., -1.);
/*
		mat3_init (filter,
			   0., -1.,  0.,
			   -1.,  4., -1.,
			   0., -1.,  0.);
		mat3_init (filter,
			   1., -2.,  1.,
			   -2.,  4., -2.,
			   1., -2.,  1.);
*/
		Img *imgc = new Img (*img);
		imgc->filter (filter);
		imgc->save ((char*)"./img_filter_laplacian.ppm");
		delete imgc;
	}

	// gradient
	{
		mat3_init (filter,
			   -1., -1., -1.,
			   1.,  1.,  1.,
			   0.,  0.,  0.);
		mat3_init (filter,
			   -1., 1., 0.,
			   -1., 1., 0.,
			   -1., 1., 0.);
		Img *imgc = new Img (*img);
		imgc->filter (filter);
		imgc->save ((char*)"./img_filter_gradient.ppm");
		delete imgc;
	}

	// sobel
	{
		Img *imgc = new Img (*img);
		imgc->filter_sobel ();
		imgc->save ((char*)"./img_filter_sobel.ppm");
		delete imgc;
	}

	// bilateral filtering
	{
		Img *imgc = new Img (*img);
		imgc->bilateral_filtering ();
		imgc->save ((char*)"./img_filter_bilateral.ppm");
		delete imgc;
	}
}


TEST(TEST_cgimg_img, filter2)
{
#if 0
	Img* img = new Img();
	img->init_test_grayscale2(50);

	Img* snow = new Img();
	snow->load("./test/data/fallout_mask.png");
	snow->resize(512, 512, 1);
	//img->smooth_transition (5);
	//img->bilateral_filtering ();
	//img->saturate (1.9);
	img->resize(snow->width(), snow->height());
	img->convert_to_grayscale();
	img->blur();
	img->sepia();
	img->multiply(snow);
	img->brightness(2.1);
	img->save("./img_filter2.png");
	//snow->brightness(2.1);
	//snow->save("../fallout_mask2.png");
#endif
}

TEST(TEST_cgimg_img, drawing)
{
	Img* img = new Img();
	img->init_test_grayscale2(50);

	Img *imgc = new Img (*img);
	imgc->draw_line(10, 50, img->m_iWidth-50, img->m_iHeight-100, 255, 0, 0, 0);
	//imgc->draw_disk(img->m_iWidth/2., img->m_iHeight/3., 60, 0, 255, 0, 0);
	//imgc->draw_circle(img->m_iWidth/2., img->m_iHeight/2., 36, 0, 0, 255, 0);
	imgc->draw_ellipse(img->m_iWidth/2., img->m_iHeight/3., 100, 50, 255, 0, 0, 255);

	vec2 line_start, line_end;
	vec2_init (line_start, 10, 50);
	vec2_init (line_end, img->m_iWidth-50, img->m_iHeight-100);
	vec2 ellipse_center, ellipse_radius;
	vec2_init (ellipse_center, img->m_iWidth/2., img->m_iHeight/3.);
	vec2_init (ellipse_radius, 100, 50);
	vec2 res1, res2;
	int nres = line_ellipse_intersection (line_start, line_end, ellipse_center, ellipse_radius, res1, res2);
	if (nres == 2)
	{
		imgc->draw_disk (res1[0], res1[1], 5, 255, 0, 0, 255);
		imgc->draw_disk (res2[0], res2[1], 5, 255, 0, 0, 255);
	}
	else if (nres == 1)
	{
		imgc->draw_disk (res1[0], res1[1], 5, 255, 0, 0, 255);
	}

	//imgc->draw_circle(img->m_iWidth/2., img->m_iHeight/2., 36, 0, 0, 255, 0);
	

	imgc->save ((char*)"./img_drawing.ppm");
	delete imgc;
}

TEST(TEST_cgimg_img, histogram)
{
	Img* img = new Img();
	img->init_test_grayscale2(50);
	
	img->convert_to_grayscale ();
	Img *imgc = new Img (*img);

	imgc->save ((char*)"./img_histo_orig.ppm");

	float h[256];
	imgc->get_histogram (h);
	output_1array (h, 256, (char*)"histogram1.dat");
	for (int i=1; i<256; i++)
		h[i] += h[i-1];
	output_1array (h, 256, (char*)"histogram11.dat");

	Img *histo = imgc->get_histogram_img (200);
	histo->save ((char*)"./img_histo_histo_orig.ppm");
	delete histo;

	imgc->histogram_equalization ();
	imgc->save ((char*)"./data_generated/img_histo_equalized.ppm");
	histo = imgc->get_histogram_img (200);
	histo->save ((char*)"./data_generated/img_histo_histo_equalized.ppm");
	delete histo;

	delete imgc;

/*

	//img->histogram_equalization_bezier ();
	img->contrast (0.);

	img->get_histogram (h);
	output_1array (h, 256, (char*)"histogram2.dat");
	for (int i=1; i<256; i++)
		h[i] += h[i-1];
	output_1array (h, 256, (char*)"histogram22.dat");
*/
}

TEST(TEST_cgimg_img, disparity)
{
	Img *pLeft = new Img ();
	Img *pRight = new Img ();
	
	if (0)
	{
		pLeft->load ("./data/SONY_DSC-W55/1L_50pc.jpg");
		pRight->load ("./data/SONY_DSC-W55/1R_50pc.jpg");
		DisparityEvaluator *pDE = new DisparityEvaluator();
		pDE->SetStereoPair (pLeft, pRight);
		pDE->Compute ();
		Img *pDisparity = pDE->GetDisparity ();
		pDisparity->save ("./data_generated/disparity.jpg");

		delete pDE;
		delete pRight;
		delete pLeft;
	}
	else
	{
		pLeft->load ("./test/data/meterbig_l.pgm");
		pRight->load ("./test/data/meterbig_r.pgm");
		pLeft->save ("./meterbig_l.bmp");
		pRight->save ("./meterbig_r.bmp");
		printf ("%d %d\n", pLeft->width(), pLeft->height());
		printf ("%d %d\n", pRight->width(), pRight->height());
		
		DisparityBirchfield *pBirchfield = new DisparityBirchfield();
		pBirchfield->SetStereoPair (pLeft, pRight);
		pBirchfield->Process ();
		Img *pDisparity = pBirchfield->GetDisparity ();
		pDisparity->save ("./disparity.bmp");

		delete pBirchfield;
		delete pRight;
		delete pLeft;
	}
}

TEST(TEST_cgimg_img, geodesic)
{
	Img *img = new Img (100, 100);
	img->init_color (255, 255, 255, 255);
	img->set_pixel (50, 50, 0, 0, 0, 255);
	img->set_pixel (25, 50, 0, 0, 0, 255);
	img->set_pixel (75, 75, 0, 0, 0, 255);

	img->geodesic ();
	img->save ("./toto.ppm");
}

TEST(TEST_cgimg_img, vectorization)
{
	Img* img = new Img();
	img->init_test_grayscale2(50);
	
	// colors in the image
	Palette *pPalette = NULL;
	
	if (1)
		pPalette = img->get_palette ();
	else
	{
		pPalette = new Palette ();
		pPalette->AddColor (255, 255, 255, 255);
		pPalette->AddColor (255, 0, 0, 255);
		pPalette->AddColor (85, 203, 65, 255);

/*
		pPalette->AddColor (54, 154, 201, 255);
		pPalette->AddColor (203, 43, 43, 255);
		pPalette->AddColor (241, 173, 43, 255);
		pPalette->AddColor (55, 155, 59, 255);
*/
	}
	pPalette->dump ();

	CLitRasterToVector *pRasterToVector = new CLitRasterToVector ();
	bool bOk = pRasterToVector->Vectorize(img,
					      Color (255, 255, 255),
					      false,
					      pPalette);
	
	if (bOk)
	{
		pRasterToVector->WriteFile(.1);
		//pRasterToVector->WriteFilePolygonWithHole(true,false,true,NULL,.1,Color(0,0,0),false);
		pRasterToVector->WriteFilePolygonWithHole(.1);
		pRasterToVector->WriteFile(.1);
	}
	
	delete pPalette;
}

static void temperature2color(float temp, unsigned char& r, unsigned char& g, unsigned char& b)
{
	temp /= 100;
	// red
	if (temp <= 66)
		r = 255;
	else
	{
		double dr = temp-60;
		dr = 329.698727446 * pow((double)dr, -0.1332047592);
		if (dr < 0)
			r = 0;
		else if (dr > 255)
			r = 255;
		else
			r = dr;
	}

	// green
	if (temp <= 66)
	{
		double dg = temp;
		dg = 99.4708025861 * log((double)dg) - 161.1195681661;
		if (dg < 0)
			g = 0;
		else if (g > 255)
			g = 255;
		else
			g = dg;
	}
	else
	{
		double dg = temp-60;
		dg = 288.1221695283 * pow ((double)dg, -0.0755148492);
		if (dg < 0)
			g = 0;
		else if (dg > 255)
			g = 255;
		else g = dg;
	}

	// blue
	if (temp >= 66)
		b = 255;
	else
	{
		if (temp <= 19)
			b = 0;
		else
		{
			double db = temp - 10;
			db = 138.5177312231 * log((double)db) - 305.0447927307;
			if (db < 0)
				b = 0;
			else if (db > 255)
				b = 255;
			else
				b = db;
		}
	}
}

TEST(TEST_cgimg_img, temperature2color)
{
	int width = 300;
	int height = 100;
	Img* pImg = new Img(width, height);
	for (int i=0; i<width; i++)
	{
		unsigned char r, g, b;
		float temp = 1000 + (11000-1000) * i / (width-1); // [1000 , 11000]
		printf ("%f\n", temp);
		temperature2color(temp, r, g, b);
		for (int j=0; j<height; j++)
		{
			pImg->set_pixel(i, j, r, g, b, 255);
		}
	}
	pImg->save("./toto.bmp");
}

