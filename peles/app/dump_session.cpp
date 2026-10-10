// ---------------------------------------------------------------------------
//  LES VERDICTS DE SESSION : statut, remplacement, repli, memoire
// ---------------------------------------------------------------------------
//
// Ce que la BOUCLE D'IMAGES a produit, lu apres elle : la zone de statut, le
// contrat de destruction, le retrecissement du vecteur lie, la memoire sur
// selections repetees. Aucun de ces verdicts n'a de sens sur une image isolee.
//
// Les declarations sont dans app/dump_modes.h, commun aux six unites.

#include "dump_modes.h"

#include <cstddef>
#include <cstdio>

// ⚠ <windows.h> AVANT <psapi.h>, ET NOMINMAX EST POSE PAR LA CONFIGURATION.
// psapi.h ne tire pas ses propres types : sans windows.h devant lui, chacune de
// ses declarations rend une erreur de syntaxe qui pointe le SDK, pas la cause.
#ifdef _WIN32
	#include <windows.h>

	#include <psapi.h>
#endif

namespace peles
{
namespace app
{

bool ReportLoadStatus (const std::string& requested, bool loaded, const std::string& failureName,
                       const std::string& failureReason, const std::string& status)
{
	std::printf ("\n--- zone de statut ---\n");
	std::printf ("fichier demande          : %s\n", requested.empty () ? "(aucun)" : requested.c_str ());
	std::printf ("chargement               : %s\n", loaded ? "REUSSI" : "ECHOUE");
	std::printf ("statut a l'ecran         : \"%s\"\n", status.c_str ());

	if (requested.empty ())
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : aucun fichier demande, donc aucun chargement "
		              "dont le statut puisse rendre compte.\n");
		return false;
	}

	if (loaded)
	{
		if (!status.empty ())
		{
			std::fprintf (stderr,
			              "STATUT INATTENDU : le chargement a REUSSI et la zone de statut porte "
			              "\"%s\".\n",
			              status.c_str ());
			return false;
		}
		std::printf ("STATUT CONFORME : chargement reussi, zone de statut vide.\n");
		return true;
	}

	// LE NOM ET LA RAISON SONT CHERCHES SEPAREMENT : le verdict doit nommer
	// celle des deux qui manque, sans quoi l'operateur relit la chaine a l'oeil.
	const bool hasName   = !failureName.empty () && status.find (failureName) != std::string::npos;
	const bool hasReason =
	    !failureReason.empty () && status.find (failureReason) != std::string::npos;

	std::printf ("nom attendu              : \"%s\" -> %s\n", failureName.c_str (),
	             hasName ? "present" : "ABSENT");
	std::printf ("raison attendue          : \"%s\" -> %s\n", failureReason.c_str (),
	             hasReason ? "presente" : "ABSENTE");

	if (!hasName || !hasReason)
	{
		std::fprintf (stderr,
		              "STATUT INCOMPLET : un fichier illisible doit nommer le fichier ET la "
		              "raison. %s\n",
		              !hasName ? (!hasReason ? "Les deux manquent." : "Le nom manque.")
		                       : "La raison manque.");
		return false;
	}

	std::printf ("STATUT CONFORME : le fichier et la raison sont nommes.\n");
	return true;
}

bool ReportSecondFile (bool opened, const std::string& fileLine, int modelCount,
                       const std::string& expectedSecond, const std::string& firstFile)
{
	std::printf ("\n--- second fichier ouvert ---\n");
	std::printf ("premier fichier          : %s\n", firstFile.empty () ? "(aucun)"
	                                                                   : firstFile.c_str ());
	std::printf ("second fichier           : %s\n", expectedSecond.c_str ());
	std::printf ("modeles dans la scene    : %d\n", modelCount);
	std::printf ("ligne \"File\" du panneau   : %s\n",
	             fileLine.empty () ? "(vide)" : fileLine.c_str ());

	if (!opened)
	{
		std::fprintf (stderr, "MESURE NON CONCLUANTE : le second fichier n'a pas ete ouvert.\n");
		return false;
	}

	if (modelCount < 2)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : la scene ne porte que %d modele(s). Avec un seul, "
		              "la selection ne peut pas designer le mauvais.\n",
		              modelCount);
		return false;
	}

	if (firstFile.empty () || firstFile == expectedSecond)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : les deux fichiers portent le meme nom, ou le "
		              "premier est absent. La comparaison serait vraie quelle que soit la "
		              "selection.\n");
		return false;
	}

	if (fileLine != expectedSecond)
	{
		std::fprintf (stderr,
		              "SELECTION FAUSSE : les panneaux decrivent \"%s\" alors que le dernier "
		              "fichier ouvert est \"%s\".\n",
		              fileLine.c_str (), expectedSecond.c_str ());
		return false;
	}

	std::printf ("SELECTION CONFORME : les panneaux decrivent le dernier fichier ouvert.\n");
	return true;
}

bool ReportSceneReplacement (bool opened, int registeredBefore, int registeredAfter)
{
	std::printf ("\n--- remplacement de la scene ---\n");
	std::printf ("fichier de remplacement     : %s\n", opened ? "ouvert" : "NON OUVERT");
	std::printf ("maillages enregistres avant : %d\n", registeredBefore);
	std::printf ("maillages enregistres apres : %d\n", registeredAfter);

	// ⚠ TESTE EN PREMIER. « avant = 1, apres = 0 » est aussi ce que produit un
	// chargement qui a echoue apres avoir vide la scene : lire les compteurs sans
	// savoir si le fichier a ete ouvert revient a conclure d'une trace ambigue.
	if (!opened)
	{
		std::printf ("REMPLACEMENT NON CONCLUANT : le fichier de remplacement n'a pas pu etre "
		             "ouvert. La scene a bien ete videe, mais rien n'a ete recharge -- les "
		             "compteurs ne prouvent donc rien.\n");
		return false;
	}

	if (registeredBefore == 0)
	{
		std::printf ("REMPLACEMENT NON CONCLUANT : aucun maillage n'etait enregistre aupres du "
		             "moteur, la destruction n'avait donc aucune cle a rendre.\n");
		return false;
	}
	if (registeredAfter != 0)
	{
		std::printf ("CONTRAT DE DESTRUCTION ROMPU : %d cle(s) pendante(s) apres le vidage de la "
		             "scene.\n",
		             registeredAfter);
		return false;
	}

	std::printf ("CONTRAT DE DESTRUCTION TENU : les %d cle(s) ont ete rendues au moteur.\n",
	             registeredBefore);
	return true;
}

bool ReportTreeChurn (int requested, int toggles, int minRows, int maxRows)
{
	std::printf ("\n--- repli et depli repetes de l'arbre ---\n");
	std::printf ("basculements demandes : %d\n", requested);
	std::printf ("basculements effectues: %d\n", toggles);
	std::printf ("lignes, minimum       : %d\n", minRows);
	std::printf ("lignes, maximum       : %d\n", maxRows);

	// ⚠ LE COMPTE DEMANDE EST COMPARE AU COMPTE EFFECTUE, et cette comparaison
	// vient EN PREMIER : un basculement par image RENDUE, donc `--frames 1
	// --churn-tree 100` n'en fait qu'un. Sans elle, le verdict etait vert sur une
	// execution qui n'avait pas fait ce qui etait demande.
	if (toggles < requested)
	{
		std::printf ("REPLI NON CONCLUANT : %d demandes, %d effectues -- un basculement par image "
		             "RENDUE, il en faut donc au moins autant que de basculements.\n",
		             requested, toggles);
		return false;
	}

	if (toggles == 0 || maxRows == minRows)
	{
		std::printf ("REPLI NON CONCLUANT : le vecteur lie n'a pas change de taille, il n'a donc "
		             "rien ete exerce.\n");
		return false;
	}

	std::printf ("REPLI OK : le vecteur lie a ete retreci puis regrandi %d fois, de %d a %d "
	             "lignes.\n",
	             toggles, minRows, maxRows);
	return true;
}

// ---------------------------------------------------------------------------
//  --reselect
// ---------------------------------------------------------------------------

bool ReadProcessFootprintKb (long long& kilobytes)
{
#ifdef _WIN32
	// ⚠ LA MEMOIRE PRIVEE VALIDEE, PAS L'ENSEMBLE DE TRAVAIL. L'ensemble de
	// travail suit la pression memoire du systeme : il monte et descend sans
	// qu'une seule allocation ait ete faite ou rendue, et il ferait du verdict
	// une loterie.
	PROCESS_MEMORY_COUNTERS_EX counters {};
	counters.cb = sizeof (counters);
	if (!GetProcessMemoryInfo (GetCurrentProcess (), (PROCESS_MEMORY_COUNTERS*)&counters,
	                           sizeof (counters)))
		return false;
	kilobytes = (long long)(counters.PrivateUsage / 1024);
	return true;
#else
	// ⚠ VmData, ET NON LE RSS. La branche Windows ci-dessus ecarte l'ensemble de
	// travail parce qu'il suit la pression memoire du systeme ; le second champ
	// de /proc/self/statm -- ce que cette branche lisait -- EST le RSS, c'est-a-
	// dire exactement la grandeur que l'autre branche rejette. Les deux mesures
	// tombaient sous une tolerance unique, calibree sur Windows seulement.
	// VmData est le pendant Linux de la memoire privee validee : la somme des
	// projections privees inscriptibles -- donnees, tas, piles -- que le noyau a
	// engagees, independamment des pages effectivement residentes.
	std::FILE* file = std::fopen ("/proc/self/status", "r");
	if (file == nullptr)
		return false;

	char      line[256];
	bool      found = false;
	long long value = 0;
	while (std::fgets (line, (int)sizeof (line), file) != nullptr)
	{
		if (std::sscanf (line, "VmData: %lld kB", &value) == 1)
		{
			found = true;
			break;
		}
	}
	std::fclose (file);

	if (!found)
		return false;
	kilobytes = value;
	return true;
#endif
}

const char* ProcessFootprintName ()
{
#ifdef _WIN32
	return "memoire privee validee, PrivateUsage";
#else
	return "memoire privee validee, VmData";
#endif
}

bool ReportSelectionMemory (int requested, int performed, int materials, bool truncated,
                            const std::vector<SelectionSample>& samples)
{
	std::printf ("--- accumulation sur selections successives ---\n");
	std::printf ("selections demandees : %d\n", requested);
	std::printf ("selections effectuees : %d\n", performed);
	std::printf ("materiaux : %d\n", materials);
	std::printf ("arbre des materiaux : %s\n", truncated ? "TRONQUE" : "complet");

	for (std::size_t i = 0; i < samples.size (); ++i)
		std::printf ("selection %d : cache %d entree(s), moteur %d maillage(s), "
		             "lignes liees %d/%d/%d (proprietes/maillages/materiaux), "
		             "empreinte %lld Kio\n",
		             (int)i + 1, samples[i].cacheEntries, samples[i].registeredMeshes,
		             samples[i].infoRows, samples[i].meshRows, samples[i].materialRows,
		             samples[i].footprintKb);

	if (performed != requested)
	{
		std::printf ("SELECTIONS NON CONCLUANTES : %d demandees, %d effectuees.\n", requested,
		             performed);
		return false;
	}

	if (samples.size () < 2)
	{
		// ⚠ MOINS DE DEUX RELEVES NE PERMET AUCUNE COMPARAISON. Le dire, plutot
		// que de rendre un verdict bon sur une mesure qui n'existe pas -- c'est
		// la meme distinction que le SKIP contre le PASS.
		std::printf ("ACCUMULATION NON CONCLUANTE : %d releve(s), il en faut au moins deux.\n",
		             (int)samples.size ());
		return false;
	}

	if (materials == 0 || truncated)
	{
		std::printf ("ACCUMULATION NON CONCLUANTE : l'arbre des materiaux est %s.\n",
		             materials == 0 ? "vide" : "tronque par le plafond");
		return false;
	}

	// ---------------------------------------------------------------------
	//  LE VERDICT PORTE D'ABORD SUR CINQ ENTIERS EXACTS, SANS TOLERANCE
	// ---------------------------------------------------------------------
	//
	// ⚠ LES ENTIERS EXACTS SONT LUS AVANT L'EMPREINTE, ET CET ORDRE EST LE BON.
	// Une grandeur exacte tranche ; une empreinte de processus ne fait que
	// tendre. Dans l'ordre inverse, une accumulation reelle et VISIBLE dans les
	// compteurs sortait sous « l'accumulation est AILLEURS » -- un diagnostic
	// faux, alors que l'instrument avait le bon sous la main.
	//
	// ⚠ LES DEUX PREMIERS SONT INDEXES PAR MAILLAGE, LES TROIS AUTRES PAR LIGNE
	// LIEE. La distinction n'est pas de la precision : sur un fichier a 60
	// materiaux et UN maillage -- la forme exacte du cas que le critere nomme --
	// les deux premiers valent « 1, 1 » quel que soit le nombre de materiaux.
	// Sans les trois suivants, le nombre « 60 » n'entre dans aucune grandeur que
	// ce verdict lit, et une accumulation de la taille de ce qu'elle remplace --
	// un InfoRow par ligne -- passe sous la tolerance a gros grain de
	// l'empreinte sans rien faire bouger d'autre.
	struct ExactCounter
	{
		const char* name;
		int SelectionSample::*field;
	};
	static const ExactCounter kExactCounters[] = {
		{ "CACHE", &SelectionSample::cacheEntries },
		{ "MOTEUR", &SelectionSample::registeredMeshes },
		{ "LIGNES DE PROPRIETES", &SelectionSample::infoRows },
		{ "LIGNES DE MAILLAGES", &SelectionSample::meshRows },
		{ "LIGNES DE MATERIAUX", &SelectionSample::materialRows },
	};

	bool stable = true;
	for (std::size_t i = 1; i < samples.size () && stable; ++i)
	{
		for (const ExactCounter& counter : kExactCounters)
		{
			const int first   = samples.front ().*(counter.field);
			const int current = samples[i].*(counter.field);
			if (first == current)
				continue;

			std::printf ("%s EN CROISSANCE : %d a la selection 1, %d a la selection %d.\n",
			             counter.name, first, current, (int)i + 1);
			stable = false;
			break;
		}
	}

	if (!stable)
		return false;

	// ---------------------------------------------------------------------
	//  L'EMPREINTE DU PROCESSUS : UN GARDE-FOU A GROS GRAIN, ET RIEN DE PLUS
	// ---------------------------------------------------------------------
	//
	// ⚠ CE QUE CETTE TOLERANCE VAUT, ET CE QU'ELLE NE VAUT PAS. Elle existe
	// parce qu'une allocation qui fuit SANS passer par l'un des cinq entiers
	// ci-dessus ne serait vue par rien d'autre : ceux-la ne voient que ce qui
	// est indexe par maillage ou par ligne liee.
	//
	// ⚠ ELLE EST GROSSIERE, ET IL FAUT LE SAVOIR AVANT DE S'Y FIER. MESURE sur
	// la machine de reference, par cet instrument meme : la croissance
	// d'amorcage vaut 70 a 200 Kio par selection, elle ne depend PAS du nombre
	// de materiaux -- un fichier a 3 materiaux croit AUTANT qu'un fichier a 60
	// -- et la meme ligne de commande rend du simple au double d'une execution
	// a l'autre. Une fuite franche, elle, se mesure en mebioctets : la fuite
	// volontaire de 8 Mio par selection a rendu +17040 Kio la ou le bruit rend
	// +192 a +560. La tolerance est placee entre les deux, ce qui la rend
	// AVEUGLE a une fuite de quelques dizaines de kibioctets par selection.
	//
	// ⚠ ELLE EST PROPORTIONNELLE AU NOMBRE D'INTERVALLES, pas fixe : une valeur
	// fixe deviendrait de plus en plus severe a mesure que --reselect grandit,
	// donc l'instrument se contredirait selon son propre argument.
	const long long kFootprintToleranceKbPerSelection = 512;

	const long long footprintGrowth = samples.back ().footprintKb - samples.front ().footprintKb;
	const long long footprintBudget =
	    kFootprintToleranceKbPerSelection * (long long)(samples.size () - 1);
	std::printf ("empreinte du processus (%s), selection 1 a %d : %+lld Kio (tolerance %lld Kio, "
	             "GROS GRAIN)\n",
	             ProcessFootprintName (), (int)samples.size (), footprintGrowth, footprintBudget);

	if (footprintGrowth > footprintBudget)
	{
		// ⚠ « AILLEURS » EST EXACT ICI, ET IL NE L'ETAIT PAS DANS L'ORDRE INVERSE :
		// les cinq entiers exacts viennent d'etre compares et sont stables, donc
		// ce qui a cru ne passe par aucun d'eux.
		std::printf ("EMPREINTE EN CROISSANCE : %lld Kio au-dela de la tolerance, sans qu'aucun "
		             "des cinq entiers exacts n'ait bouge -- l'accumulation est AILLEURS.\n",
		             footprintGrowth - footprintBudget);
		return false;
	}

	std::printf ("AUCUNE ACCUMULATION INDEXEE PAR MAILLAGE NI PAR LIGNE LIEE : %d entree(s) de "
	             "cache, %d maillage(s) enregistre(s) et %d/%d/%d ligne(s) liees a chaque "
	             "selection.\n",
	             samples.front ().cacheEntries, samples.front ().registeredMeshes,
	             samples.front ().infoRows, samples.front ().meshRows,
	             samples.front ().materialRows);

	// ⚠ CE VERDICT NE DIT RIEN DE L'EMPREINTE, ET LE TAIRE LE FERAIT LIRE COMME
	// S'IL LE DISAIT. La croissance de l'empreinte est REELLE -- de l'ordre de
	// 100 Kio par selection sur la machine de reference -- et la tolerance a
	// ete placee AU-DESSUS d'elle : elle est toleree, pas niee. Un ECART NON
	// RESOLU, dont la cause n'est pas etablie.
	if (footprintGrowth > 0)
		std::printf ("ECART NON RESOLU : l'empreinte a cru de %+lld Kio sur %d selection(s) "
		             "sans qu'aucun des cinq entiers ne bouge. Cette croissance est TOLEREE par "
		             "le garde-fou a gros grain, elle n'est pas niee.\n",
		             footprintGrowth, (int)samples.size ());
	return true;
}

} // namespace app
} // namespace peles
