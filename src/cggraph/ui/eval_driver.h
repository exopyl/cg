#pragma once
//
//  Comment le modele d'edition fait calculer -- et ou l'hote entre en jeu.
//
// EditorModel ne sait pas s'il tourne au-dessus d'un fil de calcul ou d'un hote
// mono-fil : il pose des demandes, il retire des resultats. Ce fichier nomme ce
// contrat et en donne les deux realisations.
//
// POURQUOI UN TYPE PLUTOT QU'UN POINTEUR NULLABLE. La forme evidente serait un
// std::unique_ptr<AsyncEvaluator> laisse vide sous Emscripten, avec un repli
// synchrone ecrit dans chaque methode d'EditorModel. Elle a deux defauts que
// celle-ci n'a pas :
//
//  * elle fait de « pas de fil » un ETAT verifie a chacun des six appels, et un
//    etat peut etre oublie a un site sur six. Ici c'est un TYPE, choisi une
//    fois a la construction : le compilateur ne laisse pas la moitie du repli
//    manquer ;
//  * elle rend le chemin synchrone INATTEIGNABLE depuis la suite de tests, qui
//    ne tourne pas sous Emscripten. Un repli qu'aucune CI n'execute ne protege
//    rien. Ici, le pilote en ligne s'instancie NATIVEMENT et se teste comme
//    n'importe quel objet.
//
// Le vocabulaire reste celui d'AsyncEvaluator -- Progress, Completed -- parce
// que c'est le type que l'API publique d'EditorModel rend deja, et qu'un alias
// neuf obligerait les appelants natifs a changer sans rien leur apporter.
//
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "../core/async_evaluator.h"
#include "../core/eval_context.h"
#include "../core/evaluator.h"
#include "../core/graph.h"

namespace cggraph_ui
{

class EvalDriver
{
public:
	using Progress = cggraph::AsyncEvaluator::Progress;
	using Completed = cggraph::AsyncEvaluator::Completed;

	virtual ~EvalDriver () = default;

	EvalDriver () = default;
	EvalDriver (const EvalDriver &) = delete;
	EvalDriver &operator= (const EvalDriver &) = delete;

	// Demande coalescee : elle remplace celle qui attend et repousse l'echeance.
	virtual void Request (cggraph::NodeId id) = 0;

	// Sans fenetre -- le « calculer maintenant » d'un bouton.
	virtual void RequestNow (cggraph::NodeId id) = 0;

	virtual void Cancel () = 0;

	// Vrai pendant qu'un calcul tourne.
	virtual bool IsBusy () const = 0;

	// Ni calcul en cours, ni demande en attente.
	virtual bool IsIdle () const = 0;

	// ⚠ MEME PIEGE QU'AsyncEvaluator::WaitIdle : une demande dont la fenetre
	// n'est pas ecoulee compte comme en attente. Sous horloge figee, attendre
	// l'inactivite sans avoir avance l'horloge n'aboutit jamais.
	virtual void WaitIdle () = 0;

	// ⚠ A APPELER PAR L'HOTE, HORS DE TOUTE FRAME COMMENCEE. C'est ici, et
	// nulle part ailleurs, qu'un hote mono-fil calcule. Sur un pilote a fil,
	// c'est un no-op : le fil n'a besoin de personne pour avancer.
	//
	// Hors frame, parce que le collecteur de progression d'un hote mono-fil est
	// appele DEPUIS ce calcul : s'il en profite pour redessiner, il ne doit pas
	// se retrouver au milieu d'une frame que l'appelant avait commencee.
	virtual void Pump () = 0;

	virtual std::vector<Completed> Drain () = 0;

	virtual Progress GetProgress () const = 0;

	// Combien d'evaluations ont REELLEMENT ete lancees. Instrument de la
	// coalescence, comme celui d'AsyncEvaluator.
	virtual unsigned int GetStartedCount () const = 0;

	// ⚠ REFUS NOMME, et il est la raison d'etre du bool. Un pilote a fil rend
	// false : son collecteur serait appele DEPUIS le fil de calcul, ce que le
	// §7.3 de la conception interdit -- l'hote releve la progression, il ne se
	// la fait pas pousser. Un pilote en ligne rend true : le calcul est sur le
	// fil de l'hote, il n'y a pas d'autre fil pour relever, et c'est la que le
	// pompage de frame devient possible.
	//
	// Rendre void aurait fait du refus un silence, donc un mecanisme sans filet.
	virtual bool SetProgressSink (cggraph::EvalContext::ProgressSink sink) = 0;
};

// Le pilote a fil : AsyncEvaluator, tel quel. ⚠ Son constructeur LEVE sur un
// hote sans fils (Emscripten sans -pthread).
class ThreadedEvalDriver : public EvalDriver
{
public:
	explicit ThreadedEvalDriver (cggraph::Graph &graph);

	void Request (cggraph::NodeId id) override;
	void RequestNow (cggraph::NodeId id) override;
	void Cancel () override;
	bool IsBusy () const override;
	bool IsIdle () const override;
	void WaitIdle () override;
	void Pump () override;
	std::vector<Completed> Drain () override;
	Progress GetProgress () const override;
	unsigned int GetStartedCount () const override;
	bool SetProgressSink (cggraph::EvalContext::ProgressSink sink) override;

	cggraph::AsyncEvaluator &GetAsync () { return m_async; }

private:
	cggraph::AsyncEvaluator m_async;
};

// Le pilote en ligne : le calcul a lieu dans Pump (), sur le fil de l'appelant.
// C'est l'hote WebAssembly, et c'est aussi ce qu'on peut eprouver nativement.
class InlineEvalDriver : public EvalDriver
{
public:
	using TimePoint = cggraph::AsyncEvaluator::TimePoint;
	using Clock = cggraph::AsyncEvaluator::Clock;

	explicit InlineEvalDriver (cggraph::Graph &graph);

	// Memes reglages que le pilote a fil, meme defaut de 150 ms : la
	// coalescence n'a rien a voir avec le nombre de fils, elle a a voir avec un
	// curseur qu'on deplace.
	void SetDebounce (std::chrono::milliseconds window) { m_window = window; }
	std::chrono::milliseconds GetDebounce () const { return m_window; }

	// Nulle remet l'horloge reelle.
	void SetClock (Clock clock);

	void Request (cggraph::NodeId id) override;
	void RequestNow (cggraph::NodeId id) override;
	void Cancel () override;
	bool IsBusy () const override;
	bool IsIdle () const override;
	void WaitIdle () override;
	void Pump () override;
	std::vector<Completed> Drain () override;
	Progress GetProgress () const override;
	unsigned int GetStartedCount () const override;
	bool SetProgressSink (cggraph::EvalContext::ProgressSink sink) override;

	cggraph::Evaluator &GetEvaluator () { return m_evaluator; }

private:
	cggraph::Evaluator m_evaluator;
	cggraph::EvalContext m_context;
	std::atomic<bool> m_cancel{ false };

	Clock m_clock;
	std::chrono::milliseconds m_window{ cggraph::AsyncEvaluator::kDefaultDebounceMs };

	cggraph::NodeId m_pending = cggraph::kInvalidNodeId;
	bool m_hasPending = false;
	TimePoint m_deadline;

	// ⚠ Garde de REENTRANCE. Le collecteur de progression est appele depuis
	// Evaluate ; s'il redessine, et qu'un redessin rappelait Pump, on relancerait
	// une evaluation par-dessus celle qui tourne. L'evaluateur la refuserait par
	// EvalStatus::Busy, mais le refus serait rendu a l'appelant EXTERNE. Cette
	// garde fait que Pump reentrant ne fait rien du tout.
	bool m_busy = false;

	unsigned int m_started = 0;

	Progress m_progress;
	cggraph::EvalContext::ProgressSink m_hostSink;
	std::vector<Completed> m_done;
};

enum class EvalMode
{
	// Un fil si l'hote en a, le calcul en ligne sinon. C'est le seul mode qui
	// n'a pas a etre revu quand on change d'hote.
	Auto,
	Threaded,
	Inline
};

// Vrai si l'hote sait demarrer un fil. ⚠ Ce n'est pas une mesure : c'est la
// configuration de compilation qui repond. Sous Emscripten sans -pthread, la
// reponse est non, et le constat est dans async_evaluator.h.
bool HostHasThreads ();

std::unique_ptr<EvalDriver> MakeEvalDriver (cggraph::Graph &graph, EvalMode mode = EvalMode::Auto);

} // namespace cggraph_ui
