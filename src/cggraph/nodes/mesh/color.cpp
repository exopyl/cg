#include "color.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "../../../cgmesh/material.h"
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
		d.typeName = "mesh.color";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

// « #rrggbb » -> trois octets. Le format est celui de <input type="color"> du
// navigateur, donc celui que le gabarit ecrit sans conversion : c'est ce qui
// permet a la couleur d'etre LISIBLE dans le document enregistre plutot que
// d'etre un entier qu'il faut decoder pour savoir de quoi il s'agit.
//
// Rend false sur tout ce qui n'est pas exactement sept caracteres commencant par
// '#' et suivis de six chiffres hexadecimaux -- un texte fautif ne doit pas
// devenir une couleur au hasard.
bool parseHexColor (const std::string &text, unsigned char &r, unsigned char &g,
                    unsigned char &b)
{
	if (text.size () != 7 || text[0] != '#')
		return false;

	unsigned int channels[3] = { 0, 0, 0 };
	for (int c = 0; c < 3; ++c)
	{
		unsigned int value = 0;
		for (int k = 0; k < 2; ++k)
		{
			const char ch = text[1 + 2 * c + k];
			unsigned int digit;
			if (ch >= '0' && ch <= '9') digit = (unsigned int)(ch - '0');
			else if (ch >= 'a' && ch <= 'f') digit = (unsigned int)(ch - 'a') + 10u;
			else if (ch >= 'A' && ch <= 'F') digit = (unsigned int)(ch - 'A') + 10u;
			else return false;
			value = value * 16u + digit;
		}
		channels[c] = value;
	}
	r = (unsigned char)channels[0];
	g = (unsigned char)channels[1];
	b = (unsigned char)channels[2];
	return true;
}

} // namespace

ColorMeshNode::ColorMeshNode ()
{
	// Le gris bleute que la page servait par defaut : la piece ne change pas
	// d'aspect en devenant un noeud.
	GetParams ().SetString ("color", "#b4bec8");
}

const cggraph::NodeDesc &ColorMeshNode::GetDesc () const
{
	return Desc ();
}

void ColorMeshNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	out.push_back ({ "paintedFaces", (double)m_painted.load () });
	out.push_back ({ "keptFaces", (double)m_kept.load () });
}

bool ColorMeshNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                             cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	unsigned char r = 0, g = 0, b = 0;
	// REFUS et non couleur de secours : une chaine fautive rendue en gris
	// laisserait croire que le reglage a ete pris.
	if (!parseHexColor (GetString (GetParams (), "color", std::string ()), r, g, b))
		return false;

	// Copie profonde : l'entree est LUE, jamais ecrite -- un autre consommateur
	// peut tenir le meme maillage.
	std::shared_ptr<Mesh> painted = std::make_shared<Mesh> (*input);

	MaterialColor *material = new MaterialColor (r, g, b, 255);
	material->SetName ("couleur");
	const unsigned int id = painted->Material_Add (material);

	// MATERIAL_NONE vaut (unsigned)-1, donc -1 une fois lu en int : ce sont les
	// faces que personne n'a peintes, et les seules qu'on peigne (cf. l'en-tete).
	unsigned int paintedCount = 0, keptCount = 0;
	for (unsigned int f = 0; f < painted->GetNFaces (); ++f)
	{
		if (painted->GetFaceMaterialId (f) < 0)
		{
			painted->SetFaceMaterialId (f, id);
			paintedCount++;
		}
		else
		{
			keptCount++;
		}
	}
	m_painted.store (paintedCount);
	m_kept.store (keptCount);

	painted->IncrementRevision ();
	out[0] = cggraph::Value::Make (Types ().mesh, painted);
	return true;
}

} // namespace cggraph_nodes
