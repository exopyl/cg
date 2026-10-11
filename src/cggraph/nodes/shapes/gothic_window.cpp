#include "gothic_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/parameterized.h"
#include "../../../cgmesh/parameterized_gothic.h"
#include "../../../cgmesh/print_validation.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

const cggraph::NodeDesc &Desc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "shape.gothic.window";
		// Les deux sous-objets geometriques promus. OPTIONNELS : une baie sans
		// moulure est le cas le plus courant, et c'est la position « Flat » du
		// menu qu'ils remplacent.
		d.inputs.push_back ({ "profil d'ebrasement", Types ().splayProfile, true });
		d.inputs.push_back ({ "profil de barre", Types ().barProfile, true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

// Le nom du parametre « Profile », qui n'a PAS sa place ici : les deux ports le
// remplacent. Ecrit une fois, pour que le filtre du constructeur et le test qui
// le verifie ne puissent pas diverger.
const char *const kMenuParam = "Profile";

// Controle d'impression : noms stables, comme ceux des stats.
//
// « Non fixee » se dit par un booleen, et non par une valeur sentinelle : la
// largeur fixee part telle quelle dans la garde, qui nomme une valeur hors
// domaine (0 comprise) dans printError, comme pour le maker (setPrintedWidthMm) ;
// l'enveloppe, triplet, a le meme booleen. Largeur et cotes valent 0 par
// defaut : fixees sans valeur, elles donnent une erreur nommee, jamais une
// taille supposee.
//
// Seuils : float du ParamSet, convertis en double. L'ecart a la valeur decimale
// (0,4f = 0,4 + 6e-9 mm) est sous le pas de Clipper2 (1e-6 mm). noiseDepthRatio
// et minDetailMm gardent leur defaut, comme dans le maker.
const char *const kPrintCheck       = "Print check";
const char *const kPrintWidthSet    = "Print width set";
const char *const kPrintedWidthMm   = "Printed width mm";
const char *const kPrintMinWallMm   = "Print min wall mm";
const char *const kPrintMinHoleMm   = "Print min hole mm";
const char *const kPrintMaxTipMm    = "Print max tip mm";
const char *const kPrintEnvelopeSet = "Print envelope set";
const char *const kPrintEnvelopeMm[3] = { "Print envelope x mm", "Print envelope y mm", "Print envelope z mm" };

void DeclarePrintParams (cggraph::ParamSet &params)
{
	params.SetBool (kPrintCheck, false);
	params.SetBool (kPrintWidthSet, false);
	params.SetFloat (kPrintedWidthMm, 0.0f);
	params.SetFloat (kPrintMinWallMm, static_cast<float> (kDefaultMinWallMm));
	params.SetFloat (kPrintMinHoleMm, static_cast<float> (kDefaultMinHoleMm));
	params.SetFloat (kPrintMaxTipMm, static_cast<float> (kDefaultMaxTipLengthMm));
	params.SetBool (kPrintEnvelopeSet, false);
	for (const char *side : kPrintEnvelopeMm)
		params.SetFloat (side, 0.0f);
}

PrintConstraints PrinterFrom (const cggraph::ParamSet &params)
{
	PrintConstraints printer;
	printer.minWallMm =
		static_cast<double> (GetFloat (params, kPrintMinWallMm, static_cast<float> (kDefaultMinWallMm)));
	printer.minHoleMm =
		static_cast<double> (GetFloat (params, kPrintMinHoleMm, static_cast<float> (kDefaultMinHoleMm)));
	printer.maxTipLengthMm =
		static_cast<double> (GetFloat (params, kPrintMaxTipMm, static_cast<float> (kDefaultMaxTipLengthMm)));
	if (GetBool (params, kPrintEnvelopeSet, false))
	{
		std::array<double, 3> envelope {};
		for (std::size_t k = 0; k < envelope.size (); ++k)
			envelope[k] = static_cast<double> (GetFloat (params, kPrintEnvelopeMm[k], 0.0f));
		printer.envelopeMm = envelope;
	}
	return printer;
}

std::optional<double> PrintedWidthFrom (const cggraph::ParamSet &params)
{
	if (!GetBool (params, kPrintWidthSet, false))
		return std::nullopt;
	return static_cast<double> (GetFloat (params, kPrintedWidthMm, 0.0f));
}

double ScopeCode (GothicPrintScope scope)
{
	switch (scope)
	{
	case GothicPrintScope::NotRun:            return 0.0;
	case GothicPrintScope::Complete:          return 1.0;
	case GothicPrintScope::ProfileNotChecked: return 2.0;
	}
	return 0.0;
}

// Garde d'export sur `mesh`, maillage du dernier Regenerate de `shape`, et ses
// stats print.*. Toute exception de la garde (verdict geometrique compris)
// donne print.error 1, portee 0, comptes nuls.
void AppendPrintStats (const ParameterizedGothicWindow &shape, const Mesh &mesh, const cggraph::ParamSet &params,
                       std::vector<cggraph::NodeStat> &stats)
{
	std::array<std::size_t, kPrintDefectCount> shown {};
	double scope = 0.0, printable = 0.0, evaluated = 0.0, error = 1.0;
	try
	{
		const GothicPrintVerdict verdict =
			CheckGothicExportAndPrintAtWidth (shape, mesh, PrinterFrom (params), PrintedWidthFrom (params));
		shown = PrintDisplayCounts (verdict.check.print);
		scope = ScopeCode (verdict.check.scope);
		printable = verdict.IsPrintable () ? 1.0 : 0.0;
		evaluated = verdict.check.print.evaluated ? 1.0 : 0.0;
		error = verdict.printError.empty () ? 0.0 : 1.0;
	}
	catch (...)
	{
		shown = {};
		scope = printable = evaluated = 0.0;
		error = 1.0;
	}
	for (std::size_t k = 0; k < kPrintDefectCount; ++k)
		stats.push_back ({ std::string ("print.") + PrintDefectName (static_cast<PrintDefect> (k)),
		                   static_cast<double> (shown[k]) });
	stats.push_back ({ "print.scope", scope });
	stats.push_back ({ "print.printable", printable });
	stats.push_back ({ "print.evaluated", evaluated });
	stats.push_back ({ "print.error", error });
}

} // namespace

GothicWindowNode::GothicWindowNode ()
{
	// Valeurs par defaut relevees sur la forme -- « Profile » excepte.
	ParameterizedGothicWindow shape;
	for (Parameter &p : shape.GetParameters ())
	{
		if (p.GetName () == kMenuParam)
			continue;
		switch (p.GetType ())
		{
		case Parameter::INT:
		case Parameter::ENUM:
			GetParams ().SetInt (p.GetName (), p.GetInt ());
			break;
		case Parameter::FLOAT:
			GetParams ().SetFloat (p.GetName (), p.GetFloat ());
			break;
		case Parameter::BOOL:
			GetParams ().SetBool (p.GetName (), p.GetBool ());
			break;
		case Parameter::STRING:
			GetParams ().SetString (p.GetName (), p.GetString ());
			break;
		}
	}
	DeclarePrintParams (GetParams ());
}

const cggraph::NodeDesc &GothicWindowNode::GetDesc () const
{
	return Desc ();
}

bool GothicWindowNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                cggraph::ValueList &out)
{
	(void)ctx;

	// Instance LOCALE, comme l'adaptateur generique : Regenerate () ecrit dans
	// l'objet, un membre serait un etat partage entre deux evaluations.
	ParameterizedGothicWindow shape;

	const std::size_t count = shape.GetParameters ().size ();
	for (std::size_t i = 0; i < count; ++i)
	{
		std::vector<Parameter> declared = shape.GetParameters ();
		if (i >= declared.size ())
			break;
		Parameter &p = declared[i];
		if (p.GetName () == kMenuParam)
			continue;      // remplace par les ports

		switch (p.GetType ())
		{
		case Parameter::INT:
		{
			int v = GetInt (GetParams (), p.GetName (), p.GetInt ());
			p.SetInt (std::max (p.GetMinInt (), std::min (p.GetMaxInt (), v)));
			break;
		}
		case Parameter::ENUM:
		{
			int v = GetInt (GetParams (), p.GetName (), p.GetInt ());
			const int last = static_cast<int> (p.GetChoices ().size ()) - 1;
			p.SetInt (std::max (0, std::min (last < 0 ? 0 : last, v)));
			break;
		}
		case Parameter::FLOAT:
		{
			float v = GetFloat (GetParams (), p.GetName (), p.GetFloat ());
			p.SetFloat (std::max (p.GetMinFloat (), std::min (p.GetMaxFloat (), v)));
			break;
		}
		case Parameter::BOOL:
			p.SetBool (GetBool (GetParams (), p.GetName (), p.GetBool ()));
			break;
		case Parameter::STRING:
			p.SetString (GetString (GetParams (), p.GetName (), p.GetString ()));
			break;
		}
	}

	// Les sous-objets promus. Une entree optionnelle non alimentee reste VIDE ;
	// le noeud en decide, et l'absence des deux redonne exactement la position
	// « Flat ».
	const Profile2D *splay = in[0].Get<Profile2D> (Types ().splayProfile);
	const Profile2D *bar = in[1].Get<Profile2D> (Types ().barProfile);
	shape.SetSplayProfile (splay);
	shape.SetBarProfile (bar);

	shape.Regenerate ();

	const GothicBuildReport &report = shape.LastBuildReport ();
	{
		std::vector<cggraph::NodeStat> stats;
		for (std::size_t k = 0; k < kGothicBuildWarningCount; ++k)
			stats.push_back ({ std::string ("gothic.") + GothicBuildWarningName (static_cast<GothicBuildWarning> (k)),
			                   static_cast<double> (report.counts[k]) });
		// Avant la cession : la garde juge la baie et son maillage ensemble.
		const Mesh *built = shape.GetMesh ();
		if (!report.HasErrors () && built != nullptr && GetBool (GetParams (), kPrintCheck, false))
			AppendPrintStats (shape, *built, GetParams (), stats);
		const std::lock_guard<std::mutex> held (m_statsMutex);
		m_stats = std::move (stats);
	}
	if (report.HasErrors ())
		return false;

	// CESSION : le maillage de travail est cede, jamais recopie.
	std::shared_ptr<Mesh> mesh (shape.TakeMesh ());
	if (mesh == nullptr)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, std::move (mesh));
	return true;
}

void GothicWindowNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	const std::lock_guard<std::mutex> held (m_statsMutex);
	out.insert (out.end (), m_stats.begin (), m_stats.end ());
}

} // namespace cggraph_nodes
