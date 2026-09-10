#pragma once
//
//  Tangentes par sommet
//
// Base tangente destinee a l'echantillonnage d'une CARTE DE NORMALES : le nuanceur
// reconstruit la bitangente par cross (N, T) * w, ou w vaut +1 ou -1. C'est la
// convention de glTF 2.0 (attribut TANGENT, VEC4) et celle de cgre2::VertexPBR.
//
// METHODE : accumulation par triangle des derivees de la parametrisation
// (T = dP/du, B = dP/dv), puis orthogonalisation de Gram-Schmidt contre la
// normale du sommet. C'est le noyau de MikkTSpace sans son etape de fusion par
// coin -- assez pour un stockage PAR SOMMET, qui est celui de Mesh.
//
// ⚠ CE QUE CE STOCKAGE NE SAIT PAS REPRESENTER. Un sommet partage par deux
// ILOTS UV (un coin de couture) porte deux parametrisations, donc deux
// tangentes. Une seule case existe : la moyenne ecrite y est FAUSSE pour les
// deux ilots, et aucune ecriture par sommet ne peut faire mieux. Le seul
// remede est de dupliquer le sommet AVANT
// (Mesh::SplitVerticesByUVSeams), apres quoi chaque sommet n'appartient plus
// qu'a un ilot et la moyenne redevient exacte. Cette categorie est donc exclue
// des criteres de correction de ce module, par nature et non par negligence.
//
// Les tangentes sont une DERIVATION, au meme titre que les normales par sommet :
// les ecrire NE TOUCHE PAS la revision de geometrie. La consequence pratique est
// la meme que pour les normales -- generer les tangentes APRES un televersement
// de VBO ne le fait pas refaire. On les genere a l'import, avant tout rendu.
//
#include <vector>

class Mesh;

// Ecrit les tangentes par sommet du maillage (4 flottants par sommet, xyz + w).
//
// Rend FAUX et NE TOUCHE PAS au maillage quand celui-ci n'a AUCUNE coordonnee
// de texture : sans parametrisation il n'y a pas de tangente a calculer, et
// remplir le tableau d'une valeur par defaut masquerait cette absence derriere
// une donnee d'apparence valide.
//
// `uvSet` DOIT ETRE LE JEU QUE LA CARTE DE NORMALES ECHANTILLONNE, et non un
// jeu au choix de l'appelant : une base tangente n'a de sens que pour UNE
// parametrisation. glTF 2.0 le dit explicitement -- le TANGENT accompagne « the
// texture coordinates associated with the normal texture ». Batie sur un autre
// jeu, elle reste une base valide et unitaire, donc INDISCERNABLE d'une bonne
// pour un lecteur conforme, et l'eclairage est faux sans aucun signal.
//
// Seules les valeurs 0 et 1 existent (cf. material_pbr.h, TextureRef::uvSet) :
//   0 -> Mesh::GetTextureCoordinates ()  -- resolu PAR COIN, meme regle que
//        Mesh::BuildPolygonRenderData : indice de coin quand la face en porte
//        un, indice de sommet sinon ;
//   1 -> Mesh::GetTextureCoordinates1 () -- PARALLELE AUX SOMMETS par contrat,
//        donc toujours resolu par indice de sommet.
// Toute autre valeur est rabattue sur 0.
//
// Les faces de plus de trois coins sont triangulees en eventail pour
// l'accumulation seulement -- le maillage n'est pas modifie.
//
// Les normales par sommet servent a l'orthogonalisation quand elles sont
// presentes et parallelisees aux sommets ; sinon la tangente accumulee est
// simplement normalisee et w vaut +1.
bool generateTangents (Mesh& mesh, unsigned int uvSet = 0);

// Supprime les tangentes. Apres appel, Mesh::GetNTangents() vaut 0 et
// BuildPolygonRenderData n'emet plus le tableau correspondant.
void clearTangents (Mesh& mesh);
