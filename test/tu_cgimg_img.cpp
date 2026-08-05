#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

#include <set>
#include <vector>

// Row-major 3x3 fill (replaces the removed C-API mat3_init for the filter kernels).
static void set3x3 (float m[3][3], float a, float b, float c,
		    float d, float e, float f,
		    float g, float h, float i)
{
	m[0][0]=a; m[0][1]=b; m[0][2]=c;
	m[1][0]=d; m[1][1]=e; m[1][2]=f;
	m[2][0]=g; m[2][1]=h; m[2][2]=i;
}

TEST(TEST_cgimg_img, generations)
{
	Img *img = new Img ();

	ImgTestPattern::grayscale1 (*img, 100);
	img->save ((char*)"./img_grayscale1.pgm");

	ImgTestPattern::grayscale2 (*img, 50);
	img->save ((char*)"./img_grayscale2.pgm");

	ImgTestPattern::color_jet (*img, 256, 100);
	img->save ((char*)"./img_color_jet.ppm");
}

TEST(TEST_cgimg_img, binarization)
{
	Img* img = new Img();
	ImgTestPattern::grayscale2 (*img, 50);

	img->convert_to_grayscale ();
	// threshold
	{
		Img *imgc = new Img (*img);

		unsigned char t = imgc->get_mean_value ();
		ImgBinarize::threshold (*imgc, t);
		imgc->save ((char*)"./img_bin_threshold.pgm");

		delete imgc;
	}

	// otsu
	{
		Img *imgc = new Img (*img);

		ImgBinarize::otsu (*imgc);
		imgc->save ((char*)"./img_bin_otsu.pgm");

		delete imgc;
	}

	// floyd steinberg
	{
		Img *imgc = new Img (*img);

		ImgBinarize::floyd_steinberg (*imgc);
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
		imgc->crop (img, 0, 0, img->width() - img->width()%psize, img->height() - img->height()%psize);
		
		ImgBinarize::dithering (*imgc, pattern, psize);
		imgc->save ((char*)"./img_bin_dithering.pgm");

		delete imgc;
	}

	// screening
	{
		Img *imgc = new Img (*img);
		Img *imgPattern = new Img ();
		imgPattern->load ((char*)"./test/data/halftone.pgm");

		ImgBinarize::screening (*imgc, imgPattern);
		imgc->save ((char*)"./img_bin_screening.pgm");

		delete imgPattern;
		delete imgc;
	}
}

TEST(TEST_cgimg_img, quantization)
{
	Img* img = new Img();
	ImgTestPattern::grayscale2 (*img, 50);

	// heckbert
	{
		Img *imgc = new Img (*img);
		ImgQuantize::heckbert (*imgc, 16);
		imgc->save ((char*)"./img_quant_heckbert.ppm");
		delete imgc;
	}

	// wu
	{
		Img *imgc = new Img (*img);
		ImgQuantize::wu (*imgc, 16);
		imgc->save ((char*)"./img_quant_wu.ppm");
		delete imgc;
	}

	// kmean
	{
		Img *imgc = new Img (*img);
		ImgQuantize::kmean (*imgc, 0.05);
		imgc->save ((char*)"./img_quant_kmean.ppm");
		delete imgc;
	}
}

// Erreur quadratique moyenne par canal entre deux images de meme taille.
static double img_mse (Img &a, Img &b)
{
	double s = 0.;
	for (unsigned y = 0; y < a.height(); y++)
		for (unsigned x = 0; x < a.width(); x++)
		{
			unsigned char r1, g1, b1, a1, r2, g2, b2, a2;
			a.get_pixel (x, y, &r1, &g1, &b1, &a1);
			b.get_pixel (x, y, &r2, &g2, &b2, &a2);
			s += (double)(r1-r2)*(r1-r2) + (double)(g1-g2)*(g1-g2) + (double)(b1-b2)*(b1-b2);
		}
	return s / (3. * a.width() * a.height());
}

// quant_refine implemente le pas de Lloyd : il MINIMISE l'erreur quadratique,
// donc la MSE ne peut que baisser -- et elle baisse pour les deux quantifieurs,
// qui decident tous deux sur un histogramme 5 bits/canal.
TEST(TEST_cgimg_img, quant_refine_reduces_error)
{
	Img ref;
	ImgTestPattern::color_jet (ref, 128, 96);   // degrade continu : beaucoup de couleurs

	for (int ncolors : { 4, 8, 16 })
	{
		Img wu = ref;
		ImgQuantize::wu (wu, ncolors);
		const double before = img_mse (ref, wu);

		Img wuR = ref;
		ImgQuantize::wu (wuR, ncolors);
		const int k = ImgQuantize::refine (wuR, ref, 4);
		const double after = img_mse (ref, wuR);

		EXPECT_GT (k, 0) << "la palette doit etre trouvee (ncolors=" << ncolors << ")";
		EXPECT_LE (k, ncolors) << "le raffinement n'ajoute jamais de couleur";
		EXPECT_LE (after, before + 1e-9)
			<< "Lloyd ne peut pas degrader la MSE (ncolors=" << ncolors
			<< ") : " << before << " -> " << after;

		// Heckbert part de bien plus loin ; le raffinement doit le rattraper.
		Img hb = ref;
		ImgQuantize::heckbert (hb, ncolors);
		const double hbBefore = img_mse (ref, hb);
		Img hbR = ref;
		ImgQuantize::heckbert (hbR, ncolors);
		ImgQuantize::refine (hbR, ref, 4);
		const double hbAfter = img_mse (ref, hbR);
		EXPECT_LE (hbAfter, hbBefore + 1e-9)
			<< "idem pour Heckbert : " << hbBefore << " -> " << hbAfter;

		printf ("  ncolors=%2d : Wu %8.1f -> %8.1f | Heckbert %8.1f -> %8.1f\n",
		        ncolors, before, after, hbBefore, hbAfter);
	}
}

// Garde-fous : dimensions incompatibles ou image palettisee -> refus explicite.
TEST(TEST_cgimg_img, quant_refine_rejects_mismatched_input)
{
	Img a (32, 32, false);
	a.init_color (10, 20, 30, 255);
	Img small (16, 16, false);
	small.init_color (10, 20, 30, 255);

	EXPECT_EQ (ImgQuantize::refine (a, small, 2), -1) << "dimensions differentes";
	EXPECT_EQ (ImgQuantize::refine (a, a, 0), 0)      << "0 iteration = no-op";
}

TEST(TEST_cgimg_img, filter)
{
	Img* img = new Img();
	ImgTestPattern::grayscale2 (*img, 50);

	float filter[3][3];
	set3x3 (filter,
		   1., 1., 1.,
		   1., 1., 1.,
		   1., 1., 1.);

	// passe haut
	{
		set3x3 (filter,
			   0., -1., 0.,
			   -1., 5., -1.,
			   0., -1., 0.);
		Img *imgc = new Img (*img);
		ImgFilter::convolve (*imgc, filter);
		imgc->save ((char*)"./img_filter_passe_haut.ppm");
		delete imgc;
	}

	// passe bas
	{
		set3x3 (filter,
			   1., 1., 1.,
			   1., 4., 1.,
			   1., 1., 1.);
		Img *imgc = new Img (*img);
		ImgFilter::convolve (*imgc, filter);
		imgc->save ((char*)"./img_filter_passe_bas.ppm");
		delete imgc;
	}

	// laplacian
	{
		set3x3 (filter,
			   -1., -1., -1.,
			   -1.,  8., -1.,
			   -1., -1., -1.);
/*
		set3x3 (filter,
			   0., -1.,  0.,
			   -1.,  4., -1.,
			   0., -1.,  0.);
		set3x3 (filter,
			   1., -2.,  1.,
			   -2.,  4., -2.,
			   1., -2.,  1.);
*/
		Img *imgc = new Img (*img);
		ImgFilter::convolve (*imgc, filter);
		imgc->save ((char*)"./img_filter_laplacian.ppm");
		delete imgc;
	}

	// gradient
	{
		set3x3 (filter,
			   -1., -1., -1.,
			   1.,  1.,  1.,
			   0.,  0.,  0.);
		set3x3 (filter,
			   -1., 1., 0.,
			   -1., 1., 0.,
			   -1., 1., 0.);
		Img *imgc = new Img (*img);
		ImgFilter::convolve (*imgc, filter);
		imgc->save ((char*)"./img_filter_gradient.ppm");
		delete imgc;
	}

	// sobel
	{
		Img *imgc = new Img (*img);
		ImgFilter::sobel (*imgc);
		imgc->save ((char*)"./img_filter_sobel.ppm");
		delete imgc;
	}

	// bilateral filtering
	{
		Img *imgc = new Img (*img);
		ImgFilter::bilateral (*imgc);
		imgc->save ((char*)"./img_filter_bilateral.ppm");
		delete imgc;
	}
}


TEST(TEST_cgimg_img, filter2)
{
#if 0
	Img* img = new Img();
	ImgTestPattern::grayscale2 (*img, 50);

	Img* snow = new Img();
	snow->load("./test/data/fallout_mask.png");
	snow->resize(512, 512, 1);
	//ImgDraw::smooth_transition (*img, 5);
	//ImgFilter::bilateral (*img);
	//ImgFilter::saturate (*img, 1.9);
	img->resize(snow->width(), snow->height());
	img->convert_to_grayscale();
	ImgFilter::blur (*img);
	ImgFilter::sepia (*img);
	img->multiply(snow);
	ImgFilter::brightness (*img, 2.1);
	img->save("./img_filter2.png");
	//ImgFilter::brightness (*snow, 2.1);
	//snow->save("../fallout_mask2.png");
#endif
}

TEST(TEST_cgimg_img, drawing)
{
	Img* img = new Img();
	ImgTestPattern::grayscale2 (*img, 50);

	Img *imgc = new Img (*img);
	ImgDraw::line (*imgc, 10, 50, img->width()-50, img->height()-100, 255, 0, 0, 0);
	//ImgDraw::disk (*imgc, img->width()/2., img->height()/3., 60, 0, 255, 0, 0);
	//ImgDraw::circle (*imgc, img->width()/2., img->height()/2., 36, 0, 0, 255, 0);
	ImgDraw::ellipse (*imgc, img->width()/2., img->height()/3., 100, 50, 255, 0, 0, 255);

	Vector2f line_start, line_end;
	line_start.Set (10, 50);
	line_end.Set (img->width()-50, img->height()-100);
	Vector2f ellipse_center, ellipse_radius;
	ellipse_center.Set (img->width()/2., img->height()/3.);
	ellipse_radius.Set (100, 50);
	Vector2f res1, res2;
	int nres = line_ellipse_intersection (line_start, line_end, ellipse_center, ellipse_radius, res1, res2);
	if (nres == 2)
	{
		ImgDraw::disk (*imgc, res1[0], res1[1], 5, 255, 0, 0, 255);
		ImgDraw::disk (*imgc, res2[0], res2[1], 5, 255, 0, 0, 255);
	}
	else if (nres == 1)
	{
		ImgDraw::disk (*imgc, res1[0], res1[1], 5, 255, 0, 0, 255);
	}

	//ImgDraw::circle (*imgc, img->width()/2., img->height()/2., 36, 0, 0, 255, 0);
	

	imgc->save ((char*)"./img_drawing.ppm");
	delete imgc;
}

TEST(TEST_cgimg_img, histogram)
{
	Img* img = new Img();
	ImgTestPattern::grayscale2 (*img, 50);
	
	img->convert_to_grayscale ();
	Img *imgc = new Img (*img);

	imgc->save ("./img_histo_orig.ppm");

	float h[256];
	ImgHistogram::compute (*imgc, h);
	output_1array (h, 256, "histogram1.dat");
	for (int i=1; i<256; i++)
		h[i] += h[i-1];
	output_1array (h, 256, "histogram11.dat");

	Img *histo = ImgHistogram::to_image (*imgc, 200);
	histo->save ("./img_histo_histo_orig.ppm");
	delete histo;

	ImgHistogram::equalize (*imgc);
	imgc->save ("./data_generated/img_histo_equalized.ppm");
	histo = ImgHistogram::to_image (*imgc, 200);
	histo->save ("./data_generated/img_histo_histo_equalized.ppm");
	delete histo;

	delete imgc;

/*

	//ImgHistogram::equalize_bezier (*img);
	img->contrast (0.);

	ImgHistogram::compute (*img, h);
	output_1array (h, 256, "histogram2.dat");
	for (int i=1; i<256; i++)
		h[i] += h[i-1];
	output_1array (h, 256, "histogram22.dat");
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

	ImgGeodesic::apply (*img);
	img->save ("./toto.ppm");
}

// Fill an RGBA image with a solid colour.
static void fill_rect(Img& img, int x0, int y0, int x1, int y1,
                      unsigned char r, unsigned char g, unsigned char b)
{
	for (int y = y0; y < y1; ++y)
		for (int x = x0; x < x1; ++x)
			img.set_pixel((unsigned)x, (unsigned)y, r, g, b, 255);
}

// Smoke run (kept): the historical grayscale gradient must still vectorize.
TEST(TEST_cgimg_img, vectorization)
{
	Img img;
	ImgTestPattern::grayscale2 (img, 50);

	CLitRasterToVector rtv;
	bool bOk = rtv.Vectorize(&img, Color(255, 255, 255), false, /*palette=*/nullptr);
	EXPECT_TRUE(bOk);
	if (bOk)
	{
		EXPECT_GT(rtv.GetNCoords(), 0);
		EXPECT_GT(rtv.GetNPaths(),  0);
	}
}

// Verifiable behaviour on controlled inputs: a plain image vs. one carrying an
// extra coloured region. Vectorizing must (a) succeed, (b) yield a non-empty
// path/coord set, and (c) detect MORE colour layers / paths when a distinct
// region is present.
TEST(TEST_cgimg_img, vectorization_detects_regions)
{
	const int W = 32, H = 32;

	// (1) uniform white image -> a single colour region
	Img plain(W, H, false);
	fill_rect(plain, 0, 0, W, H, 255, 255, 255);

	CLitRasterToVector rtvPlain;
	ASSERT_TRUE(rtvPlain.Vectorize(&plain, Color(255,255,255), false, nullptr));
	const int layersPlain = rtvPlain.GetNColorLayers();
	const int pathsPlain  = rtvPlain.GetNPaths();

	// (2) same white image + a red square in the middle -> two colour regions
	Img squared(W, H, false);
	fill_rect(squared, 0, 0, W, H, 255, 255, 255);
	fill_rect(squared, 10, 10, 22, 22, 255, 0, 0);

	CLitRasterToVector rtvSq;
	ASSERT_TRUE(rtvSq.Vectorize(&squared, Color(255,255,255), false, nullptr));
	const int layersSq = rtvSq.GetNColorLayers();
	const int pathsSq  = rtvSq.GetNPaths();
	const int coordsSq = rtvSq.GetNCoords();

	printf("vectorize plain: layers=%d paths=%d | squared: layers=%d paths=%d coords=%d\n",
	       layersPlain, pathsPlain, layersSq, pathsSq, coordsSq);

	// non-empty, consistent result
	EXPECT_GE(pathsSq,  1);
	EXPECT_GT(coordsSq, 0);

	// one colour layer per distinct colour: plain=1 (white), squared=2 (white+red)
	EXPECT_EQ(layersPlain, 1);
	EXPECT_EQ(layersSq,    2) << "white + red square must give exactly two colour layers";
	// the extra region adds at least one closed contour
	EXPECT_GT(pathsSq, pathsPlain) << "the red square must add a path";
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
		temperature2color(temp, r, g, b);
		for (int j=0; j<height; j++)
		{
			pImg->set_pixel(i, j, r, g, b, 255);
		}
	}
	pImg->save("./toto.bmp");
}

TEST(TEST_cgimg_img, invert)
{
    // Context
    Img img(10, 10);
    img.init_color(100, 100, 100, 255);
    img.set_pixel(0, 0, 50, 50, 50, 255);

    // Action
    img.invert();

    // Expectations
    unsigned char r, g, b, a;
    img.get_pixel(0, 0, &r, &g, &b, &a);
    EXPECT_EQ(r, 205); // 255 - 50
    img.get_pixel(5, 5, &r, &g, &b, &a);
    EXPECT_EQ(r, 155); // 255 - 100
}

TEST(TEST_cgimg_img, contrast)
{
    // Context
    Img img(10, 10);
    img.init_color(100, 100, 100, 255);
    img.set_pixel(0, 0, 50, 50, 50, 255);

    // Action
    img.contrast(1.5f);

    // Expectations
    unsigned char r, g, b, a;
    img.get_pixel(0, 0, &r, &g, &b, &a);
    EXPECT_NE(r, 50);
}

TEST(TEST_cgimg_img, get_mean_value)
{
    // Context
    Img img(10, 10);
    img.init_color(100, 100, 100, 255);

    // Action
    int mean = img.get_mean_value();

    // Expectations
    EXPECT_EQ(mean, 100);
}

TEST(TEST_cgimg_img, get_median_value)
{
    // Context
    Img img(10, 10);
    img.init_color(0, 0, 0, 255);
    for(int i=0; i<100; ++i) img.data()[4*i] = i * 2;

    // Action
    int median = img.get_median_value();

    // Expectations
    EXPECT_NEAR(median, 100, 5);
}

TEST(TEST_cgimg_img, resize)
{
    // Context
    Img img(10, 10);

    // Action
    img.resize(20, 20);

    // Expectations
    EXPECT_EQ(img.width(), 20);
    EXPECT_EQ(img.height(), 20);
}

TEST(TEST_cgimg_img, resize_pixel)
{
    // Context
    Img img(10, 10);

    // Action
    img.resize_pixel(2);

    // Expectations
    EXPECT_EQ(img.width(), 20);
    EXPECT_EQ(img.height(), 20);
}

// Régression : en bilinéaire (mode 1), resize() lisait hors du buffer source sur
// la dernière colonne/ligne (x1=x0+1 / y1=y0+1 atteignant m_iWidth/m_iHeight) ->
// segfault sur de grandes images. Scénario qui crashait : 541x800 -> 518x518.
TEST(TEST_cgimg_img, resize_large_bilinear_no_oob)
{
    Img img(541, 800);
    img.init_color(120, 130, 140, 255);

    img.resize(518, 518, 1 /*bilinear*/);

    EXPECT_EQ(img.width(), 518u);
    EXPECT_EQ(img.height(), 518u);
    // Dernier pixel : ne doit pas lire hors-bornes ; image uniforme -> couleur conservée.
    unsigned char r, g, b, a;
    img.get_pixel(517, 517, &r, &g, &b, &a);
    EXPECT_EQ(r, 120);
    EXPECT_EQ(g, 130);
    EXPECT_EQ(b, 140);
}

// Mode 1 = vraie interpolation bilinéaire (et non plus-proche-voisin) : un
// dégradé horizontal 0->200 sur 2x2, agrandi en 3x3, doit donner ~100 au centre.
TEST(TEST_cgimg_img, resize_bilinear_interpolates)
{
    Img img(2, 2);
    img.set_pixel(0, 0, 0, 0, 0, 255);     img.set_pixel(1, 0, 200, 200, 200, 255);
    img.set_pixel(0, 1, 0, 0, 0, 255);     img.set_pixel(1, 1, 200, 200, 200, 255);

    img.resize(3, 3, 1 /*bilinear*/);

    unsigned char r, g, b, a;
    img.get_pixel(1, 1, &r, &g, &b, &a);   // centre : moyenne gauche/droite
    EXPECT_NEAR(r, 100, 2);                // nearest aurait donné 0 ou 200
}

TEST(TEST_cgimg_img, copy)
{
    // Context
    Img src(10, 10);
    src.init_color(50, 50, 50, 255);
    Img dst(20, 20);
    dst.init_color(0, 0, 0, 255);

    // Action
    dst.copy(5, 5, &src);

    // Expectations
    unsigned char r, g, b, a;
    dst.get_pixel(5, 5, &r, &g, &b, &a);
    EXPECT_EQ(r, 50);
}

TEST(TEST_cgimg_img, concatenate)
{
    // Context
    Img img1(10, 10);
    Img img2(10, 10);

    // Action
    img1.concatenate(&img2);

    // Expectations
    EXPECT_EQ(img1.width(), 20);
    EXPECT_EQ(img1.height(), 10);
}

TEST(TEST_cgimg_img, rotate)
{
    // Context
    Img img(4, 2); // Non-square image to clearly see dimension swaps
    img.init_color(0, 0, 0, 255);
    img.set_pixel(0, 0, 255, 0, 0, 255); // Top-left is Red

    // Action & Expectations: Mode 0 (90 Right)
    {
        Img img0(img);
        img0.rotate(0);
        EXPECT_EQ(img0.width(), 2);
        EXPECT_EQ(img0.height(), 4);
        unsigned char r, g, b, a;
        img0.get_pixel(1, 0, &r, &g, &b, &a); // (0,0) moves to (h-1-j, i) -> (1,0)
        EXPECT_EQ(r, 255);
    }

    // Action & Expectations: Mode 1 (90 Left)
    {
        Img img1(img);
        img1.rotate(1);
        EXPECT_EQ(img1.width(), 2);
        EXPECT_EQ(img1.height(), 4);
        unsigned char r, g, b, a;
        img1.get_pixel(0, 3, &r, &g, &b, &a); // (0,0) moves to (j, w-1-i) -> (0,3)
        EXPECT_EQ(r, 255);
    }

    // Action & Expectations: Mode 2 (180)
    {
        Img img2(img);
        img2.rotate(2);
        EXPECT_EQ(img2.width(), 4);
        EXPECT_EQ(img2.height(), 2);
        unsigned char r, g, b, a;
        img2.get_pixel(3, 1, &r, &g, &b, &a); // (0,0) moves to (w-1-i, h-1-j) -> (3,1)
        EXPECT_EQ(r, 255);
    }
}

TEST(TEST_cgimg_img, resize_canvas)
{
    // Context
    Img img(10, 10);

    // Action
    img.resize_canvas(20, 20, 0, 255, 0, 0, 255);

    // Expectations
    EXPECT_EQ(img.width(), 20);
    EXPECT_EQ(img.height(), 20);
}

TEST(TEST_cgimg_img, multiply)
{
    // Context
    Img img1(10, 10);
    img1.init_color(128, 128, 128, 255);
    Img img2(10, 10);
    img2.init_color(128, 128, 128, 255);

    // Action
    img1.multiply(&img2);

    // Expectations
    unsigned char r, g, b, a;
    img1.get_pixel(5, 5, &r, &g, &b, &a);
    EXPECT_NEAR(r, 64, 2);
}

TEST(TEST_cgimg_img, histogram_equalization_bezier)
{
    // Context
    Img img(10, 10);
    img.init_color(100, 100, 100, 255);
    img.convert_to_grayscale();

    // Action
    ImgHistogram::equalize_bezier (img, nullptr);

    // Expectations
    EXPECT_EQ(img.width(), 10);
}


// ---------------------------------------------------------------------------
//  Sous-echantillonnage par vote majoritaire (resize mode 3)
// ---------------------------------------------------------------------------
//
// Tests venus de tu_cgmesh_image_pixel_blocks.cpp : le vote majoritaire y etait
// implemente dans cgmesh (pixelize_majority), il est desormais le mode 3 de
// Img::resize -- une primitive d'image, testee ici.

namespace {

void mvFillRect (Img& img, int x0, int y0, int x1, int y1,
                 unsigned char r, unsigned char g, unsigned char b)
{
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            img.set_pixel ((unsigned)x, (unsigned)y, r, g, b, 255);
}

unsigned int mvPixelAt (const Img& img, int x, int y)
{
    unsigned char r = 0, g = 0, b = 0, a = 0;
    img.get_pixel ((unsigned)x, (unsigned)y, &r, &g, &b, &a);
    return ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
}

// Toutes les couleurs distinctes d'une image, empaquetees.
std::set<unsigned int> mvColorsOf (const Img& img)
{
    std::set<unsigned int> out;
    for (unsigned int y = 0; y < img.height(); ++y)
        for (unsigned int x = 0; x < img.width(); ++x)
            out.insert (mvPixelAt (img, (int)x, (int)y));
    return out;
}

const unsigned int MV_RED   = 0xff0000;
const unsigned int MV_BLUE  = 0x0000ff;
const unsigned int MV_WHITE = 0xffffff;

} // namespace

// Un bloc source 4x4 contenant 10 rouges et 6 bleus doit sortir ROUGE. Une
// moyenne produirait une teinte violacee absente de l'image -- c'est precisement
// ce que le vote evite.
TEST(TEST_cgimg_img, resize_majority_picks_the_dominant_colour)
{
    Img img(4, 4, false);
    mvFillRect(img, 0, 0, 4, 4, 255, 0, 0);       // 16 rouges
    mvFillRect(img, 0, 0, 3, 2, 0, 0, 255);       // 6 bleus -> 10 rouges restants

    ASSERT_EQ(img.resize(1, 1, 3), 0);
    EXPECT_EQ(img.width(), 1u);
    EXPECT_EQ(img.height(), 1u);
    EXPECT_EQ(mvPixelAt(img, 0, 0), MV_RED);
}

// Le vote ne choisit que parmi des couleurs presentes : l'ensemble des couleurs
// de sortie est inclus dans celui des couleurs d'entree.
TEST(TEST_cgimg_img, resize_majority_never_invents_a_colour)
{
    Img img(64, 64, false);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
        {
            const bool red = ((x / 3) + (y / 5)) % 2 == 0;
            img.set_pixel((unsigned)x, (unsigned)y,
                          red ? 255 : 0, 0, red ? 0 : 255, 255);
        }
    const std::set<unsigned int> before = mvColorsOf(img);
    ASSERT_EQ(before.size(), 2u);

    ASSERT_EQ(img.resize(7, 7, 3), 0);            // 64 n'est pas multiple de 7
    const std::set<unsigned int> after = mvColorsOf(img);

    for (std::set<unsigned int>::const_iterator it = after.begin(); it != after.end(); ++it)
        EXPECT_EQ(before.count(*it), 1u) << "couleur inventee : " << std::hex << *it;
}

// Non-regression du defaut du mode 2 : son pas est CONSTANT (W/w en division
// entiere), donc sur 100 px en 7 cellules il s'arrete a 98 et ne voit jamais les
// deux dernieres colonnes. Le mode 3 calcule des bornes exactes et les couvre.
TEST(TEST_cgimg_img, resize_majority_covers_the_trailing_remainder)
{
    Img src(100, 10, false);
    mvFillRect(src, 0, 0, 100, 10, 255, 255, 255);
    mvFillRect(src, 98, 0, 100, 10, 255, 0, 0);   // SEULES les colonnes 98-99

    // Mode 2, pas constant 100/7 = 14 : la derniere cellule part de 84 et lit
    // 14 px, soit [84,98) -- les colonnes de queue restent invisibles.
    Img mode2 = src;
    ASSERT_EQ(mode2.resize(7, 1, 2), 0);
    EXPECT_EQ(mvPixelAt(mode2, 6, 0), MV_WHITE)
        << "comportement historique du mode 2 : le reste de division est ignore";

    // Mode 3 : la cellule 49 est exactement [98,100), 100 % rouge.
    Img mode3 = src;
    ASSERT_EQ(mode3.resize(50, 1, 3), 0);
    EXPECT_EQ(mvPixelAt(mode3, 49, 0), MV_RED)
        << "le mode 3 doit couvrir le reste de la division";
}

// A egalite stricte, la couleur de plus BASSE valeur RGB gagne : sans cette
// regle le resultat dependrait de l'ordre de parcours.
TEST(TEST_cgimg_img, resize_majority_breaks_ties_deterministically)
{
    Img img(2, 1, false);
    img.set_pixel(0, 0, 255, 0, 0, 255);          // rouge  0xff0000
    img.set_pixel(1, 0, 0, 0, 255, 255);          // bleu   0x0000ff

    ASSERT_EQ(img.resize(1, 1, 3), 0);
    EXPECT_EQ(mvPixelAt(img, 0, 0), MV_BLUE) << "0x0000ff < 0xff0000";
}

// A l'agrandissement, des cellules n'ont aucun pixel source : elles dupliquent
// le plus proche au lieu de rester non initialisees.
TEST(TEST_cgimg_img, resize_majority_upscale_is_defined)
{
    Img img(2, 2, false);
    mvFillRect(img, 0, 0, 2, 2, 10, 20, 30);

    ASSERT_EQ(img.resize(6, 6, 3), 0);
    EXPECT_EQ(img.width(), 6u);
    for (unsigned int y = 0; y < 6; ++y)
        for (unsigned int x = 0; x < 6; ++x)
            EXPECT_EQ(mvPixelAt(img, (int)x, (int)y), 0x0a141eu);
}

// ---------------------------------------------------------------------------
//  Filtre de mode
// ---------------------------------------------------------------------------

// Un pixel isole dans un fond uniforme est un mouchetis : ses 8 voisins votent
// contre lui, il disparait. Le fond, lui, ne bouge pas.
TEST(TEST_cgimg_img, filter_majority_removes_an_isolated_speck)
{
    Img img(9, 9, false);
    mvFillRect(img, 0, 0, 9, 9, 255, 255, 255);
    img.set_pixel(4, 4, 255, 0, 0, 255);

    ASSERT_EQ(ImgFilter::majority (img, 1, 1), 0);

    EXPECT_EQ(mvPixelAt(img, 4, 4), MV_WHITE);
    EXPECT_EQ(mvColorsOf(img).size(), 1u);
}

// Une frontiere franche entre deux moities NE DOIT PAS deriver : a egalite le
// pixel central est conserve. Dix passes ne doivent pas bouger la limite d'un
// seul pixel.
TEST(TEST_cgimg_img, filter_majority_keeps_a_straight_boundary)
{
    Img img(16, 16, false);
    mvFillRect(img, 0, 0, 8, 16, 255, 255, 255);
    mvFillRect(img, 8, 0, 16, 16, 0, 0, 255);

    ASSERT_EQ(ImgFilter::majority (img, 1, 10), 0);

    for (int y = 0; y < 16; ++y)
    {
        EXPECT_EQ(mvPixelAt(img, 7, y), MV_WHITE) << "y=" << y;
        EXPECT_EQ(mvPixelAt(img, 8, y), MV_BLUE)  << "y=" << y;
    }
}

// Le filtre ne cree aucune couleur : il ne recopie que des couleurs voisines.
TEST(TEST_cgimg_img, filter_majority_never_invents_a_colour)
{
    Img img(24, 24, false);
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
        {
            const int k = (x * 7 + y * 3) % 3;
            img.set_pixel((unsigned)x, (unsigned)y,
                          k == 0 ? 255 : 0, k == 1 ? 255 : 0, k == 2 ? 255 : 0, 255);
        }
    const std::set<unsigned int> before = mvColorsOf(img);

    ASSERT_EQ(ImgFilter::majority (img, 1, 3), 0);

    const std::set<unsigned int> after = mvColorsOf(img);
    for (std::set<unsigned int>::const_iterator it = after.begin(); it != after.end(); ++it)
        EXPECT_EQ(before.count(*it), 1u) << "couleur inventee : " << std::hex << *it;
}

TEST(TEST_cgimg_img, filter_majority_rejects_degenerate_parameters)
{
    Img img(4, 4, false);
    mvFillRect(img, 0, 0, 4, 4, 1, 2, 3);
    EXPECT_EQ(ImgFilter::majority (img, 0, 1), 0);      // no-op, pas une erreur
    EXPECT_EQ(ImgFilter::majority (img, 1, 0), 0);
    EXPECT_EQ(mvPixelAt(img, 0, 0), 0x010203u);
}

// ---------------------------------------------------------------------------
//  Absorption des petites regions
// ---------------------------------------------------------------------------

// Un carre 2x2 (aire 4) sous un seuil de 6 est absorbe par son unique voisin.
// Un carre 4x4 (aire 16) au-dessus du seuil survit.
TEST(TEST_cgimg_img, absorb_small_regions_merges_below_the_threshold)
{
    Img img(20, 20, false);
    mvFillRect(img, 0, 0, 20, 20, 255, 255, 255);
    mvFillRect(img, 2, 2, 4, 4, 255, 0, 0);       // aire 4  -> absorbe
    mvFillRect(img, 10, 10, 14, 14, 0, 0, 255);   // aire 16 -> conserve

    ASSERT_EQ(ImgFilter::absorb_small_regions (img, 6), 0);

    EXPECT_EQ(mvPixelAt(img, 2, 2), MV_WHITE);
    EXPECT_EQ(mvPixelAt(img, 11, 11), MV_BLUE);
}

// Deux blocs de MEME couleur mais disjoints sont deux composantes : le seuil
// s'applique a chacune, pas a l'aire totale de la couleur.
TEST(TEST_cgimg_img, absorb_small_regions_works_per_component)
{
    Img img(20, 20, false);
    mvFillRect(img, 0, 0, 20, 20, 255, 255, 255);
    mvFillRect(img, 2, 2, 3, 3, 255, 0, 0);       // aire 1  -> absorbe
    mvFillRect(img, 8, 8, 14, 14, 255, 0, 0);     // aire 36 -> conserve

    ASSERT_EQ(ImgFilter::absorb_small_regions (img, 4), 0);

    EXPECT_EQ(mvPixelAt(img, 2, 2), MV_WHITE);
    EXPECT_EQ(mvPixelAt(img, 10, 10), MV_RED);
}

// L'image reste un pavage complet : aucune couleur nouvelle, et le nombre de
// couleurs ne peut que decroitre.
TEST(TEST_cgimg_img, absorb_small_regions_never_invents_a_colour)
{
    Img img(16, 16, false);
    mvFillRect(img, 0, 0, 16, 16, 255, 255, 255);
    mvFillRect(img, 1, 1, 2, 2, 255, 0, 0);
    mvFillRect(img, 5, 5, 6, 6, 0, 0, 255);
    mvFillRect(img, 9, 9, 10, 10, 0, 255, 0);
    const std::set<unsigned int> before = mvColorsOf(img);

    ASSERT_EQ(ImgFilter::absorb_small_regions (img, 3), 0);

    const std::set<unsigned int> after = mvColorsOf(img);
    EXPECT_LE(after.size(), before.size());
    for (std::set<unsigned int>::const_iterator it = after.begin(); it != after.end(); ++it)
        EXPECT_EQ(before.count(*it), 1u) << "couleur inventee : " << std::hex << *it;
}

TEST(TEST_cgimg_img, absorb_small_regions_rejects_degenerate_parameters)
{
    Img img(4, 4, false);
    mvFillRect(img, 0, 0, 4, 4, 1, 2, 3);
    EXPECT_EQ(ImgFilter::absorb_small_regions (img, 0), 0);    // no-op, pas une erreur
    EXPECT_EQ(ImgFilter::absorb_small_regions (img, 4, 0), 0);
    EXPECT_EQ(mvPixelAt(img, 0, 0), 0x010203u);
}

// ---------------------------------------------------------------------------
//  Etiquetage en composantes connexes
// ---------------------------------------------------------------------------

// Deux blocs de meme couleur separes par du fond font DEUX composantes, et les
// indices suivent l'ordre de balayage raster.
TEST(TEST_cgimg_img, label_components_separates_disjoint_blocks)
{
    Img img(10, 10, false);
    mvFillRect(img, 0, 0, 10, 10, 255, 255, 255);
    mvFillRect(img, 1, 1, 3, 3, 255, 0, 0);
    mvFillRect(img, 6, 6, 8, 8, 255, 0, 0);       // MEME couleur, disjoint

    std::vector<int> labels;
    std::vector<Color> colors;
    const int n = img.label_components(labels, &colors);

    ASSERT_EQ(n, 3);                              // fond + 2 blocs rouges
    ASSERT_EQ(labels.size(), 100u);
    ASSERT_EQ(colors.size(), 3u);
    EXPECT_EQ(labels[0], 0);                      // le fond est rencontre en premier
    const int a = labels[1 * 10 + 1];
    const int b = labels[6 * 10 + 6];
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ((int)colors[(size_t)a].r(), 255);
    EXPECT_EQ((int)colors[(size_t)a].g(), 0);
    EXPECT_EQ((int)colors[(size_t)b].r(), 255);
}

// La diagonale n'est PAS une connexion (4-connexe, pas 8).
TEST(TEST_cgimg_img, label_components_is_four_connected)
{
    Img img(2, 2, false);
    img.set_pixel(0, 0, 255, 0, 0, 255);
    img.set_pixel(1, 1, 255, 0, 0, 255);
    img.set_pixel(1, 0, 255, 255, 255, 255);
    img.set_pixel(0, 1, 255, 255, 255, 255);

    std::vector<int> labels;
    const int n = img.label_components(labels);

    EXPECT_EQ(n, 4) << "les deux rouges en diagonale ne sont pas connexes";
}

// Une image monochrome fait UNE composante couvrant tout le raster : c'est le
// cas qui ferait exploser une implementation recursive.
TEST(TEST_cgimg_img, label_components_handles_a_monochrome_raster)
{
    Img img(200, 200, false);
    img.init_color(7, 8, 9, 255);

    std::vector<int> labels;
    std::vector<Color> colors;
    const int n = img.label_components(labels, &colors);

    ASSERT_EQ(n, 1);
    ASSERT_EQ(colors.size(), 1u);
    for (size_t i = 0; i < labels.size(); ++i)
        ASSERT_EQ(labels[i], 0) << "pixel " << i;
}

TEST(TEST_cgimg_img, label_components_rejects_an_empty_image)
{
    Img img;
    std::vector<int> labels;
    EXPECT_EQ(img.label_components(labels), -1);
}

// ---------------------------------------------------------------------------
//  Filtre bilateral : miroir de bord
// ---------------------------------------------------------------------------

// Regression : le repli du bord bas etait `iii -= iii`, soit 0 -- tout le
// demi-voisinage hors cadre s'effondrait sur la colonne/ligne 0. Sur une image
// UNIFORME le filtre doit etre l'identite exacte, y compris sur le lisere
// exterieur : un miroir correct ne peut echantillonner que la meme couleur.
TEST(TEST_cgimg_img, bilateral_filtering_mirrors_the_border)
{
    Img img(16, 16, false);
    img.init_color(90, 110, 130, 255);

    ASSERT_EQ(ImgFilter::bilateral (img), 0);

    for (unsigned int y = 0; y < 16; ++y)
        for (unsigned int x = 0; x < 16; ++x)
        {
            unsigned char r = 0, g = 0, b = 0, a = 0;
            img.get_pixel(x, y, &r, &g, &b, &a);
            ASSERT_NEAR((int)r, 90, 1) << "x=" << x << " y=" << y;
            ASSERT_NEAR((int)g, 110, 1) << "x=" << x << " y=" << y;
            ASSERT_NEAR((int)b, 130, 1) << "x=" << x << " y=" << y;
        }
}
