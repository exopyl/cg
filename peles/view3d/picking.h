#pragma once

class Model;

namespace peles
{
namespace core
{
class Document;
}

namespace view3d
{

class Camera;

// ---------------------------------------------------------------------------
//  LE PICKING : DU CALCUL PUR, DEPLACE ET NON REECRIT
// ---------------------------------------------------------------------------
//
// Portage de sinaia/wxOpenGLCanvas.cpp -- `rayAabb` (:912-935), `ScreenToRay`
// (:953-967), `PickModel` (:969-1026) et `SetPivotFromPixel` (:1035-1070).
//
// ⚠ AUCUNE LIGNE DE wx A L'ORIGINE, ET AUCUNE LIGNE DE GL ICI. Les trois
// fonctions ne lisent aucun etat GL : le rayon est construit depuis la camera,
// jamais par inversion des matrices que la derniere peinture a laissees. C'est
// ce qui rend le resultat independant du moment ou le survol arrive.
//
// ⚠ LES COORDONNEES SONT CELLES DE LA VUE, ORIGINE EN HAUT A GAUCHE DU
// RECTANGLE. L'hote historique pouvait passer les coordonnees de l'evenement
// telles quelles -- son canvas ETAIT la fenetre de rendu. Ici la vue est un
// rectangle du cadre : la soustraction de son origine est faite par l'arbitre
// d'entrees, en amont, et refaire la conversion ici la ferait deux fois.

// Ce qu'un rayon a rencontre. `model` nul : rien.
struct PickHit
{
	::Model* model = nullptr;

	// Distance MONDE le long du rayon -- la direction rendue par ScreenToRay est
	// normalisee, donc ce n'est pas un parametre d'echelle arbitraire.
	float t = 0.f;

	float point[3] = { 0.f, 0.f, 0.f };
};

// Rayon monde du pixel `(viewX, viewY)` de la vue.
//
// Rend false quand la vue n'a pas de dimensions exploitables : il n'y a alors
// aucun repere dans lequel convertir le pixel.
bool ScreenToRay (const Camera& camera, float viewX, float viewY, float origin[3],
                  float direction[3]);

// Modele le plus proche sous le pixel, ou `model` nul.
//
// ⚠ LA BOITE ENGLOBANTE N'EST PAS RECALCULEE ICI, contrairement a l'hote
// historique. Elle est calculee A L'IMPORT (core/loader.cpp) et la geometrie est
// statique ensuite -- le BVH du modele en depend deja. La recalculer parcourrait
// tous les sommets de tous les modeles A CHAQUE DEPLACEMENT DE SOURIS, pour un
// resultat identique. C'est la meme decision que pour la surbrillance de survol.
PickHit PickModel (const core::Document& document, const Camera& camera, float viewX,
                   float viewY);

// ---------------------------------------------------------------------------
//  POSER LE PIVOT SUR LA SURFACE SOUS UN PIXEL
// ---------------------------------------------------------------------------
//
// ⚠ QUATRE ISSUES, ET TROIS D'ENTRE ELLES LAISSENT LE PIVOT INCHANGE AU BIT
// PRES. Les confondre derriere un booleen ferait lire « le clic a manque » comme
// « la vue n'existe pas », et surtout laisserait croire qu'un clic dans le vide
// a fait quelque chose.
enum class PivotPickStatus
{
	Ok,
	NoView,        // la vue n'a pas de dimensions : rien a viser
	OutOfViewport, // le pixel est hors du rectangle de la vue
	NoHit          // aucune surface sous le curseur -- PIVOT INCHANGE
};

struct PivotPick
{
	PivotPickStatus status = PivotPickStatus::NoHit;
	::Model*        model  = nullptr;
	float           t      = 0.f;
	float           point[3] = { 0.f, 0.f, 0.f };
};

// La distance oeil-pivot n'est PAS touchee : choisir autour de quoi on tourne
// n'est pas choisir de combien on recule.
PivotPick SetPivotFromPixel (Camera& camera, const core::Document& document, float viewX,
                             float viewY);

} // namespace view3d
} // namespace peles
