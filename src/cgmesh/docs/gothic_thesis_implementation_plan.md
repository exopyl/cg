# Fenêtre gothique — État d'implémentation & plan (vs thèse Havemann §5.4)

Doc vivant unique du sous-système « remplage gothique » : modèle de référence, état
courant du code, et reste à faire.

Référence : Sven Havemann, *Generative Mesh Modeling*, TU Braunschweig 2005, §5.4
« Gothic Architecture » (`Document.pdf`, p. 233-262) ; Havemann & Fellner,
« Generative Parametric Design of Gothic Window Tracery » (VAST 2004). Figures clés :
paramètres 5.30 ; styles 5.31-5.33 ; constructions 5.23 / 5.25 / 5.27 / 5.36-5.39.

Fichiers : `src/cgmath/architecture_gothic{,_io}.{h,cpp}`,
`src/cgmath/gothic-tracery.schema`, `src/cgmesh/architecture_gothic.{h,cpp}`,
`src/cgmesh/surface_architecture.{h,cpp}`, `src/cgmesh/parameterized_shapes.{h,cpp}`,
`maker/wasm_api.cpp`, `maker/web/app.js`.

---

## 1. Modèle de la thèse (référence §5.4)

Une fenêtre gothique = un **assemblage de CHAMPS** (fields) découpés dans un
**plan-bordure de pierre contigu**, à partir de **cercles + segments de droite**
seulement, combinés par **intersection / offset / extrusion**.

1. **Arc brisé** (Fig 5.23) : deux cercles de rayon `r`, centres `mL,mR` sur
   l'horizontale des pieds `pL,pR`. `excess = r/dist(pL,pR)` (0.5 = plein cintre,
   1 = équilatéral, >1 = lancéolé).
2. **Offset à centres fixes** (Fig 5.26 d) : décaler un arc **change son excess**
   (centres gardés, rayon changé). Deux offsets : `bdOuter` (distance des champs à
   l'arc principal), `bdInner` (distance des champs entre eux) → forme le plan-bordure.
3. **Fenêtre prototype** (Fig 5.25) : arc principal + **2 sous-arcs** (même excess) +
   **rosace circulaire**. `arcDown` = descente verticale des bases des sous-arcs.
4. **Rosace** (Fig 5.25 c-d) : centre `mC` = intersection d'une **ellipse** (foyers
   `mR`, `mLR`, somme des rayons) avec l'axe de symétrie ; rayon déduit par tangence
   (interne à l'arc principal, externe aux sous-arcs).
5. **Les 7-8 CHAMPS** (Fig 5.25 a, 5.26) : arc principal, rosace, sous-arc G, sous-arc
   D, **et 4 FILLETS** (régions résiduelles entre rosace / sous-arcs / arc principal).
   Chaque champ **rétréci de `bdInner`** → plan-bordure contigu (la pierre) ; les
   champs rétrécis = les **vides** (verre).
6. **Fillets = reste du plan quand on retire rosace + sous-arcs** (p.254) : la thèse
   désigne le **2D-CSG sur arcs de cercle** comme LA bonne méthode (qu'elle n'avait
   pas). Disponible ici via **Clipper2**.
7. **Rosace à foils** (Fig 5.27, 5.29) : `n` foils **ronds** (`fr = sin(α/2)/(1+sin(α/2))·R`,
   `α = 2π/n`) ou **pointus** (déplacement des centres le long de `(a,m),(b,m)`,
   « pointedness ») ; rosaces couchée/debout (`phi0 += π/n`).
8. **Arc trilobé pointu** (Fig 5.27 2c-d) : scinder chaque demi-arc, remplacer la
   partie basse par **deux arcs plus petits** tangents (C1 par « centre du prochain
   arc sur la droite centre→extrémité »).
9. **Profils 3D** (Fig 5.28, 5.29, p.244) : la 3D vient de **profils balayés le long
   des bords de champ**, plan du profil ⟂ tangente ; **aux coins → plan bissecteur**.
   Deux styles : **flat** (faces avant coplanaires, champs = anneaux) et **French**
   (barres rondes). Appui (windowsill) par extrusion de chemin.
10. **Récursion** (p.240, 246, 254) : les sous-arcs **sont** des arcs brisés → style
    récursif ; profondeur 2 → 2·2·2 = 8 sous-fenêtres. Règles **harmoniques** :
    `arcDown`, `wallSetback` et l'échelle du profil **÷2 par niveau** ; nombre de
    foils **constant**.
11. **Séparation contenu / apparence** (Fig 5.30-5.33) : la géométrie (champs) est
    fixe ; le **Style** = 4 fonctions décorant les 4 types de champ (`style-main-arch`,
    `style-fillet`, `style-rosette`, `style-sub-arch`), + librairie de profils, +
    styles récursifs assemblés à la volée (`make-style-dict`). Donne styles 1-8 (Fig 5.40).

---

## 2. Architecture du code

### 2.1 Deux pipelines (ne partagent que `buildGeometryFromInstance`)

| Chaîne | Entrée → sortie | Décorations rendues |
|---|---|---|
| **A – SVG 2D** | schéma JSON → `loadInstanceFromJson` → `WindowInstance` → `toSvg` → SVG | foils, trefoil, fillets, mouchettes |
| **B – Mesh 3D** (maker) | sliders `app.js` **ou** import JSON v2 (`createGothicFromJson`→`LoadFromJson`) → `ParameterizedGothicWindow` → `buildBayStonePolygon` → `extrudeToMesh` (+ `buildBayMoulding`) → OBJ/STL/GLB | lancets, rosace, fillets, mouchettes, têtes foliées, moulures |

Deux schémas JSON distincts : **v1** `gothic-tracery.schema` alimente la chaîne SVG
(`loadInstanceFromJson`) ; **v2** `gothic-window-v2.schema` alimente le maker via
`ParameterizedGothicWindow::LoadFromJson` (§4.6). Côté mesh, **trefoil** et **foils de
lancette** ne sont pas rendus (SVG uniquement).

### 2.2 Intégration maker (pattern `IParameterized`)
`ParameterizedGothicWindow : ParameterizedMesh` :
- membres = paramètres exposés (float/int/bool/enum) ; `GetParameters()` = liste typée ;
- `Regenerate()` : remplit un `WindowInstance` → `buildGeometryFromInstance` →
  `buildBayStonePolygon` → `extrudeToMesh(zHeight)` (+ moulures) → `ComputeNormals()` ;
- enregistré au catalogue Embind (`wasm_api.cpp`).

**Robustesse (critique)** : les `buildXxx()` lèvent sur entrée invalide.
`Regenerate()` **doit** (a) clamper via les min/max des `Parameter`, (b) dériver les
params dépendants (ex. `gapFraction` borné par `count`, `inner < rayon`), (c) envelopper
le pipeline d'un **try/catch** produisant un **placeholder** en cas d'échec — sinon un
réglage invalide plante le WASM.

### 2.3 Primitives simples (`surface_architecture`, hors remplage)
Sous-système séparé, peu de params : `CreateBlock` (bloc biseauté), `CreateArch`,
`ArcBrise`, `Rosace`, `create_arc_brise/accolade/anse_de_panier/rampant`. Utile comme
maçonnerie ; non couplé au pipeline de remplage.

---

## 3. Couverture des 8 paramètres (Fig. 5.30)

| Param thèse | État | Code |
|---|---|---|
| `excess`      | conforme | `excess` (arc + lancets) |
| `arcDown`     | conforme | `drop` (« Lancet drop ») |
| `bdOuter`     | conforme | `outer` (« Offset outer ») |
| `bdInner`     | partiel  | `inner` ; offset de champ ET (layout Prototype) meneau central ; encore éclaté en insets ad hoc ailleurs (fillet, meneau `0.30·r`, anneau rosace `0.10·R`) |
| `wallSetback` | absent   | pas de mur dans le mesh |
| `heightBott`  | partiel  | consommé via import JSON v2 → `bodyHeight` (dérivé) ; pas de slider dédié |
| `kseg`        | partiel  | consommé via import JSON v2 → `m_maxAngleRad` ; défaut 1°, pas de slider |
| `Style`       | divergence | booléens/enums figés, pas un système de styles |

---

## 4. État courant de l'implémentation

### 4.1 Primitives géométriques 2D (cgmath)
- Arc brisé `buildArch` : `r = excess·width`, centres mobiles, apex = intersection ;
  familles four-centered / équilatéral / pointu.
- Offset `buildArchOffset` : centres fixes, rayon variable (change l'excess).
- Sous-fenêtres `buildSubwindows`, `SubwindowParams::Layout` :
  - **Uniform** (défaut) : `count+1` meneaux égaux sur l'ouverture brute ;
  - **Prototype** (Fig 5.39) : pieds extérieurs sur l'arc inset de `bdOuter`, `count-1`
    meneaux intérieurs de `bdInner` (exact n=2, généralisé n>2).
- Rosace `buildRosette` : ellipse à somme focale, tangente aux arcs visibles
  (`F2 = circleR`, arc face à l'axe).
- Foils `buildFoilRing` (ronds + pointus, formules exactes) ; trefoil `buildTrefoilArch` ;
  cercle inscrit `inscribedCircleOfPointedArch`.
- Mouchettes `buildMouchette(type, center, radius, rotation)` :
  **Vesica** (lentille, 2 arcs), **Teardrop** (goutte, 3 arcs), **Soufflet** (triangle
  curviligne de Reuleaux, 3 arcs convexes, largeur constante).
- Paramètres cgmath (`WindowInstance`, chaîne SVG) au-delà du mesh : `FoilsParams`
  rosace/lancette (count 3-24, type, pointedness, phi0, orientLying), trefoil arc &
  lancette (splitParameter, foilRadiusFactor), fillets (`filletsStoneBandWidth`),
  mouchettes (radiusFactor, rotation). Schéma JSON : `gothic-tracery.schema`.

### 4.2 Maillage & remplage (cgmesh)
- `buildBayStonePolygon` : polygone multi-contour (cadre + trous) tessellé GLU en une
  passe → plan de pierre unique. Sept champs : cadre, 2 lancets, rosace, fillets.
- **Fillets** : les 4 champs d'écoinçon via 2D-CSG Clipper2 (`region − rosace − sous-arcs`,
  inset `filletInset`).
- **Mouchettes** (`params.mouchettes`) : chaque champ de fillet vidé par une
  mouchette/soufflet inscrite (`mouchetteInField`), clippée au champ (Clipper2
  `Intersect`), apex vers l'oculus ; `filletInset` réduit à un fin liseré. Idem sous-
  écoinçons de la récursion (`collectUnitVoids`), garde-fou `rm < 1.5 → champ plat`.
- Têtes de lancette foliées (cuspées) via `cuspedArchOutline` ; rosace bar-tracery
  (fleur multifoil connectée) via `rosetteVoidContours`.
- **Récursion** (`recursionDepth` 0-2) : chaque lancet devient une mini-fenêtre
  (sous-lancets + sous-rosace + fillets) via `collectUnitVoids`.
- Corps vertical + seuil : `bodyHeight` ; bas des lancets = bas du cadre + `bdOuter`
  (Fig 5.36 l.45).

### 4.3 Extrusion & profils (cgmesh)
- `extrudeToMesh` (plaque plate, sommets caps/parois séparés → normales correctes) ;
  `extrudeProfiledToMesh` (chanfrein splayé par contour, scaling + skip petits/cuspés).
- **Moulures de barre** `buildBayMoulding` : balaye un profil `(u,v)` le long des
  contours d'ouverture RÉELS (cadre, lancets tête foliée + jambages, sous-tracery de
  récursion, rim de rosace) via `sweepPolylineBuffers` (plan ⟂ tangente). Limitation de
  rayon par contour (≤ 0,6 × rayon caractéristique ; sauté sous 0,2) — approximation de
  `extrudestable`. Profils : Roll (demi-rond), Keel (arête), Ogee (cyma).
- `sweepProfileAlongArc(s)` (B2) disponible.

### 4.4 UI maker (`ParameterizedGothicWindow`)
Excess, Width, Body height, Offset outer/inner, Lancet head {Plain, Foiled}, Lancet
head foils, Recursion, Lancet drop, Lancet excess, Gap fraction,
**Lancet layout {Uniform, Prototype}**, Fillets, **Mouchettes**,
**Mouchette type {Vesica, Teardrop, Soufflet}**, Rosette (+ foils, count, type,
pointedness), **Profile {Flat, Chamfer, Roll bar, Keel bar, Ogee bar}**.

### 4.6 Import JSON (fichier de description)
Schéma `src/cgmath/gothic-window-v2.schema` (Draft 2020-12, validé) : `geometry`
(Fig 5.30) + `style` (profil + décoration par type de champ) + `recursion` +
`extrusion` (hors-thèse, `zHeight`). Exemple : `maker/examples/gothic-window.json`.
Flux : `<input .json>` (« Import Gothic Window (JSON) ») → `importGothicJson` (app.js)
→ `createGothicFromJson` (wasm_api) → `ParameterizedGothicWindow::LoadFromJson` (mappe le
schéma v2 sur les membres) → `Regenerate()` (réutilise tout le driver : clamps, override
rosace, `buildBayStonePolygon`, `buildBayMoulding`). L'import force un bootstrap o3dv
(`applyShape(id, true)`) qui recadre la caméra de façon fiable (idem import SVG).
Correspondances : `heightBott`→`bodyHeight`, `kseg`→`m_maxAngleRad`, profil pris sur
`style.mainArch` (GLOBAL ; le par-champ reste §5.2), `wallSetback` non consommé.
**Export** (symétrique) : bouton « Export Gothic Window (JSON) » → `exportGothicJson`
(wasm_api) → `ParameterizedGothicWindow::ExportJson` (inverse de `LoadFromJson` : membres
courants → JSON v2, `heightBott = -bodyHeight`, index→libellés d'enum). Round-trip vérifié
(export → import reproduit le même mesh). Le fichier exporté est validé par le schéma v2.

### 4.5 Conforme à la thèse (ne pas retoucher)
Arc brisé, offset à centres fixes, formules de foils, `drop = arcDown`, cercle
inscrit, layout Prototype (Fig 5.39), rosace tangente aux arcs visibles, 4 fillets
2D-CSG Clipper2, seuil de lancet = `bdOuter`, mouchettes/soufflets, correspondance
Fillets on/off ≈ bar/plate tracery.

---

## 5. Améliorations (reste à faire)

### 5.1 Profils moulurés
- **Mitrage des coins** (plan bissecteur, §5.4.1) — jonctions « ouvertes ». *(reporté)*
- **`extrudestable` complet** (squelette droit) : le clamp par-contour ne traite pas
  les cuspides locales très serrées d'un grand contour.
- **Profils du style plat** : généraliser le chanfrein `extrudeProfiledToMesh` en
  cavetto / ogée sur le bord (distinct des barres françaises).

### 5.2 Système de styles (écart architectural de fond)
Implémenter le cœur de §5.4 : **construction par CHAMPS unifiée** (au lieu de
découper des trous) + dictionnaire de 4 fonctions de champ + librairie de profils
routée par champ + styles récursifs à la volée (`make-style-dict`, Style-8). Pas de
séparation contenu/apparence aujourd'hui. Plus gros chantier ; à défaut, documenter que
le code est « une fenêtre paramétrable », pas le système génératif.

### 5.3 Fidélité géométrique cgmath (impact SVG)
- **Rosace sur rayons bruts** : la somme focale omet `bdInner`/`bdOuter` (thèse :
  `rad+radL ± bdInner`) → rosace mal dimensionnée dès que les offsets > 0.
- **Trefoil non fidèle** : contour ouvert + un seul demi-cercle par côté au lieu de la
  paire d'arcs fermée (Fig 5.27 2c-d). Non branché au mesh.

### 5.4 Paramètres & couverture
- Exposer en **sliders** `kseg` et `heightBott` (déjà réglables via l'import JSON v2, §4.6, mais pas dans l'UI).
- **`wallSetback`** : non consommé (ni UI ni JSON) — suppose un modèle de mur, absent.
- Unifier **`bdInner`** comme offset unique (aujourd'hui éclaté en insets ad hoc).
- Brancher au mesh, ou exposer, les primitives cgmath dormantes : **trefoil**, **foils
  de lancette**, `count > 2`.

### 5.5 Récursion
- Règle harmonique thèse (offsets / échelle ÷2 par niveau) — actuellement facteurs ad hoc.
- Variation de style par niveau (dépend de 5.2).

### 5.6 Représentation
Pas de B-rep / GML ni control-mesh (kseg) — SVG + mesh triangulé. Écart assumé par
conception ; à documenter comme tel si le système de styles n'est pas visé.

### 5.7 Dette de commentaires
- `src/cgmesh/architecture_gothic.h`, bloc de tête : supprimer « fillets as voids NOT
  covered », « cusped lancet heads intentionally NOT applied », « Clipper2 not currently
  vendored » (tous faux aujourd'hui).
- `src/cgmath/architecture_gothic.h` : sémantique pL/pR (pieds, pas centres) et
  « circleL centered at pL » ; helper cercle inscrit documente `width/2` au lieu de `halfD`.
- `src/cgmath/architecture_gothic_io.h` : liste « Ignored (silently) » périmée (foils /
  trefoil / fillets / mouchettes sont parsés).

### 5.8 Dépendance build
La bascule vers une construction par champs (5.2) et les offsets de champ
(`InflatePaths`) supposent `clipper.offset.cpp` dans `CLIPPER2_SRC_FILES`
(`src/cgmesh/CMakeLists.txt`). Garder le stress test + revalidation native à chaque étape.
