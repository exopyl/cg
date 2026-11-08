#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Entablement peripherique grec : anneau balaye a section en bandes, frise
//  monotriglyphe, triglyphes en prismes
// ============================================================================
//
// Repere (D2-5, celui du temple) : X le long de la facade, Y en profondeur
// (facade en Y < 0), Z vertical ; rectangle des axes centre sur l'origine.
// Unites : celles de l'appelant. Une saillie u (ou d) se mesure depuis la
// ligne des axes du cote, positive vers l'exterieur ; z depuis le pied de
// l'entablement.
//
// Conventions (registre EN1 a EN5, EN10, EN14, EN16, TE5,
// classical_orders_implementation.md section 10.10) : face interieure plane et
// verticale, sans corniche interieure (EN1) ; profil en bandes a faces droites,
// verticales ou inclinees, et ressauts horizontaux (EN2) ; un triglyphe sur
// chaque axe et au milieu de chaque travee, la metope etant le reste (EN3) ;
// angle selon DoricCornerStrategy : par defaut, triglyphe sur l'axe d'angle de
// chaque cote et demi-metope a l'angle (EN4), sinon triglyphe d'angle en une
// piece en L (EN14) ; triglyphes en prismes poses sur la frise de l'anneau, au
// nu des metopes, les metopes etant la face de l'anneau (EN5) ; contraction
// d'angle calculee, rapportee et jamais appliquee au peristyle (EN16) ;
// ornements optionnels (EN17 a EN24) : regula sous chaque triglyphe, en L sous
// une piece d'angle, gouttes en troncs de cone, mutules en plaques sur une
// sous-face horizontale ou inclinee (EN22), via = reste, angle sans mutule,
// gouttes des mutules en rangs, d'axe normal a la sous-face (EN23, EN24).

// Sommet de section : u, distance a la ligne des axes, positive vers
// l'exterieur ; z depuis le pied.
struct RingSectionPoint
{
	double u = 0.0, z = 0.0;
};

// Balayage d'un polygone ferme simple, de sens direct dans (u, z), le long du
// rectangle des axes [-halfX, halfX] x [-halfY, halfY] : chaque sommet (u, z)
// devient le rectangle [-(halfX + u), halfX + u] x [-(halfY + u), halfY + u] a
// la cote z0 + z ; quads entre sommets voisins, le dernier relie au premier ;
// onglets a 45 degres. Solide en anneau ferme (genre 1), sans couvercle :
// 4 sommets et 8 triangles par sommet de section. Positions calculees en
// double, arrondies une fois en float. Leve std::invalid_argument, prefixe
// "Anneau : ", si moins de 3 sommets, coordonnee ou demi-longueur non finie,
// halfX + u <= 0 ou halfY + u <= 0, deux sommets voisins confondus, polygone
// non simple ou de sens indirect.
std::unique_ptr<Mesh> SweepRectangularRing (const std::vector<RingSectionPoint> &section, double halfX, double halfY,
                                            double z0);

// Volume exact du balayage : A (4 (halfX + halfY) + 8 uG), A aire et uG
// abscisse du centroide de la section (formule du lacet). Ne valide pas.
double RectangularRingVolume (const std::vector<RingSectionPoint> &section, double halfX, double halfY);

struct EntablatureBand
{
	double height = 0.0;
	double projection = 0.0;      // saillie du pied de la face, depuis la ligne des axes
	double topProjection = 0.0;   // saillie du haut de la face ; egale a projection pour une face verticale
};

struct GreekTriglyphSettings
{
	double width = 0.0;
	double face = 0.0;                     // saillie de la face des bandes du triglyphe
	std::vector<double> divisionWidths;    // de gauche a droite, largeurs quelconques, somme egale a width
	std::vector<bool>   divisionIsChannel; // demi-canaux aux deux bords
	double channelDepth = 0.0;             // canal plein : fond du V, au milieu du canal, depuis face
	double halfChannelDepth = 0.0;         // demi-canal : au bord du triglyphe, depuis face ; aucune relation d'ordre avec channelDepth
};

// Traitement de l'angle de la frise dorique (liste fermee, D3-54). F : saillie
// de la face du triglyphe (triglyph.face) ; w : sa largeur ; c = F - w / 2.
enum class DoricCornerStrategy
{
	AxisTriglyph,                 // EN4 : triglyphe sur l'axe d'angle de chaque cote, demi-metope a l'angle
	CornerTriglyphEqualMetopes,   // EN14 : triglyphe d'angle ; intermediaire de la travee d'angle a egale distance
	                              // des deux triglyphes qui l'encadrent, deux metopes egales
	CornerTriglyphMidBay,         // EN14 : triglyphe d'angle ; intermediaire au milieu de la travee d'angle
};

// Goutte (EN19) : tronc de cone de revolution, axe vertical ; generatrice avant
// a footProjection au pied et a topProjection en tete, saillies depuis la ligne
// des axes ; diametre de tete = footDiameter - 2 (footProjection -
// topProjection).
struct GreekGuttaSettings
{
	double footDiameter = 0.0;
	double footProjection = 0.0;
	double topProjection = 0.0;    // <= footProjection
	int    sides = 8;              // multiple de 4, [4, kGreekMaxGuttaSides] : sommets avant, arriere et lateraux exacts (format)
};

// Bornes d'interface, pas des valeurs du domaine : elles bornent le maillage
// d'une regula et l'allocation de ses gouttes.
constexpr int kGreekMaxGuttae = 64;        // GreekRegulaSettings::guttaCount
constexpr int kGreekMaxGuttaSides = 256;   // GreekGuttaSettings::sides

// Regulae (EN17, EN18, EN20) : une sous chaque placement de triglyphe, sous la
// tenia (bande friezeBand - 1), dos contre la face verticale de la bande
// friezeBand - 2 ; gouttes pendues du dessous de la regula au pied de cette
// bande : leur hauteur est bands[friezeBand - 2].height - height, sans champ
// propre.
struct GreekRegulaSettings
{
	double height = 0.0;           // ]0, bands[friezeBand - 2].height[
	double face = 0.0;             // ]dos, bands[friezeBand - 1].projection]
	int    guttaCount = 0;         // par regula droite, et par bras d'une regula d'angle
	double guttaGap = 0.0;         // vide entre gouttes voisines, au pied
	GreekGuttaSettings gutta;
};

// Gouttes des mutules (EN23, EN24) : rangs du devant vers le dos, perRow
// gouttes par rang. Le long de la sous-face, depuis l'arete avant-basse du
// mutule : bornes cumulees sur {rowDiameters[0], rowGaps[0], ...,
// rowDiameters[r-1]}, la premiere a 0 (avant de la tete de la goutte avant sur
// l'arete) ; marge arriere = reste. En largeur : axes extremes a edgeAxis des
// bords du mutule, pas egal (EqualParts). Tronc de cone d'axe normal a la
// sous-face : tete sur la sous-face, de diametre rowDiameters[i] ; pied a
// `height` sous elle, de diametre rowDiameters[i] + 2 conicity.
struct GreekMutuleGuttaSettings
{
	std::vector<double> rowDiameters;   // a la tete, du devant vers le dos ; [1, kGreekMaxGuttae] rangs
	std::vector<double> rowGaps;        // vides entre tetes de rangs voisins ; rowDiameters.size () - 1
	int    perRow = 0;                  // [2, kGreekMaxGuttae]
	double edgeAxis = 0.0;              // axe des gouttes extremes d'un rang, depuis le bord du mutule
	double height = 0.0;                // normale a la sous-face
	double conicity = 0.0;              // evasement : rayon de pied - rayon de tete, >= 0
	int    sides = 8;                   // multiple de 4, [4, kGreekMaxGuttaSides]
};

// Mutules (EN21) : dans la bande `band` ; dos = bands[band].projection (face
// verticale), hauteur = bands[band].height, dessus sous le ressaut de la bande
// band + 1, dessous a la cote du pied de la bande. Sous-face horizontale (EN2)
// si rise = 0 : le dos peut etre en retrait sur la bande du dessous, le mutule
// y pose (L14). Sous-face inclinee (EN22) si rise > 0 : le dessous monte de sa
// face (pivot a z[band]) a son dos (z[band] + rise), plaque verticale de
// hauteur bands[band].height ; le mutule est le prisme d'un parallelogramme.
struct GreekMutuleSettings
{
	int    band = -1;              // [friezeBand + 1, bands.size () - 2]
	double width = 0.0;
	double face = 0.0;             // ]bands[band].projection, bands[band + 1].projection]
	double rise = 0.0;             // montee du dessous, de la face au dos ; 0 : section et mutule identiques au bit pres a ceux d'une sous-face horizontale
	std::optional<GreekMutuleGuttaSettings> guttae = std::nullopt;   // absent : mutules sans gouttes
};

struct GreekEntablatureSettings
{
	std::vector<EntablatureBand> bands;    // profil exterieur, de bas en haut
	double innerFace = 0.0;                // face interieure plane, a innerFace de la ligne des axes, vers l'interieur
	int    friezeBand = -1;                // bande a face verticale : nu des metopes et dos des triglyphes
	GreekTriglyphSettings triglyph;
	DoricCornerStrategy corner = DoricCornerStrategy::AxisTriglyph;
	std::optional<GreekRegulaSettings> regulae = std::nullopt;   // absent : entablement sans regulae ni gouttes
	std::optional<GreekMutuleSettings> mutules = std::nullopt;   // absent : plaque des mutules en bande continue (EN12)
};

// Refus, std::invalid_argument, message prefixe "Entablement : " et champ entre
// apostrophes ('bands[2].height', 'triglyph.channelDepth') :
//   - moins de deux bandes ('bands') ; hauteur, saillie ou saillie du haut non
//     finie ou <= 0 ; innerFace non finie ou <= 0 ;
//   - friezeBand hors de [1, bands.size () - 2] ; face de la bande de frise non
//     verticale ('bands[k].topProjection') ;
//   - largeur du triglyphe non finie ou <= 0 ; moins de deux divisions, tailles
//     des deux listes differentes, division non finie ou <= 0, somme differente
//     de width a 1e-12 relatif pres, bord sans demi-canal ;
//   - avec b = bands[friezeBand].projection : non (b < face) ('triglyph.face') ;
//     non (0 < channelDepth < face - b) ; non (0 < halfChannelDepth < face - b) ;
//   - corner hors de DoricCornerStrategy ('corner') ;
//   - sous CornerTriglyphEqualMetopes et CornerTriglyphMidBay seulement :
//     width <= face - b ('triglyph.width' : les deux bras de la piece d'angle ne
//     reposeraient pas chacun sur une face de frise ; une telle section n'est
//     jamais simple, ses deux bouts interieurs se croisant sur la diagonale de
//     l'angle Front : ce refus precede et nomme le motif du suivant) ; section
//     en L de la piece d'angle (EN14, BuildTriglyphMesh) non simple a l'un des
//     quatre angles ('triglyph.divisionWidths' : chanfrein d'arete passe
//     derriere les nus des metopes, largeur de la division de bord au-dela de
//     2 (face - b)). La simplicite se decide en evaluation stricte (produits
//     jamais contractes en FMA) : meme verdict sur toute cible ;
//   - avec regulae, f = friezeBand, dos = bands[f - 2].projection :
//     f < 3 ('regulae' : sous la bande f - 2, une bande f - 3 separe le pied
//     des gouttes du pied de l'entablement, donc de l'abaque) ; face de la bande
//     f - 2 non verticale ('bands[f - 2].topProjection') ; height hors de
//     ]0, bands[f - 2].height[ ('regulae.height') ; face hors de
//     ]dos, bands[f - 1].projection] ('regulae.face') ; guttaCount hors de
//     [1, kGreekMaxGuttae] ('regulae.guttaCount', avant toute allocation) ;
//     guttaGap non fini ou <= 0 ('regulae.guttaGap') ; footDiameter non fini ou
//     <= 0, ou footProjection - footDiameter <= dos
//     ('regulae.gutta.footDiameter') ; footProjection non finie ou > face
//     ('regulae.gutta.footProjection') ; topProjection non finie,
//     > footProjection, ou diametre de tete <= 0 ('regulae.gutta.topProjection') ;
//     sides hors de [4, kGreekMaxGuttaSides] ou non multiple de 4
//     ('regulae.gutta.sides') ; haut de la bande f - 3 au moins a l'arriere du
//     pied des gouttes, bands[f - 3].topProjection >= footProjection -
//     footDiameter ('bands[f - 3].topProjection' : le pied des gouttes poserait
//     sur le ressaut de cette bande) ; envergure des gouttes (somme cumulee des
//     diametres et des vides) >= triglyph.width ('regulae.guttaCount') ; sous
//     AxisTriglyph, width / 2 >= dos ('triglyph.width' : les regulae de deux
//     cotes voisins se toucheraient) ; sous les strategies d'angle,
//     face - width >= dos ('triglyph.width' : regula d'angle degeneree) et
//     gouttes d'angle non disjointes, face - marge >= footProjection -
//     footDiameter ('regulae.guttaGap') ;
//   - avec mutules : band hors de [f + 1, bands.size () - 2] ('mutules.band') ;
//     face de la bande non verticale ('bands[band].topProjection') ; width non
//     finie ou <= 0 ('mutules.width') ; face hors de ]bands[band].projection,
//     bands[band + 1].projection] ('mutules.face'). Le dos n'est compare ni au
//     listel ni a la bande du dessous : y poser est un contact (L14), pas un
//     refus. rise non fini ou < 0 ('mutules.rise') ; rise > 0 et section de
//     l'anneau (GreekEntablatureSection) non simple ('mutules.rise', decide par
//     des produits jamais contractes) ;
//   - avec mutules.guttae (k = band, Db = bands[k].projection, P = face,
//     c = conicity, H = height, Ø_max le plus grand diametre de tete), dans cet
//     ordre : nombre de rangs hors de [1, kGreekMaxGuttae]
//     ('mutules.guttae.rowDiameters', avant toute allocation) ; diametre non fini
//     ou <= 0 (idem) ; rowGaps de taille differente de rangs - 1, ou vide non
//     fini ou <= 0 ('mutules.guttae.rowGaps') ; c non fini, < 0, ou c + c >= le
//     plus petit vide ('mutules.guttae.conicity' : deux pieds voisins se
//     toucheraient) ; perRow hors de [2, kGreekMaxGuttae], ou rangs x perRow
//     > kGreekMaxGuttae ('mutules.guttae.perRow') ; H non fini ou <= 0 ('mutules.guttae.height') ;
//     Ø_max + c + c >= 2 edgeAxis ('mutules.guttae.edgeAxis' : pied au bord du
//     mutule ou au-dela) ; somme cumulee de perRow - 1 copies de Ø_max + c + c
//     >= width - 2 edgeAxis ('mutules.guttae.perRow' : pieds voisins jointifs) ;
//     sides hors de [4, kGreekMaxGuttaSides] ou non multiple de 4
//     ('mutules.guttae.sides') ; garde du pied ('mutules.guttae') : avec
//     M = max (Db, saillies et saillies du haut des bandes 0 a k - 1,
//     triglyph.face, regulae.face), D = P - Db, D' = P - M et S' la somme
//     cumulee de la serie plus c, refus si D' <= 0 ou si non
//     (D'^2 (D^2 + rise^2) > (S' D + H rise)^2) : l'arriere du pied de la
//     derniere goutte reste devant toute bande, tout triglyphe et toute regula
//     du dessous ; puis, avec z_k la cote du pied de la bande k, refus si non
//     (z_k^2 (D^2 + rise^2) > (H D + c rise)^2) : le point le plus bas des
//     pieds reste au-dessus du pied de l'entablement, sous lequel sont les
//     chapiteaux (garde conservatrice, D3-70). Evaluees par
//     StrictCrossDifference : meme verdict sur toute cible.
// Hors la simplicite et la garde du pied, evaluees strictement, ces decisions
// comparent des sommes, des differences ou des copies, sans produit inexact
// (aucune n'est exposee a la contraction FMA).
// Aucune comparaison entre les deux profondeurs : un demi-canal peut etre plus
// profond qu'un canal. Seule la geometrie est contrainte (non-traversee, non
// degenerescence), jamais une proportion. Ces refus sont pris en double ; la
// non-degenerescence apres l'arrondi float des positions est verifiee par
// BuildTriglyphMesh.
void ValidateGreekEntablatureSettings (const GreekEntablatureSettings &settings);

// Section en bandes (EN2), de sens direct dans (u, z), z par CumulativeBounds
// (0, hauteurs). De bas en haut, la face de la bande k va de (projection, z_k)
// a (topProjection, z_k+1) ; ressaut horizontal a z_k+1 jusqu'a la saillie du
// pied de la bande suivante, omis si les deux saillies sont egales ; le sommet
// de jonction est alors unique, et retire si les deux faces sont verticales.
// Puis dessus horizontal jusqu'a -innerFace, face interieure, dessous. Nombre
// de sommets : 2b + 2 - j - v (b bandes, j jonctions a saillies egales, v
// celles dont les deux faces sont verticales). Sous-face inclinee (EN22,
// mutules.rise > 0, k = mutules.band) : le sommet du haut de la bande k est
// remplace par (bands[k].projection, z_k+1 + rise) puis (mutules.face, z_k+1),
// suivi du ressaut vers bands[k + 1].projection a z_k+1, omis si les deux sont
// egales (la jonction k / k + 1 compte alors dans j) ; z_k+1 + rise est
// l'expression du dessus du mutule au dos ; un sommet de plus. Valide d'abord.
std::vector<RingSectionPoint> GreekEntablatureSection (const GreekEntablatureSettings &settings);

// Cote de chaque jonction de bandes : CumulativeBounds (0, hauteurs). Valide
// d'abord.
std::vector<double> GreekEntablatureBandBounds (const GreekEntablatureSettings &settings);

// Metope : intervalle de la frise, sans maillage. CornerTriglyph : triglyphe
// d'angle en une piece en L (EN14), sous les deux strategies d'angle.
enum class FriezeElement { Triglyph, Metope, CornerTriglyph };

// a : coordonnee le long du cote, X sur Front et Back, Y sur Right et Left ;
// d : saillie depuis la ligne des axes du cote, vers l'exterieur ; z depuis le
// pied de l'entablement.
struct FriezePiece
{
	FriezeElement kind = FriezeElement::Triglyph;
	PeristyleSide side = PeristyleSide::Front;
	int    rank = -1;          // rang dans le sens de parcours de PeristyleLayout::columns, 0 a l'angle qui ouvre le cote
	double a0 = 0.0, a1 = 0.0; // a0 < a1 ; CornerTriglyph : bras du cote qu'il ouvre
	double d0 = 0.0, d1 = 0.0; // triglyphe : dos (nu des metopes) et face des bandes ; metope : d0 = d1 = nu des metopes
	double z0 = 0.0, z1 = 0.0; // bornes de la bande de frise
	double ra0 = 0.0, ra1 = 0.0; // CornerTriglyph : bras de retour, coordonnee a du cote precedent ; 0 sinon
};

struct FriezeLayout
{
	std::vector<FriezePiece> triglyphs, metopes;   // par cote dans l'ordre Front, Right, Back, Left, puis par rang
};

// Regle monotriglyphe (EN3). Sur chaque cote de n colonnes, H la demi-distance
// des axes le long du cote, b le nu des metopes, F la face du triglyphe, w sa
// largeur. Valide d'abord les reglages ; refus communs, message prefixe
// "Entablement : " : 'innerFace' si innerFace >= min (halfWidth, halfDepth) ;
// 'triglyph.width' si une metope est vide.
//
// AxisTriglyph (EN4) : un triglyphe centre sur chaque axe, angles compris, et un
// au milieu de chaque travee (EqualParts) ; metopes entre triglyphes
// (GapsBetweenCentred) ; a chaque bout, une metope d'angle du bord du triglyphe
// d'angle au coin du nu des metopes (a = +-(H + b)). Par cote : 2n - 1
// triglyphes et 2n metopes, dont deux d'angle. Refus en plus : 'triglyph.width'
// si width / 2 >= b (les triglyphes d'angle de deux cotes voisins se
// toucheraient).
//
// CornerTriglyphEqualMetopes et CornerTriglyphMidBay (EN14) : par cote, 2n - 2
// placements de triglyphe. Rang 0 : la piece d'angle de l'angle qui ouvre le
// cote, bras [-(H + F), -(H + F) + w] a l'extremite negative du cote (Front,
// Right) ou [(H + F) - w, H + F] a l'extremite positive (Back, Left) ; bras de
// retour par la meme regle a l'extremite fermante du cote precedent, sur sa
// demi-distance. Les deux bouts d'une rangee sont des negations exactes l'un de
// l'autre. Rangs suivants : les axes 1 a n - 2 et un intermediaire par travee.
// Intermediaire d'une travee courante : EqualParts (axe_i, axe_i+1, 2)[1],
// comme sous AxisTriglyph. D'une travee d'angle : sous
// CornerTriglyphEqualMetopes, EqualParts entre le centre du triglyphe voisin et
// le centre du bras d'angle, -(H + F) + w / 2 ou (H + F) - w / 2 (n = 2 : les
// deux centres d'angle) ; sous CornerTriglyphMidBay, l'expression de
// AxisTriglyph. Les placements interieurs sont, au bit pres, ceux de
// AxisTriglyph. 2n - 2 metopes par cote, entre placements consecutifs, la
// derniere jusqu'au bras de retour de la piece d'angle du cote suivant ; aucune
// metope d'angle ; bornes contigues au bit pres.
FriezeLayout LayoutGreekFrieze (const GreekEntablatureSettings &settings, const PeristyleLayout &peristyle);

// Prisme vertical d'un placement Triglyph : section en dents de scie (canaux en
// V, demi-canaux aux bords) sur un fond droit au nu des metopes (d0), de a0 a a1
// (bornes des divisions : somme courante depuis a0, la derniere egale a a1) ;
// 4 sommets par point de section, 8 (m - 1) + 4 triangles pour m points. Plans
// de contact calcules par les expressions de l'anneau : -(halfDepth + d) en
// facade, halfWidth + d sur le flanc droit, etc. Positions du repere du temple
// en double (z = zFoot + z), arrondies une fois en float.
//
// Placement CornerTriglyph (EN14) : prisme vertical d'une section en L de
// P = 2m + 1 sommets ; en coordonnees locales (X, Y), distances aux lignes des
// axes du cote precedent et du cote ouvert comptees vers l'exterieur : dos du
// bras ouvert (F - w, b), profil de ce bras (division d'un triglyphe droit de
// son cote) sans son point d'arete, profil du bras de retour sans son point
// d'arete, dos du retour (b, F - w), coin (b, b). Entre les deux profils, un
// chanfrein commun d'une bande extreme a l'autre : demi-canal plan, de
// profondeur sur chaque face la largeur de la division de bord. Sommets par les
// correspondances (a, d) -> (x, y) de chaque cote, le coin par celle du cote
// ouvert (a = -+(H + b), d = b), donc aux expressions de l'anneau ; couvercles
// par decoupe d'oreilles en double. 2P sommets partages, 4P - 4 triangles.
//
// Valide les reglages ; leve std::invalid_argument, prefixe "Frise : ", pour un
// placement Metope, un placement CornerTriglyph sous AxisTriglyph, un placement
// vide ou non fini, et quand l'arrondi float rendrait le prisme degenere ou au
// contact d'un voisin, ce que la validation en double ne voit pas : fond d'un
// canal ou d'un demi-canal confondu avec le dos (float (H + b) == float (H +
// face - profondeur), H la demi-distance des axes perpendiculaire au cote ; sur
// chaque bras d'une piece d'angle) ; bout d'un triglyphe droit atteignant le
// plan de frise d'un cote voisin (float (a1) >= float (H' + b), H' la
// demi-distance le long du cote, et de meme pour a0) ; deux sommets voisins de
// la section d'angle confondus en float (division de bord d'arete plus fine
// que l'ulp float des coordonnees). Deux refus de la piece d'angle ne sont
// atteints par aucun reglage valide et gardent seulement la construction :
// section non simple dans le repere du temple (la validation l'a verifiee a
// une translation pres) ; decoupe d'oreilles incomplete (un polygone simple a
// toujours une oreille).
std::unique_ptr<Mesh> BuildTriglyphMesh (const GreekEntablatureSettings &settings, const PeristyleLayout &peristyle,
                                         const FriezePiece &triglyph, double zFoot);

// Centre du triglyphe d'angle au-dela de l'axe d'angle : c = face - width / 2
// (README l. 255), quelle que soit la strategie. Valide d'abord.
double CornerTriglyphOffset (const GreekEntablatureSettings &settings);

// Entraxe d'angle qui rend, sous la strategie des reglages, toutes les metopes
// des travees egales a la metope courante spacing / 2 - width (EN16) :
// spacing sous AxisTriglyph ; spacing - CornerTriglyphOffset sous
// CornerTriglyphEqualMetopes (contraction simple) ; aucun sous
// CornerTriglyphMidBay. Rapporte, jamais applique au peristyle. Valide d'abord ;
// refuse, prefixe "Entablement : ", 'spacing' si spacing n'est pas fini ou est
// <= 0, 'triglyph.width' si le resultat est <= 0.
std::optional<double> ComputedCornerSpacing (const GreekEntablatureSettings &settings, double spacing);

struct DoricCornerReport
{
	PeristyleSide side = PeristyleSide::Front;
	double spacing = 0.0, cornerSpacing = 0.0;      // entraxes donnes le long du cote (PeristyleLayout)
	double cornerTriglyphOffset = 0.0;              // centre du bras ouvert de la piece d'angle placee au-dela de
	                                                // l'axe d'angle ; 0 sous AxisTriglyph
	double currentMetope = 0.0;                     // spacing / 2 - width
	double innerMetope = 0.0, outerMetope = 0.0;    // travee d'angle de l'angle qui ouvre le cote, mesurees sur
	                                                // les placements ; outer du cote de l'angle
	double intermediateShift = 0.0;                 // centre de l'intermediaire moins milieu de la travee, > 0 vers l'angle
	std::optional<double> computedCornerSpacing;    // ComputedCornerSpacing (settings, spacing)
	std::optional<double> cornerSpacingGap;         // cornerSpacing - computedCornerSpacing
};

// Un rapport par cote, dans l'ordre Front, Right, Back, Left, mesure sur
// LayoutGreekFrieze (memes refus, et aucun autre) : decalage de la piece
// d'angle, metopes et intermediaire de la travee d'angle par differences de
// bornes des placements ; seules currentMetope et la contraction calculee sont
// des formules independantes du placement. La contraction calculee et l'ecart
// sont absents sous CornerTriglyphMidBay, et sous CornerTriglyphEqualMetopes
// quand spacing <= CornerTriglyphOffset (ComputedCornerSpacing le refuserait).
std::array<DoricCornerReport, 4> ReportDoricCorners (const GreekEntablatureSettings &settings,
                                                     const PeristyleLayout &peristyle);

// Ornements de l'entablement : regulae et leurs gouttes, mutules.
// CornerRegula : regula d'angle en une piece en L (EN18), sous une piece
// d'angle de la frise.
enum class OrnamentElement { Regula, CornerRegula, Mutule };

// a, d, z comme pour FriezePiece.
struct OrnamentPiece
{
	OrnamentElement kind = OrnamentElement::Regula;
	PeristyleSide side = PeristyleSide::Front;
	int    rank = -1;              // regula : rang du placement de triglyphe (0 pour une regula d'angle) ;
	                               // mutule : rang le long du cote, 0 a l'angle qui l'ouvre
	double a0 = 0.0, a1 = 0.0;     // CornerRegula : bras du cote ouvert, jusqu'a -+(H + face de la regula)
	double ra0 = 0.0, ra1 = 0.0;   // CornerRegula : bras de retour, jusqu'a -+(H' + face) ; 0 sinon
	double d0 = 0.0, d1 = 0.0;     // dos, face
	double z0 = 0.0, z1 = 0.0;     // corps
	int    firstGutta = 0, guttaCount = 0;   // plage dans OrnamentLayout::guttae (regula) ou mutuleGuttae (mutule)
	double rise = 0.0;             // mutule : copie de mutules.rise ; dos du dessous a z0 + rise, du dessus a z1 + rise
};

struct GuttaPlacement
{
	PeristyleSide side = PeristyleSide::Front;   // face qui porte la goutte (cote precedent pour le retour)
	double a = 0.0, axis = 0.0;                  // centre le long du cote ; saillie de l'axe
	double z0 = 0.0, z1 = 0.0;                   // pied = bas de la bande friezeBand - 2 ; tete = dessous de la regula
};

// Goutte de mutule (EN23) : centre de sa tete sur le dessous du mutule.
struct MutuleGuttaPlacement
{
	PeristyleSide side = PeristyleSide::Front;
	int    row = 0;                // 0 au devant ; diametre mutules.guttae->rowDiameters[row]
	double a = 0.0;                // centre le long du cote
	double along = 0.0;            // axe, depuis l'arete avant-basse du mutule, le long de la sous-face
};

struct OrnamentLayout
{
	std::vector<OrnamentPiece>        regulae, mutules;   // par cote Front, Right, Back, Left, puis par rang
	std::vector<GuttaPlacement>       guttae;             // gouttes des regulae, dans l'ordre des regulae
	std::vector<MutuleGuttaPlacement> mutuleGuttae;       // dans l'ordre des mutules, rang par rang
};

// Placements des ornements, sur ceux de LayoutGreekFrieze (memes refus, apres
// validation) ; vide si ni regulae ni mutules.
//
// Regulae (EN17) : une par placement de triglyphe, dans l'ordre de
// FriezeLayout::triglyphs, memes a0, a1 (copies) ; d0 = bands[f - 2].projection,
// d1 = regulae.face ; z de z[f - 1] - height a z[f - 1]. Regula d'angle (EN18) :
// bras ouvert de -(H + R) a l'a1 du bras du triglyphe (Front, Right) ou de son a0
// a H + R (Back, Left), R = regulae.face, H la demi-distance le long du cote ;
// retour par la meme regle a l'extremite fermante du cote precedent. Coins
// (R, R) et (dos, dos) aux expressions de l'anneau (L13).
// Gouttes (EN19, EN20) : sous chaque regula droite, et sous chaque bras du
// triglyphe d'une piece d'angle (rien sur le carre [F, R]^2) ; bornes cumulees
// depuis a0 du triglyphe (ou du bras) sur {marge, diametre, vide, ...,
// diametre, marge}, la derniere forcee a a1, marge = (width - envergure) / 2 ;
// centre au milieu de ses bornes ; axe a footProjection - footDiameter / 2 ;
// pied a z[f - 2], tete au z0 de la regula.
// Mutules (EN21) : un centre sur chaque triglyphe droit, chaque bras d'une
// piece d'angle (sur la face qui le porte) et chaque metope, centres au milieu
// des bornes de la piece ; bornes c -+ width / 2 ; d de bands[band].projection
// a mutules.face ; z de z[band] a z[band + 1]. Par cote : 4n - 1 sous
// AxisTriglyph, 4n - 3 sous les strategies d'angle. Refus, prefixe
// "Entablement : ", 'mutules.width' : via (ecart entre deux mutules voisins)
// <= 0 ; mutule extreme d'un cote atteignant le plan des dos des mutules du cote
// voisin (|a| >= H + bands[band].projection). rise : copie de mutules.rise.
// Gouttes des mutules (EN23), avec mutules.guttae : par mutule, firstGutta et
// guttaCount indexent mutuleGuttae ; rang par rang depuis le devant, `along`
// au milieu des bornes CumulativeBounds (0, {Ø0, vide0, ..., Ø(r-1)}) ; `a`
// aux bornes de EqualParts (a0 + edgeAxis, a1 - edgeAxis, perRow - 1). Aucun
// refus de plus.
OrnamentLayout LayoutGreekOrnaments (const GreekEntablatureSettings &settings, const PeristyleLayout &peristyle);

// Une regula avec ses gouttes, ou un mutule, dans le repere du temple (z =
// zFoot + z). Valide les reglages d'abord. Regula droite et mutule : prisme
// droit de 8 sommets et 12 triangles. Regula d'angle : prisme de l'hexagone
// (F - w, dos), (F - w, R), (R, R), (R, F - w), (dos, F - w), (dos, dos), 12
// sommets, 20 triangles, couvercles par decoupe d'oreilles en double ; la
// decoupe de cet ordre de sommets contient la diagonale (dos, dos)-(R, R), et
// toute triangulation la contient si F - w + R < 2 dos.
// Goutte : tronc de cone de N cotes, eventails depuis le centre de chaque
// base, 2N + 2 sommets, 4N triangles ; sommets avant, arriere et lateraux
// poses sans trigonometrie ; tete au z de la regula (meme expression), pied a
// z[f - 2]. Mutule sous sous-face inclinee (EN22) : prisme du parallelogramme
// (d0, z0 + rise), (d1, z0), (d1, z1), (d0, z1 + rise), 8 sommets et
// 12 triangles, ceux du prisme droit quand rise = 0 ; ses gouttes (EN24) :
// avec D = d1 - d0, L = hypot (D, rise), c = D / L, s = rise / L, tete centree
// en (d1 - along c, z0 + along s), pied a `height` selon (-s, -c), anneaux de
// N cotes dans le repere ((c, -s), a), sommets lateraux exacts en a ; rise = 0
// donne c = 1, s = 0 exactement. Positions en double, arrondies une fois en
// float. Leve std::invalid_argument, prefixe "Ornement : ", pour une piece vide
// ou non finie (rise d'un mutule non fini ou < 0 compris), un mutule portant
// des gouttes (guttaCount != 0) sous des reglages sans mutules.guttae, une
// plage de gouttes hors de `ornaments` (guttae pour une regula, mutuleGuttae
// pour un mutule), une goutte de mutule d'un rang hors des reglages, un pied
// de goutte de mutule au contact, en float, du plan M de la garde du pied ou
// du pied de l'entablement ('mutules.guttae', par les positions de l'arriere
// et de l'avant du pied), une regula d'angle sous AxisTriglyph, une regula
// droite au contact, en float, du plan de dos des regulae d'un cote voisin
// (float (a1) >= float (H + d0), H la demi-distance le long du cote, et de meme
// pour a0), une goutte dont l'arriere du pied touche, en float, la face de la
// bande friezeBand - 2 ou le haut de la bande friezeBand - 3 (float (H' +
// footProjection - footDiameter) non strictement au-dela de float (H' + d), H'
// la demi-distance perpendiculaire au cote de la goutte ;
// 'regulae.gutta.footDiameter', 'bands[f - 3].topProjection'), et quand
// l'arrondi float confondrait deux sommets voisins.
std::unique_ptr<Mesh> BuildOrnamentMesh (const GreekEntablatureSettings &settings, const PeristyleLayout &peristyle,
                                         const OrnamentLayout &ornaments, const OrnamentPiece &piece, double zFoot);

// Toutes les pieces de l'entablement hors anneau, apres une seule validation
// des reglages (D3-63), dans l'ordre des parties du temple : un prisme par
// placement de FriezeLayout::triglyphs (BuildTriglyphMesh), puis une piece par
// regula avec ses gouttes, puis une par mutule (BuildOrnamentMesh). Memes
// maillages et memes refus que ces fonctions, piece par piece. `afterEach`,
// s'il est fourni, est appele sur chaque piece des qu'elle est construite,
// avant la suivante ; une exception qu'il leve arrete la construction et
// remonte telle quelle ; aucune piece n'est alors rendue.
std::vector<std::unique_ptr<Mesh>> BuildEntablaturePieceMeshes (const GreekEntablatureSettings &settings,
                                                                const PeristyleLayout &peristyle,
                                                                const FriezeLayout &frieze,
                                                                const OrnamentLayout &ornaments, double zFoot,
                                                                const std::function<void (const Mesh &)> &afterEach = {});

} // namespace classical
} // namespace cgarchi
