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

class Mesh;

struct SvgExtrudeOptions
{
    float height       = 1.0f;  // extrusion depth along +Z
    float flattenTol   = 0.5f;  // pixel-space tolerance for bezier flattening
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
};

// Parse `filename` and return a heap-allocated extruded Mesh, or nullptr on
// failure (file missing, parse error, no fillable shape).
Mesh* import_svg_extruded(const std::string& filename, const SvgExtrudeOptions& opt);
