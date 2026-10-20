#include "import_scad.h"

#include <exception>
#include <sstream>
#include <system_error>

#include "external_process.h"

#ifndef _WIN32
#include <unistd.h>
#endif
#include "mesh.h"
#include "vmeshes.h"
#include "vmeshes_io.h"

namespace cgmesh {
namespace scad {

namespace {

namespace fs = std::filesystem;

std::vector<std::string> ExecutableNames ()
{
#ifdef _WIN32
	// openscad.com n'est qu'un lanceur console de openscad.exe : appeler
	// l'executable reel evite un processus intermediaire.
	return { "openscad.exe", "openscad.com" };
#else
	return { "openscad" };
#endif
}

#ifdef _WIN32
// Un .bat ou un .cmd serait execute par cmd.exe, dont les regles
// d'echappement ne sont pas celles de CommandLineToArgvW : seuls les
// programmes PE (.exe, .com) sont admis.
bool IsAcceptedProgram (const fs::path& file)
{
	std::string ext = file.extension ().u8string ();
	for (char& c : ext)
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char> (c - 'A' + 'a');
	return ext == ".exe" || ext == ".com";
}
#endif

// `candidate` designe l'executable ou son repertoire. `note` recoit la raison
// d'un refus.
fs::path ResolveCandidate (const fs::path& candidate, [[maybe_unused]] std::string* note = nullptr)
{
	std::error_code ec;
	if (fs::is_regular_file (candidate, ec))
	{
#ifdef _WIN32
		if (!IsAcceptedProgram (candidate))
		{
			if (note)
				*note = " (rejected: only .exe and .com programs are accepted)";
			return {};
		}
#endif
		return candidate;
	}
	if (fs::is_directory (candidate, ec))
		for (const std::string& name : ExecutableNames ())
		{
			const fs::path inside = candidate / name;
			if (fs::is_regular_file (inside, ec))
				return inside;
		}
	return {};
}

bool IsIdentifier (const std::string& name)
{
	if (name.empty ())
		return false;
	auto alpha = [] (char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
	auto digit = [] (char c) { return c >= '0' && c <= '9'; };
	std::size_t i = 0;
	if (name[0] == '$')
		i = 1;
	if (i >= name.size () || !alpha (name[i]))
		return false;
	for (++i; i < name.size (); ++i)
		if (!alpha (name[i]) && !digit (name[i]))
			return false;
	return true;
}

// Les lignes qui disent POURQUOI : diagnostics ERROR, verdict final d'OpenSCAD
// sur l'objet de premier niveau, refus d'analyse. A defaut, la derniere ligne.
std::string SummarizeLog (const std::string& log)
{
	std::istringstream in (log);
	std::string line, lastNonEmpty, summary;
	while (std::getline (in, line))
	{
		if (!line.empty () && line.back () == '\r')
			line.pop_back ();
		if (line.find_first_not_of (" \t") == std::string::npos)
			continue;
		lastNonEmpty = line;
		if (line.rfind ("ERROR", 0) == 0 || line.find ("top level object") != std::string::npos
		    || line.rfind ("Can't parse", 0) == 0)
			summary += line + "\n";
	}
	if (summary.empty ())
		summary = lastNonEmpty;
	while (!summary.empty () && summary.back () == '\n')
		summary.pop_back ();
	return summary;
}

// Grammaire admise pour la valeur d'un -D : un LITTERAL OpenSCAD.
//   valeur  := nombre | true | false | undef | chaine | vecteur
//   nombre  := [+-]? (chiffres [. chiffres?] | . chiffres) ([eE] [+-]? chiffres)?
//   chaine  := " (caractere imprimable | \ caractere imprimable)* "
//   vecteur := [ ] | [ valeur (, valeur)* ]     profondeur <= kMaxDepth
// OpenSCAD concatene `nom=valeur;` au source : toute autre forme pourrait
// terminer l'affectation et ajouter des instructions (import(), surface()...).
class LiteralParser
{
public:
	explicit LiteralParser (const std::string& text) : m_s (text) {}

	bool Parse ()
	{
		SkipSpace ();
		if (!Value (0))
			return false;
		SkipSpace ();
		return m_i == m_s.size ();
	}

private:
	static constexpr int kMaxDepth = 32;

	static bool Digit (char c) { return c >= '0' && c <= '9'; }
	static bool IdentifierChar (char c)
	{
		return Digit (c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
	}
	char Peek () const { return m_i < m_s.size () ? m_s[m_i] : '\0'; }
	void SkipSpace ()
	{
		while (m_i < m_s.size () && (m_s[m_i] == ' ' || m_s[m_i] == '\t'))
			++m_i;
	}

	bool Value (int depth)
	{
		const char c = Peek ();
		if (c == '"')
			return String ();
		if (c == '[')
			return Vector (depth);
		if (c == '+' || c == '-' || c == '.' || Digit (c))
			return Number ();
		return Keyword ();
	}

	std::size_t Digits ()
	{
		const std::size_t start = m_i;
		while (Digit (Peek ()))
			++m_i;
		return m_i - start;
	}

	bool Number ()
	{
		if (Peek () == '+' || Peek () == '-')
			++m_i;
		std::size_t digits = Digits ();
		if (Peek () == '.')
		{
			++m_i;
			digits += Digits ();
		}
		if (digits == 0)
			return false;
		if (Peek () == 'e' || Peek () == 'E')
		{
			++m_i;
			if (Peek () == '+' || Peek () == '-')
				++m_i;
			if (Digits () == 0)
				return false;
		}
		return true;
	}

	bool Keyword ()
	{
		for (const char* word : { "true", "false", "undef" })
		{
			const std::string w (word);
			if (m_s.compare (m_i, w.size (), w) == 0 && !IdentifierChar (m_s.size () > m_i + w.size () ? m_s[m_i + w.size ()] : ' '))
			{
				m_i += w.size ();
				return true;
			}
		}
		return false;
	}

	bool String ()
	{
		++m_i;
		while (m_i < m_s.size ())
		{
			const unsigned char c = static_cast<unsigned char> (m_s[m_i]);
			if (c == '"')
			{
				++m_i;
				return true;
			}
			if (c < 0x20 || c == 0x7f)
				return false;
			if (c == '\\')
			{
				if (m_i + 1 >= m_s.size () || static_cast<unsigned char> (m_s[m_i + 1]) < 0x20)
					return false;
				m_i += 2;
				continue;
			}
			++m_i;
		}
		return false;
	}

	bool Vector (int depth)
	{
		if (depth >= kMaxDepth)
			return false;
		++m_i;
		SkipSpace ();
		if (Peek () == ']')
		{
			++m_i;
			return true;
		}
		for (;;)
		{
			if (!Value (depth + 1))
				return false;
			SkipSpace ();
			if (Peek () == ',')
			{
				++m_i;
				SkipSpace ();
				continue;
			}
			if (Peek () == ']')
			{
				++m_i;
				return true;
			}
			return false;
		}
	}

	const std::string& m_s;
	std::size_t        m_i = 0;
};

std::string Join (const std::vector<std::string>& items)
{
	std::string out;
	for (const std::string& s : items)
		out += "\n  - " + s;
	return out;
}

} // namespace

bool IsLiteralValue (const std::string& value)
{
	return LiteralParser (value).Parse ();
}

const char* ImportStatusName (ImportStatus status)
{
	switch (status)
	{
		case ImportStatus::Ok:                 return "ok";
		case ImportStatus::InvalidInput:       return "invalid input";
		case ImportStatus::ExecutableNotFound: return "OpenSCAD not found";
		case ImportStatus::LaunchFailed:       return "OpenSCAD could not be started";
		case ImportStatus::OpenScadError:      return "OpenSCAD reported an error";
		case ImportStatus::EmptyResult:        return "empty result";
		case ImportStatus::ReadFailed:         return "OpenSCAD output unreadable";
		case ImportStatus::Cancelled:          return "cancelled";
		case ImportStatus::TimedOut:           return "timed out";
		case ImportStatus::OutputLimitExceeded: return "OpenSCAD output too large";
	}
	return "unknown";
}

const char* DirectoryStateName (DirectoryState state)
{
	switch (state)
	{
		case DirectoryState::Empty:         return "empty";
		case DirectoryState::Relative:      return "relative";
		case DirectoryState::Missing:       return "missing";
		case DirectoryState::NotADirectory: return "not a directory";
		case DirectoryState::NoExecutable:  return "no executable";
		case DirectoryState::Valid:         return "valid";
	}
	return "unknown";
}

DirectoryState CheckOpenScadDirectory (const fs::path& directory)
{
	if (directory.empty ())
		return DirectoryState::Empty;
	if (!directory.is_absolute ())
		return DirectoryState::Relative;
	std::error_code ec;
	const fs::file_status status = fs::status (directory, ec);
	if (!fs::exists (status))
		return DirectoryState::Missing;
	if (!fs::is_directory (status))
		return DirectoryState::NotADirectory;

	const fs::file_status exe = fs::status (directory / kDirectoryExecutableName, ec);
	if (!fs::is_regular_file (exe))
		return DirectoryState::NoExecutable;
#ifndef _WIN32
	// access et non les bits : un others_exec seul ne rend pas le fichier
	// executable pour son proprietaire.
	if (access ((directory / kDirectoryExecutableName).c_str (), X_OK) != 0)
		return DirectoryState::NoExecutable;
#endif
	return DirectoryState::Valid;
}

fs::path OpenScadInDirectory (const fs::path& directory)
{
	if (CheckOpenScadDirectory (directory) != DirectoryState::Valid)
		return {};
	return directory / kDirectoryExecutableName;
}

SettingMigration MigrateOpenScadSetting (const fs::path& stored)
{
	SettingMigration out;
	if (stored.empty ())
		return out;
	const std::string shown = "former OpenSCAD setting \"" + stored.u8string () + "\"";
	if (!stored.is_absolute ())
	{
		out.note = shown + " is a relative path; it was not converted";
		return out;
	}

	std::error_code ec;
	if (fs::is_directory (stored, ec))
	{
		out.directory = stored;
		out.converted = CheckOpenScadDirectory (stored) == DirectoryState::Valid;
		return out;
	}

	const fs::path parent = stored.parent_path ();
	const DirectoryState state = CheckOpenScadDirectory (parent);
	if (state == DirectoryState::Valid)
	{
		out.directory = parent;
		out.converted = true;
		return out;
	}
	out.note = shown + " could not be converted (its directory: " + DirectoryStateName (state) + ")";
	return out;
}

ExecutableLookup FindOpenScad (const ExecutableSearch& search)
{
	ExecutableLookup lookup;

	auto tryExplicit = [&] (const fs::path& candidate, ExecutableSource source, const std::string& label) {
		std::string note;
		lookup.executable = ResolveCandidate (candidate, &note);
		lookup.searched.push_back (label + ": " + candidate.u8string () + note);
		if (!lookup.executable.empty ())
			lookup.source = source;
	};

	if (!search.configured.empty ())
	{
		tryExplicit (search.configured, ExecutableSource::Configured, "setting");
		return lookup;
	}

	const std::string fromEnvironment = process::GetEnvironmentVariableUtf8 (kEnvironmentVariable);
	if (!fromEnvironment.empty ())
	{
		try
		{
			tryExplicit (fs::u8path (fromEnvironment), ExecutableSource::Environment,
			             std::string ("environment variable ") + kEnvironmentVariable);
		}
		catch (const std::exception&)
		{
			lookup.searched.push_back (std::string ("environment variable ") + kEnvironmentVariable
			                           + ": not a valid path");
		}
		return lookup;
	}

	for (const std::string& name : ExecutableNames ())
	{
		lookup.searched.push_back ("PATH: " + name);
		const fs::path found = process::FindInSystemPath (name);
		if (!found.empty ())
		{
			lookup.executable = found;
			lookup.source     = ExecutableSource::SystemPath;
			return lookup;
		}
	}

#ifdef _WIN32
	const std::string programFiles = process::GetEnvironmentVariableUtf8 ("ProgramFiles");
	if (!programFiles.empty ())
	{
		const fs::path dir = fs::u8path (programFiles) / "OpenSCAD";
		lookup.searched.push_back ("default install: " + dir.u8string ());
		lookup.executable = ResolveCandidate (dir);
		if (!lookup.executable.empty ())
		{
			lookup.source = ExecutableSource::ProgramFiles;
			return lookup;
		}
	}
#endif

	for (const fs::path& fallback : search.fallbacks)
	{
		if (fallback.empty ())
			continue;
		lookup.searched.push_back ("build default: " + fallback.u8string ());
		lookup.executable = ResolveCandidate (fallback);
		if (!lookup.executable.empty ())
		{
			lookup.source = ExecutableSource::Fallback;
			return lookup;
		}
	}
	return lookup;
}

ImportResult ImportScad (VMeshes& out, const fs::path& scadFile, const ImportOptions& options)
{
	ImportResult result;
	bool reading = false;
	try
	{
		std::error_code ec;
		if (!fs::is_regular_file (scadFile, ec))
		{
			result.status  = ImportStatus::InvalidInput;
			result.message = "file not found: " + scadFile.u8string ();
			return result;
		}
		for (const Define& define : options.defines)
		{
			if (!IsIdentifier (define.name))
			{
				result.status  = ImportStatus::InvalidInput;
				result.message = "invalid OpenSCAD parameter name: '" + define.name + "'";
				return result;
			}
			if (!IsLiteralValue (define.value))
			{
				result.status  = ImportStatus::InvalidInput;
				result.message = "invalid value for OpenSCAD parameter '" + define.name
				               + "': only literals are accepted (number, true, false, undef, "
				                 "quoted string, vector of these)";
				return result;
			}
		}

		const ExecutableLookup lookup = FindOpenScad (options.search);
		if (lookup.executable.empty ())
		{
			result.status  = ImportStatus::ExecutableNotFound;
			result.message = "OpenSCAD executable not found. Searched:" + Join (lookup.searched);
			return result;
		}
		result.executable = lookup.executable;

		const fs::path source = fs::absolute (scadFile);
		process::TemporaryDirectory scratch ("scad", options.temporaryRoot);
		const fs::path offFile = scratch.path () / "model.off";

		process::ProcessRequest request;
		request.executable       = lookup.executable;
		request.workingDirectory = source.parent_path ();
		request.timeout          = options.timeout;
		request.shouldCancel     = options.shouldCancel;
		request.temporaryRoot    = options.temporaryRoot;
		request.arguments        = { "-o", offFile.u8string () };
		for (const Define& define : options.defines)
		{
			request.arguments.push_back ("-D");
			request.arguments.push_back (define.name + "=" + define.value);
		}
		request.arguments.push_back (source.u8string ());

		const process::ProcessResult run = process::RunProcess (request);
		result.exitCode = run.exitCode;
		result.log      = run.output;

		switch (run.outcome)
		{
			case process::ProcessOutcome::LaunchFailed:
				result.status  = ImportStatus::LaunchFailed;
				result.message = run.error + "\n(" + lookup.executable.u8string () + ")";
				return result;
			case process::ProcessOutcome::Cancelled:
				result.status  = ImportStatus::Cancelled;
				result.message = "OpenSCAD rendering cancelled";
				return result;
			case process::ProcessOutcome::TimedOut:
				result.status  = ImportStatus::TimedOut;
				result.message = "OpenSCAD rendering exceeded "
				               + std::to_string (options.timeout.count ()) + " ms";
				return result;
			case process::ProcessOutcome::OutputLimitExceeded:
				result.status  = ImportStatus::OutputLimitExceeded;
				result.message = "OpenSCAD output exceeded "
				               + std::to_string (request.maxCapturedBytes) + " bytes";
				return result;
			case process::ProcessOutcome::Exited:
				break;
		}

		if (run.exitCode != 0)
		{
			result.status = run.output.find ("top level object is empty") != std::string::npos
			              ? ImportStatus::EmptyResult : ImportStatus::OpenScadError;
			result.message = SummarizeLog (run.output);
			if (result.message.empty ())
				result.message = "OpenSCAD exited with code " + std::to_string (run.exitCode);
			return result;
		}
		if (!fs::is_regular_file (offFile, ec))
		{
			result.status  = ImportStatus::OpenScadError;
			result.message = "OpenSCAD succeeded but produced no file";
			return result;
		}

		// Chemin dans l'encodage natif etroit : celui qu'attend fopen.
		reading = true;
		VMeshes loaded;
		if (!VMeshesIO::load (loaded, offFile.string ().c_str ()))
		{
			result.status  = ImportStatus::ReadFailed;
			result.message = "cannot read the OFF file produced by OpenSCAD";
			return result;
		}
		if (loaded.GetNFaces () == 0)
		{
			result.status  = ImportStatus::EmptyResult;
			result.message = "OpenSCAD produced a mesh without faces";
			return result;
		}

		// Sans passage par CGAL, OpenSCAD ecrit des n-gones, concaves compris
		// (faces d'un linear_extrude) : le resultat est rendu en triangles.
		for (Mesh* mesh : loaded.GetMeshes ())
			mesh->Triangulate ();

		for (Mesh* mesh : loaded.GetMeshes ())
			out.AddMesh (mesh);
		loaded.GetMeshes ().clear ();

		result.status = ImportStatus::Ok;
		return result;
	}
	catch (const std::exception& e)
	{
		result.status  = reading ? ImportStatus::ReadFailed : ImportStatus::LaunchFailed;
		result.message = e.what ();
		return result;
	}
}

} // namespace scad
} // namespace cgmesh
