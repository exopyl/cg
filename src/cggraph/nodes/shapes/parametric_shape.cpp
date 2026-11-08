#include "parametric_shape.h"

#include <algorithm>
#include <deque>
#include <map>

#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/parameterized.h"
#include "../../../cgmesh/parametric_catalog.h"
#include "../node_support.h"
#include "../value_types.h"
#include "shape_catalog.h"

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

} // namespace

// GetParameters () est relu AVANT chaque ecriture, et ce n'est pas une
// precaution de style : les bornes de « Iterations » du L-systeme sont celles
// du systeme courant, qui vient d'etre pose par l'iteration precedente. Une
// liste relevee une seule fois donnerait les bornes du systeme par defaut.
void ApplyShapeParams (IParameterized &shape, const cggraph::ParamSet &params)
{
	const std::size_t count = shape.GetParameters ().size ();
	for (std::size_t i = 0; i < count; ++i)
	{
		std::vector<Parameter> declared = shape.GetParameters ();
		if (i >= declared.size ())
			break;
		Parameter &p = declared[i];
		const std::string &name = p.Id ();

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

void DeclareShapeParams (IParameterized &shape, cggraph::ParamSet &params)
{
	for (Parameter &p : shape.GetParameters ())
	{
		switch (p.GetType ())
		{
		case Parameter::INT:
		case Parameter::ENUM:
			params.SetInt (p.Id (), p.GetInt ());
			break;
		case Parameter::FLOAT:
			params.SetFloat (p.Id (), p.GetFloat ());
			break;
		case Parameter::BOOL:
			params.SetBool (p.Id (), p.GetBool ());
			break;
		case Parameter::STRING:
			params.SetString (p.Id (), p.GetString ());
			break;
		}
	}
}

namespace
{

// Ce que la couche ajoute a une forme du catalogue : son libelle de noeud et sa
// reserve. La forme est designee par son identifiant au catalogue, qui est aussi
// le type du noeud : un renommage de la forme ne retire pas la liaison.
struct ShapeNote
{
	const char *shapeId;
	const char *label;
	const char *caveat;
};

const std::vector<ShapeNote> &ShapeNotes ()
{
	static const std::vector<ShapeNote> notes = {
		{ "shape.cube", "Cube",
		  SHAPE_CAVEAT_COMMON " CreateCube rend une arete de 2 : « Edge length » "
		  "est applique en facteur edge/2 apres coup, donc c'est bien l'arete "
		  "finale" },
		{ "shape.sphere", "Sphere", SHAPE_CAVEAT_COMMON },
		{ "shape.cylinder", "Cylindre", SHAPE_CAVEAT_COMMON },
		{ "shape.cone", "Cone", SHAPE_CAVEAT_COMMON },
		{ "shape.capsule", "Capsule", SHAPE_CAVEAT_COMMON },
		{ "shape.torus", "Tore", SHAPE_CAVEAT_COMMON },
		{ "shape.seashell", "Coquillage", SHAPE_CAVEAT_COMMON },
		{ "shape.seashell.von_seggern", "Coquillage (von Seggern)", SHAPE_CAVEAT_COMMON },
		{ "shape.klein_bottle", "Bouteille de Klein", SHAPE_CAVEAT_COMMON },
		{ "shape.breather", "Breather", SHAPE_CAVEAT_COMMON },
		{ "shape.hyperbolic_paraboloid", "Paraboloide hyperbolique",
		  SHAPE_CAVEAT_COMMON " les bornes de xmin/ymin sont negatives et celles "
		  "de xmax/ymax positives : rien n'empeche un domaine degenere a zero, "
		  "qui rend une nappe vide sans le dire" },
		{ "shape.monkey_saddle", "Selle de singe",
		  SHAPE_CAVEAT_COMMON " meme domaine degenerable que le paraboloide" },
		{ "shape.blobs", "Blobs",
		  SHAPE_CAVEAT_COMMON " meme domaine degenerable que le paraboloide" },
		{ "shape.drop", "Goutte",
		  SHAPE_CAVEAT_COMMON " meme domaine degenerable que le paraboloide" },
		{ "shape.knot.torus", "Noeud torique", SHAPE_CAVEAT_COMMON },
		{ "shape.knot.cinquefoil", "Noeud quintefeuille", SHAPE_CAVEAT_COMMON },
		{ "shape.knot.trefoil", "Noeud de trefle", SHAPE_CAVEAT_COMMON },
		{ "shape.borromean_rings", "Anneaux borromeens", SHAPE_CAVEAT_COMMON },
		{ "shape.helicoid", "Helicoide", SHAPE_CAVEAT_COMMON },
		{ "shape.corkscrew", "Tire-bouchon", SHAPE_CAVEAT_COMMON },
		{ "shape.mobius_strip", "Ruban de Mobius", SHAPE_CAVEAT_COMMON },
		{ "shape.radial_wave", "Onde radiale", SHAPE_CAVEAT_COMMON },
		{ "shape.guimard", "Guimard", SHAPE_CAVEAT_COMMON },
		{ "shape.menger_sponge", "Eponge de Menger",
		  SHAPE_CAVEAT_COMMON " MengerSponge::ToMesh peut rendre NUL, seul cas du "
		  "catalogue ou Regenerate () laisse m_pMesh nul plutot que de dereferencer "
		  "-- l'adaptateur rend alors un echec. Le niveau 4 vaut 20^4 cubes" },
		{ "shape.lsystem", "L-systeme",
		  SHAPE_CAVEAT_COMMON " le plafond d'iterations depend du SYSTEME et le "
		  "corps le rapplique sur une copie, donc un reglage trop haut est "
		  "silencieusement ramene ; le trace est de surcroit tronque a 60 000 "
		  "segments sans avertissement, et les reglages Height/Join/Cap n'ont "
		  "d'effet qu'en mode Extrusion" },
		{ "shape.gothic.block", "Bloc gothique",
		  SHAPE_CAVEAT_COMMON " CreateBlock n'est pas verifie avant "
		  "ComputeNormals ()" },
		{ "shape.classical.column", "Colonne classique (Vignole)",
		  SHAPE_CAVEAT_COMMON " Les donnees des cinq ordres sont embarquees ; une "
		  "generation en echec rend un maillage VIDE (LastError), pas une "
		  "exception. Seules les volutes ioniques sont modelisees (coussinets "
		  "concatenes au tour, sans union) ; volutes composites, feuillages et "
		  "ornements de l'entablement ne le sont pas" },
		{ "shape.classical.colonnade", "Colonnade classique (Vignole)",
		  SHAPE_CAVEAT_COMMON " Colonnes, entablement file et familles d'ornements sont "
		  "des coques fermees concatenees, sans union ; au-dela de 300 000 faces, "
		  "Warnings () le signale (12 colonnes cannelees a volutes en approchent "
		  "le million)" }
	};
	return notes;
}

} // namespace

std::size_t ShapeNoteCount ()
{
	return ShapeNotes ().size ();
}

const std::vector<ShapeBinding> &ShapeBindings ()
{
	static const std::vector<ShapeBinding> bindings = [] {
		std::vector<ShapeBinding> out;
		out.reserve (ShapeNotes ().size ());
		for (const ShapeNote &note : ShapeNotes ())
			if (const ParametricShapeEntry *entry = FindCatalogShapeById (note.shapeId))
				out.push_back ({ entry, entry->id, note.label, note.caveat });
		return out;
	}();
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
	std::unique_ptr<IParameterized> shape = binding.shape->make ();
	if (shape == nullptr)
		return;
	DeclareShapeParams (*shape, GetParams ());
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
	std::unique_ptr<IParameterized> shape = m_binding->shape->make ();
	if (shape == nullptr)
		return false;

	ApplyShapeParams (*shape, GetParams ());
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
