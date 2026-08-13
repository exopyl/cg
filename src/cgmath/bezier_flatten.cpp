#include "bezier_flatten.h"

#include <algorithm>

namespace {

// Garde-fou de recursion : la subdivision s'arrete APRES ce niveau. Valeur
// reprise de import_svg.cpp pour que la bascule ne change rien au maillage
// produit. Elle borne le cout d'une courbe pathologique sans jamais etre
// atteinte sur une courbe reelle -- un contour de glyphe ou un trace SVG
// converge en trois a six niveaux.
const int kMaxDepth = 12;

Vector2f midpoint (const Vector2f& a, const Vector2f& b)
{
	return Vector2f (0.5f * (a.x + b.x), 0.5f * (a.y + b.y));
}

} // namespace

// La quantite mesuree, 2*c - p0 - p1, s'annule exactement quand le point de
// controle est au milieu de la corde, c'est-a-dire quand le segment EST cette
// corde. Elle vaut quatre fois l'ecart maximal de la courbe a sa corde
// (atteint en t = 0.5), d'ou la comparaison directe a `tol`.
bool flatEnoughQuadratic (const Vector2f& p0, const Vector2f& c,
                          const Vector2f& p1, float tol)
{
	const float ux = 2.f * c.x - p0.x - p1.x;
	const float uy = 2.f * c.y - p0.y - p1.y;
	return (ux * ux + uy * uy) <= tol * tol;
}

// Mesure de platitude classique d'une cubique : ecart des DEUX points de
// controle a la position qu'ils occuperaient si le segment etait droit. Formule
// reprise telle quelle de import_svg.cpp, dont elle vient -- le comportement de
// l'import SVG doit rester bit a bit identique apres bascule sur ce module.
bool flatEnoughCubic (const Vector2f& p0, const Vector2f& c0,
                      const Vector2f& c1, const Vector2f& p1, float tol)
{
	const float ux = 3.f * c0.x - 2.f * p0.x - p1.x;
	const float uy = 3.f * c0.y - 2.f * p0.y - p1.y;
	const float vx = 3.f * c1.x - 2.f * p1.x - p0.x;
	const float vy = 3.f * c1.y - 2.f * p1.y - p0.y;
	const float du = ux * ux + uy * uy;
	const float dv = vx * vx + vy * vy;
	return std::max (du, dv) <= tol * tol;
}

void flattenQuadratic (std::vector<Vector2f>& out,
                       const Vector2f& p0, const Vector2f& c,
                       const Vector2f& p1, float tol, int depth)
{
	if (depth > kMaxDepth || flatEnoughQuadratic (p0, c, p1, tol))
	{
		out.push_back (p1);
		return;
	}

	// De Casteljau a t = 0.5
	const Vector2f p0c = midpoint (p0, c);
	const Vector2f cp1 = midpoint (c, p1);
	const Vector2f mid = midpoint (p0c, cp1);

	flattenQuadratic (out, p0, p0c, mid, tol, depth + 1);
	flattenQuadratic (out, mid, cp1, p1, tol, depth + 1);
}

void flattenCubic (std::vector<Vector2f>& out,
                   const Vector2f& p0, const Vector2f& c0,
                   const Vector2f& c1, const Vector2f& p1, float tol,
                   int depth)
{
	if (depth > kMaxDepth || flatEnoughCubic (p0, c0, c1, p1, tol))
	{
		out.push_back (p1);
		return;
	}

	// De Casteljau a t = 0.5
	const Vector2f a  = midpoint (p0, c0);
	const Vector2f b  = midpoint (c0, c1);
	const Vector2f cc = midpoint (c1, p1);
	const Vector2f ab = midpoint (a, b);
	const Vector2f bc = midpoint (b, cc);
	const Vector2f mid = midpoint (ab, bc);

	flattenCubic (out, p0, a, ab, mid, tol, depth + 1);
	flattenCubic (out, mid, bc, cc, p1, tol, depth + 1);
}
