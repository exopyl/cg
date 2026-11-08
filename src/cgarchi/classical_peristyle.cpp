#include <cgarchi/classical_peristyle.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void Fail (const char *name, const std::string &rule)
{
	throw std::invalid_argument (std::string ("LayoutPeristyle : '") + name + "' " + rule);
}

void RequireColumnCount (const char *name, int count)
{
	if (count < 2 || count > kPeristyleMaxColumnsPerSide)
		Fail (name, "= " + std::to_string (count) + " hors de [2, " + std::to_string (kPeristyleMaxColumnsPerSide) + "]");
}

void RequireSpacing (const char *name, double spacing)
{
	if (!(std::isfinite (spacing) && spacing > 0.0))
		Fail (name, "doit etre fini et strictement positif");
}

// Axe i d'une rangee de n colonnes a l'entraxe s (expression de LayoutColonnade).
double RowAxis (int i, int n, double s)
{
	return (i - 0.5 * (n - 1)) * s;
}

// Axe i d'une rangee de n colonnes, entraxe courant s, entraxe d'angle a. Sans
// contraction, l'axe de LayoutColonnade ; avec, les axes interieurs restent
// ceux-ci et les deux angles sont a +-(axe n - 2 + a).
double ContractedRowAxis (int i, int n, double s, double a)
{
	if (a == s)
		return RowAxis (i, n, s);
	const double corner = RowAxis (n - 2, n, s) + a;
	if (i == 0)
		return -corner;
	if (i == n - 1)
		return corner;
	return RowAxis (i, n, s);
}

// Une rangee est representable si sa demi-longueur est finie et si ses axes
// voisins sont distincts (un entraxe sous-normal peut les confondre).
void RequireRepresentableRow (const char *name, int n, double s, double halfLength)
{
	bool representable = std::isfinite (halfLength);
	for (int i = 1; representable && i < n; ++i)
		representable = RowAxis (i - 1, n, s) != RowAxis (i, n, s);
	if (!representable)
		Fail (name, "donne des axes non representables en double");
}

// Rangee contractee : axes interieurs representables (entraxe courant), puis
// axes d'angle finis et distincts de leur voisin (entraxe d'angle).
void RequireRepresentableContractedRow (const char *spacingName, const char *cornerName, int n, double s, double a)
{
	bool representable = std::isfinite (RowAxis (n - 2, n, s));
	for (int i = 2; representable && i <= n - 2; ++i)
		representable = RowAxis (i - 1, n, s) != RowAxis (i, n, s);
	if (!representable)
		Fail (spacingName, "donne des axes non representables en double");
	const double corner = RowAxis (n - 2, n, s) + a;
	if (!(std::isfinite (corner) && corner != RowAxis (n - 2, n, s)))
		Fail (cornerName, "donne un axe d'angle non representable en double");
}

// Colonnes dans l'ordre du contrat, depuis les axes des deux rangees, croissants.
void PlaceColumns (PeristyleLayout &layout, const std::vector<double> &xs, const std::vector<double> &ys)
{
	const int front = layout.front, flank = layout.flank;
	layout.halfWidth = xs.back ();
	layout.halfDepth = ys.back ();
	layout.columns.reserve (static_cast<std::size_t> (2 * (front + flank) - 4));
	for (int i = 0; i < front - 1; ++i)
		layout.columns.push_back ({ xs[i], ys.front (), PeristyleSide::Front, i == 0 });
	for (int j = 0; j < flank - 1; ++j)
		layout.columns.push_back ({ xs.back (), ys[j], PeristyleSide::Right, j == 0 });
	for (int i = 0; i < front - 1; ++i)
		layout.columns.push_back ({ xs[front - 1 - i], ys.back (), PeristyleSide::Back, i == 0 });
	for (int j = 0; j < flank - 1; ++j)
		layout.columns.push_back ({ xs.front (), ys[flank - 1 - j], PeristyleSide::Left, j == 0 });
}

} // namespace

PeristyleLayout LayoutPeristyle (int front, int flank, double frontSpacing, double flankSpacing)
{
	RequireColumnCount ("front", front);
	RequireColumnCount ("flank", flank);
	RequireSpacing ("frontSpacing", frontSpacing);
	RequireSpacing ("flankSpacing", flankSpacing);
	RequireRepresentableRow ("frontSpacing", front, frontSpacing, 0.5 * (front - 1) * frontSpacing);
	RequireRepresentableRow ("flankSpacing", flank, flankSpacing, 0.5 * (flank - 1) * flankSpacing);

	PeristyleLayout layout;
	layout.front = front;
	layout.flank = flank;
	layout.frontSpacing = layout.frontCornerSpacing = frontSpacing;
	layout.flankSpacing = layout.flankCornerSpacing = flankSpacing;
	std::vector<double> xs (static_cast<std::size_t> (front)), ys (static_cast<std::size_t> (flank));
	for (int i = 0; i < front; ++i)
		xs[i] = RowAxis (i, front, frontSpacing);
	for (int j = 0; j < flank; ++j)
		ys[j] = RowAxis (j, flank, flankSpacing);
	PlaceColumns (layout, xs, ys);
	return layout;
}

PeristyleLayout LayoutPeristyle (const PeristyleSettings &s)
{
	RequireColumnCount ("front", s.front);
	RequireColumnCount ("flank", s.flank);
	RequireSpacing ("frontSpacing", s.frontSpacing);
	RequireSpacing ("flankSpacing", s.flankSpacing);
	RequireSpacing ("frontCornerSpacing", s.frontCornerSpacing);
	RequireSpacing ("flankCornerSpacing", s.flankCornerSpacing);
	if (s.frontCornerSpacing != s.frontSpacing && s.front < 4)
		Fail ("frontCornerSpacing", "differe de 'frontSpacing' sur une rangee de moins de 4 colonnes");
	if (s.flankCornerSpacing != s.flankSpacing && s.flank < 4)
		Fail ("flankCornerSpacing", "differe de 'flankSpacing' sur une rangee de moins de 4 colonnes");
	if (s.frontCornerSpacing == s.frontSpacing)
		RequireRepresentableRow ("frontSpacing", s.front, s.frontSpacing, 0.5 * (s.front - 1) * s.frontSpacing);
	else
		RequireRepresentableContractedRow ("frontSpacing", "frontCornerSpacing", s.front, s.frontSpacing,
		                                   s.frontCornerSpacing);
	if (s.flankCornerSpacing == s.flankSpacing)
		RequireRepresentableRow ("flankSpacing", s.flank, s.flankSpacing, 0.5 * (s.flank - 1) * s.flankSpacing);
	else
		RequireRepresentableContractedRow ("flankSpacing", "flankCornerSpacing", s.flank, s.flankSpacing,
		                                   s.flankCornerSpacing);

	PeristyleLayout layout;
	layout.front = s.front;
	layout.flank = s.flank;
	layout.frontSpacing = s.frontSpacing;
	layout.flankSpacing = s.flankSpacing;
	layout.frontCornerSpacing = s.frontCornerSpacing;
	layout.flankCornerSpacing = s.flankCornerSpacing;
	std::vector<double> xs (static_cast<std::size_t> (s.front)), ys (static_cast<std::size_t> (s.flank));
	for (int i = 0; i < s.front; ++i)
		xs[i] = ContractedRowAxis (i, s.front, s.frontSpacing, s.frontCornerSpacing);
	for (int j = 0; j < s.flank; ++j)
		ys[j] = ContractedRowAxis (j, s.flank, s.flankSpacing, s.flankCornerSpacing);
	PlaceColumns (layout, xs, ys);
	return layout;
}

} // namespace classical
} // namespace cgarchi
