#include <gtest/gtest.h>

#include "../src/cgmath/font.h"

#include <fstream>
#include <string>
#include <vector>

// ===========================================================================
//  Chargement de police et extraction de contours
// ===========================================================================
// La police livree avec le depot, BloomingGrove.otf (Nathan Eady, domaine
// public), couvre la voie OpenType/CFF : signature OTTO, em de 1000, contours
// en CUBIQUES. C'est la moitie qui compte le plus a verrouiller, la voie CFF
// etant celle dont le support par stb_truetype etait le moins acquis.
//
// La voie TrueType (`glyf`, quadratiques, glyphes composites) n'a PAS de police
// libre dans le depot. Les tests qui l'exercent cherchent donc une TTF sur le
// systeme et se DECLARENT SAUTES quand ils n'en trouvent pas, plutot que de
// passer pour de mauvaises raisons.

namespace {

const char* kCffFont = "./test/data/fonts/BloomingGrove.otf";

// Polices TrueType usuelles. La premiere trouvee sert ; aucune n'est requise.
const char* kTrueTypeCandidates[] = {
	"C:/Windows/Fonts/arial.ttf",
	"C:/Windows/Fonts/segoeui.ttf",
	"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	"/System/Library/Fonts/Supplemental/Arial.ttf",
};

bool exists (const char* path)
{
	std::ifstream f (path, std::ios::binary);
	return f.good();
}

const char* findTrueTypeFont ()
{
	for (const char* p : kTrueTypeCandidates)
		if (exists (p)) return p;
	return nullptr;
}

int countSegments (const std::vector<GlyphContour>& contours, GlyphSegment::Kind kind)
{
	int n = 0;
	for (const GlyphContour& c : contours)
		for (const GlyphSegment& s : c.segments)
			if (s.kind == kind) n++;
	return n;
}

}  // namespace

// ---------------------------------------------------------------------------
// Refus diagnostiques
// ---------------------------------------------------------------------------
// Un format hors perimetre doit etre REFUSE EN LE NOMMANT, pas echouer plus
// loin sur un symptome sans rapport. On ne peut pas verifier le message depuis
// gtest, mais on verifie qu'aucun de ces flux n'est accepte par megarde.

TEST(TEST_cgmath_font, unsupported_containers_are_rejected)
{
	// Buffers assez longs pour depasser le garde-fou de taille : ce que l'on veut
	// exercer ici, c'est la reconnaissance de SIGNATURE, pas le refus d'un
	// fichier tronque (teste separement ci-dessous).
	const auto padded = [] (std::initializer_list<unsigned char> head) {
		std::vector<unsigned char> b (head);
		b.resize (64, 0);
		return b;
	};

	struct Case { const char* what; std::vector<unsigned char> bytes; };
	const std::vector<Case> cases = {
		{ "WOFF",  padded ({ 'w','O','F','F' }) },
		{ "WOFF2", padded ({ 'w','O','F','2' }) },
		{ "Type1", padded ({ '%','!','P','S' }) },
		{ "PFB",   padded ({ 0x80, 0x01, 0x00, 0x00 }) },
		{ "SVG",   padded ({ '<','?','x','m' }) },
		{ "BDF",   padded ({ 'S','T','A','R' }) },
		{ "bruit", padded ({ 0x12, 0x34, 0x56, 0x78 }) },
	};

	for (const Case& c : cases)
	{
		Font font;
		EXPECT_FALSE (font.loadFromMemory (c.bytes)) << c.what;
		EXPECT_FALSE (font.isValid()) << c.what;
	}
}

// Un flux trop court pour porter un en-tete sfnt doit etre refuse AVANT toute
// lecture de table : sans ce garde-fou, la recherche du nombre de polices lirait
// au-dela du tampon.
TEST(TEST_cgmath_font, a_stream_too_short_for_a_header_is_rejected)
{
	for (size_t n : { (size_t)0, (size_t)4, (size_t)8, (size_t)11 })
	{
		Font font;
		// Signature de collection : c'est le cas ou la lecture deborderait le plus
		// loin (nombre de polices a l'offset 8).
		std::vector<unsigned char> bytes { 't','t','c','f' };
		bytes.resize (n, 0);
		EXPECT_FALSE (font.loadFromMemory (bytes)) << n << " octets";
	}
}

// Une signature acceptable mais aucune table exploitable ne doit pas passer
// non plus : c'est alors stbtt_InitFont qui doit refuser.
TEST(TEST_cgmath_font, a_headerless_sfnt_is_rejected)
{
	Font font;
	std::vector<unsigned char> bytes { 0x00, 0x01, 0x00, 0x00 };
	bytes.resize (64, 0);          // en-tete valide, zero table
	EXPECT_FALSE (font.loadFromMemory (bytes));
}

TEST(TEST_cgmath_font, a_missing_file_is_rejected)
{
	Font font;
	EXPECT_FALSE (font.loadFromFile ("./test/data/fonts/does_not_exist.ttf"));
	EXPECT_FALSE (font.isValid());
}

// ---------------------------------------------------------------------------
// La voie OpenType / CFF
// ---------------------------------------------------------------------------

TEST(TEST_cgmath_font, cff_font_loads_with_its_metrics)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));
	ASSERT_TRUE (font.isValid());

	// 1000 est l'em typique du CFF, contre 2048 en TrueType. Le lire faux
	// mettrait tout le texte a l'echelle d'un facteur 2.
	EXPECT_EQ (font.unitsPerEm(), 1000);
	EXPECT_GT (font.numGlyphs(), 0);
	EXPECT_EQ (font.numFonts(), 1);

	int asc = 0, desc = 0, gap = 0;
	font.vMetrics (asc, desc, gap);
	EXPECT_GT (asc, 0)  << "ascendante";
	EXPECT_LT (desc, 0) << "descendante, negative par convention";
}

TEST(TEST_cgmath_font, cff_cmap_maps_present_and_absent_codepoints)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));

	EXPECT_NE (font.glyphIndex (U'A'), 0);
	EXPECT_NE (font.glyphIndex (U'o'), 0);

	// Un point de code absent doit rendre .notdef, sans planter : c'est la
	// degradation propre sur laquelle la mise en page s'appuie.
	EXPECT_EQ (font.glyphIndex (U'\U0001F600'), 0) << "emoji absent";
	EXPECT_EQ (font.glyphIndex (U'\U0010FFFE'), 0) << "hors plan";
}

// La voie charstrings Type 2 doit emettre des CUBIQUES : c'est ce qui
// distingue le chemin CFF du chemin `glyf`, et c'est le risque qui restait
// ouvert dans l'analyse de faisabilite.
TEST(TEST_cgmath_font, cff_outlines_are_cubic)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));

	const auto contours = font.glyphContours (font.glyphIndex (U'A'));
	ASSERT_FALSE (contours.empty());
	EXPECT_GT (countSegments (contours, GlyphSegment::Kind::Cubic), 0)
		<< "aucune cubique : le chemin CFF n'a pas ete emprunte";
	EXPECT_EQ (countSegments (contours, GlyphSegment::Kind::Quadratic), 0)
		<< "une quadratique dans une police CFF";
}

// Le test qui compte pour l'extrusion : une contre-forme est un SECOND contour.
// S'il manquait, le « o » sortirait bouche.
TEST(TEST_cgmath_font, a_counter_is_a_second_contour)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));

	const auto o = font.glyphContours (font.glyphIndex (U'o'));
	EXPECT_EQ (o.size(), 2u) << "le o doit avoir un contour exterieur et sa contre-forme";

	for (const GlyphContour& c : o)
		EXPECT_GE (c.segments.size(), 2u) << "contour degenere";
}

TEST(TEST_cgmath_font, metrics_are_per_glyph)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));

	const int a = font.glyphIndex (U'A');
	const int i = font.glyphIndex (U'i');
	ASSERT_NE (a, 0);
	ASSERT_NE (i, 0);

	EXPECT_GT (font.advance (a), 0);
	EXPECT_GT (font.advance (i), 0);
	EXPECT_GT (font.advance (a), font.advance (i)) << "un A est plus large qu'un i";
}

// ---------------------------------------------------------------------------
// Bornes et etats degrades
// ---------------------------------------------------------------------------

// L'API prend un index de glyphe NU : un index hors bornes ferait lire `hmtx`
// et `loca` a cote, sans que stb ne s'en apercoive.
TEST(TEST_cgmath_font, out_of_range_glyphs_degrade_quietly)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));

	const int beyond = font.numGlyphs() + 1000;
	EXPECT_FALSE (font.hasGlyph (beyond));
	EXPECT_FALSE (font.hasGlyph (-1));
	EXPECT_EQ (font.advance (beyond), 0);
	EXPECT_EQ (font.lsb (-1), 0);
	EXPECT_TRUE (font.glyphContours (beyond).empty());
}

// Une police non chargee doit repondre a tout sans planter : la mise en page
// l'interroge avant meme de savoir si le chargement a reussi.
TEST(TEST_cgmath_font, an_unloaded_font_answers_without_crashing)
{
	Font font;
	EXPECT_FALSE (font.isValid());
	EXPECT_EQ (font.unitsPerEm(), 0);
	EXPECT_EQ (font.numGlyphs(), 0);
	EXPECT_EQ (font.glyphIndex (U'A'), 0);
	EXPECT_EQ (font.advance (0), 0);
	EXPECT_EQ (font.kern (0, 0), 0);
	EXPECT_TRUE (font.glyphContours (0).empty());
	EXPECT_EQ (font.kerningStatus(), KerningStatus::None);
}

// stb_truetype ne garde qu'un POINTEUR dans le buffer d'octets : deplacer la
// police doit deplacer le buffer avec elle, sinon le pointeur pend.
TEST(TEST_cgmath_font, moving_a_font_keeps_its_buffer_alive)
{
	Font moved;
	{
		Font font;
		ASSERT_TRUE (font.loadFromFile (kCffFont));
		moved = std::move (font);
	}
	ASSERT_TRUE (moved.isValid());
	EXPECT_EQ (moved.unitsPerEm(), 1000);
	EXPECT_FALSE (moved.glyphContours (moved.glyphIndex (U'A')).empty());
}

// ---------------------------------------------------------------------------
// Crenage : ce qui est lu, et ce qui est signale comme non lu
// ---------------------------------------------------------------------------

// BloomingGrove porte une table `kern` au format Apple v1.0, en sous-tables de
// format 2 : nous ne savons pas la lire. L'exigence n'est PAS d'y arriver, mais
// de ne pas mentir -- l'etat doit dire « non pris en charge », jamais « aucun
// crenage ».
TEST(TEST_cgmath_font, unreadable_kerning_is_reported_not_hidden)
{
	Font font;
	ASSERT_TRUE (font.loadFromFile (kCffFont));

	EXPECT_NE (font.kerningStatus(), KerningStatus::Applied);
	if (font.kerningStatus() == KerningStatus::Unsupported)
		EXPECT_EQ (font.kernPairCount(), 0u);
	EXPECT_EQ (font.kern (font.glyphIndex (U'A'), font.glyphIndex (U'V')), 0);
}

// ---------------------------------------------------------------------------
// La voie TrueType, quand une police est disponible
// ---------------------------------------------------------------------------

TEST(TEST_cgmath_font, truetype_outlines_are_quadratic)
{
	const char* path = findTrueTypeFont();
	if (!path) GTEST_SKIP() << "aucune police TrueType sur ce systeme";

	Font font;
	ASSERT_TRUE (font.loadFromFile (path)) << path;
	EXPECT_GT (font.unitsPerEm(), 0);

	const auto contours = font.glyphContours (font.glyphIndex (U'o'));
	ASSERT_FALSE (contours.empty());
	EXPECT_GT (countSegments (contours, GlyphSegment::Kind::Quadratic), 0)
		<< "aucune quadratique : le chemin glyf n'a pas ete emprunte";
	EXPECT_EQ (contours.size(), 2u) << "contre-forme du o";
}

// Un glyphe accentue est COMPOSITE : la lettre de base et l'accent sont deux
// glyphes references, chacun avec sa translation. C'est le cas que stb resout
// en recursif, et celui qui asserte sur la variante par appariement de points.
TEST(TEST_cgmath_font, composite_glyphs_resolve_to_several_contours)
{
	const char* path = findTrueTypeFont();
	if (!path) GTEST_SKIP() << "aucune police TrueType sur ce systeme";

	Font font;
	ASSERT_TRUE (font.loadFromFile (path)) << path;

	// Ecrit en echappement et non litteralement : ce fichier n'a pas de BOM, et
	// MSVC lirait sinon les deux octets UTF-8 comme deux caracteres CP1252.
	const int glyph = font.glyphIndex (U'\u00E9');   // e accent aigu
	if (glyph == 0) GTEST_SKIP() << "cette police n'a pas de e accent aigu";

	const auto contours = font.glyphContours (glyph);
	EXPECT_GE (contours.size(), 2u) << "e + accent : au moins deux contours";
}

// Sur les polices a table `kern` Microsoft v0, la lecture directe doit rendre
// un crenage EFFECTIF -- c'est tout l'objet du contournement : l'API de
// dispatch de stb, elle, rend 0 sur ces memes polices.
TEST(TEST_cgmath_font, microsoft_kern_tables_are_actually_read)
{
	const char* path = findTrueTypeFont();
	if (!path) GTEST_SKIP() << "aucune police TrueType sur ce systeme";

	Font font;
	ASSERT_TRUE (font.loadFromFile (path)) << path;

	if (font.kerningStatus() != KerningStatus::Applied)
		GTEST_SKIP() << "cette police n'a pas de table kern lisible";

	EXPECT_GT (font.kernPairCount(), 0u);
	// A/V est la paire de crenage canonique : les deux diagonales s'emboitent,
	// la correction est negative.
	const int av = font.kern (font.glyphIndex (U'A'), font.glyphIndex (U'V'));
	EXPECT_LT (av, 0) << "A/V devrait etre resserre";
}

// Une collection .ttc expose plusieurs polices sous un seul fichier ; l'index
// doit reellement selectionner, pas etre ignore.
TEST(TEST_cgmath_font, font_collections_are_indexed)
{
	const char* path = "C:/Windows/Fonts/cambria.ttc";
	if (!exists (path)) GTEST_SKIP() << "aucune collection .ttc sur ce systeme";

	Font first;
	ASSERT_TRUE (first.loadFromFile (path, 0));
	if (first.numFonts() < 2) GTEST_SKIP() << "collection a une seule police";

	Font second;
	ASSERT_TRUE (second.loadFromFile (path, 1));
	EXPECT_EQ (second.numFonts(), first.numFonts());

	// Deux polices distinctes de la collection : au moins un trait doit
	// differer, sans quoi l'index n'aurait pas ete pris en compte.
	int a1 = 0, d1 = 0, g1 = 0, a2 = 0, d2 = 0, g2 = 0;
	first.vMetrics (a1, d1, g1);
	second.vMetrics (a2, d2, g2);

	const bool differs =
		   (first.numGlyphs() != second.numGlyphs())
		|| (first.advance (first.glyphIndex (U'A'))
		    != second.advance (second.glyphIndex (U'A')))
		|| (a1 != a2) || (d1 != d2) || (g1 != g2)
		|| (first.kernPairCount() != second.kernPairCount())
		|| (first.glyphContours (first.glyphIndex (U'A')).size()
		    != second.glyphContours (second.glyphIndex (U'A')).size());

	EXPECT_TRUE (differs)
		<< "l'index de police semble ignore : glyphes " << first.numGlyphs()
		<< "/" << second.numGlyphs()
		<< ", avance A " << first.advance (first.glyphIndex (U'A'))
		<< "/" << second.advance (second.glyphIndex (U'A'))
		<< ", asc " << a1 << "/" << a2
		<< ", paires de crenage " << first.kernPairCount()
		<< "/" << second.kernPairCount();

	Font outOfRange;
	EXPECT_FALSE (outOfRange.loadFromFile (path, first.numFonts()));
}
