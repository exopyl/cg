#pragma once

#include <cstddef>
#include <exception>
#include <functional>

// Execution parallele des etapes independantes de la fusion des barres
// (bar_fusion.cpp). En-tete interne a cgmesh : seuls la fusion, ses tests et
// ses instruments l'incluent. La sortie de la fusion ne depend pas du nombre
// de fils, a condition que le fil appelant soit dans l'environnement flottant
// par defaut (arrondi au plus proche, sans mise a zero des denormaux) : les
// fils auxiliaires n'heritent pas partout de celui de l'appelant (sous
// Windows, MXCSR est propre a chaque fil et un nouveau fil part de la valeur
// par defaut ; pthread_create, lui, copie l'environnement de l'appelant).

// Plafond du nombre de fils d'un appel, fil appelant compris.
constexpr unsigned int kBarFusionMaxThreads = 8;

// Nombre de fils d'un appel : la limite forcee si elle est posee, sinon
// std::thread::hardware_concurrency () (kBarFusionMaxThreads s'il est
// inconnu), bornee a [1, kBarFusionMaxThreads]. Toujours 1 sous
// __EMSCRIPTEN__.
unsigned int barFusionThreadLimit ();

// Force la limite rendue par barFusionThreadLimit () ; 0 rend la valeur par
// defaut. Rend la limite forcee precedente (0 si aucune). Reserve aux tests et
// aux instruments, qui comparent les sorties obtenues sous differents nombres
// de fils.
unsigned int setBarFusionThreadLimit (unsigned int threads);

// task (i) pour chaque i de [0, n) ; les taches sont independantes. Les fils
// auxiliaires sont pris dans un budget commun au processus, de
// barFusionThreadLimit () - 1 fils au total : des appels simultanes se le
// partagent, et un appel qui n'en obtient aucun s'execute sur le seul fil
// appelant. Si des taches levent, l'exception relancee, apres la jointure de
// tous les fils, est celle du plus petit indice : celle qu'une boucle
// sequentielle aurait rencontree la premiere.
void barFusionParallelFor (std::size_t n, const std::function<void (std::size_t)> &task);

// Resultat d'une etape calculee d'avance, ou l'exception qu'elle a levee.
// run () ne laisse rien echapper ; get () relance l'exception, au point ou le
// calcul sequentiel l'aurait rencontree. Une etape jamais lue ne leve pas.
template <class T>
struct BarFusionDeferred
{
	T                  value {};
	std::exception_ptr error;

	template <class F> void run (F step)
	{
		try { value = step(); }
		catch (...) { error = std::current_exception(); }
	}

	T &get ()
	{
		if (error)
			std::rethrow_exception(error);
		return value;
	}
};
