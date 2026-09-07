#include "svg_extrude_colored.h"

#include <memory>
#include <string>
#include <vector>

#include "../../../cgmesh/import_svg.h"
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
		d.typeName = "svg.extrude.colored";
		d.inputs.push_back ({ "chemin", Types ().path, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

// Emprise XY du maillage, mesuree sur les positions -- et non par `Mesh::bbox()`,
// derivation mise en cache sans detection de peremption, vide sur un maillage
// qui n'est jamais passe par computebbox().
bool meshExtentXY (const Mesh &mesh, float &largest)
{
	const unsigned int n = mesh.GetNVertices ();
	if (n == 0u)
		return false;

	float lo[2] = { 0.0f, 0.0f }, hi[2] = { 0.0f, 0.0f };
	for (unsigned int i = 0; i < n; ++i)
	{
		float v[3];
		if (mesh.GetVertex (i, v) != 0)
			continue;
		if (i == 0u) { lo[0] = hi[0] = v[0]; lo[1] = hi[1] = v[1]; continue; }
		if (v[0] < lo[0]) lo[0] = v[0];
		if (v[0] > hi[0]) hi[0] = v[0];
		if (v[1] < lo[1]) lo[1] = v[1];
		if (v[1] > hi[1]) hi[1] = v[1];
	}

	const float w = hi[0] - lo[0];
	const float h = hi[1] - lo[1];
	largest = w > h ? w : h;
	return largest > 0.0f;
}

} // namespace

SvgExtrudeColoredNode::SvgExtrudeColoredNode ()
{
	// Memes noms et memes defauts que svg.contours : un document se transpose
	// d'un noeud a l'autre sans table de correspondance.
	GetParams ().SetFloat ("flattenTol", 0.005f);
	GetParams ().SetBool ("centerAndFit", true);
	GetParams ().SetBool ("invertY", true);
	GetParams ().SetBool ("strokeToVolume", true);
	GetParams ().SetFloat ("strokeScale", 1.0f);
	GetParams ().SetFloat ("strokeWidthFallback", 1.0f);

	// Trait des formes FERMEES ET REMPLIES. Defaut `true` ICI, alors que
	// SvgExtrudeOptions le met a `false` : l'API C++ preserve la geometrie de ses
	// appelants, le noeud sert la regle du produit -- ne rien perdre du document.
	GetParams ().SetBool ("strokeOnFilledShapes", true);

	// Largeur MINIMALE d'un trait, dans les unites du dessin normalise comme
	// `flattenTol` : sous « Taille », 0.004 vaut 0,4 mm sur une piece de 100 mm.
	// Elle ELARGIT un trait trop fin, elle n'en supprime jamais. 0 la desactive.
	GetParams ().SetFloat ("minStrokeWorldWidth", 0.0f);

	// Couleur d'une region issue d'un trait : celle du `stroke` plutot que celle
	// du `fill` de sa forme. Sans effet quand `useSvgColors` est faux.
	GetParams ().SetBool ("strokeUsesStrokeColor", true);

	// LA BASCULE, et elle en gouverne DEUX (cf. l'en-tete).
	GetParams ().SetBool ("useSvgColors", true);

	// TAILLE de la piece, en millimetres, mesuree sur son plus grand cote XY.
	// Meme role et meme defaut que sur svg.contours : a zero, les coordonnees
	// sortent telles que l'import les rend.
	GetParams ().SetFloat ("fitSize", 0.0f);

	// PROFONDEUR, en millimetres : `depth` est une EPAISSEUR, le solide occupe
	// [0, depth]. Meme sens que sur shape.extrude.
	GetParams ().SetFloat ("depth", 0.2f);
}

const cggraph::NodeDesc &SvgExtrudeColoredNode::GetDesc () const
{
	return Desc ();
}

void SvgExtrudeColoredNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	out.push_back ({ "gradientShapes", (double)m_gradientShapes.load () });
	out.push_back ({ "hiddenShapes", (double)m_hiddenShapes.load () });
	out.push_back ({ "subtractedShapes", (double)m_subtractedShapes.load () });
	out.push_back ({ "materials", (double)m_materials.load () });
}

bool SvgExtrudeColoredNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                     cggraph::ValueList &out)
{
	(void)ctx;

	const std::string *path = in[0].Get<std::string> (Types ().path);
	if (path == nullptr || path->empty ())
		return false;

	SvgExtrudeOptions options;
	options.flattenTol = GetFloat (GetParams (), "flattenTol", 0.005f);
	options.centerAndFit = GetBool (GetParams (), "centerAndFit", true);
	options.invertY = GetBool (GetParams (), "invertY", true);
	options.strokeToVolume = GetBool (GetParams (), "strokeToVolume", true);
	options.strokeScale = GetFloat (GetParams (), "strokeScale", 1.0f);
	options.strokeWidthFallback = GetFloat (GetParams (), "strokeWidthFallback", 1.0f);
	options.strokeOnFilledShapes = GetBool (GetParams (), "strokeOnFilledShapes", true);
	options.minStrokeWorldWidth = GetFloat (GetParams (), "minStrokeWorldWidth", 0.0f);
	options.strokeUsesStrokeColor = GetBool (GetParams (), "strokeUsesStrokeColor", true);

	// UN parametre, DEUX options. La palette sans la marqueterie laisserait les
	// capots superposes ; la marqueterie sans la palette changerait la geometrie
	// sans qu'aucune couleur ne le justifie.
	const bool useSvgColors = GetBool (GetParams (), "useSvgColors", true);
	options.perShapeMaterials = useSvgColors;
	options.overlapPolicy = useSvgColors
		? SvgExtrudeOptions::OverlapPolicy::Subtract
		: SvgExtrudeOptions::OverlapPolicy::None;

	// La profondeur est portee par l'EXTRUSION et non par une mise a l'echelle :
	// « Taille » ne redimensionne que le plan, si bien que les deux cotes de la
	// piece restent independantes. Meme contrat que svg.contours + shape.extrude.
	options.height = GetFloat (GetParams (), "depth", 0.2f);

	SvgExtrudeMapping mapping;
	std::shared_ptr<Mesh> mesh (import_svg_extruded (*path, options, &mapping));
	if (mesh == nullptr)
		return false;

	m_gradientShapes.store (mapping.gradientGroups);
	m_hiddenShapes.store (mapping.overlap.fullyCoveredGroups);
	m_subtractedShapes.store (mapping.overlap.partiallyCoveredGroups);
	m_materials.store (mesh->GetNMaterials ());

	// Mise a l'echelle sur l'emprise MESUREE, et non sur l'hypothese que
	// `centerAndFit` a normalise a 1.0 : le cadrage est optionnel.
	//
	// XY SEULEMENT. La profondeur est deja dans l'unite demandee, et l'etirer
	// avec le plan ferait dependre l'epaisseur de la taille.
	const float fitSize = GetFloat (GetParams (), "fitSize", 0.0f);
	float largest = 0.0f;
	if (fitSize > 0.0f && meshExtentXY (*mesh, largest))
	{
		const float k = fitSize / largest;
		for (unsigned int i = 0; i < mesh->GetNVertices (); ++i)
		{
			float v[3];
			if (mesh->GetVertex (i, v) != 0)
				continue;
			mesh->SetVertex (i, v[0] * k, v[1] * k, v[2]);
		}
	}

	out[0] = cggraph::Value::Make (Types ().mesh, mesh);
	return true;
}

} // namespace cggraph_nodes
