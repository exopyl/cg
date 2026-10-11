#pragma once

#include <string>
#include <vector>

#include "print_validation.h"   // PrintConstraints

struct GothicPrintVerdict;

//
// Serialisation du controle de fabrication (nlohmann, dans le .cpp seul :
// print_validation.cpp reste sans dependance JSON). Deux documents, hors du
// document de la fenetre gothique :
//
//   - « print-constraints/v1 » : les contraintes de l'imprimante, gardees par
//     l'utilisateur ;
//   - « print-report/v1 » : le verdict de la garde d'export
//     (CheckGothicExportAndPrint), en lecture seule.
//
// Nombres : notation JSON (point decimal) quelle que soit la locale globale,
// doubles ecrits pour etre relus a l'identique.
//

constexpr const char *kPrintConstraintsSchema = "print-constraints/v1";
constexpr const char *kPrintReportSchema      = "print-report/v1";

// Refus de lecture, nomme par le chemin JSON du champ en cause (« minWallMm »,
// « envelopeMm[1] ») ; chemin vide pour le document entier.
struct PrintJsonError
{
	std::string path;
	std::string message;
};

// Lit un document print-constraints/v1. Lecture STRICTE : document illisible,
// racine qui n'est pas un objet, `schema` absent ou autre que
// « print-constraints/v1 », champ inconnu, type faux ou valeur hors du domaine
// de PrintConstraints -> une erreur par champ en cause.
//   - mmPerUnit (fini, > 0) et envelopeMm (trois nombres finis > 0) : ABSENTS,
//     ils restent absents, sans valeur par defaut ; null est un type faux ;
//   - minWallMm, minHoleMm, maxTipLengthMm (domaine mesurable), noiseDepthRatio
//     ([0, 1[), minDetailMm (> 0), maxRecordsPerKind (entier >= 0) : absents,
//     ils prennent leur defaut constexpr (print_validation.h), et non la valeur
//     anterieure de `out`.
// Rend les erreurs ; vide : `out` recoit les contraintes lues. Sinon `out` est
// strictement inchange.
std::vector<PrintJsonError> PrintConstraintsFromJson (const std::string &jsonText, PrintConstraints &out);

// Document print-constraints/v1 de `constraints` : TOUS les champs, defauts
// compris (un document ne change pas de sens si un defaut change), sauf
// mmPerUnit et envelopeMm absents, qui restent absents. Relu par
// PrintConstraintsFromJson, il redonne les memes contraintes a l'identique.
std::string PrintConstraintsToJson (const PrintConstraints &constraints);

// Document print-report/v1 du verdict :
//   schema, scope (GothicPrintScopeName), reason, printable (IsPrintable du
//   verdict), exportAllowed, geometry {allowed, strict, message}, evaluated,
//   mmPerUnit (null sans echelle), printedSizeMm ([x, y, z] ou null),
//   counts (les 10 categories de PrintDefectName, zero compris, sommees sur les
//   tranches), displayCounts (memes cles, PrintDisplayCounts : maximum par
//   tranche), records ([{kind, boxMm, depthMm, areaMm2, contacts,
//   widthBelowMm, slice, zLowMm, zHighMm}], deja plafonnes par categorie),
//   slices ([{zLowMm, zHighMm, zCutMm, areaMm2, silhouette, counts}]),
//   error (printError, null si vide), constraints (document des contraintes
//   employees, sans `schema`, ou null).
std::string GothicPrintVerdictToJson (const GothicPrintVerdict &verdict);
