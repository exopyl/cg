#pragma once

#include <optional>

#include <cgarchi/classical_orders.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Diminution du fut (Vignole, pl. XXXI)
// ============================================================================
//
// y est compte depuis le pied du fut (sommet de la base), H est la hauteur du
// fut ; rayons et hauteurs dans la meme unite.
//
// Methode 1 (toscan, dorique) : r = R jusqu'a H/3, puis
//   r(y) = R cos(u arccos(r_sup / R)),  u = (y - H/3) / (2H/3).
//
// Methode 2 (ionique, corinthien, composite) : conchoide de Nicomede. Origine
// sur l'axe au tiers ; k = sqrt(CD^2 - r_sup^2) ; e = r_sup (2H/(3k) - 1) ; la
// droite issue de E(-e, 0) coupe l'axe en (0, t), d = sqrt(e^2 + t^2), et le
// point du contour est (CD e / d, H/3 + t (1 + CD/d)). y(t) est strictement
// croissant. Le rayon est maximal (= CD) au tiers : le contour n'est PAS
// monotone, il croit jusqu'a H/3 puis decroit.

double CosineShaftRadius (double y, double H, double lowerRadius, double upperRadius);

class Conchoid
{
public:
	// Leve std::invalid_argument si CD <= r_sup ou si e <= 0.
	Conchoid (double H, double upperRadius, double cd);

	double Cd () const { return m_cd; }

	// Rayon a la hauteur y, y dans [0, H] (bornee par la bissection en t).
	double Radius (double y) const;

	// Point (rayon, hauteur) du parametre t.
	void At (double t, double &radius, double &y) const;

private:
	double m_H, m_cd, m_e;
};

// CD qui donne un rayon `lowerRadius` au pied du fut (y = 0). C'est la regle du
// generateur : Vignole ne l'enonce pas, il grave CD = 29/27 M.
double SolveConchoidCD (double H, double upperRadius, double lowerRadius);

// Contour du fut d'un ordre, en parties : H = fut_M x parties par module,
// R = diam_inf / 2, r_sup = diam_sup / 2.
class ShaftProfile
{
public:
	explicit ShaftProfile (const OrderData &order);

	double Height () const { return m_H; }
	double LowerRadius () const { return m_lower; }
	double UpperRadius () const { return m_upper; }
	DiminutionMethod Method () const { return m_method; }
	double ConchoidCd () const { return m_cd; }   // 0 en methode 1

	double Radius (double y) const;

private:
	double m_H = 0.0, m_lower = 0.0, m_upper = 0.0, m_cd = 0.0;
	DiminutionMethod m_method = DiminutionMethod::Cosine;
	std::optional<Conchoid> m_conchoid;
};

} // namespace classical
} // namespace cgarchi
