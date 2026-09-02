#include "quantize.h"

#include <memory>

#include "../../../cgimg/image.h"
#include "../../../cgmesh/image_region_pipeline.h"
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
		d.typeName = "img.quantize";
		d.inputs.push_back ({ "image", Types ().image, false });
		d.outputs.push_back ({ "image", Types ().image, false });
		return d;
	}();
	return desc;
}

} // namespace

QuantizeImageNode::QuantizeImageNode ()
{
	GetParams ().SetInt ("maxColors", 16);

	// 0 = Wu, 1 = Heckbert. Un ENTIER et non une chaine, faute de type enumere
	// dans ParamSet (cf. debt_cggraph.md, « ParamType sans metadonnee ») : le
	// jour ou l'inspecteur saura offrir une liste de choix, ce parametre sera le
	// premier a en profiter.
	//
	// Toute valeur autre que 1 vaut Wu, et c'est le bon defaut : Heckbert n'est
	// garde que pour comparaison. Mesure citee par image_region_pipeline.h --
	// erreur quadratique contre la source sur une affiche a 4 couleurs
	// resamplee : Wu 199, Heckbert 869.
	GetParams ().SetInt ("algo", 0);

	GetParams ().SetInt ("preSmoothPasses", 1);
	GetParams ().SetInt ("refineIterations", 3);
	GetParams ().SetInt ("despecklePasses", 1);
	GetParams ().SetInt ("minRegionArea", 12);

	// 0 = pas de pixelisation. C'EST LE PARAMETRE QUI DISTINGUE LES DEUX CHAINES :
	// le relief le laisse a 0, les blocs pixelises le portent a 64. Il s'applique
	// APRES la quantification, par vote majoritaire, donc il ne cree aucune
	// couleur absente de la palette.
	GetParams ().SetInt ("pixelWidth", 0);

	// Pre-reduction de la source avant tout traitement. 0 desactive.
	GetParams ().SetInt ("workingMaxDim", 512);
}

const cggraph::NodeDesc &QuantizeImageNode::GetDesc () const
{
	return Desc ();
}

bool QuantizeImageNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                 cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Img> input = in[0].Share<Img> (Types ().image);
	if (input == nullptr)
		return false;

	RegionQuantizeOptions qo;
	qo.maxColors        = GetInt (GetParams (), "maxColors", 16);
	qo.algo             = (GetInt (GetParams (), "algo", 0) == 1) ? QuantAlgo::Heckbert
	                                                             : QuantAlgo::Wu;
	qo.preSmoothPasses  = GetInt (GetParams (), "preSmoothPasses", 1);
	qo.refineIterations = GetInt (GetParams (), "refineIterations", 3);
	qo.despecklePasses  = GetInt (GetParams (), "despecklePasses", 1);
	qo.minRegionArea    = GetInt (GetParams (), "minRegionArea", 12);
	qo.pixelWidth       = GetInt (GetParams (), "pixelWidth", 0);
	qo.workingMaxDim    = GetInt (GetParams (), "workingMaxDim", 512);

	// PAS de jeton d'annulation : quantize_image n'en prend pas. La chaine n'est
	// donc pas interruptible, et il ne faut pas faire croire le contraire en
	// fabriquant un GraphContext dont personne ne lirait le drapeau. C'est une
	// limite reelle -- sur une grande image, bilateral et Wu tiennent le fil
	// plusieurs secondes.
	std::shared_ptr<Img> result = std::make_shared<Img> ();
	if (!quantize_image (*input, qo, *result))
		return false;

	out[0] = cggraph::Value::Make (Types ().image, result);
	return true;
}

} // namespace cggraph_nodes
