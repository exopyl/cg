# Import SVG multi-formes avec couleurs — étude de faisabilité

**Périmètre** : `src/cgmesh` (import SVG, extrusion, matériaux), `src/cggraph/nodes/svg`, `maker`
(WASM + web).
**Nature** : analyse seule (recette `feasibility-analysis`). Aucune modification de code, aucun
commit. L'implémentation relève du sous-agent `developer`, après validation des pistes.
**Date** : 2026-09-05.

Convention de statut, appliquée ligne à ligne :

- **[V]** vérifié par lecture du code, avec `fichier:ligne` ;
- **[M]** mesuré, avec l'instrument nommé ;
- **[E]** estimé, avec la méthode et l'ordre de grandeur ;
- **[H]** hypothèse à reproduire par exécution (route : sous-agent `debugger`).

> ⚠ **RÉVISION DU 2026-09-06.** L'utilisateur a tranché les cinq points ouverts. Les sections 1 à 9
> ci-dessous sont l'**analyse d'origine**, conservée intacte pour la traçabilité des pistes écartées.
> **Le plan qui fait foi est la PARTIE 2** (sections 10 à 15, en fin de document) : M2a passe dans le
> MVP, M2b' est écarté, la séquence de jalons est refaite. En cas de divergence entre la partie 1 et
> la partie 2, **la partie 2 prime**.
>
> ⚠ **K0 EXÉCUTÉ le 2026-09-06** (`test/tu_cgmesh_svg_k0_metrics.cpp`, build Release MSVC, 20/20
> tests SVG verts sous ctest). Le document n'est plus « analyse seule sans exécution » : la
> **PARTIE 3 (sections 16 à 20)** porte les mesures réelles et **corrige** les sections 3, 5.1, 11.4,
> 12, 13, 14 et 15. Ordre de préséance : **partie 3 > partie 2 > partie 1**.
>
> ⚠ **D7 — 2026-09-06.** K3 est fait et vert (`ctest` 1686/1686). Nouvelle contrainte utilisateur :
> « il n'est pas permis de perdre des informations du SVG » — le trait des formes fermées ET
> remplies doit être produit (**65 formes sur 239**). Voir **PARTIE 4 (sections 21 à 25)** : décision
> D7, conception, jalon **K3b** inséré avant K4, et une alerte — **D7 remet en cause le seuil de D6
> avant même que K4 ne tourne**.
>
> ⚠ **K3b EXÉCUTÉ le 2026-09-06** (`test/tu_cgmesh_svg_k3b_stroke_rings.cpp`, 10 tests, build Release
> MSVC, `ctest` **1696/1696**). La **PARTIE 5 (section 26)** porte les mesures réelles et **corrige**
> les sections 22.4 et 22.7, dont les estimations sont pessimistes d'un facteur ~1,8. Préséance :
> **partie 5 > partie 4 > partie 3 > partie 2 > partie 1**.
>
> ⚠ **K4 EXÉCUTÉ le 2026-09-06** (`test/tu_cgmesh_svg_k4_timing.cpp` en natif,
> `maker/tools/k4_probe/` en WebAssembly, `ctest` **1700/1700**). La **PARTIE 6 (section 27)** porte
> le chronométrage des deux côtés, **confirme R6** et **applique la règle de bascule sans prendre la
> décision** : 427,6 ms au navigateur, au-dessus du seuil de ~300 ms, poste (b) à 93,4 %. Préséance :
> **partie 6 > partie 5 > partie 4 > partie 3 > partie 2 > partie 1**.
>
> ⚠ **D8 — 2026-09-06.** L'utilisateur a tranché : **le repli n°1 (indexation spatiale) SEUL**, et
> **pas** le mémo (option A), précisément parce que le repli rend R6 sans objet. Jalon **K4b**, inséré
> entre K4 et K5 comme K3b l'a été entre K3 et K4 — aucune renumérotation.
>
> ⚠ **K4b EXÉCUTÉ le 2026-09-06** (`test/tu_cgmesh_svg_k4b_indexed_subtract.cpp`, 5 tests, `ctest`
> **1705/1705**). La **PARTIE 7 (section 28)** porte les mesures et **corrige le §27.4** : le facteur
> attendu de 9,6 est **infirmé**, le facteur réel est **1,9 au navigateur**. Le total y tombe
> néanmoins de 427,6 à **244,3 ms**, sous le seuil. Préséance : **partie 7 > partie 6 > partie 5 >
> partie 4 > partie 3 > partie 2 > partie 1**.

---

## 1. Résumé exécutif

### Ce qui bloque aujourd'hui

Trois causes distinctes, indépendantes, toutes vérifiées par lecture. Aucune n'est un bug : ce sont
des choix de conception explicites, documentés dans les en-têtes, dont la conséquence combinée est le
symptôme observé.

1. **La peinture n'est jamais lue.** `svg_to_contours` ne consulte `NSVGshape::fill` que pour un test
   booléen `!= NSVG_PAINT_NONE` (`src/cgmesh/import_svg.cpp:276`) ; `fill.color`, `shape->opacity` et
   `shape->stroke.color` ne sont lus nulle part dans le fichier. L'image nanosvg est détruite à
   `import_svg.cpp:356`, **avant** la boucle qui produit les contours : la couleur n'est plus
   accessible en aval, y compris à l'intérieur de la fonction.

2. **L'identité de forme est détruite par construction.** La sortie est un
   `std::vector<ExtrudeContour>` **plat**. Les contours de chaque forme sont résolus séparément
   (`resolveShape`, `import_svg.cpp:188-220`) puis *concaténés* (`import_svg.cpp:394` :
   `out.insert(out.end(), region.begin(), region.end())`). L'en-tête assume ce choix et en donne la
   raison (`src/cgmesh/import_svg.h:110-113`) : une liste plate ne peut pas transporter la règle de
   remplissage, donc on la résout avant. Le corollaire non énoncé est qu'elle ne peut pas non plus
   transporter la couleur.

3. **Un seul `Append`, donc une seule tessellation pour tout le document.**
   `import_svg_extruded` appelle `builder.Append(contours, ao)` une seule fois
   (`import_svg.cpp:420-421`), et `tessellateContours` place *tous* les contours reçus dans **un
   unique polygone glutess** (`gluTessBeginPolygon` en `src/cgmesh/extrude_contours.cpp:182`,
   `gluTessEndPolygon` en `:226`). Avec la règle NONZERO, deux formes qui se recouvrent
   **interagissent** : de même sens elles fusionnent, de sens opposés elles se soustraient. Le
   résultat n'est donc pas seulement « non séparé » : c'est une région unique dont la frontière ne
   correspond à aucune forme du document.

### Ce qui est en revanche **déjà en place**, de bout en bout

La chaîne « un matériau par groupe de faces → JavaScript → three.js » est **complète et
opérationnelle**, utilisée aujourd'hui par le relief d'image et les blocs pixelisés :

`ExtrudeAppendOptions::materialId` (`extrude_contours.h:48`) → `Tri::materialId` →
`Face::SetMaterialId` (`extrude_contours.cpp:428`) → `Mesh::BuildPolygonRenderData` et sa table
`materialRanges` (`mesh.cpp:757`, `:917-926`) → `maker::BuildMeshPayload` et ses champs `groups` /
`materials` (`maker/mesh_payload.cpp:106-119`) → `graphMeshData` (`maker/wasm_api.cpp:775-783`) →
`viewer.js` `useGroups` / `buildOwnMaterials` (`maker/web/js/viewer.js:275-288`, `:83-120`). **[V]**

Il n'y a donc **rien à construire** entre le maillage et l'écran. Le travail est entièrement contenu
dans `import_svg.cpp`, plus un point d'entrée côté graphe.

### Précision décisive sur `maker/web/svg.html`

`svg.html` n'appelle **pas** `createSvgExtrusion`. C'est une page **gabarit** sans code propre
(`maker/web/svg.html:10-31`, `:121-131`) qui évalue le graphe
`maker/web/data/templates/svg.json` : `file.ref → svg.contours → shape.extrude → mesh.color`
(`svg.json:112-256`). **[V]** `createSvgExtrusion` (`wasm_api.cpp:265-278`) et son binding (`:804`)
restent exportés mais ne sont plus consommés par cette page — l'en-tête du gabarit le dit
explicitement (`svg.json:7-9`).

Conséquence : **le bloc monolithique est le résultat nominal du graphe actuel**. `mesh.color` peint le
maillage d'une seule couleur — et ne peint **que** les faces sans matériau
(`src/cggraph/nodes/mesh/color.cpp:114`), ce qui le rend **compatible par construction** avec un
maillage déjà peint par formes : il n'écrasera rien.

---

## 2. Cartographie de l'existant

### 2.1 Réutilisable tel quel — aucun développement

| Brique | Emplacement | Ce qu'elle apporte |
|---|---|---|
| Matériau par lot d'extrusion | `src/cgmesh/extrude_contours.h:48` (`ExtrudeAppendOptions::materialId`, défaut `(unsigned)-1` = `MATERIAL_NONE`) | Un `Append` porte **déjà** un matériau ; plusieurs `Append` sur un même builder produisent un maillage multi-matériaux (en-tête `extrude_contours.h:14-15`) **[V]** |
| Estampillage des faces | `extrude_contours.cpp:307-308` (capots), `:370-377` (parois), `:428` (`SetMaterialId`) | Capots **et** parois d'un même `Append` reçoivent le même `materialId`. Il n'y a **aucun** étiquetage distinct capot / paroi **[V]** |
| Table de matériaux | `src/cgmesh/mesh.h:639-676` (`GetNMaterials`, `Material_Add`, `GetMaterial`), `:740` (`m_faceMaterial`), `:799` (`m_materials`) | Palette possédée par le `Mesh`, un id par face **[V]** |
| Matériau couleur | `src/cgmesh/material.h:92-111` | `MaterialColor(r,g,b,a)`, **alpha stocké** (`m_a`, `material.h:110`) **[V]** |
| Groupes d'indices par matériau | `src/cgmesh/mesh.h:144-160`, `mesh.cpp:757-929` | `PolygonRenderData::materialRanges`, une plage par matériau présent **[V]** |
| Charge utile WASM | `maker/mesh_payload.h:54-65`, `mesh_payload.cpp:74-133` | `groups` + `materials` avec `kind: "color" \| "texture" \| "none"`. **Le chaînon couleur vers le JS est déjà en place** — c'était à vérifier, c'est vérifié **[V]** |
| Rendu multi-matériaux | `maker/web/js/viewer.js:275-288` (`useGroups`), `:83-120` (`buildOwnMaterials`), `:315-328` | `geometry.addGroup` + tableau de matériaux three.js, **déjà honoré** pour le relief d'image et les blocs pixelisés **[V]** |
| Précédent complet à imiter | `src/cgmesh/image_relief.h:184-197`, `image_relief.cpp:170-197` (`appendLayer`), `:223-227` (`makeColorMaterial`), `:305-316` (`Material_Add` par couche) | « Un `Mesh`, une palette, un `Append` par couche colorée » — exactement le modèle recherché, déjà en production **[V]** |
| Booléens 2D | `src/cgmesh/contour_ops.h:107` (`unionContours`), `:116` (`differenceContours`), `:123` (`intersectionContours`) | Clipper2 encapsulé, orientation Clipper2 (NonZero) garantie en sortie **[V]** |
| Concaténation de maillages | `src/cgmesh/mesh.cpp:1442-1549` (`Mesh::Append`) | Tables de matériaux concaténées avec décalage, `MATERIAL_NONE` préservé (`:1479-1483`). ⚠ **efface `m_vertexNormals` et `m_vertexColors`** (`:1530-1531`) **[V]** |
| UV planaires + matériau texturé | `src/cgmesh/image_region_pipeline.h:213` (`region_apply_planar_uvs`), `:225` (`region_make_texture_material`) | Tout ce qu'il faut pour M3b **sauf le rasteriseur** ; les deux sont compilés en WASM (`src/cgmesh/CMakeLists.txt:82`) **[V]** |

### 2.2 Manquant

| Manque | Où il faudra intervenir |
|---|---|
| Lecture de `fill.color` / `stroke.color` / `opacity` | `import_svg.cpp:269-354` (boucle sur les formes) |
| Une sortie qui porte l'identité de forme | `import_svg.h:117-118` (signature), `import_svg.cpp:378-395` (concaténation) |
| Découpage en plusieurs `Append` | `import_svg.cpp:420-421` |
| Construction de la palette | `import_svg.cpp:405-428`, sur le modèle de `image_relief.cpp:305-316` |
| Point d'entrée graphe produisant un maillage coloré | `src/cggraph/nodes/` (nouveau nœud) + `maker/web/data/templates/svg.json` |
| Résolution du recouvrement 2D | **entièrement absent**, quelle que soit la famille retenue |
| Rasteriseur SVG | `extern/nanosvg/` ne contient que `nanosvg.h` — `nanosvgrast.h` n'est **pas** vendorisé **[M : `ls extern/nanosvg/`]** |
| Alpha dans la charge utile | `mesh_payload.cpp:60-66` : `DescribeMaterial` n'émet que `r`, `g`, `b` **[V]** |
| Export coloré depuis une page gabarit | `wasm_api.cpp:785-798` : `graphExportObj` rend un **OBJ minimal sans `mtllib`**, utilisé par `template.js:390`. `exportObj(id)` (ZIP OBJ+MTL) n'existe que pour le chemin des **formes** **[V]** |

### 2.3 Contraintes héritées, remontées par lecture des déclarations

- `ExtrudeContour` (`extrude_contours.h:27-34`) est un type **partagé** : consommé par
  `text_extrude`, `contour_ops`, `extrude_profiled`, `boolean2d`, `image_relief`, et transporté par
  le type de graphe `extrudeContours` (`src/cggraph/nodes/value_types.h:141`), **dont le nom est
  sérialisé** (`value_types.h:6-7` : « le renommer casse les documents déjà écrits »). Toute
  modification de ce type est une modification de contrat public.
- Le maillage d'`ExtrudedMeshBuilder` a des sommets **volontairement disjoints** entre capots et
  parois (`extrude_contours.h:68-74`) ; les tests de trait s'appuient dessus
  (`test/tu_cgmesh_svg_stroke.cpp:38-41`).
- `Mesh::BuildPolygonRenderData` est appelée **à chaque rafraîchissement** de la vue
  (`maker/web/js/template.js:337`, `:406` → `graphMeshData` → `BuildMeshPayload` →
  `mesh_payload.cpp:83`). C'est la charge réelle, analysée en §5.3.
- `MATERIAL_NONE == (unsigned)-1` (`material.h:9`) : `mesh.color` ne peint **que** ces faces
  (`color.cpp:114`).

---

## 3. Ce que nanosvg fournit réellement — ⚠ comptes du fichier CORRIGÉS par §17.1 (K0)

Lecture de `extern/nanosvg/nanosvg.h`. **[V]**

| Élément | Ligne | Contenu exact |
|---|---|---|
| `NSVGpaintType` | `74-80` | `UNDEF=-1`, `NONE=0`, `COLOR=1`, `LINEAR_GRADIENT=2`, `RADIAL_GRADIENT=3` |
| `NSVGpaint` | `128-134` | `signed char type` + **union** `{ unsigned int color; NSVGgradient* gradient; }` |
| `NSVGgradient` | `120-126` | `xform[6]`, `spread`, `fx`, `fy`, `nstops`, `stops[1]` (tableau souple) |
| `NSVGgradientStop` | `115-118` | `unsigned int color`, `float offset` |
| `NSVGshape` | `145-167` | `id[64]`, `fill`, `stroke`, `opacity`, `strokeWidth`, `strokeLineJoin/Cap`, `fillRule`, `paintOrder`, `flags`, `bounds[4]`, `fillGradient[64]`, `strokeGradient[64]`, `xform[6]`, `paths`, `next` |
| Empaquetage couleur | `213` | `NSVG_RGB(r,g,b) = r \| g<<8 \| b<<16` — **RGB en octets croissants**, ce n'est **pas** du 0xRRGGBB |
| Alpha | `1006-1014`, `1016-1025` | `shape->fill.color \|= (unsigned)(fillOpacity*255) << 24`. **L'alpha du remplissage est déjà dans `fill.color`**, distinct de `shape->opacity` (opacité du groupe) |
| Ordre de chaînage | `1030-1035` | `nsvg__addShape` ajoute **en queue** (`shapesTail->next = shape`) ⇒ **`image->shapes` est en ordre de document, donc en z-order peintre** |
| Résolution des dégradés | `2998-3026` | `nsvg__createGradients` : après parse, `fill.type` n'est **jamais** `UNDEF` — soit `COLOR`, soit un dégradé résolu, soit `NONE` |
| `flags` / visibilité | `160`, `105-107` | `NSVG_FLAGS_VISIBLE` — **jamais testé par `import_svg.cpp`** : une forme `display:none` est extrudée aujourd'hui **[V]** |

**Conséquence** : le test actuel `hasFill = (fill.type != NSVG_PAINT_NONE)` (`import_svg.cpp:276`)
accepte aussi les formes à dégradé. Elles sont donc **déjà** extrudées ; il ne manque que leur
couleur. Aucune régression géométrique n'est à craindre de ce côté.

### Mesure du fichier de référence

Instrument : `grep -o … | wc -l` et `grep -o … | sort | uniq -c` sur
`test/data/svg/Ghostscript_Tiger.svg` (69 353 octets). **[M]**

| Grandeur | Valeur | Biais de l'instrument |
|---|---|---|
| `<path>` | **240** | exact (balise sans homonyme) |
| `<g>` | 241 | exact |
| `d="…"` fermés par `z`/`Z` | **227** | 240 `m`/`M` pour 240 `d` ⇒ **un sous-chemin par `d`**, la mesure est exacte |
| Attributs `fill` sur `<g>` | **228** | exact |
| Attributs `fill` sur `<path>` | **0** | exact — **toute la peinture est portée par les groupes** ; nanosvg la propage aux formes (comportement déjà exploité pour `id`, cf. `import_svg.h:91`) |
| Valeurs `fill` distinctes | **39**, dont `"none"` ×1 ⇒ **38 couleurs** | exact ; toutes hexadécimales, pas d'alias de nommage |
| `linearGradient` / `radialGradient` | **0** | exact |
| `url(` | **0** | exact |
| `opacity=` / `fill-opacity=` | **0** | exact |
| `fill-rule=` | **0** | exact ⇒ tout le fichier est en NONZERO (défaut, `nanosvg.h:653`) |
| `style=` / `<style>` / `class=` | **0** / **0** / **0** | exact ⇒ aucune cascade CSS à craindre |
| `stroke=` | **78** (69 `#000`, 4 `#4c0000`, 4 `#a5264c`, 1 `#a51926`) | exact |
| `stroke-width=` | 71 (4 valeurs distinctes) | exact |
| `transform=` | **1** (matrice racine) | exact |
| Jetons numériques dans les `d` | **11 402** ⇒ **5 701 paires** de coordonnées | exact |

**Lecture** : 240 formes, dont **~227 avec un remplissage réel** et **~13 héritant du `fill="none"`
racine** — ces 13 relèvent aujourd'hui du chemin `strokeToVolume`. **38 couleurs distinctes**, aucun
dégradé, aucune opacité, aucune règle even-odd, aucune feuille de style. **[M/E]**

> **RÉSERVE R1** — le nombre de `NSVGshape` réellement produits par nanosvg n'est pas mesuré : le
> comptage est textuel. Il est **majoré** par les `<g>` sans `<path>` descendant et **minoré** par les
> `<path>` à plusieurs sous-chemins (ici : aucun, cf. 240 `m` pour 240 `d`). Levée par le jalon **J0**
> (§6), qui compte les shapes après parse.

---

## 4. Pistes d'intégration explorées

### 4.1 Axe M1 — porter la peinture jusqu'au maillage : quatre pistes

#### Piste A1 — **surcharge additive** (nouvelle fonction ; l'ancienne devient un enveloppeur)

```cpp
// import_svg.h — AJOUTÉ, rien de retiré
struct SvgShapePaint
{
    unsigned int fillRGBA   = 0u;     // convention nanosvg : r | g<<8 | b<<16 | a<<24
    unsigned int strokeRGBA = 0u;
    bool         hasFill    = false;  // false => la région vient d'un TRAIT épaissi
    bool         isGradient = false;  // couleur = approximation (moyenne des stops)
    unsigned int rank       = 0u;     // rang de document, 0 = dessous
    std::string  id;                  // NSVGshape::id, pour le diagnostic
};

struct SvgShapeGroup
{
    std::vector<ExtrudeContour> contours;  // région DÉJÀ résolue (resolveShape)
    SvgShapePaint               paint;
};

bool svg_to_shape_groups(const std::string& filename, const SvgExtrudeOptions& opt,
                         std::vector<SvgShapeGroup>& out);

// INCHANGÉE — réimplémentée comme concaténation de svg_to_shape_groups
bool svg_to_contours(const std::string& filename, const SvgExtrudeOptions& opt,
                     std::vector<ExtrudeContour>& out);
```

- **Ancrage** : `import_svg.cpp:266-267` porte déjà `shapeContours` / `shapeEvenOdd`, deux vecteurs
  parallèles indexés par forme — la structure interne existe, il s'agit de ne plus l'aplatir.
- **Impact appelants : nul.** `svg_contours.cpp:85` et `parameterized_shapes.cpp:508` compilent et se
  comportent à l'identique.
- **Trade-off** : une fonction publique de plus. En contrepartie la version plate devient *dérivée* :
  elle ne peut plus diverger, ce qu'un détecteur de non-régression vérifie point par point (J1).

#### Piste A2 — **struct de sortie enrichie** (`svg_to_contours` change de signature)

```cpp
struct SvgContourSet {
    std::vector<ExtrudeContour> contours;   // plat, comme aujourd'hui
    std::vector<unsigned int>   shapeOf;    // parallèle : indice de forme
    std::vector<SvgShapePaint>  shapes;
};
bool svg_to_contours(const std::string&, const SvgExtrudeOptions&, SvgContourSet&);
```

**Écartée** : casse `svg_contours.cpp:85` et impose une adaptation du nœud de graphe pour un bénéfice
nul de son côté (il ne consomme pas la couleur). Elle paie un coût sur un appelant qui n'en tire
rien, pour économiser une déclaration.

#### Piste A3 — **champ de couleur sur `ExtrudeContour`**

`ExtrudeContour { pts, isHole, rgba, groupId }` : la liste reste plate, la signature ne bouge pas.

**Écartée, et c'est un écart de principe.** Deux raisons, la seconde décisive :

1. `ExtrudeContour` est le type pivot de six modules et d'un type de graphe **sérialisé**
   (`value_types.h:141`) ; y loger un concept propre au SVG est une inversion de dépendance.
2. **Le champ serait effacé en silence par toute opération booléenne.** `unionContours`,
   `differenceContours` et `resolveShape` reconstruisent des `ExtrudeContour` **neufs** depuis des
   `PathD` Clipper2 (motif vérifié en `import_svg.cpp:208-218`) : les attributs non géométriques ne
   survivent pas. Une couleur qui disparaît sans erreur de compilation est exactement le mode de
   panne que la discipline demande d'éliminer à la conception.

#### Piste A4 — **un `Mesh` par forme, fusionnés par `Mesh::Append`**

**Écartée** : `Mesh::Append` **efface `m_vertexNormals`** (`mesh.cpp:1530-1531`), ce qui impose un
`ComputeNormals()` global et **annule le bénéfice des blocs de sommets disjoints** documenté en
`extrude_contours.h:68-74`. De plus, 240 allocations de `Mesh` + 240 concaténations, là où
`ExtrudedMeshBuilder` accumule déjà dans un tampon unique. Un `Append` par forme sur **un seul
builder** fait le même travail sans aucun de ces défauts.

**→ Recommandé sur l'axe M1 : piste A1.**

### 4.2 Axe M2 — recouvrement / z-order

Les ~227 formes remplies du Tigre se recouvrent largement (c'est un dessin peintre). Trois familles
proposées, plus une variante qui n'était pas dans l'énoncé.

#### M2c — extrusion indépendante par forme, faces cachées laissées en place

- **Coût géométrique : nul.** Un `Append` par forme, mêmes `zBottom`/`zTop`.
- **Défaut rédhibitoire s'il est livré seul** : tous les capots supérieurs sont **exactement
  coplanaires** à `z = zTop` — `extrude_contours.cpp:277-282` empile les blocs à `opt.zTop` constant
  **[V]**. Deux formes qui se recouvrent produisent deux capots au même Z ⇒ **z-fighting** sur toute
  la zone de recouvrement **[H — à constater à l'écran]**.
- **Écartée seule**, retenue comme *base* de M2b'.

#### M2b — décalage en Z par **rang de document**

- `zTop_i = zTop + i·ε`, `zBottom` commun. **Coût : nul.**
- **Défaut de dimensionnement, à énoncer avant de l'écrire** : avec 240 rangs il faut choisir ε. Trop
  petit, le tampon de profondeur ne sépare pas et le z-fighting subsiste ; trop grand, le dessus
  devient un escalier. Ordre de grandeur **[E]** : pour une pièce de 100 mm de côté et 3 mm
  d'épaisseur, un tampon 24 bits sur un tronc de vue usuel sépare fiablement à ~10⁻⁴ de l'étendue de
  scène, soit ~0,01 mm ⇒ 240 × 0,01 = **2,4 mm sur 3 mm d'épaisseur**.
- **Écartée sous cette forme** : les critères « pas de z-fighting » et « le dessus reste plan » sont
  **incompatibles à 240 rangs**. C'est un défaut de conception, pas un réglage à trouver.

#### M2b' — décalage en Z par **rang de recouvrement** (variante ajoutée)

- Le rang n'est pas l'indice de document mais la **profondeur dans le graphe de recouvrement**. Deux
  formes disjointes peuvent partager le même Z ; seules celles qui se chevauchent doivent être
  séparées.
- **Charge, chiffrée** : 240²/2 = **28 680 tests de boîtes englobantes** sur `NSVGshape::bounds`
  (`nanosvg.h:161`, **déjà calculé par nanosvg**, aucun coût de préparation) ≈ 10⁵ opérations
  flottantes, **< 1 ms** **[E]** ; puis test exact (`intersectionContours`) **seulement** sur les
  paires dont les boîtes se coupent. Coloration gloutonne ⇒ nombre de niveaux = profondeur maximale
  d'empilement, **estimée à 5–15** pour un dessin peintre **[E]**.
- Avec 10 niveaux et ε = 0,01 mm : **0,1 mm sur 3 mm**, soit 3 % — invisible de dessus, sans
  z-fighting.
- **Trade-off** : la pièce n'est pas un solide fermé unique (faces internes conservées). Pour
  l'impression ce n'est pas un problème — un trancheur unionne des solides qui s'interpénètrent —
  mais l'oracle de volume signé de `tu_cgmesh_svg_stroke.cpp:33-41` ne s'y applique plus.

> **RÉSERVE R2** — le nombre de paires à tester exactement est inconnu ; **majoré** par 28 680,
> **minoré** par 0. La profondeur d'empilement est estimée, pas mesurée. Levée par **J0**.

#### M2a — soustraction booléenne 2D (marqueterie)

- Ordonnancement correct : parcourir **du dessus vers le dessous**, maintenir `couvert` = union de ce
  qui est au-dessus ; pour chaque forme `region_i = Difference(shape_i, couvert)` puis
  `couvert = Union(couvert, shape_i)`.
- **Chiffrage** :
  - *naïf, paire à paire* : n(n−1)/2 = **28 680 appels Clipper2** — à exclure ;
  - *accumulatif* : **2n = 480 appels Clipper2** (240 `Difference` + 240 `Union`). Mais `couvert`
    croît jusqu'à la complexité du dessin entier : avec ~10⁴ points au total (§5.1), le coût cumulé
    est de l'ordre de Σᵢ |couvert_i|·log|couvert_i| ≈ 240 × 10⁴ × 14 ≈ **3×10⁷ opérations
    élémentaires** **[E]**, soit **quelques centaines de ms à quelques secondes en WASM**.
- **Interactivité** : `SvgContoursNode::Compute` relit le fichier et refait tout le travail à chaque
  changement de paramètre amont (`svg_contours.cpp:66-126`) ; le cache du graphe ne protège que
  l'aval. Un coût de plusieurs secondes rendrait le curseur « Finesse d'échantillonnage »
  inutilisable.
- **Robustesse — point vérifié, non supposé** : `contour_ops.h:96-104` documente une mesure
  défavorable (une seconde passe Clipper2 « recolle des micro-arêtes » ⇒ peau trouée, 4 à 12 arêtes
  non partagées sur quatre polices). **Mais la même note précise** : « Une soustraction qui alimente
  une tessellation **INDÉPENDANTE**, elle, n'a pas ce problème. » C'est exactement notre cas — chaque
  région alimente son propre `Append`. **Le risque documenté ne s'applique pas ici.** **[V]**
- **Bénéfices propres** : dessus strictement plan, aucune face cachée, volume signé redevenu un
  oracle valide, et surtout **une pièce imprimable en éléments rapportés** (chaque couleur est une
  région XY disjointe). Seule famille à donner cela.
- **Défauts propres** : une forme entièrement recouverte disparaît (correct, mais à publier comme
  statistique) ; les traits épaissis, dessinés par-dessus, découpent les remplissages — fidèle au SVG,
  mais fragmentant.

> **RÉSERVE R3** — le coût Clipper2 réel n'est pas mesuré ; l'estimation est un **majorant grossier**
> (elle ignore le fait que `Difference` peut réduire les entrées). Levée par **J5**.

#### M2 — synthèse

| Famille | Coût calcul | Risque | Dessus plan | Imprimable en pièces | Verdict |
|---|---|---|---|---|---|
| M2c seule | nul | **z-fighting certain** | oui | non | écartée |
| M2b (rang document) | nul | z-fighting **ou** escalier de 2,4 mm | non | non | écartée |
| **M2b'** (rang de recouvrement) | ~10⁵ flops **[E]** | faible | quasi (≤3 % d'épaisseur) | non | **retenue — MVP** |
| **M2a** (différence accumulée) | ~3×10⁷ ops **[E]**, R3 | moyen (perf), faible (robustesse, `contour_ops.h:104`) | oui | **oui** | **retenue — cible** |

### 4.3 Axe M3 — restitution de la couleur

#### M3a — un matériau par forme (ou par couleur)

- **Ancrages, tous existants** : `ExtrudeAppendOptions::materialId` (`extrude_contours.h:48`),
  `Mesh::Material_Add` (`mesh.h:672`), `materialRanges` (`mesh.cpp:917-926`), `groups`/`materials`
  (`mesh_payload.cpp:106-119`), `buildOwnMaterials` (`viewer.js:83-120`).
- **Développement requis en aval du maillage : zéro.** C'est la conclusion principale de l'étude.
- **Déduplication par RGBA** recommandée : 38 matériaux au lieu de ~227 pour le Tigre (justification
  chiffrée en §5.3).
- **Limite** : un aplat par forme. Ni dégradé, ni translucidité.

#### M3b — rasterisation du SVG en une texture + UV planaires

- **Ancrages existants** : `region_apply_planar_uvs` (`image_region_pipeline.h:213`),
  `region_make_texture_material` (`:225`), `MaterialTexture` (`material.h:224`), `kind: "texture"`
  dans la charge utile (`mesh_payload.cpp:47-53`), `DataTexture` côté JS (`viewer.js:94-108`). Le
  précédent complet est `image_to_relief(..., textureFromSource=true)` (`image_relief.h:189-195`,
  `image_relief.cpp:294-301`).
- **Manque** : le **rasteriseur**. `extern/nanosvg/` ne contient que `nanosvg.h` **[M]**. Il faudrait
  vendoriser `nanosvgrast.h` (en-tête unique, domaine public) et l'ajouter au build WASM.
- **Apporte** : dégradés et opacité **gratuitement** (le rasteriseur les résout), **un seul draw
  call**, fidélité pixel.
- **N'apporte pas** : **aucune séparation des formes** — la géométrie reste un bloc. Les parois
  latérales reçoivent des UV étirés (projection planaire). Rien d'imprimable en pièces.

#### Réponse à la question posée

L'utilisateur écrit « géométries texturées/colorées », mais le symptôme décrit est « qu'un bloc
géométrique monolithique, **sans séparation des formes ni restitution des couleurs** ». Ce sont deux
demandes, et une seule méthode répond aux deux :

> **M3a répond au besoin.** M3b ne répond qu'à la moitié « ça ressemble au dessin à l'écran » et ne
> traite pas la séparation des formes — qui est la première moitié de la phrase de l'utilisateur.

Les deux sont **complémentaires**, non concurrentes : la géométrie est découpée par M2 et coloriée
par M3a ; M3b est un **mode de fidélité** optionnel, ajoutable plus tard exactement comme
`textureFromSource` l'a été pour le relief d'image, en réutilisant les deux fonctions déjà écrites.
**M3a en v1, M3b explicitement hors v1 mais non fermée.**

### 4.4 Axe M4 — dégradés et opacité

**Hors v1.** Quatre raisons, dont trois mesurées ou vérifiées :

1. **Le fichier de référence ne les exerce pas** : 0 dégradé, 0 attribut d'opacité **[M]**.
2. **`MaterialColor` ne peut pas porter un dégradé**, et les deux véhicules possibles sont exclusifs :
   - couleurs par sommet (`m_vertexColors`) — `BuildMeshPayload` ne les transmet que si l'une d'elles
     s'écarte de 0,5 (`mesh_payload.cpp:97-100`), **et** `viewer.js:294` les désactive dès que
     `useGroups` est vrai. Les deux mécanismes **ne coexistent pas** dans le code actuel **[V]** ;
   - texture — c'est M3b, avec son rasteriseur à vendoriser.
3. **L'alpha ne traverse pas la charge utile** : `DescribeMaterial` n'émet que `r,g,b`
   (`mesh_payload.cpp:61-65`) **[V]**. Honorer l'opacité demande de toucher `mesh_payload.cpp` et
   `viewer.js` (transparence, ordre de rendu, `depthWrite`) — chantier de rendu, pas de géométrie.
4. **L'opacité n'a pas de sens pour la sortie principale de `maker`**, qui est une pièce imprimée.

**Repli v1, à écrire dans l'en-tête** : une forme à dégradé produit sa géométrie (déjà le cas) et
reçoit une couleur = **moyenne pondérée des `stops`** ; `SvgShapePaint::isGradient` marque
l'approximation, et le nœud publie une statistique `gradientShapes` pour que l'écart soit **visible**
plutôt que silencieux. L'alpha est **lu et stocké** dans `MaterialColor` (le constructeur l'accepte
déjà, `material.h:96`) mais **non transmis** : la ligne est prête le jour où le rendu la demandera.

### 4.5 Axe « point d'entrée graphe » — trois pistes

Rappel : `svg.html` passe par le **graphe**, pas par `ParameterizedSvgExtrusion`. C'est **là** que la
feature doit atterrir pour que l'utilisateur la voie.

| Piste | Principe | Coût | Trade-off |
|---|---|---|---|
| **G1 — nœud `svg.shapes` + nouveau type de valeur** | Un type `svgShapeGroups` dans `DomainTypes` (`value_types.h:84-152`), un nœud producteur, un nœud extrudeur coloré | Élevé : nouveau nom de type **sérialisé** donc irréversible (`value_types.h:6-7`), enregistrement au catalogue, support éditeur | Composable (foreach, booléens, pièces par couleur). À réserver au jour où un besoin de composition existe |
| **G2 — nœud monolithique `svg.extrude.colored`** (chemin → maillage) | Contours + recouvrement + extrusion + palette en un nœud, au-dessus de `import_svg_extruded(opt.perShapeMaterials = true)` | Faible : un nœud, un enregistrement, une mise à jour de `svg.json` | Non composable. **Précédent assumé dans le dépôt** : « l'extrusion de texte est monolithique » (`value_types.h:138-139`) |
| **G3 — étendre `svg.contours`** | Ajouter un port de sortie couleur | Impossible sans G1 : `extrudeContours` ne peut pas porter la peinture (cf. A3) | écartée |

**→ Recommandé : G2 pour le MVP, G1 comme cible si un besoin de composition apparaît.**

Effets de bord de G2 sur `maker/web/data/templates/svg.json` : `output` change de nœud ; les
paramètres exposés `flattenTol`, `centerAndFit`, `invertY`, `strokeToVolume`, `strokeScale`,
`strokeWidthFallback`, `fitSize` (`svg.json:23-91`) et `depth` migrent sur le nouveau nœud ; le
paramètre « Couleur de la pièce » (`svg.json:104-110`) devient **inopérant sur les formes peintes**
— `mesh.color` ne peint que `MATERIAL_NONE` (`color.cpp:114`). Il faut donc soit le retirer, soit
exposer une bascule « utiliser les couleurs du SVG ». **Les documents déjà enregistrés continuent de
fonctionner** : `svg.contours` n'est pas retiré.

---

## 5. Volumétrie et charge

### 5.1 Estimation du maillage produit sur le Tigre **[E]** — ⚠ MESURÉ, voir §17.2

Méthode : comptage de jetons (§3) + raisonnement structurel. **Aucune exécution.**

| Grandeur | Estimation | Dérivation |
|---|---|---|
| Segments cubiques après conversion nanosvg | **1 800 – 2 700** | 5 701 paires − 240 (commandes `m`) = 5 461 ; une commande `C` en consomme 3, une `S` 2 ⇒ bornes 5461/3 et 5461/2 |
| Points de contour après aplatissement | **6×10³ – 2×10⁴** | 3 à 8 points par segment à `flattenTol` = 0,005 × étendue |
| Sommets du maillage | **2,4×10⁴ – 8×10⁴** | `ExtrudedMeshBuilder` émet **4 blocs de n sommets** par `Append` (`extrude_contours.cpp:259-262`) |
| Triangles | **2,4×10⁴ – 8×10⁴** | capots ≈ 2(n−2), parois = 2 par arête de contour |
| Charge utile par rafraîchissement | **1 – 3 Mo** | positions + normales = 24 o/sommet, indices = 12 o/triangle |

> **RÉSERVE R4** — ces cinq lignes sont des estimations à un facteur ~3 près, **majorées** par
> l'hypothèse de 8 points/segment et **minorées** par les points que Clipper2 ajoute aux
> intersections. Levée par **J0**. Le point important pour le plan est l'**ordre de grandeur** :
> ~10⁴–10⁵ sommets, pas 10⁶ — la charge utile reste dans ce que le navigateur transporte déjà pour un
> relief d'image.

**Point structurel** : le découpage en 240 `Append` **ne change pas** le nombre de sommets. Chaque
`Append` alloue `4·n_i` sommets pour ses propres contours, et Σn_i = n. Le coût mémoire de M2c/M2b'
est donc **strictement nul**. (M2a le **réduit**, en supprimant la matière recouverte.)

### 5.2 Matériaux, groupes, draw calls

| Sans déduplication | Avec déduplication par RGBA |
|---|---|
| ~227 matériaux | **38 matériaux** **[M sur le texte source]** |
| ~227 `MaterialRange` | 38 |
| ~227 draw calls three.js | 38 |

### 5.3 Validation de la structure **contre la charge**, pas contre le schéma

L'opération la plus fréquente du chemin réel n'est pas l'import : c'est **`BuildPolygonRenderData`,
rejouée à chaque mouvement de curseur** (`template.js:337`, `:406`). Deux coûts à nommer :

1. **Côté C++** : `buckets[matId]` est une recherche dans `std::map` **par face** (`mesh.cpp:821`),
   soit O(F·log M). Avec F ≈ 5×10⁴ et M = 38 : ~2,5×10⁵ descentes d'arbre — non dominant devant la
   tessellation. À M = 240, log M passe de 5 à 8, soit +60 % sur ce seul poste. **La structure tient
   sous la charge** ; la déduplication n'est pas requise ici.
2. **Côté JS, et c'est là que se joue la déduplication** : `buildOwnMaterials` **clone un matériau
   three.js par groupe** (`viewer.js:83-120`), et `materialSignature` reconstruit une chaîne de M
   éléments **à chaque image** (`viewer.js:316`). Un changement de signature détruit et re-clone les M
   matériaux (`viewer.js:317-322`). À M = 240, c'est 240 clones à chaque changement de palette ; à
   M = 38, six fois moins.

> **Conclusion de charge : dédupliquer par RGBA.** Le gain n'est pas dans le C++ mais dans le nombre
> de draw calls et le coût de reconstruction des matériaux côté navigateur. C'est un choix de
> conception, pas une optimisation prématurée : il coûte une `std::map<unsigned int, unsigned int>`
> de 38 entrées dans `import_svg_extruded`.

### 5.4 Est-ce tenable ?

**Oui.** 38 groupes et ~10⁵ sommets sont **du même ordre que le relief d'image déjà en production**
(`image_to_relief` jusqu'à 16 couleurs + base + mur). Aucune limite structurelle n'est approchée. La
seule inconnue de performance est le coût Clipper2 de M2a (R3) — raison pour laquelle M2a n'est pas
dans le MVP.

---

## 6. Recommandation — ⚠ SUPERSÉDÉE par la section 10 (révision du 2026-09-06)

| Axe | Retenu | Écarté, et pourquoi |
|---|---|---|
| **M1 — API** | **A1** : surcharge additive `svg_to_shape_groups` + `svg_to_contours` devenue enveloppeur | A2 (casse un appelant sans bénéfice pour lui) · **A3 (champ sur `ExtrudeContour` : effacé en silence par tout booléen Clipper2 — mode de panne muet)** · A4 (`Mesh::Append` efface les normales, `mesh.cpp:1530`) |
| **M2 — recouvrement** | **M2b' (rang de recouvrement) en MVP** ; **M2a (différence accumulée) en cible** | M2c seule (z-fighting certain) · M2b par rang de document (240 rangs = 2,4 mm d'escalier sur 3 mm, **ou** z-fighting : les deux critères sont incompatibles) |
| **M3 — couleur** | **M3a**, un matériau par **couleur** (déduplication RGBA) | M3b **non écartée mais hors v1** : elle ne sépare pas les formes, donc ne répond qu'à la moitié du besoin, et demande de vendoriser un rasteriseur absent |
| **M4 — dégradés / opacité** | **Hors v1**, avec repli explicite (moyenne des stops) et statistique publiée | Les deux véhicules possibles sont soit exclusifs de `useGroups` (`viewer.js:294`), soit égaux à M3b |
| **Point d'entrée** | **G2** : nœud monolithique `svg.extrude.colored` | G1 (nouveau type sérialisé, irréversible, sans besoin de composition avéré) · G3 (impossible, cf. A3) |

**Justification d'ensemble.** Le coût réel de cette feature n'est **pas** dans le rendu — toute la
chaîne matériau vers JS vers three.js existe et tourne déjà pour le relief d'image. Il est concentré
dans deux décisions de `import_svg.cpp` : *transporter la peinture* (peu risqué, mécanique) et
*décider quoi faire du recouvrement* (le seul vrai choix de conception). Le plan sépare délibérément
ces deux décisions — J1/J2 d'un côté, J3/J5 de l'autre — pour que la seconde puisse être révisée
après avoir vu le Tigre à l'écran, sans rien jeter de la première.

---

## 7. Plan incrémental avec jalons vérifiables — ⚠ SUPERSÉDÉ par la section 12

Chaque jalon énonce ce qui doit être **vrai** à la fin, sous une forme qu'une observation peut
**démentir**. Les catégories exclues **par nature** sont nommées ; elles appartiennent au jalon
suivant.

### J0 — Mesurer avant de construire (aucune feature)

- **Prérequis bloquant** : `test/data/svg/Ghostscript_Tiger.svg` est **non suivi par git**
  (`git status` : `?? test/data/svg/Ghostscript_Tiger.svg`) **[M]**. Il doit être ajouté au dépôt,
  sans quoi le test ne s'exécutera nulle part ailleurs que sur cette machine. Le fichier est copié
  dans l'arbre d'exécution par `test/CMakeLists.txt:63-68`.
- **Livrable** : un test qui parse le Tigre avec nanosvg et **publie** : nombre de `NSVGshape` ;
  nombre a `fill.type == NSVG_PAINT_COLOR` ; nombre a dégradé ; nombre a `NSVG_PAINT_NONE` avec
  trait ; nombre de `fill.color` distincts ; nombre de paires de boîtes englobantes sécantes ;
  profondeur maximale d'empilement ; points de contour après aplatissement ; sommets et triangles du
  maillage actuel.
- **Critères falsifiables** : `nShapes == 240`, `nDistinctFillColors == 38`, `nGradients == 0`. Un
  écart **infirme** la mesure textuelle du paragraphe 3 et lève R1, R2, R4.
- **Exclu par construction** : rien.

### J1 — `svg_to_shape_groups` (piste A1), géométrie strictement inchangée

- **Livrable** : la nouvelle fonction ; `svg_to_contours` réimplémentée comme sa concaténation.
- **Critères falsifiables** :
  1. Les 8 tests de `test/tu_cgmesh_svg.cpp` et les 7 de `test/tu_cgmesh_svg_stroke.cpp` passent
     **sans modification** — dont `square_produces_extruded_solid` qui exige exactement 16 sommets et
     12 faces (`tu_cgmesh_svg.cpp:49-50`).
  2. Un test neuf vérifie **point par point** que la concaténation des groupes est **identique** a la
     sortie actuelle sur `rose.svg`, `batman.svg`, `spiderman.svg`, `nazca.svg`,
     `Ghostscript_Tiger.svg`.
  3. **Le détecteur du point 2 est validé sur un cas positif** avant d'être cru : perturber
     volontairement un groupe et vérifier que le test **échoue**. Un détecteur qui n'a jamais rien
     trouvé ne prouve rien.
- **Exclu par construction** : rien n'est peint, rien n'est séparé, aucun changement a l'écran — c'est
  ce qui rend le critère 2 exact.

### J2 — Un matériau par couleur (M3a), un `Append` par forme, **sans** résolution du recouvrement

- **Livrable** : `SvgExtrudeOptions::perShapeMaterials` (champ additif, **défaut `false`**),
  déduplication RGBA, palette construite sur le modèle de `image_relief.cpp:305-316`.
- **Critères falsifiables** :
  1. `perShapeMaterials == false` donne un maillage **identique** a J1 (les tests existants sont
     l'oracle, et ils sont exacts).
  2. `perShapeMaterials == true` sur le Tigre : `GetNMaterials()` égale la valeur mesurée en J0 (38
     attendu) ; `materialRanges.size() == GetNMaterials()` ; **aucune** face a `MATERIAL_NONE`.
  3. Sur `square.svg` (forme unique) : `GetNMaterials() == 1` et **compte de sommets et de faces
     inchangé** (16 et 12).
- **Prédiction a consigner, probablement fausse** : sur un fichier **multi-formes**, le compte de
  sommets **changera**, parce qu'un `Append` unique tessellait toutes les formes dans **un seul
  polygone glutess** (`extrude_contours.cpp:182`) ou elles interagissaient par la règle NONZERO. Ce
  changement est **souhaité** ; il doit être **mesuré**, pas subi.
- **Exclu nommément, renvoyé a J3** : les formes qui se recouvrent produisent des capots coplanaires,
  donc du **z-fighting**. Le critère de J2 **ne porte pas** sur l'aspect visuel du Tigre. Écrire ici
  « le Tigre s'affiche correctement » serait un critère que la construction rend impossible.

### J3 — Rang de recouvrement (M2b'), le Tigre devient lisible

- **Livrable** : graphe de recouvrement par boîtes englobantes (`NSVGshape::bounds`) puis test exact
  sur les seules paires sécantes ; coloration gloutonne ; `zTop_i = zTop + niveau_i * epsilon`, avec
  epsilon exprimé en **fraction de `depth`** et non en unités absolues (même convention que
  `flattenTol`, `import_svg.h:34-49`).
- **Critères falsifiables** :
  1. Deux formes disjointes reçoivent le **même** niveau (SVG a deux carrés séparés).
  2. Deux formes sécantes reçoivent des niveaux **différents** (deux carrés qui se chevauchent).
  3. Sur le Tigre, `nLevels <= 20`. Au-dela, l'écart de hauteur devient visible et l'hypothèse « un
     dessin peintre empile peu » est **infirmée**.
  4. A l'écran (`maker/web/svg.html`) : **aucun scintillement** sur les zones de recouvrement en
     rotation. **[Constat visuel, assumé comme tel, non falsifiable automatiquement.]**
- **Exclu nommément, renvoyé a J5** : le dessus n'est **pas** plan ; le volume est sur-compté ; la
  pièce n'est pas décomposable en éléments imprimables.

### J4 — Nœud de graphe `svg.extrude.colored` (piste G2) et gabarit

- **Livrable** : le nœud, son enregistrement au catalogue, la mise a jour de
  `maker/web/data/templates/svg.json`, les statistiques `gradientShapes` et `hiddenShapes`.
- **Critères falsifiables** :
  1. Un document `svg.json` **antérieur** (`svg.contours` puis `shape.extrude` puis `mesh.color`)
     s'ouvre et s'évalue toujours : `svg.contours` n'est pas retiré.
  2. La page `svg.html` chargée avec le Tigre expose **au moins 30 couleurs distinctes**
     (`materials.filter(m => m.kind === "color").length >= 30` en console). Critère faible mais
     **infirmable** : le bloc gris actuel en produirait 0 ou 1.
  3. Le curseur « Finesse d'échantillonnage » reste utilisable : recalcul **inférieur a 500 ms** sur
     le Tigre.
- **Exclu nommément** : l'export. Le STL ne porte pas de couleur (`exporters.js:88`) et
  `graphExportObj` rend un OBJ **sans `mtllib`** (`wasm_api.cpp:745-750`). Les couleurs sont
  **visibles mais non exportables** ; c'est déja dit dans le gabarit (`svg.json:108`) et cela reste
  vrai. Un export coloré depuis une page gabarit est un chantier **distinct** (il existe côté formes :
  `exportObj` vers `MeshIO::export_obj_zip_bytes`).

### J5 (cible, sur validation) — Marqueterie par différence booléenne (M2a)

- **Livrable** : option `SvgExtrudeOptions::resolveOverlap` (`None`, `ZRank`, `Subtract`),
  ordonnancement accumulatif, statistique des formes intégralement recouvertes.
- **Critères falsifiables** :
  1. Sur deux carrés se recouvrant a moitié, l'**oracle de volume signé** de
     `tu_cgmesh_svg_stroke.cpp:33-41` redevient exact (volume == aire du capot fois hauteur), ce qui
     est **faux** sous J3. C'est le test qui distingue M2a de M2b'.
  2. Aucune paire de triangles de capot ne partage un point intérieur (régions XY disjointes).
  3. Temps de résolution mesuré sur le Tigre, **consigné quel qu'il soit** : c'est la levée de R3.
- **Exclu nommément** : les formes issues de `strokeToVolume` découpent les remplissages sous-jacents
  (conforme au SVG) ; les formes intégralement recouvertes disparaissent. Ces deux effets doivent être
  **comptés et publiés**, pas corrigés.

### J6 (optionnel, hors v1) — Mode texture (M3b)

Vendoriser `nanosvgrast.h`, l'ajouter au build WASM, réutiliser `region_apply_planar_uvs` et
`region_make_texture_material`. Critère : le Tigre affiché avec **un seul matériau `kind:"texture"`**
et un rendu fidèle des dégradés. Ne remplace pas M3a — c'est un mode, exactement comme
`textureFromSource` pour le relief d'image.

### Impact sur les tests existants — bilan

| Fichier | Impact attendu |
|---|---|
| `test/tu_cgmesh_svg.cpp` (8 tests) | **Aucun** si `perShapeMaterials` est `false` par défaut. Fixtures mono-forme ou a formes disjointes |
| `test/tu_cgmesh_svg_stroke.cpp` (7 tests) | **Aucun** : fixtures mono-forme, écrites a la volée par `writeSvg` (`:53-57`) |
| `test/tu_cgmesh_parameterized_shapes.cpp:394`, `:440` | **Aucun** : `ParameterizedSvgExtrusion` conserve son comportement |
| `test/tu_cggraph_nodes_shapes.cpp:774` | **Aucun** : `svg.contours` inchangé |
| `sinaia/SinaiaFrame.cpp:2384` | **Aucun** en v1. Sinaia honore les matériaux par face ; activer l'option y serait un gain, a traiter séparément |

---

## 8. Journal de prédictions — ⚠ SUPERSÉDÉ par la section 15

A alimenter par le sous-agent `developer` a chaque jalon terminé. La colonne « Résultat » est vide
tant que rien n'est exécuté : c'est le point de la méthode.

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| J0 | `nShapes == 240` | test nanosvg sur le Tigre | | | |
| J0 | `nDistinctFillColors == 38` | idem | | | |
| J0 | `nGradients == 0` | idem | | | |
| J0 | sommets du maillage actuel entre 2,4e4 et 8e4 | `Mesh::GetNVertices` | | | |
| J0 | profondeur d'empilement <= 20 | coloration du graphe de recouvrement | | | |
| J1 | les 15 tests SVG existants passent inchangés | `ctest` | | | |
| J1 | concaténation des groupes identique a la sortie actuelle, point par point | test de non-régression, validé sur cas positif | | | |
| J2 | `perShapeMaterials=false` donne un maillage identique | tests existants | | | |
| J2 | `GetNMaterials() == 38` sur le Tigre | test | | | |
| J2 | **le compte de sommets change sur les fichiers multi-formes** (tessellation par forme differente de la tessellation globale NONZERO) | comparaison J1 / J2 | | | |
| J3 | `nLevels <= 20` sur le Tigre | test | | | |
| J3 | pas de scintillement a l'écran | constat visuel | | | |
| J4 | recalcul inférieur a 500 ms sur le Tigre | `performance.now()` dans `updateInPlace` (`viewer.js:257`, `:350`) | | | |
| J5 | oracle de volume signé exact sous `Subtract` | `tu_cgmesh_svg_stroke.cpp:61-100` | | | |
| J5 | résolution Clipper2 inférieure a 2 s sur le Tigre (R3) | chronométrage | | | |

Liste fermée des causes : 1 affirmation sur l'existant non vérifiée · 2 instrument biaisé ·
3 hypothèse d'environnement non vérifiée · 4 structure conçue sans charge · 5 critère non falsifiable ·
6 excès de pessimisme · 7 mécanisme de vérification neutralisé par le code.

---

## 9. Risques, limites, et ce qui n'a **pas** été examiné

### Risques

| Risque | Nature | Atténuation |
|---|---|---|
| Le découpage par forme change la géométrie des fichiers multi-formes (NONZERO ne joue plus entre formes) | **[V]** sur la cause (`extrude_contours.cpp:182`), **[H]** sur l'ampleur | J2 le mesure au lieu de le supposer ; les fixtures mono-forme protègent les tests existants |
| Coût Clipper2 de M2a inconnu (**R3**) | **[E]** ~3e7 opérations | M2a hors MVP ; chronométré en J5 avant toute activation par défaut |
| Nombre de niveaux de recouvrement inconnu (**R2**) | **[E]** 5 a 15 | Mesuré en J0 ; critère de rupture explicite (`nLevels <= 20`) en J3 |
| Volumétrie estimée a un facteur ~3 près (**R4**) | **[E]** | Mesurée en J0 ; l'ordre de grandeur (1e4 a 1e5) suffit au plan |
| `Ghostscript_Tiger.svg` non suivi par git (**R5**) | **[M]** | Prérequis bloquant de J0 |
| La couleur reste **non exportable** (STL sans couleur, `graphExportObj` sans `mtllib`) | **[V]** `exporters.js:88`, `wasm_api.cpp:745-750` | Hors périmètre, déja documenté (`svg.json:108`). Chantier distinct |
| `NSVG_FLAGS_VISIBLE` jamais testé : une forme `display:none` est extrudée | **[V]** `nanosvg.h:160` contre `import_svg.cpp:276` | Défaut **préexistant**, signalé au titre de la règle d'or. Correction naturelle en J1, une ligne |

### Limites assumées de la v1

- Un aplat par forme : ni dégradé, ni translucidité.
- Les **parois latérales** héritent de la couleur de leur forme (`extrude_contours.cpp:370-377`, même
  `materialId` que les capots). Il n'existe **aucun** étiquetage capot/paroi dans le builder : les
  peindre différemment demanderait de le créer.
- La couleur du trait est appliquée a la région **épaissie** ; le trait d'une forme **remplie et
  fermée** n'est toujours pas produit (`import_svg.cpp:302-311` : une forme fermée avec `fill` part
  dans `filled`, son trait est ignoré). Manque **préexistant**, non traité ici.

### Ce qui n'a **pas** été examiné — déclaration explicite

1. **Aucune exécution.** Rien n'a été compilé, aucun test lancé, `maker` n'a pas été construit
   (`maker/build.ps1` non exécuté, emsdk non vérifié sur cette machine). **Tout diagnostic de
   comportement visuel est une hypothèse [H].**
2. `sinaia/SinaiaFrame.cpp` : seule la ligne d'instanciation (`:2384`) a été lue. Le rendu wxWidgets
   des matériaux par face n'a pas été examiné.
3. `sulina`, `vecna` : non examinés. Aucun appelant SVG n'a été trouvé, mais **une absence ne se
   constate pas par recherche** : je n'ai pas lu leurs sources.
4. `maker/web/js/template.js` : lu par extraits (lignes 19, 29, 334-410, 703-716). Le cycle de vie des
   paramètres exposés et la construction du panneau n'ont pas été lus.
5. `maker/graph_api.cpp` : seule la ligne 481 (`BuildMeshPayload`) a été lue. L'éditeur nodal et son
   renderer WebGL de worker (`graph-worker.js`) n'ont pas été examinés, alors qu'ils consomment la
   même charge utile et bénéficieraient des mêmes couleurs.
6. `src/cgmesh/image_relief.cpp` : lu par recherche ciblée (matériaux, `Append`), pas intégralement.
7. `Mesh::BuildPolygonRenderData` : lue en entier ; `forEachFaceTriangle` (`mesh.cpp:497`) ne l'a
   **pas** été.
8. `extern/clipper2` : non lu. Les coûts Clipper2 sont estimés depuis la complexité connue de
   l'algorithme de Vatti, pas depuis ce code.
9. `contour_ops.cpp` : seul son en-tête a été lu, pas l'implémentation des booléens.
10. Les autres gabarits (`pixels.json`, `relief.json`, `text3d.json`) n'ont pas été lus ; l'impact
    d'un nouveau nœud sur eux est supposé nul, **non vérifié**.

### Handoff

Analyse **seule**. Toute implémentation passe par le sous-agent `developer`, puis `/code-review`
obligatoire. La prédiction J2 (« le compte de sommets change sur les fichiers multi-formes ») est un
**diagnostic non reproduit** : si son ampleur devait surprendre, elle relève du sous-agent `debugger`
avant toute correction.

### Décisions a faire trancher avant de figer le plan

1. **M2b' (MVP) ou M2a (marqueterie) directement ?** M2a coûte l'inconnue de performance R3 mais
   donne une pièce imprimable en éléments rapportés — objectif possible, non énoncé.
2. **Que devient « Couleur de la pièce » dans `svg.json` ?** Retrait, ou bascule
   « utiliser les couleurs du SVG » avec repli sur la couleur unique.
3. **G2 (nœud monolithique) ou G1 (nouveau type de valeur composable) ?** G1 coûte un nom de type
   sérialisé, donc irréversible.
4. **Traits : couleur du `stroke` ou couleur du `fill` du groupe ?** Les 78 formes a trait du Tigre
   sont majoritairement noires ; les peindre en noir souligne le dessin, les fondre dans le
   remplissage l'adoucit.
5. **Déduplication par RGBA : oui (38 matériaux) ou une entrée par forme (227) ?** La déduplication
   perd la traçabilité forme -> matériau, utile a un futur « exporter chaque forme séparément ».

---
---

# PARTIE 2 — Décisions actées et plan révisé (2026-09-06)

Cette partie **remplace** les sections 6 (Recommandation), 7 (Plan) et 8 (Journal) de la partie 1.
Les sections 1 à 5 et 9 restent valides et ne sont pas réécrites.

## 10. Décisions actées — 2026-09-06

| # | Décision | Motif retenu | Ce que cela remplace |
|---|---|---|---|
| **D1** | **Recouvrement : M2a** (marqueterie par différence booléenne accumulée) **directement dans le MVP**. M2b' (rang de recouvrement) est **écarté**. | L'objectif est une pièce **imprimable en éléments rapportés** ; M2b' ne le donne pas (faces internes conservées, dessus non plan). | La recommandation « M2b' en MVP, M2a en cible » de la section 6 |
| **D2** | **`svg.json` : bascule « utiliser les couleurs du SVG »**, **défaut = comportement actuel** (couleur unique via `mesh.color`). Le sélecteur « Couleur de la pièce » est **conservé et grisé** quand la bascule est active. | Non-régression visible pour les documents et les habitudes existants ; le sélecteur reste explicable au lieu de disparaître. | La question 2 laissée ouverte |
| **D3** | **Point d'entrée : G2**, nœud monolithique `svg.extrude.colored`. **G1 reste la cible** si un besoin de composition apparaît. | Coût d'entrée minimal ; précédent assumé du texte 3D (`value_types.h:138-139`). Un nom de type sérialisé est irréversible, on ne le paie pas d'avance. | La question 3 |
| **D4** | **Traits : les deux comportements**, pilotés par un booléen exposé, nommé ici **`strokeUsesStrokeColor`** (nom et défaut justifiés en §11.4). | Les deux lectures sont légitimes selon le fichier ; un booléen coûte une ligne et évite d'arbitrer à la place de l'utilisateur. | La question 4 |
| **D5** | **Déduplication par RGBA : oui** (38 matériaux sur le Tigre). Correspondance forme vers matériau conservée **à part**. | 38 draw calls au lieu de 227, et six fois moins de clones de matériaux three.js (`viewer.js:83-120`). | La question 5 |
| **D6** | **Étagement du recalcul : option C** — G2 nu, **mesurer d'abord**, avec **~300 ms** pour seuil de référence. Le mémo interne (option A) est **explicitement différé** : hors périmètre de cette livraison, documenté et prêt à activer. | Ne pas payer une mécanique de cache avant d'avoir la mesure qui la justifie ; le seuil descend à 300 ms parce que la régression porte sur `depth`, le curseur le plus manipulé (§11.3). | Point d'architecture soulevé le 2026-09-06 ; seuil et report du mémo confirmés le 2026-09-06 |

### Règle de décision validée pour D6

> **Seuil de référence : ~300 ms sur le Tigre.** En dessous, la régression R6 est invisible et
> D6 est close. Au-dessus, K4 le **consigne et le remonte** — le mémo interne (option A) n'est
> **pas** activé dans la foulée : son implémentation est une décision distincte, prise au vu
> de la mesure.

Le mémo est **différé, pas écarté** : §11.3 et §13 en conservent la conception complète. Ce
report est un choix assumé — livrer sans lui et mesurer sur le fichier réel plutôt que
construire un cache contre une estimation.

Cette règle est le critère du jalon **K4** (§12). Elle est **falsifiable** : un chronomètre la tranche.

## 11. Ce que ces décisions changent, vérifié

### 11.1 Prémisse corrigée : il n'y a pas de « C++ » distinct du « WASM »

Vérification faite : `maker/CMakeLists.txt:81` lie `cgmesh`, et la liste `EMSCRIPTEN` de
`src/cgmesh/CMakeLists.txt` inclut `import_svg.cpp` (`:64`), `extrude_contours.cpp` (`:65`),
`contour_ops.cpp` (`:67`), plus Clipper2 (`CLIPPER2_SRC_FILES`) et glutess. **[V]** nanosvg,
Clipper2, la tessellation et l'extrusion sont donc **le même C++**, simplement recompilé par
Emscripten. `maker/serve.py` est un serveur de fichiers statiques : **aucun calcul serveur, aucune
géométrie en JavaScript**. Le découpage « import / booléens / extrusion » n'est donc pas un choix de
langage : c'est un **étagement du recalcul**, et c'est à ce titre qu'il est traité ci-dessous.

### 11.2 Le cache du graphe — vérification demandée, réponse : l'affirmation est **exacte** [V]

Mécanisme lu de bout en bout :

- `cggraph::Signature` (`src/cggraph/core/signature.cpp:90-137`) hache le **nom de type**, les
  **paramètres sémantiques du nœud** (`HashParamSet`, `:51-88`, qui **ignore** les paramètres non
  sémantiques, `:56-57`) et, récursivement, **la signature de chaque amont** (`:119-131`).
- `Evaluator::EvaluateNode` (`src/cggraph/core/evaluator.cpp:271-272`) cherche cette signature dans
  l'index du cache **avant** de descendre la branche ; le cache est une LRU bornée
  (`evaluator.h:170-201`).
- L'évaluateur **persiste** entre deux évaluations : `maker_graph::Host` est un singleton
  (`maker/graph_api.cpp:88-105`) qui construit **un seul** `InlineEvalDriver`, dont le budget est
  fixé à **128 Mio** (`graph_api.cpp:86`, `:96`).

**Conclusion : oui. Changer `depth` sur `shape.extrude` (nœud 2) ne modifie que la signature du
nœud 2 ; celle de `svg.contours` (nœud 1) est inchangée, son entrée de cache ressert, et l'import
plus l'aplatissement ne sont pas rejoués.** L'affirmation faite à l'utilisateur est correcte. Le
volume en jeu est négligeable devant le budget — environ 1e4 points de contour, soit environ 1e5
octets, contre 128 Mio — et le type `extrudeContours` déclare bien un `sizeHint`
(`value_types.cpp:237`), donc la comptabilité du budget est réelle et non nulle.

### 11.3 ⚠ Incohérence introduite par D3 — cette propriété est **détruite par G2**

C'est le point que la décision D3 n'a pas vu, et il est structurel.

Sous G2, `depth` **cesse d'être un paramètre du nœud aval** : il devient un paramètre **du nœud
monolithique**, celui-là même qui fait l'import nanosvg et les booléens. Par `signature.cpp:102`
(`HashCombine (h, HashParamSet (node->GetParams ()))`), **toucher `depth` change la signature de ce
nœud**, donc invalide son entrée de cache, donc **rejoue l'import ET les booléens**. **[V]**

> **Le curseur « Profondeur » est gratuit aujourd'hui et ne le sera plus après G2.** C'est une
> régression de réactivité introduite par la décision de simplification, sur un curseur probablement
> plus utilisé que « Finesse d'échantillonnage ».

Aggravant, vérifié : **la page gabarit évalue sur le thread UI**, et c'est écrit noir sur blanc dans
`maker/web/js/template.js:10-17` (« POURQUOI CETTE PAGE ÉVALUE SUR LE THREAD UI »). **[V]** Il n'y a
pas d'échappatoire asynchrone : `cggraph::AsyncEvaluator` démarre un `std::thread` à sa construction
et **lève** sous Emscripten sans `-pthread` (`maker/graph_host.h:19-23`). **[V]** Un recalcul d'une
seconde n'est donc pas « lent », c'est **une seconde de page figée par mouvement de curseur**.

Trois issues, à trancher — je recommande la **(b)** :

| Issue | Effet | Coût |
|---|---|---|
| (a) Accepter la régression | Si K4 mesure moins de 300 ms, elle est invisible et la question est close | nul |
| **(b) Abaisser le seuil de bascule : activer le mémo (option A) dès que K4 dépasse ~300 ms, et non 1 s** | La régression porte sur le curseur le plus utilisé ; le seuil « tolérable » n'est pas le même que pour un curseur rare | la mécanique du mémo, un jalon |
| (c) Revenir à G1 | Le découpage naturel du graphe **est** l'étagement du cache : deux nœuds, cache natif, rien à écrire | un nom de type sérialisé, irréversible |

**D6 (« mesurer d'abord ») reste la bonne décision**, mais son seuil doit se lire ainsi : la question
n'est pas « le recalcul est-il tolérable dans l'absolu », c'est « la **perte de gratuité de `depth`**
est-elle tolérable ». Même mesure, enjeu différent.

### 11.4 D4 — nom, défaut, et portée réelle du paramètre — ⚠ PORTÉE INVALIDÉE par D7, voir §22.3

- **Nom retenu** : `SvgExtrudeOptions::strokeUsesStrokeColor` (booléen), dans la convention des
  champs voisins `strokeToVolume`, `strokeScale`, `strokeWidthFallback` (`import_svg.h:73-83`) :
  préfixe `stroke`, nom qui énonce la règle en vigueur et non son contraire.
- **Défaut retenu : `true`** (couleur du `stroke`). Trois raisons :
  1. **Fidélité** : c'est ce que le document dit ; 69 des 78 traits du Tigre sont noirs et dessinent
     la structure du dessin **[M]**.
  2. **Sous M2a, la découpe géométrique a lieu de toute façon.** Un trait peint de la couleur du
     remplissage produit une région **séparée** portant le **même** matériau après déduplication : on
     paie la découpe sans jamais en voir le bénéfice. `false` est donc le réglage qui coûte sans
     rendre — mauvais candidat au défaut.
  3. `true` est réversible d'un clic ; un utilisateur qui veut fondre ses traits le voit tout de
     suite, l'inverse est un manque silencieux.
- **Portée réelle, à ne pas surestimer** : `import_svg.cpp:302-311` n'épaissit un trait que pour un
  chemin **ouvert** ou **sans remplissage** ; le trait d'une forme **fermée et remplie** est
  **ignoré** aujourd'hui. **[V]** Sur le Tigre, ce paramètre ne concerne donc que les **~13 formes
  `fill:none`** (240 chemins moins 227 groupes portant une couleur), et non les 78 attributs `stroke`
  comptés. **[M/E]** Il est **déterminant pour un dessin au trait, marginal pour le Tigre**.
- **Correction de la partie 1 au passage** : la crainte exprimée en §4.2 (« les traits épaissis
  découpent les remplissages, fragmentant ») porte, sur ce fichier, sur **~13 formes et non 78**.

### 11.5 D5 — la correspondance à conserver n'est pas celle qu'on croit

- **Forme vers matériau** : un `std::vector<unsigned int>` de 240 entrées, environ 1 Kio. **À
  conserver** : le coût est nul et il documente la déduplication (« quelles formes ont fusionné »).
- **Mais ce n'est pas ce dont un futur "exporter chaque forme séparément" aurait besoin.** Ce
  besoin-là veut **face vers forme**, et `Mesh` ne porte qu'un identifiant de matériau par face
  (`mesh.h:740`, `m_faceMaterial`). Deux façons de l'obtenir :
  - ajouter un tableau par face sur `Mesh` : **invasif**, touche un type central, **à refuser
    maintenant** — c'est de la spéculation payée d'avance ;
  - faire rendre à `ExtrudedMeshBuilder::Append` **la plage de faces qu'il vient d'émettre** (il
    connaît `m_faces.size()` avant et après, `extrude_contours.cpp:237-382`). Un couple d'entiers par
    `Append`, aucune structure nouvelle, aucune modification de `Mesh`.
- **Verdict** : garder les **deux vecteurs légers** (forme vers matériau, forme vers plage de faces).
  Le second n'est pas de la spéculation : il sert **immédiatement** au diagnostic (« quelle forme a
  produit ces faces aberrantes ? ») et aux statistiques du nœud. **Refuser** en revanche toute
  modification de `Mesh` au nom d'un export qui n'est pas demandé.

### 11.6 D2 — une bascule qui gouverne **deux** choses, à séparer dans l'API

La bascule « utiliser les couleurs du SVG » gouverne, telle qu'énoncée, **la couleur**. Mais pour que
son défaut tienne la promesse « comportement actuel », elle doit aussi gouverner **la résolution du
recouvrement** : si la bascule est à `false` mais que M2a s'exécute quand même, la géométrie change
sans qu'aucune couleur ne le justifie, et la promesse de non-régression est rompue.

**Recommandation : deux options indépendantes en C++, une seule bascule exposée dans le gabarit.**

```cpp
// SvgExtrudeOptions — champs AJOUTÉS, tous à défaut « comportement actuel »
bool perShapeMaterials = false;                       // D2, D5
enum class OverlapPolicy { None, Subtract };          // D1 — ZRank retiré, cf. section 13
OverlapPolicy overlapPolicy = OverlapPolicy::None;
bool strokeUsesStrokeColor = true;                    // D4 — lu seulement si perShapeMaterials
```

Le nœud `svg.extrude.colored` expose **un** paramètre `useSvgColors` qui met les deux à la fois. Une
API qui distingue ce que l'interface confond reste testable ; l'inverse ne l'est pas — et c'est
exactement ce qui permet au jalon **K2** de vérifier la non-régression **sans dépendre de M2a**.

Conséquence sur `mesh.color`, vérifiée : bascule à `false`, toutes les faces sortent à
`MATERIAL_NONE`, donc `mesh.color` les peint (`color.cpp:114`) — comportement actuel, inchangé.
Bascule à `true`, aucune face n'est `MATERIAL_NONE`, `mesh.color` ne peint rien et publie
`paintedFaces = 0` (`color.cpp:80-84`, `:124-126`) : le grisage du sélecteur côté JS dispose donc
d'un **indicateur natif**, au lieu d'une convention dupliquée dans le gabarit.

### 11.7 Étagement du recalcul — ce qu'il gagnerait, et ce qu'il ne gagnerait pas

Ce tableau reste vrai le jour où le mémo (option A) sera activé. Il est **la** raison pour laquelle
l'étagement n'est pas une solution générale : il ne protège que la moitié aval des réglages.

| Paramètre | Position dans la chaîne | Étagement le rend gratuit ? |
|---|---|---|
| `depth` (profondeur) | **aval** des booléens | **Oui** — seule l'extrusion est rejouée |
| couleurs / `useSvgColors` (au sens palette) | **aval** | **Oui** — seule la palette change |
| `flattenTol` | **amont**, avant aplatissement | **Non, et jamais** : il change le **nombre de points**, donc l'entrée des booléens. Aucun cache ne peut rattraper cela |
| `strokeScale`, `strokeWidthFallback`, `strokeToVolume` | **amont** | **Non** : ils changent les contours produits |
| `centerAndFit`, `invertY`, `fitSize` | **amont** | **Non** : ils changent les coordonnées, donc les intersections |

**Lecture** : l'étagement protège `depth` et la palette. Il ne protège **rien** de ce qui touche à la
géométrie 2D — et `flattenTol` est précisément le curseur dont la partie 1 faisait le critère
d'interactivité du jalon K4. **Conclusion : l'étagement (option A) répond à la régression de §11.3, il
ne répond pas au coût brut de M2a.** Si K4 est mauvais, c'est M2a qu'il faut alléger (§13), pas le
cache qu'il faut ajouter.

## 12. Plan révisé — séquence de jalons — ⚠ K3b INSÉRÉ avant K4 par D7 (§23)

M2a entrant dans le MVP, l'ancien J3 (rang de recouvrement) **disparaît** et l'ancien J5 remonte. La
séquence est renumérotée **K0 à K6** pour qu'aucune référence ne reste ambiguë. Correspondance :
K0 = J0, K1 = J1, K2 = J2 (allégé), **K3 = ex-J5 (M2a), remonté**, K4 = jalon de chronométrage
**nouveau**, K5 = ex-J4 (nœud + gabarit), K6 = ex-J6 (mode texture, toujours optionnel).

### K0 — Mesurer avant de construire — ✅ FAIT le 2026-09-06 (résultats §16)

- **Construit** : un test qui parse le Tigre avec nanosvg et publie ses comptes.
- **Vérification** : `nShapes == 240`, `nDistinctFillColors == 38`, `nGradients == 0`. Ajouts propres
  à M2a : **aire totale des formes** et **aire de leur union** (le rapport des deux donne le taux de
  recouvrement, donc ce que la soustraction va retirer) ; nombre de formes dont la boîte englobante
  coupe celle d'au moins une forme supérieure.
- **Prérequis bloquant** : `test/data/svg/Ghostscript_Tiger.svg` est **non suivi par git** **[M]** ;
  il doit être ajouté (`test/CMakeLists.txt:63-68` le copiera dans l'arbre d'exécution).
- **Exclu** : rien n'est construit de la feature.

### K1 — `svg_to_shape_groups` (piste A1), géométrie strictement inchangée — ✅ FAIT le 2026-09-06 (résultats §19)

- **Construit** : la fonction groupée ; `svg_to_contours` réimplémentée comme sa concaténation ;
  lecture de `fill.color`, `stroke.color`, `opacity`, `fillGradient` ; test de `NSVG_FLAGS_VISIBLE`.
- **Vérification** : les 15 tests SVG existants passent **sans modification** ; un test neuf compare
  **point par point** la concaténation des groupes à la sortie actuelle sur les cinq fixtures ; **ce
  détecteur est d'abord validé sur un cas positif** (perturber un groupe et constater l'échec).
- **Exclu** : rien n'est peint, rien n'est découpé, aucun changement à l'écran. C'est ce qui rend la
  comparaison point par point exacte.

### K2 — Palette par couleur (M3a + D5), **sans** résolution du recouvrement — ✅ FAIT le 2026-09-06 (résultats §19)

- **Construit** : `perShapeMaterials`, déduplication RGBA, `strokeUsesStrokeColor`, les deux vecteurs
  de correspondance (§11.5). `overlapPolicy` reste à `None`.
- **Vérification** :
  1. `perShapeMaterials == false` donne un maillage **identique** à K1 (les tests existants sont
     l'oracle et ils sont exacts) ;
  2. `perShapeMaterials == true` sur le Tigre : `GetNMaterials()` égale la valeur mesurée en K0 (38
     attendu ; **mesuré 39**, cf. §19 — l'attendu comptait les couleurs de remplissage, pas celles
     des régions produites) ; `materialRanges.size() == GetNMaterials()` ; aucune face à
     `MATERIAL_NONE` ;
  3. sur `square.svg` : `GetNMaterials() == 1`, 16 sommets et 12 faces inchangés.
- **Prédiction à consigner, probablement fausse** : sur un fichier multi-formes, le compte de sommets
  **changera** (un `Append` unique tessellait toutes les formes dans un seul polygone glutess,
  `extrude_contours.cpp:182`). Changement **souhaité**, à mesurer et non à subir.
- **Exclu nommément, renvoyé à K3** : les capots restent coplanaires, donc **z-fighting** sur les
  recouvrements. Le critère de K2 **ne porte pas** sur l'aspect du Tigre à l'écran. K2 n'est **pas**
  un jalon livrable à l'utilisateur : c'est l'étage sur lequel K3 se pose.

### K3 — M2a, marqueterie par différence accumulée (cœur du MVP) — ✅ FAIT le 2026-09-06 (résultats §19)

- **Construit** : `overlapPolicy = Subtract`. Balayage **du dessus vers le dessous** : pour chaque
  forme, `region_i = differenceContours(shape_i, couvert)` puis
  `couvert = unionContours(couvert, shape_i)` (`contour_ops.h:116`, `:107`). Statistiques publiées :
  formes intégralement recouvertes, aire retirée, durée.
- **Vérification** :
  1. **Oracle de volume signé** (`tu_cgmesh_svg_stroke.cpp:33-41`) exact sur deux carrés se
     recouvrant à moitié : `volume == aire du capot * hauteur`. C'est **le** test qui distingue M2a
     de tout le reste, et il est **faux** sous K2 ;
  2. aucune paire de triangles de capot ne partage un point intérieur (régions XY disjointes) ;
  3. sur le Tigre, `aire(union des régions produites) == aire(union des formes)` à la tolérance
     Clipper2 près — la soustraction ne doit **rien perdre** hors recouvrement ;
  4. le compte de formes disparues est **publié**, non nul, et cohérent avec K0.
- **Exclu nommément** : la performance. K3 vérifie la **correction**, K4 le **coût**. Séparer les deux
  est ce qui permet de garder K3 même si K4 est mauvais.

### K4 — Chronométrage, et **règle de bascule** (jalon décisionnel) — ✅ FAIT le 2026-09-06 (résultats §27)

- **Construit** : rien de fonctionnel. Un chronométrage instrumenté, en trois postes séparés :
  **(a)** parse nanosvg + aplatissement, **(b)** résolution M2a (Clipper2), **(c)** tessellation +
  extrusion. Trois postes et non un total : c'est la seule façon de savoir **quoi** alléger.
- **Instruments** : côté natif, un test chronométré sur le Tigre ; côté navigateur, le `ms` déjà rendu
  par `updateInPlace` (`viewer.js:257`, `:350`) sur un mouvement de « Profondeur » **et** sur un
  mouvement de « Finesse d'échantillonnage » — les deux, parce que §11.7 montre qu'ils ne coûtent pas
  la même chose.
- **Règle de bascule, validée par l'utilisateur le 2026-09-06** :

  | Mesure sur le Tigre | Décision |
  |---|---|
  | **< ~300 ms** | On en reste à **G2 nu**. D6 est close, §11.3 est sans conséquence, **R6 est levée**. |
  | **> ~300 ms** | **Consigner et remonter**, ne rien activer d'office. K4 publie les trois postes (§13) et l'arbitrage revient à l'utilisateur. Si c'est le poste (b) — Clipper2 — qui domine, le mémo ne suffirait de toute façon pas : lire d'abord le repli n°1 (indexation spatiale, §13). |

- **Exclu nommément** : K4 ne juge **pas** la qualité du résultat. Un chronomètre ne dit rien de la
  géométrie.

### K4b — Repli n°1, indexation spatiale de la marqueterie (inséré entre K4 et K5) — ✅ FAIT le 2026-09-06 (résultats §28)

- **Pourquoi `K4b` et non une renumérotation** : même raison qu'en K3b. K0 à K4 sont faits, verts et
  cités partout ; `K4b` insère sans rien invalider.
- **Construit** : `svg_subtract_overlaps` ne soustrait plus l'accumulateur global mais les seules
  formes sus-jacentes dont la boîte englobante coupe celle de la forme courante. Statistiques
  ajoutées : `overlapPairs` / `candidatePairs`, l'assiette de l'indexation.
- **Vérification** : changement de performance à géométrie **strictement constante**, donc les tests
  K0–K4 passent **sans modification d'aucun attendu**, plus un détecteur d'égalité ancien/nouveau,
  région par région, sur cinq fixtures — vu échouer sous perturbation avant d'être cru.
- **Exclu nommément** : le mémo (option A). L'utilisateur l'a écarté par D8.

### K5 — Nœud `svg.extrude.colored` (G2) et gabarit (D2, D3) — ✅ FAIT le 2026-09-06 (résultats §29)

- **Construit** : le nœud, son enregistrement au catalogue, la bascule `useSvgColors` qui met à la
  fois `perShapeMaterials` et `overlapPolicy` (§11.6), la mise à jour de
  `maker/web/data/templates/svg.json`, le grisage du sélecteur de couleur piloté par
  `paintedFaces == 0`, les statistiques `gradientShapes` / `hiddenShapes` / `subtractedShapes`.
- **Vérification** :
  1. un document `svg.json` **antérieur** (`svg.contours`, `shape.extrude`, `mesh.color`) s'ouvre et
     s'évalue toujours — `svg.contours` n'est pas retiré ;
  2. bascule à `false` : le rendu est **celui d'aujourd'hui**, et `mesh.color` publie
     `paintedFaces == nombre de faces` ;
  3. bascule à `true` : `materials.filter(m => m.kind === "color").length >= 30` en console, et
     `mesh.color` publie `paintedFaces == 0` ;
  4. le sélecteur « Couleur de la pièce » est **grisé** exactement dans le cas 3.
- **Exclu nommément** : l'export. Le STL ne porte pas de couleur (`exporters.js:88`) et
  `graphExportObj` rend un OBJ **sans `mtllib`** (`wasm_api.cpp:745-750`). Les couleurs sont
  **visibles mais non exportables** ; c'est déjà écrit dans le gabarit (`svg.json:108`) et cela reste
  vrai. Chantier distinct.

### K6 — Mode texture (M3b), optionnel, hors v1

Inchangé par rapport à l'ancien J6 : vendoriser `nanosvgrast.h`, réutiliser
`region_apply_planar_uvs` (`image_region_pipeline.h:213`) et `region_make_texture_material` (`:225`).
Critère : le Tigre affiché avec **un seul matériau `kind:"texture"`** et un rendu fidèle des dégradés.

### Ce qui a disparu du plan

- **Ancien J3 (rang de recouvrement)** : supprimé. M2b' est écarté par D1.
- **`OverlapPolicy::ZRank`** : **ne pas l'écrire**. Une énumération à trois valeurs dont une n'est
  jamais atteinte est un chemin mort qu'il faudra tester, documenter et maintenir. Deux valeurs
  suffisent ; la troisième s'ajoutera le jour où §13 la ramènera comme repli — et ce jour-là elle
  aura une raison d'être.

## 13. Réserves mises à jour — ⚠ SUPERSÉDÉE par la section 18

| Réserve | État au 2026-09-06 | Levée / repli |
|---|---|---|
| **R1** — nombre de `NSVGshape` réellement produits | inchangée | **K0** |
| **R2** — profondeur d'empilement du graphe de recouvrement | **SANS OBJET.** Elle n'existait que pour dimensionner le nombre de niveaux de M2b'. M2b' étant écarté (D1), plus rien n'en dépend. Le balayage accumulatif de M2a **n'a pas besoin** du graphe de recouvrement | — (voir toutefois le repli 2 ci-dessous, qui **réemploie** l'instrument qu'elle aurait servi) |
| **R3** — coût Clipper2 de M2a | **DEVIENT CRITIQUE** : elle est désormais **sur le chemin du MVP**, et non plus sur celui d'une cible lointaine | **K4**, avec la règle de bascule. Plan de repli ci-dessous |
| **R4** — volumétrie du maillage à un facteur ~3 près | inchangée. M2a la **réduit** (la matière recouverte n'est plus tessellée) : l'estimation devient un **majorant** | **K0**, puis constat en K3 |
| **R5** — `Ghostscript_Tiger.svg` non suivi par git | inchangée | prérequis bloquant de **K0** |
| **R6** *(nouvelle)* — perte de gratuité de `depth` sous G2 (§11.3) | nouvelle, introduite par D3 | **K4**, issue (b) de §11.3 |

### Plan de repli si K4 est mauvais — par ordre de préférence

Le poste chronométré qui domine décide du repli. C'est pour cela que K4 mesure **trois postes** et
non un total.

1. **Le poste (b) domine, et le fichier est très recouvrant → alléger l'accumulateur.**
   Le balayage naïf fait croître `couvert` jusqu'à la complexité du dessin entier ; chaque
   `Difference` paie alors le dessin entier pour une forme qui n'en touche qu'une fraction. Le
   remède : **indexer spatialement les formes déjà posées** et ne soustraire à `shape_i` que
   **l'union des formes supérieures dont la boîte englobante coupe la sienne**. Le coût passe de
   O(n · |total|) à O(Σ n_recouvrantes). **C'est ici que l'instrument de R2 est réemployé** — les
   28 680 tests de boîtes englobantes chiffrés en §4.2 coûtent moins d'une milliseconde **[E]** et
   servent maintenant à cela. Correction inchangée : la différence est locale par construction.
2. **Le poste (b) domine et le repli 1 ne suffit pas → réintroduire `OverlapPolicy::ZRank`**, c'est-à-dire
   M2b' (§4.2, conservé dans les pistes écartées **exactement pour ce cas**). On perd la pièce en
   éléments rapportés, on garde les couleurs et l'absence de z-fighting. **C'est un recul assumé sur
   D1, pas un contournement** : il devra être présenté comme tel à l'utilisateur.
3. **Le poste (a) domine → mémo interne (option A)**, indexé sur (chemin + identité du contenu +
   paramètres amont). Attention : §11.7 rappelle qu'il **ne protège pas** `flattenTol`.
4. **Aucun poste ne domine et tout est simplement lourd → temporiser côté interface** : anti-rebond
   sur les curseurs dans `template.js`, et **aperçu à tolérance dégradée** pendant le glissement,
   plein calcul au relâchement. Palliatif, pas correctif — à ne prendre qu'en dernier.
5. **Ce qui n'est PAS un repli disponible : le calcul asynchrone.** `cggraph::AsyncEvaluator` lève
   sous Emscripten sans `-pthread` (`maker/graph_host.h:19-23`) et la page gabarit évalue **sur le
   thread UI** (`template.js:10-17`). **[V]** Toute seconde de calcul est une seconde de page figée.
   Le dire ici évite qu'on le propose au moment où l'on cherchera une issue.

## 14. Incohérences introduites par les décisions — complétée par la section 20

Quatre points. Le premier est le seul qui soit structurel ; les trois autres sont des ajustements.

1. **D3 (G2) annule la propriété de cache qui rendait `depth` gratuit.** Détaillé en §11.3, suivi par
   R6. C'est une **régression de réactivité**, pas un défaut de correction. Elle est mesurable (K4) et
   réversible (issue b ou c). **À trancher par l'utilisateur si K4 dépasse 300 ms.**

2. **D1 et D3 se renforcent défavorablement.** D1 met le calcul le plus cher (Clipper2) dans le MVP ;
   D3 le met dans le nœud dont **tous** les paramètres invalident le cache. Prises séparément, chacune
   est raisonnable ; ensemble, elles placent le calcul le plus lourd sur le chemin le plus souvent
   rejoué. C'est exactement pourquoi K4 est un **jalon à part entière** et non une note en bas du
   jalon K3.

3. **D2 telle qu'énoncée gouverne une seule chose et doit en gouverner deux.** Si la bascule ne pilote
   que la couleur, son défaut ne tient pas la promesse « comportement actuel » dès que M2a existe.
   Résolu en §11.6 par deux options C++ derrière une bascule unique. **Sans conséquence pour
   l'utilisateur**, mais à ne pas oublier à l'implémentation : c'est une promesse silencieuse.

4. **D4 est moins important qu'il n'y paraît sur le Tigre.** Le paramètre ne joue que sur les ~13
   formes `fill:none`, parce qu'un trait de forme fermée et remplie est **ignoré** aujourd'hui
   (`import_svg.cpp:302-311`) **[V]**. Ce n'est pas une incohérence de la décision, c'est une
   **surestimation de son effet** dans la question posée. Corollaire utile : le vrai manque, pour un
   SVG à contours noirs, reste que **le trait d'une forme remplie n'est jamais produit** — c'est un
   chantier séparé, plus visible que D4, et il n'est pas dans ce plan.

*Point vérifié qui n'est PAS une incohérence, contrairement à ce qu'on pourrait craindre* : D5
(déduplication RGBA) et D1 (M2a) ne se gênent pas. La déduplication porte sur la **palette**, le
balayage M2a sur les **régions** ; deux formes de même couleur restent deux `Append` distincts, avec
le même `materialId`. `BuildPolygonRenderData` les réunit dans une seule `MaterialRange`
(`mesh.cpp:917-926`) sans que la géométrie en soit affectée. **[V]**

## 15. Journal de prédictions — révisé — ⚠ SUPERSÉDÉ par la section 19

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K0 | `nShapes == 240` | test nanosvg sur le Tigre | | | |
| K0 | `nDistinctFillColors == 38` | idem | | | |
| K0 | `nGradients == 0` | idem | | | |
| K0 | taux de recouvrement (aire totale / aire de l'union) entre 1,5 et 3 | test | | | |
| K0 | sommets du maillage actuel entre 2,4e4 et 8e4 | `Mesh::GetNVertices` | | | |
| K1 | les 15 tests SVG existants passent inchangés | `ctest` | | | |
| K1 | concaténation des groupes identique a la sortie actuelle, point par point | test de non-régression, validé sur cas positif | | | |
| K2 | `perShapeMaterials=false` donne un maillage identique | tests existants | | | |
| K2 | `GetNMaterials() == 38` sur le Tigre | test | | | |
| K2 | **le compte de sommets change sur les fichiers multi-formes** | comparaison K1 / K2 | | | |
| K3 | oracle de volume signé exact sous `Subtract` | `tu_cgmesh_svg_stroke.cpp:61-100` | | | |
| K3 | aire de l'union des régions produites égale l'aire de l'union des formes | test | | | |
| K3 | M2a **réduit** le nombre de sommets par rapport a K2 | comparaison K2 / K3 | | | |
| K4 | poste (b), Clipper2, inférieur a 1 s sur le Tigre — **R3** | chronométrage natif | | | |
| K4 | recalcul complet inférieur a 300 ms sur le Tigre | `viewer.js:257`, `:350` | | | |
| K4 | `depth` coûte **le même prix** qu'un recalcul complet sous G2 — **R6** | deux chronométrages comparés | | | |
| K5 | bascule a `false` : rendu identique a aujourd'hui, `paintedFaces == nFaces` | statistique de `mesh.color` | | | |
| K5 | bascule a `true` : au moins 30 matériaux `kind: "color"`, `paintedFaces == 0` | console navigateur | | | |

Liste fermée des causes : 1 affirmation sur l'existant non vérifiée · 2 instrument biaisé ·
3 hypothèse d'environnement non vérifiée · 4 structure conçue sans charge · 5 critère non falsifiable ·
6 excès de pessimisme · 7 mécanisme de vérification neutralisé par le code.

### Ce qui n'a pas été examiné — complément à la section 9

La révision du 2026-09-06 a **levé** les points 5 et 10 de la section 9 pour la partie cache :
`graph_api.cpp:80-105` et l'ensemble `signature.{h,cpp}` / `evaluator.{h,cpp}` ont été lus. Restent
non examinés : `src/cggraph/ui/eval_driver.h` et `editor_model.h` (le pilote et le modèle sont pris
sur parole quant au fait qu'ils n'invalident pas le cache entre deux `graphEvaluate` — **[H]**, à
confirmer si K4 mesure des recalculs inattendus), ainsi que `graph-worker.js`, hors du chemin des
pages gabarit.

---
---

# PARTIE 3 — Résultats du jalon K0 (2026-09-06)

**Statut du document.** Il cesse ici d'être une analyse purement statique. Le jalon K0 a été
**exécuté** par le sous-agent `developer` : build Release MSVC dans `build/local-windows-paths`,
**20/20 tests SVG verts** sous `ctest` (les 15 préexistants plus les 5 de
`test/tu_cgmesh_svg_k0_metrics.cpp`). **Aucun fichier de production n'a été touché.**

Frontière de statut, désormais mobile — c'est le premier jalon où elle bouge :

- **[M]** signifie maintenant **mesuré par exécution**, avec le test qui le produit. Le §3 employait
  `[M]` au sens « mesuré par `grep` sur le texte du fichier » ; cet usage est **rétrogradé** en
  **[M-grep]** partout où la partie 3 le contredit ou le confirme.
- **[E]** reste réservé à ce qu'aucune exécution n'a encore touché : essentiellement le coût de la
  moitié `Difference` de M2a, et l'extrapolation native vers WASM.

## 16. Mesures K0 — table de référence

Instrument : `test/tu_cgmesh_svg_k0_metrics.cpp`, 5 tests
(`tiger_base_counts`, `reference_files_base_counts`, `tiger_overlap_metrics`, `tiger_volumetry`,
`reference_files_volumetry`). Fichier : `test/data/svg/Ghostscript_Tiger.svg`, objet ajusté à 1.0.

| Grandeur | Prédit au doc | **Mesuré [M]** | Verdict |
|---|---|---|---|
| `nShapes` | 240 | **239** | **écart −1**, cause identifiée (§17.1) |
| `nDistinctFillColors` | 38 | **38** | exact |
| `nGradients` | 0 | **0** | exact |
| formes avec `fill` | ~227 | **226** | cohérent avec l'écart −1 |
| formes avec `stroke` | 78 | **78** | exact |
| formes avec les deux | non prédit | **65** | — |
| formes avec ni l'un ni l'autre | non prédit | **0** | — |
| formes au TRAIT seul (`stroke` sans `fill`) | ~13 (déduit) | **13** (78 − 65) | **exact** |
| sous-chemins fermés / ouverts | 227 / 13 | **226 / 13** | cohérent |
| `fillRule == EVENODD` | 0 | **0** | exact |
| formes sans `NSVG_FLAGS_VISIBLE` | inconnu | **0** | §17.4 |
| aire totale des formes | non prédit | **2,0981** | — |
| aire de leur union | non prédit | **0,6271** | — |
| **taux de recouvrement** | ratio prédit entre 1,5 et 3 | **ratio 3,346**, soit **70,1 %** | **hors fourchette** (§20.1) |
| formes coupant une forme supérieure | non prédit | **197 / 239** | — |
| paires de bbox sécantes | majorant 28 680 | **2 912** (10,2 %) | §18, repli n°1 |
| points de contour à `flattenTol = 0.005` | 6e3 – 2e4 | **8 491** | dans la fourchette, tiers bas |
| contours rendus par `svg_to_contours` | non prédit | **259** | 1,08 par forme |
| sommets du maillage | 2,4e4 – 8e4 | **36 988** | dans la fourchette, tiers bas |
| faces du maillage | 2,4e4 – 8e4 | **56 888** | dans la fourchette, **mais** §17.2 |
| union accumulée Clipper2 (239 unions) | non mesurable avant K0 | **29 – 46 ms** natif Release | §19, R3 |

Fichiers de comparaison : `rose.svg` 20 formes / 635 points / 2 540 sommets ;
`spiderman.svg` 5 formes / 1 151 points / 4 596 sommets. **[M]**

## 17. Corrections apportées aux sections antérieures

### 17.1 §3 — 239 formes et non 240 : une leçon de méthode sur la mesure par `grep`

**Le compte textuel était exact ; c'est l'inférence qui était fausse.** Il y a bien **240** balises
`<path>` dans le fichier — `grep` ne s'est pas trompé. Mais nanosvg n'en produit que **239 formes**,
et la cause est **vérifiée dans la source** :

- le chemin `d="M-65.4,9z"` est un `moveto` seul, présent une fois dans le fichier **[M-grep]** ;
- `nsvg__addPath` **écarte** tout sous-chemin de moins de 4 points de contrôle :
  `if (p->npts < 4) return;` — **`extern/nanosvg/nanosvg.h:1051`** **[V]** ;
- `nsvg__addShape` **ne crée aucune forme** quand la liste de sous-chemins est vide :
  `if (p->plist == NULL) return;` — **`extern/nanosvg/nanosvg.h:966`** **[V]**.

**Ce que cela apprend, et qui vaut au-delà de ce fichier.** La RÉSERVE R1 avait bien déclaré que le
compte par `grep` était un **majorant**, mais elle avait énuméré **deux** mécanismes de biais — les
`<g>` sans `<path>` descendant, et les `<path>` à plusieurs sous-chemins — et **manqué le troisième,
qui est celui qui s'est produit** : *un chemin syntaxiquement valide que le parseur rejette comme
dégénéré*. Un inventaire par recherche textuelle voit des **balises**, pas ce qu'un parseur en fait.
Formulation à retenir pour les mesures suivantes :

> Compter les balises d'entrée majore ce que le parseur produit d'au moins trois façons :
> conteneurs vides, agrégation de sous-chemins, **et rejet des entrées dégénérées**. La seule
> mesure fiable du nombre d'objets produits est de les compter **après parse**.

Écart : **−1 sur 240, soit 0,4 %**. Sans conséquence sur aucune conclusion du document — mais la
cause, elle, en a une : c'est la troisième fois qu'un chiffre issu de `grep` doit être requalifié, et
c'est désormais une **règle de méthode** et non un accident (§19, cause 2).

**Correction du §3** : la ligne « `<path>` : 240, exact » reste vraie comme compte de balises ; la
ligne de lecture « 240 formes, dont ~227 avec un remplissage réel et ~13 héritant du `fill="none"`
racine » devient : **239 formes, dont 226 remplies, 78 à trait, 65 aux deux, 13 au trait seul, 0
sans peinture.** **[M]**

### 17.2 §5.1 — volumétrie : les trois lignes tombent dans la fourchette, une approximation était fausse

| Ligne du §5.1 | Fourchette **[E]** | **Mesuré [M]** | Position |
|---|---|---|---|
| Points de contour après aplatissement | 6e3 – 2e4 | **8 491** | tiers bas |
| Sommets | 2,4e4 – 8e4 | **36 988** | tiers bas |
| Triangles | 2,4e4 – 8e4 | **56 888** | milieu |
| Charge utile | 1 – 3 Mo | **≈ 1,57 Mo** (24 o × 36 988 + 12 o × 56 888) | tiers bas |

**R4 est LEVÉE.** Trois observations à consigner, dont une correction :

1. **La relation « 4 blocs de n sommets » est vérifiée exactement** : 36 988 / 4 = **9 247**, entier.
   Cela confirme `extrude_contours.cpp:259-262` **et** qu'aucun sommet n'a été écarté par `Build`
   (`extrude_contours.cpp:394-413`) — il n'y a pas de `wallFilter` sur ce chemin. **[M + V]**
2. **8 491 points de contour donnent 9 247 sommets par bloc, soit +8,9 %.** Ces 756 points
   supplémentaires viennent des intersections résolues par Clipper2 **ou** de la reprise `COMBINE` de
   glutess (`extrude_contours.cpp:73-84`) — **cette mesure ne les distingue pas**. **[M pour le
   total, H pour l'attribution.]** ⚠ **K2 tranche** : ce sont des reprises `COMBINE`, dont **740**
   dues aux intersections **entre** formes et **16** aux auto-intersections d'une même forme (§19).
   L'attribution passe de [H] à [M]. R4 avait annoncé ce biais (« minorée par les points que Clipper2
   ajoute aux intersections ») : il est **confirmé dans son sens et son ordre de grandeur**.
3. **L'approximation « triangles ≈ sommets » du §5.1 était optimiste d'environ 50 %.** Le rapport
   mesuré est **56 888 / 36 988 = 1,54**. En détail : parois ≈ 2 par arête de contour ≈ 18 494 ;
   capots ≈ 38 394, soit **4,15 triangles par point de contour**, très au-dessus du `n − 2` d'une
   triangulation simple. **Cause probable [H]** : les 259 contours sont tessellés dans **un seul
   polygone glutess** en NONZERO (`extrude_contours.cpp:182`) et se recoupent massivement (70,1 % de
   recouvrement) — glutess doit résoudre ces intersections, ce qui multiplie les triangles. C'est la
   **même cause** que le symptôme initial du bloc monolithique, vue par un autre angle.
   Sans conséquence sur l'ordre de grandeur ni sur aucune décision.

### 17.3 §11.4 — D4 : la déduction est **confirmée par la mesure**, statut [H] → [V+M]

Le §11.4 avait déduit de `import_svg.cpp:302-311` que `strokeUsesStrokeColor` ne jouerait que sur les
formes `fill:none` + `stroke`, et estimé leur nombre à **~13**. La mesure donne :
**78 formes à `stroke`, dont 65 ont aussi un `fill` ⇒ 78 − 65 = 13 formes au trait seul.** **[M]**

L'estimation était **exacte**. Le paramètre `strokeUsesStrokeColor` porte donc sur **13 formes sur
239, soit 5,4 %** du Tigre. Conclusions inchangées : défaut `true`, déterminant pour un dessin au
trait, **marginal pour le Tigre**. La crainte « les traits fragmentent les remplissages sous M2a » est
définitivement dimensionnée : **13 formes découpantes, pas 78**.

### 17.4 §9 / K1 — `NSVG_FLAGS_VISIBLE` : le défaut est réel, le Tigre ne l'exerce pas

**0 forme invisible** sur 239. **[M]** Le manque signalé au §9 (« `NSVG_FLAGS_VISIBLE` jamais testé :
une forme `display:none` est extrudée », `nanosvg.h:160` contre `import_svg.cpp:276`) reste un
**vrai défaut préexistant** et le correctif d'une ligne prévu en K1 reste justifié — mais il
**ne produira aucun effet observable sur ce fichier**.

> À écrire dans le critère de K1 : **ne pas chercher un changement visuel sur le Tigre après ce
> correctif.** Un critère qui attendrait un effet ici serait un critère que la donnée rend
> impossible à satisfaire. Le vérifier demande une fixture dédiée portant un `display:none`.

## 18. Réserves après K0 — remplace la section 13 — ⚠ R3 et R6 MISES À JOUR par §27.7

| Réserve | État au 2026-09-06, après K0 | Suite |
|---|---|---|
| **R1** — nombre de `NSVGshape` réellement produits | **CLOSE.** 239 mesuré, 240 prédit. Cause vérifiée (§17.1) | — |
| **R2** — profondeur d'empilement du graphe de recouvrement | **CLOSE et sans objet** (M2b' écarté par D1). Son instrument est en revanche **mesuré et favorable** : 2 912 paires de bbox sécantes sur 28 680 possibles, soit **10,2 %** | réemployée par le repli n°1, chiffrée ci-dessous |
| **R3** — coût Clipper2 de M2a | **VERSANT ROBUSTESSE : LEVÉ** par K3 — 239 soustractions accumulées sur le Tigre, aucun contour dégénéré, aucune orientation à corriger, aucune région trouée (§19). **VERSANT COÛT : PRÉCISÉ, NON LEVÉ** — balayage complet mesuré **219 à 232 ms natif Release**, soit **~2,2 ×** l'extrapolation « ~100 ms » ci-dessous : `Difference` n'est pas à parité avec `Union` | **K4**, qui reste décisionnel |
| **R4** — volumétrie du maillage | **LEVÉE.** Les trois lignes tombent dans la fourchette (§17.2) | — |
| **R5** — `Ghostscript_Tiger.svg` non suivi par git | **TOUJOURS OUVERTE.** `git status` au 2026-09-06 : `?? test/data/svg/Ghostscript_Tiger.svg` **et** `?? test/tu_cgmesh_svg_k0_metrics.cpp` **[M]**. K0 a tourné **localement** ; en l'état, ni la fixture ni le test ne sont dans le dépôt | à stager avec la livraison de K0 |
| **R6** — perte de gratuité de `depth` sous G2 (§11.3) | inchangée | **K4** |
| **R7** — effet de M2a sur le nombre de sommets | **CLOSE, et §20.2 avait raison.** Mesuré : points de contour **8 491 → 15 213** (+79 %), sommets **36 988 → 60 580** (+64 %), triangles de capot supérieur **22 987 → 13 629** (−41 %). M2a **alourdit** le maillage ; la charge utile passe de ~1,57 Mo à ~2,14 Mo. La frontière l'emporte sur l'aire, exactement comme annoncé | — |

### R3 — ce que K0 a tranché, et ce qu'il n'a pas tranché

**Mesuré** : les **239 `Union` accumulées** coûtent **29 à 46 ms en natif Release**. **[M]** La mesure
porte sur la **bonne charge** : l'accumulateur croît réellement jusqu'à la complexité du dessin
entier, donc le terme dominant est inclus, il n'est pas extrapolé.

**Non mesuré** : la moitié `Difference`. Extrapolation à parité (même balayage de Vatti, entrées
comparables) : **~60 à 92 ms natif** pour les deux moitiés, arrondi à **~100 ms natif** **[E]**. Puis
facteur WASM de 2 à 4 sur ce type de code : **~200 à 400 ms** **[E]**.

| Ce que K0 a fait | Ce que K0 n'a **pas** fait |
|---|---|
| **Écarté le scénario « plusieurs secondes »** que l'estimation du §4.2 (« ~3e7 opérations, quelques centaines de ms à quelques secondes ») laissait ouvert. La borne haute de cette estimation est **infirmée** | **Trancher D6.** L'extrapolation ~200–400 ms est **à cheval sur le seuil de 300 ms** de la règle de bascule |
| Confirmé la borne basse de l'estimation | Mesurer `Difference`, ni le coût en WASM, ni le poste (a) parse, ni le poste (c) extrusion |

> **K0 n'a pas décidé de D6. K4 reste le jalon décisionnel**, et il l'est plus que jamais : le
> résultat attendu tombe précisément dans la zone où la règle validée dit « ne rien changer sans une
> seconde mesure ». Prévoir donc **dès K4** la mesure sur `rose.svg` et `spiderman.svg`, qui sont
> déjà instrumentés par `reference_files_volumetry`.

### Repli n°1 (indexation spatiale) — dimensionné par K0, **à ne pas écrire maintenant** — ⚠ ÉCRIT par K4b (D8) ; le facteur ~10 annoncé ici est **INFIRMÉ**, voir §28.3

- **2 912** paires de boîtes englobantes sécantes sur **28 680** possibles ⇒ chaque forme a en moyenne
  `2 × 2912 / 239 = 24,4` partenaires de recouvrement, contre **239** dans l'accumulateur naïf.
- **Facteur de réduction attendu du travail de soustraction : ~10** (239 / 24,4 = 9,8). **[E bâti sur
  M]** Ramené à l'extrapolation ci-dessus, cela ferait passer M2a de ~200–400 ms à **~20–40 ms** en
  WASM.
- **Décision : ne pas l'écrire maintenant.** Il est **dimensionné, chiffré et documenté** ; l'écrire
  avant K4 serait payer une optimisation dont la mesure n'a pas encore établi le besoin — et
  contredirait D6. Il devient le **premier réflexe** si K4 dépasse le seuil.
- Note : **197 formes sur 239 coupent une forme supérieure** **[M]**. Le repli ne peut donc pas
  espérer mieux que ce facteur ~10 : 82 % des formes ont bien du travail de soustraction à faire, ce
  n'est pas un cas où l'indexation ramènerait le coût à zéro.

### Effet de bord favorable, mesuré : D1 est confortée

**197 formes sur 239 (82 %) coupent une forme supérieure**, et le recouvrement est de **70,1 %**.
**[M]** C'est la confirmation quantitative que M2c (extrusion indépendante, capots coplanaires) aurait
produit du z-fighting sur la quasi-totalité du dessin, et que M2b' aurait eu besoin de beaucoup de
niveaux. **D1 (aller directement à M2a) était le bon choix**, et ce n'est plus une intuition.

## 19. Journal de prédictions — K0 renseigné, K3 révisé (remplace la section 15)

### Prédictions K0 — closes

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K0 | `nShapes == 240` | `tiger_base_counts` | **239** | **−1 (0,4 %)** | **2** — instrument biaisé : `grep` compte des balises, pas ce que le parseur en fait ; le mécanisme « chemin dégénéré rejeté » (`nanosvg.h:1051`, `:966`) manquait à l'inventaire de R1 |
| K0 | `nDistinctFillColors == 38` | `tiger_base_counts` | **38** | 0 | — |
| K0 | `nGradients == 0` | `tiger_base_counts` | **0** | 0 | — |
| K0 | formes au trait seul ≈ 13 (déduit de `import_svg.cpp:302-311`) | `tiger_base_counts` | **13** | 0 | — |
| K0 | ratio aire totale / aire union entre 1,5 et 3 | `tiger_overlap_metrics` | **3,346** | **+11,5 % au-dessus de la borne haute** | **1** — affirmation sur l'existant non vérifiée : la fourchette était une intuition sur « un dessin peintre », rien ne l'avait mesurée |
| K0 | points de contour entre 6e3 et 2e4 | `tiger_volumetry` | **8 491** | dans la fourchette | — |
| K0 | sommets entre 2,4e4 et 8e4 | `tiger_volumetry` | **36 988** | dans la fourchette | — |
| K0 | triangles ≈ sommets (§5.1) | `tiger_volumetry` | **56 888**, soit **1,54 ×** | **+54 %** | **4** — structure décrite sans sa charge : l'estimation supposait une triangulation simple, alors que 259 contours se recoupant à 70 % passent dans **un seul** polygone glutess |
| K0 | coût M2a « quelques centaines de ms à quelques secondes en WASM » (§4.2) | `tiger_overlap_metrics` (moitié Union) | **29–46 ms natif** ⇒ ~200–400 ms WASM **[E]** | **borne haute infirmée** | **6** — excès de pessimisme sur la borne supérieure ; la borne basse tient |

### Prédictions ouvertes — K1, K2

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K1 | les 15 tests SVG préexistants passent inchangés | `ctest` | **verts, aucun fichier de test touché** (8 `tu_cgmesh_svg` + 7 `tu_cgmesh_svg_stroke`) | 0 | — |
| K1 | concaténation des groupes identique a la sortie actuelle, point par point | `tu_cgmesh_svg_k1_groups.groups_concatenate_to_the_flat_contour_list` | **identique sur les 7 fixtures**, avec et sans `centerAndFit` | 0 | — |
| K1 | le correctif `NSVG_FLAGS_VISIBLE` ne change **rien** sur le Tigre (0 forme invisible) | `tiger_volumetry` avant / après | **8 491 points, 36 988 sommets, 56 888 faces, 259 contours — avant comme après** | 0 | — |
| K2 | `perShapeMaterials=false` donne un maillage identique | `tu_cgmesh_svg_k2_materials.disabled_materials_reproduce_the_k1_mesh` + suite existante | **identique sommet par sommet et face par face** au maillage reconstruit (`svg_to_contours` puis un `Append` unique) sur le Tigre et `square.svg` ; les 36 tests SVG préexistants verts **sans modification** ; K0 rend toujours 239 / 8 491 / 36 988 / 56 888 | 0 | — |
| K2 | `GetNMaterials() == 38` sur le Tigre | `tiger_palette_holds_one_material_per_distinct_color` | **39** | **+1** | **1** — affirmation sur l'existant non vérifiée : l'attendu comptait les couleurs de **remplissage** (K0, `nDistinctFillColors = 38`), pas celles des **régions produites**. Les 13 régions issues d'un trait portent la couleur de leur `stroke` (D4, §11.4), et l'une d'elles — `#a51926`, la seule des 4 couleurs de trait absente des remplissages — ajoute un 39ᵉ matériau |
| K2 | sommets **inchangés a 36 988** (le découpage en 239 `Append` ne change pas Σ n_i) | `Mesh::GetNVertices` | **33 824** | **−3 164, soit −8,6 %** | **4** — structure décrite sans sa charge : « Σ n_i inchangé » suppose n_i = points de contour, alors que n_i est la sortie du **tessellateur**. Détail mesuré : 8 491 points de contour, +16 sommets de reprise (COMBINE) intra-forme, −51 sommets tessellés que **aucun** triangle ne référence et que `Build` écarte (`extrude_contours.cpp:389-413`) ⇒ 8 456 × 4 = 33 824 |
| K2 | **faces en forte BAISSE** depuis 56 888 : la tessellation par forme supprime les intersections que le polygone glutess unique devait résoudre (§17.2). Bande attendue **2,0e4 – 3,5e4** | `Mesh::GetNFaces` | **32 756**, soit **−42,4 %** | **dans la bande**, tiers haut | — |

### Ce que K1 a appris, hors prédiction

- **Le détecteur a été validé sur un cas positif avant d'être cru.** Perturbation appliquée aux
  groupes juste avant la comparaison : échange des groupes 0 et 1, puis décalage de +1e-3 sur la
  première coordonnée. La suite est passée au rouge sur `rose.svg` (« contour 0 : attendu 23 points,
  obtenu 5 »), `spiderman.svg` (« attendu 280 points, obtenu 17 ») et `Ghostscript_Tiger.svg`
  (« contour 0, point 0 : attendu (-0.363938, 0.050992), obtenu (-0.356004, 0.056930) »). La
  perturbation a ensuite été retirée. Trois cas positifs **permanents** restent dans la suite
  (`the_detector_rejects_a_moved_point` / `_swapped_groups` / `_a_missing_group`) pour que le
  comparateur ne puisse pas pourrir en silence.
- **Quatre des sept fixtures ne rendent qu'UN groupe** : `square`, `triangle`, `nazca`, `batman`.
  Elles ne disent donc rien de l'ordre, et seules `rose`, `spiderman` et le Tigre l'exercent. Le test
  compte les fixtures multi-groupes et exige qu'il y en ait au moins trois — sans ce compte, la
  disparition d'une fixture multi-formes rendrait la suite verte sans plus rien vérifier.
- **Le Tigre ne porte aucune forme masquée** (§17.4) : le correctif `NSVG_FLAGS_VISIBLE` n'est
  couvert que par la fixture dédiée `test/data/svg/hidden_shape.svg`. Son oracle n'est pas un
  comptage mais une **géométrie** : les deux formes `display:none` débordent de l'emprise du carré
  visible, donc les extruder changerait l'échelle du recentrage-ajustement global — le résultat de
  `hidden_shape.svg` doit coïncider point par point avec celui de `square.svg`.
- **nanosvg n'honore que `display:none`**, en attribut ou en style ; `visibility:hidden` n'est pas lu
  (`nanosvg.h:1819-1822`) **[V]**. Un critère qui aurait porté sur `visibility` aurait été
  insatisfiable.

### Ce que K2 a appris, hors prédiction

- **L'attribution laissée ouverte au §17.2 est tranchée.** Les 756 sommets qui séparaient les 8 491
  points de contour des 9 247 sommets par bloc étaient des reprises **COMBINE de glutess**, et non
  des points ajoutés par Clipper2 — la mesure de 8 491 est prise **après** Clipper2. K2 les
  décompose : **740 venaient des intersections ENTRE formes**, que la tessellation par forme ne
  rencontre plus, et **16 seulement** des auto-intersections **d'une même** forme. **[M]**
- **51 sommets tessellés ne sont référencés par aucun triangle** et sont écartés par `Build`. Ce
  poste n'avait jamais été nommé : il ne pouvait pas l'être sous le chemin monolithique, où il se
  confondait avec les COMBINE. Sans conséquence — `Build` les retire déjà — mais il explique
  pourquoi le compte de sommets par forme tombe **sous** 4 × 8 491.
- **La palette ne se déduit pas du document, elle se mesure sur les régions produites.** Le critère
  « 38 » a été écrit à partir d'un compte de `fill`, alors que ce qui est peint est ce que l'import
  **rend** — remplissages **et** rubans de trait. Le test le formule désormais ainsi : la palette
  égale le nombre de couleurs distinctes portées par les groupes, et le compte de couleurs de
  remplissage (38) reste vérifié à part.
- **Aucun compte absolu du Tigre n'a été figé dans la suite K2.** Les oracles sont des **relations**
  — égalité sommet par sommet avec le maillage reconstruit, palette égale au nombre de couleurs des
  groupes, faces et sommets en baisse stricte par rapport au chemin monolithique mesuré dans le même
  test — et les valeurs absolues sont **imprimées**. Un nombre figé lierait la suite à la
  tessellation d'un compilateur ; c'est déjà le choix qu'avait fait K0.
- **`overlapPolicy::Subtract` est déclaré et refusé**, pas inerte : `import_svg_extruded` rend
  `nullptr` et écrit sur `stderr`. Un maillage non découpé rendu sous ce réglage passerait pour
  découpé.

### Prédictions K3 — renseignées le 2026-09-06

Instrument : `test/tu_cgmesh_svg_k3_subtract.cpp`, 7 tests, build Release MSVC.
Fichier : `test/data/svg/Ghostscript_Tiger.svg`, objet ajusté à 1.0, `flattenTol = 0.005`.

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K3 | oracle de volume signé exact sous `Subtract` sur deux carrés | `subtraction_makes_the_volume_match_the_union_of_the_shapes` | **volume 0,066666 = aire d'union 0,666666 × h 0,1**, aux deux réglages de palette ; sous `None` **0,088889** | 0 | — |
| K3 | aire de l'union des régions produites == **0,6271** | `tiger_regions_are_disjoint_and_lose_nothing` | **0,627062** en sortie, **0,627064** en entrée | 3e-6 | — |
| K3 | l'aire totale des régions produites passe de **2,0981 à 0,6271**, soit **−70,1 %** | idem | **2,09803 → 0,627063**, aire retirée **1,47097**, soit **−70,1 %** | 0 | — |
| K3 | **points de contour en HAUSSE** depuis 8 491, bande **1,2e4 – 2,5e4** | `tiger_volumetry_under_subtraction` | **15 213** (×1,79) | **dans la bande**, tiers bas | — |
| K3 | **sommets : indéterminé a priori**, bande large **2,5e4 – 9,0e4** | `Mesh::GetNVertices` | **60 580** (contre 36 988 en K1 et 33 824 en K2) | **dans la bande**, tiers haut | — |
| K3 | **triangles de capot en forte baisse** : ~1 par point au lieu de 4,15 | comptage des triangles à `z = zTop` | **13 629 pour 15 213 points, soit 0,896** | **atteint, mais l'attendu de départ était faux** | **2** — voir ci-dessous |
| K3 | formes intégralement recouvertes : **strictement positif** | `SvgOverlapStats::fullyCoveredGroups` | **2 sur 239** | strictement positif, mais **bien plus bas que ne le suggérait « 197 formes découpées »** | **1** — voir ci-dessous |

### Ce que K3 a appris, hors prédiction

- **Le chiffre de 4,15 triangles de capot par point de contour (§17.2) était une
  ESTIMATION, pas une mesure, et il est faux.** Il venait d'un partage supposé
  entre capots et parois (« parois ≈ 2 par arête ⇒ 18 494, donc capots ≈ 38 394 »).
  Le comptage direct des triangles à `z = zTop` donne, sur le maillage monolithique
  de K1 : **22 987 triangles de capot supérieur** et **10 914 triangles de paroi**
  (22 987 × 2 + 10 914 = 56 888, exact). Le rapport réel est donc **2,71** et non
  4,15. **[M]**
  Conséquence sur la prédiction : la descente vers ~1 triangle par point **était
  déjà acquise en K2** (0,939), et K3 la mène à **0,896**. Ce n'est pas K3 qui
  produit ce gain, c'est la tessellation par forme. La prédiction est vérifiée, sa
  justification ne l'est pas.
- **Le chemin monolithique perd des parois, et la marqueterie aggrave le
  phénomène au point de le rendre rédhibitoire.** `ExtrudedMeshBuilder` construit
  chaque paroi en retrouvant l'arête de contour parmi les arêtes des triangles de
  capot ; quand elle n'y est pas, la paroi est **abandonnée sans un mot**
  (`extrude_contours.cpp`, « tessellation gap »). Or la marqueterie rend les
  régions **adjacentes** : dans un polygone glutess unique, leurs frontières
  communes cessent d'être des arêtes de capot. Mesure sur le Tigre : **10 914
  triangles de paroi** sur le monolithique de K1 pour 8 491 points de contour
  (il en faudrait ~16 982) — le défaut **préexiste** ; et **1 400** seulement
  quand on donne les régions découpées à un polygone unique. Le solide n'est
  alors plus fermé : volume mesuré **0,044444** au lieu de 0,066666 sur les deux
  carrés. **[M]**
  **Décision prise, et écart au §11.6** : `Subtract` impose **une tessellation
  par région**, que `perShapeMaterials` soit demandé ou non. Les deux options
  restent indépendantes du point de vue de l'utilisateur — l'une gouverne la
  couleur, l'autre la géométrie — mais elles ne sont plus indépendantes dans
  l'implémentation. C'est exactement la condition que `contour_ops.h:104` posait :
  « une soustraction qui alimente une tessellation **INDÉPENDANTE** n'a pas ce
  problème » — encore faut-il la lui donner.
- **L'oracle de volume signé, pris seul, ne discrimine pas la marqueterie.** La
  relation `volume == aire du capot × hauteur` teste l'**étanchéité** et rien
  d'autre : elle reste vraie pour deux prismes fermés qui s'interpénètrent, dont
  les volumes et les capots s'additionnent également. Vérifié : sous `None` avec
  palette, `volume = 0,088889` et `capot × h = 0,088889` — la relation tient alors
  que la matière est comptée deux fois. Ce qui discrimine est la comparaison à une
  mesure **indépendante du maillage** : l'aire de la réunion des formes d'entrée,
  calculée par Clipper2. **[M]**
  Le §12 disait « cet oracle est **faux sous K2** » : c'est inexact. Il était faux
  sous **M2b'** (capots décalés en Z), famille écartée par D1. Sous K2 il est vrai
  et muet. Le test `the_volume_oracle_is_seen_to_fail_without_subtraction` fige les
  deux mesures pour que la distinction ne se reperde pas.
- **Seulement 2 groupes sur 239 sont intégralement recouverts.** La cohérence
  attendue avec « 197 formes coupent une forme supérieure » n'existe pas :
  l'intersection de boîtes englobantes majore très largement le recouvrement
  **total**. Un dessin peintre superpose ses formes en les débordant, il ne les
  empile pas exactement. Le compte est publié, non nul, et c'est tout ce que le
  critère demandait — mais l'attente implicite « beaucoup de formes vont
  disparaître » est **infirmée**. Conséquence pour K5 : la statistique
  `hiddenShapes` sera un petit nombre, elle ne justifie pas d'interface dédiée.
- **La moitié `Difference` n'est PAS à parité avec la moitié `Union`, et c'est
  l'inverse de ce que §18 supposait.** Balayage complet mesuré : **219 à 232 ms
  natif Release** (239 `Difference` + 239 `Union` + le calcul des aires), contre
  **29 à 46 ms** pour les 239 `Union` seules mesurées par K0. L'extrapolation
  « ~100 ms natif pour les deux moitiés » est donc **infirmée d'un facteur ~2,2**.
  **[M]** K3 ne tire aucune conclusion de ce chiffre : c'est une entrée pour K4,
  qui reste le jalon décisionnel, et le repli n°1 reste délibérément non écrit.
- **Robustesse Clipper2 : aucun incident (R3, versant robustesse).** 239
  soustractions accumulées sur le Tigre, aucun contour dégénéré, aucune orientation
  à corriger, aucune région trouée. Les seuls écarts observés sont de l'ordre de
  l'ULP de `float` — la région fait l'aller-retour `float → double` Clipper2
  `→ float` — soit **1,5e-8 sur une aire de 0,16** : un seuil absolu serré dans un
  test le prendrait pour une perte de matière. Les tolérances de comparaison sont
  donc **relatives**. **[M]**

### Prédictions K4 — décisionnelles — ⚠ RENSEIGNÉES en §27.6

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K4 | poste (b) Clipper2 : **~200–400 ms en WASM** sur le Tigre — **R3** | chronométrage navigateur | | | |
| K4 | poste (b) natif : **~100 ms** (Union mesurée 29–46 ms, `Difference` supposée a parité) | test chronométré | | | |
| K4 | recalcul complet inférieur a 300 ms sur le Tigre | `viewer.js:257`, `:350` | | | |
| K4 | `depth` coûte **le même prix** qu'un recalcul complet sous G2 — **R6** | deux chronométrages comparés | | | |

Liste fermée des causes : 1 affirmation sur l'existant non vérifiée · 2 instrument biaisé ·
3 hypothèse d'environnement non vérifiée · 4 structure conçue sans charge · 5 critère non falsifiable ·
6 excès de pessimisme · 7 mécanisme de vérification neutralisé par le code.

### Motif à surveiller

La cause **2 (instrument biaisé)** est relevée pour la **troisième** fois dans ce document, toujours
sur le même geste : **compter par `grep` ce qu'un parseur produira**. Ce n'est plus un accident, c'est
une **règle de méthode** :

> Un compte destiné à prédire le comportement d'un parseur se mesure **après parse**, jamais sur le
> texte source. Le `grep` sert à **cadrer un ordre de grandeur**, pas à fournir un attendu de test.

## 20. Conclusions antérieures que K0 invalide ou fragilise

Quatre points. Le **20.2** n'avait été relevé par personne et corrige une formulation reprise dans la
consigne de mise à jour elle-même.

### 20.1 La fourchette de recouvrement du §15 était fausse — et c'est instructif

Prédiction : ratio aire totale / aire union « entre 1,5 et 3 ». Mesuré : **3,346**. Le Tigre est
**plus empilé** que je ne l'avais supposé. Aucune décision n'en dépend — au contraire, cela
**renforce D1** (§18) — mais le mécanisme de l'erreur mérite d'être nommé : j'avais posé une
fourchette sur une **intuition de genre** (« un dessin peintre empile peu »), sans instrument. C'est
la cause 1 de la liste fermée, et elle est ici sans gravité **uniquement parce qu'elle allait dans le
sens qui conforte la décision déjà prise**. Le même geste, dans l'autre sens, aurait fait passer M2b'
pour viable.

### 20.2 ⚠ « M2a allègera le maillage » est **non démontré**, et probablement faux pour les sommets

**C'est la conclusion la plus importante de cette mise à jour, et elle contredit une affirmation que
j'ai moi-même écrite au §13 et que la consigne de révision reprend telle quelle** :

> §13, ligne R4 : « M2a la **réduit** (la matière recouverte n'est plus tessellée) : l'estimation
> devient un **majorant** ».
> Consigne du 2026-09-06 : « Recouvrement à 70 % ⇒ … **le maillage de K3 sera plus léger que les
> 36 988 sommets actuels**, pas plus lourd ».

**Ce raisonnement confond l'AIRE et la COMPLEXITÉ DE FRONTIÈRE.** Or dans `ExtrudedMeshBuilder`, le
nombre de sommets ne dépend **pas** de l'aire : il vaut **4 × (nombre de points de contour)**
(`extrude_contours.cpp:259-262`, vérifié exactement par K0 : 36 988 / 4 = 9 247). Et sous M2a, la
frontière **augmente** :

- la région gardée d'une forme découpée est bornée **en partie par sa propre frontière, en partie par
  celle de la forme qui la recouvre** ; la frontière du recouvrant est donc comptée **une fois pour
  lui-même et une fois par forme qu'il découpe** ;
- **197 formes sur 239 sont découpées** **[M]**. La duplication de frontière est donc massive, pas
  marginale.

Deux effets opposés, dont aucun n'est négligeable :

| Effet | Sens sur les sommets | Ampleur |
|---|---|---|
| Disparition de la matière recouverte (70,1 % de l'aire) | **baisse** | porte sur l'aire, donc surtout sur les **triangles de capot** |
| Duplication de frontière sur 197 découpes | **hausse** | porte sur les **points de contour**, donc directement sur les sommets |
| Formes intégralement recouvertes qui disparaissent | baisse | limité : 42 formes sur 239 ne coupent rien, les autres sont partiellement gardées |

**Conclusion honnête : le sens de variation des sommets sous M2a n'est pas prédictible depuis le taux
de recouvrement.** Ce qui est prédictible, et qui a été séparé en deux prédictions distinctes au §19 :

- **les triangles de capot baissent fortement** — les régions deviennent disjointes et simples, donc
  ~1 triangle par point au lieu des **4,15** mesurés (§17.2) ;
- **les points de contour montent** — bande annoncée 1,2e4 à 2,5e4 contre 8 491 ;
- **les sommets sont indéterminés** — bande large 2,5e4 à 9,0e4 assumée comme telle.

**Ce que cela change concrètement** : la ligne R4 du §13 (« M2a la réduit, l'estimation devient un
majorant ») est **retirée** ; R4 reste levée par la mesure directe (§17.2), mais **pas** par cet
argument-là. Et la charge utile WASM sous M2a n'est **pas** garantie inférieure à 1,57 Mo : elle doit
être **mesurée** en K3, pas déduite.

### 20.3 R5 n'est pas close : K0 a tourné hors du dépôt

`git status` au 2026-09-06 **[M]** :

```
?? test/data/svg/Ghostscript_Tiger.svg
?? test/tu_cgmesh_svg_k0_metrics.cpp
```

**La fixture ET le test sont non suivis.** Les 20/20 verts sont un résultat de **poste de travail**,
pas du dépôt : en l'état, un `ctest` sur une copie fraîche ne trouverait ni le test ni son fichier
d'entrée. Le prérequis bloquant énoncé au §12 (« il doit être ajouté ») **n'est pas encore satisfait**
— il l'est fonctionnellement, pas au sens du dépôt. À stager avec la livraison de K0 :
`test/data/svg/Ghostscript_Tiger.svg`, `test/tu_cgmesh_svg_k0_metrics.cpp`, et l'entrée
correspondante dans `test/CMakeLists.txt` si l'ajout de source y est explicite.

### 20.4 Ce que K0 **confirme** — à ne pas rouvrir

- La chaîne couleur vers three.js n'a pas été touchée et n'avait pas à l'être : **aucune** des 20
  vérifications ne la met en cause.
- L'estimation « 13 formes au trait » était exacte (§17.3) : **D4 est correctement dimensionné**.
- 38 couleurs distinctes : **D5 est confirmée sans réserve**, la déduplication ramène bien 226 formes
  peintes à 38 matériaux, soit un facteur **5,9** sur le nombre de draw calls et de clones de
  matériaux three.js (`viewer.js:83-120`).
- L'ordre de grandeur du §5.4 (« c'est tenable, du même ordre que le relief d'image ») est
  **confirmé** : 1,57 Mo de charge utile, 38 groupes.

### 20.5 Ce qui n'a toujours pas été exécuté

Pour que la frontière reste nette : **K0 a mesuré le natif, jamais le navigateur.** Rien de ce
document n'a encore été observé dans `maker` — ni le rendu, ni le temps de recalcul, ni le
comportement de la bascule. `maker/build.ps1` n'a pas été exécuté, emsdk n'a pas été vérifié.
**Tout chiffre WASM de ce document reste [E].** C'est précisément l'objet de K4.

---
---

# PARTIE 4 — D7 : le trait des formes remplies (2026-09-06)

**Contexte.** K3 est fait et vert (`ctest` 1686/1686). Ses résultats et ses trois corrections
— oracle de volume non discriminant, ratio 4,15 faux (2,71 au comptage direct), COMBINE 740 mesuré
après `Build` au lieu de 3 551 avant élagage — **sont déjà consignés** par le sous-agent `developer`
aux §17.2, §19 et §20 ; **ils ne sont pas dupliqués ici**. Le défaut préexistant des parois perdues
(36 % sur le Tigre) est versé à `debt_cgmesh.md:4986-5018`. **[V]**

## 21. Décision D7 — 2026-09-06

| # | Décision | Motif |
|---|---|---|
| **D7** | **Le trait d'une forme FERMÉE ET REMPLIE doit être produit.** Aujourd'hui ignoré (`import_svg.cpp:433-437` : un chemin fermé avec `fill` part dans `filled`, son `stroke` n'est jamais consulté) **[V]**, ce qui perd le trait de **65 formes sur 239** du Tigre **[M]** | Contrainte utilisateur : « il n'est pas permis de perdre des informations du SVG ». C'est la perte **la plus massive** de l'inventaire, et elle est **préexistante** — ce chantier ne l'a pas introduite |
| **D7-a** | **Restent ÉCARTÉS** : opacité, dégradés, formes intégralement recouvertes (les 2 groupes que M2a supprime). Statu quo | Décision utilisateur explicite |
| **D7-b** | **M2a est CONFIRMÉE.** La matière recouverte reste retirée | L'utilisateur assume que la matière cachée sous une autre forme n'est **pas** une information du dessin. D1 tient |

**Portée exacte, mesurée.** 239 formes ; 226 avec `fill` ; 78 avec `stroke` ; **65 avec les deux** ;
13 au trait seul. **[M, §16]** D7 récupère les **65** — les 13 sont déjà traitées par
`strokeToVolume`.

## 22. Conception de D7 — les six questions tranchées

### 22.1 Géométrie du trait sur contour fermé — `EndType::Joined`, pas la différence de deux décalages

**Les deux voies ont été lues.**

| Voie | Ce que dit le code | Verdict |
|---|---|---|
| **(A) `strokeToContours` étendu à `EndType::Joined`** | `strokeToContours` appelle `InflatePaths` avec `EndType::Butt/Square/Round` (`stroke_contours.cpp:33-52`), qui sont les types de **chemin ouvert**. Clipper2 expose `EndType::Joined` (`extern/clipper2/clipper.offset.h:23`) et son traitement dédié `OffsetOpenJoined` (`clipper.offset.cpp:380`), qui décale une **boucle fermée des deux côtés** — c'est exactement l'anneau cherché **[V]** | **RETENUE** |
| **(B) différence de deux `offsetContours`** | `offsetContours` utilise `EndType::Polygon` et le documente : « les EndType de `stroke_contours` … rendraient ici un ruban le long du bord au lieu d'une région » (`contour_ops.cpp`, corps de `offsetContours`) **[V]** | **écartée** |

**Pourquoi (A).**

1. **Une seule passe Clipper2** au lieu de trois (deux `InflatePaths` plus un `Difference`).
2. **Les trous sont traités correctement et gratuitement** : `InflatePaths` décale **chaque chemin
   indépendamment**, donc le contour extérieur et chacun des trous reçoivent leur propre anneau —
   ce que fait le SVG, qui trace **chaque sous-chemin**. L'orientation n'intervient pas : `Joined`
   décale des deux côtés, il n'y a **rien à réorienter**, contrairement à (B) où un delta négatif sur
   une région exige la convention extérieur/trou en sens opposés.
3. **(B) a un mode de panne propre** : un décalage négatif plus grand que la demi-épaisseur locale
   **annihile** les parties fines (documenté pour `shrink`, `image_relief.h:124-141`). Sur les
   moustaches du Tigre, `offset(-w/2)` peut disparaître, et la différence rend alors la forme
   **entière** comme trait. Ce n'est pas faux au sens SVG — un trait plus large que sa forme la
   recouvre — mais c'est obtenu par accident plutôt que par construction, et cela coûte trois passes.
4. **(A) préserve `stroke-linejoin` des deux côtés** ; (B) le perd sur le bord intérieur, où le
   décalage négatif applique le joint à une géométrie inversée.

**Coût de (A)** : une surcharge additive de `strokeToContours`, du type
`strokeClosedToContours(contours, width, join)` ou un paramètre `bool closed`. Aucun autre appelant
n'est touché — `stroke_contours.h:16-19` sert aussi les L-systèmes, qui gardent la forme ouverte.

**Point non vérifié, à lever à l'implémentation [H]** : `OffsetOpenJoined` referme-t-il lui-même la
boucle, ou attend-il que le premier point soit répété en fin de chemin ? Les `ExtrudeContour` du
dépôt ont l'arête de fermeture **implicite** (`extrude_contours.h:26-27`). À vérifier sur un carré
avant d'industrialiser : un anneau qui « oublie » son dernier côté est un défaut discret.

### 22.2 Rang et ordre — **rien à changer, c'est déjà correct** [V]

En SVG, le trait d'une forme est peint **après** son remplissage ; sous M2a, le groupe « trait » doit
donc découper le groupe « remplissage » de la même forme. Vérification dans le code livré :

- **Ordre de poussée** : le lot de remplissage est poussé **avant** celui du trait
  (`import_svg.cpp:453-460` puis `:462-492`), et le code le dit lui-même : « Le lot de REMPLISSAGE
  est pousse avant celui du trait : c'est l'ordre dans lequel les regions d'une meme forme sont
  rendues » (`import_svg.cpp:452-453`). **[V]**
- **Sens du balayage** : `svg_subtract_overlaps` parcourt `for (size_t k = groups.size(); k-- > 0; )`
  — du **dernier au premier**. **[V]** Le groupe « trait », d'indice supérieur, entre donc dans
  `covered` **avant** que le groupe « remplissage » ne soit traité : le trait découpe son propre
  remplissage. **C'est exactement la sémantique SVG.**
- **Le `rank` n'intervient pas** dans le balayage, et c'est heureux : `import_svg.h:184-188` déclare
  déjà que « les DEUX groupes issus d'une même forme … partagent le même rang » et que « ce qui est
  garanti est l'ordre, pas la contiguïté ». **[V]**

> **Conclusion : D7 n'exige aucune modification de l'ordonnancement.** Le nouveau groupe « anneau »
> se pousse là où le groupe « ruban » l'est déjà, et tout le reste suit.
>
> **Une réserve de conception, à ne pas laisser dériver** : `rank` est un index de **forme**, pas de
> **groupe**. Tant que rien ne trie par `rank`, l'ambiguïté est inerte. Le jour où un traitement
> l'utiliserait pour ordonner, deux groupes ex æquo casseraient la sémantique **en silence**.
> **Recommandation** : à la première fonctionnalité qui trierait par rang, scinder en `rank` (index
> de groupe, strictement croissant) et `shapeIndex`. Pas avant : ce serait payer d'avance.

### 22.3 Portée de D4 — la conclusion du §11.4 **tombe**, mais le défaut se **renforce**

**Ce qui tombe.** §11.4 concluait : « déterminant pour un dessin au trait, **marginal pour le
Tigre** » — 13 formes, 5,4 %. Sous D7, `strokeUsesStrokeColor` gouverne **78 formes sur 239, soit
32,6 %**. **La conclusion « marginal » est invalidée.** **[M]**

**Ce qui ne tombe pas : le défaut `true`.** Il est même **renforcé**, et l'argument du §11.4 se
transpose en plus fort :

- avec `true`, les 65 nouveaux anneaux sont noirs (69 des 78 traits du Tigre sont `#000`
  **[M-grep]**) et découpent leurs propres remplissages : on obtient **le dessin au trait du
  Tigre**, c'est-à-dire ce que le document représente ;
- avec `false`, l'anneau prend la couleur du remplissage de sa forme, **fusionne avec lui au
  dédoublonnage RGBA** (D5), et le résultat est un remplissage **fragmenté en anneau plus intérieur
  portant le même matériau**. On paie intégralement la découpe M2a, on ne voit rien, et on alourdit
  le maillage. `false` est le réglage **le plus coûteux et le moins visible** des deux.

**Correction à porter au §11.4** : remplacer « marginal pour le Tigre » par « **gouverne 32,6 % des
formes du Tigre sous D7** ». Le reste du §11.4 — nom, convention, réversibilité — est inchangé.

### 22.4 ⚠ Volumétrie et coût — **D7 remet en cause D6 avant même que K4 ne tourne**

Base mesurée par K3 **[M]** : 239 groupes, points 8 491 → 15 213, sommets 60 580, charge 2,14 Mo,
balayage **219–232 ms natif Release**.

| Grandeur | K3 (mesuré) | Sous D7 (**estimé [E]**) | Facteur |
|---|---|---|---|
| Groupes soumis à M2a | 239 | **304** (+65 anneaux) | ×1,27 |
| Soustractions | 239 | **304** | ×1,27 |
| Points **avant** soustraction | 8 491 | **~14 000** (un anneau vaut environ 2 fois son contour source, plus les points de joint `Round`) | ×1,65 |
| Points **après** soustraction | 15 213 | **26 000 – 31 000** | ×1,7 – 2,0 |
| Sommets | 60 580 | **103 000 – 121 000** | ×1,7 – 2,0 |
| Charge utile | 2,14 Mo | **3,6 – 4,3 Mo** | ×1,7 – 2,0 |
| Matériaux | 38 | **~41** (`#4c0000`, `#a5264c`, `#a51926` ; `#000` est déjà dans la palette comme remplissage) | +3 |
| **Balayage M2a** | **219 – 232 ms** | **~480 – 520 ms natif** — le coût est en somme des tailles de l'accumulateur : ×1,27 sur le nombre de passes, ×~1,7 sur sa complexité | **×2,2** |
| Anneaux (`InflatePaths`, 65 appels mono-contour) | — | **< 20 ms** | négligeable |

**Extrapolation WASM** (facteur 2 à 4, cf. §18) : **1,0 à 2,0 s**. **[E]**

> **Conséquence sur D6, à dire avant K4 et non après.**
>
> La règle validée était : « moins de ~300 ms, on garde G2 nu ; au-delà de ~1 s, on active le mémo ».
> **K3 seul plaçait déjà le pipeline à 219–232 ms natif**, soit ~0,45 à 0,9 s en WASM — déjà hors du
> confort. **D7 le pousse à 1,0–2,0 s**, c'est-à-dire **au-delà du seuil haut**.
>
> 1. **Le mémo (option A) ne suffira pas**, et §11.7 dit pourquoi : il protège `depth` et la palette,
>    **jamais** `flattenTol`, `strokeScale`, `fitSize`, `centerAndFit`, `invertY` — soit la majorité
>    des curseurs de la page.
> 2. **Le repli n°1 (indexation spatiale) passe de « documenté, non écrit » à « probablement requis
>    dans le MVP ».** Point vérifiable et favorable : son facteur ~10 **survit à D7**. Un anneau a la
>    **même boîte englobante** que sa forme source, donc la densité de paires sécantes est inchangée :
>    2 912 sur 28 680 (10,2 %) devient ~4 710 sur 46 056, soit **10,2 %**. **[E bâti sur M]** Appliqué
>    aux ~500 ms, le balayage retomberait à **~50 ms natif**, soit 0,1–0,2 s en WASM — **sous le
>    seuil**.
>
> **Recommandation** : ne pas attendre K4 pour instruire le repli n°1. Le **mesurer dans K3b**, sur
> le pipeline complet D7, et le livrer si l'écart le confirme. C'est un **infléchissement de D6**,
> pas son abandon : la règle « mesurer d'abord » est respectée, c'est le jalon de mesure qui avance.

### 22.5 Effets de bord — deux sites, l'un inerte, l'autre réel

**Site 1 — conversion de `flattenTol` : INERTE.** `controlHullLargestExtent`
(`import_svg.cpp:115`) mesure l'emprise sur les **points de contrôle** et ignore l'épaisseur des
traits. Son en-tête l'avait déjà prévu et borné : « L'ecart residuel (chemins trop courts pour etre
retenus, **epaisseur ajoutee par strokeToContours**) ne joue QUE sur la densite de tessellation,
jamais sur la position d'un sommet » (`import_svg.cpp:108-111`). **[V]** D7 élargit cet écart mais ne
change **rien** à sa nature. **Aucun test n'en dépend.**

**Site 2 — recentrage-ajustement : RÉEL, et il déplace des sommets.** `recenterAndFit`
(`import_svg.cpp:245`) s'applique à **tous** les contours aplatis, rubans compris
(`import_svg.cpp:506-511`). Les 65 anneaux débordent de leurs formes de `w/2` vers l'extérieur ; là
où un anneau touche la silhouette extérieure du dessin, l'emprise croît, donc **l'échelle globale
diminue et tous les sommets se déplacent**.

Ordre de grandeur **[E]**, méthode nommée : le `transform` racine est une **similitude uniforme**
(`matrix(1.7656463,0,0,1.7656463,…)`, **[M-grep]**), donc le rapport largeur-de-trait sur emprise est
**invariant** ; l'emprise brute, mesurée sur les points de départ des 240 sous-chemins, vaut **431
unités** (`awk` sur les attributs `d`, **[M-grep]** — et c'est un **minorant** de la vraie boîte, donc
l'effet réel est plus petit encore). La plus large demi-épaisseur vaut `2 / 2 = 1` unité, soit
**0,23 %** de l'emprise par côté, **au plus 0,46 %** au total.

> **Conséquence sur les tests** : le déplacement est petit mais **non nul**, donc **la comparaison
> point par point de K1 échouerait** si D7 était actif par défaut, et les aires figées par K0 et K3
> (2,0981 et 0,6271) bougeraient. **C'est l'argument décisif du drapeau (§22.6).** Avec le drapeau à
> `false`, **aucun** test existant ne bouge : c'est vérifiable, et c'est le premier critère de K3b.

### 22.6 Paramètre d'activation — **oui : `strokeOnFilledShapes`, défaut C++ `false`**

Même discipline que `perShapeMaterials` et `overlapPolicy`, et pour la même raison, désormais
**chiffrée** par §22.5 : sans drapeau, la non-régression de K0, K1, K2 et K3 est perdue.

```cpp
// SvgExtrudeOptions — champ AJOUTÉ
// Trait des formes FERMÉES ET REMPLIES, produit en ANNEAU (EndType::Joined).
// Défaut false : le comportement actuel est préservé bit pour bit, et les
// oracles de K0 à K3 restent valides sans être requalifiés.
bool strokeOnFilledShapes = false;
```

**Défaut du NŒUD et du gabarit : `true`.** Même dédoublement qu'au §11.6 — le C++ par défaut ne
régresse pas, le produit par défaut respecte la contrainte « ne rien perdre du SVG ». Libellé
proposé : **« Tracer les contours »**, groupe « Traits », aux côtés de `strokeToVolume` et
`strokeScale`.

**Interaction à documenter, pas à interdire** : avec `useSvgColors` à `false`, un anneau prend la
couleur de son remplissage et devient **invisible** — il ajoute de la matière et du coût sans effet
visible. Ce n'est pas une raison de l'interdire (un anneau reste une géométrie gravable), mais
l'infobulle doit le dire. **Ne pas le griser** : un réglage grisé sans explication déroute plus que
le coût qu'il évite.

### 22.7 ⚠ Découverte non demandée — sur le Tigre, les anneaux sont **plus fins que la tolérance d'aplatissement**

Ce point n'était dans aucune des six questions ; il conditionne pourtant l'utilité de D7 sur ce
fichier, et il doit être dit avant l'implémentation.

Largeurs de trait déclarées, **[M-grep]** : `0.1` (34 déclarations), `0.172` (12), `0.5` (16), `2` (9)
— 71 déclarations pour 78 formes à trait, les 7 restantes héritant d'un ancêtre. Le rapport
largeur/emprise étant invariant sous la similitude racine (§22.5), et l'emprise brute valant **431
unités** :

| `stroke-width` brut | Largeur en unités objet (objet ajusté à 1,0) | Rapport à `flattenTol` = 5,0e-3 | Sur une pièce de 100 mm |
|---|---|---|---|
| 0,1 (34 décl.) | **2,3e-4** | **21 fois plus fin** | **0,023 mm** |
| 0,172 (12 décl.) | 4,0e-4 | 12 fois plus fin | 0,040 mm |
| 0,5 (16 décl.) | 1,2e-3 | 4 fois plus fin | 0,116 mm |
| 2 (9 décl.) | 4,6e-3 | comparable | 0,46 mm |

**Deux conséquences, l'une géométrique, l'autre industrielle.**

1. **Géométrique** : pour 46 des 71 déclarations, l'anneau est **plus fin que le pas
   d'échantillonnage des courbes qu'il borde**. On produit une bande dont la largeur est un ordre de
   grandeur sous la résolution du contour lui-même. Ce n'est pas faux, c'est **sous la résolution du
   modèle** — et cela explique une bonne part du coût estimé au §22.4 : beaucoup de points pour une
   matière que rien ne peut représenter fidèlement.
2. **Industrielle, et elle touche D1** : à `fitSize = 100 mm`, 62 des 71 déclarations donnent des
   anneaux de **0,02 à 0,12 mm**, très en dessous d'une buse d'impression courante (0,4 mm). D1 veut
   une **pièce imprimable en éléments rapportés** ; D7 y ajoute des dizaines d'éléments **non
   imprimables**, qui de surcroît **découpent** les éléments qui, eux, le sont.

**Ce que je recommande, et qui ne coûte presque rien** :

- le levier existe déjà : **`strokeScale`** (`import_svg.h:79`), pensé pour cela — son en-tête dit
  « Les traits sont souvent tres fins par rapport au dessin (0.2 sur un canevas de 250) … ce facteur
  permet de les grossir sans toucher au fichier ». **[V]** Le gabarit l'expose déjà
  (`svg.json:59-69`). Il faut simplement **documenter la valeur utile** pour ce genre de fichier ;
- ajouter une **largeur minimale en unités monde** (`minStrokeWorldWidth`, 0 = désactivée) qui
  **élargit** un anneau trop fin plutôt que de le laisser sous-résolu. Une **statistique publiée**
  du nombre d'anneaux relevés rend l'écart visible ;
- **ne pas** supprimer silencieusement les traits fins : ce serait reperdre l'information que D7
  existe pour récupérer.

**À mesurer en K3b, c'est une ligne** : min, médiane et max de
`shape->strokeWidth / controlHullLargestExtent`. Le tableau ci-dessus est **[E]** ; cette mesure le
rend **[M]**.

## 23. Jalon K3b — le trait des formes remplies (inséré entre K3 et K4) — ✅ FAIT le 2026-09-06 (résultats §26)

**Pourquoi `K3b` et non une renumérotation.** K0 à K3 sont **faits, verts et cités** dans les tests,
le journal et `debt_cgmesh.md`. Renuméroter K4 en K5 rendrait fausses toutes ces références. `K3b`
insère sans rien invalider ; c'est le coût le plus bas pour la même clarté.

**Pourquoi avant K4, et non après.** K4 doit chronométrer **le pipeline réel**. Les 65 anneaux
changent le coût de façon **matérielle** (×2,2 estimé, §22.4) : mesurer avant D7 produirait un chiffre
exact décrivant une configuration qui n'est pas celle qu'on livre — le pire des résultats, parce
qu'on le croirait.

### Ce que K3b construit

1. `SvgExtrudeOptions::strokeOnFilledShapes`, **défaut `false`** (§22.6).
2. La variante **fermée** de `strokeToContours` sur `EndType::Joined` (§22.1).
3. Dans la boucle par forme (`import_svg.cpp:429-451`), un troisième lot `strokeRings` : un chemin
   **fermé avec `fill`** alimente `filled` **et**, si `strokeOnFilledShapes && hasStroke`,
   `strokeRings`. Le groupe « anneau » est poussé **après** le groupe de remplissage, à l'endroit où
   le groupe « ruban » l'est déjà (§22.2) — donc **aucun changement d'ordonnancement**.
4. Les **mesures de largeur de trait** de §22.7 (min, médiane, max de `strokeWidth / emprise`).
5. Le **chronométrage à trois postes** du balayage sous D7, et la **mesure du repli n°1** (densité de
   paires sécantes sous D7), pour instruire §22.4 avant K4.

### Critères de vérification — falsifiables

1. **Non-régression, drapeau à `false`** : les tests K0, K1, K2, K3 passent **sans modification
   d'aucun attendu** — en particulier la comparaison **point par point** de K1 et les aires figées
   2,0981 / 0,6271. Un seul chiffre qui bouge **infirme** l'analyse du §22.5.
2. **L'anneau est un anneau** : sur un carré plein de côté 1 avec `stroke-width` 0,1, la région
   « trait » a une aire de **0,4 ± 1e-3** (périmètre 4 fois largeur 0,1) et **deux** contours — un
   extérieur, un intérieur. Une aire de 0,2 signalerait un décalage d'un seul côté ; **un** seul
   contour signalerait une boucle non refermée (le doute [H] du §22.1).
3. **Les trous sont tracés** : sur un anneau SVG (carré extérieur plus carré intérieur en `evenodd`),
   la région « trait » comporte **quatre** contours — deux par sous-chemin. Zéro pour le trou
   infirmerait le point 2 du §22.1.
4. **Le trait découpe son propre remplissage** : sous `Subtract`, l'aire du groupe de remplissage
   d'une forme tracée est **strictement inférieure** à son aire sans trait, et l'aire d'union des
   deux groupes **égale** l'aire de la forme dilatée de `w/2`. Ce critère est ce qui distingue
   « le trait est produit » de « le trait est produit **au bon rang** ».
5. **Récupération mesurée** : sur le Tigre, drapeau à `true`, le nombre de groupes passe de 239 à
   **304** — soit exactement **+65**. Un autre chiffre infirmerait le compte du §16.
6. **Aire d'union préservée** : sous `Subtract` et drapeau à `true`, l'aire d'union des régions
   produites égale celle des groupes d'entrée à la tolérance Clipper2 (3e-6, comme K3).
7. **Volumétrie publiée** : points, sommets, charge utile et durée du balayage, **consignés quels
   qu'ils soient** — c'est l'entrée de K4.

### Ce que K3b exclut nommément

- **Toute décision sur D6.** K3b **mesure** le coût sous D7 ; c'est K4 qui tranche. Écrire ici un
  critère « le recalcul reste sous 300 ms » serait un critère que la construction rend probablement
  impossible (§22.4), donc un critère à ne pas écrire.
- **L'écriture du repli n°1.** K3b en mesure l'assiette (densité de paires sécantes sous D7) ; il ne
  l'implémente pas. D6 dit « mesurer d'abord », et cela reste vrai même quand la mesure semble
  jouée d'avance.
- **La largeur minimale d'anneau** (`minStrokeWorldWidth`, §22.7). K3b **mesure** les largeurs ; le
  correctif est une décision utilisateur, pas une initiative d'implémentation.
- **Le rendu à l'écran.** Rien de ce jalon n'est observé dans le navigateur ; tout chiffre WASM
  reste **[E]** jusqu'à K4.
- **Opacité, dégradés, formes recouvertes** : statu quo, D7-a.

## 24. Incohérences introduites par D7 — liste franche

Cinq points. Le **24.1** est le seul qui appelle une décision utilisateur à court terme.

### 24.1 ⚠ D7 contre D6 — le seuil de 300 ms est vraisemblablement hors d'atteinte sans le repli n°1

Détaillé au §22.4. En résumé : K3 place déjà le balayage à 219–232 ms **natif** ; D7 l'estime à
~500 ms natif, soit **1,0–2,0 s en WASM** **[E]** — au-delà du seuil haut de D6. Et le mémo (option A)
**ne protège pas** les curseurs concernés (§11.7).

**Ce n'est pas un argument contre D7** : la contrainte « ne rien perdre du SVG » est un choix de
produit, le coût est un problème d'ingénierie qui a une solution dimensionnée (repli n°1, facteur
~10, densité de paires sécantes **inchangée** sous D7). C'est un argument pour **avancer** ce repli
du statut de « documenté » à celui de « instruit en K3b, livré si la mesure le confirme ».

**À trancher par l'utilisateur** : accepte-t-il que le repli n°1 (indexation spatiale de
l'accumulateur) entre dans le MVP si K3b confirme l'estimation ? La réponse par défaut, conforme à
D6, est **oui — après la mesure de K3b, pas avant**.

### 24.2 ⚠ D7 contre D1 — D7 ajoute des pièces que D1 veut imprimables et qui ne le sont pas

Détaillé au §22.7. D1 veut **une pièce imprimable en éléments rapportés** ; D7 ajoute, sur le Tigre,
**62 déclarations de trait sur 71** dont l'anneau mesure **0,02 à 0,12 mm** à `fitSize = 100 mm` —
sous la buse, et 12 à 21 fois plus fin que `flattenTol`.

Les deux décisions ne se contredisent pas sur le **principe** (le trait *est* une information du
document), mais elles divergent sur le **résultat** : la fidélité produit de la matière que
l'impression ne peut pas rendre. Ce n'est ni un bug ni une erreur de décision — c'est une propriété
du fichier, que ni D1 ni D7 n'avaient regardée.

**Levier existant, à documenter plutôt qu'à inventer** : `strokeScale` (`import_svg.h:79`), déjà
exposé par le gabarit (`svg.json:59-69`). **Option à instruire** : `minStrokeWorldWidth`, qui
**élargit** au lieu de supprimer — supprimer reperdrait ce que D7 récupère.

### 24.3 D7 contre D4 — la portée change d'un facteur 6, le défaut ne change pas

`strokeUsesStrokeColor` passe de **13 à 78 formes** (5,4 % à 32,6 %). §11.4 est corrigé (§22.3). Le
défaut `true` **résiste** à la réévaluation, et pour une raison plus forte qu'avant : sous M2a,
`false` paie la découpe **sans** la rendre visible, puisque l'anneau fusionne avec son remplissage au
dédoublonnage RGBA. **Aucune décision à reprendre**, seulement une phrase à corriger.

### 24.4 D7 contre D5 — sans conséquence, vérifié

Les trois couleurs de trait qui s'ajoutent (`#4c0000`, `#a5264c`, `#a51926`) portent la palette de
**38 à ~41** **[E]** ; `#000` y figure déjà comme couleur de remplissage, donc les 69 anneaux noirs
**ne créent aucun matériau**. Le dédoublonnage par RGBA absorbe D7 sans modification. Le facteur de
réduction des draw calls passe de 5,9 à **304 / 41 = 7,4**. **[E]**

### 24.5 D7 contre le drapeau `useSvgColors` — une combinaison inutile qu'il faut documenter

`strokeOnFilledShapes = true` avec `useSvgColors = false` produit des anneaux **invisibles** :
matière et coût, aucun effet. §22.6 recommande de **documenter** plutôt que de griser. Signalé parce
que c'est le genre de combinaison qu'un utilisateur atteint sans le vouloir, et dont il conclut que
le réglage « ne marche pas ».

## 25. Prédictions K3b — à verser au journal (§19)

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K3b | drapeau a `false` : **aucun** attendu de K0/K1/K2/K3 ne bouge | `ctest` | **1696/1696 verts**, aucun fichier de test antérieur touché ; chiffres identiques au dernier chiffre imprimé (2,09803 · 0,627064 · 0,627063 · 0,627062 · 1,47097 · 239 groupes · 8 491 → 15 213) | 0 | — |
| K3b | carré plein, `stroke-width` 0,1 : aire de la région trait == **0,4** a 1e-3, et **2** contours | test dédié | **0,400002**, **2** contours, emprise −0,05 → 1,05 | 2e-6 | — |
| K3b | anneau SVG (carré plus trou) : **4** contours de trait | test dédié | **4** contours, aire **0,279997** pour 0,28 attendu | 3e-6 | — |
| K3b | sous `Subtract`, le trait découpe son propre remplissage (aire du remplissage strictement réduite) | test dédié | remplissage **1,0 → 0,809999** ; réunion **1,21** == forme dilatée de w/2 (oracle `offsetContours`) | 0 | — |
| K3b | Tigre, drapeau a `true` : groupes **239 -> 304**, soit **+65** | compte de groupes | **239 → 304, +65**, égal au compte indépendant des formes visibles remplies ET tracées | 0 | — |
| K3b | aire d'union préservée a **3e-6** sous D7 | test K3 étendu | **0,630164 → 0,630162**, écart relatif **3,00e-6** | 0 | — |
| K3b | points après soustraction dans la bande **2,6e4 – 3,1e4** | `tiger_volumetry` sous D7 | **18 622** (×1,224 sur les 15 213 de K3) | **−28 % sous la borne basse** | **6** — excès de pessimisme |
| K3b | sommets dans la bande **1,03e5 – 1,21e5** | `Mesh::GetNVertices` | **74 296** (×1,226) | **−28 % sous la borne basse** | **6** |
| K3b | charge utile dans la bande **3,6 – 4,3 Mo** | calcul depuis sommets et faces | **2,63 Mo** (contre 2,14 Mo en K3) | **−27 % sous la borne basse** | **6** |
| K3b | matériaux : **41** | `Mesh::GetNMaterials` | **39, inchangé par D7** | **−2** | **1** — affirmation sur l'existant non vérifiée : les trois couleurs de trait supplémentaires sont portées par des formes **au trait seul**, donc déjà dans la palette depuis K2 ; les 65 anneaux nouveaux n'apportent aucune couleur neuve |
| K3b | balayage M2a **480 – 520 ms natif** (×2,2 sur les 219–232 ms de K3) | chronométrage natif | **269 ms** (×1,23 sur les 220 ms mesurés dans la même exécution) | **−44 % sous la borne basse** | **6** — R8 avait annoncé le sens du biais, pas son ampleur |
| K3b | densité de paires de bbox sécantes **inchangée a ~10,2 %** sous D7 | métrique de recouvrement | **10,21 % → 10,51 %** (2 905/28 441 → 4 839/46 056) | +0,3 point | — |
| K3b | largeur d'anneau : médiane **inférieure a `flattenTol`** sur le Tigre | `strokeWidth / emprise` | médiane **3,23e-4** contre `flattenTol` 5,0e-3 : **15 fois plus fin** | 0 | — |
| K3b | `OffsetOpenJoined` referme la boucle sans répétition du premier point — **[H] du §22.1** | test du carré | **oui, des deux côtés** : aire 0,4 sans répétition, **et inchangée** avec le premier point répété | 0 | — |

Liste fermée des causes : 1 affirmation sur l'existant non vérifiée · 2 instrument biaisé ·
3 hypothèse d'environnement non vérifiée · 4 structure conçue sans charge · 5 critère non falsifiable ·
6 excès de pessimisme · 7 mécanisme de vérification neutralisé par le code.

### Réserve nouvelle

| Réserve | Énoncé | Sens du biais | Levée |
|---|---|---|---|
| **R8** | Coût du balayage M2a sous D7 : **~500 ms natif estimé**, extrapolé de K3 par un facteur ×2,2 déduit du nombre de groupes et de la complexité de l'accumulateur | **majorant probable** : l'estimation suppose que les anneaux alourdissent l'accumulateur autant que des formes pleines, alors qu'un anneau a une aire faible et peut être absorbé plus vite par les unions | **LEVÉE par K3b** : **269 ms**, soit ×1,23 et non ×2,2. Le sens du biais était bon, son ampleur non |
| **R9** | Largeur des anneaux du Tigre : **12 à 21 fois sous `flattenTol`** pour 46 déclarations sur 71, calculé sur une emprise brute **minorée** (431 unités, points de départ seulement) | **minorant de l'emprise ⇒ majorant du rapport** : la vraie emprise étant plus grande, les anneaux sont **encore plus fins** que ce tableau ne le dit | **LEVÉE par K3b** : emprise réelle **940,03**, donc rapports **plus petits** que le tableau — 0,1 donne 1,88e-4 et non 2,3e-4. Le sens du biais est confirmé |

---

## 26. Résultats K3b — 2026-09-06

**Instrument** : `test/tu_cgmesh_svg_k3b_stroke_rings.cpp`, 10 tests, build Release MSVC, préset
`local-windows-paths`. Suite complète `ctest -C Release` : **1696/1696**, 0 échec.
Fichier de référence : `test/data/svg/Ghostscript_Tiger.svg`, objet ajusté à 1.0,
`flattenTol = 0.005`.

### 26.1 Ce que K3b a écrit

- `SvgExtrudeOptions::strokeOnFilledShapes`, défaut `false` (§22.6). **[V]**
- `strokeClosedToContours` (`stroke_contours.h`), surcharge **additive** sur `EndType::Joined`. Le
  corps commun aux deux familles est factorisé dans un `inflate` local ; `strokeToContours` est
  inchangée pour ses appelants. **[V]**
- Un troisième accumulateur `strokeRings` dans la boucle par forme (`import_svg.cpp`) : un chemin
  **fermé avec `fill`** alimente `filled` **et**, sous drapeau, `strokeRings`. Le lot d'anneau est
  poussé **après** le remplissage et **avant** le ruban — aucun changement d'ordonnancement,
  conforme au §22.2. **[V]**
- `SvgExtrudeOptions::minStrokeWorldWidth`, **0 = désactivé** (décision utilisateur du 2026-09-06).
  Unités du **maillage produit**, converties par `controlHullLargestExtent` comme `flattenTol` —
  même circularité, même solution. Elle **élargit**, elle ne supprime jamais, et elle s'applique aux
  **deux** familles de trait, la largeur étant lue au même endroit pour les deux. **[V]**

### 26.2 Le doute [H] du §22.1 est levé — `OffsetOpenJoined` referme la boucle

**Oui, et sans répétition du premier point.** `OffsetOpenJoined` (`clipper.offset.cpp:380`) appelle
deux fois `OffsetPolygon`, dont la boucle démarre à `k = path.size() - 1` : **l'arête de fermeture
est parcourue par construction**. Et `Group::Group` applique `StripDuplicates(p, is_closed = true)`
aux groupes `Joined` (`clipper.offset.cpp:143-148`, `clipper.core.h:662-668`), qui **retire** un
premier point répété. La convention est donc **exactement** celle d'`ExtrudeContour`. **[V]**

Vérifié des deux côtés : aire **0,4** sur un carré à 4 points, **0,4** sur le même carré à 5 points
dont le dernier répète le premier. Cas positif de l'instrument : les **mêmes** points passés à
`strokeToContours` (tracé ouvert) rendent **0,3** — exactement un côté sur quatre en moins. **[M]**

### 26.3 Volumétrie et coût sous D7 — le §22.4 est pessimiste d'un facteur ~1,8

| Grandeur | K3 mesuré | **D7 mesuré** | Facteur réel | §22.4 estimait |
|---|---|---|---|---|
| Groupes | 239 | **304** | ×1,27 | ×1,27 — juste |
| Points avant soustraction | 8 491 | **11 993** | ×1,41 | ×1,65 |
| Points après soustraction | 15 213 | **18 622** | ×1,22 | ×1,7 – 2,0 |
| Sommets | 60 580 | **74 296** | ×1,23 | ×1,7 – 2,0 |
| Faces | 57 370 | **70 756** | ×1,23 | — |
| Charge utile | 2,14 Mo | **2,63 Mo** | ×1,23 | 3,6 – 4,3 Mo |
| Matériaux | 39 | **39** | ×1,00 | ~41 |
| **(a)** parse + aplatissement | 2,7 ms | **5,2 ms** | ×1,93 | — |
| **(b)** marqueterie M2a | 219,7 ms | **269,4 ms** | **×1,23** | 480 – 520 ms |
| **(c)** tessellation + extrusion | 24,6 ms | **32,2 ms** | ×1,31 | — |
| **Total natif** | 247,0 ms | **306,8 ms** | ×1,24 | — |

**[M]**, les deux colonnes venant de la même exécution : elles sont comparables sans biais de
machine.

**Pourquoi l'estimation était trop haute** : elle supposait qu'un anneau alourdit l'accumulateur
autant qu'une forme pleine. Un anneau a la **même boîte** que sa forme source et une aire faible ;
l'union l'absorbe presque intégralement dès que la forme est elle-même dans l'accumulateur. C'est le
biais que R8 déclarait : son **sens** était bon, son **ampleur** non — ×1,23 et non ×2,2.

**Ce que cela donne à K4, sans le trancher** : le poste **(b) pèse 88 %** du total natif. Le §24.1
tenait le seuil de D6 pour hors d'atteinte ; l'écart réel est bien plus faible qu'annoncé, mais
306,8 ms natif reste **au-dessus** des 300 ms. **C'est K4 qui décide, pas K3b.**

### 26.4 Assiette du repli n°1 — le facteur ~10 survit à D7, mesuré

Instrument : boîtes englobantes des **groupes**, lues sur leurs contours résolus — et non sur
`NSVGshape::bounds` comme en K0. C'est la boîte qu'une indexation de l'accumulateur utiliserait. Le
contact simple compte comme intersection : le chiffre est un **majorant**.

| | Groupes | Paires sécantes | Possibles | Densité | Partenaires par groupe |
|---|---|---|---|---|---|
| Sans anneaux | 239 | 2 905 | 28 441 | **10,21 %** | 24,3 |
| **Sous D7** | 304 | 4 839 | 46 056 | **10,51 %** | **31,8** |

**La prédiction « densité inchangée » tient** (+0,3 point). Le facteur de réduction attendu passe de
239/24,3 = 9,8 à **304/31,8 = 9,6** : le repli n°1 reste dimensionné à **~10**. **[M]** K3b ne
l'écrit pas — D6 dit « mesurer d'abord », et c'est fait.

### 26.5 §22.7 mesuré — **cinq** largeurs effectives, et non quatre

Emprise sur les points de contrôle : **940,03** unités de document, contre les **431** du §22.5
(minorant : points de départ seulement). Les rapports réels sont donc **plus petits** que le tableau
estimé, exactement comme R9 l'annonçait.

| `stroke-width` déclaré | Rapport mesuré à l'emprise | En multiples de `flattenTol` | Sur une pièce de 100 mm |
|---|---|---|---|
| 0,1 | **1,878e-4** (min) | 0,038 × — **27 fois plus fin** | **0,019 mm** |
| 0,172 | **3,231e-4** (médiane) | 0,065 × — 15 fois plus fin | 0,032 mm |
| 0,5 | **9,391e-4** | 0,188 × | 0,094 mm |
| **1,0** ⚠ | **1,878e-3** | 0,376 × | 0,188 mm |
| 2,0 | **3,757e-3** (max) | 0,751 × | **0,376 mm** |

**Le tableau des quatre largeurs déclarées ne se confirme pas : il y en a CINQ.** Le §22.7 comptait
des **déclarations** (`grep`, 71 pour 78 formes tracées) ; les 7 formes sans `stroke-width` déclaré
prennent le **défaut nanosvg de 1.0** (`nanosvg.h:649`), qui forme une cinquième classe. C'est la
leçon de méthode du §17.1 reconduite : un comptage textuel voit ce qui est écrit, pas ce que le
parseur en fait.

**Ce que la mesure confirme, en plus fort** : à `fitSize = 100 mm`, la largeur **maximale** du
fichier vaut **0,376 mm**, sous une buse de 0,4 mm. Le §24.2 disait « 62 déclarations sur 71 » ; la
mesure dit **toutes**. Aucun trait du Tigre n'est imprimable tel quel.

### 26.6 Ce que le plancher change

`minStrokeWorldWidth` est vérifié sur deux carrés tracés à 0,02 et 0,20, plancher à 0,10 : l'anneau
fin passe de **0,08 à 0,40** d'aire — soit exactement la largeur du plancher — et l'anneau large
reste à **0,80**, inchangé à 1e-6 près. **[M]** Aucun groupe n'est créé ni supprimé.

Appliqué au Tigre, le tableau du §26.5 se lit directement : à **`minStrokeWorldWidth = 0,004`**
(0,4 mm sur une pièce de 100 mm, soit une buse), **les cinq classes de largeur passent sous le
plancher** et sont toutes élargies ; à 0,001, les trois plus fines le sont. Aucune statistique n'est
publiée — l'utilisateur a choisi le plancher seul.

### 26.7 Effet de bord vérifié — le §22.5 est confirmé

Sous D7, l'aire de l'union du Tigre passe de **0,627064 à 0,630164**, soit **+0,49 %** : les anneaux
débordent bien de la silhouette, et l'échelle globale change donc sous `centerAndFit`. C'est
précisément la raison du drapeau — et, drapeau à `false`, **aucun** chiffre de K0 à K3 ne bouge,
jusqu'au dernier chiffre imprimé.

---
---

# PARTIE 6 — Résultats du jalon K4 (2026-09-06)

**Statut du document.** K4 est le **jalon décisionnel** de D6. Il ne construit rien de fonctionnel :
il mesure, et il applique la règle de bascule **sans prendre la décision**, qui appartient à
l'utilisateur. Ordre de préséance : **partie 6 > partie 5 > partie 4 > partie 3 > partie 2 >
partie 1**.

## 27. Résultats K4 — 2026-09-06

### 27.1 Instruments, et ce qu'ils ne mesurent pas

| # | Instrument | Portée |
|---|---|---|
| **I1** | `test/tu_cgmesh_svg_k4_timing.cpp`, 4 tests, build Release MSVC, préset `local-windows-paths` | natif, 7 exécutions par poste, min / médiane / moyenne / écart type |
| **I2** | `maker/tools/k4_probe/k4_probe.cpp`, compilé par `maker/tools/k4_probe/build.ps1` contre la `libcgmesh.a` du préset `maker-wasm` (mêmes `-O3 -fexceptions`), page `maker/web/k4_probe.html` | WebAssembly, Chrome headless (V8), thread UI |
| **I3** | même artefact que I2, exécuté par le node de l'emsdk | corroboration : **425,9 ms** contre 427,6 ms au navigateur, soit **0,4 %** d'écart |

Suite complète : `ctest -C Release` → **1700/1700**, 0 échec (1696 + les 4 tests de K4).

**Pourquoi un harnais et non la page `svg.html`.** Le nœud `svg.extrude.colored` (G2) n'existe qu'en
K5 ; le gabarit passe aujourd'hui par `svg.contours → shape.extrude`, qui n'exerce **ni M2a ni D7**.
Chronométrer ce chemin aurait donné un chiffre exact décrivant une configuration qu'on ne livre pas.
Le harnais appelle **les mêmes fonctions de `cgmesh`, avec les mêmes options**, dans le même moteur.

**Biais déclarés de I2, tous dans le même sens — le chiffre est un MINORANT du délai perçu** :

- il mesure le **calcul** ; ni la conversion en charge utile JS, ni l'upload three.js, ni le rendu ;
- il n'exerce pas le graphe : ni signature, ni cache, ni `InlineEvalDriver` ;
- `performance.now()` est grossi à 100 µs par le navigateur, ce qui se lit dans les chiffres
  (`342,10`) et reste sans effet à cette échelle ;
- emsdk **est disponible** sur cette machine (`C:\home\dev\extern\emsdk`, activé) : l'issue « mesure
  navigateur impossible avant K5 » n'a pas eu à être retenue.

### 27.2 Chronométrage consolidé — natif et navigateur

Tigre, `flattenTol = 0,005`, `centerAndFit`, D7 actif (`strokeOnFilledShapes`), M2a
(`overlapPolicy = Subtract`), 7 exécutions. Médianes, en ms. **[M]**

| Poste | Natif (I1) | écart type | Navigateur (I2) | écart type | Facteur WASM / natif |
|---|---|---|---|---|---|
| **(a)** parse + aplatissement | **5,30** | 0,28 | **5,70** | 0,72 | ×1,08 |
| **(b)** marqueterie M2a | **264,56** | 1,89 | **399,20** | 10,25 | **×1,51** |
| **(c)** tessellation + extrusion | **41,13** | 7,53 | **22,10** | 1,05 | ×0,54 |
| **total (a+b+c)** | **312,55** | 6,85 | **427,60** | 10,79 | **×1,37** |
| part du poste (b) | **84,6 %** | | **93,4 %** | | |
| **(d)** `BuildPolygonRenderData` | 2,23 | 0,13 | 1,50 | 0,46 | ×0,67 |

Sans anneaux (état K3, pour comparaison) : **238,12 ms natif**, **363,10 ms navigateur**.

Deux lectures à ne pas confondre :

- **Le facteur WASM est de ~1,4, et non de 2 à 4.** L'extrapolation du §18 était **pessimiste d'un
  facteur ~2**. Le total mesuré reste néanmoins au-dessus du seuil : c'est le coût natif qui est
  élevé, pas la conversion.
- **Le poste (c) est plus rapide en WASM qu'en natif** (22,1 contre 41,1 ms). Ce n'est pas une
  supériorité de WASM : le poste (c) natif porte l'écart type le plus large de la campagne (7,53 ms,
  étendue 55,6 %) — c'est l'allocateur, pas glutess. Ce chiffre est le seul du tableau à ne pas être
  stable, et il ne fonde aucune conclusion.

Fichiers ordinaires, natif, totaux (a+b+c) : **rose.svg 3,24 ms**, **spiderman.svg 2,15 ms**. **[M]**
Le §18 demandait ces deux mesures ; elles disent que le problème est **le Tigre**, pas le pipeline.

### 27.3 R6 — le coût de `depth`, mesuré des deux côtés

Trois coûts comparés sur la même charge (D7 + M2a) : `depth` sous le **graphe étagé** d'aujourd'hui
(seul le poste (c) est rejoué, §11.2), `depth` sous **G2** (le nœud monolithique rejoue tout), et un
**recalcul complet** de référence provoqué par `flattenTol`, qu'aucun étagement ne protège (§11.7).

| Coût d'un mouvement de curseur | Natif | Navigateur |
|---|---|---|
| `depth`, graphe étagé (état actuel) | **41,43 ms** | **21,20 ms** |
| `depth`, G2 monolithique | **297,09 ms** | **418,10 ms** |
| `flattenTol` (recalcul complet, référence) | **293,63 ms** | **410,00 ms** |
| **rapport G2 / recalcul complet** | **1,01** | **1,02** |
| rapport G2 / étagé | ×7,2 | **×19,7** |
| surcoût absolu de la régression | +255,7 ms | **+396,9 ms** |

> **R6 est CONFIRMÉE, et la confirmation est exacte, pas approchée.** Sous G2, un changement de
> `depth` coûte **1,02 fois** un recalcul complet : il n'existe aucune économie résiduelle qu'une
> implémentation soigneuse aurait pu garder. Le curseur « Profondeur » passe de **21 ms à 418 ms** au
> navigateur — d'un mouvement gratuit à **quatre dixièmes de seconde de page figée par cran**, sur le
> thread UI (`template.js:10-17`).

### 27.4 La règle de bascule, appliquée — et non tranchée

| Mesure | Règle validée (§12 K4) |
|---|---|
| **427,6 ms au navigateur** (312,6 ms natif) | **> ~300 ms** ⇒ **consigner et remonter. N'activer rien d'office.** |

C'est fait : **rien n'a été activé**. Ni le mémo (option A), ni le repli n°1, ni aucun allègement de
la marqueterie. L'arbitrage appartient à l'utilisateur.

**Le poste (b) domine, et il domine plus au navigateur qu'en natif : 93,4 %.** La règle de §12 dit
alors explicitement quoi lire d'abord — le repli n°1, et non le mémo. Les chiffres qui le fondent :

| | Ce que ça règle | Ce que ça ne règle pas | Coût après, au navigateur |
|---|---|---|---|
| **Mémo interne (option A)** | `depth` et la palette : R6 seule | `flattenTol`, `strokeScale`, `strokeWidthFallback`, `strokeToVolume`, `centerAndFit`, `invertY`, `fitSize` — **la majorité des curseurs de la page** (§11.7) | `depth` → ~21 ms ; **tous les autres curseurs restent à 427,6 ms** |
| **Repli n°1 (indexation spatiale)** | le poste (b), donc **tous** les curseurs à la fois | rien de connu : la différence est locale par construction (§13) | (b) 399,2 / 9,6 ≈ **41,6 ms**, total ≈ **69,4 ms** **[E bâti sur M]** — **sous le seuil**, et R6 devient sans objet |

Le facteur 9,6 n'est pas une supposition : il est **mesuré** par K3b sur les boîtes des groupes
résolus (§26.4) — 4 839 paires sécantes sur 46 056, densité 10,51 %, 31,8 partenaires par groupe.

> ⚠ **CORRIGÉ PAR K4b (§28.3).** Cette phrase est fausse dans son inférence, et l'erreur est
> instructive : la densité de paires est bien mesurée, mais **elle ne mesure pas un coût**. Le coût
> suit le nombre de POINTS du soustracteur, et l'accumulateur naïf — étant une réunion — est bien
> plus simple que la somme des formes qu'il contient. Facteur réel mesuré : **1,93 au navigateur**,
> total **244,3 ms** et non ~69,4.

**Recommandation, et non décision : le repli n°1 d'abord.** Le mémo laisserait sept curseurs sur huit
à 427 ms ; le repli n°1 ramène le pipeline entier sous le seuil et rend R6 **sans objet** — un
`depth` à ~70 ms n'a plus besoin d'être protégé par un cache. Le mémo ne redevient utile qu'**après**
le repli, et seulement pour descendre de ~70 ms à ~20 ms sur ce seul curseur.

### 27.5 Ce que K4 ne dit pas

- Il ne juge **pas** la géométrie. Un chronomètre ne dit rien de l'aspect du Tigre.
- Il ne mesure **pas** le délai perçu de bout en bout : le transport de la charge utile (2,63 Mo)
  vers three.js et l'upload GPU ne sont pas comptés. **427,6 ms est un plancher.**
- Il ne mesure **pas** le surcoût du graphe lui-même (signature, cache, pilote). Le point 5 du §9 —
  `eval_driver.h` et `editor_model.h` pris sur parole quant à l'invalidation du cache — **reste
  ouvert** : le harnais court-circuite le graphe, donc il ne pouvait pas le lever.

### 27.6 Journal de prédictions — K4 renseigné

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K4 | poste (b) Clipper2 : ~200–400 ms en WASM — **R3** | I2 | **399,2 ms** | juste, en **haut** de la fourchette | — |
| K4 | poste (b) natif ~100 ms | I1 | **264,6 ms** | ×2,6 | 6 — excès d'optimisme sur `Difference`, déjà corrigé par K3 |
| K4 | recalcul complet < 300 ms | I2 | **427,6 ms** | **infirmée** | 4 — structure conçue sans charge : la marqueterie naïve paie le dessin entier pour chaque forme |
| K4 | `depth` coûte le même prix qu'un recalcul complet sous G2 — **R6** | I1 et I2 | **rapport 1,02** | **confirmée** | — |
| K4 | facteur WASM / natif de 2 à 4 (§18) | I1 contre I2 | **×1,37** au total, **×1,51** sur (b) | **infirmée**, favorablement | 6 — excès de pessimisme |

### 27.7 Réserves après K4

| Réserve | État | Suite |
|---|---|---|
| **R3** — coût Clipper2 de M2a | **CLOSE.** Robustesse levée par K3, coût mesuré des deux côtés : 264,6 ms natif, 399,2 ms navigateur | il ne reste plus une mesure à faire, mais une décision à prendre |
| **R5** — fixture et tests hors dépôt | **TOUJOURS OUVERTE.** `Ghostscript_Tiger.svg` et les tests K0 / K3b / K4 sont non suivis par git **[M : `git status`]** | à stager |
| **R6** — perte de gratuité de `depth` sous G2 | **CONFIRMÉE, non levée.** ×19,7 au navigateur, +396,9 ms | sans objet si le repli n°1 est retenu ; sinon, arbitrage utilisateur |
| **R10** *(nouvelle)* — le délai perçu n'est pas mesuré | le harnais s'arrête au maillage : transport et rendu ne sont pas comptés | mesurable seulement après K5, sur `svg.html` |

⚠ Ce tableau est **remplacé par le §28.7** : K4b lève R6 par la règle de bascule et ouvre R11.

---
---

# PARTIE 7 — Résultats du jalon K4b (2026-09-06)

**Statut du document.** K4b applique la décision **D8** : le repli n°1 seul. Il ne change **aucune
géométrie** — c'est la propriété que sa vérification établit avant tout le reste. Ordre de
préséance : **partie 7 > partie 6 > partie 5 > partie 4 > partie 3 > partie 2 > partie 1**.

## 28. Résultats K4b — 2026-09-06

### 28.1 Ce que K4b a écrit, et la stratégie retenue

`svg_subtract_overlaps` (`import_svg.cpp`) n'entretient plus d'accumulateur. Pour chaque forme :

```
shape_i       = unionContours (shape_i, {})            // normalisation
partenaires_i = { j > i : boite_j ∩ boite_i ≠ vide }
region_i      = differenceContours (shape_i, contours des partenaires_i)
```

C'est la **stratégie (A)** — accumulateur local par forme — dans sa variante à **un seul appel
Clipper2 par forme**. Le choix entre les deux stratégies possibles n'a pas été fait par préférence :

- **(B), accumulateur global conservé + soustraction restreinte, est sans effet sur ce fichier.**
  `couvert` est le résultat d'une **union** : ses contours sont des blocs fusionnés, et sur un dessin
  connexe comme le Tigre l'enveloppe extérieure coupe la boîte de **toutes** les formes. Filtrer les
  contours de `couvert` par boîte n'écarterait presque rien. La densité de 10,51 % du §26.4 est
  mesurée sur les boîtes des **groupes**, pas sur celles de l'accumulateur : elle ne s'applique
  qu'à (A).
- **(A) sans normalisation préalable serait faux**, et c'est le piège. Clipper2 applique la règle de
  remplissage au clip **pris comme un tout** : la région soustraite est celle où la **somme** des
  nombres de tours des contours de clip est non nulle (`clipper.engine.cpp`,
  `IsContributingClosed`, cas `Difference` : `wind_cnt2 == 0` ⇒ hors du clip). Concaténer des formes
  quelconques laisse cette somme **s'annuler**.

**La démonstration**, en deux propositions séparées :

1. **Écarter les non-partenaires ne change rien.** Soit `p` un point de `shape_i`, donc de
   `boite_i`. Si `j` n'est pas partenaire, `boite_j ∩ boite_i = ∅`, donc `p ∉ boite_j ⊇ shape_j`.
   Les formes écartées ne contiennent **aucun** point de `shape_i` : les retirer du soustracteur ne
   déplace aucun point de la différence. L'unité de filtrage est la **forme**, jamais le contour —
   les trous voyagent avec leurs enveloppes, sans quoi on retirerait une enveloppe en gardant son
   trou.
2. **Concaténer les partenaires revient à soustraire leur réunion**, *à condition* que chaque forme
   soit normalisée. Un jeu de contours rendu par Clipper2 a un nombre de tours valant 0 hors de sa
   région et une même valeur non nulle dedans, **de signe fixé par la convention de la bibliothèque,
   donc identique d'une forme à l'autre**. Une somme de termes nuls ou de même signe ne s'annule que
   si tous sont nuls. La normalisation n'est pas une précaution : c'est **l'hypothèse de la
   démonstration**, et elle coûte **1,9 ms sur 110** (mesuré, §28.3).

Le piège exact décrit dans la consigne — un trou compté −1 annulé par un extérieur compté +1 — est
donc écarté par construction, et **figé par un test** :
`a_hole_above_does_not_pierce_the_region_below`.

### 28.2 Vérification — le détecteur, et le fait de l'avoir vu échouer

Instrument : `test/tu_cgmesh_svg_k4b_indexed_subtract.cpp`, 5 tests. L'algorithme **naïf de K3** y est
recopié mot pour mot comme référence figée ; les deux résolutions sont comparées **région par
région**, sur deux niveaux — aire de la différence symétrique, et coordonnées après **réalignement
cyclique** (un contour fermé n'a pas de premier sommet : comparer les tableaux bruts mesurerait le
sommet d'ouverture choisi par Clipper2, qui n'est pas de la géométrie).

| Fixture | Groupes | Structure (contours, points) | Différence symétrique | Écart de position max | Groupes identiques au bit |
|---|---|---|---|---|---|
| `Ghostscript_Tiger.svg` (D7) | 304 | **identique** | max **1,93e-7**, cumulée **1,53e-6** | **1,63e-5** | 238 / 304 |
| `rose.svg` | 19 | identique | **0** | **0** | 19 / 19 |
| `spiderman.svg` | 5 | identique | **0** | **0** | 5 / 5 |
| `batman.svg` | 1 | identique | **0** | **0** | 1 / 1 |
| `nazca.svg` | 1 | identique | **0** | **0** | 1 / 1 |
| 100 carrés tous sécants | 100 | identique | **0** | **0** | 100 / 100 |

**Le Tigre n'est pas identique au bit près, et il ne peut pas l'être.** Les deux algorithmes
n'enchaînent pas le même nombre de passes Clipper2 — le naïf soustrait un accumulateur déjà arrondi
303 fois, l'indexé soustrait des formes d'origine — et Clipper2 travaille à `precision = 6`, soit une
grille de 1e-6. L'écart observé, **1,63e-5 en position**, vaut **1/300 de `flattenTol`** (0,005), la
tolérance à laquelle la géométrie est elle-même échantillonnée. Le nombre de contours et le nombre de
points par contour, eux, sont **rigoureusement identiques sur les 304 groupes**.

**Le détecteur a été vu échouer**, discipline de K1 et K3. Perturbation : rétrécissement de chaque
boîte de l'index de 2 % autour de son centre, dans `svg_subtract_overlaps`. Deux suites passent au
rouge, dont une qui n'avait pas été écrite pour cela :

```
tu_cgmesh_svg_k4b_indexed_subtract.cpp(348): error: Expected equality of these values:
  d.pointCountMismatch   Which is: 17     0u   Which is: 0
(350): error: Expected: (d.maxSymDiffArea) < (kGroupAreaTol),
       actual: 0.00013174187915865332 vs 1e-06
(351): error: Expected: (d.sumSymDiffArea) < (kTotalAreaTol),
       actual: 0.00051551588467191323 vs 3e-06

tu_cgmesh_svg_k3_subtract.cpp(417): error: The difference between areaAfter and unionAfter is
  0.00049110522195405792, which exceeds 1e-4 * unionAfter
```

L'écart introduit est de **1,3e-4 sur une région**, soit **680 fois** le résidu d'arrondi mesuré
ci-dessus : les seuils séparent les deux phénomènes de près de trois ordres de grandeur, et ce n'est
plus une supposition. La perturbation a été retirée. Le second échec — critère 2 de K3, régions
disjointes — confirme au passage que **rater un partenaire n'est pas silencieux pour tout le monde**.

**Suite complète** : `ctest -C Release` → **1705/1705**, 0 échec (1700 + les 5 tests de K4b). Aucun
attendu de K0 à K4 n'a été modifié. Les valeurs figées de K3 sont inchangées jusqu'au dernier chiffre
imprimé : aires **2,09803 / 0,627064 / 0,627063 / 0,627062**, aire retirée **1,47097**,
`fullyCoveredGroups == 2`.

### 28.3 Chronométrage — et l'infirmation du facteur 9,6

Mêmes instruments qu'en K4 : I1 natif (`tu_cgmesh_svg_k4_timing.cpp`), I2 navigateur (Chrome
headless sur `maker/web/k4_probe.html`), I3 node de l'emsdk. 7 exécutions, médianes. **[M]**

| Poste | Natif avant | Natif après | Navigateur avant | Navigateur après |
|---|---|---|---|---|
| **(a)** parse + aplatissement | 5,31 | **5,58** | 5,70 | **6,20** |
| **(b)** marqueterie M2a | 268,17 | **122,61** | 399,20 | **215,30** |
| **(c)** tessellation + extrusion | 32,59 | **27,49** | 22,10 | **23,20** |
| **total (a+b+c)** | **307,40** | **160,87** | **427,60** | **244,30** |
| part du poste (b) | 87,2 % | **76,2 %** | 93,4 % | **88,1 %** |
| **(d)** `BuildPolygonRenderData` | 2,23 | 2,64 | 1,50 | 1,50 |

Le « avant » natif est **remesuré sur cette machine ce jour** (307,40) et non repris du §27.2
(312,55). **Biais à déclarer, et il est du côté natif** : les deux colonnes natives viennent
nécessairement de **deux campagnes** — un même binaire ne porte qu'un algorithme —, et la machine
était plus bruyante l'après-midi : le poste (c) a affiché jusqu'à **128 % d'étendue** sur sept
exécutions. Les chiffres natifs ci-dessus sont donc à ±15 %. **Les chiffres du navigateur ne
souffrent pas de ce biais** (étendue 15,9 %), et le tableau suivant encore moins : il chronomètre les
**deux algorithmes dans la même exécution**. Corroboration I3 (node) : **226,00 ms** contre 244,30 au
navigateur, soit **7,5 %** d'écart.

Le harnais mesure en outre les **deux algorithmes dans la même exécution**, ce qui débarrasse le
rapport de toute comparaison entre campagnes :

| Poste (b) seul, Tigre D7 | Natif | Navigateur |
|---|---|---|
| balayage naïf (état K4) | 272 ms | **424,70 ms** |
| balayage indexé (K4b) | 110 ms | **220,10 ms** |
| **facteur** | **2,48** | **1,93** |

> **Le facteur 9,6 est INFIRMÉ. Le facteur réel est de 1,9 à 2,5.** La cause est mesurée, pas
> supposée : **le coût ne suit pas le nombre de partenaires, il suit le nombre de POINTS de
> soustracteur** donnés à Clipper2. Sur le Tigre sous D7, **565 243** points dans le balayage naïf
> contre **272 231** dans l'indexé, soit un facteur **2,08** — qui explique le 2,48 mesuré presque
> exactement. Le §27.4 comptait des **partenaires** (304 / 31,8 = 9,6) ; il supposait implicitement
> que l'accumulateur naïf coûtait autant que la somme des formes qu'il contient. C'est faux :
> l'accumulateur est une **réunion**, elle fond les frontières intérieures, et sa complexité
> (1 859 points en moyenne) est très inférieure à celle des formes empilées (12 000 points au total).
> La leçon de méthode du §17.1 est reconduite une troisième fois : **un dénombrement d'objets ne
> mesure pas un coût**.

Le seuil est néanmoins franchi : **244,3 ms au navigateur, sous les ~300 ms** de la règle de bascule
de D6, et **tous** les curseurs en bénéficient — la différence est locale par construction, aucun
curseur n'est protégé au détriment d'un autre.

### 28.4 R6 après indexation — et le sort du mémo

| Coût d'un mouvement de curseur | Natif avant | Natif après | Navigateur avant | Navigateur après |
|---|---|---|---|---|
| `depth`, graphe étagé | 41,43 | 39,12 | 21,20 | **24,00** |
| `depth`, G2 monolithique | 297,09 | 167,04 | 418,10 | **231,30** |
| `flattenTol` (recalcul complet) | 293,63 | 157,77 | 410,00 | **232,30** |
| rapport G2 / recalcul complet | 1,01 | **1,06** | 1,02 | **1,00** |
| rapport G2 / étagé | ×7,2 | ×4,27 | ×19,7 | **×9,64** |

**R6 est LEVÉE par la règle, non par disparition du phénomène.** Le rapport « `depth` sous G2 = un
recalcul complet » reste **exact** (1,00) : l'indexation ne rend pas `depth` moins cher que le reste,
elle rend le reste moins cher. Ce qui change est le **chiffre absolu** — le curseur « Profondeur »
passe de 418 à **231 ms** —, et la règle de bascule de D6 dit alors « `< ~300 ms` ⇒ on en reste à G2
nu, §11.3 est sans conséquence, **R6 est levée** ».

**Le mémo (option A) n'a pas été écrit**, conformément à D8. La mesure ne le réclame pas : il ferait
passer ce seul curseur de 231 à ~24 ms, à l'intérieur d'un budget déjà sous le seuil, au prix d'un
cache indexé sur le contenu du fichier et les paramètres amont. **Si le besoin réapparaît** — une
page qui vise les 16 ms par image, ou un fichier plus lourd que le Tigre —, c'est là qu'il faudra le
reprendre. Ce document le dit sans l'écrire.

### 28.5 Le cas dégénéré — mesuré, et défavorable

Fixture : 100 carrés identiques décalés d'une unité ; **toutes** les paires de boîtes sont sécantes,
et aucune forme n'en recouvre entièrement une autre. `overlapPairs == candidatePairs` est **asserté**,
sans quoi le test ne mesurerait pas le pire cas qu'il prétend mesurer.

| Densité 100 %, 100 formes | Natif |
|---|---|
| balayage naïf | **2,32 ms** |
| balayage indexé | **8,85 ms** |
| **facteur** | **0,26 — l'indexation est 3,8 fois plus LENTE** |

**Pourquoi**, et c'est structurel : à densité 100 % l'index n'écarte personne, donc il paie ses
n(n−1)/2 tests de boîtes pour rien — mais surtout il **perd l'accumulateur**, qui fondait les formes
déjà vues en une région plus simple que leur somme. Le naïf soustrait un carré ; l'indexé soustrait
`n−k` carrés bruts. Le travail passe de O(n·|réunion|) à **O(n²·|forme|)**.

Cette borne est **connue et assumée** : le résultat géométrique reste identique au bit près (dernière
ligne du tableau du §28.2), et le facteur défavorable ne se manifeste que sur un document dont toutes
les formes se recouvrent deux à deux — ce qu'un dessin vectoriel n'est pas. Le chiffre à retenir est
le rapport, pas la milliseconde : 8,85 ms pour 100 formes reste sans conséquence pratique, mais la
loi en n² dit que ce ne le serait plus à 10 000 formes toutes sécantes.

`overlapPairs` / `candidatePairs` sont publiés par `SvgOverlapStats` **pour que ce cas soit
visible** : une densité proche de 100 % annonce que l'indexation ne sert à rien sur ce fichier.

### 28.6 Journal de prédictions — K4b renseigné

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K4b | facteur ~9,6 sur le poste (b) — §27.4 | I1 et I2 | **2,48 natif, 1,93 navigateur** | **infirmée**, 4 à 5 fois trop optimiste | 4 — un dénombrement de partenaires n'est pas une mesure de coût ; l'accumulateur naïf est plus simple que la somme de ses formes |
| K4b | total sous 300 ms au navigateur | I2 | **244,3 ms** | **confirmée**, sans grande marge | — |
| K4b | géométrie strictement constante | test K4b | structure identique sur 304 groupes, différence symétrique cumulée 1,5e-6 | **confirmée** au sens géométrique, **nuancée** au sens binaire : 238 groupes sur 304 identiques au bit | 6 — les deux algorithmes n'enchaînent pas le même nombre de passes Clipper2 |
| K4b | R6 devient sans objet — §27.4 | I1 et I2 | rapport G2 / complet toujours **1,00**, mais 231 ms absolus | **confirmée par la règle**, **infirmée sur le mécanisme** | 4 — « sans objet » confondait le rapport et le budget |
| K4b | l'indexation ne peut pas nuire | test K4b, cas dégénéré | **×0,26 à densité 100 %** | **infirmée** | 4 — l'accumulateur avait une vertu non nommée : il simplifiait le soustracteur |

### 28.7 Réserves après K4b — ⚠ REMPLACÉE par le §29.7

| Réserve | État | Suite |
|---|---|---|
| **R3** — coût Clipper2 de M2a | **CLOSE** depuis K4 ; le coût est désormais de 122,6 ms natif / 215,3 ms navigateur | — |
| **R5** — fixture et tests hors dépôt | **TOUJOURS OUVERTE.** `Ghostscript_Tiger.svg` et les tests K0 / K3b / K4 / K4b sont non suivis par git **[M : `git status`]** | à stager |
| **R6** — perte de gratuité de `depth` sous G2 | **LEVÉE par la règle de bascule** (244,3 < 300). Le rapport G2 / recalcul complet reste 1,00 : le phénomène est intact, son coût ne l'est plus | rouvrir si un fichier plus lourd que le Tigre repasse au-dessus du seuil |
| **R10** — le délai perçu n'est pas mesuré | inchangée : le harnais s'arrête au maillage | **K5**, sur `svg.html` |
| **R11** *(nouvelle)* — comportement en n² à forte densité | l'indexation est **3,8 fois plus lente** que l'accumulateur quand toutes les formes se recouvrent (§28.5). Aucune fixture du dépôt n'est dans ce régime ; `overlapPairs` le rend visible | rouvrir si un document réel publie une densité > 50 % |

---
---

# PARTIE 8 — Résultats du jalon K5 (2026-09-06)

**Statut du document.** K5 est le **premier jalon visible** : il se juge à l'écran. Ordre de
préséance : **partie 8 > partie 7 > partie 6 > partie 5 > partie 4 > partie 3 > partie 2 >
partie 1**.

## 29. Résultats K5 — 2026-09-06

### 29.1 Ce que K5 a écrit

| Fichier | Contenu |
|---|---|
| `src/cggraph/nodes/svg/svg_extrude_colored.{h,cpp}` | le nœud **`svg.extrude.colored`** (G2), un port `chemin` en entrée, un port `maillage` en sortie, douze paramètres, quatre mesures publiées |
| `src/cggraph/nodes/catalog.cpp` | son enregistrement, catégorie « Forme 2D », inséré **après** `svg.contours`, qui n'est pas retiré |
| `test/data/templates/svg.json` | la chaîne devient `file.ref → svg.extrude.colored → mesh.color` ; `shape.extrude` sort du document, `depth` migre sur le nœud unique |
| `maker/web/js/template.js` | `disableWhen`, un grisage piloté par une **mesure de nœud** ; et une poignée de diagnostic `window.maker` |
| `maker/web/style.css` | `.param.off` — grisé, jamais masqué |
| `src/cgmesh/import_svg.{h,cpp}` | deux compteurs de plus : `SvgOverlapStats::partiallyCoveredGroups` et `SvgExtrudeMapping::gradientGroups`. **Aucune géométrie touchée**, et l'algorithme de marqueterie est inchangé — le premier s'incrémente là où l'aire gardée était déjà calculée |
| `test/tu_cggraph_svg_k5_colored.cpp`, `test/data/graphs/svg_before_k5.json` | 9 cas, et le document **antérieur** figé sur disque |
| `test/tu_cggraph_template.cpp` | un cas de plus : le gabarit **livré** s'évalue, et la bascule se vérifie sur lui |

**La bascule.** `useSvgColors` est **un** paramètre exposé qui écrit **deux** options C++ :
`perShapeMaterials` et `overlapPolicy` (`Subtract` ou `None`). L'API C++ ne les fusionne pas — c'est
ce qui laisse K2 vérifier la palette sans la marqueterie, et K3 la marqueterie sans la palette.

### 29.2 Les quatre critères — résultats

| # | Critère | Résultat |
|---|---|---|
| 1 | un document antérieur s'ouvre et s'évalue | **tenu.** `test/data/graphs/svg_before_k5.json` (le graphe du gabarit d'avant K5) se charge contre le catalogue vivant, s'évalue sur `rose.svg` et rend un maillage à **un** matériau |
| 2 | bascule à `false` : le rendu d'aujourd'hui, `paintedFaces == nombre de faces` | **tenu, et pris au mot** : `paintedFaces == 94 828 == nFaces` au navigateur. Et l'égalité **sommet par sommet, bit pour bit**, avec `svg.contours → shape.extrude` sur le Tigre — sous deux réserves nommées au §29.4 |
| 3 | bascule à `true` : `materials.filter(m => m.kind === "color").length >= 30`, `paintedFaces == 0` | **tenu.** **39** matériaux `kind:"color"`, **39** couleurs distinctes, `paintedFaces == 0`, `keptFaces == 70 756` |
| 4 | « Couleur de la pièce » grisé **exactement** dans le cas 3 | **tenu**, et sur l'indicateur natif : `disabled == true` et classe `off` quand `paintedFaces == 0`, `false` sinon. Capture du panneau dans les deux états |

### 29.3 Ce que l'écran montre — R10 close

Instrument : Chrome `headless=new`, SwiftShader, page `svg.html` servie depuis `maker/web`,
Tigre chargé **par le sélecteur de fichier de la page**, pilotage CDP. Le module WASM a été
reconstruit (`maker/build.ps1`) — il datait d'avant K1.

- **Les couleurs sont distinctes par forme** : 39 matériaux, **39** valeurs RGB distinctes dans la
  charge utile. À l'écran, l'orange du pelage, le noir des rayures, le blanc du poitrail, le rose de
  la gueule, l'ivoire des crocs et le vert des yeux se lisent séparément. **[M + vu]**
- **Le z-fighting a disparu.** La comparaison est faite sur **la même vue** et **le même cadrage**,
  bascule à `true` puis à `false` : à `false`, la silhouette grise est **mouchetée** de points
  sombres sur toute sa surface — les capots coplanaires que la tessellation monolithique superpose ;
  à `true`, la surface est **franche**, chaque région est un aplat net. La marqueterie a bien rendu
  les capots disjoints. **[vu, deux captures au même cadrage]**
- **Ce qui reste visible et n'est pas un défaut de K5** : les moustaches sortent **pointillées**.
  C'est la sous-résolution mesurée par K3b (§26.5) — leurs traits valent 1,9e-4 à 3,8e-3 d'emprise,
  soit 27 à 1,3 fois plus fin que `flattenTol`. Le levier existe et est exposé :
  « Largeur minimale ».

**Délai perçu — R10 est CLOSE.** Mesuré dans la page par `MutationObserver` sur la ligne d'état, du
mouvement de curseur à l'affichage du résultat, donc **calcul + charge utile + mise à jour du
viewer** — ce que le harnais de K4b ne comptait pas.

| | Navigateur |
|---|---|
| `graphEvaluate` seul, médiane de 5 (curseur « Profondeur ») | **227 ms** (222 – 240) |
| **délai perçu**, médiane de 5 | **305 ms** (296 – 315) |
| harnais K4b, calcul seul (a+b+c) | 244,3 ms |

Le calcul mesuré **dans la page** (227 ms) est **en dessous** des 244,3 ms du harnais : la page
n'ajoute pas de coût de calcul, et l'écart tient à la machine et au moment. Le **transport et le
rendu** coûtent **+78 ms**, soit **34 %** de plus que le calcul. **Le délai perçu reste sous les
~300 ms de D6 à la médiane, et le franchit sur les tirages les plus lents.**

### 29.4 Deux réserves nommées, et non contournées

**(a) « Le rendu d'aujourd'hui » demande DEUX réglages, pas un.** Le nœud arme
`strokeOnFilledShapes` **par défaut** (§22.6 : défaut C++ `false`, défaut produit `true`). Décocher
la seule bascule `useSvgColors` donne donc l'**aspect** d'aujourd'hui — une pièce d'un seul ton — mais
pas sa **géométrie** : les anneaux sont toujours produits. L'égalité bit pour bit avec
`svg.contours → shape.extrude` exige de décocher aussi « Tracer les contours ». C'est figé par
`without_colors_and_without_rings_it_is_the_previous_chain_vertex_for_vertex`.

**(b) L'égalité au bit ne survit pas à la mise à l'échelle, et la cause est mesurée.** Sous
« Taille » = 100 mm, les deux chaînes ne rendent **pas** le même nombre de sommets :
**37 006** par `svg.contours` contre **36 988** par le nœud unique, soit **18 sommets (0,05 %)**.
Elles ne tessellent pas à la même échelle — `svg.contours` met les **contours** à la taille demandée
**avant** glutess, le nœud unique tesselle le dessin normalisé puis met le **maillage** à l'échelle,
et glutess décide de ses fusions (COMBINE) sur les coordonnées qu'il reçoit. Les cotes de la pièce,
elles, sont identiques. Écart **borné par un test** (`< 1 %`), pour qu'une dérive rougisse.

### 29.5 Les mesures publiées, sur le Tigre

| Mesure | Valeur | Lecture |
|---|---|---|
| `gradientShapes` | **0** | conforme à K0 (`nGradients == 0`). La mesure existe pour les fichiers qui en portent |
| `hiddenShapes` | **2** | `fullyCoveredGroups`, la valeur figée par K3. Et **non** les « 197 formes coupant une forme supérieure » de K0, qui comptaient des **boîtes englobantes** |
| `subtractedShapes` | **218** | *(nouveau)* groupes **amputés mais vivants**, sur 304. Compté sur les **aires**, pas sur les boîtes : c'est un compte géométrique, pas un majorant |
| `materials` | **39** | identique à K3b : les 65 anneaux n'apportent aucune couleur neuve |

Le seuil de `subtractedShapes` est **relatif** (un millionième de l'aire de la forme) : une différence
Clipper2 réaligne ses sommets sur une grille de 1e-6, donc l'égalité exacte ne peut pas servir de
critère.

### 29.6 Journal de prédictions — K5 renseigné

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| K5 | palette ≥ 30 couleurs au navigateur | console, `graphMeshData(0).materials` | **39**, dont 39 RGB distincts | confirmée | — |
| K5 | `paintedFaces == 0` bascule à `true`, `== nFaces` à `false` | `graphNodeInfo(3).stats` | **0 / 94 828** | confirmée | — |
| K5 | bascule à `false` == rendu d'aujourd'hui | comparaison sommet par sommet | **égal au bit**, mais **seulement** sans « Tracer les contours » **et** sans mise à l'échelle | **nuancée deux fois** | 1 — « le rendu d'aujourd'hui » supposait un seul réglage, et supposait la tessellation indépendante de l'échelle |
| K5 | le z-fighting disparaît sous marqueterie | deux captures au même cadrage | **confirmé** : moucheture à `false`, aplats francs à `true` | — | — |
| K5 | délai perçu ≈ coût de calcul (R10) | `MutationObserver` dans la page | **305 ms perçus** pour **227 ms** de calcul | +34 % | 2 — le harnais s'arrêtait au maillage, par construction |
| K5 | `hiddenShapes` vaut 2, et non « beaucoup » | mesure du nœud | **2** | confirmée | — |

### 29.7 Réserves après K5

| Réserve | État | Suite |
|---|---|---|
| **R5** — fixture et tests hors dépôt | **TOUJOURS OUVERTE**, et K5 en ajoute : `Ghostscript_Tiger.svg`, les tests K0 à K5, `test/data/graphs/svg_before_k5.json` | à stager |
| **R6** | **LEVÉE** par K4b, inchangée | — |
| **R10** — le délai perçu n'est pas mesuré | **CLOSE.** 305 ms perçus, 227 ms de calcul, +78 ms de transport et de rendu | rouvrir sur un fichier plus lourd |
| **R11** — comportement en n² à forte densité | inchangée | — |
| **R12** *(nouvelle)* — l'ordre « tesseller puis mettre à l'échelle » n'est pas neutre | glutess fusionne selon les coordonnées reçues : 18 sommets d'écart sur le Tigre à 100 mm (§29.4b). Sans conséquence sur les cotes ni sur l'aspect | rouvrir si un oracle exige l'égalité exacte entre les deux chaînes sous mise à l'échelle |

**Ce que K5 n'a pas fait, et le dit** : l'export reste sans couleur (STL sans matière, OBJ sans
`mtllib`) — chantier distinct, déjà annoncé par le gabarit ; le mode texture (K6) reste hors v1 ;
le mémo interne n'a pas été écrit (D8).

> **Fermé le 2026-09-06** par `src/cgmesh/docs/export_materiaux_faisabilite.md`, jalons G0 à G3 :
> l'export OBJ d'une page gabarit rend une archive `{ .obj, .mtl }` et le Tigre en sort avec ses
> 39 couleurs **[M]**. Le STL, lui, reste sans couleur — c'est le format qui n'en porte pas.

---
---

# PARTIE 9 — Correctif : le trait d'une forme sans remplissage (2026-09-06)

**Statut du document.** Cette partie corrige un défaut **préexistant** au chantier « import SVG
multi-formes avec couleurs » : il vivait dans `import_svg.cpp` avant D7 et n'a été ni introduit ni
aggravé par lui. Elle prime sur toute description antérieure du routage par chemin. Ordre de
préséance : **partie 9 > partie 8 > … > partie 1**.

## 30. Le trait d'une forme sans remplissage était un ruban, pas un anneau

### 30.1 Le défaut

`svg_to_shape_groups` décide **par chemin**, sur `NSVGpath::closed`. Trois cas existent ; il n'en
traitait que deux :

| Chemin | Attendu | Produit avant correction |
|---|---|---|
| fermé, forme remplie | tessellation, plus l'anneau du trait sous D7 | conforme |
| fermé, forme **sans** remplissage | **anneau** (`strokeClosedToContours`, `EndType::Joined`) | **ruban ouvert** (`strokeToContours`, `EndType::Butt/Square/Round`) |
| ouvert | ruban | conforme |

`strokeRings.push_back` n'était atteignable que depuis la branche `closed && hasFill`. Un tracé
fermé d'une forme en `fill="none"` tombait donc dans la branche des tracés ouverts et recevait un
décalage de chemin **ouvert**, qui ne parcourt pas l'arête de fermeture. La primitive correcte
existait — elle avait été livrée par D7 — mais n'était jamais atteinte pour ce cas.

### 30.2 La mesure

Instrument : `TU.exe`, aire du capot supérieur du prisme extrudé, `centerAndFit = false` (la sortie
est alors dans les unités du document, donc comparable à une aire analytique). Le contrôle négatif
a été rejoué en désactivant la seule branche ajoutée, tout le reste identique.

| Fixture | Sans la branche | Avec la branche | Anneau analytique |
|---|---|---|---|
| `<rect 100×100 fill="none" stroke-width=4>` | **1200**, 1 contour | **1600**, 2 contours | 4 × 100 × 4 = 1600 |
| `<polygon 100×100 fill:none stroke-width:10>` | **3000** | **4000** | 4 × 100 × 10 = 4000 |
| `<circle r=50 fill="none" stroke-width=4>` | 1256,78 | 1256,81 | 400 π ≈ 1256,64 |

1200 = 3 côtés × 100 × 4 : **un côté sur quatre manquait**, soit 25 % de l'aire sur un contour
rectiligne. Le cas circulaire ne bougeait pas de façon visible — une arête de fermeture entre deux
points d'un cercle aplati est négligeable devant le périmètre — ce qui explique qu'aucune fixture
courbe n'ait pu servir de détecteur.

### 30.3 La correction

Une branche intercalée dans la décision par chemin, symétrique du cas fermé-et-rempli :

```cpp
if      (closed && hasFill)                                        { /* surface (+ anneau sous D7) */ }
else if (closed && opt.strokeToVolume && hasStroke && pts.size() >= 3) { /* anneau */ }
else if (opt.strokeToVolume && hasStroke)                          { /* ruban ouvert */ }
```

Deux points sur lesquels la correction évidente aurait été fausse :

- **Le cas n'est pas soumis à `ringsWanted`.** `strokeOnFilledShapes` (D7) gouverne le trait des
  formes **remplies**, et son défaut C++ est `false` pour préserver la non-régression. Le trait
  d'une forme **sans** remplissage relève de `strokeToVolume` et est produit depuis toujours : le
  soumettre à `strokeOnFilledShapes` l'aurait fait disparaître par défaut.
- **Le seuil est de trois points, pas deux.** C'est celui de `strokeClosedToContours` : une boucle à
  deux points n'a pas d'intérieur, donc pas d'anneau. Un tel tracé n'est pas écarté pour autant — la
  condition étant dans la garde de la branche et non dans son corps, il retombe sur le ruban, ce qui
  est bien son trait.

### 30.4 Portée mesurée — aucune non-régression ne bouge

Instrument : suite `TEST_cgmesh_svg*` complète, sortie comparée ligne à ligne avant / après.

- Tigre : **13 formes au trait seul, toutes à chemins ouverts** ; `nazca`, `batman`, `spiderman`,
  `rose` n'en ont aucune. La correction ne peut donc pas les atteindre.
- Vérifié au chiffre près : K0 (239 formes, 8 491 points, 36 988 sommets, 56 888 faces), K3
  (2,09803 / 0,627064 / 1,47097), K1 (égalité point par point, 238 groupes identiques au bit).
- Les seules lignes qui diffèrent sont **chronométriques** et ne sont assertées nulle part.

### 30.5 La leçon de méthode — un test nommé d'après ce qu'il ne vérifie pas

`TEST_cgmesh_svg_stroke.unfilled_polygon_becomes_a_thickened_ring` existait, portait le nom exact du
comportement attendu, et son commentaire annonçait l'anneau. Il mesurait 3000 au lieu de 4000. Ses
assertions — étanchéité du solide, aire de l'anneau strictement inférieure à celle du carré plein —
sont vraies **aussi bien pour un U** : aucune ne pouvait distinguer les deux.

Un tel test est **pire que pas de test** : il a fait croire à une couverture pendant tout le
chantier, et le nom du test a servi de preuve à sa place. La revue elle-même serait passée à côté
sans une passe qui a **mesuré** au lieu de relire.

Règle retenue : **une assertion sur un ordre de grandeur ne vérifie pas une géométrie.** Quand un
oracle analytique existe — ici l'aire de l'anneau, périmètre × largeur — c'est lui qu'il faut
asserter, à côté d'un second oracle **structurel** (le nombre de contours : deux pour un anneau, un
pour un ruban) qui interdit qu'une aire juste soit obtenue par une forme fausse.

### 30.6 Ce que le correctif a écrit

| Fichier | Contenu |
|---|---|
| `src/cgmesh/import_svg.cpp` | la branche « fermé sans remplissage → anneau », et la table de décision par chemin remise à jour |
| `test/data/svg/closed_stroke_only.svg` | fixture neuve : `<rect 100×100 fill="none" stroke-width="4">`, joint mitre explicite |
| `test/tu_cgmesh_svg_stroke.cpp` | `closed_stroke_only_yields_a_ring_of_perimeter_times_width` (1600 et deux contours), `unfilled_circle_yields_a_ring_of_the_analytic_area` (400 π, fige l'aplatissement), et l'assertion chiffrée manquante sur `unfilled_polygon_becomes_a_thickened_ring` |

**Réserve R5** (fixtures et tests hors dépôt) s'étend à `test/data/svg/closed_stroke_only.svg`.
