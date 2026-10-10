// ---------------------------------------------------------------------------
//  LES FONCTIONS PURES DE peles_core
// ---------------------------------------------------------------------------
//
// ⚠ CE FICHIER N'EXIGE NI wxWidgets NI CONTEXTE GL, et c'est sa raison d'etre :
// tu_supported_formats.cpp, lui, inclut sinaia/SupportedFormats.h, donc il ne
// compile que sous ENABLE_SINAIA=ON -- configuration qu'aucune CI ne reunit.
// Ce fichier ne demande que peles_core, donc ENABLE_PELES seul.
//
// ⚠ IL EXERCE CE QUE LA FIXTURE NE PEUT PAS EXERCER. Le departage par les
// octets bruts de NameLess ne se declenche que sur deux noms qui ne different
// que par la casse -- or NTFS INTERDIT que deux entrees d'un meme repertoire ne
// different que par la casse : la fixture ne peut PAS contenir ce cas, et aucun
// vidage ne l'atteindra jamais. Seul un appel direct le couvre.

#include <gtest/gtest.h>

#ifdef CG_HAS_PELES_CORE

	#include "directory_tree.h"
	#include "document.h"
	#include "file_catalog.h"
	#include "loader.h"
	#include "model_info.h"
	#include "path_text.h"
	#include "splitter_bounds.h"
	#include "topology_stats.h"

	#include "material.h"
	#include "material_pbr.h"
	#include "mesh.h"
	#include "mesh_data_manager.h"
	#include "vmeshes_io.h"
	#include "vmodels.h"

	#include <algorithm>
	#include <filesystem>
	#include <iterator>
	#include <limits>
	#include <string>
	#include <vector>

using peles::core::BuildEmptyModelInfo;
using peles::core::BuildMaterialTree;
using peles::core::BuildMeshTree;
using peles::core::BuildModelInfo;
using peles::core::ComputeTopologyStats;
using peles::core::CountMaterials;
using peles::core::DirectoryLabel;
using peles::core::InfoRow;
using peles::core::TopologyStats;
using peles::core::IsSupportedExtension;
using peles::core::KindClassForExtension;
using peles::core::NameLess;
using peles::core::PathToUtf8;
using peles::core::ToLowerAscii;

// ---------------------------------------------------------------------------
//  NameLess -- ordre TOTAL, deterministe
// ---------------------------------------------------------------------------

TEST (PelesCoreNameLess, InsensibleALaCasseAsciiPourDesNomsDifferents)
{
	EXPECT_TRUE (NameLess ("alpha", "Beta"));
	EXPECT_FALSE (NameLess ("Beta", "alpha"));

	// Le souligne (0x5F) precede les lettres minuscules en ASCII : c'est ce qui
	// fait diverger cet ordre de celui de NTFS, et c'est cette divergence qui
	// rend l'oracle des vidages falsifiable.
	EXPECT_TRUE (NameLess ("_under", "alpha"));
	EXPECT_FALSE (NameLess ("alpha", "_under"));
}

// LA BRANCHE DE DEPARTAGE, QU'AUCUNE FIXTURE NE PEUT ATTEINDRE.
//
// ⚠ SANS ELLE, "MODEL.OBJ" ET "model.obj" SERAIENT EQUIVALENTS, et leur ordre
// relatif dependrait de l'algorithme de tri -- donc de l'implementation de la
// bibliotheque standard, donc de la plateforme. L'oracle cesserait d'etre un
// oracle sans que rien ne le signale.
TEST (PelesCoreNameLess, DepartageParLesOctetsBrutsQuandLaCasseSeuleDiffere)
{
	// 'M' (0x4D) precede 'm' (0x6D).
	EXPECT_TRUE (NameLess ("MODEL.OBJ", "model.obj"));
	EXPECT_FALSE (NameLess ("model.obj", "MODEL.OBJ"));
}

// IRREFLEXIVITE : `NameLess(x, x)` est faux pour tout x. C'est la condition
// qu'un comparateur doit remplir pour etre un « strict weak ordering » ; un
// comparateur qui la viole fait sortir std::sort des bornes du tableau, ce que
// les iterateurs debogues de MSVC signalent par une assertion et que Release
// n'attrape pas du tout.
TEST (PelesCoreNameLess, EstIrreflexif)
{
	EXPECT_FALSE (NameLess ("", ""));
	EXPECT_FALSE (NameLess ("model.obj", "model.obj"));
	EXPECT_FALSE (NameLess ("MODEL.OBJ", "MODEL.OBJ"));
	EXPECT_FALSE (NameLess ("_under", "_under"));
}

// ANTISYMETRIE sur le cas qui compte : deux noms distincts ne peuvent pas etre
// chacun inferieur a l'autre.
TEST (PelesCoreNameLess, EstAntisymetrique)
{
	const char* const noms[] = { "", "_under", "alpha", "Beta", "MODEL.OBJ", "model.obj", "z" };
	for (const char* left : noms)
	{
		for (const char* right : noms)
		{
			if (NameLess (left, right))
				EXPECT_FALSE (NameLess (right, left)) << left << " / " << right;
		}
	}
}

TEST (PelesCoreToLowerAscii, NeToucheQueLASCII)
{
	EXPECT_EQ (ToLowerAscii ("MODEL.OBJ"), "model.obj");
	EXPECT_EQ (ToLowerAscii ("model.obj"), "model.obj");

	// Les octets hors ASCII sont laisses TELS QUELS : ils ne sont pas convertis
	// a l'aveugle, ils sont ordonnes par leur valeur. Une conversion dependante
	// de la locale ferait dependre l'ordre des lignes de l'environnement
	// d'execution.
	const std::string accentue = "\xC3\x89TE"; // "ÉTE" en UTF-8
	EXPECT_EQ (ToLowerAscii (accentue), "\xC3\x89te");
}

// ---------------------------------------------------------------------------
//  DirectoryLabel -- un separateur final ne change pas le libelle
// ---------------------------------------------------------------------------

TEST (PelesCoreDirectoryLabel, UnSeparateurFinalNeChangeRien)
{
	EXPECT_EQ (DirectoryLabel ("a/b/"), DirectoryLabel ("a/b"));
	EXPECT_EQ (DirectoryLabel ("a/b"), "b");
	EXPECT_EQ (DirectoryLabel ("a/b/"), "b");

	#ifdef _WIN32
	EXPECT_EQ (DirectoryLabel ("a\\b\\"), DirectoryLabel ("a\\b"));
	#endif
}

TEST (PelesCoreDirectoryLabel, UnCheminSansNomDeFichierRendLeCheminEntier)
{
	// "C:\" et "/" n'ont pas de nom de fichier : le libelle est alors le chemin.
	EXPECT_FALSE (DirectoryLabel ("/").empty ());
	#ifdef _WIN32
	EXPECT_FALSE (DirectoryLabel ("C:\\").empty ());
	#endif
}

// ---------------------------------------------------------------------------
//  KindClassForExtension -- le catalogue EST le filtre
// ---------------------------------------------------------------------------

TEST (PelesCoreKindClass, LePointDeTeteEtLaCasseSontIndifferents)
{
	EXPECT_EQ (KindClassForExtension (".OBJ"), KindClassForExtension ("obj"));
	EXPECT_EQ (KindClassForExtension (".obj"), KindClassForExtension ("OBJ"));
	EXPECT_EQ (KindClassForExtension ("obj"), "kind-obj");
	EXPECT_EQ (KindClassForExtension (".stl"), "kind-stl");
	EXPECT_EQ (KindClassForExtension ("off"), "kind-generic");
}

// ⚠ LA CHAINE VIDE EST LE FILTRE, pas une valeur par defaut. Une extension
// absente du catalogue ne s'affiche pas « avec l'icone generique » : elle ne
// s'affiche pas du tout. `mtl` est le cas qui compte -- il accompagne chaque
// `obj` sans etre importable.
TEST (PelesCoreKindClass, UneExtensionHorsCatalogueRendUneChaineVide)
{
	EXPECT_EQ (KindClassForExtension ("mtl"), "");
	EXPECT_EQ (KindClassForExtension (".mtl"), "");
	EXPECT_EQ (KindClassForExtension ("txt"), "");
	EXPECT_EQ (KindClassForExtension (""), "");
	EXPECT_EQ (KindClassForExtension ("."), "");

	EXPECT_FALSE (IsSupportedExtension ("mtl"));
	EXPECT_TRUE (IsSupportedExtension (".OBJ"));
}

// ---------------------------------------------------------------------------
//  PathToUtf8 -- l'identite d'un cote, l'affichage de l'autre
// ---------------------------------------------------------------------------

TEST (PelesCorePathToUtf8, RendLesMemesOctetsQuePathPourDeLASCII)
{
	// Sur de l'ASCII pur, l'UTF-8 et la page de codes coincident : c'est ce qui
	// rend les oracles des vidages comparables entre Windows et Linux.
	EXPECT_EQ (PathToUtf8 (std::filesystem::path ("fixture")), "fixture");
	EXPECT_EQ (PathToUtf8 (std::filesystem::path ("a/b/cube.obj")),
	           std::filesystem::path ("a/b/cube.obj").string ());
}

TEST (PelesCorePathToUtf8, NeLevePasSurUnCheminVide)
{
	EXPECT_EQ (PathToUtf8 (std::filesystem::path ()), "");
}

// ---------------------------------------------------------------------------
//  model_info -- LES QUATORZE LIGNES, ET LEUR ORDRE
// ---------------------------------------------------------------------------
//
// ⚠ L'ORDRE EST CELUI DE L'HOTE HISTORIQUE (sinaia/SinaiaFrame.cpp:2187-2201),
// SUIVI DE « Path » -- la ligne qui remplace son infobulle, RmlUi n'en ayant
// pas. Ce n'est pas une mise en forme : c'est le contrat que --dump-info
// imprime et que l'oracle versionne compare.

namespace
{

const char* const kExpectedInfoLabels[] = { "File",
	                                        "Vertices",
	                                        "Faces",
	                                        "Meshes",
	                                        "Triangle mesh",
	                                        "Borders",
	                                        "Non manifold edges",
	                                        "Bounding box X",
	                                        "Bounding box Y",
	                                        "Bounding box Z",
	                                        "Size X",
	                                        "Size Y",
	                                        "Size Z",
	                                        "Path" };

std::string ValueOf (const std::vector<InfoRow>& rows, const char* label)
{
	for (const InfoRow& row : rows)
	{
		if (row.label == label)
			return row.value;
	}
	return "<absente>";
}

std::string SwatchOf (const std::vector<InfoRow>& rows, const std::string& label)
{
	for (const InfoRow& row : rows)
	{
		if (row.label == label)
			return row.swatch;
	}
	return "<absente>";
}

bool HasNode (const std::vector<InfoRow>& rows, const std::string& label)
{
	return std::any_of (rows.begin (), rows.end (),
	                    [&] (const InfoRow& row) { return row.label == label; });
}

// LA FIXTURE, LUE DIRECTEMENT PAR cgmesh. Le repertoire de travail des tests
// contient test/data, recopie par le POST_BUILD de la cible.
const char* const kPanelsPath = "./test/data/peles/info/panels.obj";

} // namespace

// ---------------------------------------------------------------------------
//  LE CACHE GLOBAL DE cgmesh EST PURGE ENTRE DEUX TESTS
// ---------------------------------------------------------------------------
//
// ⚠ CES TESTS POSSEDENT LEURS MAILLAGES PAR UN VModels LOCAL, PAS PAR UN
// core::Document : leur destruction n'annonce RIEN, donc MeshDataManager --
// cache global indexe par `Mesh*` -- garde les cles des maillages morts. C'est
// exactement le mode de panne que `debt_cgmesh.md` documente, et que
// `Document::AnnounceRelease` ferme cote produit : le reproduire dans le
// fichier de test qui l'etablit laisserait le binaire TU accumuler des cles
// pendantes, et une adresse reattribuee heriterait de la topologie de son
// predecesseur -- d'un test a l'autre.
//
// ⚠ TearDown ET NON SetUp : une purge d'entree laisserait le dernier test
// polluer ce qui suit dans le binaire, y compris ce qui n'est pas dans ce
// fichier.
class PelesCoreModelInfo : public ::testing::Test
{
protected:
	void TearDown () override { MeshDataManager::GetInstance ().Clear (); }
};

TEST_F (PelesCoreModelInfo, UnModeleAbsentRendQuatorzeLignesDansLOrdre)
{
	const std::vector<InfoRow> rows = BuildEmptyModelInfo ();

	// ⚠ LE NOMBRE ET L'ORDRE SONT VERIFIES ENSEMBLE. Le nombre seul passerait
	// sur une permutation ; l'ordre seul passerait sur une ligne retiree en fin
	// de liste.
	ASSERT_EQ (rows.size (), std::size (kExpectedInfoLabels));
	for (std::size_t i = 0; i < rows.size (); ++i)
		EXPECT_EQ (rows[i].label, kExpectedInfoLabels[i]) << "rang " << i;
}

// ⚠ UN VECTEUR VIDE N'EST PAS UN MODELE ABSENT. Le panneau se reduirait et la
// disposition sauterait a chaque deselection : les quatorze lignes restent, ce
// sont leurs VALEURS qui s'effacent.
TEST_F (PelesCoreModelInfo, UnModeleAbsentEffaceLesValeursSansEffacerLesLignes)
{
	const std::vector<InfoRow> rows = BuildEmptyModelInfo ();

	EXPECT_EQ (ValueOf (rows, "File"), "");
	EXPECT_EQ (ValueOf (rows, "Path"), "");
	EXPECT_EQ (ValueOf (rows, "Triangle mesh"), "");
	EXPECT_EQ (ValueOf (rows, "Bounding box X"), "");
	EXPECT_EQ (ValueOf (rows, "Size Z"), "");

	// Les compteurs valent ZERO et non la chaine vide : c'est ce que fait
	// l'hote historique, qui leur passe wxVariant(0).
	EXPECT_EQ (ValueOf (rows, "Vertices"), "0");
	EXPECT_EQ (ValueOf (rows, "Borders"), "0");
	EXPECT_EQ (ValueOf (rows, "Non manifold edges"), "0");
}

// ⚠ LE CHAMP swatch N'EST JAMAIS VIDE, SUR AUCUNE LIGNE. Il est consomme par
// `data-style-background-color`, et Element::SetProperty journalise en
// LT_WARNING toute valeur qu'il ne sait pas analyser -- une chaine vide en est
// une, et l'hote compte ces messages.
TEST_F (PelesCoreModelInfo, AucuneLigneNePorteUnePastilleVide)
{
	VModels scene;
	Model*  model = scene.Add ("panels.obj");
	ASSERT_NE (model, nullptr);
	ASSERT_TRUE (VMeshesIO::load (model->m_meshes, kPanelsPath));

	const TopologyStats topology = ComputeTopologyStats (*model);

	for (const InfoRow& row : BuildModelInfo (*model, topology))
		EXPECT_FALSE (row.swatch.empty ()) << row.label;
	for (const InfoRow& row : BuildMeshTree (*model))
		EXPECT_FALSE (row.swatch.empty ()) << row.label;
	for (const InfoRow& row : BuildMaterialTree (*model))
		EXPECT_FALSE (row.swatch.empty ()) << row.label;
	for (const InfoRow& row : BuildEmptyModelInfo ())
		EXPECT_FALSE (row.swatch.empty ()) << row.label;
}

// ---------------------------------------------------------------------------
//  LES VALEURS, CONTRE DEUX NOMBRES CALCULES A LA MAIN
// ---------------------------------------------------------------------------
//
// ⚠ CE TEST NE COMPARE PAS A sinaia, ET IL NE PRETEND PAS LE FAIRE.
//
// ⚠ ET IL NE RECOUPE PAS `ComputeTopologyStats` PAR UNE SOURCE INDEPENDANTE
// NON PLUS. `ComputeTopologyStats` appelle `MeshDataManager::GetTopologicIssues`,
// qui appelle `Mesh::GetTopologicIssues` -- c'est-a-dire la MEME fonction que
// la boucle ci-dessous. Ce que la comparaison etablit se reduit a la division
// par deux et a la somme sur les maillages. De meme, les onze comparaisons de
// valeurs qui suivent opposent `Text ("%u", x)` a `std::to_string (x)` : elles
// verifient la mise en forme, pas la donnee.
//
// ⚠ CE QUI PORTE REELLEMENT L'INDEPENDANCE, CE SONT LES DEUX NOMBRES ECRITS EN
// LITTERAL : `Borders = 9` et `Non manifold edges = 1` sont CALCULABLES A LA
// MAIN sur la fixture, et c'est pour cela qu'elle a ete construite ainsi. Le
// second garant est `expected_dump_info.txt`, dont chaque valeur est
// recalculable a partir du fichier de fixture.
TEST_F (PelesCoreModelInfo, LesValeursSuiventUneSecondeLectureDeLAPI)
{
	VModels scene;
	Model*  model = scene.Add ("panels.obj");
	ASSERT_NE (model, nullptr);
	ASSERT_TRUE (VMeshesIO::load (model->m_meshes, kPanelsPath));

	unsigned int borders     = 0;
	unsigned int nonManifold = 0;
	for (Mesh* mesh : model->m_meshes.GetMeshes ())
	{
		std::vector<unsigned int> issuesNonManifold;
		std::vector<unsigned int> issuesBorders;
		mesh->GetTopologicIssues (issuesNonManifold, issuesBorders);
		borders += (unsigned int)issuesBorders.size () / 2;
		nonManifold += (unsigned int)issuesNonManifold.size () / 2;
	}

	// ⚠ LA FIXTURE EST CONSTRUITE POUR QUE CES DEUX NOMBRES SOIENT CALCULABLES A
	// LA MAIN : le tetraedre ferme n'a aucun bord, le triangle isole en a trois,
	// et les trois triangles qui partagent une arete en ont six de plus, pour
	// une seule arete non manifold. Sans ce calcul a la main, le test ne ferait
	// que comparer une implementation a elle-meme.
	EXPECT_EQ (borders, 9u);
	EXPECT_EQ (nonManifold, 1u);

	const TopologyStats topology = ComputeTopologyStats (*model);
	EXPECT_EQ (topology.borders, borders);
	EXPECT_EQ (topology.nonManifoldEdges, nonManifold);

	const std::vector<InfoRow> rows = BuildModelInfo (*model, topology);

	EXPECT_EQ (ValueOf (rows, "File"), "panels.obj");
	EXPECT_EQ (ValueOf (rows, "Vertices"), std::to_string (model->m_meshes.GetNVertices ()));
	EXPECT_EQ (ValueOf (rows, "Faces"), std::to_string (model->m_meshes.GetNFaces ()));
	EXPECT_EQ (ValueOf (rows, "Meshes"), std::to_string (model->m_meshes.GetNMeshes ()));
	EXPECT_EQ (ValueOf (rows, "Triangle mesh"),
	           model->m_meshes.IsTriangleMesh () ? "True" : "False");
	EXPECT_EQ (ValueOf (rows, "Borders"), std::to_string (borders));
	EXPECT_EQ (ValueOf (rows, "Non manifold edges"), std::to_string (nonManifold));

	float minimum[3] = { 0.f, 0.f, 0.f };
	float maximum[3] = { 0.f, 0.f, 0.f };
	ASSERT_TRUE (model->ComputeBBox ().GetMinMax (minimum, maximum));
	EXPECT_FLOAT_EQ (maximum[0] - minimum[0], 5.f);
	EXPECT_EQ (ValueOf (rows, "Bounding box X"), "[ 0 , 5 ]");
	EXPECT_EQ (ValueOf (rows, "Bounding box Y"), "[ -1 , 1 ]");
	EXPECT_EQ (ValueOf (rows, "Size X"), "5");
	EXPECT_EQ (ValueOf (rows, "Size Y"), "2");
}

// ⚠ « %.4g » N'EST PAS UN DETAIL DE PRESENTATION : c'est ce qui rend la
// comparaison avec l'hote historique TEXTUELLE au lieu d'etre visuelle. Quatre
// chiffres significatifs, et la notation exponentielle au-dela -- un « %g » nu
// en donnerait six, un « %f » en donnerait six APRES la virgule.
TEST_F (PelesCoreModelInfo, LeFormatDesNombresEstCeluiDeLHoteHistorique)
{
	VModels scene;
	Model*  model = scene.Add ("grand.obj");
	ASSERT_NE (model, nullptr);

	const float vertices[9] = { 0.f, 0.f, 0.f, 11570.f, 0.f, 0.f, 0.f, 0.123456f, 0.f };
	Mesh*       mesh        = new Mesh ();
	mesh->SetVertices (3, vertices);
	model->m_meshes.AddMesh (mesh);

	const std::vector<InfoRow> rows = BuildModelInfo (*model, TopologyStats ());

	EXPECT_EQ (ValueOf (rows, "Bounding box X"), "[ 0 , 1.157e+04 ]");
	EXPECT_EQ (ValueOf (rows, "Size X"), "1.157e+04");
	EXPECT_EQ (ValueOf (rows, "Size Y"), "0.1235");
}

// ---------------------------------------------------------------------------
//  LES QUATRE BRANCHES DE TYPE DE MATERIAU
// ---------------------------------------------------------------------------
//
// ⚠ AUCUNE FIXTURE DU DEPOT NE PORTE LES QUATRE. Un MTL ne produit que
// MATERIAL_COLOR_ADV, un glTF que MATERIAL_PBR : MATERIAL_COLOR et
// MATERIAL_TEXTURE n'apparaissent dans aucun fichier versionne, et une branche
// non exercee est une branche dont personne ne sait si elle rend quoi que ce
// soit. Seule une construction en memoire les atteint toutes.
TEST_F (PelesCoreModelInfo, LesQuatreBranchesDeTypeSontCouvertes)
{
	VModels scene;
	Model*  model = scene.Add ("materiaux");
	ASSERT_NE (model, nullptr);

	Mesh* mesh = new Mesh ();
	mesh->SetName ("mesh");

	MaterialColor* plain = new MaterialColor (255, 128, 0);
	plain->SetName ("plain");
	mesh->Material_Add (plain);

	MaterialColorExt* extended = new MaterialColorExt ();
	extended->SetName ("extended");
	extended->SetDiffuse (0.f, 0.5f, 1.f, 1.f);
	extended->SetAmbient (0.f, 0.f, 0.f, 1.f);
	mesh->Material_Add (extended);

	MaterialTexture* textured = new MaterialTexture ("absente.png");
	textured->SetName ("textured");
	mesh->Material_Add (textured);

	MaterialPbr* pbr = new MaterialPbr ();
	pbr->SetName ("pbr");
	pbr->EditFactors ().baseColor[0] = 1.f;
	pbr->EditFactors ().baseColor[1] = 0.f;
	pbr->EditFactors ().baseColor[2] = 0.f;
	pbr->EditFactors ().metallic     = 0.25f;
	pbr->EditFactors ().roughness    = 0.75f;
	mesh->Material_Add (pbr);

	model->m_meshes.AddMesh (mesh);
	EXPECT_EQ (CountMaterials (*model), 4);

	const std::vector<InfoRow> rows = BuildMaterialTree (*model);

	EXPECT_TRUE (HasNode (rows, "Materials"));
	EXPECT_TRUE (HasNode (rows, "plain"));
	EXPECT_TRUE (HasNode (rows, "extended"));
	EXPECT_TRUE (HasNode (rows, "textured"));
	EXPECT_TRUE (HasNode (rows, "pbr"));

	// MATERIAL_COLOR : une seule ligne de detail, et sa pastille.
	EXPECT_EQ (SwatchOf (rows, "Color: RGB(1.00, 0.50, 0.00)"), "#ff8000");

	// MATERIAL_COLOR_ADV.
	EXPECT_TRUE (HasNode (rows, "Type: Advanced Color"));
	EXPECT_EQ (SwatchOf (rows, "Diffuse: RGB(0.00, 0.50, 1.00)"), "#0080ff");

	// MATERIAL_TEXTURE : l'ETAT DU DECODAGE est une ligne. Un nom de fichier
	// present sans image veut dire introuvable ou non gere ; sans cette ligne,
	// le panneau laisserait croire que la texture est en place.
	EXPECT_TRUE (HasNode (rows, "Texture: absente.png"));
	EXPECT_TRUE (HasNode (rows, "Size: (image non chargee)"));
	EXPECT_TRUE (HasNode (rows, "Shininess: 0.000  (GL 0)"));

	// MATERIAL_PBR.
	EXPECT_TRUE (HasNode (rows, "Type: PBR (metallic-roughness)"));
	EXPECT_EQ (SwatchOf (rows, "Base color: RGBA(1.00, 0.00, 0.00, 1.00)"), "#ff0000");
	EXPECT_TRUE (HasNode (rows, "Metallic: 0.250   Roughness: 0.750"));
}

// ⚠ UNE PASTILLE EST BORNEE, et la borne n'est pas defensive : un MTL mal forme
// porte ce qu'il veut, et une composante hors de 0..1 produirait une chaine que
// RCSS refuse -- donc un LT_WARNING a chaque image, sur un document par
// ailleurs valide.
TEST_F (PelesCoreModelInfo, UneComposanteHorsBornesNeProduitPasUneCouleurInvalide)
{
	VModels scene;
	Model*  model = scene.Add ("hors-bornes");
	ASSERT_NE (model, nullptr);

	Mesh*             mesh     = new Mesh ();
	MaterialColorExt* extended = new MaterialColorExt ();
	extended->SetName ("hors");
	extended->SetDiffuse (-3.f, 42.f, 0.5f, 1.f);
	extended->SetAmbient (0.f, 0.f, 0.f, 1.f);
	mesh->Material_Add (extended);
	model->m_meshes.AddMesh (mesh);

	bool sawSwatch = false;
	for (const InfoRow& row : BuildMaterialTree (*model))
	{
		if (row.swatch == "transparent")
			continue;
		sawSwatch = true;
		ASSERT_EQ (row.swatch.size (), 7u) << row.label;
		EXPECT_EQ (row.swatch[0], '#');
		EXPECT_TRUE (row.swatch.find_first_not_of ("0123456789abcdef", 1) == std::string::npos)
		    << row.swatch;
	}
	EXPECT_TRUE (sawSwatch);
	EXPECT_EQ (SwatchOf (BuildMaterialTree (*model), "Diffuse: RGB(-3.00, 42.00, 0.50)"),
	           "#00ff80");
}

// ---------------------------------------------------------------------------
//  L'arbre des maillages
// ---------------------------------------------------------------------------

TEST_F (PelesCoreModelInfo, LArbreDesMaillagesPorteUneRacineEtDeuxFeuillesParMaillage)
{
	VModels scene;
	Model*  model = scene.Add ("panels.obj");
	ASSERT_NE (model, nullptr);
	ASSERT_TRUE (VMeshesIO::load (model->m_meshes, kPanelsPath));

	const std::vector<InfoRow> rows = BuildMeshTree (*model);

	// Racine + 3 x (1 noeud + 2 feuilles).
	ASSERT_EQ (rows.size (), 1u + 3u * 3u);
	EXPECT_EQ (rows.front ().label, "Meshes");
	EXPECT_EQ (rows.front ().depth, 0);
	EXPECT_EQ (rows.front ().styleClass, "d0");

	EXPECT_TRUE (HasNode (rows, "closed"));
	EXPECT_TRUE (HasNode (rows, "4 vertices"));
	EXPECT_TRUE (HasNode (rows, "4 faces"));
	EXPECT_TRUE (HasNode (rows, "open"));
	EXPECT_TRUE (HasNode (rows, "3 vertices"));
	EXPECT_TRUE (HasNode (rows, "1 faces"));
}

// ---------------------------------------------------------------------------
//  LE PLAFOND DE LIGNES, ET CE QU'IL DIT QUAND IL EST ATTEINT
// ---------------------------------------------------------------------------
//
// ⚠ AUCUNE FIXTURE DU DEPOT N'ATTEINT kMaxInfoRows, et le chemin de troncature
// n'etait donc exerce par rien -- alors que la garde du critere 4
// (« ACCUMULATION NON CONCLUANTE ») en est l'unique lecteur. Ces deux tests
// construisent le modele EN MEMOIRE, seule facon d'atteindre le plafond sans
// versionner un fichier de plusieurs milliers de lignes.
//
// ⚠ ET CE QUI EST VERIFIE N'EST PAS SEULEMENT « une ligne de debordement est
// la » : c'est le NOMBRE ANNONCE, calcule a la main a partir du plafond.

TEST_F (PelesCoreModelInfo, LArbreDesMaillagesAnnonceSaTroncature)
{
	VModels scene;
	Model*  model = scene.Add ("beaucoup-de-maillages");
	ASSERT_NE (model, nullptr);

	// 1 racine + 3 lignes par maillage, pour 999 lignes de contenu -- la
	// millieme place etant reservee a la ligne de debordement. Le 333e maillage
	// (1 + 3 x 332 = 997) pose son nom et ses sommets, et perd sa ligne de
	// faces ; a partir du 334e, plus rien ne passe.
	const int kMeshes = 400;
	for (int i = 0; i < kMeshes; ++i)
	{
		Mesh* mesh = new Mesh ();
		mesh->SetName ("mesh" + std::to_string (i));
		model->m_meshes.AddMesh (mesh);
	}

	const std::vector<InfoRow> rows = BuildMeshTree (*model);

	// ⚠ LE PLAFOND BORNE LE TOTAL, LIGNE DE DEBORDEMENT COMPRISE. Une ligne
	// posee par-dessus en ferait 1001, et le plafond cesserait d'en etre un.
	ASSERT_EQ ((int)rows.size (), peles::core::kMaxInfoRows);

	EXPECT_NE (rows.back ().styleClass.find ("overflow"), std::string::npos)
	    << rows.back ().styleClass;
	EXPECT_NE (rows.back ().label.find ("67 maillage(s)"), std::string::npos)
	    << rows.back ().label;

	// LE NOMBRE ANNONCE EST CELUI DES MAILLAGES ENTIEREMENT ABSENTS : 400 - 333.
	EXPECT_FALSE (HasNode (rows, "mesh333"));
	EXPECT_TRUE (HasNode (rows, "mesh332"));
}

// ⚠ CE TEST COUVRE AUSSI LE DECALAGE D'UNE UNITE. Le 499e materiau voit sa
// ligne de NOM passer et ses details tomber : il est AFFICHE, et le compter
// parmi les absents donnerait 3 au lieu de 2.
TEST_F (PelesCoreModelInfo, LArbreDesMateriauxAnnonceSaTroncatureSansDecalage)
{
	VModels scene;
	Model*  model = scene.Add ("beaucoup-de-materiaux");
	ASSERT_NE (model, nullptr);

	Mesh* mesh = new Mesh ();
	mesh->SetName ("mesh");

	// MaterialColor produit EXACTEMENT deux lignes : son nom et sa couleur.
	// 1 racine + 1 nom de maillage + 2 x 498 = 998 ; le 499e (indice 498) pose
	// son nom en 999e ligne et perd sa couleur. Restent deux materiaux
	// entierement absents.
	const int kMaterials = 501;
	for (int i = 0; i < kMaterials; ++i)
	{
		MaterialColor* color = new MaterialColor (255, 0, 0);
		color->SetName ("m" + std::to_string (i));
		mesh->Material_Add (color);
	}
	model->m_meshes.AddMesh (mesh);
	ASSERT_EQ (CountMaterials (*model), kMaterials);

	const std::vector<InfoRow> rows = BuildMaterialTree (*model);

	ASSERT_EQ ((int)rows.size (), peles::core::kMaxInfoRows);
	EXPECT_NE (rows.back ().styleClass.find ("overflow"), std::string::npos)
	    << rows.back ().styleClass;
	EXPECT_NE (rows.back ().label.find ("2 materiau(x)"), std::string::npos)
	    << rows.back ().label;

	// Le 499e materiau est NOMME mais sans details ; les deux suivants sont
	// absents.
	EXPECT_TRUE (HasNode (rows, "m498"));
	EXPECT_FALSE (HasNode (rows, "m499"));
	EXPECT_FALSE (HasNode (rows, "m500"));
}

// ---------------------------------------------------------------------------
//  LA PASTILLE NE DEPEND PAS DE LA CIBLE
// ---------------------------------------------------------------------------
//
// ⚠ NaN, L'INFINI ET 1e30f NE SONT PAS DES CAS TORDUS : ce sont les valeurs
// qu'un MTL mal forme produit, et la conversion flottant -> entier est un
// COMPORTEMENT INDEFINI des que la valeur tronquee ne tient pas dans `int`
// ([conv.fpint]/1). x86-64 rend INT_MIN -- donc une pastille NOIRE -- et ARM64
// sature -- donc BLANCHE. Les valeurs -3 et 42 des autres tests, elles, tiennent
// dans `int` : elles n'exercent pas ce chemin.
TEST_F (PelesCoreModelInfo, UnePastilleNeDependPasDeLaCibleSurLesValeursExtremes)
{
	VModels scene;
	Model*  model = scene.Add ("extremes");
	ASSERT_NE (model, nullptr);

	const float infinity = std::numeric_limits<float>::infinity ();
	const float notANumber = std::numeric_limits<float>::quiet_NaN ();

	Mesh*             mesh     = new Mesh ();
	MaterialColorExt* extended = new MaterialColorExt ();
	extended->SetName ("extremes");
	extended->SetDiffuse (notANumber, infinity, -infinity, 1.f);
	extended->SetAmbient (1e30f, -1e30f, 0.5f, 1.f);
	mesh->Material_Add (extended);
	model->m_meshes.AddMesh (mesh);

	const std::vector<InfoRow> rows = BuildMaterialTree (*model);

	bool sawSwatch = false;
	for (const InfoRow& row : rows)
	{
		if (row.swatch == "transparent")
			continue;
		sawSwatch = true;
		ASSERT_EQ (row.swatch.size (), 7u) << row.label;
		EXPECT_EQ (row.swatch[0], '#');
		EXPECT_TRUE (row.swatch.find_first_not_of ("0123456789abcdef", 1) == std::string::npos)
		    << row.swatch;
	}
	EXPECT_TRUE (sawSwatch);

	// NaN et les negatifs rendent 00, l'infini et 1e30f rendent ff -- et ce sont
	// des valeurs, pas seulement des chaines bien formees.
	for (const InfoRow& row : rows)
	{
		if (row.label.rfind ("Diffuse:", 0) == 0)
			EXPECT_EQ (row.swatch, "#00ff00") << row.label;
		if (row.label.rfind ("Ambient:", 0) == 0)
			EXPECT_EQ (row.swatch, "#ff0080") << row.label;
	}
}

// ⚠ UN NOM PLUS LONG QUE LE TAMPON DE MISE EN FORME EST COUPE, ET LA COUPURE SE
// VOIT. Sans marqueur, un chemin de texture de 600 octets rendrait une ligne
// indiscernable d'un chemin reellement court -- dans le panneau comme dans
// l'oracle textuel.
TEST_F (PelesCoreModelInfo, UnNomPlusLongQueLeTamponEstCoupeVISIBLEMENT)
{
	VModels scene;
	Model*  model = scene.Add ("nom-long");
	ASSERT_NE (model, nullptr);

	const std::string longName = std::string (600, 'a') + ".png";

	Mesh*            mesh     = new Mesh ();
	MaterialTexture* textured = new MaterialTexture (longName.c_str ());
	textured->SetName ("longue");
	mesh->Material_Add (textured);
	model->m_meshes.AddMesh (mesh);

	bool sawTruncation = false;
	for (const InfoRow& row : BuildMaterialTree (*model))
	{
		if (row.label.rfind ("Texture: ", 0) != 0)
			continue;
		sawTruncation = true;
		EXPECT_LT (row.label.size (), 600u) << "la ligne n'a pas ete coupee";
		EXPECT_NE (row.label.find ("...(tronque)"), std::string::npos) << row.label.size ();
	}
	EXPECT_TRUE (sawTruncation);
}

// ---------------------------------------------------------------------------
//  LE CACHE GLOBAL DE cgmesh, RENDU A LA DESTRUCTION DU DOCUMENT
// ---------------------------------------------------------------------------
//
// ⚠ C'EST LE CORRECTIF DE L'ETAPE 5, ET IL N'AVAIT AUCUN TEST. `--reselect` le
// mesure, mais il exige une fenetre et un contexte GL : il n'est ni une cible
// CTest, ni exercice dans la CI. Ici le chemin est le meme -- AnnounceRelease,
// point de passage unique des trois chemins de mort d'un modele -- sans GL.
TEST_F (PelesCoreModelInfo, LaDestructionDUnModeleRendSesClesAuCacheDeCgmesh)
{
	MeshDataManager& cache = MeshDataManager::GetInstance ();
	cache.Clear ();
	ASSERT_EQ (cache.CachedEntryCount (), 0u);

	peles::core::Document document;

	const peles::core::LoadResult loaded = peles::core::LoadModel (document, kPanelsPath);
	ASSERT_NE (loaded.model, nullptr) << loaded.error;

	// C'est ComputeTopologyStats qui PEUPLE le cache : sans cet appel, le test
	// verifierait qu'un cache vide le reste.
	ComputeTopologyStats (*loaded.model);
	const std::size_t populated = cache.CachedEntryCount ();
	ASSERT_GT (populated, 0u);

	document.Clear ();
	EXPECT_EQ (cache.CachedEntryCount (), 0u)
	    << populated << " entree(s) peuplee(s), il en reste " << cache.CachedEntryCount ();
}

// ---------------------------------------------------------------------------
//  LA LIGNE DE DEBORDEMENT PAR NOEUD -- LA SEULE DES CINQ QUE LE PRODUIT
//  N'ATTEINT PAS
// ---------------------------------------------------------------------------
//
// ⚠ ELLE EST INATTEIGNABLE DANS LE BINAIRE LIVRE, et c'est MESURE :
// kMaxChildrenPerNode == kMaxVisibleRows == 1000, donc un noeud qui depasse son
// propre plafond porte au moins 1001 enfants et le TOTAL franchit le plafond
// GLOBAL d'abord -- c'est la ligne globale qui sort. `debt_peles.md` §7.
//
// ⛔ LES DEUX CHIFFRES NE SONT PAS RENDUS DISTINCTS POUR CE TEST. Ils sont
// mesures (≈ 7,7 µs/ligne) ; les changer pour rendre une ligne atteignable
// serait faire prescrire le produit par l'oracle. Le plafond est INJECTE, les
// constantes livrees ne bougent pas.
//
// ⚠ LE TEST PORTE SES DEUX VERSANTS. Le versant negatif -- plafond par defaut,
// aucune ligne de debordement -- n'est probant que parce que le versant positif
// en produit une sur la MEME fixture : un detecteur qui ne trouve rien
// n'etablit rien tant qu'il n'a pas trouve quelque chose.
class PelesCoreDirectoryTree : public ::testing::Test
{
protected:
	// Le nombre de SOUS-REPERTOIRES de la fixture, compte a l'execution : le
	// figer a quatre ferait echouer ce test le jour ou la fixture gagne un
	// repertoire, pour une raison sans rapport avec ce qu'il mesure.
	static int CountSubdirectories (const std::filesystem::path& directory)
	{
		int                   count = 0;
		std::error_code       ec;
		for (std::filesystem::directory_iterator it (directory, ec);
		     !ec && it != std::filesystem::directory_iterator (); it.increment (ec))
		{
			if (std::filesystem::is_directory (it->path (), ec) && !ec)
				++count;
		}
		return count;
	}

	static bool HasRowLabelled (const std::vector<peles::core::TreeRow>& rows,
	                            const std::string&                      label)
	{
		return std::any_of (rows.begin (), rows.end (),
		                    [&] (const peles::core::TreeRow& row) { return row.label == label; });
	}
};

TEST_F (PelesCoreDirectoryTree, UnPlafondParNoeudAbaisseRendLaLigneDeDebordement)
{
	const std::filesystem::path fixture = "./test/data/peles/fixture";
	const int                   children = CountSubdirectories (fixture);

	// Le plafond doit etre STRICTEMENT sous le nombre d'enfants, sans quoi le
	// test passerait en ne mesurant rien.
	constexpr int kInjectedCap = 3;
	ASSERT_GT (children, kInjectedCap) << "fixture " << fixture.string () << " : " << children
	                                   << " sous-repertoire(s), il en faut plus de "
	                                   << kInjectedCap;

	peles::core::DirectoryTree tree;
	tree.SetMaxChildrenPerNode (kInjectedCap);
	ASSERT_TRUE (tree.SetRoot (fixture)) << tree.LastError ();
	ASSERT_TRUE (tree.Toggle (0)) << tree.LastError ();

	const std::string expected =
	    "... " + std::to_string (children - kInjectedCap) + " entree(s) non affichee(s)";
	EXPECT_TRUE (HasRowLabelled (tree.Rows (), expected))
	    << "ligne attendue : \"" << expected << "\" ; " << tree.Rows ().size () << " ligne(s) rendues";

	// ⚠ LE PLAFOND GLOBAL N'A PAS MORDU, et c'est ce qui distingue les deux
	// lignes : c'est bien celle du plafond PAR NOEUD qui vient d'etre vue.
	EXPECT_FALSE (tree.Truncated ());
	EXPECT_EQ ((int)tree.Rows ().size (), 1 + kInjectedCap + 1);
}

TEST_F (PelesCoreDirectoryTree, AuPlafondPAR_DEFAUT_AucuneLigneDeDebordementParNoeud)
{
	const std::filesystem::path fixture  = "./test/data/peles/fixture";
	const int                   children = CountSubdirectories (fixture);
	ASSERT_GT (children, 0);

	peles::core::DirectoryTree tree;
	ASSERT_TRUE (tree.SetRoot (fixture)) << tree.LastError ();
	ASSERT_TRUE (tree.Toggle (0)) << tree.LastError ();

	for (const peles::core::TreeRow& row : tree.Rows ())
		EXPECT_EQ (row.label.rfind ("... ", 0), std::string::npos) << "ligne inattendue : "
		                                                           << row.label;
	EXPECT_EQ ((int)tree.Rows ().size (), 1 + children);
}

// UNE VALEUR NULLE OU NEGATIVE EST IGNOREE : un plafond de zero ferait de chaque
// noeud une ligne de debordement, ce qu'aucun appelant ne peut vouloir.
TEST_F (PelesCoreDirectoryTree, UnPlafondNulOuNegatifEstIgnore)
{
	const std::filesystem::path fixture  = "./test/data/peles/fixture";
	const int                   children = CountSubdirectories (fixture);

	peles::core::DirectoryTree tree;
	tree.SetMaxChildrenPerNode (0);
	tree.SetMaxChildrenPerNode (-7);
	ASSERT_TRUE (tree.SetRoot (fixture)) << tree.LastError ();
	ASSERT_TRUE (tree.Toggle (0)) << tree.LastError ();

	EXPECT_EQ ((int)tree.Rows ().size (), 1 + children);
}

// ---------------------------------------------------------------------------
//  LE CHEVRON : `expandable`, ET LA CLASSE QUI LE PORTE
// ---------------------------------------------------------------------------
//
// ⚠ LES DEUX VERSANTS SUR LA MEME FIXTURE : `alpha` a un sous-repertoire,
// `Beta`, `gamma` et `_under` n'en ont pas. Un test qui ne verrait que des
// lignes sans chevron ne distinguerait pas « regle juste » de « champ jamais
// renseigne ».
namespace
{

const peles::core::TreeRow* FindTreeRow (const std::vector<peles::core::TreeRow>& rows,
                                         const std::string&                      label)
{
	const auto it = std::find_if (rows.begin (), rows.end (),
	                              [&] (const peles::core::TreeRow& row) { return row.label == label; });
	return it == rows.end () ? nullptr : &*it;
}

bool HasClass (const peles::core::TreeRow& row, const std::string& name)
{
	const std::string padded = " " + row.styleClass + " ";
	return padded.find (" " + name + " ") != std::string::npos;
}

} // namespace

TEST_F (PelesCoreDirectoryTree, UnNoeudNonEnumerePorteUnChevron)
{
	peles::core::DirectoryTree tree;
	ASSERT_TRUE (tree.SetRoot ("./test/data/peles/fixture")) << tree.LastError ();
	ASSERT_EQ (tree.Rows ().size (), 1u);

	const peles::core::TreeRow& root = tree.Rows ().front ();
	EXPECT_TRUE (root.expandable);
	EXPECT_TRUE (HasClass (root, "expandable")) << root.styleClass;
	EXPECT_TRUE (HasClass (root, "closed")) << root.styleClass;
	EXPECT_FALSE (root.drive);
	EXPECT_FALSE (HasClass (root, "drive")) << root.styleClass;
}

TEST_F (PelesCoreDirectoryTree, UnRepertoireEnumereVidePerdSonChevron)
{
	peles::core::DirectoryTree tree;
	ASSERT_TRUE (tree.SetRoot ("./test/data/peles/fixture")) << tree.LastError ();
	tree.ExpandToDepth (2);
	const std::vector<peles::core::TreeRow>& rows = tree.Rows ();

	// Enumeres et non vides : chevron.
	for (const char* label : { "fixture", "alpha", "inner" })
	{
		const peles::core::TreeRow* row = FindTreeRow (rows, label);
		ASSERT_NE (row, nullptr) << label;
		EXPECT_TRUE (row->expandable) << label;
		EXPECT_TRUE (HasClass (*row, "expandable")) << label << " : " << row->styleClass;
		EXPECT_TRUE (HasClass (*row, "open")) << label << " : " << row->styleClass;
	}

	// Pas encore enumere (profondeur 3, au-dela de l'ouverture) : chevron.
	const peles::core::TreeRow* deeper = FindTreeRow (rows, "deeper");
	ASSERT_NE (deeper, nullptr);
	EXPECT_TRUE (deeper->expandable);
	EXPECT_TRUE (HasClass (*deeper, "closed")) << deeper->styleClass;

	// Enumeres sans sous-repertoire : ouvrables, mais sans chevron.
	for (const char* label : { "Beta", "gamma", "_under" })
	{
		const peles::core::TreeRow* row = FindTreeRow (rows, label);
		ASSERT_NE (row, nullptr) << label;
		EXPECT_TRUE (row->openable) << label;
		EXPECT_FALSE (row->expandable) << label;
		EXPECT_FALSE (HasClass (*row, "expandable")) << label << " : " << row->styleClass;
	}
}

// Le chevron d'un noeud enumere depend de ses enfants, pas de son etat : replie,
// il garde le chevron qui permet de le rouvrir.
TEST_F (PelesCoreDirectoryTree, UnNoeudEnumerePuisReplieGardeSonChevron)
{
	peles::core::DirectoryTree tree;
	ASSERT_TRUE (tree.SetRoot ("./test/data/peles/fixture")) << tree.LastError ();
	tree.ExpandToDepth (2);

	const peles::core::TreeRow* alpha = FindTreeRow (tree.Rows (), "alpha");
	ASSERT_NE (alpha, nullptr);
	ASSERT_TRUE (alpha->expanded);
	const int alphaId = alpha->id;

	ASSERT_TRUE (tree.Toggle (alphaId));

	alpha = FindTreeRow (tree.Rows (), "alpha");
	ASSERT_NE (alpha, nullptr);
	EXPECT_FALSE (alpha->expanded);
	EXPECT_TRUE (alpha->expandable);
	EXPECT_TRUE (HasClass (*alpha, "closed")) << alpha->styleClass;
	EXPECT_TRUE (HasClass (*alpha, "expandable")) << alpha->styleClass;
	EXPECT_EQ (FindTreeRow (tree.Rows (), "inner"), nullptr) << "alpha replie montre encore inner";
}

TEST_F (PelesCoreDirectoryTree, UneLigneDeDebordementNaNiChevronNiIcone)
{
	peles::core::DirectoryTree tree;
	tree.SetMaxChildrenPerNode (1);
	ASSERT_TRUE (tree.SetRoot ("./test/data/peles/fixture")) << tree.LastError ();
	ASSERT_TRUE (tree.Toggle (0)) << tree.LastError ();

	const std::vector<peles::core::TreeRow>& rows = tree.Rows ();
	ASSERT_FALSE (rows.empty ());
	const peles::core::TreeRow& overflow = rows.back ();
	ASSERT_EQ (overflow.id, -1) << overflow.label;
	EXPECT_FALSE (overflow.expandable);
	EXPECT_TRUE (HasClass (overflow, "overflow")) << overflow.styleClass;

	// Les classes qui selectionnent une image dans peles.rcss.
	for (const char* imageClass : { "expandable", "open", "closed", "drive" })
		EXPECT_FALSE (HasClass (overflow, imageClass)) << imageClass << " : " << overflow.styleClass;

	EXPECT_TRUE (rows.front ().expandable);
}

TEST_F (PelesCoreDirectoryTree, LesRacinesSystemeSontDesLecteurs)
{
	peles::core::DirectoryTree tree;
	tree.SetSystemRoots ();
	ASSERT_FALSE (tree.Rows ().empty ()) << "aucun lecteur pret sur cette machine";

	for (const peles::core::TreeRow& row : tree.Rows ())
	{
		EXPECT_TRUE (row.drive) << row.label;
		EXPECT_TRUE (HasClass (row, "drive")) << row.label << " : " << row.styleClass;
		EXPECT_TRUE (row.expandable) << row.label;
	}
}

// ---------------------------------------------------------------------------
//  Bornes des separateurs de panneaux
// ---------------------------------------------------------------------------
//
// L'etendue partagee est celle du panneau plus celle de la vue ; les valeurs
// ci-dessous sont celles du cadre 1280x800 par defaut (panneau gauche 240, vue
// 760, donc 1000 partages), puis celles d'une fenetre trop petite pour les deux
// minimums.

using peles::core::ClampPanelExtent;
using peles::core::DraggedPanelExtent;
using peles::core::kMinPanelExtent;
using peles::core::kMinViewportExtent;
using peles::core::SplitterSide;

TEST (PelesCoreSplitter, LeGlisserSuitLeSensDuPanneau)
{
	// Panneau gauche : il grandit vers la droite.
	EXPECT_EQ (DraggedPanelExtent (240, 150, SplitterSide::PanelBefore), 390);
	EXPECT_EQ (DraggedPanelExtent (240, -150, SplitterSide::PanelBefore), 90);
	// Panneaux droit et bas : ils grandissent vers la gauche et vers le haut.
	EXPECT_EQ (DraggedPanelExtent (280, -150, SplitterSide::PanelAfter), 430);
	EXPECT_EQ (DraggedPanelExtent (180, 100, SplitterSide::PanelAfter), 80);
}

TEST (PelesCoreSplitter, UneEtendueEntreLesBornesEstRendueTelleQuelle)
{
	EXPECT_EQ (ClampPanelExtent (390, 1000), 390);
	EXPECT_EQ (ClampPanelExtent (kMinPanelExtent, 1000), kMinPanelExtent);
	EXPECT_EQ (ClampPanelExtent (1000 - kMinViewportExtent, 1000), 1000 - kMinViewportExtent);
}

TEST (PelesCoreSplitter, LePanneauNeDescendPasSousSonMinimum)
{
	EXPECT_EQ (ClampPanelExtent (90, 1000), kMinPanelExtent);
	EXPECT_EQ (ClampPanelExtent (-500, 1000), kMinPanelExtent);
}

TEST (PelesCoreSplitter, LaVueGardeSonMinimum)
{
	EXPECT_EQ (ClampPanelExtent (5000, 1000), 1000 - kMinViewportExtent);
	EXPECT_EQ (ClampPanelExtent (801, 1000), 800);
}

// ⚠ LE CAS QUI DEPARTAGE LES DEUX MINIMUMS : l'etendue partagee ne suffit pas a
// les tenir ensemble. La vue passe avant -- le panneau descend sous son minimum
// plutot que de laisser la vue sans surface.
TEST (PelesCoreSplitter, QuandLesDeuxMinimumsNeTiennentPasLaVuePasseAvant)
{
	const int shared = kMinPanelExtent + kMinViewportExtent - 40;
	EXPECT_EQ (ClampPanelExtent (400, shared), shared - kMinViewportExtent);
	EXPECT_LT (ClampPanelExtent (400, shared), kMinPanelExtent);
}

TEST (PelesCoreSplitter, LEtendueNEstJamaisNegative)
{
	EXPECT_EQ (ClampPanelExtent (400, kMinViewportExtent - 1), 0);
	EXPECT_EQ (ClampPanelExtent (400, 0), 0);
	EXPECT_EQ (ClampPanelExtent (400, -100), 0);
}

// ---------------------------------------------------------------------------
//  Bornes d'un axe entier : tous ses panneaux en une passe
// ---------------------------------------------------------------------------

using peles::core::AxisPanel;
using peles::core::ClampAxisExtents;
using peles::core::SharedAxisExtent;

namespace
{

AxisPanel Adjustable (int extent, int requested)
{
	AxisPanel panel;
	panel.extent    = extent;
	panel.requested = requested;
	return panel;
}

AxisPanel Fixed (int extent)
{
	AxisPanel panel;
	panel.extent = extent;
	return panel;
}

} // namespace

TEST (PelesCoreSplitterAxis, AucunPanneauNeRendRien)
{
	EXPECT_TRUE (ClampAxisExtents ({}, 1000).empty ());
}

TEST (PelesCoreSplitterAxis, UnSeulPanneauSuitLaRegleDuPanneauSeul)
{
	for (const int shared : { 1000, 500, 330, 250, 150, 0 })
	{
		const std::vector<int> t = ClampAxisExtents ({ Adjustable (400, 400) }, shared);
		ASSERT_EQ (t.size (), 1u);
		EXPECT_EQ (t[0], ClampPanelExtent (400, shared)) << "partage " << shared;
	}
}

// Le scenario de la revue : gauche 400, droit 400. A 700 px, borner chaque
// panneau contre « lui-meme plus la vue » laissait gauche 100 (< 120) et droit
// 400. Le manque est pris au-dessus des minimums, gauche d'abord.
TEST (PelesCoreSplitterAxis, DeuxPanneauxCedentAuDessusDeLeurMinimumAvantDeDescendre)
{
	const std::vector<AxisPanel> both = { Adjustable (400, 400), Adjustable (400, 400) };
	EXPECT_EQ (ClampAxisExtents (both, 1280), (std::vector<int> { 400, 400 }));
	// Manque 40 : le gauche seul.
	EXPECT_EQ (ClampAxisExtents (both, 960), (std::vector<int> { 360, 400 }));
	// Manque 300 : le gauche a son minimum, le droit cede le reste.
	EXPECT_EQ (ClampAxisExtents (both, 700), (std::vector<int> { 120, 380 }));
	// Manque 400 : le gauche a son minimum, le droit cede 120. Manque 560 : les
	// deux exactement a leur minimum.
	EXPECT_EQ (ClampAxisExtents (both, 600), (std::vector<int> { 120, 280 }));
	EXPECT_EQ (ClampAxisExtents (both, 440), (std::vector<int> { 120, 120 }));
}

TEST (PelesCoreSplitterAxis, UnManqueSuperieurAuxMargesPasseSousLesMinimums)
{
	const std::vector<AxisPanel> both = { Adjustable (400, 400), Adjustable (400, 400) };
	// Somme des marges 560 ; manque 600 -> 40 sous le minimum du gauche.
	EXPECT_EQ (ClampAxisExtents (both, 400), (std::vector<int> { 80, 120 }));
	EXPECT_EQ (ClampAxisExtents (both, 200), (std::vector<int> { 0, 0 }));
	// Le manque qui reste est celui de la vue ; jamais une etendue negative.
	EXPECT_EQ (ClampAxisExtents (both, 50), (std::vector<int> { 0, 0 }));
}

TEST (PelesCoreSplitterAxis, UnPanneauNonReglableNeCedeRien)
{
	// Le droit est a sa taille par defaut (280) : seul le gauche cede.
	const std::vector<AxisPanel> panels = { Adjustable (400, 400), Fixed (280) };
	EXPECT_EQ (ClampAxisExtents (panels, 800), (std::vector<int> { 320, 280 }));
	EXPECT_EQ (ClampAxisExtents (panels, 500), (std::vector<int> { 20, 280 }));
	EXPECT_EQ (ClampAxisExtents (panels, 300), (std::vector<int> { 0, 280 }));
}

TEST (PelesCoreSplitterAxis, LaDemandeEstRenduQuandLaPlaceRevient)
{
	// Etendue courante reduite par une fenetre plus petite ; demande 400.
	const std::vector<AxisPanel> panels = { Adjustable (120, 400), Adjustable (380, 400) };
	EXPECT_EQ (ClampAxisExtents (panels, 1280), (std::vector<int> { 400, 400 }));
}

// La vue a 0 et la ligne qui deborde : sans la correction, « panneaux plus vue »
// surestimerait l'etendue partagee de tout l'excedent.
TEST (PelesCoreSplitterAxis, LeDebordementEstRetrancheDeLEtenduePartagee)
{
	// Ligne de 600 : gauche 400 + droit 400 + vue 0 + separateurs 0 = 800.
	EXPECT_EQ (SharedAxisExtent (800, 0, 800, 600), 600);
	// Sans debordement, panneaux plus vue.
	EXPECT_EQ (SharedAxisExtent (640, 640, 1280, 1280), 1280);
	EXPECT_EQ (SharedAxisExtent (240, 760, 1000, 1280), 1000);
}

#else // CG_HAS_PELES_CORE

// Meme regle que pour tu_supported_formats : un test qui DISPARAIT ne produit
// aucun signal, et « aucun test decouvert » ne se distingue pas de « tout
// passe ».
TEST (PelesCore, NonCompile)
{
	GTEST_SKIP () << "les fonctions pures de peles_core n'ont PAS ete exercees : ce test exige "
	                 "ENABLE_PELES=ON.";
}

#endif // CG_HAS_PELES_CORE
