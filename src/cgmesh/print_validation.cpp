#include "print_validation.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

#include "contour_ops.h"

namespace
{

// Pas d'echantillonnage du bord pour la profondeur, en fraction de r : sous le
// plafond, la profondeur est sous-estimee d'au plus la moitie du pas.
constexpr double kDepthSampleRatio = 0.05;

// Tolerance d'arrondi delta, en fraction de r, avec un plancher en ulp float
// de la demi-emprise : la region et la partie conservee passent par Clipper2 en
// float. Elle borne l'epaisseur des rubans d'arrondi ecartes ; sa moitie est la
// tolerance de contact.
constexpr double kRoundingTolRatio = 1e-3;
constexpr double kRoundingTolUlps  = 16.0;

// Marge du rectangle des vides, en multiples de r_trou : la bande exterieure
// est deux fois plus epaisse que le diametre d'ouverture.
constexpr double kVoidBoxMarginRatio = 4.0;

// Plafond de profondeur, en multiple du plus grand seuil qui la lit (bruit,
// longueur de pointe) : au-dela, le classement ne change plus.
constexpr double kDepthCapFactor = 2.0;

// ---------------------------------------------------------------------------
//  Domaine
// ---------------------------------------------------------------------------

// Flux des messages : locale classique (point decimal, sans separateur de
// milliers), quelle que soit la locale globale.
std::ostringstream classicStream ()
{
	std::ostringstream os;
	os.imbue (std::locale::classic ());
	return os;
}

// Message d'une entree hors domaine : le nom et la VALEUR en cause, puis la
// regle, chiffres compris.
[[noreturn]] void outOfDomain (const char *what, double v, const std::string &rule)
{
	std::ostringstream os = classicStream ();
	os << "CheckPrintRegion: " << what << ' ' << v << ' ' << rule;
	throw std::invalid_argument (os.str());
}

void requireFinitePositive (double v, const char *what)
{
	if (!std::isfinite (v) || v <= 0.0)
		outOfDomain (what, v, "must be finite and > 0");
}

void requirePrintLength (double v, const char *what)
{
	if (!std::isfinite (v) || v < kMinPrintLengthMm || v > kMaxPrintLengthMm)
	{
		std::ostringstream rule = classicStream ();
		rule << "must be within [" << kMinPrintLengthMm << ", " << kMaxPrintLengthMm << "] mm";
		outOfDomain (what, v, rule.str());
	}
}

void validateConstraints (const PrintConstraints &c)
{
	if (c.mmPerUnit) requireFinitePositive (*c.mmPerUnit, "mmPerUnit");
	requirePrintLength (c.minWallMm, "minWallMm");
	requirePrintLength (c.minHoleMm, "minHoleMm");
	requirePrintLength (c.maxTipLengthMm, "maxTipLengthMm");
	requireFinitePositive (c.minDetailMm, "minDetailMm");
	if (!std::isfinite (c.noiseDepthRatio) || c.noiseDepthRatio < 0.0 || c.noiseDepthRatio >= 1.0)
		outOfDomain ("noiseDepthRatio", c.noiseDepthRatio, "must be in [0, 1[");
	if (c.envelopeMm)
		for (double e : *c.envelopeMm)
			requireFinitePositive (e, "envelopeMm");
}

double roundingTolerance (double r, double halfExtent)
{
	return std::max (kRoundingTolRatio * r, kRoundingTolUlps * FLT_EPSILON * halfExtent);
}

// ---------------------------------------------------------------------------
//  Geometrie
// ---------------------------------------------------------------------------

struct Box
{
	double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
	bool   empty = true;

	void Add (double x, double y)
	{
		if (empty) { x0 = x1 = x; y0 = y1 = y; empty = false; return; }
		x0 = std::min (x0, x); x1 = std::max (x1, x);
		y0 = std::min (y0, y); y1 = std::max (y1, y);
	}
	bool Contains (double x, double y) const { return !empty && x >= x0 && x <= x1 && y >= y0 && y <= y1; }
	bool Overlaps (const Box &o, double margin) const
	{
		return !empty && !o.empty && x0 <= o.x1 + margin && o.x0 - margin <= x1
		    && y0 <= o.y1 + margin && o.y0 - margin <= y1;
	}
};

Box boxOf (const std::vector<ExtrudeContour> &contours)
{
	Box b;
	for (const ExtrudeContour &c : contours)
		for (const Vector2f &p : c.pts)
			b.Add (p.x, p.y);
	return b;
}

double segmentDistance2 (double px, double py, const Vector2f &a, const Vector2f &b)
{
	const double ax = a.x, ay = a.y;
	const double dx = (double)b.x - ax, dy = (double)b.y - ay;
	const double len2 = dx * dx + dy * dy;
	double t = len2 > 0.0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0;
	t = std::clamp (t, 0.0, 1.0);
	const double ex = ax + t * dx - px, ey = ay + t * dy - py;
	return ex * ex + ey * ey;
}

// Pair-impair sur les contours d'UNE region (enveloppe et ses trous).
bool insideRegion (const std::vector<ExtrudeContour> &region, double x, double y)
{
	bool in = false;
	for (const ExtrudeContour &c : region)
	{
		const std::size_t n = c.pts.size();
		for (std::size_t i = 0, j = n - 1; i < n; j = i++)
		{
			const double xi = c.pts[i].x, yi = c.pts[i].y, xj = c.pts[j].x, yj = c.pts[j].y;
			if ((yi > y) != (yj > y) && x < xi + (y - yi) * (xj - xi) / (yj - yi))
				in = !in;
		}
	}
	return in;
}

struct Segment
{
	Vector2f a, b;
	int      id = 0;
};

// Segments ranges dans une grille uniforme, chacun inscrit dans les cellules
// que touche sa boite elargie de `reach` : toute requete a moins de `reach` ne
// lit que la cellule du point.
class SegmentGrid
{
public:
	SegmentGrid (std::vector<Segment> segments, double reach)
		: m_segments (std::move (segments)), m_reach (reach)
	{
		if (m_segments.empty()) return;
		Box b;
		for (const Segment &s : m_segments) { b.Add (s.a.x, s.a.y); b.Add (s.b.x, s.b.y); }
		m_x0 = b.x0 - reach;
		m_y0 = b.y0 - reach;
		const double w = b.x1 - b.x0 + 2.0 * reach, h = b.y1 - b.y0 + 2.0 * reach;
		// Environ un segment par cellule pour une repartition uniforme ; jamais
		// moins que la portee, qui fixerait sinon le nombre d'inscriptions.
		m_cell = std::max (std::sqrt (w * h / (double)m_segments.size()), reach);
		if (!(m_cell > 0.0)) m_cell = 1.0;
		m_nx = (std::size_t)(w / m_cell) + 1;
		m_ny = (std::size_t)(h / m_cell) + 1;
		m_cells.assign (m_nx * m_ny, {});
		for (std::size_t k = 0; k < m_segments.size(); ++k)
		{
			const Segment &s = m_segments[k];
			const std::size_t i0 = cellX (std::min (s.a.x, s.b.x) - reach), i1 = cellX (std::max (s.a.x, s.b.x) + reach);
			const std::size_t j0 = cellY (std::min (s.a.y, s.b.y) - reach), j1 = cellY (std::max (s.a.y, s.b.y) + reach);
			for (std::size_t j = j0; j <= j1; ++j)
				for (std::size_t i = i0; i <= i1; ++i)
					m_cells[j * m_nx + i].push_back ((unsigned)k);
		}
	}

	// Identifiant d'un segment a moins de `tol` (<= portee) du point, -1 sinon.
	int NearId (double x, double y, double tol) const
	{
		const std::vector<unsigned> *cell = cellAt (x, y);
		if (!cell) return -1;
		const double tol2 = tol * tol;
		for (unsigned k : *cell)
			if (segmentDistance2 (x, y, m_segments[k].a, m_segments[k].b) < tol2)
				return m_segments[k].id;
		return -1;
	}

	// Distance au segment le plus proche, plafonnee a la portee.
	double Distance (double x, double y) const
	{
		double best2 = m_reach * m_reach;
		if (const std::vector<unsigned> *cell = cellAt (x, y))
			for (unsigned k : *cell)
				best2 = std::min (best2, segmentDistance2 (x, y, m_segments[k].a, m_segments[k].b));
		return std::sqrt (best2);
	}

private:
	std::size_t cellX (double x) const
	{
		return (std::size_t)std::clamp (std::floor ((x - m_x0) / m_cell), 0.0, (double)(m_nx - 1));
	}
	std::size_t cellY (double y) const
	{
		return (std::size_t)std::clamp (std::floor ((y - m_y0) / m_cell), 0.0, (double)(m_ny - 1));
	}
	const std::vector<unsigned> *cellAt (double x, double y) const
	{
		if (m_cells.empty() || x < m_x0 || y < m_y0) return nullptr;
		const std::size_t i = (std::size_t)((x - m_x0) / m_cell), j = (std::size_t)((y - m_y0) / m_cell);
		if (i >= m_nx || j >= m_ny) return nullptr;
		return &m_cells[j * m_nx + i];
	}

	std::vector<Segment> m_segments;
	double m_reach = 0.0;
	double m_x0 = 0.0, m_y0 = 0.0, m_cell = 1.0;
	std::size_t m_nx = 0, m_ny = 0;
	std::vector<std::vector<unsigned>> m_cells;
};

// ---------------------------------------------------------------------------
//  Morceaux du reste
// ---------------------------------------------------------------------------

struct Contacts
{
	unsigned runs = 0;          // suites cycliques d'aretes de contact
	unsigned components = 0;    // composantes conservees distinctes touchees
	std::vector<std::vector<char>> edge;   // par contour, par arete
};

Contacts countContacts (const std::vector<ExtrudeContour> &piece, const SegmentGrid &kept, double tol)
{
	Contacts out;
	std::vector<int> seen;
	out.edge.resize (piece.size());
	for (std::size_t c = 0; c < piece.size(); ++c)
	{
		const std::vector<Vector2f> &p = piece[c].pts;
		const std::size_t n = p.size();
		out.edge[c].assign (n, 0);
		std::size_t inContact = 0;
		for (std::size_t i = 0; i < n; ++i)
		{
			const Vector2f &a = p[i], &b = p[(i + 1) % n];
			const int id = kept.NearId (0.5 * ((double)a.x + b.x), 0.5 * ((double)a.y + b.y), tol);
			if (id < 0) continue;
			out.edge[c][i] = 1;
			++inContact;
			if (std::find (seen.begin(), seen.end(), id) == seen.end()) seen.push_back (id);
		}
		if (inContact == 0) continue;
		if (inContact == n) { ++out.runs; continue; }
		for (std::size_t i = 0; i < n; ++i)
			if (out.edge[c][i] && !out.edge[c][(i + n - 1) % n])
				++out.runs;
	}
	out.components = (unsigned)seen.size();
	return out;
}

// Plus grande distance d'un point du bord echantillonne aux aretes de contact
// du morceau, plafonnee a `cap` ; s'arrete au premier echantillon qui l'atteint.
double boundaryDepth (const std::vector<ExtrudeContour> &piece, const Contacts &contacts,
                      double step, double cap)
{
	std::vector<Segment> segs;
	for (std::size_t c = 0; c < piece.size(); ++c)
	{
		const std::vector<Vector2f> &p = piece[c].pts;
		for (std::size_t i = 0; i < p.size(); ++i)
			if (contacts.edge[c][i])
				segs.push_back ({p[i], p[(i + 1) % p.size()], 0});
	}
	const SegmentGrid grid (std::move (segs), cap);

	double best = 0.0;
	for (std::size_t c = 0; c < piece.size(); ++c)
	{
		const std::vector<Vector2f> &p = piece[c].pts;
		for (std::size_t i = 0; i < p.size(); ++i)
		{
			if (contacts.edge[c][i]) continue;
			const Vector2f &a = p[i], &b = p[(i + 1) % p.size()];
			const double dx = (double)b.x - a.x, dy = (double)b.y - a.y;
			const std::size_t steps = (std::size_t)std::ceil (std::sqrt (dx * dx + dy * dy) / step);
			for (std::size_t s = 0; s <= steps; ++s)
			{
				const double t = steps ? (double)s / (double)steps : 0.0;
				best = std::max (best, grid.Distance (a.x + t * dx, a.y + t * dy));
				if (best >= cap) return cap;
			}
		}
	}
	return best;
}

double regionArea (const std::vector<ExtrudeContour> &region)
{
	double a = 0.0;
	for (const ExtrudeContour &c : region) a += (double)contourSignedArea (c.pts);
	return std::fabs (a);
}

// ---------------------------------------------------------------------------
//  Rapport
// ---------------------------------------------------------------------------

class ReportBuilder
{
public:
	ReportBuilder (PrintabilityReport &r, const PrintConstraints &c) : m_r (r), m_c (c) {}

	// Origine du repere de calcul, en mm du repere du modele : ajoutee aux boites.
	void SetOrigin (double offX, double offY) { m_offX = offX; m_offY = offY; }

	// Tranche courante d'une pile : chaque enregistrement en porte l'indice et la
	// couche, et ses comptes s'ajoutent a son resume. -1 : hors pile.
	void SetSlice (int slice)
	{
		m_slice = slice;
		m_sliceCounts = (slice >= 0) ? &m_r.slices[(std::size_t)slice].counts : nullptr;
	}

	void Add (PrintDefect kind, const Box &box, double depth, double area, unsigned contacts, double widthBelowMm)
	{
		PrintDefectRecord d;
		d.kind = kind;
		if (!box.empty)
			d.boxMm = {box.x0 + m_offX, box.y0 + m_offY, box.x1 + m_offX, box.y1 + m_offY};
		d.depthMm = depth;
		d.areaMm2 = area;
		d.contacts = contacts;
		d.widthBelowMm = widthBelowMm;
		Add (d);
	}

	void Add (PrintDefectRecord d)
	{
		const std::size_t k = static_cast<std::size_t>(d.kind);
		++m_r.counts[k];
		if (m_sliceCounts) ++(*m_sliceCounts)[k];
		if (m_kept[k] >= m_c.maxRecordsPerKind) return;
		++m_kept[k];
		if (m_slice >= 0)
		{
			const PrintSliceSummary &sum = m_r.slices[(std::size_t)m_slice];
			d.slice = m_slice;
			d.zLowMm = sum.zLowMm;
			d.zHighMm = sum.zHighMm;
		}
		m_r.records.push_back (d);
	}

private:
	PrintabilityReport &m_r;
	const PrintConstraints &m_c;
	double m_offX = 0.0, m_offY = 0.0;
	int m_slice = -1;
	std::array<std::size_t, kPrintDefectCount> *m_sliceCounts = nullptr;
	std::array<std::size_t, kPrintDefectCount> m_kept {};
};

struct Side
{
	bool   solid = true;
	double r = 0.0;          // rayon d'ouverture, mm
	double tol = 0.0;        // delta, mm
	double widthMm = 0.0;    // seuil de la categorie
};

struct Context
{
	const PrintConstraints &c;
	PrintabilityReport     &report;
	ReportBuilder          &out;
	double                  depthCap = 0.0;
	Box                     solidBox;
};

// Composantes conservees contenant au moins deux composantes de l'erosion : un
// col plus etroit que 2r sans reste.
void reportPinches (const std::vector<ExtrudeContour> &eroded,
                    const std::vector<std::vector<ExtrudeContour>> &keptRegions,
                    const std::vector<Box> &keptBoxes, const Side &side, Context &ctx)
{
	std::vector<unsigned> cores (keptRegions.size(), 0);
	for (const std::vector<ExtrudeContour> &core : contourRegions (eroded))
	{
		// Un sommet de l'erosion est a r + delta du bord de sa composante
		// conservee : le test de point est franc.
		const Vector2f &p = core[0].pts[0];
		for (std::size_t k = 0; k < keptRegions.size(); ++k)
			if (keptBoxes[k].Contains (p.x, p.y) && insideRegion (keptRegions[k], p.x, p.y))
			{
				++cores[k];
				break;
			}
	}
	for (std::size_t k = 0; k < keptRegions.size(); ++k)
		if (cores[k] >= 2)
			ctx.out.Add (side.solid ? PrintDefect::ThinWall : PrintDefect::NarrowOpening,
			             keptBoxes[k], 0.0, 0.0, cores[k], side.widthMm);
}

void classifyPiece (const std::vector<ExtrudeContour> &piece, const SegmentGrid &keptGrid,
                    const Side &side, Context &ctx)
{
	const Box box = boxOf (piece);
	// Coin du rectangle des vides : aucune matiere a portee.
	if (!side.solid && !box.Overlaps (ctx.solidBox, side.r)) return;
	++(side.solid ? ctx.report.solidPieces : ctx.report.voidPieces);

	const Contacts contacts = countContacts (piece, keptGrid, 0.5 * side.tol);
	const double area = regionArea (piece);
	if (contacts.runs == 0)
	{
		ctx.out.Add (side.solid ? PrintDefect::ThinIsland : PrintDefect::ClosedOpening,
		             box, 0.0, area, 0, side.widthMm);
		return;
	}

	const bool bridge = contacts.components >= 2;
	const double depth = boundaryDepth (piece, contacts, kDepthSampleRatio * side.r, ctx.depthCap);
	if (!bridge && depth < ctx.c.noiseDepthRatio * side.r)
	{
		++ctx.report.noisePieces;
		return;
	}
	const bool neck = bridge || contacts.runs >= 2;
	if (side.solid)
	{
		if (neck)                              ctx.out.Add (PrintDefect::ThinWall, box, depth, area, contacts.runs, side.widthMm);
		else if (depth > ctx.c.maxTipLengthMm) ctx.out.Add (PrintDefect::FragileTip, box, depth, area, contacts.runs, 0.0);
		else                                   ++ctx.report.bluntTips;
	}
	else
	{
		if (neck) ctx.out.Add (PrintDefect::NarrowOpening, box, depth, area, contacts.runs, side.widthMm);
		else      ctx.out.Add (PrintDefect::BluntedVoidCorner, box, depth, area, contacts.runs, 0.0);
	}
}

// Ouvre `region` (normalisee, en mm) et classe pincements et morceaux du reste.
void classifySide (const std::vector<ExtrudeContour> &region, const Side &side, Context &ctx)
{
	const std::vector<ExtrudeContour> eroded = offsetContours (region, (float)-side.r, StrokeJoin::Round);
	// Partie conservee : l'ouverture dilatee de delta, en une seule dilatation.
	const std::vector<ExtrudeContour> kept = eroded.empty()
		? eroded : offsetContours (eroded, (float)(side.r + side.tol), StrokeJoin::Round);

	const std::vector<std::vector<ExtrudeContour>> keptRegions = contourRegions (kept);
	std::vector<Box> keptBoxes;
	std::vector<Segment> keptSegments;
	for (std::size_t k = 0; k < keptRegions.size(); ++k)
	{
		keptBoxes.push_back (boxOf (keptRegions[k]));
		for (const ExtrudeContour &c : keptRegions[k])
			for (std::size_t i = 0; i < c.pts.size(); ++i)
				keptSegments.push_back ({c.pts[i], c.pts[(i + 1) % c.pts.size()], (int)k});
	}
	const SegmentGrid keptGrid (std::move (keptSegments), side.tol);

	reportPinches (eroded, keptRegions, keptBoxes, side, ctx);
	for (const std::vector<ExtrudeContour> &piece : contourRegions (differenceContours (region, kept)))
		classifyPiece (piece, keptGrid, side, ctx);
}

} // namespace

const char *PrintDefectName (PrintDefect kind)
{
	switch (kind)
	{
	case PrintDefect::ThinWall:          return "thin_wall";
	case PrintDefect::ThinIsland:        return "thin_island";
	case PrintDefect::FragileTip:        return "fragile_tip";
	case PrintDefect::ClosedOpening:     return "closed_opening";
	case PrintDefect::NarrowOpening:     return "narrow_opening";
	case PrintDefect::ThinSlab:          return "thin_slab";
	case PrintDefect::DetailTooSmall:    return "detail_too_small";
	case PrintDefect::EnvelopeExceeded:  return "envelope_exceeded";
	case PrintDefect::BluntedVoidCorner: return "blunted_void_corner";
	case PrintDefect::ScaleUnset:        return "scale_unset";
	}
	return "unknown";
}

bool PrintabilityReport::IsPrintable () const
{
	if (!evaluated) return false;
	for (std::size_t k = 0; k < kPrintDefectCount; ++k)
		if (!IsPrintInformation (static_cast<PrintDefect>(k)) && counts[k] != 0)
			return false;
	return true;
}

namespace
{

void requireFiniteRegion (const std::vector<ExtrudeContour> &region, const char *who)
{
	for (const ExtrudeContour &c : region)
		for (const Vector2f &p : c.pts)
			if (!std::isfinite (p.x) || !std::isfinite (p.y))
				throw std::invalid_argument (std::string (who) + ": non-finite point in region");
}

// Echelle et rayons du rapport. Sans echelle : ScaleUnset, et faux.
bool beginReport (PrintabilityReport &report, ReportBuilder &out, const PrintConstraints &constraints)
{
	if (!constraints.mmPerUnit)
	{
		PrintDefectRecord d;
		d.kind = PrintDefect::ScaleUnset;
		out.Add (d);
		return false;
	}
	const double s = *constraints.mmPerUnit;
	report.evaluated = true;
	report.mmPerUnit = s;
	const double rWall = 0.5 * constraints.minWallMm, rHole = 0.5 * constraints.minHoleMm;
	report.wallRadius = rWall / s;
	report.holeRadius = rHole / s;
	report.depthCapMm = kDepthCapFactor * std::max (constraints.noiseDepthRatio * std::max (rWall, rHole),
	                                                constraints.maxTipLengthMm);
	return true;
}

struct Sides
{
	bool solid = true;   // parois, ilots, pointes de matiere
	bool voids = true;   // ouvertures, coins de vide
};

// Mesure la REUNION des regions (unites du modele, chacune sous NonZero, points
// finis) sur les cotes demandes ; rend l'aire de la matiere, en mm2. Repere de
// calcul : recentre sur l'emprise commune, en mm. Une seule region est lue
// telle quelle (une union NonZero) ; plusieurs sont normalisees chacune, puis
// reunies, pour qu'un trou de l'une ne retranche rien a la matiere d'une autre.
double measureRegions (const std::vector<const std::vector<ExtrudeContour> *> &regions,
                       const PrintConstraints &constraints, Sides sides,
                       PrintabilityReport &report, ReportBuilder &out, const char *who)
{
	Box input;
	for (const std::vector<ExtrudeContour> *region : regions)
		for (const ExtrudeContour &c : *region)
			for (const Vector2f &p : c.pts)
				input.Add (p.x, p.y);
	if (input.empty) return 0.0;

	const double s = report.mmPerUnit;
	const double rWall = 0.5 * constraints.minWallMm, rHole = 0.5 * constraints.minHoleMm;

	// Repere de calcul : recentre, en millimetres.
	const double cx = 0.5 * (input.x0 + input.x1), cy = 0.5 * (input.y0 + input.y1);
	out.SetOrigin (cx * s, cy * s);
	std::vector<std::vector<ExtrudeContour>> mm (regions.size());
	double halfExtent = 0.0, inputArea = 0.0;
	for (std::size_t k = 0; k < regions.size(); ++k)
	{
		mm[k].reserve (regions[k]->size());
		double regionArea2 = 0.0;
		for (const ExtrudeContour &c : *regions[k])
		{
			ExtrudeContour e;
			e.pts.reserve (c.pts.size());
			for (std::size_t i = 0; i < c.pts.size(); ++i)
			{
				const Vector2f &p = c.pts[i], &q = c.pts[(i + 1) % c.pts.size()];
				const double x = ((double)p.x - cx) * s, y = ((double)p.y - cy) * s;
				const double qx = ((double)q.x - cx) * s, qy = ((double)q.y - cy) * s;
				if (!std::isfinite (x) || !std::isfinite (y))
					throw std::invalid_argument (std::string (who) + ": region overflows once converted to millimetres");
				halfExtent = std::max ({halfExtent, std::fabs (x), std::fabs (y)});
				regionArea2 += x * qy - qx * y;
				e.pts.push_back (Vector2f ((float)x, (float)y));
			}
			mm[k].push_back (std::move (e));
		}
		inputArea += std::fabs (0.5 * regionArea2);
	}
	const double maxHalfExtent = kMaxRoundingShare * std::min (rWall, rHole) / (kRoundingTolUlps * FLT_EPSILON);
	if (halfExtent > maxHalfExtent)
	{
		std::ostringstream os = classicStream ();
		os << who << ": half-extent " << halfExtent << " mm exceeds " << maxHalfExtent
		   << " mm for " << (rWall <= rHole ? "minWallMm " : "minHoleMm ")
		   << (rWall <= rHole ? constraints.minWallMm : constraints.minHoleMm)
		   << " (float precision of the opening radius)";
		throw std::invalid_argument (os.str());
	}

	// Union NonZero : recouvrements fondus, orientation de Clipper2.
	std::vector<ExtrudeContour> solid;
	if (mm.size() == 1)
		solid = offsetContours (mm[0], 0.f, StrokeJoin::Round);
	else
	{
		std::vector<ExtrudeContour> each;
		for (const std::vector<ExtrudeContour> &r : mm)
			for (ExtrudeContour &c : offsetContours (r, 0.f, StrokeJoin::Round))
				each.push_back (std::move (c));
		solid = offsetContours (each, 0.f, StrokeJoin::Round);
	}
	if (solid.empty())
	{
		// De la matiere sous la resolution de Clipper2 (1e-6 mm).
		if (sides.solid && inputArea > 0.0)
		{
			Box b;
			b.Add ((input.x0 - cx) * s, (input.y0 - cy) * s);
			b.Add ((input.x1 - cx) * s, (input.y1 - cy) * s);
			out.Add (PrintDefect::ThinIsland, b, 0.0, inputArea, 0, constraints.minWallMm);
		}
		return 0.0;
	}

	Context ctx {constraints, report, out, report.depthCapMm, boxOf (solid)};
	if (sides.solid)
		classifySide (solid, {true, rWall, roundingTolerance (rWall, halfExtent), constraints.minWallMm}, ctx);
	if (sides.voids)
	{
		const double margin = kVoidBoxMarginRatio * rHole;
		const Box &sb = ctx.solidBox;
		std::vector<ExtrudeContour> box (1);
		box[0].pts = roundedRectContour ((float)(sb.x0 - margin), (float)(sb.y0 - margin),
		                                 (float)(sb.x1 + margin), (float)(sb.y1 + margin), 0.f);
		classifySide (differenceContours (box, solid),
		              {false, rHole, roundingTolerance (rHole, halfExtent + margin), constraints.minHoleMm}, ctx);
	}
	return regionArea (solid);
}

} // namespace

PrintabilityReport CheckPrintRegion (const std::vector<ExtrudeContour> &region,
                                     const PrintConstraints &constraints)
{
	validateConstraints (constraints);
	requireFiniteRegion (region, "CheckPrintRegion");

	PrintabilityReport report;
	ReportBuilder out (report, constraints);
	if (beginReport (report, out, constraints))
		measureRegions ({&region}, constraints, Sides {}, report, out, "CheckPrintRegion");
	return report;
}

PrintabilityReport CheckPrintExtrusion (const std::vector<ExtrudeContour> &region, double heightUnits,
                                        const PrintConstraints &constraints)
{
	if (!std::isfinite (heightUnits) || heightUnits < 0.0)
	{
		std::ostringstream os = classicStream ();
		os << "CheckPrintExtrusion: height " << heightUnits << " must be finite and >= 0";
		throw std::invalid_argument (os.str());
	}
	PrintabilityReport report = CheckPrintRegion (region, constraints);
	const Box box = boxOf (region);
	if (!report.evaluated || box.empty)
		return report;

	const double s = report.mmPerUnit;
	if (heightUnits * s < constraints.minWallMm)
	{
		// Une seule plaque, et CheckPrintRegion n'enregistre aucun ThinSlab.
		PrintDefectRecord d;
		d.kind = PrintDefect::ThinSlab;
		d.boxMm = {box.x0 * s, box.y0 * s, box.x1 * s, box.y1 * s};
		d.widthBelowMm = constraints.minWallMm;
		++report.counts[static_cast<std::size_t>(PrintDefect::ThinSlab)];
		if (constraints.maxRecordsPerKind > 0)
			report.records.push_back (d);
	}
	return report;
}

PrintabilityReport CheckPrintSlices (const std::vector<PrintSlice> &slices,
                                     const PrintConstraints &constraints)
{
	validateConstraints (constraints);
	for (std::size_t i = 0; i < slices.size(); ++i)
	{
		const PrintSlice &sl = slices[i];
		if (!std::isfinite (sl.zLow) || !std::isfinite (sl.zHigh) || !std::isfinite (sl.zCut)
		    || !(sl.zLow <= sl.zCut && sl.zCut <= sl.zHigh))
		{
			std::ostringstream os = classicStream ();
			os << "CheckPrintSlices: slice " << i << " heights zLow " << sl.zLow << ", zCut " << sl.zCut
			   << ", zHigh " << sl.zHigh << " must be finite with zLow <= zCut <= zHigh";
			throw std::invalid_argument (os.str());
		}
		requireFiniteRegion (sl.region, "CheckPrintSlices");
	}

	PrintabilityReport report;
	ReportBuilder out (report, constraints);
	if (!beginReport (report, out, constraints) || slices.empty())
		return report;

	// Cotes converties en mm : finies, ainsi que la hauteur de la pile.
	const double s = report.mmPerUnit;
	for (std::size_t i = 0; i < slices.size(); ++i)
	{
		const PrintSlice &sl = slices[i];
		if (!std::isfinite (sl.zLow * s) || !std::isfinite (sl.zHigh * s) || !std::isfinite (sl.zCut * s)
		    || !std::isfinite ((sl.zHigh - sl.zLow) * s))
		{
			std::ostringstream os = classicStream ();
			os << "CheckPrintSlices: slice " << i << " heights zLow " << sl.zLow << ", zCut " << sl.zCut
			   << ", zHigh " << sl.zHigh << " overflow once converted to millimetres (mmPerUnit " << s << ")";
			throw std::invalid_argument (os.str());
		}
	}

	// Resumes poses avant toute mesure : le constructeur de rapport y compte.
	double zMin = slices[0].zLow, zMax = slices[0].zHigh;
	report.slices.reserve (slices.size() + 1);
	for (const PrintSlice &sl : slices)
	{
		PrintSliceSummary sum;
		sum.zLowMm = sl.zLow * s;
		sum.zHighMm = sl.zHigh * s;
		sum.zCutMm = sl.zCut * s;
		report.slices.push_back (sum);
		zMin = std::min (zMin, sl.zLow);
		zMax = std::max (zMax, sl.zHigh);
	}
	if (!std::isfinite ((zMax - zMin) * s))
	{
		std::ostringstream os = classicStream ();
		os << "CheckPrintSlices: stack from " << zMin << " to " << zMax
		   << " overflows once converted to millimetres (mmPerUnit " << s << ")";
		throw std::invalid_argument (os.str());
	}
	PrintSliceSummary projection;
	projection.silhouette = true;
	projection.zLowMm = zMin * s;
	projection.zHighMm = zMax * s;
	report.slices.push_back (projection);

	// Matiere : chaque tranche.
	std::vector<const std::vector<ExtrudeContour> *> all;
	all.reserve (slices.size());
	for (std::size_t i = 0; i < slices.size(); ++i)
	{
		out.SetSlice ((int)i);
		report.slices[i].areaMm2 = measureRegions ({&slices[i].region}, constraints, Sides {true, false},
		                                           report, out, "CheckPrintSlices");
		all.push_back (&slices[i].region);
	}

	// Vides traversants : complement de la silhouette.
	const std::size_t silhouette = slices.size();
	out.SetSlice ((int)silhouette);
	report.slices[silhouette].areaMm2 = measureRegions (all, constraints, Sides {false, true},
	                                                    report, out, "CheckPrintSlices");

	// Plaque : hauteur de la pile, comme CheckPrintExtrusion.
	Box box;
	for (const std::vector<ExtrudeContour> *region : all)
		for (const ExtrudeContour &c : *region)
			for (const Vector2f &p : c.pts)
				box.Add (p.x, p.y);
	if (!box.empty && (zMax - zMin) * s < constraints.minWallMm)
	{
		Box mmBox;
		mmBox.Add (box.x0 * s, box.y0 * s);
		mmBox.Add (box.x1 * s, box.y1 * s);
		out.SetOrigin (0.0, 0.0);
		out.Add (PrintDefect::ThinSlab, mmBox, 0.0, 0.0, 0, constraints.minWallMm);
	}
	return report;
}

std::array<std::size_t, kPrintDefectCount> PrintDisplayCounts (const PrintabilityReport &report)
{
	std::array<std::size_t, kPrintDefectCount> shown = report.counts;
	if (report.slices.empty())
		return shown;
	for (std::size_t k = 0; k < kPrintDefectCount; ++k)
	{
		std::size_t sliceMax = 0, sliceSum = 0;
		for (const PrintSliceSummary &sum : report.slices)
		{
			sliceMax = std::max (sliceMax, sum.counts[k]);
			sliceSum += sum.counts[k];
		}
		shown[k] = sliceMax + (report.counts[k] - std::min (report.counts[k], sliceSum));
	}
	return shown;
}

PrintabilityReport CheckPrintEnvelope (const PrintExtent &extentUnits, const PrintConstraints &constraints)
{
	validateConstraints (constraints);
	for (std::size_t k = 0; k < 3; ++k)
	{
		const double lo = extentUnits.lo[k], hi = extentUnits.hi[k];
		if (!std::isfinite (lo) || !std::isfinite (hi) || lo > hi)
		{
			std::ostringstream os = classicStream ();
			os << "CheckPrintEnvelope: extent [" << lo << ", " << hi << "] on axis " << k
			   << " must be finite with lo <= hi";
			throw std::invalid_argument (os.str());
		}
	}

	PrintabilityReport report;
	if (!constraints.mmPerUnit)
		return report;
	const double s = *constraints.mmPerUnit;
	report.evaluated = true;
	report.mmPerUnit = s;

	std::array<double, 3> lo {}, hi {}, size {};
	for (std::size_t k = 0; k < 3; ++k)
	{
		lo[k] = extentUnits.lo[k] * s;
		hi[k] = extentUnits.hi[k] * s;
		size[k] = hi[k] - lo[k];
		if (!std::isfinite (lo[k]) || !std::isfinite (hi[k]) || !std::isfinite (size[k]))
		{
			std::ostringstream os = classicStream ();
			os << "CheckPrintEnvelope: extent [" << extentUnits.lo[k] << ", " << extentUnits.hi[k] << "] on axis "
			   << k << " overflows once converted to millimetres (mmPerUnit " << s << ")";
			throw std::invalid_argument (os.str());
		}
	}
	if (!constraints.envelopeMm)
		return report;

	// Une permutation des axes loge la boite si et seulement si les cotes
	// triees de la boite sont, une a une, au plus celles de l'enveloppe.
	std::array<double, 3> piece = size, envelope = *constraints.envelopeMm;
	std::sort (piece.begin(), piece.end());
	std::sort (envelope.begin(), envelope.end());
	if (piece[0] <= envelope[0] && piece[1] <= envelope[1] && piece[2] <= envelope[2])
		return report;

	PrintDefectRecord d;
	d.kind = PrintDefect::EnvelopeExceeded;
	d.boxMm = {lo[0], lo[1], hi[0], hi[1]};
	d.zLowMm = lo[2];
	d.zHighMm = hi[2];
	ReportBuilder out (report, constraints);
	out.Add (d);
	return report;
}

double MmPerUnitForPrintedWidth (double printedWidthMm, double widthUnits)
{
	const double s = printedWidthMm / widthUnits;
	if (!std::isfinite (printedWidthMm) || printedWidthMm <= 0.0 || !std::isfinite (widthUnits) || widthUnits <= 0.0
	    || !std::isfinite (s) || s <= 0.0)
	{
		std::ostringstream os = classicStream ();
		os << "MmPerUnitForPrintedWidth: printed width " << printedWidthMm << " mm over " << widthUnits
		   << " units gives mmPerUnit " << s << ", which must be finite and > 0";
		throw std::invalid_argument (os.str());
	}
	return s;
}
