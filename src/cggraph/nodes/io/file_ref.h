#pragma once
//
//  FileRef -- une RESSOURCE DESIGNEE, rendue sur un port sous forme de chemin.
//
// Ce noeud existe pour une raison precise, et ce n'est pas l'uniformite : sans
// lui, UN DOCUMENT ENREGISTRE NE SE ROUVRE PAS. Les sources qui portent leur
// contenu en memoire ne le serialisent pas -- seule son EMPREINTE l'est, par le
// parametre interne `source.identity`. Un document relu porte donc le hash d'un
// contenu qu'il n'a plus : il reclame une ressource qu'il ne sait pas nommer.
//
// IL NE LIT RIEN, et c'est pourquoi il ne s'appelle pas `file.load`. Il DESIGNE
// un fichier ; ce sont les chargeurs en aval qui l'ouvrent, chacun avec l'API
// qui lui convient -- Font::loadFromFile, Img::load, MeshIO::load. Le nom
// `file.load` reste donc libre pour un vrai lecteur vers un tampon, si le besoin
// s'en presente.
//
// POURQUOI UN CHEMIN ET NON DES OCTETS. MeshIO est integralement base sur des
// noms de fichiers : quatorze importeurs, aucune entree en memoire. Et un OBJ
// resout son .mtl compagnon par chemin RELATIF (`import_mtl (Mesh&, filename,
// path)`) -- un tampon n'a pas de repertoire, donc une conception « octets » ne
// pourrait pas charger un OBJ avec ses materiaux. Pas « difficilement » : pas du
// tout, sans inventer un schema de resolution virtuel.
//
// IL N'EST PAS INERTE POUR AUTANT. Il SURVEILLE le fichier : RefreshExternalState
// releve son etat et le verse a `source.identity`, parametre SEMANTIQUE. Comme la
// signature d'un noeud inclut toute sa branche amont, un fichier modifie invalide
// le cache de ses consommateurs -- alors meme que la chaine qui transite sur le
// lien, elle, n'a pas bouge. C'est la vraie justification du noeud : UN SEUL
// endroit qui surveille le fichier, au lieu de chaque chargeur reimplementant son
// stat.
//
// `path` designe un fichier reellement ouvrable par fopen. Ce n'est pas une URL :
// nativement c'est un chemin de systeme de fichiers, et sous WebAssembly un
// chemin MEMFS -- c'est a l'HOTE d'y avoir depose la ressource, y compris quand
// elle vient d'un fetch ou d'un fichier choisi a la souris.
//
#include <string>

#include "../../core/node.h"

namespace cggraph_nodes
{

class FileRefNode : public cggraph::Node
{
public:
	FileRefNode ();

	const cggraph::NodeDesc &GetDesc () const override;
	bool Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
	              cggraph::ValueList &out) override;

	// Releve l'etat du fichier designe. Sans ce moment, la signature resterait
	// celle du dernier calcul et le cache resservirait un resultat perime apres
	// modification du fichier -- sans jamais planter.
	//
	// Un STAT et non un hash : hacher pour decider s'il faut relire OBLIGE a
	// lire, donc annule exactement l'economie que le cache existe a produire
	// (cf. file_identity.h).
	//
	// ⚠ Sous WebAssembly, le mtime d'un fichier MEMFS est celui de son ECRITURE.
	// Un hote qui reecrit la meme ressource a chaque import lui donnerait donc
	// une identite neuve a chaque fois, et le cache manquerait a tous les coups.
	// L'hote doit n'ecrire QUE si le chemin est absent.
	void RefreshExternalState () override;

	void SetPath (const std::string &path);
	std::string GetPath () const;

private:
	void RefreshIdentity ();
};

} // namespace cggraph_nodes
