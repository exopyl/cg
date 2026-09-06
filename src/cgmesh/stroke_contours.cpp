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
	// sont resolus par la bibliotheque, ce qui rend cette voie praticable sur un
	// trace qui se touche lui-meme des milliers de fois.
	//
	// precision 6 et non le defaut 2 : les traces peuvent faire 0.2 unite de large
	// sur un canevas de 250, et deux decimales arrondiraient la moitie de cette
	// epaisseur.
	//
	// A savoir : cette precision fait sortir des points consecutifs separes d'un
	// ULP de float une fois l'echelle changee en aval. C'est sans consequence ici,
	// tessellateContours (extrude_contours.cpp) les fusionnant avant tessellation
	// -- sans quoi glutess les ecarterait et chaque doublon emporterait trois
	// parois laterales.
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
	// Deux points suffisent pour un trait -- contrairement a un contour a
	// remplir, qui en exige trois. Un point ISOLE n'a en revanche aucune
	// direction, donc aucune epaisseur : les SVG generes en contiennent
	// (`<path d="M239.9,239.8 "/>`), il faut les ecarter explicitement.
	const EndType et = (cap == StrokeCap::Butt)   ? EndType::Butt
	                 : (cap == StrokeCap::Square) ? EndType::Square
	                                              : EndType::Round;
	return inflate (polylines, width, toJoinType (join), et, /*minPoints*/ 2);
}

std::vector<std::vector<std::array<float, 2>>>
strokeClosedToContours (const std::vector<std::vector<std::array<float, 2>>>& contours,
                        float width, StrokeJoin join)
{
	// `EndType::Joined` traite le chemin comme une BOUCLE : Clipper2 le decale
	// une fois a l'endroit et une fois a l'envers (OffsetOpenJoined,
	// clipper.offset.cpp), puis unit les deux sous la regle Positive -- les
	// nombres d'enroulement s'annulent a l'interieur, ce qui laisse exactement
	// l'anneau. L'arete de fermeture est parcourue par le decalage lui-meme, il
	// n'y a donc pas de point a repeter.
	return inflate (contours, width, toJoinType (join), EndType::Joined,
	                /*minPoints*/ 3);
}
