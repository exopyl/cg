#include "contour_ops.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "../../extern/clipper2/clipper.h"

namespace
{

// Clipper2 travaille en double ; les contours du depot sont en float. La
// conversion est faite ici et nulle part ailleurs.
Clipper2Lib::PathsD toPaths (const std::vector<ExtrudeContour>& in)
{
	Clipper2Lib::PathsD paths;
	paths.reserve (in.size());
	for (const ExtrudeContour& c : in)
	{
		if (c.pts.size() < 3) continue;   // ne delimite rien
		Clipper2Lib::PathD p;
		p.reserve (c.pts.size());
		for (const Vector2f& v : c.pts)
			p.push_back (Clipper2Lib::PointD ((double)v.x, (double)v.y));
		paths.push_back (std::move (p));
	}
	return paths;
}

std::vector<ExtrudeContour> toContours (const Clipper2Lib::PathsD& paths)
{
	std::vector<ExtrudeContour> out;
	out.reserve (paths.size());
	for (const Clipper2Lib::PathD& p : paths)
	{
		if (p.size() < 3) continue;
		ExtrudeContour c;
		c.pts.reserve (p.size());
		for (const Clipper2Lib::PointD& q : p)
			c.pts.push_back (Vector2f ((float)q.x, (float)q.y));
		out.push_back (std::move (c));
	}
	return out;
}

} // namespace

float contourSignedArea (const std::vector<Vector2f>& pts)
{
	float sum = 0.f;
	const size_t n = pts.size();
	for (size_t i = 0; i < n; ++i)
	{
		const Vector2f& a = pts[i];
		const Vector2f& b = pts[(i + 1) % n];
		sum += a.x * b.y - b.x * a.y;
	}
	return 0.5f * sum;
}

bool contoursBBox (const std::vector<ExtrudeContour>& contours,
                   float& x0, float& y0, float& x1, float& y1)
{
	bool first = true;
	for (const ExtrudeContour& c : contours)
		for (const Vector2f& p : c.pts)
		{
			if (first) { x0 = x1 = p.x; y0 = y1 = p.y; first = false; continue; }
			x0 = std::min (x0, p.x); x1 = std::max (x1, p.x);
			y0 = std::min (y0, p.y); y1 = std::max (y1, p.y);
		}
	return !first;
}

std::vector<Vector2f> roundedRectContour (float x0, float y0, float x1, float y1,
                                          float radius, int cornerSegments)
{
	std::vector<Vector2f> pts;
	const float maxR = 0.5f * std::min (x1 - x0, y1 - y0);
	if (radius > maxR) radius = maxR;
	if (radius <= 0.f || cornerSegments < 1)
	{
		pts.push_back (Vector2f (x0, y0));
		pts.push_back (Vector2f (x1, y0));
		pts.push_back (Vector2f (x1, y1));
		pts.push_back (Vector2f (x0, y1));
		return pts;
	}
	const float PI = 3.14159265358979323846f;
	// Quatre quarts de cercle, centres rentres du rayon, parcourus dans le sens
	// trigonometrique en partant du coin bas-droit.
	const float cx[4] = { x1 - radius, x1 - radius, x0 + radius, x0 + radius };
	const float cy[4] = { y0 + radius, y1 - radius, y1 - radius, y0 + radius };
	for (int c = 0; c < 4; ++c)
	{
		const float a0 = -0.5f * PI + 0.5f * PI * (float)c;
		for (int k = 0; k <= cornerSegments; ++k)
		{
			const float a = a0 + 0.5f * PI * (float)k / (float)cornerSegments;
			pts.push_back (Vector2f (cx[c] + radius * std::cos (a),
			                         cy[c] + radius * std::sin (a)));
		}
	}
	return pts;
}

std::vector<Vector2f> circleContour (float cx, float cy, float radius, int segments)
{
	std::vector<Vector2f> pts;
	if (radius <= 0.f) return pts;
	// Huit au minimum : en dessous, un « cercle » est un polygone qu'on lit comme
	// tel, et un trou de vis octogonal ne se visse pas mieux qu'un rond.
	if (segments < 8) segments = 8;

	const float PI = 3.14159265358979323846f;
	pts.reserve ((std::size_t)segments);
	for (int k = 0; k < segments; ++k)
	{
		const float a = 2.f * PI * (float)k / (float)segments;
		pts.push_back (Vector2f (cx + radius * std::cos (a), cy + radius * std::sin (a)));
	}
	return pts;
}

std::vector<ExtrudeContour> offsetContours (const std::vector<ExtrudeContour>& in,
                                            float delta, StrokeJoin join,
                                            float miterLimit, int* topLevelCount)
{
	if (topLevelCount) *topLevelCount = 0;

	const Clipper2Lib::PathsD subject = toPaths (in);
	if (subject.empty()) return {};

	const Clipper2Lib::JoinType jt =
		  (join == StrokeJoin::Miter) ? Clipper2Lib::JoinType::Miter
		: (join == StrokeJoin::Bevel) ? Clipper2Lib::JoinType::Bevel
		                              : Clipper2Lib::JoinType::Round;

	// EndType::Polygon : les contours sont FERMES, et le decalage porte sur la
	// region qu'ils delimitent. Les EndType de `stroke_contours` (Round, Square,
	// Butt) decalent les deux cotes d'un chemin OUVERT -- ils rendraient ici un
	// ruban le long du bord au lieu d'une region.
	//
	// `precision` = 6 chiffres : la meme que l'union des glyphes
	// (text_extrude.cpp). A l'echelle du millimetre, cela laisse le nanometre.
	Clipper2Lib::PathsD inflated;
	if (std::fabs ((double)delta) < 1e-12)
	{
		// Un decalage nul n'est pas un cas degenere : c'est l'identite, et
		// InflatePaths le rendrait quand meme via son union terminale. On garde
		// l'union -- elle NORMALISE l'orientation, ce que le reste du contrat
		// promet --, mais on evite l'offseteur.
		inflated = Clipper2Lib::Union (subject, Clipper2Lib::FillRule::NonZero, 6);
	}
	else
	{
		inflated = Clipper2Lib::InflatePaths (subject, (double)delta, jt,
		                                     Clipper2Lib::EndType::Polygon,
		                                     (double)miterLimit, 6);
	}
	if (inflated.empty()) return {};

	// Le COMPTE DES MORCEAUX demande un PolyTree : la sortie plate ne dit pas
	// quel contour contient quel autre, et compter les aires positives
	// confondrait un ilot dans un trou avec un morceau distinct.
	if (topLevelCount)
	{
		Clipper2Lib::ClipperD clipper (6);
		clipper.AddSubject (inflated);
		Clipper2Lib::PolyTreeD tree;
		if (clipper.Execute (Clipper2Lib::ClipType::Union,
		                     Clipper2Lib::FillRule::NonZero, tree))
			*topLevelCount = (int)tree.Count();
	}

	return toContours (inflated);
}

std::vector<ExtrudeContour> unionContours (const std::vector<ExtrudeContour>& a,
                                           const std::vector<ExtrudeContour>& b)
{
	Clipper2Lib::PathsD subject = toPaths (a);
	const Clipper2Lib::PathsD other = toPaths (b);
	subject.insert (subject.end(), other.begin(), other.end());
	if (subject.empty()) return {};

	// Une seule passe suffit : la reunion de deux jeux, c'est l'union du jeu
	// concatene. precision 6, comme partout ailleurs ici -- a l'echelle du
	// millimetre, cela laisse le nanometre.
	return toContours (Clipper2Lib::Union (subject, Clipper2Lib::FillRule::NonZero, 6));
}

std::vector<ExtrudeContour> differenceContours (const std::vector<ExtrudeContour>& a,
                                                const std::vector<ExtrudeContour>& b)
{
	const Clipper2Lib::PathsD subject = toPaths (a);
	if (subject.empty()) return {};

	const Clipper2Lib::PathsD clip = toPaths (b);
	// Rien a retirer : la region est rendue telle quelle, en passant tout de meme
	// par une union -- c'est elle qui NORMALISE l'orientation, ce que le contrat
	// promet a l'appelant.
	if (clip.empty())
		return toContours (Clipper2Lib::Union (subject, Clipper2Lib::FillRule::NonZero, 6));

	return toContours (Clipper2Lib::Difference (subject, clip,
	                                            Clipper2Lib::FillRule::NonZero, 6));
}

std::vector<ExtrudeContour> intersectionContours (const std::vector<ExtrudeContour>& a,
                                                  const std::vector<ExtrudeContour>& b)
{
	const Clipper2Lib::PathsD subject = toPaths (a);
	const Clipper2Lib::PathsD clip = toPaths (b);
	// Intersecter avec RIEN rend rien -- et non « tout », ce qu'un raccourci
	// symetrique a celui de la difference aurait fait dire.
	if (subject.empty() || clip.empty()) return {};

	return toContours (Clipper2Lib::Intersect (subject, clip,
	                                           Clipper2Lib::FillRule::NonZero, 6));
}
