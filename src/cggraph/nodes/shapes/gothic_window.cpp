#include "gothic_window.h"

#include <algorithm>
#include <vector>

#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/parameterized.h"
#include "../../../cgmesh/parameterized_shapes.h"
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
		d.typeName = "shape.gothic.window";
		// Les deux sous-objets geometriques promus. OPTIONNELS : une baie sans
		// moulure est le cas le plus courant, et c'est la position « Flat » du
		// menu qu'ils remplacent.
		d.inputs.push_back ({ "profil d'ebrasement", Types ().splayProfile, true });
		d.inputs.push_back ({ "profil de barre", Types ().barProfile, true });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

// Le nom du parametre « Profile », qui n'a PAS sa place ici : les deux ports le
// remplacent. Ecrit une fois, pour que le filtre du constructeur et le test qui
// le verifie ne puissent pas diverger.
const char *const kMenuParam = "Profile";

} // namespace

GothicWindowNode::GothicWindowNode ()
{
	// Valeurs par defaut relevees sur la forme -- « Profile » excepte.
	ParameterizedGothicWindow shape;
	for (Parameter &p : shape.GetParameters ())
	{
		if (p.GetName () == kMenuParam)
			continue;
		switch (p.GetType ())
		{
		case Parameter::INT:
		case Parameter::ENUM:
			GetParams ().SetInt (p.GetName (), p.GetInt ());
			break;
		case Parameter::FLOAT:
			GetParams ().SetFloat (p.GetName (), p.GetFloat ());
			break;
		case Parameter::BOOL:
			GetParams ().SetBool (p.GetName (), p.GetBool ());
			break;
		case Parameter::STRING:
			GetParams ().SetString (p.GetName (), p.GetString ());
			break;
		}
	}
}

const cggraph::NodeDesc &GothicWindowNode::GetDesc () const
{
	return Desc ();
}

bool GothicWindowNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                cggraph::ValueList &out)
{
	(void)ctx;

	// Instance LOCALE, comme l'adaptateur generique : Regenerate () ecrit dans
	// l'objet, un membre serait un etat partage entre deux evaluations.
	ParameterizedGothicWindow shape;

	const std::size_t count = shape.GetParameters ().size ();
	for (std::size_t i = 0; i < count; ++i)
	{
		std::vector<Parameter> declared = shape.GetParameters ();
		if (i >= declared.size ())
			break;
		Parameter &p = declared[i];
		if (p.GetName () == kMenuParam)
			continue;      // remplace par les ports

		switch (p.GetType ())
		{
		case Parameter::INT:
		{
			int v = GetInt (GetParams (), p.GetName (), p.GetInt ());
			p.SetInt (std::max (p.GetMinInt (), std::min (p.GetMaxInt (), v)));
			break;
		}
		case Parameter::ENUM:
		{
			int v = GetInt (GetParams (), p.GetName (), p.GetInt ());
			const int last = static_cast<int> (p.GetChoices ().size ()) - 1;
			p.SetInt (std::max (0, std::min (last < 0 ? 0 : last, v)));
			break;
		}
		case Parameter::FLOAT:
		{
			float v = GetFloat (GetParams (), p.GetName (), p.GetFloat ());
			p.SetFloat (std::max (p.GetMinFloat (), std::min (p.GetMaxFloat (), v)));
			break;
		}
		case Parameter::BOOL:
			p.SetBool (GetBool (GetParams (), p.GetName (), p.GetBool ()));
			break;
		case Parameter::STRING:
			p.SetString (GetString (GetParams (), p.GetName (), p.GetString ()));
			break;
		}
	}

	// Les sous-objets promus. Une entree optionnelle non alimentee reste VIDE ;
	// le noeud en decide, et l'absence des deux redonne exactement la position
	// « Flat ».
	const Profile2D *splay = in[0].Get<Profile2D> (Types ().splayProfile);
	const Profile2D *bar = in[1].Get<Profile2D> (Types ().barProfile);
	shape.SetSplayProfile (splay);
	shape.SetBarProfile (bar);

	shape.Regenerate ();

	// CESSION : le maillage de travail est cede, jamais recopie.
	std::shared_ptr<Mesh> mesh (shape.TakeMesh ());
	if (mesh == nullptr)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, std::move (mesh));
	return true;
}

} // namespace cggraph_nodes
