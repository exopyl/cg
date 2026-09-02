#pragma once
//
//  Types de domaine qui transitent sur les liens.
//
// Le moteur ne connait aucun type applicatif ; c'est ICI qu'ils sont nommes et
// enregistres. La convention est "domaine.Type", et le nom est SERIALISE : le
// renommer casse les documents deja ecrits.
//
// Un seul registre pour toute la couche, et il n'y a pas d'autre choix :
// l'egalite de type est une egalite de POINTEUR de descripteur, donc deux
// registres produiraient deux "cgmesh.Mesh" incompatibles entre eux, refuses a
// la connexion sans que rien ne dise pourquoi.
//
// Ce fichier est a la RACINE de la couche, non dans un sous-repertoire de
// domaine : mesh/ et text/ en dependent tous les deux, et le faire vivre dans
// l'un des deux ferait de l'autre son client. Le rangement par domaine ne doit
// pas devenir un couplage entre domaines.
//
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "../core/type_registry.h"

// Declaree et non incluse : ce fichier ne sert que d'entete de types, et
// mesh.h coute cher a chaque unite qui l'inclut. shared_ptr accepte un type
// incomplet -- son suppresseur est fixe a la CONSTRUCTION, dans value_types.cpp
// et dans les adaptateurs, qui eux ont la definition.
class Mesh;

// Declaree et non incluse, pour la meme raison que Mesh : image.h tire
// cgmath.h en entier (cf. l'audit de dette, debt_cgimg.md), et ce fichier ne
// sert que d'en-tete de types.
class Img;

namespace cggraph_nodes
{

// Sous-ensemble de sommets d'un maillage, par indice. C'est le type du port
// "zone" du descripteur de lissage : une zone absente lisse tout le maillage.
struct Selection
{
	std::vector<unsigned int> vertices;
};

// Champ scalaire par sommet -- ce qu'une mesure rend, et ce qu'un coloriage
// consomme. Trois noeuds le produisent (courbure, occlusion ambiante,
// epaisseur) et un le consomme (carte de couleurs) : c'est le seul type du
// catalogue qui existe pour RELIER deux familles d'algorithmes plutot que pour
// emballer une structure de cgmesh.
//
// `defined` VIDE signifie « tout est defini ». Ce n'est pas une commodite : les
// corps emballes rendent tantot un tableau de drapeaux (epaisseur), tantot
// rien du tout (occlusion), et un vecteur de 1 fabrique par l'adaptateur
// couterait une allocation par sommet pour dire ce que son absence dit deja.
struct ScalarField
{
	std::vector<float> values;
	std::vector<char>  defined;

	bool IsDefined (std::size_t i) const
	{
		return defined.empty () || (i < defined.size () && defined[i] != 0);
	}
};

// Suite ORDONNEE de maillages -- ce qu'une decomposition rend, et ce qu'un
// ForEach parcourt. Elle porte des `shared_ptr<const Mesh>` et non des Mesh :
// un element se retrouve tel quel sur le lien d'un sous-graphe, sans copie,
// exactement comme la valeur d'un port de maillage.
//
// L'ORDRE est une propriete, pas une commodite : c'est lui que ForEach verse a
// la signature de chaque iteration sous forme d'INDICE. Une source qui rendrait
// ses pieces dans un ordre variable ferait varier les signatures sans que le
// contenu bouge -- des recalculs, jamais un resultat faux.
struct MeshArray
{
	std::vector<std::shared_ptr<const Mesh>> items;
};

// Les onze descripteurs enregistres. Leurs adresses sont stables pour la duree
// du programme : le registre les stocke en deque et ne les detruit jamais.
struct DomainTypes
{
	const cggraph::TypeDesc *mesh = nullptr;

	// Raster RGBA8. UN SEUL type d'image, et non un « image brute » distinct d'un
	// « image quantifiee » : la difference entre les deux n'est pas une propriete
	// de la structure -- un Img quantifie est un Img dont les pixels ne prennent
	// qu'un petit nombre de valeurs -- et rien dans le type ne pourrait la
	// verifier. La distinguer au systeme de types promettrait donc une garantie
	// que la connexion ne tient pas ; c'est la documentation des noeuds qui dit
	// lesquels attendent un raster deja quantifie.
	//
	// (C'est l'inverse du choix fait pour les contours et les profils juste
	// dessous, ou les deux formes different REELLEMENT -- courbes contre
	// polylignes, ouvert contre ferme -- et ou la confusion serait muette.)
	const cggraph::TypeDesc *image = nullptr;

	// CHEMIN d'une ressource -- ce qu'un noeud file.ref publie et ce que les
	// chargeurs acceptent en entree optionnelle.
	//
	// Un chemin et non des octets, et ce n'est pas un detail de commodite :
	// MeshIO est INTEGRALEMENT base sur des noms de fichiers (quatorze
	// importeurs, aucune entree en memoire), et un OBJ resout son .mtl compagnon
	// par chemin RELATIF -- `import_mtl (Mesh&, filename, path)`. Un tampon n'a
	// pas de repertoire : une conception « octets » ne pourrait pas charger un
	// OBJ avec ses materiaux. Les trois consommateurs, eux, savent tous lire par
	// nom (Font::loadFromFile, Img::load, MeshIO::load).
	//
	// Type DISTINCT d'une chaine ordinaire, dans le meme esprit que splayProfile
	// et barProfile : on ne doit pas pouvoir brancher un parametre texte
	// quelconque dans une entree de fichier.
	//
	// ⚠ Le lien porte un NOM, pas un contenu. Ce qui relie la signature de l'aval
	// au CONTENU du fichier, c'est le parametre semantique `source.identity` de
	// file.ref : la signature d'un noeud inclut toute sa branche amont, donc un
	// fichier modifie invalide le cache de ses consommateurs bien que la chaine
	// sur le lien n'ait pas bouge.
	const cggraph::TypeDesc *path = nullptr;

	// Type DISTINCT de `mesh`, et non un maillage qu'on lirait par morceaux :
	// brancher une suite la ou un maillage est attendu est refuse a la
	// connexion, ce qui est la seule facon d'empecher qu'un noeud ne traite en
	// silence le premier element pour le tout.
	const cggraph::TypeDesc *meshArray = nullptr;

	const cggraph::TypeDesc *font = nullptr;
	const cggraph::TypeDesc *selection = nullptr;
	const cggraph::TypeDesc *scalarField = nullptr;

	// Deux types DISTINCTS pour les contours, et c'est structurel : un contour
	// de glyphe porte des courbes en unites de police, un contour a extruder
	// porte des polylignes en unites monde. L'aplatissement qui mene du premier
	// au second depend de l'echelle finale ; les confondre le ferait a la
	// mauvaise finesse, silencieusement. Aucun noeud du catalogue ne les fait
	// transiter -- l'extrusion de texte est monolithique --, la distinction vit
	// donc au systeme de types, ou une connexion croisee est refusee.
	const cggraph::TypeDesc *glyphContours = nullptr;    // COURBES, unites de police
	const cggraph::TypeDesc *extrudeContours = nullptr;  // APLATIS, unites monde

	// Deux types pour UN meme Profile2D, et la distinction est de meme nature
	// que celle des contours ci-dessus : un profil d'EBRASEMENT est une
	// polyligne ouverte partant de (0, 0), enfoncee sous la face avant ; un
	// profil de BARRE est une section FERMEE, balayee le long d'un chemin. Le
	// consommateur de l'un lirait l'autre sans broncher et rendrait une piece
	// repliee -- l'echec serait geometrique, donc muet. Deux descripteurs, et
	// c'est la connexion qui refuse.
	const cggraph::TypeDesc *splayProfile = nullptr;   // OUVERT, depuis (0, 0)
	const cggraph::TypeDesc *barProfile = nullptr;     // FERME, section balayee
};

const DomainTypes &Types ();

} // namespace cggraph_nodes
