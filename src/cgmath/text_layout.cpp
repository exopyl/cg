#include "text_layout.h"

#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
// Decodage UTF-8
// ---------------------------------------------------------------------------
// Detail d'implementation, et non de la geometrie : il reste PRIVE a ce fichier.
// ~40 lignes contre une dependance -- le calcul est vite fait.
//
// Politique de tolerance : tout octet ou toute sequence invalide donne U+FFFD et
// la lecture reprend au caractere suivant. Un texte a moitie lisible vaut mieux
// qu'un echec muet, et un rendu 3D n'a aucune raison d'etre plus severe qu'un
// navigateur.

const char32_t kReplacement = 0xFFFDu;

std::vector<char32_t> decodeUtf8 (const std::string& s)
{
	std::vector<char32_t> out;
	out.reserve (s.size());

	size_t i = 0;
	while (i < s.size())
	{
		const unsigned char b0 = (unsigned char)s[i];

		int extra = 0;
		char32_t cp = 0;
		if      (b0 < 0x80)                  { cp = b0;        extra = 0; }
		else if ((b0 & 0xE0) == 0xC0)        { cp = b0 & 0x1F; extra = 1; }
		else if ((b0 & 0xF0) == 0xE0)        { cp = b0 & 0x0F; extra = 2; }
		else if ((b0 & 0xF8) == 0xF0)        { cp = b0 & 0x07; extra = 3; }
		else { out.push_back (kReplacement); i++; continue; }   // octet de tete invalide

		if (i + (size_t)extra >= s.size())   // sequence tronquee en fin de chaine
		{
			out.push_back (kReplacement);
			break;
		}

		bool ok = true;
		for (int k = 1; k <= extra; k++)
		{
			const unsigned char bk = (unsigned char)s[i + (size_t)k];
			if ((bk & 0xC0) != 0x80) { ok = false; break; }
			cp = (cp << 6) | (char32_t)(bk & 0x3F);
		}

		// Surrogates et depassement du plan 16 : invalides en UTF-8.
		if (!ok || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu))
		{
			out.push_back (kReplacement);
			i++;                       // on ne saute PAS la sequence : elle est fausse
			continue;
		}

		out.push_back (cp);
		i += (size_t)extra + 1;
	}
	return out;
}

// Une ligne en cours de composition, dans les unites de sortie.
struct Line
{
	size_t first = 0;   // index du premier glyphe dans TextLayout::glyphs
	size_t count = 0;
	float  width = 0.f;
};

} // namespace

TextLayout layoutText (const std::string& utf8, const IGlyphMetrics& metrics,
                       const TextLayoutOptions& opt)
{
	TextLayout layout;

	const int upem = metrics.unitsPerEm();
	// Une police sans em exploitable ne peut rien mettre a l'echelle : mieux vaut
	// une page vide qu'une division par zero silencieuse.
	if (upem <= 0) return layout;

	layout.scale = opt.size / (float)upem;

	int ascender = 0, descender = 0, lineGap = 0;
	metrics.vMetrics (ascender, descender, lineGap);
	// descender est negatif par convention : l'interligne naturel est donc
	// ascender - descender + lineGap.
	layout.lineHeight = (float)(ascender - descender + lineGap) * layout.scale
	                  * opt.lineSpacing;

	const std::vector<char32_t> codepoints = decodeUtf8 (utf8);

	std::vector<Line> lines;
	Line current;
	current.first = 0;

	float pen = 0.f;
	int   previousGlyph = -1;
	int   lineIndex = 0;

	const auto closeLine = [&] () {
		current.width = pen;
		lines.push_back (current);
		current = Line();
		current.first = layout.glyphs.size();
		pen = 0.f;
		previousGlyph = -1;
		lineIndex++;
	};

	for (const char32_t cp : codepoints)
	{
		if (cp == U'\r') continue;          // CRLF : le \n qui suit fait le travail
		if (cp == U'\n') { closeLine(); continue; }

		const int glyph = metrics.glyphIndex (cp);

		if (previousGlyph >= 0)
		{
			// L'interlettrage est applique AVANT chaque glyphe sauf le premier :
			// une ligne ne traine ainsi aucun espace en fin, et sa largeur reste
			// exacte pour l'alignement.
			pen += opt.letterSpacing;
			if (opt.kerning)
				pen += (float)metrics.kern (previousGlyph, glyph) * layout.scale;
		}

		PlacedGlyph placed;
		placed.glyphIndex = glyph;
		placed.pen        = Vector2f (pen, -(float)lineIndex * layout.lineHeight);
		placed.line       = lineIndex;
		layout.glyphs.push_back (placed);
		current.count++;

		pen += (float)metrics.advance (glyph) * layout.scale;
		previousGlyph = glyph;
	}
	closeLine();   // ferme la derniere ligne, meme vide

	layout.lineCount = (int)lines.size();

	// --- Alignement -----------------------------------------------------------
	// Chaque ligne est composee a partir de x = 0 puis DECALEE : c'est plus
	// simple que de deviner sa largeur a l'avance, et cela reste exact quel que
	// soit le crenage.
	float minX = 0.f, maxX = 0.f;
	bool  anyLine = false;

	for (const Line& line : lines)
	{
		float shift = 0.f;
		if      (opt.align == TextAlign::Center) shift = -0.5f * line.width;
		else if (opt.align == TextAlign::Right)  shift = -line.width;

		if (shift != 0.f)
			for (size_t k = line.first; k < line.first + line.count; k++)
				layout.glyphs[k].pen.x += shift;

		const float left  = shift;
		const float right = shift + line.width;
		if (!anyLine) { minX = left; maxX = right; anyLine = true; }
		else { minX = std::min (minX, left); maxX = std::max (maxX, right); }
	}

	// --- Emprise --------------------------------------------------------------
	// Verticalement, on encadre par les metriques de la police et non par les
	// contours : la ligne de base de la premiere ligne etant en y = 0, le haut
	// est l'ascendante et le bas la descendante de la DERNIERE ligne.
	const float top    = (float)ascender * layout.scale;
	const float bottom = -(float)(layout.lineCount - 1) * layout.lineHeight
	                   + (float)descender * layout.scale;

	layout.bboxMin = Vector2f (minX, bottom);
	layout.bboxMax = Vector2f (maxX, top);

	return layout;
}
