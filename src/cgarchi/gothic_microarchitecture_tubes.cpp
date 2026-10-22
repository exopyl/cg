#include <cgarchi/gothic_microarchitecture_tubes.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include <locale>

#include <cgmath/TVector3.h>
#include "surface_basic.h"

namespace cgarchi::microarch
{

namespace
{
	// Diagonale du repere canonique dans lequel CreateTubes est appele.
	constexpr double kCanonicalDiagonal = 256.0;
	// Distance minimale, dans ce repere, entre deux echantillons gardes : au-dessus de
	// la diagonale d'une cellule de la grille de CreateTubes (sqrt(3) * 1e-4).
	constexpr double kMinCanonicalStep = 4e-4;
	// CreateTubes soude en boucle une polyligne d'au moins 4 points dont les bouts sont
	// a moins de r/2 (surface_basic.cpp:817-821). Marge relative sur le seuil au carre,
	// pour couvrir les arrondis float de CreateTubes.
	constexpr std::size_t kWeldMinPoints = 4;
	constexpr double kWeldMargin = 1e-3;

	std::string number (double v)
	{
		std::ostringstream os;
		os.precision(10);
		os << v;
		return os.str();
	}

	bool validParams (const TubeParams &p, MicroarchReport &report)
	{
		const std::size_t before = report.ErrorCount();
		if (!(std::isfinite(p.radius_ratio) && p.radius_ratio > 0.0 && p.radius_ratio <= TubeLimits::max_radius_ratio))
			report.Record(MicroarchError::TubeInvalidParameter, "tubes.radius_ratio",
			              number(p.radius_ratio) + " outside (0, " + number(TubeLimits::max_radius_ratio) + "]");
		if (p.sides < TubeLimits::min_sides || p.sides > TubeLimits::max_sides)
			report.Record(MicroarchError::TubeInvalidParameter, "tubes.sides",
			              std::to_string(p.sides) + " outside [" + std::to_string(TubeLimits::min_sides) + ", "
			              + std::to_string(TubeLimits::max_sides) + "]");
		const double a = p.sampling.max_angle;
		if (!(std::isfinite(a) && a > 0.0 && a <= TubeLimits::max_angle))
			report.Record(MicroarchError::TubeInvalidParameter, "tubes.sampling.max_angle",
			              number(a) + " outside (0, pi/8]");
		return report.ErrorCount() == before;
	}

	// Repere canonique : centre de la boite 3D des echantillons, diagonale ramenee a 256.
	struct Frame
	{
		Vector3d center;
		double   toCanonical     = 0.0;
		double   radiusCanonical = 0.0;
	};

	bool frameOf (const Network &network, const SamplingParams &sampling, double radius, Frame &frame)
	{
		const double inf = std::numeric_limits<double>::infinity();
		double lo[3] = {inf, inf, inf}, hi[3] = {-inf, -inf, -inf};
		for (const SpaceCurve &c : network.curves)
			for (double s : SampleParameters(c, sampling))
			{
				const Vector3d p = PointAt(c, s);
				const double q[3] = {p.x, p.y, p.z};
				for (int k = 0; k < 3; ++k)
				{
					lo[k] = std::min(lo[k], q[k]);
					hi[k] = std::max(hi[k], q[k]);
				}
			}
		const double ex = hi[0] - lo[0], ey = hi[1] - lo[1], ez = hi[2] - lo[2];
		const double E = std::sqrt(ex * ex + ey * ey + ez * ez);
		if (!(E > 0.0 && std::isfinite(E)))
			return false;
		frame.center          = Vector3d(0.5 * (lo[0] + hi[0]), 0.5 * (lo[1] + hi[1]), 0.5 * (lo[2] + hi[2]));
		frame.toCanonical     = kCanonicalDiagonal / E;
		frame.radiusCanonical = radius * frame.toCanonical;
		return true;
	}

	// Polyligne canonique d'une courbe, prefiltree : un echantillon a moins de
	// kMinCanonicalStep du dernier garde est retire ; le dernier est toujours garde et
	// remplace le precedent s'il en est trop proche.
	std::vector<Vector3f> canonicalPolyline (const SpaceCurve &c, const SamplingParams &sampling, const Frame &f)
	{
		std::vector<Vector3f> poly;
		double lx = 0.0, ly = 0.0, lz = 0.0;
		const std::vector<double> samples = SampleParameters(c, sampling);
		for (std::size_t k = 0; k < samples.size(); ++k)
		{
			const Vector3d p = PointAt(c, samples[k]);
			const double qx = (p.x - f.center.x) * f.toCanonical;
			const double qy = (p.y - f.center.y) * f.toCanonical;
			const double qz = (p.z - f.center.z) * f.toCanonical;
			const bool last = k + 1 == samples.size();
			const bool close = !poly.empty()
			                && std::sqrt((qx - lx) * (qx - lx) + (qy - ly) * (qy - ly) + (qz - lz) * (qz - lz)) < kMinCanonicalStep;
			if (close && !last)
				continue;
			if (close && last && poly.size() > 1)
				poly.pop_back();
			poly.emplace_back(static_cast<float>(qx), static_cast<float>(qy), static_cast<float>(qz));
			lx = qx; ly = qy; lz = qz;
		}
		return poly;
	}

	// Morceaux [lo, hi] (indices inclus, extremites partagees) qu'aucun appel a
	// CreateTubes ne soude en boucle : coupe recursive au milieu tant qu'un morceau
	// d'au moins kWeldMinPoints points a ses bouts a moins de r/2.
	void openPieces (const std::vector<Vector3f> &poly, std::size_t lo, std::size_t hi, double radius,
	                 std::vector<std::pair<std::size_t, std::size_t>> &out)
	{
		const double dx = double(poly[hi].x) - poly[lo].x, dy = double(poly[hi].y) - poly[lo].y, dz = double(poly[hi].z) - poly[lo].z;
		const bool welds = hi - lo + 1 >= kWeldMinPoints
		                && dx * dx + dy * dy + dz * dz < 0.25 * radius * radius * (1.0 + kWeldMargin);
		if (!welds)
		{
			out.emplace_back(lo, hi);
			return;
		}
		const std::size_t mid = lo + (hi - lo) / 2;
		openPieces(poly, lo, mid, radius, out);
		openPieces(poly, mid, hi, radius, out);
	}

	std::vector<std::pair<std::size_t, std::size_t>> piecesOf (const std::vector<Vector3f> &poly, double radius)
	{
		std::vector<std::pair<std::size_t, std::size_t>> out;
		if (poly.size() >= 2)
			openPieces(poly, 0, poly.size() - 1, radius, out);
		return out;
	}

	// Triangles d'un tube ouvert de n anneaux et K cotes : 2K par segment, K - 2 par capuchon.
	std::size_t pieceTriangles (std::size_t n, std::size_t K)
	{
		return n >= 2 ? 2 * K * (n - 1) + 2 * (K - 2) : 0;
	}
}

std::size_t CountTubeTriangles (const Network &network, const TubeParams &params, std::size_t limit)
{
	if (params.sides < TubeLimits::min_sides)
		return 0;
	if (CountNetworkSamples(network, params.sampling, SamplingLimits::max_network_points) > SamplingLimits::max_network_points)
		return std::numeric_limits<std::size_t>::max();
	const std::size_t K = static_cast<std::size_t>(params.sides);
	Frame frame;
	if (!frameOf(network, params.sampling, params.radius_ratio * network.tol.scale, frame))
		return 0;
	std::size_t total = 0;
	for (const SpaceCurve &c : network.curves)
	{
		const std::vector<Vector3f> poly = canonicalPolyline(c, params.sampling, frame);
		for (const auto &[lo, hi] : piecesOf(poly, frame.radiusCanonical))
			total += pieceTriangles(hi - lo + 1, K);
		if (total > limit)
			return total;
	}
	return total;
}

TubeMesh BuildTubeMesh (const Network &network, const TubeParams &params, MicroarchReport &report,
                        const TubeLimits &limits)
{
	TubeMesh out;
	if (!validParams(params, report))
		return out;

	const std::size_t points = CountNetworkSamples(network, params.sampling, SamplingLimits::max_network_points);
	if (points > SamplingLimits::max_network_points)
	{
		report.Record(MicroarchError::SampleLimitExceeded, "sampling.max_angle",
		              "sampling the network would produce more than " + std::to_string(SamplingLimits::max_network_points)
		              + " points (max_angle = " + number(params.sampling.max_angle) + ") ; increase max_angle");
		return out;
	}
	auto refuse = [&](std::size_t triangles) {
		report.Record(MicroarchError::TubeLimitExceeded, "tubes",
		              std::to_string(triangles) + " triangles > " + std::to_string(limits.max_triangles)
		              + " ; reduce tubes.sides, increase tubes.sampling.max_angle, or simplify the network");
		return TubeMesh{};
	};
	// Controle prealable, sans allocation du maillage : compte exact, meme prefiltre et
	// meme decoupe que la construction. Compte complet, pour que le message le donne.
	const std::size_t K = static_cast<std::size_t>(params.sides);
	const std::size_t expected = CountTubeTriangles(network, params, std::numeric_limits<std::size_t>::max());
	if (expected > limits.max_triangles)
		return refuse(expected);

	const double radius = params.radius_ratio * network.tol.scale;
	Frame frame;
	if (!frameOf(network, params.sampling, radius, frame))
		return out;
	const double toWorld = 1.0 / frame.toCanonical;

	std::vector<float> vertices;
	std::vector<unsigned int> faces;
	vertices.reserve(3 * K * points);
	faces.reserve(3 * expected);
	out.vertex_begin.reserve(network.curves.size() + 1);
	out.triangle_begin.reserve(network.curves.size() + 1);
	for (const SpaceCurve &c : network.curves)
	{
		out.vertex_begin.push_back(vertices.size() / 3);
		out.triangle_begin.push_back(faces.size() / 3);
		const std::vector<Vector3f> poly = canonicalPolyline(c, params.sampling, frame);
		for (const auto &[lo, hi] : piecesOf(poly, frame.radiusCanonical))
		{
			const std::vector<Vector3f> piece(poly.begin() + static_cast<std::ptrdiff_t>(lo),
			                                  poly.begin() + static_cast<std::ptrdiff_t>(hi) + 1);
			const std::unique_ptr<Mesh> tube(CreateTubes({piece}, static_cast<float>(frame.radiusCanonical),
			                                             static_cast<int>(K), true));
			if (!tube)
				continue;
			const unsigned int nf = tube->GetNFaces();
			// Filet de securite : le compte exact ci-dessus suppose la disposition de
			// CreateTubes (un anneau par echantillon, aucune enveloppe).
			if (faces.size() / 3 + nf > limits.max_triangles)
				return refuse(faces.size() / 3 + nf);
			const std::size_t offset = vertices.size() / 3;
			const std::vector<float> &v = tube->GetVertices();
			// Sommets relatifs a l'origine du maillage (centre de la boite) : leur
			// precision ne depend pas de la position du dessin.
			for (float x : v)
				vertices.push_back(static_cast<float>(x * toWorld));
			for (unsigned int f = 0; f < nf; ++f)
			{
				const auto face = tube->FaceAt(f);
				if (face->GetNVertices() != 3)
					continue;
				for (int k = 0; k < 3; ++k)
					faces.push_back(static_cast<unsigned int>(face->GetVertex(static_cast<unsigned int>(k)) + offset));
			}
		}
	}
	out.vertex_begin.push_back(vertices.size() / 3);
	out.triangle_begin.push_back(faces.size() / 3);

	out.mesh = std::make_unique<Mesh>();
	out.mesh->SetVertices(static_cast<unsigned int>(vertices.size() / 3), vertices.data());
	out.mesh->SetFaces(static_cast<unsigned int>(faces.size() / 3), 3, faces.data());
	out.mesh->ComputeNormals();
	out.radius = radius;
	out.origin = frame.center;

	std::size_t hidden = 0;
	double smallest = std::numeric_limits<double>::infinity();
	for (const VisualCrossing &x : network.crossings)
		if (x.min_dz <= 2.0 * radius)
		{
			++hidden;
			smallest = std::min(smallest, x.min_dz);
		}
	if (hidden > 0)
		report.Record(MicroarchWarning::TubeRadiusHidesCrossing, "tubes.radius_ratio",
		              std::to_string(hidden) + " visual crossings have min_dz <= 2 r (smallest " + number(smallest)
		              + ") : the tubes touch there ; radius_ratio below " + number(smallest / (2.0 * network.tol.scale))
		              + " keeps them apart");
	return out;
}

std::string TubeMeshToObj (const Mesh &mesh, const Vector3d &origin)
{
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os.precision(std::numeric_limits<double>::max_digits10);
	const std::vector<float> &v = mesh.GetVertices();
	os << "# gothic-microarchitecture tubes : " << v.size() / 3 << " vertices ; " << mesh.GetNFaces() << " triangles\n";
	for (std::size_t k = 0; k + 2 < v.size(); k += 3)
		os << "v " << static_cast<double>(v[k]) + origin.x << " " << static_cast<double>(v[k + 1]) + origin.y << " "
		   << static_cast<double>(v[k + 2]) + origin.z << "\n";
	for (unsigned int f = 0; f < mesh.GetNFaces(); ++f)
	{
		const auto face = mesh.FaceAt(f);
		if (face->GetNVertices() != 3)
			continue;
		os << "f " << face->GetVertex(0) + 1 << " " << face->GetVertex(1) + 1 << " " << face->GetVertex(2) + 1 << "\n";
	}
	return os.str();
}

} // namespace cgarchi::microarch
