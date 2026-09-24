#pragma once

class Mesh;

// ============================================================================
//  Epaississement d'une surface -- « extrusion » d'une forme parametrique
// ============================================================================
//
// Decale la surface de ±thickness/2 le long des normales de sommet (epaisseur
// CENTREE sur la surface d'origine), et relie chaque bord libre par une paroi.
// Une surface ouverte (helicoide, selle) devient une plaque pleine ; une surface
// fermee (sphere, tore) devient une coque creuse.
//
// Trois choix qui ne se devinent pas depuis la signature :
//
//  - Les sommets COINCIDENTS sont soudes pour la topologie et les normales. Les
//    grilles parametriques dupliquent leurs coutures (meridien de la sphere,
//    poles, tour du tore) : sans soudure, chaque couture serait vue comme un bord
//    et recevrait une paroi interne, et ses deux copies s'ecarteraient.
//
//  - Deux copies d'un meme point ne partagent leur normale que si leurs normales
//    propres ne s'opposent pas (cos > -0.5). C'est ce qui garde le ruban de
//    Mobius correct : a sa couture, l'orientation s'inverse, et moyenner les deux
//    cotes donnerait une normale nulle. Les normales y etant opposees, la face +
//    d'un cote coincide avec la face - de l'autre : la couture ne recoit pas de
//    paroi, et le ruban est ferme une fois ses sommets soudes.
//
//  - Epaisseur UNIFORME : le decalage d'un sommet est divise par le plus petit
//    cosinus entre sa normale et celles de ses faces (plafonne a x3). Sans cela,
//    un coin de cube ne recevrait que thickness/√3 sur chaque face.
//
// Sortie entierement triangulee. Renvoie false -- et copie `in` dans `out` --
// si thickness <= 0 ou si `in` n'a aucun triangle.
bool SolidifyMesh (const Mesh &in, float thickness, Mesh &out);
