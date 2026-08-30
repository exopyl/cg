#include <gtest/gtest.h>

#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cggraph/ui/editor_model.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <deque>
#include <set>
#include <string>
#include <thread>
#include <vector>

// ===========================================================================
//  Modele d'edition : palette et inspecteur DERIVES du registre
// ===========================================================================
// Ce qui se teste ici n'est pas l'interface -- un rendu ne se teste guere --,
// c'est la GENERATION. Chaque cas qui parcourt le catalogue est ecrit pour
// echouer le jour ou un type de noeud demanderait du code d'interface qui lui
// soit propre : c'est la seule facon d'eprouver « ajouter un noeud n'ajoute
// aucun fichier de UI » depuis une suite de tests, qui ne peut pas relire un
// diff.

using namespace cggraph_ui;

namespace {

std::vector<std::string> CatalogTypeNames ()
{
	std::vector<std::string> names;
	for (const cggraph_nodes::CatalogEntry &entry : cggraph_nodes::Catalog ())
		names.push_back (entry.typeName);
	return names;
}

const ParamField *FindParam (const Inspector &inspector, const std::string &name)
{
	for (const ParamField &field : inspector.GetParams ())
		if (field.name == name)
			return &field;
	return nullptr;
}

std::size_t PublicParamCount (const cggraph::Node &node)
{
	std::size_t count = 0;
	for (const cggraph::ParamEntry &entry : node.GetParams ().GetEntries ())
		if (entry.visibility == cggraph::ParamVisibility::Public)
			++count;
	return count;
}

const PortField *FindInput (const Inspector &inspector, const std::string &name)
{
	for (const PortField &field : inspector.GetInputs ())
		if (field.name == name)
			return &field;
	return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
//  Palette
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_palette, every_catalog_entry_reaches_the_palette_and_nothing_else_does)
{
	// Les deux sens comptent. « Tous les types y sont » serait satisfait par une
	// palette qui publie n'importe quoi ; « rien d'autre » serait satisfait par
	// une palette vide.
	const std::vector<std::string> names = CatalogTypeNames ();
	const Palette palette;

	EXPECT_EQ (names.size (), palette.GetItemCount ());
	for (const std::string &name : names)
		EXPECT_NE (nullptr, palette.Find (name)) << "type absent de la palette : " << name;

	std::set<std::string> published;
	for (const PaletteCategory &category : palette.GetCategories ())
		for (const PaletteItem &item : category.items)
			published.insert (item.typeName);
	EXPECT_EQ (names.size (), published.size ());
}

TEST (TEST_cggraph_ui_palette, an_unknown_type_is_not_found)
{
	const Palette palette;
	EXPECT_EQ (nullptr, palette.Find ("type.qui.nexiste.pas"));
}

TEST (TEST_cggraph_ui_palette, the_items_are_grouped_by_category_in_declaration_order)
{
	const Palette palette;
	ASSERT_FALSE (palette.GetCategories ().empty ());

	// L'ordre attendu est celui du catalogue, categorie par categorie, sans
	// doublon : deux groupes du meme nom voudraient dire que le regroupement
	// n'en est pas un.
	std::vector<std::string> expected;
	for (const cggraph_nodes::CatalogEntry &entry : cggraph_nodes::Catalog ())
	{
		const std::string name = entry.category != nullptr ? entry.category : kUncategorized;
		bool seen = false;
		for (const std::string &known : expected)
			if (known == name)
			{
				seen = true;
				break;
			}
		if (!seen)
			expected.push_back (name);
	}

	ASSERT_EQ (expected.size (), palette.GetCategories ().size ());
	for (std::size_t i = 0; i < expected.size (); ++i)
		EXPECT_EQ (expected[i], palette.GetCategories ()[i].name);
}

TEST (TEST_cggraph_ui_palette, the_label_and_the_caveat_of_the_catalog_are_carried_verbatim)
{
	// La reserve est le seul endroit du depot ou soit ecrit ce que le corps
	// emballe fait et que sa declaration tait. Une palette qui la perd oblige a
	// la reecrire dans l'interface, c'est-a-dire par type de noeud.
	const Palette palette;

	std::size_t withCaveat = 0;
	for (const cggraph_nodes::CatalogEntry &entry : cggraph_nodes::Catalog ())
	{
		const PaletteItem *item = palette.Find (entry.typeName);
		ASSERT_NE (nullptr, item);
		EXPECT_STREQ (entry.label != nullptr ? entry.label : entry.typeName, item->label);
		EXPECT_EQ (entry.caveat, item->caveat);
		if (entry.caveat != nullptr)
			++withCaveat;
	}

	// Temoin : si le catalogue cessait d'en porter, le cas ci-dessus passerait
	// sans rien etablir.
	EXPECT_GT (withCaveat, 0u);
}

// ---------------------------------------------------------------------------
//  Instanciation : la palette ne propose que ce que la fabrique sait construire
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_model, every_palette_item_can_be_created_selected_and_inspected)
{
	// LE cas de l'etape. Il parcourt le catalogue et n'en nomme aucun membre :
	// un type ajoute demain y entre sans qu'une ligne de ce fichier bouge, et
	// un type que la palette proposerait sans que la fabrique le connaisse le
	// fait echouer.
	EditorModel model;
	const Palette &palette = model.GetPalette ();
	ASSERT_GT (palette.GetItemCount (), 0u);

	float y = 0.0f;
	for (const PaletteCategory &category : palette.GetCategories ())
		for (const PaletteItem &item : category.items)
		{
			const cggraph::NodeId id = model.AddNode (item.typeName, 10.0f, y);
			y += 40.0f;
			ASSERT_NE (cggraph::kInvalidNodeId, id) << "type non instanciable : " << item.typeName;

			const cggraph::Node *node = model.GetGraph ().FindNode (id);
			ASSERT_NE (nullptr, node);
			EXPECT_EQ (item.typeName, node->GetDesc ().typeName);

			model.Select (id);
			const Inspector &inspector = model.GetInspector ();
			EXPECT_EQ (id, inspector.GetNode ());
			EXPECT_EQ (item.typeName, inspector.GetTypeName ());
			EXPECT_EQ (item.label, inspector.GetLabel ());

			// Le panneau decrit le noeud en entier : ports declares et
			// parametres poses, sans qu'aucun code ne soit ecrit par type. Les
			// parametres internes en sont retires -- et c'est le decompte
			// derive du noeud, jamais une constante, qui le dit.
			EXPECT_EQ (node->GetDesc ().inputs.size (), inspector.GetInputs ().size ());
			EXPECT_EQ (node->GetDesc ().outputs.size (), inspector.GetOutputs ().size ());
			EXPECT_EQ (PublicParamCount (*node), inspector.GetParams ().size ());
			for (const ParamField &field : inspector.GetParams ())
			{
				EXPECT_NE (nullptr, field.value) << field.name;
				const cggraph::ParamEntry *entry = node->GetParams ().FindEntry (field.name);
				ASSERT_NE (nullptr, entry);
				EXPECT_EQ (cggraph::ParamVisibility::Public, entry->visibility) << field.name;
			}
		}
}

TEST (TEST_cggraph_ui_model, a_type_absent_from_the_catalog_is_refused_and_adds_nothing)
{
	EditorModel model;
	const std::size_t before = model.GetGraph ().GetNodeCount ();
	EXPECT_EQ (cggraph::kInvalidNodeId, model.AddNode ("type.qui.nexiste.pas", 0.0f, 0.0f));
	EXPECT_EQ (before, model.GetGraph ().GetNodeCount ());
}

TEST (TEST_cggraph_ui_model, a_created_node_keeps_the_position_it_was_dropped_at)
{
	EditorModel model;
	const std::string type = CatalogTypeNames ().front ();
	const cggraph::NodeId id = model.AddNode (type, 120.0f, -37.5f);
	ASSERT_NE (cggraph::kInvalidNodeId, id);

	float x = 0.0f;
	float y = 0.0f;
	ASSERT_TRUE (model.GetGraph ().GetNodePosition (id, x, y));
	EXPECT_FLOAT_EQ (120.0f, x);
	EXPECT_FLOAT_EQ (-37.5f, y);
}

TEST (TEST_cggraph_ui_model, selecting_nothing_empties_the_panel)
{
	EditorModel model;
	const cggraph::NodeId id = model.AddNode (CatalogTypeNames ().front (), 0.0f, 0.0f);
	model.Select (id);
	ASSERT_NE (cggraph::kInvalidNodeId, model.GetInspector ().GetNode ());

	model.Select (cggraph::kInvalidNodeId);
	EXPECT_EQ (cggraph::kInvalidNodeId, model.GetInspector ().GetNode ());
	EXPECT_TRUE (model.GetInspector ().GetParams ().empty ());
	EXPECT_TRUE (model.GetInspector ().GetInputs ().empty ());
}

// ---------------------------------------------------------------------------
//  Inspecteur
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_inspector, the_ports_of_the_canonical_descriptor_are_published_with_their_state)
{
	// Le descripteur du §8.1 est le seul qui porte une entree optionnelle. Un
	// panneau qui ne distinguerait pas ses deux entrees laisserait croire qu'il
	// faut cabler la zone pour lisser.
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	ASSERT_NE (cggraph::kInvalidNodeId, id);
	model.Select (id);

	const Inspector &inspector = model.GetInspector ();
	ASSERT_EQ (2u, inspector.GetInputs ().size ());
	ASSERT_EQ (1u, inspector.GetOutputs ().size ());

	const PortField *mesh = FindInput (inspector, "maillage");
	const PortField *zone = FindInput (inspector, "zone");
	ASSERT_NE (nullptr, mesh);
	ASSERT_NE (nullptr, zone);
	EXPECT_EQ (cggraph::PortState::MissingRequired, mesh->state);
	EXPECT_EQ (cggraph::PortState::MissingOptional, zone->state);
	EXPECT_EQ (cggraph::NodeReadiness::MissingRequiredInput, inspector.GetReadiness ());
	EXPECT_NE (nullptr, mesh->type);
}

TEST (TEST_cggraph_ui_inspector, wiring_an_input_moves_its_state_in_the_panel)
{
	EditorModel model;
	const cggraph::NodeId source = model.AddNode ("mesh.io.load", 0.0f, 0.0f);
	const cggraph::NodeId smooth = model.AddNode ("mesh.smooth.laplacian", 200.0f, 0.0f);
	ASSERT_NE (cggraph::kInvalidNodeId, source);
	ASSERT_NE (cggraph::kInvalidNodeId, smooth);
	model.Select (smooth);
	ASSERT_EQ (cggraph::PortState::MissingRequired, FindInput (model.GetInspector (), "maillage")->state);

	ASSERT_EQ (cggraph::ConnectStatus::Ok, model.Connect (source, 0, smooth, 0));
	EXPECT_EQ (cggraph::PortState::Connected, FindInput (model.GetInspector (), "maillage")->state);

	// La zone reste libre, et le noeud devient pret quand meme : c'est ce que
	// « optionnelle » veut dire, et un panneau qui l'oublierait exigerait un
	// cablage qui n'a pas lieu d'etre.
	EXPECT_EQ (cggraph::PortState::MissingOptional, FindInput (model.GetInspector (), "zone")->state);
	EXPECT_EQ (cggraph::NodeReadiness::Ready, model.GetInspector ().GetReadiness ());

	ASSERT_TRUE (model.Disconnect (smooth, 0));
	EXPECT_EQ (cggraph::PortState::MissingRequired, FindInput (model.GetInspector (), "maillage")->state);
}

TEST (TEST_cggraph_ui_inspector, a_field_writes_THROUGH_to_the_parameter_of_the_node)
{
	// Sans ce cas, l'inspecteur pourrait publier une copie et l'edition ne
	// changerait rien -- un panneau qu'on regle et qui n'a aucun effet.
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	model.Select (id);

	const ParamField *iterations = FindParam (model.GetInspector (), "iterations");
	ASSERT_NE (nullptr, iterations);
	ASSERT_EQ (cggraph::ParamType::Int, iterations->type);
	iterations->value->intValue = 17;

	const cggraph::Node *node = model.GetGraph ().FindNode (id);
	ASSERT_NE (nullptr, node);
	const cggraph::ParamValue *stored = node->GetParams ().Find ("iterations");
	ASSERT_NE (nullptr, stored);
	EXPECT_EQ (17, stored->intValue);
}

TEST (TEST_cggraph_ui_inspector, a_driven_value_is_published_as_an_expression_and_a_non_semantic_one_says_so)
{
	// Les deux etats qu'un panneau doit rendre autrement qu'un entier ordinaire :
	// une expression que personne ne sait evaluer, et un reglage qui ne
	// recalcule rien.
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	cggraph::Node *node = model.GetGraph ().FindNode (id);
	ASSERT_NE (nullptr, node);
	node->GetParams ().SetDriven ("lambda", cggraph::ParamType::Float, "frame * 0.01");
	node->GetParams ().SetInt ("apercu", 4, cggraph::ParamRole::NonSemantic);
	model.Select (id);

	const ParamField *driven = FindParam (model.GetInspector (), "lambda");
	ASSERT_NE (nullptr, driven);
	EXPECT_EQ (cggraph::ParamKind::Driven, driven->kind);
	EXPECT_EQ ("frame * 0.01", driven->value->expression);

	const ParamField *preview = FindParam (model.GetInspector (), "apercu");
	ASSERT_NE (nullptr, preview);
	EXPECT_EQ (cggraph::ParamRole::NonSemantic, preview->role);

	const ParamField *semantic = FindParam (model.GetInspector (), "iterations");
	ASSERT_NE (nullptr, semantic);
	EXPECT_EQ (cggraph::ParamKind::Literal, semantic->kind);
	EXPECT_EQ (cggraph::ParamRole::Semantic, semantic->role);
}

TEST (TEST_cggraph_ui_inspector, the_parameters_are_published_in_declaration_order)
{
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	model.Select (id);

	const cggraph::Node *node = model.GetGraph ().FindNode (id);
	ASSERT_NE (nullptr, node);
	const std::deque<cggraph::ParamEntry> &entries = node->GetParams ().GetEntries ();
	ASSERT_EQ (PublicParamCount (*node), model.GetInspector ().GetParams ().size ());
	std::size_t i = 0;
	for (const cggraph::ParamEntry &entry : entries)
	{
		if (entry.visibility != cggraph::ParamVisibility::Public)
			continue;
		EXPECT_EQ (entry.name, model.GetInspector ().GetParams ()[i].name);
		++i;
	}
}

// ---------------------------------------------------------------------------
//  P4 -- l'adresse d'une valeur liee a un panneau
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_inspector, a_bound_field_survives_ANY_growth_the_storage_allows)
{
	// C'est ici que la panne de P4 se manifesterait, et nulle part ailleurs :
	// un champ lie ECRIT a travers son pointeur. Une reallocation ne rendrait
	// pas la valeur perimee, elle ecrirait dans de la memoire liberee, sans
	// qu'aucune assertion ne s'en apercoive.
	//
	// La croissance provoquee depasse de loin le noeud le plus charge du depot
	// (22 parametres) : le critere demande toute croissance que le stockage
	// autorise, pas une croissance plausible.
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	model.Select (id);

	const ParamField *iterations = FindParam (model.GetInspector (), "iterations");
	const ParamField *lambda = FindParam (model.GetInspector (), "lambda");
	ASSERT_NE (nullptr, iterations);
	ASSERT_NE (nullptr, lambda);

	cggraph::ParamValue *boundInt = iterations->value;
	cggraph::ParamValue *boundFloat = lambda->value;
	boundInt->intValue = 3;
	boundFloat->floatValue = 0.25f;

	cggraph::Node *node = model.GetGraph ().FindNode (id);
	ASSERT_NE (nullptr, node);
	for (int i = 0; i < 1000; ++i)
		node->GetParams ().SetInt ("croissance_" + std::to_string (i), i);
	ASSERT_EQ (1002u, node->GetParams ().GetCount ());

	// L'adresse d'abord : le panneau tient encore le meme objet.
	EXPECT_EQ (boundInt, node->GetParams ().Find ("iterations"));
	EXPECT_EQ (boundFloat, node->GetParams ().Find ("lambda"));

	// Puis l'ecriture a travers elle, qui est le vrai danger.
	boundInt->intValue = 42;
	boundFloat->floatValue = 0.75f;
	EXPECT_EQ (42, node->GetParams ().Find ("iterations")->intValue);
	EXPECT_FLOAT_EQ (0.75f, node->GetParams ().Find ("lambda")->floatValue);

	// Et le panneau non reconstruit designe toujours les memes objets.
	EXPECT_EQ (boundInt, FindParam (model.GetInspector (), "iterations")->value);
	EXPECT_EQ (boundFloat, FindParam (model.GetInspector (), "lambda")->value);
}

// ---------------------------------------------------------------------------
//  Validation vue du modele
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_model, an_incomplete_graph_is_editable_and_says_what_it_lacks)
{
	// Un graphe en cours d'edition est legitimement incomplet : la connexion ne
	// refuse rien, et c'est la validation qui repond, sans calculer.
	EditorModel model;
	const cggraph::NodeId smooth = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	const cggraph::NodeId save = model.AddNode ("mesh.io.save", 200.0f, 0.0f);
	ASSERT_EQ (cggraph::ConnectStatus::Ok, model.Connect (smooth, 0, save, 0));

	const cggraph::BranchValidation validation = model.Validate (save);
	EXPECT_EQ (cggraph::NodeReadiness::MissingRequiredInput, validation.readiness);
	EXPECT_EQ (1u, validation.GetMissingRequiredCount ());
	ASSERT_EQ (2u, validation.nodes.size ());
	EXPECT_EQ (smooth, validation.nodes[0].node);
	EXPECT_EQ ("maillage", validation.nodes[0].inputs[0].name);

	// L'evaluation dit la meme chose, en s'arretant au premier manque et sans
	// que le canvas ait eu besoin de la lancer pour l'apprendre.
	const cggraph::EvalResult result = model.Evaluate (save);
	EXPECT_EQ (cggraph::EvalStatus::MissingInput, result.status);
	EXPECT_EQ (smooth, result.node);
	EXPECT_EQ ("maillage", result.detail);
}

// ---------------------------------------------------------------------------
//  Le modele derriere le fil de calcul
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_model, a_requested_evaluation_returns_at_once_and_arrives_by_poll)
{
	EditorModel model;
	const cggraph::NodeId load = model.AddNode ("mesh.io.load", 0.0f, 0.0f);
	ASSERT_NE (load, cggraph::kInvalidNodeId);

	// La demande rend la main SANS avoir calcule : rien n'est encore arrive, et
	// c'est ce que l'interface gagne au fil separe.
	model.RequestEvaluation (load);
	EXPECT_FALSE (model.Poll ());

	// Le resultat arrive plus tard, par la lecture de frame. La fenetre de
	// coalescence par defaut vaut 150 ms ; l'attente est bornee bien au-dela.
	bool arrived = false;
	for (int tick = 0; tick < 400 && !arrived; ++tick)
	{
		arrived = model.Poll ();
		if (!arrived)
			std::this_thread::sleep_for (std::chrono::milliseconds (10));
	}
	ASSERT_TRUE (arrived);

	// Le chemin est vide : le noeud echoue, et c'est un resultat comme un autre.
	EXPECT_EQ (cggraph::EvalStatus::ComputeFailed, model.GetLastResult ().status);
	EXPECT_FALSE (model.IsEvaluating ());
}

// ---------------------------------------------------------------------------
//  Visibilite : ce que le panneau montre et ce qu'il tait
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_inspector, an_internal_parameter_gets_no_field_at_all)
{
	// `source.identity` doit rester Semantic -- c'est lui qui invalide le cache
	// -- et n'a rien a faire dans un panneau. Sans un second axe, l'un des deux
	// serait faux.
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.io.load", 0.0f, 0.0f);
	ASSERT_NE (cggraph::kInvalidNodeId, id);
	model.Select (id);

	const cggraph::Node *node = model.GetGraph ().FindNode (id);
	ASSERT_NE (nullptr, node);

	// Le noeud le porte, semantique, et le panneau ne le publie pas : les deux
	// moities du cas, sans lesquelles supprimer le parametre le satisferait.
	const cggraph::ParamEntry *stored = node->GetParams ().FindEntry ("source.identity");
	ASSERT_NE (nullptr, stored);
	EXPECT_EQ (cggraph::ParamRole::Semantic, stored->role);
	EXPECT_EQ (nullptr, FindParam (model.GetInspector (), "source.identity"));

	// Les publics du meme noeud, eux, y sont.
	EXPECT_NE (nullptr, FindParam (model.GetInspector (), "path"));
	EXPECT_NE (nullptr, FindParam (model.GetInspector (), "verifyHash"));
	EXPECT_EQ (node->GetParams ().GetCount () - 1u, model.GetInspector ().GetParams ().size ());
}

TEST (TEST_cggraph_ui_inspector, at_least_one_catalog_node_carries_an_internal_parameter)
{
	// Temoin du filtre : le jour ou plus aucun noeud du catalogue ne porterait
	// d'interne, tous les cas ci-dessus passeraient sans rien etablir.
	EditorModel model;
	std::size_t hidden = 0;
	for (const PaletteCategory &category : model.GetPalette ().GetCategories ())
		for (const PaletteItem &item : category.items)
		{
			const cggraph::NodeId id = model.AddNode (item.typeName, 0.0f, 0.0f);
			ASSERT_NE (cggraph::kInvalidNodeId, id);
			const cggraph::Node *node = model.GetGraph ().FindNode (id);
			ASSERT_NE (nullptr, node);
			hidden += node->GetParams ().GetCount () - PublicParamCount (*node);
		}
	EXPECT_GT (hidden, 0u);
}

// ---------------------------------------------------------------------------
//  Suppression vue du modele
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_model, removing_the_selected_node_empties_the_panel)
{
	// L'inspecteur pointe DANS les parametres du noeud. Le laisser sur un noeud
	// detruit ne rendrait pas des valeurs perimees, il lirait de la memoire
	// liberee au rendu suivant.
	EditorModel model;
	const cggraph::NodeId source = model.AddNode ("mesh.io.load", 0.0f, 0.0f);
	const cggraph::NodeId smooth = model.AddNode ("mesh.smooth.laplacian", 200.0f, 0.0f);
	ASSERT_EQ (cggraph::ConnectStatus::Ok, model.Connect (source, 0, smooth, 0));

	model.Select (smooth);
	ASSERT_EQ (smooth, model.GetInspector ().GetNode ());

	EXPECT_EQ (cggraph::RemoveStatus::Ok, model.RemoveNode (smooth));
	EXPECT_EQ (cggraph::kInvalidNodeId, model.GetSelection ());
	EXPECT_EQ (cggraph::kInvalidNodeId, model.GetInspector ().GetNode ());
	EXPECT_TRUE (model.GetInspector ().GetParams ().empty ());
	EXPECT_EQ (1u, model.GetGraph ().GetNodeCount ());
	EXPECT_TRUE (model.GetGraph ().GetLinks ().empty ());
}

TEST (TEST_cggraph_ui_model, removing_an_upstream_node_updates_the_panel_of_the_one_that_stays)
{
	// La selection survit, mais son cablage a change : un panneau non
	// reconstruit continuerait d'afficher une entree alimentee par un noeud qui
	// n'existe plus.
	EditorModel model;
	const cggraph::NodeId source = model.AddNode ("mesh.io.load", 0.0f, 0.0f);
	const cggraph::NodeId smooth = model.AddNode ("mesh.smooth.laplacian", 200.0f, 0.0f);
	ASSERT_EQ (cggraph::ConnectStatus::Ok, model.Connect (source, 0, smooth, 0));

	model.Select (smooth);
	ASSERT_EQ (cggraph::PortState::Connected, FindInput (model.GetInspector (), "maillage")->state);

	EXPECT_EQ (cggraph::RemoveStatus::Ok, model.RemoveNode (source));
	EXPECT_EQ (smooth, model.GetSelection ());
	EXPECT_EQ (cggraph::PortState::MissingRequired,
	           FindInput (model.GetInspector (), "maillage")->state);
	EXPECT_EQ (cggraph::NodeReadiness::MissingRequiredInput, model.GetInspector ().GetReadiness ());
}

TEST (TEST_cggraph_ui_model, removing_an_unknown_node_is_named_and_leaves_the_selection_alone)
{
	EditorModel model;
	const cggraph::NodeId id = model.AddNode ("mesh.smooth.laplacian", 0.0f, 0.0f);
	model.Select (id);

	EXPECT_EQ (cggraph::RemoveStatus::UnknownNode, model.RemoveNode (999));
	EXPECT_EQ (id, model.GetSelection ());
	EXPECT_EQ (id, model.GetInspector ().GetNode ());
	EXPECT_EQ (1u, model.GetGraph ().GetNodeCount ());
}

// ===========================================================================
//  Le pilote d'evaluation -- et le fait qu'un hote SANS FILS calcule quand meme
// ===========================================================================
//
// Ces cas existent parce que le repli mono-fil vit sur une cible que `TU` ne
// lie pas. Ecrits sur EditorModel seul, ils n'auraient couvert que le pilote a
// fil et auraient laisse le chemin WebAssembly sans filet -- une case cochee
// sur une zone non couverte. Le pilote etant un TYPE et non un etat, il
// s'instancie nativement, et c'est toute la raison de sa forme.

namespace
{

// Une chaine qui CALCULE vraiment, et qui rend de la progression : un
// generateur, puis le seul adaptateur du catalogue qui appelle ctx.Progress.
struct ProgressChain
{
	cggraph::Graph graph;
	cggraph::NodeId shape = cggraph::kInvalidNodeId;
	cggraph::NodeId smooth = cggraph::kInvalidNodeId;

	// ⚠ LA FIXTURE S'ASSERTE ELLE-MEME. Sans ces trois lignes, un catalogue qui
	// renommerait « shape.torus » rendrait des noeuds nuls et un cablage refuse :
	// les cas ci-dessous continueraient de tourner, sur un graphe vide, et
	// certains resteraient verts en n'etablissant plus rien.
	ProgressChain ()
	{
		shape = graph.AddNode (cggraph_nodes::MakeNode ("shape.torus"));
		smooth = graph.AddNode (cggraph_nodes::MakeNode ("mesh.smooth.laplacian"));
		//  EXPECT et non ASSERT : ces macros-ci ne rendent pas la main, et un
		//  ASSERT dans un constructeur est refuse par MSVC (C2534).
		EXPECT_NE (cggraph::kInvalidNodeId, shape);
		EXPECT_NE (cggraph::kInvalidNodeId, smooth);
		EXPECT_NE (nullptr, graph.FindNode (smooth)->GetParams ().SetInt ("iterations", 3));
		EXPECT_EQ (cggraph::ConnectStatus::Ok, graph.Connect (shape, 0, smooth, 0));
	}
};

} // namespace

TEST (TEST_cggraph_ui_driver, the_inline_driver_computes_in_pump_and_nowhere_else)
{
	// LA propriete du repli : Request rend la main sans avoir calcule, comme sur
	// un fil -- mais rien n'avancera tant que l'hote n'aura pas pompe. Un
	// Request qui calculerait sur place rendrait 1 des la premiere assertion, et
	// il appellerait le collecteur de progression AU MILIEU de la frame de
	// l'hote.
	ProgressChain chain;
	InlineEvalDriver driver (chain.graph);
	driver.SetDebounce (std::chrono::milliseconds (0));

	driver.RequestNow (chain.smooth);
	EXPECT_EQ (0u, driver.GetStartedCount ());
	EXPECT_TRUE (driver.Drain ().empty ());
	EXPECT_FALSE (driver.IsBusy ());
	EXPECT_FALSE (driver.IsIdle ());

	driver.Pump ();
	EXPECT_EQ (1u, driver.GetStartedCount ());
	EXPECT_TRUE (driver.IsIdle ());

	const std::vector<EvalDriver::Completed> done = driver.Drain ();
	ASSERT_EQ (1u, done.size ());
	EXPECT_EQ (chain.smooth, done[0].node);
	EXPECT_EQ (cggraph::EvalStatus::Ok, done[0].result.status);

	// Un second pompage sans demande ne relance rien : la demande est CONSOMMEE.
	driver.Pump ();
	EXPECT_EQ (1u, driver.GetStartedCount ());
	EXPECT_TRUE (driver.Drain ().empty ());
}

TEST (TEST_cggraph_ui_driver, an_inline_burst_coalesces_into_a_single_evaluation)
{
	// Meme instrument que le pilote a fil, meme raison : un curseur qu'on
	// deplace ne doit pas faire calculer ses etats intermediaires. L'horloge est
	// FIGEE, sans quoi le cas mesurerait l'ordonnanceur autant que le code.
	ProgressChain chain;
	InlineEvalDriver driver (chain.graph);

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now ();
	driver.SetClock ([&now] { return now; });
	driver.SetDebounce (std::chrono::milliseconds (150));

	for (int i = 0; i < 20; ++i)
	{
		driver.Request (chain.smooth);
		driver.Pump (); // l'hote pompe a chaque frame, la fenetre n'est pas ecoulee
	}
	EXPECT_EQ (0u, driver.GetStartedCount ());

	now += std::chrono::milliseconds (151);
	driver.Pump ();
	EXPECT_EQ (1u, driver.GetStartedCount ());
	EXPECT_EQ (1u, driver.Drain ().size ());
}

TEST (TEST_cggraph_ui_driver, the_progress_sink_is_accepted_inline_and_refused_on_a_thread)
{
	// Le refus est le contrat, pas un manque : sur un pilote a fil, le
	// collecteur serait appele DEPUIS le fil de calcul. Un `void` aurait fait de
	// ce refus un silence, et un hote qui installe un pompage de frame sur un
	// pilote a fil ne l'aurait jamais su.
	ProgressChain a;
	InlineEvalDriver inlineDriver (a.graph);
	EXPECT_TRUE (inlineDriver.SetProgressSink ([] (float, const char *) {}));

	ProgressChain b;
	ThreadedEvalDriver threaded (b.graph);
	EXPECT_FALSE (threaded.SetProgressSink ([] (float, const char *) {}));

	// Et le refus n'ampute rien d'autre : le pilote a fil calcule toujours.
	threaded.RequestNow (b.smooth);
	threaded.WaitIdle ();
	EXPECT_EQ (1u, threaded.GetStartedCount ());
}

TEST (TEST_cggraph_ui_driver, the_host_sink_sees_the_progress_of_a_running_evaluation)
{
	// C'est le pompage de frame du 7.3 sur un hote mono-fil : il n'y a pas
	// d'autre fil pour RELEVER la progression, donc le collecteur POUSSE. Les
	// deux moities sont verifiees -- que l'hote soit appele, et qu'il le soit
	// pendant que le calcul tourne, pas apres.
	ProgressChain chain;
	InlineEvalDriver driver (chain.graph);
	driver.SetDebounce (std::chrono::milliseconds (0));

	unsigned int ticks = 0;
	unsigned int busyDuringTicks = 0;
	float last = -1.0f;
	std::string label;
	ASSERT_TRUE (driver.SetProgressSink ([&] (float t, const char *name) {
		++ticks;
		if (driver.IsBusy ())
			++busyDuringTicks;
		last = t;
		label = name != nullptr ? name : "";
	}));

	driver.RequestNow (chain.smooth);
	driver.Pump ();

	EXPECT_EQ (3u, ticks); // une par iteration demandee
	EXPECT_EQ (ticks, busyDuringTicks);
	EXPECT_FLOAT_EQ (1.0f, last);
	EXPECT_EQ ("mesh.smooth.laplacian", label);

	// La progression deposee reste lisible apres coup, comme sur le pilote a
	// fil : l'hote qui ne pousse pas peut toujours relever.
	EXPECT_EQ (chain.smooth, driver.GetProgress ().node);
	EXPECT_FLOAT_EQ (1.0f, driver.GetProgress ().t);
}

TEST (TEST_cggraph_ui_driver, a_request_from_the_sink_cancels_the_running_one_and_the_reentrant_pump_does_nothing)
{
	// Sur un hote mono-fil, le collecteur rappelle du code exterieur PENDANT le
	// calcul : c'est le seul chemin de reentrance qui existe la-bas. Deux
	// mecanismes distincts s'y rencontrent, et ce cas les separe.
	//
	// ⚠ MESURE, PAS PREDICTION -- la premiere ecriture de ce cas attendait `Ok`
	// et a ete DEMENTIE. Une demande posee pendant un calcul annule ce calcul,
	// exactement comme AsyncEvaluator::Request le fait sur un fil : « son
	// resultat ne serait plus celui qu'on demande ». Le resultat rendu est donc
	// `Aborted`, apres UNE seule iteration de lissage sur les trois demandees.
	// Les deux pilotes se piegent de la meme facon, et c'est ce qu'on veut.
	//
	// Ce que la GARDE de reentrance ajoute par-dessus : le Pump reentrant ne
	// lance rien. Sans elle, il rentrerait dans l'evaluateur, en ressortirait
	// par EvalStatus::Busy, et deposerait ce refus dans la file de resultats de
	// l'appelant EXTERNE, qui n'a rien demande de tel.
	ProgressChain chain;
	InlineEvalDriver driver (chain.graph);
	driver.SetDebounce (std::chrono::milliseconds (0));

	unsigned int ticks = 0;
	ASSERT_TRUE (driver.SetProgressSink ([&] (float, const char *) {
		++ticks;
		driver.RequestNow (chain.shape);
		driver.Pump (); // reentrant : doit etre sans effet
	}));

	driver.RequestNow (chain.smooth);
	driver.Pump ();

	// Une seule iteration a eu lieu : la demande reentrante a leve le drapeau.
	EXPECT_EQ (1u, ticks);

	// LA propriete de la garde : une seule evaluation lancee, un seul resultat
	// depose. Les deux comptes sont les observables du Pump reentrant.
	EXPECT_EQ (1u, driver.GetStartedCount ());

	const std::vector<EvalDriver::Completed> done = driver.Drain ();
	ASSERT_EQ (1u, done.size ());
	EXPECT_EQ (chain.smooth, done[0].node);
	EXPECT_EQ (cggraph::EvalStatus::Aborted, done[0].result.status);

	// La demande posee depuis le collecteur, elle, a bien survecu : elle attend,
	// et le pompage suivant la sert. Sans quoi la reentrance perdrait la demande
	// au lieu de la differer.
	EXPECT_FALSE (driver.IsIdle ());
	driver.Pump ();
	EXPECT_EQ (2u, driver.GetStartedCount ());
	const std::vector<EvalDriver::Completed> after = driver.Drain ();
	ASSERT_EQ (1u, after.size ());
	EXPECT_EQ (chain.shape, after[0].node);
	EXPECT_EQ (cggraph::EvalStatus::Ok, after[0].result.status);
}

TEST (TEST_cggraph_ui_driver, cancelling_inline_drops_the_request_that_was_waiting)
{
	ProgressChain chain;
	InlineEvalDriver driver (chain.graph);
	driver.SetDebounce (std::chrono::milliseconds (0));

	driver.RequestNow (chain.smooth);
	EXPECT_FALSE (driver.IsIdle ());
	driver.Cancel ();
	EXPECT_TRUE (driver.IsIdle ());

	driver.Pump ();
	EXPECT_EQ (0u, driver.GetStartedCount ());
	EXPECT_TRUE (driver.Drain ().empty ());
}

// ---------------------------------------------------------------------------
//  Le modele au-dessus des deux pilotes -- meme API, memes reponses
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_ui_model, the_five_methods_answer_the_same_thing_on_a_host_without_threads)
{
	// Ce cas est la raison d'etre du pilote en ligne : la cible WebAssembly ne
	// lie aucun test, et sans lui son EditorModel serait un objet dont personne
	// n'aurait jamais verifie une methode. Les deux modeles construisent le meme
	// graphe et sont interroges par les MEMES appels.
	EditorModel threaded (EvalMode::Threaded);
	EditorModel inlined (EvalMode::Inline);

	EditorModel *models[2] = { &threaded, &inlined };
	cggraph::EvalStatus status[2] = { cggraph::EvalStatus::Busy, cggraph::EvalStatus::Busy };
	unsigned int reached[2] = { 0u, 0u };

	for (int k = 0; k < 2; ++k)
	{
		EditorModel &model = *models[k];
		const cggraph::NodeId shape = model.AddNode ("shape.torus", 0.0f, 0.0f);
		const cggraph::NodeId smooth = model.AddNode ("mesh.smooth.laplacian", 200.0f, 0.0f);
		ASSERT_NE (cggraph::kInvalidNodeId, shape);
		ASSERT_NE (cggraph::kInvalidNodeId, smooth);
		ASSERT_EQ (cggraph::ConnectStatus::Ok, model.Connect (shape, 0, smooth, 0));

		EXPECT_FALSE (model.IsEvaluating ());
		EXPECT_EQ (cggraph::kInvalidNodeId, model.GetProgress ().node);

		model.RequestEvaluation (smooth);
		EXPECT_FALSE (model.Poll ());

		bool arrived = false;
		for (int tick = 0; tick < 400 && !arrived; ++tick)
		{
			model.Pump ();
			arrived = model.Poll ();
			if (!arrived)
				std::this_thread::sleep_for (std::chrono::milliseconds (10));
		}
		ASSERT_TRUE (arrived) << "pilote " << k;

		status[k] = model.GetLastResult ().status;
		reached[k] = model.GetProgress ().node == smooth ? 1u : 0u;
		EXPECT_FALSE (model.IsEvaluating ());

		// Annuler quand il n'y a rien a annuler ne casse ni l'un ni l'autre.
		model.CancelEvaluation ();
		EXPECT_FALSE (model.IsEvaluating ());
	}

	EXPECT_EQ (cggraph::EvalStatus::Ok, status[0]);
	EXPECT_EQ (status[0], status[1]);
	EXPECT_EQ (1u, reached[0]);
	EXPECT_EQ (reached[0], reached[1]);
}

TEST (TEST_cggraph_ui_model, an_inline_model_stays_silent_until_the_host_pumps)
{
	// Le mecanisme que Pump () existe pour rendre visible : un hote qui ne pompe
	// pas ne calcule JAMAIS sur cette cible, alors qu'il calcule normalement sur
	// l'autre. Sans ce cas, l'oubli ne se verrait qu'au navigateur.
	EditorModel model (EvalMode::Inline);
	const cggraph::NodeId shape = model.AddNode ("shape.torus", 0.0f, 0.0f);
	ASSERT_NE (cggraph::kInvalidNodeId, shape);

	model.RequestEvaluation (shape);
	for (int tick = 0; tick < 20; ++tick)
	{
		EXPECT_FALSE (model.Poll ());
		std::this_thread::sleep_for (std::chrono::milliseconds (10));
	}

	model.Pump ();
	ASSERT_TRUE (model.Poll ());
	EXPECT_EQ (cggraph::EvalStatus::Ok, model.GetLastResult ().status);
}

TEST (TEST_cggraph_ui_model, the_progress_sink_refusal_travels_up_to_the_model)
{
	// L'hote installe son pompage de frame par le MODELE, et c'est le modele qui
	// lui rend le refus du pilote a fil. Le taire ici rendrait le refus
	// inatteignable depuis l'endroit ou un hote l'appelle.
	EditorModel threaded (EvalMode::Threaded);
	EditorModel inlined (EvalMode::Inline);
	EXPECT_FALSE (threaded.SetProgressSink ([] (float, const char *) {}));
	EXPECT_TRUE (inlined.SetProgressSink ([] (float, const char *) {}));
}

TEST (TEST_cggraph_ui_model, the_revision_and_the_outputs_are_what_a_host_with_a_preview_reads)
{
	// Les deux accesseurs que l'hote web ajoute, et le motif de leur existence :
	// c'est le CANVAS qui appelle Poll, pas l'hote. Un hote qui affiche le
	// resultat ne voit donc jamais passer le booleen de Poll, et sans compteur il
	// televerserait le maillage a chaque frame ou jamais.
	//
	// ⚠ Ce cas est le seul filet de ces deux accesseurs, et il tourne en CI
	// alors que leur consommateur, lui, n'y tourne pas.
	EditorModel model (EvalMode::Inline);
	const cggraph::NodeId shape = model.AddNode ("shape.torus", 0.0f, 0.0f);
	ASSERT_NE (cggraph::kInvalidNodeId, shape);

	EXPECT_EQ (0u, model.GetResultRevision ());
	EXPECT_TRUE (model.GetLastOutputs ().empty ());

	ASSERT_EQ (cggraph::EvalStatus::Ok, model.Evaluate (shape).status);
	EXPECT_EQ (1u, model.GetResultRevision ());
	ASSERT_EQ (1u, model.GetLastOutputs ().size ());

	// La sortie est bien la geometrie que l'apercu televersera, et elle est
	// PLEINE : un Value vide passerait le test de type sans rien porter.
	//
	// ⚠ Le maillage n'est pas DEREFERENCE ici, et ce n'est pas de la paresse :
	// inclure cgmesh/mesh.h dans ce fichier rend `Palette` ambigu -- cgmesh en
	// declare une, cggraph_ui aussi, et le fichier ouvre `using namespace
	// cggraph_ui`. Le type et la taille suffisent a ce qu'on veut etablir ; ce
	// que le maillage contient se teste dans tu_cggraph_nodes.
	const cggraph::Value &output = model.GetLastOutputs ()[0];
	EXPECT_EQ (cggraph_nodes::Types ().mesh, output.GetType ());
	EXPECT_FALSE (output.IsEmpty ());
	EXPECT_GT (output.GetSizeHint (), 0u);

	// Une frame qui ne rapporte rien ne fait pas bouger la revision : sans quoi
	// l'hote televerserait le meme maillage a chaque tour de boucle.
	EXPECT_FALSE (model.Poll ());
	EXPECT_EQ (1u, model.GetResultRevision ());

	// Un second calcul la fait avancer d'exactement un.
	//
	// ⚠ MESURE, PAS PREDICTION -- la premiere ecriture de ce cas pompait UNE
	// fois et attendait le resultat. RequestEvaluation est COALESCEE : sa
	// fenetre de 150 ms n'etait pas ecoulee, et le pompage unique ne servait
	// rien. C'est le comportement voulu, et c'est le meme des deux cotes du
	// pilote ; le cas boucle donc, comme le ferait une boucle de frames.
	model.RequestEvaluation (shape);
	EXPECT_FALSE (model.Poll ());
	bool arrived = false;
	for (int tick = 0; tick < 400 && !arrived; ++tick)
	{
		model.Pump ();
		arrived = model.Poll ();
		if (!arrived)
			std::this_thread::sleep_for (std::chrono::milliseconds (10));
	}
	ASSERT_TRUE (arrived);
	EXPECT_EQ (2u, model.GetResultRevision ());
}
