#include "svg_contours.h"

#include <memory>
#include <string>
#include <vector>

#include "../../../cgmesh/import_svg.h"
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
		d.typeName = "svg.contours";
		d.inputs.push_back ({ "chemin", Types ().path, false });
		d.outputs.push_back ({ "contours", Types ().extrudeContours, false });
		return d;
	}();
	return desc;
}

} // namespace

SvgContoursNode::SvgContoursNode ()
{
	// `flattenTol` s'exprime dans les unites du RESULTAT et non du document :
	// l'importeur convertit, sans quoi la finesse des courbes dependrait de
	// l'echelle du fichier source.
	GetParams ().SetFloat ("flattenTol", 0.005f);
	GetParams ().SetBool ("centerAndFit", true);
	GetParams ().SetBool ("invertY", true);
	GetParams ().SetBool ("strokeToVolume", true);
	GetParams ().SetFloat ("strokeScale", 1.0f);
	GetParams ().SetFloat ("strokeWidthFallback", 1.0f);

	// TAILLE de la piece, en millimetres, mesuree sur son plus grand cote.
	//
	// A zero -- le defaut -- rien ne change : les contours sortent tels que
	// svg_to_contours les rend, donc normalises a 1.0 sous `centerAndFit`, ce qui
	// ne designe aucune longueur reelle. Renseignee, elle est LA cote absolue de
	// la chaine : la profondeur de shape.extrude s'exprime alors dans la meme
	// unite.
	//
	// Contrairement aux blocs et au relief, cette page n'a pas d'unite plus
	// petite -- ni cellule ni pixel -- dont la taille totale pourrait decouler.
	// Un dessin vectoriel n'a pas de grain : sa taille EST la cote de reference.
	GetParams ().SetFloat ("fitSize", 0.0f);

	// `height` n'est PAS expose : c'est la profondeur d'extrusion, elle se regle
	// sur shape.extrude. L'exposer ici donnerait un curseur sans effet, puisque
	// svg_to_contours ne le lit pas.
}

const cggraph::NodeDesc &SvgContoursNode::GetDesc () const
{
	return Desc ();
}

bool SvgContoursNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
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

	std::shared_ptr<std::vector<ExtrudeContour>> contours =
		std::make_shared<std::vector<ExtrudeContour>> ();
	if (!svg_to_contours (*path, options, *contours))
		return false;

	// Mise a l'echelle sur la BOITE MESUREE, et non sur l'hypothese que
	// `centerAndFit` a normalise a 1.0 : le cadrage est optionnel, et sans lui
	// les points sont dans les coordonnees du document. Mesurer vaut dans les
	// deux cas.
	const float fitSize = GetFloat (GetParams (), "fitSize", 0.0f);
	if (fitSize > 0.0f)
	{
		bool any = false;
		float lo[2] = { 0.0f, 0.0f }, hi[2] = { 0.0f, 0.0f };
		for (const ExtrudeContour &c : *contours)
			for (const Vector2f &pt : c.pts)
			{
				if (!any) { lo[0] = hi[0] = pt.x; lo[1] = hi[1] = pt.y; any = true; continue; }
				if (pt.x < lo[0]) lo[0] = pt.x;
				if (pt.x > hi[0]) hi[0] = pt.x;
				if (pt.y < lo[1]) lo[1] = pt.y;
				if (pt.y > hi[1]) hi[1] = pt.y;
			}

		const float w = hi[0] - lo[0];
		const float h = hi[1] - lo[1];
		const float largest = w > h ? w : h;
		// Un dessin degenere -- un seul point, un trait sans epaisseur -- n'a pas
		// de taille a ramener : le mettre a l'echelle diviserait par zero.
		if (any && largest > 0.0f)
		{
			const float k = fitSize / largest;
			for (ExtrudeContour &c : *contours)
				for (Vector2f &pt : c.pts)
				{
					pt.x *= k;
					pt.y *= k;
				}
		}
	}

	out[0] = cggraph::Value::Make (Types ().extrudeContours, contours);
	return true;
}

} // namespace cggraph_nodes
