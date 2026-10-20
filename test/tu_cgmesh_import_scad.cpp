// ===========================================================================
//  Import OpenSCAD (.scad) par l'executable OpenSCAD
// ===========================================================================
//
// Les tests qui EVALUENT un .scad exigent OpenSCAD. Ils sont sautes
// (GTEST_SKIP) quand l'executable est introuvable par les voies de
// FindOpenScad, plus le repertoire EXTERN_OPENSCAD transmis par CMake
// (CG_TEST_OPENSCAD_HINT). Les tests du lanceur de processus, de la
// recherche de l'executable et des entrees invalides n'en dependent pas.
//
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "../src/cgmesh/external_process.h"
#include "../src/cgmesh/import_scad.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "../src/cgmesh/vmeshes.h"
#include "scad_test_support.h"

namespace fs = std::filesystem;
using namespace cgmesh;

namespace {

const fs::path kData = fs::path ("test") / "data" / "scad";

// `root` : repertoire temporaire dedie au test, pour que le controle de
// nettoyage ne depende pas des autres utilisateurs du repertoire du systeme.
scad::ImportOptions TestOptions (const fs::path& root = {})
{
	scad::ImportOptions options;
	options.search        = scad_test::Search ();
	options.temporaryRoot = root;
	return options;
}


struct ScadBox
{
	std::array<float, 3> min { std::numeric_limits<float>::max (),  std::numeric_limits<float>::max (),  std::numeric_limits<float>::max () };
	std::array<float, 3> max { -std::numeric_limits<float>::max (), -std::numeric_limits<float>::max (), -std::numeric_limits<float>::max () };
};

ScadBox MeasureBox (VMeshes& vm)
{
	ScadBox box;
	for (Mesh* mesh : vm.GetMeshes ())
		for (unsigned int i = 0; i < mesh->GetNVertices (); ++i)
		{
			float v[3] = {0.f, 0.f, 0.f};
			mesh->GetVertex (i, v);
			for (int k = 0; k < 3; ++k)
			{
				box.min[k] = std::min (box.min[k], v[k]);
				box.max[k] = std::max (box.max[k], v[k]);
			}
		}
	return box;
}

void ExpectBox (const ScadBox& box, std::array<float, 3> lo, std::array<float, 3> hi, float tol = 1e-4f)
{
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_NEAR (box.min[k], lo[k], tol) << "axe " << k;
		EXPECT_NEAR (box.max[k], hi[k], tol) << "axe " << k;
	}
}

// Entrees d'un repertoire temporaire dedie : un import ne doit en laisser
// aucune derriere lui.
std::set<std::string> TemporaryEntries (const fs::path& root)
{
	std::set<std::string> names;
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator (root, ec))
		names.insert (entry.path ().filename ().u8string ());
	return names;
}

} // namespace

// ---------------------------------------------------------------------------
//  Regle CG_REQUIRE_OPENSCAD (sans OpenSCAD ni environnement)
// ---------------------------------------------------------------------------

TEST(ScadTestSupport, require_variable_turns_a_missing_openscad_into_a_failure)
{
	using scad_test::Action;
	using scad_test::Decide;

	// OpenSCAD trouve : execute, quelle que soit la variable.
	for (const char* v : { "", "0", "1", "true" })
		EXPECT_EQ (Decide (true, v), Action::Run) << "'" << v << "'";

	// Introuvable, non exige : saute.
	EXPECT_EQ (Decide (false, ""), Action::Skip);
	EXPECT_EQ (Decide (false, "0"), Action::Skip);

	// Introuvable, exige : echoue. Toute valeur non vide autre que "0" exige.
	for (const char* v : { "1", "true", "yes", "2", "00", " " })
		EXPECT_EQ (Decide (false, v), Action::Fail) << "'" << v << "'";
}

// ---------------------------------------------------------------------------
//  Lanceur de processus (sans OpenSCAD)
// ---------------------------------------------------------------------------

TEST(ExternalProcess, missing_program_is_a_launch_failure)
{
	process::ProcessRequest request;
	request.executable = fs::temp_directory_path () / "cg-no-such-program-4f1e" / "prog";
	const process::ProcessResult r = process::RunProcess (request);
	EXPECT_EQ (r.outcome, process::ProcessOutcome::LaunchFailed);
	EXPECT_NE (r.error.find ("not found"), std::string::npos) << r.error;
}

namespace {

// Interpreteur du systeme, pour les tests du lanceur : sortie, code de retour,
// repertoire de travail.
struct Shell
{
	fs::path                 program;
	std::vector<std::string> prefix;
};

Shell SystemShell ()
{
#ifdef _WIN32
	const std::string comspec = process::GetEnvironmentVariableUtf8 ("ComSpec");
	return { fs::u8path (comspec.empty () ? "C:\\Windows\\System32\\cmd.exe" : comspec), { "/d", "/c" } };
#else
	return { "/bin/sh", { "-c" } };
#endif
}

} // namespace

TEST(ExternalProcess, captures_output_and_exit_code_in_working_directory)
{
	const Shell shell = SystemShell ();
	ASSERT_TRUE (fs::is_regular_file (shell.program)) << shell.program.u8string ();

	process::TemporaryDirectory dir ("process-test");
	const fs::path cwd = dir.path () / fs::u8path (u8"r\u00e9pertoire avec espaces");
	fs::create_directory (cwd);

	process::ProcessRequest request;
	request.executable       = shell.program;
	request.arguments        = shell.prefix;
	request.workingDirectory = cwd;
#ifdef _WIN32
	request.arguments.push_back ("echo marker-out & echo marker-err 1>&2 & cd & exit 3");
#else
	request.arguments.push_back ("echo marker-out; echo marker-err 1>&2; pwd; exit 3");
#endif
	const process::ProcessResult r = process::RunProcess (request);
	ASSERT_EQ (r.outcome, process::ProcessOutcome::Exited) << r.error;
	EXPECT_EQ (r.exitCode, 3);
	EXPECT_NE (r.output.find ("marker-out"), std::string::npos) << r.output;
	EXPECT_NE (r.output.find ("marker-err"), std::string::npos) << r.output;
	EXPECT_NE (r.output.find ("avec espaces"), std::string::npos) << r.output;
}

namespace {

// Le petit-enfant (ping / sleep, lance par le shell) est-il encore vivant ?
// Windows : il tient held.txt ouvert (redirection), donc le fichier ne peut
// pas etre supprime tant qu'il vit. POSIX : son pid est ecrit par le shell ;
// un zombie compte comme mort.
struct Grandchild
{
	fs::path dir;

	std::string Command () const
	{
#ifdef _WIN32
		return "ping -n 30 127.0.0.1 > held.txt";
#else
		return "sleep 30 & echo $! > grandchild.pid; wait";
#endif
	}

	bool Alive () const
	{
#ifdef _WIN32
		std::error_code ec;
		if (!fs::exists (dir / "held.txt", ec))
			return false;
		return !fs::remove (dir / "held.txt", ec);
#else
		std::ifstream in (dir / "grandchild.pid");
		long pid = 0;
		if (!(in >> pid) || pid <= 0)
			return false;
		std::ifstream stat ("/proc/" + std::to_string (pid) + "/stat");
		std::string line;
		if (!std::getline (stat, line))
			return false;
		const std::size_t close = line.rfind (')');
		return close != std::string::npos && close + 2 < line.size () && line[close + 2] != 'Z';
#endif
	}

	bool DiesWithin (std::chrono::milliseconds limit) const
	{
		const auto end = std::chrono::steady_clock::now () + limit;
		while (Alive ())
		{
			if (std::chrono::steady_clock::now () > end)
				return false;
			std::this_thread::sleep_for (std::chrono::milliseconds (20));
		}
		return true;
	}
};

} // namespace

// Le delai et l'annulation tuent l'arbre entier : le petit-enfant lance par le
// shell meurt aussi. Le detecteur de vie est valide pendant l'attente : il
// doit voir le petit-enfant vivant avant l'annulation.
TEST(ExternalProcess, timeout_and_cancellation_kill_the_process_tree)
{
	const Shell shell = SystemShell ();
	const auto t0 = std::chrono::steady_clock::now ();

	for (const bool useTimeout : { true, false })
	{
		process::TemporaryDirectory dir ("process-test");
		const Grandchild grandchild { dir.path () };

		process::ProcessRequest request;
		request.executable       = shell.program;
		request.arguments        = shell.prefix;
		request.workingDirectory = dir.path ();
		request.arguments.push_back (grandchild.Command ());

		bool seenAlive = false;
		const auto start = std::chrono::steady_clock::now ();
		auto watch = [&] () {
			if (!seenAlive && std::chrono::steady_clock::now () - start > std::chrono::milliseconds (300))
				seenAlive = grandchild.Alive ();
			return seenAlive;
		};
		if (useTimeout)
		{
			request.timeout      = std::chrono::milliseconds (2000);
			request.shouldCancel = [&] () { watch (); return false; };
		}
		else
			request.shouldCancel = watch;

		const process::ProcessResult r = process::RunProcess (request);
		EXPECT_EQ (r.outcome, useTimeout ? process::ProcessOutcome::TimedOut : process::ProcessOutcome::Cancelled);
		EXPECT_TRUE (seenAlive) << "detecteur : petit-enfant jamais vu vivant";
		EXPECT_TRUE (grandchild.DiesWithin (std::chrono::milliseconds (2000))) << "petit-enfant survivant";
	}

	EXPECT_LT (std::chrono::steady_clock::now () - t0, std::chrono::seconds (15));
}

// Une sortie sans fin est bornee sur disque : le programme est tue au-dela
// du seuil.
TEST(ExternalProcess, output_limit_kills_a_chatty_process)
{
	const Shell shell = SystemShell ();
	process::ProcessRequest request;
	request.executable = shell.program;
	request.arguments  = shell.prefix;
#ifdef _WIN32
	request.arguments.push_back ("for /l %i in (0,0,1) do @echo 0123456789012345678901234567890123456789");
#else
	request.arguments.push_back ("while :; do echo 0123456789012345678901234567890123456789; done");
#endif
	request.maxCapturedBytes = 64 * 1024;
	const auto t0 = std::chrono::steady_clock::now ();
	const process::ProcessResult r = process::RunProcess (request);
	EXPECT_EQ (r.outcome, process::ProcessOutcome::OutputLimitExceeded);
	EXPECT_STREQ (process::ProcessOutcomeName (r.outcome), "output limit exceeded");
	EXPECT_LT (std::chrono::steady_clock::now () - t0, std::chrono::seconds (10));
	EXPECT_LE (r.output.size (), request.maxOutputBytes + 16);
}

#ifndef _WIN32
// Les arguments arrivent intacts, metacaracteres du shell compris : ils ne
// passent par aucun interpreteur.
TEST(ExternalProcess, arguments_are_passed_verbatim)
{
	process::ProcessRequest request;
	request.executable = "/bin/sh";
	request.arguments  = { "-c", "printf '<%s>' \"$@\"", "sh", "a b", "$(echo injected)", "x;y", "\"q\"" };
	const process::ProcessResult r = process::RunProcess (request);
	ASSERT_EQ (r.outcome, process::ProcessOutcome::Exited) << r.error;
	EXPECT_EQ (r.output, "<a b><$(echo injected)><x;y><\"q\">");
}
#endif

#ifdef _WIN32
// La ligne de commande construite se relit, par les regles de
// CommandLineToArgvW, en exactement les arguments d'origine.
TEST(ExternalProcess, windows_quoting_round_trips_through_CommandLineToArgvW)
{
	const std::vector<std::wstring> arguments = {
		L"plain", L"with space", L"", L"quote\"inside", L"trailing\\", L"trailing space\\",
		L"C:\\dir with space\\file.scad", L"\\\\server\\share\\", L"a\\\\\"b", L"&|<>^%", L"-D", L"name=\"text\"",
	};
	std::wstring commandLine = L"prog.exe";
	for (const std::wstring& a : arguments)
		commandLine += L' ' + process::QuoteWindowsArgument (a);

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW (commandLine.c_str (), &argc);
	ASSERT_NE (argv, nullptr);
	ASSERT_EQ (static_cast<std::size_t> (argc), arguments.size () + 1);
	for (std::size_t i = 0; i < arguments.size (); ++i)
		EXPECT_EQ (std::wstring (argv[i + 1]), arguments[i]) << "argument " << i;
	LocalFree (argv);
}
#endif

// ---------------------------------------------------------------------------
//  Recherche de l'executable et entrees invalides (sans OpenSCAD)
// ---------------------------------------------------------------------------

TEST(ImportScad, configured_executable_that_does_not_exist_is_reported_not_bypassed)
{
	VMeshes vm;
	scad::ImportOptions options = TestOptions ();
	options.search.configured = fs::temp_directory_path () / "cg-no-such-openscad-8b2c" / "openscad.exe";

	const scad::ImportResult r = scad::ImportScad (vm, kData / "cube_minus_sphere.scad", options);
	EXPECT_EQ (r.status, scad::ImportStatus::ExecutableNotFound);
	EXPECT_NE (r.message.find ("cg-no-such-openscad-8b2c"), std::string::npos) << r.message;
	EXPECT_NE (r.message.find ("setting"), std::string::npos) << r.message;
	EXPECT_EQ (vm.GetNMeshes (), 0u);
}

TEST(ImportScad, missing_source_file_and_invalid_parameter_name_are_invalid_input)
{
	VMeshes vm;
	EXPECT_EQ (scad::ImportScad (vm, kData / "does_not_exist.scad", TestOptions ()).status,
	           scad::ImportStatus::InvalidInput);

	for (const char* bad : { "", "1x", "a b", "-o", "x=1", "$" })
	{
		scad::ImportOptions options = TestOptions ();
		options.defines.push_back ({ bad, "1" });
		const scad::ImportResult r = scad::ImportScad (vm, kData / "parametric.scad", options);
		EXPECT_EQ (r.status, scad::ImportStatus::InvalidInput) << "'" << bad << "'";
	}
	EXPECT_EQ (vm.GetNMeshes (), 0u);
}

// La valeur d'un -D est concatenee au source par OpenSCAD (`nom=valeur;`) :
// seule une valeur LITTERALE est admise, sans quoi `1; cube(3)` ajouterait
// une instruction.
TEST(ImportScad, define_values_must_be_literals)
{
	for (const char* good : { "10", "-2.5e3", "+.5", "1.", "1E-7", "true", "false", "undef",
	                          "\"a;b\"", "\"esc \\\" q\\\\\"", "\"\"", "[]", "[1,[2,3],\"x\"]",
	                          " [ 1 , [ true , undef ] ] ", "[[[[[1]]]]]" })
		EXPECT_TRUE (scad::IsLiteralValue (good)) << good;

	std::string deep;
	for (int i = 0; i < 40; ++i) deep += "[";
	deep += "1";
	for (int i = 0; i < 40; ++i) deep += "]";

	for (const std::string& bad : { std::string ("1; translate([100,0,0]) cube(3)"), std::string ("1;"),
	                                std::string (""), std::string ("  "), std::string ("a"), std::string ("1+1"),
	                                std::string ("-x"), std::string ("\"unterminated"), std::string ("\"a\" ; cube(1)"),
	                                std::string ("\"a\"\"b\""), std::string ("[1,2"), std::string ("[1,,2]"),
	                                std::string ("[1,]"), std::string ("1 2"), std::string ("import(\"x.stl\")"),
	                                std::string ("[0:10]"), std::string ("\"a\nb\""), std::string ("1e"), std::string ("."),
	                                deep })
		EXPECT_FALSE (scad::IsLiteralValue (bad)) << bad;

	VMeshes vm;
	scad::ImportOptions options = TestOptions ();
	options.defines.push_back ({ "size", "1; translate([100,0,0]) cube(3)" });
	const scad::ImportResult r = scad::ImportScad (vm, kData / "parametric.scad", options);
	EXPECT_EQ (r.status, scad::ImportStatus::InvalidInput);
	EXPECT_NE (r.message.find ("'size'"), std::string::npos) << r.message;
	EXPECT_NE (r.message.find ("literal"), std::string::npos) << r.message;
	EXPECT_EQ (vm.GetNMeshes (), 0u);
}

#ifdef _WIN32
// Un .bat ou un .cmd passerait par cmd.exe, dont l'echappement differe de
// celui de CommandLineToArgvW : seuls .exe et .com sont admis.
TEST(ImportScad, windows_scripts_are_not_accepted_as_the_executable)
{
	process::TemporaryDirectory dir ("scad-test");
	for (const char* name : { "openscad.bat", "openscad.cmd" })
		std::ofstream (dir.path () / name) << "@echo off\n";
	std::ofstream (dir.path () / "OpenSCAD.BAT") << "@echo off\n";

	scad::ExecutableSearch search;
	search.configured = dir.path () / "openscad.bat";
	scad::ExecutableLookup lookup = scad::FindOpenScad (search);
	EXPECT_TRUE (lookup.executable.empty ()) << lookup.executable.u8string ();
	ASSERT_FALSE (lookup.searched.empty ());
	EXPECT_NE (lookup.searched.back ().find ("rejected"), std::string::npos) << lookup.searched.back ();

	search.configured = dir.path ();
	EXPECT_TRUE (scad::FindOpenScad (search).executable.empty ());

	// Temoin : le meme repertoire avec un openscad.exe est accepte.
	std::ofstream (dir.path () / "openscad.exe") << "MZ";
	EXPECT_EQ (scad::FindOpenScad (search).executable, dir.path () / "openscad.exe");
}
#endif

// ---------------------------------------------------------------------------
//  Chemins d'erreur et de recherche (sans OpenSCAD)
// ---------------------------------------------------------------------------

namespace {

// Pose (ou retire, value == nullptr) une variable d'environnement le temps d'un
// test, et rend la valeur d'origine a la destruction.
class ScopedEnv
{
public:
	ScopedEnv (const char* name, const char* value) : m_name (name)
	{
		const std::string old = process::GetEnvironmentVariableUtf8 (name);
		m_had = !old.empty ();
		m_old = old;
		Set (value);
	}
	~ScopedEnv () { Set (m_had ? m_old.c_str () : nullptr); }
	ScopedEnv (const ScopedEnv&) = delete;
	ScopedEnv& operator= (const ScopedEnv&) = delete;

private:
	void Set (const char* value)
	{
#ifdef _WIN32
		const std::wstring name (m_name.begin (), m_name.end ());
		const std::wstring wide = value ? fs::u8path (value).wstring () : std::wstring ();
		SetEnvironmentVariableW (name.c_str (), value ? wide.c_str () : nullptr);
#else
		if (value)
			setenv (m_name.c_str (), value, 1);
		else
			unsetenv (m_name.c_str ());
#endif
	}

	std::string m_name;
	std::string m_old;
	bool        m_had = false;
};

// Repertoire contenant un faux executable OpenSCAD (jamais lance).
fs::path FakeOpenScadDirectory (const fs::path& parent, const char* name)
{
	const fs::path dir = parent / name;
	fs::create_directory (dir);
	std::ofstream (dir / scad::kDirectoryExecutableName) << "MZ";
	return dir;
}

} // namespace

TEST(ExternalProcess, working_directory_must_exist)
{
	const Shell shell = SystemShell ();
	process::ProcessRequest request;
	request.executable       = shell.program;
	request.workingDirectory = fs::temp_directory_path () / "cg-no-such-dir-77c1";
	const process::ProcessResult r = process::RunProcess (request);
	EXPECT_EQ (r.outcome, process::ProcessOutcome::LaunchFailed);
	EXPECT_NE (r.error.find ("working directory"), std::string::npos) << r.error;
}

// Un fichier qui n'est pas un programme : le systeme refuse de l'executer, et
// l'echec remonte comme tel (tube de controle sous POSIX, CreateProcessW sous
// Windows).
TEST(ExternalProcess, a_file_that_is_not_a_program_is_a_launch_failure)
{
	process::TemporaryDirectory dir ("process-test");
#ifdef _WIN32
	const fs::path file = dir.path () / "not-a-program.exe";
#else
	const fs::path file = dir.path () / "not-a-program";
#endif
	std::ofstream (file) << "plain text\n";
	process::ProcessRequest request;
	request.executable = file;
	const process::ProcessResult r = process::RunProcess (request);
	EXPECT_EQ (r.outcome, process::ProcessOutcome::LaunchFailed);
	EXPECT_FALSE (r.error.empty ());
}

// Au-dela de maxOutputBytes, seule la fin de la sortie est rendue, marquee.
TEST(ExternalProcess, long_output_keeps_its_tail)
{
	const Shell shell = SystemShell ();
	process::ProcessRequest request;
	request.executable     = shell.program;
	request.arguments      = shell.prefix;
	request.maxOutputBytes = 100;
#ifdef _WIN32
	request.arguments.push_back ("(for /l %i in (1,1,60) do @echo 0123456789012345678901234567890123456789) & echo END-MARKER");
#else
	request.arguments.push_back ("i=0; while [ $i -lt 60 ]; do echo 0123456789012345678901234567890123456789; i=$((i+1)); done; echo END-MARKER");
#endif
	const process::ProcessResult r = process::RunProcess (request);
	ASSERT_EQ (r.outcome, process::ProcessOutcome::Exited) << r.error;
	EXPECT_EQ (r.output.rfind ("[...]\n", 0), 0u) << r.output.substr (0, 20);
	EXPECT_NE (r.output.find ("END-MARKER"), std::string::npos);
	EXPECT_LE (r.output.size (), request.maxOutputBytes + 6);
}

TEST(ExternalProcess, every_outcome_and_status_has_a_name)
{
	std::set<std::string> names;
	for (process::ProcessOutcome o : { process::ProcessOutcome::Exited, process::ProcessOutcome::LaunchFailed,
	                                   process::ProcessOutcome::TimedOut, process::ProcessOutcome::Cancelled,
	                                   process::ProcessOutcome::OutputLimitExceeded })
		names.insert (process::ProcessOutcomeName (o));
	EXPECT_EQ (names.size (), 5u);

	names.clear ();
	for (scad::ImportStatus s : { scad::ImportStatus::Ok, scad::ImportStatus::InvalidInput,
	                              scad::ImportStatus::ExecutableNotFound, scad::ImportStatus::LaunchFailed,
	                              scad::ImportStatus::OpenScadError, scad::ImportStatus::EmptyResult,
	                              scad::ImportStatus::ReadFailed, scad::ImportStatus::Cancelled,
	                              scad::ImportStatus::TimedOut, scad::ImportStatus::OutputLimitExceeded })
		names.insert (scad::ImportStatusName (s));
	EXPECT_EQ (names.size (), 10u);

	names.clear ();
	for (scad::DirectoryState s : { scad::DirectoryState::Empty, scad::DirectoryState::Relative,
	                                scad::DirectoryState::Missing, scad::DirectoryState::NotADirectory,
	                                scad::DirectoryState::NoExecutable, scad::DirectoryState::Valid })
		names.insert (scad::DirectoryStateName (s));
	EXPECT_EQ (names.size (), 6u);
	EXPECT_EQ (names.count ("unknown"), 0u);
}

// Ordre de recherche generique : variable d'environnement (exclusive), PATH,
// puis recours. Les variables sont posees le temps du test.
TEST(ImportScad, generic_lookup_follows_environment_then_path_then_fallbacks)
{
	process::TemporaryDirectory root ("scad-test");
	const fs::path envDir      = FakeOpenScadDirectory (root.path (), "env");
	const fs::path pathDir     = FakeOpenScadDirectory (root.path (), "path");
	const fs::path fallbackDir = FakeOpenScadDirectory (root.path (), "fallback");
	const fs::path emptyDir    = root.path () / "empty";
	fs::create_directory (emptyDir);

	scad::ExecutableSearch search;
	search.fallbacks = { fs::path (), fallbackDir };

	{
		ScopedEnv env (scad::kEnvironmentVariable, envDir.u8string ().c_str ());
		const scad::ExecutableLookup l = scad::FindOpenScad (search);
		EXPECT_EQ (l.source, scad::ExecutableSource::Environment);
		EXPECT_EQ (l.executable, envDir / scad::kDirectoryExecutableName);
	}
	{
		// Exclusive : une variable qui ne mene a rien n'est pas contournee.
		ScopedEnv env (scad::kEnvironmentVariable, (root.path () / "absent").u8string ().c_str ());
		const scad::ExecutableLookup l = scad::FindOpenScad (search);
		EXPECT_TRUE (l.executable.empty ());
		ASSERT_FALSE (l.searched.empty ());
		EXPECT_NE (l.searched.back ().find ("environment variable"), std::string::npos);
	}

	ScopedEnv noEnv (scad::kEnvironmentVariable, nullptr);
	ScopedEnv noProgramFiles ("ProgramFiles", emptyDir.u8string ().c_str ());
	{
		ScopedEnv path ("PATH", pathDir.u8string ().c_str ());
		const scad::ExecutableLookup l = scad::FindOpenScad (search);
		EXPECT_EQ (l.source, scad::ExecutableSource::SystemPath);
		EXPECT_EQ (l.executable, pathDir / scad::kDirectoryExecutableName);
	}
	{
		ScopedEnv path ("PATH", emptyDir.u8string ().c_str ());
		const scad::ExecutableLookup l = scad::FindOpenScad (search);
		EXPECT_EQ (l.source, scad::ExecutableSource::Fallback);
		EXPECT_EQ (l.executable, fallbackDir / scad::kDirectoryExecutableName);
	}
	{
		ScopedEnv path ("PATH", emptyDir.u8string ().c_str ());
		const scad::ExecutableLookup l = scad::FindOpenScad (scad::ExecutableSearch ());
		EXPECT_TRUE (l.executable.empty ());
		EXPECT_EQ (l.source, scad::ExecutableSource::None);

		// Le message d'ImportScad liste les emplacements essayes.
		VMeshes vm;
		scad::ImportOptions options;
		const scad::ImportResult r = scad::ImportScad (vm, kData / "parametric.scad", options);
		EXPECT_EQ (r.status, scad::ImportStatus::ExecutableNotFound);
		EXPECT_NE (r.message.find ("PATH"), std::string::npos) << r.message;
	}
}

// Un executable trouve qui ne demarre pas : LaunchFailed, pas « introuvable ».
TEST(ImportScad, an_executable_that_cannot_start_is_a_launch_failure)
{
	process::TemporaryDirectory root ("scad-test");
	const fs::path dir = FakeOpenScadDirectory (root.path (), "broken");   // contenu "MZ", non executable
	VMeshes vm;
	scad::ImportOptions options;
	options.search.configured = dir / scad::kDirectoryExecutableName;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "parametric.scad", options);
	EXPECT_EQ (r.status, scad::ImportStatus::LaunchFailed) << r.message;
	EXPECT_NE (r.message.find (options.search.configured.u8string ()), std::string::npos) << r.message;
	EXPECT_EQ (vm.GetNMeshes (), 0u);
}

#ifndef _WIN32
// Faux OpenSCAD (script shell) : les issues qu'un vrai OpenSCAD ne produit pas
// a la demande. Le script recoit les arguments reels : $2 est le fichier OFF.
namespace {

fs::path FakeOpenScadScript (const fs::path& dir, const std::string& body)
{
	const fs::path script = dir / "openscad";
	std::ofstream (script) << "#!/bin/sh\n" << body << "\n";
	fs::permissions (script, fs::perms::owner_all);
	return script;
}

scad::ImportResult RunFake (const std::string& body)
{
	process::TemporaryDirectory dir ("scad-test");
	VMeshes vm;
	scad::ImportOptions options;
	options.search.configured = FakeOpenScadScript (dir.path (), body);
	scad::ImportResult r = scad::ImportScad (vm, kData / "parametric.scad", options);
	EXPECT_EQ (vm.GetNMeshes (), 0u);
	return r;
}

} // namespace

TEST(ImportScad, fake_openscad_outcomes_are_classified)
{
	scad::ImportResult r = RunFake ("exit 0");
	EXPECT_EQ (r.status, scad::ImportStatus::OpenScadError);
	EXPECT_NE (r.message.find ("no file"), std::string::npos) << r.message;

	r = RunFake ("printf 'not an off file' > \"$2\"");
	EXPECT_EQ (r.status, scad::ImportStatus::ReadFailed) << r.message;

	r = RunFake ("printf 'OFF\\n0 0 0\\n' > \"$2\"");
	EXPECT_EQ (r.status, scad::ImportStatus::EmptyResult) << r.message;

	// Code non nul sans diagnostic reconnu : la derniere ligne fait foi, et a
	// defaut le code de sortie.
	r = RunFake ("echo 'something went wrong' 1>&2; exit 3");
	EXPECT_EQ (r.status, scad::ImportStatus::OpenScadError);
	EXPECT_EQ (r.exitCode, 3);
	EXPECT_NE (r.message.find ("something went wrong"), std::string::npos) << r.message;

	r = RunFake ("exit 4");
	EXPECT_EQ (r.status, scad::ImportStatus::OpenScadError);
	EXPECT_NE (r.message.find ("code 4"), std::string::npos) << r.message;
}
#endif

// ---------------------------------------------------------------------------
//  Lecteur OFF : polygones et en-tetes faux (sans OpenSCAD)
// ---------------------------------------------------------------------------

namespace {

int LoadOffText (const std::string& text, Mesh& mesh)
{
	process::TemporaryDirectory dir ("off-test");
	const fs::path file = dir.path () / "m.off";
	{
		std::ofstream out (file, std::ios::binary);
		out << text;
	}
	return mesh.load (file.string ().c_str ());
}

const char* kQuadCube =
	"OFF 8 6 0\n"
	"0 0 5\n5 0 5\n5 5 5\n0 5 5\n0 5 0\n5 5 0\n5 0 0\n0 0 0\n"
	"4 0 1 2 3\n4 4 5 6 7\n4 7 6 1 0\n4 6 5 2 1\n4 5 4 3 2\n4 4 7 0 3\n";

} // namespace

TEST(OffReader, reads_polygonal_faces_and_ignores_face_colours)
{
	Mesh quads;
	ASSERT_EQ (LoadOffText (kQuadCube, quads), 0);
	EXPECT_EQ (quads.GetNVertices (), 8u);
	ASSERT_EQ (quads.GetNFaces (), 6u);
	for (unsigned int f = 0; f < quads.GetNFaces (); ++f)
		EXPECT_EQ (quads.FaceAt (f)->GetNVertices (), 4);
	EXPECT_EQ (quads.FaceAt (2)->GetVertex (0), 7);

	Mesh coloured;
	ASSERT_EQ (LoadOffText ("OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2 255 0 0\n", coloured), 0);
	EXPECT_EQ (coloured.GetNFaces (), 1u);
}

TEST(OffReader, rejects_lying_headers_and_out_of_range_indices)
{
	const char* bad[] = {
		"OFF\n4000000000 1 0\n0 0 0\n",                       // en-tete demesure
		"OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 7\n",          // indice hors bornes
		"OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n2 0 1\n",            // face de deux sommets
		"OFF\n3 2 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n",          // face manquante
		"OFF\n3 1 0\n0 0 0\n1 0\n",                            // sommet tronque
		"NOT\n3 1 0\n",
	};
	for (const char* text : bad)
	{
		Mesh mesh;
		EXPECT_NE (LoadOffText (text, mesh), 0) << text;
		EXPECT_EQ (mesh.GetNFaces (), 0u) << text;
	}
}

// ---------------------------------------------------------------------------
//  Evaluation par OpenSCAD
// ---------------------------------------------------------------------------

// Objet hors CGAL : OpenSCAD ecrit des quadrilateres, rendus en triangles.
TEST(ImportScad, primitive_cube_written_as_quads_is_triangulated_and_closed)
{
	REQUIRE_OPENSCAD ();
	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "parametric.scad", TestOptions ());
	ASSERT_TRUE (r.ok ()) << r.message << "\n" << r.log;
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	EXPECT_EQ (vm.GetNFaces (), 12u);
	const MeshValidationReport report = ValidateMesh (*vm.GetMeshes ()[0]);
	EXPECT_TRUE (report.IsValid ());
	EXPECT_NEAR (report.signedVolume, 125.0, 1e-3);
}

namespace {

// Prisme en L tel qu'OpenSCAD l'ecrit pour concave_polyhedron.scad.
const char* kLPrism =
	"OFF 12 8 0\n"
	"10 0 0\n0 0 0\n0 10 0\n4 10 0\n4 4 0\n10 4 0\n"
	"10 4 2\n4 4 2\n4 10 2\n0 10 2\n0 0 2\n10 0 2\n"
	"6 0 1 2 3 4 5\n6 6 7 8 9 10 11\n"
	"4 4 7 6 5\n4 3 8 7 4\n4 2 9 8 3\n4 1 10 9 2\n4 0 11 10 1\n4 5 6 11 0\n";

// Surface attendue : deux profils de 64 et un pourtour de 40 sur 2 de haut.
constexpr double kLPrismArea = 2 * 64.0 + 40.0 * 2.0;

Mesh FanTriangulated (const Mesh& polygons)
{
	unsigned int nTriangles = 0;
	for (unsigned int f = 0; f < polygons.GetNFaces (); ++f)
		nTriangles += static_cast<unsigned int> (polygons.FaceAt (f)->GetNVertices () - 2);
	Mesh fan (polygons.GetNVertices (), nTriangles);
	for (unsigned int i = 0; i < polygons.GetNVertices (); ++i)
	{
		float v[3] = {0.f, 0.f, 0.f};
		polygons.GetVertex (i, v);
		fan.SetVertex (i, v[0], v[1], v[2]);
	}
	unsigned int t = 0;
	for (unsigned int f = 0; f < polygons.GetNFaces (); ++f)
	{
		const auto face = polygons.FaceAt (f);
		for (int k = 1; k + 1 < face->GetNVertices (); ++k)
			fan.FaceAt (t++)->SetTriangle (static_cast<unsigned int> (face->GetVertex (0)),
			                               static_cast<unsigned int> (face->GetVertex (k)),
			                               static_cast<unsigned int> (face->GetVertex (k + 1)));
	}
	return fan;
}

} // namespace

// Temoin negatif du test suivant : l'aire signee, donc le volume, d'un eventail
// est TOUJOURS celle du polygone, meme quand l'eventail deborde de l'encoche.
// Seule l'aire non signee voit l'erreur.
TEST(OffReader, negative_control_fan_triangulation_is_seen_by_area_not_volume)
{
	Mesh polygons;
	ASSERT_EQ (LoadOffText (kLPrism, polygons), 0);

	const MeshValidationReport fan = ValidateMesh (FanTriangulated (polygons));
	EXPECT_NEAR (std::fabs (fan.signedVolume), 128.0, 1e-3);
	EXPECT_GT (fan.area, kLPrismArea + 50.0);

	polygons.Triangulate ();
	const MeshValidationReport good = ValidateMesh (polygons);
	EXPECT_NEAR (good.area, kLPrismArea, 1e-3);
}

// Faces concaves a six sommets : une triangulation en eventail depuis le
// premier sommet couvrirait l'encoche.
TEST(ImportScad, concave_polygonal_faces_keep_their_area)
{
	REQUIRE_OPENSCAD ();
	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "concave_polyhedron.scad", TestOptions ());
	ASSERT_TRUE (r.ok ()) << r.message << "\n" << r.log;
	const MeshValidationReport report = ValidateMesh (*vm.GetMeshes ()[0]);
	EXPECT_EQ (report.Count (MeshDefect::BoundaryEdge), 0u);
	EXPECT_EQ (report.Count (MeshDefect::NonTriangleFace), 0u);
	EXPECT_NEAR (report.area, kLPrismArea, 1e-3);
	EXPECT_NEAR (std::fabs (report.signedVolume), 128.0, 1e-3);
	ExpectBox (MeasureBox (vm), { 0.f, 0.f, 0.f }, { 10.f, 10.f, 2.f });
}

// Temoin du detecteur de nettoyage employe plus bas : il voit un repertoire
// vivant, et ne le voit plus apres destruction, fichier compris.
TEST(ExternalProcess, temporary_directory_is_visible_then_removed_with_content)
{
	process::TemporaryDirectory root ("scad-test-root");
	EXPECT_TRUE (TemporaryEntries (root.path ()).empty ());
	{
		process::TemporaryDirectory dir ("scad", root.path ());
		EXPECT_EQ (dir.path ().parent_path (), root.path ());
		std::ofstream (dir.path () / "model.off") << "OFF\n0 0 0\n";
		EXPECT_EQ (TemporaryEntries (root.path ()).size (), 1u);
	}
	EXPECT_TRUE (TemporaryEntries (root.path ()).empty ());
}

TEST(ImportScad, cube_minus_sphere_gives_a_closed_mesh_with_the_expected_box)
{
	REQUIRE_OPENSCAD ();
	process::TemporaryDirectory root ("scad-test-root");

	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "cube_minus_sphere.scad", TestOptions (root.path ()));
	ASSERT_TRUE (r.ok ()) << scad::ImportStatusName (r.status) << ": " << r.message << "\n" << r.log;
	ASSERT_EQ (vm.GetNMeshes (), 1u);
	EXPECT_GT (vm.GetNFaces (), 0u);
	ExpectBox (MeasureBox (vm), { -10.f, -10.f, -10.f }, { 10.f, 10.f, 10.f });

	Mesh& mesh = *vm.GetMeshes ()[0];
	MeshValidationOptions vopt;
	vopt.checkSelfIntersection = false;
	const MeshValidationReport report = ValidateMesh (mesh, vopt);
	EXPECT_EQ (report.Count (MeshDefect::BoundaryEdge), 0u);
	EXPECT_EQ (report.Count (MeshDefect::NonManifoldEdge), 0u);
	EXPECT_EQ (report.Count (MeshDefect::InconsistentOrientation), 0u);
	EXPECT_EQ (report.Count (MeshDefect::InvalidIndex), 0u);
	// Le cube plein vaut 8000 ; la sphere en retire une part, sans tout retirer.
	EXPECT_GT (report.signedVolume, 1000.0);
	EXPECT_LT (report.signedVolume, 8000.0);

	EXPECT_TRUE (TemporaryEntries (root.path ()).empty ());
}

// Temoin negatif : le detecteur de fermeture employe ci-dessus echoue bien sur
// le meme maillage perce d'une face.
TEST(ImportScad, negative_control_closedness_detector_sees_a_removed_face)
{
	REQUIRE_OPENSCAD ();
	VMeshes vm;
	ASSERT_TRUE (scad::ImportScad (vm, kData / "cube_minus_sphere.scad", TestOptions ()).ok ());
	Mesh& mesh = *vm.GetMeshes ()[0];
	mesh.RemoveFace (0);
	MeshValidationOptions vopt;
	vopt.checkSelfIntersection = false;
	EXPECT_GT (ValidateMesh (mesh, vopt).Count (MeshDefect::BoundaryEdge), 0u);
}

// Le parametre -D change la geometrie : la boite englobante n'est donc pas
// une constante que le test retrouverait quoi qu'il arrive.
TEST(ImportScad, defines_override_top_level_variables)
{
	REQUIRE_OPENSCAD ();
	VMeshes byDefault;
	ASSERT_TRUE (scad::ImportScad (byDefault, kData / "parametric.scad", TestOptions ()).ok ());
	ExpectBox (MeasureBox (byDefault), { 0.f, 0.f, 0.f }, { 5.f, 5.f, 5.f });

	VMeshes overridden;
	scad::ImportOptions options = TestOptions ();
	options.defines.push_back ({ "size", "7.5" });
	const scad::ImportResult r = scad::ImportScad (overridden, kData / "parametric.scad", options);
	ASSERT_TRUE (r.ok ()) << r.message;
	ExpectBox (MeasureBox (overridden), { 0.f, 0.f, 0.f }, { 7.5f, 7.5f, 7.5f });
}

// Litteraux composes : un vecteur devient la taille du cube ; une chaine
// contenant `;` reste une chaine, sans instruction ajoutee.
TEST(ImportScad, vector_and_quoted_semicolon_defines_are_inert_literals)
{
	REQUIRE_OPENSCAD ();
	VMeshes vector;
	scad::ImportOptions options = TestOptions ();
	options.defines.push_back ({ "size", "[2, 3, 4]" });
	scad::ImportResult r = scad::ImportScad (vector, kData / "parametric.scad", options);
	ASSERT_TRUE (r.ok ()) << r.message << "\n" << r.log;
	ExpectBox (MeasureBox (vector), { 0.f, 0.f, 0.f }, { 2.f, 3.f, 4.f });

	VMeshes quoted;
	options.defines = { { "label", "\"a; translate([100,0,0]) cube(3)\"" } };
	r = scad::ImportScad (quoted, kData / "parametric.scad", options);
	ASSERT_TRUE (r.ok ()) << r.message << "\n" << r.log;
	ExpectBox (MeasureBox (quoted), { 0.f, 0.f, 0.f }, { 5.f, 5.f, 5.f });
}

TEST(ImportScad, relative_include_resolves_next_to_the_source)
{
	REQUIRE_OPENSCAD ();
	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "uses_include.scad", TestOptions ());
	ASSERT_TRUE (r.ok ()) << r.message << "\n" << r.log;
	ExpectBox (MeasureBox (vm), { 1.f, 2.f, 3.f }, { 5.f, 7.f, 9.f });
}

TEST(ImportScad, path_with_spaces_accents_and_shell_metacharacters)
{
	REQUIRE_OPENSCAD ();
	process::TemporaryDirectory dir ("scad-test");
	const fs::path sub = dir.path () / fs::u8path (u8"mod\u00e8le & co; (v2)");
	fs::create_directory (sub);
	const fs::path copy = sub / fs::u8path (u8"pi\u00e8ce 1.scad");
	fs::copy_file (kData / "cube_minus_sphere.scad", copy);

	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, copy, TestOptions ());
	ASSERT_TRUE (r.ok ()) << r.message << "\n" << r.log;
	ExpectBox (MeasureBox (vm), { -10.f, -10.f, -10.f }, { 10.f, 10.f, 10.f });
}

TEST(ImportScad, syntax_error_reports_openscad_message_and_leaves_output_untouched)
{
	REQUIRE_OPENSCAD ();
	process::TemporaryDirectory root ("scad-test-root");

	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "syntax_error.scad", TestOptions (root.path ()));
	EXPECT_EQ (r.status, scad::ImportStatus::OpenScadError);
	EXPECT_NE (r.exitCode, 0);
	EXPECT_NE (r.message.find ("Parser error"), std::string::npos) << r.message;
	EXPECT_NE (r.log.find ("syntax_error.scad"), std::string::npos) << r.log;
	EXPECT_EQ (vm.GetNMeshes (), 0u);
	EXPECT_TRUE (TemporaryEntries (root.path ()).empty ());
}

TEST(ImportScad, empty_top_level_object_is_an_empty_result)
{
	REQUIRE_OPENSCAD ();
	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "empty.scad", TestOptions ());
	EXPECT_EQ (r.status, scad::ImportStatus::EmptyResult);
	EXPECT_NE (r.message.find ("empty"), std::string::npos) << r.message;
	EXPECT_NE (r.log.find ("nothing to render"), std::string::npos) << r.log;
	EXPECT_EQ (vm.GetNMeshes (), 0u);
}

TEST(ImportScad, two_dimensional_result_is_an_openscad_error)
{
	REQUIRE_OPENSCAD ();
	VMeshes vm;
	const scad::ImportResult r = scad::ImportScad (vm, kData / "flat_2d.scad", TestOptions ());
	EXPECT_EQ (r.status, scad::ImportStatus::OpenScadError);
	EXPECT_NE (r.message.find ("not a 3D object"), std::string::npos) << r.message;
	EXPECT_EQ (vm.GetNMeshes (), 0u);
}

TEST(ImportScad, long_render_is_cancelled_and_cleaned_up)
{
	REQUIRE_OPENSCAD ();
	process::TemporaryDirectory root ("scad-test-root");

	VMeshes vm;
	scad::ImportOptions options = TestOptions (root.path ());
	const auto t0 = std::chrono::steady_clock::now ();
	options.shouldCancel = [t0] () { return std::chrono::steady_clock::now () - t0 > std::chrono::milliseconds (500); };
	const scad::ImportResult cancelled = scad::ImportScad (vm, kData / "slow.scad", options);
	EXPECT_EQ (cancelled.status, scad::ImportStatus::Cancelled) << cancelled.message;

	options.shouldCancel = nullptr;
	options.timeout      = std::chrono::milliseconds (500);
	const scad::ImportResult timedOut = scad::ImportScad (vm, kData / "slow.scad", options);
	EXPECT_EQ (timedOut.status, scad::ImportStatus::TimedOut) << timedOut.message;

	EXPECT_LT (std::chrono::steady_clock::now () - t0, std::chrono::seconds (15));
	EXPECT_EQ (vm.GetNMeshes (), 0u);
	EXPECT_TRUE (TemporaryEntries (root.path ()).empty ());
}
