#pragma once

// Document JSON des reglages d'une colonne classique (ClassicalColumnSettings),
// schema "classical-column/v1" : src/cgarchi/schemas/classical-column-v1.schema.
//
// Un objet plat, un champ par reglage, tous requis :
//
//   schema          "classical-column/v1"
//   order           cle de l'ordre (classical::OrderKey) : "toscan", "dorique",
//                   "ionique", "corinthien", "composite"
//   module          nombre, [kClassicalModuleMin, kClassicalModuleMax], borne
//                   controlee sur la valeur arrondie en float
//   pedestal, entablature, envelope, volutes, flutes   booleens
//   doricCornice    "dentils" | "mutules"            (classical::DoricCornice)
//   doricFlutes     "segment60" | "semicircle"       (classical::FluteSection)
//   radialSegments  entier, [kClassicalRadialSegmentsMin, kClassicalRadialSegmentsMax]
//   curveSegments   entier, [kClassicalCurveSegmentsMin, kClassicalCurveSegmentsMax]
//
// Les domaines sont ceux des Parameter de ParameterizedClassicalColumn.

#include <optional>
#include <string>

#include <cgarchi/parameterized_classical.h>

namespace cgarchi
{

struct GothicBuildReport;

// Identifiant du schema ecrit et seul accepte.
constexpr const char *kClassicalColumnSchema = "classical-column/v1";

// Lecture stricte d'un document "classical-column/v1". Rend les reglages, ou
// nullopt si le document est refuse. Refus, chacun nomme par le chemin JSON du
// champ dans `report` (codes json_* de GothicBuildError) :
//   - json_parse : texte illisible ;
//   - json_schema : `schema` autre que "classical-column/v1" (version future
//     comprise), champ inconnu, cle en double ;
//   - json_missing : `schema` ou un reglage absent ;
//   - json_type : type faux, racine non objet ;
//   - json_value : valeur hors domaine, ordre inconnu, nombre non fini (en
//     double ou une fois arrondi en float).
// Une cle en double est signalee pendant le parsing : si le texte est ensuite
// illisible (tronque apres le doublon), le rapport porte les deux diagnostics,
// json_schema puis json_parse.
// Les messages citent les nombres en locale classique.
std::optional<ClassicalColumnSettings> LoadClassicalColumnFromJson (const std::string &jsonText,
                                                                    GothicBuildReport *report = nullptr);

// Document canonique de `settings` : cles dans l'ordre lexicographique,
// indentation de 2 espaces, `\n` sans fin de ligne finale, nombres en locale
// classique, `module` ecrit par la plus courte ecriture decimale qui redonne
// le meme float (std::to_chars). LoadClassicalColumnFromJson le relit a
// l'identique.
// Leve std::invalid_argument si un reglage est hors du domaine de son
// Parameter (module NaN compris) : un document ecrit est toujours relisible.
// Leve std::logic_error si l'ecriture de `module` ne se relit pas en le meme
// float, ce que le contrat de std::to_chars exclut.
std::string ExportClassicalColumnJson (const ClassicalColumnSettings &settings);

} // namespace cgarchi
