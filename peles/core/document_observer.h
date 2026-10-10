#pragma once

class Mesh;

namespace peles
{
namespace core
{

// Ce que le document ANNONCE avant de detruire de la geometrie.
//
// ⚠ INTERFACE DECLAREE DANS core/, IMPLEMENTEE DANS view3d/. C'est une
// inversion de dependance, et c'est elle qui permet au document de prevenir le
// rendu sans connaitre son existence : peles_core ne lie pas cgre, donc aucun
// type de rendu ne peut apparaitre ici.
//
// ⚠ LE CONTRAT PORTE PAR CETTE INTERFACE N'EST PAS UNE COMMODITE.
// MeshRenderer indexe ses tampons GL par Mesh* (src/cgre/mesh_renderer.h). Un
// maillage detruit sans que le rendu ait ete prevenu y laisse une cle pendante,
// et un Mesh realloue a la meme adresse -- ce que l'allocateur fait
// volontiers -- heriterait des tampons du precedent : une geometrie s'afficherait
// a la place d'une autre, sans une seule erreur GL.
class DocumentObserver
{
public:
	virtual ~DocumentObserver () = default;

	// Appelee AVANT la destruction de `mesh`, qui est encore VIVANT a cet
	// instant. La liberation des materiaux enumere le maillage : l'appeler
	// apres coup lirait de la memoire liberee.
	//
	// ⚠ UN CONTEXTE GL DU GROUPE DE PARTAGE COURANT EST EXIGE DE L'APPELANT,
	// c'est-a-dire de celui qui detruit de la geometrie. L'implementation rend au
	// moteur de rendu les cles du maillage, et cette restitution libere des
	// objets GL. La precondition est reportee sur Document::RemoveModel et
	// Document::Clear, les deux seuls emetteurs.
	virtual void OnMeshReleasing (Mesh* mesh) = 0;
};

} // namespace core
} // namespace peles
