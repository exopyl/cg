#pragma once

// ============================================================================
//  SVG → extruded Mesh
// ============================================================================
//
// Parse an SVG file (via extern/nanosvg), flatten cubic Bezier paths into
// polylines, tessellate the resulting 2D filled regions with glutess, then
// extrude along Z to produce a 3D solid Mesh.
//
//   - SVG Y axis points down by convention; the importer flips Y so the
//     result is upright in a standard right-handed +Y-up viewer.
//   - When centerXY is true, the produced mesh is recentered on its XY
//     bounding box and uniformly scaled so the longest XY dimension equals
//     1.0 (consistent with the other parameterized geometries in sinaia).
//   - Each <path> is treated as a contour; multiple contours within one
//     <shape> are passed to the tessellator together (NONZERO winding so
//     internal holes are subtracted as expected by most SVG authors).
//
// ============================================================================

#include <string>
#include <utility>
#include <vector>

#include "extrude_contours.h"   // ExtrudeContour

class Mesh;

struct SvgExtrudeOptions
{
    float height       = 1.0f;  // extrusion depth along +Z

    // ------------------------------------------------------------------------
    //  Tolerance d'aplatissement des courbes
    // ------------------------------------------------------------------------
    // Exprimee dans les unites du MAILLAGE PRODUIT, et non dans celles du
    // document source. Avec `centerAndFit`, l'objet fait 1.0 unite : 0.005 est
    // donc le 1/200e de sa plus grande dimension, quelle que soit l'echelle du
    // fichier -- icone de 24 ou plan de travail de 2048. C'est la meme convention
    // que TextExtrudeOptions::flattenTol (text_extrude.h), et les deux valeurs
    // sont desormais comparables.
    //
    // Jusqu'au 2026-08-14 cette tolerance etait consommee dans les unites du
    // DOCUMENT, la mise a l'echelle n'intervenant qu'apres l'aplatissement. La
    // finesse des courbes dependait donc d'un nombre que l'utilisateur ne
    // choisit pas : sur un plan de travail de 2048, la course entiere du curseur
    // tenait sous 0.005 unite monde -- impossible de degrossir ; sur une icone de
    // 24, son maximum donnait 42 % d'ecart. La conversion est faite dans
    // import_svg.cpp, ou la circularite qu'elle contourne est expliquee.
    float flattenTol   = 0.005f;

    bool  centerAndFit = true;  // recenter on XY bbox and normalize size
    bool  invertY      = true;  // SVG Y points down; flip it

    // ------------------------------------------------------------------------
    //  Formes au TRAIT (stroke sans fill)
    // ------------------------------------------------------------------------
    // Une forme sans remplissage etait purement ignoree. C'est correct au sens
    // strict -- du dessin au trait n'a pas de surface -- mais cela rendait
    // inexploitable tout SVG de ce genre, et masquait un piege : omettre `fill`
    // en SVG veut dire NOIR, pas « aucun remplissage », si bien qu'un cadre de
    // page cense etre un simple trait ressortait en plaque pleine.
    //
    // Quand `strokeToVolume` est vrai, une forme en `fill:none` porteuse d'un
    // `stroke` voit son trace EPAISSI selon son `stroke-width` : chaque
    // polyligne ouverte devient un contour ferme, que l'extrusion traite ensuite
    // comme n'importe quelle surface. C'est la seule operation qui ait un sens
    // sur un trace qui se replie sur lui-meme -- une courbe du dragon remplie
    // « comme une surface » degenere en damier, faute de pouvoir designer un
    // interieur.
    //
    // Les formes AVEC remplissage ne sont pas affectees : elles suivent le
    // chemin de tessellation habituel.
    bool  strokeToVolume = true;

    // Multiplicateur applique au `stroke-width` du fichier. Les traits sont
    // souvent tres fins par rapport au dessin (0.2 sur un canevas de 250), ce
    // qui donne un volume trop grele pour etre imprime ; ce facteur permet de
    // les grossir sans toucher au fichier.
    float strokeScale = 1.0f;

    // Largeur de repli, en unites SVG, quand la forme declare un stroke sans
    // `stroke-width` exploitable (absent ou nul).
    float strokeWidthFallback = 1.0f;

    // ------------------------------------------------------------------------
    //  Trait des formes FERMEES ET REMPLIES
    // ------------------------------------------------------------------------
    // Une forme fermee porteuse d'un `fill` ET d'un `stroke` ne rendait que son
    // remplissage : son trait etait perdu. Sur un dessin dont la peinture est
    // portee par les groupes, c'est la perte la plus massive du document.
    //
    // A `true`, le trait d'une telle forme est produit en ANNEAU
    // (strokeClosedToContours) : un groupe DE PLUS, pousse apres le remplissage
    // de la meme forme -- donc peint par-dessus lui, et sous `Subtract`
    // decoupant celui-ci. C'est la semantique SVG.
    //
    // Defaut `false`, et ce n'est pas de la prudence : l'anneau deborde de sa
    // forme d'une demi-largeur, ce qui elargit l'emprise du dessin. Sous
    // `centerAndFit` l'echelle globale change alors, et TOUS les sommets se
    // deplacent -- y compris ceux des formes sans trait.
    //
    // Avec `perShapeMaterials` a faux, un anneau prend la couleur de son
    // remplissage : il ajoute de la matiere et du cout sans rien montrer.
    bool strokeOnFilledShapes = false;

    // ------------------------------------------------------------------------
    //  Largeur minimale de trait
    // ------------------------------------------------------------------------
    // Exprimee dans les unites du MAILLAGE PRODUIT, comme `flattenTol` et pour
    // la meme raison : avec `centerAndFit` l'objet fait 1.0, si bien qu'une
    // largeur en unites de DOCUMENT ne dit rien tant qu'on ne connait pas
    // l'echelle du fichier. La conversion vers les unites du document est faite
    // dans import_svg.cpp, avec la meme emprise estimee que la tolerance, et
    // c'est la meme circularite qu'elle contourne.
    //
    // Un trait plus fin que ce plancher est ELARGI a la largeur du plancher. Il
    // n'est jamais supprime : un trait sous-resolu reste une information du
    // document, et l'ecarter reperdrait ce que `strokeOnFilledShapes` recupere.
    //
    // S'applique aux DEUX familles de trait -- anneaux des formes fermees et
    // rubans des traces ouverts --, la largeur etant lue au meme endroit pour
    // les deux.
    //
    // 0 DESACTIVE le plancher, et c'est le defaut : aucune largeur n'est alors
    // touchee.
    float minStrokeWorldWidth = 0.0f;

    // ------------------------------------------------------------------------
    //  Formes a IGNORER, par identifiant
    // ------------------------------------------------------------------------
    // Toute forme dont l'`id` vaut cette chaine est ecartee, ce qui permet
    // d'exclure du volume un element de MISE EN PAGE -- un cadre, un reperage --
    // sans retoucher le fichier. nanosvg propage l'id d'un `<g>` a ses formes
    // (nanosvg.h:147), donc enrober suffit a marquer le decor.
    //
    // Critere DECLARATIF, et non geometrique : reconnaitre « le contour qui
    // coincide avec le canevas » supposerait un seuil, et se tromperait sur un
    // dessin qui remplit legitimement sa page.
    //
    // Vide par DEFAUT : rien n'est ignore sans demande explicite. Les SVG generes
    // par le depot n'ont plus de cadre du tout -- ne pas l'ecrire est plus simple
    // que de le filtrer -- donc cette option n'a aujourd'hui aucun appelant
    // interne ; elle sert aux fichiers venus d'ailleurs.
    std::string ignoreShapeId;

    // ------------------------------------------------------------------------
    //  Couleurs du document
    // ------------------------------------------------------------------------
    // A `false`, toutes les regions sont tessellees ENSEMBLE et aucune face ne
    // porte de materiau : elles sortent toutes a MATERIAL_NONE, donc un noeud de
    // peinture aval les peint toutes (cggraph/nodes/mesh/color.cpp).
    //
    // A `true`, chaque region est extrudee separement et estampillee du materiau
    // de sa couleur. Les regions de couleur IDENTIQUE partagent un materiau : la
    // palette compte les couleurs distinctes, pas les formes -- c'est le nombre
    // de lots de rendu, et le navigateur reconstruit un materiau par lot a chaque
    // changement de palette.
    //
    // Ce drapeau gouverne la COULEUR seule. Le recouvrement reste regi par
    // `overlapPolicy` : deux capots superposes restent coplanaires tant qu'il
    // vaut None.
    bool perShapeMaterials = false;

    // Traitement des formes qui se recouvrent.
    //   None     : rien n'est retire, les regions se superposent telles quelles.
    //   Subtract : marqueterie -- chaque region est amputee de tout ce qui la
    //              couvre, si bien que les regions rendues sont DISJOINTES en XY
    //              (cf. svg_subtract_overlaps).
    //
    // Ce reglage gouverne la GEOMETRIE seule et se combine librement avec
    // `perShapeMaterials`, qui gouverne la couleur : la marqueterie a un sens
    // sans palette -- elle rend un dessus strictement plan et un volume egal a
    // celui de la reunion des formes.
    //
    // Subtract impose en revanche UNE TESSELLATION PAR REGION, que la palette
    // soit demandee ou non : les regions decoupees sont adjacentes, et un
    // polygone glutess unique les refondrait en perdant les parois de leurs
    // frontieres communes (voir import_svg.cpp). Les comptes de sommets et de
    // faces changent donc par rapport a None, meme sans couleur.
    enum class OverlapPolicy { None, Subtract };
    OverlapPolicy overlapPolicy = OverlapPolicy::None;

    // Couleur d'une region issue d'un TRAIT epaissi (forme `fill:none` porteuse
    // d'un `stroke`, cf. strokeToVolume) : celle de son `stroke` quand ce drapeau
    // est vrai, celle de son `fill` sinon -- soit noir transparent, une forme
    // sans remplissage n'en ayant pas.
    //
    // Lu SEULEMENT quand `perShapeMaterials` est vrai. Il ne deplace aucun
    // sommet : la geometrie ne depend pas de lui.
    bool strokeUsesStrokeColor = true;
};

// ============================================================================
//  Peinture d'une région produite par l'import
// ============================================================================
//
// Les couleurs sont empaquetées A LA CONVENTION NANOSVG, `r | g<<8 | b<<16 |
// a<<24` (nanosvg.h:213) : c'est celle du champ source, la reconvertir ici
// obligerait chaque consommateur à savoir laquelle a été choisie.
//
// L'alpha combine les deux opacités que SVG distingue : celle du remplissage,
// que nanosvg a déjà repliée dans `fill.color`, et celle du groupe
// (`NSVGshape::opacity`), qui les multiplie. Il est LU ET STOCKÉ mais n'est
// transmis à aucun étage aval : la chaîne de rendu ne porte pas la
// translucidité (mesh_payload.cpp n'émet que r, g, b). Le stocker coûte zéro et
// évite d'avoir à reparcourir le document le jour où elle la portera.
struct SvgShapePaint
{
    unsigned int fillRGBA   = 0u;
    unsigned int strokeRGBA = 0u;

    // false => la région ne vient pas d'un remplissage mais d'un TRAIT épaissi.
    bool hasFill = false;

    // Une des deux couleurs ci-dessus approxime un dégradé par la moyenne de ses
    // stops, pondérée par la portion de rampe que chacun gouverne. Un aplat ne
    // peut pas rendre un dégradé ; ce drapeau rend l'écart visible plutôt que
    // silencieux.
    bool isGradient = false;

    // Rang de la forme SOURCE dans le document, 0 = dessous. nanosvg chaîne ses
    // formes en ordre de document (nanosvg.h:1030-1035), qui est l'ordre du
    // peintre : le rang est donc directement le z-order.
    //
    // Il compte TOUTES les formes produites par nanosvg, y compris celles que
    // l'import écarte : la suite peut donc présenter des trous, et TOUS les
    // groupes issus d'une même forme (son remplissage, puis son trait) partagent
    // le même rang. Ce qui est garanti est l'ordre, pas la contiguïté.
    unsigned int rank = 0u;

    // `NSVGshape::id`, pour le diagnostic. nanosvg propage l'id d'un `<g>` à ses
    // formes, donc plusieurs groupes peuvent porter le même.
    std::string id;
};

// Une région du document et sa peinture. Les contours sont DÉJÀ résolus : la
// règle de remplissage de la forme source y a été appliquée, ils sortent
// orientés pour NonZero.
struct SvgShapeGroup
{
    std::vector<ExtrudeContour> contours;
    SvgShapePaint               paint;
};

// ============================================================================
//  Marqueterie par différence, indexée par boîtes englobantes
// ============================================================================
//
// Balayage du DESSUS vers le DESSOUS. Chaque forme est amputée de ce qui la
// couvre. Ce qui la couvre n'est PAS le dessin entier : ce sont les seules
// formes sus-jacentes dont la boîte englobante coupe la sienne.
//
//     shape_i       = unionContours (shape_i, {})          — normalisation
//     partenaires_i = { j > i : boite_j ∩ boite_i ≠ vide }
//     region_i      = differenceContours (shape_i, contours des partenaires_i)
//
// Deux propriétés, à démontrer séparément, portent l'exactitude du résultat.
//
// (1) ÉCARTER LES NON-PARTENAIRES NE CHANGE RIEN. Soit p un point de shape_i,
//     donc de boite_i. Si j n'est pas partenaire, boite_j ne coupe pas boite_i,
//     donc p n'est pas dans boite_j, donc p n'est pas dans shape_j. Les formes
//     écartées ne contiennent aucun point de shape_i : les retirer du
//     soustracteur ne déplace aucun point de la différence. C'est une égalité,
//     pas une approximation.
//
//     L'unité de filtrage est la FORME, jamais le contour : les trous d'une
//     forme voyagent avec ses enveloppes. Filtrer contour par contour
//     retirerait une enveloppe en gardant son trou.
//
// (2) CONCATÉNER LES PARTENAIRES REVIENT À SOUSTRAIRE LEUR RÉUNION — et c'est
//     le point qui ne se devine pas. Clipper2 applique la règle de remplissage
//     au clip pris comme un tout : la région soustraite est l'ensemble des
//     points où la SOMME des nombres de tours des contours de clip est non
//     nulle (clipper.engine.cpp, `IsContributingClosed`). Sur des formes
//     quelconques, cette somme peut S'ANNULER — un trou compté −1 contre
//     l'extérieur d'une autre forme compté +1 — et le trou du dessus
//     repercerait alors la région du dessous.
//
//     La normalisation préalable l'interdit : un jeu de contours rendu par
//     Clipper2 a un nombre de tours valant 0 hors de sa région et une même
//     valeur non nulle dedans, de signe fixé par la convention de la
//     bibliothèque, donc IDENTIQUE d'une forme à l'autre. Une somme de termes
//     nuls ou de même signe ne s'annule que si tous sont nuls : la région de
//     clip vaut exactement la réunion des partenaires. La normalisation n'est
//     donc pas une précaution, c'est l'hypothèse de la démonstration.
//
// Le soustracteur est bâti sur les formes ENTIÈRES et non sur les régions déjà
// gardées : les deux réunions sont égales par construction, mais partir des
// formes évite d'enchaîner les arrondis d'une différence sur la suivante.
//
// COÛT. n(n−1)/2 tests de boîtes, puis une normalisation et une différence par
// forme, cette dernière sur ses seuls partenaires. Le gain vaut ce que vaut la
// DENSITÉ de paires sécantes, publiée par `overlapPairs` : à 10 % elle divise
// le travail par autant, à 100 % l'indexation ne retire rien et le balayage
// redevient celui du dessin entier, en payant les tests de boîtes en plus.
//
// Ce que le résultat garantit, et qui est la raison d'être de l'opération : les
// régions rendues sont deux à deux DISJOINTES, et leur réunion vaut celle des
// formes d'entrée. Rien n'est perdu hors recouvrement, rien n'est compté deux
// fois — le dessus devient strictement plan et le volume signé redevient un
// oracle.
//
// Un groupe INTÉGRALEMENT recouvert ressort avec zéro contour. C'est un
// résultat et non une panne — le document le cache derrière ce qui est au-dessus
// — mais il est compté, faute de quoi la disparition serait silencieuse.
//
// Les contours rendus portent l'orientation de Clipper2, extérieurs et trous en
// sens opposés : c'est déjà ce qu'attend un remplissage NonZero, il n'y a rien à
// réorienter. Les réorienter d'après l'aire signée reboucherait les
// contre-formes.
struct SvgOverlapStats
{
    // Groupes qu'un groupe supérieur recouvre entièrement : ils sortent sans
    // contour et n'émettent aucune face.
    unsigned int fullyCoveredGroups = 0u;

    // Groupes AMPUTÉS d'une partie de leur matière, et qui en gardent : ils
    // émettent des faces, mais moins d'emprise que la forme d'entrée. Disjoint
    // de `fullyCoveredGroups` — un groupe est dans l'un ou dans l'autre, jamais
    // dans les deux.
    //
    // Le seuil est RELATIF (un millionième de l'aire de la forme) et non nul :
    // une différence Clipper2 réaligne ses sommets sur une grille de 1e-6, si
    // bien qu'une égalité exacte ne peut pas servir de critère. Une perte plus
    // petite que ce seuil n'est pas un recouvrement, c'est du bruit d'arrondi.
    unsigned int partiallyCoveredGroups = 0u;

    // Aires nettes cumulées, en unités monde, AVANT et APRÈS le balayage. La
    // différence est ce que le recouvrement a retiré. Les régions rendues étant
    // disjointes, `keptArea` vaut l'aire de la réunion des formes d'entrée, à la
    // tolérance de Clipper2 près.
    double inputArea   = 0.0;
    double keptArea    = 0.0;
    double removedArea = 0.0;

    // Assiette de l'indexation : paires de boîtes englobantes SÉCANTES sur les
    // n(n−1)/2 possibles. Leur rapport est le facteur de réduction que le
    // balayage peut espérer. Publié parce qu'une densité proche de 100 % rend
    // l'indexation inutile, et que sans ce chiffre le fait resterait invisible.
    unsigned int overlapPairs   = 0u;
    unsigned int candidatePairs = 0u;

    // Durée du balayage SEUL : ni le parse, ni l'aplatissement, ni la
    // tessellation. C'est un chiffre de diagnostic ; il ne fonde aucune décision
    // de performance, qui relève d'un chronométrage dédié des trois postes.
    double sweepMs = 0.0;
};

// Applique la marqueterie EN PLACE, dans l'ordre des groupes rendus par
// svg_to_shape_groups (rang croissant, le dernier est au-dessus).
//
// Les groupes gardent leur place et leur peinture : la correspondance
// indice -> forme survit à l'opération, y compris pour ceux qui disparaissent.
// `stats` est optionnel.
void svg_subtract_overlaps(std::vector<SvgShapeGroup>& groups,
                           SvgOverlapStats* stats = nullptr);

// Régions du SVG, en unités monde, chacune avec sa peinture — même étage que
// svg_to_contours, sans l'aplatissement final.
//
// Une forme SOURCE donne de zéro à TROIS groupes, dans cet ordre : son
// remplissage, l'anneau de son trait sur ses contours fermés (cf.
// `strokeOnFilledShapes`), puis le ruban issu de ses tracés ouverts. Les groupes
// sans contour ne sont pas rendus.
//
// Rend false si le fichier est illisible ou ne donne aucune région.
bool svg_to_shape_groups(const std::string& filename, const SvgExtrudeOptions& opt,
                         std::vector<SvgShapeGroup>& out);

// Parse `filename` and return a heap-allocated extruded Mesh, or nullptr on
// failure (file missing, parse error, no fillable shape).
// Contours 2D du SVG, en unités monde, prêts à extruder — l'étage que
// import_svg_extruded enchaîne en interne, rendu accessible pour qu'un nœud de
// graphe puisse s'y brancher.
//
// ⚠ La règle de remplissage de CHAQUE forme (even-odd ou non-zero) est résolue
// ici, par une passe Clipper2 : une liste plate de contours ne peut pas la
// transporter, et elle n'est connue nulle part ailleurs. Les contours rendus
// sortent donc orientés pour NonZero, extérieurs et trous en sens opposés.
//
// `height` n'est pas lu : c'est un réglage d'extrusion.
// Rend false si le fichier est illisible ou ne donne aucun contour.
//
// C'est exactement la CONCATÉNATION de svg_to_shape_groups, dans l'ordre des
// groupes : la version plate est dérivée de la version groupée et ne peut donc
// pas diverger.
bool svg_to_contours(const std::string& filename, const SvgExtrudeOptions& opt,
                     std::vector<ExtrudeContour>& out);

// ============================================================================
//  Correspondances rendues par l'extrusion
// ============================================================================
//
// Les deux vecteurs sont indexes par GROUPE, dans l'ordre de
// svg_to_shape_groups. Ils servent au diagnostic (« quelle forme a produit ces
// faces ? ») et aux statistiques publiees par un noeud de graphe.
//
// `Mesh` ne porte qu'un identifiant de MATERIAU par face : la correspondance
// face -> forme n'existe donc que sous cette forme, et lui ajouter un tableau
// par face pour la reproduire couterait une modification d'un type central.
struct SvgExtrudeMapping
{
    // Materiau estampille sur les faces du groupe. Vaut MATERIAL_NONE partout
    // quand SvgExtrudeOptions::perShapeMaterials est faux.
    std::vector<unsigned int> materialOfGroup;

    // Plage de faces emise par le groupe : (premiere face, nombre de faces),
    // dans la numerotation du Mesh rendu. Un groupe qui n'a rien tessele a un
    // nombre nul.
    //
    // VIDE sur le chemin MONOLITHIQUE -- `perShapeMaterials` faux ET
    // `overlapPolicy` a None --, ou une seule tessellation prend tous les
    // groupes et ou aucune face ne se rattache donc a l'un d'eux. Une plage
    // inventee dans ce cas serait fausse.
    std::vector<std::pair<unsigned int, unsigned int>> facesOfGroup;

    // Groupes dont la couleur APPROXIME un degrade par la moyenne de ses etapes
    // (SvgShapePaint::isGradient). Rendu ici plutot que par un parcours des
    // groupes cote appelant : import_svg_extruded est le seul etage qui les
    // tienne, et les redemander couterait un second parse du document.
    //
    // Compte les groupes RETENUS, degrade de trait comme de remplissage, que la
    // palette soit demandee ou non -- l'ecart existe des que la couleur est lue,
    // et il vaut d'etre visible meme sur le chemin monolithique.
    unsigned int gradientGroups = 0u;

    // Bilan du balayage de marqueterie. Tout a zero quand `overlapPolicy` vaut
    // None : rien n'a alors ete retire.
    //
    // Ces chiffres sont publies ICI, et non par une sortie a part, parce qu'ils
    // ont le meme producteur et le meme consommateur que les deux
    // correspondances ci-dessus : un seul appel les rend tous, et un appelant
    // qui n'en veut pas passe deja nullptr.
    SvgOverlapStats overlap;
};

// `mapping` est optionnel : nullptr signifie que l'appelant n'en veut pas, et
// rien n'est alors accumule.
Mesh* import_svg_extruded(const std::string& filename, const SvgExtrudeOptions& opt,
                          SvgExtrudeMapping* mapping = nullptr);
