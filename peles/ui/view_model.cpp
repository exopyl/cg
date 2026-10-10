#include "view_model.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Variant.h>

#include <utility>

namespace peles
{
namespace ui
{

namespace
{

// PREMIER ARGUMENT DU RAPPEL, EN ENTIER. Un `data-event-click="onFileRow(
// file.id)"` livre ses arguments sous forme de variants ; un rappel appele sans
// argument -- faute de frappe dans le document -- doit rendre une ligne
// INEXISTANTE et non la ligne 0, qui existe presque toujours.
int FirstRowId (const Rml::VariantList& arguments)
{
	if (arguments.empty ())
		return -1;
	return arguments[0].Get<int> (-1);
}

// Fabrique un rappel qui depose une commande d'un genre donne.
//
// ⚠ IL NE FAIT RIEN D'AUTRE, ET C'EST LE POINT. Voir ui_command.h : ce code
// court a l'interieur du parcours d'arbre de RmlUi.
Rml::DataEventFunc MakeCommandCallback (const CommandSink& sink, UiCommandKind kind)
{
	return [sink, kind] (Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
	{
		const int rowId = FirstRowId (arguments);
		if (rowId < 0 || !sink)
			return;
		UiCommand command;
		command.kind  = kind;
		command.rowId = rowId;
		sink (command);
	};
}

} // namespace

bool BindViewModel (Rml::Context* context, const char* modelName, ViewModel& model,
                    CommandSink sink, Rml::DataModelHandle& handle)
{
	if (context == nullptr || modelName == nullptr)
		return false;

	Rml::DataModelConstructor constructor = context->CreateDataModel (modelName);
	if (!constructor)
		return false;

	// ⚠ CHAQUE MEMBRE LIE EST PUBLIC, NON `const` ET NON HERITE. RmlUi ne lie
	// rien d'autre : un membre qui cesserait de remplir ces conditions serait
	// absent a l'affichage.
	//
	// ⚠ ET LE REFUS EXISTE : RegisterMember et Bind rendent un `bool`. Jeter ces
	// retours laisserait le seul diagnostic possible -- « ce champ n'apparait
	// pas » -- a une inspection visuelle. TOUS les retours sont agreges, et un
	// seul faux fait echouer le liage.
	bool bound = true;

	if (auto treeRow = constructor.RegisterStruct<core::TreeRow> ())
	{
		bound = treeRow.RegisterMember ("id", &core::TreeRow::id) && bound;
		bound = treeRow.RegisterMember ("depth", &core::TreeRow::depth) && bound;
		bound = treeRow.RegisterMember ("label", &core::TreeRow::label) && bound;
		bound = treeRow.RegisterMember ("path", &core::TreeRow::path) && bound;
		bound = treeRow.RegisterMember ("styleClass", &core::TreeRow::styleClass) && bound;
		bound = treeRow.RegisterMember ("expanded", &core::TreeRow::expanded) && bound;
		bound = treeRow.RegisterMember ("openable", &core::TreeRow::openable) && bound;
		bound = treeRow.RegisterMember ("error", &core::TreeRow::error) && bound;
		bound = treeRow.RegisterMember ("expandable", &core::TreeRow::expandable) && bound;
		bound = treeRow.RegisterMember ("drive", &core::TreeRow::drive) && bound;
		bound = treeRow.RegisterMember ("selected", &core::TreeRow::selected) && bound;
	}
	else
	{
		return false;
	}

	if (auto fileRow = constructor.RegisterStruct<core::FileRow> ())
	{
		bound = fileRow.RegisterMember ("id", &core::FileRow::id) && bound;
		bound = fileRow.RegisterMember ("label", &core::FileRow::label) && bound;
		bound = fileRow.RegisterMember ("path", &core::FileRow::path) && bound;
		bound = fileRow.RegisterMember ("kind", &core::FileRow::kind) && bound;
		bound = fileRow.RegisterMember ("selected", &core::FileRow::selected) && bound;
	}
	else
	{
		return false;
	}

	if (auto infoRow = constructor.RegisterStruct<core::InfoRow> ())
	{
		bound = infoRow.RegisterMember ("label", &core::InfoRow::label) && bound;
		bound = infoRow.RegisterMember ("value", &core::InfoRow::value) && bound;
		bound = infoRow.RegisterMember ("depth", &core::InfoRow::depth) && bound;
		bound = infoRow.RegisterMember ("styleClass", &core::InfoRow::styleClass) && bound;
		bound = infoRow.RegisterMember ("swatch", &core::InfoRow::swatch) && bound;
		bound = infoRow.RegisterMember ("copyable", &core::InfoRow::copyable) && bound;
	}
	else
	{
		return false;
	}

	if (!constructor.RegisterArray<std::vector<core::TreeRow>> ())
		return false;
	if (!constructor.RegisterArray<std::vector<core::FileRow>> ())
		return false;
	if (!constructor.RegisterArray<std::vector<core::InfoRow>> ())
		return false;

	// ⚠ LES NOMS VIENNENT DE view_model.h, ET C'EST LE SEUL ENDROIT OU ILS SONT
	// ECRITS. UiController::Publish* salit ces memes variables ; un nom diverge
	// ici et le panneau se fige en Release sans un mot.
	bound = constructor.Bind (binding::kTreeRows, &model.treeRows) && bound;
	bound = constructor.Bind (binding::kFileRows, &model.fileRows) && bound;
	bound = constructor.Bind (binding::kCurrentDirectory, &model.currentDirectory) && bound;
	bound = constructor.Bind (binding::kTreeStatus, &model.treeStatus) && bound;
	bound = constructor.Bind (binding::kFilesStatus, &model.filesStatus) && bound;
	bound = constructor.Bind (binding::kFilesHidden, &model.filesHidden) && bound;
	bound = constructor.Bind (binding::kInfoRows, &model.infoRows) && bound;
	bound = constructor.Bind (binding::kMeshRows, &model.meshRows) && bound;
	bound = constructor.Bind (binding::kMaterialRows, &model.materialRows) && bound;
	bound = constructor.Bind (binding::kStatus, &model.status) && bound;

	// LES TROIS RAPPELS D'EVENEMENT, AGREGES COMME LES LIAISONS.
	//
	// ⚠ MEME RAISON : BindEventCallback rend un `bool`. Un rappel refuse -- nom
	// deja pris -- laisserait un clic sans effet, et le seul diagnostic possible
	// serait « ca ne fait rien quand je clique ».
	bound = constructor.BindEventCallback (
	            binding::kOnTreeRow,
	            MakeCommandCallback (sink, UiCommandKind::ToggleDirectory)) &&
	        bound;
	bound = constructor.BindEventCallback (
	            binding::kOnFileRow, MakeCommandCallback (sink, UiCommandKind::SelectFile)) &&
	        bound;
	bound = constructor.BindEventCallback (
	            binding::kOnFileOpen, MakeCommandCallback (sink, UiCommandKind::OpenFile)) &&
	        bound;

	if (!bound)
	{
		// ⚠ LE MODELE PARTIELLEMENT CONSTRUIT EST RETIRE. Le laisser en place
		// ferait echouer une seconde tentative sur « nom deja pris », donc sur
		// une cause qui n'est pas la bonne.
		UnbindViewModel (context, modelName);
		return false;
	}

	handle = constructor.GetModelHandle ();
	return true;
}

void UnbindViewModel (Rml::Context* context, const char* modelName)
{
	if (context == nullptr || modelName == nullptr)
		return;
	context->RemoveDataModel (modelName);
}

} // namespace ui
} // namespace peles
