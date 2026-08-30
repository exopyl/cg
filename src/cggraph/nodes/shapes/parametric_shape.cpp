#include "parametric_shape.h"

#include <algorithm>
#include <deque>
#include <map>

#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/parameterized.h"
#include "../../../cgmesh/parametric_catalog.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

// Reserve COMMUNE aux formes emballees par cet adaptateur -- elle vient de la
// lecture des corps, pas de leurs declarations. Concatenee a la reserve propre
// de chaque forme, quand celle-ci en a une.
#define SHAPE_CAVEAT_COMMON                                                     \
	"le constructeur de la forme appelle Regenerate () : chaque evaluation "    \
	"paie une generation JETEE, aux valeurs par defaut, avant celle qui "       \
	"compte. Et Regenerate () ne verifie pas ce que son generateur rend -- "    \
	"seule l'eponge de Menger teste le pointeur avant ComputeNormals (). "      \
	"Les bornes vivent dans Parameter, pas dans le ParamSet : c'est "           \
	"l'adaptateur qui les applique."

const cggraph::NodeDesc &DescOf (const ShapeBinding &binding);

// Ecrit dans la forme les valeurs du jeu de parametres, bornes comprises.
//
// GetParameters () est relu AVANT chaque ecriture, et ce n'est pas une
// precaution de style : les bornes de « Iterations » du L-systeme sont celles
// du systeme courant, qui vient d'etre pose par l'iteration precedente. Une
// liste relevee une seule fois donnerait les bornes du systeme par defaut.
void ApplyParams (IParameterized &shape, const cggraph::ParamSet &params)
{
	const std::size_t count = shape.GetParameters ().size ();
	for (std::size_t i = 0; i < count; ++i)
	{
		std::vector<Parameter> declared = shape.GetParameters ();
		if (i >= declared.size ())
			break;
		Parameter &p = declared[i];
		const std::string &name = p.GetName ();

		switch (p.GetType ())
		{
		case Parameter::INT:
		{
			int v = GetInt (params, name, p.GetInt ());
			v = std::max (p.GetMinInt (), std::min (p.GetMaxInt (), v));
			p.SetInt (v);
			break;
		}
		case Parameter::ENUM:
		{
			// MakeEnum ne pose ni min ni max : la borne est la liste de choix.
			int v = GetInt (params, name, p.GetInt ());
			const int last = static_cast<int> (p.GetChoices ().size ()) - 1;
			v = std::max (0, std::min (last < 0 ? 0 : last, v));
			p.SetInt (v);
			break;
		}
		case Parameter::FLOAT:
		{
			float v = GetFloat (params, name, p.GetFloat ());
			v = std::max (p.GetMinFloat (), std::min (p.GetMaxFloat (), v));
			p.SetFloat (v);
			break;
		}
		case Parameter::BOOL:
			p.SetBool (GetBool (params, name, p.GetBool ()));
			break;
		case Parameter::STRING:
			// Sans bornes par nature. Aucune forme de ce catalogue n'en expose
			// aujourd'hui -- la seule qui le fasse, le texte 3D, tient sa police
			// d'un constructeur et n'entre donc pas ici.
			p.SetString (GetString (params, name, p.GetString ()));
			break;
		}
	}
}

} // namespace

const std::vector<ShapeBinding> &ShapeBindings ()
{
	static const std::vector<ShapeBinding> bindings = {
		{ "shape.cube", "Cube", "Cube",
		  SHAPE_CAVEAT_COMMON " CreateCube rend une arete de 2 : « Edge length » "
		  "est applique en facteur edge/2 apres coup, donc c'est bien l'arete "
		  "finale" },
		{ "shape.sphere", "Sphere", "Sphere", SHAPE_CAVEAT_COMMON },
		{ "shape.cylinder", "Cylinder", "Cylindre", SHAPE_CAVEAT_COMMON },
		{ "shape.cone", "Cone", "Cone", SHAPE_CAVEAT_COMMON },
		{ "shape.capsule", "Capsule", "Capsule", SHAPE_CAVEAT_COMMON },
		{ "shape.torus", "Torus", "Tore", SHAPE_CAVEAT_COMMON },
		{ "shape.seashell", "Seashell", "Coquillage", SHAPE_CAVEAT_COMMON },
		{ "shape.seashell.von_seggern", "Seashell (von Seggern)",
		  "Coquillage (von Seggern)", SHAPE_CAVEAT_COMMON },
		{ "shape.klein_bottle", "Klein Bottle", "Bouteille de Klein", SHAPE_CAVEAT_COMMON },
		{ "shape.breather", "Breather", "Breather", SHAPE_CAVEAT_COMMON },
		{ "shape.hyperbolic_paraboloid", "Hyperbolic Paraboloid",
		  "Paraboloide hyperbolique",
		  SHAPE_CAVEAT_COMMON " les bornes de xmin/ymin sont negatives et celles "
		  "de xmax/ymax positives : rien n'empeche un domaine degenere a zero, "
		  "qui rend une nappe vide sans le dire" },
		{ "shape.monkey_saddle", "Monkey Saddle", "Selle de singe",
		  SHAPE_CAVEAT_COMMON " meme domaine degenerable que le paraboloide" },
		{ "shape.blobs", "Blobs", "Blobs",
		  SHAPE_CAVEAT_COMMON " meme domaine degenerable que le paraboloide" },
		{ "shape.drop", "Drop", "Goutte",
		  SHAPE_CAVEAT_COMMON " meme domaine degenerable que le paraboloide" },
		{ "shape.knot.torus", "Torus Knot", "Noeud torique", SHAPE_CAVEAT_COMMON },
		{ "shape.knot.cinquefoil", "Cinquefoil Knot", "Noeud quintefeuille",
		  SHAPE_CAVEAT_COMMON },
		{ "shape.knot.trefoil", "Trefoil Knot", "Noeud de trefle", SHAPE_CAVEAT_COMMON },
		{ "shape.borromean_rings", "Borromean Rings", "Anneaux borromeens",
		  SHAPE_CAVEAT_COMMON },
		{ "shape.helicoid", "Helicoid", "Helicoide", SHAPE_CAVEAT_COMMON },
		{ "shape.corkscrew", "Corkscrew", "Tire-bouchon", SHAPE_CAVEAT_COMMON },
		{ "shape.mobius_strip", "Mobius Strip", "Ruban de Mobius", SHAPE_CAVEAT_COMMON },
		{ "shape.radial_wave", "Radial Wave", "Onde radiale", SHAPE_CAVEAT_COMMON },
		{ "shape.guimard", "Guimard", "Guimard", SHAPE_CAVEAT_COMMON },
		{ "shape.menger_sponge", "Menger Sponge", "Eponge de Menger",
		  SHAPE_CAVEAT_COMMON " MengerSponge::ToMesh peut rendre NUL, seul cas du "
		  "catalogue ou Regenerate () laisse m_pMesh nul plutot que de dereferencer "
		  "-- l'adaptateur rend alors un echec. Le niveau 4 vaut 20^4 cubes" },
		{ "shape.lsystem", "L-system", "L-systeme",
		  SHAPE_CAVEAT_COMMON " le plafond d'iterations depend du SYSTEME et le "
		  "corps le rapplique sur une copie, donc un reglage trop haut est "
		  "silencieusement ramene ; le trace est de surcroit tronque a 60 000 "
		  "segments sans avertissement, et les reglages Height/Join/Cap n'ont "
		  "d'effet qu'en mode Extrusion" },
		{ "shape.gothic.block", "Gothic Block", "Bloc gothique",
		  SHAPE_CAVEAT_COMMON " CreateBlock n'est pas verifie avant "
		  "ComputeNormals ()" }
	};
	return bindings;
}

const ShapeBinding *FindShapeBinding (const std::string &typeName)
{
	for (const ShapeBinding &binding : ShapeBindings ())
		if (typeName == binding.typeName)
			return &binding;
	return nullptr;
}

namespace
{

// Un descripteur par type, construits TOUS en une fois : leur adresse est
// distribuee, donc aucun ne doit bouger, et une construction paresseuse au fil
// des demandes serait une ecriture concurrente de plus.
struct DescTable
{
	std::deque<cggraph::NodeDesc> storage;
	std::map<std::string, const cggraph::NodeDesc *> byName;
};

const DescTable &Descs ()
{
	static const DescTable table = [] {
		DescTable t;
		for (const ShapeBinding &binding : ShapeBindings ())
		{
			cggraph::NodeDesc d;
			d.typeName = binding.typeName;
			// Aucune entree, et c'est la definition meme d'un generateur sans
			// ressource : tout ce qu'il consomme est un parametre.
			d.outputs.push_back ({ "maillage", Types ().mesh, false });
			t.storage.push_back (std::move (d));
			t.byName[binding.typeName] = &t.storage.back ();
		}
		return t;
	}();
	return table;
}

const cggraph::NodeDesc &DescOf (const ShapeBinding &binding)
{
	const DescTable &table = Descs ();
	auto it = table.byName.find (binding.typeName);
	// La liaison vient de ShapeBindings (), qui a construit la table : la
	// recherche ne peut pas echouer.
	return *it->second;
}

} // namespace

ParametricShapeNode::ParametricShapeNode (const ShapeBinding &binding)
	: m_binding (&binding), m_desc (&DescOf (binding))
{
	// Les valeurs par defaut du noeud sont celles de la forme, relevees sur une
	// instance neuve. C'est la seule facon de les connaitre : elles ne sont
	// declarees nulle part ailleurs que dans les membres de la classe emballee.
	std::unique_ptr<IParameterized> shape = MakeParametricShape (binding.shapeName);
	if (shape == nullptr)
		return;
	for (Parameter &p : shape->GetParameters ())
	{
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

const cggraph::NodeDesc &ParametricShapeNode::GetDesc () const
{
	return *m_desc;
}

bool ParametricShapeNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                   cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;

	// La forme est construite ICI, et jetee a la sortie : le noeud n'en detient
	// AUCUNE entre deux calculs. Ce n'est pas de la frugalite -- deux
	// evaluations concurrentes du meme graphe partageraient l'objet, et
	// Regenerate () ecrit dedans. Une instance par calcul rend la question sans
	// objet.
	std::unique_ptr<IParameterized> shape = MakeParametricShape (m_binding->shapeName);
	if (shape == nullptr)
		return false;

	ApplyParams (*shape, GetParams ());
	shape->Regenerate ();

	// CESSION : TakeMesh () transfere la propriete du maillage de travail. Le
	// recopier ici doublerait le pic pour rien.
	std::shared_ptr<Mesh> mesh (shape->TakeMesh ());
	if (mesh == nullptr)
		return false;

	out[0] = cggraph::Value::Make (Types ().mesh, std::move (mesh));
	return true;
}

#undef SHAPE_CAVEAT_COMMON

} // namespace cggraph_nodes
