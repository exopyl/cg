// ---------------------------------------------------------------------------
//  LES ORACLES TEXTUELS : --dump-tree, --dump-files, --dump-info
// ---------------------------------------------------------------------------
//
// Les trois seuls modes qui n'ouvrent NI contexte GL NI document RML : ils
// n'appellent que peles_core. C'est ce qui les rend executables sur une machine
// sans interface graphique, donc en CI -- et c'est la raison pour laquelle ils
// vivent dans une unite a part, dont la liste d'inclusions le montre.
//
// Les declarations sont dans app/dump_modes.h, commun aux six unites.

#include "dump_modes.h"
#include "directory_tree.h"
#include "document.h"
#include "file_list.h"
#include "loader.h"
#include "model_info.h"
#include "topology_stats.h"

#include "vmodels.h"

#include <cstdio>

#ifdef _WIN32
	#include <fcntl.h>
	#include <io.h>
#endif

namespace peles
{
namespace app
{

namespace
{

// PROFONDEUR OUVERTE PAR --dump-tree.
//
// ⚠ ELLE EST BORNEE, ET C'EST INDISPENSABLE. Un vidage qui ouvrirait tout
// parcourrait le disque entier sur `--dump-tree C:\` : l'instrument doit
// terminer sur n'importe quelle entree, pas seulement sur la fixture pour
// laquelle il a ete ecrit. Trois niveaux suffisent a exercer le chargement
// paresseux, l'indentation et le tri a plusieurs profondeurs.
const int kDumpTreeDepth = 3;

// ⚠ LA SORTIE D'UN ORACLE NE DOIT PAS DEPENDRE DE LA CONVENTION DE FIN DE LIGNE
// DE LA PLATEFORME. En mode texte, le CRT de Windows traduit chaque \n en
// \r\n : le meme vidage, produit par le meme code, differerait d'un octet par
// ligne entre Windows et Linux, et la comparaison au fichier attendu versionne
// cesserait d'etre une comparaison d'OCTETS.
void UseBinaryStdout ()
{
#ifdef _WIN32
	std::fflush (stdout);
	_setmode (_fileno (stdout), _O_BINARY);
#endif
}

// Imprime une ligne de propriete SANS ESPACE EN FIN DE LIGNE.
//
// ⚠ UN ORACLE COMPARE OCTET POUR OCTET NE TOLERE PAS D'ESPACE FINAL : la
// moitie des editeurs les suppriment a l'enregistrement, et le fichier attendu
// cesserait alors de correspondre pour une raison sans rapport avec ce qu'il
// mesure. Les valeurs vides -- il y en a neuf quand aucun fichier n'est
// charge -- sont donc imprimees sans le separateur.
void PrintInfoProperty (const core::InfoRow& row)
{
	if (row.value.empty ())
		std::printf ("%-18s :\n", row.label.c_str ());
	else
		std::printf ("%-18s : %s\n", row.label.c_str (), row.value.c_str ());
}

// Une ligne d'arbre : sa profondeur, sa pastille et son texte. La pastille est
// imprimee parce qu'elle est une DONNEE de la ligne, au meme titre que son
// texte : ne pas la sortir laisserait la seule partie du panneau qu'aucun
// oracle ne couvre.
void PrintInfoNode (const core::InfoRow& row)
{
	const bool hasSwatch = row.swatch != "transparent";
	std::printf ("d%d %-7s %s\n", row.depth, hasSwatch ? row.swatch.c_str () : "-",
	             row.label.c_str ());
}

} // namespace

// ---------------------------------------------------------------------------
//  --dump-tree et --dump-files
// ---------------------------------------------------------------------------

int DumpTree (const std::string& directory)
{
	UseBinaryStdout ();

	core::DirectoryTree tree;
	if (!tree.SetRoot (directory))
	{
		std::fprintf (stderr, "peles : --dump-tree : %s.\n", tree.LastError ().c_str ());
		return 2;
	}

	// OUVERTURE PAR Toggle ET PAR RIEN D'AUTRE : le vidage mesure le chemin reel
	// de l'interface, pas un parcours ecrit pour lui. Ouvrir les noeuds
	// jusqu'a kDumpTreeDepth - 1 produit des lignes jusqu'a kDumpTreeDepth.
	tree.ExpandToDepth (kDumpTreeDepth - 1);

	const std::vector<core::TreeRow>& rows = tree.Rows ();
	std::printf ("--- arbre de repertoires ---\n");
	std::printf ("racine             : %s\n", rows.empty () ? "" : rows.front ().label.c_str ());
	std::printf ("profondeur ouverte : %d\n", kDumpTreeDepth);
	std::printf ("lignes             : %d\n", (int)rows.size ());

	for (const core::TreeRow& row : rows)
	{
		// [-] ouvert, [+] ferme, [!] acces refuse, blanc pour une ligne de
		// debordement, qui ne designe aucun noeud.
		const char* marker = row.error ? "[!]" : (row.id < 0 ? "   " : (row.expanded ? "[-]" : "[+]"));
		std::printf ("d%d %s %s\n", row.depth, marker, row.label.c_str ());
	}

	return 0;
}

int DumpFiles (const std::string& directory)
{
	UseBinaryStdout ();

	core::FileList files;
	const bool     populated = files.Populate (directory);
	if (!populated && files.Rows ().empty ())
	{
		std::fprintf (stderr, "peles : --dump-files : %s.\n", files.LastError ().c_str ());
		return 2;
	}

	std::printf ("--- liste de fichiers ---\n");
	std::printf ("repertoire    : %s\n", core::DirectoryLabel (directory).c_str ());
	std::printf ("fichiers      : %d\n", (int)files.Rows ().size ());
	std::printf ("non affiches  : %d\n", files.HiddenCount ());

	for (const core::FileRow& row : files.Rows ())
		std::printf ("%-12s %s\n", row.kind.c_str (), row.label.c_str ());

	// Une lecture interrompue en cours de route rend une liste PARTIELLE : elle
	// est imprimee -- ce qui a ete lu est vrai -- mais le code de sortie et le
	// message disent qu'elle est incomplete.
	if (!populated)
	{
		std::fprintf (stderr, "peles : --dump-files : %s.\n", files.LastError ().c_str ());
		return 2;
	}
	return 0;
}

// ---------------------------------------------------------------------------
//  --dump-info
// ---------------------------------------------------------------------------

int DumpInfo (const std::string& path)
{
	UseBinaryStdout ();

	// LE DOCUMENT EST LOCAL, ET IL N'A PAS D'OBSERVATEUR. Aucun maillage n'est
	// enregistre aupres d'un moteur de rendu ici : la destruction annoncee
	// n'atteint personne, et aucun contexte GL n'est requis.
	core::Document document;

	std::string          error;
	::Model*             model = nullptr;
	std::vector<core::InfoRow> properties;
	std::vector<core::InfoRow> meshes;
	std::vector<core::InfoRow> materials;
	int                        materialCount = 0;

	if (!path.empty ())
	{
		const core::LoadResult loaded = core::LoadModel (document, path);
		model                         = loaded.model;
		error                         = loaded.error;
	}

	if (model != nullptr)
	{
		const core::TopologyStats topology = core::ComputeTopologyStats (*model);
		properties                         = core::BuildModelInfo (*model, topology);
		meshes                             = core::BuildMeshTree (*model);
		materials                          = core::BuildMaterialTree (*model);
		materialCount                      = core::CountMaterials (*model);
	}
	else
	{
		properties = core::BuildEmptyModelInfo ();
	}

	std::printf ("--- informations du modele ---\n");
	std::printf ("proprietes        : %d\n", (int)properties.size ());
	std::printf ("lignes maillages  : %d\n", (int)meshes.size ());
	std::printf ("lignes materiaux  : %d\n", (int)materials.size ());
	std::printf ("materiaux         : %d\n", materialCount);

	std::printf ("--- proprietes ---\n");
	for (const core::InfoRow& row : properties)
		PrintInfoProperty (row);

	std::printf ("--- maillages ---\n");
	for (const core::InfoRow& row : meshes)
		PrintInfoNode (row);

	std::printf ("--- materiaux ---\n");
	for (const core::InfoRow& row : materials)
		PrintInfoNode (row);

	// ⚠ LES QUATORZE LIGNES SONT IMPRIMEES AVANT CE TEST, ET C'EST VOULU : le
	// critere porte sur leur presence MEME sans modele. Le code de sortie, lui,
	// distingue « aucun fichier demande » de « fichier demande et illisible ».
	if (!path.empty () && model == nullptr)
	{
		std::fprintf (stderr, "peles : --dump-info : %s : %s.\n", path.c_str (), error.c_str ());
		return 2;
	}
	return 0;
}

} // namespace app
} // namespace peles
