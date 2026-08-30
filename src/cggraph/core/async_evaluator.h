#pragma once
//
//  Evaluation sur un fil separe -- avec coalescence des demandes.
//
// Deux problemes distincts, un seul objet, et ils sont indissociables : un noeud
// long ne doit pas geler l'interface, et un curseur agite ne doit pas empiler
// des calculs morts. Le premier demande un fil ; le second demande que N
// demandes rapprochees n'en declenchent qu'UNE, et que celle qui tourne encore
// soit abandonnee des qu'une demande la remplace.
//
// UNE SEULE DEMANDE EN ATTENTE, jamais une file. Une file coalescerait mal :
// elle ferait calculer les etats intermediaires d'un curseur qu'on deplace,
// c'est-a-dire exactement ce que la fenetre existe pour eviter. La demande
// nouvelle remplace l'ancienne.
//
// FRONTIERE : le fil appelant ne touche jamais l'evaluateur, ni le graphe
// pendant un calcul. Il pose une demande, il retire des resultats -- des Value,
// donc derriere shared_ptr<const>. C'est la discipline du §7.3 de la conception,
// et c'est ce qui garde l'option (b) a portee d'un changement de drapeaux.
//
// ⚠ CET OBJET EST RESERVE AUX HOTES QUI ONT DES FILS. Sous Emscripten sans
// -pthread -- la cible WebAssembly du depot --, il compile et il lie, mais son
// constructeur LEVE : std::system_error, « thread constructor failed: Not
// supported ». Un hote mono-fil appelle Evaluator::Evaluate directement et pompe
// sa frame depuis le collecteur de progression ; c'est ce que fait le pont web.
// La sonde qui etablit ce comportement est conservee.
//
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "eval_context.h"
#include "evaluator.h"
#include "graph.h"

namespace cggraph
{

class AsyncEvaluator
{
public:
	using TimePoint = std::chrono::steady_clock::time_point;

	// Horloge INJECTABLE, et c'est ce qui rend la coalescence testable sans
	// dormir : une horloge figee fait qu'aucune fenetre ne s'ecoule toute seule,
	// donc qu'un test decide seul du moment ou elle expire. Avec une horloge
	// reelle, un test de coalescence mesure l'ordonnanceur autant que le code.
	using Clock = std::function<TimePoint ()>;

	struct Completed
	{
		NodeId node = kInvalidNodeId;
		EvalResult result;
		ValueList outputs;
	};

	// Derniere progression rendue par le calcul en cours. Elle est COPIEE :
	// l'appelant ne lit rien qui puisse bouger sous lui.
	struct Progress
	{
		NodeId node = kInvalidNodeId;
		float t = 0.0f;
		std::string label;
	};

	// 150 ms : au-dessus du temps d'un geste continu, en dessous de ce qu'un
	// operateur percoit comme un retard. C'est un parametre, pas une constante --
	// une cible ou l'evaluation est bon marche voudra une fenetre plus courte.
	static constexpr int kDefaultDebounceMs = 150;

	// Le graphe doit survivre a l'evaluateur asynchrone. Le fil demarre ici.
	explicit AsyncEvaluator (Graph &graph);

	// Leve le jeton d'annulation, puis joint le fil. ⚠ Il le DEMANDE, il ne
	// l'impose pas : un noeud qui ignore le jeton fait attendre la fermeture
	// jusqu'a la fin de son calcul. C'est la contrepartie de l'annulation
	// cooperative, et c'est la raison pour laquelle le critere 6.1 porte sur les
	// BOUCLES EXTERNES et pas seulement sur l'entree des algorithmes.
	~AsyncEvaluator ();

	AsyncEvaluator (const AsyncEvaluator &) = delete;
	AsyncEvaluator &operator= (const AsyncEvaluator &) = delete;

	void SetDebounce (std::chrono::milliseconds window);
	std::chrono::milliseconds GetDebounce () const;

	// Nulle remet l'horloge reelle.
	void SetClock (Clock clock);

	// Demande COALESCEE : elle remplace toute demande en attente et repousse
	// l'echeance d'une fenetre. Elle annule aussi le calcul en cours -- son
	// resultat ne serait plus celui qu'on demande.
	void Request (NodeId id);

	// Sans fenetre : l'echeance est immediate. C'est le « calculer maintenant »
	// d'un bouton, par opposition au reglage d'un curseur.
	void RequestNow (NodeId id);

	// Abandonne le calcul en cours et la demande en attente.
	void Cancel ();

	bool IsBusy () const;

	// Ni calcul en cours, ni demande en attente. ⚠ Une demande dont la fenetre
	// n'est pas ecoulee compte comme en attente : sous horloge figee, attendre
	// l'inactivite sans avoir avance l'horloge n'aboutit jamais.
	bool IsIdle () const;
	void WaitIdle () const;

	// Retire les resultats prets, dans l'ordre d'achevement. Les abandons sont
	// rendus eux aussi : un resultat qu'on jette est un diagnostic que personne
	// ne peut plus lire.
	std::vector<Completed> Drain ();

	Progress GetProgress () const;

	// Nombre d'evaluations REELLEMENT lancees depuis la construction. C'est
	// l'instrument de la coalescence : sans lui, « N demandes n'ont declenche
	// qu'un calcul » ne s'observe que par le temps que cela a pris.
	unsigned int GetStartedCount () const;

private:
	void Loop ();

	// Le graphe n'est pas repris ici : l'evaluateur le detient, et un second
	// chemin vers lui inviterait le fil appelant a le lire pendant un calcul.
	Evaluator m_evaluator;
	EvalContext m_context;

	// Le drapeau appartient a cet objet et survit au contexte, comme le contrat
	// l'exige. Il est remis a faux au demarrage de chaque calcul.
	std::atomic<bool> m_cancel{ false };

	mutable std::mutex m_mutex;
	mutable std::condition_variable m_wake;    // le fil de calcul attend ici
	mutable std::condition_variable m_settled; // les appelants attendent ici

	Clock m_clock;
	std::chrono::milliseconds m_window{ kDefaultDebounceMs };

	NodeId m_pending = kInvalidNodeId;
	bool m_hasPending = false;
	TimePoint m_deadline;

	bool m_busy = false;
	bool m_stop = false;
	unsigned int m_started = 0;

	Progress m_progress;
	std::vector<Completed> m_done;

	// Declare EN DERNIER : le fil demarre a la construction et lit tout ce qui
	// precede. L'ordre des membres est ici une garantie d'initialisation.
	std::thread m_worker;
};

} // namespace cggraph
