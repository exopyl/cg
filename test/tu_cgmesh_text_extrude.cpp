#include <gtest/gtest.h>

#include "../src/cgmesh/text_extrude.h"

#include "../src/cgmath/font.h"
#include "../src/cgmesh/mesh.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// ===========================================================================
//  Texte -> solide extrude
// ===========================================================================
// L'oracle central est le VOLUME SIGNE : pour un prisme ferme il vaut
// exactement aire_du_capot x hauteur. Aucun compte de faces ne le remplace --
// ExtrudedMeshBuilder emet volontairement capots et parois en blocs de sommets
// DISJOINTS, donc des aretes de bord existent par construction. La mesure est
// reprise de tu_cgmesh_extrude_contours.cpp, ou elle a deja fait ses preuves :
// c'est elle qui avait revele que 190 aretes sur 1633 sortaient sans paroi.
//
// Les DEUX voies a contours du lecteur de polices sont exercees ici, chacune par
// une police livree avec le depot :
//
//   BloomingGrove.otf  OpenType/CFF -- contours en CUBIQUES, aucun glyphe
//                      composite, et crenage dans un format que nous ne lisons
//                      pas (KerningStatus::Unsupported).
//   DejaVuSans.ttf     TrueType -- contours en QUADRATIQUES, glyphes composites,
//                      et table `kern` Microsoft v0 lue (KerningStatus::Applied).
//
// Une seule des deux ne suffit pas : la branche QUADRATIQUE de l'aplatissement
// et le contournement du crenage ne sont atteignables que par la TrueType, les
// glyphes composites non plus.

namespace {

const char* kCffFont      = "./test/data/fonts/BloomingGrove.otf";
const char* kTrueTypeFont = "./test/data/fonts/DejaVuSans.ttf";

std::unique_ptr<Font> loadFont (const char* path)
{
	auto font = std::make_unique<Font>();
	if (!font->loadFromFile (path)) return nullptr;
	return font;
}

// Sert de garde-fou aux tests qui ciblent UNE branche d'aplatissement : sans lui,
// un test cense exercer les quadratiques passerait tel quel sur une police
// cubique, sans rien exercer du tout.
int countSegments (const std::vector<GlyphContour>& contours, GlyphSegment::Kind kind)
{
	int n = 0;
	for (const GlyphContour& c : contours)
		for (const GlyphSegment& s : c.segments)
			if (s.kind == kind) n++;
	return n;
}

// Volume signe (theoreme de la divergence) et aire du capot superieur.
void measure (Mesh& m, float height, double& volume, double& topArea)
{
	volume = 0.0; topArea = 0.0;
	for (unsigned int f = 0; f < m.GetNFaces(); f++)
	{
		if (m.GetFaceNVertices (f) != 3) continue;
		float p[3][3];
		for (int k = 0; k < 3; k++)
			m.GetVertex ((unsigned int)m.GetFaceVertex (f, k), p[k]);

		volume += ((double)p[0][0]*((double)p[1][1]*p[2][2]-(double)p[2][1]*p[1][2])
		         - (double)p[0][1]*((double)p[1][0]*p[2][2]-(double)p[2][0]*p[1][2])
		         + (double)p[0][2]*((double)p[1][0]*p[2][1]-(double)p[2][0]*p[1][1])) / 6.0;

		const bool atTop = std::fabs (p[0][2]-height) < 1e-4f
		                && std::fabs (p[1][2]-height) < 1e-4f
		                && std::fabs (p[2][2]-height) < 1e-4f;
		if (atTop)
			topArea += (((double)p[1][0]-p[0][0])*((double)p[2][1]-p[0][1])
			          - ((double)p[1][1]-p[0][1])*((double)p[2][0]-p[0][0])) / 2.0;
	}
}

void bbox (Mesh& m, float lo[3], float hi[3])
{
	for (int k = 0; k < 3; k++) { lo[k] = 1e30f; hi[k] = -1e30f; }
	for (unsigned int v = 0; v < m.GetNVertices(); v++)
	{
		float p[3];
		m.GetVertex (v, p);
		for (int k = 0; k < 3; k++)
		{
			lo[k] = std::min (lo[k], p[k]);
			hi[k] = std::max (hi[k], p[k]);
		}
	}
}

TextExtrudeOptions defaults ()
{
	TextExtrudeOptions opt;
	opt.size       = 1.f;
	opt.depth      = 0.25f;
	opt.flattenTol = 0.005f;
	return opt;
}

}  // namespace

// ---------------------------------------------------------------------------
// Etancheite
// ---------------------------------------------------------------------------

// Le temoin. Sans lui, un test qui passerait pour de mauvaises raisons ne se
// distinguerait pas.
TEST(TEST_cgmesh_text_extrude, an_extruded_letter_is_watertight)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "A", opt));
	ASSERT_NE (m, nullptr);

	double volume = 0, topArea = 0;
	measure (*m, opt.depth, volume, topArea);

	EXPECT_GT (std::fabs (topArea), 0.0) << "capot vide";
	EXPECT_NEAR (volume, topArea * (double)opt.depth, 1e-6)
		<< "volume " << volume << " au lieu de " << (topArea * opt.depth)
		<< " : des parois manquent";
}

TEST(TEST_cgmesh_text_extrude, a_whole_word_is_watertight)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "Hello", opt));
	ASSERT_NE (m, nullptr);

	double volume = 0, topArea = 0;
	measure (*m, opt.depth, volume, topArea);
	EXPECT_NEAR (volume, topArea * (double)opt.depth, 1e-6);
}

// La branche QUADRATIQUE de l'aplatissement n'est atteignable que par une police
// TrueType : les charstrings Type 2 de la police CFF n'emettent que des
// cubiques. Le o de DejaVu est decrit uniquement par des quadratiques.
TEST(TEST_cgmesh_text_extrude, a_quadratic_glyph_is_watertight)
{
	auto font = loadFont (kTrueTypeFont);
	ASSERT_NE (font, nullptr);

	const auto outline = font->glyphContours (font->glyphIndex (U'o'));
	ASSERT_GT (countSegments (outline, GlyphSegment::Kind::Quadratic), 0)
		<< "ce glyphe n'exerce pas la branche quadratique";
	ASSERT_EQ (countSegments (outline, GlyphSegment::Kind::Cubic), 0);

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "o", opt));
	ASSERT_NE (m, nullptr);

	double volume = 0, topArea = 0;
	measure (*m, opt.depth, volume, topArea);

	EXPECT_GT (std::fabs (topArea), 0.0) << "capot vide";
	EXPECT_NEAR (volume, topArea * (double)opt.depth, 1e-6)
		<< "volume " << volume << " au lieu de " << (topArea * opt.depth)
		<< " : des parois manquent";

	// Et la branche SUBDIVISE reellement. Ce glyphe ne porte aucun segment
	// droit : une quadratique rendue comme une simple corde donnerait donc le
	// meme nombre de sommets aux deux tolerances.
	TextExtrudeOptions coarse = opt, fine = opt;
	coarse.flattenTol = 0.05f;
	fine.flattenTol   = 0.002f;
	std::unique_ptr<Mesh> c (text_to_extruded_mesh (*font, "o", coarse));
	std::unique_ptr<Mesh> f (text_to_extruded_mesh (*font, "o", fine));
	ASSERT_NE (c, nullptr);
	ASSERT_NE (f, nullptr);
	EXPECT_GT (f->GetNVertices(), c->GetNVertices())
		<< "la tolerance n'agit pas : les quadratiques ne sont pas subdivisees";
}

// Un glyphe composite reference d'autres glyphes, chacun avec sa translation :
// l'accent arrive donc comme un contour SEPARE et FLOTTANT, ni imbrique dans la
// lettre ni connexe a elle. Deux ilots disjoints dans un meme Append sont le cas
// que la tessellation non-zero et l'emission des parois doivent tenir.
TEST(TEST_cgmesh_text_extrude, a_composite_glyph_is_watertight)
{
	auto font = loadFont (kTrueTypeFont);
	ASSERT_NE (font, nullptr);

	// Point de code et chaine ecrits en ECHAPPEMENT : ce fichier n'a pas de BOM,
	// et MSVC lirait sinon les octets UTF-8 comme autant de caracteres CP1252.
	const auto outline = font->glyphContours (font->glyphIndex (U'\u00E9'));
	ASSERT_EQ (outline.size(), 3u) << "e + contre-forme + accent";

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "\xC3\xA9", opt));
	ASSERT_NE (m, nullptr);

	double volume = 0, topArea = 0;
	measure (*m, opt.depth, volume, topArea);

	EXPECT_GT (std::fabs (topArea), 0.0) << "capot vide";
	EXPECT_NEAR (volume, topArea * (double)opt.depth, 1e-6)
		<< "volume " << volume << " au lieu de " << (topArea * opt.depth)
		<< " : l'accent flottant a coute des parois";
}

// ---------------------------------------------------------------------------
// Les contre-formes
// ---------------------------------------------------------------------------

// LE test qui compte pour de la typographie : le trou du « o » doit rester
// ouvert. Sa surface de capot doit donc etre STRICTEMENT inferieure a celle du
// « O » qui l'englobe... comparaison indisponible ici, on compare donc au
// meme glyphe dont on connait l'emprise : l'aire du capot doit etre nettement
// inferieure a celle du rectangle englobant, ce qu'un « o » bouche ne
// satisferait pas.
TEST(TEST_cgmesh_text_extrude, a_counter_stays_open)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "o", opt));
	ASSERT_NE (m, nullptr);

	double volume = 0, topArea = 0;
	measure (*m, opt.depth, volume, topArea);

	float lo[3], hi[3];
	bbox (*m, lo, hi);
	const double boxArea = (double)(hi[0]-lo[0]) * (double)(hi[1]-lo[1]);

	ASSERT_GT (boxArea, 0.0);
	// Un anneau ne couvre qu'une fraction de son rectangle englobant. Un « o »
	// dont la contre-forme aurait ete rebouchee en couvrirait au contraire
	// l'essentiel -- un disque plein en occupe deja ~78 %.
	EXPECT_LT (std::fabs (topArea), 0.70 * boxArea)
		<< "aire de capot " << std::fabs (topArea) << " pour une emprise de "
		<< boxArea << " : la contre-forme semble bouchee";

	// Et il reste ferme.
	EXPECT_NEAR (volume, topArea * (double)opt.depth, 1e-6);
}

// Le meme oracle sur la voie TrueType : la contre-forme doit rester ouverte
// quelle que soit la nature des segments qui la decrivent. Une quadratique
// aplatie a l'envers reboucherait le trou sans que l'etancheite s'en apercoive.
TEST(TEST_cgmesh_text_extrude, a_truetype_counter_stays_open)
{
	auto font = loadFont (kTrueTypeFont);
	ASSERT_NE (font, nullptr);

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "o", opt));
	ASSERT_NE (m, nullptr);

	double volume = 0, topArea = 0;
	measure (*m, opt.depth, volume, topArea);

	float lo[3], hi[3];
	bbox (*m, lo, hi);
	const double boxArea = (double)(hi[0]-lo[0]) * (double)(hi[1]-lo[1]);

	ASSERT_GT (boxArea, 0.0);
	EXPECT_LT (std::fabs (topArea), 0.70 * boxArea)
		<< "aire de capot " << std::fabs (topArea) << " pour une emprise de "
		<< boxArea << " : la contre-forme semble bouchee";

	EXPECT_NEAR (volume, topArea * (double)opt.depth, 1e-6);
}

// ---------------------------------------------------------------------------
// Les parametres agissent
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_text_extrude, depth_scales_the_volume_linearly)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	TextExtrudeOptions thin = defaults(), thick = defaults();
	thin.depth  = 0.1f;
	thick.depth = 0.3f;

	std::unique_ptr<Mesh> a (text_to_extruded_mesh (*font, "A", thin));
	std::unique_ptr<Mesh> b (text_to_extruded_mesh (*font, "A", thick));
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	double va = 0, ta = 0, vb = 0, tb = 0;
	measure (*a, thin.depth,  va, ta);
	measure (*b, thick.depth, vb, tb);

	EXPECT_NEAR (std::fabs (ta), std::fabs (tb), 1e-9) << "meme capot";
	EXPECT_NEAR (std::fabs (vb), 3.0 * std::fabs (va), 1e-6);
}

TEST(TEST_cgmesh_text_extrude, size_scales_the_footprint)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	TextExtrudeOptions small = defaults(), big = defaults();
	small.size = 1.f;
	big.size   = 2.f;
	// Meme finesse RELATIVE, pour que les deux capots soient homothetiques.
	big.flattenTol = 2.f * small.flattenTol;

	std::unique_ptr<Mesh> a (text_to_extruded_mesh (*font, "A", small));
	std::unique_ptr<Mesh> b (text_to_extruded_mesh (*font, "A", big));
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	float loA[3], hiA[3], loB[3], hiB[3];
	bbox (*a, loA, hiA);
	bbox (*b, loB, hiB);

	EXPECT_NEAR ((double)(hiB[0]-loB[0]), 2.0 * (hiA[0]-loA[0]), 1e-4);
	EXPECT_NEAR ((double)(hiB[1]-loB[1]), 2.0 * (hiA[1]-loA[1]), 1e-4);
}

// La tolerance d'aplatissement est en unites MONDE : la resserrer doit raffiner
// le contour, donc produire davantage de sommets.
TEST(TEST_cgmesh_text_extrude, a_tighter_tolerance_refines_the_outline)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	TextExtrudeOptions coarse = defaults(), fine = defaults();
	coarse.flattenTol = 0.05f;
	fine.flattenTol   = 0.002f;

	std::unique_ptr<Mesh> a (text_to_extruded_mesh (*font, "o", coarse));
	std::unique_ptr<Mesh> b (text_to_extruded_mesh (*font, "o", fine));
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	EXPECT_GT (b->GetNVertices(), a->GetNVertices());
}

// Un texte multiligne doit occuper deux fois la hauteur, et rester ferme.
TEST(TEST_cgmesh_text_extrude, a_second_line_sits_below_the_first)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	const TextExtrudeOptions opt = defaults();
	std::unique_ptr<Mesh> one (text_to_extruded_mesh (*font, "A", opt));
	std::unique_ptr<Mesh> two (text_to_extruded_mesh (*font, "A\nA", opt));
	ASSERT_NE (one, nullptr);
	ASSERT_NE (two, nullptr);

	float lo1[3], hi1[3], lo2[3], hi2[3];
	bbox (*one, lo1, hi1);
	bbox (*two, lo2, hi2);

	EXPECT_LT (lo2[1], lo1[1]) << "la seconde ligne doit descendre";
	EXPECT_NEAR ((double)hi2[1], (double)hi1[1], 1e-5) << "le haut ne bouge pas";

	double v = 0, t = 0;
	measure (*two, opt.depth, v, t);
	EXPECT_NEAR (v, t * (double)opt.depth, 1e-6);
}

TEST(TEST_cgmesh_text_extrude, centering_moves_the_bbox_onto_the_origin)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	TextExtrudeOptions opt = defaults();
	opt.centerOnOrigin = true;

	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "Hello", opt));
	ASSERT_NE (m, nullptr);

	float lo[3], hi[3];
	bbox (*m, lo, hi);
	// L'emprise recentree est TYPOGRAPHIQUE (avances, ascendante/descendante) et
	// non celle des contours : le centre des contours n'a donc aucune raison
	// d'etre exactement en 0, mais il doit en etre proche a l'echelle du corps.
	EXPECT_LT (std::fabs (0.5f * (lo[0] + hi[0])), 0.25f * opt.size);
	EXPECT_LT (std::fabs (0.5f * (lo[1] + hi[1])), 0.25f * opt.size);
}

// Le crenage est le seul reglage de mise en page dont l'effet exige une police
// dont la table soit LUE : sur la police CFF du depot, kern() rend 0 et l'option
// n'a aucun effet observable. C'est donc ici, et nulle part ailleurs, que le
// contournement du crenage est verifie de bout en bout.
TEST(TEST_cgmesh_text_extrude, kerning_tightens_the_word)
{
	auto font = loadFont (kTrueTypeFont);
	ASSERT_NE (font, nullptr);

	const int av = font->kern (font->glyphIndex (U'A'), font->glyphIndex (U'V'));
	ASSERT_EQ (font->kerningStatus(), KerningStatus::Applied);
	ASSERT_LT (av, 0) << "A/V doit etre resserre, sinon le test ne prouve rien";

	TextExtrudeOptions kerned = defaults(), loose = defaults();
	kerned.kerning = true;
	loose.kerning  = false;

	std::unique_ptr<Mesh> a (text_to_extruded_mesh (*font, "AV", kerned));
	std::unique_ptr<Mesh> b (text_to_extruded_mesh (*font, "AV", loose));
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	float loA[3], hiA[3], loB[3], hiB[3];
	bbox (*a, loA, hiA);
	bbox (*b, loB, hiB);

	EXPECT_LT (hiA[0]-loA[0], hiB[0]-loB[0])
		<< "le crenage n'a pas resserre le maillage";

	// L'ecart vaut EXACTEMENT la correction de la paire ramenee aux unites
	// monde : seul le V se deplace, et c'est lui qui porte le bord droit de
	// l'emprise. La valeur est lue dans la police, non figee ici.
	const double expected = -(double)av * (double)kerned.size
	                      / (double)font->unitsPerEm();
	EXPECT_NEAR ((double)(hiB[0]-loB[0]) - (double)(hiA[0]-loA[0]), expected, 1e-5);

	// Le crenage DEPLACE, il ne deforme pas : chaque glyphe etant extrude par un
	// Append distinct, la somme des volumes est invariante par translation.
	double va = 0, ta = 0, vb = 0, tb = 0;
	measure (*a, kerned.depth, va, ta);
	measure (*b, loose.depth,  vb, tb);
	EXPECT_NEAR (va, vb, 1e-6) << "le crenage a change le volume";
	EXPECT_NEAR (ta, tb, 1e-6) << "le crenage a change le capot";
}

// ---------------------------------------------------------------------------
// Fusion des recouvrements
// ---------------------------------------------------------------------------

// Quand deux glyphes se chevauchent, la passe Clipper2 doit produire un solide
// dont le capot ne compte la zone commune QU'UNE FOIS. Sans elle, les deux
// capots se superposent et l'aire est comptee deux fois.
TEST(TEST_cgmesh_text_extrude, unioning_overlaps_removes_the_double_count)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	TextExtrudeOptions apart = defaults(), merged = defaults();
	// Interlettrage tres negatif : les deux « o » se recouvrent largement.
	apart.letterSpacing  = -0.4f;
	merged.letterSpacing = -0.4f;
	merged.unionOverlaps = true;

	std::unique_ptr<Mesh> a (text_to_extruded_mesh (*font, "oo", apart));
	std::unique_ptr<Mesh> b (text_to_extruded_mesh (*font, "oo", merged));
	ASSERT_NE (a, nullptr);
	ASSERT_NE (b, nullptr);

	double va = 0, ta = 0, vb = 0, tb = 0;
	measure (*a, apart.depth,  va, ta);
	measure (*b, merged.depth, vb, tb);

	EXPECT_LT (std::fabs (tb), std::fabs (ta))
		<< "la fusion devrait supprimer la surface comptee deux fois";
	EXPECT_NEAR (vb, tb * (double)merged.depth, 1e-6) << "le solide fusionne reste ferme";
}

// ---------------------------------------------------------------------------
// Degradations
// ---------------------------------------------------------------------------

TEST(TEST_cgmesh_text_extrude, an_unloaded_font_produces_nothing)
{
	const Font font;
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (font, "A", defaults()));
	EXPECT_EQ (m, nullptr);
}

TEST(TEST_cgmesh_text_extrude, an_empty_text_produces_nothing)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "", defaults()));
	EXPECT_EQ (m, nullptr);
}

// Une chaine faite uniquement de blancs occupe de la place mais n'emet aucun
// contour : il n'y a pas de maillage a produire, et ce n'est pas une erreur.
TEST(TEST_cgmesh_text_extrude, blanks_alone_produce_nothing)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);
	std::unique_ptr<Mesh> m (text_to_extruded_mesh (*font, "   ", defaults()));
	EXPECT_EQ (m, nullptr);
}

// Un glyphe absent ne doit ni faire echouer, ni decaler le reste : il occupe
// son avance (nulle pour .notdef ici) et n'emet rien.
TEST(TEST_cgmesh_text_extrude, a_missing_glyph_degrades_without_failing)
{
	auto font = loadFont (kCffFont);
	ASSERT_NE (font, nullptr);

	// BloomingGrove n'a pas d'emoji.
	std::unique_ptr<Mesh> m (
		text_to_extruded_mesh (*font, "A\xF0\x9F\x98\x80" "A", defaults()));
	ASSERT_NE (m, nullptr);

	double v = 0, t = 0;
	measure (*m, defaults().depth, v, t);
	EXPECT_NEAR (v, t * (double)defaults().depth, 1e-6);
}
