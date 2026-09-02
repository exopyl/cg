#include "editor_model.h"

#include <utility>

#include "../core/serialize.h"
#include "../nodes/catalog.h"
#include "../nodes/catalog_factory.h"

namespace cggraph_ui
{

EditorModel::EditorModel (EvalMode mode) : m_driver (MakeEvalDriver (m_graph, mode))
{
}

EditorModel::EditorModel (const DriverFactory &makeDriver) : m_driver (makeDriver (m_graph))
{
}

std::unique_ptr<EditorModel> EditorModel::OpenDocument (const std::string &path,
                                                        std::string &detail)
{
	detail.clear ();
	if (path.empty ())
	{
		detail = "reference vide";
		return nullptr;
	}

	std::unique_ptr<EditorModel> model (new EditorModel ());
	const cggraph_nodes::CatalogFactory factory;
	const cggraph::LoadResult result =
		cggraph::LoadGraphFromFile (path, factory, model->GetGraph ());
	if (!result.IsOk ())
	{
		// Le statut, et le detail SEULEMENT s'il ajoute quelque chose. Sur un
		// fichier illisible LoadGraph nomme le chemin, que l'appelant vient de
		// passer : le repeter donnerait « repeat_body.json : fichier illisible :
		// repeat_body.json ».
		detail = std::string (cggraph::ToString (result.status));
		if (!result.detail.empty () && result.detail != path)
			detail += " : " + result.detail;
		return nullptr;
	}
	return model;
}

std::unique_ptr<EditorModel> EditorModel::OpenDocumentText (const std::string &document,
                                                            std::string &detail)
{
	detail.clear ();
	if (document.empty ())
	{
		detail = "document vide";
		return nullptr;
	}

	std::unique_ptr<EditorModel> model (new EditorModel ());
	const cggraph_nodes::CatalogFactory factory;
	const cggraph::LoadResult result =
		cggraph::LoadGraph (document, factory, model->GetGraph ());
	if (!result.IsOk ())
	{
		detail = std::string (cggraph::ToString (result.status));
		if (!result.detail.empty ())
			detail += " : " + result.detail;
		return nullptr;
	}
	return model;
}

cggraph::NodeId EditorModel::AddNode (const std::string &typeName, float x, float y)
{
	std::unique_ptr<cggraph::Node> node = cggraph_nodes::MakeNode (typeName);
	if (node == nullptr)
		return cggraph::kInvalidNodeId;

	const cggraph::NodeId id = m_graph.AddNode (std::move (node));
	if (id != cggraph::kInvalidNodeId)
		m_graph.SetNodePosition (id, x, y);
	return id;
}

cggraph::ConnectStatus EditorModel::Connect (cggraph::NodeId from, cggraph::PortIdx fromPort,
                                             cggraph::NodeId to, cggraph::PortIdx toPort)
{
	const cggraph::ConnectStatus status = m_graph.Connect (from, fromPort, to, toPort);
	if (status == cggraph::ConnectStatus::Ok)
		RefreshInspector ();
	return status;
}

bool EditorModel::Disconnect (cggraph::NodeId to, cggraph::PortIdx toPort)
{
	const bool done = m_graph.Disconnect (to, toPort);
	if (done)
		RefreshInspector ();
	return done;
}

cggraph::RemoveStatus EditorModel::RemoveNode (cggraph::NodeId id)
{
	const cggraph::RemoveStatus status = m_graph.RemoveNode (id);
	if (status != cggraph::RemoveStatus::Ok)
		return status;

	if (m_selection == id)
		m_selection = cggraph::kInvalidNodeId;

	// Dans les deux cas : la suppression a pu emporter un lien qui alimentait le
	// noeud selectionne, et l'etat de ses ports a donc change.
	RefreshInspector ();
	return status;
}

void EditorModel::Select (cggraph::NodeId id)
{
	m_selection = id;
	RefreshInspector ();
}

void EditorModel::RefreshInspector ()
{
	if (m_selection == cggraph::kInvalidNodeId)
	{
		m_inspector.Clear ();
		return;
	}
	m_inspector.Build (m_palette, m_graph, m_selection);
}

cggraph::BranchValidation EditorModel::Validate (cggraph::NodeId id) const
{
	return cggraph::ValidateBranch (m_graph, id);
}

cggraph::EvalResult EditorModel::Evaluate (cggraph::NodeId id)
{
	m_driver->RequestNow (id);
	m_driver->WaitIdle ();
	Poll ();
	return m_lastResult;
}

void EditorModel::RequestEvaluation (cggraph::NodeId id)
{
	m_driver->Request (id);
}

bool EditorModel::Poll ()
{
	const std::vector<EvalDriver::Completed> ready = m_driver->Drain ();
	if (ready.empty ())
		return false;

	// Le DERNIER seul : les precedents ont ete remplaces par une demande plus
	// recente, et les afficher ferait clignoter l'interface sur des etats que
	// personne n'a demandes.
	const EvalDriver::Completed &last = ready.back ();
	m_lastResult = last.result;
	m_outputs = last.outputs;
	// Le noeud QUI a ete calcule -- `last.result.node` ne nomme qu'un refus, et
	// vaut kInvalidNodeId quand tout s'est bien passe.
	m_lastEvaluated = last.node;
	// FUSION et non remplacement : cf. m_previews dans l'en-tete.
	for (std::map<cggraph::NodeId, cggraph::Thumbnail>::const_iterator it = last.previews.begin ();
	     it != last.previews.end (); ++it)
		m_previews[it->first] = it->second;
	++m_revision;
	return true;
}

void EditorModel::CancelEvaluation ()
{
	m_driver->Cancel ();
}

bool EditorModel::IsEvaluating () const
{
	return m_driver->IsBusy ();
}

cggraph::AsyncEvaluator::Progress EditorModel::GetProgress () const
{
	return m_driver->GetProgress ();
}

// Un no-op sur un pilote a fil, le seul lieu de calcul sur un pilote en ligne.
// L'hote appelle sans savoir lequel il tient.
void EditorModel::Pump ()
{
	m_driver->Pump ();
}

bool EditorModel::SetProgressSink (cggraph::EvalContext::ProgressSink sink)
{
	return m_driver->SetProgressSink (std::move (sink));
}

} // namespace cggraph_ui

namespace cggraph_ui
{

const cggraph::Thumbnail *EditorModel::GetPreview (cggraph::NodeId id) const
{
	std::map<cggraph::NodeId, cggraph::Thumbnail>::const_iterator it = m_previews.find (id);
	return it == m_previews.end () ? nullptr : &it->second;
}

} // namespace cggraph_ui
