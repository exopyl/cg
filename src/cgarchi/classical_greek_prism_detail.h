#pragma once

// En-tete interne de l'architecture classique grecque, hors de l'API publique :
// prismes droits fermes d'un polygone simple, partages par la cella,
// l'interieur du naos et le couronnement, et bandes moulees a bouts d'onglet.

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Mesh;

namespace cgarchi
{
namespace classical
{
namespace detail
{

// Axe d'extrusion d'un prisme. Le polygone est dans le plan (u, v), extrude le
// long de w, (u, v, w) directs : axe X, (u, v, w) = (Y, Z, X) ; axe Y,
// (X, Z, -Y) ; axe Z, (X, Y, Z).
enum class Extrusion { AlongX, AlongY, AlongZ };

// Prisme droit ferme du polygone simple `outline` (sens quelconque), de w = w0 a
// w = w1, triangles, normales sortantes. Les sommets consecutifs confondus et
// les sommets alignes sont retires (une oreille doit etre strictement convexe).
// Chaque sommet est calcule en double puis arrondi une fois en float : les
// coordonnees sont des copies des valeurs du polygone et de w0, w1, sans
// produit. Leve std::logic_error si le polygone nettoye n'est pas simple.
std::unique_ptr<Mesh> PrismMesh (const std::vector<std::array<double, 2>> &outline, Extrusion axis, double w0, double w1);

// Boite alignee sur les axes, [x0, x1] x [y0, y1] x [z0, z1].
std::unique_ptr<Mesh> BoxMesh (double x0, double x1, double y0, double y1, double z0, double z1);

// Axe horizontal le long duquel court une bande moulee.
enum class StripAxis { AlongX, AlongY };

// Sens de la saillie d'une bande : vers les coordonnees croissantes ou
// decroissantes de l'axe horizontal qui n'est pas celui de la bande.
enum class StripOutward { Increasing, Decreasing };

// Bande moulee : profil simple (d, z), d >= 0 compte depuis la face verticale
// `face` (coordonnee de l'autre axe horizontal) vers l'exterieur, dans le sens
// `outward` ; prisme le
// long de `axis`, de t0 a t1. Un bout d'onglet recule de d : t0 + d au debut
// (`mitre0`), t1 - d a la fin (`mitre1`), le plan vertical a 45 degres d'un
// angle rentrant ; un bout droit reste a t0 ou t1. Chaque coordonnee est une
// somme ou une difference de deux valeurs donnees, sans produit, arrondie une
// fois en float. Triangles, normales sortantes, ferme. Sommets alignes et
// consecutifs confondus retires comme PrismMesh. Leve std::logic_error si une
// saillie du profil est negative ou non finie, si le profil nettoye n'est pas
// simple, ou si un sommet du debut n'est pas strictement avant celui de la fin,
// une fois arrondis en float.
std::unique_ptr<Mesh> StripMesh (const std::vector<std::array<double, 2>> &profile, StripAxis axis, double face,
                                 StripOutward outward, double t0, bool mitre0, double t1, bool mitre1);

// Bord d'une union de cellules d'une grille rectiligne : xs, ys, zs strictement croissants ; cellule (i, j, k)
// = [xs[i], xs[i+1]] x [ys[j], ys[j+1]] x [zs[k], zs[k+1]]. Chaque face entre une cellule occupee et une
// cellule vide ou l'exterieur donne deux triangles (diagonale fixee : du premier coin au troisieme), normales
// sortantes ; sommets partages sur la grille, seuls ceux des faces emises, numerotes par (k, j, i) croissants,
// chacun copie d'une coordonnee et arrondi une fois en float. Leve std::logic_error si une suite n'est pas
// strictement croissante, si deux coordonnees voisines se confondent en float, ou si deux cellules occupees ne se
// touchent que par une arete (les deux autres cellules de leur bloc 2 x 2 vides) ou que par un sommet (les six
// autres cellules de leur bloc 2 x 2 x 2 vides), ou si deux cellules vides ne se touchent que par un sommet (les six
// autres cellules de leur bloc 2 x 2 x 2 occupees) : sommet non-variete.
std::unique_ptr<Mesh> GridSolidMesh (const std::vector<double> &xs, const std::vector<double> &ys, const std::vector<double> &zs,
                                     const std::function<bool (std::size_t, std::size_t, std::size_t)> &occupied);

} // namespace detail
} // namespace classical
} // namespace cgarchi
