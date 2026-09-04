#include "text_extrude.h"

#include <cgmath/context.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "contour_ops.h"      // roundedRectContour, contourSignedArea, contoursBBox
#include "extrude_contours.h"
#include "mesh.h"

#include <cgmath/bezier_flatten.h>
#include <cgmath/font.h>

#include "../../extern/clipper2/clipper.h"

namespace {

// Aplatit UN contour de glyphe, en le mettant au passage a l'echelle. La mise a
// l'echelle est faite AVANT l'aplatissement, sur les points de controle : la
// tolerance s'exprime ainsi en unites monde.
//
// La PLUME n'intervient pas ici. Une translation ne change rien au nombre de
// subdivisions, donc aplatir un glyphe une fois puis translater le resultat
// donne exactement les memes points -- et « MISSISSIPPI » ne subdivise alors
// que quatre glyphes au lieu de onze.
std::vector<Vector2f> flattenContour (const GlyphContour& contour,
                                      float scale, float tol)
{
	const auto place = [&] (const Vector2f& p) {
		return Vector2f (p.x * scale, p.y * scale);
	};

	std::vector<Vector2f> pts;
	pts.reserve (contour.segments.size() * 4 + 1);

	Vector2f from = place (contour.start);
	pts.push_back (from);          // le depart, pousse UNE fois

	for (const GlyphSegment& s : contour.segments)
	{
		const Vector2f to = place (s.to);
		switch (s.kind)
		{
		case GlyphSegment::Kind::Line:
			pts.push_back (to);
			break;
		case GlyphSegment::Kind::Quadratic:
			flattenQuadratic (pts, from, place (s.c0), to, tol);
			break;
		case GlyphSegment::Kind::Cubic:
			flattenCubic (pts, from, place (s.c0), place (s.c1), to, tol);
			break;
		}
		from = to;
	}

	// stb_truetype referme chaque contour par un segment explicite vers son
	// point de depart (stbtt__close_shape) : le dernier point est donc un
	// DOUBLON du premier. L'arete de fermeture etant implicite dans
	// ExtrudeContour, le garder creerait une arete de longueur nulle -- que
	// glutess ecarterait, emportant trois parois avec elle (cf.
	// tu_cgmesh_extrude_contours.cpp).
	//
	// Le seuil est exprime en unites de police ramenees au monde : les deux
	// points viennent des memes coordonnees entieres transformees a l'identique,
	// donc l'egalite est en pratique exacte -- un milliieme d'unite de police
	// suffit largement, et reste sous la resolution de la police quelle que soit
	// la taille demandee.
	if (pts.size() >= 2)
	{
		const float eps = 1e-3f * scale;
		if (std::fabs (pts.front().x - pts.back().x) < eps
		 && std::fabs (pts.front().y - pts.back().y) < eps)
			pts.pop_back();
	}
	return pts;
}

// Tous les contours d'un glyphe, mis a l'echelle et aplatis, origine sur la
// plume. Vide pour un glyphe blanc.
std::vector<ExtrudeContour> flattenGlyph (const std::vector<GlyphContour>& outline,
                                          float scale, float tol)
{
	std::vector<ExtrudeContour> contours;
	contours.reserve (outline.size());
	for (const GlyphContour& c : outline)
	{
		ExtrudeContour ec;
		ec.pts = flattenContour (c, scale, tol);
		// Moins de trois points ne delimite aucune surface.
		if (ec.pts.size() >= 3) contours.push_back (std::move (ec));
	}
	return contours;
}

// Le meme jeu de contours, porte a la plume.
std::vector<ExtrudeContour> translated (const std::vector<ExtrudeContour>& contours,
                                        const Vector2f& pen)
{
	std::vector<ExtrudeContour> out = contours;
	for (ExtrudeContour& c : out)
		for (Vector2f& p : c.pts)
		{
			p.x += pen.x;
			p.y += pen.y;
		}
	return out;
}

// Fusionne les contours en une seule region. NonZero est la regle des polices
// comme celle de Clipper2 ici : les contre-formes, tracees en sens inverse de
// leur enveloppe, restent donc soustraites.
std::vector<ExtrudeContour> unionContours (const std::vector<ExtrudeContour>& in)
{
	using namespace Clipper2Lib;

	PathsD subjects;
	subjects.reserve (in.size());
	for (const ExtrudeContour& c : in)
	{
		PathD p;
		p.reserve (c.pts.size());
		for (const Vector2f& q : c.pts)
			p.emplace_back ((double)q.x, (double)q.y);
		subjects.push_back (std::move (p));
	}

	// precision 6 et non le defaut 2 : un corps de 1.0 unite porte des details
	// de l'ordre du centieme, que deux decimales arrondiraient a plat.
	const PathsD merged = Union (subjects, FillRule::NonZero, 6);

	std::vector<ExtrudeContour> out;
	out.reserve (merged.size());
	for (const PathD& p : merged)
	{
		if (p.size() < 3) continue;
		ExtrudeContour c;
		c.pts.reserve (p.size());
		for (const PointD& q : p)
			c.pts.emplace_back ((float)q.x, (float)q.y);
		out.push_back (std::move (c));
	}
	return out;
}


// Contours du support, dans le repere MONDE deja translate par la plume.
//
// ORIENTATION -- le point qui ne se devine pas, et qui decide entre un socle et
// un pochoir. L'union est en NonZero et les contours de glyphes gardent
// l'orientation qu'a choisie la police : TrueType et CFF ne tracent pas leurs
// contours exterieurs dans le meme sens (cf. ExtrudeAppendOptions::
// normalizeOrientation, laisse a false ici pour cette raison). Un support trace
// dans le sens INVERSE de celui des lettres les soustrairait de la plaque au
// lieu de s'y ajouter -- le meme code rendrait un socle avec une police et un
// pochoir avec une autre. On aligne donc le support sur le sens du plus grand
// contour de glyphe present.
std::vector<ExtrudeContour> supportContours (const TextExtrudeOptions& opt,
                                             const std::vector<ExtrudeContour>& glyphs,
                                             float baselineY)
{
	std::vector<ExtrudeContour> out;
	if (opt.support == TextExtrudeOptions::Support::None || glyphs.empty()) return out;

	float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
	bool first = true;
	float widestArea = 0.f;
	float solidSign = 1.f;
	for (const ExtrudeContour& c : glyphs)
	{
		const float a = contourSignedArea (c.pts);
		if (std::fabs (a) > std::fabs (widestArea)) { widestArea = a; solidSign = (a >= 0.f) ? 1.f : -1.f; }
		for (const Vector2f& p : c.pts)
		{
			if (first) { x0 = x1 = p.x; y0 = y1 = p.y; first = false; continue; }
			x0 = std::min (x0, p.x); x1 = std::max (x1, p.x);
			y0 = std::min (y0, p.y); y1 = std::max (y1, p.y);
		}
	}
	if (first) return out;

	const float m = (opt.supportMargin > 0.f) ? opt.supportMargin : 0.f;
	const float t = (opt.supportThickness > 0.f) ? opt.supportThickness : 0.f;
	const float r = (opt.supportCornerRadius > 0.f) ? opt.supportCornerRadius : 0.f;

	// Un contour est ajoute a la matiere s'il tourne comme les lettres, et
	// creuse un vide s'il tourne a l'envers.
	const auto push = [&](std::vector<Vector2f> pts, bool solid) {
		const float want = solid ? solidSign : -solidSign;
		if (contourSignedArea (pts) * want < 0.f)
			std::reverse (pts.begin(), pts.end());
		ExtrudeContour c;
		c.pts = std::move (pts);
		if (c.pts.size() >= 3) out.push_back (std::move (c));
	};

	switch (opt.support)
	{
	case TextExtrudeOptions::Support::Plate:
		push (roundedRectContour (x0 - m, y0 - m, x1 + m, y1 + m, r), true);
		break;
	case TextExtrudeOptions::Support::Bar:
	{
		// Bandeau horizontal dont le HAUT mord de supportOverlap dans les lettres :
		// c'est ce recouvrement, et lui seul, qui fait de l'union une piece
		// unique. Sa marge s'applique en largeur.
		//
		// ⚠ LA REFERENCE EST LA LIGNE DE BASE, et non le bas de l'emprise. Ce
		// n'est pas une preference d'aspect, c'est ce qui fait tenir la piece :
		// l'emprise descend jusqu'au JAMBAGE le plus bas, si bien qu'un bandeau
		// pose la ne mordait QUE dans la lettre qui descend le plus -- un « p »,
		// un « g » -- et laissait toutes les autres flotter au-dessus de lui,
		// desolidarisees. Sur la ligne de base, il mord dans chaque lettre qui
		// s'y pose, c'est-a-dire dans presque toutes.
		//
		// Contrepartie assumee : les jambages TRAVERSENT le bandeau et passent
		// dessous. C'est ce qui les relie au reste, et c'est ainsi que le fait
		// n'importe quelle plaque de nom.
		const float top = baselineY + ((opt.supportOverlap > 0.f) ? opt.supportOverlap : 0.f);
		const float bottom = top - ((t > 0.f) ? t : 0.f);
		// Garde DEFENSIVE, et mesuree comme telle : la retirer ne change aucun
		// resultat, l'union ecartant d'elle-meme un contour d'aire nulle. Elle
		// reste parce qu'il vaut mieux ne pas soumettre a un moteur booleen une
		// figure dont on sait deja qu'elle ne delimite rien.
		if (top > bottom)
			push (roundedRectContour (x0 - m, bottom, x1 + m, top, 0.f), true);
		break;
	}
	case TextExtrudeOptions::Support::Frame:
	{
		// Meme garde defensive que le bandeau : a epaisseur nulle les deux
		// anneaux coincident et s'annulent sous NonZero.
		if (t > 0.f)
		{
			push (roundedRectContour (x0 - m - t, y0 - m - t, x1 + m + t, y1 + m + t, r), true);
			push (roundedRectContour (x0 - m, y0 - m, x1 + m, y1 + m, (r > t) ? (r - t) : 0.f), false);
		}
		break;
	}
	case TextExtrudeOptions::Support::None:
		break;
	}
	return out;
}
// ---------------------------------------------------------------------------
//  Cote demandee -> corps em, et tolerance automatique
// ---------------------------------------------------------------------------
//
// Les deux conversions que les appelants publics partagent, et qu'ils faisaient
// deux fois chacun. Elles vivent ici et pas dans text_layout : celui-la ne
// connait que IGlyphMetrics, sans hauteur de capitale (cf. font.h).

// Corps em equivalent a la cote demandee. En mode CapHeight, une cote de 30 mm
// veut dire « capitales de 30 mm », donc un em de 30 x upem / capHeight.
float emSizeFor (const Font& font, const TextExtrudeOptions& opt)
{
	if (opt.sizeMode != TextExtrudeOptions::SizeMode::CapHeight) return opt.size;

	const int cap = font.capHeight();
	const int upem = font.unitsPerEm();
	// Police sans capitale lisible, ou em inexploitable : on ne peut pas honorer
	// la cote. Retomber sur l'em est la seule autre reference -- et le DIRE, car
	// la piece sortira alors a une hauteur qui n'est pas celle demandee.
	if (cap <= 0 || upem <= 0)
	{
		std::fprintf (stderr, "text_extrude: hauteur de capitale introuvable, "
		                      "la cote est interpretee comme un corps em\n");
		return opt.size;
	}
	return opt.size * (float)upem / (float)cap;
}

// Tolerance d'aplatissement effective : `flattenTol` tel quel, ou size / 600
// quand il est nul ou negatif (cf. text_extrude.h).
float flattenTolFor (const TextExtrudeOptions& opt)
{
	if (opt.flattenTol > 0.f) return opt.flattenTol;
	// Une cote nulle ne produira rien de toute facon ; la garde evite juste une
	// tolerance nulle qui subdiviserait sans fin.
	return opt.size > 0.f ? opt.size / 600.f : 0.01f;
}

} // namespace

bool text_to_contours (const Font& font, const std::string& utf8,
                       const TextExtrudeOptions& opt,
                       std::vector<ExtrudeContour>& out,
                       TextExtrudeStats* stats,
                       const Context* ctx)
{
	out.clear();

	if (!font.isValid())
	{
		std::fprintf (stderr, "text_extrude: police non chargee\n");
		return false;
	}

	TextLayoutOptions lo;
	// La cote demandee peut mesurer la hauteur de CAPITALE ; le compositeur ne
	// sait mettre a l'echelle que par l'em (cf. emSizeFor).
	lo.size          = emSizeFor (font, opt);
	lo.lineSpacing   = opt.lineSpacing;
	lo.letterSpacing = opt.letterSpacing;
	lo.align         = opt.align;
	lo.kerning       = opt.kerning;

	// Tolerance effective, calculee UNE fois : elle sert a chaque glyphe distinct.
	const float tol = flattenTolFor (opt);

	const TextLayout layout = layoutText (utf8, font, lo);
	if (layout.glyphs.empty())
	{
		std::fprintf (stderr, "text_extrude: texte vide\n");
		return false;
	}

	// Ligne de base la plus BASSE, dans le repere final : c'est la reference du
	// bandeau (cf. supportContours). Sur un texte d'une seule ligne elle vaut
	// `origin.y` ; sur plusieurs, celle du bas -- un bandeau unique ne peut
	// relier que la derniere.
	float lowestPen = 0.f;
	for (const PlacedGlyph& placed : layout.glyphs)
		lowestPen = std::min (lowestPen, placed.pen.y);

	Vector2f origin (0.f, 0.f);
	if (opt.centerOnOrigin)
		origin = Vector2f (-0.5f * (layout.bboxMin.x + layout.bboxMax.x),
		                   -0.5f * (layout.bboxMin.y + layout.bboxMax.y));
	const float baselineY = origin.y + lowestPen;

	// Un texte repete ses lettres : « MISSISSIPPI » n'a que quatre glyphes
	// distincts sur onze. On ne les lit -- et surtout on ne les subdivise --
	// qu'une fois, puisque la plume ne fait ensuite que translater le resultat.
	std::unordered_map<int, std::vector<ExtrudeContour>> glyphCache;

	if (stats)
		stats->glyphsPlaced = layout.glyphs.size();

	std::vector<ExtrudeContour> pooled;
	for (const PlacedGlyph& placed : layout.glyphs)
	{
		// Un glyphe est l'unite de travail, et l'aplatissement d'un seul peut
		// couter cher. Rien n'est rendu d'un texte a moitie pose -- une forme
		// tronquee ressemblerait a un resultat.
		if (ctx && ctx->IsAborted())
			return false;

		auto it = glyphCache.find (placed.glyphIndex);
		if (it == glyphCache.end())
		{
			if (stats)
				stats->glyphsFlattened++;
			it = glyphCache.emplace (
				placed.glyphIndex,
				flattenGlyph (font.glyphContours (placed.glyphIndex),
				              layout.scale, tol)).first;
		}
		if (it->second.empty()) continue;    // espace, .notdef, glyphe blanc

		const Vector2f pen (placed.pen.x + origin.x, placed.pen.y + origin.y);
		const std::vector<ExtrudeContour> contours = translated (it->second, pen);
		pooled.insert (pooled.end(), contours.begin(), contours.end());
	}

	if (pooled.empty())
	{
		std::fprintf (stderr, "text_extrude: aucun contour a extruder\n");
		return false;
	}

	// LE contour de plus, ajoute AVANT l'union : il traverse le meme Union
	// (subjects, NonZero, 6) que les glyphes, et en ressort fondu avec eux.
	// Aucune coque a recoller, aucun booleen 3D.
	if (opt.support != TextExtrudeOptions::Support::None)
	{
		const std::vector<ExtrudeContour> support = supportContours (opt, pooled, baselineY);
		pooled.insert (pooled.end(), support.begin(), support.end());
	}

	// UNION TOUJOURS, et c'est un ecart assume avec text_to_extruded_mesh, qui
	// ne la faisait que sur demande (unionOverlaps) ou quand un support
	// l'imposait.
	//
	// Le motif est le TYPE de sortie : un port de forme 2D porte une liste plate
	// de contours, pas le decoupage par glyphe que l'ancien chemin exploitait
	// pour appeler l'extrudeur une fois par lettre. Rendre les glyphes non
	// fusionnes sur ce port, puis les extruder d'un seul tenant, donnerait des
	// murs interieurs la ou deux lettres se touchent. Une region unique est ce
	// qu'un consommateur de contours attend, et c'est aussi ce qui rend le
	// solide etanche.
	//
	// Le prix est une passe Clipper2 meme sur un texte sans chevauchement.
	out = unionContours (pooled);
	return !out.empty();
}

Mesh* text_to_extruded_mesh (const Font& font, const std::string& utf8,
                             const TextExtrudeOptions& opt,
                             TextExtrudeStats* stats,
                             const Context* ctx)
{

	if (!font.isValid())
	{
		std::fprintf (stderr, "text_extrude: police non chargee\n");
		return nullptr;
	}

	TextLayoutOptions lo;
	// La cote demandee peut mesurer la hauteur de CAPITALE ; le compositeur ne
	// sait mettre a l'echelle que par l'em (cf. emSizeFor).
	lo.size          = emSizeFor (font, opt);
	lo.lineSpacing   = opt.lineSpacing;
	lo.letterSpacing = opt.letterSpacing;
	lo.align         = opt.align;
	lo.kerning       = opt.kerning;

	// Tolerance effective, calculee UNE fois : elle sert a chaque glyphe distinct.
	const float tol = flattenTolFor (opt);

	const TextLayout layout = layoutText (utf8, font, lo);
	if (layout.glyphs.empty())
	{
		std::fprintf (stderr, "text_extrude: texte vide\n");
		return nullptr;
	}

	// Ligne de base la plus BASSE, dans le repere final : c'est la reference du
	// bandeau (cf. supportContours). Sur un texte d'une seule ligne elle vaut
	// `origin.y` ; sur plusieurs, celle du bas -- un bandeau unique ne peut
	// relier que la derniere.
	float lowestPen = 0.f;
	for (const PlacedGlyph& placed : layout.glyphs)
		lowestPen = std::min (lowestPen, placed.pen.y);

	Vector2f origin (0.f, 0.f);
	if (opt.centerOnOrigin)
		origin = Vector2f (-0.5f * (layout.bboxMin.x + layout.bboxMax.x),
		                   -0.5f * (layout.bboxMin.y + layout.bboxMax.y));
	const float baselineY = origin.y + lowestPen;

	ExtrudeAppendOptions ao;
	ao.zBottom    = 0.f;
	ao.zTop       = opt.depth;
	ao.materialId = opt.materialId;
	ao.winding    = ExtrudeWinding::NonZero;
	// L'orientation est celle qu'a AUTORISEE la police -- TrueType et CFF ne
	// tracent pas leurs contours exterieurs dans le meme sens, et leurs
	// contre-formes a l'inverse du leur dans les deux cas. NonZero le traite
	// correctement ; reorienter d'apres l'aire signee reboucherait les
	// contre-formes, exactement comme pour le SVG.
	ao.normalizeOrientation = false;

	// Un texte repete ses lettres : « MISSISSIPPI » n'a que quatre glyphes
	// distincts sur onze. On ne les lit -- et surtout on ne les subdivise --
	// qu'une fois, puisque la plume ne fait ensuite que translater le resultat.
	// stbtt_GetGlyphShape alloue a chaque appel, l'economie est donc double.
	std::unordered_map<int, std::vector<ExtrudeContour>> glyphCache;

	if (stats)
		stats->glyphsPlaced = layout.glyphs.size();

	ExtrudedMeshBuilder builder;
	// Un support impose l'union : c'est elle qui fond la plaque et les lettres en
	// une region unique, donc en un solide etanche.
	const bool pool = opt.unionOverlaps || opt.support != TextExtrudeOptions::Support::None;
	std::vector<ExtrudeContour> pooled;   // seulement quand pool

	for (const PlacedGlyph& placed : layout.glyphs)
	{
		// Boucle externe : un glyphe est l'unite de travail, et l'aplatissement
		// d'un seul peut couter cher. Rien n'est rendu d'un texte a moitie pose
		// -- un solide tronque ressemblerait a un resultat.
		if (ctx && ctx->IsAborted ())
			return nullptr;

		auto it = glyphCache.find (placed.glyphIndex);
		if (it == glyphCache.end())
		{
			if (stats)
				stats->glyphsFlattened++;
			it = glyphCache.emplace (
				placed.glyphIndex,
				flattenGlyph (font.glyphContours (placed.glyphIndex),
				              layout.scale, tol)).first;
		}
		if (it->second.empty()) continue;    // espace, .notdef, glyphe blanc

		const Vector2f pen (placed.pen.x + origin.x, placed.pen.y + origin.y);
		std::vector<ExtrudeContour> contours = translated (it->second, pen);

		if (pool)
			pooled.insert (pooled.end(), contours.begin(), contours.end());
		else
			// UN Append par glyphe : c'est ce qui laisse la porte ouverte a un
			// materiau par glyphe (cf. ExtrudeAppendOptions::materialId), et cela
			// borne le travail de glutess a un glyphe a la fois.
			builder.Append (contours, ao);
	}

	if (pool && !pooled.empty())
	{
		// LE contour de plus, ajoute AVANT l'union : il traverse le meme
		// Union (subjects, NonZero, 6) que les glyphes, et en ressort fondu avec
		// eux. Aucune coque a recoller, aucun booleen 3D.
		const std::vector<ExtrudeContour> support = supportContours (opt, pooled, baselineY);
		pooled.insert (pooled.end(), support.begin(), support.end());
		builder.Append (unionContours (pooled), ao);
	}

	if (builder.Empty())
	{
		std::fprintf (stderr, "text_extrude: aucun contour a extruder\n");
		return nullptr;
	}

	return builder.Build();
}
