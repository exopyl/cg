#pragma once

//
// BASE DE COUPE : un tapis quadrillé affiché sous le modèle, pour lire ses
// dimensions à vue contre les graduations.
//
// Ce N'EST PAS un Model de la scène, et c'est délibéré. Un Model apparaîtrait
// dans le panneau « Models », serait supprimable, sélectionnable au picking,
// compté dans le diagnostic topologique, cadré par la caméra (toute pièce de
// 20 mm serait alors vue à l'échelle des 450 mm du tapis) et — le pire — ÉCRIT
// dans le fichier au Save. La base de coupe est donc un widget du canvas, frère
// de `repere` et de `grid`, piloté par prop.display_cutting_mat.
//
// Second motif, décisif : `prop` est GLOBAL au canvas. Un tapis rendu par le
// pipeline habituel perdrait sa texture — donc son quadrillage, donc sa raison
// d'être — dès que l'ombrage passe en « Neutre » ou l'affichage en fil de fer.
// CuttingMat garde ses propres rendering_properties : matériaux imposés,
// remplissage imposé, surcouches éteintes, plan de coupe désactivé.
//
// REPÈRE. Le modèle n'est JAMAIS déplacé : c'est le tapis qui vient à lui.
//
// La raison est dans les coordonnées elles-mêmes : l'import ne normalise pas par
// défaut, donc elles ont un sens métrique, et les modifier ferait mentir toute
// cote lue ensuite.
//
// Le placement se répartit donc ainsi :
//   - EN X ET Y, une translation CUITE au chargement, qui met le centre du
//     quadrillage sur la graduation (21, 13) et l'origine du monde avec lui ;
//   - EN Z, rien de cuit : la face utile est en Z = 0 dans le repère local, et
//     Draw() reçoit la cote à laquelle la poser. Le canvas lui passe le minimum Z
//     de la scène VISIBLE, si bien que le modèle repose toujours sur la plaque,
//     quelles que soient ses coordonnées.
//
// Le Z est porté par la matrice de vue et non par les sommets : le maillage reste
// immobile, donc son VBO n'est jamais retéléversé quand le niveau change.
//
// gl_wrapper AVANT mesh_renderer : ce dernier tire vertex_buffer_manager.h, qui
// declare des GLuint sans les definir lui-meme. L'ordre etait tenu par hasard
// dans wxOpenGLCanvas.cpp ; ici il est explicite.
#include "../src/cgre/gl_wrapper.h"
#include "../src/cgre/mesh_renderer.h"

class VMeshes;

// UNE INSTANCE PAR CANVAS, et non un singleton partagé — malgré les 54 Mo de
// texture que cela duplique à chaque onglet. La raison est dans le canvas :
// MyGLCanvas construit `new wxGLContext(this)` sans contexte de partage, donc
// chaque onglet a son PROPRE contexte GL. Les identifiants de texture et de VBO
// distribués par MaterialRenderer / MeshRenderer sont propres au contexte où ils
// ont été créés ; un tapis unique partagé entre onglets lierait dans l'un des
// identifiants nés dans l'autre. Des Mesh distincts par canvas garantissent au
// contraire des entrées distinctes dans le renderer, donc des ressources créées
// dans le bon contexte.
class CuttingMat
{
public:
	CuttingMat() = default;
	~CuttingMat();

	CuttingMat(const CuttingMat&)            = delete;
	CuttingMat& operator=(const CuttingMat&) = delete;

	// Dessine le tapis. Charge le fichier au PREMIER appel (donc sous contexte GL
	// courant : le téléversement des textures se fait à l'activation du matériau).
	// Sans effet si le chargement a échoué — l'échec n'est tenté qu'une fois.
	//
	// `light` suit le canvas : c'est un réglage d'éclairage, et la texture y
	// survit. `shading` non — voir l'en-tête. `smooth` non plus, et pour une
	// raison géométrique : les 8 sommets de la plaque sont partagés entre des
	// faces de normales différentes, donc des normales PAR SOMMET y sont
	// forcément des moyennes qui pointent en diagonale. Lissée, la plaque
	// porterait un dégradé sur sa face utile — un faux relief sur l'instrument
	// de mesure. À plat, le rendu prend les normales de FACE, exactes.
	// `z` : cote monde à laquelle poser la face utile de la plaque — le minimum Z
	// de la scène visible, calculé par MyGLCanvas::UpdateCuttingMatLevel.
	void Draw(bool light, float z);

	// Boîte englobante monde du tapis posé à la cote `z`. Statique et calculée sur
	// les cotes du fichier, donc disponible SANS charger l'asset : la caméra en a
	// besoin pour ses plans de coupe, et elle les règle avant que le tapis n'ait
	// jamais été dessiné.
	//
	// Une BOÎTE et non un rayon : la caméra l'unit à la boîte de la scène visible
	// avant d'en tirer une sphère, et un rayon seul ne dirait pas où le tapis se
	// trouve. Il faudrait alors le supposer centré sur l'origine du monde, ce qui
	// gonfle la sphère de profondeur de toute l'excentricité de la scène.
	//
	// Sans cette boîte, la plaque (450 mm de large) sortirait du plan far dès que
	// le modèle observé est petit — ses coins disparaîtraient. C'est le même
	// besoin que couvrait l'`aggregateBbox.AddPoint(2, 2, 1)` d'ApplyNormalization
	// pour la grille, mais réglé sur les plans de coupe seuls : élargir la boîte
	// de cadrage éloignerait aussi la caméra, et le modèle serait vu de trop loin.
	static void Bounds(float z, float outMin[3], float outMax[3]);

private:
	bool Load();

	VMeshes* m_meshes     = nullptr;
	bool     m_loaded     = false;
	bool     m_loadFailed = false;
};
