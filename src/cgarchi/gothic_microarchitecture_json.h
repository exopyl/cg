#pragma once

// Specification JSON de la microarchitecture gothique, schema
// "gothic-microarchitecture/v1" : src/cgarchi/schemas/gothic-microarchitecture-v1.schema.

#include <string>

#include <cgarchi/gothic_microarchitecture.h>

namespace cgarchi::microarch
{

// Valeur du champ `schema` d'un document v1.
inline constexpr const char *kMicroarchSchemaV1 = "gothic-microarchitecture/v1";

// Valeur du champ `schema` du document de reseau (NetworkToJson).
inline constexpr const char *kMicroarchNetworkSchemaV1 = "gothic-microarchitecture-network/v1";

// Un texte de plus de SpecLimits::max_text_bytes octets est refuse avant le parsing,
// par la seule erreur json_too_large.
// Lit un document v1 et releve toutes ses erreurs en une passe : erreurs JSON
// (json_*), puis controles structurels (ValidateStructure). Un epuisement de la
// memoire pendant la lecture est une erreur json_parse comptee sans entree ; seul
// l'ajout des diagnostics a `report` peut encore lever std::bad_alloc. Transactionnel :
// `out` n'est ecrit que si le document est accepte, c'est-a-dire sans erreur.
// Les diagnostics s'ajoutent a `report`.
bool LoadSpecFromJson (const std::string &text, MicroarchSpec &out, MicroarchReport &report);

// Comme LoadSpecFromJson ; un fichier illisible est une erreur json_parse.
bool LoadSpecFromFile (const std::string &path, MicroarchSpec &out, MicroarchReport &report);

// Document v1 de la specification. Une specification valide se relit a
// l'identique, doubles compris. Un id en UTF-8 invalide est ecrit avec U+FFFD.
std::string SpecToJson (const MicroarchSpec &spec);

// {"errors": {name: count}, "warnings": {name: count}, "entries": [{"severity":
// "error" | "warning", "code": name, "parameter": path, "message": ...}]} : meme
// format que GothicBuildReportToJson.
std::string ReportToJson (const MicroarchReport &report);

// Document du reseau, format "gothic-microarchitecture-network/v1" (plan
// d'execution, §7 et §10.9) : echelle, tolerances resolues, niveaux, courbes de
// base (PRC et candidats, profil avec ses centres), repliques (courbe de base,
// element de symetrie, courbe du plan exacte), jonctions, croisements visuels,
// extremites libres et rapport (meme objet que ReportToJson). Analytique : aucun
// point echantillonne. Les doubles se relisent a l'identique. `spec` est celle
// dont `network` est issu (BuildNetwork).
std::string NetworkToJson (const Network &network, const MicroarchSpec &spec, const MicroarchReport &report);

} // namespace cgarchi::microarch
