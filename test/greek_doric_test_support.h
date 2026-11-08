#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"

// Jeu d'essai de la colonne dorique grecque, partage par les tests et les bancs.
// Valeurs synthetiques, hors des points fixes (non exactes en binaire,
// differentes des valeurs par defaut) : elles ne decrivent aucun monument et ne
// sont pas des valeurs par defaut de la bibliotheque (D3-4).

namespace cgarchi_test
{

// Chapiteau : gorgerin de 0,097 avec rainure de 0,0071 creusee de 0,0263 (le
// fond des cannelures A est a 0,0225 sous le rayon du sommet, 0,415) ; trois
// listels de saillies croissantes ; echine droite de 0,061 a 0,205, puis levre
// de rayon 0,047 qui sort jusqu'a 0,2274 et rentre a 0,223 sous l'abaque.
inline cgarchi::classical::GreekDoricColumnSettings SyntheticGreekDoricColumnSettings ()
{
	cgarchi::classical::GreekDoricColumnSettings s;
	s.height = 5.3;
	s.lowerDiameter = 1.07;
	s.upperDiameter = 0.83;
	s.entasisSagitta = 0.013;
	s.entasisHeight = 1.9;
	s.fluteCount = 20;
	s.fluteSection = cgarchi::classical::FluteSection::Segment60;
	s.hypotrachelionHeight = 0.097;
	s.grooveHeight = 0.0071;
	s.grooveDepth = 0.0263;
	s.annulets = {{0.0113, 0.023}, {0.0097, 0.037}, {0.0089, 0.049}};
	s.echinusHeight = 0.27;
	s.echinusFootProjection = 0.061;
	s.echinusJunctionHeight = 0.21;
	s.echinusJunctionProjection = 0.205;
	s.echinusTopProjection = 0.223;
	s.echinusLipRadius = 0.047;
	s.abacusHeight = 0.19;
	s.abacusProjection = 0.27;
	return s;
}

// Chapiteau etroit : annelets et echine de saillie au plus 0,0071, sous un abaque
// de saillie `abacusProjection` (au moins 0,0071). La levre (jonction a 0,25,
// rayon 0,047) sort jusqu'a 0,00431, au-dela de sa tete.
inline void NarrowCapital (cgarchi::classical::GreekDoricColumnSettings &s, double abacusProjection)
{
	s.annulets = {{0.0113, 0.0029}, {0.0097, 0.0047}, {0.0089, 0.0071}};
	s.echinusFootProjection = 0.0;
	s.echinusJunctionHeight = 0.25;
	s.echinusJunctionProjection = 0.002;
	s.echinusTopProjection = 0.004;
	s.echinusLipRadius = 0.047;
	s.abacusProjection = abacusProjection;
}

// Fond de cannelure au milieu, rapporte au rayon des aretes, b = pi / N, corde
// 2 sin b : section A, cos b - 2 sin b (1 - sqrt3 / 2) ; section B,
// cos b - sin b ; trois centres, cos b - 2 sin b q, q = profondeur / corde.
inline double FluteBottomRatio (int count, cgarchi::classical::FluteSection section, double depthRatio)
{
	using cgarchi::classical::FluteSection;
	const double beta = 3.14159265358979323846 / count;
	if (section == FluteSection::Segment60)
		return std::cos (beta) - 2.0 * std::sin (beta) * (1.0 - std::sqrt (3.0) / 2.0);
	if (section == FluteSection::Semicircle)
		return std::cos (beta) - std::sin (beta);
	return std::cos (beta) - 2.0 * std::sin (beta) * depthRatio;
}

// Rainure creusee a 0,0041 sous le fond des cannelures au sommet du fut, pour
// les variantes qui changent le nombre, la section des cannelures ou le
// diametre superieur. Sans effet sur un fut lisse ou sans rainure.
inline void FitGrooveUnderFlutes (cgarchi::classical::GreekDoricColumnSettings &s)
{
	if (s.fluteCount == 0 || s.grooveHeight == 0.0)
		return;
	const double radius = 0.5 * s.upperDiameter;
	s.grooveDepth = radius * (1.0 - FluteBottomRatio (s.fluteCount, s.fluteSection, s.fluteDepthRatio)) + 0.0041;
}

// Section a trois centres du jeu synthetique : rapport profondeur / corde 0,19,
// hors des points fixes et sans valeur du monument (le README donne
// 1,1 / 6,25 = 0,176). Etoilee de 4 a 48 cannelures sauf a 4 (seuil 0,147).
inline void ThreeCentredFlutes (cgarchi::classical::GreekDoricColumnSettings &s, double depthRatio = 0.19)
{
	s.fluteSection = cgarchi::classical::FluteSection::ThreeCentred;
	s.fluteDepthRatio = depthRatio;
	FitGrooveUnderFlutes (s);
}

// Temple synthetique : colonne courante, la colonne ci-dessus ; colonne d'angle,
// la meme au diametre inferieur 1,09 ; peristyle de 4 x 5 colonnes, entraxes
// courants 1,9 et 1,7, entraxes d'angle 1,73 et 1,61 ; stylobate de demi-cotes
// axes d'angle + 0,61 et + 0,67 (2,68 + 0,61 et 3,31 + 0,67), deux degres.
// Entraxes, travees d'angle, marges et diametres different en X et en Y et
// entre angle et colonne courante, pour qu'une permutation se voie. Aucune
// valeur ne vient d'un monument (D3-4).
inline cgarchi::classical::TempleSettings SyntheticTempleSettings ()
{
	cgarchi::classical::TempleSettings t;
	t.column = SyntheticGreekDoricColumnSettings ();
	t.cornerColumn = SyntheticGreekDoricColumnSettings ();
	t.cornerColumn.lowerDiameter = 1.09;
	t.peristyle.front = 4;
	t.peristyle.flank = 5;
	t.peristyle.frontSpacing = 1.9;
	t.peristyle.flankSpacing = 1.7;
	t.peristyle.frontCornerSpacing = 1.73;
	t.peristyle.flankCornerSpacing = 1.61;
	t.krepis.stylobateHalfWidth = 3.29;
	t.krepis.stylobateHalfDepth = 3.98;
	t.krepis.stylobateHeight = 0.37;
	t.krepis.lowerSteps = {{0.29, 0.43, 0.23}, {0.31, 0.41, 0.19}};
	return t;
}

// Entablement synthetique, sans valeur d'un monument : neuf bandes (hauteur ;
// saillie du pied -> du haut) : face d'architrave inclinee (0,55 ; 0,475 ->
// 0,47), haut d'architrave (0,07 ; 0,47), tenia (0,06 ; 0,50), frise (0,58 ;
// 0,43), chapiteau du triglyphe (0,06 ; 0,495), listel (0,03 ; 0,505),
// corniche (0,05 ; 0,78), (0,17 ; 0,83), (0,03 ; 0,85). Une jonction a saillie
// egale derriere une face inclinee : 19 sommets de section. Triglyphe de 0,37,
// face a 0,49 (0,06 devant la frise), sept divisions inegales (demi-canal,
// bande, canal, bande, canal, bande, demi-canal), canal de 0,0375, demi-canal
// de 0,04, plus profond que le canal. Face interieure a 0,44.
inline cgarchi::classical::GreekEntablatureSettings SyntheticEntablatureSettings ()
{
	cgarchi::classical::GreekEntablatureSettings e;
	e.bands = {{0.55, 0.475, 0.47}, {0.07, 0.47, 0.47}, {0.06, 0.50, 0.50},   {0.58, 0.43, 0.43}, {0.06, 0.495, 0.495},
	           {0.03, 0.505, 0.505}, {0.05, 0.78, 0.78}, {0.17, 0.83, 0.83}, {0.03, 0.85, 0.85}};
	e.innerFace = 0.44;
	e.friezeBand = 3;
	e.triglyph.width = 0.37;
	e.triglyph.face = 0.49;
	e.triglyph.divisionWidths = {0.03, 0.05, 0.075, 0.06, 0.075, 0.05, 0.03};
	e.triglyph.divisionIsChannel = {true, false, true, false, true, false, true};
	e.triglyph.channelDepth = 0.0375;
	e.triglyph.halfChannelDepth = 0.04;
	return e;
}

// Entablement synthetique avec ornements, sans valeur d'un monument :
// celui de SyntheticEntablatureSettings dont seule la bande 6 change, saillie
// 0,48 au lieu de 0,78, en retrait du listel (0,505) pour que le mutule y pose
// (contact L14). Regulae : hauteur 0,05 (bande 1 : 0,07), face 0,50 (= tenia),
// 6 gouttes, vide 0,035 ; gouttes de diametre 0,025 au pied, avant a 0,50 au
// pied et 0,498 en tete, 8 cotes. Mutules : bande 6, largeur 0,30, face 0,78.
inline cgarchi::classical::GreekEntablatureSettings SyntheticOrnamentSettings ()
{
	cgarchi::classical::GreekEntablatureSettings e = SyntheticEntablatureSettings ();
	e.bands[6].projection = 0.48;
	e.bands[6].topProjection = 0.48;
	cgarchi::classical::GreekRegulaSettings r;
	r.height = 0.05;
	r.face = 0.50;
	r.guttaCount = 6;
	r.guttaGap = 0.035;
	r.gutta.footDiameter = 0.025;
	r.gutta.footProjection = 0.50;
	r.gutta.topProjection = 0.498;
	r.gutta.sides = 8;
	e.regulae = r;
	cgarchi::classical::GreekMutuleSettings m;
	m.band = 6;
	m.width = 0.30;
	m.face = 0.78;
	e.mutules = m;
	return e;
}

// Entablement synthetique a sous-face inclinee, sans valeur d'un monument :
// celui de SyntheticOrnamentSettings, montee du dessous des mutules 0,06 (de la
// face, 0,78 a z[6], au dos, 0,48 a z[6] + 0,06), et 4 gouttes par rang sur
// 3 rangs ; diametres de tete 0,030 / 0,034 / 0,032 (distincts, pour qu'une
// permutation se voie), vides entre tetes 0,08 / 0,09, axes extremes a 0,04
// des bords, hauteur 0,02, evasement 0,002, 8 cotes.
inline cgarchi::classical::GreekEntablatureSettings SyntheticInclinedSoffitSettings ()
{
	cgarchi::classical::GreekEntablatureSettings e = SyntheticOrnamentSettings ();
	e.mutules->rise = 0.06;
	cgarchi::classical::GreekMutuleGuttaSettings g;
	g.rowDiameters = {0.030, 0.034, 0.032};
	g.rowGaps = {0.08, 0.09};
	g.perRow = 4;
	g.edgeAxis = 0.04;
	g.height = 0.02;
	g.conicity = 0.002;
	g.sides = 8;
	e.mutules->guttae = g;
	return e;
}

// Temple synthetique (SyntheticTempleSettings) avec l'entablement synthetique.
inline cgarchi::classical::TempleSettings SyntheticTempleWithEntablature ()
{
	cgarchi::classical::TempleSettings t = SyntheticTempleSettings ();
	t.entablature = SyntheticEntablatureSettings ();
	return t;
}

// Temple synthetique (SyntheticTempleSettings) avec l'entablement a ornements.
inline cgarchi::classical::TempleSettings SyntheticTempleWithOrnaments ()
{
	cgarchi::classical::TempleSettings t = SyntheticTempleSettings ();
	t.entablature = SyntheticOrnamentSettings ();
	return t;
}

// Temple synthetique (SyntheticTempleSettings) avec l'entablement a sous-face
// inclinee.
inline cgarchi::classical::TempleSettings SyntheticTempleWithInclinedSoffit ()
{
	cgarchi::classical::TempleSettings t = SyntheticTempleSettings ();
	t.entablature = SyntheticInclinedSoffitSettings ();
	return t;
}

// Fronton synthetique, sans valeur d'un monument : montee 0,85 du dessus de
// l'anneau au haut de la corniche rampante dans l'axe ; pente 1 : 4, d'ou un
// pied du haut a 3,4, en deca de l'avant du couronnement (X_e = 3,53 sur
// l'anneau synthetique) ; nu du tympan a 0,45 (metopes a 0,43, face
// interieure a 0,44) ; section de 14 sommets (u depuis le nu du tympan, h
// normale a la pente), concave (gorge et bec), simple, directe, sans sommets
// alignes ; dessous h = 0 de u = 0 a 0,03 ; T = 0,22.
inline cgarchi::classical::GreekPedimentSettings SyntheticPedimentSettings ()
{
	cgarchi::classical::GreekPedimentSettings p;
	p.apexRise = 0.85;
	p.rakingRun = 4.0;
	p.tympanumFace = 0.45;
	p.rakingSection = {{0.0, 0.0},   {0.03, 0.0},  {0.03, 0.02}, {0.05, 0.02}, {0.05, 0.06}, {0.20, 0.06}, {0.30, 0.04},
	                   {0.33, 0.01}, {0.35, 0.01}, {0.35, 0.18}, {0.37, 0.20}, {0.39, 0.20}, {0.39, 0.22}, {0.0, 0.22}};
	return p;
}

// Temple synthetique a sous-face inclinee (SyntheticTempleWithInclinedSoffit)
// avec le fronton synthetique.
inline cgarchi::classical::TempleSettings SyntheticTempleWithPediment ()
{
	cgarchi::classical::TempleSettings t = SyntheticTempleWithInclinedSoffit ();
	t.pediment = SyntheticPedimentSettings ();
	return t;
}

// Raffinement synthetique (lot R0a), sans valeur d'un monument : quatre fleches
// distinctes, pour qu'un echange de cotes se voie (Front 0,0123, Right 0,0171,
// Back 0,0089, Left 0,0137), inclinaison 0,0237, ecart de corde 7,3e-5.
inline cgarchi::classical::TempleRefinementSettings SyntheticRefinementSettings ()
{
	cgarchi::classical::TempleRefinementSettings r;
	r.sagittaFront = 0.0123;
	r.sagittaRight = 0.0171;
	r.sagittaBack = 0.0089;
	r.sagittaLeft = 0.0137;
	r.inclination = 0.0237;
	r.chordDeviation = 7.3e-5;
	return r;
}

// Temple synthetique a fronton (SyntheticTempleWithPediment) raffine.
inline cgarchi::classical::TempleSettings SyntheticRefinedTemple ()
{
	cgarchi::classical::TempleSettings t = SyntheticTempleWithPediment ();
	t.refinement = SyntheticRefinementSettings ();
	return t;
}

// Ecart entre deux surfaces en contact sous raffinement (D3-110) : max
// |z_dessus - z_dessous|, aux sommets de chaque surface situes dans la
// projection d'un triangle de l'autre, |ecart| < 0,01 ; faces de normale z de
// signe donne, |n_z| > minCos de la norme (0,9 par defaut). Minorant de l'ecart
// vrai (il peut culminer a l'interieur d'un triangle).
struct ContactTriangle
{
	std::array<std::array<double, 3>, 3> p {};
	double lo[2] = {0.0, 0.0}, hi[2] = {0.0, 0.0};
};

inline std::vector<ContactTriangle> ContactFaces (const Mesh &m, const cgarchi::classical::TempleMeshPart &part, int sign,
                                                  double minCos = 0.9)
{
	std::vector<ContactTriangle> out;
	for (unsigned f = part.firstFace; f < part.firstFace + part.faceCount; ++f)
	{
		ContactTriangle t;
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			m.GetVertex (static_cast<unsigned> (m.GetFaceVertex (f, j)), q);
			t.p[j] = {q[0], q[1], q[2]};
		}
		const double u[3] = {t.p[1][0] - t.p[0][0], t.p[1][1] - t.p[0][1], t.p[1][2] - t.p[0][2]};
		const double w[3] = {t.p[2][0] - t.p[0][0], t.p[2][1] - t.p[0][1], t.p[2][2] - t.p[0][2]};
		const double n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
		const double l = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if (!(l > 0.0 && n[2] * sign > minCos * l))
			continue;
		for (int c = 0; c < 2; ++c)
		{
			t.lo[c] = std::min ({t.p[0][c], t.p[1][c], t.p[2][c]});
			t.hi[c] = std::max ({t.p[0][c], t.p[1][c], t.p[2][c]});
		}
		out.push_back (t);
	}
	return out;
}

inline bool ContactZAt (const ContactTriangle &t, double x, double y, double &z)
{
	if (x < t.lo[0] - 1e-9 || x > t.hi[0] + 1e-9 || y < t.lo[1] - 1e-9 || y > t.hi[1] + 1e-9)
		return false;
	const double x0 = t.p[0][0], y0 = t.p[0][1], x1 = t.p[1][0], y1 = t.p[1][1], x2 = t.p[2][0], y2 = t.p[2][1];
	const double d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
	if (d == 0.0)
		return false;
	const double a = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / d, b = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / d;
	const double c = 1.0 - a - b, e = -1e-12;
	if (a < e || b < e || c < e)
		return false;
	z = a * t.p[0][2] + b * t.p[1][2] + c * t.p[2][2];
	return true;
}

struct ContactGap
{
	double crossing = std::numeric_limits<double>::infinity ();     // min (z_dessus - z_dessous)
	double separation = -std::numeric_limits<double>::infinity ();  // max
	std::size_t samples = 0;
	void Add (const ContactGap &o)
	{
		samples += o.samples;
		crossing = std::min (crossing, o.crossing);
		separation = std::max (separation, o.separation);
	}
	double Max () const { return samples == 0 ? 0.0 : std::max (-crossing, separation); }
};

inline ContactGap MeasureContactGap (const std::vector<ContactTriangle> &lower, const std::vector<ContactTriangle> &upper)
{
	ContactGap g;
	auto probe = [&] (const std::vector<ContactTriangle> &from, const std::vector<ContactTriangle> &into, double sign) {
		for (const ContactTriangle &t : from)
			for (const auto &p : t.p)
				for (const ContactTriangle &u : into)
				{
					double z;
					if (!ContactZAt (u, p[0], p[1], z))
						continue;
					const double gap = sign * (z - p[2]);
					if (!(std::fabs (gap) < 0.01))
						continue;
					++g.samples;
					g.crossing = std::min (g.crossing, gap);
					g.separation = std::max (g.separation, gap);
					break;
				}
	};
	probe (lower, upper, 1.0);
	probe (upper, lower, -1.0);
	return g;
}

// Ecarts des contacts horizontaux d'un temple a fronton (D3-110) : L1, L2, L3
// (abaques courants et d'angle), pieces posees sur un ressaut de l'anneau (L5,
// L14), pieces sous un ressaut (L6, L8, L11), L16, L18. Faces de pied des
// Lambda (L18) : |n_z| > kFootFaceCos de la norme ; une paroi du Lambda contient
// la direction de la pente 1 : r, d'ou |n_z| <= r / hypot (r, 1) < kFootFaceCos
// pour r < 7 (0,971 au README, 0,970 au jeu synthetique).
constexpr double kFootFaceCos = 0.99;

struct RefinedContactGaps
{
	ContactGap l1, l2, l3, l3Corner, onLedge, underLedge, l16, l18;
	double Max () const
	{
		double m = 0.0;
		for (const ContactGap *g : {&l1, &l2, &l3, &l3Corner, &onLedge, &underLedge, &l16, &l18})
			m = std::max (m, g->Max ());
		return m;
	}
};

inline RefinedContactGaps MeasureRefinedContactGaps (const Mesh &m, const cgarchi::classical::TempleLayout &l,
                                                     const std::vector<cgarchi::classical::TempleMeshPart> &parts)
{
	RefinedContactGaps g;
	const std::size_t steps = l.krepis.steps.size (), nc = l.columns.size (), ring = steps + nc;
	for (std::size_t k = 0; k + 1 < steps; ++k)
		g.l1.Add (MeasureContactGap (ContactFaces (m, parts[k + 1], 1), ContactFaces (m, parts[k], -1)));
	const std::vector<ContactTriangle> stylobate = ContactFaces (m, parts[0], 1), ringDown = ContactFaces (m, parts[ring], -1),
	                                   ringUp = ContactFaces (m, parts[ring], 1);
	for (std::size_t c = 0; c < nc; ++c)
	{
		g.l2.Add (MeasureContactGap (stylobate, ContactFaces (m, parts[steps + c], -1)));
		(l.columns[c].corner ? g.l3Corner : g.l3).Add (MeasureContactGap (ContactFaces (m, parts[steps + c], 1), ringDown));
	}
	const bool pediment = parts.back ().address.element == cgarchi::classical::TempleElement::Roof;
	for (std::size_t k = ring + 1; k < parts.size () - (pediment ? 3 : 0); ++k)
	{
		g.onLedge.Add (MeasureContactGap (ringUp, ContactFaces (m, parts[k], -1)));
		g.underLedge.Add (MeasureContactGap (ContactFaces (m, parts[k], 1), ringDown));
	}
	if (pediment)
	{
		g.l16.Add (MeasureContactGap (ringUp, ContactFaces (m, parts.back (), -1)));
		for (const std::size_t k : {parts.size () - 3, parts.size () - 2})
			g.l18.Add (MeasureContactGap (ringUp, ContactFaces (m, parts[k], -1, kFootFaceCos)));
	}
	return g;
}

} // namespace cgarchi_test
