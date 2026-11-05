#include <cgarchi/classical_entablature.h>

#include <array>
#include <stdexcept>

#include <cgarchi/classical_mesh.h>

namespace cgarchi
{
namespace classical
{

std::vector<BandOverride> OrnamentBackOverrides (const OrderData &order)
{
	std::vector<BandOverride> out;
	const EntablatureOrnaments &e = order.entablatureOrnaments;
	auto add = [&] (EntablatureOrnament ornament, double back) {
		const OrnamentMember *link = EntablatureOrnamentMember (order.key, ornament);
		if (link)
			out.push_back ({link->block, link->member, back});
	};
	if (e.dentils)
		add (EntablatureOrnament::Dentils, e.dentils->back);
	if (e.mutules)
		add (EntablatureOrnament::Mutules, e.mutules->back);
	if (e.modillions)
	{
		// Le modillon pend de la sous-face du larmier : son dos est a sa face
		// moins sa longueur.
		add (EntablatureOrnament::Modillions,
		     LinkedOrnamentMember (order, EntablatureOrnament::Modillions).projection.main.bottom - e.modillions->length);
	}
	if (e.blocks)
	{
		add (EntablatureOrnament::Blocks, LinkedOrnamentMember (order, EntablatureOrnament::Blocks).projection.main.bottom - e.blocks->depth);
	}
	return out;
}

ColumnProfile BuildEntablatureSection (const OrderData &order, const EntablatureOptions &options)
{
	ColumnOptions column;
	column.order = order.order;
	column.doricCornice = options.doricCornice;
	column.curveSegments = options.curveSegments;
	const ColumnProfile section =
		BuildEntablatureProfile (order, column, options.ornaments ? OrnamentBackOverrides (order) : std::vector<BandOverride> {});
	for (const ProfileVertex &v : section.vertices)
		if (v.ring.shape != RingShape::Square)
			throw std::invalid_argument ("BuildEntablatureSection : anneau non carre dans l'entablement de " + order.key);
	return section;
}

std::unique_ptr<Mesh> SweepEntablature (const ColumnProfile &section, double halfLength, double scale, double z0,
                                        bool closeEnds)
{
	const std::vector<ProfileVertex> &pv = section.vertices;
	if (pv.size () < 2)
		throw std::invalid_argument ("SweepEntablature : profil de moins de deux sommets");
	if (!(halfLength >= 0.0))
		throw std::invalid_argument ("SweepEntablature : demi-longueur negative");

	std::vector<std::array<double, 3>> positions;
	std::vector<std::array<unsigned, 3>> triangles;
	for (const ProfileVertex &v : pv)
	{
		if (v.ring.shape != RingShape::Square)
			throw std::invalid_argument ("SweepEntablature : anneau non carre");
		const double d = v.ring.radius, L = halfLength, z = z0 + v.y;
		// Sens trigonometrique vu de dessus.
		positions.push_back ({(-L - d) * scale, -d * scale, z * scale});
		positions.push_back ({(L + d) * scale, -d * scale, z * scale});
		positions.push_back ({(L + d) * scale, d * scale, z * scale});
		positions.push_back ({(-L - d) * scale, d * scale, z * scale});
	}
	for (unsigned i = 0; i + 1 < pv.size (); ++i)
		for (unsigned k = 0; k < 4; ++k)
		{
			const unsigned a = 4 * i + k, b = 4 * i + (k + 1) % 4, c = 4 * (i + 1) + (k + 1) % 4, d = 4 * (i + 1) + k;
			triangles.push_back ({a, b, c});
			triangles.push_back ({a, c, d});
		}
	if (closeEnds)
	{
		const unsigned t = 4 * static_cast<unsigned> (pv.size () - 1);
		triangles.push_back ({0, 2, 1});
		triangles.push_back ({0, 3, 2});
		triangles.push_back ({t, t + 1, t + 2});
		triangles.push_back ({t, t + 2, t + 3});
	}

	return MakeTriangleMesh (positions, triangles);
}

double SweptVolume (const ColumnProfile &section, double halfLength)
{
	// Rectangle (2L + 2d) x 2d, d affine en y sur chaque segment.
	double volume = 0.0;
	const double L = halfLength;
	for (std::size_t i = 1; i < section.vertices.size (); ++i)
	{
		const double d0 = section.vertices[i - 1].ring.radius, d1 = section.vertices[i].ring.radius;
		const double h = section.vertices[i].y - section.vertices[i - 1].y;
		volume += h * (4.0 * L * 0.5 * (d0 + d1) + 4.0 * (d0 * d0 + d0 * d1 + d1 * d1) / 3.0);
	}
	return volume;
}

} // namespace classical
} // namespace cgarchi
