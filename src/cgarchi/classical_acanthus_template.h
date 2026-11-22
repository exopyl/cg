#pragma once

// Gabarits plans de la feuille d'acanthe, compiles hors ligne
// (data/acanthus_leaves_from_drawing.py) dans data/acanthus_leaves.json, format
// « cgarchi-acanthus-leaves/2 », et embarques (AcanthusLeavesJson). Le C++ ne
// triangule pas : il lit ces maillages plans et les plaque sur la porteuse.
//
// Repere d'un gabarit : demi-feuille u >= 0, axe u = 0 (couture, la symetrie
// se fait au plaquage) ; y developpe le long du meridien d'axe, corps [0, 1],
// arc du bec [1, 2], bande de tige [-1, 0). Une feuille seule (F) lit le
// prefixe de leafVertices sommets et leafTriangles triangles ; une feuille
// avec sa tige (E) lit le tout.

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <cgarchi/classical_acanthus.h>

namespace cgarchi
{

// Texte UTF-8 de src/cgarchi/data/acanthus_leaves.json, embarque a la build
// (embed_file.cmake). Valide pour la duree du programme.
std::string_view AcanthusLeavesJson ();

namespace classical
{
namespace detail
{

// Style de feuille d'acanthe : Olive, lobes arrondis en folioles d'olivier
// (Vignole, pl. XXVI), gabarit des rangs F et E ; OliveMinor, meme contour et
// meme relief a un budget de triangles propre, gabarit des feuilles D
// (registre AC-55). Cles du document : « olive », « oliveMinor ».
enum class LeafStyle { Olive, OliveMinor };

struct LeafTemplateVertex
{
	double u = 0.0, y = 0.0;   // repere du gabarit
	double H = 0.0;            // relief de la face exterieure (p)
	double Hb = 0.0;           // relief du revers (p)
	int    region = 0;
};

struct LeafTemplateRegion
{
	std::string id;
	double layer = 0.0;
	double cup = 0.0;
};

// Arete de bord orientee comme son triangle (sens trigonometrique dans (u, y)).
using LeafTemplateEdge = std::array<unsigned, 2>;

// Bord d'une variante : aretes hors couture (murs de la coque) et sommets de
// la couture (u = 0), dans l'ordre des indices.
struct LeafTemplateRim
{
	std::vector<LeafTemplateEdge> edges;
	std::vector<unsigned>         seam;
};

struct LeafTemplate
{
	LeafStyle      style = LeafStyle::Olive;
	OrnamentDetail detail = OrnamentDetail::Light;
	std::vector<LeafTemplateVertex>      vertices;       // feuille, puis bande de tige
	std::vector<std::array<unsigned, 3>> triangles;      // sens trigonometrique dans (u, y)
	std::vector<int>                     triangleRegion;
	unsigned leafVertices = 0, leafTriangles = 0;        // prefixe de la feuille seule
	LeafTemplateRim leafRim;                             // bord de la feuille seule
	LeafTemplateRim stemRim;                             // bord de la feuille et de sa bande
	std::vector<LeafTemplateRegion>   regions;
	std::vector<std::array<double, 2>> outline;          // contour dessine, (u, y)
	int    stemRegion = 0;
	double stemU = 0.0;                                  // demi-largeur de la tige au pied
	double stemTaper = 1.0;                              // retrecissement de la tige a l'astragale
	// Rapport de forme du dessin : metrique nominale de la triangulation (p).
	double nominalHalfWidth = 0.0, nominalBody = 0.0, nominalCurl = 0.0;
	double backHeight = 0.0;                             // Hb de reference du revers (capBack)
	std::array<double, 2> fade {};                       // effacement du relief exterieur sur [fade[0], fade[1]] en y
	double capHalfWidth = 0.0;                           // |u| maximal du gabarit sur y > 1
};

// Lit un document « cgarchi-acanthus-leaves/2 » : un gabarit par style et par
// niveau (Light, Heavy). Leve std::invalid_argument, message prefixe de
// « ParseLeafTemplates : » et nomme par le chemin JSON, si le document est
// illisible ou d'un autre format, si un style est inconnu, si un nombre n'est
// pas fini, si un indice de sommet ou de region sort de sa plage, si un relief
// est negatif ou si Hb est non nul sous fade[0], si un triangle est retourne
// ou plat dans (u, y), si le prefixe de la feuille seule deborde ou melange
// feuille et bande, si une arete est partagee par plus de deux triangles ou
// parcourue deux fois dans le meme sens, si un sommet d'une variante n'est sur
// aucun de ses triangles, si le bord d'une variante n'est pas une boucle
// (couture comprise), si un sommet est a moins de 1e-9 de l'axe sans y etre,
// ou si les comptes ne sont pas ceux du document.
std::vector<LeafTemplate> ParseLeafTemplates (std::string_view json);

// Gabarit embarque de `style` au niveau `detail`, lu une fois. Leve
// std::invalid_argument pour OrnamentDetail::None.
const LeafTemplate &LeafTemplateOf (LeafStyle style, OrnamentDetail detail);

} // namespace detail
} // namespace classical
} // namespace cgarchi
