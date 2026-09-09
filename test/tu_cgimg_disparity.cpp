#include <gtest/gtest.h>

#include <vector>

#include "../src/cgimg/cgimg.h"

// ===========================================================================
//  Disparite stereo (Birchfield & Tomasi)
// ===========================================================================
//
// image_disparity_birchfield.cpp fait 1 177 lignes -- 15 % de cgimg -- et
// n'etait exerce que par un test sans oracle, sur une paire reelle depourvue de
// verite terrain : on ne pouvait rien affirmer de son resultat.
//
// Ici les paires sont SYNTHETIQUES et leur verite terrain est donc CONNUE :
// makePair produit une translation globale d'une quantite donnee, et
// makeOccluderPair un plan a deux disparites. Dans les deux cas la disparite
// attendue s'exige au pixel pres.
//
// ---------- Contraintes de l'implementation, constatees par lecture ---------
//
// 1. COLS (512) et ROWS (480) sont des constantes de COMPILATION et Process()
//    boucle dessus au lieu d'utiliser les dimensions reelles de l'image. Une
//    paire d'une autre taille provoque des lectures hors bornes -- interceptees
//    par Img::get_r, qui journalise et renvoie 0 -- ou un traitement partiel.
//    D'ou le 512x480 ci-dessous, taille des donnees meterbig_*.pgm existantes.
//
// 2. La disparite est stockee EN CLAIR dans le niveau de gris : la valeur du
//    pixel EST le decalage en pixels, entre 0 et MAXDISP (20). Pas d'echelle.
//
// 3. Convention de signe, ETABLIE PAR MESURE et non supposee : un motif situe
//    en x dans l'image gauche doit apparaitre en x - d dans l'image droite.
//    Le sens oppose ne produit aucun appariement (mode 0 sur 10 % de l'image).
//
// 4. GetDisparity() renvoie toujours m_pDisparity1. PostProcess() remplit
//    m_pDisparity2, qu'AUCUN accesseur public n'expose : son resultat est
//    inatteignable depuis l'exterieur. Signale ci-dessous, pas corrige.

namespace {

const unsigned int kW = 512;   // COLS
const unsigned int kH = 480;   // ROWS

// Generateur deterministe : une texture ALEATOIRE evite les appariements
// ambigus qu'un motif periodique (rayures) provoquerait, et rend le test
// reproductible sans fichier de donnees.
unsigned int lcg (unsigned int& state)
{
	state = state * 1103515245u + 12345u;
	return (state >> 16) & 0xFF;
}

// Construit une paire dont la droite est la gauche translatee de `d` pixels
// vers la GAUCHE : right[x] == left[x + d]. C'est la convention retenue par
// l'implementation (cf. note 3 ci-dessus), donc la disparite attendue vaut d.
void makePair (Img& left, Img& right, int d)
{
	const unsigned int pad = 64;
	std::vector<unsigned char> row (kW + 2 * pad);
	unsigned int seed = 12345u;

	left  = Img (kW, kH);
	right = Img (kW, kH);
	for (unsigned int y = 0; y < kH; y++)
	{
		for (unsigned int x = 0; x < row.size(); x++)
			row[x] = (unsigned char)lcg (seed);
		for (unsigned int x = 0; x < kW; x++)
		{
			const unsigned char l = row[x + pad];
			const unsigned char r = row[x + pad + d];
			left.set_pixel  (x, y, l, l, l, 255);
			right.set_pixel (x, y, r, r, r, 255);
		}
	}
}

// --------------------------------------------------------------------------
// Paire « occulteur » : un rectangle opaque a la disparite kOccD, pose devant
// un fond a la disparite 0. Il occupe les colonnes [kObjLo, kObjHi] de l'image
// GAUCHE, donc [kObjLo - kOccD, kObjHi - kOccD] de la droite.
//
// Contrairement a makePair, les deux vues ne sont PAS la meme image translatee :
// leurs cartes de gradients d'intensite different. La texture de l'objet est
// choisie pour qu'elles divergent a son bord DROIT, seul endroit ou la
// contrainte « une discontinuite de profondeur doit coincider avec un gradient
// d'intensite » decide du resultat :
//
//   - colonnes [kFlatLo, kFlatHi] : intensite constante, donc aucune fenetre de
//     3 px qui s'y inscrit ne varie. La fenetre lue dans l'image GAUCHE a
//     l'indice kObjHi - kOccD y tombe entierement ;
//   - trois dernieres colonnes de l'objet : un creneau 0/255/0. Dans l'image
//     DROITE ces colonnes occupent [kObjHi - kOccD - 2, kObjHi - kOccD], soit
//     exactement la fenetre lue au meme indice.
//
// La transition kOccD -> 0 y est donc AUTORISEE par les gradients de l'image
// droite et INTERDITE par ceux de la gauche : c'est ce qui rend la paire
// discriminante.
const int kOccD   = 8;    // disparite de l'occulteur
const int kObjLo  = 200;  // premiere colonne de l'objet, image gauche
const int kObjHi  = 300;  // derniere colonne de l'objet, image gauche
const int kFlatLo = 261;  // plage d'intensite constante, image gauche
const int kFlatHi = 297;

static_assert (kFlatLo <= kObjHi - kOccD - 2 && kObjHi - kOccD <= kFlatHi,
	"la fenetre lue dans l'image gauche doit etre entierement plate");
static_assert (kFlatHi < kObjHi - 2,
	"les trois dernieres colonnes de l'objet doivent rester texturees");

void makeOccluderPair (Img& left, Img& right)
{
	std::vector<unsigned char> bg (kW), obj (kW);
	unsigned int seed = 987654321u;

	left  = Img (kW, kH);
	right = Img (kW, kH);
	for (unsigned int y = 0; y < kH; y++)
	{
		for (unsigned int x = 0; x < kW; x++)
			bg[x] = (unsigned char)lcg (seed);
		for (int x = kObjLo; x <= kObjHi; x++)
		{
			if (x >= kFlatLo && x <= kFlatHi)   obj[x] = 128;
			else if (x == kObjHi - 2)           obj[x] = 0;
			else if (x == kObjHi - 1)           obj[x] = 255;
			else if (x == kObjHi)               obj[x] = 0;
			else                                obj[x] = (unsigned char)lcg (seed);
		}
		for (int x = 0; x < (int)kW; x++)
		{
			// L'objet cache le fond en x a gauche, en x - kOccD a droite.
			const int rx = x + kOccD;
			const unsigned char l = (x  >= kObjLo && x  <= kObjHi) ? obj[x]  : bg[x];
			const unsigned char r = (rx >= kObjLo && rx <= kObjHi) ? obj[rx] : bg[x];
			left.set_pixel  (x, y, l, l, l, 255);
			right.set_pixel (x, y, r, r, r, 255);
		}
	}
}

// Part des pixels de la bande de colonnes [x0, x1] qui portent la disparite
// `value`, toutes lignes confondues.
double fractionEqual (Img& d, int x0, int x1, int value)
{
	long total = 0, hits = 0;
	for (unsigned int y = 0; y < d.height(); y++)
		for (int x = x0; x <= x1; x++)
		{
			total++;
			if (d.get_r ((unsigned int)x, y) == value) hits++;
		}
	return total ? (double)hits / (double)total : 0.0;
}

// Disparite la plus frequente sur une bande interieure, et sa part. On ecarte
// les bords : sur une bande de `margin` px, un motif n'a pas de correspondant
// dans l'autre vue et l'algorithme y signale une occlusion.
int modalDisparity (Img& d, unsigned int margin, double* coverage)
{
	int hist[256] = {0};
	long total = 0;
	for (unsigned int y = margin; y + margin < d.height(); y++)
		for (unsigned int x = margin; x + margin < d.width(); x++)
		{
			unsigned char r, g, b, a;
			d.get_pixel (x, y, &r, &g, &b, &a);
			hist[r]++;
			total++;
		}
	int best = 0;
	for (int i = 1; i < 256; i++)
		if (hist[i] > hist[best]) best = i;
	if (coverage) *coverage = total ? (double)hist[best] / (double)total : 0.0;
	return best;
}

}  // namespace

// Temoin principal : une translation connue doit etre retrouvee EXACTEMENT.
// Sans lui, un algorithme qui renverrait une carte plausible mais fausse ne se
// distinguerait pas d'un algorithme correct.
TEST(TEST_cgimg_disparity, recovers_a_known_uniform_disparity)
{
	Img left, right;
	makePair (left, right, 8);

	DisparityBirchfield d;
	d.SetStereoPair (&left, &right);
	d.Process ();

	Img* out = d.GetDisparity ();
	ASSERT_NE (out, nullptr);
	ASSERT_EQ (out->width(),  kW);
	ASSERT_EQ (out->height(), kH);

	double coverage = 0.;
	EXPECT_EQ (modalDisparity (*out, 40, &coverage), 8)
		<< "la disparite dominante doit valoir la translation appliquee";
	EXPECT_GT (coverage, 0.90)
		<< "elle doit couvrir l'essentiel de la bande interieure, pas une minorite";
}

// La meme exigence a plusieurs disparites : un resultat juste pour une seule
// valeur pourrait n'etre qu'une coincidence (un biais constant, par exemple).
TEST(TEST_cgimg_disparity, recovers_several_disparities)
{
	for (int expected : {4, 8, 12})
	{
		SCOPED_TRACE (testing::Message() << "disparite attendue = " << expected);
		Img left, right;
		makePair (left, right, expected);

		DisparityBirchfield d;
		d.SetStereoPair (&left, &right);
		d.Process ();

		double coverage = 0.;
		EXPECT_EQ (modalDisparity (*d.GetDisparity (), 40, &coverage), expected);
		EXPECT_GT (coverage, 0.90);
	}
}

// Cas degenere utile : deux vues identiques n'ont aucune parallaxe, donc une
// disparite nulle partout. C'est aussi le test qui echouerait si la carte
// etait remplie d'une constante arbitraire.
TEST(TEST_cgimg_disparity, identical_views_give_zero_disparity)
{
	Img left, right;
	makePair (left, right, 0);

	DisparityBirchfield d;
	d.SetStereoPair (&left, &right);
	d.Process ();

	double coverage = 0.;
	EXPECT_EQ (modalDisparity (*d.GetDisparity (), 40, &coverage), 0);
	EXPECT_GT (coverage, 0.95);
}

// Les deux reglages publics sont stockes multiplies par deux en interne (« pour
// que tout reste entier », dit le code). Les accesseurs doivent donc rendre la
// valeur d'origine, et une valeur negative est ramenee a zero.
TEST(TEST_cgimg_disparity, penalty_and_reward_round_trip)
{
	DisparityBirchfield d;

	d.setOcclusionPenalty (25);
	EXPECT_EQ (d.getOcclusionPenalty (), 25);
	d.setReward (7);
	EXPECT_EQ (d.getReward (), 7);

	// Documente le comportement reel : la valeur negative est corrigee, pas
	// rejetee (l'implementation journalise et retombe a zero).
	d.setOcclusionPenalty (-3);
	EXPECT_EQ (d.getOcclusionPenalty (), 0);
	d.setReward (-1);
	EXPECT_EQ (d.getReward (), 0);
}

// Le reglage doit AGIR : une penalite d'occlusion extreme change la carte
// produite. Sans cette verification, setOcclusionPenalty pourrait n'avoir aucun
// effet sans que rien ne le signale.
TEST(TEST_cgimg_disparity, occlusion_penalty_changes_the_result)
{
	Img left, right;
	makePair (left, right, 8);

	auto run = [&](int penalty) {
		DisparityBirchfield d;
		d.setOcclusionPenalty (penalty);
		d.SetStereoPair (&left, &right);
		d.Process ();
		Img* out = d.GetDisparity ();
		long sum = 0;
		for (unsigned int y = 0; y < kH; y += 8)
			for (unsigned int x = 0; x < kW; x += 8)
			{
				unsigned char r, g, b, a;
				out->get_pixel (x, y, &r, &g, &b, &a);
				sum += r;
			}
		return sum;
	};

	// Une penalite nulle autorise les occlusions sans cout ; une penalite tres
	// forte les interdit en pratique. Les deux cartes ne peuvent pas coincider.
	EXPECT_NE (run (0), run (500));
}

// Bord droit d'un occulteur. Les tests ci-dessus emploient une paire dont les
// deux vues sont la meme texture translatee : leurs gradients d'intensite
// coincident partout, donc rien n'y distingue les gradients de la vue droite de
// ceux de la gauche. Cette paire-ci les fait diverger a l'endroit exact ou
// l'algorithme consulte les seconds -- la ou la disparite retombe de celle de
// l'objet a celle du fond.
TEST(TEST_cgimg_disparity, occluder_right_border_needs_the_right_view_gradients)
{
	Img left, right;
	makeOccluderPair (left, right);

	DisparityBirchfield d;
	// La penalite d'occlusion et la recompense sont des variables GLOBALES du
	// module, pas des membres : le dernier test qui les regle les laisse en
	// place pour les suivants. Elles sont donc fixees ici a leurs valeurs par
	// defaut, faute de quoi le verdict dependrait de l'ordre d'execution.
	d.setOcclusionPenalty (25);
	d.setReward (5);
	d.SetStereoPair (&left, &right);
	d.Process ();

	Img* out = d.GetDisparity ();
	ASSERT_NE (out, nullptr);

	// Verite terrain : l'objet est a kOccD jusqu'a kObjHi inclus, le fond au-dela
	// est a 0. Les colonnes kObjHi+1 a kObjHi+7 sont laissees de cote, la
	// discontinuite s'y etale sur quelques pixels.
	EXPECT_GT (fractionEqual (*out, 250, kObjHi, kOccD), 0.99)
		<< "l'occulteur doit garder sa disparite jusqu'a son bord droit";
	EXPECT_GT (fractionEqual (*out, kObjHi + 8, 400, 0), 0.99)
		<< "le fond a droite de l'occulteur doit revenir a la disparite nulle";
}
