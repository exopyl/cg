#include <gtest/gtest.h>

#include "../src/cgmath/text_layout.h"

#include <cmath>
#include <string>
#include <vector>

// ===========================================================================
//  Mise en page : UTF-8 -> glyphes places
// ===========================================================================
// Aucun fichier de police n'est ouvert ici, et c'est le but : layoutText ne
// depend que de IGlyphMetrics. Un bouchon aux valeurs CHOISIES rend les oracles
// exacts au lieu d'approximatifs -- avec une vraie police, « la largeur vaut a
// peu pres la somme des avances » serait tout ce que l'on pourrait affirmer.

namespace {

// Metriques jouets : em de 1000, une avance de 500 pour tout glyphe sauf 'i'
// (200), et une seule paire creneee, A/V a -100.
class StubMetrics : public IGlyphMetrics
{
public:
	int  unitsPerEm () const override { return 1000; }

	void vMetrics (int& asc, int& desc, int& gap) const override
	{
		asc = 800; desc = -200; gap = 0;     // interligne naturel = 1000, soit 1 em
	}

	// Le point de code EST l'index de glyphe : les oracles restent lisibles.
	// Seul '?' est declare absent, pour exercer le .notdef.
	int glyphIndex (char32_t cp) const override
	{
		return (cp == U'?') ? 0 : (int)cp;
	}

	int advance (int glyph) const override
	{
		if (glyph == 0) return 0;                 // .notdef : aucune avance
		return (glyph == (int)U'i') ? 200 : 500;
	}

	int kern (int g1, int g2) const override
	{
		return (g1 == (int)U'A' && g2 == (int)U'V') ? -100 : 0;
	}
};

float width (const TextLayout& l) { return l.bboxMax.x - l.bboxMin.x; }
float height (const TextLayout& l) { return l.bboxMax.y - l.bboxMin.y; }

}  // namespace

// ---------------------------------------------------------------------------
// Avance et echelle
// ---------------------------------------------------------------------------

TEST(TEST_cgmath_text_layout, pen_advances_by_the_scaled_advance)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	opt.size = 1.f;                       // 1000 unites de police -> 1.0

	const TextLayout l = layoutText ("AAA", m, opt);

	ASSERT_EQ (l.glyphs.size(), 3u);
	EXPECT_FLOAT_EQ (l.scale, 0.001f);
	EXPECT_FLOAT_EQ (l.glyphs[0].pen.x, 0.0f);
	EXPECT_FLOAT_EQ (l.glyphs[1].pen.x, 0.5f);
	EXPECT_FLOAT_EQ (l.glyphs[2].pen.x, 1.0f);
	EXPECT_FLOAT_EQ (width (l), 1.5f);
}

// L'oracle du document de faisabilite : la largeur de l'emprise est bien
// l'avance cumulee. C'est ce qui garantit qu'un texte deux fois plus long
// occupe deux fois plus de place.
TEST(TEST_cgmath_text_layout, bbox_width_is_the_accumulated_advance)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	opt.size = 2.f;                       // 1000 unites -> 2.0

	const TextLayout l = layoutText ("Aii", m, opt);
	// (500 + 200 + 200) / 1000 * 2
	EXPECT_FLOAT_EQ (width (l), 1.8f);
}

TEST(TEST_cgmath_text_layout, size_scales_everything_linearly)
{
	const StubMetrics m;
	TextLayoutOptions small, big;
	small.size = 1.f;
	big.size   = 3.f;

	const TextLayout a = layoutText ("Hello", m, small);
	const TextLayout b = layoutText ("Hello", m, big);

	ASSERT_EQ (a.glyphs.size(), b.glyphs.size());
	EXPECT_FLOAT_EQ (width (b), 3.f * width (a));
	for (size_t i = 0; i < a.glyphs.size(); i++)
		EXPECT_FLOAT_EQ (b.glyphs[i].pen.x, 3.f * a.glyphs[i].pen.x);
}

// ---------------------------------------------------------------------------
// Crenage
// ---------------------------------------------------------------------------

// L'oracle canonique : « AV » doit etre plus etroit avec crenage que sans. Un
// crenage lu mais jamais applique passerait inapercu sans ce test.
TEST(TEST_cgmath_text_layout, kerning_tightens_the_AV_pair)
{
	const StubMetrics m;
	TextLayoutOptions on, off;
	on.size = off.size = 1.f;
	on.kerning  = true;
	off.kerning = false;

	const TextLayout kerned   = layoutText ("AV", m, on);
	const TextLayout unkerned = layoutText ("AV", m, off);

	EXPECT_LT (width (kerned), width (unkerned));
	EXPECT_FLOAT_EQ (width (unkerned) - width (kerned), 0.1f);   // 100 / 1000
	EXPECT_FLOAT_EQ (kerned.glyphs[1].pen.x, 0.4f);              // 500 - 100
}

// Le crenage ne doit s'appliquer qu'aux paires qui en portent.
TEST(TEST_cgmath_text_layout, an_unkerned_pair_is_untouched)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	const TextLayout l = layoutText ("VA", m, opt);   // paire inverse : non creneee
	EXPECT_FLOAT_EQ (l.glyphs[1].pen.x, 0.5f);
}

// Le crenage ne franchit pas une fin de ligne : la plume repart de zero.
TEST(TEST_cgmath_text_layout, kerning_does_not_cross_a_line_break)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	const TextLayout l = layoutText ("A\nV", m, opt);
	ASSERT_EQ (l.glyphs.size(), 2u);
	EXPECT_FLOAT_EQ (l.glyphs[1].pen.x, 0.f);
}

// ---------------------------------------------------------------------------
// Multiligne
// ---------------------------------------------------------------------------

// L'oracle du document : deux lignes occupent deux fois la hauteur d'une, a
// l'interligne pres. Ici asc-desc = 1000 et l'interligne naturel vaut 1 em, donc
// une ligne fait 1.0 de haut et deux lignes 2.0.
TEST(TEST_cgmath_text_layout, two_lines_are_twice_as_tall_as_one)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	opt.size = 1.f;

	const TextLayout one = layoutText ("AA", m, opt);
	const TextLayout two = layoutText ("AA\nAA", m, opt);

	EXPECT_EQ (one.lineCount, 1);
	EXPECT_EQ (two.lineCount, 2);
	EXPECT_FLOAT_EQ (height (one), 1.0f);
	EXPECT_FLOAT_EQ (height (two), 2.0f);
	EXPECT_FLOAT_EQ (two.lineHeight, 1.0f);
}

TEST(TEST_cgmath_text_layout, later_lines_sit_below_the_first)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	const TextLayout l = layoutText ("A\nB\nC", m, opt);

	ASSERT_EQ (l.glyphs.size(), 3u);
	EXPECT_FLOAT_EQ (l.glyphs[0].pen.y,  0.0f);
	EXPECT_FLOAT_EQ (l.glyphs[1].pen.y, -1.0f);
	EXPECT_FLOAT_EQ (l.glyphs[2].pen.y, -2.0f);
	EXPECT_EQ (l.glyphs[2].line, 2);
}

TEST(TEST_cgmath_text_layout, line_spacing_stretches_the_block)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	opt.lineSpacing = 1.5f;

	const TextLayout l = layoutText ("A\nB", m, opt);
	EXPECT_FLOAT_EQ (l.lineHeight, 1.5f);
	EXPECT_FLOAT_EQ (l.glyphs[1].pen.y, -1.5f);
	EXPECT_FLOAT_EQ (height (l), 2.5f);      // 1.5 d'interligne + 1.0 d'em
}

// Les fins de ligne Windows ne doivent pas produire de glyphe fantome.
TEST(TEST_cgmath_text_layout, carriage_returns_are_ignored)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	const TextLayout crlf = layoutText ("AB\r\nCD", m, opt);
	const TextLayout lf   = layoutText ("AB\nCD",   m, opt);

	ASSERT_EQ (crlf.glyphs.size(), lf.glyphs.size());
	EXPECT_EQ (crlf.lineCount, lf.lineCount);
	for (size_t i = 0; i < lf.glyphs.size(); i++)
		EXPECT_FLOAT_EQ (crlf.glyphs[i].pen.x, lf.glyphs[i].pen.x);
}

// ---------------------------------------------------------------------------
// Alignement
// ---------------------------------------------------------------------------

TEST(TEST_cgmath_text_layout, alignment_shifts_each_line_independently)
{
	const StubMetrics m;

	// Deux lignes de largeurs DIFFERENTES : c'est ce qui rend l'alignement
	// observable. "AA" fait 1.0, "i" fait 0.2.
	TextLayoutOptions left, center, right;
	left.align   = TextAlign::Left;
	center.align = TextAlign::Center;
	right.align  = TextAlign::Right;

	const TextLayout l = layoutText ("AA\ni", m, left);
	const TextLayout c = layoutText ("AA\ni", m, center);
	const TextLayout r = layoutText ("AA\ni", m, right);

	ASSERT_EQ (l.glyphs.size(), 3u);

	// A gauche, les deux lignes commencent en 0.
	EXPECT_FLOAT_EQ (l.glyphs[0].pen.x, 0.f);
	EXPECT_FLOAT_EQ (l.glyphs[2].pen.x, 0.f);

	// Centre : chaque ligne recule de la moitie de SA largeur.
	EXPECT_FLOAT_EQ (c.glyphs[0].pen.x, -0.5f);
	EXPECT_FLOAT_EQ (c.glyphs[2].pen.x, -0.1f);

	// A droite, chaque ligne se termine en 0.
	EXPECT_FLOAT_EQ (r.glyphs[0].pen.x, -1.0f);
	EXPECT_FLOAT_EQ (r.glyphs[2].pen.x, -0.2f);

	// L'emprise couvre la plus large des lignes, quel que soit l'alignement.
	EXPECT_FLOAT_EQ (width (l), 1.0f);
	EXPECT_FLOAT_EQ (width (c), 1.0f);
	EXPECT_FLOAT_EQ (width (r), 1.0f);
}

// ---------------------------------------------------------------------------
// Interlettrage
// ---------------------------------------------------------------------------

// Applique ENTRE les glyphes, donc n - 1 fois : une ligne ne doit pas trainer
// d'espace en fin, sans quoi l'alignement a droite serait faux.
TEST(TEST_cgmath_text_layout, letter_spacing_applies_between_glyphs_only)
{
	const StubMetrics m;
	TextLayoutOptions opt;
	opt.letterSpacing = 0.25f;

	const TextLayout l = layoutText ("AAA", m, opt);
	ASSERT_EQ (l.glyphs.size(), 3u);
	EXPECT_FLOAT_EQ (l.glyphs[1].pen.x, 0.75f);   // 0.5 + 0.25
	EXPECT_FLOAT_EQ (l.glyphs[2].pen.x, 1.50f);
	EXPECT_FLOAT_EQ (width (l), 2.0f);            // 3 avances + 2 espacements

	const TextLayout single = layoutText ("A", m, opt);
	EXPECT_FLOAT_EQ (width (single), 0.5f) << "un glyphe seul ne traine pas d'espacement";
}

// ---------------------------------------------------------------------------
// UTF-8 et cas degrades
// ---------------------------------------------------------------------------

TEST(TEST_cgmath_text_layout, multibyte_sequences_decode_to_one_glyph_each)
{
	const StubMetrics m;
	TextLayoutOptions opt;

	// « e accent aigu » (2 octets), « fleche » (3 octets), « emoji » (4 octets).
	const TextLayout l = layoutText ("\xC3\xA9\xE2\x86\x92\xF0\x9F\x98\x80", m, opt);
	EXPECT_EQ (l.glyphs.size(), 3u) << "un glyphe par POINT DE CODE, pas par octet";
	EXPECT_EQ (l.glyphs[0].glyphIndex, 0x00E9);
	EXPECT_EQ (l.glyphs[1].glyphIndex, 0x2192);
	EXPECT_EQ (l.glyphs[2].glyphIndex, 0x1F600);
}

// Un octet invalide doit produire U+FFFD et laisser la lecture se poursuivre :
// une chaine mal encodee ne doit ni tout perdre, ni faire boucler le decodeur.
TEST(TEST_cgmath_text_layout, invalid_utf8_degrades_to_a_replacement_glyph)
{
	const StubMetrics m;
	TextLayoutOptions opt;

	const TextLayout l = layoutText ("A\xFF" "B", m, opt);
	ASSERT_EQ (l.glyphs.size(), 3u);
	EXPECT_EQ (l.glyphs[0].glyphIndex, (int)U'A');
	EXPECT_EQ (l.glyphs[1].glyphIndex, 0xFFFD);
	EXPECT_EQ (l.glyphs[2].glyphIndex, (int)U'B');
}

TEST(TEST_cgmath_text_layout, a_truncated_sequence_does_not_loop)
{
	const StubMetrics m;
	TextLayoutOptions opt;

	// Tete de sequence a 3 octets, mais la chaine s'arrete.
	const TextLayout l = layoutText ("A\xE2\x86", m, opt);
	ASSERT_EQ (l.glyphs.size(), 2u);
	EXPECT_EQ (l.glyphs[1].glyphIndex, 0xFFFD);
}

TEST(TEST_cgmath_text_layout, an_empty_string_lays_out_to_nothing)
{
	const StubMetrics m;
	TextLayoutOptions opt;

	const TextLayout l = layoutText ("", m, opt);
	EXPECT_TRUE (l.glyphs.empty());
	EXPECT_EQ (l.lineCount, 1);
	EXPECT_FLOAT_EQ (width (l), 0.f);
}

// Un point de code absent de la police devient .notdef. Il reste PLACE -- c'est
// volontaire : le texte ne doit pas se decaler silencieusement.
TEST(TEST_cgmath_text_layout, a_missing_codepoint_is_placed_as_notdef)
{
	const StubMetrics m;
	TextLayoutOptions opt;

	const TextLayout l = layoutText ("A?A", m, opt);
	ASSERT_EQ (l.glyphs.size(), 3u);
	EXPECT_EQ (l.glyphs[1].glyphIndex, 0);
}

// Une police sans em exploitable ne peut rien mettre a l'echelle : mieux vaut
// une page vide qu'une division par zero.
TEST(TEST_cgmath_text_layout, a_font_without_an_em_lays_out_nothing)
{
	class BrokenMetrics : public IGlyphMetrics
	{
	public:
		int  unitsPerEm () const override { return 0; }
		void vMetrics (int& a, int& d, int& g) const override { a = d = g = 0; }
		int  glyphIndex (char32_t) const override { return 1; }
		int  advance (int) const override { return 500; }
		int  kern (int, int) const override { return 0; }
	};

	const BrokenMetrics m;
	TextLayoutOptions opt;
	const TextLayout l = layoutText ("AAA", m, opt);
	EXPECT_TRUE (l.glyphs.empty());
}
