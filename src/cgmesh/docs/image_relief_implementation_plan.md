# Plan d'implémentation — Image → Relief coloré par extrusion de régions

> **État : implémenté** (v1 + étape 5 partielle). Fichiers livrés :
> `cgmesh/image_relief.{h,cpp}` (orchestrateur), `cgmesh/extrude_contours.{h,cpp}`
> (primitive d'extrusion extraite d'`import_svg.cpp`), `CLitRasterToVector::GetLayers()`
> dans `cgmesh/image_vectorization.{h,cpp}`, tests dans `test/tu_cgmesh_image_relief.cpp`.
> Écarts et décisions prises à l'implémentation : voir §8 en fin de document.

## 1. Objectif

À partir d'une image, générer une géométrie 3D par :

1. **Quantification** de l'image à un nombre de couleurs max (Wu).
2. **Segmentation + vectorisation** en régions de couleur (contours + trous).
3. **Extrusion** de chaque bloc de couleur, à **hauteur uniforme**, **depuis le haut d'une plaque de base** (relief depuis z=0).
4. Encadrement par une **plaque de base plus large** (marge) bordée d'un **mur périmétrique** contenant les blocs.

Sortie : **un mesh multi-couleurs** (défaut, un `Material` par couleur) **ou un mesh par couleur** (via paramètre).

### Décisions actées (interlocuteur)

| Sujet | Décision |
|---|---|
| Hauteur | **Uniforme** : tous les blocs à la même hauteur `H`. |
| Fond | **Un bloc par couleur, fond inclus** (surface pleine, aucun vide). |
| Base | **Plaque plus large** que la géométrie (marge param.) **+ mur périmétrique** encadrant les blocs. |
| Origine Z | Blocs montant depuis **z=0** (le haut de la base), relief. |
| Quantif. | **Wu** par défaut (`quant_wu(n)`). |
| Sortie | Multi-couleurs par défaut ; un-mesh-par-couleur en option (paramètre). |
| Palette de référence | **Phase 2** (hors périmètre v1). |

## 2. Constat de réutilisation (déjà présent)

Aucune brique algorithmique nouvelle. Existant réutilisé :

| Étape | Existant | Fichier |
|---|---|---|
| Chargement | `Img::load` (PNG/JPG/TGA/P*M via stb) | `cgimg/image.cpp`, `image_io.h` |
| Quantification | `Img::quant_wu(n)` (+ heckbert/kmean) | `cgimg/image.cpp`, `image_quantization_wu.h` |
| Palette | `Palette`, `Color`, `Img::get_palette`, `palettize` | `cgimg/palette.h`, `color.h` |
| Segmentation + vectorisation | `CLitRasterToVector::Vectorize(Img*, mask, useMask, Palette*)` — contours, trous (`RemoveHoles`), simplification, lissage, adjacence, couleur par path (`m_pathColor`) | `cgmesh/image_vectorization.{h,cpp}` |
| Tessellation + extrusion (caps + murs + trous) | logique de `import_svg_extruded` (glutess, winding nonzero/evenodd, caps bas/haut, murs à normales sortantes) | `cgmesh/import_svg.cpp` |
| Booléens/offset polygones | Clipper2 | `extern/clipper2/*` |
| Mesh multi-matériaux | `Material`, `Material_Add`, `SetFaceMaterialId`, `MaterialRange` (rendu multi-matériaux en un VBO), couleurs sommets | `cgmesh/mesh.h` |

Le travail est donc : **(a) un accesseur structuré sur le vectoriseur**, **(b) un refactor de l'extrudeur d'`import_svg` en fonction réutilisable**, **(c) un orchestrateur** qui assemble régions + base + mur en un mesh multi-matériaux.

## 3. API cible (nouveau fichier `cgmesh/image_relief.{h,cpp}`)

```cpp
#pragma once
#include <string>
#include <vector>

class Mesh;

enum class QuantAlgo { Wu, Heckbert };

struct ImageReliefOptions
{
    // --- Quantification / vectorisation ---
    int       maxColors     = 16;            // nombre de couleurs max
    QuantAlgo algo          = QuantAlgo::Wu; // Wu par défaut
    float     simplifyErr   = 1.0f;          // simplification des contours (px)

    // --- Mise à l'échelle XY ---
    // L'empreinte de l'image (contenu) est mise à l'échelle pour que sa plus
    // grande dimension XY vaille fitSize. Toutes les longueurs ci-dessous sont
    // exprimées dans cette même unité monde.
    float     fitSize       = 1.0f;

    // --- Géométrie du relief ---
    float     blockHeight   = 0.10f;         // hauteur uniforme H des blocs (au-dessus de la base)
    float     baseThickness = 0.05f;         // épaisseur de la plaque de base
    float     margin        = 0.05f;         // jeu entre le contenu et la face interne du mur
    float     wallThickness = 0.03f;         // épaisseur du mur périmétrique
    float     wallHeight    = 0.10f;         // hauteur du mur (défaut = blockHeight)

    // --- Sortie ---
    bool      oneMeshPerColor = false;       // false = un mesh multi-couleurs (défaut)
    bool      emitInternalWalls = true;      // v1: chaque bloc = solide fermé (murs internes coïncidents tolérés)

    // --- Matériaux base/mur ---
    // (couleurs par défaut si non fournies)
    // Color baseColor, wallColor;           // à préciser à l'implémentation
};

// Sortie multi-couleurs (défaut). Retourne nullptr en cas d'échec.
Mesh* image_to_relief(const std::string& filename, const ImageReliefOptions& opt);

// Sortie un-mesh-par-couleur : chaque couleur = un solide fermé.
// (La base + le mur sont retournés comme mesh(es) additionnel(s) ; convention
//  d'ordre à figer à l'implémentation.)
std::vector<Mesh*> image_to_relief_per_color(const std::string& filename,
                                             const ImageReliefOptions& opt);
```

## 4. Modèle géométrique

Repère : Y-up (comme le viewer), image « à l'endroit » (flip Y du repère image).

```
  z
  ^                        wallHeight
  |        ┌──┐  ┌──┐         ┌──┐  <- mur (anneau rectangulaire)
  |    ┌───┤R ├──┤ V├───┐     │  │
  | H  │   └──┘  └──┘   │     │  │     <- blocs (top coplanaire à z = baseThickness + H)
  |    │  contenu (W×H) │     │  │
  +----┴────────────────┴─────┴──┴--- z = baseThickness  (haut de la base = origine des blocs)
  |    ███████████████████████████    <- plaque de base pleine
  +----------------------------------- z = 0
       |<-- contenu -->|<-margin->|<-wallThickness->|
```

- **Contenu** : empreinte de l'image, `W×H` px → mis à l'échelle (`fitSize`). Chaque région de couleur est extrudée en solide fermé `z ∈ [baseThickness, baseThickness + H]`.
- **Base** : rectangle plein `z ∈ [0, baseThickness]`. Dimensions = contenu `+ (margin + wallThickness)` de chaque côté.
- **Mur** : anneau rectangulaire (contour externe = bord de la base ; contour interne = contenu `+ margin`), `z ∈ [baseThickness, baseThickness + wallHeight]`.
- **Hauteur uniforme + fond inclus** ⇒ le dessus des blocs est un plan unique ; la différenciation est **la couleur** (matériau), pas l'altitude.

## 5. Étapes d'implémentation

### Étape 0 — Repérage du modèle de trous du vectoriseur *(0.5 j)*
Inspecter `image_vectorization.cpp` (`RemoveHoles`, `WriteFilePolygonWithHole`) pour établir **comment un trou est représenté** dans `m_mapListPath`/`m_mapCoord` (contour imbriqué, orientation, marquage). Prérequis à l'étape 1.
- **Livrable** : note interne + décision sur l'orientation des contours (extérieur CCW / trous CW pour un winding NONZERO côté extrudeur).

### Étape 1 — Accesseur structuré sur `CLitRasterToVector` *(1 j)*
Ajouter un accesseur exposant, **par couleur**, les contours en coordonnées finales :
```cpp
struct VectorContour { std::vector<Vector2f> pts; bool isHole; };
struct VectorLayer   { Color color; std::vector<VectorContour> contours; };
std::vector<VectorLayer> GetLayers() const;   // construit depuis m_mapListPath / m_mapCoord / m_pathColor
```
- Réutilise le classement trou/extérieur de l'étape 0. Aucune modif de l'algo de vectorisation.
- **Test** : sur l'image « fond blanc + carré rouge » (cf. `tu_cgimg_img.vectorization_detects_regions`), `GetLayers()` doit rendre 2 layers, le layer non-fond ayant 1 contour extérieur.

### Étape 2 — Refactor de l'extrudeur d'`import_svg` en primitive réutilisable *(1 j)*
Extraire de `import_svg.cpp` (aujourd'hui privé au fichier) une primitive qui **tesselle + extrude un jeu de contours** dans un buffer partagé, à Z arbitraire et avec un matériau :
```cpp
// Ajoute au builder les caps (bas/haut) + murs pour 'contours' (avec trous via winding),
// géométrie à z ∈ [zBottom, zTop], faces marquées 'materialId'.
void appendExtrudedContours(MeshBuilder& b,
                            const std::vector<VectorContour>& contours,
                            float zBottom, float zTop,
                            unsigned int materialId,
                            bool emitWalls);
```
- Généralise `buildExtrudedMesh` (actuellement : nouveau Mesh, z∈[0,h], normales caps/murs déjà gérées, COMBINE glutess géré).
- `import_svg_extruded` est réécrit par-dessus cette primitive (non-régression garantie par ses tests existants).
- **Test** : un carré unité → boîte fermée `[zBottom,zTop]`, 12 triangles (2 caps + 4 murs×... ) manifold, normales sortantes.

### Étape 3 — Orchestrateur `image_to_relief` (multi-couleurs) *(1–2 j)*
Chaîne complète :
1. `Img::load` → `quant_wu(maxColors)` (ou heckbert) → palette.
2. `CLitRasterToVector::Vectorize(&img, mask, /*useMask=*/false, palette)` (fond inclus).
3. `GetLayers()` → transformer chaque contour pixel→monde (flip Y, scale `fitSize`, offset pour centrer).
4. Pour chaque layer : `Material_Add(couleur)` → `appendExtrudedContours(..., baseThickness, baseThickness+H, matId, emitInternalWalls)`.
5. **Base** : `appendExtrudedContours({rect_base}, 0, baseThickness, matBase, walls=true)`.
6. **Mur** : `appendExtrudedContours({rect_ext, rect_int(hole)}, baseThickness, baseThickness+wallHeight, matMur, walls=true)`.
7. Finaliser le Mesh : `ComputeNormals`, regrouper les faces par matériau et remplir `materialRanges` (rendu multi-matériaux en un VBO).
- **Test** : image 32×32 (fond + carré) → mesh non vide, `GetNMaterials() == nb_couleurs + 2` (base+mur), bbox = contenu + 2·(margin+wallThickness) en XY, `zmax == baseThickness + max(H, wallHeight)`.

### Étape 4 — Variante `image_to_relief_per_color` *(0.5 j)*
Même chaîne, mais un `Mesh` par layer (chaque couleur = solide fermé) + mesh(es) base/mur ; convention d'ordre documentée.
- **Test** : `size() == nb_couleurs (+ base/mur)` ; chaque mesh couleur watertight.

### Étape 5 — Qualité des bords inter-régions *(0.5–1 j, optionnel v1.1)*
Les régions voisines vectorisées indépendamment peuvent laisser slivers/recouvrements.
- Atténuation : offset Clipper2 léger et/ou snapping via la **carte d'adjacence déjà calculée** (`BuildAdjacencyMap`).
- Optimisation liée : suppression des **murs internes coïncidents** entre blocs de même hauteur (`emitInternalWalls=false`) via l'adjacence → mesh plus propre. Reporté hors v1 par défaut.

### Étape 6 — Intégration build + tests + démo *(0.5 j)*
- `CMakeLists.txt` cgmesh : ajouter `image_relief.cpp`.
- Tests dans `test/tu_cgimg_img.cpp` (ou nouveau `tu_image_relief.cpp`).
- Export du mesh (`.ply`/`.gltf`) pour inspection visuelle dans le viewer.

**Estimation totale : ~4 à 5 jours** (câblage + refactor, pas de recherche).

## 6. Décisions restantes / points à confirmer

Aucun blocage : les points ci-dessous sont **exposés en paramètres** (pas d'hypothèse silencieuse). À confirmer si l'on veut d'autres défauts :

1. **`wallHeight` par défaut** = `blockHeight` (mur affleurant le sommet des blocs). Alternative : mur dépassant (rebord). → paramètre.
2. **`margin`** défini comme le **jeu entre le contenu et la face interne du mur** (la base externe = contenu + margin + wallThickness). À valider (autre convention possible : margin = bord externe total).
3. **Murs internes** entre blocs de même hauteur : v1 `emitInternalWalls=true` (chaque bloc = solide fermé, faces internes coïncidentes tolérées car cachées). Passage à `false` = mesh manifold plus propre (étape 5).
4. **Couleurs base/mur** : défaut à fixer (gris neutre ?) ou paramètres `baseColor`/`wallColor`.
5. **Mise à l'échelle XY** : `fitSize` normalise la plus grande dimension du contenu à 1.0 et **préserve le ratio** de l'image. À confirmer (vs. `pixelSize` absolu).

## 7. Risques

- **Explosion du nombre de contours** sur photos peu quantifiées → mesh lourd. Mitigation : `maxColors` bas, `simplifyErr`, filtrage des régions sous une aire minimale (paramètre à ajouter si besoin).
- **Étanchéité des frontières** (étape 5) : principal point de qualité géométrique.
- **Orientation des trous** : dépend de l'étape 0 ; à câbler correctement sur le winding glutess (NONZERO).
- **Cohérence des unités** : toutes les longueurs dans l'unité `fitSize` ; centraliser la transformée pixel→monde.

## 8. Notes d'implémentation (rétrospective)

### Étape 0 — modèle de trous : résolu par l'orientation, pas par l'imbrication

`RemoveHoles` est **commenté** dans `GeneratePath` (`image_vectorization.cpp`), donc
`m_mapListPath[i]` contient déjà *tous* les contours fermés de la couleur *i* — bords extérieurs
**et** bords de trous. Aucune modification de l'algo de vectorisation n'a été nécessaire.

Le classement trou/extérieur se fait par **aire signée**, non par parité point-dans-polygone : le
tracé (`miniStripElement`) tourne toujours autour de la couleur dans le même sens et les fusions
(`AddPath`/`MergePath`) préservent le sens, donc dans le repère image (x droite, y **vers le bas**)
un contour extérieur a une aire **négative** et un trou une aire **positive**. `SmoothCoords` et
`Simplify` sont affines à échelle positive → le signe survit. Deux avantages sur la parité : les
contours d'une même couleur peuvent se toucher en un point (cas diagonaux 6 et 9 de
`miniStripElement`), ce qui casse un test de containment sur sommet ; et l'imbrication arbitraire
(îlot de même couleur dans un trou) est gérée gratuitement.

### Étape 5 — murs internes : implémentée (exacte), pas seulement atténuée

`emitInternalWalls = false` supprime réellement les murs coïncidents, sans passer par Clipper2 ni
par `BuildAdjacencyMap` : `m_mapCoord` est indexé par point de grille et `Simplify` opère sur la
carte d'adjacence **globale** (toutes couches confondues) puis filtre tous les chemins, donc deux
couches adjacentes portent **exactement** la même polyligne le long de leur frontière commune. Un
simple comptage d'arêtes sur toutes les couches distingue donc à coup sûr une arête de silhouette
(vue 1 fois) d'une frontière interne (vue 2 fois). Mesuré sur `ouaf.png` (399×438, 6 couleurs) :
33 738 → 17 216 faces, soit **−49 %**. Le défaut reste `true` (blocs = solides fermés).

Les sommets de murs devenus orphelins sont compactés dans `ExtrudedMeshBuilder::Build()` — sans
quoi la moitié du mesh aurait été des sommets inutilisés à normale nulle.

### Bug corrigé en amont : `Img::quant_wu` non déterministe

`MedianCut_Wu` (`cgimg/image_quantization_wu.cpp`) remettait à zéro `mr/mg/mb/m2` mais **pas `wt`**,
alors que `Hist3d` accumule dans les cinq (« NB: these must start out 0! »). Un deuxième appel à
`quant_wu` dans le même process héritait des poids de l'image précédente : découpage de boîtes et
palette corrompus, donc nombre de couches variable d'un appel à l'autre. Une ligne ajoutée au
reset. Sans ce correctif, `image_to_relief` n'était pas appelable deux fois de suite.

Effet de bord également neutralisé : `CalculateLayerOrder` écrivait `sOneColorByPolygon.bmp` dans le
cwd à chaque vectorisation (bloc de debug passé sous `if (0)`, comme les autres du fichier).

### Fidélité de la segmentation : lisser AVANT de quantifier

La cause première n'est ni la palette ni la géométrie, c'est que **les entrées réelles sont
rééchantillonnées**. `obama.jpg` porte des EXIF Photoshop CS2 et une résolution asymétrique
98×106 dpi : chaque bord arrive donc **anticrénelé sur plusieurs pixels**. Mesuré sur ce fichier :

| Mesure | Valeur |
|---|---|
| Pixels valant **exactement** une des 4 couleurs de l'affiche | **32,4 %** |
| Pixels à plus de 20 unités de **toute** couleur de la palette | **17,8 %** |
| Zones plates | pixels exacts, distance 0 |
| Zones de transition | rampes lisses sur ~10 px |

Trancher ces pixels ambigus **un par un** fait errer la frontière de région pixel après pixel : la
segmentation cesse de suivre les formes de l'original (symptôme rapporté : « la segmentation est
éloignée de l'image d'origine, surtout au niveau de la joue »). Aucun nettoyage *après*
quantification ne peut réparer ça — l'information a été détruite au moment de la décision.

D'où `preSmoothPasses` (défaut 1) : `Img::bilateral_filtering()` **avant** `quant_wu`. Le bilatéral
préserve les bords tout en aplatissant l'intérieur des régions, donc chaque rampe anticrénelée
bascule de façon cohérente d'un côté ou de l'autre et la frontière atterrit sur une courbe propre.
Ses paramètres sont fixes (σ_D = 5, σ_R = 100, fenêtre 7×7) ; sur un bord franc rouge/blanc le poids
de similarité vaut `exp(-6,5) ≈ 0,0015`, donc une image de synthèse propre est laissée intacte (les
tests unitaires passent sans modification).

Effet mesuré sur `obama.jpg` à 4 couleurs, en cumul avec l'anti-mouchetis ci-dessous :

| Réglages | Faces | Couverture |
|---|---|---|
| `ps=0 dp=0 ma=0` (brut) | 10 418 | 100,000 % |
| `ps=0 dp=1 ma=12` (anti-mouchetis seul) | 7 684 | 100,000 % |
| **`ps=1 dp=1 ma=12` (défauts)** | **5 360** | **100,000 %** |
| `ps=2 dp=2 ma=25` | 4 880 | 100,000 % |

Composantes connexes : 533 (brut) → 264 (1 passe de bilatéral) → 228 (2 passes). Coût ≈ 100 ms par
passe en WASM optimisé sur 375×564.

### Raffinement de palette (k-means) : le meilleur rapport gain/coût du pipeline

Wu et Heckbert prennent **tous deux** leurs décisions sur un histogramme 5 bits/canal : Wu affecte
chaque pixel via ses boîtes 32³, Heckbert via le centroïde de cube. Un pas de Lloyd (k-means) rejuge
ensuite chaque pixel en **RGB 8 bits pleins** et recalcule chaque couleur comme la moyenne de ses
pixels — exactement le critère que minimise l'erreur quadratique, donc la MSE décroît de façon
monotone. Implémenté dans `Img::quant_refine(reference, iterations)` (`cgimg`), exposé par
`ImageReliefOptions::refineIterations` (défaut 3).

MSE mesurée sur `obama.jpg` (4 couleurs, après bilatéral) :

| n | Wu | Wu + raffinement | Heckbert | Heckbert + raffinement |
|---|---|---|---|---|
| 4 | 199,0 | **187,3** | 868,8 | **187,3** |
| 6 | 121,0 | 113,8 | 599,6 | 114,4 |
| 8 | 84,2 | 75,8 | 452,4 | **70,7** (mieux que Wu) |
| 16 | 36,4 | 29,3 | 105,8 | 36,2 |

Deux conséquences :

1. **L'écart Wu / Heckbert disparaît.** À 4 couleurs les deux convergent vers la *même* palette, au
   composant près : `(10,49,77) (127,160,168) (202,31,39) (242,222,168)`. La palette initiale ne
   décide plus de la qualité, seulement du minimum local atteint (à n=8 Heckbert finit même devant).
   Le choix du quantifieur devient un détail.
2. **Le coût est négatif.** Mesuré dans le pipeline WASM à 4 couleurs : Heckbert 8 848 → 5 348 faces
   et 615 → 604 ms. Le raffinement rend la segmentation plus propre, donc il y a *moins* de contours
   à vectoriser et à extruder : il se paie tout seul.

Convergence : 1 itération suffit à 4 couleurs, ~3 à 16 ; à 8 ça grappille encore jusqu'à 12
(75,8 → 69,1). D'où le défaut à 3. La couverture du plan supérieur reste à 100,000 %.

Ordre imposé dans le pipeline : **lissage → quantification → raffinement → anti-mouchetis**. Le
raffinement réaffecte chaque pixel au plus proche, donc le passer *après* l'anti-mouchetis
réintroduirait le mouchetis qu'on vient d'ôter.

### Anti-mouchetis : à faire sur les ÉTIQUETTES, jamais sur les contours

Une source lossy est le cas normal et elle détruit les frontières de régions. Le ringing JPEG floute
un bord net en dégradé ; le seuillage dur à N couleurs transforme ce dégradé en **hachures de 1 px**.
Mesuré sur `obama.jpg` (375×564, JPEG de l'affiche « Hope » à 4 couleurs) :

| Mesure | Valeur |
|---|---|
| Couleurs distinctes dans le JPEG | **39 155** (l'affiche en a 4) |
| Part des 4 couleurs d'origine | 32,4 % des pixels |
| Après `quant_wu(4)` | 4 couleurs, mais **533 composantes connexes** |
| Composantes ≤ 200 px | 497 — dont **157 intérieures** et **340 à cheval** sur deux couleurs |

**Le filtrage des contours vectorisés par aire ne marche pas** et a été retiré. Le raisonnement
« un mouchetis de couleur A dans B produit deux contours de même aire (le bord de A et le trou
jumeau de B), donc un seuil uniforme les supprime ensemble et la surface reste fermée » n'est vrai
que pour un mouchetis **strictement intérieur** à une seule région. Or 340 des 497 petites régions
sont *à cheval* sur une frontière : aucun voisin ne porte de trou jumeau, et les supprimer **perce
de vrais trous** (couverture mesurée tombée à 99,80 %). Les hachures sont en plus **allongées**, donc
un seuil d'aire ne les attrape pas sans détruire de la géométrie utile.

La version retenue travaille sur l'image d'**étiquettes** (le résultat de la quantification), avant
vectorisation, et ne fait que remplacer une étiquette par celle d'un voisin. L'étiquetage reste donc
un **pavage complet** : impossible d'ouvrir un trou, par construction.

- `despecklePasses` (défaut 1) : passes d'un filtre **majoritaire 3×3**. C'est ce qui enlève les
  hachures — une structure fine perd le vote face à son entourage quelle que soit sa longueur, ce
  qu'un seuil d'aire ne peut pas faire. Égalité ⇒ on garde le pixel central, pour ne pas faire
  dériver une vraie frontière.
- `minRegionArea` (défaut 12 px) : après les passes majoritaires, toute composante connexe encore
  plus petite est **absorbée dans sa couleur voisine majoritaire** (jamais supprimée).

Résultat sur `obama.jpg` à 4 couleurs : 533 → 271 composantes (1 passe) → 195 (2 passes), 1,09 % des
pixels modifiés ; mesh 10 418 → 7 684 faces aux défauts, et **couverture du plan supérieur =
100,000 % à tous les réglages** (contre 99,80 % avec l'ancien filtre par aire).

> Piège de mesure : avec les défauts `wallHeight == blockHeight`, le haut du **mur** est au même z que
> le haut des blocs. Toute vérification de couverture doit l'exclure (par son matériau), sinon elle
> gonfle l'aire *et* la bbox — ce qui a d'abord produit un faux « chevauchement de 17 % ».

### Retrait des régions (`shrink`) — offset négatif après vectorisation

`ImageReliefOptions::shrink` érode chaque polygone de contour de N px vers l'intérieur, via
`Clipper2Lib::InflatePaths(paths, -shrink, JoinType::Miter, EndType::Polygon)`. Deux blocs voisins
cessent d'être jointifs : il reste un sillon de `2*shrink` qui laisse voir la plaque de base, donc
chaque région se lit comme une pièce rapportée (et l'assemblage gagne une tolérance à l'impression).
Défaut 0 = géométrie inchangée.

Quatre points non évidents :

1. **Une seule passe pour TOUTE la couche.** Les contours d'une couche (extérieurs + trous) sont
   passés ensemble à `InflatePaths` : sur un polygone à trous, un delta négatif érode le contour
   extérieur **et dilate les trous**, c'est-à-dire retire de la matière des deux côtés. Contour par
   contour, les trous seraient rognés et la région grossirait par l'intérieur.
2. **L'orientation est imposée explicitement.** Clipper2 déduit le sens de poussée du signe de
   l'aire (extérieur positif, trou négatif) — l'inverse de la convention interne
   (`VectorContour::area`, extérieur négatif en repère image). On force le signe depuis `isHole`
   plutôt que d'enchaîner deux conventions, puis on renverse au retour et on recalcule `area` /
   `isHole`. `InflatePaths` peut scinder ou supprimer des contours : la topologie de sortie est
   relue, jamais supposée.
3. **En PIXELS source, pas en unités monde.** Cohérent avec `simplifyErr` et `minRegionArea`, et
   surtout `PathsD` arrondit **par défaut à 2 décimales** : en coordonnées monde (ordre de 0,5) tout
   s'effondrerait sur une grille de 100×100. On passe `precision = 4` et on travaille avant la
   transformée pixel→monde.
4. **La bbox est mesurée AVANT le retrait.** Sinon rétrécir la couche de fond rétrécirait
   l'empreinte, que le recadrage à `fitSize` regrandirait ensuite : le paramètre se mordrait la
   queue. Vérifié : bbox constante à 0,825×1,160 pour `shrink` ∈ {0 ; 0,5 ; 1 ; 2 ; 4}.

Mesures sur `obama.jpg` à 4 couleurs (aire du dessus des blocs / empreinte) :

| shrink (px) | 0 | 0,5 | 1 | 2 | 4 |
|---|---|---|---|---|---|
| aire des blocs | 99,93 % | 94,51 % | 89,25 % | 79,24 % | 61,66 % |
| faces | 5 396 | 5 452 | 5 356 | 4 792 | 3 588 |

Deux effets voulus : une région plus fine que `2*shrink` **disparaît** (et sa couche avec, si elle
n'a rien d'autre) ; et les blocs ne partageant plus d'arête, `emitInternalWalls` n'a plus rien à
dédupliquer et tous les murs sont émis — ce sont désormais les flancs visibles des sillons.

`JoinType::Miter` (limite 2) et non `Round` : conserve les angles vifs, ce qui va bien à une affiche,
et n'ajoute aucun sommet d'arc — donc pas d'inflation du maillage après le `Simplify` du vectoriseur.

### Décisions figées sur les points ouverts du §6

| Point | Décision |
|---|---|
| 1. `wallHeight` | Paramètre, défaut `0.10f` = défaut de `blockHeight` (mur affleurant). |
| 2. `margin` | Jeu entre le contenu et la face **interne** du mur ; base externe = contenu + margin + wallThickness. |
| 3. Murs internes | `emitInternalWalls = true` par défaut ; `false` opérationnel (cf. ci-dessus). |
| 4. Couleurs base/mur | Paramètres `baseColor` (160,160,160) / `wallColor` (120,120,120). |
| 5. Échelle XY | `fitSize` normalise la plus grande dimension du contenu, ratio préservé. |
| Ordre `per_color` | `[couleur_0 … couleur_N-1, base, mur]`, couleurs dans l'ordre d'index de palette, un `Material` par mesh. |
| `maxColors` | Borné à [2, 250] : `Img::palettize` stocke un index sur **un octet** et le vectoriseur réserve l'index suivant pour son anneau de bordure. |

### API réellement livrée pour l'étape 2

`ExtrudedMeshBuilder` (classe accumulant plusieurs jeux de contours) plutôt qu'une fonction libre
`appendExtrudedContours(MeshBuilder&, …)` — il n'existait pas de `MeshBuilder` dans la codebase. Les
contours sont attendus **en XY monde final** (flip Y et mise à l'échelle à la charge de l'appelant),
ce qui garde la primitive purement géométrique ; `import_svg_extruded` est réécrit par-dessus et ses
tests existants (`tu_cgmesh_svg.cpp`) passent à l'identique. `extrude_contours.cpp` a été ajouté à
la liste explicite `if (EMSCRIPTEN)` du CMakeLists (build WASM `maker` vérifié).

### Vérification

968/968 tests passent (`TU.exe`), dont 7 nouveaux pour ce module. Contrôles numériques sur
`ouaf.png` : les seuls plans Z du mesh sont `{0, baseThickness, baseThickness+blockHeight,
baseThickness+wallHeight}` (le relief part bien du **haut** de la base) ; les caps supérieurs ont
une orientation +Z homogène (aire signée == aire absolue) et couvrent 99,88 % du rectangle de
contenu — le déficit de 0,12 % est l'arrondi des coins par le lissage Taubin, pas des trous.
