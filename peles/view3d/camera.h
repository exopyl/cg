#pragma once

#include "bounding_box.h"

#include <memory>

class Ctrackball;

namespace peles
{
namespace view3d
{

// L'ETAT DE VUE : orientation, pivot, distance d'oeil, plans de coupe.
//
// ⚠ AUCUN EN-TETE GL ICI. Ctrackball tire gl_wrapper.h, donc glad/gl.h, qui
// redefinit chaque entree GL par macro : le laisser paraitre dans cet en-tete
// le ferait entrer dans toute unite qui inclut la camera. Il est donc tenu
// derriere un pointeur opaque, comme la fenetre glfw l'est dans platform/.
class Camera
{
public:
	Camera ();
	~Camera ();

	Camera (const Camera&)            = delete;
	Camera& operator= (const Camera&) = delete;

	// ⚠ LES DIMENSIONS SONT CELLES DU RECTANGLE DE LA VUE, JAMAIS CELLES DE LA
	// FENETRE. Elles entrent dans le rapport d'aspect de la projection, dans la
	// normalisation du trackball et dans le rayon de picking : les confondre
	// avec la taille de la fenetre produit une orbite qui saute et un picking
	// decale de la largeur du panneau gauche, sans qu'aucun pixel ne paraisse
	// faux.
	void SetViewSize (int width, int height);

	// Dimensions EFFECTIVEMENT posees, relues a leur source -- le trackball --
	// et non recopiees dans un membre de cette classe.
	//
	// ⚠ ELLES SONT LE DENOMINATEUR DE LA CONVERSION PIXEL -> NDC. Une copie
	// locale pourrait differer de celle que la projection emploie, et le rayon
	// de picking cesserait d'etre l'inverse exact de la projection sans qu'aucun
	// pixel ne paraisse faux.
	int ViewWidth () const;
	int ViewHeight () const;

	// Cadre `target` : pivot au centre, distance d'oeil telle que la sphere
	// circonscrite tienne dans le champ vertical.
	//
	// ⚠ `scene` N'EST PAS `target`, ET LES CONFONDRE FAIT DISPARAITRE DE LA VUE
	// CE QUI N'EST PAS CADRE. `target` fixe ou se place l'oeil ; `scene` fixe ce
	// que les plans near/far doivent encadrer. Cadrer sur un modele parmi
	// plusieurs doit resserrer le premier et laisser le second intact.
	//
	// Une boite cible vide ou reduite a un point laisse l'etat inchange : il n'y
	// a pas de distance a deduire d'un rayon nul.
	void Frame (const BoundingBox& target, const BoundingBox& scene);

	// Pose FRONTALE : rotation identite, l'oeil sur +Z. C'est la seule pose pour
	// laquelle la couleur du pixel central d'une face axiale se calcule a la
	// main, donc la seule sous laquelle le rendu se compare a une valeur
	// predite.
	void SetFrontalRotation ();

	// Emet projection et modelview dans le pipeline fixe. Les deux matrices sont
	// CHARGEES, pas multipliees a ce qui s'y trouve.
	void Apply ();

	// -----------------------------------------------------------------------
	//  LA MANIPULATION, NOMMEE PAR SON EFFET ET NON PAR UN BOUTON DE SOURIS
	// -----------------------------------------------------------------------
	//
	// ⚠ AUCUN BOUTON DE SOURIS DANS CETTE INTERFACE, ET C'EST STRUCTUREL.
	// peles_view3d ne connait ni glfw ni RmlUi : la correspondance geste ->
	// effet est une decision d'interface, elle vit chez l'hote. Ce que la camera
	// expose est ce qu'elle sait faire.
	//
	// ⚠ LES COORDONNEES SONT CELLES DE LA VUE, origine en haut a gauche du
	// RECTANGLE de la vue -- pas de la fenetre. La soustraction de l'origine du
	// rectangle appartient a l'arbitre d'entrees ; la faire deux fois, ou pas du
	// tout, produit une orbite qui saute et un picking decale de la largeur du
	// panneau gauche.
	enum class Drag
	{
		Orbit,
		Dolly,
		Pan
	};

	void BeginDrag (Drag drag, int viewX, int viewY);
	void DragTo (int viewX, int viewY);
	void EndDrag (int viewX, int viewY);

	// Un cran de molette : positif rapproche l'oeil.
	void ZoomStep (float notches);

	// -----------------------------------------------------------------------
	//  LECTURE DE L'ETAT, pour le picking et pour l'instrumentation
	// -----------------------------------------------------------------------

	// Rayon monde du pixel `(viewX, viewY)` de la VUE. La conversion pixel ->
	// NDC vit dans view3d/picking.cpp, seul site du projet ou elle est ecrite.
	void GetPickRay (float ndcX, float ndcY, float origin[3], float direction[3]) const;

	// Direction de visee, du pivot vers l'avant : `normalise(pivot - oeil)`.
	//
	// ⚠ C'EST LA GRANDEUR CONTRE LAQUELLE LE RAYON CENTRAL SE COMPARE. Le rayon
	// du pixel central du rectangle lui est colineaire PAR CONSTRUCTION -- son
	// NDC vaut (0,0) -- donc tout ecart mesure entre les deux est un defaut de
	// conversion de repere, et rien d'autre.
	void GetViewAxis (float axis[3]) const;

	void GetPivot (float pivot[3]) const;
	void SetPivot (const float pivot[3]);

private:
	std::unique_ptr<Ctrackball> m_trackball;
};

} // namespace view3d
} // namespace peles
