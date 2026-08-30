#pragma once
//
//  Canvas de graphe -- UNIQUE, et c'est une decision (D4).
//
// Une seule implementation de canvas pour tous les hotes. Ce fichier ne connait
// ni fenetre, ni contexte graphique, ni boucle d'evenements : il ne fait que
// des appels ImGui, et l'hote lui fournit une frame commencee. C'est ce qui
// permet a un second hote -- une page web, une vue Qt -- de le reutiliser sans
// qu'une ligne soit reecrite, et c'est aussi ce qui rend visible le jour ou
// quelqu'un ecrirait un deuxieme canvas : il faudrait le mettre ailleurs.
//
// Il ne dessine rien qu'il decide lui-meme : la palette, les panneaux et l'etat
// des ports viennent du modele, qui les derive du registre. Aucun type de noeud
// n'est nomme ici, et il ne doit jamais y en avoir -- ce fichier ne change pas
// quand le catalogue grandit.
//
// FRONTIERE §7.3, et elle n'est plus une precaution : l'evaluation TOURNE sur un
// fil separe. Le canvas lit le modele, jamais l'interieur d'un noeud ; il ne
// detient aucune valeur, ne prend aucun pointeur sur une sortie, et ne declenche
// de calcul que par EditorModel::RequestEvaluation. Une frame commence par
// EditorModel::Poll -- rien ne remonte du fil de calcul autrement.
//
// La seule chose que le canvas ecrit dans un noeud, ce sont les champs de
// l'inspecteur, qui pointent DANS ses parametres. Ils sont donc grises tant que
// EditorModel::IsEvaluating rend vrai. C'est le seul endroit du depot ou la
// discipline du §7.3 se traduit en code, et ce fichier n'est couvert par aucun
// test : voir le commentaire pose a l'endroit du gel.
//
#include <string>
#include <vector>

#include "../core/graph.h"
#include "canvas_layout.h"

namespace ax
{
namespace NodeEditor
{
struct EditorContext;
}
} // namespace ax

namespace cggraph_ui
{
class EditorModel;
}

namespace cggraph_canvas
{

class NodeCanvas
{
public:
	NodeCanvas ();
	~NodeCanvas ();

	NodeCanvas (const NodeCanvas &) = delete;
	NodeCanvas &operator= (const NodeCanvas &) = delete;

	// A appeler entre le debut et la fin d'une frame ImGui de l'hote. Dessine la
	// palette, le graphe et l'inspecteur, et applique a `model` les gestes que
	// l'utilisateur a faits.
	void Draw (cggraph_ui::EditorModel &model);

	// L'HOTE VIENT DE CHARGER UN DOCUMENT : cadrer la vue sur les noeuds a la
	// prochaine frame, une fois.
	//
	// Le declencheur est ici et non dans le canvas parce que « un chargement a
	// eu lieu » n'est pas une information que le canvas possede : il voit un
	// graphe, jamais l'evenement qui l'a rempli. Une detection interne devrait
	// s'appuyer sur un changement de structure -- or ajouter un noeud a la
	// souris en est un, et celui-la ne doit surtout pas recadrer.
	//
	// La demande est CONSOMMEE UNE FOIS. La rejouer a chaque frame reprendrait
	// la vue a l'utilisateur des qu'il la deplace, et l'editeur deviendrait
	// innavigable.
	//
	// ⚠ LE ZOOM CHANGE AUSSI, et c'est assume : le cadrage se fait avec marge.
	// Un recentrage a zoom constant laisserait hors cadre tout document plus
	// large que l'affichage.
	void RequestFitToContent ();

private:
	void DrawPalette (cggraph_ui::EditorModel &model);
	void DrawGraph (cggraph_ui::EditorModel &model);
	void DrawInspector (cggraph_ui::EditorModel &model);

	ax::NodeEditor::EditorContext *m_context = nullptr;

	// Disposition en cours, et la taille d'affichage dont elle est derivee. Les
	// deux panneaux flottants ne sont REPOSES que lorsque cette taille change :
	// entre deux changements l'utilisateur les deplace et les redimensionne
	// librement, et rien ne les lui reprend. Le graphe, lui, est repose a chaque
	// frame -- il doit rester a (0, 0), sans quoi l'editeur de noeuds perd
	// autant de pixels que son origine (voir canvas_layout.h).
	CanvasLayout m_layout;
	float m_displayWidth = 0.0f;
	float m_displayHeight = 0.0f;
	bool m_relayout = true;

	// Noeuds dont la position a deja ete poussee vers l'editeur. Sans cette
	// trace, la position du graphe serait reimposee a chaque frame et le
	// deplacement a la souris serait annule aussitot fait.
	std::vector<cggraph::NodeId> m_placed;

	// Demande de cadrage en attente, posee par l'hote et retiree par la
	// premiere frame qui a soumis au moins un noeud -- voir DrawGraph pour la
	// raison de ce report.
	bool m_fitPending = false;

	// Ou deposer le prochain noeud cree depuis la palette. L'abscisse est un
	// DECALAGE a partir du bord droit de la palette, non une position absolue :
	// la palette flotte au-dessus du graphe, et un noeud depose sous elle
	// naitrait invisible.
	float m_dropX = 30.0f;
	float m_dropY = 40.0f;

	std::string m_message;
};

} // namespace cggraph_canvas
