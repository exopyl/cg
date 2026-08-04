#include "stroke_contours.h"

#include "../../extern/clipper2/clipper.h"

std::vector<std::vector<std::array<float, 2>>>
strokeToContours (const std::vector<std::vector<std::array<float, 2>>>& polylines,
                  float width, StrokeJoin join, StrokeCap cap)
{
	using namespace Clipper2Lib;

	PathsD open;
	open.reserve (polylines.size());
	for (const auto& pl : polylines)
	{
		// Deux points suffisent pour un trait -- contrairement a un contour a
		// remplir, qui en exige trois. Un point ISOLE n'a en revanche aucune
		// direction, donc aucune epaisseur : les SVG generes en contiennent
		// (`<path d="M239.9,239.8 "/>`), il faut les ecarter explicitement.
		if (pl.size() < 2) continue;
		PathD p;
		p.reserve (pl.size());
		for (const auto& q : pl)
			p.emplace_back ((double)q[0], (double)q[1]);
		open.push_back (std::move (p));
	}

	std::vector<std::vector<std::array<float, 2>>> out;
	if (open.empty() || width <= 0.f)
		return out;

	const JoinType jt = (join == StrokeJoin::Miter) ? JoinType::Miter
	                  : (join == StrokeJoin::Bevel) ? JoinType::Bevel
	                                                : JoinType::Round;
	const EndType  et = (cap == StrokeCap::Butt)   ? EndType::Butt
	                  : (cap == StrokeCap::Square) ? EndType::Square
	                                              : EndType::Round;

	// `EndType::Round`/`Square`/`Butt` decalent les DEUX cotes d'un chemin ouvert,
	// et InflatePaths termine par un Union (clipper.offset.cpp:623) : les
	// recouvrements sont donc resolus par la bibliotheque.
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
	const PathsD inflated = InflatePaths (open, 0.5 * (double)width, jt, et,
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
