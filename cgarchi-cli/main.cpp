// cgarchi-cli : acces en ligne de commande aux colonnes des cinq ordres de
// Vignole (cgarchi) et a leurs donnees embarquees.
//
//   cgarchi-cli column    --settings <fichier|-> --out <chemin.obj|.glb|.stl>
//   cgarchi-cli colonnade --settings <fichier|-> --out <chemin> [--columns N] [--no-ornaments]
//   cgarchi-cli levels    --settings <fichier|->
//   cgarchi-cli defaults  [--order toscan|dorique|ionique|corinthien|composite]
//   cgarchi-cli data      orders|hephaistos
//
// Les reglages sont un document "classical-column/v1"
// (src/cgarchi/schemas/classical-column-v1.schema), lu par
// LoadClassicalColumnFromJson. Toute sortie est un JSON sur stdout :
// {"ok": true, ...} ou {"ok": false, "error": ..., "report": ...} ; code de
// retour 0 en succes, 1 sur refus ou echec, 2 sur ligne de commande invalide.
// `data` ecrit le JSON embarque tel quel.
//
// Toute la logique vit dans cgarchi ; ce fichier ne fait que la cabler.

#include <cgarchi/architecture_gothic.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_orders_data.h>
#include <cgarchi/parameterized_classical.h>
#include <cgarchi/parameterized_classical_json.h>

#include "mesh.h"
#include "mesh_io.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using nlohmann::json;

namespace
{

constexpr const char *kUsage =
	"usage:\n"
	"  cgarchi-cli column    --settings <file|-> --out <path.obj|.glb|.stl>\n"
	"  cgarchi-cli colonnade --settings <file|-> --out <path> [--columns N] [--no-ornaments]\n"
	"  cgarchi-cli levels    --settings <file|->\n"
	"  cgarchi-cli defaults  [--order toscan|dorique|ionique|corinthien|composite]\n"
	"  cgarchi-cli data      orders|hephaistos\n";

// Cles des ordres, dans l'ordre de classical::Order (schema classical-column/v1).
const std::vector<std::string> kOrderKeys = {"toscan", "dorique", "ionique", "corinthien", "composite"};

struct CliError
{
	int code;
	std::string message;
};

int Emit (const json &j, int code)
{
	std::cout << j.dump (2) << '\n';
	return code;
}

int Fail (const std::string &message, const std::string &reportJson = {})
{
	json j = {{"ok", false}, {"error", message}};
	if (!reportJson.empty ())
		j["report"] = json::parse (reportJson, nullptr, false);
	return Emit (j, 1);
}

// Options "--cle valeur" et drapeaux "--cle" (ceux de `flags`).
std::map<std::string, std::string> ParseOptions (const std::vector<std::string> &args, std::size_t first,
                                                 const std::vector<std::string> &flags)
{
	std::map<std::string, std::string> opts;
	for (std::size_t i = first; i < args.size (); ++i)
	{
		const std::string &a = args[i];
		if (a.rfind ("--", 0) != 0)
			throw CliError {2, "argument inattendu : " + a};
		if (std::find (flags.begin (), flags.end (), a) != flags.end ())
		{
			opts[a] = "1";
			continue;
		}
		if (i + 1 >= args.size ())
			throw CliError {2, "valeur manquante pour " + a};
		opts[a] = args[++i];
	}
	return opts;
}

std::string Require (const std::map<std::string, std::string> &opts, const std::string &key)
{
	auto it = opts.find (key);
	if (it == opts.end ())
		throw CliError {2, "option requise : " + key};
	return it->second;
}

void CheckKnown (const std::map<std::string, std::string> &opts, const std::vector<std::string> &known)
{
	for (const auto &kv : opts)
		if (std::find (known.begin (), known.end (), kv.first) == known.end ())
			throw CliError {2, "option inconnue : " + kv.first};
}

std::string ReadText (const std::string &path)
{
	if (path == "-")
		return std::string (std::istreambuf_iterator<char> (std::cin), {});
	std::ifstream in (path, std::ios::binary);
	if (!in)
		throw CliError {1, "lecture impossible : " + path};
	return std::string (std::istreambuf_iterator<char> (in), {});
}

cgarchi::ClassicalColumnSettings LoadSettings (const std::string &path)
{
	const std::string text = ReadText (path);
	cgarchi::GothicBuildReport report;
	std::optional<cgarchi::ClassicalColumnSettings> s = cgarchi::LoadClassicalColumnFromJson (text, &report);
	if (!s)
		throw CliError {1, "reglages refuses (" + path + ")\n" + cgarchi::GothicBuildReportToJson (report)};
	return *s;
}

std::string Extension (const std::string &path)
{
	const std::size_t dot = path.find_last_of ('.');
	std::string ext = dot == std::string::npos ? std::string () : path.substr (dot + 1);
	std::transform (ext.begin (), ext.end (), ext.begin (), [] (unsigned char c) { return char (std::tolower (c)); });
	return ext;
}

// Ecrit le maillage selon l'extension ; leve CliError en echec.
void WriteMesh (const Mesh &mesh, const std::string &path)
{
	const std::string ext = Extension (path);
	int rc = -1;
	if (ext == "obj")
		rc = MeshIO::export_obj (mesh, path.c_str ());
	else if (ext == "glb")
		rc = MeshIO::export_glb (mesh, path.c_str ());
	else if (ext == "stl")
		rc = MeshIO::export_stl_binary (mesh, path.c_str ());
	else
		throw CliError {2, "format de sortie non pris en charge (obj, glb, stl) : " + path};
	if (rc != 0)
		throw CliError {1, "ecriture impossible : " + path};
}

json MeshSummary (const Mesh &mesh, const std::string &path)
{
	return {{"out", path}, {"vertices", mesh.GetNVertices ()}, {"faces", mesh.GetNFaces ()}};
}

json LevelsJson (const cgarchi::ClassicalColumnLevels &l)
{
	auto opt = [] (const std::optional<double> &v) { return v ? json (*v) : json (nullptr); };
	return {{"foot", l.foot},
	        {"pedestalTop", opt (l.pedestalTop)},
	        {"shaftFoot", l.shaftFoot},
	        {"shaftTop", l.shaftTop},
	        {"capitalBottom", l.capitalBottom},
	        {"capitalTop", l.capitalTop},
	        {"top", l.top},
	        {"entablatureFoot", opt (l.entablatureFoot)}};
}

int CmdColumn (const std::vector<std::string> &args)
{
	auto opts = ParseOptions (args, 2, {});
	CheckKnown (opts, {"--settings", "--out"});
	const cgarchi::ClassicalColumnSettings s = LoadSettings (Require (opts, "--settings"));
	const std::string out = Require (opts, "--out");
	std::vector<cgarchi::classical::PlanConflict> conflicts;
	std::unique_ptr<Mesh> mesh = cgarchi::BuildClassicalColumnMesh (s, nullptr, &conflicts);
	if (!mesh || mesh->GetNFaces () == 0)
		return Fail ("maillage vide");
	WriteMesh (*mesh, out);
	json j = MeshSummary (*mesh, out);
	j["ok"] = true;
	j["settings"] = json::parse (cgarchi::ExportClassicalColumnJson (s));
	j["levels"] = LevelsJson (cgarchi::ComputeClassicalColumnLevels (s));
	j["planConflicts"] = conflicts.size ();
	return Emit (j, 0);
}

// Reporte les reglages sur les Parameter de la colonnade, par identifiant.
void ApplyColonnadeSettings (cgarchi::ParameterizedClassicalColonnade &c, const cgarchi::ClassicalColumnSettings &s,
                             int columns, bool ornaments)
{
	for (Parameter &p : c.GetParameters ())
	{
		const std::string &id = p.Id ();
		if (id == "Order") p.SetInt (s.order);
		else if (id == "Module") p.SetFloat (s.module);
		else if (id == "Columns") p.SetInt (columns);
		else if (id == "Pedestal") p.SetBool (s.pedestal);
		else if (id == "Doric cornice") p.SetInt (s.doricCornice);
		else if (id == "Ornaments") p.SetBool (ornaments);
		else if (id == "Ornament envelope") p.SetBool (s.envelope);
		else if (id == "Volutes") p.SetBool (s.volutes);
		else if (id == "Flutes") p.SetBool (s.flutes);
		else if (id == "Doric flutes") p.SetInt (s.doricFlutes);
		else if (id == "Radial segments") p.SetInt (s.radialSegments);
		else if (id == "Curve segments") p.SetInt (s.curveSegments);
		else throw CliError {1, "parametre de colonnade inattendu : " + id};
	}
}

int CmdColonnade (const std::vector<std::string> &args)
{
	auto opts = ParseOptions (args, 2, {"--no-ornaments"});
	CheckKnown (opts, {"--settings", "--out", "--columns", "--no-ornaments"});
	const cgarchi::ClassicalColumnSettings s = LoadSettings (Require (opts, "--settings"));
	const std::string out = Require (opts, "--out");
	int columns = 4;
	if (opts.count ("--columns"))
	{
		try { columns = std::stoi (opts["--columns"]); }
		catch (const std::exception &) { throw CliError {2, "--columns : entier attendu"}; }
		if (columns < 1 || columns > 12)
			throw CliError {2, "--columns : hors de [1, 12]"};
	}
	const bool ornaments = !opts.count ("--no-ornaments");

	cgarchi::ParameterizedClassicalColonnade c;
	ApplyColonnadeSettings (c, s, columns, ornaments);
	c.Regenerate ();
	if (!c.LastError ().empty ())
		return Fail (c.LastError ());
	const Mesh *mesh = c.GetMesh ();
	if (!mesh || mesh->GetNFaces () == 0)
		return Fail ("maillage vide");
	WriteMesh (*mesh, out);
	json j = MeshSummary (*mesh, out);
	j["ok"] = true;
	j["columns"] = columns;
	j["ornaments"] = ornaments;
	j["settings"] = json::parse (cgarchi::ExportClassicalColumnJson (s));
	if (!c.Warnings ().empty ())
		j["warnings"] = c.Warnings ();
	return Emit (j, 0);
}

int CmdLevels (const std::vector<std::string> &args)
{
	auto opts = ParseOptions (args, 2, {});
	CheckKnown (opts, {"--settings"});
	const cgarchi::ClassicalColumnSettings s = LoadSettings (Require (opts, "--settings"));
	return Emit ({{"ok", true},
	              {"settings", json::parse (cgarchi::ExportClassicalColumnJson (s))},
	              {"levels", LevelsJson (cgarchi::ComputeClassicalColumnLevels (s))}},
	             0);
}

int CmdDefaults (const std::vector<std::string> &args)
{
	auto opts = ParseOptions (args, 2, {});
	CheckKnown (opts, {"--order"});
	cgarchi::ClassicalColumnSettings s;
	if (opts.count ("--order"))
	{
		auto it = std::find (kOrderKeys.begin (), kOrderKeys.end (), opts["--order"]);
		if (it == kOrderKeys.end ())
			throw CliError {2, "ordre inconnu : " + opts["--order"]};
		s.order = int (it - kOrderKeys.begin ());
	}
	std::cout << cgarchi::ExportClassicalColumnJson (s) << '\n';
	return 0;
}

int CmdData (const std::vector<std::string> &args)
{
	if (args.size () != 3)
		throw CliError {2, "usage : cgarchi-cli data orders|hephaistos"};
	if (args[2] == "orders")
		std::cout << cgarchi::ClassicalOrdersJson ();
	else if (args[2] == "hephaistos")
		std::cout << cgarchi::TempleHephaistosJson ();
	else
		throw CliError {2, "jeu de donnees inconnu : " + args[2]};
	return 0;
}

} // namespace

int main (int argc, char **argv)
{
#ifdef _WIN32
	// Sorties UTF-8 telles quelles : pas de traduction de fin de ligne.
	_setmode (_fileno (stdout), _O_BINARY);
#endif
	const std::vector<std::string> args (argv, argv + argc);
	if (args.size () < 2)
	{
		std::cerr << kUsage;
		return 2;
	}
	const std::string &cmd = args[1];
	try
	{
		if (cmd == "column")    return CmdColumn (args);
		if (cmd == "colonnade") return CmdColonnade (args);
		if (cmd == "levels")    return CmdLevels (args);
		if (cmd == "defaults")  return CmdDefaults (args);
		if (cmd == "data")      return CmdData (args);
		std::cerr << kUsage;
		return 2;
	}
	catch (const CliError &e)
	{
		if (e.code == 2)
		{
			std::cerr << e.message << '\n' << kUsage;
			return 2;
		}
		const std::size_t nl = e.message.find ('\n');
		return nl == std::string::npos ? Fail (e.message) : Fail (e.message.substr (0, nl), e.message.substr (nl + 1));
	}
	catch (const std::exception &e)
	{
		return Fail (e.what ());
	}
}
