#pragma once

namespace cgre
{

class GlProgram;

//
// LE PROGRAMME DE SURFACE -- palier 0 du passage au rendu par shaders.
//
// Second chemin de dessin, parallele au pipeline fixe : quand il est actif,
// VBOManager::DrawMaterialGroups lie ce programme avant sa boucle et le delie
// apres. Tout le reste de l'image -- surcouches, repere, grille, tapis de coupe --
// continue en fixe-fonction, ce que le contexte de compatibilite autorise.
//
// ACTIF PAR DEFAUT. La bascule est un booleen, pas une valeur d'enumeration de
// CG_rendering_method : aucun `switch` a rouvrir, et la comparaison A/B se fait
// dans la meme session, a l'ecran. C'est le seul oracle dont dispose un module
// sans test de rendu -- raison pour laquelle `shader off` reste disponible meme
// maintenant que le shader est le chemin normal.
//
// UN CHANGEMENT DE COMPORTEMENT ASSUME, revele par les captures de reference :
// le mode d'ombrage « couleurs par sommet » ne faisait RIEN dans le chemin fixe.
// ActivateNeutralMaterial fait glDisable(GL_COLOR_MATERIAL) et DrawMaterialGroups
// ne le reactive jamais -- le seul glEnable du module est dans mesh_draw, le
// chemin en mode immediat. Le tableau de couleurs etait donc lie au GPU puis
// ignore sous eclairage. Le shader honore uUseVertexColors : il est plus correct
// que ce qu'il remplace, et ce mode se met a fonctionner.
//
// ------------------------------------------------------------------------
// DETTE ASSUMEE, ET SA CONDITION DE SORTIE
// ------------------------------------------------------------------------
// Le GLSL est en `#version 120` et lit les VARIABLES INTEGREES du pipeline
// fixe : gl_ModelViewProjectionMatrix, gl_NormalMatrix, gl_LightSource[],
// gl_FrontMaterial. C'est ce qui permet au chemin VBO existant -- alimente par
// glVertexPointer / glEnableClientState, donc par l'interface de GL 1.5 -- de
// fonctionner INCHANGE : aucun VAO, aucun attribut generique, aucun uniforme,
// aucune modification de MaterialRenderer, de Ctrackball ni de sinaia.
//
// C'est un ECHAFAUDAGE, pas une cible. Il lie cgre au profil de compatibilite.
// Le retirer demande les attributs generiques (palier 2) puis les uniformes
// explicites (palier 3), soit 4 a 7 jours SANS aucun benefice visible : c'est du
// remboursement, pas de la fonctionnalite. Le report est une decision legitime,
// prise en connaissance de cause -- elle est ecrite ici pour que la question
// puisse se reposer, au lieu de disparaitre.
//

// Le programme, construit paresseusement au premier appel : la compilation exige
// un contexte GL courant, ce qu'aucun constructeur statique ne peut garantir.
// Renvoie nullptr si la construction a echoue -- l'appelant retombe alors sur le
// pipeline fixe plutot que de ne rien dessiner. La cause part dans le journal.
//
// Un programme est un objet de DONNEES : il appartient au groupe de partage GL,
// donc un seul suffit pour tous les onglets de sinaia.
const GlProgram* SurfaceProgram ();

// Bascule. Pilotable depuis la console distante : `shader on` / `shader off`.
void SetSurfaceShaderEnabled (bool enabled);
bool IsSurfaceShaderEnabled ();

} // namespace cgre
