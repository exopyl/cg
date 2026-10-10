#pragma once

#include <string>

namespace peles
{
namespace app
{

// Resultat de la resolution du repertoire d'assets.
//
// `error` non vide signifie ECHEC, et `directory` est alors sans valeur.
// L'appelant DOIT sortir en code non nul : ouvrir une fenetre sans document ni
// police produirait un cadre vide, qui est le mode de panne le plus probable du
// lot et le plus difficile a diagnostiquer.
struct AssetsResolution
{
	std::string directory;
	// Quelle regle a repondu : sert au diagnostic, et a rien d'autre.
	std::string origin;
	std::string error;
};

// Quatre niveaux, dans cet ordre (peles_implementation.md §1.10) :
//   1. l'argument --assets
//   2. la variable d'environnement PELES_ASSETS_DIR
//   3. <repertoire de l'executable>/assets
//   4. la macro compilee vers l'arbre source
//
// (3) AVANT (4) pour qu'une INSTALLATION trouve ses assets meme si l'arbre
// source existe encore sur la machine.
//
// ⚠ LES DEUX PREMIERS NIVEAUX NE RETOMBENT PAS SUR LES SUIVANTS. Un --assets
// errone qui glisserait vers l'arbre source rendrait l'application muette sur
// la faute et le critere d'acceptation infalsifiable : ce qu'on demande
// explicitement, on l'obtient ou on echoue.
AssetsResolution ResolveAssetsDir (const std::string& explicitDir);

// Repertoire de l'executable en cours, chaine vide s'il n'est pas determinable.
std::string ExecutableDirectory ();

} // namespace app
} // namespace peles
