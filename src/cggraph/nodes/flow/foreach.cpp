#include "foreach.h"

#include <string>

#include "../../../cgmesh/mesh.h"
#include "../value_types.h"

namespace cggraph_nodes
{
namespace flow
{

ForEachNode::ForEachNode ()
	: SubgraphHostNode ("flow.foreach")
{
}

void ForEachNode::PublishPorts (const SubgraphInstance &instance)
{
	m_desc.inputs.clear ();
	m_desc.outputs.clear ();
	for (const Boundary &boundary : instance.inputs)
		m_desc.inputs.push_back ({ boundary.name, Types ().meshArray, false });
	for (const Boundary &boundary : instance.outputs)
		m_desc.outputs.push_back ({ boundary.name, Types ().meshArray, false });
}

bool ForEachNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                           cggraph::ValueList &out)
{
	LoadStatus status = LoadStatus::Ok;
	std::string detail;
	const std::unique_ptr<SubgraphInstance> instance = Open (status, detail);
	if (instance == nullptr)
	{
		NoteResult (cggraph::EvalStatus::ComputeFailed, std::string (ToString (status)) + " " + detail);
		return false;
	}

	std::vector<const MeshArray *> sources;
	std::size_t count = 0;
	for (std::size_t i = 0; i < in.size (); ++i)
	{
		const MeshArray *array = in[i].Get<MeshArray> (Types ().meshArray);
		if (array == nullptr)
		{
			NoteResult (cggraph::EvalStatus::MissingInput, m_desc.inputs[i].name);
			return false;
		}
		// Les suites doivent avoir la MEME longueur : l'element k de l'une va
		// avec l'element k de l'autre, et il n'existe aucune interpretation
		// raisonnable de longueurs differentes. La plus courte serait un choix
		// silencieux, et la plus longue un acces hors bornes.
		if (i == 0)
			count = array->items.size ();
		else if (array->items.size () != count)
		{
			NoteResult (cggraph::EvalStatus::MissingInput, m_desc.inputs[i].name);
			return false;
		}
		sources.push_back (array);
	}

	std::vector<std::shared_ptr<MeshArray>> results;
	for (std::size_t j = 0; j < out.size (); ++j)
		results.push_back (std::make_shared<MeshArray> ());

	// UN evaluateur pour TOUTE la boucle, et c'est le point de conception. Les
	// branches du document qui ne dependent pas de l'element s'y calculent une
	// fois et servent count fois ; celles qui en dependent s'y recalculent, parce
	// que la cle de liaison deplace leur signature. Un evaluateur par passe
	// perdrait la premiere moitie ; pas de cle de liaison rendrait la seconde
	// FAUSSE.
	cggraph::Evaluator evaluator (instance->graph);

	for (std::size_t k = 0; k < count; ++k)
	{
		std::vector<cggraph::Value> element;
		for (const MeshArray *array : sources)
			element.push_back (
				cggraph::Value::Make (Types ().mesh, array->items[k]));

		std::vector<cggraph::Value> produced;
		const PassResult pass = RunPass (*instance, evaluator, ctx, std::to_string (k), element,
		                                 produced, SinksRequested ());
		NotePass ();
		if (!pass.IsOk ())
		{
			NoteResult (pass.status, pass.detail);
			// Les ecritures deja faites sont republiees MEME en cas d'echec : un
			// pilote doit pouvoir nommer ce qui est sur le disque, surtout quand
			// le calcul s'est arrete en chemin.
			NoteWritten (CollectWritten (*instance));
			return false;
		}

		for (std::size_t j = 0; j < results.size () && j < produced.size (); ++j)
			results[j]->items.push_back (produced[j].Share<Mesh> (Types ().mesh));
	}

	NoteResult (cggraph::EvalStatus::Ok, std::string ());
	NoteWritten (CollectWritten (*instance));
	for (std::size_t j = 0; j < out.size (); ++j)
		out[j] = cggraph::Value::Make (Types ().meshArray, results[j]);
	return true;
}

} // namespace flow
} // namespace cggraph_nodes
