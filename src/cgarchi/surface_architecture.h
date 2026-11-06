#pragma once

#include <memory>
#include <vector>

#include <cgmath/TVector2.h>

#include "mesh.h"
#include "polygon2.h"
#include "profile2d.h"

namespace cgarchi
{

// Maillages de demonstration, axes x (largeur), y (epaisseur), z (hauteur).
// Longueurs sans unite propre (unites de la scene). Le maillage rendu n'est
// jamais nul et appartient a l'appelant.
//
// CreateArch : porte en plein cintre entre deux piles, 94 sommets, 133 faces
// (quadrangles et triangles), coque fermee de boite [-5, 5] x [-1, 1] x [0, 6].
// CreateArch2 : 360 sommets sur un cercle de rayon 4 dans le plan z = 0, AUCUNE
// face : forme visee non definie (debt_cgarchi.md).
// CreateBlock : bloc chanfreine centre sur l'origine, 24 sommets, 26 faces
// (18 quadrangles, 8 triangles d'angle), coque fermee de boite
// width (x) x depth (y) x height (z). Aucun parametre n'est valide : le contrat
// suppose des dimensions positives et 0 <= bevel < moitie de la plus petite.
std::unique_ptr<Mesh> CreateArch ();
std::unique_ptr<Mesh> CreateArch2 ();
std::unique_ptr<Mesh> CreateBlock (float width = 1.61803399f, float height = 1.f,
                                   float depth = 1.f, float bevel = 0.1f);

// Echantillonnage d'un arc dans le plan (x, y), pieds sur y = 0.
// points[i] et tangents[i] vont ensemble ; tangents[i] est unitaire et suit le
// sens croissant des indices. Les deux vecteurs ont toujours la meme taille.
struct ArcSamples
{
	std::vector<Vector2f> points;
	std::vector<Vector2f> tangents;
};

// Contrat commun aux quatre fonctions d'arc :
//   - longueurs (a, b, offset) dans l'unite de l'appelant, reprise telle quelle
//     par les echantillons ; e et t sont des rapports -- sauf pour l'accolade
//     et le rampant, qui ignorent a, b, e et t (voir ci-dessous) ; resultat
//     rendu par valeur ;
//   - npts = 0 : ArcSamples vide ;
//   - npts = 1 : std::invalid_argument (un arc exige ses deux extremites) ;
//   - npts >= 2 : exactement npts echantillons.

// Arc brise (arc en tiers-point generalise) de demi-ouverture a et de fleche b,
// tous deux augmentes de `offset`. Pieds en (+-(a+offset), 0), sommet en
// (0, b+offset). Le premier point est le pied droit, le parcours monte vers le
// sommet puis redescend au pied gauche, par symetrie x -> -x.
// npts pair : le sommet figure deux fois (indices npts/2-1 et npts/2).
// npts impair : l'echantillon du milieu est le sommet, de tangente (-1, 0)
// (bissectrice des deux demi-tangentes du point anguleux).
// Leve std::invalid_argument si un parametre n'est pas fini, si a+offset <= 0,
// ou si b+offset <= a+offset (l'arc ne serait pas brise).
ArcSamples sampleArcBrise (float a, float b, unsigned npts, float offset);

// Arc en accolade. ⚠ a, b et e sont IGNORES : la construction utilise toujours
// a = 5, b = 3, e = 0,3 (debt_cgarchi.md). npts/2 echantillons sur chacun des
// deux arcs ; npts impair ajoute un dernier echantillon a l'extremite du second
// arc, qui repete le precedent des que npts >= 5.
ArcSamples sampleArcAccolade (float a, float b, float e, unsigned npts);

// Arc en anse de panier a trois centres, de demi-ouverture a ; e dans [0, 1[
// place les centres lateraux en (+-a*e, 0). e = 0 rend le plein cintre.
// Echantillons a l'angle pi*i/(npts-1), du pied droit au pied gauche.
// Leve std::invalid_argument si un parametre n'est pas fini, si a <= 0 ou si e
// n'est pas dans [0, 1[.
ArcSamples sampleArcAnseDePanier (float a, float e, unsigned npts);

// Arc rampant. ⚠ a et t sont IGNORES : la construction utilise toujours a = 1,
// t = 0,6 (debt_cgarchi.md). npts/2 echantillons sur le premier quart de cercle,
// npts - npts/2 sur le second, dont l'extremite n'est pas atteinte.
ArcSamples sampleArcRampant (float a, float t, unsigned npts);

// Balaie la section fermee `moulure` le long de `curve`, portee dans le plan
// (x, 0, y), sans mise a l'echelle : la courbe et la moulure partagent l'unite
// de l'appelant. Repere en chaque echantillon c : T = tangente portee en (tx, 0, ty),
// B = (0, -1, 0), N = B x T ; le point de section (u, v) (u = enfoncement,
// v = decalage, cf. profile2d.h) va en c + v*N + u*B.
// Resultat : n*m sommets (n echantillons, m points de section), 2*m*(n-1)
// triangles, section refermee sur elle-meme, sans capots. Tout le contenu de
// `out` est remplace (attributs par sommet compris) et sa revision croit. `out`
// est inchange si les arguments sont refuses ou si la construction echoue ; la
// recopie finale dans `out` n'a pas cette garantie, Mesh n'ayant ni deplacement
// ni echange sans exception (debt_cgarchi.md).
// Leve std::invalid_argument si la moulure a moins de 3 points, si la courbe a
// moins de 2 echantillons, si points et tangents n'ont pas la meme taille, si
// une tangente est nulle, ou si une coordonnee n'est pas finie.
void extrudeMoulureAlongCurve (const ArcSamples &curve, const Profile2D &moulure, Mesh &out);

// Rosace de nFoils lobes (3 par defaut), sans unite, inscrite dans le cercle
// unite centre a l'origine : chaque lobe est un cercle tangent interieurement au
// cercle unite et a ses deux voisins ; le premier est centre a l'angle
// pi / nFoils, les suivants s'en deduisent par rotation de 2 pi / nFoils.
// L'appelant met a l'echelle.
class Rosace
{
public:
	Rosace ();
	~Rosace ();

	// Precondition NON VERIFIEE : nFoils >= 2. nFoils = 0 fait ecrire Generate
	// hors de son contour (comportement indefini) ; nFoils = 1 rend un lobe de
	// rayon nul.
	inline void SetNFoils (unsigned int nFoils) { m_nFoils = nFoils; };

	// Rend un Polygon2 alloue par new, jamais nul, dont l'appelant devient
	// proprietaire (delete, ou std::unique_ptr<Polygon2>). Un seul contour
	// (indice 0) de 20 * nFoils points, parcouru lobe apres lobe dans le sens
	// trigonometrique ; il part du point de contact entre le premier lobe et son
	// voisin precedent, et chaque lobe commence au point de contact du precedent.
	Polygon2* Generate (void);

private:
	unsigned int m_nFoils;
	float m_fRadius;
};

// Remplage d'un arc brise dans le plan (x, y), y vertical, dans l'unite de
// l'appelant : un arc principal, deux arcs secondaires identiques centres en
// x = -width / 4 et x = +width / 4, et un cercle central entre eux.
class ArcBrise
{
public:
	ArcBrise ();
	~ArcBrise ();

	// Arc principal : pieds en (+-width / 2, altitude), sommet en
	// (0, altitude + height). width est l'ouverture complete, height la fleche.
	void SetPrincipalArc (float altitude, float width, float height);
	// Arcs secondaires : ouverture width2, fleche height2, pieds a y = altitude2.
	void SetSecondArc (float altitude2, float width2, float height2);

	// Rend un Polygon2 alloue par new, dont l'appelant devient proprietaire
	// (delete, ou std::unique_ptr<Polygon2>), ou nullptr si width2 > width / 2.
	//   - contour 0, 96 points : l'arc principal du pied droit au pied gauche
	//     (32 points, sommet repete), puis l'arc secondaire gauche et l'arc
	//     secondaire droit, chacun de son pied gauche a son pied droit ;
	//   - contour 1, 32 points : le cercle central, sens horaire, premier et
	//     dernier points quasi confondus. Il n'est juste que pour des altitudes
	//     nulles (debt_cgarchi.md).
	// Leve std::invalid_argument si l'un des deux arcs n'est pas un arc brise
	// (voir sampleArcBrise : ouverture positive, fleche > ouverture / 2, valeurs
	// finies). Precondition : SetPrincipalArc et SetSecondArc ont ete appeles ;
	// sinon les valeurs initiales (-1) font lever ou rendre nullptr.
	Polygon2* Generate (void);

private:
	float m_fAltitude, m_fWidth, m_fHeight;
	float m_fAltitude2, m_fWidth2, m_fHeight2;
};

} // namespace cgarchi
