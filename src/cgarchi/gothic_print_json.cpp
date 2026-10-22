#include <cgarchi/gothic_print_json.h>

#include <string>

#include <cgarchi/parameterized_gothic.h>   // GothicPrintVerdict, GothicPrintScopeName

namespace cgarchi
{

std::string GothicPrintVerdictToJson (const GothicPrintVerdict &verdict)
{
	const GothicPrintCheck &check = verdict.check;
	PrintReportEnvelope envelope;
	envelope.scope            = GothicPrintScopeName(check.scope);
	envelope.reason           = check.reason;
	envelope.printable        = verdict.IsPrintable();
	envelope.exportAllowed    = verdict.ExportAllowed();
	envelope.geometry.allowed = check.geometry.allowed;
	envelope.geometry.strict  = check.geometry.strict;
	envelope.geometry.message = check.geometry.message;
	envelope.printedSizeMm    = verdict.printedSizeMm;
	envelope.error            = verdict.printError;
	envelope.constraints      = verdict.constraints;
	return PrintReportToJson(envelope, check.print);
}

} // namespace cgarchi
