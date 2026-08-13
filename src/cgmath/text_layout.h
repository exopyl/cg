#pragma once

// ============================================================================
//  Mise en page d'un texte latin : UTF-8 -> glyphes places
// ============================================================================
//
// Arithmetique de plume, pure et sans geometrie : ce module ne sait ni lire une
// police, ni dessiner un contour. Il ne connait que IGlyphMetrics, une interface
// de METRIQUES. Deux consequences voulues :
//
//   - il se teste avec un bouchon, sans le moindre fichier de police ;
//   - un changement de bibliotheque de parsing (font.h) ne le touche pas.
//
// Perimetre : ecritures LATINES, multiligne, crenage au mieux. Pas de
// facconnage contextuel -- l'arabe, l'hebreu et les ecritures indiennes sont
// hors d'atteinte, et pas seulement degrades : ils exigent HarfBuzz.
//
// ============================================================================

#include <string>
#include <vector>

#include "TVector2.h"

// Les metriques dont la mise en page a besoin, toutes en UNITES DE POLICE
// (l'entier brut du fichier). La conversion vers les unites de sortie est faite
// ici, une fois, via unitsPerEm().
class IGlyphMetrics
{
public:
	virtual ~IGlyphMetrics () = default;

	// Cote de l'em, en unites de police : 2048 en TrueType, 1000 en CFF.
	virtual int  unitsPerEm () const = 0;

	// Ascendante (> 0), descendante (< 0) et gouttiere entre lignes.
	virtual void vMetrics (int& ascender, int& descender, int& lineGap) const = 0;

	// Index de glyphe d'un point de code, 0 (.notdef) si absent.
	virtual int  glyphIndex (char32_t codepoint) const = 0;

	// Avance horizontale du glyphe.
	virtual int  advance (int glyph) const = 0;

	// Correction de crenage a appliquer ENTRE deux glyphes, generalement
	// negative. 0 quand la paire n'est pas creneee -- ou quand la police porte
	// son crenage dans un format que l'on ne sait pas lire (cf.
	// Font::kerningStatus, font.h) : la mise en page ne fait pas la difference,
	// c'est a l'interface utilisateur de la signaler.
	virtual int  kern (int glyph1, int glyph2) const = 0;
};

enum class TextAlign { Left, Center, Right };

struct TextLayoutOptions
{
	// Hauteur d'em en unites de SORTIE : c'est le facteur d'echelle demande, et
	// non une hauteur de capitale. Deux polices de meme `size` n'ont donc pas
	// forcement des majuscules de meme hauteur -- c'est la convention
	// typographique, celle du « corps ».
	float size = 1.f;

	// Multiplicateur de l'interligne NATUREL (ascender - descender + lineGap).
	float lineSpacing = 1.f;

	// Ajout entre deux glyphes consecutifs, en unites de sortie. Applique AVANT
	// chaque glyphe sauf le premier de la ligne, de sorte qu'une ligne ne traine
	// jamais d'espace en fin et que l'alignement reste exact.
	float letterSpacing = 0.f;

	TextAlign align = TextAlign::Left;

	// Coupe le crenage meme quand la police en porte.
	bool kerning = true;
};

// Un glyphe et son origine (le point de reference sur la ligne de base), en
// unites de SORTIE.
struct PlacedGlyph
{
	int      glyphIndex = 0;
	Vector2f pen;
	int      line = 0;
};

struct TextLayout
{
	std::vector<PlacedGlyph> glyphs;

	// Emprise D'AVANCE, en unites de sortie : hauteur d'ascendante a
	// descendante, largeur cumulee des avances. Ce n'est PAS l'emprise des
	// contours -- la mise en page n'en voit aucun. Un `f` deborde a droite de
	// son avance, un `j` a gauche de la sienne ; c'est normal et voulu, le cadre
	// typographique etant plus stable que l'encombrement reel.
	Vector2f bboxMin;
	Vector2f bboxMax;

	int   lineCount = 0;

	// Unites de police -> unites de sortie (size / unitsPerEm). Expose pour que
	// l'extrusion mette les CONTOURS a la meme echelle que les plumes.
	float scale = 1.f;

	// Interligne effectif, en unites de sortie.
	float lineHeight = 0.f;
};

// La ligne de base de la PREMIERE ligne est en y = 0 ; les suivantes
// descendent. L'origine horizontale depend de l'alignement : x = 0 est le bord
// gauche en Left, le centre en Center, le bord droit en Right.
//
// Les octets UTF-8 invalides sont remplaces par U+FFFD plutot que rejetes : un
// texte a moitie lisible vaut mieux qu'un echec muet. '\n' ouvre une ligne,
// '\r' est ignore (fins de ligne Windows).
TextLayout layoutText (const std::string& utf8, const IGlyphMetrics& metrics,
                       const TextLayoutOptions& opt);
