#pragma once
//
//  mesh.mounts -- accrocher la piece a un mur.
//
// Un BANDEAU HORIZONTAL prolonge de deux OREILLES PERCEES, fusionne a la piece.
// Rien de plus, et surtout rien d'autre : c'est la plaque murale ordinaire.
//
// ============================================================================
//  POURQUOI un bandeau, et pas deux pastilles posees aux extremites
// ============================================================================
//
// Parce que les vis d'un mur sont de NIVEAU. Deux points pris sur le contour du
// modele -- « la ou il finit a gauche », « la ou il finit a droite » -- sont a
// des hauteurs quelconques : un modele bas a gauche et haut a droite pendrait de
// travers. La ligne de fixation est donc IMPOSEE, jamais deduite de la forme :
// une hauteur, et deux centres sur cette hauteur.
//
// Reste que la matiere n'est pas forcement la a cette hauteur-la. Ce qui relie
// les deux oreilles au corps est alors une bande horizontale -- c'est-a-dire un
// bandeau. Il n'y a donc pas deux objets a concevoir, un « support » et des
// « pastilles » : la fixation EST un bandeau dont les extremites sont percees.
//
// ============================================================================
//  POURQUOI au niveau du MAILLAGE, et donc a la toute fin
// ============================================================================
//
// Parce qu'on AJOUTE de la matiere, et qu'ajouter ne demande aucun booleen 3D :
// la fixation est un solide autonome -- ses trous sont interieurs a son propre
// contour -- et elle se colle au reste par concatenation, exactement comme le
// socle (cf. mesh/merge.h et le §1 bis du dossier de faisabilite).
//
// C'est ce qui la rend GENERALE : elle s'applique a n'importe quel maillage, y
// compris ceux qui ne passent par aucun contour -- une forme parametrique, une
// baie gothique, un fichier importe. PERCER un modele existant, en revanche,
// demanderait de retirer de la matiere, donc ses contours (chaine 2,5D) ou un
// booleen 3D que le depot n'a pas. Ce noeud ne perce que ce qu'il apporte.
//
// ============================================================================
//  OU S'ARRETE LE BANDEAU -- mesure A SA HAUTEUR, pas sur l'emprise totale
// ============================================================================
//
// Un L va jusqu'a la pointe de son pied en bas, et pas plus loin que son
// montant a mi-hauteur. La boite englobante ne rend que la premiere de ces deux
// largeurs : une fixation posee a mi-hauteur d'apres elle deborderait de
// plusieurs millimetres dans le VIDE, oreille comprise.
//
// Les deux bouts sont donc pris sur ce que le maillage occupe DANS LA TRANCHE
// du bandeau (cgmesh/mesh_slab.h), la tranche etant exactement celle que le
// bandeau va occuper. Deux consequences :
//
//   - la fixation se resserre sur la matiere au lieu de suivre un rectangle
//     dessine autour d'elle ;
//   - ses deux extremites tombent SUR de la matiere, puisqu'elles sont prises
//     sur elle -- ce que l'emprise totale ne garantissait pas.
//
// Une tranche VIDE -- un bandeau mince tombant entre deux lignes de texte --
// est refusee : ce n'est pas une panne, c'est la seule reponse honnete.
//
// ⚠ LIMITE, et elle reste reelle. Connaitre les deux BOUTS n'est pas connaitre
// ce qu'il y a entre eux : sur un modele en ANNEAU, les deux bouts sont bien sur
// la matiere, mais le bandeau traverse le vide central entre les deux. Le
// verifier demanderait un test de recouvrement entre solides, ce qui est un
// autre chantier ; en attendant, c'est dit plutot que masque.
//
// ⚠ La cote Z : le bandeau part du BAS de la piece et monte de son epaisseur.
// C'est ce qui le fait mordre dans le socle quand il y en a un, et dans les
// lettres quand il n'y en a pas -- les deux commencent au plateau.
//
// ============================================================================
//  LA FRAISURE, et pourquoi elle ne creuse rien
// ============================================================================
//
// Une tete de vis fraisee doit affleurer, sinon la piece porte sur la tete et
// non sur le mur. Le creux qu'elle demande est un CONE -- et le depot n'a aucun
// booleen 3D pour le retirer. La piece se construit donc a l'envers :
//
//   1. le bandeau est perce au diametre de la BOUCHE, de part en part -- donc
//      trop large, et c'est voulu ;
//   2. une BAGUE par trou remet la matiere qu'il ne fallait pas retirer :
//      pleine en bas jusqu'au diametre du trou de vis, evidee en cone vers le
//      haut (cgmesh/countersink.h).
//
// L'union des trois solides est la piece fraisee, et aucune soustraction n'a eu
// lieu. Zero -- le defaut -- vaut « aucune fraisure », et la piece est alors
// exactement celle d'avant : ni bague, ni percage elargi.
//
// ⚠ LA PROFONDEUR N'EST PAS UN REGLAGE. Diametre de bouche, diametre de trou et
// angle au sommet la determinent ; l'offrir en quatrieme serait offrir de
// decrire un cone qui n'existe pas. Elle est deduite, et une fraisure qui
// traverserait la plaque est REFUSEE plutot que rendue.
//
#include <atomic>

#include "../../core/node.h"

namespace cggraph_nodes
{

class MountsNode : public cggraph::Node
{
public:
	MountsNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// L'ENTRAXE, en millimetres : la distance entre les deux trous. C'est le seul
	// chiffre dont on ait besoin devant le mur, et il ne se regle pas -- il tombe
	// de l'emprise de la piece et du debord. Le publier est donc la seule facon
	// de le connaitre sans mesurer le STL.
	void PublishStats (std::vector<cggraph::NodeStat> &out) const override;

	float GetHoleSpacing () const { return m_holeSpacing.load (); }

private:
	std::atomic<float> m_holeSpacing { 0.f };
};

} // namespace cggraph_nodes
