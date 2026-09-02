#include "relief.h"

#include <memory>

#include "../../../cgimg/color.h"
#include "../../../cgimg/image.h"
#include "../../../cgmesh/image_relief.h"
#include "../../../cgmesh/mesh.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

// Les DEUX noeuds portent exactement le meme jeu de parametres, et c'est
// volontaire : ils decrivent la meme geometrie, seule leur sortie differe. Un
// document qui remplace l'un par l'autre garde donc ses reglages.
void DeclareReliefParams (cggraph::ParamSet &params)
{
	// Simplification des contours, en pixels de l'image quantifiee.
	params.SetFloat ("simplifyErr", 1.0f);

	// Retrait des regions (offset negatif), en pixels. Deux blocs voisins cessent
	// d'etre jointifs : il reste entre eux un sillon de 2*shrink, qui laisse la
	// tolerance d'assemblage a l'impression. 0 = geometrie inchangee.
	params.SetFloat ("shrink", 0.0f);

	// Echelle : le contenu est mis a fitSize sur son plus grand cote XY, rapport
	// d'aspect preserve. Toutes les longueurs ci-dessous sont dans cette unite.
	params.SetFloat ("fitSize", 1.0f);

	params.SetFloat ("blockHeight", 0.10f);
	params.SetFloat ("baseThickness", 0.05f);
	params.SetFloat ("margin", 0.05f);
	params.SetFloat ("wallThickness", 0.03f);
	params.SetFloat ("wallHeight", 0.10f);

	// Plaquage de l'image d'origine, arrivant par le port "texture". DEFAUT FAUX :
	// les aplats quantifies sont le resultat attendu d'un relief, et un document
	// existant doit continuer a rendre ce qu'il rendait. Le parametre ne designe
	// pas l'image -- c'est le port --, il dit seulement s'il faut s'en servir.
	params.SetBool ("useTexture", false);

	params.SetBool ("emitBase", true);
	params.SetBool ("emitWall", true);

	// false allege le maillage en supprimant les murs coincidents entre blocs
	// adjacents ; les blocs ne sont alors plus des solides fermes pris isolement.
	params.SetBool ("emitInternalWalls", true);

	// Couleurs du cadre, en trois composantes 0-255. Trois entiers et non une
	// couleur, faute de type couleur dans ParamSet -- meme limite que l'enum de
	// img.quantize (cf. debt_cggraph.md).
	params.SetInt ("baseColorR", 160);
	params.SetInt ("baseColorG", 160);
	params.SetInt ("baseColorB", 160);
	params.SetInt ("wallColorR", 120);
	params.SetInt ("wallColorG", 120);
	params.SetInt ("wallColorB", 120);
}

unsigned char ColorComponent (const cggraph::ParamSet &params, const char *name, int fallback)
{
	const int v = GetInt (params, name, fallback);
	// Ecrete SANS avertissement, et il faut le savoir : ParamSet ne porte pas de
	// bornes, donc rien en amont ne refuse un 300. Le meme defaut est signale sur
	// mesh.thickness, dont numRays est ramene dans [1, 256] en silence.
	if (v < 0)   return 0;
	if (v > 255) return 255;
	return static_cast<unsigned char> (v);
}

ImageReliefOptions ReadOptions (const cggraph::ParamSet &params)
{
	ImageReliefOptions opt;

	// Les champs de quantification d'ImageReliefOptions -- maxColors, algo,
	// preSmoothPasses, refineIterations, despecklePasses, minRegionArea,
	// workingMaxDim -- sont DELIBEREMENT laisses a leur defaut : les surcharges
	// prenant une image ne les lisent pas (cf. image_relief.h). Les renseigner
	// ici ferait croire qu'ils agissent.

	opt.simplifyErr   = GetFloat (params, "simplifyErr", 1.0f);
	opt.shrink        = GetFloat (params, "shrink", 0.0f);
	opt.fitSize       = GetFloat (params, "fitSize", 1.0f);
	opt.blockHeight   = GetFloat (params, "blockHeight", 0.10f);
	opt.baseThickness = GetFloat (params, "baseThickness", 0.05f);
	opt.margin        = GetFloat (params, "margin", 0.05f);
	opt.wallThickness = GetFloat (params, "wallThickness", 0.03f);
	opt.wallHeight    = GetFloat (params, "wallHeight", 0.10f);

	opt.emitBase          = GetBool (params, "emitBase", true);
	opt.emitWall          = GetBool (params, "emitWall", true);
	opt.emitInternalWalls = GetBool (params, "emitInternalWalls", true);

	opt.baseColor = Color (ColorComponent (params, "baseColorR", 160),
	                       ColorComponent (params, "baseColorG", 160),
	                       ColorComponent (params, "baseColorB", 160));
	opt.wallColor = Color (ColorComponent (params, "wallColorR", 120),
	                       ColorComponent (params, "wallColorG", 120),
	                       ColorComponent (params, "wallColorB", 120));
	return opt;
}

const cggraph::NodeDesc &ReliefDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "img.relief";
		d.inputs.push_back ({ "image", Types ().image, false });
		// TEXTURE, OPTIONNELLE : l'image d'AVANT la quantification. Meme motif
		// que sur img.pixel_blocks -- un port et non un parametre, l'image a
		// plaquer n'etant pas celle que ce noeud recoit. Entree optionnelle,
		// version du descripteur inchangee : les documents existants restent
		// lisibles et rendent la meme chose.
		d.inputs.push_back ({ "texture", Types ().image, true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

const cggraph::NodeDesc &LayersDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "img.relief.layers";
		d.inputs.push_back ({ "image", Types ().image, false });
		d.outputs.push_back ({ "pieces", Types ().meshArray, false });
		return d;
	}();
	return desc;
}

} // namespace

// ---------------------------------------------------------------------------

ReliefNode::ReliefNode ()
{
	DeclareReliefParams (GetParams ());
}

const cggraph::NodeDesc &ReliefNode::GetDesc () const
{
	return ReliefDesc ();
}

bool ReliefNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                          cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Img> image = in[0].Share<Img> (Types ().image);
	if (image == nullptr)
		return false;

	// Le port optionnel rend une valeur VIDE quand rien n'est branche : Share
	// donne alors nullptr, et le pipeline retombe sur les aplats.
	const std::shared_ptr<const Img> texture =
		(GetBool (GetParams (), "useTexture", false) && in.size () > 1)
			? in[1].Share<Img> (Types ().image)
			: nullptr;

	// La surcharge copie l'image : la vectorisation palettise son entree, et la
	// valeur qui arrive ici est partagee par tout ce qui lit ce lien.
	Mesh *mesh = image_to_relief (*image, ReadOptions (GetParams ()), texture.get ());
	if (mesh == nullptr)
		return false;

	// L'appelant possede le maillage rendu (cf. image_relief.h) : on le CEDE au
	// shared_ptr, sans copie -- la regle de node_support.h.
	out[0] = cggraph::Value::Make (Types ().mesh, std::shared_ptr<Mesh> (mesh));
	return true;
}

// ---------------------------------------------------------------------------

ReliefLayersNode::ReliefLayersNode ()
{
	DeclareReliefParams (GetParams ());
}

const cggraph::NodeDesc &ReliefLayersNode::GetDesc () const
{
	return LayersDesc ();
}

bool ReliefLayersNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Img> image = in[0].Share<Img> (Types ().image);
	if (image == nullptr)
		return false;

	const std::vector<Mesh *> meshes =
		image_to_relief_per_color (*image, ReadOptions (GetParams ()));
	if (meshes.empty ())
		return false;

	// CESSION immediate de chaque maillage : la brique rend des pointeurs nus
	// dont l'appelant est proprietaire, et une exception entre ici et la fin de
	// la boucle les fuirait. Les emballer un par un des la premiere iteration
	// borne la fuite a rien.
	std::shared_ptr<MeshArray> array = std::make_shared<MeshArray> ();
	array->items.reserve (meshes.size ());
	for (Mesh *mesh : meshes)
		array->items.push_back (std::shared_ptr<const Mesh> (mesh));

	out[0] = cggraph::Value::Make (Types ().meshArray, array);
	return true;
}

} // namespace cggraph_nodes
