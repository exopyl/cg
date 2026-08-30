#pragma once
//
//  Modele d'edition -- ce qu'un canvas manipule, sans rien dessiner.
//
// Tout ce qu'un editeur de graphe fait qui ne soit pas du dessin vit ici :
// instancier un type depuis le catalogue, cabler, selectionner, valider,
// calculer. Le canvas n'a plus qu'a rendre ce modele et a lui renvoyer les
// gestes de la souris ; c'est ce qui rend testable la partie de l'etape qui
// peut l'etre, une interface ne se testant pas.
//
// ⚠ FRONTIERE DU §7.3 : les sorties d'une evaluation sont detenues ici comme
// des Value, donc derriere shared_ptr<const> -- le modele ne peut pas ecrire
// dedans, et le canvas encore moins. Aucun interne d'un noeud n'est expose ; un
// canvas qui lirait dans les structures d'un noeud en cours de calcul serait
// une course, l'evaluation tournant desormais sur un fil a part.
//
// ⚠ ET LA MOITIE QUE LE MODELE NE PEUT PAS TENIR SEUL : l'inspecteur distribue
// des pointeurs DANS les parametres des noeuds, en ecriture. Pendant un calcul,
// ces parametres sont lus par le fil de calcul. C'est a l'hote de ne pas les
// laisser modifier tant que IsEvaluating () rend vrai -- le modele le dit, il ne
// peut pas l'empecher, les champs etant ecrits directement par le panneau.
//
// ⚠ CE MODELE EST PORTABLE, ET IL NE L'ETAIT PAS. Il DETENAIT un
// AsyncEvaluator par valeur, dont le constructeur leve sous Emscripten sans
// -pthread : une EditorModel n'y etait pas constructible, bien que la cible s'y
// compile et s'y lie. Il detient desormais un EvalDriver, dont le pilote a fil
// n'est qu'une des deux realisations -- voir eval_driver.h pour le motif de la
// forme. L'API publique ci-dessous n'a pas change ; Pump () s'y ajoute, et
// l'hote doit l'appeler.
//
#include <functional>
#include <memory>
#include <string>

#include "../core/async_evaluator.h"
#include "../core/eval_context.h"
#include "../core/graph.h"
#include "../core/validate.h"
#include "eval_driver.h"
#include "inspector.h"
#include "palette.h"

namespace cggraph_ui
{

class EditorModel
{
public:
	// Auto : un fil si l'hote en a, le calcul en ligne sinon. Le defaut ne
	// change RIEN pour l'hote natif -- il rend le meme pilote a fil qu'avant.
	explicit EditorModel (EvalMode mode = EvalMode::Auto);

	// Pour l'hote qui a besoin de GARDER la main sur son pilote -- l'hote web
	// regle le budget de cache de l'evaluateur et lit ses statistiques, ce que
	// le modele ne relaie pas et ne doit pas relayer : sur un pilote a fil, ces
	// deux gestes seraient des courses. Le graphe est passe a la fabrique parce
	// qu'il est un membre, donc construit avant le pilote.
	using DriverFactory = std::function<std::unique_ptr<EvalDriver> (cggraph::Graph &)>;
	explicit EditorModel (const DriverFactory &makeDriver);

	const Palette &GetPalette () const { return m_palette; }

	cggraph::Graph &GetGraph () { return m_graph; }
	const cggraph::Graph &GetGraph () const { return m_graph; }

	// Pas d'accesseur vers l'evaluateur : il appartient au fil de calcul, et le
	// lui emprunter depuis le fil d'interface serait exactement la course que
	// cette etape ferme. Ce qui se demande d'ici se demande par les quatre
	// methodes ci-dessous.

	// Instancie le type par la fabrique du catalogue, et pose sa position.
	// kInvalidNodeId si le type n'y figure pas -- une palette derivee du
	// catalogue ne peut pas proposer un type que la fabrique ignore, et ce
	// refus le dit si les deux divergeaient.
	cggraph::NodeId AddNode (const std::string &typeName, float x, float y);

	cggraph::ConnectStatus Connect (cggraph::NodeId from, cggraph::PortIdx fromPort,
	                                cggraph::NodeId to, cggraph::PortIdx toPort);
	bool Disconnect (cggraph::NodeId to, cggraph::PortIdx toPort);

	// Supprime le noeud et ses liens. Abandonne la selection si elle portait sur
	// lui : les champs de l'inspecteur pointent DANS ses parametres, et les
	// garder apres destruction ferait lire de la memoire liberee au rendu
	// suivant.
	cggraph::RemoveStatus RemoveNode (cggraph::NodeId id);

	// Selectionner reconstruit l'inspecteur : les champs pointent dans le noeud
	// selectionne, et deux panneaux vivants sur deux noeuds n'auraient pas de
	// sens dans un editeur a un seul inspecteur.
	void Select (cggraph::NodeId id);
	cggraph::NodeId GetSelection () const { return m_selection; }

	const Inspector &GetInspector () const { return m_inspector; }

	// A rappeler apres tout geste qui change le cablage ou les parametres du
	// noeud selectionne. Une relecture de document l'exige : Clear() sur le jeu
	// invalide toutes les adresses publiees.
	void RefreshInspector ();

	// Avant calcul, sans calculer -- c'est ce que le canvas interroge pour
	// griser un port ou un bouton.
	cggraph::BranchValidation Validate (cggraph::NodeId id) const;

	// Ne court-circuite PAS la validation : l'evaluateur refuse deja une entree
	// obligatoire libre, en la nommant. Deux refus pour le meme fait finiraient
	// par ne plus dire la meme chose.
	//
	// SYNCHRONE, et elle passe quand meme par le fil de calcul : elle demande
	// sans fenetre, attend, et retire le resultat. Un second chemin d'evaluation
	// qui contournerait le fil ferait deux caches sur un meme graphe.
	cggraph::EvalResult Evaluate (cggraph::NodeId id);

	// Demande COALESCEE -- c'est celle qu'un curseur appelle. Elle rend la main
	// aussitot ; le resultat se retire par Poll.
	void RequestEvaluation (cggraph::NodeId id);

	// A appeler a chaque frame. Vrai si un resultat est arrive, auquel cas
	// GetLastResult a change.
	bool Poll ();

	void CancelEvaluation ();

	// ⚠ A APPELER PAR L'HOTE, UNE FOIS PAR TOUR DE BOUCLE, HORS DE TOUTE FRAME
	// COMMENCEE. Sur un hote a fil c'est un no-op ; sur un hote mono-fil c'est
	// le SEUL endroit ou une demande devient un calcul. Un hote qui ne l'appelle
	// pas ne calcule jamais sur cette cible-la, et calcule normalement sur
	// l'autre : c'est la divergence que ce point unique existe pour eviter.
	void Pump ();

	// Collecteur de progression de l'HOTE -- le pompage de frame du §7.3.
	// ⚠ Rend FALSE sur un pilote a fil, et ce refus est le contrat : le
	// collecteur y serait appele depuis le fil de calcul. Voir eval_driver.h.
	bool SetProgressSink (cggraph::EvalContext::ProgressSink sink);

	// Vrai tant qu'un calcul tourne. L'hote grise ce qui ecrit dans un noeud.
	bool IsEvaluating () const;

	cggraph::AsyncEvaluator::Progress GetProgress () const;

	const cggraph::EvalResult &GetLastResult () const { return m_lastResult; }

	// Sorties de la derniere evaluation retiree par Poll. Ce sont des Value,
	// donc derriere shared_ptr<const> : la frontiere du §5 tient jusqu'ici, et
	// un hote qui affiche le resultat n'a pas besoin de plus. L'hote natif n'a
	// pas d'apercu, personne n'en avait donc eu besoin avant l'hote web.
	const cggraph::ValueList &GetLastOutputs () const { return m_outputs; }

	// Incremente a chaque fois que Poll retire un resultat. C'est ce qui dit a
	// un hote « il y a autre chose a televerser » : Poll est appele par le
	// canvas, l'hote ne voit donc pas son booleen passer.
	unsigned int GetResultRevision () const { return m_revision; }

private:
	cggraph::Graph m_graph;

	// Declare APRES le graphe : il le detient par reference, et le pilote a fil
	// demarre son fil a la construction.
	std::unique_ptr<EvalDriver> m_driver;
	Palette m_palette;
	Inspector m_inspector;
	cggraph::NodeId m_selection = cggraph::kInvalidNodeId;

	cggraph::ValueList m_outputs;
	cggraph::EvalResult m_lastResult;
	unsigned int m_revision = 0;
};

} // namespace cggraph_ui
