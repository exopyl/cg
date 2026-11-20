#pragma once
//
//  Normales a seuil d'angle (crease angle), par COIN de face
//
// Une normale par sommet, moyenne de TOUTES les faces incidentes
// (Mesh::ComputeNormals), est fausse a une arete vive : le coin d'un prisme
// melange des faces a 90 degres ou plus, et le lissage de Gouraud etale cette
// normale sur toute l'etendue des triangles voisins.
//
// REGROUPEMENT PAR ARETES. Autour de chaque sommet, les faces de l'eventail
// sont reunies en groupes : deux faces qui partagent une arete issue du sommet
// sont dans le meme groupe quand l'angle entre leurs normales est STRICTEMENT
// inferieur au seuil, et la relation est propagee de proche en proche. Chaque
// groupe recoit UNE normale, donnee a tous ses coins. Une arete plus vive que
// le seuil coupe donc l'eventail ; une surface facettee plus douce que le
// seuil garde une normale unique par sommet, meme quand ses faces extremes
// font entre elles plus que le seuil (sphere, cylindre).
//
// Eventail ouvert (bord) : seules les aretes partagees relient des faces.
// Arete non variete (trois faces ou plus) : toutes les paires de faces sous
// le seuil sont reliees.
//
// PONDERATION : par l'angle INTERIEUR du coin de chaque face au sommet
// (Thurmer et Wuthrich, 1998) -- au-dela de 180 degres au sommet rentrant d'un
// polygone concave. Elle rend la normale independante de la triangulation : un
// quad coupe en deux triangles pese autant qu'un quad entier, et le flanc d'un
// cylindre facette recoit la normale radiale.
//
// NORMALE DE FACE : celle de Newell, en double. Elle vaut la normale du plan
// pour un triangle et reste definie pour un polygone non plan.
//
// Rien ici ne modifie le maillage : Mesh::ComputeNormals garde sa semantique
// (moyenne sans seuil, une normale par sommet), sur laquelle reposent la
// decimation, la courbure et l'export.
//
#include <vector>

class Mesh;

// Seuil par defaut, en degres. Il separe les facettes d'une surface courbe
// maillee finement (sous le seuil) des aretes vives du modele (au-dessus).
// Mesure qui le justifie : debt_cgmesh.md §2.9.
constexpr float kDefaultCreaseAngleDeg = 22.0f;

// Valeur de seuil qui demande les normales PAR SOMMET du maillage
// (Mesh::BuildPolygonRenderData) au lieu des normales a seuil. Tout seuil
// negatif a le meme sens.
constexpr float kCreaseAngleMeshNormals = -1.0f;

struct CreaseNormals
{
	// Premier coin de la face f : cornerOffsets[f] ; la face f a
	// cornerOffsets[f + 1] - cornerOffsets[f] coins, dans l'ordre de ses
	// sommets. Taille GetNFaces () + 1. Une face supprimee (trou, cf.
	// Mesh::RemoveFace) a zero coin.
	std::vector<unsigned int> cornerOffsets;

	// 3 flottants par coin, unitaires. Deux coins d'un meme groupe portent des
	// valeurs IDENTIQUES au bit pres : un consommateur peut dedoubler par
	// egalite exacte.
	std::vector<float> cornerNormals;

	unsigned int GetNCorners (void) const
		{ return cornerOffsets.empty () ? 0u : cornerOffsets.back (); }
};

// Normales de coin du maillage, sans le modifier.
//
// `creaseAngleDeg` est borne a [0, 180]. A 0, chaque coin recoit la normale de
// sa face (ombrage plat) ; a 180, toute paire de faces voisines est reliee sauf
// deux faces exactement opposees.
//
// Cas degeneres :
//   - face d'aire negligeable (norme de Newell sous 1e-6 fois le carre de sa
//     plus longue arete), de moins de trois coins, ou dont un indice de sommet
//     sort des bornes : elle ne relie rien et ne contribue a aucune normale.
//     Ses coins recoivent la moyenne ponderee de toutes les faces valides
//     incidentes au sommet, et (0, 0, 1) s'il n'y en a aucune ;
//   - somme nulle (faces opposees) : la normale de la face du coin.
CreaseNormals ComputeCreaseNormals (const Mesh &mesh, float creaseAngleDeg = kDefaultCreaseAngleDeg);

// Copie du maillage, TRIANGULEE (Mesh::Triangulate), dont les sommets sont
// dedoubles la ou les normales de coin divergent, et qui porte ces normales par
// sommet. Le maillage source n'est pas modifie.
//
// La triangulation est faite d'abord, et les normales calculees sur la copie
// triangulee : un consommateur qui rend les polygones a plat
// (Mesh::BuildPolygonRenderData, donc l'export glTF) recoit ainsi des
// triangles, qui portent les normales par sommet.
//
// Les GetNVertices () premiers sommets de la copie sont ceux de la source, dans
// le meme ordre ; les doublons sont ajoutes a la suite. Les materiaux, les
// points et les lignes sont conserves. Voir Mesh::SplitVerticesByCornerNormals
// pour le transport des autres attributs.
//
// ⚠ Exception a l'ordre des sommets : un maillage dont les UV ne sont pas
// paralleles aux sommets passe par Mesh::SplitVerticesByUVSeams, qui
// renumerote les sommets. Les normales de coin, elles, sont calculees avant :
// une couture d'UV ne coupe pas le lissage.
Mesh BuildCreaseSplitMesh (const Mesh &mesh, float creaseAngleDeg = kDefaultCreaseAngleDeg);
