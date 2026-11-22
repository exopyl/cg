#pragma once

// En-tete interne des ornements du chapiteau corinthien, hors de l'API
// publique : cotes lues dans les membres d'ornement du chapiteau, et
// instruments de mesure d'un maillage d'ornement contre ces cotes.
//
// Hauteur h : en parties, comptee depuis le pied du premier membre du bloc
// chapiteau (« hauteur depuis le bas du chapiteau » des tables du JSON). Rayons
// en parties depuis l'axe de la colonne. Un maillage est lu dans le repere du
// tour : axe Z, z = y du profil fois `scale`.

#include <array>
#include <memory>
#include <vector>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_template.h>
#include <cgarchi/classical_orders.h>
#include <cgarchi/classical_profile.h>

namespace cgarchi
{
namespace classical
{
namespace detail
{

// Membre d'ornement (saillie CoreEnvelope) : hauteurs et saillies du noyau et
// de l'enveloppe, depuis distance_axe du bloc.
struct CapitalEnvelopeBand
{
	double h0 = 0.0, h1 = 0.0;
	Span   core;
	Span   envelope;
};

struct CapitalEnvelope
{
	double axisDistance = 0.0;              // distance_axe du chapiteau : nu superieur du fut
	double foot = 0.0;                      // y du profil du pied du chapiteau
	double scale = 1.0;                     // unites monde par partie, celles du maillage mesure
	std::vector<CapitalEnvelopeBand> bands; // de bas en haut, contigus en h et en saillie
	Ring   abacus;                          // anneau du membre « B · abaco »
	Ring   abacusOutline;                   // arete superieure de l'abaque construit : anneau du sommet le plus haut de « A · cimatio dell'abaco (talon) »

	// Interpolation lineaire dans la bande qui contient h ; hors de [h0 du
	// premier, h1 du dernier], prolongement de la bande extreme.
	double CoreRadius (double h) const;
	double ConeRadius (double h) const;
	// Borne du feuillage : max (noyau, min (cone, contour en plan de l'abaque)).
	// Le noyau la releve la ou la levre de la corbeille passe la face concave
	// de l'abaque (h >= 30,85 sur l'axe de face, >= 34,45 a 22,5 degres).
	double EnvelopeRadius (double h, double theta) const;
	// Borne des feuilles F et E : max (noyau, cone), sans l'abaque. La saillie
	// des feuilles s'arrete a la droite tiree de la corne de l'abaque a
	// l'astragale (Vignole, pl. XXV) ; l'abaque des donnees ne borne que C ; les
	// volutes sont bornees en plan par l'arete superieure de l'abaque construit
	// (VoluteSpec::planBound).
	double LeafBoundRadius (double h) const;
	double BandBottom () const { return bands.front ().h0; }
	double BandTop () const { return bands.back ().h1; }
};

// Lit les membres d'ornement du bloc chapiteau de `order` (saillie
// CoreEnvelope) et leur place dans `profile` (memes noms, bloc « chapiteau »),
// puis l'anneau de l'abaque (B) et celui de l'arete superieure de l'abaque
// construit (sommet le plus haut du talon de A) dans les sommets du profil.
// `scale` : celle du maillage que les instruments mesureront.
// Leve std::invalid_argument si le chapiteau, un membre d'ornement, l'abaque ou
// le talon de A manque au profil, si un membre y figure plusieurs fois, si les membres ne
// sont pas contigus en hauteur et en saillie (noyau et enveloppe), ou si
// `scale` n'est pas fini et > 0.
CapitalEnvelope ReadCapitalEnvelope (const OrderData &order, const ColumnProfile &profile, double scale = 1.0);

// Mesures sur les sommets [vertexBegin, vertexEnd) de `mesh`. Echantillonnage
// aux sommets : une face qui traverse la surface entre ses sommets n'est pas
// vue (CoreClearance est un majorant du jeu, EnvelopeExcess un minorant du
// debord). Levent std::invalid_argument si la plage sort du maillage ou si
// un sommet ne s'y lit pas.
//
// min (r - CoreRadius (h)) : negatif si un sommet entre dans la corbeille ;
// +infini sur une plage vide.
double CoreClearance (const Mesh &mesh, const CapitalEnvelope &env, unsigned vertexBegin, unsigned vertexEnd);
double CoreClearance (const Mesh &mesh, const CapitalEnvelope &env);
double CoreClearance (const Mesh &mesh, const CapitalEnvelope &env, const CapitalPiece &piece);

// max (r - EnvelopeRadius (h, theta)) : positif si un sommet sort de la borne ;
// -infini sur une plage vide. Surcharge par piece, borne du rang : F, E, D,
// volutes et helices contre LeafBoundRadius (h) (cone sans l'abaque ; D, volutes
// et helices posees sur leurs cotes, garde sur le cone, registre AC-2), les autres rangs contre
// EnvelopeRadius (h, theta) ; un fleuron (pose sur l'abaque, hors de la
// borne) rend -infini.
double EnvelopeExcess (const Mesh &mesh, const CapitalEnvelope &env, unsigned vertexBegin, unsigned vertexEnd);
double EnvelopeExcess (const Mesh &mesh, const CapitalEnvelope &env);
double EnvelopeExcess (const Mesh &mesh, const CapitalEnvelope &env, const CapitalPiece &piece);

// Part de la bande [hLo, hHi] de la grille (theta, h) de thetaCells x hCells
// cellules que recouvre la projection angulaire des faces de `pieces` : chaque
// sommet est projete sur (theta, h), son rayon r est ignore. Une cellule est
// couverte si son centre tombe dans la projection d'une face, cotes droits en
// (theta, h) (biais d'ordre de la courbure de la face, nul a la limite des
// petites faces) ; une face vue par la tranche ne couvre rien. Une face
// interieure a la corbeille ou eloignee d'elle compte comme une face posee
// dessus : la mesure dit ce qu'un rayon horizontal issu de l'axe traverse, pas
// ce qui est visible de l'exterieur ; CoreClearance borne a part la position
// radiale.
// Leve std::invalid_argument si hLo >= hHi, si une dimension est < 1, si la
// plage de faces d'une piece sort du maillage, si une face n'est pas un
// triangle ou si un de ses sommets ne se lit pas.
double BellCoverage (const Mesh &mesh, const std::vector<CapitalPiece> &pieces, const CapitalEnvelope &env,
                     double hLo, double hHi, int thetaCells = 360, int hCells = 48);

// Part visible de la corbeille dans l'elevation de face (theta = 0 vers
// l'observateur, projection orthographique le long de -x), fenetre |y| <=
// halfWidth, h dans [hLo, hHi], en cellules carrees de `cell` parties : une
// cellule est visible si aucune face de `pieces` ne la couvre devant le noyau
// (x de la face > x du noyau, sqrt (CoreRadius (h)^2 - y^2)). Une face derriere
// le noyau ne cache rien. Grandeur de meme projection que la part de corbeille
// tracee sur la pl. XXVI (V-1, registre AC-7).
// Leve std::invalid_argument si la fenetre est vide, si `cell` n'est pas > 0,
// si la fenetre deborde la silhouette du noyau (halfWidth >= CoreRadius), si la
// plage de faces d'une piece sort du maillage, si une face n'est pas un
// triangle ou si un de ses sommets ne se lit pas.
double VisibleBellFraction (const Mesh &mesh, const std::vector<CapitalPiece> &pieces, const CapitalEnvelope &env,
                            double halfWidth, double hLo, double hHi, double cell = 0.05);


// ---------------------------------------------------------------------------
//  Feuille d'acanthe seule
// ---------------------------------------------------------------------------
//
// Une feuille est un gabarit plan (LeafTemplate, compile depuis le dessin)
// plaque sur une porteuse (LeafBend) : coque fermee de deux faces decalees de
// la surface moyenne S et de murs sur le bord.

// Forme d'une feuille, en parties. Valeurs par defaut : DefaultLeafSpec
// (registre AC-8, AC-9 et AC-12 de classical_orders_implementation.md).
struct LeafSpec
{
	OrnamentDetail detail = OrnamentDetail::Light;
	LeafStyle      style = LeafStyle::Olive;
	double thickness = 0.2;      // t : epaisseur de chaque coque
	double clearance = 0.05;     // c : jeu entre la face interieure et le noyau
	double margin = 0.05;        // retrait de la face exterieure sous la borne
	double gutter = 0.2;         // gouttiere du corps : ecart au noyau retire a l'axe, fraction
	double reliefShare = 0.8;    // psi : part de la place libre le long de la normale que le relief peut prendre
	double reliefScale = 1.0;    // facteur sur H et Hb du gabarit, avant le plafond
};

// Valeurs du registre pour un niveau et un style.
LeafSpec DefaultLeafSpec (OrnamentDetail detail, LeafStyle style);

// Pose d'une feuille sur la corbeille, lue par LeafBend.
struct LeafPlacement
{
	double theta = 0.0;           // axe de la feuille (rad)
	double hFoot = 0.0;           // pied du feuillage, h depuis le pied du chapiteau
	double hTop = 0.0;            // crete du bec (surface moyenne)
	double hStem = 0.0;           // pied de la tige ; < hFoot : bande de tige de hStem a hFoot
	double halfWidth = 0.0;       // hw (p) : X = u hw
	double bendRadius = 0.0;      // rayon de l'extrados du bec (p), pl. XXV
	double tipTurn = 0.0;         // flexion totale du bec, de la verticale montante a la pointe (rad)
	double footLean = 0.0;        // inclinaison du corps au pied, vers l'exterieur (rad)
	double footOffset = 0.0;      // ecart off au pied du corps (p) ; > 0 seulement sans bande de tige
	double axisLean = 0.0;        // inclinaison de l'axe dans le plan tangent (rad), signe : sens des theta croissants
	bool   explicitBend = false;  // bec pose sur bendCenter au lieu du contact avec la borne
	std::array<double, 2> bendCenter {};   // centre (r, h) de l'arc du bec en pose explicite (p)
	double bodyHold = 0.0;        // > 0 : corps tenu vertical a footOffset jusqu'a cette hauteur, puis deux coudes (pose explicite)
};

// Cote d'une feuille D par rapport a sa tige de caulicole : Inner vers l'axe de
// face (sous l'helice), Outer vers la corne (sous la volute).
enum class MinorLeafSide { Inner, Outer };

// Pose par defaut d'une feuille D de la tige d'axe stemTheta (cotes de la
// pl. XXVI et du plan de la pl. XXV, registre AC-53 et AC-57) : pied sur le dessus de la bague (h 17,87 : haut
// de la bague + c + 0,02 pour le relief du bord, footOffset 0,5 p), bec en pose explicite ;
// D interieur vers l'axe de face le plus proche (hw 2,4), D exterieur vers la
// corne (hw 2,3), corps tenu le long de la corbeille jusqu'a h 23,1 (au plus
// 1 p de la corbeille jusqu'a h 24,3). Pied decale de la tige vers son cote :
// le contour du corps reste a c / r au moins du plan axial de la tige ;
// axe cisaille, ancre au pied, alpha deduit pour que l'apex tombe a
// stemTheta -+ 17,0 degres (Inner) ou +- 19,0 degres (Outer). Lit `env`
// (place du bec, rayon de l'apex). Pose calculee sur le niveau Light, commune
// aux deux niveaux (meme contour, meme metrique). `iterations`, s'il n'est pas
// nul, recoit le nombre de pas du point fixe. Leve std::invalid_argument si
// stemTheta n'est pas fini, si le point fixe ne converge pas en 50 pas (ecart
// >= 1e-9 rad), ou comme LeafBend.
LeafPlacement DefaultMinorLeafPlacement (const CapitalEnvelope &env, MinorLeafSide side, double stemTheta,
                                         int *iterations = nullptr);

// Poses par defaut des quatre feuilles D representantes, indice 2 k + cote
// (k = 0, 1 : tiges a RankTheta (Cauliculus, k) ; cote 0 Inner, 1 Outer),
// egales a DefaultMinorLeafPlacement (env, cote, RankTheta (Cauliculus, k)).
// Le point fixe ne lit que les cotes de `env` en parties : les poses sont
// gardees en cache, sous verrou, avec pour cle le contenu de `env` hors
// `scale` et `foot` (distance a l'axe, bandes, anneau de l'abaque). L'enveloppe
// est validee a chaque appel, avant la lecture du cache. Leve
// std::invalid_argument comme DefaultMinorLeafPlacement.
std::array<LeafPlacement, 4> DefaultMinorLeafPlacements (const CapitalEnvelope &env);

// Forme par defaut d'une feuille D : gabarit OliveMinor, relief reduit pour la
// garde de courbure du petit bec et du coude du corps tenu (Inner 0,35, Outer 0,30).
LeafSpec DefaultMinorLeafSpec (OrnamentDetail detail, MinorLeafSide side);

// Pieces par rang (F, E, tiges des caulicoles) de BuildCorinthianOrnament.
constexpr int kLeavesPerRank = 8;
// Fleurons G de BuildCorinthianOrnament, un par face de l'abaque.
constexpr int kFleurons = 4;

// Axe de la piece k d'un rang, registre AC-45 : F et tiges des caulicoles
// pi/8 + k pi/4, E k pi/4. Leve std::invalid_argument pour un autre rang ou k
// hors de [0, kLeavesPerRank).
double RankTheta (CapitalPieceKind kind, int k);

// Pose par defaut d'un rang : F (LowerLeaf) et E (MiddleLeaf), registre AC-40,
// AC-41 et AC-49 ; footLean 25 degres pour F, 0 pour E : le corps de E part tangent a sa bande de tige,
// verticale (un coin concave a la jonction plierait la face exterieure). Les feuilles
// reposent sur l'astragale : pied de F a 0,70 p, majorant de (t/2 + H) sin 25 + c
// pour le relief de la cote (H 1,40) ; bande de E a partir de c (0,05 p). Leve
// std::invalid_argument pour un autre rang ou theta non fini.
LeafPlacement DefaultLeafPlacement (CapitalPieceKind kind, double theta);

// Porteuse d'une feuille sur la corbeille, meridien d'axe dans le plan (off, h),
// off = r - Base (h), Base (h) = noyau + clearance + t/2 :
//   - bande de tige (si hStem < hFoot) : off = 0, de hStem a hFoot ;
//   - corps : Bezier cubique du pied (0, hFoot), incline de footLean, au depart
//     du bec P_b (off_b, h_b), vertical ; controles a 0,4 (h_b - hFoot) ;
//   - bec : arc de rayon R_S = bendRadius - t/2 - Hb_ref, de la verticale
//     montante (phi = 0) a phi = tipTurn, crete a hTop, centre (off_b + R_S, h_b).
// off_b est le plus grand ecart tel que l'extrados de l'arc (revers, a
// t/2 + Hb_ref de S) reste sous LeafBoundRadius - margin ; s'il est negatif, R_S
// est reduit par pas de 2 %. Hb_ref = backHeight du gabarit.
// Pose explicite (explicitBend) : centre de l'arc en bendCenter (r, h), d'ou
// h_b = h et off_b = r - R_S - Base (h) ; hTop n'est pas lu ; l'extrados doit
// rester sous LeafBoundRadius - margin (garde « pose explicite au-dessus de la
// borne »). Le corps part de (footOffset, hFoot). Axe incline (axisLean = a) :
// le meridien u est enroule a theta + tan a S (y) / r_0 (y) + X / (cos a r),
// S (y) longueur developpee du meridien d'axe depuis le pied, r_0 (y) son rayon ;
// a = 0 laisse l'enroulement theta + X / r inchange.
// Corps tenu (bodyHold = h_t > 0, pose explicite et footLean nul) : le corps
// est une droite verticale de (footOffset, hFoot) a (footOffset, h_t), un coude
// d'un quart de tour vers l'exterieur, un palier horizontal et un coude d'un
// quart de tour vers le haut jusqu'au depart du bec, rayons (h_b - h_t) / 2 ;
// garde « corps tenu » si le palier serait negatif. bodyHold = 0 : Bezier.
// Repere du gabarit : y < 0 bande, [0, 1] corps (abscisse curviligne), [1, 2]
// arc (angle) ; X = u hw ; enroulement phi = theta + X / r.
// Point le plus serre de la garde de courbure d'une porteuse : marge = radius -
// need, need = 2 t + H, au meridien u, abscisse y.
struct LeafCurvatureMargin
{
	double margin = 0.0, radius = 0.0, need = 0.0, u = 0.0, y = 0.0;
};

// Rampes du dome et de la gouttiere : smootherstep (6 t^5 - 15 t^4 + 10 t^3,
// classe C2) de y_d a y_crete, y_d = fin de l'effacement du relief du gabarit
// (fade[1], au moins 1) : dome et relief plein ne se cumulent pas.
// Dome de calotte sur l'arc : deplacement de kappa u_cap hw (u / u_cap)^2 rampe
// vers le centre de l'arc (kappa 0,2, registre AC-43).
// Gouttiere du corps (registre AC-9) : off multiplie par 1 - w g (1 - ue^2)^2,
// w = 1 jusqu'a y_d, puis 1 - rampe.
// Garde de courbure (registre AC-8) : sur les meridiens du corps (u = 0 a
// u_corps par quarts, y dans [0, 1]) et de l'arc (u = 0, u_cap / 2, u_cap, y
// dans [1, 2]), rayon de courbure en forme close >= 2 t + H, H = relief plaque
// maximal de la tranche de y (CurvatureMargin) ; un coin (tangentes distinctes)
// au depart du bec ou, avec bande, au pied du feuillage vaut un rayon nul.
// Base est lineaire par bandes de l'enveloppe : la garde lit la pente de la
// bande au point, sans les cassures entre bandes.
// Leve std::invalid_argument (« BuildLeafSheet : » ...) si l'enveloppe, la pose
// ou la forme sortent de leur domaine (clearance >= 0 compris), si le gabarit
// n'a aucun sommet sur le bec (y > 1) ou efface son relief au-dela de la crete,
// si aucune place ne reste pour le bec sous la borne, si le bec passe sous le
// pied, ou si la garde de courbure refuse un meridien (« rayon de courbure »).
class LeafBend
{
public:
	static constexpr double kDome = 0.2;
	static constexpr int kCurvatureSamples = 16;   // echantillons par intervalle lisse avant l'affinage

	// env est garde par reference : il doit survivre a la porteuse. spec et
	// placement sont copies.
	LeafBend (const CapitalEnvelope &env, const LeafSpec &spec, const LeafPlacement &placement,
	          const LeafTemplate &tpl);
	LeafBend (CapitalEnvelope &&, const LeafSpec &, const LeafPlacement &, const LeafTemplate &) = delete;
	LeafBend (const CapitalEnvelope &, const LeafSpec &, const LeafPlacement &, LeafTemplate &&) = delete;

	double BendRadius () const { return radius_; }       // R_S, surface moyenne (p)
	double TipTurn () const { return placement_.tipTurn; }
	double BendStartOffset () const { return offB_; }   // off_b
	double BendStartHeight () const { return hB_; }     // h_b
	double StemLength () const { return placement_.hFoot - placement_.hStem; }
	double BodyLength () const { return bodyLength_; }
	double BendLength () const { return radius_ * placement_.tipTurn; }
	double CrestY () const { return 1.0 + 0.5 * kPiValue / placement_.tipTurn; }
	double BodyHalfWidth () const { return bodyHalfWidth_; }   // |u| maximal du gabarit sur le corps
	double GutterDepth () const { return gutter_; }
	double Base (double h) const;

	// Point de S au meridien u, abscisse y, dans le plan (r, h), dome compris.
	std::array<double, 2> Meridian (double u, double y) const;
	// Normale unitaire de S dans le plan (r, h), cote revers (vers le noyau sur
	// le corps, hors du centre sur l'arc). Sans bande de tige, y est ramene a
	// [0, 2] ; avec, a [-1, 2].
	std::array<double, 2> BackNormal (double u, double y) const;
	// Point 3D de S (repere du tour en parties, sans le pied) : enroulement
	// theta + u hw / r.
	std::array<double, 3> At (double u, double y) const;
	// Plus grand d dans [0, min (reach, 40)] tel que s + d dir reste sous
	// LeafBoundRadius - margin - t/2 (plafond bilateral : dir = +n_S pour H, -n_S
	// pour Hb). La recherche s'arrete a la portee : un appelant qui plafonne une
	// valeur x par psi Free passe reach = x / psi ; une portee infinie vaut 40.
	double Free (const std::array<double, 3> &s, const std::array<double, 3> &dir, double reach = 40.0) const;
	// Plus grand d dans [0, min (reach, 40)] tel que s + d dir reste au-dessus
	// de CoreRadius + clearance + t/2 (plafond du revers cote noyau) ; 0 si s
	// n'y est pas deja.
	double CoreRoom (const std::array<double, 3> &s, const std::array<double, 3> &dir, double reach = 40.0) const;
	// Garde de courbure (voir la classe) : sur chaque intervalle ou la courbure et
	// H sont lisses (bornes : tranches de H, debut de rampe, crete, raccords),
	// `samples` echantillons, puis affinage du maximum de courbure par section
	// doree. Le resultat ne depend pas de `samples` des que le maximum de chaque
	// intervalle est isole. Leve std::invalid_argument si samples < 2.
	LeafCurvatureMargin CurvatureMargin (int samples = kCurvatureSamples) const;
	// Courbure (1/p) du meridien u en y, en forme close, celle que lit la garde ;
	// 0 la ou la derivee du meridien est nulle.
	double MeridianCurvature (double u, double y) const;

private:
	static constexpr double kPiValue = 3.14159265358979323846;
	using Jet = std::array<std::array<double, 2>, 3>;   // point, derivees premiere et seconde en y
	Jet PlaneJet (double y) const;                       // (off, h) du meridien d'axe, sans dome
	double Developed (double y) const;                   // longueur developpee du meridien d'axe depuis le pied
	Jet MeridianJet (double u, double y) const;           // (r, h), dome et gouttiere compris
	std::array<double, 3> Dome (double u, double y) const;     // deplacement et ses derivees en y
	std::array<double, 3> Gutter (double u, double y) const;   // facteur sur off et ses derivees en y
	double CoreSlope (double h) const;                   // d Base / dh dans la bande de h
	double ReliefAt (double y) const;                    // H plaque maximal de la tranche de y et de ses voisines

	const CapitalEnvelope &env_;
	LeafSpec               spec_;
	LeafPlacement          placement_;
	double radius_ = 0.0, offB_ = 0.0, hB_ = 0.0, bodyLength_ = 0.0, capHalfWidth_ = 1.0, domeStart_ = 1.0;
	double gutter_ = 0.0, bodyHalfWidth_ = 0.0;
	double holdRadius_ = 0.0, holdRun_ = 0.0;             // corps tenu : rayon des deux coudes, palier horizontal
	std::array<std::array<double, 2>, 4> bezier_ {};
	std::vector<double> bodyT_, bodyS_;                   // parametre et abscisse curviligne cumulee du corps
	std::array<double, 64> reliefBins_ {};                // H plaque maximal par tranche de y sur [0, 2]
};

// Feuille plaquee en un maillage. Sommets : face exterieure [0, faceVertices),
// face interieure [faceVertices, 2 faceVertices), meme ordre. Triangles : face
// exterieure [0, faceTriangles), face interieure [faceTriangles,
// 2 faceTriangles), puis murs du bord.
struct LeafSheet
{
	std::unique_ptr<Mesh> mesh;
	std::vector<std::array<double, 4>> planar;   // par sommet d'une face : X = u hw (p), y, H et Hb appliques (p)
	std::vector<int> faceRegion;                 // par triangle : region du gabarit, -1 pour un mur du bord
	unsigned faceVertices = 0;
	unsigned faceTriangles = 0;
	unsigned stemVertices = 0;                   // sommets d'une face dans la bande de tige (y < 0)
};

// Garde de pli d'une face decalee de S : chaque triangle de `face` doit avoir,
// par rapport a la normale n_S moyenne de ses sommets (`normals`), l'orientation
// du meme triangle pris sur `surface` ; un triangle retourne a ete replie par
// le decalage. `planar` (X, y, H, Hb par sommet) nomme le lieu du pli. Leve
// std::invalid_argument (« pli de la face `faceName` ») au premier triangle
// retourne. Avec le plafond de Hb cote noyau et la garde de courbure de
// LeafBend (2 t + H), un gabarit valide ne la declenche pas : elle tient la
// coque si ces gardes changent.
void CheckFaceFold (const std::vector<std::array<double, 3>> &face, const std::vector<std::array<double, 3>> &surface,
                    const std::vector<std::array<unsigned, 3>> &triangles, const std::vector<std::array<double, 3>> &normals,
                    const std::vector<std::array<double, 4>> &planar, const char *faceName);

// Feuille plaquee du gabarit `tpl` sur la porteuse LeafBend, repere du tour
// (z = (foot + h) * env.scale). Feuille seule si hStem >= hFoot (prefixe du
// gabarit), feuille et bande de tige sinon. Demi-feuille du gabarit et son
// miroir u -> -u, cousus sur l'axe ; faces a S + (t/2 + H) n_S et
// S - (t/2 + Hb) n_S, n_S normale exterieure de S (cote oppose au revers),
// H = reliefScale H du gabarit plafonne a reliefShare fois la place libre le
// long de +n_S, Hb de meme le long de -n_S (plafond bilateral, LeafBend::Free)
// et a la place au-dessus du noyau (LeafBend::CoreRoom) ; murs sur le bord
// hors couture ; coque orientee de volume positif.
// Leve std::invalid_argument comme LeafBend, si le gabarit n'est pas celui du
// style et du niveau de `spec`, si reliefScale n'est pas fini et >= 0, si
// reliefShare sort de ]0, 1], si un relief du gabarit est negatif, si un
// triangle d'une face est retourne par rapport a S (« pli »), ou si un sommet
// descend sous h = clearance (« pied sous le plancher du chapiteau ») : la
// feuille repose sur le haut du fut sans y entrer.
LeafSheet BuildLeafSheet (const CapitalEnvelope &env, const LeafSpec &spec, const LeafPlacement &placement,
                          const LeafTemplate &tpl);


// ---------------------------------------------------------------------------
//  Tige d'un caulicole (C)
// ---------------------------------------------------------------------------
//
// Tube droit cannele d'axe vertical, au rayon axisRadius et a l'azimut theta,
// de hFoot a hHead, ferme par deux disques ; bague de deux cordons au haut de
// la tige. Valeurs par defaut : DefaultCauliculusSpec (registre AC-50, AC-52 et
// AC-56 ; cotes de la pl. XXVI).
struct CauliculusSpec
{
	OrnamentDetail detail = OrnamentDetail::Light;
	double axisRadius = 16.9;        // r_C : rayon de l'axe de la tige depuis l'axe de la colonne (p)
	double radius = 1.0;             // R : rayon du fut de la tige
	int    flutes = 8;               // N cannelures
	double fluteWidth = 0.55;        // corde d'une cannelure, au rayon R
	double fluteDepth = 0.15;        // fleche du fond, arc de cercle
	double fluteFade = 0.3;          // effacement des cannelures sous la bague (smootherstep)
	double hFoot = 12.40;            // pied : crete extrados de F (12,35) + c
	double hHead = 17.8;             // tete : haut de la bague
	double lowerCordBottom = 15.3;   // cordon bas de lowerCordBottom a gorge
	double gorge = 16.6;             // gorge entre les cordons (rayon R), cordon haut de gorge a hHead
	double lowerCordRadius = 1.2;
	double upperCordRadius = 1.55;
	double clearance = 0.05;         // c : jeu minimal au noyau et au-dessus de la crete de F
	int    samplesPerFlute = 3;      // s : n_phi = N s
	int    fadeRings = 3;            // anneaux de la rampe d'effacement
	int    cordRings = 5;            // anneaux par cordon
};

// Valeurs du registre : Light s = 3, rampe 3, 5 anneaux par cordon ; Heavy
// s = 6, rampe 6, 12 anneaux par cordon. Leve std::invalid_argument pour None
// ou un niveau hors de l'enumeration.
CauliculusSpec DefaultCauliculusSpec (OrnamentDetail detail);

// Rayon de la section de la tige, cannelures exclues : R sur le fut, cordons en
// demi-tores aplatis (R + (R_cordon - R) (1 - |2 t - 1|^3)^(1/3), t dans
// [0, 1] le long du cordon).
double CauliculusRadius (const CauliculusSpec &spec, double h);

// Hauteurs des anneaux de la grille, de bas en haut : pied, debut de la rampe,
// fadeRings anneaux de la rampe (le dernier au bas du cordon), cordRings par
// cordon (le dernier a la gorge, puis a la tete). n_z = 2 + fadeRings + 2 cordRings.
std::vector<double> CauliculusRings (const CauliculusSpec &spec);

// Tige en un maillage, repere du tour (z = (foot + h) * env.scale). Sommets :
// n_phi n_z de la grille, anneau par anneau, angle local alpha_i = 2 pi i / n_phi
// compte depuis la direction radiale exterieure (une cannelure centree en
// alpha = 0), puis le centre du disque du pied et celui de la tete. Triangles :
// 2 n_phi (n_z - 1) de la paroi, puis n_phi par disque ; coque orientee de volume
// positif. Section r (alpha, h) = CauliculusRadius (h) - fluteDepth w (h) g (alpha),
// w = 1 sur le fut, 0 sur la bague (smootherstep sur fluteFade).
// Leve std::invalid_argument (« BuildCauliculusShell : » ...) si la forme sort
// de son domaine, si la tige entre dans la corbeille (r_C - rayon maximal <
// noyau + c), ou si le pied est sous la crete extrados de F plus c (« pied de
// tige sous la crete de F »).
struct CauliculusSheet
{
	std::unique_ptr<Mesh> mesh;
	unsigned angularSamples = 0;   // n_phi
	unsigned rings = 0;            // n_z
};
CauliculusSheet BuildCauliculusSheet (const CapitalEnvelope &env, const CauliculusSpec &spec, double theta);

// ---------------------------------------------------------------------------
//  Volutes (C : volute d'angle), registre AC-59 a AC-63
// ---------------------------------------------------------------------------
//
// Une volute par tige de caulicole, sous la corne la plus proche. Representante
// de la tige 0 (RankTheta (Cauliculus, 0) = pi/8), corne a pi/4 :
//   - spirale : contour exterieur de la construction a 12 centres de Vignole
//     (BuildTwelveCentreVolute, raccord CentreLine) pour un oeil de diametre
//     2 k et un sommet a 9 k du centre (homothetie k des cotes ioniques) ;
//   - plan vertical de direction pi/2 - planeAngle (planeAngle depuis le plan
//     de la face), passant par l'oeil (eyeRadius, eyeTheta, eyeHeight) ; u vers
//     la corne, v vers le haut, n = normale du plan vers la face ;
//   - ruban : bord exterieur sur le contour, bord interieur decale vers le centre
//     de l'arc de w (phi) = min (loi des tours, |O| - k - c), loi lineaire en
//     angle entre turnWidths (sommets des tours 1, 2, 3), constante ensuite ;
//     arret a la premiere station ou w < minWidth ;
//   - section : dos plat a -depth/2, face vue a +depth/2 portant deux bourrelets
//     (demi-sinus de hauteur beadHeight w sur beadShare de la largeur) et une
//     rainure centrale (grooveShare de la largeur, profondeur grooveDepth) ;
//   - tige : nee sur la corbeille a stemTheta (decalage depuis l'axe de face),
//     plus bas point a stemFoot, centre a noyau + c + depth/2 + stemLift, epaisseur
//     stemThin depth au pied, rendue a depth par smootherstep sur stemThinLength de
//     longueur developpee (face interieure fixe), a plat (n normale
//     exterieure du noyau) jusqu'a stemBend, puis Hermite cubique G1 jusqu'a la
//     premiere station de la spirale (tangente u) ; repere a rotation minimale
//     depuis stemBend et torsion repartie (smootherstep) pour rejoindre celui de
//     la spirale ;
//   - oeil : disque de rayon k, epaisseur depth + 0,1, coque a part ;
//   - borne en plan (planBound) : tout sommet a r <= R (theta) - c, R l'anneau de
//     l'arete superieure de l'abaque construit (CapitalEnvelope::abacusOutline).
// La representante de la tige 1 (3 pi/8) est l'image miroir par le plan
// theta = pi/4. Toutes les cotes sont en parties, h depuis le pied du chapiteau.
struct VoluteSpec
{
	double scale = 0.470;                            // k
	double eyeRadius = 0.0;                          // R de l'oeil
	double eyeTheta = 0.0;                           // azimut de l'oeil (rad), tige 0
	double eyeHeight = 31.5;
	double planeAngle = 0.0;                         // depuis le plan de la face (rad)
	std::array<double, 3> turnWidths {1.43, 0.70, 0.60};
	double minWidth = 0.15;
	double depth = 1.0;                              // b
	double clearance = 0.05;                         // c
	double beadShare = 0.35, beadHeight = 0.175;     // bourrelet : part de la largeur, hauteur / w ; 2 beadShare + grooveShare = 1
	double grooveShare = 0.30, grooveDepth = 0.15;   // rainure : part de la largeur, profondeur (p)
	double eyeExtraDepth = 0.1;
	double stemTheta = 0.0;                          // azimut de la tige sur la corbeille (rad), tige 0
	double stemFoot = 24.5;                          // plus bas point de la tige
	double stemBend = 27.5;                          // fin du trace a plat sur la corbeille
	double stemLift = 0.0;                           // ecart de la tige au noyau au-dela de c + depth/2
	double stemThin = 1.0;                           // epaisseur au pied / depth (face interieure fixe)
	double stemThinLength = 1.0;                     // longueur developpee de la rampe d'epaisseur (p)
	double stemTangents = 0.5;                       // longueur des tangentes d'Hermite / corde
	double maxTwistRate = 0.5;                       // rad / p
	int samplesPerQuarter = 3;                       // stations de la spirale par quart de tour
	int bellSegments = 4, bendSegments = 10;         // stations de la tige
	int beadSegments = 2, grooveSegments = 2;        // segments de la face vue
	int eyeSegments = 12;
	bool planBound = false;                          // borne en plan sous l'arete superieure de l'abaque, marge c
};

// Valeurs du registre (AC-59 a AC-63 ; pose de l'oeil AC-81) pour un niveau :
// oeil R 27,2, theta 40 degres, h 31,5, plan a 22,5 degres de la face, borne en
// plan ; Light 3 stations
// par quart, tige 4 + 10, face vue 2 + 2 + 2, oeil 12 ; Heavy 8, 8 + 20, 4 + 4 + 4, 32.
VoluteSpec DefaultVoluteSpec (OrnamentDetail detail);

// Helices (C : helice), registre AC-59 a AC-63 : meme construction, representante
// de la tige 0 vers l'axe de face theta = 0 (oeil en x, y, h ; plan a 45 degres de
// la face, u vers l'axe de face et vers l'avant ; tige nee derriere le D interieur),
// sans borne en plan.
// La representante de la tige 1 est l'image miroir par le plan theta = pi/4.
VoluteSpec DefaultHelixSpec (OrnamentDetail detail);

// Coques d'une volute dans le repere du tour (sommets en double) : le ruban
// (anneaux de section, stations de la tige puis de la spirale), puis l'oeil.
struct VolutePiece
{
	std::vector<std::array<double, 3>>   positions;
	std::vector<std::array<unsigned, 3>> triangles;
	unsigned ribbonVertices = 0, ribbonTriangles = 0;
	unsigned stemStations = 0, spiralStations = 0, sectionPoints = 0;
	double turns = 0.0;                    // tours parcourus par la spirale
	std::array<double, 3> eye {};          // centre de l'oeil (x, y, h en parties)
};

// Volute de la tige `stem` (0 ou 1) sous la corne pi/4. Leve
// std::invalid_argument (« BuildVoluteRibbon : » ...) si la forme sort de son
// domaine, si la largeur atteint le rayon local - c, si l'oeil laisse moins d'un
// tour, si la tige se courbe sous la demi-diagonale de sa section + c ou se tord
// plus vite que maxTwistRate, si la rampe d'epaisseur (stemThin < 1) depasse la
// longueur de la tige, ou si un sommet entre dans la corbeille (< noyau
// + c), sort du cone, monte au-dessus de l'abaque - c, ou, si planBound, sort de
// l'abaque en plan (r > abacusOutline (theta) - c).
VolutePiece BuildVoluteRibbon (const CapitalEnvelope &env, const VoluteSpec &spec, int stem);

// ---------------------------------------------------------------------------
//  Tour du chapiteau et fleuron G (fiore)
// ---------------------------------------------------------------------------
//
// Tour tel que LatheColumnProfile le maille : un anneau par sommet du profil,
// un point par angle de `angles` (LatheAngles du reglage de la colonne), deux
// anneaux consecutifs relies par les triangles (a, b, c) et (a, c, d) de chaque
// secteur. Entre deux angles le tour est la corde, pas Ring::RadiusAt : sur une
// face concave (abaque) la corde passe devant l'anneau. h depuis le pied du
// chapiteau, en parties.
struct CapitalTour
{
	std::vector<double> angles;                                // croissants dans [0, 2 pi)
	std::vector<double> h;                                     // un par sommet du profil
	std::vector<std::vector<std::array<double, 2>>> points;    // (x, y) de chaque anneau, un par angle
	double top = 0.0;                                          // haut du chapiteau
};

// Les anneaux circulaires des membres du chapiteau dans les bandes d'ornement
// (h dans [env.BandBottom (), env.BandTop ()]) sont lus au noyau de `env` :
// identiques au profil a noyau, ils remplacent l'enveloppe d'ornement
// (ColumnOptions::ornamentEnvelope), volume schematique que les pieces
// recouvrent ; l'abaque et le reste du profil sont lus tels quels. h depuis
// env.foot : `env` est lue sur le meme profil (ReadCapitalEnvelope).
// Leve std::invalid_argument si le profil n'a pas de chapiteau ou moins de deux
// sommets, ou si `angles` est vide, non croissant ou hors de [0, 2 pi).
CapitalTour ReadCapitalTour (const ColumnProfile &profile, const std::vector<double> &angles, const CapitalEnvelope &env);

// Plus grand x des points du maillage du tour tels que h dans [hLo, hHi] et y
// dans [yLo, yHi] (parties ; -infini si aucun) : exact, chaque triangle etant
// decoupe par les quatre plans de la boite.
double TourDepthMax (const CapitalTour &tour, double hLo, double hHi, double yLo, double yHi);

// Fleuron G de la face theta = 0, repere local : x radial (normale de la face),
// y le long de la face, h vertical. Rosace : coque unique sur une grille polaire
// (rho, phi) du plan (y, h), centree en (0, hc), hc = tour.top - clearance -
// tipRadius ; phi_i = pi/2 + 2 pi i / (petals samplesPerPetal), une pointe en
// haut et en bas ;
//   - petales en cuiller : bout circulaire de rayon a centre a tip - a, flancs
//     tangents issus du centre, a tel que deux bouts voisins laissent petalGap
//     entre eux ; entre deux flancs le contour descend a notchRadius ;
//   - anneaux : 1 au bord du noyau (coreRadius), buttonRings = max (1, (rings -
//     1) / 4) jusqu'au bord du bouton, puis les petales jusqu'au contour ;
//   - face avant x = front + calotte du bouton (fleche buttonRise) - l (petalHollow
//     sin (pi u) + tipFall u^2), u de 0 au bord du bouton a 1 au contour, l = 1 -
//     (delta / flanc)^2 sur l'axe du petale, 0 entre les flancs ;
//   - dos : noyau (rho <= coreRadius, cache derriere le bouton) conforme au tour,
//     x = TourDepthMax sur la boite (y, h) du sommet et de ses voisins elargie de
//     clearance, + clearance ; petales : tambour plan a front - petalThickness,
//     ramene a la meme borne la ou le tour passe devant ; tout point du dos et du
//     bord est ainsi a clearance au moins du maillage du tour ;
//   - bord : quads entre les contours avant et arriere.
// Tige : demi-jonc a dos plat sur la corbeille, axe de face, sections
// horizontales de stemFoot a cornetTop (stemRings anneaux reguliers en h) ;
// largeur stemWidth, evasee par smootherstep jusqu'a cornetWidth entre
// cornetBottom et cornetTop ; face avant en demi-ellipse de fleche stemDepth
// (stemSection - 2 aretes) ; dos plat a TourDepthMax sur la boite (y, h) de
// l'anneau elargie de clearance, + clearance, releve la ou, sur l'un des huit
// sous-intervalles d'une facette entre deux anneaux, la corde du dos passe
// sous le maximum du tour sur la boite du sous-intervalle elargie de
// clearance, + clearance ; un sommet au milieu du dos ; bouts fermes en
// eventail depuis ce sommet.
// Les sommets de y < 0 sont les images exactes de ceux de y > 0, et les
// triangles, ceux des triangles de y > 0.
struct FleuronSpec
{
	int    petals = 16;
	double tipRadius = 3.95, notchRadius = 1.55, buttonRadius = 1.25, coreRadius = 1.0;
	double petalGap = 0.15;
	double front = 21.66, petalThickness = 2.35;
	double buttonRise = 0.45, petalHollow = 0.15, tipFall = 0.3, minThickness = 0.3;
	double clearance = 0.05;
	double stemWidth = 0.67, stemDepth = 0.35, stemFoot = 24.0;
	double cornetBottom = 30.8, cornetTop = 33.0, cornetWidth = 0.94;
	int    samplesPerPetal = 8, rings = 4, stemSection = 6, stemRings = 10;
};

// Valeurs du registre : Light 8 echantillons par petale, 4 anneaux, tige 6 x 10 ;
// Heavy 12, 9, tige 12 x 24.
// Bouton de fleche 0,45 : sommet a front + 0,45, debord sur la face de B 4,51,
// sous celui de la gravure (4,06) + 0,5.
FleuronSpec DefaultFleuronSpec (OrnamentDetail detail);

// Repere du tour (x, y, (h + foot)) fois env.scale, sommets en double. Rosace :
// centre avant, anneaux avant (anneau j, echantillon i : 1 + (j - 1) n + i), puis
// la meme grille au dos ; triangles : avant, dos, bord. Puis la tige (anneau i,
// point p : rosetteVertices + i stemSection + p ; points 0 a stemSection - 2 sur
// la demi-ellipse, de y > 0 a y < 0, puis le milieu du dos) : cotes, puis bouts.
struct FleuronPiece
{
	std::vector<std::array<double, 3>>   positions;
	std::vector<std::array<unsigned, 3>> triangles;
	unsigned rosetteVertices = 0, rosetteTriangles = 0;
};

// Leve std::invalid_argument (« BuildFleuron : » ...) si la forme sort de son
// domaine (petals impair ou < 4, samplesPerPetal impair ou < 2, rings < 3,
// valeur non finie ou negative), si le noyau deborde du bouton, si l'encoche est
// sous le bouton ou au-dela du flanc des petales, si le jour entre petales
// depasse leur pas, si le tour manque derriere la rosace, si l'epaisseur descend
// sous minThickness, si un sommet sort du carre des cornes de l'abaque (x >
// env.abacus.radius / sqrt 2 - clearance),
// si la tige sort de son domaine (stemSection impair ou < 4, stemRings < 2,
// stemFoot < cornetBottom < cornetTop non tenu) ou monte a moins de clearance
// du bas de la rosace, ou si le tour manque derriere elle.
FleuronPiece BuildFleuron (const CapitalEnvelope &env, const CapitalTour &tour, const FleuronSpec &spec);

// ---------------------------------------------------------------------------
//  Ornement assemble, avant le maillage
// ---------------------------------------------------------------------------
//
// Sommets (repere du tour, en double), triangles et plages des pieces de
// BuildCorinthianOrnament, qui en fait son maillage (sommets convertis en
// float). Vide pour OrnamentDetail::None. Memes exceptions que
// BuildCorinthianOrnament.
struct OrnamentGeometry
{
	std::vector<std::array<double, 3>>   positions;
	std::vector<std::array<unsigned, 3>> triangles;
	std::vector<CapitalPiece>            pieces;
};
OrnamentGeometry BuildCorinthianOrnamentGeometry (const OrderData &order, const ColumnProfile &profile,
                                                  const AcanthusOptions &options);

} // namespace detail
} // namespace classical
} // namespace cgarchi
