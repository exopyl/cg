#include "ui_controller.h"

#include "model_info.h"
#include "topology_stats.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <utility>

namespace peles
{
namespace ui
{

struct UiController::Binding
{
	Rml::Context*        context = nullptr;
	std::string          name;
	Rml::DataModelHandle handle;
};

UiController::UiController () : m_binding (std::make_unique<Binding> ())
{
	// LES QUATORZE LIGNES EXISTENT AVANT TOUT MODELE. Le panneau de droite a
	// donc sa hauteur definitive des la premiere mise en page, et l'ouverture
	// d'un fichier ne la fait pas changer.
	m_model.infoRows = core::BuildEmptyModelInfo ();
}

UiController::~UiController ()
{
	// ⚠ LE MODELE EST DELIE AVANT QUE SES CIBLES NE MEURENT. Les variables
	// liees designent des membres de m_model, detruit juste apres ce
	// destructeur : un contexte qui survivrait a ce controleur lirait de la
	// memoire liberee au prochain Update.
	UnbindViewModel (m_binding->context, m_binding->name.c_str ());
}

bool UiController::Bind (Rml::Context* context, const char* modelName)
{
	if (context == nullptr || modelName == nullptr)
		return false;

	// ⚠ LE PUITS CAPTURE `this`, ET LE MODELE EST DELIE DANS LE DESTRUCTEUR.
	// L'ordre des deux est ce qui empeche un rappel de RmlUi d'ecrire dans un
	// controleur detruit.
	CommandSink sink = [this] (const UiCommand& command) { m_commands.push_back (command); };

	if (!BindViewModel (context, modelName, m_model, std::move (sink), m_binding->handle))
		return false;

	m_binding->context = context;
	m_binding->name    = modelName;
	return true;
}

void UiController::PublishTree ()
{
	m_model.treeRows   = m_tree.Rows ();
	m_model.treeStatus = m_tree.LastError ();
	if (m_binding->handle)
	{
		// UNE SEULE VARIABLE SALIE PAR OPERATION. Le vecteur entier est
		// reconstruit par l'operation qui l'a change : il n'y a rien a salir
		// plus finement, et RmlUi ne le permettrait pas.
		m_binding->handle.DirtyVariable (binding::kTreeRows);
		m_binding->handle.DirtyVariable (binding::kTreeStatus);
	}
}

void UiController::PublishFiles ()
{
	// LA COPIE EMPORTE LA SELECTION, elle ne la porte pas : FileList::Rows()
	// rend deja des lignes dont `selected` reflete FileList::Selected().
	m_model.fileRows         = m_files.Rows ();
	m_model.filesHidden      = m_files.HiddenCount ();
	m_model.filesStatus      = m_files.LastError ();
	m_model.currentDirectory = m_files.DirectoryDisplay ();
	if (m_binding->handle)
	{
		m_binding->handle.DirtyVariable (binding::kFileRows);
		m_binding->handle.DirtyVariable (binding::kFilesHidden);
		m_binding->handle.DirtyVariable (binding::kFilesStatus);
		m_binding->handle.DirtyVariable (binding::kCurrentDirectory);
	}
}

void UiController::PublishInfo ()
{
	if (m_binding->handle)
	{
		m_binding->handle.DirtyVariable (binding::kInfoRows);
		m_binding->handle.DirtyVariable (binding::kMeshRows);
		m_binding->handle.DirtyVariable (binding::kMaterialRows);
	}
}

void UiController::PublishStatus ()
{
	if (m_binding->handle)
		m_binding->handle.DirtyVariable (binding::kStatus);
}

void UiController::SetStatus (const std::string& status)
{
	if (m_model.status == status)
		return;
	m_model.status = status;
	PublishStatus ();
}

std::vector<UiCommand> UiController::TakeCommands ()
{
	std::vector<UiCommand> commands;
	commands.swap (m_commands);
	return commands;
}

bool UiController::ActivateDirectoryRow (int rowId)
{
	if (m_tree.PathOf (rowId).empty ())
		return false;

	// L'OUVERTURE PRECEDE LA SELECTION, et l'ordre compte : Toggle recalcule les
	// lignes, SelectDirectory publie. L'inverse publierait deux fois pour un
	// seul clic.
	m_tree.Toggle (rowId);
	return SelectDirectory (rowId);
}

std::filesystem::path UiController::SelectFileRow (int rowId)
{
	const std::filesystem::path path = m_files.PathOf (rowId);
	if (path.empty ())
		return path;

	m_files.Select (rowId);
	PublishFiles ();
	return path;
}

void UiController::ShowModelInfo (::Model* model)
{
	if (model == nullptr)
	{
		m_model.infoRows = core::BuildEmptyModelInfo ();
		m_model.meshRows.clear ();
		m_model.materialRows.clear ();
		PublishInfo ();
		return;
	}

	// LES COMPTEURS TOPOLOGIQUES VIENNENT DE core/, PAS DE LA VUE. C'est ce qui
	// permet a cette cible de ne lier ni cgre ni glfw.
	const core::TopologyStats topology = core::ComputeTopologyStats (*model);

	m_model.infoRows     = core::BuildModelInfo (*model, topology);
	m_model.meshRows     = core::BuildMeshTree (*model);
	m_model.materialRows = core::BuildMaterialTree (*model);
	PublishInfo ();
}

void UiController::ShowSystemRoots ()
{
	m_tree.SetSystemRoots ();
	m_files.Clear ();
	PublishTree ();
	PublishFiles ();
}

bool UiController::ShowDirectory (const std::filesystem::path& directory)
{
	const bool opened = m_tree.SetRoot (directory);
	if (!opened)
	{
		m_files.Clear ();
		PublishTree ();
		PublishFiles ();
		return false;
	}

	const std::vector<core::TreeRow>& rows = m_tree.Rows ();
	if (!rows.empty ())
	{
		const int rootId = rows.front ().id;
		m_tree.Toggle (rootId);
		return SelectDirectory (rootId);
	}

	PublishTree ();
	PublishFiles ();
	return true;
}

bool UiController::ToggleRow (int rowId)
{
	const bool toggled = m_tree.Toggle (rowId);
	// Publie MEME EN ECHEC : un noeud dont l'ouverture a echoue devient une
	// ligne en erreur, et cette ligne doit apparaitre.
	PublishTree ();
	return toggled;
}

void UiController::ExpandToDepth (int maxDepth)
{
	m_tree.ExpandToDepth (maxDepth);
	PublishTree ();
}

bool UiController::SelectDirectory (int rowId)
{
	const std::filesystem::path path = m_tree.PathOf (rowId);
	if (path.empty ())
		return false;

	m_tree.Select (rowId);
	const bool populated = m_files.Populate (path);
	PublishTree ();
	PublishFiles ();
	return populated;
}

void UiController::SelectFile (const std::filesystem::path& path)
{
	const int previous = m_files.Selected ();
	if (m_files.SelectByPath (path) == previous)
		return;

	// La republication recopie les lignes, selection comprise : il n'y a pas de
	// second endroit ou la poser.
	PublishFiles ();
}

} // namespace ui
} // namespace peles
