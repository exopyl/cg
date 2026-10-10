#pragma once

#include <string>

class Model;

namespace peles
{
namespace core
{

class Document;

// Resultat d'un chargement : le modele ajoute, ou la raison de l'echec.
//
// ⚠ UN ECHEC N'EST PAS UNE PANNE APPLICATIVE. Un fichier illisible est une
// donnee d'entree, pas un defaut du programme : le document reste utilisable et
// l'appelant decide quoi en dire. Rendre nullptr sans raison obligerait
// l'appelant a en inventer une.
struct LoadResult
{
	Model*      model = nullptr;
	std::string error;
};

// Charge `path` comme un NOUVEAU modele du document.
//
// Le modele porte le nom de fichier seul ; son chemin complet reste dans
// Model::m_path. En cas d'echec, le modele vide cree pour l'accueillir est
// retire : un document ne contient jamais d'entree sans geometrie.
//
// ⚠ SYNCHRONE, et c'est assume : la thread-safety de cgmesh et de ses
// importateurs n'a pas ete examinee, et un STEP passe par OCCT.
LoadResult LoadModel (Document& document, const std::string& path);

} // namespace core
} // namespace peles
