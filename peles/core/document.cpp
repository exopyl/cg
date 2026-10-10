#include "document.h"

#include "mesh.h"
#include "mesh_data_manager.h"

#include <algorithm>

namespace peles
{
namespace core
{

Document::Document () = default;

Document::~Document ()
{
	// La destruction du document detruit la scene : elle passe donc par le
	// meme chemin annonce que toute autre suppression.
	Clear ();
}

void Document::AddObserver (DocumentObserver* observer)
{
	if (observer == nullptr)
		return;
	if (std::find (m_observers.begin (), m_observers.end (), observer) != m_observers.end ())
		return;
	m_observers.push_back (observer);
}

void Document::RemoveObserver (DocumentObserver* observer)
{
	m_observers.erase (std::remove (m_observers.begin (), m_observers.end (), observer),
	                   m_observers.end ());
}

Model* Document::AddModel (const std::string& name)
{
	return m_models.Add (name);
}

bool Document::Owns (const Model* model) const
{
	if (model == nullptr)
		return false;
	for (const std::unique_ptr<Model>& candidate : m_models.GetModels ())
	{
		if (candidate.get () == model)
			return true;
	}
	return false;
}

void Document::AnnounceRelease (Model& model)
{
	// ⚠ ITERATION SUR UNE COPIE. Un observateur a le droit de se retirer depuis
	// son propre rappel -- c'est meme ce que fait un objet qui se detruit -- et
	// RemoveObserver ecrit dans m_observers. Iterer le membre invaliderait
	// l'iterateur en cours.
	const std::vector<DocumentObserver*> observers = m_observers;

	for (Mesh* mesh : model.m_meshes.GetMeshes ())
	{
		if (mesh == nullptr)
			continue;
		for (DocumentObserver* observer : observers)
			observer->OnMeshReleasing (mesh);

		// ⚠ LA DONNEE DERIVEE MEURT AVEC LE MAILLAGE DONT ELLE DERIVE. Le cache
		// de cgmesh est indexe par `Mesh*` et n'est purge par personne : demi-
		// aretes, triangulation et compteurs topologiques y resteraient apres la
		// destruction du maillage, et un Mesh realloue a la meme adresse
		// heriterait de la topologie du precedent. C'est le pendant exact de ce
		// que l'observateur fait pour les tampons GL, et la seule chose qui fasse
		// tenir la memoire sur une suite d'ouvertures du meme fichier.
		MeshDataManager::GetInstance ().Invalidate (mesh);
	}
}

void Document::ForgetIfPointsTo (const Model* model)
{
	if (m_selected == model)
		m_selected = nullptr;
	if (m_hovered == model)
		m_hovered = nullptr;
}

bool Document::RemoveModel (Model* model)
{
	std::vector<std::unique_ptr<Model>>& models = m_models.GetModels ();
	for (std::size_t i = 0; i < models.size (); ++i)
	{
		if (models[i].get () != model)
			continue;

		AnnounceRelease (*model);
		ForgetIfPointsTo (model);
		return m_models.Remove (i);
	}
	return false;
}

void Document::Clear ()
{
	for (const std::unique_ptr<Model>& model : m_models.GetModels ())
	{
		if (model)
			AnnounceRelease (*model);
	}
	m_selected = nullptr;
	m_hovered  = nullptr;
	m_models.Clear ();
}

const std::vector<std::unique_ptr<Model>>& Document::Models () const
{
	return m_models.GetModels ();
}

void Document::Select (Model* model)
{
	m_selected = Owns (model) ? model : nullptr;
}

void Document::Hover (Model* model)
{
	m_hovered = Owns (model) ? model : nullptr;
}

BoundingBox Document::VisibleBounds ()
{
	return m_models.AggregateBBox (true);
}

} // namespace core
} // namespace peles
