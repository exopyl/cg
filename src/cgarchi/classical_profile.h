#pragma once

#include <optional>
#include <string>
#include <vector>

#include <cgarchi/classical_orders.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Profil d'une colonne : membres traces, empiles, et anneaux en plan
// ============================================================================
//
// Unites : parties de module. y est vertical, compte depuis le pied de
// l'element le plus bas ; s est la saillie depuis la face de reference du bloc.

struct ProfilePoint
{
	double s = 0.0;
	double y = 0.0;
};

// Trace d'un membre, de y = 0 a y = h, dans la boite [min s, max s] x [0, h]
// (conventions.formes du JSON). `span` = (s_bas, s_haut) ; `extremum` = s_max
// d'un tore, fond d'une scotie. Si s_bas > s_haut, la boite est retournee
// verticalement. `curveSegments` >= 1 segments par courbe.
//
//   listel, face        : segment vertical
//   ornement            : segment de s_bas a s_haut (tronc de cone en revolution)
//   ove                 : quart d'ellipse convexe, tangente horizontale cote
//                         rentrant, verticale cote saillant
//   cavet, conge        : quart d'ellipse concave (un conge dans une boite non
//                         carree est elliptique)
//   doucine             : deux arcs de 60 degres sur les demi-diagonales,
//                         convexe en bas, concave en haut ; talon : l'inverse.
//                         Si la diagonale fait moins de 30 degres avec
//                         l'horizontale, y n'est plus monotone le long du
//                         trace ; s'il sort alors de [0, h] (toujours pour une
//                         doucine, pour un talon assez plat), TraceMember leve
//                         std::invalid_argument
//   tore                : demi-ellipse de demi-axes (h/2, s_max - attache),
//                         s_max a mi-hauteur
//   scotie              : fond a 3h/5 du bas ; quart d'ellipse (s_bas - fond,
//                         3h/5) en bas, (s_haut - fond, 2h/5) en haut
std::vector<ProfilePoint> TraceMember (MouldingForm form, double h, Span span, double extremum,
                                       int curveSegments);

// ---------------------------------------------------------------------------
//  Anneau : section horizontale du solide, fonction rayon(theta) etoilee
// ---------------------------------------------------------------------------

enum class RingShape
{
	Circle,          // rayon R
	Square,          // demi-cote R, cotes paralleles aux axes
	ConcaveSquare,   // cornes a R (theta = 45 + k 90 degres), milieu de face a F
	Fluted           // cercle R creuse de `flutes` cannelures (classical_flutes.h)
};

// Anneau a symetrie d'ordre 4 et de miroir sur les axes et les diagonales :
// RadiusAt(theta) se lit sur [0, pi/4].
//
// Carre concave : chaque face est l'arc de cercle passant par ses deux cornes
// et par son milieu (Vignole : arc centre au sommet du triangle equilateral
// construit sur la face). Les cornes sont vives : le pan coupe des cornes est
// hors perimetre.
struct Ring
{
	RingShape shape = RingShape::Circle;
	double    radius = 0.0;   // Circle, Fluted : R ; Square : demi-cote ; ConcaveSquare : cornes
	double    face   = 0.0;   // ConcaveSquare : distance du milieu de face a l'axe
	int       flutes = 0;     // Fluted : nombre de cannelures
	double    halfAngle = 0.0;   // Fluted : demi-angle de chaque cannelure
	FluteSection section = FluteSection::Semicircle;   // Fluted

	static Ring MakeCircle (double r);
	static Ring MakeSquare (double halfSide);
	// Ramene a un cercle si face == corne (l'arc passant par les trois points
	// est alors le cercle de rayon `corner`), a un carre si face == corne / sqrt 2.
	// Leve std::invalid_argument si un rayon issu de l'axe coupe la face deux
	// fois ou ne la coupe pas : face creusee plus profonde que (2 - sqrt2) a,
	// a = corne / sqrt2, ou face bombee dont le cercle n'entoure pas l'axe.
	static Ring MakeConcaveSquare (double corner, double face);
	// Leve std::invalid_argument si R <= 0, count < 3 ou halfAngle hors de
	// ]0, pi / count]. La symetrie d'ordre 4 demande count multiple de 4.
	static Ring MakeFluted (double R, int count, double halfAngle, FluteSection section);

	double RadiusAt (double theta) const;

	bool operator== (const Ring &o) const;
	bool operator!= (const Ring &o) const { return !(*this == o); }
};

// Angles de [0, pi/4] ou r_a - r_b change de signe, croissants. Deux anneaux
// qui se croisent ainsi ne peuvent pas etre relies par un anneau plat sans
// recoupement : le maillage ajoute ces angles (et leurs images par symetrie) a
// l'echantillonnage.
std::vector<double> RingCrossings (const Ring &a, const Ring &b);

// ---------------------------------------------------------------------------
//  Profil assemble
// ---------------------------------------------------------------------------

enum class DoricCornice { Dentils, Mutules };

struct ColumnOptions
{
	Order        order = Order::Tuscan;
	bool         pedestal = false;
	bool         entablature = false;
	DoricCornice doricCornice = DoricCornice::Dentils;
	bool         ornamentEnvelope = false;   // false : noyau ; true : enveloppe
	int          curveSegments = 6;          // >= 1
	int          shaftSegments = 24;         // >= 2
	// Ionique : la bande des volutes (listel et canal, plan oriente_ionique)
	// prend le plan des faces des volutes, carre de demi-cote
	// faces_des_volutes_en_plan / 2 (ornements.volute). Sans effet sur les
	// autres ordres.
	bool         ionicVolutes = false;
	// Conges du fut (conventions.conges_du_fut). false : le fut rejoint ses
	// listels par des anneaux plats (raccord sans conge).
	bool         shaftFillets = true;
	// Cannelures (ornements.cannelures), sans effet sur un ordre qui n'en a
	// pas. `doricFlutes` choisit la section du dorique (A : Segment60,
	// B : Semicircle). `fluteSamples` : segments d'arc par demi-cannelure (0 :
	// max(6, curveSegments)) ; `fluteHeadRings` anneaux de tete.
	bool         flutes = false;
	FluteSection doricFlutes = FluteSection::Segment60;
	int          fluteSamples = 0;
	int          fluteHeadRings = 2;
};

struct ProfileMember
{
	std::string block;
	std::string name;
	double      y0 = 0.0, y1 = 0.0;
	PlanForm    plan = PlanForm::Revolution;
};

struct ProfileVertex
{
	double y = 0.0;
	Ring   ring;
	int    member = -1;   // indice dans ColumnProfile::members ; -1 : fut
};

// Zone du fut, en y du profil : du sommet du listel du pied au pied du listel
// de fut_haut ; le vif est entre les deux conges.
struct ShaftZone
{
	double foot = 0.0;        // sommet du listel du pied
	double vifBottom = 0.0;   // sommet du conge inferieur
	double vifTop = 0.0;      // pied du conge superieur
	double top = 0.0;         // pied du listel de fut_haut
	double footFillet = 0.0;  // rayon du conge inferieur (0 sans conge)
	double topFillet = 0.0;   // rayon du conge superieur
};

// Grille angulaire des cannelures, pour le tour : centres 2 pi j / count,
// sommets a +-offsets autour de chaque centre (FluteGridOffsets).
struct FluteGrid
{
	int    count = 0;
	double halfAngle = 0.0;
	int    samples = 0;                  // segments d'arc par demi-cannelure
	FluteSection section = FluteSection::Semicircle;
	double headBottom = 0.0;             // y du bas de la tete (cannelure pleine au-dessous)
	std::vector<double> offsets;         // >= 0, croissants
	std::vector<double> headHalfAngles;  // decroissants
};

struct ColumnProfile
{
	std::vector<ProfileVertex> vertices;   // de bas en haut
	std::vector<ProfileMember> members;    // de bas en haut
	double height = 0.0;
	ShaftZone shaft;
	std::optional<FluteGrid> flutes;
};

// Plan d'un membre traduit en anneau (v1) :
//   revolution                    -> cercle
//   carre, lineaire, oriente_ionique -> carre (entablement en ressaut sur la
//                                    colonne ; oriente_ionique : demi-cote
//                                    distance_axe + s, ou F avec ionicVolutes)
//   carre_concave a deux coupes   -> carre concave (diagonale, face)
//   carre_concave a coupe unique  -> cercle de rayon distance_axe + s : la meme
//                                    saillie dans les deux coupes
// Les deux coupes d'un membre partagent leur parametrage : la coupe par la face
// est l'image de la coupe diagonale par l'affinite horizontale qui envoie une
// boite sur l'autre.
//
// Assemblage de bas en haut : piedestal (option) ; base sans ses membres hors
// module ; membres hors module ; fut (ShaftProfile, y depuis le sommet de la
// base) jusqu'a H - h(fut_haut) : conge inferieur, vif r(y), conge superieur ;
// fut_haut ; chapiteau ; architrave, frise, corniche (option). Le fut occupe
// fut_M x P, membres hors module, conges et fut_haut compris. Les sommets
// consecutifs identiques sont fusionnes.
ColumnProfile BuildColumnProfile (const OrderData &order, const ColumnOptions &options);

// Saillie imposee a un membre (bande au fond d'un ornement).
struct BandOverride
{
	std::string block;
	std::string member;
	double s = 0.0;
};

// Entablement seul (architrave, frise, corniche), de y = 0 a sa hauteur, comme
// BuildColumnProfile l'empile ; `overrides` remplace la saillie des membres
// nommes. Leve std::invalid_argument si un membre de `overrides` n'existe pas.
ColumnProfile BuildEntablatureProfile (const OrderData &order, const ColumnOptions &options,
                                       const std::vector<BandOverride> &overrides = {});

// Jonction plate ou les deux anneaux se croisent.
struct PlanConflict
{
	std::size_t vertex = 0;        // indice du sommet du dessus de la jonction
	std::string lower, upper;      // "bloc/membre" ; "fut" pour le fut
	double      y = 0.0;
	std::vector<double> angles;    // RingCrossings, dans [0, pi/4]
};

std::vector<PlanConflict> FindPlanConflicts (const ColumnProfile &profile);

} // namespace classical
} // namespace cgarchi
