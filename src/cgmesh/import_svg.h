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

    // Tolerance d'aplatissement des courbes, exprimee dans les unites du
    // MAILLAGE PRODUIT et non dans celles du document source : sous
    // `centerAndFit` l'objet fait 1.0 unite, donc 0.005 en est le 1/200e quelle
    // que soit l'echelle du fichier. Meme convention que
    // TextExtrudeOptions::flattenTol (text_extrude.h). La conversion vers les
    // unites du document est faite dans import_svg.cpp.
    float flattenTol   = 0.005f;

    bool  centerAndFit = true;  // recenter on XY bbox and normalize size
    bool  invertY      = true;  // SVG Y points down; flip it

    // Epaissit en contour ferme le trace d'une forme `fill:none` porteuse d'un
    // `stroke`, selon son `stroke-width` ; l'extrusion le traite ensuite comme
    // n'importe quelle surface. Les formes AVEC remplissage ne sont pas
    // affectees.
    //
    // Attention : omettre `fill` en SVG veut dire NOIR, pas « aucun
    // remplissage » -- seul un `fill:none` explicite designe une forme au trait.
    bool  strokeToVolume = true;

    // Multiplicateur applique au `stroke-width` du fichier, pour grossir sans
    // toucher au document des traits trop greles pour etre imprimes.
    float strokeScale = 1.0f;

    // Largeur de repli, en unites SVG, quand la forme declare un stroke sans
    // `stroke-width` exploitable (absent ou nul).
    float strokeWidthFallback = 1.0f;

    // Produit le trait d'une forme fermee ET remplie en ANNEAU
    // (strokeClosedToContours) : un groupe de plus, pousse apres le remplissage
    // de la meme forme -- donc peint par-dessus lui, et sous `Subtract`
    // decoupant celui-ci. C'est la semantique SVG.
    //
    // Attention : l'anneau debordant de sa forme d'une demi-largeur, il elargit
    // l'emprise du dessin ; sous `centerAndFit` l'echelle globale change alors
    // et TOUS les sommets se deplacent, y compris ceux des formes sans trait.
    bool strokeOnFilledShapes = false;

    // Plancher de largeur de trait, dans les unites du MAILLAGE PRODUIT comme
    // `flattenTol` et pour la meme raison ; converti vers les unites du document
    // dans import_svg.cpp. S'applique aux deux familles de trait -- anneaux des
    // formes fermees et rubans des traces ouverts.
    //
    // Un trait plus fin est ELARGI a cette largeur, jamais supprime. 0 desactive
    // le plancher.
    float minStrokeWorldWidth = 0.0f;

    // Toute forme dont l'`id` vaut cette chaine est ecartee, ce qui permet
    // d'exclure du volume un element de MISE EN PAGE. Critere DECLARATIF et non
    // geometrique. nanosvg propage l'id d'un `<g>` a ses formes (nanosvg.h:147),
    // donc enrober suffit a marquer le decor.
    std::string ignoreShapeId;

    // A `false`, toutes les regions sont tessellees ENSEMBLE et sortent a
    // MATERIAL_NONE, si bien qu'un noeud de peinture aval les peint toutes
    // (cggraph/nodes/mesh/color.cpp).
    //
    // A `true`, chaque region est extrudee separement et estampillee du materiau
    // de sa couleur ; les regions de couleur IDENTIQUE partagent un materiau,
    // donc la palette compte les couleurs distinctes et non les formes.
    //
    // Gouverne la COULEUR seule : le recouvrement reste regi par
    // `overlapPolicy`.
    bool perShapeMaterials = false;

    // Traitement des formes qui se recouvrent.
    //   None     : rien n'est retire, les regions se superposent telles quelles.
    //   Subtract : marqueterie -- chaque region est amputee de tout ce qui la
    //              couvre, si bien que les regions rendues sont DISJOINTES en XY
    //              (cf. svg_subtract_overlaps).
    //
    // Gouverne la GEOMETRIE seule et se combine librement avec
    // `perShapeMaterials`, qui gouverne la couleur.
    //
    // Subtract impose UNE TESSELLATION PAR REGION, que la palette soit demandee
    // ou non : les regions decoupees sont adjacentes, et un polygone glutess
    // unique les refondrait en perdant les parois de leurs frontieres communes.
    // Les comptes de sommets et de faces different donc de ceux de None.
    enum class OverlapPolicy { None, Subtract };
    OverlapPolicy overlapPolicy = OverlapPolicy::None;

    // Couleur d'une region issue d'un TRAIT epaissi (cf. strokeToVolume) : celle
    // de son `stroke` quand ce drapeau est vrai, celle de son `fill` sinon --
    // soit noir transparent, une forme sans remplissage n'en ayant pas.
    //
    // Lu SEULEMENT quand `perShapeMaterials` est vrai ; ne deplace aucun sommet.
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
// translucidité (mesh_payload.cpp n'émet que r, g, b).
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
// Balayage du DESSUS vers le DESSOUS. Chaque forme est amputée des seules
// formes sus-jacentes dont la boîte englobante coupe la sienne :
//
//     shape_i       = unionContours (shape_i, {})          — normalisation
//     partenaires_i = { j > i : boite_j ∩ boite_i ≠ vide }
//     region_i      = differenceContours (shape_i, contours des partenaires_i)
//
// Trois règles portent l'exactitude du résultat.
//
// (1) L'unité de filtrage est la FORME, jamais le contour : les trous d'une
//     forme voyagent avec ses enveloppes. Filtrer contour par contour
//     retirerait une enveloppe en gardant son trou.
//
// (2) La NORMALISATION préalable est l'hypothèse qui autorise à CONCATÉNER les
//     partenaires plutôt qu'à soustraire leur réunion. Clipper2 applique la
//     règle de remplissage au clip pris comme un tout (clipper.engine.cpp,
//     `IsContributingClosed`) : sur des contours quelconques la somme des
//     nombres de tours peut S'ANNULER, et le trou du dessus repercerait alors
//     la région du dessous. Après normalisation ces nombres sont nuls ou de
//     signe fixé par la bibliothèque, donc leur somme ne s'annule pas.
//
// (3) Le soustracteur est bâti sur les formes ENTIÈRES et non sur les régions
//     déjà gardées : les deux réunions sont égales, mais partir des formes évite
//     d'enchaîner les arrondis d'une différence sur la suivante.
//
// Le résultat garantit des régions deux à deux DISJOINTES dont la réunion vaut
// celle des formes d'entrée : le dessus devient strictement plan et le volume
// signé redevient un oracle.
//
// Un groupe INTÉGRALEMENT recouvert ressort avec zéro contour ; c'est un
// résultat, mais il est compté pour que la disparition ne soit pas silencieuse.
//
// ⚠ Les contours rendus portent DÉJÀ l'orientation de Clipper2, extérieurs et
// trous en sens opposés, qui est ce qu'attend un remplissage NonZero : les
// réorienter d'après l'aire signée reboucherait les contre-formes.
struct SvgOverlapStats
{
    // Groupes qu'un groupe supérieur recouvre entièrement : ils sortent sans
    // contour et n'émettent aucune face.
    unsigned int fullyCoveredGroups = 0u;

    // Groupes AMPUTÉS d'une partie de leur matière, et qui en gardent. Disjoint
    // de `fullyCoveredGroups` — un groupe est dans l'un ou dans l'autre, jamais
    // dans les deux.
    //
    // Le seuil est RELATIF (un millionième de l'aire de la forme) et non nul :
    // une différence Clipper2 réaligne ses sommets sur une grille de 1e-6, si
    // bien qu'une égalité exacte ne peut pas servir de critère.
    unsigned int partiallyCoveredGroups = 0u;

    // Aires nettes cumulées, en unités monde, AVANT et APRÈS le balayage. Les
    // régions rendues étant disjointes, `keptArea` vaut l'aire de la réunion des
    // formes d'entrée, à la tolérance de Clipper2 près.
    double inputArea   = 0.0;
    double keptArea    = 0.0;
    double removedArea = 0.0;

    // Assiette de l'indexation : paires de boîtes englobantes SÉCANTES sur les
    // n(n−1)/2 possibles. Leur rapport est le facteur de réduction que le
    // balayage peut espérer ; une densité proche de 100 % rend l'indexation
    // inutile.
    unsigned int overlapPairs   = 0u;
    unsigned int candidatePairs = 0u;

    // Durée du balayage SEUL : ni le parse, ni l'aplatissement, ni la
    // tessellation. Chiffre de diagnostic ; il ne fonde aucune décision de
    // performance.
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

// Contours 2D du SVG, en unités monde, prêts à extruder — l'étage que
// import_svg_extruded enchaîne en interne, rendu accessible pour qu'un nœud de
// graphe puisse s'y brancher. C'est exactement la CONCATÉNATION de
// svg_to_shape_groups, dans l'ordre des groupes.
//
// ⚠ La règle de remplissage de CHAQUE forme (even-odd ou non-zero) est résolue
// ici, par une passe Clipper2 : une liste plate de contours ne peut pas la
// transporter, et elle n'est connue nulle part ailleurs. Les contours rendus
// sortent donc orientés pour NonZero, extérieurs et trous en sens opposés.
//
// `height` n'est pas lu : c'est un réglage d'extrusion.
// Rend false si le fichier est illisible ou ne donne aucun contour.
bool svg_to_contours(const std::string& filename, const SvgExtrudeOptions& opt,
                     std::vector<ExtrudeContour>& out);

// ============================================================================
//  Correspondances rendues par l'extrusion
// ============================================================================
//
// Les deux vecteurs sont indexes par GROUPE, dans l'ordre de
// svg_to_shape_groups. `Mesh` ne portant qu'un identifiant de MATERIAU par
// face, la correspondance face -> forme n'existe que sous cette forme.
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
    // groupes et ou aucune face ne se rattache donc a l'un d'eux.
    std::vector<std::pair<unsigned int, unsigned int>> facesOfGroup;

    // Groupes dont la couleur APPROXIME un degrade par la moyenne de ses etapes
    // (SvgShapePaint::isGradient). Compte les groupes RETENUS, degrade de trait
    // comme de remplissage, que la palette soit demandee ou non.
    unsigned int gradientGroups = 0u;

    // Bilan du balayage de marqueterie. Tout a zero quand `overlapPolicy` vaut
    // None : rien n'a alors ete retire.
    SvgOverlapStats overlap;
};

// Parse `filename` and return a heap-allocated extruded Mesh, or nullptr on
// failure (file missing, parse error, no fillable shape).
//
// `mapping` est optionnel : nullptr signifie que l'appelant n'en veut pas, et
// rien n'est alors accumule.
Mesh* import_svg_extruded(const std::string& filename, const SvgExtrudeOptions& opt,
                          SvgExtrudeMapping* mapping = nullptr);
