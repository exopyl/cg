#include <cgarchi/classical_partition.h>

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

[[noreturn]] void Fail (std::string_view field, const std::string &rule)
{
	throw std::invalid_argument ("Partition : '" + std::string (field) + "' " + rule);
}

[[noreturn]] void FailAt (std::string_view field, std::size_t index, const std::string &rule)
{
	throw std::invalid_argument ("Partition : '" + std::string (field) + "[" + std::to_string (index) + "]' " + rule);
}

} // namespace

std::vector<double> CumulativeBounds (double start, const std::vector<double> &lengths, std::string_view field)
{
	if (!std::isfinite (start))
		Fail (field, "a un debut non fini");
	std::vector<double> bounds;
	bounds.reserve (lengths.size () + 1);
	bounds.push_back (start);
	for (std::size_t k = 0; k < lengths.size (); ++k)
	{
		if (!(std::isfinite (lengths[k]) && lengths[k] > 0.0))
			FailAt (field, k, "doit etre une longueur finie et strictement positive");
		const double next = bounds.back () + lengths[k];
		if (!std::isfinite (next))
			FailAt (field, k, "donne une borne non finie");
		bounds.push_back (next);
	}
	return bounds;
}

std::vector<double> EqualParts (double lo, double hi, int n, std::string_view field)
{
	if (n < 1)
		Fail (field, "demande au moins une partie");
	if (!(std::isfinite (lo) && std::isfinite (hi)))
		Fail (field, "a une borne non finie");
	if (!(lo < hi))
		Fail (field, "a un intervalle vide");
	std::vector<double> bounds (static_cast<std::size_t> (n) + 1);
	bounds.front () = lo;
	bounds.back () = hi;
	const double count = static_cast<double> (n);
	for (int k = 1; k < n; ++k)
	{
		// (n - k) lo + k hi : les deux produits s'echangent par negation et
		// renversement, d'ou la symetrie au bit pres -- a condition que chacun
		// soit arrondi en double avant la somme. Une contraction en FMA (GCC et
		// Clang la font sur une cible a FMA, au travers des instructions pour
		// GCC) garderait l'un des deux exact et briserait cet echange. La
		// lecture d'un volatile n'est pas un produit : rien a contracter, sur
		// aucun compilateur, quels que soient ses drapeaux.
		const volatile double left = static_cast<double> (n - k) * lo;
		const volatile double right = static_cast<double> (k) * hi;
		const double b = (left + right) / count;
		if (!std::isfinite (b))
			FailAt (field, static_cast<std::size_t> (k), "donne une borne non finie");
		bounds[static_cast<std::size_t> (k)] = b;
	}
	return bounds;
}

std::vector<Interval> GapsBetweenCentred (const std::vector<double> &centres, double width, std::string_view field)
{
	if (!(std::isfinite (width) && width > 0.0))
		Fail (field, "doit etre une largeur finie et strictement positive");
	for (std::size_t k = 0; k < centres.size (); ++k)
	{
		if (!std::isfinite (centres[k]))
			FailAt (field, k, "a un centre non fini");
		if (k > 0 && !(centres[k - 1] < centres[k]))
			FailAt (field, k, "a des centres non strictement croissants");
	}
	std::vector<Interval> gaps;
	if (centres.size () < 2)
		return gaps;
	gaps.reserve (centres.size () - 1);
	const double half = 0.5 * width;
	for (std::size_t k = 0; k + 1 < centres.size (); ++k)
	{
		const Interval gap {centres[k] + half, centres[k + 1] - half};
		if (!(gap.lo < gap.hi))
			FailAt (field, k, "laisse un intervalle vide entre deux parties");
		gaps.push_back (gap);
	}
	return gaps;
}

} // namespace classical
} // namespace cgarchi
