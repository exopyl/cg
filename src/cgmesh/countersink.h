#pragma once
//
//  Bague de FRAISURE : le cone qui recoit une tete de vis fraisee.
//
// ============================================================================
//  POURQUOI UNE BAGUE, alors qu'une fraisure est un CREUX
// ============================================================================
//
// Parce que le depot n'a aucun booleen 3D, et qu'on ne creuse donc rien. La
// piece percee se construit a l'envers :
//
//   1. la plaque est percee au diametre de la BOUCHE (le grand), de part en
//      part -- une extrusion plate ordinaire, celle qu'on sait deja faire ;
//   2. cette bague REMET la matiere qu'il ne fallait pas retirer : pleine en
//      bas jusqu'au diametre du trou de vis, evidee en cone vers le haut.
//
// L'union des deux est la piece fraisee. Aucune soustraction n'a lieu, et les
// deux solides sont fermes : c'est le meme procede que le socle sous les
// lettres (cf. mesh/merge.h), a ceci pres qu'ici il sert a RETIRER de la
// matiere en n'en ajoutant jamais.
//
// Le recouvrement radial (`bite`) n'est pas une precaution decorative : sans
// lui, la paroi exterieure de la bague et celle du percage de la plaque
// seraient EXACTEMENT confondues, deux surfaces opposees au meme endroit, ce
// qu'un trancheur arbitre comme il peut. La bague mord donc de quelques
// centiemes dans la plaque, et l'union redevient sans ambiguite.
//
// ============================================================================
//  CE QU'ELLE N'EST PAS
// ============================================================================
//
// Elle ne connait ni la plaque ni son contour : elle ne voit qu'un CENTRE, deux
// rayons et deux cotes. C'est a l'appelant de garantir que la matiere est
// presente autour -- sans quoi il obtient une rondelle libre, exactement comme
// une pastille qui ne touche rien.
//
#include <vector>

class Mesh;

struct CountersinkCollarOptions
{
	float cx = 0.f, cy = 0.f;

	// Rayon du trou de vis (la partie cylindrique, en bas).
	float holeRadius = 2.25f;
	// Rayon de la BOUCHE, atteint a `zTop`. Doit etre strictement superieur au
	// precedent : une bouche plus etroite que le trou n'est pas une fraisure.
	float mouthRadius = 4.f;

	float zBottom = 0.f;
	float zTop = 1.f;
	// Hauteur du cone, mesuree SOUS `zTop`. Doit rester inferieure a l'epaisseur
	// (`zTop - zBottom`), sinon la fraisure traverse et il ne reste rien pour
	// guider la vis.
	float coneDepth = 1.75f;

	// Echantillonnage du tour. Borne a 8 au minimum, comme circleContour : en
	// dessous, un trou de vis octogonal ne se visse pas mieux qu'un rond.
	int segments = 32;

	// Recouvrement radial dans la plaque, en millimetres (cf. l'en-tete).
	float bite = 0.05f;

	unsigned int materialId = (unsigned int)-1;
};

// Solide FERME, alloue sur le tas (l'appelant le possede), normales calculees et
// tournees vers l'exterieur. Nul quand rien de sense ne peut etre construit :
// bouche plus etroite ou egale au trou, cone plus profond que l'epaisseur,
// epaisseur nulle, rayon negatif.
Mesh* countersinkCollar (const CountersinkCollarOptions& opt);
