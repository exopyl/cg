#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

#include <set>
#include <vector>

// ===========================================================================
//  Oracles partages
// ===========================================================================
// Ces tests exercaient les algorithmes puis ecrivaient un fichier, sans rien
// verifier : ils ne detectaient qu'un plantage. Les helpers ci-dessous donnent
// de quoi affirmer une propriete plutot qu'une absence de crash.

// Niveau de gris d'un pixel (canal rouge : convert_to_grayscale ecrit la meme
// valeur sur les trois canaux).
static int gray_at (Img& img, unsigned x, unsigned y)
{
	unsigned char r, g, b, a;
	img.get_pixel (x, y, &r, &g, &b, &a);
	(void)g; (void)b; (void)a;
	return r;
}

// Couleurs RGB distinctes presentes dans l'image. Sert d'oracle a la
// quantification (« au plus n couleurs ») comme a la binarisation
// (« exactement deux niveaux »).
static std::set<int> distinct_colors (Img& img)
{
	std::set<int> s;
	for (unsigned y = 0; y < img.height(); y++)
		for (unsigned x = 0; x < img.width(); x++)
		{
			unsigned char r, g, b, a;
			img.get_pixel (x, y, &r, &g, &b, &a);
			s.insert ((int)r << 16 | (int)g << 8 | (int)b);
		}
	return s;
}

// Vrai si l'image ne porte qu'une seule couleur. Une sortie uniforme est le
// symptome classique d'un algorithme qui a tout ecrase : plusieurs oracles
// ci-dessous s'en servent comme garde-fou minimal.
static bool is_uniform (Img& img)
{
	return distinct_colors (img).size() == 1u;
}

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

	// grayscale1 : rampe HORIZONTALE de 255 colonnes. Le niveau vaut l'abscisse
	// et ne depend pas de la ligne -- c'est tout le contrat de la mire.
	ImgTestPattern::grayscale1 (*img, 100);
	img->save ((char*)"./img_grayscale1.pgm");
	ASSERT_EQ (img->width(),  255u);
	ASSERT_EQ (img->height(), 100u);
	EXPECT_EQ (gray_at (*img, 0, 0),    0);
	EXPECT_EQ (gray_at (*img, 254, 0),  254);
	EXPECT_EQ (gray_at (*img, 137, 0),  137);
	EXPECT_EQ (gray_at (*img, 137, 99), 137) << "la rampe doit etre invariante en y";

	// grayscale2 : damier 8x8 de blocs de `size` px, niveau 4*(8*j+i). Deux
	// blocs voisins different donc de 4, et le dernier vaut 4*63 = 252.
	ImgTestPattern::grayscale2 (*img, 50);
	img->save ((char*)"./img_grayscale2.pgm");
	ASSERT_EQ (img->width(),  400u);
	ASSERT_EQ (img->height(), 400u);
	EXPECT_EQ (gray_at (*img, 0, 0),     0);
	EXPECT_EQ (gray_at (*img, 60, 0),    4)   << "bloc (1,0)";
	EXPECT_EQ (gray_at (*img, 0, 60),    32)  << "bloc (0,1) = 4*8";
	EXPECT_EQ (gray_at (*img, 399, 399), 252) << "bloc (7,7) = 4*63";
	EXPECT_EQ (gray_at (*img, 10, 10), gray_at (*img, 40, 40))
		<< "un bloc est uniforme";

	// color_jet : degrade en fausses couleurs. La couleur ne depend que de la
	// colonne, et les extremes du degrade different.
	ImgTestPattern::color_jet (*img, 256, 100);
	img->save ((char*)"./img_color_jet.ppm");
	ASSERT_EQ (img->width(),  256u);
	ASSERT_EQ (img->height(), 100u);
	{
		unsigned char r0, g0, b0, a0, r1, g1, b1, a1;
		img->get_pixel (0,   0,  &r0, &g0, &b0, &a0);
		img->get_pixel (0,   99, &r1, &g1, &b1, &a1);
		EXPECT_EQ (r0, r1); EXPECT_EQ (g0, g1); EXPECT_EQ (b0, b1)
			<< "une colonne est uniforme";
		img->get_pixel (255, 0,  &r1, &g1, &b1, &a1);
		EXPECT_TRUE (r0 != r1 || g0 != g1 || b0 != b1)
			<< "les deux extremites du degrade doivent differer";
		EXPECT_GT (distinct_colors (*img).size(), 8u) << "un degrade, pas un aplat";
	}
	delete img;
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

		// Contrat exact : strictement au-dessus du seuil -> 255, sinon 0.
		// Verifiable pixel par pixel contre la source, pas seulement en
		// comptant les niveaux.
		const std::set<int> lv = distinct_colors (*imgc);
		EXPECT_LE (lv.size(), 2u) << "une binarisation ne laisse que deux niveaux";
		for (unsigned y = 0; y < img->height(); y += 37)
			for (unsigned x = 0; x < img->width(); x += 37)
			{
				const int expected = (gray_at (*img, x, y) > (int)t) ? 255 : 0;
				ASSERT_EQ (gray_at (*imgc, x, y), expected)
					<< "seuil " << (int)t << " en (" << x << "," << y << ")";
			}

		delete imgc;
	}

	// otsu
	{
		Img *imgc = new Img (*img);

		ImgBinarize::otsu (*imgc);
		imgc->save ((char*)"./img_bin_otsu.pgm");

		// Otsu cherche le seuil qui separe au mieux deux populations : sur une
		// mire qui couvre toute la dynamique, les deux niveaux doivent etre
		// presents. Une sortie uniforme signifierait un seuil degenere (0 ou
		// 255) -- exactement le bug qu'un test sans assertion laissait passer.
		const std::set<int> lv = distinct_colors (*imgc);
		EXPECT_EQ (lv.size(), 2u) << "Otsu doit produire deux classes non vides";
		EXPECT_TRUE (lv.count (0x000000) && lv.count (0xFFFFFF))
			<< "les deux niveaux attendus sont 0 et 255";

		delete imgc;
	}

	// floyd steinberg
	{
		Img *imgc = new Img (*img);

		ImgBinarize::floyd_steinberg (*imgc);
		imgc->save ((char*)"./img_bin_floyd_steinberg.pgm");

		// Diffusion d'erreur : sortie binaire, et les deux niveaux coexistent
		// puisque la mire couvre toute la dynamique.
		const std::set<int> lv = distinct_colors (*imgc);
		EXPECT_EQ (lv.size(), 2u);
		EXPECT_FALSE (is_uniform (*imgc)) << "le tramage ne doit pas tout ecraser";

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
		imgc->crop (*img, 0, 0, img->width() - img->width()%psize, img->height() - img->height()%psize);
		
		ImgBinarize::dithering (*imgc, pattern, psize);
		imgc->save ((char*)"./img_bin_dithering.pgm");

		// Le crop a d'abord ramene les dimensions a un multiple de psize.
		EXPECT_EQ (imgc->width()  % (unsigned)psize, 0u);
		EXPECT_EQ (imgc->height() % (unsigned)psize, 0u);
		EXPECT_LE (distinct_colors (*imgc).size(), 2u) << "sortie binaire";
		EXPECT_FALSE (is_uniform (*imgc)) << "le tramage ordonne ne doit pas tout ecraser";

		delete imgc;
	}

	// screening
	{
		Img *imgc = new Img (*img);
		Img *imgPattern = new Img ();
		imgPattern->load ((char*)"./test/data/halftone.pgm");

		const bool patternLoaded = (imgPattern->width() > 0 && imgPattern->height() > 0);
		ImgBinarize::screening (*imgc, *imgPattern);
		imgc->save ((char*)"./img_bin_screening.pgm");

		// La trame vient d'un fichier : si test/data/halftone.pgm manque, le
		// test dirait n'importe quoi. On l'affirme d'abord, puis on verifie la
		// sortie -- dimensions conservees et resultat binaire.
		ASSERT_TRUE (patternLoaded) << "test/data/halftone.pgm introuvable";
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_LE (distinct_colors (*imgc).size(), 2u) << "sortie binaire";

		delete imgPattern;
		delete imgc;
	}

	delete img;
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
		// Contrat d'un quantifieur : AU PLUS n couleurs, et l'image reste
		// exploitable (dimensions inchangees, pas d'aplat).
		EXPECT_LE (distinct_colors (*imgc).size(), 16u);
		EXPECT_EQ (imgc->width(), img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc));
		delete imgc;
	}

	// wu
	{
		Img *imgc = new Img (*img);
		ImgQuantize::wu (*imgc, 16);
		imgc->save ((char*)"./img_quant_wu.ppm");
		EXPECT_LE (distinct_colors (*imgc).size(), 16u);
		EXPECT_EQ (imgc->width(), img->width());
		EXPECT_FALSE (is_uniform (*imgc));
		delete imgc;
	}

	// kmean
	{
		Img *imgc = new Img (*img);
		ImgQuantize::kmean (*imgc, 0.05);
		imgc->save ((char*)"./img_quant_kmean.ppm");
		// kmean n'a pas de n impose : son seul contrat verifiable ici est de
		// REDUIRE le nombre de couleurs sans tout ecraser.
		EXPECT_LE (distinct_colors (*imgc).size(), distinct_colors (*img).size());
		EXPECT_FALSE (is_uniform (*imgc));
		delete imgc;
	}

	delete img;
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
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc)) << "le filtre ne doit pas tout ecraser";
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
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc)) << "le filtre ne doit pas tout ecraser";
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
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc)) << "le filtre ne doit pas tout ecraser";
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
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc)) << "le filtre ne doit pas tout ecraser";
		delete imgc;
	}

	// sobel
	{
		Img *imgc = new Img (*img);
		ImgFilter::sobel (*imgc);
		imgc->save ((char*)"./img_filter_sobel.ppm");
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc)) << "le filtre ne doit pas tout ecraser";
		delete imgc;
	}

	// bilateral filtering
	{
		Img *imgc = new Img (*img);
		ImgFilter::bilateral (*imgc);
		imgc->save ((char*)"./img_filter_bilateral.ppm");
		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc)) << "le filtre ne doit pas tout ecraser";
		delete imgc;
	}

	delete img;
}


// Oracle MATHEMATIQUE de la convolution, la ou les blocs ci-dessus ne verifient
// que des invariants faibles.
//
// Sur une image UNIFORME de valeur V :
//   - un noyau de somme nulle (laplacien, gradient, Sobel) rend 0 ;
//   - un noyau passe-bas rend V, puisque convolve() normalise par la somme du
//     noyau quand `divide` vaut 0.
//
// La BORDURE d'un pixel est exclue : convolve() balaie de 1 a n-2 et laisse le
// tampon a 0 sur le pourtour. C'est le comportement reel, pas un oubli du test.
TEST(TEST_cgimg_img, convolve_of_a_uniform_image_follows_the_kernel_sum)
{
	const unsigned int W = 32, H = 24;
	const unsigned char V = 120;

	// --- noyau de somme nulle -> reponse nulle a l'interieur ---------------
	{
		Img img (W, H);
		img.init_color (V, V, V, 255);
		float k[3][3];
		set3x3 (k, 0.f, -1.f, 0.f,
		          -1.f,  4.f, -1.f,
		           0.f, -1.f, 0.f);   // laplacien, somme = 0
		ASSERT_EQ (ImgFilter::convolve (img, k), 0);
		for (unsigned y = 1; y < H - 1; y++)
			for (unsigned x = 1; x < W - 1; x++)
				ASSERT_EQ (gray_at (img, x, y), 0)
					<< "somme nulle sur une image plate en (" << x << "," << y << ")";
	}

	// --- passe-bas normalise -> image inchangee a l'interieur --------------
	{
		Img img (W, H);
		img.init_color (V, V, V, 255);
		float k[3][3];
		set3x3 (k, 1.f, 1.f, 1.f,
		           1.f, 1.f, 1.f,
		           1.f, 1.f, 1.f);    // somme = 9, normalisee par convolve()
		ASSERT_EQ (ImgFilter::convolve (img, k), 0);
		for (unsigned y = 1; y < H - 1; y++)
			for (unsigned x = 1; x < W - 1; x++)
				ASSERT_EQ (gray_at (img, x, y), (int)V)
					<< "moyenne d'une image plate en (" << x << "," << y << ")";
	}

	// --- Sobel : contours d'une image plate = rien -------------------------
	{
		Img img (W, H);
		img.init_color (V, V, V, 255);
		ASSERT_EQ (ImgFilter::sobel (img), 0);
		for (unsigned y = 1; y < H - 1; y++)
			for (unsigned x = 1; x < W - 1; x++)
				ASSERT_EQ (gray_at (img, x, y), 0)
					<< "Sobel doit etre nul sans contour, en (" << x << "," << y << ")";
	}
}

// NOTE : le test « filter2 » a ete supprime. Son corps etait integralement
// encadre par #if 0 : il ne compilait aucun appel et ne pouvait donc rien
// verifier, pas meme l'absence de plantage. Le remplacer par des assertions
// aurait demande de le reecrire entierement ; la chaine qu'il decrivait
// (blur + sepia + multiply + brightness) est deja couverte par les blocs du
// test `filter` et par convolve_of_a_uniform_image_follows_the_kernel_sum.

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
	// Oracle : le trace DOIT poser la couleur demandee la ou il passe, et ne
	// pas repeindre toute l'image. On verifie les deux bouts du segment (ses
	// extremites exactes) et le centre du disque.
	{
		unsigned char r, g, b, a;
		imgc->get_pixel (10, 50, &r, &g, &b, &a);
		EXPECT_EQ (r, 255) << "extremite du segment";
		imgc->get_pixel (img->width() - 50, img->height() - 100, &r, &g, &b, &a);
		EXPECT_EQ (r, 255) << "autre extremite du segment";

		// Un disque plein de rayon 5 : son centre est necessairement peint.
		ImgDraw::disk (*imgc, 200, 200, 5, 0, 255, 0, 255);
		imgc->get_pixel (200, 200, &r, &g, &b, &a);
		EXPECT_EQ (g, 255) << "centre du disque";
		// ... et un point tres au-dela du rayon ne l'est pas.
		imgc->get_pixel (200, 180, &r, &g, &b, &a);
		EXPECT_NE (g, 255) << "hors du disque, la couleur ne doit pas avoir bavé";

		EXPECT_EQ (imgc->width(),  img->width());
		EXPECT_EQ (imgc->height(), img->height());
		EXPECT_FALSE (is_uniform (*imgc));
	}

	delete imgc;
	delete img;
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
	// Normalise par defaut : la somme des casiers vaut 1.
	{
		double sum = 0.;
		for (int i = 0; i < 256; i++) sum += h[i];
		EXPECT_NEAR (sum, 1.0, 1e-4) << "histogramme normalise";
	}
	// Non normalise : la somme vaut le nombre de pixels.
	{
		float raw[256];
		ImgHistogram::compute (*imgc, raw, 0);
		double sum = 0.;
		for (int i = 0; i < 256; i++) sum += raw[i];
		EXPECT_NEAR (sum, (double)imgc->width() * imgc->height(), 1.0);
	}
	output_1array (h, 256, "histogram1.dat");
	for (int i=1; i<256; i++)
		h[i] += h[i-1];
	output_1array (h, 256, "histogram11.dat");

	Img *histo = ImgHistogram::to_image (*imgc, 200);
	histo->save ("./img_histo_histo_orig.ppm");
	// Le rendu d'histogramme fait 256 colonnes (un casier par colonne) et la
	// hauteur demandee.
	ASSERT_NE (histo, nullptr);
	EXPECT_EQ (histo->width(),  256u);
	EXPECT_EQ (histo->height(), 200u);
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

		// La paire vient de fichiers : sans eux le test ne mesurerait rien.
		// L'affirmer AVANT de calculer, sinon la donnee manquante se
		// manifesterait comme un resultat vide plutot que comme sa vraie cause.
		ASSERT_GT (pLeft->width(),  0u) << "test/data/meterbig_l.pgm introuvable";
		ASSERT_GT (pRight->width(), 0u) << "test/data/meterbig_r.pgm introuvable";
		ASSERT_EQ (pLeft->width(),  pRight->width());
		ASSERT_EQ (pLeft->height(), pRight->height());
		
		DisparityBirchfield *pBirchfield = new DisparityBirchfield();
		pBirchfield->SetStereoPair (pLeft, pRight);
		pBirchfield->Process ();
		Img *pDisparity = pBirchfield->GetDisparity ();
		pDisparity->save ("./disparity.bmp");

		// Faute de verite terrain, on verifie ce qui l'est : la carte a la taille
		// de la paire, et elle porte plusieurs valeurs. Une carte uniforme
		// signifierait que l'appariement n'a rien trouve -- exactement ce que
		// l'absence d'assertion laissait passer.
		ASSERT_NE (pDisparity, nullptr);
		EXPECT_EQ (pDisparity->width(),  pLeft->width());
		EXPECT_EQ (pDisparity->height(), pLeft->height());
		EXPECT_FALSE (is_uniform (*pDisparity))
			<< "carte de disparite uniforme : aucun appariement";

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
	img->save ("./geodesic.ppm");

	// La transformee geodesique repartit une distance depuis les germes noirs.
	// Oracles : dimensions conservees, sortie NON uniforme (sinon la carte de
	// distance n'a rien produit), et un germe reste extremal -- il ne peut pas
	// etre plus loin de lui-meme qu'un point quelconque.
	ASSERT_EQ (img->width(),  100u);
	ASSERT_EQ (img->height(), 100u);
	EXPECT_FALSE (is_uniform (*img)) << "aucune distance n'a ete propagee";
	const int atSeed = gray_at (*img, 50, 50);
	EXPECT_LE (atSeed, gray_at (*img, 5, 5))
		<< "un germe doit etre au moins aussi proche que n'importe quel autre point";
	EXPECT_LE (atSeed, gray_at (*img, 95, 5));

	delete img;
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
	pImg->save("./temperature2color.bmp");

	// Oracle PHYSIQUE : une temperature de couleur basse tire vers le rouge,
	// une haute vers le bleu. C'est la definition meme du corps noir, donc la
	// seule propriete que ce degrade doit respecter.
	//
	// NOTE : temperature2color() est definie dans CE fichier de test, pas dans
	// cgimg -- ce test n'exerce donc aucun code de la bibliotheque. Le laisser
	// tel quel serait trompeur ; le supprimer perdrait la fonction. A trancher :
	// soit elle descend dans cgimg (color.h), soit ce test disparait avec elle.
	{
		unsigned char r1, g1, b1, r2, g2, b2;
		temperature2color (1000.f,  r1, g1, b1);   // rouge profond
		temperature2color (11000.f, r2, g2, b2);   // bleu
		EXPECT_GT ((int)r1, (int)b1) << "1000 K doit tirer vers le rouge";
		EXPECT_GT ((int)b2, (int)r2) << "11000 K doit tirer vers le bleu";
		EXPECT_GT ((int)b2, (int)b1) << "le bleu croit avec la temperature";
		EXPECT_FALSE (is_uniform (*pImg)) << "le degrade ne doit pas etre un aplat";
	}

	delete pImg;
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
    dst.copy(5, 5, src);

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
    img1.concatenate(img2);

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
    img1.multiply(img2);

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
