#include "repeat.h"

#include <string>

#include "../../../cgmesh/mesh.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{
namespace flow
{

RepeatNode::RepeatNode ()
	: SubgraphHostNode ("flow.repeat")
{
	// Semantic, donc hache par HashParamSet : changer n change la signature, et
	// le cache ne peut pas servir le resultat de n = 3 a une demande de n = 4.
	GetParams ().SetInt ("n", 1);

	// Les ports ne dependent PAS du document ; ils sont poses des la
	// construction, et PublishPorts n'a rien a republier.
	m_desc.inputs.push_back ({ "entree", Types ().mesh, false });
	m_desc.outputs.push_back ({ "sortie", Types ().mesh, false });
}

void RepeatNode::PublishPorts (const SubgraphInstance &instance)
{
	(void)instance;
}

bool RepeatNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                          cggraph::ValueList &out)
{
	const int n = GetInt (GetParams (), "n", 1);
	if (n < 0)
	{
		// REFUS, et non un rabotage silencieux dans [0, n]. Le catalogue porte
		// deja plusieurs corps qui ramenent leurs parametres dans un intervalle
		// sans le dire, et chacun a du etre signale comme une reserve : un
		// parametre corrige en silence rend un resultat que personne n'a demande.
		NoteResult (cggraph::EvalStatus::ComputeFailed, "n");
		return false;
	}

	// n = 0, c'est zero application : l'identite. Ce n'est pas un cas
	// degenere qu'on tolere, c'est la valeur juste, et un test la fige.
	if (n == 0)
	{
		NoteResult (cggraph::EvalStatus::Ok, std::string ());
		out[0] = in[0];
		return true;
	}

	LoadStatus status = LoadStatus::Ok;
	std::string detail;
	const std::unique_ptr<SubgraphInstance> instance = Open (status, detail);
	if (instance == nullptr)
	{
		NoteResult (cggraph::EvalStatus::ComputeFailed, std::string (ToString (status)) + " " + detail);
		return false;
	}

	if (instance->inputs.size () != 1 || instance->outputs.size () != 1)
	{
		NoteResult (cggraph::EvalStatus::MissingInput, "frontiere");
		return false;
	}

	cggraph::Evaluator evaluator (instance->graph);

	cggraph::Value current = in[0];
	for (int k = 0; k < n; ++k)
	{
		const std::vector<cggraph::Value> element (1, current);
		std::vector<cggraph::Value> produced;
		const PassResult pass = RunPass (*instance, evaluator, ctx, std::to_string (k), element,
		                                 produced, SinksRequested ());
		NotePass ();
		if (!pass.IsOk ())
		{
			NoteResult (pass.status, pass.detail);
			NoteWritten (CollectWritten (*instance));
			return false;
		}
		// GARDE DEFENSIVE, ET NON COUVERTE -- dit ici plutot que compte comme
		// une couverture qu'on n'a pas. Aucun document fait des types PUBLIES
		// ne l'atteint : flow.out repete son entree, laquelle est obligatoire,
		// donc une passe qui reussit rend toujours une valeur. Elle protege le
		// contrat, pas le catalogue : rien dans Node::Compute n'interdit a un
		// noeud de rendre vrai sans remplir sa sortie -- l'etape 3 en a ecrit un
		// pour le prouver. Sabotage S26 : VERT, et c'est la reponse attendue.
		if (produced.empty () || produced[0].IsEmpty ())
		{
			NoteResult (cggraph::EvalStatus::MissingInput, "sortie");
			return false;
		}
		current = produced[0];
	}

	NoteResult (cggraph::EvalStatus::Ok, std::string ());
	NoteWritten (CollectWritten (*instance));
	out[0] = current;
	return true;
}

} // namespace flow
} // namespace cggraph_nodes
