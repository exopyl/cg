#include <cgarchi/classical_greek_pediment.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_pediment_detail.h>
#include <cgarchi/classical_greek_section_detail.h>
#include <cgarchi/classical_mesh.h>
#include <cgarchi/classical_partition.h>
#include <cgarchi/classical_temple_refinement.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void Fail (const std::string &field, const std::string &rule)
{
	throw std::invalid_argument ("Fronton : '" + field + "' " + rule);
}

const char *const kSection = "pediment.rakingSection";

thread_local std::size_t pedimentOwnValidations = 0;

// Rapport des cotes aux hauteurs normales a la pente 1 : r (PE2').
double SlopeScale (double r)
{
	return std::hypot (r, 1.0) / r;
}

// Montees au-dessus de z11 dans l'axe, rise[k] = H_b + h_k s, H_b = apexRise -
// T s (PE2'). Section validee : T = h du dernier sommet. Seule expression de
// ces montees : la validation de H_b, le repere et les pieds la relisent. Les
// produits T s et h_k s sont arrondis en double avant la somme, jamais
// contractes en FMA : H_b, rise, foot et xTop, et les refus qui les comparent,
// sont les memes sur toute cible.
std::vector<double> Rises (const GreekPedimentSettings &p)
{
	const double s = SlopeScale (p.rakingRun);
	const volatile double top = p.rakingSection.back ().z * s;
	const double base = p.apexRise - top;
	std::vector<double> rise;
	rise.reserve (p.rakingSection.size ());
	for (const RingSectionPoint &q : p.rakingSection)
	{
		const volatile double h = q.z * s;
		rise.push_back (base + h);
	}
	return rise;
}

// Refus du pied du haut de la corniche rampante au-dela de l'avant du
// couronnement, commun a la construction et a la mise en place du temple.
void RequireTopWithinCrown (double xTop, double xEave)
{
	if (!(xTop <= xEave))
		Fail ("pediment.rakingRun", "met le pied du haut de la corniche rampante, r apexRise, au-dela de l'avant du "
		                            "couronnement, halfWidth + bands.back ().topProjection");
}

// Champs propres du fronton, l'entablement etant valide.
void ValidateOwn (const GreekPedimentSettings &p, const GreekEntablatureSettings &e)
{
	++pedimentOwnValidations;
	if (!(std::isfinite (p.apexRise) && p.apexRise > 0.0))
		Fail ("pediment.apexRise", "doit etre fini et strictement positif");
	if (!(std::isfinite (p.rakingRun) && p.rakingRun > 0.0))
		Fail ("pediment.rakingRun", "doit etre fini et strictement positif (pente 1 : r)");
	const double crown = e.bands.back ().topProjection;
	if (!(std::isfinite (p.tympanumFace) && -e.innerFace < p.tympanumFace && p.tympanumFace < crown))
		Fail ("pediment.tympanumFace", "doit etre fini et dans ]-innerFace, couronnement[ (bands.back ().topProjection)");

	const std::vector<RingSectionPoint> &s = p.rakingSection;
	const std::size_t n = s.size ();
	if (n < 3)
		Fail (kSection, "compte " + std::to_string (n) + " sommet(s), au moins 3 attendus");
	for (std::size_t k = 0; k < n; ++k)
		if (!(std::isfinite (s[k].u) && std::isfinite (s[k].z)))
			Fail (std::string (kSection) + "[" + std::to_string (k) + "]", "n'est pas fini");
	if (!(s.front ().u == 0.0 && s.front ().z == 0.0))
		Fail (kSection, "doit commencer en (0, 0) : bas du dos, sur le dessous de la section");
	double top = s.front ().z;
	for (const RingSectionPoint &q : s)
		top = std::max (top, q.z);
	if (!(s.back ().u == 0.0 && s.back ().z == top && top > 0.0))
		Fail (kSection, "doit finir en (0, T), T le plus grand h, strictement positif : haut du dos");
	for (std::size_t k = 1; k + 1 < n; ++k)
		if (!(s[k].u > 0.0 && s[k].z >= 0.0))
			Fail (kSection, "a un sommet " + std::to_string (k) + " d'u <= 0 (au dos ou derriere) ou de h < 0 (sous le dessous)");
	for (std::size_t k = 0; k < n; ++k)
	{
		const RingSectionPoint &a = s[k], &b = s[(k + 1) % n];
		if (a.u == b.u && a.z == b.z)
			Fail (kSection, "a deux sommets consecutifs egaux, " + std::to_string (k) + " et " + std::to_string ((k + 1) % n));
	}
	if (!detail::IsSimple (s))
		Fail (kSection, "n'est pas un polygone simple");
	if (!(Rises (p).front () > 0.0))
		Fail ("pediment.apexRise", "doit depasser T s, hauteur verticale de la corniche rampante : le dessous, apexRise - T s, "
		                           "doit etre au-dessus de z11 dans l'axe");
}

// Repere du fronton (D3-89, D3-90) : toute cote partagee entre deux pieces est
// calculee ici une fois et relue.
struct Frame
{
	double xEave = 0.0, xTop = 0.0, yTympanum = 0.0, zBase = 0.0;
	std::vector<double> rise, ridge, foot, depth;   // par sommet de section ; depth : |y|
};

Frame FrameOf (const GreekPedimentSettings &p, const GreekEntablatureSettings &e, const PeristyleLayout &peristyle,
               double zFoot)
{
	std::vector<double> heights;
	heights.reserve (e.bands.size ());
	for (const EntablatureBand &b : e.bands)
		heights.push_back (b.height);
	const double z11 = CumulativeBounds (0.0, heights, "bands").back ();

	Frame f;
	f.xEave = peristyle.halfWidth + e.bands.back ().topProjection;
	f.yTympanum = peristyle.halfDepth + p.tympanumFace;
	f.zBase = zFoot + z11;
	f.rise = Rises (p);
	const std::size_t n = p.rakingSection.size ();
	f.ridge.reserve (n);
	f.foot.reserve (n);
	f.depth.reserve (n);
	for (std::size_t k = 0; k < n; ++k)
	{
		f.ridge.push_back (f.zBase + f.rise[k]);
		f.foot.push_back (p.rakingRun * f.rise[k]);
		f.depth.push_back (peristyle.halfDepth + (p.tympanumFace + p.rakingSection[k].u));
	}
	f.xTop = f.foot.back ();
	return f;
}

// Deux sommets voisins de la section, au faite (x commun) ou au pied (z commun),
// confondus une fois arrondis en float.
bool FloatCollapse (const Frame &f)
{
	const std::size_t n = f.depth.size ();
	for (std::size_t k = 0; k < n; ++k)
	{
		const std::size_t j = (k + 1) % n;
		if (static_cast<float> (f.depth[k]) != static_cast<float> (f.depth[j]))
			continue;
		if (static_cast<float> (f.foot[k]) == static_cast<float> (f.foot[j]) ||
		    static_cast<float> (f.ridge[k]) == static_cast<float> (f.ridge[j]))
			return true;
	}
	return false;
}

void PushTriangle (std::vector<std::array<unsigned, 3>> &triangles, unsigned a, unsigned b, unsigned c, bool reversed)
{
	if (reversed)
		triangles.push_back ({a, c, b});
	else
		triangles.push_back ({a, b, c});
}

// Lambda d'une facade : sommets de faite 0 .. n - 1, pieds du bras droit
// (x = +foot[k]) n .. 2n - 1, du bras gauche 2n .. 3n - 1. Pour un bras dont le
// sens de la pente, du pied au faite, est celui de la normale de la section
// directe (u, h) -- bras droit du Front, bras gauche du Back --, la paroi
// (F_k, F_k+1, R_k+1, R_k) est sortante et la face de pied est la section
// retournee ; l'inverse pour l'autre bras.
std::unique_ptr<Mesh> LambdaMesh (const Frame &f, const std::vector<std::array<unsigned, 3>> &caps, bool back)
{
	const unsigned n = static_cast<unsigned> (f.depth.size ());
	std::vector<std::array<double, 3>> positions;
	positions.reserve (3 * static_cast<std::size_t> (n));
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (6 * static_cast<std::size_t> (n) - 4);
	auto y = [&] (unsigned k) { return back ? f.depth[k] : -f.depth[k]; };
	for (unsigned k = 0; k < n; ++k)
		positions.push_back ({0.0, y (k), f.ridge[k]});
	for (const bool left : {false, true})
		for (unsigned k = 0; k < n; ++k)
			positions.push_back ({left ? -f.foot[k] : f.foot[k], y (k), f.zBase});
	for (const bool left : {false, true})
	{
		const unsigned end = left ? 2 * n : n;
		const bool reversed = back != left;
		for (unsigned k = 0; k < n; ++k)
		{
			const unsigned j = (k + 1) % n;
			PushTriangle (triangles, end + k, end + j, j, reversed);
			PushTriangle (triangles, end + k, j, k, reversed);
		}
		for (const std::array<unsigned, 3> &c : caps)
			PushTriangle (triangles, end + c[0], end + c[1], end + c[2], !reversed);
	}
	return MakeTriangleMesh (positions, triangles);
}

// Comble : triangle direct dans (x, z), de normale -y ; prisme de l'about Back
// (3 .. 5) a l'about Front (0 .. 2), couvercles Front (0, 1, 2) et Back
// retourne.
std::unique_ptr<Mesh> RoofMesh (const Frame &f)
{
	const std::size_t T = f.ridge.size () - 1;
	const std::array<std::array<double, 2>, 3> triangle = {{{-f.xTop, f.zBase}, {f.xTop, f.zBase}, {0.0, f.ridge[T]}}};
	const unsigned m = static_cast<unsigned> (triangle.size ());
	std::vector<std::array<double, 3>> positions;
	positions.reserve (2 * m);
	for (const double y : {-f.yTympanum, f.yTympanum})
		for (const std::array<double, 2> &q : triangle)
			positions.push_back ({q[0], y, q[1]});
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (2 * m + 2);
	for (unsigned k = 0; k < m; ++k)
	{
		const unsigned j = (k + 1) % m;
		PushTriangle (triangles, m + k, m + j, j, false);
		PushTriangle (triangles, m + k, j, k, false);
	}
	PushTriangle (triangles, 0, 1, 2, false);
	PushTriangle (triangles, m, m + 1, m + 2, true);
	return MakeTriangleMesh (positions, triangles);
}

// Criteres stricts de la poche contre le comble, sur des coordonnees donnees
// (en double, ou arrondies en float puis relues en double). Rend le champ fautif
// et la regle, ou deux chaines vides. Produits du critere des pans arrondis en
// double avant la comparaison, jamais contractes en FMA.
std::pair<std::string, std::string> PocketConflict (double x0, double x1, double y0, double y1, double zTop, double xTop,
                                                    double yTympanum, double zBase, double zRidge)
{
	if (!(x0 < x1))
		return {"roofPocket.x", "a une emprise vide ou inversee : x0 < x1 attendu"};
	if (!(y0 < y1))
		return {"roofPocket.y", "a une emprise vide ou inversee : y0 < y1 attendu"};
	if (!(zTop > zBase))
		return {"roofPocket.zTop", "doit etre au-dessus de la base du comble, zFoot + z11"};
	if (!(-xTop < x0 && x1 < xTop))
		return {"roofPocket.x", "touche ou traverse un bord de la base du comble : -xTop < x0 et x1 < xTop attendus"};
	if (!(-yTympanum < y0 && y1 < yTympanum))
		return {"roofPocket.y", "touche ou traverse un tympan : -yTympanum < y0 et y1 < yTympanum attendus"};
	const double xFar = std::max (-x0, x1);
	const volatile double ceiling = (zTop - zBase) * xTop;
	const volatile double pan = (zRidge - zBase) * (xTop - xFar);
	if (!(ceiling < pan))
		return {"roofPocket.zTop", "touche ou traverse un pan : un coin du plafond n'est pas strictement sous les pans"};
	return {};
}

void RequirePocket (const Frame &f, const RoofPocket &p)
{
	if (!(std::isfinite (p.x0) && std::isfinite (p.x1) && std::isfinite (p.y0) && std::isfinite (p.y1) && std::isfinite (p.zTop)))
		Fail ("roofPocket", "a une borne non finie");
	const double zRidge = f.ridge.back ();
	const std::pair<std::string, std::string> exact =
	    PocketConflict (p.x0, p.x1, p.y0, p.y1, p.zTop, f.xTop, f.yTympanum, f.zBase, zRidge);
	if (!exact.first.empty ())
		Fail (exact.first, exact.second);
	auto rounded = [] (double v) { return static_cast<double> (static_cast<float> (v)); };
	const std::pair<std::string, std::string> stored =
	    PocketConflict (rounded (p.x0), rounded (p.x1), rounded (p.y0), rounded (p.y1), rounded (p.zTop), rounded (f.xTop),
	                    rounded (f.yTympanum), rounded (f.zBase), rounded (zRidge));
	if (!stored.first.empty ())
		Fail ("roofPocket", "touche le comble une fois les sommets arrondis en float (" + stored.first + " " + stored.second + ")");
}

// Comble a poche : sommets du prisme (RoofMesh), puis le bas de la poche
// (6 .. 9, z = zBase) et son plafond (10 .. 13, z = zTop), coins dans l'ordre
// (x0, y0), (x1, y0), (x1, y1), (x0, y1). Pans, abouts, base en couronne de
// normale -z, parois de normale vers le vide, plafond de normale -z.
std::unique_ptr<Mesh> RoofMeshWithPocket (const Frame &f, const RoofPocket &p)
{
	const std::size_t T = f.ridge.size () - 1;
	const std::array<std::array<double, 2>, 3> triangle = {{{-f.xTop, f.zBase}, {f.xTop, f.zBase}, {0.0, f.ridge[T]}}};
	const unsigned m = static_cast<unsigned> (triangle.size ());
	const std::array<std::array<double, 2>, 4> corners = {{{p.x0, p.y0}, {p.x1, p.y0}, {p.x1, p.y1}, {p.x0, p.y1}}};
	std::vector<std::array<double, 3>> positions;
	positions.reserve (2 * m + 2 * corners.size ());
	for (const double y : {-f.yTympanum, f.yTympanum})
		for (const std::array<double, 2> &q : triangle)
			positions.push_back ({q[0], y, q[1]});
	for (const double z : {f.zBase, p.zTop})
		for (const std::array<double, 2> &c : corners)
			positions.push_back ({c[0], c[1], z});

	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (24);
	for (unsigned k = 1; k < m; ++k)
	{
		const unsigned j = (k + 1) % m;
		PushTriangle (triangles, m + k, m + j, j, false);
		PushTriangle (triangles, m + k, j, k, false);
	}
	PushTriangle (triangles, 0, 1, 2, false);
	PushTriangle (triangles, m, m + 1, m + 2, true);
	// Bord de la base, sens direct vu de +z : (-xTop, -yT), (xTop, -yT), (xTop, yT), (-xTop, yT).
	const std::array<unsigned, 4> outer = {0, 1, m + 1, m};
	const unsigned low = 2 * m, high = low + 4;
	for (unsigned k = 0; k < 4; ++k)
	{
		const unsigned j = (k + 1) % 4;
		PushTriangle (triangles, outer[k], low + k, low + j, false);
		PushTriangle (triangles, outer[k], low + j, outer[j], false);
	}
	for (unsigned k = 0; k < 4; ++k)
	{
		const unsigned j = (k + 1) % 4;
		PushTriangle (triangles, low + k, high + k, high + j, false);
		PushTriangle (triangles, low + k, high + j, low + j, false);
	}
	PushTriangle (triangles, high, high + 2, high + 1, false);
	PushTriangle (triangles, high, high + 3, high + 2, false);
	return MakeTriangleMesh (positions, triangles);
}

// Intervalles ouverts ]a0, a1[ et ]b0, b1[ d'intersection non vide.
bool OpenIntervalsMeet (double a0, double a1, double b0, double b1)
{
	return std::max (a0, b0) < std::min (a1, b1);
}

bool PocketsOverlap (const RoofPocket &a, const RoofPocket &b)
{
	return OpenIntervalsMeet (a.x0, a.x1, b.x0, b.x1) && OpenIntervalsMeet (a.y0, a.y1, b.y0, b.y1);
}

// Poches deux a deux (CE-34) : emprises d'intersection d'aire nulle, le contact
// par une face admis. Verdict en double, qui vaut aussi pour les bornes
// arrondies en float : l'arrondi est monotone, deux emprises disjointes ou au
// contact le restent.
void RequireSeparatePockets (const std::vector<RoofPocket> &pockets)
{
	for (std::size_t k = 0; k < pockets.size (); ++k)
		for (std::size_t l = k + 1; l < pockets.size (); ++l)
			if (PocketsOverlap (pockets[k], pockets[l]))
				Fail ("roofPocket", ": les poches " + std::to_string (k) + " et " + std::to_string (l) + " se recouvrent");
}

// Bornes triees, sans doublon ; refus si deux bornes distinctes se confondent
// une fois arrondies en float.
std::vector<double> DistinctPocketBounds (std::vector<double> bounds)
{
	std::sort (bounds.begin (), bounds.end ());
	bounds.erase (std::unique (bounds.begin (), bounds.end ()), bounds.end ());
	for (std::size_t k = 1; k < bounds.size (); ++k)
		if (static_cast<float> (bounds[k - 1]) == static_cast<float> (bounds[k]))
			throw std::invalid_argument ("Fronton : sommets confondus en float : bornes de deux poches");
	return bounds;
}

// Dessous du comble dans une tranche : hauteur par intervalle de X, de -xTop a
// xTop ; zBase hors des poches, zTop sur une poche. Deux intervalles voisins de
// meme hauteur sont fusionnes : pas de sommet aligne sans paroi.
struct SliceProfile
{
	std::vector<double> x;   // bornes croissantes, x.size () == z.size () + 1
	std::vector<double> z;
};

SliceProfile ProfileOf (const Frame &f, const std::vector<const RoofPocket *> &active)
{
	SliceProfile p;
	auto push = [&p] (double x1, double z) {
		if (!p.z.empty () && p.z.back () == z)
			p.x.back () = x1;
		else
		{
			p.z.push_back (z);
			p.x.push_back (x1);
		}
	};
	p.x.push_back (-f.xTop);
	for (const RoofPocket *q : active)
	{
		if (p.x.back () < q->x0)
			push (q->x0, f.zBase);
		push (q->x1, q->zTop);
	}
	push (f.xTop, f.zBase);
	return p;
}

// Hauteur du profil sur la colonne qui commence en x0, borne du profil ou d'un
// profil voisin, strictement avant xTop.
double HeightFrom (const SliceProfile &p, double x0)
{
	const std::size_t k = static_cast<std::size_t> (std::upper_bound (p.x.begin (), p.x.end (), x0) - p.x.begin ()) - 1;
	return p.z[k];
}

// Sommets d'un plan de coupe y = cte entre la tranche inferieure et la tranche
// superieure (la meme aux deux abouts) : abscisses, bornes des deux profils, et
// a chacune les hauteurs des colonnes qui la bordent dans les deux profils.
// Toute face du comble dont un bord est dans ce plan est subdivisee par ces
// sommets : aucune T-jonction.
struct PlaneVertices
{
	std::vector<double> x;
	std::vector<std::vector<double>> z;   // z[j] croissantes, sommets d'abscisse x[j]
	std::vector<double> lower, upper;     // par colonne [x[j], x[j + 1]] : hauteurs des deux profils
};

void RequireDistinctInFloat (const std::vector<double> &values, const char *what)
{
	for (std::size_t k = 1; k < values.size (); ++k)
		if (static_cast<float> (values[k - 1]) == static_cast<float> (values[k]))
			throw std::invalid_argument (std::string ("Fronton : sommets confondus en float : ") + what);
}

PlaneVertices PlaneOf (const SliceProfile &lower, const SliceProfile &upper)
{
	PlaneVertices v;
	v.x = lower.x;
	v.x.insert (v.x.end (), upper.x.begin (), upper.x.end ());
	std::sort (v.x.begin (), v.x.end ());
	v.x.erase (std::unique (v.x.begin (), v.x.end ()), v.x.end ());
	RequireDistinctInFloat (v.x, "bornes de deux poches");
	const std::size_t columns = v.x.size () - 1;
	v.lower.reserve (columns);
	v.upper.reserve (columns);
	for (std::size_t j = 0; j < columns; ++j)
	{
		v.lower.push_back (HeightFrom (lower, v.x[j]));
		v.upper.push_back (HeightFrom (upper, v.x[j]));
	}
	// Contact par une arete seule : a une borne, une cote ou chaque tranche n'est
	// pleine que d'un cote, et pas du meme ; l'arete y serait portee par quatre faces.
	for (std::size_t j = 1; j < columns; ++j)
	{
		const double l0 = v.lower[j - 1], l1 = v.lower[j], u0 = v.upper[j - 1], u1 = v.upper[j];
		if (std::max (l0, u1) < std::min (l1, u0) || std::max (l1, u0) < std::min (l0, u1))
			throw std::invalid_argument ("Fronton : 'roofPocket' : deux poches de tranches voisines ne se touchent que par une arete");
	}
	v.z.resize (v.x.size ());
	for (std::size_t j = 0; j < v.x.size (); ++j)
	{
		std::vector<double> &z = v.z[j];
		for (const std::size_t c : {j - 1, j})
			if (c < columns)
				z.insert (z.end (), {v.lower[c], v.upper[c]});
		std::sort (z.begin (), z.end ());
		z.erase (std::unique (z.begin (), z.end ()), z.end ());
		RequireDistinctInFloat (z, "plafonds de deux poches au contact");
	}
	return v;
}

// Une tranche en construction : sommets partages par position, triangles
// orientes sur une normale sortante donnee.
class SliceBuilder
{
public:
	unsigned Vertex (double x, double y, double z)
	{
		const std::array<double, 3> q {x, y, z};
		const auto found = index_.find (q);
		if (found != index_.end ())
			return found->second;
		const unsigned v = static_cast<unsigned> (positions_.size ());
		positions_.push_back (q);
		index_.emplace (q, v);
		return v;
	}

	// Triangle retourne au besoin pour que sa normale ait un produit scalaire
	// positif avec `outward` ; normale calculee en double sur les positions.
	void Triangle (std::array<unsigned, 3> t, const std::array<double, 3> &outward)
	{
		const std::array<double, 3> &a = positions_[t[0]], &b = positions_[t[1]], &c = positions_[t[2]];
		const std::array<double, 3> u {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
		const std::array<double, 3> n {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
		if (n[0] * outward[0] + n[1] * outward[1] + n[2] * outward[2] < 0.0)
			std::swap (t[1], t[2]);
		triangles_.push_back (t);
	}

	// Bande plane entre deux chaines de memes extremites, `low` dans un plan,
	// `high` dans l'autre, abscisses `lowAt` et `highAt` le long de la bande.
	void Band (const std::vector<unsigned> &low, const std::vector<double> &lowAt, const std::vector<unsigned> &high,
	           const std::vector<double> &highAt, const std::array<double, 3> &outward)
	{
		std::vector<std::array<unsigned, 3>> band;
		AppendZipper (low, lowAt, high, highAt, false, band);
		for (const std::array<unsigned, 3> &t : band)
			Triangle (t, outward);
	}

	// Bande horizontale a la cote z, de x0 a x1, entre le plan `low` (y0) et le
	// plan `high` (y1) ; chaque bord porte toutes les abscisses de son plan dans
	// [x0, x1].
	void HorizontalBand (const PlaneVertices &low, double y0, const PlaneVertices &high, double y1, double x0, double x1,
	                     double z, const std::array<double, 3> &outward)
	{
		HorizontalChain (low, y0, x0, x1, z, a_, aAt_);
		HorizontalChain (high, y1, x0, x1, z, c_, cAt_);
		Band (a_, aAt_, c_, cAt_, outward);
	}

	// Paroi a l'abscisse x, de z0 a z1, entre le plan `low` (y0) et le plan
	// `high` (y1) ; chaque bord porte toutes les cotes de son plan a x dans
	// [z0, z1].
	void VerticalBand (const PlaneVertices &low, double y0, const PlaneVertices &high, double y1, double x, double z0,
	                   double z1, const std::array<double, 3> &outward)
	{
		VerticalChain (low, y0, x, z0, z1, a_, aAt_);
		VerticalChain (high, y1, x, z0, z1, c_, cAt_);
		Band (a_, aAt_, c_, cAt_, outward);
	}

	// Rectangle d'un plan de coupe y, de x0 a x1 et de z0 a z1, ses deux cotes
	// verticaux portant toutes les cotes du plan a leur abscisse.
	void CutRectangle (const PlaneVertices &v, double y, double x0, double x1, double z0, double z1,
	                   const std::array<double, 3> &outward)
	{
		VerticalChain (v, y, x0, z0, z1, a_, aAt_);
		VerticalChain (v, y, x1, z0, z1, c_, cAt_);
		Band (a_, aAt_, c_, cAt_, outward);
	}

	std::unique_ptr<Mesh> Build () const
	{
		return MakeTriangleMesh (positions_, triangles_);
	}

private:
	void HorizontalChain (const PlaneVertices &v, double y, double x0, double x1, double z, std::vector<unsigned> &chain,
	                      std::vector<double> &at)
	{
		chain.clear ();
		at.clear ();
		for (const double x : v.x)
			if (x0 <= x && x <= x1)
			{
				chain.push_back (Vertex (x, y, z));
				at.push_back (x);
			}
	}

	void VerticalChain (const PlaneVertices &v, double y, double x, double z0, double z1, std::vector<unsigned> &chain,
	                    std::vector<double> &at)
	{
		chain.clear ();
		at.clear ();
		const std::size_t j = static_cast<std::size_t> (std::lower_bound (v.x.begin (), v.x.end (), x) - v.x.begin ());
		for (const double z : v.z[j])
			if (z0 <= z && z <= z1)
			{
				chain.push_back (Vertex (x, y, z));
				at.push_back (z);
			}
	}

	std::vector<unsigned> a_, c_;
	std::vector<double> aAt_, cAt_;
	std::vector<std::array<double, 3>> positions_;
	std::vector<std::array<unsigned, 3>> triangles_;
	std::map<std::array<double, 3>, unsigned> index_;
};

// Tranche [y0, y1] de profil `p`, plans de coupe `low` (y0) et `high` (y1) ;
// `front` et `back` : ses bouts sont les tympans.
std::unique_ptr<Mesh> SliceMesh (const Frame &f, const SliceProfile &p, double y0, double y1, const PlaneVertices &low,
                                 const PlaneVertices &high, bool front, bool back)
{
	const double zRidge = f.ridge.back (), rise = zRidge - f.zBase;
	SliceBuilder b;
	// Pans, sans sommet entre pied et faite.
	for (const double side : {-1.0, 1.0})
	{
		const std::vector<unsigned> a {b.Vertex (side * f.xTop, y0, f.zBase), b.Vertex (0.0, y0, zRidge)};
		const std::vector<unsigned> c {b.Vertex (side * f.xTop, y1, f.zBase), b.Vertex (0.0, y1, zRidge)};
		const std::vector<double> at {0.0, 1.0};
		b.Band (a, at, c, at, {side * rise, 0.0, f.xTop});
	}
	// Dessous : intervalles du profil (normale -z), parois entre eux (normale vers la colonne la plus haute).
	for (std::size_t k = 0; k < p.z.size (); ++k)
	{
		b.HorizontalBand (low, y0, high, y1, p.x[k], p.x[k + 1], p.z[k], {0.0, 0.0, -1.0});
		if (k > 0)
			b.VerticalBand (low, y0, high, y1, p.x[k], std::min (p.z[k - 1], p.z[k]), std::max (p.z[k - 1], p.z[k]),
			                {p.z[k] > p.z[k - 1] ? 1.0 : -1.0, 0.0, 0.0});
	}
	// Bouts aux tympans : la section pleine.
	for (const bool atBack : {false, true})
		if (atBack ? back : front)
		{
			const double y = atBack ? y1 : y0;
			b.Triangle ({b.Vertex (-f.xTop, y, f.zBase), b.Vertex (f.xTop, y, f.zBase), b.Vertex (0.0, y, zRidge)},
			            {0.0, atBack ? 1.0 : -1.0, 0.0});
		}
	// Bouts dans un plan de coupe : colonnes ou cette tranche est pleine et sa voisine vide, de la plus basse a la
	// plus haute des deux hauteurs.
	for (const bool atBack : {false, true})
	{
		if (atBack ? back : front)
			continue;
		const PlaneVertices &v = atBack ? high : low;
		for (std::size_t j = 0; j + 1 < v.x.size (); ++j)
		{
			const double own = atBack ? v.lower[j] : v.upper[j], other = atBack ? v.upper[j] : v.lower[j];
			if (own < other)
				b.CutRectangle (v, atBack ? y1 : y0, v.x[j], v.x[j + 1], own, other, {0.0, atBack ? 1.0 : -1.0, 0.0});
		}
	}
	return b.Build ();
}

// Comble en tranches le long de Y (CE-34), une piece par tranche, par Y
// croissants. Chaque tranche porte ses pans, son dessous (base, parois et
// plafonds des poches) et, dans un plan de coupe, la seule partie de sa section
// que la tranche voisine ne couvre pas ; aux tympans, sa section entiere. Les
// tranches ne sont donc pas fermees une a une ; leur reunion, sommets confondus
// par position, l'est. Profils et plans de coupe sont formes, et leurs bornes
// verifiees, avant le premier maillage.
std::vector<std::unique_ptr<Mesh>> RoofSliceMeshes (const Frame &f, const std::vector<RoofPocket> &pockets)
{
	std::vector<double> ys {-f.yTympanum, f.yTympanum};
	for (const RoofPocket &p : pockets)
		ys.insert (ys.end (), {p.y0, p.y1});
	ys = DistinctPocketBounds (std::move (ys));

	std::vector<SliceProfile> profiles;
	profiles.reserve (ys.size () - 1);
	for (std::size_t i = 0; i + 1 < ys.size (); ++i)
	{
		std::vector<const RoofPocket *> active;
		for (const RoofPocket &p : pockets)
			if (OpenIntervalsMeet (p.y0, p.y1, ys[i], ys[i + 1]))
				active.push_back (&p);
		std::sort (active.begin (), active.end (), [] (const RoofPocket *a, const RoofPocket *b) { return a->x0 < b->x0; });
		std::vector<double> xs {-f.xTop, f.xTop};
		for (const RoofPocket *p : active)
			xs.insert (xs.end (), {p->x0, p->x1});
		DistinctPocketBounds (std::move (xs));

		for (std::size_t k = 1; k < active.size (); ++k)
			if (active[k - 1]->x1 == active[k]->x0 && active[k - 1]->zTop != active[k]->zTop &&
			    static_cast<float> (active[k - 1]->zTop) == static_cast<float> (active[k]->zTop))
				throw std::invalid_argument ("Fronton : sommets confondus en float : plafonds de deux poches au contact");
		profiles.push_back (ProfileOf (f, active));
	}
	// Les poches ne touchent pas les tympans : les tranches extremes sont pleines.
	if (profiles.front ().z.size () != 1 || profiles.back ().z.size () != 1)
		throw std::logic_error ("Fronton : poche dans une tranche extreme du comble");

	const std::size_t n = profiles.size ();
	std::vector<PlaneVertices> planes;
	planes.reserve (n + 1);
	for (std::size_t k = 0; k <= n; ++k)
		planes.push_back (PlaneOf (profiles[k == 0 ? 0 : k - 1], profiles[k == n ? n - 1 : k]));

	std::vector<std::unique_ptr<Mesh>> slices;
	slices.reserve (n);
	for (std::size_t i = 0; i < n; ++i)
		slices.push_back (SliceMesh (f, profiles[i], ys[i], ys[i + 1], planes[i], planes[i + 1], i == 0, i + 1 == n));
	return slices;
}

} // namespace

void ValidateGreekPedimentSettings (const GreekPedimentSettings &pediment, const GreekEntablatureSettings &entablature)
{
	ValidateGreekEntablatureSettings (entablature);
	ValidateOwn (pediment, entablature);
}

namespace
{

struct Prepared
{
	Frame frame;
	std::vector<std::array<unsigned, 3>> caps;
};

// Appariement des deux preuves de validation, repere, gardes et decoupe des
// faces de pied, communs aux deux constructions.
Prepared Prepare (const detail::CheckedPediment &checked, const detail::CheckedEntablature &checkedEntablature,
                  const PeristyleLayout &peristyle, double zFoot)
{
	if (&checked.Entablature () != &checkedEntablature.Settings ())
		throw std::logic_error ("Fronton : reglages valides contre un autre entablement");
	const GreekPedimentSettings &pediment = checked.Settings ();
	const GreekEntablatureSettings &entablature = checkedEntablature.Settings ();
	if (!(std::isfinite (peristyle.halfWidth) && std::isfinite (peristyle.halfDepth) && std::isfinite (zFoot)))
		throw std::invalid_argument ("Fronton : demi-distance des axes ou cote du pied non finie");
	Frame f = FrameOf (pediment, entablature, peristyle, zFoot);
	if (!(f.xEave > 0.0))
		Fail ("peristyle.halfWidth", "met l'avant du couronnement, halfWidth + bands.back ().topProjection, a x <= 0");
	if (!(f.yTympanum > 0.0))
		Fail ("peristyle.halfDepth", "met le plan du tympan, halfDepth + tympanumFace, a y <= 0");
	RequireTopWithinCrown (f.xTop, f.xEave);
	// Paroi en lame : un dessous dans l'axe confondu en float avec le dessus de
	// l'anneau (H_b > 0 trop petit), pied pourtant hors de l'axe.
	for (const double z : f.ridge)
		if (static_cast<float> (z) == static_cast<float> (f.zBase))
			Fail ("pediment.apexRise", "met un sommet de faite de la corniche rampante, zBase + H_b + h s, a la cote du "
			                           "dessus de l'anneau une fois arrondi en float : H_b trop petit");
	if (FloatCollapse (f))
		throw std::invalid_argument ("Fronton : sommets confondus en float : deux sommets voisins de 'pediment.rakingSection' "
		                             "se confondent une fois arrondis");
	const std::vector<RingSectionPoint> &s = pediment.rakingSection;
	if (!(detail::TwiceSignedArea (s) > 0.0))
		throw std::logic_error ("Fronton : section rampante de sens indirect apres validation");
	std::vector<std::array<double, 2>> polygon;
	polygon.reserve (s.size ());
	for (const RingSectionPoint &q : s)
		polygon.push_back ({q.u, q.z});
	std::vector<std::array<unsigned, 3>> caps = detail::EarClip (polygon);
	if (caps.size () + 2 != s.size ())
		throw std::logic_error ("Fronton : decoupe d'oreilles incomplete de la section rampante");
	return {std::move (f), std::move (caps)};
}

// Stations des trajectoires (D3-90) : pour chaque sommet k de la section, les
// abscisses de S_X dans ]-foot[k], foot[k][, hors du faite, et leurs cotes,
// calculees une fois et relues par les deux Lambda et par le comble (k = T).
struct RakeStations
{
	std::vector<std::vector<double>> x;   // x[k] croissantes
	std::vector<std::vector<double>> z;   // z[k][i] : cote de la station x[k][i]
};

RakeStations RakeStationsOf (const Frame &f, const std::vector<double> &stationsX)
{
	const std::size_t n = f.foot.size ();
	RakeStations r;
	r.x.resize (n);
	r.z.resize (n);
	for (std::size_t k = 0; k < n; ++k)
		for (const double s : stationsX)
			if (-f.foot[k] < s && s < f.foot[k] && s != 0.0)
			{
				r.x[k].push_back (s);
				r.z[k].push_back (f.zBase + f.rise[k] * ((f.foot[k] - std::fabs (s)) / f.foot[k]));
			}
	return r;
}

// Indices dans x[k] des stations d'un bras, du pied vers le faite.
std::vector<std::size_t> ArmStations (const std::vector<double> &x, bool left)
{
	std::vector<std::size_t> arm;
	arm.reserve (x.size ());
	for (std::size_t i = 0; i < x.size (); ++i)
		if ((x[i] < 0.0) == left)
			arm.push_back (i);
	if (!left)
		std::reverse (arm.begin (), arm.end ());
	return arm;
}

// Paroi plane entre la trajectoire `high` (k) et la trajectoire `low` (k + 1),
// chaines du pied au faite d'abscisses croissantes (-|x|) : fermeture eclair,
// le sommet de `low` d'abord a egalite. Triangles (high[j], low[i], low[i + 1])
// et (high[j], low[i], high[j + 1]) : sans station, ceux de LambdaMesh.
void AppendWall (const std::vector<unsigned> &low, const std::vector<double> &lowAt, const std::vector<unsigned> &high,
                 const std::vector<double> &highAt, bool reversed, std::vector<std::array<unsigned, 3>> &triangles)
{
	std::size_t i = 0, j = 0;
	while (i + 1 < low.size () || j + 1 < high.size ())
	{
		const bool advanceLow = j + 1 == high.size () || (i + 1 < low.size () && lowAt[i + 1] <= highAt[j + 1]);
		if (advanceLow)
		{
			PushTriangle (triangles, high[j], low[i], low[i + 1], reversed);
			++i;
		}
		else
		{
			PushTriangle (triangles, high[j], low[i], high[j + 1], reversed);
			++j;
		}
	}
}

// Lambda a stations : sommets de faite 0 .. n - 1, puis, par bras (droit puis
// gauche), les n pieds, puis les stations de chaque trajectoire du pied vers le
// faite ; parois en fermeture eclair entre trajectoires voisines, face de pied
// au pied.
std::unique_ptr<Mesh> LambdaMeshAt (const Frame &f, const RakeStations &r, const std::vector<std::array<unsigned, 3>> &caps,
                                    bool back)
{
	const unsigned n = static_cast<unsigned> (f.depth.size ());
	auto y = [&] (unsigned k) { return back ? f.depth[k] : -f.depth[k]; };
	std::size_t stations = 0;
	for (const std::vector<double> &x : r.x)
		stations += x.size ();
	std::vector<std::array<double, 3>> positions;
	positions.reserve (3 * static_cast<std::size_t> (n) + stations);
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (6 * static_cast<std::size_t> (n) + 2 * stations);
	for (unsigned k = 0; k < n; ++k)
		positions.push_back ({0.0, y (k), f.ridge[k]});
	std::vector<std::vector<unsigned>> chain (n);
	std::vector<std::vector<double>> at (n);
	for (const bool left : {false, true})
	{
		const unsigned feet = static_cast<unsigned> (positions.size ());
		for (unsigned k = 0; k < n; ++k)
			positions.push_back ({left ? -f.foot[k] : f.foot[k], y (k), f.zBase});
		for (unsigned k = 0; k < n; ++k)
		{
			const std::vector<std::size_t> arm = ArmStations (r.x[k], left);
			chain[k].clear ();
			at[k].clear ();
			chain[k].push_back (feet + k);
			at[k].push_back (-f.foot[k]);
			for (const std::size_t i : arm)
			{
				chain[k].push_back (static_cast<unsigned> (positions.size ()));
				at[k].push_back (-std::fabs (r.x[k][i]));
				positions.push_back ({r.x[k][i], y (k), r.z[k][i]});
			}
			chain[k].push_back (k);
			at[k].push_back (0.0);
		}
		const bool reversed = back != left;
		for (unsigned k = 0; k < n; ++k)
		{
			const unsigned j = (k + 1) % n;
			AppendWall (chain[j], at[j], chain[k], at[k], reversed, triangles);
		}
		for (const std::array<unsigned, 3> &c : caps)
			PushTriangle (triangles, feet + c[0], feet + c[1], feet + c[2], !reversed);
	}
	return MakeTriangleMesh (positions, triangles);
}

// Comble a stations : base en grille ({-xTop, stations de T, xTop} x S'_Y) a
// zBase ; pans en grille, en X le pied, les stations de la trajectoire T et le
// faite, a ses cotes, les colonnes +-xTop etant celles de la base ; abouts
// y = -+yTympanum en fermeture eclair entre la base et le haut, sans les deux
// triangles plats des pieds communs.
std::unique_ptr<Mesh> RoofMeshAt (const Frame &f, const RakeStations &r, const std::vector<double> &stationsY)
{
	const std::size_t T = f.ridge.size () - 1;
	const std::vector<double> &sx = r.x[T];
	const std::vector<double> xBase = SpanWithStations (-f.xTop, f.xTop, sx);
	// Haut sans ses deux bouts : stations negatives, faite, stations positives.
	std::vector<double> xMid, zMid;
	xMid.reserve (sx.size () + 1);
	zMid.reserve (sx.size () + 1);
	for (const bool positive : {false, true})
	{
		if (positive)
		{
			xMid.push_back (0.0);
			zMid.push_back (f.ridge[T]);
		}
		for (std::size_t i = 0; i < sx.size (); ++i)
			if ((sx[i] > 0.0) == positive)
			{
				xMid.push_back (sx[i]);
				zMid.push_back (r.z[T][i]);
			}
	}
	const std::vector<double> ys = SpanWithStations (-f.yTympanum, f.yTympanum, stationsY);
	const std::size_t ny = ys.size (), nb = xBase.size (), nt = xMid.size () + 2;
	const unsigned mid = static_cast<unsigned> (nb * ny);
	auto base = [ny] (std::size_t i, std::size_t j) { return static_cast<unsigned> (i * ny + j); };
	// Colonne i du haut entier (0 et nt - 1 : bords de la base).
	auto top = [&] (std::size_t i, std::size_t j) {
		if (i == 0)
			return base (0, j);
		if (i + 1 == nt)
			return base (nb - 1, j);
		return mid + static_cast<unsigned> ((i - 1) * ny + j);
	};

	std::vector<std::array<double, 3>> positions;
	positions.reserve ((nb + xMid.size ()) * ny);
	for (const double x : xBase)
		for (const double y : ys)
			positions.push_back ({x, y, f.zBase});
	for (std::size_t i = 0; i < xMid.size (); ++i)
		for (const double y : ys)
			positions.push_back ({xMid[i], y, zMid[i]});
	std::vector<std::array<unsigned, 3>> triangles;
	triangles.reserve (2 * (nb + nt) * ny);
	auto quad = [&] (unsigned a, unsigned b, unsigned c, unsigned d) {
		triangles.push_back ({a, b, c});
		triangles.push_back ({a, c, d});
	};
	for (std::size_t i = 0; i + 1 < nb; ++i)
		for (std::size_t j = 0; j + 1 < ny; ++j)
			quad (base (i, j), base (i, j + 1), base (i + 1, j + 1), base (i + 1, j));
	for (std::size_t i = 0; i + 1 < nt; ++i)
		for (std::size_t j = 0; j + 1 < ny; ++j)
			quad (top (i, j), top (i + 1, j), top (i + 1, j + 1), top (i, j + 1));
	std::vector<double> xHigh;
	xHigh.reserve (nt);
	xHigh.push_back (-f.xTop);
	xHigh.insert (xHigh.end (), xMid.begin (), xMid.end ());
	xHigh.push_back (f.xTop);
	std::vector<unsigned> low, high;   // aretes d'un about, reutilisees
	std::vector<std::array<unsigned, 3>> about;
	low.reserve (nb);
	high.reserve (nt);
	for (const bool backAbout : {false, true})
	{
		const std::size_t j = backAbout ? ny - 1 : 0;
		low.clear ();
		high.clear ();
		about.clear ();
		for (std::size_t i = 0; i < nb; ++i)
			low.push_back (base (i, j));
		for (std::size_t i = 0; i < nt; ++i)
			high.push_back (top (i, j));
		AppendZipper (low, xBase, high, xHigh, backAbout, about);
		for (const std::array<unsigned, 3> &t : about)
			if (t[0] != t[1] && t[1] != t[2] && t[2] != t[0])
				triangles.push_back (t);
	}
	return MakeTriangleMesh (positions, triangles);
}

// Abscisses consecutives confondues une fois arrondies en float.
bool SpanCollapses (const std::vector<double> &span)
{
	for (std::size_t k = 0; k + 1 < span.size (); ++k)
		if (static_cast<float> (span[k]) == static_cast<float> (span[k + 1]))
			return true;
	return false;
}

void RequireStations (const std::vector<double> &s, const char *name)
{
	for (std::size_t k = 0; k < s.size (); ++k)
		if (!std::isfinite (s[k]) || (k > 0 && !(s[k - 1] < s[k])))
			throw std::invalid_argument (std::string ("Fronton : stations ") + name + " non finies ou non strictement croissantes");
}

} // namespace

std::vector<std::unique_ptr<Mesh>> detail::BuildPedimentPieces (const CheckedPediment &pediment, const CheckedEntablature &entablature,
                                                                const PeristyleLayout &peristyle, double zFoot,
                                                                const std::function<void (const Mesh &)> &afterEach,
                                                                const std::vector<RoofPocket> &pockets)
{
	const Prepared prepared = Prepare (pediment, entablature, peristyle, zFoot);
	const Frame &f = prepared.frame;
	const std::vector<std::array<unsigned, 3>> &caps = prepared.caps;
	for (const RoofPocket &p : pockets)
		RequirePocket (f, p);
	// Aiguillage (CE-34) : 0 poche, le prisme ; 1, le comble a poche ; plusieurs, les tranches, formees et
	// verifiees avant la premiere piece.
	std::vector<std::unique_ptr<Mesh>> slices;
	if (pockets.size () > 1)
	{
		RequireSeparatePockets (pockets);
		slices = RoofSliceMeshes (f, pockets);
	}

	std::vector<std::unique_ptr<Mesh>> meshes;
	meshes.reserve (2 + std::max<std::size_t> (1, slices.size ()));
	auto keep = [&] (std::unique_ptr<Mesh> mesh) {
		if (afterEach)
			afterEach (*mesh);
		meshes.push_back (std::move (mesh));
	};
	keep (LambdaMesh (f, caps, false));
	keep (LambdaMesh (f, caps, true));
	if (pockets.empty ())
		keep (RoofMesh (f));
	else if (pockets.size () == 1)
		keep (RoofMeshWithPocket (f, pockets.front ()));
	else
		for (std::unique_ptr<Mesh> &slice : slices)
			keep (std::move (slice));
	return meshes;
}

std::vector<std::unique_ptr<Mesh>> detail::BuildPedimentPieces (const CheckedPediment &pediment, const CheckedEntablature &entablature,
                                                                const PeristyleLayout &peristyle, double zFoot,
                                                                const RefinementStations &stations,
                                                                const std::function<void (const Mesh &)> &afterEach,
                                                                const std::vector<RoofPocket> &pockets)
{
	if (!pockets.empty ())
		Fail ("roofPocket", "ne se combine pas avec les stations du raffinement");
	const Prepared prepared = Prepare (pediment, entablature, peristyle, zFoot);
	const Frame &f = prepared.frame;
	RequireStations (stations.x, "x");
	RequireStations (stations.yLower, "yLower");
	const RakeStations r = RakeStationsOf (f, stations.x);
	bool collapse = SpanCollapses (SpanWithStations (-f.yTympanum, f.yTympanum, stations.yLower));
	for (std::size_t k = 0; k < r.x.size () && !collapse; ++k)
	{
		std::vector<double> span {-f.foot[k]};
		span.insert (span.end (), r.x[k].begin (), r.x[k].end ());
		span.push_back (f.foot[k]);
		collapse = SpanCollapses (span) || SpanCollapses (std::vector<double> {-f.foot[k], 0.0, f.foot[k]}) ||
		           std::any_of (r.x[k].begin (), r.x[k].end (), [] (double s) { return static_cast<float> (s) == 0.0f; });
	}
	if (collapse)
		throw std::invalid_argument ("Fronton : sommets confondus en float : une station se confond avec une voisine, un pied "
		                             "ou le faite une fois arrondie");

	std::vector<std::unique_ptr<Mesh>> meshes;
	meshes.reserve (3);
	auto keep = [&] (std::unique_ptr<Mesh> mesh) {
		if (afterEach)
			afterEach (*mesh);
		meshes.push_back (std::move (mesh));
	};
	keep (LambdaMeshAt (f, r, prepared.caps, false));
	keep (LambdaMeshAt (f, r, prepared.caps, true));
	// Sans station dans le comble : le comble d'origine, au bit pres.
	const bool bareRoof = r.x.back ().empty () && std::none_of (stations.yLower.begin (), stations.yLower.end (),
	                                                             [&] (double y) { return -f.yTympanum < y && y < f.yTympanum; });
	keep (bareRoof ? RoofMesh (f) : RoofMeshAt (f, r, stations.yLower));
	return meshes;
}

detail::CheckedPediment detail::CheckPediment (const GreekPedimentSettings &p, const CheckedEntablature &e)
{
	ValidateOwn (p, e.Settings ());
	return CheckedPediment (p, e.Settings ());
}

void detail::RequirePedimentTopWithinCrown (const CheckedPediment &p, double halfWidth)
{
	RequireTopWithinCrown (PedimentFeet (p).back (), halfWidth + p.Entablature ().bands.back ().topProjection);
}

std::vector<double> detail::PedimentFeet (const CheckedPediment &p)
{
	const GreekPedimentSettings &s = p.Settings ();
	std::vector<double> feet = Rises (s);
	for (double &x : feet)
		x = s.rakingRun * x;
	return feet;
}

bool detail::PocketMeetsRoof (const CheckedPediment &p, const CheckedEntablature &e, const PeristyleLayout &peristyle, double zFoot,
                              const RoofPocket &q)
{
	const Frame f = FrameOf (p.Settings (), e.Settings (), peristyle, zFoot);
	const double zRidge = f.ridge.back ();
	auto r = [] (double v) { return static_cast<double> (static_cast<float> (v)); };
	return !PocketConflict (q.x0, q.x1, q.y0, q.y1, q.zTop, f.xTop, f.yTympanum, f.zBase, zRidge).first.empty () ||
	       !PocketConflict (r (q.x0), r (q.x1), r (q.y0), r (q.y1), r (q.zTop), r (f.xTop), r (f.yTympanum), r (f.zBase),
	                        r (zRidge)).first.empty ();
}

std::vector<std::unique_ptr<Mesh>> BuildGreekPedimentPieceMeshes (const GreekPedimentSettings &pediment,
                                                                  const GreekEntablatureSettings &entablature,
                                                                  const PeristyleLayout &peristyle, double zFoot,
                                                                  const std::function<void (const Mesh &)> &afterEach,
                                                                  const std::vector<RoofPocket> &pockets)
{
	const detail::CheckedEntablature e = detail::CheckEntablature (entablature);
	return detail::BuildPedimentPieces (detail::CheckPediment (pediment, e), e, peristyle, zFoot, afterEach, pockets);
}

std::vector<std::unique_ptr<Mesh>> BuildGreekPedimentPieceMeshes (const GreekPedimentSettings &pediment,
                                                                  const GreekEntablatureSettings &entablature,
                                                                  const PeristyleLayout &peristyle, double zFoot,
                                                                  const RefinementStations &stations,
                                                                  const std::function<void (const Mesh &)> &afterEach,
                                                                  const std::vector<RoofPocket> &pockets)
{
	const detail::CheckedEntablature e = detail::CheckEntablature (entablature);
	return detail::BuildPedimentPieces (detail::CheckPediment (pediment, e), e, peristyle, zFoot, stations, afterEach, pockets);
}

std::size_t detail::PedimentOwnValidations () noexcept
{
	return pedimentOwnValidations;
}

} // namespace classical
} // namespace cgarchi
