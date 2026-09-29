# Slicing de maillages par plans horizontaux (`mesh_slicing`)

Ce document décrit la conception du module de slicing de cgmesh : l'API, le pipeline, les conventions géométriques, les limites connues et la stratégie de test. Les étiquettes **D1 à D8** citées dans les commentaires du code renvoient au §5.

Fichiers :
- `src/cgmesh/mesh_slicing.h/.cpp` : API, intersection, contours 2D, imbrication, parallélisme ;
- `src/cgmesh/slice_linker.h/.cpp` : chaînage des segments et réparations ;
- `src/cgmesh/contour_ops.h/.cpp` : `contourRegions`, imbrication par enroulement via Clipper2 ;
- `test/tu_cgmesh_mesh_slicing.cpp` : tests.

L'ancien slicer à demi-arêtes (`slicer.*`, `clipper.*`) est conservé tel quel. Le nouveau module ne s'en sert pas.

---

## 1. API

```cpp
struct SliceInput  { const Mesh* mesh; Matrix4f matrix; bool isSupport, isHole; const Mesh* hollowing; };
struct SliceOptions{ Nesting nesting = Nesting::Containment; unsigned int threads = 0; };
std::vector<SliceLayer> sliceMeshes (inputs, zs, options, progress);
```

- **Entrée.** Chaque entrée comprend :
  - un maillage non possédé, qui doit survivre à l'appel ;
  - une matrice objet → plateau, supposée affine ;
  - les drapeaux support et trou ;
  - un maillage d'évidement optionnel, découpé avec la matrice du maillage principal.
- **Sortie.** `out[k]` est la couche à l'altitude `zs[k]`. C'est une liste de `SliceRegion`, et chaque région contient :
  - `contours[0]` : l'enveloppe (`ExtrudeContour`, `isHole = false`), suivie de ses trous ;
  - `faceIds` : les faces du maillage principal qui ont produit la région, triées, doublons compris pour les faces à plus de 3 sommets ;
  - `hollowFaceIds` : les faces du maillage d'évidement, triées de même ;
  - `sourceMesh`, `isSupport`, `isHole`.
- **Regroupement.** Les régions sont regroupées par entrée. Deux entrées ne sont jamais fusionnées entre elles.
- **Imbrication** (`Nesting`) :
  - `Containment` (défaut) : imbrication par contenance, en O(N²) sur deux niveaux. Un solide contenu dans un autre de même orientation y devient un trou (D7).
  - `Winding` : union Clipper2 en NonZero, puis parcours de l'arbre de polygones. Un îlot situé dans un trou devient une région séparée.
- **Contrat de sortie :**
  - chaque contour a au moins 3 points ;
  - l'enveloppe a une aire positive et les trous une aire négative, ce qui est compatible avec `ExtrudeContour` et `contour_ops`. C'est garanti en mode `Winding`. En mode `Containment`, c'est garanti pour tout contour d'aire nettement non nulle ; un contour aligné d'au moins 3 points garde une orientation indéfinie.
- **Altitudes.** Pour `zs` vide, la sortie est vide. Une altitude non finie donne une couche vide, sans effet sur les autres couches.
- **Parallélisme.** Les plans sont répartis en lots contigus sur `std::thread`, dans la limite de `min(threads, nombre de plans, 64)`. Sous `__EMSCRIPTEN__`, le calcul est séquentiel. La sortie est identique au bit près quel que soit le nombre de threads.

## 2. Pipeline, pour un maillage et un plan

1. **Préparation**, une fois par maillage (`buildPlateMesh`) :
   - les sommets sont transformés en repère plateau, de sorte que le plan de coupe est toujours horizontal ;
   - si det(M) < 0, l'ordre des sommets de chaque triangle est inversé, pour que les normales restent tournées vers l'extérieur ;
   - les faces à plus de 3 sommets sont triangulées : en éventail si elles sont convexes, par glutess sinon. Pour chaque triangle, on garde la face d'origine et la normale de Newell de cette face ;
   - les triangles sont triés par z minimal. Un balayage en z fournit, pour chaque plan, un sur-ensemble exact des triangles candidats.
2. **Intersection** (`intersectTriangles`) :
   - chaque sommet est classé *dessus*, *dessous* ou *sur* le plan (tolérance ±FLT_EPSILON) ;
   - chaque triangle croisé produit un segment, dont les extrémités sont identifiées par la clé de l'arête coupée, ou par le sommet s'il est sur le plan ;
   - les arêtes contenues dans le plan passent par une décision par arête (§3.1).
3. **Chaînage** (`SliceLinker`) :
   - si la couche est une variété sur les indices, les segments sont chaînés directement ;
   - sinon, on applique les réparations : suppression des segments de longueur nulle, des arêtes dupliquées, des « poils », des sommets non-variétés, puis comblement des trous par proximité (0,005).
4. **Chemins** (`traceChains`) : on obtient des boucles, avec un indicateur de fermeture topologique.
5. **Contours 2D** :
   - chaque boucle fermée est ancrée sur son point canonique (§3.3) ;
   - `cleanContour` retire les points proches (0,01, axe par axe) et les angles plats (0,001 rad) ;
   - la boucle est découpée aux points répétés ;
   - les contours de moins de 3 points sont écartés ;
   - les boucles restantes sont triées dans un ordre canonique.
6. **Imbrication**, selon `Nesting`, puis orientation de sortie.

## 3. Conventions géométriques

### 3.1 Arêtes contenues dans le plan : convention « solide fermé »

Un plan posé exactement sur une face horizontale rend la **section fermée** du solide, en haut comme en bas : un cube coupé sur sa face du dessous comme sur sa face du dessus donne le carré.

- **Côté d'une arête.** Une arête contenue dans le plan reçoit un « côté » de chacune de ses deux faces :
  - une face non horizontale donne le côté de son troisième sommet ;
  - une face horizontale (les trois sommets sur le plan) donne le côté de sa normale de Newell : vers le haut → *dessus*, vers le bas → *dessous*. Elle ne vote que si |nz| > 1 − 1e-3.
- **Décision.** Elle passe par une table indexée par la clé d'arête et ne dépend pas de l'ordre des faces :
  - deux côtés **opposés** : l'arête est gardée une fois, avec le segment de la face « dessous » ;
  - deux côtés **identiques** (arête posée sur le plan, diagonale d'une face horizontale) : l'arête est supprimée ;
  - une seule entrée, ou plus de deux (bord, arête non-variété) : la première entrée réelle est émise, et elle est marquée invalide si une entrée ultérieure est du même côté.
- **Maillage soudé.** La convention suppose un maillage soudé (`MergeVertices`). Sur une soupe de triangles, les arêtes n'ont qu'une entrée chacune.

### 3.2 Orientation des segments

L'orientation est **combinatoire** : un segment va du point où l'enroulement du triangle passe de *dessous* à *dessus* vers celui où il repasse de *dessus* à *dessous*. La règle est déclinée pour 0, 1 ou 2 sommets sur le plan.

- Sur un triangle non dégénéré, elle équivaut à `dir = n × z`. Pour un maillage orienté vers l'extérieur, la boucle brute tourne dans le sens horaire, et la sortie est réorientée ensuite.
- Elle reste définie pour un segment de longueur nulle ou un triangle dégénéré.

### 3.3 Invariance à l'ordre des faces

- Une boucle fermée démarre sur son point lexicographiquement minimal (x, puis y). En cas d'égalité, on compare les points suivants.
- Le premier point n'est retiré, pour ancrer la boucle, que s'il coïncide avec le dernier à 4 ULP float près.
- Les boucles sont triées, puis les `faceIds`.
- La sortie est alors identique au bit près pour tout ordre des faces, les sommets de chaque face restant inchangés. Ce n'est pas garanti pour les chemins ouverts, les heuristiques de réparation, ni les arêtes à 1 ou plus de 2 entrées.

## 4. Stratégie de test

Toutes les fixtures sont construites **en mémoire** à partir des générateurs de cg : `CreateCube`, `ParametricTorus`, `CreateOctahedron`, `CreateCylinder`, prismes et marches construits à la main, texte extrudé avec une police de `test/data/fonts`. Aucun fichier de maillage n'est nécessaire.

- **Préconditions** : chaque fixture a un volume signé positif et la topologie voulue.
- **Oracles transverses :**
  - O1 : conservation du volume (Σ aire · Δz ≈ volume) ;
  - O2 : déterminisme entre nombres de threads, exécutions et ordre des plans ;
  - O3 : trancher avec une matrice donne le même résultat que trancher le maillage déjà transformé ;
  - O4 : convention d'orientation de sortie.
- **Cas couverts :**
  - cube, y compris des plans posés sur ses faces ;
  - cubes partageant une arête ;
  - évidement et provenance des faces ;
  - matrices, dont une matrice miroir ;
  - octaèdres qui se touchent ou posés sur une pointe ;
  - tore couché, avec plans tangents, quasi tangents et passant par des sommets ;
  - prisme avec une face manquante ;
  - solide imbriqué ;
  - défauts synthétiques : face dupliquée, face retournée, couture non soudée, sommet pincé, arête nulle ;
  - marche d'escalier à l'endroit et renversée ;
  - faces non convexes ;
  - faces quasi horizontales ;
  - cylindres très finement facettés ;
  - segments de longueur nulle ;
  - invariance à l'ordre des faces ;
  - liens asymétriques dans le chaînage.
- **Valeurs figées** : les tests qui figent une limite connue le disent dans leur commentaire.

## 5. Défauts et limites (registre D1 à D8)

| Réf. | Sujet | État |
|---|---|---|
| D1 | Arêtes contenues dans le plan. Une face horizontale n'émettait rien, ce qui créait des jonctions en T et envoyait toute la couche en réparation sur un maillage sain. | **Corrigé** : convention solide fermé (§3.1). La tolérance absolue ±FLT_EPSILON du classement demeure. |
| D2 | Suppression des segments de longueur nulle (seuil absolu 3,45e-4) : un contour très facetté s'effondrait (cylindre r = 1 à 20 000 facettes : aire 0) et le coût était en O(k²). | **Corrigé sur le chemin nominal.** Limite : une couche non-variété passe encore par cette suppression avant les réparations. |
| D3 | Rejet des faces quasi horizontales d'après leur normale : une face presque plate qui traverse le plan ouvrait le contour. | **Corrigé** : une face horizontale est reconnue au fait que ses trois sommets sont sur le plan. |
| D4 | Matrice miroir : l'enroulement s'inversait. | **Corrigé** : repère plateau et inversion des triangles si det < 0. |
| D5 | Plan tangent. | **Sans objet** : un plan tangent exact rend 0 région ; un plan quasi tangent rend une bande fine géométriquement juste. Les boucles plus petites que la tolérance de nettoyage sont écartées. |
| D6 | Segments sans direction (longueur nulle) : orientation arbitraire. | **Corrigé** : orientation combinatoire (§3.2). |
| D7 | Mode `Containment` : un solide imbriqué de même orientation devient un trou. | **Limite assumée** : utiliser `Winding`. |
| D8 | Dépendance à l'ordre des faces. | **Corrigé pour les boucles fermées** (§3.3). |

Autres limites connues :
- **Maillage ouvert** : le comblement des trous ne relie que des extrémités distantes de moins de 0,005. Une brèche plus grande laisse un chemin ouvert, écarté s'il a moins de 3 points.
- **Tolérances absolues** : nettoyage (0,01), comblement (0,005), contenance (0,0012). Elles supposent un maillage en millimètres, à l'échelle d'impression.
- **Pincements** : les sommets où le contour se touche lui-même (4 segments légitimes) passent par les réparations.
- **Réparations globales** : elles s'appliquent à toute la couche dès qu'une seule clé est ambiguë.
- **Face auto-intersectée à plus de 3 sommets** : la triangulation peut être partielle.
