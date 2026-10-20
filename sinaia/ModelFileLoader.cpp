#include "ModelFileLoader.h"

#include <wx/config.h>
#include <wx/filename.h>
#include <wx/progdlg.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include "../src/cgmesh/import_scad.h"
#include "../src/cgmesh/vmeshes.h"
#include "FormatCatalog.h"
#include "../src/cgmesh/vmeshes_io.h"

namespace sinaia {

namespace {

const wxChar* const kOpenScadDirectoryKey = wxT("/External/OpenSCADDirectory");
const wxChar* const kLegacyOpenScadKey    = wxT("/External/OpenSCAD");

std::filesystem::path ToPath (const wxString& s)
{
	return std::filesystem::u8path (s.utf8_string ());
}

// Le dialogue n'apparait qu'au-dela d'une demi-seconde : un rendu bref ne doit
// pas faire clignoter une fenetre. Pulse() traite les evenements de
// l'interface, donc le bouton d'annulation reste actif pendant le rendu.
class LazyProgress
{
public:
	LazyProgress (wxWindow* parent, const wxString& file)
		: m_parent (parent), m_file (file), m_start (std::chrono::steady_clock::now ()) {}

	bool ShouldCancel ()
	{
		if (!m_dialog)
		{
			if (std::chrono::steady_clock::now () - m_start < std::chrono::milliseconds (500))
				return false;
			m_dialog = std::make_unique<wxProgressDialog> (
				_("OpenSCAD"),
				wxString::Format (_("Rendering %s with OpenSCAD..."), m_file),
				100, m_parent, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME);
		}
		return !m_dialog->Pulse ();
	}

private:
	wxWindow*                             m_parent;
	wxString                              m_file;
	std::chrono::steady_clock::time_point m_start;
	std::unique_ptr<wxProgressDialog>     m_dialog;
};

ModelLoadResult LoadScad (VMeshes& out, const wxString& path, wxWindow* parent)
{
	ModelLoadResult result;
	const std::filesystem::path executable = cgmesh::scad::OpenScadInDirectory (ToPath (LoadOpenScadDirectory ()));
	if (executable.empty ())
	{
		result.unavailable = true;
		result.error       = ScadUnavailableMessage ();
		return result;
	}

	// Chemin explicite : la recherche generique de cgmesh (CG_OPENSCAD, PATH...)
	// n'est pas consultee, un reglage explicite etant exclusif.
	cgmesh::scad::ImportOptions options;
	options.search.configured = executable;

	auto progress = std::make_shared<LazyProgress> (parent, wxFileName (path).GetFullName ());
	options.shouldCancel = [progress] () { return progress->ShouldCancel (); };

	const cgmesh::scad::ImportResult r =
		cgmesh::scad::ImportScad (out, std::filesystem::u8path (path.utf8_string ()), options);

	result.ok        = r.ok ();
	result.cancelled = r.status == cgmesh::scad::ImportStatus::Cancelled;
	result.log       = wxString::FromUTF8 (r.log);
	if (r.ok ())
		return result;

	result.error = wxString::FromUTF8 (r.message);
	if (r.status == cgmesh::scad::ImportStatus::ExecutableNotFound
	    || r.status == cgmesh::scad::ImportStatus::LaunchFailed)
		result.error += _("\n\nCheck the OpenSCAD directory in File > Settings > Import.");
	return result;
}

} // namespace

bool IsScadFile (const wxString& path)
{
	return IsScadExtension (wxFileName (path).GetExt ().utf8_string ());
}

ModelLoadResult LoadModelFile (VMeshes& out, const wxString& path, wxWindow* parent)
{
	if (IsScadFile (path))
		return LoadScad (out, path, parent);

	ModelLoadResult result;
	const std::string utf8 = path.utf8_string ();
	result.ok = VMeshesIO::load (out, utf8.c_str ());
	if (!result.ok)
		result.error = _("unsupported or unreadable file");
	return result;
}

OpenScadSetting LoadOpenScadSetting ()
{
	wxConfigBase* c = wxConfig::Get ();
	OpenScadSetting setting;
	wxString legacy;
	setting.legacyPresent = c->Read (kLegacyOpenScadKey, &legacy) && !legacy.empty ();

	wxString value;
	if (c->Read (kOpenScadDirectoryKey, &value) && !value.empty ())
	{
		setting.directory = value;
		return setting;
	}
	if (!setting.legacyPresent)
		return setting;
	const cgmesh::scad::SettingMigration m = cgmesh::scad::MigrateOpenScadSetting (ToPath (legacy));
	setting.directory = wxString::FromUTF8 (m.directory.u8string ());
	setting.note      = wxString::FromUTF8 (m.note);
	return setting;
}

wxString LoadOpenScadDirectory ()
{
	return LoadOpenScadSetting ().directory;
}

void SaveOpenScadDirectory (const wxString& directory, bool deleteLegacy)
{
	wxConfigBase* c = wxConfig::Get ();
	c->Write (kOpenScadDirectoryKey, directory);
	if (deleteLegacy)
		c->DeleteEntry (kLegacyOpenScadKey);
	c->Flush ();
}

cgmesh::scad::DirectoryState OpenScadDirectoryState ()
{
	return cgmesh::scad::CheckOpenScadDirectory (ToPath (LoadOpenScadDirectory ()));
}

bool IsScadImportAvailable ()
{
	return OpenScadDirectoryState () == cgmesh::scad::DirectoryState::Valid;
}

wxString ScadUnavailableMessage ()
{
	const wxString directory = LoadOpenScadDirectory ();
	return wxString::FromUTF8 (ScadUnavailableText (
		cgmesh::scad::CheckOpenScadDirectory (ToPath (directory)), directory.utf8_string ()));
}

} // namespace sinaia
