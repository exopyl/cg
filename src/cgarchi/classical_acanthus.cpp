#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_detail.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <cgarchi/classical_lathe.h>
#include <cgarchi/classical_mesh.h>
#include "mesh.h"

namespace cgarchi::classical
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr const char *kCapitalBlock = "chapiteau";
constexpr const char *kAbacusMember = "B · abaco";
constexpr const char *kAbacusTalonMember = "A · cimatio dell'abaco (talon)";

using Point3 = std::array<double, 3>;
using Triangle = std::array<unsigned, 3>;

// Sommets et faces d'une piece, indices locaux.
struct PieceGeometry
{
	std::vector<Point3>   positions;
	std::vector<Triangle> triangles;
};

// Plage semi-ouverte d'une piece dans un maillage concatene.
struct AppendedRange
{
	unsigned faceBegin = 0, faceEnd = 0;
	unsigned vertexBegin = 0, vertexEnd = 0;
};

// Ajoute `piece` a la fin de `positions` et `triangles`, indices decales. Leve
// std::length_error, au nom de `caller`, au-dela des indices unsigned.
AppendedRange AppendPiece (std::vector<Point3> &positions, std::vector<Triangle> &triangles, const PieceGeometry &piece,
                           const char *caller)
{
	const std::size_t limit = std::numeric_limits<unsigned>::max ();
	if (positions.size () + piece.positions.size () > limit || triangles.size () + piece.triangles.size () > limit)
		throw std::length_error (std::string (caller) + " : maillage au-dela des indices unsigned");
	AppendedRange range;
	const unsigned offset = static_cast<unsigned> (positions.size ());
	range.vertexBegin = offset;
	range.faceBegin = static_cast<unsigned> (triangles.size ());
	positions.insert (positions.end (), piece.positions.begin (), piece.positions.end ());
	for (const Triangle &t : piece.triangles)
		triangles.push_back ({t[0] + offset, t[1] + offset, t[2] + offset});
	range.vertexEnd = static_cast<unsigned> (positions.size ());
	range.faceEnd = static_cast<unsigned> (triangles.size ());
	return range;
}

// Concatene les pieces en un seul tableau de sommets et de faces, et note la
// plage de chacune. AddRotated ecrit directement dans les tableaux la copie de
// `piece` tournee de `angle` autour de l'axe Z du tour : (x, y) tournes en
// double, z et triangles inchanges.
class OrnamentAssembly
{
public:
	void Reserve (std::size_t vertices, std::size_t triangles)
	{
		positions_.reserve (vertices);
		triangles_.reserve (triangles);
	}

	void Add (CapitalPieceKind kind, const PieceGeometry &piece)
	{
		Record (kind, AppendPiece (positions_, triangles_, piece, "BuildCorinthianOrnament"));
	}

	void AddRotated (CapitalPieceKind kind, const PieceGeometry &piece, double angle)
	{
		const std::size_t limit = std::numeric_limits<unsigned>::max ();
		if (positions_.size () + piece.positions.size () > limit || triangles_.size () + piece.triangles.size () > limit)
			throw std::length_error ("BuildCorinthianOrnament : maillage au-dela des indices unsigned");
		const double c = std::cos (angle), s = std::sin (angle);
		AppendedRange range;
		const unsigned offset = static_cast<unsigned> (positions_.size ());
		range.vertexBegin = offset;
		range.faceBegin = static_cast<unsigned> (triangles_.size ());
		for (const Point3 &p : piece.positions)
			positions_.push_back ({c * p[0] - s * p[1], s * p[0] + c * p[1], p[2]});
		for (const Triangle &t : piece.triangles)
			triangles_.push_back ({t[0] + offset, t[1] + offset, t[2] + offset});
		range.vertexEnd = static_cast<unsigned> (positions_.size ());
		range.faceEnd = static_cast<unsigned> (triangles_.size ());
		Record (kind, range);
	}

	detail::OrnamentGeometry Release ()
	{
		detail::OrnamentGeometry geometry;
		geometry.positions = std::move (positions_);
		geometry.triangles = std::move (triangles_);
		geometry.pieces = std::move (pieces_);
		return geometry;
	}

private:
	void Record (CapitalPieceKind kind, const AppendedRange &r)
	{
		CapitalPiece range;
		range.kind = kind;
		range.faceBegin = r.faceBegin;
		range.faceEnd = r.faceEnd;
		range.vertexBegin = r.vertexBegin;
		range.vertexEnd = r.vertexEnd;
		pieces_.push_back (range);
	}

	std::vector<Point3>        positions_;
	std::vector<Triangle>      triangles_;
	std::vector<CapitalPiece> pieces_;
};

// Seul membre du chapiteau de ce nom dans le profil ; `index` recoit son rang
// dans profile.members. Leve std::invalid_argument, au nom de `caller`, s'il
// manque ou s'il figure plusieurs fois.
const ProfileMember &UniqueCapitalMember (const ColumnProfile &profile, const std::string &name, const char *caller,
                                          std::size_t *index = nullptr)
{
	const ProfileMember *found = nullptr;
	for (std::size_t i = 0; i < profile.members.size (); ++i)
	{
		const ProfileMember &member = profile.members[i];
		if (member.block != kCapitalBlock || member.name != name)
			continue;
		if (found)
			throw std::invalid_argument (std::string (caller) + " : membre en double dans le profil : " + name);
		found = &member;
		if (index)
			*index = i;
	}
	if (!found)
		throw std::invalid_argument (std::string (caller) + " : membre absent du profil : " + name);
	return *found;
}

Point3 Cross (const Point3 &a, const Point3 &b)
{
	return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

Point3 Normalize (Point3 v)
{
	const double length = std::sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (length > 1e-12)
		for (double &component : v)
			component /= length;
	return v;
}

bool Close (double a, double b)
{
	return std::fabs (a - b) <= 1e-9 * std::max (1.0, std::fabs (b));
}

double Lerp (const Span &span, double h0, double h1, double h)
{
	return span.bottom + (span.top - span.bottom) * (h - h0) / (h1 - h0);
}

const detail::CapitalEnvelopeBand &BandAt (const std::vector<detail::CapitalEnvelopeBand> &bands, double h)
{
	for (const detail::CapitalEnvelopeBand &band : bands)
		if (h <= band.h1)
			return band;
	return bands.back ();
}

// Rayon, angle et hauteur d'un sommet du maillage, en parties.
struct Cylindrical
{
	double r = 0.0, theta = 0.0, h = 0.0;
};

Cylindrical VertexAt (const Mesh &mesh, const detail::CapitalEnvelope &env, unsigned i, const char *caller)
{
	float v[3] {};
	if (mesh.GetVertex (i, v) != 0)
		throw std::invalid_argument (std::string (caller) + " : sommet " + std::to_string (i) + " hors du maillage");
	const double x = v[0] / env.scale, y = v[1] / env.scale, z = v[2] / env.scale;
	return {std::hypot (x, y), std::atan2 (y, x), z - env.foot};
}

void CheckVertexRange (const Mesh &mesh, unsigned vertexBegin, unsigned vertexEnd, const char *caller)
{
	if (vertexBegin > vertexEnd || vertexEnd > static_cast<unsigned> (mesh.GetNVertices ()))
		throw std::invalid_argument (std::string (caller) + " : plage de sommets hors du maillage");
}

} // namespace

namespace detail
{

double CapitalEnvelope::CoreRadius (double h) const
{
	const CapitalEnvelopeBand &band = BandAt (bands, h);
	return axisDistance + Lerp (band.core, band.h0, band.h1, h);
}

double CapitalEnvelope::ConeRadius (double h) const
{
	const CapitalEnvelopeBand &band = BandAt (bands, h);
	return axisDistance + Lerp (band.envelope, band.h0, band.h1, h);
}

double CapitalEnvelope::EnvelopeRadius (double h, double theta) const
{
	return std::max (CoreRadius (h), std::min (ConeRadius (h), abacus.RadiusAt (theta)));
}

double CapitalEnvelope::LeafBoundRadius (double h) const
{
	return std::max (CoreRadius (h), ConeRadius (h));
}

CapitalEnvelope ReadCapitalEnvelope (const OrderData &order, const ColumnProfile &profile, double scale)
{
	if (!std::isfinite (scale) || !(scale > 0.0))
		throw std::invalid_argument ("ReadCapitalEnvelope : scale fini et > 0 requis");
	const Block *capital = order.FindBlock (kCapitalBlock);
	if (!capital)
		throw std::invalid_argument ("ReadCapitalEnvelope : chapiteau absent de l'ordre " + order.key);
	const auto first = std::find_if (profile.members.begin (), profile.members.end (),
	                                 [] (const ProfileMember &m) { return m.block == kCapitalBlock; });
	if (first == profile.members.end ())
		throw std::invalid_argument ("ReadCapitalEnvelope : chapiteau absent du profil");

	CapitalEnvelope env;
	env.axisDistance = capital->axisDistance;
	env.foot = first->y0;
	env.scale = scale;
	for (const Member &member : capital->members)
	{
		if (member.projection.kind != ProjectionKind::CoreEnvelope)
			continue;
		const ProfileMember &placed = UniqueCapitalMember (profile, member.name, "ReadCapitalEnvelope");
		if (!(placed.y1 > placed.y0))
			throw std::invalid_argument ("ReadCapitalEnvelope : membre de hauteur nulle : " + member.name);
		env.bands.push_back ({placed.y0 - env.foot, placed.y1 - env.foot, member.projection.main, member.projection.envelope});
	}
	if (env.bands.empty ())
		throw std::invalid_argument ("ReadCapitalEnvelope : aucun membre d'ornement au chapiteau de " + order.key);
	std::sort (env.bands.begin (), env.bands.end (),
	           [] (const CapitalEnvelopeBand &a, const CapitalEnvelopeBand &b) { return a.h0 < b.h0; });
	for (std::size_t i = 0; i + 1 < env.bands.size (); ++i)
	{
		const CapitalEnvelopeBand &lo = env.bands[i], &hi = env.bands[i + 1];
		if (!Close (lo.h1, hi.h0) || !Close (lo.core.top, hi.core.bottom) || !Close (lo.envelope.top, hi.envelope.bottom))
			throw std::invalid_argument ("ReadCapitalEnvelope : membres d'ornement non contigus a h = " +
			                             std::to_string (lo.h1));
	}

	std::size_t abacusIndex = 0;
	UniqueCapitalMember (profile, kAbacusMember, "ReadCapitalEnvelope", &abacusIndex);
	bool found = false;
	for (const ProfileVertex &vertex : profile.vertices)
	{
		if (vertex.member != static_cast<int> (abacusIndex))
			continue;
		if (found && vertex.ring != env.abacus)
			throw std::invalid_argument ("ReadCapitalEnvelope : anneau de l'abaque variable en hauteur");
		env.abacus = vertex.ring;
		found = true;
	}
	if (!found)
		throw std::invalid_argument ("ReadCapitalEnvelope : abaque sans sommet dans le profil");

	std::size_t talonIndex = 0;
	UniqueCapitalMember (profile, kAbacusTalonMember, "ReadCapitalEnvelope", &talonIndex);
	double talonTop = -std::numeric_limits<double>::infinity ();
	for (const ProfileVertex &vertex : profile.vertices)
		if (vertex.member == static_cast<int> (talonIndex) && vertex.y > talonTop)
		{
			talonTop = vertex.y;
			env.abacusOutline = vertex.ring;
		}
	if (!std::isfinite (talonTop))
		throw std::invalid_argument ("ReadCapitalEnvelope : talon de l'abaque sans sommet dans le profil");
	return env;
}

double CoreClearance (const Mesh &mesh, const CapitalEnvelope &env, unsigned vertexBegin, unsigned vertexEnd)
{
	CheckVertexRange (mesh, vertexBegin, vertexEnd, "CoreClearance");
	double clearance = std::numeric_limits<double>::infinity ();
	for (unsigned i = vertexBegin; i < vertexEnd; ++i)
	{
		const Cylindrical p = VertexAt (mesh, env, i, "CoreClearance");
		clearance = std::min (clearance, p.r - env.CoreRadius (p.h));
	}
	return clearance;
}

double CoreClearance (const Mesh &mesh, const CapitalEnvelope &env)
{
	return CoreClearance (mesh, env, 0, static_cast<unsigned> (mesh.GetNVertices ()));
}

double CoreClearance (const Mesh &mesh, const CapitalEnvelope &env, const CapitalPiece &piece)
{
	return CoreClearance (mesh, env, piece.vertexBegin, piece.vertexEnd);
}

double EnvelopeExcess (const Mesh &mesh, const CapitalEnvelope &env, unsigned vertexBegin, unsigned vertexEnd)
{
	CheckVertexRange (mesh, vertexBegin, vertexEnd, "EnvelopeExcess");
	double excess = -std::numeric_limits<double>::infinity ();
	for (unsigned i = vertexBegin; i < vertexEnd; ++i)
	{
		const Cylindrical p = VertexAt (mesh, env, i, "EnvelopeExcess");
		excess = std::max (excess, p.r - env.EnvelopeRadius (p.h, p.theta));
	}
	return excess;
}

double EnvelopeExcess (const Mesh &mesh, const CapitalEnvelope &env)
{
	return EnvelopeExcess (mesh, env, 0, static_cast<unsigned> (mesh.GetNVertices ()));
}

double EnvelopeExcess (const Mesh &mesh, const CapitalEnvelope &env, const CapitalPiece &piece)
{
	CheckVertexRange (mesh, piece.vertexBegin, piece.vertexEnd, "EnvelopeExcess");
	if (piece.kind == CapitalPieceKind::Fleuron)
		return -std::numeric_limits<double>::infinity ();
	if (piece.kind != CapitalPieceKind::LowerLeaf && piece.kind != CapitalPieceKind::MiddleLeaf &&
	    piece.kind != CapitalPieceKind::MinorLeaf && piece.kind != CapitalPieceKind::Volute &&
	    piece.kind != CapitalPieceKind::Helix)
		return EnvelopeExcess (mesh, env, piece.vertexBegin, piece.vertexEnd);
	double excess = -std::numeric_limits<double>::infinity ();
	for (unsigned i = piece.vertexBegin; i < piece.vertexEnd; ++i)
	{
		const Cylindrical p = VertexAt (mesh, env, i, "EnvelopeExcess");
		excess = std::max (excess, p.r - env.LeafBoundRadius (p.h));
	}
	return excess;
}

double BellCoverage (const Mesh &mesh, const std::vector<CapitalPiece> &pieces, const CapitalEnvelope &env,
                     double hLo, double hHi, int thetaCells, int hCells)
{
	if (!(hLo < hHi) || thetaCells < 1 || hCells < 1)
		throw std::invalid_argument ("BellCoverage : bande vide ou grille sans cellule");
	const unsigned faceCount = static_cast<unsigned> (mesh.GetNFaces ());
	const double dTheta = 2.0 * kPi / thetaCells;
	const double dH = (hHi - hLo) / hCells;
	std::vector<char> covered (static_cast<std::size_t> (thetaCells) * static_cast<std::size_t> (hCells), 0);
	for (const CapitalPiece &piece : pieces)
	{
		if (piece.faceBegin > piece.faceEnd || piece.faceEnd > faceCount)
			throw std::invalid_argument ("BellCoverage : plage de faces hors du maillage");
		for (unsigned f = piece.faceBegin; f < piece.faceEnd; ++f)
		{
			if (mesh.GetFaceNVertices (f) != 3)
				throw std::invalid_argument ("BellCoverage : face " + std::to_string (f) + " non triangulaire");
			std::array<double, 3> t {}, h {};
			for (int k = 0; k < 3; ++k)
			{
				const Cylindrical p = VertexAt (mesh, env, static_cast<unsigned> (mesh.GetFaceVertex (f, k)), "BellCoverage");
				// Angle deroule autour du premier sommet : une face a cheval sur
				// theta = pi reste d'un seul tenant.
				t[k] = k == 0 ? p.theta : t[0] + std::remainder (p.theta - t[0], 2.0 * kPi);
				h[k] = p.h;
			}
			const double area = (t[1] - t[0]) * (h[2] - h[0]) - (t[2] - t[0]) * (h[1] - h[0]);
			if (std::fabs (area) < 1e-18)
				continue;
			const double hMin = std::min ({h[0], h[1], h[2]}), hMax = std::max ({h[0], h[1], h[2]});
			const double tMin = std::min ({t[0], t[1], t[2]}), tMax = std::max ({t[0], t[1], t[2]});
			const int rowLo = std::max (0, static_cast<int> (std::ceil ((hMin - hLo) / dH - 0.5)));
			const int rowHi = std::min (hCells - 1, static_cast<int> (std::floor ((hMax - hLo) / dH - 0.5)));
			const int colLo = static_cast<int> (std::ceil ((tMin + kPi) / dTheta - 0.5));
			const int colHi = static_cast<int> (std::floor ((tMax + kPi) / dTheta - 0.5));
			const double sign = area > 0.0 ? 1.0 : -1.0;
			for (int row = rowLo; row <= rowHi; ++row)
			{
				const double hc = hLo + (row + 0.5) * dH;
				for (int col = colLo; col <= colHi; ++col)
				{
					const double tc = -kPi + (col + 0.5) * dTheta;
					bool inside = true;
					for (int e = 0; e < 3 && inside; ++e)
					{
						const int a = e, b = (e + 1) % 3;
						inside = sign * ((t[b] - t[a]) * (hc - h[a]) - (tc - t[a]) * (h[b] - h[a])) >= 0.0;
					}
					if (inside)
						covered[static_cast<std::size_t> (row) * thetaCells +
						        static_cast<std::size_t> (((col % thetaCells) + thetaCells) % thetaCells)] = 1;
				}
			}
		}
	}
	const std::size_t count = static_cast<std::size_t> (std::count (covered.begin (), covered.end (), 1));
	return static_cast<double> (count) / static_cast<double> (covered.size ());
}

double VisibleBellFraction (const Mesh &mesh, const std::vector<CapitalPiece> &pieces, const CapitalEnvelope &env,
                            double halfWidth, double hLo, double hHi, double cell)
{
	if (!(halfWidth > 0.0) || !(hLo < hHi) || !(cell > 0.0) || !std::isfinite (halfWidth) || !std::isfinite (hHi - hLo) ||
	    !std::isfinite (cell))
		throw std::invalid_argument ("VisibleBellFraction : fenetre vide ou cellule non > 0");
	const int cols = std::max (1, static_cast<int> (std::lround (2.0 * halfWidth / cell)));
	const int rows = std::max (1, static_cast<int> (std::lround ((hHi - hLo) / cell)));
	const double dy = 2.0 * halfWidth / cols, dh = (hHi - hLo) / rows;
	// Profondeur du noyau par cellule ; une face cache la cellule si elle passe devant.
	std::vector<double> bell (static_cast<std::size_t> (cols) * static_cast<std::size_t> (rows));
	for (int j = 0; j < rows; ++j)
	{
		const double core = env.CoreRadius (hLo + (j + 0.5) * dh);
		if (!(halfWidth < core))
			throw std::invalid_argument ("VisibleBellFraction : fenetre plus large que la corbeille a h " +
			                             std::to_string (hLo + (j + 0.5) * dh));
		for (int i = 0; i < cols; ++i)
		{
			const double y = -halfWidth + (i + 0.5) * dy;
			bell[static_cast<std::size_t> (j) * cols + static_cast<std::size_t> (i)] = std::sqrt (core * core - y * y);
		}
	}
	std::vector<char> hidden (bell.size (), 0);
	const unsigned faceCount = static_cast<unsigned> (mesh.GetNFaces ());
	for (const CapitalPiece &piece : pieces)
	{
		if (piece.faceBegin > piece.faceEnd || piece.faceEnd > faceCount)
			throw std::invalid_argument ("VisibleBellFraction : plage de faces hors du maillage");
		for (unsigned f = piece.faceBegin; f < piece.faceEnd; ++f)
		{
			if (mesh.GetFaceNVertices (f) != 3)
				throw std::invalid_argument ("VisibleBellFraction : face " + std::to_string (f) + " non triangulaire");
			std::array<double, 3> x {}, y {}, h {};
			for (int k = 0; k < 3; ++k)
			{
				const unsigned v = static_cast<unsigned> (mesh.GetFaceVertex (f, k));
				float p[3] {};
				if (mesh.GetVertex (v, p) != 0)
					throw std::invalid_argument ("VisibleBellFraction : sommet " + std::to_string (v) + " hors du maillage");
				x[static_cast<std::size_t> (k)] = p[0] / env.scale;
				y[static_cast<std::size_t> (k)] = p[1] / env.scale;
				h[static_cast<std::size_t> (k)] = p[2] / env.scale - env.foot;
			}
			const double det = (y[1] - y[0]) * (h[2] - h[0]) - (y[2] - y[0]) * (h[1] - h[0]);
			if (std::fabs (det) < 1e-18)
				continue;
			const int i0 = std::max (0, static_cast<int> (std::ceil ((std::min ({y[0], y[1], y[2]}) + halfWidth) / dy - 0.5)));
			const int i1 = std::min (cols - 1, static_cast<int> (std::floor ((std::max ({y[0], y[1], y[2]}) + halfWidth) / dy - 0.5)));
			const int j0 = std::max (0, static_cast<int> (std::ceil ((std::min ({h[0], h[1], h[2]}) - hLo) / dh - 0.5)));
			const int j1 = std::min (rows - 1, static_cast<int> (std::floor ((std::max ({h[0], h[1], h[2]}) - hLo) / dh - 0.5)));
			for (int j = j0; j <= j1; ++j)
			{
				const double hc = hLo + (j + 0.5) * dh;
				for (int i = i0; i <= i1; ++i)
				{
					const double yc = -halfWidth + (i + 0.5) * dy;
					const double w1 = ((yc - y[0]) * (h[2] - h[0]) - (y[2] - y[0]) * (hc - h[0])) / det;
					const double w2 = ((y[1] - y[0]) * (hc - h[0]) - (yc - y[0]) * (h[1] - h[0])) / det;
					const double w0 = 1.0 - w1 - w2;
					if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0)
						continue;
					const std::size_t at = static_cast<std::size_t> (j) * cols + static_cast<std::size_t> (i);
					if (w0 * x[0] + w1 * x[1] + w2 * x[2] > bell[at])
						hidden[at] = 1;
				}
			}
		}
	}
	const std::size_t count = static_cast<std::size_t> (std::count (hidden.begin (), hidden.end (), 1));
	return 1.0 - static_cast<double> (count) / static_cast<double> (hidden.size ());
}

namespace
{

// Valeur d'une rampe de 0 a 1 et ses derivees premiere et seconde en x.
struct Ramp
{
	double value = 0.0, slope = 0.0, bend = 0.0;
};

// Smootherstep de a a b (6 t^5 - 15 t^4 + 10 t^3, classe C2) ; marche en b si
// b <= a.
Ramp Smootherstep (double a, double b, double x)
{
	if (!(b > a))
		return {x >= b ? 1.0 : 0.0, 0.0, 0.0};
	const double length = b - a, t = std::clamp ((x - a) / length, 0.0, 1.0);
	return {t * t * t * (10.0 - 15.0 * t + 6.0 * t * t), 30.0 * t * t * (1.0 - t) * (1.0 - t) / length,
	        60.0 * t * (1.0 - t) * (1.0 - 2.0 * t) / (length * length)};
}

// ---------------------------------------------------------------------------
//  Gardes d'entree
// ---------------------------------------------------------------------------

[[noreturn]] void Fail (const std::string &what)
{
	throw std::invalid_argument ("BuildLeafSheet : " + what);
}

// Leve std::invalid_argument si !ok. `what` : texte, ou fonction qui le rend,
// appelee seulement en cas d'echec.
template <class Message>
void Require (bool ok, const Message &what)
{
	if (ok)
		return;
	if constexpr (std::is_invocable_v<const Message &>)
		Fail (what ());
	else
		Fail (what);
}

// Vrai si x est fini et dans [lo, hi] (faux pour NaN).
bool Within (double x, double lo, double hi)
{
	return x >= lo && x <= hi;
}

void CheckEnvelope (const CapitalEnvelope &env)
{
	Require (std::isfinite (env.scale) && env.scale > 0.0 && std::isfinite (env.foot) && !env.bands.empty (),
	         "enveloppe hors domaine : scale fini et > 0, foot fini, bandes non vides");
}

// ---------------------------------------------------------------------------
//  Maillage des pieces
// ---------------------------------------------------------------------------

// Faces exterieure (indices du plan) et interieure (decalees de `layer`,
// sens inverse) d'une coque.
void AddFaces (std::vector<Triangle> &triangles, const std::vector<Triangle> &planar, unsigned layer)
{
	for (const Triangle &t : planar)
		triangles.push_back (t);
	for (const Triangle &t : planar)
		triangles.push_back ({t[0] + layer, t[2] + layer, t[1] + layer});
}

// Retourne toutes les faces si le volume signe est negatif.
void OrientOutward (PieceGeometry &piece)
{
	double volume = 0.0;
	for (const Triangle &tri : piece.triangles)
	{
		const Point3 &p = piece.positions[tri[0]], &q = piece.positions[tri[1]], &w = piece.positions[tri[2]];
		volume += p[0] * (q[1] * w[2] - q[2] * w[1]) - p[1] * (q[0] * w[2] - q[2] * w[0]) + p[2] * (q[0] * w[1] - q[1] * w[0]);
	}
	if (volume < 0.0)
		for (Triangle &tri : piece.triangles)
			std::swap (tri[1], tri[2]);
}

// Portee maximale d'une recherche de place (p) : au-dela, la place est tenue
// pour illimitee.
constexpr double kMaxReach = 40.0;

// Plus grand d dans [0, min (reach, kMaxReach)] tel que room (d) > 0, par pas
// de 0,02 puis bissection ; 0 si room (0) <= 0 ou si reach n'est pas > 0, la
// portee si room y est encore > 0 et que les pas n'ont pas trouve de fin de
// place avant.
template <class Room>
double LargestStep (const Room &room, double reach)
{
	reach = std::min (reach, kMaxReach);
	if (!(reach > 0.0) || !(room (0.0) > 0.0))
		return 0.0;
	constexpr double kStep = 0.02;
	double lo = 0.0, hi = kStep;
	while (hi <= reach && room (hi) > 0.0)
	{
		lo = hi;
		hi += kStep;
	}
	if (hi > reach)
	{
		if (room (reach) > 0.0)
			return reach;
		hi = reach;
	}
	for (int i = 0; i < 40; ++i)
	{
		const double mid = 0.5 * (lo + hi);
		(room (mid) > 0.0 ? lo : hi) = mid;
	}
	return lo;
}

} // namespace

LeafPlacement DefaultLeafPlacement (CapitalPieceKind kind, double theta)
{
	if (!std::isfinite (theta))
		throw std::invalid_argument ("DefaultLeafPlacement : theta non fini");
	constexpr double kDeg = kPi / 180.0;
	LeafPlacement p;
	p.theta = theta;
	p.footLean = 25.0 * kDeg;
	switch (kind)
	{
	case CapitalPieceKind::LowerLeaf:
		p.hStem = 0.70;
		p.hFoot = 0.70;
		p.hTop = 12.0;
		p.halfWidth = 4.85;
		p.bendRadius = 2.44;
		p.tipTurn = 169.6 * kDeg;
		return p;
	case CapitalPieceKind::MiddleLeaf:
		p.hStem = 0.05;
		p.hFoot = 9.40;
		p.hTop = 24.0;
		p.halfWidth = 5.25;
		p.bendRadius = 3.23;
		p.tipTurn = 172.3 * kDeg;
		p.footLean = 0.0;
		return p;
	default:
		throw std::invalid_argument ("DefaultLeafPlacement : rang sans pose de feuille, F ou E attendu");
	}
}

LeafPlacement DefaultMinorLeafPlacement (const CapitalEnvelope &env, MinorLeafSide side, double stemTheta, int *iterations)
{
	if (!std::isfinite (stemTheta))
		throw std::invalid_argument ("DefaultMinorLeafPlacement : stemTheta non fini");
	constexpr double kDeg = kPi / 180.0;
	// Sens des theta croissants vers l'axe de face le plus proche de la tige ; signe de la pose du cote.
	const double towardFace = std::remainder (stemTheta, 0.5 * kPi) > 0.0 ? -1.0 : 1.0;
	const double sign = side == MinorLeafSide::Inner ? towardFace : -towardFace;
	LeafPlacement p;
	p.hStem = 17.87;
	p.hFoot = 17.87;
	p.footOffset = 0.5;
	p.explicitBend = true;
	double tipOffset = 0.0;
	if (side == MinorLeafSide::Inner)
	{
		p.halfWidth = 2.4;
		p.bendRadius = 1.5;
		p.bendCenter = {18.1, 25.0};
		p.hTop = 26.5;
		p.tipTurn = 160.5 * kDeg;
		tipOffset = 17.0 * kDeg;
	}
	else
	{
		p.halfWidth = 2.3;
		p.bendRadius = 2.0;
		p.bendCenter = {24.7, 26.0};
		p.hTop = 28.0;
		p.tipTurn = 187.6 * kDeg;
		p.bodyHold = 23.1;
		tipOffset = 19.0 * kDeg;
	}
	// Pied : le contour du corps (y <= 1) reste, du cote de l'autre feuille D, a c / r au moins du plan axial de la
	// tige. Axe : cisaillement ancre au pied, alpha tel que l'apex tombe a stemTheta + sign tipOffset. Le meridien ne
	// depend ni de theta ni de alpha (LeafBend::At) : une porteuse sans cisaillement donne, pour chaque point du
	// contour, son rayon r, le rayon r_0 et la longueur developpee S de l'axe a son y ; le point fixe sur (theta,
	// alpha) se resout ensuite sans reconstruire la porteuse.
	const LeafSpec spec = DefaultMinorLeafSpec (OrnamentDetail::Light, side);
	const LeafTemplate &tpl = LeafTemplateOf (LeafStyle::OliveMinor, OrnamentDetail::Light);
	p.theta = stemTheta;
	const LeafBend flat (env, spec, p, tpl);
	const double reach = flat.BodyLength () + flat.BendLength (), apexRadius = flat.Meridian (0.0, 2.0)[0];
	struct Edge
	{
		double lean, width, radius;   // S / r_0, u hw / r, r
	};
	std::vector<Edge> edges;
	for (const std::array<double, 2> &q : tpl.outline)
		if (q[1] <= 1.0)
		{
			const double r = flat.Meridian (q[0], q[1])[0];
			edges.push_back ({q[1] * flat.BodyLength () / flat.Meridian (0.0, q[1])[0], q[0] * p.halfWidth / r, r});
		}
	double deficit = std::numeric_limits<double>::infinity ();
	int iter = 0;
	for (; iter < 50 && !(deficit < 1e-13); ++iter)
	{
		p.axisLean = std::atan ((stemTheta + sign * tipOffset - p.theta) * apexRadius / reach);
		const double tanLean = std::tan (p.axisLean), cosLean = std::cos (p.axisLean);
		deficit = 0.0;
		for (const Edge &e : edges)
		{
			const double theta = p.theta + tanLean * e.lean - sign * e.width / cosLean;
			deficit = std::max (deficit, spec.clearance / e.radius - sign * (theta - stemTheta));
		}
		p.theta += sign * deficit;
	}
	if (!(deficit < 1e-9))
		throw std::invalid_argument ("DefaultMinorLeafPlacement : point fixe du pied non converge (ecart " + std::to_string (deficit) + " rad)");
	if (iterations)
		*iterations = iter;
	p.axisLean = std::atan ((stemTheta + sign * tipOffset - p.theta) * apexRadius / reach);
	return p;
}

std::array<LeafPlacement, 4> DefaultMinorLeafPlacements (const CapitalEnvelope &env)
{
	CheckEnvelope (env);
	// Cle : toutes les cotes de l'enveloppe en parties ; scale et foot n'entrent
	// que dans la conversion vers le repere du tour.
	std::vector<double> key {env.axisDistance};
	key.reserve (20 + 6 * env.bands.size ());
	for (const CapitalEnvelopeBand &band : env.bands)
		key.insert (key.end (), {band.h0, band.h1, band.core.bottom, band.core.top, band.envelope.bottom, band.envelope.top});
	for (const Ring *a : {&env.abacus, &env.abacusOutline})
		key.insert (key.end (), {static_cast<double> (a->shape), a->radius, a->face, static_cast<double> (a->flutes), a->halfAngle,
		                         static_cast<double> (a->section), a->depthRatio, a->centralRadiusRatio});

	struct Entry
	{
		std::vector<double> key;
		std::array<LeafPlacement, 4> poses;
	};
	constexpr std::size_t kCapacity = 8;
	static std::mutex mutex;
	static std::vector<Entry> cache;
	std::lock_guard<std::mutex> lock (mutex);
	for (const Entry &entry : cache)
		if (entry.key == key)
			return entry.poses;
	std::array<LeafPlacement, 4> poses;
	for (int k = 0; k < 2; ++k)
		for (int side = 0; side < 2; ++side)
			poses[static_cast<std::size_t> (2 * k + side)] =
			    DefaultMinorLeafPlacement (env, side == 0 ? MinorLeafSide::Inner : MinorLeafSide::Outer,
			                               RankTheta (CapitalPieceKind::Cauliculus, k));
	if (cache.size () == kCapacity)
		cache.erase (cache.begin ());
	cache.push_back ({std::move (key), poses});
	return poses;
}

LeafSpec DefaultMinorLeafSpec (OrnamentDetail detail, MinorLeafSide side)
{
	LeafSpec spec = DefaultLeafSpec (detail, LeafStyle::OliveMinor);
	spec.reliefScale = side == MinorLeafSide::Inner ? 0.35 : 0.30;
	return spec;
}

double RankTheta (CapitalPieceKind kind, int k)
{
	if (k < 0 || k >= kLeavesPerRank)
		throw std::invalid_argument ("RankTheta : k " + std::to_string (k) + " hors de [0, " + std::to_string (kLeavesPerRank) + ")");
	const double step = 2.0 * kPi / kLeavesPerRank;
	switch (kind)
	{
	case CapitalPieceKind::LowerLeaf:
	case CapitalPieceKind::Cauliculus:
		return 0.5 * step + k * step;
	case CapitalPieceKind::MiddleLeaf:
		return k * step;
	default:
		throw std::invalid_argument ("RankTheta : rang sans pieces en couronne, F, E ou tige de caulicole attendu");
	}
}

LeafBend::LeafBend (const CapitalEnvelope &env, const LeafSpec &spec, const LeafPlacement &placement,
                    const LeafTemplate &tpl)
	: env_ (env), spec_ (spec), placement_ (placement)
{
	CheckEnvelope (env);
	const auto pose = [] (bool ok, const char *name) { Require (ok, [name] { return std::string ("pose hors domaine : ") + name; }); };
	const LeafPlacement &p = placement_;
	pose (std::isfinite (p.theta), "theta fini");
	pose (std::isfinite (p.hStem) && std::isfinite (p.hFoot) && p.hStem <= p.hFoot, "hStem et hFoot finis, hStem <= hFoot");
	pose (std::isfinite (p.hTop) && p.hTop > p.hFoot, "hTop fini et > hFoot");
	pose (std::isfinite (p.halfWidth) && p.halfWidth > 0.0, "halfWidth fini et > 0");
	pose (std::isfinite (p.bendRadius) && p.bendRadius > 0.0, "bendRadius fini et > 0");
	pose (p.tipTurn > 0.0 && p.tipTurn < 2.0 * kPi, "tipTurn dans ]0, 2 pi[");
	pose (Within (p.footLean, 0.0, 0.5 * kPi) && p.footLean < 0.5 * kPi, "footLean dans [0, pi/2[");
	pose (std::isfinite (p.footOffset) && p.footOffset >= 0.0, "footOffset fini et >= 0");
	pose (std::isfinite (p.bodyHold) && p.bodyHold >= 0.0, "bodyHold fini et >= 0");
	pose (p.footOffset == 0.0 || !(p.hStem < p.hFoot), "footOffset nul avec une bande de tige");
	pose (std::fabs (p.axisLean) < kPi / 3.0, "axisLean dans ]-60, 60[ degres");
	pose (!p.explicitBend || (std::isfinite (p.bendCenter[0]) && std::isfinite (p.bendCenter[1])), "bendCenter fini");
	const auto shape = [] (bool ok, const char *name) { Require (ok, [name] { return std::string ("forme hors domaine : ") + name; }); };
	shape (std::isfinite (spec.thickness) && spec.thickness > 0.0, "thickness fini et > 0");
	shape (std::isfinite (spec.clearance) && spec.clearance >= 0.0, "clearance fini et >= 0");
	shape (std::isfinite (spec.margin) && spec.margin >= 0.0, "margin fini et >= 0");
	Require (tpl.leafVertices > 0 && tpl.leafVertices <= tpl.vertices.size () && std::isfinite (tpl.backHeight) &&
	             tpl.backHeight >= 0.0,
	         "gabarit hors domaine : prefixe de la feuille vide ou trop long, ou backHeight negatif");
	Require (tpl.capHalfWidth > 0.0, "gabarit hors domaine : aucun sommet sur le bec (y > 1)");
	capHalfWidth_ = tpl.capHalfWidth;
	domeStart_ = std::max (1.0, tpl.fade[1]);
	Require (domeStart_ < CrestY (), "gabarit hors domaine : effacement du relief au-dela de la crete");
	gutter_ = spec.gutter;
	Require (Within (gutter_, 0.0, 1.0) && gutter_ < 1.0, "forme hors domaine : gutter dans [0, 1[");
	for (unsigned i = 0; i < tpl.leafVertices; ++i)
		if (tpl.vertices[i].y >= 0.0 && tpl.vertices[i].y <= 1.0)
			bodyHalfWidth_ = std::max (bodyHalfWidth_, tpl.vertices[i].u);
	Require (bodyHalfWidth_ > 0.0, "gabarit hors domaine : corps sans largeur");

	const double t = spec.thickness, back = 0.5 * t + tpl.backHeight;
	// H plaque (reliefScale H du gabarit) par tranches de y sur [0, 2] : la garde de courbure exige
	// 2 t + H au point.
	Require (std::isfinite (spec.reliefScale) && spec.reliefScale >= 0.0, "forme hors domaine : reliefScale fini et >= 0");
	const int bins = static_cast<int> (reliefBins_.size ());
	for (unsigned i = 0; i < tpl.leafVertices; ++i)
	{
		const LeafTemplateVertex &v = tpl.vertices[i];
		const int k = std::clamp (static_cast<int> (v.y * 0.5 * bins), 0, bins - 1);
		const double relief = spec.reliefScale * v.H;
		reliefBins_[static_cast<std::size_t> (k)] = std::max (reliefBins_[static_cast<std::size_t> (k)], relief);
	}

	radius_ = p.bendRadius - back;
	Require (radius_ > 0.0, "rayon de courbure : extrados du bec sous t/2 + Hb");
	constexpr int kArc = 160;
	// Ecart de l'extrados de l'arc (centre (offB_ + radius_, hB_)) sous la borne, minimum sur l'arc.
	const auto room = [&] {
		double off = std::numeric_limits<double>::infinity ();
		for (int i = 0; i <= kArc; ++i)
		{
			const double phi = p.tipTurn * i / kArc, c = std::cos (phi), s = std::sin (phi);
			const double h = hB_ + radius_ * s;
			off = std::min (off, env.LeafBoundRadius (h + back * s) - spec.margin - Base (h) - (offB_ + radius_ * (1.0 - c) - back * c));
		}
		return off;
	};
	if (p.explicitBend)
	{
		hB_ = p.bendCenter[1];
		offB_ = p.bendCenter[0] - radius_ - Base (hB_);
		Require (offB_ >= 0.0, "pose explicite : centre du bec sous le noyau (off_b < 0)");
		const double left = room ();
		Require (left >= 0.0, [&] { return "pose explicite au-dessus de la borne de " + std::to_string (-left) + " p"; });
	}
	Require (p.explicitBend || p.hTop - radius_ > p.hFoot, "bec plus bas que le pied du feuillage");
	for (int iter = 0; !p.explicitBend; ++iter)
	{
		hB_ = p.hTop - radius_;
		double off = std::numeric_limits<double>::infinity ();
		for (int i = 0; i <= kArc; ++i)
		{
			const double phi = p.tipTurn * i / kArc, c = std::cos (phi), s = std::sin (phi);
			const double h = hB_ + radius_ * s;
			off = std::min (off, env.LeafBoundRadius (h + back * s) - spec.margin - Base (h) -
			                         (radius_ * (1.0 - c) - back * c));
		}
		offB_ = off;
		if (offB_ >= 0.0)
			break;
		Require (iter < 400, "aucune place pour le bec sous la borne");
		radius_ *= 0.98;
	}
	Require (radius_ >= 2.0 * t + ReliefAt (1.0), [&] {
		return "rayon de courbure du bec " + std::to_string (radius_) + " < 2 t + H = " + std::to_string (2.0 * t + ReliefAt (1.0));
	});
	Require (hB_ > p.hFoot, "bec plus bas que le pied du feuillage");

	const double lift = 0.4 * (hB_ - p.hFoot);
	bezier_ = {{{p.footOffset, p.hFoot},
	            {p.footOffset + lift * std::sin (p.footLean), p.hFoot + lift * std::cos (p.footLean)},
	            {offB_, hB_ - lift},
	            {offB_, hB_}}};
	constexpr int kBody = 256;
	bodyT_.resize (kBody + 1);
	bodyS_.assign (kBody + 1, 0.0);
	std::array<double, 2> prev {};
	for (int i = 0; i <= kBody; ++i)
	{
		const double tt = static_cast<double> (i) / kBody, u = 1.0 - tt;
		const std::array<double, 2> q {
			u * u * u * bezier_[0][0] + 3 * u * u * tt * bezier_[1][0] + 3 * u * tt * tt * bezier_[2][0] + tt * tt * tt * bezier_[3][0],
			u * u * u * bezier_[0][1] + 3 * u * u * tt * bezier_[1][1] + 3 * u * tt * tt * bezier_[2][1] + tt * tt * tt * bezier_[3][1]};
		bodyT_[static_cast<std::size_t> (i)] = tt;
		if (i > 0)
			bodyS_[static_cast<std::size_t> (i)] = bodyS_[static_cast<std::size_t> (i) - 1] + std::hypot (q[0] - prev[0], q[1] - prev[1]);
		prev = q;
	}
	bodyLength_ = bodyS_.back ();
	if (p.bodyHold > 0.0)
	{
		Require (p.explicitBend && p.footLean == 0.0, "corps tenu : pose explicite et footLean nul requis");
		Require (p.bodyHold > p.hFoot && p.bodyHold < hB_, "corps tenu : bodyHold dans ]hFoot, h_b[");
		holdRadius_ = 0.5 * (hB_ - p.bodyHold);
		holdRun_ = offB_ - p.footOffset - 2.0 * holdRadius_;
		Require (holdRun_ >= 0.0, [&] {
			return "corps tenu : depart du bec a " + std::to_string (offB_ - p.footOffset) + " p du pied, moins que deux rayons de coude " +
			       std::to_string (2.0 * holdRadius_);
		});
		bodyLength_ = (p.bodyHold - p.hFoot) + kPi * holdRadius_ + holdRun_;
	}
	// Corps sous la borne : l'extrados du meridien d'axe (S a t/2 + Hb_ref vers l'exterieur) reste sous
	// LeafBoundRadius - margin, comme celui du bec.
	for (int i = 0; i <= kBody; ++i)
	{
		const std::array<double, 2> q = PlaneJet (static_cast<double> (i) / kBody)[0];
		const double over = Base (q[1]) + q[0] + back - (env.LeafBoundRadius (q[1]) - spec.margin);
		Require (over <= 1e-9, [&] {
			return "corps au-dessus de la borne de " + std::to_string (over) + " p a h " + std::to_string (q[1]);
		});
	}

	const LeafCurvatureMargin guard = CurvatureMargin ();
	Require (guard.margin >= 0.0, [&] {
		return "rayon de courbure " + std::to_string (guard.radius) + " < 2 t + H = " + std::to_string (guard.need) +
		       " au meridien u = " + std::to_string (guard.u) + ", y = " + std::to_string (guard.y);
	});
}

LeafCurvatureMargin LeafBend::CurvatureMargin (int samples) const
{
	Require (samples >= 2, "CurvatureMargin : au moins 2 echantillons par intervalle");
	constexpr double kCorner = 1e-6;   // rad : au-dela, les tangentes de part et d'autre d'un raccord font un coin
	const double t = spec_.thickness, b = bodyHalfWidth_, c = capHalfWidth_, crest = CrestY ();
	LeafCurvatureMargin worst;
	worst.margin = std::numeric_limits<double>::infinity ();
	const auto consider = [&] (double u, double y, double radius, double need) {
		if (radius - need < worst.margin)
			worst = {radius - need, radius, need, u, y};
	};
	// Tangentes de part et d'autre du raccord en y (0 : bande / corps ; 1 : corps / arc).
	const auto corner = [&] (double u, double y) {
		const std::array<double, 2> a = MeridianJet (u, std::nextafter (y, -2.0))[1];
		const std::array<double, 2> d = MeridianJet (u, y >= 1.0 ? std::nextafter (y, 3.0) : y)[1];
		if (std::atan2 (std::fabs (a[0] * d[1] - a[1] * d[0]), a[0] * d[0] + a[1] * d[1]) > kCorner)
			consider (u, y, 0.0, 2.0 * t + ReliefAt (y));
	};
	const std::array<double, 5> bodyMeridians {0.0, 0.25 * b, 0.5 * b, 0.75 * b, b};
	const std::array<double, 3> capMeridians {0.0, 0.5 * c, c};
	for (const double u : bodyMeridians)
	{
		corner (u, 1.0);
		if (StemLength () > 0.0)
			corner (u, 0.0);
	}
	for (const double u : capMeridians)
		corner (u, 1.0);

	const int bins = static_cast<int> (reliefBins_.size ());
	const auto run = [&] (double y0, double y1, double u) {
		std::vector<double> cuts {y0, y1};
		for (int k = 1; k < bins; ++k)
			cuts.push_back (2.0 * k / bins);
		cuts.push_back (domeStart_);
		cuts.push_back (crest);
		std::sort (cuts.begin (), cuts.end ());
		cuts.erase (std::unique (cuts.begin (), cuts.end ()), cuts.end ());
		cuts.erase (std::remove_if (cuts.begin (), cuts.end (), [&] (double x) { return x < y0 || x > y1; }), cuts.end ());
		for (std::size_t k = 1; k < cuts.size (); ++k)
		{
			const double lo = cuts[k - 1], hi = cuts[k];
			int best = 0;
			double kBest = -1.0;
			for (int i = 0; i <= samples; ++i)
			{
				const double kappa = MeridianCurvature (u, lo + (hi - lo) * i / samples);
				if (kappa > kBest)
				{
					kBest = kappa;
					best = i;
				}
			}
			// Section doree sur les deux pas voisins du meilleur echantillon.
			constexpr double kGolden = 0.6180339887498949;
			double a = lo + (hi - lo) * std::max (0, best - 1) / samples, d = lo + (hi - lo) * std::min (samples, best + 1) / samples;
			double yBest = lo + (hi - lo) * best / samples;
			double x1 = d - kGolden * (d - a), x2 = a + kGolden * (d - a);
			double k1 = MeridianCurvature (u, x1), k2 = MeridianCurvature (u, x2);
			for (int i = 0; i < 60; ++i)
			{
				if (k1 > k2)
				{
					d = x2;
					x2 = x1;
					k2 = k1;
					x1 = d - kGolden * (d - a);
					k1 = MeridianCurvature (u, x1);
				}
				else
				{
					a = x1;
					x1 = x2;
					k1 = k2;
					x2 = a + kGolden * (d - a);
					k2 = MeridianCurvature (u, x2);
				}
			}
			if (std::max (k1, k2) > kBest)
			{
				kBest = std::max (k1, k2);
				yBest = k1 > k2 ? x1 : x2;
			}
			const double radius = kBest > 0.0 ? 1.0 / kBest : std::numeric_limits<double>::infinity ();
			consider (u, yBest, radius, 2.0 * t + ReliefAt (0.5 * (lo + hi)));
		}
	};
	for (const double u : bodyMeridians)
		run (0.0, 1.0, u);
	for (const double u : capMeridians)
		run (1.0, 2.0, u);
	return worst;
}

double LeafBend::MeridianCurvature (double u, double y) const
{
	const Jet m = MeridianJet (u, y);
	const double speed = std::hypot (m[1][0], m[1][1]);
	return speed > 0.0 ? std::fabs (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / (speed * speed * speed) : 0.0;
}

double LeafBend::Base (double h) const
{
	return env_.CoreRadius (h) + spec_.clearance + 0.5 * spec_.thickness;
}

double LeafBend::CoreSlope (double h) const
{
	const CapitalEnvelopeBand &band = BandAt (env_.bands, h);
	return (band.core.top - band.core.bottom) / (band.h1 - band.h0);
}

double LeafBend::ReliefAt (double y) const
{
	const int bins = static_cast<int> (reliefBins_.size ());
	const int k = std::clamp (static_cast<int> (y * 0.5 * bins), 0, bins - 1);
	double h = 0.0;
	for (int j = std::max (0, k - 1); j <= std::min (bins - 1, k + 1); ++j)
		h = std::max (h, reliefBins_[static_cast<std::size_t> (j)]);
	return h;
}

LeafBend::Jet LeafBend::PlaneJet (double y) const
{
	if (y < 0.0)
		return {{{0.0, placement_.hFoot + std::max (-1.0, y) * StemLength ()}, {0.0, y >= -1.0 ? StemLength () : 0.0}, {0.0, 0.0}}};
	if (y <= 1.0 && placement_.bodyHold > 0.0)
	{
		// Corps tenu : droite verticale jusqu'a bodyHold, coude d'un quart de tour vers l'exterieur, palier
		// horizontal, coude d'un quart de tour vers le haut jusqu'au depart du bec ; abscisse curviligne s = y L.
		const double L = bodyLength_, rho = holdRadius_, off0 = placement_.footOffset, hold = placement_.bodyHold;
		const double rise = hold - placement_.hFoot, turn = 0.5 * kPiValue * rho;
		double s = y * L;
		const auto jet = [L] (std::array<double, 2> point, double tx, double ty, double kappa) {
			return Jet {{point, {L * tx, L * ty}, {-L * L * kappa * ty, L * L * kappa * tx}}};
		};
		if (s <= rise)
			return jet ({off0, placement_.hFoot + s}, 0.0, 1.0, 0.0);
		s -= rise;
		if (s <= turn)
		{
			const double phi = s / rho;
			return jet ({off0 + rho * (1.0 - std::cos (phi)), hold + rho * std::sin (phi)}, std::sin (phi), std::cos (phi), -1.0 / rho);
		}
		s -= turn;
		if (s <= holdRun_)
			return jet ({off0 + rho + s, hold + rho}, 1.0, 0.0, 0.0);
		const double psi = std::min (s - holdRun_, turn) / rho;
		return jet ({offB_ - rho + rho * std::sin (psi), hB_ - rho * std::cos (psi)}, std::cos (psi), std::sin (psi), 1.0 / rho);
	}
	if (y <= 1.0)
	{
		const double s = y * bodyLength_;
		const auto it = std::lower_bound (bodyS_.begin (), bodyS_.end (), s);
		const std::size_t k = std::clamp<std::size_t> (static_cast<std::size_t> (it - bodyS_.begin ()), 1, bodyS_.size () - 1);
		const double w = (s - bodyS_[k - 1]) / std::max (1e-300, bodyS_[k] - bodyS_[k - 1]);
		const double tt = bodyT_[k - 1] + w * (bodyT_[k] - bodyT_[k - 1]), u = 1.0 - tt;
		const std::array<double, 2> point {
			u * u * u * bezier_[0][0] + 3 * u * u * tt * bezier_[1][0] + 3 * u * tt * tt * bezier_[2][0] + tt * tt * tt * bezier_[3][0],
			u * u * u * bezier_[0][1] + 3 * u * u * tt * bezier_[1][1] + 3 * u * tt * tt * bezier_[2][1] + tt * tt * tt * bezier_[3][1]};
		// Derivees de la Bezier en son parametre, puis en y le long de l'abscisse curviligne (s = y L).
		std::array<double, 2> d1 {}, d2 {};
		for (std::size_t i = 0; i < 2; ++i)
		{
			d1[i] = 3 * u * u * (bezier_[1][i] - bezier_[0][i]) + 6 * u * tt * (bezier_[2][i] - bezier_[1][i]) +
			        3 * tt * tt * (bezier_[3][i] - bezier_[2][i]);
			d2[i] = 6 * u * (bezier_[2][i] - 2 * bezier_[1][i] + bezier_[0][i]) + 6 * tt * (bezier_[3][i] - 2 * bezier_[2][i] + bezier_[1][i]);
		}
		const double speed = std::hypot (d1[0], d1[1]), L = bodyLength_;
		const double kappa = (d1[0] * d2[1] - d1[1] * d2[0]) / (speed * speed * speed);
		const double tx = d1[0] / speed, ty = d1[1] / speed;
		return {{point, {L * tx, L * ty}, {-L * L * kappa * ty, L * L * kappa * tx}}};
	}
	const double phi = (std::min (y, 2.0) - 1.0) * placement_.tipTurn;
	const double rate = y <= 2.0 ? placement_.tipTurn : 0.0, c = std::cos (phi), s = std::sin (phi);
	return {{{offB_ + radius_ * (1.0 - std::cos (phi)), hB_ + radius_ * std::sin (phi)},
	         {radius_ * rate * s, radius_ * rate * c},
	         {radius_ * rate * rate * c, -radius_ * rate * rate * s}}};
}

std::array<double, 3> LeafBend::Dome (double u, double y) const
{
	if (y <= 1.0)
		return {0.0, 0.0, 0.0};
	const double xi = u / capHalfWidth_;
	const double amplitude = kDome * capHalfWidth_ * placement_.halfWidth * xi * xi;
	const Ramp r = Smootherstep (domeStart_, CrestY (), y);
	return {amplitude * r.value, amplitude * r.slope, amplitude * r.bend};
}

std::array<double, 3> LeafBend::Gutter (double u, double y) const
{
	const double crest = CrestY ();
	if (y >= crest)
		return {1.0, 0.0, 0.0};
	const double ue = std::min (1.0, std::fabs (u) / bodyHalfWidth_), inside = 1.0 - ue * ue;
	if (y <= domeStart_)
		return {1.0 - gutter_ * inside * inside, 0.0, 0.0};
	const Ramp r = Smootherstep (domeStart_, crest, y);
	const double w = 1.0 - r.value, depth = gutter_ * inside * inside;
	return {1.0 - w * depth, depth * r.slope, depth * r.bend};
}

LeafBend::Jet LeafBend::MeridianJet (double u, double y) const
{
	const Jet plane = PlaneJet (y);
	const std::array<double, 3> g = Gutter (u, y), d = Dome (u, y);
	double q0 = plane[0][0] * g[0], q1 = plane[0][1];
	double q0p = plane[1][0] * g[0] + plane[0][0] * g[1], q1p = plane[1][1];
	double q0pp = plane[2][0] * g[0] + 2.0 * plane[1][0] * g[1] + plane[0][0] * g[2], q1pp = plane[2][1];
	if (y > 1.0)
	{
		const double phi = (std::min (y, 2.0) - 1.0) * placement_.tipTurn;
		const double rate = y <= 2.0 ? placement_.tipTurn : 0.0, c = std::cos (phi), s = std::sin (phi);
		if (d[0] != 0.0)
		{
			q0 += d[0] * std::cos (phi);
			q1 -= d[0] * std::sin (phi);
		}
		q0p += d[1] * c - d[0] * s * rate;
		q0pp += d[2] * c - 2.0 * d[1] * s * rate - d[0] * c * rate * rate;
		q1p -= d[1] * s + d[0] * c * rate;
		q1pp -= d[2] * s + 2.0 * d[1] * c * rate - d[0] * s * rate * rate;
	}
	const double slope = CoreSlope (q1);
	return {{{Base (q1) + q0, q1}, {slope * q1p + q0p, q1p}, {slope * q1pp + q0pp, q1pp}}};
}

std::array<double, 2> LeafBend::Meridian (double u, double y) const
{
	return MeridianJet (u, y)[0];
}

std::array<double, 2> LeafBend::BackNormal (double u, double y) const
{
	constexpr double kStep = 1e-4;
	const double yMin = StemLength () > 0.0 ? -1.0 : 0.0;
	const double lo = std::max (yMin, std::min (y, 2.0) - kStep), hi = std::min (2.0, std::max (y, yMin) + kStep);
	const std::array<double, 2> a = Meridian (u, lo), b = Meridian (u, hi);
	const double dr = b[0] - a[0], dh = b[1] - a[1], n = std::hypot (dr, dh);
	return {-dh / n, dr / n};
}

double LeafBend::Developed (double y) const
{
	if (y < 0.0)
		return std::max (-1.0, y) * StemLength ();
	if (y <= 1.0)
		return y * bodyLength_;
	return bodyLength_ + radius_ * placement_.tipTurn * (std::min (y, 2.0) - 1.0);
}

std::array<double, 3> LeafBend::At (double u, double y) const
{
	const std::array<double, 2> q = Meridian (u, y);
	double phi = placement_.theta + u * placement_.halfWidth / q[0];
	if (placement_.axisLean != 0.0)
	{
		const double axisRadius = Meridian (0.0, y)[0];
		phi = placement_.theta + std::tan (placement_.axisLean) * Developed (y) / axisRadius +
		      u * placement_.halfWidth / (std::cos (placement_.axisLean) * q[0]);
	}
	return {q[0] * std::cos (phi), q[0] * std::sin (phi), q[1]};
}

double LeafBend::Free (const std::array<double, 3> &s, const std::array<double, 3> &dir, double reach) const
{
	return LargestStep ([&] (double d) {
		const double x = s[0] + d * dir[0], y = s[1] + d * dir[1], h = s[2] + d * dir[2];
		return env_.LeafBoundRadius (h) - spec_.margin - 0.5 * spec_.thickness - std::hypot (x, y);
	}, reach);
}

double LeafBend::CoreRoom (const std::array<double, 3> &s, const std::array<double, 3> &dir, double reach) const
{
	return LargestStep ([&] (double d) {
		const double x = s[0] + d * dir[0], y = s[1] + d * dir[1], h = s[2] + d * dir[2];
		return std::hypot (x, y) - env_.CoreRadius (h) - spec_.clearance - 0.5 * spec_.thickness;
	}, reach);
}

void CheckFaceFold (const std::vector<Point3> &positions, const std::vector<Point3> &carrier, const std::vector<Triangle> &triangles,
                    const std::vector<Point3> &carrierNormals, const std::vector<std::array<double, 4>> &planar, const char *face)
{
	const auto side = [&] (const std::vector<Point3> &p, const Triangle &t) {
		const Point3 &a = p[t[0]], &b = p[t[1]], &c = p[t[2]];
		const Point3 n = Cross ({b[0] - a[0], b[1] - a[1], b[2] - a[2]}, {c[0] - a[0], c[1] - a[1], c[2] - a[2]});
		double dot = 0.0;
		for (const unsigned v : t)
			dot += n[0] * carrierNormals[v][0] + n[1] * carrierNormals[v][1] + n[2] * carrierNormals[v][2];
		return dot;
	};
	for (std::size_t k = 0; k < triangles.size (); ++k)
	{
		const double onS = side (carrier, triangles[k]), onFace = side (positions, triangles[k]);
		Require (onS * onFace > 0.0, [&] {
			const unsigned a = triangles[k][0];
			return std::string ("pli de la face ") + face + " : triangle " + std::to_string (k) + " retourne par rapport a S pres de (X " +
			       std::to_string (planar[a][0]) + ", y " + std::to_string (planar[a][1]) + ")";
		});
	}
}

namespace
{

// Feuille plaquee en double, avant la conversion en float : coque (repere du
// tour, unites monde, pied et echelle compris) et metadonnees de LeafSheet.
struct LeafShell
{
	PieceGeometry geometry;
	std::vector<std::array<double, 4>> planar;
	std::vector<int> faceRegion;
	unsigned faceVertices = 0;
	unsigned faceTriangles = 0;
	unsigned stemVertices = 0;
};

// Construction de BuildLeafSheet, memes gardes, sans le maillage float.
LeafShell BuildLeafShell (const CapitalEnvelope &env, const LeafSpec &spec, const LeafPlacement &placement,
                          const LeafTemplate &tpl)
{
	CheckEnvelope (env);
	Require (tpl.style == spec.style && tpl.detail == spec.detail, "gabarit d'un autre style ou d'un autre niveau que la forme");
	Require (std::isfinite (spec.reliefScale) && spec.reliefScale >= 0.0, "forme hors domaine : reliefScale fini et >= 0");
	Require (spec.reliefShare > 0.0 && spec.reliefShare <= 1.0, "forme hors domaine : reliefShare dans ]0, 1]");
	const LeafBend bend (env, spec, placement, tpl);
	const bool withStem = placement.hStem < placement.hFoot;
	const unsigned nv = withStem ? static_cast<unsigned> (tpl.vertices.size ()) : tpl.leafVertices;
	const unsigned nt = withStem ? static_cast<unsigned> (tpl.triangles.size ()) : tpl.leafTriangles;
	const LeafTemplateRim &rim = withStem ? tpl.stemRim : tpl.leafRim;

	// Miroir u -> -u : un sommet de l'axe est partage, les autres sont doubles.
	std::vector<unsigned> mirror (nv);
	unsigned faceVertices = nv;
	for (unsigned i = 0; i < nv; ++i)
		mirror[i] = tpl.vertices[i].u == 0.0 ? i : faceVertices++;

	const double half = 0.5 * spec.thickness, s = env.scale, psi = spec.reliefShare;
	const double yMin = withStem ? -1.0 : 0.0;
	std::vector<std::array<double, 4>> planar (faceVertices);
	std::vector<Point3> outer (faceVertices), inner (faceVertices), normals (faceVertices), carrier (faceVertices);
	unsigned stemVertices = 0;
	for (unsigned i = 0; i < nv; ++i)
		for (const double side : {1.0, -1.0})
		{
			const LeafTemplateVertex &v = tpl.vertices[i];
			Require (v.H >= 0.0 && v.Hb >= 0.0, [&] { return "gabarit hors domaine : relief negatif au sommet " + std::to_string (i); });
			if (side < 0.0 && v.u == 0.0)
				continue;
			const unsigned f = side > 0.0 ? i : mirror[i];
			const double X = side * v.u * placement.halfWidth;
			const double u = X / placement.halfWidth, y = v.y;
			const Point3 p = bend.At (u, y);
			constexpr double kStep = 1e-4;
			const double yLo = std::max (yMin, y - kStep), yHi = std::min (2.0, y + kStep);
			const Point3 pu0 = bend.At (u - kStep, y), pu1 = bend.At (u + kStep, y), py0 = bend.At (u, yLo), py1 = bend.At (u, yHi);
			Point3 n = Normalize (Cross ({pu1[0] - pu0[0], pu1[1] - pu0[1], pu1[2] - pu0[2]},
			                             {py1[0] - py0[0], py1[1] - py0[1], py1[2] - py0[2]}));
			const std::array<double, 2> back = bend.BackNormal (u, y);
			const double phi = std::atan2 (p[1], p[0]);
			const Point3 front {-back[0] * std::cos (phi), -back[0] * std::sin (phi), -back[1]};
			if (n[0] * front[0] + n[1] * front[1] + n[2] * front[2] < 0.0)
				n = {-n[0], -n[1], -n[2]};
			const double Hwanted = spec.reliefScale * v.H, Hbwanted = spec.reliefScale * v.Hb;
			// Une place au moins egale a la portee laisse le relief voulu tel quel.
			const double reachH = Hwanted / psi, freeH = bend.Free (p, n, reachH);
			const double H = freeH >= reachH ? Hwanted : std::min (Hwanted, psi * freeH);
			const Point3 back3 {-n[0], -n[1], -n[2]};
			const double reachHb = Hbwanted / psi, freeHb = bend.Free (p, back3, reachHb);
			const double Hb = std::min (freeHb >= reachHb ? Hbwanted : std::min (Hbwanted, psi * freeHb),
			                            bend.CoreRoom (p, back3, Hbwanted));
			planar[f] = {X, y, H, Hb};
			normals[f] = n;
			carrier[f] = p;
			const double eo = half + H, ei = half + Hb;
			const double lowest = std::min (p[2] + eo * n[2], p[2] - ei * n[2]);
			Require (lowest >= spec.clearance - 1e-9, [&] {
				return "pied sous le plancher du chapiteau : h " + std::to_string (lowest) + " < clearance " +
				       std::to_string (spec.clearance) + " pres de (X " + std::to_string (X) + ", y " + std::to_string (y) + ")";
			});
			outer[f] = {(p[0] + eo * n[0]) * s, (p[1] + eo * n[1]) * s, (env.foot + p[2] + eo * n[2]) * s};
			inner[f] = {(p[0] - ei * n[0]) * s, (p[1] - ei * n[1]) * s, (env.foot + p[2] - ei * n[2]) * s};
			if (y < 0.0)
				++stemVertices;
		}

	std::vector<Triangle> face;
	std::vector<int> region;
	face.reserve (2 * nt);
	for (unsigned k = 0; k < nt; ++k)
	{
		const std::array<unsigned, 3> &t = tpl.triangles[k];
		face.push_back ({t[0], t[1], t[2]});
		region.push_back (tpl.triangleRegion[k]);
	}
	for (unsigned k = 0; k < nt; ++k)
	{
		const std::array<unsigned, 3> &t = tpl.triangles[k];
		face.push_back ({mirror[t[0]], mirror[t[2]], mirror[t[1]]});
		region.push_back (tpl.triangleRegion[k]);
	}
	CheckFaceFold (outer, carrier, face, normals, planar, "exterieure");
	CheckFaceFold (inner, carrier, face, normals, planar, "interieure");

	PieceGeometry shell;
	shell.positions = outer;
	shell.positions.insert (shell.positions.end (), inner.begin (), inner.end ());
	std::vector<int> faceRegion = region;
	faceRegion.insert (faceRegion.end (), region.begin (), region.end ());
	AddFaces (shell.triangles, face, faceVertices);
	for (const LeafTemplateEdge &e : rim.edges)
	{
		for (const auto &[a, b] : {std::make_pair (e[0], e[1]), std::make_pair (mirror[e[1]], mirror[e[0]])})
		{
			shell.triangles.push_back ({a, a + faceVertices, b});
			shell.triangles.push_back ({b, a + faceVertices, b + faceVertices});
			faceRegion.push_back (-1);
			faceRegion.push_back (-1);
		}
	}
	OrientOutward (shell);

	LeafShell result;
	result.geometry = std::move (shell);
	result.planar = std::move (planar);
	result.faceRegion = std::move (faceRegion);
	result.faceVertices = faceVertices;
	result.faceTriangles = static_cast<unsigned> (face.size ());
	result.stemVertices = stemVertices;
	return result;
}

} // namespace

LeafSheet BuildLeafSheet (const CapitalEnvelope &env, const LeafSpec &spec, const LeafPlacement &placement,
                          const LeafTemplate &tpl)
{
	LeafShell shell = BuildLeafShell (env, spec, placement, tpl);
	LeafSheet sheet;
	sheet.mesh = MakeTriangleMesh (shell.geometry.positions, shell.geometry.triangles);
	sheet.planar = std::move (shell.planar);
	sheet.faceRegion = std::move (shell.faceRegion);
	sheet.faceVertices = shell.faceVertices;
	sheet.faceTriangles = shell.faceTriangles;
	sheet.stemVertices = shell.stemVertices;
	return sheet;
}

LeafSpec DefaultLeafSpec (OrnamentDetail detail, LeafStyle style)
{
	LeafSpec spec;
	spec.detail = detail;
	spec.style = style;
	return spec;
}

CauliculusSpec DefaultCauliculusSpec (OrnamentDetail detail)
{
	CauliculusSpec spec;
	spec.detail = detail;
	switch (detail)
	{
	case OrnamentDetail::Light:
		return spec;
	case OrnamentDetail::Heavy:
		spec.samplesPerFlute = 6;
		spec.fadeRings = 6;
		spec.cordRings = 12;
		return spec;
	default:
		throw std::invalid_argument ("DefaultCauliculusSpec : niveau Light ou Heavy attendu");
	}
}

double CauliculusRadius (const CauliculusSpec &spec, double h)
{
	const auto cord = [&] (double h0, double h1, double peak) {
		const double t = std::clamp ((h - h0) / (h1 - h0), 0.0, 1.0), e = std::fabs (2.0 * t - 1.0);
		return spec.radius + (peak - spec.radius) * std::cbrt (std::max (0.0, 1.0 - e * e * e));
	};
	if (h > spec.lowerCordBottom && h < spec.gorge)
		return cord (spec.lowerCordBottom, spec.gorge, spec.lowerCordRadius);
	if (h > spec.gorge && h < spec.hHead)
		return cord (spec.gorge, spec.hHead, spec.upperCordRadius);
	return spec.radius;
}

std::vector<double> CauliculusRings (const CauliculusSpec &spec)
{
	const double fade0 = spec.lowerCordBottom - spec.fluteFade;
	std::vector<double> rings {spec.hFoot, fade0};
	for (int i = 1; i <= spec.fadeRings; ++i)
		rings.push_back (fade0 + spec.fluteFade * i / spec.fadeRings);
	for (int i = 1; i <= spec.cordRings; ++i)
		rings.push_back (spec.lowerCordBottom + (spec.gorge - spec.lowerCordBottom) * i / spec.cordRings);
	for (int i = 1; i <= spec.cordRings; ++i)
		rings.push_back (spec.gorge + (spec.hHead - spec.gorge) * i / spec.cordRings);
	return rings;
}

namespace
{

[[noreturn]] void FailCauliculus (const std::string &what)
{
	throw std::invalid_argument ("BuildCauliculusShell : " + what);
}

// Fleche relative g (0 a 1) d'une cannelure a l'angle local alpha : fond en arc
// de cercle de corde fluteWidth et de fleche fluteDepth, centre en alpha = 0 mod
// 2 pi / N ; 0 sur les listels.
double FluteShape (const CauliculusSpec &spec, double alpha)
{
	const double pitch = 2.0 * kPi / spec.flutes;
	const double x = spec.radius * std::remainder (alpha, pitch), c = 0.5 * spec.fluteWidth, s = spec.fluteDepth;
	if (std::fabs (x) >= c)
		return 0.0;
	const double rho = (c * c + s * s) / (2.0 * s);
	return (std::sqrt (rho * rho - x * x) - (rho - s)) / s;
}

// Construction de BuildCauliculusSheet, memes gardes, sans le maillage float.
PieceGeometry BuildCauliculusShell (const CapitalEnvelope &env, const CauliculusSpec &spec, double theta)
{
	const auto require = [] (bool ok, const char *what) {
		if (!ok)
			FailCauliculus (std::string ("forme hors domaine : ") + what);
	};
	CheckEnvelope (env);
	require (spec.detail == OrnamentDetail::Light || spec.detail == OrnamentDetail::Heavy, "detail Light ou Heavy");
	require (std::isfinite (theta), "theta fini");
	require (std::isfinite (spec.axisRadius) && spec.axisRadius > 0.0, "axisRadius fini et > 0");
	require (std::isfinite (spec.radius) && spec.radius > 0.0, "radius fini et > 0");
	require (spec.flutes >= 1 && spec.samplesPerFlute >= 1 && spec.fadeRings >= 1 && spec.cordRings >= 2,
	         "flutes, samplesPerFlute, fadeRings >= 1, cordRings >= 2");
	require (std::isfinite (spec.fluteDepth) && spec.fluteDepth > 0.0 && spec.fluteDepth < spec.radius &&
	             std::isfinite (spec.fluteWidth) && spec.fluteWidth > 0.0 &&
	             spec.fluteWidth < 2.0 * kPi * spec.radius / spec.flutes && spec.fluteDepth <= 0.5 * spec.fluteWidth,
	         "cannelure : 0 < fluteDepth <= fluteWidth / 2, fluteDepth < radius, fluteWidth < pas");
	require (std::isfinite (spec.fluteFade) && spec.fluteFade > 0.0, "fluteFade fini et > 0");
	require (std::isfinite (spec.hFoot) && std::isfinite (spec.hHead) &&
	             spec.hFoot < spec.lowerCordBottom - spec.fluteFade && spec.lowerCordBottom < spec.gorge && spec.gorge < spec.hHead,
	         "hauteurs : hFoot < lowerCordBottom - fluteFade < lowerCordBottom < gorge < hHead");
	require (std::isfinite (spec.lowerCordRadius) && spec.lowerCordRadius >= spec.radius && std::isfinite (spec.upperCordRadius) &&
	             spec.upperCordRadius >= spec.radius,
	         "cordons de rayon >= radius");
	require (std::isfinite (spec.clearance) && spec.clearance >= 0.0, "clearance fini et >= 0");

	// Pied au-dessus de l'extrados de la crete de F (crete de la surface moyenne
	// plus t/2 + Hb de reference du revers), plus c.
	const LeafSpec leaf = DefaultLeafSpec (spec.detail, LeafStyle::Olive);
	const double crestF = DefaultLeafPlacement (CapitalPieceKind::LowerLeaf, theta).hTop + 0.5 * leaf.thickness +
	                      LeafTemplateOf (LeafStyle::Olive, spec.detail).backHeight;
	if (spec.hFoot < crestF + spec.clearance - 1e-9)
		FailCauliculus ("pied de tige sous la crete de F : hFoot " + std::to_string (spec.hFoot) + " < " +
		                std::to_string (crestF + spec.clearance));

	const std::vector<double> rings = CauliculusRings (spec);
	const double widest = std::max ({spec.radius, spec.lowerCordRadius, spec.upperCordRadius});
	for (const double h : rings)
		if (spec.axisRadius - widest < env.CoreRadius (h) + spec.clearance - 1e-9)
			FailCauliculus ("tige dans la corbeille a h " + std::to_string (h) + " : r_C - " + std::to_string (widest) +
			                " < noyau + c");

	const unsigned nPhi = static_cast<unsigned> (spec.flutes * spec.samplesPerFlute);
	const unsigned nZ = static_cast<unsigned> (rings.size ());
	const double s = env.scale, ct = std::cos (theta), st = std::sin (theta);
	const double cx = spec.axisRadius * ct, cy = spec.axisRadius * st;
	const double fade0 = spec.lowerCordBottom - spec.fluteFade;
	PieceGeometry shell;
	shell.positions.reserve (static_cast<std::size_t> (nPhi) * nZ + 2);
	for (const double h : rings)
	{
		const double w = 1.0 - Smootherstep (fade0, spec.lowerCordBottom, h).value;
		const double base = CauliculusRadius (spec, h);
		for (unsigned i = 0; i < nPhi; ++i)
		{
			const double alpha = 2.0 * kPi * i / nPhi, ca = std::cos (alpha), sa = std::sin (alpha);
			const double r = base - spec.fluteDepth * w * FluteShape (spec, alpha);
			// Direction locale : alpha = 0 vers l'exterieur (e_r), alpha = pi/2 vers e_theta.
			const double dx = ca * ct - sa * st, dy = ca * st + sa * ct;
			shell.positions.push_back ({(cx + r * dx) * s, (cy + r * dy) * s, (env.foot + h) * s});
		}
	}
	const unsigned foot = static_cast<unsigned> (shell.positions.size ());
	shell.positions.push_back ({cx * s, cy * s, (env.foot + rings.front ()) * s});
	const unsigned head = static_cast<unsigned> (shell.positions.size ());
	shell.positions.push_back ({cx * s, cy * s, (env.foot + rings.back ()) * s});

	shell.triangles.reserve (2 * static_cast<std::size_t> (nPhi) * nZ);
	for (unsigned j = 0; j + 1 < nZ; ++j)
		for (unsigned i = 0; i < nPhi; ++i)
		{
			const unsigned i1 = (i + 1) % nPhi;
			const unsigned a = j * nPhi + i, b = j * nPhi + i1, c = (j + 1) * nPhi + i1, d = (j + 1) * nPhi + i;
			shell.triangles.push_back ({a, b, c});
			shell.triangles.push_back ({a, c, d});
		}
	for (unsigned i = 0; i < nPhi; ++i)
		shell.triangles.push_back ({foot, (i + 1) % nPhi, i});
	for (unsigned i = 0; i < nPhi; ++i)
		shell.triangles.push_back ({head, (nZ - 1) * nPhi + i, (nZ - 1) * nPhi + (i + 1) % nPhi});
	OrientOutward (shell);
	return shell;
}

} // namespace

CauliculusSheet BuildCauliculusSheet (const CapitalEnvelope &env, const CauliculusSpec &spec, double theta)
{
	const PieceGeometry shell = BuildCauliculusShell (env, spec, theta);
	CauliculusSheet sheet;
	sheet.mesh = MakeTriangleMesh (shell.positions, shell.triangles);
	sheet.angularSamples = static_cast<unsigned> (spec.flutes * spec.samplesPerFlute);
	sheet.rings = static_cast<unsigned> (CauliculusRings (spec).size ());
	return sheet;
}

} // namespace detail

namespace detail
{

OrnamentGeometry BuildCorinthianOrnamentGeometry (const OrderData &order, const ColumnProfile &profile,
                                                  const AcanthusOptions &options)
{
	if (order.order != Order::Corinthian)
		throw std::invalid_argument ("BuildCorinthianOrnament : ordre corinthien requis");
	if (!(options.scale > 0.0) || !std::isfinite (options.scale))
		throw std::invalid_argument ("BuildCorinthianOrnament : options invalides (scale non fini ou <= 0)");
	if (options.detail != OrnamentDetail::None && options.detail != OrnamentDetail::Light &&
	    options.detail != OrnamentDetail::Heavy)
		throw std::invalid_argument ("BuildCorinthianOrnament : detail hors de l'enumeration");
	const CapitalEnvelope env = ReadCapitalEnvelope (order, profile, options.scale);
	if (options.detail == OrnamentDetail::None)
		return {};

	// La borne des feuilles F et E est de revolution (LeafBoundRadius) : la
	// feuille posee a theta_k est la representante tournee de theta_k - theta_0.
	const LeafSpec spec = DefaultLeafSpec (options.detail, LeafStyle::Olive);
	const LeafTemplate &tpl = LeafTemplateOf (LeafStyle::Olive, options.detail);
	std::array<PieceGeometry, 2> ranks;
	for (std::size_t r = 0; r < 2; ++r)
	{
		const CapitalPieceKind kind = r == 0 ? CapitalPieceKind::LowerLeaf : CapitalPieceKind::MiddleLeaf;
		ranks[r] = BuildLeafShell (env, spec, DefaultLeafPlacement (kind, RankTheta (kind, 0)), tpl).geometry;
	}
	// Tige de revolution autour de son axe, cannelures symetriques par le plan
	// radial : la tige a theta_k est la representante tournee de theta_k - theta_0.
	const double thetaC = RankTheta (CapitalPieceKind::Cauliculus, 0);
	const PieceGeometry stem = BuildCauliculusShell (env, DefaultCauliculusSpec (options.detail), thetaC);
	// Feuilles D : la pose depend du cote de la tige par rapport a l'axe de face (D i. et D e. des tiges 0 et 1 sont
	// les images miroir l'une de l'autre) ; les quatre representantes sont copiees par rotation de pi/2 (groupe C4 de
	// l'abaque). Ordre : par tige k croissante, D interieur puis D exterieur.
	const LeafTemplate &minorTpl = LeafTemplateOf (LeafStyle::OliveMinor, options.detail);
	const std::array<LeafPlacement, 4> poses = DefaultMinorLeafPlacements (env);
	std::array<PieceGeometry, 4> minor;
	for (std::size_t i = 0; i < 4; ++i)
		minor[i] = BuildLeafShell (env, DefaultMinorLeafSpec (options.detail, i % 2 == 0 ? MinorLeafSide::Inner : MinorLeafSide::Outer),
		                           poses[i], minorTpl)
		               .geometry;

	// Volutes : les representantes des tiges 0 et 1 (paire sous la corne pi/4, images miroir l'une de l'autre) sont
	// copiees par rotation de pi/2 ; ordre par tige k croissante.
	const VoluteSpec voluteSpec = DefaultVoluteSpec (options.detail);
	std::array<PieceGeometry, 2> volutes;
	for (std::size_t i = 0; i < 2; ++i)
	{
		VolutePiece v = BuildVoluteRibbon (env, voluteSpec, static_cast<int> (i));
		volutes[i].positions = std::move (v.positions);
		volutes[i].triangles = std::move (v.triangles);
	}
	// Helices : meme construction ; les representantes des tiges 0 et 1 vont vers les axes de face 0 et pi/2 (images
	// miroir par theta = pi/4), copiees par rotation de pi/2 ; ordre par tige k croissante.
	const VoluteSpec helixSpec = DefaultHelixSpec (options.detail);
	std::array<PieceGeometry, 2> helices;
	for (std::size_t i = 0; i < 2; ++i)
	{
		VolutePiece h = BuildVoluteRibbon (env, helixSpec, static_cast<int> (i));
		helices[i].positions = std::move (h.positions);
		helices[i].triangles = std::move (h.triangles);
	}

	// Fleurons : la representante de la face theta = 0, dos sur la corde du tour au reglage de la colonne, copiee
	// par rotation de k pi/2.
	LatheOptions lathe;
	lathe.radialSegments = options.radialSegments;
	FleuronPiece fleuronPiece = BuildFleuron (env, ReadCapitalTour (profile, LatheAngles (profile, lathe), env), DefaultFleuronSpec (options.detail));
	PieceGeometry fleuron;
	fleuron.positions = std::move (fleuronPiece.positions);
	fleuron.triangles = std::move (fleuronPiece.triangles);

	std::size_t vertices = kLeavesPerRank * (ranks[0].positions.size () + ranks[1].positions.size () + stem.positions.size ());
	std::size_t triangles = kLeavesPerRank * (ranks[0].triangles.size () + ranks[1].triangles.size () + stem.triangles.size ());
	for (const PieceGeometry &piece : minor)
	{
		vertices += 4 * piece.positions.size ();
		triangles += 4 * piece.triangles.size ();
	}
	for (const std::array<PieceGeometry, 2> *family : {&volutes, &helices})
		for (const PieceGeometry &piece : *family)
		{
			vertices += 4 * piece.positions.size ();
			triangles += 4 * piece.triangles.size ();
		}
	vertices += kFleurons * fleuron.positions.size ();
	triangles += kFleurons * fleuron.triangles.size ();
	OrnamentAssembly assembly;
	assembly.Reserve (vertices, triangles);
	for (std::size_t r = 0; r < 2; ++r)
	{
		const CapitalPieceKind kind = r == 0 ? CapitalPieceKind::LowerLeaf : CapitalPieceKind::MiddleLeaf;
		const double theta0 = RankTheta (kind, 0);
		assembly.Add (kind, ranks[r]);
		for (int k = 1; k < kLeavesPerRank; ++k)
			assembly.AddRotated (kind, ranks[r], RankTheta (kind, k) - theta0);
	}
	assembly.Add (CapitalPieceKind::Cauliculus, stem);
	for (int k = 1; k < kLeavesPerRank; ++k)
		assembly.AddRotated (CapitalPieceKind::Cauliculus, stem, RankTheta (CapitalPieceKind::Cauliculus, k) - thetaC);
	for (int k = 0; k < kLeavesPerRank; ++k)
		for (std::size_t side = 0; side < 2; ++side)
		{
			const PieceGeometry &rep = minor[2 * static_cast<std::size_t> (k % 2) + side];
			if (k < 2)
				assembly.Add (CapitalPieceKind::MinorLeaf, rep);
			else
				assembly.AddRotated (CapitalPieceKind::MinorLeaf, rep, 0.5 * kPi * (k / 2));
		}
	for (int k = 0; k < kLeavesPerRank; ++k)
	{
		const PieceGeometry &rep = volutes[static_cast<std::size_t> (k % 2)];
		if (k < 2)
			assembly.Add (CapitalPieceKind::Volute, rep);
		else
			assembly.AddRotated (CapitalPieceKind::Volute, rep, 0.5 * kPi * (k / 2));
	}
	for (int k = 0; k < kLeavesPerRank; ++k)
	{
		const PieceGeometry &rep = helices[static_cast<std::size_t> (k % 2)];
		if (k < 2)
			assembly.Add (CapitalPieceKind::Helix, rep);
		else
			assembly.AddRotated (CapitalPieceKind::Helix, rep, 0.5 * kPi * (k / 2));
	}
	assembly.Add (CapitalPieceKind::Fleuron, fleuron);
	for (int k = 1; k < kFleurons; ++k)
		assembly.AddRotated (CapitalPieceKind::Fleuron, fleuron, 2.0 * kPi * k / kFleurons);
	return assembly.Release ();
}

} // namespace detail

CapitalOrnament BuildCorinthianOrnament (const OrderData &order, const ColumnProfile &profile,
                                         const AcanthusOptions &options)
{
	detail::OrnamentGeometry geometry = detail::BuildCorinthianOrnamentGeometry (order, profile, options);
	if (options.detail == OrnamentDetail::None)
		return {std::make_unique<Mesh> (), {}};
	CapitalOrnament ornament;
	ornament.mesh = MakeTriangleMesh (geometry.positions, geometry.triangles);
	ornament.pieces = std::move (geometry.pieces);
	return ornament;
}

} // namespace cgarchi::classical
