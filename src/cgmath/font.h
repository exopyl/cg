#pragma once

// ============================================================================
//  Chargement d'une police a contours -> glyphes, metriques, contours
// ============================================================================
//
// Couche MINCE au-dessus de stb_truetype (extern/stb/stb_truetype.h, domaine
// public / MIT). Formats acceptes : TrueType (.ttf, table `glyf`, quadratiques),
// OpenType/CFF (.otf, charstrings Type 2, cubiques) et les collections
// .ttc/.otc, indexees. Refuses AVEC DIAGNOSTIC : WOFF/WOFF2 (tables
// compressees), Type 1, polices SVG, bitmap. Cf. loadFromMemory().
//
// Les contours sortent en UNITES DE POLICE et sous forme de COURBES, pas de
// polylignes : l'aplatissement doit avoir lieu apres la mise a l'echelle
// finale, sans quoi la finesse dependrait de l'em de la police plutot que de la
// taille demandee (cf. bezier_flatten.h).
//
// stbtt_fontinfo n'apparait PAS dans cet en-tete : extern/ est un include
// PRIVATE de cgmath, donc une fuite ici casserait la compilation de tout
// consommateur de la bibliotheque. D'ou le pimpl -- garde-fou verifie par le
// compilateur, pas simple affaire de style.
//
// Cet en-tete n'est volontairement PAS inclus par l'ombrelle cgmath.h : il
// s'inclut explicitement, comme TTransformation.h.
//
// ============================================================================

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "TVector2.h"
#include "text_layout.h"   // IGlyphMetrics

// Un segment de contour de glyphe, en UNITES DE POLICE. Le point de depart est
// le point d'arrivee du segment precedent (celui de GlyphContour::start pour le
// premier).
struct GlyphSegment
{
	enum class Kind
	{
		Line,        // droite
		Quadratic,   // quadratique, un point de controle : `c0` (TrueType)
		Cubic        // cubique, deux points de controle : `c0`, `c1` (CFF)
	};

	Kind     kind = Kind::Line;
	Vector2f c0;
	Vector2f c1;
	Vector2f to;
};

// Un contour FERME : l'arete de fermeture (dernier point -> `start`) est
// implicite, comme dans ExtrudeContour.
struct GlyphContour
{
	Vector2f                  start;
	std::vector<GlyphSegment> segments;
};

// Ce que l'on sait faire du crenage de la police chargee. Le tri-etat distingue
// « cette police n'a pas de crenage » de « elle en a, dans un format que nous ne
// lisons pas » -- sans quoi la limite serait silencieuse.
enum class KerningStatus
{
	None,        // ni table `kern` ni `GPOS` : il n'y a rien a appliquer
	Applied,     // paires lues, crenage effectif
	Unsupported  // crenage present, format non lu (kern Apple v1.0, format 2,
	             // ou crenage porte uniquement par GPOS)
};

class Font : public IGlyphMetrics
{
public:
	Font ();
	~Font () override;

	// Le buffer d'octets est POSSEDE par la classe : stb_truetype n'en garde
	// qu'un pointeur, il ne peut donc etre ni temporaire ni copie a la legere.
	// D'ou le deplacement seul, sans copie.
	Font (const Font&) = delete;
	Font& operator= (const Font&) = delete;
	Font (Font&&) noexcept;
	Font& operator= (Font&&) noexcept;

	// Chargent la police d'index `fontIndex` (0 hors collection). Renvoient
	// false ET decrivent la cause sur stderr, en NOMMANT le format detecte quand
	// il est reconnu mais hors perimetre. Un echec laisse l'objet invalide.
	bool loadFromFile (const std::string& filename, int fontIndex = 0);
	bool loadFromMemory (std::vector<unsigned char> bytes, int fontIndex = 0);

	bool isValid () const;

	// Nombre de polices dans le fichier : > 1 pour une collection .ttc/.otc.
	int  numFonts () const;
	int  numGlyphs () const;

	// Vrai quand `glyph` est un index valide pour cette police. Toutes les
	// fonctions par glyphe ci-dessous s'y ramenent et degradent en silence
	// (0 / contours vides) sur un index hors bornes.
	bool hasGlyph (int glyph) const;

	// --- IGlyphMetrics ---
	int  unitsPerEm () const override;
	void vMetrics (int& ascender, int& descender, int& lineGap) const override;
	int  glyphIndex (char32_t codepoint) const override;
	int  advance (int glyph) const override;
	int  kern (int glyph1, int glyph2) const override;

	// Approche gauche du glyphe (left side bearing).
	int  lsb (int glyph) const;

	KerningStatus kerningStatus () const;
	// Nombre de paires effectivement lues (0 hors etat Applied).
	std::size_t   kernPairCount () const;

	// Contours du glyphe, en unites de police, Y VERS LE HAUT (convention des
	// polices, deja celle du monde 3D : aucun retournement a prevoir, contrairement
	// au SVG). Vide pour un glyphe blanc ou inexistant.
	//
	// L'orientation est celle qu'AUTORISE la police -- TrueType trace ses
	// contours exterieurs dans un sens, CFF dans l'autre, et les contre-formes a
	// l'inverse du leur. La regle de remplissage non-zero les traite correctement
	// dans les deux cas, sans reorientation.
	std::vector<GlyphContour> glyphContours (int glyph) const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
