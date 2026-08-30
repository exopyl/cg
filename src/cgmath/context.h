#pragma once
//
//  Contexte optionnel des algorithmes annulables.
//
// CONTRAT ABSTRAIT, et rien d'autre : il dit ce qu'un algorithme demande a son
// appelant -- « dois-je m'arreter ? » --, jamais par quel moyen la reponse est
// obtenue. Drapeau atomique, echeance, sondage du systeme : c'est l'implementation
// qui choisit, et c'est pourquoi ce fichier n'inclut rien.
//
// Il est declare ICI, a la base de la chaine, parce que le type qui traverse les
// signatures ne peut appartenir a aucune des couches qui le fabriquent : celles-ci
// sont toujours au-dessus. cgimg, cgmesh et cgre le passent donc sans rien savoir
// de qui l'implemente, et une nouvelle facon d'annuler s'ajoute en derivant, sans
// rouvrir une seule signature.
//
// Il se passe en DERNIER parametre, toujours optionnel : les appelants qui
// n'annulent rien compilent inchanges, et un algorithme annulable se distingue
// d'un algorithme qui ne l'est pas par sa seule signature.
//
// Un jeton porte par une variable thread_local a ete ecarte : parallelChunks
// (cgmesh, thickness.cpp) cree son propre pool de std::thread, ou chaque fil
// aurait le sien, vierge. L'algorithme le plus long a annuler est precisement
// celui ou cette voie n'annulerait jamais, en silence. Le contexte voyage donc
// par la signature, et franchit les frontieres de fils avec elle.
//
class Context
{
public:
	virtual ~Context () = default;

	// Interroge depuis les boucles EXTERNES des algorithmes, jamais depuis une
	// boucle chaude : l'appel est virtuel, et son cout n'est nul que la ou il
	// est espace.
	virtual bool IsAborted () const = 0;
};
