#pragma once

// Visualisation tubulaire du reseau de la microarchitecture gothique (J5) : un
// maillage d'AFFICHAGE, construit avec CreateTubes (cgmesh), une courbe par appel.
// Le reseau (JSON) et l'export OBJ n'en dependent pas.
//
// Limites (plan d'execution, §12.3) :
// - les tubes des courbes d'une jonction se traversent sur ~r autour d'elle ; les
//   capuchons d'une T-jonction sont dans le tube de l'autre courbe ;
// - chaque courbe donne une surface fermee orientee ; leur reunion s'auto-intersecte
//   et ne borde aucun solide : ni maillage imprimable, ni coque unique ;
// - un rayon superieur a l'espacement des courbes les fait se toucher a l'ecran ;
//   tube_radius_hides_crossing ne signale que les croisements a min_dz <= 2r
//   (condition suffisante : minorant) ; courbes voisines paralleles et bouts libres
//   proches ne sont pas signales ;
// - un rayon superieur au rayon de courbure local replie le tube (profil
//   « aiguille », M13), sans diagnostic ;
// - CreateTubes soude en boucle une polyligne d'au moins 4 points dont les bouts sont
//   a moins de r/2 : une telle courbe est coupee en morceaux ouverts (coupe recursive
//   au milieu), dont les extremites partagees portent deux capuchons ; une courbe
//   plus courte que le rayon reste ainsi ouverte ;
// - le prefiltre peut ecarter l'echantillon du PRC : ecart d'au plus 4e-4 E / 256
//   ~ 1,6e-6 E ;
// - sommets en float, relatifs a TubeMesh::origin : l'anneau est deforme d'environ
//   3e-8 E / r en relatif (E : diagonale 3D des echantillons), quelle que soit la
//   position du dessin ;
// - les comptes exacts reposent sur la disposition interne de CreateTubes (un
//   anneau par echantillon, K - 2 triangles par capuchon).
// Aucune exception volontaire ; un epuisement de la memoire peut lever.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "mesh.h"
#include <cgarchi/gothic_microarchitecture.h>

namespace cgarchi::microarch
{

struct TubeParams
{
	double         radius_ratio = 0.005;   // rayon = radius_ratio * D (D = network.tol.scale), dans (0, max_radius_ratio] (M21)
	int            sides        = 6;       // dans [min_sides, max_sides] ; refuse hors bornes, jamais ramene
	SamplingParams sampling;               // max_angle dans (0, TubeLimits::max_angle] (M22)
};

struct TubeLimits
{
	// Triangles au plus (decision M20). Mesure du pic au plafond : plan
	// d'execution, §12.6.
	std::size_t max_triangles = std::size_t(1) << 23;   // 8 388 608
	static constexpr double max_radius_ratio = 0.1;
	static constexpr int    min_sides = 3;
	static constexpr int    max_sides = 64;
	// pi/8 : par pas, la tangente 3D tourne d'au plus 2 max_angle, et le virage entre
	// deux cordes reste sous pi/2 : pas de biseau dans CreateTubes, un anneau par echantillon.
	static constexpr double max_angle = 0.39269908169872414;
};

struct TubeMesh
{
	std::unique_ptr<Mesh>    mesh;             // nullptr si erreur ; normales calculees
	std::vector<std::size_t> vertex_begin;     // C + 1 entrees : sommets de la courbe i dans [vertex_begin[i], vertex_begin[i+1])
	std::vector<std::size_t> triangle_begin;   // idem pour les triangles
	double                   radius = 0.0;     // rayon dans le repere du dessin
	// Origine du maillage, en double : centre de la boite 3D des echantillons. Les
	// sommets de `mesh` sont relatifs a cette origine ; l'appelant l'ajoute pour
	// l'affichage (point du dessin = sommet + origin).
	Vector3d                 origin;
};

// Nombre de triangles de BuildTubeMesh, compte avec le meme prefiltre et la meme
// decoupe des courbes a souder : 2K (n - 1) + 2 (K - 2) par morceau de n
// echantillons. Exact tant que CreateTubes garde sa disposition (un anneau par
// echantillon, K - 2 triangles par capuchon, aucune enveloppe : voir debt_cgmesh.md
// §2.1). Fait une passe sur les echantillons pour la boite, sans allouer le
// maillage. S'arrete des que le resultat depasse `limit` (resultat > limit).
// Au-dela du plafond M15 (SamplingLimits::max_network_points echantillons, compte
// sans allocation), le resultat est SIZE_MAX, qui depasse toute limite utile.
std::size_t CountTubeTriangles (const Network &network, const TubeParams &params, std::size_t limit);

// OBJ du maillage des tubes, origine appliquee : une ligne `v` par sommet, valeur
// (double) sommet + origin ecrite a 17 chiffres significatifs, puis une ligne `f`
// par triangle (indices a partir de 1). Pas de normales. Le point decimal ne
// depend pas de la locale.
std::string TubeMeshToObj (const Mesh &mesh, const Vector3d &origin);

// Maillage des tubes. Parametres hors bornes : tube_invalid_parameter sur
// tubes.radius_ratio, tubes.sides ou tubes.sampling.max_angle (tous relevés).
// Plafond M15 : sample_limit_exceeded sur sampling.max_angle. Au-dela de
// limits.max_triangles (avant toute allocation du maillage, par le compte exact ; le
// controle cumule pendant la construction est un filet qu'aucune entree connue
// n'atteint) : tube_limit_exceeded
// sur tubes. Resultat vide (mesh nul, plages vides) en cas d'erreur. Un croisement a
// min_dz <= 2r donne une fois tube_radius_hides_crossing sur tubes.radius_ratio.
// CreateTubes est appele dans un repere centre sur la boite 3D des echantillons, de
// diagonale 256, pour que ses seuils absolus (1e-6, grille de 1e-4) ne dependent pas
// de l'unite du dessin ; un echantillon a moins de 4e-4 du precedent garde y est retire.
TubeMesh BuildTubeMesh (const Network &network, const TubeParams &params, MicroarchReport &report,
                        const TubeLimits &limits = TubeLimits{});

} // namespace cgarchi::microarch
