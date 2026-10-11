#pragma once

#include <array>
#include <optional>
#include <string>

#include "architecture_gothic.h"   // GothicBuildReport, GothicMeshParams
#include "bar_fusion.h"            // FusedBarsResult
#include "parameterized_shapes.h"   // ParameterizedMesh
#include "print_validation.h"      // PrintConstraints, PrintabilityReport
#include "profile2d.h"

//
// Formes parametriques de l'architecture gothique : bloc de maconnerie
// biseaute (ParameterizedGothicBlock) et fenetre a remplage parametrique
// (ParameterizedGothicWindow).
//

// ---------------------------------------------------------------------------
// Gothic architecture
// ---------------------------------------------------------------------------

// Simple beveled masonry block (CreateBlock).
class ParameterizedGothicBlock : public ParameterizedMesh
{
public:
	ParameterizedGothicBlock();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Gothic Block"; }
private:
	float m_width = 1.618f, m_height = 1.f, m_depth = 1.f, m_bevel = 0.1f;
};

// Reglages d'une fenetre gothique : les parametres exposes (GetParameters), plus
// ceux que seul le JSON porte (nombre de lancettes, foils de lancette, trefle,
// rayon de mouchette, pas angulaire). N'en font PAS partie : les seuils
// d'elements (SetFeatureThresholds) et les profils explicites (SetSplayProfile,
// SetBarProfile), qui restent des membres de la fenetre. Valeur copiable ;
// operator== compare champ a champ (NaN differe de lui-meme).
struct GothicWindowSettings
{
	// arch
	float width = 200.f, excess = 1.0f, offsetOuter = 16.f, offsetInner = 10.f;
	// straight body below the pointed heads (main frame + lancets) : turns the
	// bare pointed arch into a tall window silhouette. 0 = pure arch (old look).
	float bodyHeight = 260.f;
	// lancets
	int   subCount = 2; float subDrop = 0.f, subExcess = 1.0f, gapFraction = 0.11f;
	int   lancetLayout = 0;   // 0 = Uniform (count+1 equal mullions), 1 = Prototype (Havemann Fig 5.39)
	// recursion : each lancet -> mini-window (sub-lancets + small rosette), 0..2
	int   recursion = 0;
	// lancet head : 0 = plain pointed, 1 = foiled (small foiled circle in the head)
	int   lancetHead = 1; int lancetHeadFoils = 4;
	// rosette + foils
	bool  rosette = true;
	bool  rosetteFoils = true; int rosetteFoilCount = 6; int rosetteFoilType = 1; float rosetteFoilPointed = 0.5f;
	bool  subFoils = true;     int subFoilCount = 3;     int subFoilType = 0;     float subFoilPointed = 0.5f;
	// trefoil (on the main arch)
	bool  archTrefoil = false; float trefoilSplit = 0.45f, trefoilFoilRadius = 0.30f;
	// mouchettes
	bool  mouchettes = false; int mouchetteType = 0; float mouchetteRadius = 0.18f;
	// fillets + mesh
	bool  fillets = true;
	float zHeight = 20.f;
	// arc tessellation step (rad/segment) ; default 1°. Set from `kseg` on JSON load.
	double maxAngleRad = 3.14159265358979323846 / 180.0;
	// 3D moulding profile on the field borders. Index dans la liste exposee par
	// GetParameters : 0 = Flat (parois droites), 1 = Chamfer (ebrasement biseaute),
	// 2 = Roll bar, 3 = Keel bar, 4 = Ogee bar. Phase 2.
	//
	// FLAT par defaut : c'est la forme la plus simple, celle qui laisse lire la
	// geometrie de la baie sans le relief des moulures, et le point de depart
	// naturel avant d'enrichir.
	int   profile = 0;

	bool operator== (const GothicWindowSettings &o) const;
	bool operator!= (const GothicWindowSettings &o) const { return !(*this == o); }
};

// Full parametric Gothic window tracery (Havemann pipeline) : pointed arch +
// offset + lancets + rosette + foils + trefoil + mouchettes + fillets, then the
// stone region is tessellated and (optionally) extruded to a 3D Mesh. All the
// buildXxx() steps throw on invalid inputs, so Regenerate() clamps parameters
// and wraps the pipeline in try/catch with a placeholder fallback.
class ParameterizedGothicWindow : public ParameterizedMesh
{
public:
	ParameterizedGothicWindow();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Gothic Window"; }

	// Charge une description JSON dans les reglages et les seuils d'elements.
	// Deux formats : v2 (src/cgmath/gothic-window-v2.schema) et v1
	// (src/cgmath/gothic-tracery.schema), converti en v2 puis lu par le meme
	// lecteur. Version : v1 si `$schema` contient « /v1/ », ou si `window` est un
	// objet sans `geometry` ; sinon v2, et un `schema` autre que
	// « gothic-window/v2 » est refuse.
	//
	// Lecture stricte : document illisible, type faux, valeur hors schema
	// (enumeration inconnue, borne, valeur non finie une fois convertie en
	// float), champ inconnu ou champ requis absent -> erreur json_*, nommee par le
	// chemin JSON. Rend alors false, et les reglages comme les seuils restent
	// strictement inchanges ; sinon ils sont poses ensemble. Un champ absent garde
	// sa valeur, sauf `style.rosette.foils` : une rosace decrite sans foils est
	// une rosace sans lobes. Un champ lu sans effet sur la baie donne
	// field_ignored, un champ v1 traduit par son plus proche equivalent
	// field_converted, un heightBott positif un corps nul et parameter_clamped.
	// Les bornes des reglages float s'appliquent a la valeur arrondie en float.
	//
	// `report`, s'il est fourni, recoit ces diagnostics (ajoutes) ;
	// LastLoadReport les garde jusqu'au chargement suivant, refuse ou non. Ne
	// regenere pas : l'appelant lance Regenerate().
	bool LoadFromJson (const std::string &jsonText, GothicBuildReport *report = nullptr);

	// Diagnostics du dernier LoadFromJson.
	const GothicBuildReport &LastLoadReport () const { return m_lastLoadReport; }

	// Reglages et seuils courants en description v2 (JSON indente). Relue par
	// LoadFromJson, elle redonne les memes reglages et seuils, sans avertissement,
	// si chaque reglage est dans le schema et exact a 1e-4 pres (l'export arrondit
	// les reglages a 1e-4 ; les seuils sont a pleine precision). Hors de ces cas,
	// non garanti : un reglage hors schema rend la relecture refusee, et Fillets 0
	// avec Mouchettes 1 est relu Mouchettes 0.
	std::string ExportJson() const;
// Profils EXPLICITES -- ce que le menu `Profile` ne sait pas dire.
	//
	// Le parametre `Profile` est une enumeration a cinq positions : Flat,
	// Chamfer, et trois barres. Un profil de moulure est pourtant une COURBE --
	// une ebrasure a sa pente, une doucine sa courbure --, et l'ecraser en cinq
	// positions figees etait la limite du modele plat. Ces deux poseurs
	// l'ouvrent : le profil devient une VALEUR, produite ailleurs, et la famille
	// cesse d'etre un menu.
	//
	//   - ebrasement (ouvert, en (u = enfoncement, v = rentrant)) : remplace la
	//     position Chamfer, consomme par extrudeProfiledToMesh ;
	//   - barre (ferme) : remplace les trois positions de barre, balaye par
	//     buildBayMoulding le long des vraies silhouettes d'ouverture.
	//
	// Nul = revenir au menu. Les deux sont independants : poser le seul
	// ebrasement laisse les barres au menu, et l'inverse.
	//
	// ⚠ Les points sont en UNITES MONDE de la baie, dont la largeur par defaut
	// vaut 200 : un profil d'une unite y est invisible. Ils sont divises par
	// l'echelle de la baie pour entrer dans le repere canonique. Le u des barres est
	// RELATIF a la face avant ; c'est buildBayMoulding qui recoit la cote de
	// cette face, et non l'auteur du profil.
	void SetSplayProfile (const Profile2D *profile);
	void SetBarProfile (const Profile2D *profile);

	// Vrai si Regenerate pose un RELIEF sur la plaque : Extrusion > 0, et un
	// ebrasement (port, ou position Chamfer) ou des barres (port, ou positions
	// Roll, Keel, Ogee). C'est le relief DEMANDE : un repli de la construction
	// (profile_fallback, moulding_dropped) n'est pas lu.
	bool HasReliefProfile () const;

	// Vrai si Regenerate pose des BARRES : Extrusion > 0, et port de barre ou
	// position Roll, Keel, Ogee. Relief demande, comme HasReliefProfile.
	bool HasBarRelief () const;

	// Debord du relief hors de la plaque extrudee, en unites monde : plus grande
	// coordonnee absolue du profil de barre place (port, ou position du menu),
	// soit sa demi-largeur et sa hauteur au-dessus de la face avant. 0 sans
	// barre ou sans extrusion ; un ebrasement n'ote que de la pierre (0). Leve
	// comme BuildStonePolygon.
	double ReliefOverhang () const;

	// Vrai si les reglages, les seuils d'elements et les deux ports sont ceux du
	// dernier Regenerate (memes valeurs, comparees champ a champ : un reglage NaN
	// differe de lui-meme). Faux apres SetSettings, LoadFromJson,
	// SetFeatureThresholds ou un poseur de profil qui change une valeur.
	bool MatchesLastRegenerate () const;

	// Region de pierre 2D que Regenerate extrude, construite par la meme
	// correspondance des parametres : c'est l'entree de la validation des
	// contours (contour_validation.h). Leve sur une combinaison de parametres
	// invalide, la ou Regenerate rend le triangle de repli. `report`, s'il est
	// fourni, recoit les avertissements de construction, et l'erreur
	// invalid_parameter avant de lever sur un reglage non fini. Une region de
	// niveau 0 qui garde un defaut de contour apres reparation est rendue telle
	// quelle, sans exception, avec region_invalid : seul Regenerate, qui a le
	// maillage, en fait une erreur (mesh_not_strict).
	Polygon2 BuildStonePolygon (GothicBuildReport *report = nullptr) const;

	// Baie a barres fusionnees : la plaque et les barres de profil en une
	// seule coque (fuseBarsOnPlate), dans le repere monde de Regenerate. Regenerate
	// ne l'appelle pas. Les contours des barres sont ceux de buildBayMoulding
	// (collectBayMouldingOutlines), le profil celui de Regenerate (port de barre,
	// sinon position du menu). Rend false, `out` vide, sans barre (menu Flat ou
	// Chamfer sans port de barre, Extrusion 0), avec un profil d'ebrasement (port
	// ou Chamfer : la fusion ne le combine pas), ou si l'assemblage echoue.
	// `result` et `report`, s'ils sont fournis, recoivent l'issue de l'assemblage
	// et les avertissements de construction. Leve comme BuildStonePolygon, et sur
	// un profil de barre non fini (std::invalid_argument).
	bool BuildFusedBay (Mesh &out, FusedBarsResult *result = nullptr, GothicBuildReport *report = nullptr) const;

	// Rapport du dernier Regenerate. Sur un triangle de repli, il porte l'erreur
	// (invalid_parameter, ou construction_failed avec le texte de l'exception)
	// et les avertissements enregistres avant elle. Les clamps des reglages y
	// sont des avertissements parameter_clamped, nommes par le nom du parametre.
	// Une region de niveau 0 encore invalide apres reparation (region_invalid)
	// est extrudee ; si la pierre extrudee n'est pas stricte (une composante,
	// aucun defaut de ValidateMesh, soudure 5e-4 x largeur / 200 ; Extrusion 0 :
	// aretes de bord admises ; barres exclues), le rapport porte l'erreur
	// mesh_not_strict et le maillage est garde.
	const GothicBuildReport &LastBuildReport () const { return m_lastReport; }

	// Seuils d'elements, en UNITES MONDE (defauts : GothicFeatureThresholds).
	// Toute la chaine calcule dans un repere canonique de largeur 200, ou les
	// seuils sont convertis ; ils ne suivent donc pas l'echelle de la baie :
	// pour une baie reduite d'un facteur k a l'identique, les reduire aussi
	// (GothicFeatureThresholds::Scaled).
	void SetFeatureThresholds (const GothicFeatureThresholds &thresholds) { m_thresholds = thresholds; }
	const GothicFeatureThresholds &FeatureThresholds () const { return m_thresholds; }

	// Reglages courants. SetSettings les remplace en entier, sans regenerer.
	// Les seuils et les profils explicites n'en font pas partie.
	const GothicWindowSettings &Settings () const { return m_settings; }
	void SetSettings (const GothicWindowSettings &settings) { m_settings = settings; }

private:
	// Taille de la mouchette dans son champ : fixe, exportee telle quelle.
	static constexpr double kMouchetteSize = 0.92;

	// Remplit la geometrie et les parametres de maillage dans le repere
	// canonique, et rend l'echelle monde / canonique (largeur / 200).
	double prepareBay (WindowGeometry &geom, GothicMeshParams &mp, GothicBuildReport *report) const;

	// Profil de barre de la baie, dans le repere canonique et relatif a la face
	// avant : le port de barre s'il est branche, sinon la position du menu
	// (Roll, Keel, Ogee) ; vide sinon. `s` est l'echelle monde / canonique.
	Profile2D barProfile (const GothicMeshParams &mp, double s) const;

	GothicBuildReport       m_lastReport;
	GothicBuildReport       m_lastLoadReport;
	GothicFeatureThresholds m_thresholds;

	bool     m_hasSplayProfile = false;
	Profile2D m_splayProfile;
	bool     m_hasBarProfile = false;
	Profile2D m_barProfile;

	GothicWindowSettings m_settings;

	// Entrees du dernier Regenerate (MatchesLastRegenerate).
	struct Snapshot
	{
		GothicWindowSettings    settings;
		GothicFeatureThresholds thresholds;
		bool                    hasSplay = false, hasBar = false;
		Profile2D               splay, bar;
	};
	Snapshot m_lastRegenerate;
};

// Verdict d'export d'une Gothic Window (maker : exportObj, exportGlb).
struct GothicExportCheck
{
	bool        allowed = false;   // faux : le dernier Regenerate porte une erreur, rien n'est exporte
	bool        strict  = false;   // maillage strict (CheckGothicExport)
	std::string message;           // motif du refus, ou defauts d'un maillage non strict ; vide sinon
};

// Verdict sur `mesh`, le maillage du dernier Regenerate de `window`. Refus si
// son rapport (LastBuildReport) porte une erreur, quelle qu'elle soit ; le
// motif est la premiere erreur. Le maillage refuse est le triangle de repli
// (invalid_parameter, construction_failed) ou le maillage reel dont la pierre
// n'est pas stricte (mesh_not_strict). Sinon l'export est permis, et `strict`
// dit si le maillage est d'un seul tenant et sans aucune categorie de defaut de
// ValidateMesh, test d'auto-intersection non tronque, soudure 5e-4 x largeur /
// 200 (la tolerance suit la baie).
GothicExportCheck CheckGothicExport (const ParameterizedGothicWindow &window, const Mesh &mesh);

// Portee du controle de fabrication d'une Gothic Window (CheckGothicPrint).
enum class GothicPrintScope
{
	NotRun,              // geometrie refusee : rien n'est mesure
	Complete,            // plaque plate, ou ebrasement couvert (limites : CheckGothicPrint)
	ProfileNotChecked,   // relief non couvert (barres, ebrasement hors garde) : mesure partielle
};

// Nom stable en snake_case : « not_run », « complete », « profile_not_checked ».
const char *GothicPrintScopeName (GothicPrintScope scope);

// Verdict de fabrication : la geometrie d'abord, l'impression ensuite.
struct GothicPrintCheck
{
	GothicExportCheck  geometry;   // CheckGothicExport
	PrintabilityReport print;      // vide (evaluated faux) si scope == NotRun
	GothicPrintScope   scope = GothicPrintScope::NotRun;
	std::string        reason;     // motif d'un NotRun ou d'un ProfileNotChecked ; vide sinon

	// Geometrie permise ET stricte, plaque entierement mesuree (Complete) et
	// aucun defaut d'impression. Un relief non mesure n'est jamais declare
	// imprimable.
	bool IsPrintable () const;
};

// Controle de fabrication de `mesh`, qui doit etre le maillage du dernier
// Regenerate de `window`. `constraints.mmPerUnit` est en mm par unite MONDE de
// la baie : une largeur de 200 a 0,1 mm / u fait 20 mm.
//
//   1. geometry = CheckGothicExport (window, mesh). Refusee : NotRun, et rien
//      d'autre n'est calcule (contraintes non lues).
//   2. Concordance, sinon NotRun avec son motif : les reglages, seuils et ports
//      sont ceux du dernier Regenerate (MatchesLastRegenerate) ; l'emprise du
//      maillage est celle de la region de pierre en x et y, et va de 0 a
//      l'Extrusion en z (au-dela seulement avec un relief), a 1e-3 x largeur
//      pres. Un maillage d'une autre baie, ou d'avant un changement de
//      reglage, n'est donc pas juge sur la region courante.
//   3. Sans relief : BuildStonePolygon (pierre et vides, en unites monde), extrudee
//      sur Extrusion (zHeight, plancher 0), mesuree par CheckPrintExtrusion :
//      parois, ouvertures, pointes de pierre, thin_slab. Une pointe de vide
//      reste une information (blunted_void_corner).
//   4. Barres (HasBarRelief) : scope ProfileNotChecked, motif nomme. La plaque
//      est mesuree comme en 3, comme si elle etait plate ; les barres ne sont
//      pas mesurees. IsPrintable est faux.
//   5. Ebrasement seul (port ou position Chamfer) : pile de tranches LUE SUR LE
//      MAILLAGE (CheckPrintSlices). Niveaux = cotes distinctes des sommets ;
//      coupes aux deux bouts de chaque couche, en retrait de 1e-4 de son
//      epaisseur, confondues quand leurs sections sont egales (couche
//      prismatique, ou niveau sans face horizontale) ; couche prismatique
//      coupee en son milieu. Matiere par tranche, vides sur la silhouette.
//      Scope Complete si la pile est COUVERTE : toute face inclinee regarde
//      vers l'arriere (sections emboitees : chaque paroi s'amincit vers
//      l'arriere, sa largeur coupee n'est pas optimiste), et les coins de la
//      face avant, que les tranches horizontales ne voient pas, ne sont pas
//      fragiles. Majorant de leur partie mince : r = minWallMm / 2, a =
//      inclinaison de la face inclinee la plus couchee, b = angle 2D du coin
//      convexe le plus aigu de la face avant borde par une face inclinee
//      (180 sans coin convexe : la levre), r (sqrt ((cot (a/2) / sin (b/2))^2
//      + 1) - 1) <= maxTipLengthMm. Sinon ProfileNotChecked, motif nomme (coin,
//      levre, valeurs), la pile restant mesuree. Les tranches ne mesurent que
//      des largeurs horizontales : une paroi penchee ou une arete de relief hors
//      de ces deux cas n'est pas vue.
//   6. Enveloppe (constraints.envelopeMm, avec une echelle) : CheckPrintEnvelope
//      sur l'emprise du MAILLAGE (debord des barres compris : c'est ce qui est
//      imprime), apres la concordance et avant la mesure 2D ; ses comptes et
//      son enregistrement s'ajoutent au rapport.
//
// Leve comme BuildStonePolygon, CheckPrintEnvelope, CheckPrintExtrusion et
// CheckPrintSlices (std::invalid_argument sur une contrainte hors domaine).
GothicPrintCheck CheckGothicPrint (const ParameterizedGothicWindow &window, const Mesh &mesh,
                                   const PrintConstraints &constraints);

// Verdict d'export et de fabrication en un seul passage : la garde d'export,
// qui AVERTIT seulement. ExportAllowed ne depend que de la geometrie.
struct GothicPrintVerdict
{
	// check.geometry : CheckGothicExport, calcule une seule fois ; le reste
	// comme CheckGothicPrint.
	GothicPrintCheck check;
	// Texte de l'exception du controle d'impression, capturee ; vide sinon.
	// Domaine (std::invalid_argument : contrainte, demi-emprise en mm...) : son
	// message. Autre std::exception (allocation, Clipper2...) : son type, puis
	// son message ; « out of memory » si la memoire manque pour l'ecrire. Le
	// rapport garde ce qui a ete mesure avant elle (l'enveloppe), evaluated
	// faux. Texte destine a etre LU, pas analyse : le nom de type depend de la
	// plateforme ; la categorie se lit sur check.reason.
	std::string      printError;
	// Emprise du maillage x mmPerUnit, en mm (x, y, z) ; absente sans echelle
	// ou si la geometrie est refusee.
	std::optional<std::array<double, 3>> printedSizeMm;
	// Contraintes employees, telles que recues (echo du rapport).
	std::optional<PrintConstraints> constraints;

	bool ExportAllowed () const { return check.geometry.allowed; }
	bool IsPrintable () const { return printError.empty() && check.IsPrintable(); }
};

// Garde d'export de `mesh`, maillage du dernier Regenerate de `window` :
//   1. check.geometry = CheckGothicExport (window, mesh), une fois. Refusee :
//      NotRun, motif « geometry refused », rien d'autre. Une exception levee
//      ici se propage : sans verdict geometrique, l'export n'est pas juge.
//   2. Sans contraintes, ou sans mmPerUnit : AUCUN controle d'impression
//      (ni BuildStonePolygon, ni pile, ni CheckPrint*) ; NotRun, motif
//      « print scale not set », rapport ScaleUnset (plafond des contraintes,
//      32 sans contraintes). Des contraintes hors domaine donnent printError.
//   3. Avec une echelle : printedSizeMm, puis le controle de CheckGothicPrint
//      (concordance, enveloppe, mesure 2D) sur la geometrie deja calculee.
//   4. Toute std::exception des etapes 2 et 3 est capturee dans printError,
//      portee NotRun : motif « print check error » pour une
//      std::invalid_argument (entree hors domaine), « print check internal
//      error » pour toute autre (defaut du controle, memoire). Une fois la
//      geometrie permise, la garde n'empeche donc jamais l'export ; seule une
//      exception qui ne derive pas de std::exception se propagerait.
GothicPrintVerdict CheckGothicExportAndPrint (const ParameterizedGothicWindow &window, const Mesh &mesh,
                                              const std::optional<PrintConstraints> &constraints);

// Echelle qui donne au maillage EXPORTE la largeur imprimee `printedWidthMm` :
// printedWidthMm / emprise en x de `mesh` (MmPerUnitForPrintedWidth). L'emprise
// est celle du maillage, et non le reglage Width (qui n'est pas la largeur de
// la piece : le cadre deborde) ni la region de pierre (des barres peuvent la
// deborder) : c'est la largeur que montre le slicer. Changer Width garde donc la
// largeur imprimee et change l'echelle. Leve std::invalid_argument comme
// MmPerUnitForPrintedWidth, maillage vide compris.
double GothicMmPerUnitForPrintedWidth (const Mesh &mesh, double printedWidthMm);

// Garde d'export a largeur imprimee (maker) : l'echelle vient de la piece, les
// seuils et l'enveloppe de l'imprimante.
//   - printer.mmPerUnit est IGNORE : l'echelle d'un document d'imprimante
//     figerait la taille de la piece quand Width change ;
//   - sans printedWidthMm : CheckGothicExportAndPrint sans echelle (seuils
//     controles, aucune mesure, motif « print scale not set ») ;
//   - avec : mmPerUnit = GothicMmPerUnitForPrintedWidth (mesh, printedWidthMm),
//     puis CheckGothicExportAndPrint. Une largeur hors domaine (non finie,
//     <= 0, maillage vide) ne bloque rien : verdict sans echelle, puis, si la
//     geometrie est permise, motif « print check error » et printError
//     « printed width: ... ».
// Les contraintes en echo portent l'echelle derivee. Exceptions : celles de
// CheckGothicExportAndPrint.
GothicPrintVerdict CheckGothicExportAndPrintAtWidth (const ParameterizedGothicWindow &window, const Mesh &mesh,
                                                     const PrintConstraints &printer,
                                                     const std::optional<double> &printedWidthMm);
