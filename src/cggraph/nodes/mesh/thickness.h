#pragma once
//
//  Thickness -- epaisseur de paroi par sommet (Shape Diameter Function).
//
// Emballe MeshAlgoThickness::ComputeShapeDiameter (thickness.h). Corps lu, et
// cinq choses en sortent que la declaration ne dit pas :
//
//  - la signature prend `Mesh &` NON const, et le corps y ECRIT : il recalcule
//    les normales par sommet si elles manquent, et rafraichit la boite
//    englobante. D'ou la copie profonde ici ;
//  - le corps est PARALLELE -- parallelChunks cree son propre pool de
//    std::thread. Le jeton d'annulation doit atteindre le lambda, ce qui est
//    exactement la raison pour laquelle D12 passe un objet et non un
//    thread_local ;
//  - `numRays` et `coneHalfAngleDeg` sont BORNES EN SILENCE par le corps :
//    numRays est ramene dans [1, 256], le demi-angle dans [0, 80 degres]. Un
//    reglage hors bornes ne produit ni erreur ni avertissement ;
//  - un sommet sans normale utilisable, ou dont aucun rayon ne rencontre de
//    paroi opposee, reste a 0 avec `defined` a 0. Zero n'est donc PAS une
//    epaisseur nulle, et confondre les deux donne une carte fausse la ou le
//    maillage est ouvert ;
//  - le lissage final (`smoothIterations`) porte sur le champ, pas sur la
//    geometrie, et il ne franchit pas la frontiere des sommets non definis.
//
// Avec numRays = 1, coneHalfAngleDeg = 0 et smoothIterations = 0, le corps se
// reduit exactement a ComputeWallThickness -- le rayon unique de la methode M2.
// Les trois parametres sont donc portes tels quels, sans qu'aucun ne soit
// realise par l'adaptateur.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

class ThicknessNode : public cggraph::Node
{
public:
	ThicknessNode (int numRays = 16, float coneHalfAngleDeg = 60.0f,
	               int smoothIterations = 1);

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
