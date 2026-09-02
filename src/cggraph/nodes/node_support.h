#pragma once
//
//  Ce que tous les adaptateurs partagent, et rien de plus.
//
// Fichier de RACINE, comme value_types.h : mesh/ et text/ s'en servent tous les
// deux, et le loger dans l'un ferait de l'autre son client.
//
#include <atomic>
#include <string>
#include <vector>

#include "../core/eval_context.h"
#include "../core/param_set.h"
#include "../../cgmath/context.h"

namespace cggraph_nodes
{

// SEULE implementation du contrat d'annulation de cgmath, et elle appartient a
// la couche nodale : c'est ici que le moyen -- un drapeau atomique detenu par le
// moteur -- est connu. Les algorithmes de cgmesh ne voient que Context.
class GraphContext : public Context
{
public:
	// Le drapeau appartient a l'appelant et doit survivre au contexte.
	void SetCancellationFlag (const std::atomic<bool> *flag) { m_cancelled = flag; }

	bool IsAborted () const override
	{
		return m_cancelled != nullptr && m_cancelled->load (std::memory_order_relaxed);
	}

private:
	const std::atomic<bool> *m_cancelled = nullptr;
};

// VOIE UNIQUE du jeton d'annulation, du moteur vers les algorithmes. Tout
// adaptateur qui appelle un algorithme annulable ecrit exactement cet appel :
// une seconde forme rendrait invisible, a la lecture, le fait que le jeton
// passe partout de la meme facon.
inline GraphContext MakeGraphContext (const cggraph::EvalContext &ctx)
{
	GraphContext graphContext;
	graphContext.SetCancellationFlag (ctx.GetCancellationFlag ());
	return graphContext;
}

// Ce qu'un noeud a EFFET DE BORD a ecrit. Un pilote en tete haute doit pouvoir
// nommer les fichiers produits sans connaitre le type du puits ; c'est aussi ce
// qui rend le nommage deterministe VERIFIABLE -- deux executions du meme
// document sur la meme entree doivent nommer les memes fichiers, et sans cette
// liste personne ne peut le constater.
class FileSink
{
public:
	virtual ~FileSink () = default;

	// Dans l'ordre d'ecriture, depuis la construction du noeud. Rendu PAR VALEUR :
	// le puits ecrit cette liste pendant Compute, et une reference sur un vecteur
	// qu'un autre fil fait croitre serait invalidee sans que personne le voie.
	virtual std::vector<std::string> GetWrittenPaths () const = 0;
};

// Ce qu'un noeud SOURCE alimente par des OCTETS sait recevoir.
//
// Symetrique de FileSink ci-dessus, et retrouve de la meme facon -- par
// dynamic_cast depuis un hote qui ne connait pas les types concrets. C'est ce
// qui evite a la facade Embind de maker (graph_api.cpp) d'enumerer les noeuds
// sources : une liste centrale aurait grossi a chaque ajout, et le sixieme
// oubli y aurait ete silencieux.
//
// POURQUOI DES OCTETS ET NON UN CHEMIN. Les sources par chemin -- mesh.io.load
// -- sont deja servies sous WebAssembly : l'hote ecrit le fichier dans MEMFS et
// pose le chemin dans un parametre. Cela ne convient PAS a une police ni a une
// image : leur identite de cache est le hash du contenu, le chemin temporaire
// est souvent supprime avant le premier calcul, et le detour par le disque
// laisse des fichiers que personne ne nettoie (maker les garde vivants pour la
// duree de la page, faute de mieux). Un port d'octets n'a aucun de ces defauts.
//
// Les deux methodes sont celles que portaient deja LoadFontNode et
// LoadImageNode : l'interface ne fait que les nommer ensemble.
class ByteSource
{
public:
	virtual ~ByteSource () = default;

	// Pose le contenu ET verse aussitot son hash a la signature. Les deux dans
	// la meme operation, sans quoi l'identite de cache pourrait se desynchroniser
	// du contenu -- et le cache resservirait un resultat perime sans jamais
	// planter.
	virtual void SetBytes (std::vector<unsigned char> bytes) = 0;

	// Purement decoratif, HORS signature : renommer ne recalcule rien.
	virtual void SetName (const std::string &name) = 0;
};

// Lectures de parametres qui rendent le defaut quand l'entree manque, n'a pas le
// type demande, ou est INTERNE. Un adaptateur ne devine jamais : c'est son
// constructeur qui a pose les valeurs, donc l'absence est une erreur de
// programmation, pas un cas d'usage a diagnostiquer.
//
// Le refus des parametres INTERNES n'est pas une commodite, c'est ce qui tient
// l'invariant de l'etape 6 : un parametre interne est ecrit par
// RefreshExternalState, donc pendant la pre-passe d'une evaluation, qui peut etre
// celle d'un AUTRE fil. Le lire depuis Compute serait la seule lecture de
// parametre concurrente d'une ecriture. Un noeud qui a besoin de son etat
// exterieur pendant le calcul le releve lui-meme, il ne le relit pas dans le jeu.
int GetInt (const cggraph::ParamSet &params, const std::string &name, int fallback);
float GetFloat (const cggraph::ParamSet &params, const std::string &name, float fallback);
bool GetBool (const cggraph::ParamSet &params, const std::string &name, bool fallback);
std::string GetString (const cggraph::ParamSet &params, const std::string &name,
                       const std::string &fallback);

// SORTIE D'UN ADAPTATEUR QUI PASSE PAR Mesh_half_edge -- regle, pas conseil.
//
// `Mesh_half_edge (const Mesh *)` fait deja une COPIE PROFONDE de l'entree :
// c'est elle qui tient la frontiere d'immuabilite, et elle ne se negocie pas.
// Ce qui se negocie, c'est la sortie. Ecrire
//
//     out[0] = Value::Make (Types ().mesh, std::make_shared<Mesh> (*model.m_pMesh));
//
// fait une SECONDE copie profonde, d'un objet que l'enveloppe detruit a la ligne
// suivante. Sur un maillage de 2 M de triangles cela vaut 88 Mio, au pic, pour
// rien. La forme juste CEDE le maillage de travail :
//
//     out[0] = Value::Make (Types ().mesh, std::shared_ptr<Mesh> (model.release ()));
//
// ⚠ release () vide l'enveloppe : plus aucun acces a `model`, a `model.m_pMesh`
// ni a un pointeur qui en vient apres cette ligne. Elle doit etre la DERNIERE.
//
// Cette regle n'a pas de filet automatique : rien ne fait echouer un adaptateur
// qui copie, il est seulement deux fois trop lourd. C'est la relecture qui la
// tient, et c'est pour cela qu'elle est ecrite ici plutot que dans un document.

} // namespace cggraph_nodes
