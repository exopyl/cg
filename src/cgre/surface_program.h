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

//
// LE PROGRAMME PBR -- metallic-roughness, le modele de glTF 2.0.
//
// UN SECOND PROGRAMME, ET NON UNE BRANCHE DANS LE PREMIER. Deux raisons, et
// aucune des deux n'est esthetique :
//
//   - le programme de surface doit rester BIT-IDENTIQUE pour tout materiau non
//     PBR. Quatre harnais le gardent aujourd'hui, et c'est ce qui rend cette
//     etape verifiable : une branche dans le fragment existant aurait change sa
//     compilation, donc potentiellement son resultat, sur tous les modeles ;
//   - le vertex shader PBR DIVERGE : la carte de normales exige un attribut
//     generique tangente, que le programme de surface n'a pas.
//
// CE QU'IL FAIT. GGX direct depuis les deux gl_LightSource que sinaia pose, plus
// un terme d'irradiance ambiante constant. Echantillonne la couleur de base, la
// carte de normales, la carte metallique-rugosite et l'emissive. L'eclairage se calcule en LINEAIRE et
// la sortie est encodee par pow(1/2.2) DANS LE FRAGMENT : GL_FRAMEBUFFER_SRGB
// re-encoderait aussi les surcouches, dessinees en fixe-fonction dans le meme
// tampon.
//
// CE QU'IL NE FAIT PAS, et ce n'est pas un oubli : ni alphaMode, ni occlusion,
// ni eclairage d'environnement.
//
// Construit paresseusement AU PREMIER MATERIAU PBR, pas au premier dessin : une
// session sans aucun materiau PBR ne paie donc rien, et son rendu ne peut pas
// dependre d'un programme qui n'existe pas. Renvoie nullptr si la construction
// echoue, auquel cas l'appelant retombe sur la projection de Phong.
const GlProgram* PbrProgram ();

//
// LE PROGRAMME DE PHONG ENRICHI -- celui du mode « Materiaux sans PBR ».
//
// LA SOURCE DU PROGRAMME DE SURFACE, compilee une seconde fois avec
// `#define CG_PHONG_EXTRAS`. Les blocs qui en dependent sont retires par le
// preprocesseur du programme ordinaire : la suite de lexemes que celui-ci
// compile est celle d'avant leur ajout, et c'est ce qui le garde identique au
// bit pres pour tout materiau et dans tous les modes existants -- la garantie
// de SurfaceProgram n'est pas negociee contre une branche sur uniforme.
//
// N'EST LIE QUE POUR UN MATERIAU PBR, dans ce mode seulement. Un materiau non PBR
// n'a rien de plus a lui donner : il reste sur SurfaceProgram, donc rendu
// exactement comme en mode Materiaux. Ce que la variante ajoute, et pourquoi le
// reste n'y est pas, est decrit a cgre::PhongExtras (phong_extras.h).
//
// ⚠ gl_FrontMaterial.emission N'Y EST PAS LUE : l'emission arrive par uniforme
// (sinon la projection non texturee, qui en porte deja une, compterait double).
// C'est aussi pourquoi la variante n'est pas faite pour un materiau non PBR.
//
// Construit paresseusement au premier materiau PBR rendu dans ce mode ; nullptr
// si la construction echoue, et l'appelant retombe sur SurfaceProgram.
const GlProgram* PhongExtrasProgram ();

// Location de l'attribut generique `aTangent` dans le programme PBR, ou -1 si
// le programme n'existe pas, si le compilateur l'a elimine, ou s'il l'a place a
// la location 0.
//
// ⚠ LA LOCATION 0 EST REFUSEE, ET CE N'EST PAS DE LA PRUDENCE. En profil de
// compatibilite -- celui que `caps` releve -- l'attribut generique 0 est ALIASE
// sur gl_Vertex : y ecrire une tangente ecraserait les positions et detruirait
// la geometrie, sans erreur GL. Le cas est refuse et journalise, l'appelant
// retombe sur la normale geometrique.
//
// Resolue une fois a la construction du programme, et tracee : c'est la levee
// de la reserve R2.
int PbrTangentAttribLocation ();

// CANAL D'INSPECTION DU PROGRAMME PBR.
//
// `off` est le defaut et le rendu nominal : toute autre valeur fait ecrire au
// fragment un canal BRUT, ni eclaire ni encode, a la place de la couleur.
//
// L'objet est de produire un MASQUE DE SEGMENTATION INVARIANT A L'ECLAIRAGE.
// Un masque tire du rendu final depend du cadrage et des lumieres, donc de ce
// qu'on cherche a mesurer ; tire du canal `metallic`, il ne depend que de la
// donnee, et une capture par cadrage suffit.
//
// Etat GLOBAL du moteur, comme la reserve R2 l'est deja : il n'appartient a
// aucun maillage ni a aucun materiau, et le poser par materiau donnerait un
// masque composite qu'aucune population ne decrirait.
enum class PbrChannel
{
	off       = 0,   // rendu nominal, identique au bit pres
	metallic  = 1,
	roughness = 2,
	normal    = 3,   // normale en espace oeil, apres perturbation, encodee *0,5+0,5
	base      = 4,   // couleur de base LINEAIRE, facteur et carte compris

	// TERME EMISSIF SEUL. Il ne sert pas a regarder la flamme mais a L'EXCLURE :
	// les texels emissifs de Lantern portent aussi metallic = 1, et quelques
	// centaines de pixels satures suffisent a deplacer la moyenne de la
	// population metallique de 0,15 a 0,19 et son ecart-type de 0,08 a 0,24.
	// Une statistique de reflectance polluee par une source n'en est plus une.
	emissive  = 5
};

void       SetPbrChannel (PbrChannel channel);
PbrChannel GetPbrChannel ();

// ENVIRONNEMENT ANALYTIQUE du programme PBR : un degrade lineaire ciel/sol,
// monochrome, evalue EN ESPACE OEIL.
//
// L'espace oeil n'est pas un raccourci : les deux lampes de sinaia sont posees
// avec la modelview a l'identite, donc deja attachees a la camera. Un
// environnement attache au monde exigerait une matrice inverse de plus ET
// tournerait par rapport a des lampes qui, elles, ne tournent pas.
//
// `enabled` faux fait SAUTER le bloc dans le fragment : l'image d'avant E7 est
// alors rendue au bit pres, par construction et non par un argument sur
// l'arithmetique flottante.
//
// Etat global du moteur, comme le canal d'inspection : un environnement par
// materiau n'aurait pas de sens physique.
struct PbrEnvironment
{
	bool  enabled = false;
	float sky     = 0.f;   // radiance au zenith
	float ground  = 0.f;   // radiance au nadir
};

void           SetPbrEnvironment (const PbrEnvironment& env);
PbrEnvironment GetPbrEnvironment ();

// Unites d'echantillonnage du programme PBR : l'indice de cgpbr::MapSlot DECALE
// DE DEUX, et le decalage n'est pas cosmetique.
//
// Les unites 0 et 1 appartiennent au pipeline fixe : ActivateMaterial y lie
// l'albedo et la carte de reflexion, et RIEN NE LES DEFAIT avant les surcouches.
// Le chemin VBO appelle mesh_draw avec `surfaceAlreadyDrawn`, ce qui saute le
// bloc de remplissage et donc l'ActivateDefaultMaterial qui s'y trouve -- le
// seul endroit qui aurait desactive GL_TEXTURE_2D.
//
// Un chemin PBR qui lierait sa couleur de base a l'unite 0 y laisserait une
// texture sRGB la ou le pipeline fixe attend la forme heritee. Le fil de fer n'y
// perdrait rien, etant trace en noir et donc insensible a toute modulation ;
// mais les NORMALES, les POINTS et le repere, dessines juste apres en
// fixe-fonction, s'en trouveraient assombris.
//
// A partir de l'unite 2, le chemin PBR n'a plus rien a defaire : ses liaisons
// sont inertes pour tout ce qui suit, puisque GL_TEXTURE_2D n'y est jamais
// active. `caps` releve 32 unites sur ce materiel, pour 5 requises.
enum PbrTextureUnit
{
	kPbrUnitBaseColor         = 2,
	kPbrUnitNormal            = 3,
	kPbrUnitMetallicRoughness = 4,
	kPbrUnitOcclusion         = 5,
	kPbrUnitEmissive          = 6
};

// Bascule. Pilotable depuis la console distante : `shader on` / `shader off`.
void SetSurfaceShaderEnabled (bool enabled);
bool IsSurfaceShaderEnabled ();

} // namespace cgre
