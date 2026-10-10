#pragma once

#include "document_observer.h"

#include <cstddef>
#include <set>

class Mesh;

namespace peles
{
namespace core
{
class Document;
}

namespace view3d
{

class Camera;

// Rendu de la scene 3D dans un rectangle du cadre.
//
// ⚠ CETTE CLASSE NE CONNAIT PAS L'HOTE. Elle ne remonte a aucun type
// applicatif : la cible peles_view3d ne lie ni RmlUi ni glfw, donc le
// `dynamic_cast` vers la fenetre qui parcourt sinaia/wxOpenGLCanvas.cpp ne
// compile simplement pas ici. La frontiere est tenue par l'editeur de liens, pas
// par une convention.
//
// ⚠ ELLE N'EST PAS LE DOCUMENT : elle le LIT. La scene, la selection et le
// survol appartiennent a core::Document ; ce qui vit ici est le seul etat de
// rendu -- ce que les maillages deviennent a l'ecran.
//
// ⚠ ELLE N'EFFACE PAS LE CADRE. glClear appartient a la phase 4 de l'hote : une
// vue qui efface elle-meme effacerait aussi ce que l'interface a dessine avant
// elle.
class SceneRenderer : public core::DocumentObserver
{
public:
	// S'enregistre aupres du document et s'en retire a sa destruction. Le
	// document doit donc lui survivre.
	explicit SceneRenderer (core::Document& document);
	~SceneRenderer () override;

	SceneRenderer (const SceneRenderer&)            = delete;
	SceneRenderer& operator= (const SceneRenderer&) = delete;

	// Ombrage PLAT : chaque face garde la normale de son plan. En ombrage
	// lisse, les sommets d'une arete vive portent une normale moyennee sur
	// toutes les faces qui s'y rejoignent, et une face axiale cesse d'avoir une
	// normale constante -- donc une couleur predictible.
	void SetFlatShading (bool flat);

	// ECLAIRAGE DE MESURE au lieu de l'eclairage produit, et c'est le TROISIEME
	// terme de la pose de mesure, a cote de la rotation frontale et de l'ombrage
	// plat.
	//
	// ⚠ CE COMMUTATEUR EXISTE POUR QUE L'ECLAIRAGE PRODUIT NE SOIT PAS DICTE PAR
	// SON ORACLE. L'eclairage produit est celui de l'hote historique
	// (sinaia/wxOpenGLCanvas.cpp, InitGL) : deux lampes obliques, ambiante
	// globale non nulle, eclairage deux faces. Aucune couleur de pixel ne s'y
	// calcule a la main, donc aucune comparaison a une valeur predite n'y est
	// possible. L'eclairage de mesure -- une lampe directionnelle colineaire a
	// l'axe de vue, tous les autres termes noirs -- la rend possible.
	//
	// ⚠ LE DEFAUT EST L'ECLAIRAGE PRODUIT. L'inverse ferait de la mesure le
	// comportement normal, et la restauration de la fidelite deviendrait une
	// regression aux yeux de l'instrument.
	void SetMeasurementLighting (bool measurement);

	// Dessine dans le rectangle donne en COORDONNEES GL -- origine en bas a
	// gauche. La conversion depuis le repere de RmlUi vit dans
	// ui/viewport_rect.h et nulle part ailleurs.
	void Draw (Camera& camera, int x, int y, int width, int height);

	// Nombre de maillages dont le moteur tient encore les cles.
	//
	// ⚠ C'EST LA GRANDEUR QUE LE CONTRAT DE DESTRUCTION CONCERNE, et rien
	// d'autre ne la rend observable : elle doit retomber a ZERO quand la scene
	// est videe. Une valeur non nulle apres un vidage signifie qu'un Mesh
	// detruit a laisse une cle pendante -- ce qu'aucune erreur GL ne dirait.
	std::size_t RegisteredMeshCount () const { return m_registered.size (); }

	// core::DocumentObserver
	void OnMeshReleasing (Mesh* mesh) override;

private:
	void DrawModels ();
	void DrawHoverHighlight ();

	core::Document& m_document;

	// Maillages confies a MeshRenderer, donc ceux dont il faut lui rendre la
	// cle. Le renderer est un singleton qui survit a cette classe : sans cette
	// liste, sa table conserverait des adresses mortes apres destruction.
	std::set<Mesh*> m_registered;

	bool m_flatShading         = false;
	bool m_measurementLighting = false;
};

} // namespace view3d
} // namespace peles
