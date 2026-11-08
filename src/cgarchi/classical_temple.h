#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_peristyle.h>

namespace cgarchi
{
namespace classical
{

// ============================================================================
//  Temple : krepis, puis peristyle de colonnes doriques grecques posees sur le
//  stylobate ; placements, puis maillage
// ============================================================================
//
// Repere (D2-5) : X le long de la facade, Y en profondeur (facade en Y < 0),
// Z vertical ; krepis et axes centres sur l'origine ; pied du degre le plus bas
// en z = 0 (D3-16). Unites : celles de l'appelant.
//
// Conventions (registre TE1 a TE6, classical_orders_implementation.md section
// 10.8) : deux jeux de reglages, colonne courante et colonne d'angle (TE1) ;
// peristyle centre sur le stylobate (TE2) ; pied de chaque colonne dans le
// dessus du stylobate, bord compris (TE3) ; aucun contact entre colonnes (TE4) ;
// pieces fermees concatenees sans union, seuls recouvrements les contacts plans
// L1 (degres) et L2 (pied sur stylobate), et, avec un entablement, L3 a L6
// (TE5, ci-dessous) ; travee d'angle simple et symetrique (TE6,
// LayoutPeristyle) ; pied de l'entablement au sommet float des colonnes
// (EN10).
//
// Entablement optionnel (classical_greek_entablature.h) : absent, le temple est
// celui de la krepis et du peristyle seuls, au bit pres. Present, il ajoute un
// anneau balaye pose sur les abaques et un prisme par placement de triglyphe
// pose sur la frise de l'anneau, en L pour une piece d'angle (EN14, sous les
// strategies d'angle) ; les metopes sont la face de l'anneau (EN5). La strategie
// d'angle ne change ni le peristyle ni les colonnes. Recouvrements admis en plus
// de L1 et L2 (TE5) : L3, dessous de l'anneau sur le dessus de chaque abaque ;
// L4, dos d'un triglyphe sur la face de frise de l'anneau, sur les deux faces de
// frise de son angle pour une piece d'angle ; L5, dessous d'un triglyphe sur le
// ressaut de la bande sous la frise ; L6, dessus d'un triglyphe sous le ressaut
// de la bande au-dessus ; L7, sous une strategie d'angle, arete du coin (b, b)
// d'une piece d'angle confondue au bit pres avec l'arete d'onglet de la frise
// de l'anneau : apres soudure, une arete non manifold par piece d'angle et une
// composante de moins.
//
// Ornements optionnels de l'entablement (regulae, mutules) : une partie
// par regula avec ses gouttes, une par mutule, apres les triglyphes. Contacts
// admis en plus (TE5) : L8, dessus d'une regula sous le ressaut de la tenia ;
// L9, dos d'une regula sur la face de la bande friezeBand - 2 (deux faces pour
// une regula d'angle) ; L10, tete d'une goutte sous le dessous de sa regula ;
// L11, dessus d'un mutule sous le ressaut de la bande du dessus ; L12, dos d'un
// mutule sur la face de sa bande ; L13, sous une strategie d'angle, coins
// (R, R) et (dos, dos) du dessus de la regula d'angle sur deux sommets d'onglet
// de l'anneau, et diagonale (dos, dos)-(R, R) de son couvercle sur l'arete
// d'onglet du ressaut de la tenia : apres soudure, une arete non manifold
// (4 faces) et une composante de moins par regula d'angle ; L14, dessous d'un
// mutule sur le dessus de la bande sous la sienne, la ou son dos est en retrait
// sur elle. Une goutte est une composante a part. Sous sous-face inclinee
// (EN22, mutules.rise > 0) : L11, dessus du mutule sur la face inclinee
// de l'anneau, coplanaire au bit pres (memes couples (d, z)) ; aucun L14 ;
// L15, tete d'une goutte de mutule sur le dessous de son mutule, coplanaire a
// l'arrondi float pres : ValidateMesh ne la juge coplanaire que si cet ecart
// reste sous sa tolerance (registre TE5 ; CT3 valide a 1e-4 m, D3-80) ;
// le sommet avant de la tete avant touche l'arete avant-basse du mutule, sans
// traversee.

struct TempleSettings
{
	KrepisSettings           krepis;
	PeristyleSettings        peristyle;
	GreekDoricColumnSettings column;         // les 2 (front + flank) - 8 colonnes courantes
	GreekDoricColumnSettings cornerColumn;   // les 4 colonnes d'angle
	std::optional<GreekEntablatureSettings> entablature;   // absent : krepis et peristyle seuls
};

enum class TempleElement { KrepisStep, Column, Entablature, Triglyph, Regula, Mutule };

// Adresse stable (DG-4) : ne depend que des comptes et de la strategie d'angle
// de la frise, pas des longueurs. Par defaut, index = -1 : TempleAddress {} ne
// designe aucun element.
struct TempleAddress
{
	TempleElement element = TempleElement::KrepisStep;
	PeristyleSide side = PeristyleSide::Front;   // colonnes et triglyphes ; Front pour un degre et l'anneau
	// Degre : 0 stylobate, puis vers le bas. Colonne : rang dans son cote, dans
	// l'ordre de PeristyleLayout::columns, 0 pour l'angle qui ouvre le cote.
	// Anneau de l'entablement : 0. Triglyphe : rang de son placement dans son
	// cote (FriezePiece::rank) ; une piece d'angle a le rang 0 du cote qu'elle
	// ouvre. Regula : rang du triglyphe qui la porte (OrnamentPiece::rank), une
	// goutte n'ayant pas d'adresse propre. Mutule : rang le long de son cote.
	int index = -1;
};

struct TemplePlacement   // une colonne
{
	TempleAddress address;
	double x = 0.0, y = 0.0, z = 0.0;   // translation du pied de l'axe ; z = stylobateTop
	bool corner = false;                // reglages cornerColumn si vrai, column sinon
};

struct TempleLayout
{
	KrepisLayout                 krepis;
	PeristyleLayout              peristyle;
	GreekDoricColumnHalfExtents  columnExtents;         // colonne courante
	GreekDoricColumnHalfExtents  cornerColumnExtents;   // colonne d'angle
	std::vector<TemplePlacement> columns;               // ordre de peristyle.columns
	// Avec un entablement : pied, double (float (column.height)) + stylobateTop,
	// et frise (triglyphes et metopes, ces dernieres sans maillage). Sans : 0 et
	// absente.
	double                       entablatureFoot = 0.0;
	std::optional<FriezeLayout>  frieze;
	// Avec un entablement a regulae ou a mutules : LayoutGreekOrnaments ; absent
	// sinon.
	std::optional<OrnamentLayout> ornaments;
};

// Valide les composants : krepis, peristyle, puis les deux jeux de reglages de
// colonne ; leurs exceptions remontent telles quelles, avec le prefixe de leur
// composant. Leve ensuite std::invalid_argument, prefixe "Temple : " et chemin
// du champ entre apostrophes, si, sur les axes places par LayoutPeristyle
// (e_n, e_c : demi-etendues `overall` des colonnes courante et d'angle) :
//   - deux axes voisins d'une rangee sont a une distance au plus egale a la
//     somme des demi-etendues de leurs colonnes (e_c aux deux bouts, e_n entre) ;
//     le champ est l'entraxe d'angle pour la premiere et la derniere travee
//     ('peristyle.frontCornerSpacing', 'peristyle.flankCornerSpacing'),
//     l'entraxe courant sinon ('peristyle.frontSpacing', 'peristyle.flankSpacing') ;
//   - une rangee de deux colonnes a sa travee au plus egale a 2 max (e_c, e_n)
//     quand les rangees opposees, qui se font face a travers elle, ont au moins
//     trois colonnes ;
//   - a l'un des quatre angles, les deux voisines, l'une sur la facade, l'autre
//     sur le flanc, sont decalees sur les deux axes d'au plus la somme de leurs
//     demi-etendues ('peristyle.frontCornerSpacing') ;
//   - le pied d'une colonne, courante ou d'angle, de demi-etendue `foot` de son
//     jeu, sort du dessus du stylobate : |x| + foot > stylobateHalfWidth
//     ('krepis.stylobateHalfWidth'), |y| + foot > stylobateHalfDepth
//     ('krepis.stylobateHalfDepth').
// Garantit que les boites alignees sur les axes de deux colonnes placees, de
// demi-cote `overall`, sont disjointes (registre TE4), et que le carre de
// demi-cote `foot` de chaque pied est dans le dessus du stylobate. Pour les
// colonnes elles-memes, la condition est exacte quand les anneaux les plus
// larges des deux colonnes sont a la meme cote et sont des abaques ; elle est
// seulement suffisante sinon : deux sections rondes peuvent avoir des
// projections qui se recouvrent sans se toucher, et une colonne courante et une
// colonne d'angle de hauteurs differentes (acceptees) ont leurs anneaux les
// plus larges a des cotes differentes. Ne garantit pas la validite au sens de
// ValidateMesh d'un maillage du temple (D3-19) : elle demande en plus que toute
// separation de ce maillage depasse 2 fois sa tolerance de soudure, qui depend
// du maillage entier (condition de domaine, non imposee).
// Avec un entablement, valide en plus, dans cet ordre : ses reglages
// (ValidateGreekEntablatureSettings, exceptions "Entablement : " remontees
// telles quelles) ; l'egalite en float des hauteurs des deux jeux de colonne,
// sinon std::invalid_argument "Temple : 'cornerColumn.height'" (l'anneau ne peut
// poser sur deux hauteurs) ; la frise sur les axes places (LayoutGreekFrieze) ;
// les ornements, s'il y en a (LayoutGreekOrnaments).
void ValidateTempleSettings (const TempleSettings &settings);

// Placements, sans maillage. Valide d'abord, comme ValidateTempleSettings.
TempleLayout LayoutTemple (const TempleSettings &settings);

struct TempleMeshPart   // plage d'une piece dans le maillage
{
	TempleAddress address;
	unsigned firstVertex = 0, vertexCount = 0;
	unsigned firstFace = 0, faceCount = 0;
};

// Borne d'interface, pas une valeur du domaine (D3-17) : elle borne la memoire
// du maillage assemble.
constexpr std::size_t kTempleMaxFaces = std::size_t (1) << 24;

// Un maillage a la borne tient en indices unsigned et en tailles size_t d'une
// cible 32 bits (WebAssembly) : 3 indices par face, et au plus 3 sommets de
// 3 coordonnees par face. Le nombre maximal de faces d'un tour n'est pas une
// constante de compilation (la grille du tour recoit a l'execution les angles
// des croisements de plan) : le compte des faces et des sommets s'accumule donc
// sans debordement, chaque piece comparee a la marge restante avant d'etre
// ajoutee (CountTempleMeshFaces).
static_assert (9 * kTempleMaxFaces <= 0xFFFFFFFFu,
               "le maillage a la borne doit tenir en indices et en tailles de 32 bits");

// Vrai si un maillage de `faces` faces respecte la borne.
constexpr bool TempleMeshFacesWithinBound (std::size_t faces)
{
	return faces <= kTempleMaxFaces;
}

// Nombre de faces du maillage de BuildTempleMesh, sans l'assembler : seuls la
// krepis, les deux tours et, avec un entablement, l'anneau et ses pieces
// (BuildEntablaturePieceMeshes) sont construits. Valide d'abord, comme LayoutTemple ;
// leve std::invalid_argument ('peristyle') des que le compte depasserait
// kTempleMaxFaces faces, ou 3 kTempleMaxFaces sommets, compare apres chaque
// piece construite. Les refus "Frise : " de BuildTriglyphMesh (prisme degenere
// ou au contact d'un voisin une fois arrondi en float) et "Ornement : " de
// BuildOrnamentMesh (regula droite au contact, en float, du plan de dos des
// regulae d'un cote voisin ; sommets d'une regula, d'une goutte ou d'un mutule
// confondus une fois arrondis en float) remontent tels quels, ici comme dans
// BuildTempleMesh ; LayoutTemple et ValidateTempleSettings, qui ne maillent pas,
// ne les voient pas.
std::size_t CountTempleMeshFaces (const TempleSettings &settings);

// Krepis (BuildKrepisMesh), puis, par placement et dans l'ordre de
// layout.columns, une copie translatee du tour de la colonne courante ou de la
// colonne d'angle (deux tours en tout), accumulees en un bloc, triangles
// seulement : chaque position est calculee en double (sommet float du tour plus
// translation) et arrondie une fois en float ; le pied de chaque colonne est
// donc, au bit pres, a la cote float (stylobateTop) du dessus du stylobate.
// Parties, dans l'ordre du maillage : un degre par KrepisLayout::steps
// (adresse KrepisStep, index du degre), puis une colonne par placement, puis,
// avec un entablement, l'anneau (adresse Entablature, index 0 ;
// SweepRectangularRing de GreekEntablatureSection sur les demi-distances des
// axes, a la cote entablatureFoot) et un prisme par placement de triglyphe,
// en L pour une piece d'angle, dans l'ordre de FriezeLayout::triglyphs (adresse
// Triglyph, cote et rang du placement ; BuildTriglyphMesh), puis, avec des
// ornements, une regula avec ses gouttes par placement de
// OrnamentLayout::regulae (adresse Regula, cote et rang du triglyphe) et un
// mutule par placement de OrnamentLayout::mutules (adresse Mutule, cote et
// rang) ; toutes les pieces hors anneau sont construites apres une seule
// validation des reglages (BuildEntablaturePieceMeshes). Toute cote de l'entablement vaut float (entablatureFoot
// + z) : le dessous de l'anneau est, au bit pres, a la cote du sommet des
// abaques. Les plages sont contigues et couvrent le maillage. Valide d'abord, comme
// LayoutTemple ; leve aussi std::invalid_argument ('peristyle') si le maillage
// depasserait kTempleMaxFaces faces, comme CountTempleMeshFaces, avant
// l'allocation du resultat. N'ecrit
// dans `layout` et `parts` qu'en cas de succes.
std::unique_ptr<Mesh> BuildTempleMesh (const TempleSettings &settings, TempleLayout *layout = nullptr,
                                       std::vector<TempleMeshPart> *parts = nullptr);

} // namespace classical
} // namespace cgarchi
