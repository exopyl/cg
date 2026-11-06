# cgmath

Bibliothèque statique de base du dépôt (`cg::cgmath`), C++17. Aucune dépendance interne.

## Responsabilité

Ce qui appartient au module :

- algèbre : `TVector2/3/4`, `TMatrix2/3/4`, `TSquareMatrix`, `TQuaternion`, polynômes (`polynomial*`, `bernstein`) ;
- géométrie analytique : `Plane`, `Circle`, `Ellipse`, `Arc`, `Sphere`, `Torus`, `Triangle` (`geometry.h`), `AABox`,
  droites, segments, rayons, quadriques, intersections algébriques ;
- courbes génériques : Bézier, NURBS, courbes analytiques (`curve*.h`), aplatissement de Bézier ;
- caméras (`TCamera`, `OrbitCamera`), générateur aléatoire ;
- typographie : lecture TrueType (`font.h`, stb_truetype) et mise en page de texte latin (`text_layout.h`). Elle
  sort de la définition « mathématiques » ; elle reste ici parce que `cgmesh` et `cggraph/nodes` la consomment tous deux.

Ce qui n'y appartient pas :

- **aucun code gothique ni architectural** : il vit dans `cgarchi`. Le mot « gothic » n'apparaît que dans deux
  commentaires de `geometry.h` (l. 110 et 217) ;
- aucun maillage : `cgmath` n'inclut ni `cgimg`, ni `cgmesh`, ni `cgarchi`. Exception connue, à supprimer :
  `ProfilePolyline::generate_surface_revolution` et `generate_frame` (`polyline.h:22-23`) produisent des maillages en
  tableaux `float**` ; ils n'ont aucun appelant de production (`debt_cgmath.md`).

Rien dans CMake n'interdit une inclusion remontante : `cgmath` expose `src/` entier (`CMakeLists.txt:21-22`), donc
`#include <cgarchi/...>` compilerait. Le sens unique tient par discipline.

## Dépendances

| Nature | Dépendance | Activation |
|---|---|---|
| interne | aucune | — |
| tierce | stb_truetype (`extern/stb`), inclus par chemin relatif dans `font.cpp:22` | toujours |

Aucune option `ENABLE_*` ne touche ce module. Exigences d'usage (`CMakeLists.txt`) : `cxx_std_17` PUBLIC (l. 27),
répertoire d'inclusion `src/` PUBLIC (l. 21-22), avertissements `-Wall -Wextra` ou `/W4` PRIVATE (l. 32-34).

## Usage minimal

Repris de `test/tu_cgmath_arc.cpp` (tests `TessellateQuarterCircle`, l. 203-204, et `SemicircleCcwSpanAndLength`,
l. 61-63) :

```cpp
#include <cgmath/geometry.h>

const double PI = 3.14159265358979323846;
Arc a (Circle (Vector2d (0.0, 0.0), 1.0), 0.0, PI / 2.0, true);  // quart de cercle, sens trigonométrique
const double len = a.length ();                                   // PI / 2
auto pts = a.tessellate (4);                                      // 5 points, de theta = 0 à PI / 2
```

## Tests

Fichiers `test/tu_cgmath_*.cpp`, suites `TEST_cgmath_*`, dans l'exécutable `TU` (non construit sous Emscripten).

```powershell
ctest --test-dir build/<preset> -C Release -R "TEST_cgmath_"
```

- `-C` est requis par les générateurs multi-configurations (Visual Studio) ; Debug et Release partagent le répertoire
  d'exécution `test-run` : ne pas lancer les deux en même temps.
- Le motif attrape aussi `TEST_cgmath_raytracer`, défini dans `test/tu_cgmesh_raytracer.cpp`, qui teste le lanceur de
  rayons de `cgmesh`.
