#pragma once

#include <array>
#include <memory>
#include <vector>

#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>
#include <cgarchi/classical_volute.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Chapiteau ionique : coussinets a volutes
// ============================================================================
//
// Un coussinet est un prisme de la section S (classical_volute.h) le long de
// son axe w, entre les deux faces des volutes w = -F et w = +F ; sur chaque
// face, le relief R (listel et oeil) avance de d, le canal C reste au plan de
// la face. Un seul maillage ferme : faces planes (C en +-F, R en +-(F + d)),
// parois sur les memes sommets, toutes coupees en +-F.
//
// Simplifications : canal plat, ni ceinture (baltheus) ni etranglement du
// coussinet, oeil plat sans rosette, ni oves ni palmettes.

// Triangulation d'un polygone simple oriente en sens trigonometrique (indices
// dans `points`), sans sommet ajoute ; triangles dans le meme sens. Leve std::invalid_argument si le
// tessellateur cree un sommet, ne rend rien, rend un triangle d'aire non
// positive, ou si la somme des aires des triangles differe de l'aire du
// polygone de plus de 1e-9 en relatif.
std::vector<std::array<unsigned, 3>> TessellatePolygon (const std::vector<VolutePoint> &points,
                                                        const std::vector<unsigned> &polygon);

// Coussinet dans le repere de la section : x = u, y = v, z = w.
// `splitWallsAtFaces` = false laisse les parois communes a S et R d'un seul
// tenant de -(F + d) a F + d (jonctions en T : maillage ouvert).
// La section doit etre celle de BuildVoluteSection, qui l'a validee
// (ValidateVoluteSection) ; BuildBolster n'en refait que les controles
// lineaires. Leve std::invalid_argument si F ou d n'est pas positif, si C et R
// ne partitionnent pas S en aire, si le bord de C n'est pas la difference
// symetrique des bords de S et de R, ou si TessellatePolygon refuse C ou R.
std::unique_ptr<Mesh> BuildBolster (const VoluteSection &section, double halfDepth, double relief,
                                    bool splitWallsAtFaces = true);

// ---------------------------------------------------------------------------
//  Placement (axe de la colonne : Z, theta = 0 sur +X, comme le tour)
// ---------------------------------------------------------------------------
//
// Deux coussinets, sur les cotes +X et -X ; leurs faces a volutes sont en
// Y = +-F. Oeil a X = +-centre_oeil_depuis_axe, Z = sommet du chapiteau -
// haut_volute_sous_sommet_chapiteau - centre_depuis_haut ; u croit en
// s'eloignant de l'axe (X = +-(centre_oeil_depuis_axe + u)), v = Z - Z_oeil,
// w = Y. Le sommet du chapiteau est lu sur le profil (membres du bloc
// « chapiteau »). Les coussinets sont des coques fermees separees : elles
// penetrent le tour sans union.

struct VoluteOptions
{
	int    segmentsPerQuarter = 6;
	double scale = 1.0;        // unites monde par partie
};

// Leve std::invalid_argument si l'ordre n'a pas de volute ionique ou si le
// profil n'a pas de chapiteau.
std::vector<std::unique_ptr<Mesh>> BuildIonicVolutes (const OrderData &order, const ColumnProfile &profile,
                                                      const VoluteOptions &options);

} // namespace classical
} // namespace cgarchi
