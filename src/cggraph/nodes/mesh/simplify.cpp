#include "simplify.h"

#include <memory>

#include "../../../cgmesh/mesh_half_edge.h"
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
		d.typeName = "mesh.simplify";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

SimplifyNode::SimplifyNode (float targetRatio)
{
	GetParams ().SetFloat ("targetRatio", targetRatio);
	GetParams ().SetBool ("preserveFeatures", true);
	GetParams ().SetFloat ("featureAngleDeg", 45.0f);
	GetParams ().SetBool ("preserveAttributes", true);
	GetParams ().SetFloat ("maxError", 0.0f);
	GetParams ().SetBool ("exactError", false);
}

const cggraph::NodeDesc &SimplifyNode::GetDesc () const
{
	return Desc ();
}

bool SimplifyNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	Mesh_half_edge::SimplifyOptions options;
	options.preserve_features = GetBool (GetParams (), "preserveFeatures", true);
	options.feature_angle_deg = GetFloat (GetParams (), "featureAngleDeg", 45.0f);
	options.preserve_attributes = GetBool (GetParams (), "preserveAttributes", true);
	options.max_error = GetFloat (GetParams (), "maxError", 0.0f);
	options.exact_error = GetBool (GetParams (), "exactError", false);

	// Copie profonde faite par l'enveloppe : l'entree reste lue seule.
	Mesh_half_edge model (input.get ());
	const GraphContext graphContext = MakeGraphContext (ctx);
	model.simplify (GetFloat (GetParams (), "targetRatio", 0.5f), options, &graphContext);

	// Annule : rien a rendre. `false` est le code d'un calcul qui n'a pas produit
	// ce qu'on lui demandait, et c'est l'evaluateur qui decide s'il faut le nommer
	// ComputeFailed ou Aborted -- il interroge le contexte AVANT ce code de
	// retour. La couche B n'a plus a connaitre cet ordre.
	if (ctx.IsAborted ())
		return false;

	// CESSION, pas copie -- cf. la regle de node_support.h. L'enveloppe a copie
	// l'entree a la construction ; recopier son resultat doublerait la facture
	// pour un objet qu'elle va detruire.
	out[0] = cggraph::Value::Make (Types ().mesh, std::shared_ptr<Mesh> (model.release ()));
	return true;
}

} // namespace cggraph_nodes
