#include "text_extrude.h"

#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "extrude_contours.h"
#include "mesh.h"

#include "../cgmath/bezier_flatten.h"
#include "../cgmath/font.h"

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

} // namespace

Mesh* text_to_extruded_mesh (const Font& font, const std::string& utf8,
                             const TextExtrudeOptions& opt)
{
	if (!font.isValid())
	{
		std::fprintf (stderr, "text_extrude: police non chargee\n");
		return nullptr;
	}

	TextLayoutOptions lo;
	lo.size          = opt.size;
	lo.lineSpacing   = opt.lineSpacing;
	lo.letterSpacing = opt.letterSpacing;
	lo.align         = opt.align;
	lo.kerning       = opt.kerning;

	const TextLayout layout = layoutText (utf8, font, lo);
	if (layout.glyphs.empty())
	{
		std::fprintf (stderr, "text_extrude: texte vide\n");
		return nullptr;
	}

	Vector2f origin (0.f, 0.f);
	if (opt.centerOnOrigin)
		origin = Vector2f (-0.5f * (layout.bboxMin.x + layout.bboxMax.x),
		                   -0.5f * (layout.bboxMin.y + layout.bboxMax.y));

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

	ExtrudedMeshBuilder builder;
	std::vector<ExtrudeContour> pooled;   // seulement quand unionOverlaps

	for (const PlacedGlyph& placed : layout.glyphs)
	{
		auto it = glyphCache.find (placed.glyphIndex);
		if (it == glyphCache.end())
			it = glyphCache.emplace (
				placed.glyphIndex,
				flattenGlyph (font.glyphContours (placed.glyphIndex),
				              layout.scale, opt.flattenTol)).first;
		if (it->second.empty()) continue;    // espace, .notdef, glyphe blanc

		const Vector2f pen (placed.pen.x + origin.x, placed.pen.y + origin.y);
		std::vector<ExtrudeContour> contours = translated (it->second, pen);

		if (opt.unionOverlaps)
			pooled.insert (pooled.end(), contours.begin(), contours.end());
		else
			// UN Append par glyphe : c'est ce qui laisse la porte ouverte a un
			// materiau par glyphe (cf. ExtrudeAppendOptions::materialId), et cela
			// borne le travail de glutess a un glyphe a la fois.
			builder.Append (contours, ao);
	}

	if (opt.unionOverlaps && !pooled.empty())
		builder.Append (unionContours (pooled), ao);

	if (builder.Empty())
	{
		std::fprintf (stderr, "text_extrude: aucun contour a extruder\n");
		return nullptr;
	}

	return builder.Build();
}
