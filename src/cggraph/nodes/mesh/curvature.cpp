#include "curvature.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/DiffParamEvaluator.h"
#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/mesh_half_edge.h"
#include "../../../cgmesh/normals.h"
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
		d.typeName = "mesh.curvature";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "champ", Types ().scalarField, false });
		return d;
	}();
	return desc;
}

// Les quatre methodes que ce noeud expose, dans l'ordre du parametre
// "method". TENSOR_STEINER n'y est PAS : son corps est sous #if 0. Une
// cinquieme position qui ne calcule rien serait un reglage qui ment.
const TensorMethodId kMethods[] = { TENSOR_TAUBIN, TENSOR_HAMANN,
                                    TENSOR_DESBRUN, TENSOR_GOLDFEATHER,
                                    TENSOR_HYBRID };
const int kMethodCount = 5;

// L'ordre du parametre "curvature" suit celui de l'enumeration de cgmesh.
const CurvatureType kCurvatures[] = { CurvatureType::Min, CurvatureType::Max,
                                      CurvatureType::Mean, CurvatureType::Gaussian };
const int kCurvatureCount = 4;

} // namespace

CurvatureNode::CurvatureNode (int method, int curvature)
{
	GetParams ().SetInt ("method", method);
	GetParams ().SetInt ("curvature", curvature);
}

const cggraph::NodeDesc &CurvatureNode::GetDesc () const
{
	return Desc ();
}

bool CurvatureNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	int method = GetInt (GetParams (), "method", 0);
	if (method < 0 || method >= kMethodCount)
		return false;
	int curvature = GetInt (GetParams (), "curvature", 3);
	if (curvature < 0 || curvature >= kCurvatureCount)
		return false;

	const unsigned int nv = input->GetNVertices ();
	if (nv == 0 || input->GetNFaces () == 0)
		return false;

	// Copie profonde faite par l'enveloppe : l'entree est lue, jamais ecrite.
	Mesh_half_edge model (input.get ());
	model.create_half_edge ();

	// Les estimateurs LISENT les normales par sommet et ne les calculent
	// jamais. Sans cette ligne, ils indexent un tableau vide.
	model.m_pMesh->InitVertexNormals ();
	Normals normals;
	normals.EvalOnVertices (&model, Normals::THURMER);

	const GraphContext graphContext = MakeGraphContext (ctx);
	MeshAlgoTensorEvaluator algo;
	if (!algo.Init (&model))
		return false;
	// Echec ou annulation, meme code de retour : l'evaluateur nomme le refus.
	if (!algo.Evaluate (kMethods[method], &graphContext))
		return false;

	std::shared_ptr<ScalarField> field = std::make_shared<ScalarField> ();
	field->values.assign (nv, 0.0f);
	field->defined.assign (nv, (char)0);
	for (unsigned int i = 0; i < nv; ++i)
	{
		const Tensor *tensor = algo.GetDiffParam (static_cast<int> (i));
		if (tensor == nullptr)
			continue;
		field->values[i] = tensor->GetCurvature (kCurvatures[curvature]);
		field->defined[i] = 1;
	}

	out[1] = cggraph::Value::Make (Types ().scalarField, std::move (field));

	// CESSION, pas copie -- cf. la regle de node_support.h. L'enveloppe a copie
	// l'entree a la construction et porte les tenseurs ; recopier son resultat
	// doublerait la facture pour un objet qu'elle va detruire.
	//
	// Elle est la DERNIERE ligne a toucher `model` : apres release (), ni
	// `model` ni `algo` -- qui le pointe -- ne designent le maillage rendu.
	out[0] = cggraph::Value::Make (Types ().mesh, std::shared_ptr<Mesh> (model.release ()));
	return true;
}

} // namespace cggraph_nodes
