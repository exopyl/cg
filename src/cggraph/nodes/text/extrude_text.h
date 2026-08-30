#pragma once
//
//  ExtrudeText -- texte + police -> solide extrude, en UN noeud.
//
// Emballe text_to_extruded_mesh TEL QUEL, et le monolithe est ici un choix
// mesure, pas une facilite : la fonction memoise par glyphe -- « MISSISSIPPI »
// place onze glyphes et n'en aplatit que quatre. Decouper en mise en page,
// aplatissement et tessellation ferait de chaque OCCURRENCE une evaluation
// distincte au regard de la signature, et le cache du graphe ne pourrait pas
// recuperer cette economie : sa granularite est le noeud.
//
// Corps lu. Deux comportements a connaitre, tous deux silencieux du point de
// vue du code de retour : un glyphe absent de la police occupe son avance sans
// emettre de contour, comme une espace ; et un texte entierement compose de
// glyphes blancs rend nullptr, ce qui est un echec ici.
//
// L'aplatissement a lieu APRES la mise a l'echelle : "flattenTol" s'exprime donc
// dans les unites du maillage produit, jamais en unites de police.
//
// PLAQUE DE SUPPORT -- le dernier maillon du cas cible. Elle est un CONTOUR de
// plus dans l'union 2D deja ecrite, ajoute avant l'appel Union (subjects,
// NonZero, 6) : un seul volume etanche, aucune coque a recoller, aucun booleen
// 3D -- capacite que ce depot n'a pas.
//
// ⚠ Contrainte, non adoucie : support et texte partagent la profondeur. Il n'y
// a qu'un champ depth. Consequence a connaitre avant de choisir la forme : la
// plaque PLEINE contient l'emprise du texte, donc a profondeur egale elle rend
// sa propre silhouette et les lettres disparaissent dedans -- ce n'est pas une
// panne, c'est ce que « meme profondeur » veut dire. Le bandeau et le cadre,
// eux, ne couvrent pas les lettres et les relient en une piece unique.
//
#include <atomic>

#include "../../core/node.h"
#include "../../../cgmesh/text_extrude.h"

namespace cggraph_nodes
{

class ExtrudeTextNode : public cggraph::Node
{
public:
	ExtrudeTextNode (const std::string &text = std::string ());

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Ce que la DERNIERE passe a reellement execute. C'est l'instrument du
	// monolithe : sans lui, « la memoisation tient » ne serait qu'une lecture de
	// code, et un decoupage ulterieur la detruirait sans qu'un test bronche.
	//
	// Rendu PAR VALEUR, et l'algorithme ecrit dans une variable LOCALE de Compute
	// avant publication : lui laisser l'adresse d'un membre reviendrait a lui
	// faire ecrire l'etat de l'instance, que deux evaluations concurrentes se
	// partageraient. Deux compteurs atomiques suffisent -- la mesure porte sur la
	// derniere passe achevee, jamais sur une passe en cours.
	TextExtrudeStats GetLastStats () const;

private:
	std::atomic<std::size_t> m_glyphsPlaced{ 0 };
	std::atomic<std::size_t> m_glyphsFlattened{ 0 };
};

} // namespace cggraph_nodes
