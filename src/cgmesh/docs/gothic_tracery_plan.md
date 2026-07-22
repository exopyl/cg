# Remplage gothique — analyse & plan d'implémentation

Référence : **Havemann, _Generative Mesh Modeling_ (thèse, TU Braunschweig 2005), §5.4
« Gothic Architecture »** (`Document.pdf`, p. 236‑262) et Havemann & Fellner, VAST 2004.
Croisé avec l'état du code dans `src/cgmath` + `src/cgmesh` et l'app `maker/`.

---

## Partie 1 — Le modèle de la thèse (la référence)

La thèse construit une fenêtre gothique comme un **assemblage de CHAMPS** (fields)
découpés dans un **plan‑bordure contigu** (la pierre), à partir de **cercles et
segments de droite** uniquement, combinés par **intersection / offset / extrusion**.

1. **Arc brisé** (§5.4.1, Fig 5.23) : deux cercles de rayon `r`, centres `mL,mR`
   sur l'horizontale des points de base `pL,pR`. `excess = r/dist(pL,pR)`
   (0.5 = plein cintre, 1 = équilatéral, >1 = lancéolé).
2. **Offset à centres fixes** (Fig 5.26d) : décaler un arc brisé **change son
   excess** (on garde les centres, on change le rayon). Deux offsets distincts :
   - `bdOuter` : distance des champs à l'arc principal,
   - `bdInner` : distance des champs **entre eux** → crée le plan‑bordure.
3. **Fenêtre prototype** (Fig 5.25) : arc principal + **2 sous‑arcs** (mêmes
   excess) + **rosace circulaire**. `arcDown` = descente verticale des bases des
   sous‑arcs (dégage la place du couronnement).
4. **Rosace** (Fig 5.25c‑d) : centre `mC` = intersection d'une **ellipse**
   (foyers `mR`,`mLR`, somme des rayons) avec l'axe de symétrie ; rayon déduit.
5. **Les 7‑8 CHAMPS** (Fig 5.25a, 5.26) : arc principal, rosace, sous‑arc G,
   sous‑arc D, **et 4 FILLETS** (les régions résiduelles entre rosace/sous‑arcs/
   arc principal). Chaque champ est **rétréci (offset) de `bdInner`** → il reste
   un **plan‑bordure contigu** = la pierre du remplage ; les champs rétrécis = les
   **vides** (verre).
6. **Fillets = reste du plan quand on retire rosace + sous‑arcs** (p.254) :
   > « the fillets are only the remains of the front plane when the circle and the
   > sub‑arches are removed. So the solution would be to use a **2D‑CSG algorithm
   > that can handle curves made not only of line segments but also of circle
   > segments. This is planned in the future. »

   ⇒ **La thèse elle‑même désigne le 2D‑CSG (booléen polygonal) comme LA bonne
   méthode, qu'elle n'avait pas.** Nous l'avons maintenant : **Clipper2**.
7. **Rosace à foils** (Fig 5.27, 5.29) : `n` foils **ronds** (`r =
   sin(α/2)/(1+sin(α/2))`, `α=2π/n`) ou **pointus** (déplacement des centres le
   long de `(a,m),(b,m)`, « pointedness »). Rosaces **couchée/debout**. Les foils
   forment eux‑mêmes un **plan‑bordure connecté** (mêmes offsets).
8. **Arc trilobé pointu** (Fig 5.27 2c‑d) : on scinde chaque demi‑arc et on
   remplace la partie basse par **deux arcs plus petits** tangents (continuité C1
   par la règle « centre du prochain arc sur la droite centre→extrémité »).
9. **Profils 3D** (Fig 5.28, 5.29, p.244) : la 3D vient de **profils balayés le
   long des bords de champ**, plan du profil ⟂ tangente ; **aux coins → plan
   bissecteur**. Deux styles : **flat** (les faces avant forment un plan contigu,
   les champs sont des anneaux) et **French** (barres rondes). Appui de fenêtre
   (windowsill) par extrusion de chemin.
10. **Récursion** (p.240, 246, 254) : les sous‑arcs **sont** des arcs brisés →
    **style récursif**. Profondeur 2 → 2·2·2 = 8 sous‑fenêtres. Règles
    **harmoniques** : `arcDown`, `wallSetback` et l'échelle du profil sont **÷2 à
    chaque niveau** ; le **nombre de foils reste constant** entre niveaux.
11. **Séparation contenu / apparence** (Fig 5.30‑5.33) : la géométrie (champs) est
    fixe ; le **Style** = 4 fonctions décorant les 4 types de champ (main‑arch,
    fillet, rosette, sub‑arch). C'est ce qui donne styles 1‑8 (Fig 5.40).

---

## Partie 2 — Ce qui est déjà fait dans cgmath / cgmesh

**cgmath (`architecture_gothic.{h,cpp}`) — primitives 2D, complètes :**
- `buildArch` (excess), `buildArchOffset` (centres fixes ✓), `buildSubwindows`
  (arcDown, gap), `buildRosette` (ellipse ✓), `buildFoilRing` (ronds+pointus,
  formule exacte ✓), `inscribedCircleOfPointedArch`, `buildTrefoilArch`
  (arc trilobé ✓), `buildFillets` (version « offset de cercles explicites »),
  `buildMouchettes`.

**cgmesh :**
- `architecture_gothic.{h,cpp}` : `buildBayStonePolygon` (outer + trous),
  `tessellateToMesh`, `extrudeToMesh` (**sommets caps/parois séparés** → normales
  correctes ✓), `appendMesh`.
- **Machinerie de profils DÉJÀ présente** : `sweepProfileAlongArc(s)` +
  profils `rectangularProfile` / `chamferProfile` / `cavettoProfile` — mais
  utilisée seulement pour des tubes autonomes, **pas** pour la fenêtre.
- **Clipper2 intégré** (`extern/clipper2`, moteur booléen ; offset/rectclip/
  triangulation présents mais hors build).
- Rosace en **bar tracery via Clipper2** (`buildRosetteStone` : anneau + rayons +
  moyeu + pétales via `Union`/`Difference`) ✓.
- **Corps vertical** (`archWithBody`, param « Body height ») → fenêtres hautes ✓.
- **Récursion** crude (`collectUnitVoids`, param « Recursion » 0‑2) : chaque
  lancette → sous‑lancettes + petite rosace, `Difference(ouverture, sous‑vides)`.

**maker UI :** excess, width, body height, offset out/in, lancets, recursion,
drop, lancet excess, gap, rosette (+foils : count/type/pointedness), extrusion.

**Correctifs déjà faits** : hang rosette (cap + `-fexceptions`), trous quads
(`GetTriangles`→`BuildTriangulation`), Klein, débordement glutess (tampons
extensibles), débordement rosace (shrink‑fit), normales (split verts + bascule
`updateInPlace` au bootstrap), orientation foils (`phi0=π/2`).

---

## Partie 3 — Écarts avec la référence (ce qui reste)

Constat visuel (rec=1/2 vs Fig 5.22 rang 3 / image de droite de `gothic1.png`) :
**trop de pierre, pas assez de vide ; barres trop épaisses ; pas de relief ;
lancettes non festonnées ; fillets absents.** Décomposé :

| # | Écart | Cause | Pièce manquante |
|---|-------|-------|-----------------|
| **A** | Construction **ad‑hoc** (on découpe l'arc de lancette + la rosace séparément) au lieu du **plan‑bordure par champs** | pas de pipeline « champs offset » unifié | refonte field‑based CSG (Clipper2) |
| **B** | **Fillets absents** (les 4 champs d'angle entre rosace/sous‑arcs/arc) | jamais rendus ; `buildFillets` fragile (dégénérescences, cf. p.254) | fillets = `mainArch − rosace − sous‑arcs` via **Clipper2** (méthode que la thèse voulait) |
| **C** | **Rendu « plaque plate »** (emporte‑pièce) | extrusion droite, pas de profils | **profils balayés** le long des bords (flat/French) — machinerie déjà là |
| **D** | **Têtes de lancettes non festonnées** | `buildTrefoilArch` non branché | intégrer l'arc trilobé pointu (Fig 5.27 2c‑d) |
| **E** | **Barres trop épaisses / largeur non maîtrisée** | offsets ad‑hoc, pas de `bdInner` uniforme | `bdInner`/`bdOuter` appliqués par offset de champ (InflatePaths) |
| **F** | **Récursion crude** (dégénère à depth 2, pas de fillets/profils/règles harmoniques) | récursion sur les vides seulement | récurser la **construction de champs** + règles ÷2/niveau |
| **G** | Pas de **séparation style/contenu**, pas de windowsill/gable | hors périmètre initial | optionnel |

---

## Partie 4 — Nouveau plan d'implémentation (phasé)

Idée directrice : **basculer d'un découpage de trous vers une construction par
CHAMPS + CSG Clipper2**, exactement le modèle de la thèse (que Clipper2 débloque),
puis ajouter le **relief par profils** (la vraie 3D), puis les **têtes cuspées** et
enfin **propager le tout dans la récursion**.

### Phase 1 — Pipeline « champs » (2D‑CSG Clipper2) — *fondation, gros gain*
Remplacer le cœur de `buildBayStonePolygon` par :
1. `region = ` intérieur de l'arc principal (contour `buildArch`).
2. Champs bruts : `rosetteDisk`, `subArchL`, `subArchR` (contours cgmath), puis
   **`fillets = region − rosetteDisk − subArchL − subArchR`** (Clipper2
   `Difference`) → règle exactement le point **B** (et §p.254).
3. **Offset des champs** : rétrécir chaque champ de `bdInner` (et l'ensemble de
   `bdOuter` depuis l'arc) via **`InflatePaths`** (delta négatif).
   ⇒ **ré‑ajouter `clipper.offset.cpp` au build** (c'est le cas d'usage prévu).
4. `openings = Union(champs rétrécis)` ; `stone2D = Difference(region, openings)`.
5. Tesséler `stone2D` (GLU actuel, ou `TriangulatePaths` de Clipper2 en option).

Livrable : plan‑bordure correct, **fillets présents**, **largeur de barres =
`bdInner`** maîtrisée (point **E**), une seule passe robuste. La rosace foliée
(`buildRosetteStone`) devient un champ parmi les autres.
Risques : orientation des paths (PolyTree) → conversion outer/hole par parité de
profondeur ; garder le fallback placeholder.
Validation : stress test existant + visuel (fillets visibles, barres fines).

### Phase 2 — Profils 3D (le relief) — *plus gros gain visuel*
Au lieu d'`extrudeToMesh` (extrusion droite), **balayer un profil le long des
bords de champ** :
1. Pour chaque contour de `stone2D` (bords d'ouverture), échantillonner la courbe
   (arcs + lignes) et **balayer `chamferProfile`/`cavettoProfile`** avec
   `sweepProfileAlongArc(s)` (déjà présent).
2. **Coins → plan bissecteur** (règle p.244) : au raccord de deux arcs, continuer
   le profil sur la bissectrice.
3. Deux styles : **flat** (chanfrein → plan avant contigu, champs = anneaux) et
   **French** (profil rond/tore). Param UI « Profile » (None/Chamfer/Roll).
4. Échelle du profil = f(profondeur d'extrusion) pour garder les proportions.

Livrable : passage « galette » → **pierre taillée**. Réutilise la machinerie
existante ; le travail = générer les polylignes de bord depuis `stone2D` et gérer
les coins. Risques : auto‑intersections du sweep sur courbes concaves →
`extrudestable`/straight‑skeleton non dispo ; commencer par chanfrein faible.

### Phase 3 — Têtes de lancettes festonnées (trilobe pointu)
1. Brancher `buildTrefoilArch` (cgmath, déjà là) : remplacer le contour d'un
   sous‑arc par l'arc trilobé (Fig 5.27 2c‑d) quand « Lancet head = Cusped ».
2. Alternative n‑foils : foliation de la tête via champ folié (comme la rosace)
   avec `inscribedCircleOfPointedArch`.
Param UI « Lancet head » (Plain / Trefoil / Foiled).

### Phase 4 — Récursion fidèle
1. Faire récurser la **construction de champs** (Phase 1) et non les vides seuls :
   chaque sous‑arc → mini‑fenêtre **avec ses fillets + rosace + profils**.
2. **Règles harmoniques** (p.254) : `arcDown`, `bdInner/bdOuter` et échelle de
   profil **÷2 par niveau** ; **nombre de foils constant**.
3. Corriger la dégénérescence depth 2 : clamps déjà là ; avec des barres plus
   fines (Phase 1) les sous‑arches restent constructibles plus longtemps.
Param « Recursion » déjà présent.

### Phase 5 (option) — Style & finitions
- **Séparation contenu/apparence** : structurer en 4 « fonctions de champ »
  (main‑arch, fillet, rosette, sub‑arch) → styles interchangeables (Fig 5.40).
- **Windowsill** (appui) par extrusion de chemin ; **gable/canopy** (Fig 5.43)
  hors périmètre app.
- **Foils pointus** exacts (déplacement `m′,m″`) déjà dans cgmath → exposer.

---

## Ordre recommandé & effort

1. **Phase 1** (field‑CSG + fillets) — *fondation, M+*. Débloque B, E, A.
2. **Phase 2** (profils) — *plus gros gain visuel, M+*. Débloque C.
3. **Phase 3** (têtes cuspées) — *S+*. Débloque D.
4. **Phase 4** (récursion fidèle) — *M*. Débloque F.
5. **Phase 5** (styles/finitions) — *S/L*, optionnel. G.

Note dépendance build : **Phase 1 nécessite `clipper.offset.cpp`** (InflatePaths)
→ le remettre dans `CLIPPER2_SRC_FILES` (`src/cgmesh/CMakeLists.txt`). Le reste de
Clipper2 (rectclip) reste hors build ; `triangulation` seulement si on remplace GLU.

Tout reste **borné et rattrapable** : la géométrie dégénérée lève (rattrapé par le
`try/catch` → placeholder) ; Clipper2 est robuste aux auto‑intersections ; garder
le stress test (`test_envelope.mjs`) + revalidation native à chaque phase.
