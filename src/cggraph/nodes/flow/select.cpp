#include "select.h"

#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{
namespace flow
{

namespace
{

// Nombre de ports d'entree, et donc de variantes selectionnables.
const int kInputs = 3;

} // namespace

SelectNode::SelectNode (const cggraph::TypeDesc *type, const char *typeName)
	: m_type (type)
{
	m_desc.typeName = typeName;
	// La PREMIERE est obligatoire : un selecteur sans aucune source n'a rien a
	// choisir, et le dire au type plutot qu'a l'execution evite un graphe
	// silencieusement inerte.
	m_desc.inputs.push_back ({ "variante 0", type, false });
	m_desc.inputs.push_back ({ "variante 1", type, true });
	m_desc.inputs.push_back ({ "variante 2", type, true });
	m_desc.outputs.push_back ({ "choisie", type, false });

	GetParams ().SetInt ("index", 0);
}

bool SelectNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                          cggraph::ValueList &out)
{
	(void)ctx;

	// Bornage, comme partout ou ParamSet ne sait pas borner : un index hors
	// intervalle ne designe rien, et retomber en silence sur zero masquerait le
	// reglage fautif.
	int index = GetInt (GetParams (), "index", 0);
	if (index < 0) index = 0;
	if (index >= kInputs) index = kInputs - 1;

	if ((std::size_t)index >= in.size ())
		return false;

	// La valeur est REPARTAGEE telle quelle : ce noeud ne fabrique rien, il
	// choisit. Aucune copie, quel que soit le poids du maillage.
	const cggraph::Value &chosen = in[(std::size_t)index];
	if (chosen.IsEmpty ())
		return false;   // port vide : erreur de graphe, pas defaut a combler

	out[0] = chosen;
	(void)m_type;
	return true;
}

SelectMeshNode::SelectMeshNode ()
	: SelectNode (Types ().mesh, "flow.select.mesh")
{
}

SelectContoursNode::SelectContoursNode ()
	: SelectNode (Types ().extrudeContours, "flow.select.contours")
{
}

// EBRASEMENT et non section de barre : un selecteur par type de port, comme le
// consommateur lui-meme (cf. shapes/profile.h).
SelectProfileNode::SelectProfileNode ()
	: SelectNode (Types ().splayProfile, "flow.select.profile")
{
}

} // namespace flow
} // namespace cggraph_nodes
