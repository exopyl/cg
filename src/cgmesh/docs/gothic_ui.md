# Architecture gothique dans maker — paramètres & intégration UI

> Analyse (pas d'implémentation). Objectif : ajouter les surfaces d'architecture
> gothique à la liste des géométries paramétriques de `maker`.
> Date : 2026-07-16. Sources code : `src/cgmath/architecture_gothic.{h,cpp}`,
> `architecture_gothic_io.{h,cpp}`, `gothic-tracery.schema` ; `src/cgmesh/
> architecture_gothic.{h,cpp}`, `surface_architecture.{h,cpp}`.
> Base théorique : Havemann & Fellner, « Generative Parametric Design of Gothic
> Window Tracery » (VAST 2004) ; Havemann, thèse TU Braunschweig (2005).

---

## Partie 1 — Le point sur ce qui existe

Il y a **deux niveaux** de code gothique, complémentaires :

### A. Le remplage (tracery) paramétrique complet — cgmath
Pipeline data‑driven basé sur la thèse de Havemann. Sortie = géométrie **2D**
(Circle/Arc/Vector2d), assemblée dans `WindowGeometry` puis convertie en **Mesh 3D**
côté cgmesh (tessellation GLU + extrusion). C'est **la** pièce maîtresse.

Chaîne : `WindowInstance` (paramètres) → `buildGeometryFromInstance` →
`WindowGeometry` → `buildBayStonePolygon` → `extrudeToMesh(zHeight)` → `Mesh`
(exportable OBJ/STL/PLY/…).

**Paramètres de `WindowInstance`** (ce qui est réellement câblé) :

| Groupe | Paramètre | Type | Domaine / défaut | Rôle |
|---|---|---|---|---|
| Arc principal | `excess` | double | > 0.5 (1 = ogive équilatérale) | acuité de l'ogive (r / largeur) |
| | largeur (via pL,pR) | double | > 0 | largeur de la baie |
| Offset pierre | `outer` | double | ≥ 0 | épaisseur vers la face pierre |
| | `inner` | double | ≥ 0, < rayon | épaisseur vers le vide (verre) |
| Lancettes | `count` | int | **[1, 6]** | nombre de lancettes |
| | `drop` | double | ≥ 0 | descente des bases sous l'arc principal |
| | `excess` (sub) | double | > 0.5 | acuité des sous‑arcs |
| | `gap.mode` | enum | Fraction \| Absolute | mode de largeur des meneaux |
| | `gap.gapFraction` | double | (0, 1/(count+1)) | largeur meneau (fraction) |
| | `gap.absoluteWidth` | double | (0, mainW/(count+1)) | largeur meneau (absolue) |
| Rosette | `hasRosette` | bool | — | cercle couronnant la baie |
| Foils rosette | `hasRosetteFoils` | bool | — | active l'anneau de foils |
| | `count` | int | **[3, 24]** | nombre de foils |
| | `type` | enum | Round \| Pointed | foils ronds ou en pointe |
| | `pointedness` | double | (0, 2] (si Pointed) | acuité des foils pointés |
| | `phi0` | double | rad | rotation initiale |
| | `orientLying` | bool | — | décalage de π/n |
| Foils lancettes | `hasSubwindowFoils` + mêmes champs FoilsParams | | | appliqués à chaque lancette |
| Trèfle arc | `hasArchTrefoil` | bool | — | découpe l'arc + foil tangent (C1) |
| | `splitParameter` | double | (0, 1) | position de la coupe le long du demi‑arc |
| | `foilRadiusFactor` | double | (0, 0.5] | rayon du foil / rayon arc |
| Trèfle lancettes | `hasSubwindowTrefoil` + mêmes champs | | | par lancette |
| Fillets | `hasFillets` | bool | — | 2 fillets latéraux (régions à 3 arcs) |
| | `filletsStoneBandWidth` | double | > 0 | bande de pierre |
| Mouchettes | `hasMouchettes` | bool | — | formes flamboyantes entre lancettes |
| | `type` | enum | Vesica \| Teardrop \| Soufflet* | (*Soufflet aliasé à Teardrop) |
| | `radiusFactor` | double | (0, 0.5] | taille / largeur de sous‑baie |
| | `rotation` | double | rad | orientation |

**Paramètres de maillage** (`GothicMeshParams`, `src/cgmesh/architecture_gothic.h`) :

| Paramètre | Type | Défaut | Rôle |
|---|---|---|---|
| `zHeight` | double | 0 | > 0 → extrusion solide (caps + parois) ; 0 → plat |
| `maxAngleRad` | double | 1° | finesse d'échantillonnage des arcs |

Sorties annexes : `toSvg(WindowGeometry)` (remplage en SVG 2D), `writeBayMesh` (one‑shot),
`sweepProfileAlongArc(s)` (moulures : profil extrudé le long d'un arc, avec
profils prêts : `rectangularProfile`, `chamferProfile`, `cavettoProfile`).

**Limites connues (documentées dans le code)** : les foils imbriqués dans un vide
deviennent des « îlots de pierre » (winding NONZERO de GLU) — visuellement OK ;
trèfles/mouchettes/fillets pas encore soustraits comme vrais vides ; profils B2
(moulures) présents mais non branchés au bay.

### B. Primitives simples — cgmesh `surface_architecture`
Produisent directement un `Mesh` ou un `Polygon2`, peu de paramètres :

| Fonction / classe | Paramètres | Sortie |
|---|---|---|
| `CreateBlock` | width, height, depth, bevel | Mesh (bloc biseauté) |
| `CreateArch`, `CreateArch2` | — (aucun) | Mesh (arc pré‑construit) |
| `ArcBrise` | altitude, width, height (+ 2ᵉ arc) | Polygon2 |
| `Rosace` | nFoils, radius | Polygon2 |
| `create_arc_brise/accolade/anse_de_panier/rampant` | a, b/e/t, offset, npts | courbes 2D (p, tgt) |

---

## Partie 2 — Intégration dans l'interface maker

### 2.1 Faisabilité (vérifiée dans le code)
- **[vérifié]** cgmath compile en WASM via `GLOB *.cpp` → `architecture_gothic` + `_io`
  (nlohmann/json header‑only) sont **déjà** dans le module WASM.
- **[vérifié]** Côté cgmesh, `architecture_gothic.cpp` et `surface_architecture.cpp`
  ne sont **pas** dans le core `if(EMSCRIPTEN)` → à **ajouter** (leurs deps
  `polygon2.cpp`, `clipper.cpp`, glutess y sont déjà).
- **[vérifié]** Le pipeline `WindowInstance → buildGeometryFromInstance →
  buildBayStonePolygon → extrudeToMesh → Mesh` existe. On peut **construire
  `WindowInstance` en C++** depuis des `Parameter` — **pas besoin de JSON** pour
  l'interactif (le JSON reste utile pour charger des presets).

### 2.2 Le pattern IParameterized
Un `ParameterizedGothicWindow : ParameterizedMesh` (dans `parameterized_shapes.{h,cpp}`) :
- membres = les paramètres exposés (float/int/bool/enum) ;
- `GetParameters()` renvoie la liste typée ;
- `Regenerate()` : remplit un `WindowInstance` depuis les membres →
  `buildGeometryFromInstance` → `buildBayStonePolygon` → `extrudeToMesh(zHeight)` →
  `m_pMesh` → `ComputeNormals()`.
- ajout au catalogue du pont Embind (`wasm_api.cpp`) : `{"Gothic Window", make<…>()}`.

### 2.3 Le vrai enjeu : ~25 paramètres + interdépendances
Le remplage a beaucoup de paramètres, dont des **sous‑features optionnelles**
(rosette, foils, trèfle, fillets, mouchettes) chacune derrière un bool, et des
**contraintes croisées** (ex. `gapFraction < 1/(count+1)`, `inner < rayon`,
`excess > 0.5`). Deux implications :

1. **UI** : le panneau `maker` est une liste plate de widgets (INT/FLOAT/BOOL/ENUM).
   Il gère ~25 widgets sans souci ; les bool servent d'interrupteurs de feature.
   Les params dépendants (ex. `pointedness` seulement si `Pointed`) restent
   affichés mais ignorés quand inactifs — acceptable (ou griser plus tard).
2. **Robustesse CRITIQUE** : les `buildXxx()` **lèvent des exceptions** sur entrée
   invalide. `Regenerate()` **doit** : (a) clamper via les min/max des `Parameter`,
   (b) dériver les params dépendants (ex. `gapFraction` borné par `count`),
   (c) **try/catch** autour du pipeline et, en cas d'échec, produire un mesh
   valide de repli (dernier bon mesh, ou petit placeholder) — sinon un réglage
   invalide fait planter le WASM. (Même esprit que les gardes L‑système.)

### 2.4 Options d'intégration (une décision à prendre)

| Option | Contenu | Pour / Contre |
|---|---|---|
| **A. Une entrée « Gothic Window »** (reco) | tout le remplage riche, ~15‑25 params exposés, extrusion zHeight | + le plus impressionnant, un seul objet ; − beaucoup de widgets |
| **B. Entrée réduite « Gothic Window (simple) »** | arc + offset + lancettes + rosette + zHeight (~8 params), foils/trèfle/mouchettes en dur ou off | + panneau digeste ; − cache la richesse |
| **C. Primitives simples séparées** | « Gothic Block », « Gothic Arch », « Rosace » (peu de params) | + trivial, robuste ; − pas le remplage |
| **D. Presets JSON** | charger `.json` (schéma tracery) via l'import fichier, comme le SVG | + expose toute la puissance sans 25 sliders ; − pas d'édition live fine |

### 2.5 Recommandation
- **A. « Gothic Window » riche** comme intégration principale — c'est la valeur
  ajoutée unique de ce sous‑système. Exposer : `excess`, `width`, `offset
  outer/inner`, `lancettes count/drop/excess/gapFraction`, `rosette` (bool),
  `foils rosette` (bool/count/type/pointedness), `foils lancettes`
  (bool/count/type), `trèfle arc` (bool/split/foilRadius), `mouchettes`
  (bool/type/radiusFactor), `fillets` (bool), `zHeight`, `finesse arcs`.
- **+ C. « Gothic Block »** en bonus (trivial, robuste, utile comme masonry).
- Robustesse : clamp + try/catch + mesh de repli **obligatoires**.
- **D.** possible plus tard : réutiliser l'import fichier de maker pour charger un
  preset `.json` (le schéma existe) — complète A sans alourdir le panneau.

### 2.6 Plan incrémental proposé
1. Ajouter `architecture_gothic.cpp` + `surface_architecture.cpp` au core WASM
   (`src/cgmesh/CMakeLists.txt`, branche `if(EMSCRIPTEN)`). Effort S. Vérifier
   que ça compile sous emcc (exceptions déjà activées `-fexceptions`).
2. `ParameterizedGothicBlock` (CreateBlock) — trivial, valide la chaîne. Effort S.
3. `ParameterizedGothicWindow` : sous‑ensemble d'abord (arc+offset+lancettes+
   rosette+zHeight), avec clamp+try/catch+repli. Effort M. Valider un rendu.
4. Étendre aux foils / trèfle / mouchettes / fillets (bools + params). Effort M.
5. (option) Import preset `.json` via l'`<input file>` existant. Effort S‑M.

### 2.7 Points ouverts (à valider)
1. Option **A / B / C / D** (ou combinaison) pour la première livraison ?
2. Extrusion par défaut (`zHeight`) : valeur, et unité relative à la largeur de baie.
3. Griser les params inactifs (dépend d'un bool) dès maintenant, ou plus tard ?
4. Nom(s) dans le catalogue (« Gothic Window », « Gothic Block », …).
</content>
