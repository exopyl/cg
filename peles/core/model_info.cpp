#include "model_info.h"

#include "bounding_box.h"
#include "material.h"
#include "material_pbr.h"
#include "mesh.h"
#include "vmodels.h"

#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <string>

namespace peles
{
namespace core
{

namespace
{

// ⚠ LES FONCTIONS VARIADIQUES NE FONT VERIFIER AUCUN DE LEURS ARGUMENTS. Tous
// les appels ci-dessous convertissent EXPLICITEMENT vers le type que la
// conversion attend -- `unsigned int` pour %u, `double` pour les formats
// flottants -- plutot que de compter sur la promotion par defaut.
std::string Text (const char* format, ...)
{
	char    buffer[512];
	va_list arguments;
	va_start (arguments, format);
	const int written = std::vsnprintf (buffer, sizeof (buffer), format, arguments);
	va_end (arguments);

	if (written < 0)
		return std::string ();

	// ⚠ UNE TRONCATURE SILENCIEUSE SE LIRAIT COMME UN NOM COURT. vsnprintf rend
	// la longueur QU'IL AURAIT ECRITE : un nom de materiau ou un chemin de
	// texture au-dela du tampon serait coupe au milieu, et rien ne distinguerait
	// le resultat d'un nom reellement court. Le marqueur est la seule chose qui
	// rende la coupure lisible -- y compris dans l'oracle textuel.
	if ((std::size_t)written >= sizeof (buffer))
		return std::string (buffer, sizeof (buffer) - 1) + "...(tronque)";

	return std::string (buffer, (std::size_t)written);
}

// LA CLASSE DE PROFONDEUR, ET RIEN D'AUTRE.
std::string DepthClass (int depth)
{
	const int clamped =
	    depth < 0 ? 0 : (depth > kMaxInfoDepthClass ? kMaxInfoDepthClass : depth);
	return "d" + std::to_string (clamped);
}

// LA COULEUR DE LA PASTILLE, EN "#rrggbb".
//
// ⚠ LES COMPOSANTES SONT BORNEES. Material les porte en 0..1 par convention,
// mais rien ne le garantit : un MTL mal forme peut porter n'importe quoi, et
// une composante hors bornes produirait une valeur que RCSS refuserait -- donc
// un LT_WARNING a chaque image.
//
// ⚠ LA BORNE PORTE SUR LE FLOTTANT, ET ELLE DOIT Y PORTER. Convertir d'abord
// puis borner l'entier ne borne PAS la conversion : une valeur dont la partie
// entiere ne tient pas dans `int` -- au-dela d'environ 8,4e6 une fois
// multipliee par 255 -- et NaN sont des COMPORTEMENTS INDEFINIS
// ([conv.fpint]/1). x86-64 rend alors INT_MIN, ARM64 sature : la meme pastille
// sortirait noire sur une cible et blanche sur l'autre.
//
// ⚠ `!(value > 0.f)` ET NON `value <= 0.f` : c'est la forme qui attrape aussi
// NaN, pour lequel toute comparaison est fausse.
std::string Swatch (float r, float g, float b)
{
	auto to8 = [] (float value) -> int
	{
		if (!(value > 0.f))
			return 0;
		if (value >= 1.f)
			return 255;
		return (int)(value * 255.f + 0.5f);
	};
	return Text ("#%02x%02x%02x", (unsigned int)to8 (r), (unsigned int)to8 (g),
	             (unsigned int)to8 (b));
}

InfoRow Property (const char* label, std::string value)
{
	InfoRow row;
	row.label      = label;
	row.value      = std::move (value);
	row.styleClass = DepthClass (0);
	return row;
}

InfoRow Node (int depth, std::string label)
{
	InfoRow row;
	row.label      = std::move (label);
	row.depth      = depth;
	row.styleClass = DepthClass (depth);
	return row;
}

InfoRow ColorNode (int depth, std::string label, float r, float g, float b)
{
	InfoRow row  = Node (depth, std::move (label));
	row.swatch   = Swatch (r, g, b);
	// ⚠ LA CLASSE EST CE QUI REND LA PASTILLE VISIBLE. Le liseré de l'hote
	// historique n'etait pas decoratif : sans lui, un materiau blanc se confond
	// avec le fond. Ici il est une regle de feuille de style, posee sur les
	// seules lignes qui portent une couleur.
	row.styleClass += " sw";
	return row;
}

// PLAFOND DES LIGNES DE CONTENU : kMaxInfoRows moins la place de la ligne de
// debordement.
//
// ⚠ LA PLACE EST RESERVEE D'AVANCE, ET NON PRISE APRES COUP. Poser la ligne de
// debordement sur un vecteur deja plein en produirait kMaxInfoRows + 1, donc le
// plafond cesserait d'en etre un -- et il borne le TOTAL, seule grandeur dont le
// cout de liage depende. Retirer la derniere ligne pour lui faire place rendrait
// en revanche le compte annonce FAUX : cette ligne peut etre le NOM d'un element
// que l'on vient de compter comme affiche, qui deviendrait alors absent sans que
// le compte bouge.
constexpr int kMaxInfoContentRows = kMaxInfoRows - 1;

// Rend false quand le plafond est atteint : l'appelant cesse d'ajouter et pose
// la ligne de debordement.
bool Push (std::vector<InfoRow>& rows, InfoRow row)
{
	if ((int)rows.size () >= kMaxInfoContentRows)
		return false;
	rows.push_back (std::move (row));
	return true;
}

// LA LIGNE DE DEBORDEMENT OCCUPE LA PLACE QUE kMaxInfoContentRows LUI RESERVE.
//
// `unit` nomme ce qui est compte : « maillage(s) » ou « materiau(x) ». Il est
// un parametre parce que les deux arbres posent la meme ligne sur deux
// grandeurs differentes, et qu'un libelle en dur dans l'un des deux mentirait
// dans l'autre.
//
// ⚠ LA LIGNE EST POSEE DES QUE QUELQUE CHOSE A ETE COUPE, ET `hidden` PEUT
// VALOIR ZERO. Un element dont le nom est affiche mais dont les details sont
// tombes n'est pas « absent » : le compte ne le prend pas, mais la coupure doit
// quand meme etre annoncee -- c'est la troncature muette que ce lot traite comme
// un defaut.
void PushOverflow (std::vector<InfoRow>& rows, int hidden, const char* unit)
{
	InfoRow row;
	row.label      = "... plafond de " + std::to_string (kMaxInfoRows) +
	            " ligne(s) atteint : la suite est coupee, " + std::to_string (hidden) + " " +
	            unit + " entierement absent(s)";
	row.styleClass = DepthClass (1) + " overflow";
	rows.push_back (std::move (row));
}

// LES CINQ EMPLACEMENTS DE CARTE DU MODELE METALLIC-ROUGHNESS.
//
// ⚠ TABLE INDEXEE PAR cgpbr::MapSlot : un emplacement ajoute a l'enumeration
// ferait lire hors bornes sans un mot du compilateur, et la garde ci-dessous ne
// porte que sur le CARDINAL -- reordonner l'enumeration laisse `count == 5` et
// fait afficher les mauvais libelles. L'ordre de cette table suit celui de
// l'enumeration.
const char* const kPbrSlotNames[] = { "Base color", "Normal", "Metallic-roughness", "Occlusion",
	                                  "Emissive" };
static_assert (std::size (kPbrSlotNames) == (std::size_t)cgpbr::MapSlot::count,
               "kPbrSlotNames doit couvrir exactement cgpbr::MapSlot");

// ---------------------------------------------------------------------------
//  LES QUATRE BRANCHES DE TYPE
// ---------------------------------------------------------------------------
//
// Chacune rend false des que le plafond de lignes est atteint.

bool AppendColorDetails (std::vector<InfoRow>& rows, const MaterialColor& material)
{
	return Push (rows,
	             ColorNode (3,
	                        Text ("Color: RGB(%.2f, %.2f, %.2f)",
	                              (double)material.GetFloatRed (),
	                              (double)material.GetFloatGreen (),
	                              (double)material.GetFloatBlue ()),
	                        material.GetFloatRed (), material.GetFloatGreen (),
	                        material.GetFloatBlue ()));
}

bool AppendTextureDetails (std::vector<InfoRow>& rows, MaterialTexture& material)
{
	if (!Push (rows, Node (3, Text ("Texture: %s", material.GetFilename ().c_str ()))))
		return false;

	// ETAT DU DECODAGE. Un nom present sans image veut dire fichier introuvable
	// ou format non gere : sans cette ligne, le panneau laisserait croire que la
	// texture est en place alors que rien n'a ete charge.
	const Img* image = material.GetImage ();
	if (!Push (rows, Node (4, image != nullptr
	                             ? Text ("Size: %u x %u", image->width (), image->height ())
	                             : std::string ("Size: (image non chargee)"))))
		return false;

	if (!material.GetReflectionFilename ().empty ())
	{
		if (!Push (rows,
		           Node (3, Text ("Reflection: %s", material.GetReflectionFilename ().c_str ()))))
			return false;

		const Img* reflection = material.GetReflectionImage ();
		if (!Push (rows,
		           Node (4, reflection != nullptr ? Text ("Size: %u x %u", reflection->width (),
		                                                  reflection->height ())
		                                          : std::string ("Size: (image non chargee)"))))
			return false;
	}

	// LES COULEURS D'ECLAIRAGE D'UN MATERIAU TEXTURE MODULENT LE TEXEL : les
	// voir explique pourquoi une texture peut sortir noire (diffuse quasi nulle)
	// ou saturee (speculaire blanche a exposant nul).
	const float* diffuse  = material.GetDiffuse ();
	const float* ambient  = material.GetAmbient ();
	const float* specular = material.GetSpecular ();

	if (!Push (rows, ColorNode (4,
	                            Text ("Diffuse: RGB(%.2f, %.2f, %.2f)", (double)diffuse[0],
	                                  (double)diffuse[1], (double)diffuse[2]),
	                            diffuse[0], diffuse[1], diffuse[2])))
		return false;
	if (!Push (rows, ColorNode (4,
	                            Text ("Ambient: RGB(%.2f, %.2f, %.2f)", (double)ambient[0],
	                                  (double)ambient[1], (double)ambient[2]),
	                            ambient[0], ambient[1], ambient[2])))
		return false;
	if (!Push (rows, ColorNode (4,
	                            Text ("Specular: RGB(%.2f, %.2f, %.2f)", (double)specular[0],
	                                  (double)specular[1], (double)specular[2]),
	                            specular[0], specular[1], specular[2])))
		return false;

	return Push (rows, Node (4, Text ("Shininess: %.3f  (GL %.0f)",
	                                  (double)material.GetShininess (),
	                                  128. * (double)material.GetShininess ())));
}

bool AppendColorExtDetails (std::vector<InfoRow>& rows, const MaterialColorExt& material)
{
	if (!Push (rows, Node (3, "Type: Advanced Color")))
		return false;

	const float* diffuse = material.GetDiffuse ();
	const float* ambient = material.GetAmbient ();

	if (!Push (rows, ColorNode (4,
	                            Text ("Diffuse: RGB(%.2f, %.2f, %.2f)", (double)diffuse[0],
	                                  (double)diffuse[1], (double)diffuse[2]),
	                            diffuse[0], diffuse[1], diffuse[2])))
		return false;
	return Push (rows, ColorNode (4,
	                              Text ("Ambient: RGB(%.2f, %.2f, %.2f)", (double)ambient[0],
	                                    (double)ambient[1], (double)ambient[2]),
	                              ambient[0], ambient[1], ambient[2]));
}

bool AppendPbrDetails (std::vector<InfoRow>& rows, const MaterialPbr& material)
{
	const cgpbr::Factors& factors = material.GetFactors ();

	if (!Push (rows, Node (3, "Type: PBR (metallic-roughness)")))
		return false;
	if (!Push (rows, ColorNode (4,
	                            Text ("Base color: RGBA(%.2f, %.2f, %.2f, %.2f)",
	                                  (double)factors.baseColor[0], (double)factors.baseColor[1],
	                                  (double)factors.baseColor[2], (double)factors.baseColor[3]),
	                            factors.baseColor[0], factors.baseColor[1], factors.baseColor[2])))
		return false;
	if (!Push (rows, Node (4, Text ("Metallic: %.3f   Roughness: %.3f", (double)factors.metallic,
	                                (double)factors.roughness))))
		return false;

	for (int slot = 0; slot < (int)cgpbr::MapSlot::count; ++slot)
	{
		const cgpbr::MapSlot named = (cgpbr::MapSlot)slot;
		if (!material.HasMap (named))
			continue;

		// LE JEU D'UV DESIGNE UN JEU DU MAILLAGE, pas l'indice ecrit dans le
		// fichier : l'afficher evite de chercher un TEXCOORD_1 absent du .gltf.
		const cgpbr::TextureRef& reference = material.GetMap (named);
		if (!Push (rows, Node (4, Text ("%s map: %s  (UV%u)", kPbrSlotNames[slot],
		                                reference.name.c_str (), (unsigned int)reference.uvSet))))
			return false;
	}

	return true;
}

bool AppendMaterialDetails (std::vector<InfoRow>& rows, Material& material)
{
	// ⚠ LE dynamic_cast EST CONFINE ICI, ET C'EST TOUT LE MOTIF DES DTO :
	// Material est POLYMORPHE, donc ni liable ni copiable sans trancher ses
	// sous-objets. Ce qui sort de cette fonction est du texte et des couleurs.
	switch (material.GetType ())
	{
	case MATERIAL_COLOR:
		if (const MaterialColor* color = dynamic_cast<MaterialColor*> (&material))
			return AppendColorDetails (rows, *color);
		return true;

	case MATERIAL_TEXTURE:
		if (MaterialTexture* texture = dynamic_cast<MaterialTexture*> (&material))
			return AppendTextureDetails (rows, *texture);
		return true;

	case MATERIAL_COLOR_ADV:
		if (const MaterialColorExt* extended = dynamic_cast<MaterialColorExt*> (&material))
			return AppendColorExtDetails (rows, *extended);
		return true;

	case MATERIAL_PBR:
		if (const MaterialPbr* pbr = dynamic_cast<MaterialPbr*> (&material))
			return AppendPbrDetails (rows, *pbr);
		return true;

	default:
		// MATERIAL_NONE : le materiau se reduit a son nom, deja pose par
		// l'appelant.
		return true;
	}
}

// ---------------------------------------------------------------------------
//  LA TABLE DE PROPRIETES, DECLAREE UNE SEULE FOIS
// ---------------------------------------------------------------------------
//
// ⚠ L'ORDRE DES QUATORZE LIGNES EST UN CONTRAT, pas une mise en forme : c'est
// celui de l'hote historique, suivi de la ligne « Path » qui remplace son
// infobulle. Il est verifie par un oracle versionne.
//
// ⚠ TABLE INDEXEE PAR InfoLine, et le static_assert ci-dessous tient le
// cardinal. Une ligne ajoutee a l'enumeration sans son entree ici -- ou
// l'inverse -- ne compile pas.
struct InfoLineSpec
{
	const char* label;

	// ⚠ « 0 » ET NON LA CHAINE VIDE SUR LES COMPTEURS : c'est ce que fait l'hote
	// historique, qui leur passe wxVariant(0). Les etendues et les tailles, elles,
	// restent vides -- zero y serait un nombre, donc une information fausse.
	const char* emptyValue;
};

const InfoLineSpec kInfoLines[] = {
	{ "File", "" },           { "Vertices", "0" },
	{ "Faces", "0" },         { "Meshes", "0" },
	{ "Triangle mesh", "" },  { "Borders", "0" },
	{ "Non manifold edges", "0" },
	{ "Bounding box X", "" }, { "Bounding box Y", "" },
	{ "Bounding box Z", "" }, { "Size X", "" },
	{ "Size Y", "" },         { "Size Z", "" },
	{ "Path", "" },
};
static_assert (std::size (kInfoLines) == (std::size_t)InfoLine::count,
               "kInfoLines doit couvrir exactement InfoLine");

// ⚠ LES TROIS AXES SONT CONSECUTIFS DANS L'ENUMERATION, et BuildModelInfo en
// depend pour ecrire ses six lignes par une boucle. Ce n'est pas une
// coincidence a preserver a la main : c'est ce que ces deux assertions tiennent.
static_assert ((int)InfoLine::BoundingBoxZ - (int)InfoLine::BoundingBoxX == 2,
               "les trois etendues de boite englobante doivent etre consecutives");
static_assert ((int)InfoLine::SizeZ - (int)InfoLine::SizeX == 2,
               "les trois tailles doivent etre consecutives");

InfoRow& At (std::vector<InfoRow>& rows, InfoLine line, int offset = 0)
{
	return rows[(std::size_t)((int)line + offset)];
}

} // namespace

std::vector<InfoRow> BuildEmptyModelInfo ()
{
	std::vector<InfoRow> rows;
	rows.reserve ((std::size_t)InfoLine::count);

	for (const InfoLineSpec& spec : kInfoLines)
		rows.push_back (Property (spec.label, spec.emptyValue));

	At (rows, InfoLine::Path).copyable = true;

	return rows;
}

std::vector<InfoRow> BuildModelInfo (Model& model, const TopologyStats& topology)
{
	std::vector<InfoRow> rows = BuildEmptyModelInfo ();

	VMeshes& meshes = model.m_meshes;

	At (rows, InfoLine::File).value         = model.m_name;
	At (rows, InfoLine::Vertices).value     = Text ("%u", meshes.GetNVertices ());
	At (rows, InfoLine::Faces).value        = Text ("%u", meshes.GetNFaces ());
	At (rows, InfoLine::Meshes).value       = Text ("%u", (unsigned int)meshes.GetNMeshes ());
	At (rows, InfoLine::TriangleMesh).value = meshes.IsTriangleMesh () ? "True" : "False";
	At (rows, InfoLine::Borders).value      = Text ("%u", topology.borders);
	At (rows, InfoLine::NonManifoldEdges).value = Text ("%u", topology.nonManifoldEdges);
	At (rows, InfoLine::Path).value             = model.m_path;

	// LA BOITE ENGLOBANTE : une etendue « [ min , max ] » par axe, puis une
	// taille par axe. Une boite vide laisse les six lignes vides -- zero y
	// serait un nombre, donc une information fausse.
	const BoundingBox& bbox = model.ComputeBBox ();
	if (!bbox.IsEmpty ())
	{
		float minimum[3] = { 0.f, 0.f, 0.f };
		float maximum[3] = { 0.f, 0.f, 0.f };
		bbox.GetMinMax (minimum, maximum);

		for (int axis = 0; axis < 3; ++axis)
		{
			At (rows, InfoLine::BoundingBoxX, axis).value =
			    Text ("[ %.4g , %.4g ]", (double)minimum[axis], (double)maximum[axis]);
			At (rows, InfoLine::SizeX, axis).value =
			    Text ("%.4g", (double)(maximum[axis] - minimum[axis]));
		}
	}

	return rows;
}

// ---------------------------------------------------------------------------
//  LES DEUX ARBRES
// ---------------------------------------------------------------------------

std::vector<InfoRow> BuildMeshTree (Model& model)
{
	std::vector<InfoRow> rows;

	if (!Push (rows, Node (0, "Meshes")))
		return rows;

	// ⚠ DEUX GRANDEURS DISTINCTES, ET LES CONFONDRE FAISAIT TAIRE UN CAS. « Une
	// coupure a eu lieu » n'est pas « N maillages sont absents » : le dernier
	// maillage nomme peut perdre ses seules feuilles, auquel cas le compte vaut
	// zero et la coupure est pourtant reelle.
	int  hiddenMeshes = 0;
	bool truncated    = false;

	for (Mesh* mesh : model.m_meshes.GetMeshes ())
	{
		if (mesh == nullptr)
			continue;

		// Une fois le plafond atteint, le test de gauche evite de construire des
		// lignes que Push refuserait.
		if (truncated || !Push (rows, Node (1, mesh->GetName ())))
		{
			truncated = true;
			++hiddenMeshes;
			continue;
		}

		// LES DEUX FEUILLES D'UN MAILLAGE DEJA NOMME. Leur refus ne le retire pas
		// du compte des affiches -- son nom est a l'ecran.
		if (!Push (rows, Node (2, Text ("%u vertices", mesh->GetNVertices ()))) ||
		    !Push (rows, Node (2, Text ("%u faces", mesh->GetNFaces ()))))
			truncated = true;
	}

	if (truncated)
		PushOverflow (rows, hiddenMeshes, "maillage(s)");

	return rows;
}

std::vector<InfoRow> BuildMaterialTree (Model& model)
{
	std::vector<InfoRow> rows;

	if (!Push (rows, Node (0, "Materials")))
		return rows;

	// MEMES DEUX GRANDEURS QUE DANS L'ARBRE DES MAILLAGES, pour la meme raison.
	int  hiddenMaterials = 0;
	bool truncated       = false;

	for (Mesh* mesh : model.m_meshes.GetMeshes ())
	{
		if (mesh == nullptr || mesh->GetNMaterials () == 0)
			continue;

		if (truncated || !Push (rows, Node (1, mesh->GetName ())))
		{
			truncated = true;
			hiddenMaterials += (int)mesh->GetNMaterials ();
			continue;
		}

		for (unsigned int index = 0; index < mesh->GetNMaterials (); ++index)
		{
			Material* material = mesh->GetMaterial (index);
			if (material == nullptr)
				continue;

			const std::string name = material->GetName ().empty ()
			                             ? Text ("Material %u", index)
			                             : material->GetName ();

			// ⚠ UN MATERIAU DONT LE NOM A ETE POSE N'EST PAS « NON AFFICHE » :
			// sa ligne est a l'ecran, seuls ses details manquent. Le compter
			// avec les absents donnerait un nombre d'une unite trop grand.
			const bool named = Push (rows, Node (2, name));
			if (!named || !AppendMaterialDetails (rows, *material))
			{
				truncated = true;
				hiddenMaterials += (int)(mesh->GetNMaterials () - index) - (named ? 1 : 0);
				break;
			}
		}
	}

	if (truncated)
		PushOverflow (rows, hiddenMaterials, "materiau(x)");

	return rows;
}

int CountMaterials (Model& model)
{
	int total = 0;
	for (const Mesh* mesh : model.m_meshes.GetMeshes ())
	{
		if (mesh != nullptr)
			total += (int)mesh->GetNMaterials ();
	}
	return total;
}

} // namespace core
} // namespace peles
