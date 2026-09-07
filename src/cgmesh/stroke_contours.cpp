#include "stroke_contours.h"

#include "../../extern/clipper2/clipper.h"

namespace {

using namespace Clipper2Lib;

JoinType toJoinType (StrokeJoin join)
{
	return (join == StrokeJoin::Miter) ? JoinType::Miter
	     : (join == StrokeJoin::Bevel) ? JoinType::Bevel
	                                   : JoinType::Round;
}

// Corps commun aux deux familles de trait. `minPoints` ecarte les traces trop
// courts pour avoir une epaisseur : deux points suffisent pour un ruban, il en
// faut trois pour une boucle.
std::vector<std::vector<std::array<float, 2>>>
inflate (const std::vector<std::vector<std::array<float, 2>>>& polylines,
         float width, JoinType jt, EndType et, size_t minPoints)
{
	PathsD paths;
	paths.reserve (polylines.size());
	for (const auto& pl : polylines)
	{
		if (pl.size() < minPoints) continue;
		PathD p;
		p.reserve (pl.size());
		for (const auto& q : pl)
			p.emplace_back ((double)q[0], (double)q[1]);
		paths.push_back (std::move (p));
	}

	std::vector<std::vector<std::array<float, 2>>> out;
	if (paths.empty() || width <= 0.f)
		return out;

	// InflatePaths termine par un Union (clipper.offset.cpp) : les recouvrements
	// sont resolus par la bibliotheque.
	//
	// precision 6 et non le defaut 2 : les traces peuvent faire 0.2 unite de large
	// sur un canevas de 250, et deux decimales arrondiraient la moitie de cette
	// epaisseur.
	//
	// Cette precision fait sortir des points consecutifs separes d'un ULP de float
	// une fois l'echelle changee en aval ; tessellateContours
	// (extrude_contours.cpp) les fusionne avant tessellation, sans quoi glutess
	// les ecarterait et chaque doublon emporterait trois parois laterales.
	const PathsD inflated = InflatePaths (paths, 0.5 * (double)width, jt, et,
	                                      /*miter_limit*/ 2.0, /*precision*/ 6);

	out.reserve (inflated.size());
	for (const PathD& p : inflated)
	{
		if (p.size() < 3) continue;
		std::vector<std::array<float, 2>> c;
		c.reserve (p.size());
		for (const PointD& q : p)
			c.push_back ({ (float)q.x, (float)q.y });
		out.push_back (std::move (c));
	}
	return out;
}

} // namespace

std::vector<std::vector<std::array<float, 2>>>
strokeToContours (const std::vector<std::vector<std::array<float, 2>>>& polylines,
                  float width, StrokeJoin join, StrokeCap cap)
{
	// `EndType::Round`/`Square`/`Butt` decalent les DEUX cotes d'un chemin ouvert
	// et lui posent une extremite.
	//
	// Deux points suffisent pour un trait, contrairement a un contour a remplir
	// qui en exige trois. Un point ISOLE n'a aucune direction, donc aucune
	// epaisseur : un SVG peut en contenir (`<path d="M239.9,239.8 "/>`), d'ou le
	// seuil explicite.
	const EndType et = (cap == StrokeCap::Butt)   ? EndType::Butt
	                 : (cap == StrokeCap::Square) ? EndType::Square
	                                              : EndType::Round;
	return inflate (polylines, width, toJoinType (join), et, /*minPoints*/ 2);
}

std::vector<std::vector<std::array<float, 2>>>
strokeClosedToContours (const std::vector<std::vector<std::array<float, 2>>>& contours,
                        float width, StrokeJoin join)
{
	// `EndType::Joined` traite le chemin comme une BOUCLE : Clipper2 le decale a
	// l'endroit puis a l'envers (OffsetOpenJoined, clipper.offset.cpp) et unit
	// les deux sous la regle Positive, ce qui laisse exactement l'anneau. Le
	// decalage parcourt l'arete de fermeture : aucun point a repeter.
	return inflate (contours, width, toJoinType (join), EndType::Joined,
	                /*minPoints*/ 3);
}
