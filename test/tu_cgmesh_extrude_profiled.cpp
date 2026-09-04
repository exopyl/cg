#include <gtest/gtest.h>

#include "../src/cgmesh/contour_ops.h"
#include "../src/cgmesh/extrude_profiled.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/profile2d.h"
#include "../src/cgmesh/text_extrude.h"
#include "../src/cgmath/font.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

// ===========================================================================
//  Arete profilee : chanfrein, biseau, conge
// ===========================================================================
//
// Trois choses se gardent ici, et la troisieme est la moins evidente :
//
//   1. le SENS du decalage. Inward preserve l'emprise nominale -- 20 mm
//      demandes, 20 mm mesures --, Outward la fait croitre. C'est la decision
//      D1 du dossier de faisabilite, et un cas la tient ;
//   2. l'ETANCHEITE. La surface est cousue de couronnes tessellees
//      independamment ; si la classification des sommets par anneau derapait,
//      des trous apparaitraient sans que rien d'autre ne le dise ;
//   3. QUELLE FORME rend quel profil. Cela ne se deduit pas de la lecture du
//      code : la meme courbe donne un conge convexe ou une gorge concave selon
//      le sens ou le consommateur la lit. On le MESURE, par le volume.
//
// ===========================================================================

namespace {

std::vector<ExtrudeContour> square (float half)
{
	ExtrudeContour c;
	c.pts.push_back (Vector2f (-half, -half));
	c.pts.push_back (Vector2f ( half, -half));
	c.pts.push_back (Vector2f ( half,  half));
	c.pts.push_back (Vector2f (-half,  half));
	return { c };
}

std::vector<ExtrudeContour> rect (float cx, float halfW, float halfH)
{
	ExtrudeContour c;
	c.pts.push_back (Vector2f (cx - halfW, -halfH));
	c.pts.push_back (Vector2f (cx + halfW, -halfH));
	c.pts.push_back (Vector2f (cx + halfW,  halfH));
	c.pts.push_back (Vector2f (cx - halfW,  halfH));
	return { c };
}

void bbox (Mesh& m, float* lo, float* hi)
{
	for (int a = 0; a < 3; ++a) { lo[a] = 1e30f; hi[a] = -1e30f; }
	for (unsigned int v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		for (int a = 0; a < 3; ++a)
		{
			lo[a] = std::min (lo[a], p[a]);
			hi[a] = std::max (hi[a], p[a]);
		}
	}
}

// Emprise des sommets d'une TRANCHE de cote, a la tolerance pres. Sert a lire la
// largeur de la face du dessus sans supposer l'ordre des sommets.
void sliceExtentXY (Mesh& m, float z, float tol, float& width)
{
	float lo = 1e30f, hi = -1e30f;
	for (unsigned int v = 0; v < m.GetNVertices (); ++v)
	{
		float p[3];
		m.GetVertex (v, p);
		if (std::fabs (p[2] - z) > tol) continue;
		lo = std::min (lo, p[0]);
		hi = std::max (hi, p[0]);
	}
	width = (hi > lo) ? (hi - lo) : 0.f;
}

// Volume signe par le theoreme de la divergence. Positif quand les normales
// regardent vers l'exterieur -- donc il MESURE l'orientation en meme temps que
// le volume.
double signedVolume (Mesh& m)
{
	double vol = 0.0;
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		if (m.GetFaceNVertices (f) != 3) continue;
		float p[3][3];
		for (int k = 0; k < 3; ++k)
			m.GetVertex ((unsigned int)m.GetFaceVertex (f, k), p[k]);
		vol += ((double)p[0][0] * ((double)p[1][1] * p[2][2] - (double)p[2][1] * p[1][2])
		      - (double)p[1][0] * ((double)p[0][1] * p[2][2] - (double)p[2][1] * p[0][2])
		      + (double)p[2][0] * ((double)p[0][1] * p[1][2] - (double)p[1][1] * p[0][2])) / 6.0;
	}
	return vol;
}

// Aretes ANORMALES d'une peau, et la distinction n'est pas cosmetique :
//
//   n == 1  TROU. La surface est ouverte : le solide n'en est pas un, et un
//           slicer rendra n'importe quoi. Inacceptable.
//   n >= 3  PINCEMENT. La surface se touche elle-meme le long d'une arete, ce
//           qu'un decalage vers l'interieur produit legitimement des qu'un
//           empattement se referme sur lui-meme. La peau reste FERMEE -- tout
//           chemin sortant traverse une face -- elle n'est simplement plus une
//           variete. Les slicers l'admettent, comme ils admettent deux coques
//           qui s'interpenetrent.
//
// Les sommets sont soudes par coordonnee, la surface etant cousue de morceaux
// tessellees separement.
void skinDefects (Mesh& m, std::size_t& holes, std::size_t& pinches)
{
	// Cle EXACTE, un triplet quantifie -- pas un hachage combine : un XOR de
	// produits collisionne sur des coordonnees symetriques, ce qui fabriquerait
	// de fausses aretes non-manifold sur un carre centre a l'origine. (Constate,
	// pas suppose : c'est ce qui faisait echouer ce fichier sur un simple carre.)
	typedef std::array<long long, 3> Key;
	std::map<std::pair<Key, Key>, int> count;
	const auto key = [&] (unsigned int v) -> Key {
		float p[3];
		m.GetVertex (v, p);
		return Key { (long long)std::lround (p[0] * 10000.0),
		             (long long)std::lround (p[1] * 10000.0),
		             (long long)std::lround (p[2] * 10000.0) };
	};
	for (unsigned int f = 0; f < m.GetNFaces (); ++f)
	{
		if (m.GetFaceNVertices (f) != 3) continue;
		Key k[3];
		for (int i = 0; i < 3; ++i) k[i] = key ((unsigned int)m.GetFaceVertex (f, i));
		for (int i = 0; i < 3; ++i)
		{
			Key a = k[i], b = k[(i + 1) % 3];
			if (b < a) std::swap (a, b);
			if (a == b) continue;   // arete degeneree : elle ne borde rien
			count[{ a, b }]++;
		}
	}
	holes = pinches = 0;
	for (const auto& e : count)
	{
		if (e.second == 1) holes++;
		else if (e.second > 2) pinches++;
	}
}

std::size_t boundaryEdges (Mesh& m)
{
	std::size_t holes = 0, pinches = 0;
	skinDefects (m, holes, pinches);
	return holes;
}

ProfiledExtrudeOptions box (float zBottom, float zTop,
                            ProfiledExtrudeOptions::Direction dir
                              = ProfiledExtrudeOptions::Direction::Inward)
{
	ProfiledExtrudeOptions o;
	o.zBottom = zBottom;
	o.zTop = zTop;
	o.direction = dir;
	return o;
}

}  // namespace

// --- sens du decalage (D1) --------------------------------------------------

TEST (TEST_cgmesh_extrude_profiled, inward_keeps_the_nominal_footprint)
{
	Mesh mesh;
	ProfiledExtrudeStats stats;
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), chamferSplayProfile (2.0, 2.0),
	                                      box (0.f, 10.f), mesh, &stats));

	float lo[3], hi[3];
	bbox (mesh, lo, hi);
	// 20 mm demandes = 20 mm mesures : c'est toute la raison du mode Inward.
	EXPECT_NEAR (hi[0] - lo[0], 20.f, 1e-3f);
	EXPECT_NEAR (hi[1] - lo[1], 20.f, 1e-3f);
	EXPECT_NEAR (lo[2], 0.f, 1e-4f);
	EXPECT_NEAR (hi[2], 10.f, 1e-4f);

	// Et la face du dessus est RENTREE de la largeur du profil, de chaque cote.
	float top = 0.f;
	sliceExtentXY (mesh, 10.f, 1e-3f, top);
	EXPECT_NEAR (top, 20.f - 2.f * 2.f, 0.05f);

	// Deux points de profil, donc deux anneaux et UNE couronne.
	EXPECT_EQ (stats.rings, 2u);
	EXPECT_EQ (stats.bands, 1u);
	EXPECT_EQ (stats.vanishedPieces, 0u);
	EXPECT_EQ (stats.steinerPoints, 0u);
}

TEST (TEST_cgmesh_extrude_profiled, outward_grows_the_footprint_instead)
{
	Mesh mesh;
	ASSERT_TRUE (extrudeProfiledContours (
		square (10.f), chamferSplayProfile (2.0, 2.0),
		box (0.f, 10.f, ProfiledExtrudeOptions::Direction::Outward), mesh, nullptr));

	float lo[3], hi[3];
	bbox (mesh, lo, hi);
	// L'emprise CROIT de 2 x 2 : c'est ce que fait ExtrudeGeometry de three.js,
	// et pourquoi stltext.com affiche 26,86 mm pour 24 demandes.
	EXPECT_NEAR (hi[0] - lo[0], 24.f, 1e-3f);

	// La face du dessus, elle, reste nominale.
	float top = 0.f;
	sliceExtentXY (mesh, 10.f, 1e-3f, top);
	EXPECT_NEAR (top, 20.f, 0.05f);
}

// --- etancheite et orientation ----------------------------------------------

TEST (TEST_cgmesh_extrude_profiled, the_skin_closes_and_faces_outward)
{
	// Un carre PERCE : la couronne porte alors une enveloppe et un trou, et la
	// classification des sommets doit tenir sur les deux.
	std::vector<ExtrudeContour> ring = square (10.f);
	ExtrudeContour hole;
	hole.pts.push_back (Vector2f (-3.f, -3.f));
	hole.pts.push_back (Vector2f (-3.f,  3.f));
	hole.pts.push_back (Vector2f ( 3.f,  3.f));
	hole.pts.push_back (Vector2f ( 3.f, -3.f));   // sens INVERSE : c'est un trou
	ring.push_back (hole);

	Mesh mesh;
	ProfiledExtrudeStats stats;
	ASSERT_TRUE (extrudeProfiledContours (ring, cavettoSplayProfile (1.5, 1.5, 6),
	                                      box (0.f, 8.f), mesh, &stats));

	EXPECT_EQ (boundaryEdges (mesh), 0u)
		<< "la peau n'est pas fermee : la classification des sommets par anneau "
		   "a probablement derape";
	EXPECT_GT (signedVolume (mesh), 0.0) << "les normales regardent vers l'interieur";
	EXPECT_EQ (stats.steinerPoints, 0u);
	// Sept points de cavet, donc sept anneaux et six couronnes.
	EXPECT_EQ (stats.rings, 7u);
	EXPECT_EQ (stats.bands, 6u);
}

// --- QUELLE forme rend quel profil ------------------------------------------

TEST (TEST_cgmesh_extrude_profiled, the_cavetto_profile_reads_as_a_CONVEX_roundover)
{
	// LE CAS QUI NOMME LES FORMES, et il ne se deduit pas du code : sous la
	// lecture de ce consommateur (l'anneau du dessus est rentre de v_max, la
	// section pleine est atteinte a u_max), une courbe peut rendre un conge
	// CONVEXE -- qui roule l'arete, donc retire MOINS qu'une coupe droite -- ou
	// une gorge CONCAVE, qui creuse davantage.
	//
	// Le volume le dit sans ambiguite : a largeur et profondeur egales, le
	// chanfrein est la CORDE. Ce qui bombe au-dela retire moins.
	const double w = 3.0, dep = 3.0;
	Mesh chamfer, cavetto;
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), chamferSplayProfile (w, dep),
	                                      box (0.f, 10.f), chamfer, nullptr));
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), cavettoSplayProfile (w, dep, 8),
	                                      box (0.f, 10.f), cavetto, nullptr));

	const double vChamfer = signedVolume (chamfer);
	const double vCavetto = signedVolume (cavetto);
	ASSERT_GT (vChamfer, 0.0);
	ASSERT_GT (vCavetto, 0.0);

	EXPECT_GT (vCavetto, vChamfer)
		<< "le cavet retire PLUS que la corde : sous cette lecture il rend une "
		   "gorge concave, et le conge convexe demande alors un producteur de "
		   "profil de plus (cf. G2 du dossier de faisabilite). volumes : "
		<< vCavetto << " contre " << vChamfer;

	// Et les deux restent bornes par le prisme nu et par le prisme moins le coin
	// droit : une courbe ne peut ni ajouter de matiere ni en retirer plus que la
	// coupe la plus creuse.
	const double prism = 20.0 * 20.0 * 10.0;
	EXPECT_LT (vChamfer, prism);
	EXPECT_LT (vCavetto, prism);
}

// --- ce que le profil detruit, et qui doit se savoir ------------------------

TEST (TEST_cgmesh_extrude_profiled, a_profile_wider_than_the_matter_is_refused)
{
	// Une barre de 4 mm de large, un profil de 3 mm de chaque cote : l'anneau du
	// dessus n'existe pas. On REFUSE, plutot que de rendre une piece sans face
	// superieure.
	Mesh mesh;
	EXPECT_FALSE (extrudeProfiledContours (rect (0.f, 2.f, 20.f),
	                                       chamferSplayProfile (3.0, 3.0),
	                                       box (0.f, 10.f), mesh, nullptr));
}

TEST (TEST_cgmesh_extrude_profiled, a_vanished_piece_is_counted_not_hidden)
{
	// Deux barres : une large (10 mm) qui survit, une mince (1,6 mm) que le
	// profil consomme. La piece se construit -- l'autre barre est intacte -- et
	// le compte DIT ce qui a disparu. C'est le cas d'un delie de cursive.
	std::vector<ExtrudeContour> two = rect (-20.f, 5.f, 10.f);
	std::vector<ExtrudeContour> thin = rect (20.f, 0.8f, 10.f);
	two.insert (two.end (), thin.begin (), thin.end ());

	Mesh mesh;
	ProfiledExtrudeStats stats;
	ASSERT_TRUE (extrudeProfiledContours (two, chamferSplayProfile (1.0, 1.0),
	                                      box (0.f, 6.f), mesh, &stats));
	EXPECT_EQ (stats.vanishedPieces, 1u)
		<< "la barre mince a ete mangee sans que rien ne le signale";

	// La piece reste etanche : ce qui disparait, disparait proprement.
	EXPECT_EQ (boundaryEdges (mesh), 0u);
	EXPECT_GT (signedVolume (mesh), 0.0);
}

// --- refus de contrat -------------------------------------------------------

TEST (TEST_cgmesh_extrude_profiled, a_malformed_profile_is_refused_not_corrected)
{
	Mesh mesh;
	const std::vector<ExtrudeContour> region = square (10.f);

	// Moins de deux points : ce n'est pas un profil.
	Profile2D single;
	single.points.push_back (Vector2d (0.0, 0.0));
	EXPECT_FALSE (extrudeProfiledContours (region, single, box (0.f, 10.f), mesh, nullptr));

	// Ne part pas de l'origine : un profil mal oriente produirait une piece
	// repliee que rien ne signalerait. Meme refus que extrudeProfiledToMesh.
	Profile2D offOrigin;
	offOrigin.points.push_back (Vector2d (1.0, 0.0));
	offOrigin.points.push_back (Vector2d (2.0, 1.0));
	EXPECT_FALSE (extrudeProfiledContours (region, offOrigin, box (0.f, 10.f), mesh, nullptr));

	// Coordonnee negative.
	Profile2D negative;
	negative.points.push_back (Vector2d (0.0, 0.0));
	negative.points.push_back (Vector2d (2.0, -1.0));
	EXPECT_FALSE (extrudeProfiledContours (region, negative, box (0.f, 10.f), mesh, nullptr));

	// Profil plus PROFOND que la piece : il n'y a pas de place pour la paroi.
	EXPECT_FALSE (extrudeProfiledContours (region, chamferSplayProfile (1.0, 12.0),
	                                       box (0.f, 10.f), mesh, nullptr));

	// Hauteur nulle ou inversee.
	EXPECT_FALSE (extrudeProfiledContours (region, chamferSplayProfile (1.0, 1.0),
	                                       box (5.f, 5.f), mesh, nullptr));
	EXPECT_FALSE (extrudeProfiledContours (region, chamferSplayProfile (1.0, 1.0),
	                                       box (5.f, 1.f), mesh, nullptr));

	// Region vide.
	EXPECT_FALSE (extrudeProfiledContours ({}, chamferSplayProfile (1.0, 1.0),
	                                       box (0.f, 10.f), mesh, nullptr));
}

// --- un biseau n'est pas un chanfrein ---------------------------------------

TEST (TEST_cgmesh_extrude_profiled, a_bevel_is_a_chamfer_at_another_angle)
{
	// Le chanfrein est le cas w == d ; le biseau est le cas general. Les deux
	// passent par le meme producteur, et c'est le POINT : « chanfrein » et
	// « biseau » ne sont pas deux formes mais deux reglages d'une seule.
	Mesh square45, shallow;
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), chamferSplayProfile (2.0, 2.0),
	                                      box (0.f, 10.f), square45, nullptr));
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), chamferSplayProfile (2.0, 5.0),
	                                      box (0.f, 10.f), shallow, nullptr));

	// Meme retrait en plan, profondeur differente : le biseau mange plus.
	float lo1[3], hi1[3], lo2[3], hi2[3];
	bbox (square45, lo1, hi1);
	bbox (shallow, lo2, hi2);
	EXPECT_NEAR (hi1[0] - lo1[0], hi2[0] - lo2[0], 1e-3f);
	EXPECT_LT (signedVolume (shallow), signedVolume (square45));
}

TEST (TEST_cgmesh_extrude_profiled, a_plain_square_with_one_band_is_closed)
{
	Mesh m;
	ProfiledExtrudeStats s;
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), chamferSplayProfile (2.0, 2.0),
	                                      box (0.f, 10.f), m, &s));
	EXPECT_EQ (boundaryEdges (m), 0u) << "carre nu, une bande";
	EXPECT_EQ (s.steinerPoints, 0u);
	EXPECT_GT (signedVolume (m), 0.0);
}

TEST (TEST_cgmesh_extrude_profiled, a_plain_square_with_six_bands_is_closed)
{
	Mesh m;
	ProfiledExtrudeStats s;
	ASSERT_TRUE (extrudeProfiledContours (square (10.f), cavettoSplayProfile (1.5, 1.5, 6),
	                                      box (0.f, 8.f), m, &s));
	EXPECT_EQ (boundaryEdges (m), 0u) << "carre nu, six bandes";
	EXPECT_EQ (s.steinerPoints, 0u);
}

// --- les cas durs du catalogue de polices -----------------------------------
//
// Un carre ne prouve pas grand-chose : ce qui casse une arete profilee, ce sont
// les contours REELS -- deliés fins, contre-formes serrees, empattements, et des
// centaines de points par glyphe. Les quatre polices ci-dessous sont celles que
// le catalogue signale comme difficiles, et elles sont exercees ici sur la
// chaine complete : police -> contours -> arete profilee.

namespace {

std::vector<ExtrudeContour> textContours (const char* path, const char* text, float size)
{
	Font font;
	if (!font.loadFromFile (path)) return {};

	TextExtrudeOptions opt;
	opt.size = size;
	opt.flattenTol = size / 600.f;
	std::vector<ExtrudeContour> out;
	if (!text_to_contours (font, text, opt, out, nullptr, nullptr)) return {};
	return out;
}

}  // namespace

TEST (TEST_cgmesh_extrude_profiled, the_hard_fonts_of_the_catalogue_stay_watertight)
{
	struct Case { const char* file; const char* why; };
	const Case cases[] = {
		{ "./test/data/fonts/Cinzel.ttf",              "empattements lapidaires" },
		{ "./test/data/fonts/UnifrakturMaguntia.ttf",  "gothique textura" },
		{ "./test/data/fonts/Lobster.ttf",             "contours qui se chevauchent" },
		{ "./test/data/fonts/GreatVibes.ttf",          "delies fins, auto-intersections" },
	};

	int exercised = 0;
	for (const Case& c : cases)
	{
		const std::vector<ExtrudeContour> contours = textContours (c.file, "Ohm", 30.f);
		if (contours.empty ()) continue;   // police absente : le cas se saute
		exercised++;

		// Un chanfrein FIN : large, il mangerait les delies, ce que le cas
		// suivant mesure exprès. Ici on veut la COUTURE, pas la disparition.
		Mesh mesh;
		ProfiledExtrudeStats stats;
		ASSERT_TRUE (extrudeProfiledContours (contours, chamferSplayProfile (0.4, 0.4),
		                                      box (0.f, 3.f), mesh, &stats))
			<< c.file << " (" << c.why << ")";

		std::size_t holes = 0, pinches = 0;
		skinDefects (mesh, holes, pinches);
		// AUCUN TROU : c'est la seule exigence dure. Les PINCEMENTS, eux, sont
		// admis et rapportes -- Cinzel en produit sur ses empattements, la ou le
		// decalage vers l'interieur referme un serif sur lui-meme. La peau y reste
		// fermee ; elle cesse seulement d'etre une variete, ce qu'un slicer traite
		// comme il traite deux coques qui s'interpenetrent.
		EXPECT_EQ (holes, 0u)
			<< c.file << " (" << c.why << ") : la peau est OUVERTE";
		if (pinches > 0)
			std::cout << "[ INFO     ] " << c.file << " : " << pinches
			          << " arete(s) pincee(s) -- attendu sur un empattement qui se referme\n";
		EXPECT_GT (signedVolume (mesh), 0.0)
			<< c.file << " : normales retournees";
		// LE compteur d'honnetete : s'il grimpe sur une vraie police, la
		// reconnaissance des sommets par anneau ne suffit plus.
		EXPECT_EQ (stats.steinerPoints, 0u)
			<< c.file << " (" << c.why << ") : " << stats.steinerPoints
			<< " sommets qu'aucun anneau ne reclame";
	}
	ASSERT_GT (exercised, 0) << "aucune police de test presente : le cas n'a rien exerce";
}

TEST (TEST_cgmesh_extrude_profiled, a_wide_profile_eats_the_thin_strokes_of_a_script)
{
	// GreatVibes est signalee « delies fins, auto-intersections -- cas dur » dans
	// le catalogue. Un chanfrein plus large que la demi-epaisseur d'un delie le
	// fait DISPARAITRE : c'est le prix du decalage interieur retenu en D1, et la
	// raison pour laquelle `vanishedPieces` existe.
	const std::vector<ExtrudeContour> contours =
		textContours ("./test/data/fonts/GreatVibes.ttf", "elle", 30.f);
	if (contours.empty ()) GTEST_SKIP () << "GreatVibes.ttf absent";

	Mesh fine, wide;
	ProfiledExtrudeStats fineStats, wideStats;
	ASSERT_TRUE (extrudeProfiledContours (contours, chamferSplayProfile (0.2, 0.2),
	                                      box (0.f, 3.f), fine, &fineStats));
	const bool built = extrudeProfiledContours (contours, chamferSplayProfile (1.5, 1.5),
	                                            box (0.f, 3.f), wide, &wideStats);

	// Deux issues acceptables, et une seule inacceptable : soit la piece se
	// construit en signalant ce qu'elle a perdu, soit elle refuse. Ce qu'on
	// interdit, c'est qu'elle perde des morceaux SANS LE DIRE.
	if (built)
	{
		EXPECT_EQ (boundaryEdges (wide), 0u);
		EXPECT_GT (wideStats.vanishedPieces, fineStats.vanishedPieces)
			<< "un chanfrein sept fois plus large n'a fait disparaitre aucun delie : "
			   "le compteur ne mesure rien";
	}
	SUCCEED () << "chanfrein large : "
	           << (built ? "construit" : "refuse")
	           << ", morceaux perdus " << wideStats.vanishedPieces;
}
