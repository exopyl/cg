// ===========================================================================
//  Réglage « répertoire d'OpenSCAD » de sinaia, et filtre des formats
// ===========================================================================
//
// La règle de validité (cgmesh::scad::CheckOpenScadDirectory), la conversion
// de l'ancien réglage (MigrateOpenScadSetting) et le catalogue des formats de
// sinaia (sinaia/FormatCatalog.h, sans wxWidgets) décident ensemble si un
// .scad est proposé et ouvrable. Rien ici ne lance OpenSCAD : les
// « exécutables » sont des fichiers factices.
//
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "../src/cgmesh/external_process.h"
#include "../src/cgmesh/import_scad.h"
#include "../sinaia/FormatCatalog.h"

namespace fs = std::filesystem;
using cgmesh::scad::DirectoryState;

namespace {

void Touch (const fs::path& file, bool executable = true)
{
	std::ofstream (file) << "MZ";
#ifndef _WIN32
	fs::permissions (file, executable ? fs::perms::owner_all : fs::perms::owner_read | fs::perms::owner_write,
	                 fs::perm_options::replace);
#else
	(void) executable;
#endif
}

std::size_t Count (const std::string& text, const std::string& needle)
{
	std::size_t n = 0;
	for (std::size_t at = text.find (needle); at != std::string::npos; at = text.find (needle, at + 1))
		++n;
	return n;
}

} // namespace

// --- Règle de validité -----------------------------------------------------

TEST(ScadSetting, directory_states)
{
	cgmesh::process::TemporaryDirectory root ("scad-setting");
	const fs::path exe = cgmesh::scad::kDirectoryExecutableName;

	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (fs::path ()), DirectoryState::Empty);
	// Relatif : refusé quel que soit le répertoire courant, même "." ou un
	// chemin relatif qui y désignerait un répertoire valide.
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory ("."), DirectoryState::Relative);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory ("tools/openscad"), DirectoryState::Relative);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (root.path () / "absent"), DirectoryState::Missing);

	const fs::path file = root.path () / "file.txt";
	Touch (file);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (file), DirectoryState::NotADirectory);

	const fs::path emptyDir = root.path () / "empty";
	fs::create_directory (emptyDir);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (emptyDir), DirectoryState::NoExecutable);
	EXPECT_TRUE (cgmesh::scad::OpenScadInDirectory (emptyDir).empty ());

	// Témoin : un RÉPERTOIRE portant le nom de l'exécutable ne suffit pas.
	const fs::path fake = root.path () / "fake";
	fs::create_directories (fake / exe);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (fake), DirectoryState::NoExecutable);

#ifdef _WIN32
	// openscad.com seul : le réglage exige openscad.exe.
	const fs::path comOnly = root.path () / "com";
	fs::create_directory (comOnly);
	Touch (comOnly / "openscad.com");
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (comOnly), DirectoryState::NoExecutable);
#else
	// Sans bit d'exécution, le fichier n'est pas un exécutable.
	const fs::path noExec = root.path () / "noexec";
	fs::create_directory (noExec);
	Touch (noExec / exe, false);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (noExec), DirectoryState::NoExecutable);

	// others_exec seul : exécutable pour les autres, pas pour son propriétaire.
	// root contourne les bits de permission (access rend X_OK dès qu'un bit x
	// est posé) : le cas n'a de sens que pour un utilisateur ordinaire.
	const fs::path othersOnly = root.path () / "othersonly";
	fs::create_directory (othersOnly);
	Touch (othersOnly / exe, false);
	fs::permissions (othersOnly / exe, fs::perms::owner_read | fs::perms::owner_write | fs::perms::others_exec,
	                 fs::perm_options::replace);
	if (geteuid () != 0)
		EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (othersOnly), DirectoryState::NoExecutable);
#endif

	const fs::path good = root.path () / "OpenSCAD 2021.01";
	fs::create_directory (good);
	Touch (good / exe);
	EXPECT_EQ (cgmesh::scad::CheckOpenScadDirectory (good), DirectoryState::Valid);
	EXPECT_EQ (cgmesh::scad::OpenScadInDirectory (good), good / exe);
}

// --- Conversion de l'ancien réglage (chemin de l'exécutable) ---------------

TEST(ScadSetting, legacy_executable_setting_is_migrated_or_kept_with_a_note)
{
	using cgmesh::scad::MigrateOpenScadSetting;
	cgmesh::process::TemporaryDirectory root ("scad-setting");
	const fs::path exe = cgmesh::scad::kDirectoryExecutableName;

	const fs::path good = root.path () / "good";
	fs::create_directory (good);
	Touch (good / exe);
	auto m = MigrateOpenScadSetting (good / exe);
	EXPECT_EQ (m.directory, good);
	EXPECT_TRUE (m.converted);
	EXPECT_TRUE (m.note.empty ());

	// Un ancien chemin dont le nom n'est pas openscad* est traité de même : son
	// répertoire s'il est valide.
	m = MigrateOpenScadSetting (good / "renamed-openscad.exe");
	EXPECT_EQ (m.directory, good);
	EXPECT_TRUE (m.converted);

	// Non convertible (répertoire sans exécutable, chemin disparu, relatif) :
	// rien n'est retenu, mais la note dit pourquoi -- jamais de disparition
	// silencieuse, ni de « not a directory » sans trace.
	const fs::path bad = root.path () / "bad";
	fs::create_directory (bad);
	Touch (bad / "openscad.com");
	for (const fs::path& p : { bad / "openscad.com", root.path () / "gone" / "openscad.exe",
	                           root.path () / "gone" / "oscad.bin", fs::path ("tools/openscad.exe") })
	{
		m = MigrateOpenScadSetting (p);
		EXPECT_TRUE (m.directory.empty ()) << p.u8string ();
		EXPECT_FALSE (m.converted) << p.u8string ();
		EXPECT_NE (m.note.find (p.u8string ()), std::string::npos) << m.note;
	}

	// Un répertoire absolu existant est retenu tel quel, valide ou non.
	m = MigrateOpenScadSetting (good);
	EXPECT_EQ (m.directory, good);
	EXPECT_TRUE (m.converted);
	m = MigrateOpenScadSetting (bad);
	EXPECT_EQ (m.directory, bad);
	EXPECT_FALSE (m.converted);

	m = MigrateOpenScadSetting (fs::path ());
	EXPECT_TRUE (m.directory.empty ());
	EXPECT_TRUE (m.note.empty ());
}

// L'ancienne clé n'est effacée que si la nouvelle valeur la remplace pour de
// bon : sinon un ancien réglage momentanément inaccessible serait perdu sur un
// simple OK.
TEST(ScadSetting, legacy_key_is_deleted_only_when_replaced)
{
	EXPECT_FALSE (sinaia::ShouldDeleteLegacyOpenScadSetting (false, false));
	EXPECT_TRUE  (sinaia::ShouldDeleteLegacyOpenScadSetting (true,  false));
	EXPECT_TRUE  (sinaia::ShouldDeleteLegacyOpenScadSetting (false, true));
	EXPECT_TRUE  (sinaia::ShouldDeleteLegacyOpenScadSetting (true,  true));
}

// --- Filtre des dialogues et catalogue -------------------------------------

TEST(ScadSetting, open_dialog_offers_scad_only_when_available)
{
	const std::string with    = sinaia::BuildOpenWildcardText (true);
	const std::string without = sinaia::BuildOpenWildcardText (false);

	// Témoin du détecteur : avec OpenSCAD, *.scad apparaît dans l'agrégat, dans
	// le libellé de sa propre entrée et dans son masque.
	EXPECT_EQ (Count (with, "*.scad"), 3u) << with;
	EXPECT_NE (with.find ("OpenSCAD (*.scad)|*.scad|"), std::string::npos) << with;

	EXPECT_EQ (Count (without, "scad"), 0u) << without;
	EXPECT_EQ (Count (without, "OpenSCAD"), 0u) << without;

	// Rien d'autre ne change : les deux chaînes ne diffèrent que de l'OpenSCAD.
	for (const sinaia::FormatSpec& f : sinaia::FormatCatalog ())
		for (const char* e : f.exts)
			if (!f.requiresOpenScad)
			{
				const std::string mask = std::string ("*.") + e;
				EXPECT_EQ (Count (with, mask), Count (without, mask)) << mask;
				EXPECT_GE (Count (without, mask), 3u) << mask;
			}

	EXPECT_EQ (sinaia::OfferedFormats (true).size (), sinaia::FormatCatalog ().size ());
	EXPECT_EQ (sinaia::OfferedFormats (false).size (), sinaia::FormatCatalog ().size () - 1);
	for (const sinaia::FormatSpec& f : sinaia::OfferedFormats (false))
		EXPECT_FALSE (f.requiresOpenScad) << f.label;
}

// Le panneau "Files" garde les .scad visibles : leur ouverture sans OpenSCAD
// doit pouvoir expliquer la marche à suivre.
TEST(ScadSetting, files_panel_catalog_keeps_scad)
{
	const sinaia::FormatSpec* scad = sinaia::FindFormat ("SCAD");
	ASSERT_NE (scad, nullptr);
	EXPECT_TRUE (scad->requiresOpenScad);
	EXPECT_TRUE (sinaia::IsScadExtension ("Scad"));
	EXPECT_FALSE (sinaia::IsScadExtension ("stl"));
	EXPECT_FALSE (sinaia::IsScadExtension (""));
	EXPECT_EQ (sinaia::FindFormat ("xyz"), nullptr);
}

TEST(ScadSetting, refusal_message_says_where_to_set_the_directory)
{
	EXPECT_TRUE (sinaia::ScadUnavailableText (DirectoryState::Valid, "C:/x").empty ());
	for (DirectoryState s : { DirectoryState::Empty, DirectoryState::Relative, DirectoryState::Missing, DirectoryState::NotADirectory,
	                          DirectoryState::NoExecutable })
	{
		const std::string m = sinaia::ScadUnavailableText (s, "C:/somewhere");
		EXPECT_NE (m.find ("File > Settings > Import"), std::string::npos) << m;
		EXPECT_NE (m.find (cgmesh::scad::kDirectoryExecutableName), std::string::npos) << m;
		EXPECT_FALSE (sinaia::OpenScadDirectoryStatusText (s).empty ());
		if (s != DirectoryState::Empty)
		{
			EXPECT_NE (m.find ("C:/somewhere"), std::string::npos) << m;
		}
	}
	EXPECT_NE (sinaia::OpenScadDirectoryStatusText (DirectoryState::Valid).find ("Valid"), std::string::npos);
	EXPECT_NE (sinaia::OpenScadDirectoryStatusText (DirectoryState::NoExecutable).find ("Invalid"), std::string::npos);
}
