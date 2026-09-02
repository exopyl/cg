#include "catalog.h"

#ifndef __EMSCRIPTEN__
// Noeuds d'ANALYSE, natifs seulement. Voir plus bas, a l'endroit de leur
// enregistrement, pourquoi et par quoi c'est tenu.
#include "mesh/ambient_occlusion.h"
#include "mesh/colormap.h"
#include "mesh/convex_hull.h"
#include "mesh/curvature.h"
#include "mesh/icp_align.h"
#include "mesh/thickness.h"
// Decomposition multi-objets : vmeshes_io.cpp n'est pas davantage dans la liste
// EMSCRIPTEN de cgmesh. Meme motif, meme double condition.
#include "mesh/load_parts.h"
#endif
#include "flow/boundary.h"
#include "flow/foreach.h"
#include "flow/repeat.h"
#include "flow/subgraph.h"
#include "img/load_image.h"
#include "io/file_ref.h"
#include "img/pixel_blocks.h"
#include "img/quantize.h"
#include "img/relief.h"
#include "mesh/load_mesh.h"
#include "mesh/save_mesh.h"
#include "mesh/simplify.h"
#include "mesh/smooth_laplacian.h"
#include "shapes/gothic_window.h"
#include "shapes/parametric_shape.h"
#include "shapes/profile.h"
#include "shapes/extrude.h"
#include "svg/svg_contours.h"
#include "text/text_contours.h"
#include "text/load_font.h"

namespace cggraph_nodes
{

namespace
{

template <class T>
std::unique_ptr<cggraph::Node> Make ()
{
	return std::unique_ptr<cggraph::Node> (new T ());
}

} // namespace

const std::vector<CatalogEntry> &Catalog ()
{
	static const std::vector<CatalogEntry> entries = [] {
	std::vector<CatalogEntry> table = {
		{ "mesh.io.load", "Maillage", "Maillage", &Make<LoadMeshNode>,
		  "MeshIO::import_obj rend 0 -- succes -- sur un nom de fichier nul "
		  "(mesh_io_obj.cpp:266) : le code de retour ne suffit pas a decider "
		  "qu'un maillage a ete lu" },
		{ "mesh.smooth.laplacian", "Lissage laplacien", "Maillage", &Make<SmoothLaplacianNode>,
		  "Apply fait UNE passe et preserve le bord sans condition : iterations "
		  "boucle dans l'adaptateur, et preserveBoundary n'est pas un choix" },
		{ "mesh.simplify", "Simplification", "Maillage", &Make<SimplifyNode>,
		  "maxError est un PROXY du cout QEM, pas une borne de Hausdorff, sauf "
		  "sous exactError ou il borne la distance des SOMMETS a la surface" },
		{ "mesh.io.save", "Enregistrement de maillage", "Maillage", &Make<SaveMeshNode>,
		  "MeshIO::save compare l'extension par strcmp sans normaliser la casse "
		  "(mesh_io.cpp:69-89), contrairement a load : un chemin en .OBJ n'est "
		  "pas reconnu" },
		// La police est un PORT, donc un noeud a part.
		{ "text.font.load", "Police", "Texte", &Make<LoadFontNode>, nullptr },
		// CONTOUR 2D -- deux producteurs, un extrudeur. `text.extrude`, qui
		// enchainait mise en page, aplatissement et extrusion en un bloc, a ete
		// RETIRE : extruder un texte et extruder un dessin refaisaient la meme
		// derniere etape, et rien ne pouvait s'intercaler entre les deux.
		{ "text.contours", "Contours de texte", "Texte", &Make<TextContoursNode>,
		  "les glyphes sont TOUJOURS fusionnes en une region unique, la ou "
		  "text.extrude ne le faisait que sur demande : une liste plate de "
		  "contours ne peut pas porter le decoupage par glyphe. Un glyphe absent "
		  "de la police occupe son avance sans emettre de contour, et un texte "
		  "entierement blanc est un echec" },
		{ "svg.contours", "Contours SVG", "Forme 2D", &Make<SvgContoursNode>,
		  "la regle de remplissage de CHAQUE forme -- even-odd ou non-zero -- est "
		  "resolue par une passe Clipper2 en amont : elle n'est connue nulle part "
		  "ailleurs et une liste plate ne la transporte pas. `height` n'est pas "
		  "un reglage de ce noeud, la profondeur se regle sur shape.extrude" },
		{ "shape.extrude", "Extrusion", "Forme 2D", &Make<ExtrudeNode>,
		  "n'ajoute PAS de plaque de support : le support est un contour de plus, "
		  "fondu par l'union 2D, donc il se decide chez le producteur de contours "
		  "-- text.contours le porte, svg.contours non" },
		// ---------------------------------------------------------------
		// IMAGE -- les deux chaines « image -> regions extrudees », celles des
		// pages « Image to puzzle » et « Blocs pixelises » de maker.
		//
		// PORTABLES, et c'est verifie et non suppose : image_relief.cpp,
		// image_pixel_blocks.cpp, image_region_pipeline.cpp et
		// image_vectorization.cpp figurent tous dans la liste EMSCRIPTEN de
		// src/cgmesh/CMakeLists.txt, et cgimg est globe en entier. Rien a
		// exclure ici, contrairement aux sept noeuds d'analyse plus bas.
		//
		// La source est un noeud SEPARE, comme la police : c'est ce qui donne a
		// l'image un port, donc une place dans la signature, donc un partage
		// entre les deux chaines d'un meme document.
		// FICHIER -- la ressource devenue un noeud, donc un port.
		//
		// Portable : il ne lit que <cstdio>. Sous WebAssembly sa lecture de fichier
		// echoue par construction (une uri y est une adresse HTTP), et c'est
		// l'HOTE qui resout l'uri puis repose les octets -- cf. l'en-tete du noeud.
		{ "file.ref", "Fichier", "Fichier", &Make<FileRefNode>,
		  "IL NE LIT RIEN -- il DESIGNE, les chargeurs en aval ouvrent. Le lien "
		  "porte un NOM et non un contenu : ce qui relie la signature de l'aval au "
		  "contenu du fichier, c'est son parametre semantique `source.identity`, "
		  "releve par un STAT. Sous WebAssembly le mtime d'un fichier MEMFS est "
		  "celui de son ECRITURE : un hote qui reecrit la meme ressource a chaque "
		  "import manquerait le cache a tous les coups" },
		{ "img.io.load", "Image", "Image", &Make<LoadImageNode>,
		  "le format est reconnu au CONTENU et non a l'extension, la couverture "
		  "est donc plus large que celle d'Img::load et ne depend pas de "
		  "CGIMG_WITH_PNG / CGIMG_WITH_JPG ; un buffer vide est un echec, pas "
		  "une image vide" },
		{ "img.quantize", "Quantification", "Image", &Make<QuantizeImageNode>,
		  "N'EST PAS ANNULABLE : quantize_image ne prend pas de contexte, et le "
		  "filtrage bilateral puis Wu tiennent le fil plusieurs secondes sur une "
		  "grande image. `algo` autre que 1 vaut Wu. `pixelWidth` est le seul "
		  "parametre qui distingue les deux chaines aval : 0 pour le relief, 64 "
		  "pour les blocs" },
		{ "img.relief", "Relief colore", "Image", &Make<ReliefNode>,
		  "attend une image DEJA QUANTIFIEE (img.quantize) et n'en verifie rien : "
		  "branche sur img.io.load brut, il sort un relief a autant de couleurs "
		  "que l'image en compte, sans erreur. Les champs de quantification "
		  "d'ImageReliefOptions ne sont pas lus par cette voie" },
		{ "img.relief.layers", "Relief : couches par couleur", "Image",
		  &Make<ReliefLayersNode>,
		  "suite DENSE : une couche qui ne tesselle rien est OMISE, donc la "
		  "position i ne designe pas la couleur i -- seul le nom de materiau "
		  "\"color_NN\" porte l'index de palette. Base et mur sont les dernieres "
		  "entrees quand ils sont demandes" },
		{ "img.pixel_blocks", "Blocs pixelises", "Image", &Make<PixelBlocksNode>,
		  "attend une image quantifiee ET PIXELISEE (img.quantize avec "
		  "pixelWidth). Sur une image non pixelisee la segmentation REUSSIT et "
		  "rend des milliers de blocs minuscules : rien n'echoue, le resultat "
		  "est seulement ininterpretable" },
		{ "img.pixel_blocks.parts", "Blocs pixelises : pieces separables", "Image",
		  &Make<PixelBlocksPartsNode>,
		  "meme piege de pixelisation que img.pixel_blocks, et il coute ici un "
		  "maillage par region ; le nom de materiau \"block_NNNN_color_NN\" est le "
		  "seul lien fiable entre la position dans la suite et le bloc d'origine" }
#ifndef __EMSCRIPTEN__
		,
		// ---------------------------------------------------------------
		// ANALYSE -- natifs seulement.
		//
		// La cible WASM ne compile qu'un SOUS-ENSEMBLE de cgmesh (la liste
		// EMSCRIPTEN de src/cgmesh/CMakeLists.txt) ; les corps emballes
		// ci-dessous n'y sont pas. Les enregistrer sans les exclure ferait
		// echouer le LIEN de maker sur des symboles introuvables -- exactement
		// la panne que l'etape 7 a payee, et qui distingue « compile » de
		// « lie ».
		//
		// La condition est portee DEUX FOIS, ici et dans le CMakeLists de cette
		// couche, qui retire les memes sources sous Emscripten. Les deux sens de
		// derive sont BRUYANTS, et c'est ce qui rend le doublon acceptable :
		//   - source retiree, enregistrement reste  -> erreur de LIEN ;
		//   - enregistrement retire, source restee  -> objet jamais tire, module
		//     inchange, et la mesure de taille de maker.wasm le montre.
		//
		// Le decoupage n'est PAS fait par repertoire : le critere d'exclusion est
		// la PORTABILITE, qui n'est pas un domaine. En faire un sous-repertoire
		// inscrirait une propriete de build dans un rangement que D14 reserve aux
		// domaines.
		{ "mesh.color.map", "Carte de couleurs", "Analyse", &Make<ColormapNode>,
		  "InitVertexColorsFromArray n'honore PAS son tableau `defined` : il "
		  "ecrit du noir puis appelle color_jet sans else et l'ecrase "
		  "(mesh.cpp:142-148). L'adaptateur neutralise les valeurs non definies "
		  "avant l'appel. Le corps ecrit aussi les UV et active leurs indices "
		  "par face : colorier change la parametrisation" },
		{ "mesh.hull.convex", "Enveloppe convexe", "Analyse", &Make<ConvexHullNode>,
		  "Chull3D::compute rend void et IGNORE l'echec de double_triangle sur "
		  "une entree colineaire, signale par un simple printf ; la mise en "
		  "forme du resultat est quadratique (get_vertex_index parcourt la "
		  "liste pour chaque coin)" },
		{ "mesh.align.icp", "Recalage ICP", "Analyse", &Make<IcpAlignNode>,
		  "le BVH construit sur la cible garde un POINTEUR NU sur ses positions "
		  "(bvh.cpp:56) ; et une entree inutilisable rend un ICPResult par "
		  "defaut -- identite, rmsError = -1 -- indistinguable d'un recalage "
		  "reussi sur des maillages deja alignes" },
		{ "mesh.ambient_occlusion", "Occlusion ambiante", "Analyse",
		  &Make<AmbientOcclusionNode>,
		  "rayon d'influence CABLE a un dixieme de la diagonale de la boite "
		  "englobante, octree cable a 300 elements et 3 niveaux ; un sommet "
		  "d'aire nulle garde une occlusion de 0, que le corps ne distingue pas "
		  "d'un sommet non occulte -- c'est l'adaptateur qui le marque non defini. "
		  "Evaluate lisait ce rayon dans une boite englobante jamais calculee, "
		  "donc dans de la memoire non initialisee : corrige par un "
		  "computebbox () en tete" },
		{ "mesh.thickness", "Epaisseur de paroi", "Analyse", &Make<ThicknessNode>,
		  "numRays est ramene dans [1, 256] et le demi-angle dans [0, 80] SANS "
		  "avertissement ; une epaisseur de 0 avec defined = 0 n'est pas une "
		  "paroi mince mais une absence de mesure" },
		{ "mesh.curvature", "Courbure", "Analyse", &Make<CurvatureNode>,
		  "ApplySteiner a son corps entierement sous #if 0 : la methode n'est "
		  "pas exposee par ce noeud. ApplyHybrid en tire ses directions "
		  "principales, si bien que Hybrid rend aujourd'hui un Desbrun. Les "
		  "sommets de bord et non manifold n'ont pas de tenseur, et zero n'y "
		  "est pas une courbure" },
		// Source de SUITE -- la seule du catalogue, et celle que flow.foreach
		// attendait. Elle est ici, dans le bloc natif, pour la meme raison que
		// les six precedentes : son corps n'est pas compile pour le web.
		{ "mesh.io.load_parts", "Chargement multi-objets", "Maillage", &Make<LoadPartsNode>,
		  "l'import OBJ lit le fichier DEUX FOIS -- une passe Mesh::load pour la "
		  "geometrie, une passe texte pour les frontieres d'objets ; un fichier "
		  "sans 'o' ni 'g' rend UNE piece nommee \"default\", si bien qu'« une "
		  "seule piece » ne se distingue pas de « pas de decoupage »" }
#endif
	};

	// ------------------------------------------------------------------
	// FLUX -- les trois constructions qui cassent le DAG simple.
	//
	// Elles sont PORTABLES : elles n'emballent aucun corps de cgmesh, seulement
	// le serialiseur et l'evaluateur, tous deux de couche A. Rien a exclure
	// sous Emscripten -- et c'est verifie au LIEN de maker, pas suppose.
	//
	// Les deux noeuds de FRONTIERE sont au catalogue au meme titre que les
	// autres, et il le faut : un document de sous-graphe se relit par la meme
	// fabrique que n'importe quel document, laquelle ne connait que le
	// catalogue. Les en tenir a l'ecart aurait exige une seconde fabrique,
	// c'est-a-dire une seconde liste.
	table.push_back (CatalogEntry{ flow::kInputTypeName, "Entree de sous-graphe", "Flux",
	                               &Make<flow::GraphInputNode>,
	                               "n'a de sens que DANS un document delegue : hors "
	                               "d'un hote, rien ne lui lie de valeur et son "
	                               "calcul echoue" });
	table.push_back (CatalogEntry{ flow::kOutputTypeName, "Sortie de sous-graphe", "Flux",
	                               &Make<flow::GraphOutputNode>, nullptr });
	table.push_back (CatalogEntry{ "flow.subgraph", "Sous-graphe", "Flux",
	                               &Make<flow::SubgraphNode>,
	                               "ses PORTS viennent du document delegue : un noeud "
	                               "sans reference, ou dont la reference est illisible, "
	                               "n'a aucun port et ne se connecte a rien. Le document "
	                               "est relu a chaque calcul" });
	table.push_back (CatalogEntry{ "flow.foreach", "Pour chaque element", "Flux",
	                               &Make<flow::ForEachNode>,
	                               "une passe par element, donc une evaluation par "
	                               "element au regard de la signature : sert la "
	                               "DIVERGENCE, jamais la repetition d'un meme "
	                               "sous-calcul, qui doit rester dans le noeud" });
	table.push_back (CatalogEntry{ "flow.repeat", "Repeter n fois", "Flux",
	                               &Make<flow::RepeatNode>,
	                               "ports FIXES a une entree et une sortie, la ou les "
	                               "deux autres derivent les leurs : le chainage exige "
	                               "une correspondance un pour un. n negatif est REFUSE, "
	                               "jamais rabote ; n = 0 est l'identite" });

	// GENERATEURS -- les 26 formes que l'adaptateur GENERIQUE sait emballer.
	// Elles ne sont pas epelees ici : elles se derivent des liaisons, qui se
	// derivent elles-memes du catalogue de formes de cgmesh. Une liste recopiee
	// aurait diverge au premier ajout, exactement comme la fabrique de l'etape 3
	// qui delegue au catalogue plutot que de le reciter.
	//
	// La 27e forme -- la fenetre gothique -- a son propre descripteur, plus bas :
	// c'est le seul objet du catalogue a sous-objets geometriques.
	//
	// PORTABLES : les corps emballes vivent dans parameterized_shapes.cpp et
	// dans les generateurs qu'il appelle, tous presents dans la liste EMSCRIPTEN
	// de cgmesh. Rien a exclure ici -- et c'est verifie au LIEN de maker, pas
	// suppose.
	for (const ShapeBinding &binding : ShapeBindings ())
	{
		const ShapeBinding *bound = &binding;
		table.push_back (CatalogEntry{ binding.typeName, binding.label, "Formes",
		                               [bound] {
			                               return std::unique_ptr<cggraph::Node> (
				                               new ParametricShapeNode (*bound));
		                               },
		                               binding.caveat });
	}

	table.push_back (CatalogEntry{ "shape.gothic.window", "Fenetre gothique", "Formes",
	                               &Make<GothicWindowNode>,
	                               "Regenerate () rattrape TOUTE exception par un "
	                               "PLACEHOLDER -- un unique triangle -- et rend donc "
	                               "un succes sur une combinaison de parametres "
	                               "invalide ; trefoil, mouchettes et foils de "
	                               "lancette restent des membres que le constructeur "
	                               "de maillage ne decoupe pas ; le nombre de "
	                               "lancettes est FIGE a 2, la these ne definissant "
	                               "la tangence de la rosette que pour deux "
	                               "sous-fenetres" });

	// PROFILS -- les sous-objets geometriques devenus des noeuds.
	table.push_back (CatalogEntry{ "profile.chamfer", "Profil : chanfrein", "Profil",
	                               &Make<ChamferProfileNode>, nullptr });
	table.push_back (CatalogEntry{ "profile.cavetto", "Profil : cavet", "Profil",
	                               &Make<CavettoProfileNode>, nullptr });
	table.push_back (CatalogEntry{ "profile.bar.roll", "Profil : bourdon", "Profil",
	                               &Make<RollBarProfileNode>,
	                               "buildBayMoulding laisse les joints d'angle OUVERTS "
	                               "-- aucun onglet n'est calcule -- et rend un "
	                               "maillage vide, sans erreur, quand la geometrie "
	                               "n'offre rien a balayer" });
	table.push_back (CatalogEntry{ "profile.bar.keel", "Profil : arete", "Profil",
	                               &Make<KeelBarProfileNode>,
	                               "meme joint d'angle ouvert que le bourdon" });
	table.push_back (CatalogEntry{ "profile.bar.ogee", "Profil : doucine", "Profil",
	                               &Make<OgeeBarProfileNode>,
	                               "meme joint d'angle ouvert que le bourdon" });

	return table;
	}();
	return entries;
}

std::unique_ptr<cggraph::Node> MakeNode (const std::string &typeName)
{
	for (const CatalogEntry &entry : Catalog ())
		if (typeName == entry.typeName)
			return entry.make ();
	return nullptr;
}

} // namespace cggraph_nodes
