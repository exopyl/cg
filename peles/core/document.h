#pragma once

#include "document_observer.h"

#include "bounding_box.h"
#include "vmodels.h"

#include <memory>
#include <string>
#include <vector>

namespace peles
{
namespace core
{

// LA SCENE ET LES ETATS DE DOCUMENT : les modeles charges, la selection, le
// survol.
//
// ⚠ CE N'EST PAS UNE VUE, ET LA FRONTIERE EST TENUE PAR L'EDITEUR DE LIENS.
// peles_core ne lie ni cgre, ni glfw, ni RmlUi : aucun type de rendu, de
// fenetre ou d'interface ne peut apparaitre dans cette classe, et elle ne
// dispose d'aucun type auquel remonter. La selection et le survol vivent ici
// parce qu'ils decrivent le DOCUMENT -- quel fichier est designe -- et non la
// facon de le regarder, qui est l'affaire de view3d::Camera.
//
// ⚠ LA DESTRUCTION EST ANNONCEE AVANT D'ETRE FAITE. Tout chemin qui detruit un
// maillage passe par AnnounceRelease : c'est la seule chose qui empeche un
// tampon GL d'etre reattribue a une geometrie qui n'est pas la sienne
// (cf. DocumentObserver).
class Document
{
public:
	Document ();
	~Document ();

	Document (const Document&)            = delete;
	Document& operator= (const Document&) = delete;

	// L'observateur doit survivre au document, ou se retirer avant de mourir.
	void AddObserver (DocumentObserver* observer);
	void RemoveObserver (DocumentObserver* observer);

	// Cree un modele vide et rend un pointeur STABLE : VModels possede ses
	// Model par unique_ptr, donc l'adresse ne bouge pas quand le vecteur croit.
	Model* AddModel (const std::string& name);

	// Retire le modele et detruit ses maillages, apres annonce. Rend false si
	// le modele n'appartient pas a ce document.
	//
	// ⚠ PRECONDITION : UN CONTEXTE GL DU GROUPE DE PARTAGE COURANT. Le document
	// ne connait pas le rendu, mais l'annonce qu'il emet, elle, le rejoint :
	// l'observateur rend au moteur les cles des maillages detruits, ce qui
	// LIBERE DE VRAIS OBJETS GL. Appelee sans contexte courant, la liberation est
	// silencieusement perdue -- et les tampons avec elle.
	bool RemoveModel (Model* model);

	// ⚠ MEME PRECONDITION QUE RemoveModel, pour la meme raison : Clear annonce
	// chaque maillage avant de le detruire. Le destructeur du document appelle
	// Clear -- l'ordre de declaration des membres de l'hote est donc ce qui
	// garantit que la fenetre, donc le contexte, lui survit.
	void Clear ();

	// ⚠ ACCES EN LECTURE SEULE AU CONTENEUR, MAIS Model* MUTABLE : le rendu a
	// besoin de Mesh* non const (MeshRenderer::GetMeshId), et rendre le vecteur
	// modifiable ouvrirait un chemin de destruction qui contournerait l'annonce.
	const std::vector<std::unique_ptr<Model>>& Models () const;

	Model* Selected () const { return m_selected; }
	Model* Hovered () const { return m_hovered; }

	// `model` doit appartenir a ce document ; nullptr efface l'etat.
	void Select (Model* model);
	void Hover (Model* model);

	// Boite englobante agregee des modeles VISIBLES. Vide si la scene l'est.
	BoundingBox VisibleBounds ();

private:
	bool Owns (const Model* model) const;
	void AnnounceRelease (Model& model);
	void ForgetIfPointsTo (const Model* model);

	VModels                          m_models;
	std::vector<DocumentObserver*>   m_observers;

	// Non proprietaires : les Model appartiennent a m_models.
	Model* m_selected = nullptr;
	Model* m_hovered  = nullptr;
};

} // namespace core
} // namespace peles
