#include "font.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unordered_map>

// stb_truetype ASSERTE sur deux cas qu'il ne sait pas traiter : un glyphe
// composite positionne par APPARIEMENT DE POINTS (stb_truetype.h:1836, « @TODO
// handle matching point ») et une cmap a haut octet (:1514). Aucun des deux
// n'est une erreur de notre cote, et les deux degradent proprement -- la
// matrice du composite reste a sa valeur initiale {1,0,0,1,0,0}, la recherche
// de glyphe rend 0 (.notdef). Laisser l'assert vif ferait donc AVORTER tout le
// processus en Debug pour un seul glyphe exotique.
//
// sizeof() plutot que ((void)0) : l'expression reste « utilisee » du point de
// vue du compilateur, donc pas de C4189/-Wunused sur les variables qui ne
// servaient qu'a l'assert.
#define STBTT_assert(x) ((void)sizeof((x) ? 1 : 0))
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../extern/stb/stb_truetype.h"

namespace {

// ---------------------------------------------------------------------------
// Lecture du repertoire sfnt
// ---------------------------------------------------------------------------
// stb garde son stbtt__find_table pour lui (static). On en refait le minimum :
// il faut pouvoir lire `head` (unitsPerEm exact) et constater la PRESENCE de
// `kern` / `GPOS`, ce qu'aucune fonction publique n'expose.

unsigned int readU32 (const unsigned char* p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16)
	     | ((unsigned int)p[2] << 8)  | (unsigned int)p[3];
}

unsigned int readU16 (const unsigned char* p)
{
	return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

// Offset ABSOLU de la table, ou 0 si absente. `fontStart` est le debut de la
// police dans le fichier -- non nul dans une collection .ttc.
unsigned int findTable (const std::vector<unsigned char>& data,
                        unsigned int fontStart, const char tag[4])
{
	if (data.size() < (size_t)fontStart + 12) return 0;
	const unsigned int numTables = readU16 (&data[fontStart + 4]);
	for (unsigned int i = 0; i < numTables; i++)
	{
		const size_t loc = (size_t)fontStart + 12 + (size_t)i * 16;
		if (loc + 16 > data.size()) break;
		if (std::memcmp (&data[loc], tag, 4) == 0)
			return readU32 (&data[loc + 8]);
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Reconnaissance du format
// ---------------------------------------------------------------------------

// nullptr quand la signature est celle d'un format PRIS EN CHARGE ; sinon le
// nom du format detecte, pour que le refus soit diagnostique et non muet.
const char* unsupportedFormatName (const std::vector<unsigned char>& d)
{
	// 12 octets et non 4 : c'est la taille de l'en-tete sfnt (et de l'en-tete
	// d'une collection, que stbtt_GetNumberOfFonts lit jusqu'a l'offset 12).
	// Refuser plus court evite une lecture hors bornes sur un fichier tronque.
	if (d.size() < 12) return "fichier tronque (en-tete sfnt incomplet)";

	const unsigned char* p = d.data();
	const unsigned int tag = readU32 (p);

	// Les quatre signatures que nous savons traiter.
	if (tag == 0x00010000u)                     return nullptr;  // TrueType
	if (std::memcmp (p, "true", 4) == 0)        return nullptr;  // TrueType Apple
	if (std::memcmp (p, "OTTO", 4) == 0)        return nullptr;  // OpenType/CFF
	if (std::memcmp (p, "ttcf", 4) == 0)        return nullptr;  // collection

	if (std::memcmp (p, "wOFF", 4) == 0)
		return "WOFF (tables compressees zlib -- hors perimetre)";
	if (std::memcmp (p, "wOF2", 4) == 0)
		return "WOFF2 (Brotli + transformations glyf/loca -- hors perimetre)";
	if (std::memcmp (p, "%!PS", 4) == 0 || (p[0] == 0x80 && p[1] == 0x01))
		return "PostScript Type 1 (obsolete, abandonne par Adobe en 2023)";
	if (std::memcmp (p, "<?xm", 4) == 0 || std::memcmp (p, "<svg", 4) == 0)
		return "police SVG (depreciee par le W3C)";
	if (std::memcmp (p, "STAR", 4) == 0)     // STARTFONT
		return "BDF (bitmap : aucun contour a extruder)";

	return "signature inconnue";
}

// Cle de paire de crenage. Les index de glyphe tiennent sur 16 bits, mais
// stb_truetype les manipule en int : on les empaquete en 64 bits sans
// hypothese sur leur amplitude.
unsigned long long kernKey (int g1, int g2)
{
	return ((unsigned long long)(unsigned int)g1 << 32)
	     | (unsigned long long)(unsigned int)g2;
}

// Libere le tableau de sommets alloue par stbtt_GetGlyphShape, quel que soit le
// chemin de sortie.
struct ShapeGuard
{
	const stbtt_fontinfo* info;
	stbtt_vertex*         verts;

	~ShapeGuard () { if (verts) stbtt_FreeShape (info, verts); }
};

} // namespace

// ===========================================================================
//  Font::Impl
// ===========================================================================

struct Font::Impl
{
	std::vector<unsigned char> data;      // POSSEDE : stb n'en garde qu'un pointeur
	stbtt_fontinfo             info{};
	bool                       valid = false;
	int                        fontCount = 0;
	unsigned int               fontStart = 0;
	int                        upem = 0;

	KerningStatus                              kernStatus = KerningStatus::None;
	std::unordered_map<unsigned long long, int> kernPairs;
};

Font::Font ()
	: m_impl (new Impl)
{
}

Font::~Font () = default;
Font::Font (Font&&) noexcept = default;
Font& Font::operator= (Font&&) noexcept = default;

bool Font::loadFromFile (const std::string& filename, int fontIndex)
{
	std::ifstream f (filename, std::ios::binary);
	if (!f)
	{
		std::fprintf (stderr, "font: impossible d'ouvrir %s\n", filename.c_str());
		return false;
	}

	std::vector<unsigned char> bytes ((std::istreambuf_iterator<char>(f)),
	                                   std::istreambuf_iterator<char>());
	if (bytes.empty())
	{
		std::fprintf (stderr, "font: %s est vide\n", filename.c_str());
		return false;
	}
	return loadFromMemory (std::move (bytes), fontIndex);
}

bool Font::loadFromMemory (std::vector<unsigned char> bytes, int fontIndex)
{
	*m_impl = Impl();

	if (const char* rejected = unsupportedFormatName (bytes))
	{
		std::fprintf (stderr, "font: format non pris en charge -- %s\n", rejected);
		return false;
	}

	m_impl->data = std::move (bytes);

	m_impl->fontCount = stbtt_GetNumberOfFonts (m_impl->data.data());
	if (m_impl->fontCount <= 0) m_impl->fontCount = 1;
	if (fontIndex < 0 || fontIndex >= m_impl->fontCount)
	{
		std::fprintf (stderr, "font: index %d hors bornes (%d police(s))\n",
		              fontIndex, m_impl->fontCount);
		m_impl->data.clear();
		return false;
	}

	const int offset = stbtt_GetFontOffsetForIndex (m_impl->data.data(), fontIndex);
	if (offset < 0)
	{
		std::fprintf (stderr, "font: police d'index %d introuvable\n", fontIndex);
		m_impl->data.clear();
		return false;
	}
	m_impl->fontStart = (unsigned int)offset;

	if (!stbtt_InitFont (&m_impl->info, m_impl->data.data(), offset))
	{
		std::fprintf (stderr, "font: tables sfnt illisibles (glyf/loca ou CFF manquants)\n");
		m_impl->data.clear();
		return false;
	}

	// unitsPerEm : lu DIRECTEMENT dans `head` (offset 18), et non deduit de
	// stbtt_ScaleForMappingEmToPixels -- l'aller-retour par un float rend 1000
	// a un ulp pres, la lecture directe rend l'entier exact.
	const unsigned int head = findTable (m_impl->data, m_impl->fontStart, "head");
	if (head != 0 && (size_t)head + 20 <= m_impl->data.size())
		m_impl->upem = (int)readU16 (&m_impl->data[head + 18]);
	if (m_impl->upem <= 0)
	{
		std::fprintf (stderr, "font: unitsPerEm absent ou nul (table head)\n");
		m_impl->data.clear();
		return false;
	}

	// --- Crenage -------------------------------------------------------------
	// stbtt_GetGlyphKernAdvance N'EST PAS UTILISE : sur les polices de bureau
	// courantes il rend 0. Son dispatch fait `if (GPOS) ... else if (kern)`, et
	// son lecteur GPOS ignore les lookups de type 9 (Extension Positioning) dans
	// lesquels ces polices emballent leur PairPos -- la branche `kern`, qui
	// contient pourtant la donnee, n'est alors jamais atteinte.
	//
	// stbtt_GetKerningTableLength, lui, lit la table `kern` en direct. On la
	// vide une fois pour toutes dans une table de hachage : plus juste ET plus
	// rapide (O(1) par paire, contre une recherche par appel).
	const int nPairs = stbtt_GetKerningTableLength (&m_impl->info);
	if (nPairs > 0)
	{
		std::vector<stbtt_kerningentry> table ((size_t)nPairs);
		const int got = stbtt_GetKerningTable (&m_impl->info, table.data(), nPairs);
		m_impl->kernPairs.reserve ((size_t)(got > 0 ? got : 0));
		for (int i = 0; i < got; i++)
			m_impl->kernPairs[kernKey (table[i].glyph1, table[i].glyph2)] = table[i].advance;
	}

	if (!m_impl->kernPairs.empty())
	{
		m_impl->kernStatus = KerningStatus::Applied;
	}
	else
	{
		// Distinguer « pas de crenage » de « crenage illisible » : c'est toute la
		// valeur du tri-etat. Les formats hors d'atteinte sont la table `kern`
		// Apple v1.0, les sous-tables de format 2 (crenage par classes) et le
		// crenage porte uniquement par GPOS.
		const bool hasKern = findTable (m_impl->data, m_impl->fontStart, "kern") != 0;
		const bool hasGpos = findTable (m_impl->data, m_impl->fontStart, "GPOS") != 0;
		m_impl->kernStatus = (hasKern || hasGpos) ? KerningStatus::Unsupported
		                                          : KerningStatus::None;
	}

	m_impl->valid = true;
	return true;
}

bool Font::isValid () const           { return m_impl->valid; }
int  Font::numFonts () const          { return m_impl->valid ? m_impl->fontCount : 0; }
int  Font::numGlyphs () const         { return m_impl->valid ? m_impl->info.numGlyphs : 0; }
int  Font::unitsPerEm () const        { return m_impl->valid ? m_impl->upem : 0; }
KerningStatus Font::kerningStatus () const { return m_impl->kernStatus; }
std::size_t   Font::kernPairCount () const { return m_impl->kernPairs.size(); }

void Font::vMetrics (int& ascender, int& descender, int& lineGap) const
{
	ascender = descender = lineGap = 0;
	if (!m_impl->valid) return;
	stbtt_GetFontVMetrics (&m_impl->info, &ascender, &descender, &lineGap);
}

int Font::glyphIndex (char32_t codepoint) const
{
	if (!m_impl->valid) return 0;
	// stb prend un int : au-dela du plan 0x10FFFF il n'y a rien a chercher.
	if (codepoint > 0x10FFFFu) return 0;
	return stbtt_FindGlyphIndex (&m_impl->info, (int)codepoint);
}

// Un index de glyphe hors bornes ferait lire `hmtx` / `loca` a cote : stb ne
// verifie rien, c'est a l'appelant de le faire. Un tel index n'a aucune source
// legitime (glyphIndex() rend 0 pour un point de code absent), mais l'API est
// publique et prend un int nu.
bool Font::hasGlyph (int glyph) const
{
	return m_impl->valid && glyph >= 0 && glyph < m_impl->info.numGlyphs;
}

int Font::advance (int glyph) const
{
	if (!hasGlyph (glyph)) return 0;
	int adv = 0, bearing = 0;
	stbtt_GetGlyphHMetrics (&m_impl->info, glyph, &adv, &bearing);
	return adv;
}

int Font::lsb (int glyph) const
{
	if (!hasGlyph (glyph)) return 0;
	int adv = 0, bearing = 0;
	stbtt_GetGlyphHMetrics (&m_impl->info, glyph, &adv, &bearing);
	return bearing;
}

int Font::kern (int glyph1, int glyph2) const
{
	if (m_impl->kernPairs.empty()) return 0;
	const auto it = m_impl->kernPairs.find (kernKey (glyph1, glyph2));
	return (it != m_impl->kernPairs.end()) ? it->second : 0;
}

std::vector<GlyphContour> Font::glyphContours (int glyph) const
{
	std::vector<GlyphContour> contours;
	if (!hasGlyph (glyph)) return contours;

	stbtt_vertex* verts = nullptr;
	const int nVerts = stbtt_GetGlyphShape (&m_impl->info, glyph, &verts);
	const ShapeGuard guard { &m_impl->info, verts };
	if (nVerts <= 0 || !verts) return contours;

	GlyphContour current;
	bool open = false;

	// Un contour est clos par le vmove SUIVANT (ou par la fin du flux) : stb
	// n'emet aucun marqueur de fermeture, l'arete de retour au depart etant
	// implicite -- exactement la convention d'ExtrudeContour.
	for (int i = 0; i < nVerts; i++)
	{
		const stbtt_vertex& v = verts[i];
		const Vector2f to  ((float)v.x,   (float)v.y);
		const Vector2f c0  ((float)v.cx,  (float)v.cy);
		const Vector2f c1  ((float)v.cx1, (float)v.cy1);

		switch (v.type)
		{
		case STBTT_vmove:
			if (open && !current.segments.empty())
				contours.push_back (std::move (current));
			current = GlyphContour();
			current.start = to;
			open = true;
			break;

		case STBTT_vline:
			if (open) current.segments.push_back ({ GlyphSegment::Kind::Line, c0, c1, to });
			break;

		case STBTT_vcurve:
			if (open) current.segments.push_back ({ GlyphSegment::Kind::Quadratic, c0, c1, to });
			break;

		case STBTT_vcubic:
			if (open) current.segments.push_back ({ GlyphSegment::Kind::Cubic, c0, c1, to });
			break;

		default:
			break;
		}
	}
	if (open && !current.segments.empty())
		contours.push_back (std::move (current));

	return contours;
}
