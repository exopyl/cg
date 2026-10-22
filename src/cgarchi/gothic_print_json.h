#pragma once

#include <string>

#include "print_validation_json.h"   // PrintReportToJson, print-constraints/v1

namespace cgarchi
{

struct GothicPrintVerdict;

// Document print-report/v1 du verdict de la garde d'export d'une Gothic Window
// (CheckGothicExportAndPrint), par PrintReportToJson :
//   scope = GothicPrintScopeName (check.scope), reason = check.reason,
//   printable = IsPrintable du verdict, exportAllowed = ExportAllowed,
//   geometry = check.geometry, rapport = check.print, printedSizeMm,
//   error = printError, constraints = contraintes employees.
std::string GothicPrintVerdictToJson (const GothicPrintVerdict &verdict);

} // namespace cgarchi
