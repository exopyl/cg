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
#include <functional>
#include <map>
#include <memory>
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

	// DEMANDE DE SELECTION DE FICHIER, posee par le bouton « Parcourir… » de
	// l'inspecteur et retiree par l'hote.
	//
	// Le canvas ne PEUT PAS ouvrir de selecteur : il est dessine par ImGui, dans
	// un Worker, sans acces au DOM. Il ne fait donc que DEMANDER, et l'hote --
	// qui, lui, a un <input type=file> -- s'en charge puis repose le chemin par
	// l'API ordinaire des parametres. C'est la meme separation que pour le
	// pompage de frame : le canvas dit ce qu'il veut, l'hote sait comment.
	//
	// `node` vaut 0 quand rien n'est demande. La lecture EFFACE la demande : un
	// hote qui la lirait deux fois ouvrirait deux selecteurs.
	// `param` nomme le PARAMETRE a renseigner, ou reste VIDE quand le noeud
	// prend une ressource par octets et n'a aucun chemin a ecrire -- l'hote
	// verse alors les octets au lieu de deposer un fichier.
	struct FileRequest
	{
		cggraph::NodeId node = 0;
		std::string param;
	};
	FileRequest TakeFileRequest ();

	// SONDE « CE NOEUD PREND-IL DES OCTETS ? », fournie par l'hote.
	//
	// Le canvas ne peut pas y repondre seul : ByteSource est une interface de la
	// couche des noeuds, et l'y faire descendre echangerait une convention sur
	// un nom de parametre contre une dependance de couche. L'hote, lui, sait
	// deja -- c'est un dynamic_cast qu'il fait par ailleurs. Meme separation que
	// TakeFileRequest et le televerseur de vignette : le canvas dit ce qu'il
	// veut, l'hote sait comment.
	//
	// Sans sonde, seuls les noeuds portant un parametre `path` proposent de
	// charger une ressource -- le comportement d'avant.
	using ByteSourceProbe = std::function<bool (cggraph::NodeId)>;
	void SetByteSourceProbe (ByteSourceProbe probe);

	// SEPARATEUR : la part de la largeur qui revient a l'editeur, le reste
	// allant a la vue 3D de l'hote. 1 rend la disposition d'avant -- l'editeur
	// occupe tout, la vue est derriere lui.
	//
	// La valeur vit chez l'HOTE et descend ici, et non l'inverse : c'est lui qui
	// pose le viewport de la scene, et c'est sa page qui porte la poignee de
	// glissement. Deux detenteurs auraient fini par se decaler d'un pixel.
	void SetSplit (float fraction);
	float GetSplit () const { return m_split; }

	// TELEVERSEUR DE VIGNETTE, fourni par l'hote.
	//
	// Le canvas ne connait AUCUNE API graphique -- il dessine par ImGui, et
	// ImGui ne sait pas creer de texture. L'hote, lui, en a une : il transforme
	// des octets RGBA en identifiant de texture ImGui, et le canvas s'en sert
	// sans savoir comment. Meme separation que TakeFileRequest et que le pompage
	// de frame : le canvas dit ce qu'il veut, l'hote sait comment.
	//
	// `reuse` est l'identifiant rendu au coup precedent, ou nullptr : l'hote peut
	// reecrire la meme texture au lieu d'en creer une seconde. Rendre nullptr
	// signifie « pas de vignette », et le canvas n'en dessine simplement aucune
	// -- c'est le cas de tout hote qui n'installe pas de televerseur.
	//
	// `void *` et non ImTextureID pour garder imgui.h hors de cet en-tete ; les
	// deux sont le meme type.
	using TextureUploader =
		std::function<void *(const unsigned char *rgba, int width, int height, void *reuse)>;
	void SetTextureUploader (TextureUploader uploader);

	// DESCENTE DANS UN SOUS-GRAPHE. Un double-clic sur un noeud qui delegue un
	// document -- flow.subgraph, flow.foreach, flow.repeat -- fait afficher CE
	// document a la place du graphe courant, et un fil d'Ariane remonte.
	//
	// LA PILE VIT ICI, pas chez l'hote, et c'est un choix : la facade
	// procedurale de l'hote web (graphAddNode, graphEvaluate...) doit continuer
	// de designer le document RACINE meme quand l'oeil est descendu. Descendre
	// est un geste de LECTURE ; deplacer sous les pieds de l'hote ce que
	// « le document » veut dire ferait evaluer le corps au lieu du tout.
	//
	// ⚠ CE QUI EST AFFICHE EN DESCENTE N'EST PAS CALCULABLE, et le canvas le
	// dit plutot que de laisser essayer : les `flow.in` du corps n'ont rien de
	// lie hors de leur hote, donc toute evaluation echouerait par « rien de lie »
	// -- un refus juste, mais qui se lirait comme une panne.
	std::size_t GetDepth () const { return m_descent.size (); }

	// Reference du document affiche, vide a la racine. L'hote s'en sert pour
	// dire ce qu'on regarde : ses propres compteurs, eux, restent ceux du
	// document racine, et sans cette mention les deux se contrediraient a
	// l'ecran.
	const std::string &GetCurrentReference () const;

private:
	// Un etage de la descente : le document delegue et d'ou l'on vient.
	struct Level
	{
		std::string reference;
		std::string label;
		std::unique_ptr<cggraph_ui::EditorModel> model;
	};

	// Le modele effectivement dessine : celui de l'hote a la racine, celui du
	// sommet de la pile sinon.
	cggraph_ui::EditorModel &Current (cggraph_ui::EditorModel &root);

	// Change d'etage : la bookkeeping des positions est REMISE A ZERO, sans quoi
	// l'editeur de noeuds appliquerait au document arrivant les positions qu'il
	// garde pour les memes identifiants du document quitte -- deux documents
	// numerotent leurs noeuds a partir de 1, donc ils entrent tous en collision.
	void SwitchLevel ();

	void DrawBreadcrumb (cggraph_ui::EditorModel &root);

	// Les gestes de navigation sont ENREGISTRES puis appliques au debut de la
	// frame suivante, jamais servis sur place : le double-clic est lu au milieu
	// d'une frame de l'editeur de noeuds, et changer de modele a cet instant
	// laisserait la palette et l'inspecteur de la meme frame travailler sur un
	// graphe qui n'est plus celui que le graphe vient de dessiner.
	void ApplyPendingNavigation ();

	std::vector<Level> m_descent;
	std::string m_descentError;
	ByteSourceProbe m_byteProbe;
	float m_split = 1.0f;

	std::string m_pendingDescent;
	std::string m_pendingDescentText;
	std::string m_pendingDescentLabel;
	bool m_pendingAscend = false;

	void DrawPalette (cggraph_ui::EditorModel &model);
	void DrawGraph (cggraph_ui::EditorModel &model);
	void DrawInspector (cggraph_ui::EditorModel &model);
	void RefreshThumbnails (cggraph_ui::EditorModel &model);

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

	// Demande de selection de fichier en attente. Cf. TakeFileRequest.
	FileRequest m_fileRequest;

	// UNE TEXTURE PAR NOEUD. Le moteur rend les vignettes de toute la branche
	// traversee, pas seulement du noeud terminal ; le canvas les televerse et
	// garde chaque texture pour la REECRIRE au calcul suivant plutot que d'en
	// creer une seconde.
	struct Thumb
	{
		void *texture = nullptr;
		int width = 0;
		int height = 0;
	};
	TextureUploader m_uploader;
	std::map<cggraph::NodeId, Thumb> m_thumbs;
	unsigned int m_thumbRevision = 0;

	// Ou deposer le prochain noeud cree depuis la palette. L'abscisse est un
	// DECALAGE a partir du bord droit de la palette, non une position absolue :
	// la palette flotte au-dessus du graphe, et un noeud depose sous elle
	// naitrait invisible.
	float m_dropX = 30.0f;
	float m_dropY = 40.0f;

	std::string m_message;
};

} // namespace cggraph_canvas
