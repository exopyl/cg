#pragma once
//
//  Les noeuds de PROFIL -- le sous-objet geometrique devenu valeur.
//
// Cinq noeuds pour ce qui etait cinq positions d'un menu, et la difference n'est
// pas cosmetique :
//
//  - un profil se REGLE. « Chanfrein » etait une position ; c'est desormais une
//    largeur et une profondeur, et « Cavet » une courbure de plus ;
//  - un profil se PARTAGE. Le meme noeud alimente la moulure d'une baie
//    gothique, et alimentera le biseau d'un texte ou une section balayee le long
//    d'un chemin -- les trois chantiers qui demandaient ce type ;
//  - un profil se met en CACHE pour lui-meme. Le regler ne refait pas la baie
//    entiere, seulement l'extrusion qui le consomme ;
//  - la famille est OUVERTE. Un profil de plus est un fichier de plus, pas une
//    position a inserer dans une enumeration que deux fichiers epellent.
//
// DEUX TYPES DE PORT, et ils ne sont pas interchangeables : un profil
// d'EBRASEMENT est une polyligne ouverte partant de (0, 0), un profil de BARRE
// est une section fermee. Les brancher l'un pour l'autre produirait une piece
// repliee sans que rien ne le dise ; ici la connexion est REFUSEE.
//
// UNITES : u = enfoncement sous la face de reference, v = decalage dans le plan.
// Le u est RELATIF -- c'est le consommateur qui place le profil sur sa face.
// Les valeurs sont en unites MONDE de la piece consommatrice ; les defauts sont
// ceux que la baie gothique derivait de ses propres offsets par defaut, de
// sorte que brancher un profil neuf sur une baie neuve rende exactement ce que
// rendait la position du menu correspondante.
//
#include "../../core/node.h"

namespace cggraph_nodes
{

// Ebrasement droit -- l'ancienne position « Chamfer ».
class ChamferProfileNode : public cggraph::Node
{
public:
	ChamferProfileNode ();
	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

// Ebrasement en quart de cercle concave. Aucune position du menu ne lui
// correspondait : c'est le premier profil que le modele plat ne savait pas dire.
class CavettoProfileNode : public cggraph::Node
{
public:
	CavettoProfileNode ();
	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

// Barre en demi-rond -- l'ancienne position « Roll bar ».
class RollBarProfileNode : public cggraph::Node
{
public:
	RollBarProfileNode ();
	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

// Barre en arete -- l'ancienne position « Keel bar ».
class KeelBarProfileNode : public cggraph::Node
{
public:
	KeelBarProfileNode ();
	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

// Barre en doucine -- l'ancienne position « Ogee bar ».
class OgeeBarProfileNode : public cggraph::Node
{
public:
	OgeeBarProfileNode ();
	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;
};

} // namespace cggraph_nodes
