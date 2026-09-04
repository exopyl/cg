#include "merge.h"

#include <memory>

#include "../../../cgmesh/mesh.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

const cggraph::NodeDesc &Desc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "mesh.merge";
		d.inputs.push_back ({ "maillage A", Types ().mesh, false });
		// Optionnelle : cf. l'en-tete -- une page a graphe fixe ne debranche pas,
		// elle omet.
		d.inputs.push_back ({ "maillage B", Types ().mesh, true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

const cggraph::NodeDesc &MergeMeshNode::GetDesc () const
{
	return Desc ();
}

bool MergeMeshNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Mesh> a = in[0].Share<Mesh> (Types ().mesh);
	if (a == nullptr)
		return false;

	const std::shared_ptr<const Mesh> b =
		in.size () > 1 ? in[1].Share<Mesh> (Types ().mesh) : nullptr;

	// Rien a fusionner : on REPARTAGE l'entree au lieu d'en faire une copie. Le
	// maillage est immuable une fois publie, donc deux ports peuvent tenir le
	// meme -- et une page sans socle ne paie alors aucune copie a chaque
	// deplacement de curseur.
	if (b == nullptr)
	{
		out[0] = cggraph::Value::Make (Types ().mesh, a);
		return true;
	}

	// Copie profonde de A (Mesh a un constructeur de copie par defaut sur des
	// vecteurs) : l'entree est LUE, jamais ecrite. Append ecrit dans sa cible.
	std::shared_ptr<Mesh> merged = std::make_shared<Mesh> (*a);

	// Append prend un Mesh* non const et ne modifie pas sa source (il la lit pour
	// concatener sommets, faces et table de materiaux, avec decalage des index).
	// Le const_cast est cantonne a cet appel plutot que d'elargir une signature
	// partagee -- c'est le meme choix que graphExportObj dans maker/wasm_api.cpp.
	merged->Append (const_cast<Mesh *> (b.get ()));

	// Les normales viennent des deux sources telles quelles ; aucune face
	// nouvelle n'est creee, donc rien a recalculer. La revision, en revanche,
	// change : c'est elle que les consommateurs regardent pour se mettre a jour.
	merged->IncrementRevision ();

	out[0] = cggraph::Value::Make (Types ().mesh, merged);
	return true;
}

} // namespace cggraph_nodes
