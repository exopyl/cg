#pragma once
//
//  Contexte d'evaluation -- annulation et progression.
//
// Le contexte traverse Compute sans qu'un noeud ait a le comprendre : il ne
// porte que ce qu'un calcul long doit pouvoir consulter. Le drapeau
// d'annulation reste la propriete de l'appelant et n'est pas detenu ici :
// plusieurs contextes peuvent partager le meme drapeau, et un calcul non
// annulable est le cas nominal, pas une erreur.
//
#include <atomic>
#include <functional>
#include <utility>

namespace cggraph
{

class EvalContext
{
public:
	// t est dans [0,1] ; label peut etre nul.
	using ProgressSink = std::function<void (float t, const char *label)>;

	EvalContext () = default;

	// Le drapeau doit survivre au contexte.
	void SetCancellationFlag (std::atomic<bool> *flag) { m_cancelled = flag; }

	// Le drapeau est expose parce que le moteur ne peut pas fabriquer lui-meme
	// le contexte que les algorithmes de domaine attendent : nommer ce type ici
	// ferait dependre le moteur d'une bibliotheque de domaine. La couche des
	// adaptateurs relaie le drapeau, elle ne le duplique pas.
	const std::atomic<bool> *GetCancellationFlag () const { return m_cancelled; }

	void SetProgressSink (ProgressSink sink) { m_progress = std::move (sink); }

	bool IsAborted () const
	{
		return m_cancelled != nullptr && m_cancelled->load (std::memory_order_relaxed);
	}

	void Progress (float t, const char *label) const
	{
		if (m_progress)
			m_progress (t, label);
	}

private:
	std::atomic<bool> *m_cancelled = nullptr;
	ProgressSink m_progress;
};

} // namespace cggraph
