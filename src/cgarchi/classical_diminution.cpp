#include <cgarchi/classical_diminution.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cgarchi
{
namespace classical
{

double CosineShaftRadius (double y, double H, double lowerRadius, double upperRadius)
{
	const double third = H / 3.0;
	if (y <= third)
		return lowerRadius;
	const double u = std::min (1.0, (y - third) / (2.0 * third));
	return lowerRadius * std::cos (u * std::acos (upperRadius / lowerRadius));
}

Conchoid::Conchoid (double H, double upperRadius, double cd)
	: m_H (H), m_upper (upperRadius), m_cd (cd), m_e (0.0)
{
	if (!(H > 0.0) || !(upperRadius > 0.0) || !(cd > upperRadius))
		throw std::invalid_argument ("Conchoid : il faut H > 0 et CD > r_sup > 0");
	const double k = std::sqrt (cd * cd - upperRadius * upperRadius);
	m_e = upperRadius * (2.0 * H / (3.0 * k) - 1.0);
	if (!(m_e > 0.0))
		throw std::invalid_argument ("Conchoid : pole E non defini (e <= 0)");
}

void Conchoid::At (double t, double &radius, double &y) const
{
	const double d = std::sqrt (m_e * m_e + t * t);
	radius = m_cd * m_e / d;
	y = m_H / 3.0 + t * (1.0 + m_cd / d);
}

double Conchoid::Radius (double y) const
{
	// y(t) est strictement croissant et |y(t) - H/3| >= |t| : la racine est
	// dans [-|y - H/3|, |y - H/3|].
	const double target = y;
	const double span = std::fabs (target - m_H / 3.0) + 1.0;
	double lo = -span;
	double hi = span;
	double r = 0.0, yy = 0.0;
	for (int i = 0; i < 200; ++i)
	{
		const double mid = 0.5 * (lo + hi);
		At (mid, r, yy);
		if (yy < target)
			lo = mid;
		else
			hi = mid;
		if (hi - lo <= 1e-14 * std::max (1.0, std::fabs (mid)))
			break;
	}
	At (0.5 * (lo + hi), r, yy);
	return r;
}

double SolveConchoidCD (double H, double upperRadius, double lowerRadius)
{
	// Sur l'intervalle de validite (CD > r_sup, e > 0), r(0) part de r_sup
	// (CD -> r_sup : la conchoide tend vers la verticale r = CD) et retombe
	// vers 0 (e -> 0) : il n'est pas monotone. La racine retenue est la
	// PREMIERE en partant de r_sup, celle du contour regulier ; elle est
	// encadree par balayage puis affinee par bissection.
	const double kMax = 2.0 * H / 3.0;
	const double cdMin = upperRadius * (1.0 + 1e-9);
	const double cdMax = std::sqrt (kMax * kMax + upperRadius * upperRadius) * (1.0 - 1e-9);
	auto f = [&] (double cd) { return Conchoid (H, upperRadius, cd).Radius (0.0) - lowerRadius; };

	constexpr int kSteps = 4096;
	double lo = cdMin;
	double flo = f (lo);
	double hi = lo;
	bool bracketed = false;
	for (int i = 1; i <= kSteps && !bracketed; ++i)
	{
		hi = cdMin + (cdMax - cdMin) * i / kSteps;
		const double fhi = f (hi);
		if ((flo < 0.0) != (fhi < 0.0))
			bracketed = true;
		else
		{
			lo = hi;
			flo = fhi;
		}
	}
	if (!bracketed)
		throw std::invalid_argument ("SolveConchoidCD : rayon au pied hors d'atteinte");
	for (int i = 0; i < 200 && hi - lo > 1e-15 * hi; ++i)
	{
		const double mid = 0.5 * (lo + hi);
		const double fm = f (mid);
		if ((fm < 0.0) == (flo < 0.0))
		{
			lo = mid;
			flo = fm;
		}
		else
			hi = mid;
	}
	return 0.5 * (lo + hi);
}

ShaftProfile::ShaftProfile (const OrderData &order)
{
	const double P = order.partsPerModule;
	m_H = order.general.shaft_M * P;
	m_lower = order.general.lowerDiameter_p / 2.0;
	m_upper = order.general.upperDiameter_p / 2.0;
	m_method = order.general.diminution;
	if (m_method == DiminutionMethod::Conchoid)
	{
		m_cd = SolveConchoidCD (m_H, m_upper, m_lower);
		m_conchoid.emplace (m_H, m_upper, m_cd);
	}
}

double ShaftProfile::Radius (double y) const
{
	const double yc = std::clamp (y, 0.0, m_H);
	if (m_conchoid)
		return m_conchoid->Radius (yc);
	return CosineShaftRadius (yc, m_H, m_lower, m_upper);
}

} // namespace classical
} // namespace cgarchi
