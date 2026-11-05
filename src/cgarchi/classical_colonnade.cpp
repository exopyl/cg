#include <cgarchi/classical_colonnade.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

#include <cgarchi/classical_volute.h>   // VolutePoint
#include <cgarchi/classical_mesh.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

const ProfileMember &SectionMember (const ColumnProfile &section, const std::string &block, const std::string &name)
{
	const ProfileMember *found = nullptr;
	for (const ProfileMember &m : section.members)
		if (m.block == block && m.name == name)
		{
			if (found)
				throw std::invalid_argument ("membre ambigu dans l'entablement : " + block + "/" + name);
			found = &m;
		}
	if (!found)
		throw std::invalid_argument ("membre absent de l'entablement : " + block + "/" + name);
	return *found;
}

// Centres anchor + k p dont l'etendue [c - w/2, c + w/2] tient dans [lo, hi].
std::vector<double> Centres (double lo, double hi, double anchor, double p, double w)
{
	std::vector<double> out;
	long long k = static_cast<long long> (std::floor ((lo - anchor) / p)) - 1;
	for (;; ++k)
	{
		const double c = anchor + static_cast<double> (k) * p;
		if (c - 0.5 * w > hi + kEps)
			break;
		if (c - 0.5 * w >= lo - kEps && c + 0.5 * w <= hi + kEps)
			out.push_back (c);
	}
	return out;
}

struct Row
{
	OrnamentKind kind;
	double width, period, anchorFront;   // ancrage sur les faces longues
	double back, face;                   // saillies depuis le plan des axes
	double z0, z1;
};

// Pose d'une rangee sur les quatre faces ; rend les placements ajoutes.
std::vector<OrnamentPlacement> PlaceRow (const Row &row, const ColonnadeLayout &layout)
{
	std::vector<OrnamentPlacement> out;
	const ColonnadeFace faces[] = { ColonnadeFace::Front, ColonnadeFace::Back, ColonnadeFace::Left, ColonnadeFace::Right };
	for (ColonnadeFace face : faces)
	{
		const bool along = face == ColonnadeFace::Front || face == ColonnadeFace::Back;
		const double lo = along ? layout.axes.front () - row.back : -row.back;
		const double hi = along ? layout.axes.back () + row.back : row.back;
		for (double c : Centres (lo, hi, along ? row.anchorFront : 0.0, row.period, row.width))
		{
			OrnamentPlacement p;
			p.kind = row.kind;
			p.face = face;
			p.centre = c;
			p.a0 = c - 0.5 * row.width;
			p.a1 = c + 0.5 * row.width;
			p.d0 = row.back;
			p.d1 = row.face;
			p.z0 = row.z0;
			p.z1 = row.z1;
			out.push_back (p);
		}
	}
	return out;
}

// Gouttes : grille rows x perRow sous un element, centres en a et en d.
void AddGuttae (std::vector<OrnamentPlacement> &out, OrnamentKind kind, const OrnamentPlacement &host,
                const std::vector<double> &along, const std::vector<double> &depths, double zTop, double height,
                double rBottom, double rTop)
{
	for (double a : along)
		for (double d : depths)
		{
			OrnamentPlacement g;
			g.kind = kind;
			g.face = host.face;
			g.centre = host.centre + a;
			g.a0 = g.centre - rBottom;
			g.a1 = g.centre + rBottom;
			g.depthCentre = d;
			g.d0 = d - rBottom;
			g.d1 = d + rBottom;
			g.z0 = zTop - height;
			g.z1 = zTop;
			g.radiusBottom = rBottom;
			g.radiusTop = rTop;
			out.push_back (g);
		}
}

std::vector<double> Spread (int count, double pitch)
{
	std::vector<double> out;
	for (int i = 0; i < count; ++i)
		out.push_back ((i - 0.5 * (count - 1)) * pitch);
	return out;
}

// --------------------------------------------------------------------------
//  Maillage
// --------------------------------------------------------------------------

class Builder
{
public:
	unsigned Add (double x, double y, double z)
	{
		positions.push_back ({x, y, z});
		return static_cast<unsigned> (positions.size () - 1);
	}
	void Tri (unsigned a, unsigned b, unsigned c) { triangles.push_back ({a, b, c}); }

	// Prisme vertical d'une section monotone en a : fond droit en d = back,
	// profil `top` (a strictement croissant, d > back) ; `map` envoie (a, d)
	// sur (x, y) monde. Faces planes triangulees en bandes entre le fond et
	// le profil.
	template <class Map>
	void Section (const std::vector<VolutePoint> &top, double back, Map map, double z0, double z1)
	{
		const unsigned n = static_cast<unsigned> (top.size ());
		// Sens de l'application (a, d) -> (x, y) : un determinant negatif
		// retourne les faces.
		const VolutePoint o = map (0.0, 0.0), ea = map (1.0, 0.0), ed = map (0.0, 1.0);
		const bool flip = (ea.u - o.u) * (ed.v - o.v) - (ea.v - o.v) * (ed.u - o.u) < 0.0;
		auto tri = [&] (unsigned a, unsigned b, unsigned c) {
			if (flip)
				Tri (a, c, b);
			else
				Tri (a, b, c);
		};
		const unsigned base = static_cast<unsigned> (positions.size ());
		// Par point i : fond bas, profil bas, fond haut, profil haut.
		for (const VolutePoint &q : top)
		{
			const VolutePoint f = map (q.u, back), t = map (q.u, q.v);
			Add (f.u, f.v, z0);
			Add (t.u, t.v, z0);
			Add (f.u, f.v, z1);
			Add (t.u, t.v, z1);
		}
		auto B = [&] (unsigned i, int level) { return base + 4 * i + (level ? 2u : 0u); };
		auto T = [&] (unsigned i, int level) { return base + 4 * i + (level ? 3u : 1u); };
		for (unsigned i = 0; i + 1 < n; ++i)
		{
			// Dessus (z1) : (B_i, B_i+1, T_i+1, T_i) en sens direct dans (a, d).
			tri (B (i, 1), B (i + 1, 1), T (i + 1, 1));
			tri (B (i, 1), T (i + 1, 1), T (i, 1));
			// Dessous (z0) : sens inverse.
			tri (B (i, 0), T (i + 1, 0), B (i + 1, 0));
			tri (B (i, 0), T (i, 0), T (i + 1, 0));
			// Parois du fond et du profil.
			tri (B (i, 0), B (i + 1, 0), B (i + 1, 1));
			tri (B (i, 0), B (i + 1, 1), B (i, 1));
			tri (T (i + 1, 0), T (i, 0), T (i, 1));
			tri (T (i + 1, 0), T (i, 1), T (i + 1, 1));
		}
		// Parois des deux bouts.
		tri (T (0, 0), B (0, 0), B (0, 1));
		tri (T (0, 0), B (0, 1), T (0, 1));
		tri (B (n - 1, 0), T (n - 1, 0), T (n - 1, 1));
		tri (B (n - 1, 0), T (n - 1, 1), B (n - 1, 1));
	}

	// Tronc de cone vertical : rayon rb a zb, rt a zt.
	void Frustum (double cx, double cy, double zb, double zt, double rb, double rt, int sides)
	{
		const unsigned base = static_cast<unsigned> (positions.size ());
		const unsigned n = static_cast<unsigned> (std::max (3, sides));
		for (unsigned i = 0; i < n; ++i)
		{
			const double t = 2.0 * kPi * i / n;
			Add (cx + rb * std::cos (t), cy + rb * std::sin (t), zb);
		}
		for (unsigned i = 0; i < n; ++i)
		{
			const double t = 2.0 * kPi * i / n;
			Add (cx + rt * std::cos (t), cy + rt * std::sin (t), zt);
		}
		const unsigned bottom = Add (cx, cy, zb), top = Add (cx, cy, zt);
		for (unsigned i = 0; i < n; ++i)
		{
			const unsigned j = (i + 1) % n;
			Tri (base + i, base + j, base + n + j);
			Tri (base + i, base + n + j, base + n + i);
			Tri (bottom, base + j, base + i);
			Tri (top, base + n + i, base + n + j);
		}
	}

	std::unique_ptr<Mesh> Build () const { return MakeTriangleMesh (positions, triangles); }

	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
};

// (a, d) d'une face -> (x, y) monde, en parties.
VolutePoint ToWorld (const ColonnadeLayout &layout, ColonnadeFace face, double a, double d)
{
	switch (face)
	{
	case ColonnadeFace::Front: return {a, -d};
	case ColonnadeFace::Back:  return {a, d};
	case ColonnadeFace::Left:  return {layout.axes.front () - d, a};
	case ColonnadeFace::Right: return {layout.axes.back () + d, a};
	}
	return {};
}

// Profil de face du triglyphe en (a, d), a croissant : canaux en V ouverts
// sur la face (d1), demi-canaux aux bords.
std::vector<VolutePoint> TriglyphProfile (const TriglyphData &t, const OrnamentPlacement &p)
{
	std::vector<VolutePoint> s;
	double a = p.a0;
	const std::size_t n = t.divisionWidths.size ();
	auto push = [&] (double u, double v) {
		if (s.empty () || std::fabs (s.back ().u - u) > 1e-12 || std::fabs (s.back ().v - v) > 1e-12)
			s.push_back ({u, v});
	};
	for (std::size_t i = 0; i < n; ++i)
	{
		const double w = t.divisionWidths[i];
		if (!t.divisionIsChannel[i])
		{
			push (a, p.d1);
			a += w;
			push (a, p.d1);
		}
		else if (i == 0)
		{
			push (a, p.d1 - t.halfChannelDepth);
			a += w;
			push (a, p.d1);
		}
		else if (i + 1 == n)
		{
			a += w;
			push (a, p.d1 - t.halfChannelDepth);
		}
		else
		{
			push (a + 0.5 * w, p.d1 - t.channelDepth);
			a += w;
			push (a, p.d1);
		}
	}
	return s;
}

} // namespace

ColonnadeLayout LayoutColonnade (const OrderData &order, int columns)
{
	if (columns < 1)
		throw std::invalid_argument ("LayoutColonnade : au moins une colonne");
	ColonnadeLayout layout;
	layout.axisSpacing = AxisSpacing (order);
	for (int i = 0; i < columns; ++i)
		layout.axes.push_back ((i - 0.5 * (columns - 1)) * layout.axisSpacing);
	layout.halfLength = 0.5 * (columns - 1) * layout.axisSpacing;
	return layout;
}

const char *OrnamentKindName (OrnamentKind kind)
{
	switch (kind)
	{
	case OrnamentKind::Triglyph:       return "triglyphes";
	case OrnamentKind::Regula:         return "regulae";
	case OrnamentKind::Gutta:          return "gouttes";
	case OrnamentKind::Mutule:         return "mutules";
	case OrnamentKind::MutuleListel:   return "listels des mutules";
	case OrnamentKind::MutuleGutta:    return "gouttes des mutules";
	case OrnamentKind::SoffitGutta:    return "gouttes du plafond";
	case OrnamentKind::Dentil:         return "denticules";
	case OrnamentKind::Modillion:      return "modillons";
	case OrnamentKind::CompositeBlock: return "blocs";
	}
	return "";
}

namespace
{

// Rangees de chaque famille. `out` recoit les placements ajoutes.

std::vector<OrnamentPlacement> PlaceTriglyphs (const OrderData &order, const ColonnadeLayout &layout,
                                               const ColumnProfile &section, std::vector<OrnamentPlacement> &out)
{
	const TriglyphData &t = *order.entablatureOrnaments.triglyph;
	const Block *frieze = order.FindBlock ("frise");
	const ProfileMember &fm = SectionMember (section, "frise", frieze->members.front ().name);
	const double da = frieze->axisDistance;
	const std::vector<OrnamentPlacement> triglyphs = PlaceRow (
		{OrnamentKind::Triglyph, t.width, t.spacing, layout.axes.front (), da, da + t.projection, fm.y0, fm.y1}, layout);
	out.insert (out.end (), triglyphs.begin (), triglyphs.end ());
	return triglyphs;
}

// Regula sous la tenia (membre du haut de l'architrave), du nu de
// l'architrave a la face de la tenia ; gouttes sous la regula.
void PlaceRegulae (const OrderData &order, const ColumnProfile &section, const std::vector<OrnamentPlacement> &triglyphs,
                   std::vector<OrnamentPlacement> &out)
{
	const GuttaeData &g = *order.entablatureOrnaments.guttae;
	const Block *arch = order.FindBlock ("architrave");
	const Member &tenia = arch->members.front ();
	if (tenia.form != MouldingForm::Fillet || tenia.projection.kind != ProjectionKind::Constant)
		throw std::invalid_argument ("tenia attendue en haut de l'architrave : " + order.key);
	const ProfileMember &tm = SectionMember (section, "architrave", tenia.name);
	for (const OrnamentPlacement &t : triglyphs)
	{
		OrnamentPlacement r = t;
		r.kind = OrnamentKind::Regula;
		r.d0 = arch->axisDistance;
		r.d1 = arch->axisDistance + tenia.projection.main.bottom;
		r.z1 = tm.y0;
		r.z0 = tm.y0 - g.regulaHeight;
		out.push_back (r);
		AddGuttae (out, OrnamentKind::Gutta, t, Spread (g.count, g.spacing), {arch->axisDistance + 0.5 * g.diameterBottom},
		           r.z0, g.guttaHeight, 0.5 * g.diameterBottom, 0.5 * g.diameterTop);
	}
}

// Gouttes du plafond de la corniche a denticules, au droit des triglyphes,
// pendues a la sous-face du larmier (membre face de plus grande saillie).
void PlaceSoffitGuttae (const OrderData &order, const ColumnProfile &section,
                        const std::vector<OrnamentPlacement> &triglyphs, std::vector<OrnamentPlacement> &out)
{
	const EntablatureOrnaments &e = order.entablatureOrnaments;
	const Block *c = order.FindBlock ("corniche_denticules");
	const Member *larmier = nullptr;
	for (const Member &m : c->members)
		if (m.form == MouldingForm::Face && m.projection.kind == ProjectionKind::Constant &&
		    (!larmier || m.projection.main.bottom > larmier->projection.main.bottom))
			larmier = &m;
	if (!larmier)
		throw std::invalid_argument ("larmier introuvable : " + order.key);
	const ProfileMember &lm = SectionMember (section, "corniche_denticules", larmier->name);
	const SoffitGuttaeData &s = *e.soffitGuttae;
	std::vector<double> depths;
	for (double d : Spread (s.rows, s.pitchDepth))
		depths.push_back (c->axisDistance + 0.5 * (s.from + s.to) + d);
	for (const OrnamentPlacement &t : triglyphs)
		AddGuttae (out, OrnamentKind::SoffitGutta, t, Spread (s.perRow, s.pitchAlong), depths, lm.y0, e.guttae->guttaHeight,
		           0.5 * s.diameterBottom, 0.5 * e.guttae->diameterTop);
}

// Mutules sur l'axe des triglyphes ; sous chacune, un listel de la meme
// emprise, d'ou pendent les gouttes.
void PlaceMutules (const OrderData &order, const ColonnadeLayout &layout, const ColumnProfile &section,
                   std::vector<OrnamentPlacement> &out)
{
	const EntablatureOrnaments &e = order.entablatureOrnaments;
	const OrnamentMember *link = EntablatureOrnamentMember (order.key, EntablatureOrnament::Mutules);
	const Block *c = order.FindBlock (link->block);
	const ProfileMember &mm = SectionMember (section, link->block, link->member);
	const MutuleData &m = *e.mutules;
	const double da = c->axisDistance;
	const std::vector<OrnamentPlacement> mutules = PlaceRow (
		{OrnamentKind::Mutule, m.width, e.triglyph->spacing, layout.axes.front (), da + m.back, da + m.face, mm.y0, mm.y1},
		layout);
	out.insert (out.end (), mutules.begin (), mutules.end ());
	std::vector<double> depths;
	for (int j = 0; j < m.rows; ++j)
		depths.push_back (da + m.back + m.guttaPitch * (j + 0.5));
	for (const OrnamentPlacement &mu : mutules)
	{
		OrnamentPlacement listel = mu;
		listel.kind = OrnamentKind::MutuleListel;
		listel.z1 = mu.z0;
		listel.z0 = mu.z0 - m.listelHeight;
		out.push_back (listel);
		AddGuttae (out, OrnamentKind::MutuleGutta, mu, Spread (m.perRow, m.guttaPitch), depths, listel.z0, m.guttaHeight,
		           0.5 * m.guttaDiameterBottom, 0.5 * m.guttaDiameterTop);
	}
}

// Denticules : sur les axes (dorique, corinthien : un denticule sous chaque
// modillon), centres sur la colonnade a l'ionique (alignement non donne).
void PlaceDentils (const OrderData &order, const ColonnadeLayout &layout, const ColumnProfile &section,
                   const std::string &corniceKey, std::vector<OrnamentPlacement> &out)
{
	const OrnamentMember *link = EntablatureOrnamentMember (order.key, EntablatureOrnament::Dentils);
	if (!link || corniceKey != link->block)
		return;
	const Block *c = order.FindBlock (link->block);
	const ProfileMember &dm = SectionMember (section, link->block, link->member);
	const DentilData &d = *order.entablatureOrnaments.dentils;
	const double anchor = order.order == Order::Ionic ? 0.0 : layout.axes.front ();
	const std::vector<OrnamentPlacement> row = PlaceRow ({OrnamentKind::Dentil, d.width, d.width + d.gap, anchor,
	                                                      c->axisDistance + d.back, c->axisDistance + d.back + d.depth,
	                                                      dm.y0, dm.y1},
	                                                     layout);
	out.insert (out.end (), row.begin (), row.end ());
}

// Rangee pendue d'un membre lie, de sa face moins `depth` a sa face.
void PlaceHanging (const OrderData &order, const ColonnadeLayout &layout, const ColumnProfile &section,
                   EntablatureOrnament ornament, OrnamentKind kind, double width, double period, double anchor,
                   double depth, std::vector<OrnamentPlacement> &out)
{
	const OrnamentMember *link = EntablatureOrnamentMember (order.key, ornament);
	const Block *c = order.FindBlock (link->block);
	const ProfileMember &pm = SectionMember (section, link->block, link->member);
	const double face = c->axisDistance + LinkedOrnamentMember (order, ornament).projection.main.bottom;
	const std::vector<OrnamentPlacement> row =
		PlaceRow ({kind, width, period, anchor, face - depth, face, pm.y0, pm.y1}, layout);
	out.insert (out.end (), row.begin (), row.end ());
}

} // namespace

std::vector<OrnamentPlacement> PlaceEntablatureOrnaments (const OrderData &order, const ColonnadeLayout &layout,
                                                          const ColumnProfile &section, DoricCornice cornice)
{
	std::vector<OrnamentPlacement> out;
	const EntablatureOrnaments &e = order.entablatureOrnaments;
	const bool doric = order.order == Order::Doric;
	const std::string corniceKey =
		doric ? (cornice == DoricCornice::Mutules ? "corniche_mutules" : "corniche_denticules") : "corniche";

	std::vector<OrnamentPlacement> triglyphs;
	if (e.triglyph)
		triglyphs = PlaceTriglyphs (order, layout, section, out);
	if (e.triglyph && e.guttae)
		PlaceRegulae (order, section, triglyphs, out);
	if (doric && e.soffitGuttae && e.guttae && cornice == DoricCornice::Dentils)
		PlaceSoffitGuttae (order, section, triglyphs, out);
	if (doric && e.mutules && e.triglyph && cornice == DoricCornice::Mutules)
		PlaceMutules (order, layout, section, out);
	if (e.dentils)
		PlaceDentils (order, layout, section, corniceKey, out);
	if (e.modillions)
		PlaceHanging (order, layout, section, EntablatureOrnament::Modillions, OrnamentKind::Modillion,
		              e.modillions->width, e.modillions->spacing, layout.axes.front (), e.modillions->length, out);
	if (e.blocks)
		PlaceHanging (order, layout, section, EntablatureOrnament::Blocks, OrnamentKind::CompositeBlock, e.blocks->width,
		              e.blocks->width + e.blocks->gap, 0.0, e.blocks->depth, out);
	return out;
}

std::vector<std::pair<OrnamentKind, std::unique_ptr<Mesh>>> BuildOrnamentMeshes (
	const OrderData &order, const ColonnadeLayout &layout, const std::vector<OrnamentPlacement> &placements, double z0,
	double scale, int guttaSides)
{
	std::array<Builder, kOrnamentKindCount> builders;
	std::array<bool, kOrnamentKindCount> used {};
	for (const OrnamentPlacement &p : placements)
	{
		const int k = static_cast<int> (p.kind);
		Builder &b = builders[k];
		used[k] = true;
		const double zb = (z0 + p.z0) * scale, zt = (z0 + p.z1) * scale;
		if (p.kind == OrnamentKind::Gutta || p.kind == OrnamentKind::MutuleGutta || p.kind == OrnamentKind::SoffitGutta)
		{
			const VolutePoint c = ToWorld (layout, p.face, p.centre, p.depthCentre);
			// Petite base en haut, contre l'element qui porte la goutte.
			b.Frustum (c.u * scale, c.v * scale, zb, zt, p.radiusBottom * scale, p.radiusTop * scale, guttaSides);
			continue;
		}
		std::vector<VolutePoint> top {{p.a0, p.d1}, {p.a1, p.d1}};
		double back = p.d0;
		if (p.kind == OrnamentKind::Triglyph)
		{
			if (!order.entablatureOrnaments.triglyph)
				throw std::invalid_argument ("triglyphe sans donnees");
			// Les canaux (profondeur 1) sont aussi profonds que la saillie du
			// triglyphe sur la metope (1) : le dos du prisme est engage d'une
			// profondeur de canal dans la frise, pour un solide d'un seul tenant.
			back = p.d0 - order.entablatureOrnaments.triglyph->channelDepth;
			top = TriglyphProfile (*order.entablatureOrnaments.triglyph, p);
		}
		const ColonnadeFace face = p.face;
		b.Section (top, back,
		           [&] (double a, double d) {
			           const VolutePoint w = ToWorld (layout, face, a, d);
			           return VolutePoint {w.u * scale, w.v * scale};
		           },
		           zb, zt);
	}
	std::vector<std::pair<OrnamentKind, std::unique_ptr<Mesh>>> out;
	for (int k = 0; k < kOrnamentKindCount; ++k)
		if (used[k])
			out.emplace_back (static_cast<OrnamentKind> (k), builders[k].Build ());
	return out;
}

} // namespace classical
} // namespace cgarchi
