#include "pixel_blocks.h"

#include <memory>

#include "../../../cgimg/color.h"
#include "../../../cgimg/image.h"
#include "../../../cgmesh/image_pixel_blocks.h"
#include "../../../cgmesh/mesh.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

// Meme jeu de parametres pour les deux noeuds, comme pour le relief : ils
// decrivent la meme geometrie, seule la sortie differe.
//
// Il n'y a PAS de `simplifyErr` ici, et ce n'est pas un oubli : les contours des
// blocs sont traces sans lissage ni simplification (Vectorize avec bSmooth a
// false et une erreur negative). Un bloc pixelise n'a que des aretes axiales ;
// les arrondir detruirait precisement ce qu'on cherche a montrer.
void DeclarePixelBlocksParams (cggraph::ParamSet &params)
{
	// Retrait des blocs (offset negatif), en CELLULES de la grille et non en
	// pixels source. C'est lui qui creuse le sillon d'assemblage entre pieces.
	params.SetFloat ("shrink", 0.0f);

	params.SetFloat ("fitSize", 1.0f);
	params.SetFloat ("blockHeight", 0.10f);
	params.SetFloat ("baseThickness", 0.05f);
	params.SetFloat ("margin", 0.05f);
	params.SetFloat ("wallThickness", 0.03f);
	params.SetFloat ("wallHeight", 0.10f);

	// COTE DE CELLULE, en millimetres, et UNIQUE longueur absolue du noeud. A
	// zero -- le defaut -- rien ne change : `fitSize` et les cinq longueurs
	// ci-dessus font foi, en unites monde, comme avant. Renseignee, elle prend le
	// pas sur elles et la taille totale de l'objet devient une CONSEQUENCE :
	//
	//     taille totale = cellSize x plus grande dimension du raster (en cellules)
	//
	// Un MODE et non un remplacement, parce que IsVersionCompatible est une
	// egalite stricte, sans crochet de migration : monter la version du
	// descripteur condamnerait tous les documents deja enregistres.
	params.SetFloat ("cellSize", 0.0f);

	// Les cinq memes longueurs, exprimees en MULTIPLES DE CELLULE. Des noms
	// distincts et non les memes parametres relus autrement : un nom qui change
	// d'unite selon l'etat d'un autre ne se lit pas sans connaitre cet etat.
	// Elles ne sont consultees que si cellSize > 0.
	params.SetFloat ("blockHeightCells", 1.0f);
	params.SetFloat ("baseThicknessCells", 0.5f);
	params.SetFloat ("marginCells", 0.5f);
	params.SetFloat ("wallThicknessCells", 0.5f);
	params.SetFloat ("wallHeightCells", 1.2f);

	// Plaquage de l'image d'origine, arrivant par le port "texture". DEFAUT FAUX,
	// et c'est un choix : les aplats quantifies font l'identite du module « blocs
	// pixelises », et un document existant doit continuer a rendre ce qu'il
	// rendait. Le parametre ne DESIGNE pas l'image -- c'est le port qui le fait --,
	// il dit seulement s'il faut s'en servir. Sans lui, un gabarit ne pourrait
	// basculer d'un mode a l'autre qu'en recablant le graphe.
	params.SetBool ("useTexture", false);

	params.SetBool ("emitBase", true);
	params.SetBool ("emitWall", true);
	params.SetBool ("emitInternalWalls", true);

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
	if (v < 0)   return 0;
	if (v > 255) return 255;
	return static_cast<unsigned char> (v);
}

// `cells` est la plus grande dimension du raster, en cellules. Le raster qui
// parvient a ce noeud EST la grille : quantize_image le ramene a `pixelWidth` de
// large (pixelize_to_width -> Img::resize). La cote de cellule suffit donc a
// deduire la taille totale, sans que ce noeud ait a connaitre pixelWidth -- qui
// se regle en amont, sur img.quantize.
// `fitSize` cadre la PLUS GRANDE dimension XY du contenu : c'est donc elle, et
// non la largeur seule, qui multiplie la cote de cellule.
unsigned int LargestSide (const Img &image)
{
	return image.width () > image.height () ? image.width () : image.height ();
}

ImagePixelBlocksOptions ReadOptions (const cggraph::ParamSet &params, unsigned int cells)
{
	ImagePixelBlocksOptions opt;

	// pixelWidth, workingMaxDim et les champs de palette restent a leur defaut :
	// les surcharges prenant une image ne les lisent pas (cf.
	// image_pixel_blocks.h). Les exposer ici ferait croire que la grille se regle
	// sur ce noeud, alors qu'elle se decide dans img.quantize.

	// Deja une fraction de cellule, dans les deux modes : il s'applique sur la
	// grille, avant toute mise a l'echelle.
	opt.shrink = GetFloat (params, "shrink", 0.0f);

	const float cellSize = GetFloat (params, "cellSize", 0.0f);
	if (cellSize > 0.0f && cells > 0)
	{
		opt.fitSize       = cellSize * static_cast<float> (cells);
		opt.blockHeight   = cellSize * GetFloat (params, "blockHeightCells", 1.0f);
		opt.baseThickness = cellSize * GetFloat (params, "baseThicknessCells", 0.5f);
		opt.margin        = cellSize * GetFloat (params, "marginCells", 0.5f);
		opt.wallThickness = cellSize * GetFloat (params, "wallThicknessCells", 0.5f);
		opt.wallHeight    = cellSize * GetFloat (params, "wallHeightCells", 1.2f);
	}
	else
	{
		opt.fitSize       = GetFloat (params, "fitSize", 1.0f);
		opt.blockHeight   = GetFloat (params, "blockHeight", 0.10f);
		opt.baseThickness = GetFloat (params, "baseThickness", 0.05f);
		opt.margin        = GetFloat (params, "margin", 0.05f);
		opt.wallThickness = GetFloat (params, "wallThickness", 0.03f);
		opt.wallHeight    = GetFloat (params, "wallHeight", 0.10f);
	}

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

const cggraph::NodeDesc &BlocksDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "img.pixel_blocks";
		d.inputs.push_back ({ "image", Types ().image, false });
		// TEXTURE, OPTIONNELLE : l'image d'origine, celle d'AVANT la
		// quantification. Non alimentee, les blocs gardent leur aplat par
		// couleur de palette -- exactement le comportement d'avant ce port, donc
		// aucun document existant ne change de resultat et la version du
		// descripteur n'a pas bouge (IsVersionCompatible est une egalite stricte,
		// sans crochet de migration : un bump les condamnerait tous).
		//
		// Un second PORT et non un booleen : l'image a texturer n'est pas celle
		// que ce noeud recoit. Elle vient d'ailleurs dans le graphe -- en general
		// du meme img.io.load, branche en parallele de img.quantize --, et un
		// parametre ne saurait pas la designer.
		d.inputs.push_back ({ "texture", Types ().image, true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

const cggraph::NodeDesc &PartsDesc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "img.pixel_blocks.parts";
		d.inputs.push_back ({ "image", Types ().image, false });
		d.outputs.push_back ({ "pieces", Types ().meshArray, false });
		return d;
	}();
	return desc;
}

} // namespace

// ---------------------------------------------------------------------------

PixelBlocksNode::PixelBlocksNode ()
{
	DeclarePixelBlocksParams (GetParams ());
}

const cggraph::NodeDesc &PixelBlocksNode::GetDesc () const
{
	return BlocksDesc ();
}

bool PixelBlocksNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
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

	Mesh *mesh = image_to_pixel_blocks (*image, ReadOptions (GetParams (), LargestSide (*image)), texture.get ());
	if (mesh == nullptr)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, std::shared_ptr<Mesh> (mesh));
	return true;
}

// ---------------------------------------------------------------------------

PixelBlocksPartsNode::PixelBlocksPartsNode ()
{
	DeclarePixelBlocksParams (GetParams ());
}

const cggraph::NodeDesc &PixelBlocksPartsNode::GetDesc () const
{
	return PartsDesc ();
}

bool PixelBlocksPartsNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                    cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Img> image = in[0].Share<Img> (Types ().image);
	if (image == nullptr)
		return false;

	const std::vector<Mesh *> meshes =
		image_to_pixel_blocks_per_component (*image, ReadOptions (GetParams (), LargestSide (*image)));
	if (meshes.empty ())
		return false;

	// Cession immediate, comme pour les couches du relief : la brique rend des
	// pointeurs nus dont l'appelant est proprietaire.
	std::shared_ptr<MeshArray> array = std::make_shared<MeshArray> ();
	array->items.reserve (meshes.size ());
	for (Mesh *mesh : meshes)
		array->items.push_back (std::shared_ptr<const Mesh> (mesh));

	out[0] = cggraph::Value::Make (Types ().meshArray, array);
	return true;
}

} // namespace cggraph_nodes
