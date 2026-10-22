#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "print_validation.h"   // PrintConstraints, PrintabilityReport

//
// Serialisation du controle de fabrication (nlohmann, dans le .cpp seul :
// print_validation.cpp reste sans dependance JSON). Deux documents :
//
//   - « print-constraints/v1 » : les contraintes de l'imprimante, gardees par
//     l'utilisateur ;
//   - « print-report/v1 » : le verdict d'un controle de fabrication, en
//     lecture seule. Le producteur du verdict remplit une PrintReportEnvelope.
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

// Ce que le producteur d'un verdict de fabrication sait, hors du rapport de
// mesure : portee, motif, decisions et echo des entrees.
struct PrintReportGeometry
{
	bool        allowed = false;   // l'export de la geometrie est permis
	bool        strict  = false;   // maillage strict
	std::string message;           // motif du refus, ou defauts ; vide sinon
};

struct PrintReportEnvelope
{
	std::string         scope;                 // portee du controle, nom stable en snake_case
	std::string         reason;                // motif d'une portee partielle ou nulle ; vide sinon
	bool                printable     = false;
	bool                exportAllowed = false;
	PrintReportGeometry geometry;
	std::optional<std::array<double, 3>> printedSizeMm;   // emprise imprimee (x, y, z), en mm
	std::string         error;                 // erreur du controle ; vide : null
	std::optional<PrintConstraints> constraints;   // contraintes employees
};

// Document print-report/v1 :
//   schema, scope, reason, printable, exportAllowed, geometry {allowed, strict,
//   message} (enveloppe), evaluated, mmPerUnit (null si <= 0), printedSizeMm
//   ([x, y, z] ou null), counts (les 10 categories de PrintDefectName, zero
//   compris, sommees sur les tranches), displayCounts (memes cles,
//   PrintDisplayCounts : maximum par tranche), records ([{kind, boxMm, depthMm,
//   areaMm2, contacts, widthBelowMm, slice, zLowMm, zHighMm}], deja plafonnes
//   par categorie), slices ([{zLowMm, zHighMm, zCutMm, areaMm2, silhouette,
//   counts}]), error (null si vide), constraints (document des contraintes,
//   sans `schema`, ou null). Les octets invalides en UTF-8 des textes sont
//   remplaces.
std::string PrintReportToJson (const PrintReportEnvelope &envelope, const PrintabilityReport &report);
