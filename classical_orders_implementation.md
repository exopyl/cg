# Cinq ordres de Vignole — plan d'exécution et journal

> **Date** : 2026-11-04. **Périmètre** : `src/cgarchi/classical_*`, `src/cgarchi/parameterized_classical.*`,
> catalogue réuni de `cggraph_nodes`, page `maker/web/orders.html`.
> **Source domaine** : `Architecture/Antique/cinq_ordres.json` (version 2026-11-04), documentée en §9.0 de
> `cinq_ordres.html` ; copie à l'identique dans `src/cgarchi/data/cinq_ordres.json`.
> **Conception** : plan validé par l'architecte (décisions D1 à D6, tests T1 à T8), repris ici en bref.
> **Dette** : `debt_cgarchi.md` §2.2.

Statuts : **[M]** mesuré (instrument nommé), **[V]** vérifié par lecture, **[E]** estimé (méthode
nommée), **[H]** hypothèse.

---

## 1. Décisions (rappel)

| | Décision |
|---|---|
| D1 | JSON copié à l'identique, embarqué par une source C++ générée (`embed_file.cmake`, tableau d'octets : MSVC refuse un littéral de plus de 64 Ko), lu une fois par nlohmann (`ClassicalOrders ()`). Chaque nombre est lu par son champ exact `f`. |
| D2 | API pure testable (`classical_orders`, `classical_diminution`, `classical_profile`, `classical_lathe`), séparée de la classe `IParameterized`. |
| D3 | `cgarchi::ClassicalParametricShapes ()` réunie dans `cggraph_nodes::CatalogShapes ()` après cgmesh et le gothique ; liaison `shape.classical.column` ; catégorie `"classical"` dans `maker/wasm_api.cpp`, sans Extrude. |
| D4 | Page `maker/web/orders.html` + `js/pages/orders.js`, carte dans `index.html`, deep-link `orders.html?shape=Classical%20Column&Order=3`. |
| D5 | Maillage unique « tour sur plan » : un anneau par sommet du profil, fonction θ → rayon selon le plan ; fermé par construction. |
| D6 | Hors périmètre v1 : ornements (volutes, acanthe, caulicoles, fleurons, triglyphes, métopes, gouttes, denticules, mutules, modillons), cannelures, entablement filé, arcades, colonne torse. |

## 2. Étapes

| Étape | Contenu | Tests |
|---|---|---|
| E1 | Données embarquées, modèle typé, lecture stricte, `RecomputeControls` | T1 (2 cas : reproduction, divergence) + lecture (4 cas : données, variantes, refus, nombres exacts) |
| E2 | Diminution : cosinus (méthode 1), conchoïde (méthode 2), `SolveConchoidCD` | T3, T4 |
| E3 | Tracé des moulures, assemblage du profil | T2, T5 (dont le refus d'un tracé qui sort de sa boîte) |
| E4 | Anneaux, `RingCrossings`, `FindPlanConflicts` | T8, garde du carré concave |
| E5 | Tour sur plan, `ParameterizedClassicalColumn`, catalogue, liaison de nœud | T6, T7 |
| E6 | Page maker, build WASM | taille gzip avant/après |

Tests : `test/tu_cgarchi_classical_orders.cpp` (27 cas, dont un labellisé `slow`),
`test/tu_cgarchi_classical_volute.cpp` (20 cas, volutes : §7),
`test/tu_cgarchi_classical_flutes.cpp` (17 cas, dont un labellisé `slow`, congés et cannelures : §8),
`test/tu_cgarchi_classical_colonnade.cpp` (13 cas, colonnade et ornements : §9) et
`test/tu_cggraph_nodes_shapes.cpp` (`the_classical_column_node_wraps_the_catalogue_shape`, tailles de la
réunion). Chaque famille a un cas négatif qui doit pouvoir échouer.

## 3. Lecture des données — variantes recensées

Instrument **[M]** : `keys.py`, `plans.py`, `cyma.py` (Python `-I`, scratchpad de la session) sur la
version 2026-11-04 ; 239 membres, 36 blocs.

- `distance_axe` et `plan` présents sur les 36 blocs ; 24 membres surchargent le plan.
- Saillies : `{s}` (127 : 78 listels, 49 faces ; plus 6 ornements), `{s_bas, s_haut}` (talon 25, ove 12,
  doucine 7, cavet 3, congé 1), tore `{attache, s_max}` (36), scotie `{s_haut, fond, s_bas}` (6),
  deux coupes `{diagonale, face}` (5 : face 2, listel 1, talon 2), `{noyau, enveloppe}` (11). Aucune
  saillie nulle.
- Plans : `revolution`, `carre`, `carre_concave`, `oriente_ionique`, `lineaire`.
- **Variante non couverte par le plan** : `carre_concave` à coupe unique — canal de la volute
  composite, saillie `{s: 13/2}`. Lue comme « même saillie dans les deux coupes », ce qui donne
  exactement le cercle de rayon `distance_axe + s` (l'arc passant par les deux cornes et le milieu de
  face est alors centré sur l'axe).
- **Ornement à saillie unique** `{s}` (denticules, mutules, modillons, blocs à pomme de pin) : segment
  vertical, comme un listel.
- **Talons à diagonale de moins de 30°** (corinthien piédestal 29,7°, corinthien corniche 23,2°,
  coupe diagonale de la cimaise composite 29,7°) : avec deux arcs de 60°, le tracé n'est pas monotone
  en y. Les deux coupes d'un membre sont donc appariées par affinité horizontale, pas par hauteur.
  Ces talons restent dans leur boîte [0, h], coupes diagonale et face comprises **[M]** test
  `a_flat_box_that_pushes_the_arcs_out_is_refused` (34 doucines et talons). Une **doucine** à moins
  de 30° sortirait de sa boîte (arc convexe sous le pied) ; aucune dans les données, et `TraceMember`
  refuse tout tracé qui sort de [0, h].

La lecture refuse toute forme, tout plan ou tout jeu de clés de saillie hors de cette liste
(`an_unknown_projection_key_set_is_refused`).

## 4. Géométrie retenue

- Assemblage de bas en haut : piédestal, base sans ses membres hors module, membres hors module, fût
  `r(y)` (y depuis le sommet de la base) jusqu'à `H − h(fut_haut)`, `fut_haut`, chapiteau, architrave,
  frise, une corniche. `H = fut_M × P` contient les membres hors module et `fut_haut` : la colonne
  retombe sur `colonne_M` (T2, 48 configurations).
- Plans → anneaux : `revolution` → cercle ; `carre`, `lineaire`, `oriente_ionique` → carré ;
  `carre_concave` (deux coupes) → carré concave, chaque face étant l'arc par ses deux cornes et son
  milieu (Vignole : arc centré au sommet du triangle équilatéral construit sur la face ; vérifié sur
  la cimaise de l'abaque corinthien, rayon de l'arc 50,944 pour une face de 36√2 = 50,912 **[M]** calcul
  Python, l'écart venant de l'arrondi de la cote 18,64).
  Validité : une face creusée n'est vue depuis l'axe que si face ≥ (2 − √2)·a, a = corne/√2 (sinon
  le rayon à 45° rencontre l'arc avant la corne) ; `Ring::MakeConcaveSquare` refuse le reste. Les
  faces des données sont entre 0,73 a et 0,78 a, pour un seuil à 0,586 a **[M]** calcul Python sur
  les abaques et cimaises corinthiens et composites.
- **Jonctions croisées** : à une jonction plate, si le rayon du dessous et celui du dessus changent de
  signe de différence avec θ (rond sous carré, par exemple), le tour ajoute les angles de croisement à
  l'échantillonnage et soude les sommets confondus. L'anneau plat devient la différence symétrique
  exacte des deux sections, orientée vers le bas là où le dessus déborde et vers le haut ailleurs. Sans
  ce découpage, `ValidateMesh` relève des auto-intersections (négatif de T6).

## 5. Journal de prédictions

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| E1 | Tous les contrôles calculables reproduits | T1 (`RecomputeControls`, tolérance 1e-9 relative) | **[M]** 52 contrôles calculables sur 60, 52 reproduits ; 8 « mesure sur la gravure » non calculables depuis les profils. Négatif : tore toscan h 5 → 6, un seul contrôle diverge (base, 13 au lieu de 12) | aucun | — |
| E2 | CD résolu ≈ 1,0795 (ionique), 1,0796 (corinthien) | T3, `SolveConchoidCD` | **[M]** égal à `CD_calcule` à 5e-5 près pour les deux ; composite : même CD que le corinthien (mêmes proportions). Négatif : CD de la planche (29/27 M) donne r(0) à 0,0599 p de 1 M | aucun | — |
| E4 | 0 conflit de plan | T8, `FindPlanConflicts` | **[M]** 3 jonctions réellement croisées, présentes dans 20 configurations sur 48 : ionique ove (rond 22) / canal de la volute (carré 17), 8 ; corinthien caulicoles noyau (rond 20,5) / abaque (cornes 32, faces 17,6), 4 ; composite canal (rond 21,5) / abaque (cornes 32,5, faces 17,73), 8. Un contact sans croisement (toscan, ove rond 13,5 / abaque carré 13,5) | 3 jonctions au lieu de 0 | ionique : `oriente_ionique` rendu carré (en réalité l'échine déborde en façade entre les volutes) ; corinthien : le noyau des caulicoles est sous l'abaque, que seule l'enveloppe atteint ; composite : canal à coupe unique lu comme un cercle. Traitées par le maillage (§4), signalées, non masquées |
| E5 | < 10 000 triangles par défaut ; Regenerate ~ ms | `GetNFaces`, `std::chrono` (test `the_parameterized_column_follows_its_parameters`, **build Debug MSVC**) | **[M]** défaut (toscan, colonne seule, 48 × 6) : 5 752 triangles, 2 878 sommets. Configuration la plus lourde (composite, piédestal, entablement) : 29 552. Corinthien complet : 27 872 triangles, Regenerate 16,5 ms en Debug ; sous WASM (Node, §6), Regenerate + sérialisation OBJ : 14,8 ms (défaut), 31,6 ms (corinthien complet) | aucun pour le défaut | Release natif non mesuré |
| E6 | maker.wasm gzip +25 à 40 Ko | `gzip -c maker/web/maker.wasm \| wc -c` avant/après | **[M]** 1 129 162 → 1 189 422 o, **+60 260 o** (+58,8 Kio) ; brut 3 410 277 → 3 787 124 o (+376 847) | +20 Ko au-delà de la borne haute | Le JSON seul pèse 28 319 o gzip **[M]** ; le code (lecture nlohmann, tracé, tour, conchoïde) compte pour le reste, ≈ 32 Ko par différence **[E]** (les tailles compressées ne s'additionnent pas exactement). La prédiction n'avait pas séparé les deux parts |
| VE2 | 9/8/7/6 exacts ; cassure Axis 4,39° ; fin 0,971 | `tu_cgarchi_classical_volute`, scripts de l'architecte (`vol/v12.py`) | **[M]** extrêmes 9/8/7/6 et passage du 4e arc en (−½, 5) à 1e-12 ; cassure G1 < 1e-9 rad, Axis 4,393° ; fin extérieure 0,9709 ; listel min 0,0078, canal min 1,0403 (Python 0,008 et 1,0407) | aucun sur les valeurs | Le négatif prévu (centres intérieurs vers l'extérieur ⇒ canal −0,345) n'est pas constructible : dès −a1/48, des rayons intérieurs deviennent négatifs (−0,41 et −0,83 aux arcs 11 et 12 pour −a1/12, que le script enchaîne sans contrôle). Remplacé par le refus de ce décalage (`invalid_argument`) et par deux bords intérieurs décalés radialement, qui font échouer chaque critère |
| VE3 | partition à 1e-9, polygones simples | tests VE3 | **[M]** A(S) 158,80 ; A(R) 51,39 ; A(C) 107,41 ; partition et simplicité tenues à 2, 6 et 32 segments par quart ; entrées dans l'œil 1061,05° et 1055,68° (plan 1060,8° et 1055,4°, échantillonnage différent) | **simplicité perdue à 2 segments par quart avec un échantillonnage par arc** | Risque R1 avéré : flèche de corde 0,076 contre un listel de quelques centièmes. Corrigé à la racine : contour, bord intérieur et œil échantillonnés aux mêmes angles polaires autour de l'œil (grille 2π-périodique + raccords, extrêmes d'axe, points d'entrée), ce qui interdit le croisement des cordes à toute résolution |
| VE4 | coussinet valide du premier coup ; glutess sans sommet créé | `ValidateMesh`, garde de `TessellatePolygon` | **[M]** valide, 1 composante, volume = 2F·A(S) + 2d·A(R) à 1e-4 près, à 2, 6 et 32 segments ; 0 sommet créé ; négatifs : parois non coupées en ±F ⇒ arêtes de bord, nœud papillon ⇒ refus | aucun | — |
| VE6 | +2 650 triangles pour l'ionique ; Regenerate ≤ +2 ms | `GetNFaces`, `std::chrono` (Debug), sonde Node (WASM) | **[M]** 2 220 triangles par coussinet à 6 segments par quart ; ionique 11 632 → 15 848 (+4 216 ; la bande carrée fusionne des anneaux du tour). Regenerate : Debug 6,3 → 29,9 ms ; WASM (Regenerate + OBJ) 8,4 → 15,3 ms | +1 566 triangles ; +6,9 ms en WASM | Les raccords et extrêmes d'axe ajoutés à la grille (nécessaires aux cotes exactes 8/7/6) valent pour chaque tour et chaque courbe ; `ValidateVoluteSection` (simplicité en O(n²)) tourne deux fois par coussinet **[H]**, ce qu'un profilage confirmerait |
| VE6 | wasm gzip +5 à 15 Ko | `gzip -c maker/web/maker.wasm \| wc -c` | **[M]** 1 189 422 → 1 199 550 o, **+10 128 o** ; brut +33 507 | aucun | — |
| Revue | validation de section une seule fois, balayage en O(n log n + k) | sonde de chronométrage (Debug MSVC, meilleur de 7), `BuildIonicVolutes` et une `ValidateVoluteSection` | **[M]** s = 6 : 12,77 → 3,37 ms, validation 4,80 → 0,24 ms (et une fois au lieu de deux) ; s = 32 : 132,23 → 11,96 ms, validation 61,02 → 0,87 ms | — | En WASM (sonde Node, 3 runs), l'ionique avec volutes reste à 14,4–16,1 ms contre 7,2–8,3 ms sans : l'hypothèse « la validation explique le surcoût WASM » est **démentie** ; le reste (tessellation, `appendMesh`, normales, OBJ de 4 216 faces de plus) n'est pas ventilé **[H]** |
| Revue | maker.wasm | `gzip -c \| wc -c` | **[M]** 1 199 550 → 1 203 260 o (+3 710) ; brut 3 820 631 → 3 838 264 | — | garde d'aire de la tessellation, garde du décalage, balayage |
| C1 | hauteurs et contrôles inchangés ; +2 × courbe-segments anneaux par fût | T1, T2, T8, tests C1 | **[M]** T1 52/52 calculables (61 contrôles depuis la mise à jour du JSON : +1 mesure sur gravure), T2 48/48, 3 conflits, volutes inchangées ; toscan par défaut 5 752 → 6 712 triangles (+960 : 10 anneaux nets de 96 triangles, 5 par congé, les deux anneaux plats disparaissant dans les raccords) | aucun | — |
| F1 | surplomb du demi-cercle exact 4,8e-4 (ionique) / 8,3e-4 rad (dorique B) | tests F1 | **[M]** 6,5e-4 rad (ionique), 1,97e-3 rad (dorique B) ; écrêtage : aire −0,10 % / −0,31 % ; au pied ionique corde 3,905, arc 3,912 (39/10), profondeur 1,952 (39/20) | ×1,35 et ×2,4 | Le plan supposait un listel de 0,25 du pas et des cannelures doriques non jointives ; les données donnent 0,17 et des cannelures jointives |
| F2 | ≈120 angles ionique/corinthien, 80–104 dorique (radial 48) | `LatheAngles` | **[M]** ionique 176 (24 × 7 + 8 du croisement ove/canal), dorique 120 | ionique +46 % | 3 sommets par demi-cannelure (moitié de `Curve segments` = 6) au lieu de 2 ou 3 au total |
| F3 | ionique avec volutes et cannelures ≈ 35 000 faces [E ±25 %] | `GetNFaces` | **[M]** 45 496 (17 080 lisse) ; dorique 21 080 (8 064 lisse), A et B de même topologie ; corinthien cannelé 44 240 | +30 %, hors fourchette | Grille plus dense que prévu (F2) et commune à tout le tour (R1) |
| F4 | toutes valides ; passe slow ≤ 2 min Debug [E] | `ValidateMesh` | **[M]** toutes valides (dorique A et B, ionique, corinthien, composite × piédestal × entablement, volutes comprises) ; passe rapide 29,9 s, passe slow 72,5 s | aucun | — |
| F5 | Regenerate WASM ionique ≈ 35 ms [E] ; gzip +3 à 8 Ko | sonde Node (2 runs), `gzip -c \| wc -c` | **[M]** ionique à volutes cannelé 39,9–40,3 ms (Regenerate + OBJ) contre 21,4–23,4 lisse ; dorique A 17,2–18,1 ms, B 17,3–26,9 ms ; gzip 1 203 260 → 1 210 011 (+6 751, JSON agrandi de 7 779 o compris) | +5 ms | OBJ de 28 416 faces de plus compris dans la mesure |
| Revue (cannelures) | paroi d'arête maillée, section à < 5 % de la profondeur et aire à < 1 % à la résolution par défaut | test `the_sampled_sections_follow_the_exact_ones_at_the_default_resolution` (interpolation du rayon entre les angles du tour, hors bande de paroi) ; sonde du relecteur | **[M]** avant (grille uniforme βk/3) : écart radial max 28 % de la profondeur, aire −8,1 % (ionique), 0,66 p et −8,6 % (dorique B), sans convergence. Après (grille curviligne m = 6, méridien de paroi à β(1 − 10⁻²), 2 anneaux de tête) : vif 0,9 % (A), 1,2 % (B), 1,5 % (ionique, corinthien, composite) de la profondeur ; aire 0,57 / 0,65 / 0,67 % ; têtes 2,4 / 7,3 / 7,8 % | — | Le chanfrein venait de la grille : la paroi de l'écrêtage n'avait pas de méridien. Un écart de paroi de 10⁻³ faisait souder les deux méridiens par `ValidateMesh` sur les grandes colonnes (corinthien et composite avec piédestal et entablement : 112 faces dégénérées, 72 arêtes non manifold) : porté à 10⁻² |
| Revue (cannelures) | coût | `GetNFaces`, chrono Debug, sonde Node | **[M]** angles du tour : ionique 176 → 464, dorique A 120 → 240, dorique B 120 → 360 ; ionique à volutes cannelé 45 496 → 112 888 triangles (lisse : 17 080, ×6,6), Regenerate Debug 73 → 116 ms, WASM (avec OBJ) 40 → 76–84 ms ; dorique A 21 080 → 42 200, B 21 080 → 63 320 ; passe slow cannelée 72,5 → 151,7 s, passe rapide (colonne seule) 17,0 s ; gzip 1 210 011 → 1 214 269 (+4 258) | ×2,5 en faces | Précision demandée à la résolution par défaut, sur une grille commune à tout le tour (dette §2.2) |
| O2 | 48/48 OBJ de Classical Column identiques après la refonte | `cmp` (outil `snap` du scratchpad, même build) ; empreinte portable permanente | **[M]** 48/48 identiques, empreintes identiques ; négatif : Curve segments 6 → 7 change l'empreinte | aucun | — |
| O3 | balayage < 600 triangles | `GetNFaces` | **[M]** entablement toscan : 244 triangles (31 sommets de profil, 8(n − 1) + 4) | aucun | — |
| O4 | toscan N = 4 ≈ 27 000 faces ; ionique cannelé à volutes N = 6 ≈ 680 000 | `GetNFaces` | **[M]** 27 092 ; 680 380 ; corinthien N = 4 : 57 508 ; dorique à mutules N = 4 : 79 148 ; pire cas (ionique cannelé, N = 12) : 1 360 540 | aucun | — |
| O4 | Regenerate + OBJ WASM, ionique cannelé N = 6 : 0,4 à 0,6 s | sonde Node | **[M]** 478 ms, OBJ 26,2 Mo ; défaut 15,7 ms, OBJ 0,92 Mo | aucun | — |
| O5 | comptes N = 4 : triglyphes 22, mutules 22, denticules doriques 216, ioniques 144, corinthiens 148, modillons 42, blocs 92 | `PlaceEntablatureOrnaments` | **[M]** identiques ; plus regulae 22, gouttes 132, gouttes du plafond 396, gouttes des mutules 792 | aucun | Les fonds mis à jour (denticules doriques à 4 au lieu de 7/2, mutules à 13/2) ne changent aucun compte |
| O6 | ornements doriques à mutules N = 4 ≈ 25 000 faces ; denticules ≤ 3 000 | `GetNFaces` (avec moins sans ornements) | **[M]** 46 552 | ×1,9 | 924 gouttes à 12 côtés (48 triangles chacune) : 44 000 à elles seules |
| O7 | maker.wasm gzip +8 à +20 Ko | `gzip -c \| wc -c` | **[M]** 1 214 269 → 1 239 747 (+25 478), dont +4 767 pour le JSON compressé (30 928 → 35 695) | +5,5 Ko | Code de la colonnade (placement, coques, balayage) ≈ 20,7 Ko par différence **[E]** |
| Revue (colonnade) | gouttes à 6 côtés au défaut au lieu de 2 × Curve segments | `GetNFaces` (ornements = avec moins sans) | **[M]** dorique à mutules N = 4 : ornements 46 552 → 24 640 triangles (22 listels de mutule ajoutés compris), colonnade 79 148 → 57 236 ; gzip 1 239 747 → 1 240 016 (+269) | — | 924 gouttes à 24 triangles au lieu de 48 |
| Revue (colonnade) | position des ornements | test `every_ornament_sits_against_the_entablature_without_entering_it` (5 cas, toutes familles, 4 faces) | **[M]** aucun point intérieur d'élément dans l'entablement balayé, tous les éléments en appui ; négatif : mutules montées de 3, constat | aucun | — |

T6 : les 48 configurations, à 16 angles × 2 segments de courbe et à la résolution par défaut
48 × 6 (fût 24), sont fermées, d'une composante, de volume positif, sans auto-intersection ni test
tronqué **[M]** `ValidateMesh` (soudure par défaut). Durées en Debug MSVC **[M]** (gtest) : 5,6 s
(16 × 2) et 27,7 s (48 × 6). La seconde passe, seule à échantillonner l'intérieur des talons non
monotones, est labellisée `slow` (`test/slow_tests.cmake`, écartée par `ctest -LE slow`).

## 6. Build WebAssembly

- `maker/build.ps1` (preset `maker-wasm`, emsdk `C:\home\dev\extern\emsdk`) : succès, aucun
  avertissement dans les unités `classical_*`, `parameterized_classical`, `shape_catalog`, `wasm_api`
  **[M]** journal de build.
- Référence « avant » : l'artefact `maker/web/maker.wasm` du dépôt (11:03), aucune source `.cpp`, `.h`
  ni `CMakeLists.txt` de `src/`, `maker/`, `extern/` plus récente **[M]** `find -newer` ; mêmes options
  de compilation que la build « après » **[H]** (le preset est inchangé).
- Sonde Node du module (`probe_orders.mjs`, scratchpad de la session, `fetch` servi depuis le disque) :
  `listShapes("classical")` = `[{"name":"Classical Column","group":""}]`, absente de `"parametric"`,
  catégorie inconnue → `[]`, 9 paramètres dans l'ordre déclaré (dont `Volutes`) ; toscan 14 unités de haut, 5 752
  faces, 14,8 ms (Regenerate + OBJ) ; corinthien avec piédestal et entablement 32 unités, 27 872 faces,
  31,6 ms **[M]**.
- Même sonde, ionique : avec volutes, largeur en X 2,8889 = 52/18, 15 848 faces, 15,3 ms ; sans
  volutes, 2,7778 (plinthe de 50 parties), 11 632 faces, 8,4 ms **[M]**.
- Page `orders.html` non ouverte dans un navigateur : rendu o3dv, deep-link et panneau non vérifiés
  visuellement **[H]** (même chemin que `shapes.html` : `createShell`, `listShapes`, `createShape`).

## 7. Volutes ioniques (VE1 à VE6)

Plan de l'architecte : `plan_volutes.md` (scratchpad de la session) ; référence : `cinq_ordres.html`
§5.7 et §4.3 ; données `ordres.ionique.ornements.volute` (valeurs en chaînes).

- **VE1** `OrderData::volute` (`IonicVoluteData`), lu strictement : une clé absente donne une erreur
  qui nomme `ornements/volute/<clé>`. Les erreurs de champ manquant nomment désormais toutes le chemin
  complet du champ (`.../<clé> : champ manquant`). Relations vérifiées : 16 = 9 + 7, 14 = 8 + 6,
  36 = 2 × 18, 3 + 9 = 12 = hauteur du chapiteau.
- **VE2** `classical_volute` : spirale à douze centres, raccord G1 sur la ligne des centres (défaut) ou
  sur l'axe du centre (section 5.7, gardé comme négatif calibré). Listel aux centres a'j = aj − a1/12,
  prolongé par un segment horizontal jusqu'à la cathète.
- **VE3** Section : S = premier tour fermé par u = −½ ; R = listel ∪ œil ; C = S ∖ R. Avec le raccord
  G1, le 4e arc ne s'arrête pas en (−½, 5) mais en (0,578 ; 4,894) : (−½, 5) en est un point
  intérieur, à la verticale de son centre, et la fermeture coupe l'arc à cet angle.
- **VE4** `classical_ionic_capital` : prisme étagé fermé (C en ±F, R en ±(F + d), F = 17,5, d = 2),
  parois coupées en ±F. La section est validée une fois, par `BuildVoluteSection` ; `BuildBolster` ne
  refait que les contrôles linéaires (partition des aires, bords), et `TessellatePolygon` refuse une
  triangulation qui crée un sommet, rend un triangle d'aire non positive ou ne couvre pas l'aire du
  polygone à 1e-9 près (le tessellateur signale ses erreurs sans les rendre).
- **VE5** Placement : coussinets sur ±X, faces en Y = ±F, œil à X = ±18 et au bas du chapiteau ;
  emprise X ∈ ±[12, 26], Y ∈ [−19,5 ; 19,5], Z ∈ [Z_œil − 7, Z_œil + 9]. Avec `ionicVolutes`, la
  bande (listel et canal, `oriente_ionique`) prend le carré de demi-côté F. Le conflit ionique (ove de
  rayon 22 sous la bande) reste une jonction croisée du tour ; les tests montrent que sur ±X l'excès
  de l'ove est noyé dans le coussinet et que sur ±Y il déborde entre les volutes.
  L'assemblage (tour + deux coussinets) n'est pas une variété unique : coques fermées concaténées
  (`appendMesh`), contrôlées partie par partie.
- **VE6** Paramètre `Volutes` (défaut vrai, sans effet hors de l'ionique) ; `Curve segments` sert de
  segments par quart d'arc. Toscan par défaut inchangé (5 752 triangles). Ionique à volutes : largeur
  en X 52/18 M (module 1), faces = tour + deux coussinets.

Image de contrôle (session) : rendu logiciel de l'OBJ exporté par la sonde Node, vues de face et de
trois quarts du chapiteau ionique.

**Incohérences des données, consignées sans corriger le JSON** (section 9 du plan) :

1. Listel (9/4 → demi-côté 17,25) et canal (2 → 17) du bloc chapiteau ≠ F = 17,5 (`faces_des_volutes_en_plan`
   / 2) ; leur relief de 0,25 au profil contre `listel_face` = 2 **[V]** données.
2. Section 5.7 : « le douzième arc s'achève à 1,01 du centre » : 1,009 est la hauteur du point d'arrivée,
   sa distance au centre est 1,023 (raccord Axis) **[M]** `vol/v12.py`.
3. Le tracé à douze centres recalculé en 5.7 n'est pas G1 : cassures de 2,05° et 4,39° **[M]** test VE2.
4. `distances_au_centre` 8/7/6 ne valent que pour les douze centres ; la construction BCD donne 7,90 /
   6,95 / 6,11 et une hauteur de 15,965 **[M]** `vol/bcd.py` de l'architecte, non recalculé ici.

## 8. Congés du fût et cannelures (C0, C1, F1 à F5)

Plan de l'architecte : `plan_cannelures.md` (scratchpad de la session) ; données mises à jour le
2026-11-04 à 23:07 (`conventions.conges_du_fut`, `ordres.*.ornements.cannelures`), recopiées à
l'identique (`cmp`).

- **C0** Lecture stricte : `OrdersData::shaftFilletRule`, `OrderData::flutes` (`FluteData` : nombre,
  listels, `au_pied`, sections). Le schéma diffère selon les ordres : `listels` booléen au dorique,
  `separees_par` et `fond` ailleurs ; les deux sont lus. `au_pied.pas` est un texte (« π·36/24 ≈ 4,71 ») :
  le pas est recalculé (π D / N) et seule la cohérence de `au_pied` est testée.
- **C1** Congés : quart de cercle dont l'écart au vif local vaut ρ (1 − sin φ) en bas et ρ (1 − cos φ)
  en haut, ρ = rayon du listel raccordé − r(y) du vif à la jonction ; tangent au listel (horizontale)
  et au vif diminué. Pris dans fut_M : hauteurs inchangées. Rayons mesurés (pied / haut, parties) :
  toscan 1,5 / 0,43 ; dorique 1,5 / 1,45 ; ionique 2,29 / 1,40 ; corinthien et composite 1,96 / 1,16
  et 1,41. Le toscan a bien un listel de pied (B · imo scapo, 3/2).
- **F1** Section (`classical_flutes`) : cannelures centrées en 2πj/N, demi-angle β = (π/N)(1 − λ),
  λ = listel / pas lu sur `au_pied` (0,1698) ; dorique jointif, β = π/20. A : arc de rayon = corde,
  centre du côté opposé à l'axe (cannelure creuse, flèche 0,134 corde). B et ordres à listels :
  demi-cercle exact sur la corde, écrêté sous l'arête (convention, §B option 2 du plan ; dette
  `debt_cgarchi.md` §2.2).
- **F2** Grille du tour : pour chaque cannelure, son centre, m sommets uniformes en abscisse curviligne
  sur l'arc de coupe (du fond au pied de l'arc), l'arête, et pour une section à paroi (demi-cercle) un
  méridien à β(1 − 10⁻²) qui porte la paroi ; idem pour l'arête de chaque anneau de tête ; puis les
  angles kπ/4 et les croisements, sans jamais remplacer un angle de la grille (`MergeAngles`, même
  tolérance de 10⁻⁹ rad que l'arête de `FlutedRadius`). m = max(6, Curve segments,
  ⌈Radial segments / 2N⌉).
- **F3** Profil : anneau rond puis anneau cannelé au sommet du congé inférieur (arrêt droit, face
  plane) ; vif cannelé ; tête en demi-cercle de rayon R β (développé, R au pied du congé supérieur),
  sommet au pied du congé ; deux anneaux de tête, de demi-angles pris parmi les sommets de l'arc, à la
  hauteur où la tête a cette demi-largeur.
- **F5** Paramètres `Flutes` (décoché par défaut, `facultatives`) et `Doric flutes` (A par défaut).

Défaut révélé par un test, et non par `ValidateMesh` : la tolérance d'arête de `FlutedRadius` manquait
à un premier passage (une commande enchaînée avait échoué avant l'écriture), et une partie des arêtes
retombait à R cos 2β. Les maillages restaient valides ; le test de convergence d'aire, puis un test
direct de toutes les arêtes, l'ont détecté.

**Anomalies et ambiguïtés des données :**

1. Dorique A : « centre … vers l'axe » (JSON et HTML §4.2, « côté axe ») donne, pris à la lettre, un
   arc convexe ; le module creuse la cannelure (à faire trancher).
2. Dorique B : des demi-cercles jointifs sur la corde se recoupent sous l'arête (R cos 2β = 0,951 R) ;
   « arêtes vives » sur le cercle du fût et « jointives » ne peuvent être exactes ensemble.
3. Le HTML (dorique, pl. XII) grave un congé « de rayon 2 », la règle (saillie du listel) donne 1½.
4. Ionique : congé de 2,4 p mesuré (pl. XVIII) contre 2,29 p par la règle sur le vif local (2,33 sur le
   vif nominal).
5. `controles` : 61 entrées, la nouvelle (ionique, pas des cannelures) insérée en 34 ; la clé
   `version` n'a pas changé (2026-11-04) alors que le contenu a changé.
6. Schéma des cannelures hétérogène (`listels` booléen contre `separees_par`) ; `au_pied.pas` textuel.

Images de contrôle (session) : `flutes_img/` du scratchpad, vues d'ensemble et gros plans têtes / pied
du dorique A, du dorique B et de l'ionique cannelé à volutes.

## 9. Colonnade, entablement filé et ornements d'entablement (O0 à O8)

Plan de l'architecte : `plan_colonnade.md` (scratchpad de la session) ; données du 2026-11-05 22:35
recopiées à l'identique (`cmp`), clé `version` inchangée.

- **O0** Instrument : OBJ des 48 configurations de Classical Column pris après la recopie du JSON,
  avant la refonte (outil `snap`, scratchpad).
- **O1** Lecture stricte des ornements d'entablement (`EntablatureOrnaments`) et de
  `face_arriere_entablement.cas_colonnade_isolee.coupe_symetrique`. Valeurs textuelles lues par
  repère (« rangs de », « au pas de », « du bas », « 2 2/5 en profondeur », « entre 6 et 14 ») avec
  erreur nommant la clé. Lien ornement → membre : table en code (`EntablatureOrnamentMember`), refus
  si le membre est absent, ambigu, ou n'est pas un ornement à saillie {s}. Contrôles de cohérence lus :
  Σ divisions = 12, triglyphe + métope = entraxe, 6 × 2 = 12, largeur + caisson = entraxe.
- **O2** Refonte : `BuildClassicalColumnMesh` (extrait de `Regenerate`), `BuildEntablatureProfile`
  (assemblage de l'entablement partagé). Empreinte portable permanente des 48 configurations.
- **O3** `SweepEntablature` : rectangle [−L − d, L + d] × [−d, d] par sommet de profil, coupe
  symétrique. Volume exact `SweptVolume` (sommets float : 1e-6 relatif, et non 1e-9).
- **O4** « Classical Colonnade » : N copies de la colonne (sans entablement) aux axes
  (entraxe = entrecolonnement + diamètre inférieur : 80, 90, 117, 120, 120 p), entablement balayé
  posé sur le chapiteau ; paramètres Columns (1–12, 4), Ornaments (coché), et ceux de la colonne.
- **O5** Placement : rangées de période p et de largeur w sur les quatre faces, ancrées sur l'axe de la
  première colonne (triglyphes, mutules, modillons, denticules doriques et corinthiens) ou sur X = 0
  (denticules ioniques, blocs composites ; convention), règle d'angle sur les plans de fond.
- **O6** Coques par famille (prismes à section monotone triangulée en bandes, troncs de cône à
  max(6, Curve segments) côtés), validées famille par famille ; position contrôlée contre
  l'entablement balayé (pas d'intérieur dans le solide, appui au-dessus ou sur l'hôte). Sous chaque
  mutule, un listel de 1/2 (`mutules.gouttes` : « h 3/2 sous un listel de 1/2 ») porte les gouttes.
- Lecture : angle des canaux pleins (2 atan((w/2)/p) = 90°), hauteurs du triglyphe (= frise), de son
  chapiteau (= membre du bas de chaque corniche), des denticules, mutules et blocs (= membre lié)
  vérifiés ; une fonction de lecture et une de placement par famille.
- **O7** maker : catégorie `classical` à deux formes, binding `shapeWarnings`, bannière au-delà de
  300 000 faces.

Défaut révélé en cours d'étape : à l'échelle du module (1/12), le tessellateur rendait un triangle plat
dans la section du triglyphe (sommets alignés sur la face) ; la garde d'aire de `TessellatePolygon`
l'a refusé. Les sections étant monotones, elles sont désormais triangulées en bandes, sans tessellateur.

**Anomalies et conventions de données :**

1. Canaux du triglyphe aussi profonds (1) que sa saillie sur la métope (1) : le fond des V touche la
   métope ; dos du prisme engagé dans la frise.
2. `mutules.plan` : « arrière vers 13/2 », limite non lue exactement ; retenue telle quelle.
3. `plafond_corniche_denticules` : hauteur et petite base des gouttes non données (3/2 et 3/4 des
   gouttes du triglyphe retenus).
4. Alignement des denticules ioniques et des blocs composites non donné : ancrage au centre.
5. `version` inchangée malgré le changement de contenu (dette §2.2).
6. `canaux_detail` : les demi-canaux (largeur 1, `profondeur_demi_canal` 1/2) impliquent un V à
   126,9°, contre « angle 90° » ; un V à 90° coupé à son axe aurait 1 de profondeur au bord. Le
   contrôle d'angle ne porte que sur les canaux pleins (cohérents) ; les demi-canaux suivent la
   profondeur donnée. À faire trancher.

Images de contrôle (session) : `colonnade_img/` du scratchpad.

## 10. Registre des conventions prises sans appui des sources

Une convention de ce registre est un choix du module, et non une valeur ou une règle de
`cinq_ordres.json` ou de `cinq_ordres.html`. Les conventions que les données portent elles-mêmes (statut
« convention » dans le JSON) n'y figurent pas. Chacune est à confirmer ou à remplacer par l'auteur du
relevé ; la colonne « Écart » borne son effet quand il est connu.

### 10.1 Tracé du profil

| # | Convention du module | Ce que disent les sources | Écart | Où |
|---|---|---|---|---|
| P1 | **Tore** tracé en demi-ellipse de demi-axes h/2 (vertical) et s_max − attache (horizontal) | `conventions.formes.tore` : « demi-cercle de diamètre h » | identique quand s_max − attache = h/2 ; 8 membres diffèrent, pire cas les astragales corinthiennes (profondeur ½ pour h ½, rapport 2) ; un demi-cercle strict violerait s_max ou h | `classical_profile.cpp` (`TraceMember`, `Torus`) |
| P2 | **Congé** (forme `conge`) tracé comme le cavet : quart d'ellipse de la boîte [s_bas, s_haut] × h | `conventions.formes.conge` : « quart de cercle concave » | identique quand la boîte est carrée | `classical_profile.cpp` (`TraceStandard`) |
| P3 | **Diminution** : H = fut_M·P, origine au sommet de la base ; le listel hors module est dans fut_M | §9.0 du HTML ambigu sur fut_M (« exclut le listel hors_module ») ; la lecture retenue est la seule qui rende colonne_M | la lecture concurrente (H réduit de 4,5 p) donne CD 1,07946, indiscernable de `CD_calcule` (1,0795) ; écart de rayon au sommet du contour ≈ 0,1 p **[E]** (architecte) | `classical_diminution.cpp`, §4 |
| P4 | **CD du composite** = CD du corinthien (même H et mêmes rayons) | `CD_calcule` absent pour le composite | — | §5 (E2) |
| P5 | **Coupe par la face** d'un membre à deux coupes = affinité horizontale de la coupe diagonale | coupes données séparément, sans règle d'appariement | voir dette §2.2 | §3 |
| P6 | **`carre_concave` à coupe unique** (canal composite `{s: 13/2}`) lu « même saillie dans les deux coupes » = cercle de rayon distance_axe + s | non précisé | source d'un des trois conflits de plan | §3 |
| P7 | **Ornement à saillie unique `{s}`** rendu comme un listel (segment vertical) | enveloppe h × s (`conventions.formes`) | — | §3 |
| P8 | **Plans approchés** : `oriente_ionique` et `lineaire` → carré dans Classical Column ; cornes de `carre_concave` vives (pan coupé « 6 » ignoré) | plans décrits, sans géométrie complète | voir dette §2.2 | §4 |

### 10.2 Volutes ioniques

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| V1 | Raccord **G1 sur la ligne des centres** | §5.7 recalcule un raccord sur l'axe du centre, non G1 (cassures 2,05° et 4,39°) | mêmes cotes 9/8/7/6 et même premier tour | §7 (VE2) |
| V2 | « Parte 35 » (`faces_des_volutes_en_plan`) lu comme le **fond du canal** : F = 17,5, listel à 19,5 < abaque 20 | non précisé | l'autre lecture (35 au listel) déplacerait les faces de 2 p au plus **[E]** (architecte) | §7 (VE4) |
| V3 | Bande des volutes (listel et canal du chapiteau) au **carré de demi-côté F** quand `Volutes` est coché | profil : 17,25 et 17 ; plan : 17,5 | relief 0,25 au profil contre `listel_face` 2 (anomalie §7.1) | §7 (VE5) |
| V4 | Canal plat, coussinet en prisme droit (sans baltheus), œil plat | non décrits | dette §2.2 | §7 |

### 10.3 Congés du fût et cannelures

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| F1 | Rayon du congé = saillie du listel sur le **vif local** r(y) | « rayon égal à la saillie de ce listel » | ionique 2,29 p contre 2,33 sur le vif nominal et 2,4 mesuré (§8, anomalie 4) | §8 (C1) |
| F2 | Demi-cercle **écrêté** sous l'arête, paroi portée par deux méridiens | « demi-cercle », non représentable par un rayon à valeur unique | aire −0,10 % (ionique), −0,31 % (dorique B) | §8 (F1) |
| F3 | Dorique A **creux** (centre opposé à l'axe) | « centre … vers l'axe » | à faire trancher (§8, anomalie 1) | §8 (F1) |
| F4 | Tête = section réduite (demi-angle β·√(1 − u²)), deux anneaux ; pied à arrêt droit | tête en demi-cercle ; pied « angles à peine arrondis » | têtes à 7,8 % de la profondeur | §8 (F3) |
| F5 | Nombre de cannelures **multiple de 4**, refusé sinon à la lecture | 20 et 24, sans contrainte | contrainte du maillage (symétrie d'ordre 4 du tour), non de Vignole | `classical_orders.cpp` (`ParseFlutes`) |

### 10.4 Colonnade et ornements d'entablement

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| C1 | Entraxe = entrecolonnement + diam_inf **pour les cinq ordres** | « nu à nu » prouvé au dorique (§4.2), écrit au toscan ; l'ionique ne donne que « M. 4½ » **[H]** | — | §9 (O4) |
| C2 | **Avec piédestaux**, même entraxe que sans | colonnades gravées sans piédestal ; écart libre entre piédestaux 39 à 54 p, toujours positif **[M]** (architecte) | — | `classical_colonnade.cpp` (`LayoutColonnade`) |
| C3 | **Larmier** de la corniche à denticules = membre `face` de plus grande saillie | non nommé dans les données | — | `classical_colonnade.cpp` (`PlaceSoffitGuttae`) |
| C4 | Regula du nu de l'architrave à la face de la tenia | non donné | — | dette §2.2 (3) |
| C5 | Gouttes du plafond : h 3/2 et petite base 3/4, reprises des gouttes du triglyphe | non donné | — | dette §2.2 (3) |
| C6 | Denticules ioniques et blocs composites **ancrés au centre** de la colonnade | alignement non donné ; période incommensurable avec l'entraxe | — | dette §2.2 (5) |
| C7 | **Règle d'angle** : élément gardé seulement entre les plans de fond des retours ; angle nu | « retours d'équerre aux angles » | dé, pomme de pin, modillon d'angle absents | dette §2.2 (6) |
| C8 | Modillon en **bloc droit** 8 × 35/2 × 6 ; mutules horizontales | console en S non cotée ; inclinaison non donnée | — | dette §2.2 (7) |
| C9 | Canaux du triglyphe sur toute la hauteur (têtes non modélisées) ; contrôle d'angle limité aux canaux pleins | têtes en biseau et quart de cercle données | demi-canaux à 126,9° (§9, anomalie 6) | dette §2.2 (1) |

### 10.5 Valeurs par défaut de l'interface

Choix du module, sans appui des sources sauf mention : `Volutes` coché ; `Flutes` décoché (appui :
`cannelures.facultatives`) ; `Doric flutes` = A (cité en premier sur la pl. XII) ; `Ornament envelope`
décoché (noyau) ; `Pedestal` et `Entablature` décochés pour Classical Column ; Classical Colonnade :
`Columns` 4 (bornes 1 à 12), `Ornaments` coché, avertissement au-delà de 300 000 faces ;
`Radial segments` 48, `Curve segments` 6.

### 10.6 Colonne dorique grecque (lot G0, `plan_implementation_03_revision.md` §11)

Famille distincte des cinq ordres (D3-2) : la source attendue est le document de monument de l'utilisateur (lot D0,
D3-1, D3-4), encore absent. Chaque forme ci-dessous est donc une convention du module, prise le 2026-11-10, à
confirmer ou à remplacer quand ce document arrivera. Aucune valeur n'y est fixée : les cotes sont des réglages, et le
jeu d'essai `SyntheticGreekDoricColumnSettings` ne décrit aucun monument.

**Révision du lot G2a (2026-11-11, `plan_implementation_03_revision.md` §17, D3-37 à D3-42).** Le chapiteau est
confronté au README du temple d'Héphaïstos (`C:\home\doc\papers\Architecture\Antique\TempleHephaistos\README.md`,
SHA-256 `91f1c083…18f4`, cité « README l. n ») : GD3 et GD4 sont remplacées (formes de G0 retirées, D3-41), GD3a,
GD3c, GD4a et GD2b apparaissent, GD1 est révisée, GD6 et GD5 gagnent une source, GD8 a sa citation corrigée. La
section de cannelure (GD2, GD2a) relève du lot G2b, la conversion des valeurs (GD5a, GD3b) du lot G2c.

**Révision du lot G2b (2026-11-11, `plan_implementation_03_revision.md` §17.19, D3-39).** GD2 est révisée (la
section à trois centres s'ajoute à A et B), GD2a apparaît.

**Révision du lot G2c (2026-11-12, `plan_implementation_03_revision.md` §18, D3-43 à D3-45).** Les valeurs du README
entrent dans la colonne par `MonumentColumnSettings` (`classical_monument.cpp`). README révisé par l'auteur, SHA-256
`4420ab20…c659` (432 lignes, 39 867 octets) : les citations « README l. n » de cette section suivent désormais cette
version. GD1, GD3a et GD3c sont révisées ; GD3d et GD5a apparaissent ; GD4a gagne le statut du rayon de sa lèvre.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| GD1 | **Entasis** : écart horizontal à la droite qui joint les deux rayons, en deux arcs de parabole e = f (1 − u²) qui se rejoignent en zf, nuls au pied et au sommet du fût, sans palier cylindrique ; f et zf sont des réglages. Pour le temple du README, f = 7 mm et zf = 2,605 m **lus**, zf n'étant pas recalculé sur le fût du modèle. *Révisée le 2026-11-11 (G2a), puis le 2026-11-12 (G2c)* | README l. 156 : « un seul arc de cercle », loi `lu` [D] p. 120, flèche ≈ 7 mm `déduit` [D] ; l. 157 : maximum à mi-hauteur du fût, « 2,605 m au-dessus du stylobate », `déduit` (la convention des 2/5 est abandonnée par l'auteur) | parabole et arc de cercle de même flèche et mêmes extrémités : 2 × 10⁻⁸ m sur un fût de 5,210 m, sous l'ulp `float` (plan 3 §17.2, MS-3). La rainure étant portée par le chapiteau du modèle (GD3c), son fût est plus court de 0,188 in : zf = 2,605 lu n'en est plus la moitié (2,6026 m, à −2,414 mm), les deux branches valent 2,605 et 2,600 m, et la courbe du modèle reste à 0,0256 mm au plus de l'arc unique du README (plan 3 §18.3, MS-18, MS-19). zf recalculé sur le fût du modèle manquerait le contrôle de 2,4 mm (mutation MC8). Profil C¹ en zf, rupture de pente aux deux bouts | `classical_greek_doric_column.cpp` (`ShaftRadius`) ; `classical_monument.cpp` (`MonumentColumnSettings`) |
| GD2 | **Cannelures** jointives (demi-angle π/N), arêtes vives sur le cercle du fût ; nombre multiple de 4 (F5 reconduit) ; fond de cannelure à au moins 0,01 R de l'axe (4 cannelures B refusées, leur fond est sur l'axe). ~~Section A ou B des cinq ordres, sans profondeur libre~~ — **section révisée le 2026-11-11 (lot G2b, D3-39)** : A, B, ou à trois centres de Stuart et Revett (`FluteSection::ThreeCentred`, rapport profondeur / corde en réglage, GD2a). A et B restent des réglages de la colonne, sans source grecque. L'étendue en hauteur est GD2b | README l. 150 (20 cannelures, arêtes vives, `lu`) ; section : l. 154 (« fausse ellipse » à trois centres, `lu` [S] pl. IX C), désormais construite | A et B : écart radial maximal de 7,2 mm à la section du README pour A (plan 3 §17.2, MS-2) ; la section à trois centres le supprime | `classical_greek_doric_column.cpp` (`BuildGreekDoricColumnProfile`) |
| GD2a | **Section à trois centres homothétique** : le rapport profondeur / corde est constant sur toute la hauteur du fût et du gorgerin, si bien que la section d'une cote ne dépend que de ce rapport et de son rayon. Construction (`classical_flutes.h`) : droites e-f et e-g par les points de division de la corde à 1/5 et 4/5, centres latéraux aux tiers de e-f et e-g côté f et g, rayon latéral Rc/3, Rc résolu pour que l'arc latéral passe par l'arête. **Limite de représentation** : une section non étoilée depuis l'axe (non fonction de l'angle) est refusée en nommant `fluteDepthRatio`, par un critère analytique ; seuils mesurés de 0,1472 (4 cannelures) à 0,3106 (48). Inscrite le 2026-11-11 (G2b) | README l. 155 (sommet, « même tracé, réduit dans le rapport des diamètres ») et l. 130 (angle, homothétie), au statut **`convention`** de la source ; aux cotes intermédiaires, extension du module. Points de division : non nommés par le texte de la l. 154, fixés par la valeur que le README en déduit (Rc/3 = 2,12 in ; 2/5 et 3/5 donneraient 3,81 in, plan 3 §17.2, MS-1) | une profondeur absolue constante, ou une profondeur qui ne suivrait pas le diamètre, n'est pas représentable. Rc = 6,3652 in pour la corde de 6,25 in, contre 6,36 lu : 0,0052 in, au-delà de la demi-unité (question Q3-47, plan 3 §17.19) | `classical_flutes.cpp` (`SolveThreeCentredFlute`, `ThreeCentredFluteRadius`, `IsThreeCentredFluteStarShaped`) ; `classical_greek_doric_column.cpp` (validation, grille) |
| GD2b | **Étendue des cannelures** : du stylobate au sommet du gorgerin, arrêtées à plat sous le listel le plus bas (ou sous le pied de l'échine sans listel). Inscrite le 2026-11-11 (G2a) | README l. 169 : « gorgerin cannelé » (`lu` [S] pl. IX A ; « l. 168 » avant la révision du README) ; arrêt : aucune | une tête de cannelure relevée (arrêt arrondi ou en biseau) n'est pas représentée | idem (`flutedOrCircle`, `FluteGrid::headBottom`) |
| ~~GD3~~ | ~~**Annelets** : n listels identiques séparés par des intervalles ; pas de gorgerin distinct ; pas de rainures d'hypotrachélion~~ — **remplacée le 2026-11-11 (G2a, D3-41)** par GD3a et GD3c ; un intervalle de G0 est un listel de saillie nulle | README l. 167 (annelets en gradins), l. 169 et 177-184 (gorgerin et rainure ; « l. 167, 168 » avant la révision du README) | — | — |
| GD3a | **Annelets en gradins** : suite de listels jointifs {hauteur, saillie}, de bas en haut ; chaque listel est vertical, et deux listels voisins se raccordent par une jonction plate. Au plus `kGreekDoricMaxAnnulets` (8) listels, borne d'interface. Pour le temple du README : `annuletCount` gradins égaux en hauteur (1,234 / 4 = 0,3085 in) et en retrait (0,916 / 4 = 0,229 in), **face puis lit** (phase B) ; le listel du haut porte la saillie du pied de l'échine, au bit près, et le lit du listel du bas se prolonge par le ressaut jusqu'au gorgerin (1,113 in). Inscrite le 2026-11-11 (G2a), *révisée le 2026-11-12 (G2c)* | README l. 167 : 4 annelets, hauteur 1,234 in et retraits extrêmes 5,334 → 6,25 in, `lu` [S] pl. IX A ; l. 171-175 : gradins égaux, face verticale et lit horizontal, **`convention` de la source** (GD3b n'est donc pas une convention du module) ; phase : l. 173 (« face verticale, lit horizontal », dans cet ordre) et l. 188-190 (dessus des annelets au retrait 5,334, arête du premier gradin) | la face inférieure biseautée des annelets (l. 171) n'est pas représentée ; la phase (A), lit puis face, est écartée : elle poserait l'échine en surplomb de 0,229 in sur le premier annelet, ce que le texte ne dit pas (plan 3 §18.3, MS-23) ; « .134 » de la pl. IX B est lu mais pas interprété par la source. ~~La source est incohérente sur les retraits (MS-6)~~ : lecture retirée par l'auteur (l. 171) | idem (`AppendStraight`) ; `classical_monument.cpp` (`MonumentColumnSettings`) |
| GD3c | **Rainure sous le gorgerin** : cylindre circulaire de rayon diamètre supérieur / 2 − profondeur, raccordé au fût et au gorgerin par deux jonctions plates, **porté par le chapiteau du modèle** (`hypotrachelionHeight` = gorgerin + rainure, « rainure comprise »), alors que le README la met au haut du fût ; son fond est au plus au fond des cannelures au sommet du fût, sinon refus nommé (`grooveDepth`). Inscrite le 2026-11-11 (G2a), *révisée le 2026-11-12 (G2c, D3-44 : choix par défaut, à confirmer)* | README l. 178 : hauteur 0,188 in, `lu` [S] pl. IX A ; l. 179 : sous le gorgerin, hors de ses 4,3 in, au haut du fût, `déduit` (§12, l. 423) ; l. 183 : profondeur `manquant` (incision en V) ; l. 184 : fond par rapport aux cannelures `manquant` | partition chapiteau / fût décalée de 0,188 in (4,8 mm), sans effet sur l'ordre des anneaux ni sur le maillage ; rayon au pied de la rainure d/2 au lieu de d/2 + 0,0256 mm (plan 3 §18.3, MS-19) ; section rectangulaire contre une incision en V (l. 183). Une rainure moins profonde que les cannelures est **refusée** : limite de représentation (la section serait min (cercle, cannelure), forme d'anneau qui n'existe pas, dans `classical_profile`, lié par `maker`), pas règle de domaine. L'option « rainure au haut du fût dans la colonne » (Q3-49 b) changerait la sémantique de deux champs | idem (`AppendCylinder`, `ValidateGreekDoricColumnSettings`) ; `classical_monument.cpp` (`MonumentColumnSettings`) |
| GD3d | **Profondeur de la rainure du temple du README, limite de représentation** : fond sous le fond des cannelures au sommet du fût, d'une hauteur de rainure : `grooveDepth` = d/2 (1 − `GreekDoricFluteBottomRatio`) + `grooveHeight` = 30,86 mm (1,2149 in) depuis d/2. Appliquée seulement à une profondeur `manquant` au document ; une profondeur donnée est refusée par la conversion (`'columns.grooveDepth'`), faute de lecture. **Ce n'est pas une restitution.** Inscrite le 2026-11-12 (G2c) | **aucune** : README l. 183 et 184, profondeur et fond `manquant` ; l'auteur écrit que les ≈ 31 mm de `cg` sont « une limite du modèle », « contraire à l'incision en V de la planche », et « ne doi[ven]t pas être repris comme donnée » (l. 184) | 30,9 mm de creux pour 4,8 mm de haut, contre une incision en V peu profonde à la planche ; la marge d'une hauteur de rainure écarte la couronne dégénérée d'un fond confondu avec celui des cannelures (mutation MC7 rouge). Une rainure fidèle demande l'anneau min (cercle, cannelure) (règle 3, `maker` reconstruit) | `classical_monument.cpp` (`MonumentColumnSettings`) |
| ~~GD4~~ | ~~**Échine** : arc de Bézier quadratique du pied à la tête (`echinusBulge`)~~ — **remplacée le 2026-11-11 (G2a, D3-41)** par GD4a | README l. 186-199 (profil par points, droite et lèvre ; « l. 172-179 » avant la révision du README) | — | — |
| GD4a | **Échine en droite et lèvre** : droite du pied à la jonction, puis arc de cercle convexe de rayon donné par la jonction et la tête (les deux points gravés), centre du côté rentrant de la corde ; **arête** à la jonction (aucune tangence imposée) ; le point le plus saillant de la lèvre, s'il est intérieur, est un sommet du profil, et le demi-côté de l'abaque doit le contenir (`abacusProjection` ≥ saillie maximale de l'échine, règle de représentation). Inscrite le 2026-11-11 (G2a, D3-40) | README l. 195-199 (« l. 177-179 » avant la révision du README) : retraits 0,625 / 1,0 / 3,5 / 5,334 gravés, droite à 48°, lèvre de rayon 1,15 in `mesuré` [S] pl. IX A, ± 0,05 in, « ajusté sur les points mesurés, 1,10 à 1,20 in » (statut écrit le 2026-11-12, l. 197) ; profondeur de la jonction 1,7 in `mesuré` [S] (l. 193) ; jonction : aucune. Écarts du modèle aux points mesurés relus par J13 (G2c) : −0,008, −0,024, −0,047 in | sur les valeurs du README (plan 3 §17.2, MS-10) : saillie maximale 0,396 in du nu contre 0,42 mesuré ; écarts aux points mesurés −0,008, −0,024, −0,047 in, sous la précision annoncée (± 0,1 in) ; arête de 19,5° à la jonction, entre la tangente à la lèvre (28,4° sur l'horizontale) et la droite (47,9°) ; 16,9° et 22,7° aux rayons 1,20 et 1,10 (valeur corrigée le 2026-11-11, revue de G2a : 13,7° comparait l'angle polaire du rayon à la pente de la droite, plan 3 §17.18). Les lectures (b) par la saillie mesurée et (c) tangente à la droite ne sont pas retenues (MS-11) | idem (`LipOf`, `LipPoint`, `LipExtremum`, `AppendEchinus`) |
| GD5 | **Saillies du chapiteau** comptées depuis le rayon du sommet du fût (diamètre supérieur / 2) ; **diamètres** du fût pris aux arêtes | README l. 151 : « D est donc le diamètre sur les arêtes » (contrôle par la corde) ; saillies : aucune | un relevé en diamètres absolus se convertit sans perte ; conversion des rayons de [S] au lot G2c (GD5a) | `classical_greek_doric_column.h` |
| GD5a | **Rayons du chapiteau de [S] en saillies depuis d/2 de [D]** : un retrait r depuis le nu de l'abaque devient la saillie (A − r) − d/2, A = demi-côté de l'abaque ; le gorgerin et la rainure sont rapportés à d/2. Inscrite le 2026-11-12 (G2c) | README l. 92 (d = 0,775 m, `lu` [D]) ; l. 165 (A = 22,451 in, `lu` [S] pl. IX A) ; l. 169 et 180 (gorgerin au rayon du fût) ; l. 425 (rayon supérieur de [S], 22,451 − 7,134 = 15,317 in) | rayon supérieur de [S] 15,317 in = 0,389052 m contre d/2 = 0,3875 m : 1,552 mm (plan 3 §18.3, MS-24 ; J11, propriété `gd5a_upper_radius_gap_mm`). Abaque, échine et annelets gardent leurs rayons de [S] depuis l'axe ; seul le gorgerin, à d/2, est 1,552 mm en deçà, si bien que le ressaut sous les annelets s'allonge d'autant. J15 affirme R + saillie = A à 10⁻¹² m (mutation MC10, plan 3 §18.18) | `classical_monument.cpp` (`MonumentColumnSettings`) |
| GD6 | **Abaque** : prisme carré nu, sans cimaise ; la hauteur de la colonne va du stylobate au sommet de l'abaque | README l. 165 : « carré, sans moulure » (`lu`) ; l. 93 : « Hauteur de colonne (chapiteau compris) » (`lu`). Source gagnée le 2026-11-11 (G2a) | — | idem |
| GD7 | **Fût évasé accepté** : diamètre inférieur et diamètre supérieur sont libres l'un par rapport à l'autre ; un fût plus large en tête qu'au pied est construit, et non refusé | aucune ; le dorique grec connu diminue vers le haut, mais aucune source fournie ne l'énonce comme règle de domaine | le refuser serait une règle de domaine inventée (D3-4) ; la géométrie reste valide (variante « fût évasé » des tests G1 à G9) | `classical_greek_doric_column.h` |
| GD8 | **Orientation des cannelures** : une cannelure centrée sur chacun des deux axes X et Y (centres en 2πj/N, `classical_profile.h:164-165`), d'où une demi-étendue en X et en Y de R cos (π/N) pour un anneau cannelé, et non R. Inscrite le 2026-11-11 (lot K1a, D3-23) ; le choix du code est antérieur | README du temple d'Héphaïstos (`C:\home\doc\papers\Architecture\Antique\TempleHephaistos\README.md`, l. 152 ; citation « l. 151 » corrigée le 2026-11-11, lot G2a, décalage de D3-32) : même règle, au statut **`convention`** de la source ([S] pl. VI ne tranche pas). Convention du module qui coïncide avec une convention de la source, à reconfirmer par le lecteur de D0 | avec une arête sur l'axe, l'emprise vaudrait R : écart R (1 − cos π/N), 0,0066 pour le pied synthétique (R = 0,535, N = 20) | `classical_greek_doric_column.cpp` (`ComputeGreekDoricColumnHalfExtents`) ; grille du tour, `classical_lathe.cpp` |

### 10.7 Krépis (lot K0, `plan_implementation_03_revision.md` §13)

Même statut qu'au §10.6 : la source attendue est le document de monument de l'utilisateur (lot D0), encore absent.
Chaque forme ci-dessous est une convention du module, prise le 2026-11-11, à confirmer ou à remplacer quand ce
document arrivera. Aucune valeur n'y est fixée : hauteurs et débords sont des réglages, et le jeu d'essai
`SyntheticKrepisSettings` (test) ne décrit aucun monument. Les choix de format (bornes `kKrepisMaxLowerSteps` et
`kKrepisMaxLength`, origine en z = 0 au pied du degré le plus bas, ordre de `KrepisLayout::steps`) ne figurent pas
ici : ils sont consignés dans l'en-tête et dans le plan (D3-16, D3-17).

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| KR1 | **Degré** : prisme droit à base rectangulaire, arêtes vives, dessus horizontal ; ni pente de giron, ni courbure (R0), ni anathyrose, ni joints de blocs | aucune ; la forme relevée des degrés viendra de D0 | courbure et pente des girons non représentables ; leur amplitude sur le monument est inconnue tant que le relevé manque | `classical_krepis.h` |
| KR2 | **Degrés pleins, concaténés sans union** : chacun de son pied à son dessus sur toute son emprise ; un degré pose sur le suivant par contact plan, seul recouvrement de la krépis (première entrée de la liste fermée de P3-3, lot K1) | aucune | volume intérieur compté par degré, contact coplanaire signalé par `ValidateMesh` comme auto-intersection (S5) ; un solide soudé demanderait un constructeur propre | `classical_krepis.cpp` (`BuildKrepisMesh`) |
| KR3 (amendée au lot P1, §10.11) | **La krépis s'arrête au pied de son degré le plus bas** : ni euthyntéria ni sol modélisés à part ; une euthyntéria donnée par les sources entre comme un degré de `lowerSteps` | aucune | — | `classical_krepis.h` |

### 10.8 Temple : krépis et péristyle (lot K1, `plan_implementation_03_revision.md` §14)

Même statut qu'aux §10.6 et §10.7 : conventions du module, prises le 2026-11-11 au lot K1a, à confirmer ou à remplacer
par le lecteur du document de monument (lot D0). La colonne « Sources » cite le README du temple d'Héphaïstos
(`C:\home\doc\papers\Architecture\Antique\TempleHephaistos\README.md`) quand il porte la même règle : la convention du
module coïncide alors avec une convention ou une valeur de la source. Aucune valeur n'est fixée : le jeu d'essai
`SyntheticTempleSettings` (`test/greek_doric_test_support.h`) ne décrit aucun monument. TE5 (pièces concaténées sans
union, liste fermée des recouvrements) est inscrite au lot K1b, avec le maillage qui l'applique. Les choix de format
(ordre des placements, rang des adresses) restent dans l'en-tête et le plan (D3-17).

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| TE1 | **Deux jeux de réglages de colonne** : un pour les quatre colonnes d'angle, un pour toutes les autres | README §4.4 (l. 121-138 ; citation « l. 120-137 » corrigée le 2026-11-11, lot G2a, décalage de D3-32) : diamètre inférieur d'angle `lu` [S], le reste identique par `convention` de la source | une colonne courante qui diffère d'une autre (façade et flanc, par exemple) n'est pas représentable | `classical_temple.h` (`TempleSettings`) |
| TE2 | **Péristyle centré sur le stylobate** : le reste se partage à parts égales de part et d'autre (RG4, place du reste) | README §3.1, §4.3 : origine au centre du stylobate, axes symétriques | une marge relevée asymétrique n'est pas représentable | `classical_temple.cpp` (`LayoutTemple`) |
| TE3 | **Chaque pied, courant ou d'angle, entièrement sur le stylobate, bord compris** : \|x\| + `foot` ≤ demi-côté, et de même en Y ; sinon refus, sans mise à l'échelle ni rognage (RG4). Le chapiteau peut déborder | aucune règle ; le README donne un débord positif de 0,057 m au-delà du nu des colonnes d'angle (l. 79) | un pied en porte-à-faux n'est pas représentable | `classical_temple.cpp` (`RequireFeetOnStylobate`) |
| TE4 | **Aucun contact entre colonnes**, sur les boîtes alignées sur les axes de demi-côté `overall`, jugé sur les axes **placés** : deux colonnes voisines d'une rangée, les deux voisines d'un angle, et, pour une rangée de deux colonnes, les colonnes des deux rangées opposées qui se font face à travers elle ; des abaques jointifs sont refusés | aucune | exacte pour les boîtes ; pour les colonnes, seulement suffisante si les anneaux les plus larges ne sont pas des abaques à la même cote (sections rondes, colonnes de hauteurs différentes) | `classical_temple.cpp` (`RequireRowSeparation`, `RequireCornerNeighboursSeparation`) |
| TE5 | **Pièces concaténées sans union** : krépis et colonnes, chacune fermée, mises bout à bout dans un seul maillage ; seuls recouvrements admis, liste fermée : L1, contact plan entre degrés (KR2), et L2, pied d'une colonne sur le dessus du stylobate, deux faces horizontales à `float (stylobateTop)` | aucune | contacts signalés par `ValidateMesh` comme auto-intersections (T3) ; un solide soudé demanderait une union | `classical_temple.cpp` (`BuildTempleMesh`) |
| TE6 | **Contraction simple et symétrique** : une seule travée d'angle par bout de rangée, la même aux deux bouts et sur les deux rangées opposées ; une rangée de moins de quatre colonnes n'en a pas | README §4.2-4.3 : une travée d'angle par côté, axes symétriques (`déduit` [S]) | double contraction et contraction asymétrique non représentables (N1, Q3-7) | `classical_peristyle.cpp` (`LayoutPeristyle (const PeristyleSettings &)`) |

### 10.9 Document du monument (lot D0, `plan_implementation_03_revision.md` §15)

Conventions du module prises le 2026-11-11 au lot D0, en lisant le document du monument
(`src/cgarchi/data/temple_hephaistos.json`, dérivé du README du temple d'Héphaïstos,
`C:\home\doc\papers\Architecture\Antique\TempleHephaistos\README.md`, SHA-256 `91f1c083…18f4`). Les valeurs du
document, leurs statuts et leurs sources sont ceux du README ; le script les vérifie et n'en attribue aucun. Le giron
du 3ᵉ degré porte le statut `convention` de la source (README l. 78) : ce n'est pas une convention du module, et il
n'entre pas ici. Les choix de format (clés en anglais, ordre des clés, indentation, `value` absente pour `manquant`)
restent dans l'en-tête, le schéma et le plan (D3-17, D3-27).

**Révision du lot G2c (2026-11-12, `plan_implementation_03_revision.md` §18).** Document `monument/v2`, dérivé du
README `4420ab20…c659` : clé `unit` dans chaque valeur (D3-37 ; une valeur manquante porte l'unité attendue, le mètre
du README l. 31), section `columns` de 22 valeurs, objet `columnControls`. MO1 est étendue, MO3 apparaît. Choix de
format restés hors du registre (D3-17) : la corde au pied est lue en pouces (6,25 in, cote gravée) et non en mètres
(0,159 m, son arrondi), parce que le README calcule la fausse ellipse sur elle ; les contrôles du péristyle restent sans
unité, en mètres.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| MO1 | **Tolérance d'un contrôle qui confronte des valeurs de sources différentes** : la somme des demi-unités du dernier chiffre affiché de chaque terme (propagation de l'arrondi de la source). Débord du stylobate au-delà du nu de la colonne d'angle : 2,25 mm en façade (W/2, axe d'angle 1,5 n + a, D_a/2, débord : 0,25 + 1,25 + 0,25 + 0,5), 4,0 mm en flanc (0,25 + 3,0 + 0,25 + 0,5) | aucune : le README écrit « ✓ » sans tolérance | une incohérence de la source sous la borne passe ; écarts mesurés : +0,5 mm en façade, −0,5 mm en flanc (J8, propriétés XML). Cause : le README arrondit n et n_f au millimètre ; les valeurs exactes de son calcul (§4.2) sont n = 7,765 / 3 = 2,58833… et n_f = 25,809 / 10 = 2,5809. L'axe d'angle 1,5 n + a en façade est donc trop court de 0,5 mm, et 5 n_f + a_f en flanc trop long de 0,5 mm. Avec les valeurs exactes, le débord serait 0,057 m des deux côtés ; une transcription fausse la dépasse (MJ9 : 9 cm). **Étendue le 2026-11-12 (G2c) à un contrôle produit ou quotient** : somme des demi-unités relatives des facteurs, rapportée à la valeur. Profondeur des cannelures au sommet (README l. 155, 1,1 × 4,81 / 6,25 = 0,85 in) : 0,85 × (0,05/1,1 + 0,005/4,81 + 0,005/6,25) = ± 0,0402 in ; modèle 0,8391 in, écart 0,011 in, parce que le README calcule sur l'arc et le d de [S], le modèle sur la corde et le d de [D] (plan 3 §18.3, MS-22). **Appliquée le 2026-11-12 (N0b) à la métope lue** (README l. 251-252, 2 ft 6,475 in) contre la métope courante du modèle, n/2 − w : 0,25 mm (n_f / 2) + 1,27 mm (8,3 in) + 0,0127 mm (6,475 in) = 1,5327 mm ; écarts calculés 0,815 mm en flanc (dedans) et 4,315 mm en façade (dehors), ce que le README conclut (plan 3 §20.2, MS-41) | `test/tu_cgarchi_classical_monument.cpp` (J8, J11, J19) |
| MO2 | **La façade de D2-5 (rangée `front`, en Y < 0) est l'entrée du monument** : façade est du README (l. 52, `lu`) ; (X, Y, Z) = (y, −x, z + `stylobateTop`) ; est = `Front`, nord = `Right`, ouest = `Back`, sud = `Left` | D2-5 ne nomme pas la façade d'un monument orienté | éprouvée sur le contrat de `TempleFrameFromMonument` par J6 : est en Y < 0, nord en X > 0 ; MJ14 rouge (`plan_implementation_03_revision.md` §15.15). Le contenu du document reste symétrique par x → −x et y → −y : la confrontation à une donnée asymétrique attend le lot R0 (courbures par côté) ; un monument dont l'entrée n'est pas sur un petit côté demanderait une autre règle | `classical_monument.h` (`TempleFrameFromMonument`, conversions) |
| MO3 | **Règles de lecture du chapitre 5 du README** (document `monument/v2`), chacune lit ce que l'auteur a écrit, aucune n'infère : **R1**, clause étiquetée « étiquettes : statut [réf] », la cellule de statut se coupant à « ; » et à « . » suivi d'un mot à majuscule initiale, hors parenthèses ; **R2**, valeur repérée par son libellé et son unité n'importe où dans la cellule, une seule occurrence ; **R3**, tableau à lignes de statut, colonne désignée par le nombre qui commence son en-tête ; **R4**, statut de prose entre parenthèses simples sur la ligne de puce de la valeur. Inscrite le 2026-11-12 (G2c, D3-43) | README §5.1-5.2, forme écrite par l'auteur (corrections A1 à A5 faites, l. 156, 165, 178, 193, 197) | un libellé reformulé, une étiquette renommée, une valeur dupliquée, un statut vidé ou sorti de sa puce, une unité changée font **refuser** le script, clé nommée (contrôles négatifs NC10 à NC14 et NC16, plan 3 §18.17 et §18.18) ; il ne devine pas. L'unité d'un sélecteur doit suivre directement la valeur qu'il capture (NC15). « pl. IX » ne coupe pas une clause (majuscule suivie d'une majuscule) : une abréviation suivie d'un mot ordinaire à majuscule le ferait. **Étendue le 2026-11-12 (N0b, D3-52 amendé par D3-53, document `monument/v3`) au chapitre 6** : **R5**, suite de valeurs à unité finale partagée (« en plan : demi-canal 2,0 in, bande 2,8, … demi-canal 2,0 in ; »), lue par position, chaque terme « mot nombre [unité] » avec son mot recopié ; nombre de termes et unité finale exigés, toute unité intermédiaire égale à la finale, suite close par « ; » ; **R6**, pieds et pouces (« 2 ft 8,55 in »), deux nombres lus tels qu'écrits dans deux clés (`…Ft`, `…In`), une unité par groupe qui suit directement son groupe, pieds entiers ≥ 1 (« 0 ft » refusé, NC27, plan 3 §20.19), unité `ft` dans la liste fermée (D3-37 amendé) et réservée aux clés `…Ft` ; la somme se fait dans le lecteur. R1' et R7 de D3-52 ne sont pas écrites (D3-53). **Étendue le 2026-11-12 (N1b, D3-56, document `monument/v4`)** : **R8**, « et » séparateur d'unité partagée entre deux groupes capturés, comme « → » et « × » (« 0,685 et 0,916 m » : les deux en mètres, l'unité suivant le dernier) ; pour toute unité partagée, le séparateur est suivi directement du groupe suivant, et seuls des décimaux la partagent, jamais un compte (revue de N1b, contrôles LOW1-a et LOW1-b, plan 3 §21.22) ; sans effet sur la dérivation v3 (plan 3 MS-49). Homonyme sans rapport : « R8 » de la fenêtre gothique (`debt_cgarchi.md`). **Étendue le 2026-11-14 (N3b, D3-75, document `monument/v6`)** : **R9**, unité « ° » collée au nombre (« **inclinée de 14°** »), sans blanc, un seul groupe décimal ; « ° » entre dans la liste fermée des unités (D3-37 amendé), et le lecteur la réserve aux angles ; l'étiquette R1 « gouttes par rang » est l'une des deux étiquettes de la clause « rangs et gouttes par rang : lu [S] … », découpée à « et » par R1 elle-même, sans règle nouvelle (l'étiquette entière est refusée, MS-72) | README §6.1 l. 209, 214, 216 et §6.3 l. 250, 251 (R6) ; §6.3 l. 253 (R5) ; §6.3 l. 256 (R8 : « 0,685 et 0,916 m », « 0,686 et 0,918 m ») ; README `0faba870…` §6.4 l. 276 (R9 : « 14° ») et l. 275 (« rangs et gouttes par rang ») | une suite à un terme de moins, une unité intermédiaire différente, une unité finale absente, « pi » au lieu de « ft », des pieds décimaux font refuser le script, clé nommée (NC17 à NC21, plan 3 §20.18) ; une valeur replacée dans une cellule de statut (forme de R7) ou une phrase à référence ajoutée à la première clause d'un statut (forme de R1') aussi (D3-53, contrôles D53-R7 et D53-R1p du §20.18). R8 est sensible à la forme comme R1 à R6 : « 0,685 m et 0,916 m » fait refuser la clé intérieure, motif trouvé 0 fois (NC28) ; sans R8, la table des sélecteurs est refusée avant toute lecture (NC29, plan 3 §21.21). R9 aussi : « 14 ° » (un blanc) fait refuser la pente, motif trouvé 0 fois (NC40, plan 3 §23.24) | `src/cgarchi/data/temple_hephaistos_from_readme.py` |

### 10.10 Entablement périphérique (lot N0a, `plan_implementation_03_revision.md` §19)

Conventions du module prises le 2026-11-12 au lot N0a, sous les décisions D3-46 (angle C7 provisoire), D3-49 (balayage
nouveau à côté de `SweepEntablature`) et D3-51 (Q3-57 a : frise de l'anneau au nu des métopes, triglyphes seuls en
pièces). Aucune valeur n'est fixée : le jeu d'essai `SyntheticEntablatureSettings` (`test/greek_doric_test_support.h`)
ne décrit aucun monument. La colonne « Sources » cite le README du temple d'Héphaïstos (`cb0ee7fa…1b02`, numéros de
ligne de cette version) ; le lot N0b y ajoute les valeurs de EN1 et inscrit EN8, EN9, EN11 et EN12 (§20.6). EN6 et
EN7 du §19.7 sont retirées (D3-51 ; division lue sur [S] pl. VI fig. 3, sans donnée de Vignole). Les choix de format
(`std::optional` pour un temple sans entablement, ordre des pièces, rang des placements de frise, coordonnée `a` le
long du côté) restent dans l'en-tête et le plan (D3-17). Le lot N1a (2026-11-12, plan 3 §21, décisions D3-54 et
D3-57) révise EN4 et inscrit EN14 et EN16, sur réglages synthétiques seulement. Le lot N1b (2026-11-12, plan 3 §21.6,
décisions D3-55 et D3-56) inscrit EN15, stratégie du monument, et retire ainsi EN4 pour le temple du README ; il étend
MO3 (R8, document `monument/v4`). Le lot N2a (2026-11-13, plan 3 §22, décisions D3-60 à D3-64) inscrit EN17 à EN21, étend TE5 (L8 à L14, L13 révisée par D3-64 sur la mesure de la sonde du §22.18) et révise le texte de EN2 et de EN12, sur réglages synthétiques seulement ; citations au README `d98ef659…` (mêmes numéros de ligne que `cb0ee7fa…` pour les lignes citées). Le lot N2b (2026-11-13, plan 3 §22.20, D3-60, D3-61) applique EN17 à EN21 au temple du README (document `monument/v5`), retire EN12 pour lui et révise le texte de EN15 ; citations au README `47d83afd…` (458 lignes, mêmes numéros pour les lignes citées). Le lot N3a (2026-11-14, plan 3 §23 et §23.21, décisions D3-66 à D3-71, D3-76 et D3-80) révise EN2 et EN21, inscrit EN22 à EN24 et étend TE5 (L11 révisée, L14 absente sous sous-face inclinée, L15), sur réglages synthétiques seulement ; citations au README `0faba870…` (458 lignes, mêmes numéros que `47d83afd…`). Le lot N3b (2026-11-14, plan 3 §23.24, D3-67, D3-75 à D3-79) applique EN22 à EN24 au temple du README (document `monument/v6`, 10 clés et 18 contrôles des l. 274 à 276), y inscrit les valeurs de EN2 et de EN21 à EN24 et étend MO3 (R9) ; citations au README `0faba870…`.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| EN1 | **Dos simplifié** : face intérieure plane et verticale sur toute la hauteur, à `innerFace` de la ligne des axes, vers l'intérieur ; dessus et dessous horizontaux jusqu'à elle ; sans corniche intérieure | aucune : la face intérieure de l'entablement du péristyle est `manquant` (README l. 243, B3) ; précédent, `cinq_ordres.json:59` (`dos_simplifie`, « si un toit ou une galerie cache le dos ») | plafond du ptéron, poutres et caissons non représentés ; la face extérieure de l'architrave s'incline (l. 229), la face intérieure non. **Valeur pour le README (lot N0b, 2026-11-12)** : `innerFace` = saillie du **pied** de la face de l'architrave, 19,459 in = 0,4942586 m (l. 229), dessous symétrique sur la ligne des axes, sous l'abaque de demi-côté 0,570 m ; appliquée au seul statut `manquant`, ~~une valeur donnée au document est refusée~~ ; **en repli depuis le lot C4a (2026-11-19, CE-32)** : une face donnée est lue (Héphaïstos : 19,891 in, déduit [S], C4-9), EN1 ne vaut que pour une face manquante ; clé facultative, non consommée (MO-P1) | `classical_greek_entablature.h` (`GreekEntablatureSection`) ; `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN2 | **Profil en bandes à faces droites, verticales ou inclinées** (`projection` au pied, `topProjection` en haut), ressauts horizontaux ; un ressaut entre deux saillies égales est omis, et le sommet de jonction retiré quand les deux faces sont verticales ; sous-face du géison horizontale **par défaut** (`mutules.rise` = 0, section de N2 au bit près), inclinée sous EN22 (lot N3a ; temple du README, lot N3b) | README l. 209-242 (hauteurs et saillies) ; l. 229 (face inclinée de l'architrave, 0,2 in) ; l. 217 et 276 (`0faba870…` : bec en lèvre verticale, sous-face à 14°, `mesuré`) | moulures non tracées (l. 414) ; aucun pour le monument : sous-face inclinée appliquée au temple du README au lot N3b (EN22 : `mutules.rise` = 15,876 sin 14° in = 0,0975551 m, anneau de 24 sommets) ; MS-57 retirée : le bec est une lèvre verticale (l. 217), non la tranche de la sous-face | `classical_greek_entablature.cpp` (`GreekEntablatureSection`) |
| EN3 | **Rythme monotriglyphe sur les axes donnés** : un triglyphe centré sur chaque axe et au milieu de chaque travée (`EqualParts`) ; la métope est le reste (`GapsBetweenCentred`), un résultat et non une entrée | README l. 249 (« règle du dorique ») ; la métope courante est `déduit` des axes par la source (l. 252) | la métope lue (0,774 m, l. 251) n'est qu'un contrôle (MS-29) | `classical_greek_entablature.cpp` (`LayoutGreekFrieze`) |
| EN4 (révisée, lot N1a, 2026-11-12) | **Angle C7 : stratégie `DoricCornerStrategy::AxisTriglyph`, valeur par défaut de `GreekEntablatureSettings::corner`** : triglyphe sur l'axe d'angle de chaque côté, demi-métope à l'angle, du bord du triglyphe d'angle au coin du nu des métopes (a = ± (demi-distance des axes + b)), sur les quatre côtés ; refus si width / 2 ≥ b (les triglyphes d'angle de deux côtés voisins se toucheraient), sous cette stratégie seulement. Les deux autres stratégies (`CornerTriglyphEqualMetopes`, `CornerTriglyphMidBay`) posent le triglyphe d'angle (EN14) ; liste fermée (D3-54) | registre C7 (§10.4) ; Q3-7, première stratégie ; D3-46 ; D3-54 | **contraire au README** (l. 255-256 : triglyphe d'angle à 0,231 m au-delà de l'axe, métopes d'angle de 0,801 m) : retirée **pour le monument** en N1b (EN15 ; fait le 2026-11-12), gardée pour le jeu synthétique et pour tout appelant qui ne choisit pas de stratégie ; sous elle, 72 prismes au lieu de 68 triglyphes sur le README | `classical_greek_entablature.{h,cpp}` (`DoricCornerStrategy`, `LayoutGreekFrieze`) |
| EN5 | **Triglyphes en prismes posés sur la frise de l'anneau, au nu des métopes** ; les métopes sont la face de l'anneau, des intervalles de placement sans maillage ; la face de la bande de frise est verticale (refus sinon) | README l. 236 (nu des métopes, ressaut de 2,8 in, `déduit` [S] fig. 3) ; l. 260-262 (construction de la frise écrite par l'auteur : anneau au nu des métopes, prisme de 0,418 à 0,489 m) ; dos du triglyphe non décrit ; D3-51 | dos du triglyphe invisible, aucune cote ajoutée ; une métope à part (les 18 sculptées, hors périmètre) demanderait de la découper de l'anneau | `classical_greek_entablature.cpp` (`LayoutGreekFrieze`, `BuildTriglyphMesh`) ; `classical_temple.cpp` (pièces) |
| EN8 | **Chapiteau du triglyphe en deux bandes continues de l'anneau** : bande de 3,6 in à 20,259 in, listel de 1,0 in à 20,659 in, régnant au-dessus des triglyphes et des métopes ; le prisme du triglyphe s'arrête sous elles. Lot N0b (2026-11-12) | README l. 234 (saillies, `lu` [S]) ; l. 235 (partage 3,6 / 1,0 in, **`convention` de la source**, sans référence) ; l. 263 (continuité et appartenance à l'anneau, choix de construction écrit par l'auteur) | aucune cote de `cg` ; le partage reste une convention de la source, à remplacer si l'auteur le cote ; la conversion refuse un partage dont la somme n'est pas la hauteur du chapiteau (4,6 in, l. 215) à 10⁻⁹ in près | `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN9 | **Valeurs en pouces telles qu'écrites, jamais le nombre en gras en mètres** ; zone des regulae et des gouttes (2,734 + 0,95 = 3,684 in) en une bande verticale au nu du haut de l'architrave (19,259 in) ; corps de la frise = hauteur de la frise − chapiteau du triglyphe (32,55 − 4,6 = 27,95 in). Lot N0b (2026-11-12) | README l. 209-217 ; l. 214 (« et non 7,378 », la lecture en gras est réfutée par la source) ; l. 233 (triglyphe au nu du haut de l'architrave) | regulae (saillie 20,859 in) et gouttes non représentées avant N2 : la zone est une bande lisse sous la ténia | `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN11 | **Profondeur des canaux par l'angle droit** : canal en V à angle droit, profondeur = demi-largeur (3,95 / 2 = 1,975 in) ; demi-canal « de même pente », profondeur = sa largeur (2,0 in) ; la profondeur mesurée (1,98 in ± 0,1 in) n'entre pas, c'est le contrôle. Lot N0b (2026-11-12) | README l. 254 (angle droit constaté au tracé, profondeur non cotée, `mesuré` [S]) | demi-canaux plus profonds que les canaux de 0,025 in ; écart au mesuré 0,005 in | `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN12 (révisée, lot N2a, 2026-11-13) | **Plaque des mutules en bande continue** de 2,35 in, saillie 35,825 in, sans mutules distincts ni vias creusées. Lot N0b (2026-11-12). Depuis N2a, c'est la **valeur par défaut** : sans réglage `mutules` (`GreekEntablatureSettings::mutules` absent), l'entablement est celui de N0b et N1 au bit près ; gardée pour les jeux de N0a et N1a. **Retirée pour le monument (lot N2b, 2026-11-13)** : `MonumentEntablatureSettings` pose `mutules` (bande 6) et ramène la saillie de la bande 6 au dos des mutules (EN21) | README l. 217 (hauteur), l. 237 (saillie), l. 273 (mutules et vias, N2) | mutules, vias et leurs gouttes absents (jeux de N0a et N1a seulement) | `classical_greek_entablature.h` (`GreekEntablatureSettings::mutules` absent) |
| EN10 | **Pied de l'entablement au sommet `float` des colonnes** : `entablatureFoot` = `double (float (column.height))` + `stylobateTop`, l'expression de `Accumulator::Append` pour le sommet d'un tour ; les deux jeux de colonne de même hauteur en `float` (sinon refus, `'cornerColumn.height'`) ; toute cote de l'entablement vaut `float (entablatureFoot + z)` | README l. 209 (architrave de 5,715 à 6,551 m) ; hauteur commune des colonnes (§4.4) | aucune | `classical_temple.cpp` (`LayoutTemple`) |
| TE5 (étendue) | **Liste fermée des recouvrements avec l'entablement** : L1 et L2 (§10.8), plus L3 (dessous de l'anneau sur le dessus de chaque abaque, horizontal à `float (entablatureFoot)`), L4 (dos d'un triglyphe sur la face de frise de l'anneau, plan vertical calculé par la même expression `double` dans les deux pièces), L5 (dessous d'un triglyphe sur le ressaut de la bande sous la frise), L6 (dessus d'un triglyphe sous le ressaut de la bande au-dessus) ; L7 et L8 du §19.7 retirées (D3-51) | aucune | toute traversée exclue ; contacts signalés par `ValidateMesh` comme auto-intersections (T12) | `classical_temple.cpp` (`BuildTempleMesh`) |
| EN13 | **Canaux sans tête** : canaux et demi-canaux du triglyphe en V, de section constante sur toute la hauteur de la bande de frise, ouverts sous le chapiteau du triglyphe ; aucune terminaison (tête arrondie ou plate) en haut. Inscrite à la revue de N0a (§19.21) : EN7 du §19.7 la portait (C9) et a été retirée sans qu'elle soit reprise | aucune : le README (`cb0ee7fa…`, l. 249-256 et note de construction l. 260-267) décrit la division en plan et la profondeur des canaux, pas leur terminaison ; « en tête des glyphes » (l. 233) désigne un lieu de mesure de la face | terminaison des canaux de [S] non représentée | `classical_greek_entablature.cpp` (`TriglyphProfile`, `PrismSection`) |
| TE5 (étendue, lot N1a, 2026-11-12) | **L7 : arête du coin de la pièce d'angle sur l'arête d'onglet de la frise de l'anneau.** L'arête verticale du coin (b, b) de la section en L (EN14) et l'arête d'onglet de la face de frise de l'anneau ont les mêmes extrémités au bit près (mêmes expressions `double`, ±(H + b), cotes `float (entablatureFoot + z)` de la bande de frise) ; après soudure, `ValidateMesh` les compte pour une arête portée par quatre faces (`non_manifold_edge`, deux faces de l'anneau et deux de la pièce) et réunit la pièce et l'anneau en une composante. Un enregistrement par pièce d'angle, rien d'autre ; L4 vaut sur les deux faces de frise de l'angle | aucune ; constat de mesure (CT1, plan 3 §21.19) : conséquence de EN5 (frise de l'anneau au nu des métopes) et de EN14 (pièce posée sur l'angle de l'anneau) | contact sans traversée, comme L1 à L6 ; quatre arêtes non manifold et quatre composantes de moins sur un temple à triglyphes d'angle ; mesuré aussi sur le temple du README (lot N1b : anneau et quatre pièces d'angle réunis, 4 arêtes, une composante) | `classical_temple.cpp` (`BuildTempleMesh`) ; CT1 ; J19 |
| EN14 (lot N1a, 2026-11-12) | **Triglyphe d'angle en une pièce** (`FriezeElement::CornerTriglyph`), sous `CornerTriglyphEqualMetopes` et `CornerTriglyphMidBay` : prisme vertical d'une section en L posée sur l'angle de l'anneau, rang 0 du côté que son angle ouvre (comme la colonne d'angle) ; bras du côté ouvert [a0, a1] et bras de retour [ra0, ra1] sur le côté précédent, chacun de largeur w, bout extérieur à ± (H + F) (plan des faces des triglyphes du côté voisin). Chaque bras porte la division d'un triglyphe droit de son côté à la même place (bornes cumulées depuis a0, a croissant, la dernière égale à a1, comme N0a) ; **un seul demi-canal à l'arête, plan** (chanfrein), du bord de la bande extrême d'un bras à celui de l'autre : sa profondeur sur chaque face est la largeur de la division de bord à l'arête, et non `halfChannelDepth`. Section de 2m + 1 sommets (m points de profil d'un triglyphe droit) : dos du bras ouvert, son profil sans le point d'arête, profil du retour sans le point d'arête, dos du retour, coin (b, b) du nu des métopes. Refus sous ces deux stratégies : width ≤ F − b (`'triglyph.width'`, les deux bras ne reposeraient pas chacun sur une face de frise) ; section non simple à l'un des quatre angles (`'triglyph.divisionWidths'`) | README l. 255 (« demi-canal commun à l'arête », position du centre à F − w/2 au-delà de l'axe ; face de retour : « la même division que la face, lue depuis l'arête », chanfrein à 45° de 2,0 in sur chaque face, réponse à B9 du 2026-11-13) ; l. 265 (« un prisme à deux faces posé sur l'angle de l'anneau ») ; l. 253 (division lue sur la coupe du triglyphe d'angle) ; plan 3 §21.4 | aucune sur le README depuis la réponse à B9 (RN12 levée) : sous EN11, le chanfrein est exactement la réunion des deux demi-canaux de 2,0 in à 45° (MS-46) ; sur le jeu synthétique (bord 0,03, demi-canal 0,04), le chanfrein n'est pas à 45°, ce que la source ne dit pas. Pour une division non symétrique, le côté Back et le côté Left la portent dans le sens inverse de leur parcours, comme leurs triglyphes droits depuis N0a | `classical_greek_entablature.{h,cpp}` (`ValidateGreekEntablatureSettings`, `LayoutGreekFrieze`, `BuildTriglyphMesh`) |
| EN16 (lot N1a, 2026-11-12) | **Contraction calculée** (`ComputedCornerSpacing`) : l'entraxe d'angle qui rend, sous la stratégie des réglages, toutes les métopes d'une rangée égales à la courante s/2 − w : s sous `AxisTriglyph` ; contraction simple s − c sous `CornerTriglyphEqualMetopes`, c = F − w/2 (`CornerTriglyphOffset`) ; aucune sous `CornerTriglyphMidBay`. **Rapportée** (`ReportDoricCorners`, écart = entraxe d'angle donné − calculé), **jamais appliquée** au péristyle : la contraction donnée pilote (D3-22) | README l. 101, 128 (contraction relevée), l. 132 (le reste du conflit va aux métopes d'angle) ; D3-22 ; D3-54 | ni contraction double ni décalage réparti (D3-54, Q3-60 a) | `classical_greek_entablature.{h,cpp}` (`ComputedCornerSpacing`, `ReportDoricCorners`) |
| EN15 (lot N1b, 2026-11-12) | **Stratégie d'angle du monument : `CornerTriglyphEqualMetopes`**, posée par la conversion (`MonumentEntablatureSettings`, D3-55) : triglyphe d'angle (EN14), deux métopes égales dans chaque travée d'angle, intermédiaire décalé vers l'angle ; sur la contraction **relevée** du péristyle (`MonumentPeristyleSettings` intact, D3-22), la contraction calculée étant rapportée (EN16). Sous elle, le temple du README a 68 triglyphes, dont 4 pièces d'angle, et 68 métopes | README l. 256 : « la première solution est la règle grecque (**convention**) » : convention **de la source**, nommée par elle ; vérifiée par l'auteur sur [S] pl. VI fig. 1 (README `47d83afd…` l. 256, 13 novembre 2026) : triglyphe d'angle dessiné à l'arête, métope voisine cotée 0,774 m ; une « troisième solution » (métope d'angle de 0,774 m) n'est pas tranchée par la figure, et la convention des deux métopes égales est conservée (choix de l'utilisateur), sans sélecteur pour la troisième ; l. 249 (68 métopes), l. 255 (triglyphe d'angle), l. 265 (prisme à deux faces) | aucune en valeur : les contrôles de la l. 255-256 (`monument/v4`, 0,231, 2,632, 0,801, 0,802, 0,116 m) tiennent à la demi-unité (`monument/v5` les garde tels quels) ; ceux de la seconde solution (0,685 / 0,916, 0,686 / 0,918 m) tiennent sous `CornerTriglyphMidBay` seulement (J20). Le mot « règle grecque » n'est lu par aucun sélecteur (D3-55, Q3-61) : une autre stratégie se change dans la conversion, et J20 la fait rougir | `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN17 (lot N2a, 2026-11-13) | **Regula** : prisme droit sous la ténia (bande `friezeBand − 1`), dos contre la face verticale de la bande `friezeBand − 2`, face au réglage `regulae.face` ; une par placement de triglyphe, de même étendue `a0`, `a1` (copies) ; hauteur `regulae.height` sous le dessous de la ténia ; aucun creux au joint ténia / regula | README l. 211 (hauteur), l. 212 (« largeur de la regula (20,3 in) », celle du triglyphe), l. 231 (saillie) ; précédent de Vignole (`classical_colonnade.cpp:325-326`) | creux de la l. 230 non modélisé (B18) | `classical_greek_entablature.cpp` (`LayoutGreekOrnaments`) |
| EN18 (lot N2a, 2026-11-13) | **Regula d'angle en une pièce**, sous une pièce d'angle (EN14) : prisme d'un hexagone en L, (F − w, dos), (F − w, R), (R, R), (R, F − w), (dos, F − w), (dos, dos), en distances aux deux lignes des axes de l'angle (F face du triglyphe, w sa largeur, R face de la regula, dos face de la bande `friezeBand − 2`) ; il enveloppe l'angle de cette bande jusqu'au plan de face de la regula (README : largeur vue 21,9 in par face) ; coins écrits ±(H + R) et ±(H + dos) ; 6 gouttes par bras, sous le bras du triglyphe [F − w, F] ; carré [F, R]² sans goutte | aucune | **B12** (retour de la regula et gouttes de l'angle non décrits) | `classical_greek_entablature.cpp` (`LayoutGreekOrnaments`, `BuildOrnamentMesh`) |
| EN19 (lot N2a, 2026-11-13) | **Goutte en tronc de cône de révolution, axe vertical** ; génératrice avant au nu de la regula au pied (`gutta.footProjection`), à `gutta.topProjection` en tête ; **diamètre lu = diamètre au pied** ; diamètre de tête = diamètre − 2 × (`footProjection` − `topProjection`) ; pied au bas de la bande `friezeBand − 2`, tête au dessous de la regula (hauteur = bande moins regula, sans réglage propre) ; le pied ne pose sur rien : `friezeBand` ≥ 3, et le haut de la bande `friezeBand − 3` en deçà de l'arrière du pied des gouttes (refus sinon ; README : 19,259 in contre 19,393 in, marge 0,134 in) ; 8 côtés (format, D3-17), sommets avant, arrière et latéraux exacts | README l. 212 (Ø 1,466 in, 6 par regula), l. 232 (20,809 in en haut, 20,859 in en bas) | **B13** (Ø en tête : jeu au dos 0,034 in au lieu de 0,134) | `classical_greek_entablature.cpp` (`BuildOrnamentMesh`) |
| EN20 (lot N2a, 2026-11-13) | **Gouttes à marges égales** : bornes cumulées depuis a0 du triglyphe (ou du bras) sur {marge, Ø, vide, …, Ø, marge}, la dernière forcée à a1 ; marge = (w − envergure) / 2, envergure par sommes ; centres au milieu de leurs bornes | README l. 212 : « deux marges de 0,127 in (**convention**) » : convention **de la source** | aucune en valeur (0,127 in, MS-53) | `classical_greek_entablature.cpp` (`LayoutGreekOrnaments`) |
| EN21 (lot N2a, 2026-11-13) | **Mutules en plaques** dans la bande `mutules.band` : un centré sur chaque triglyphe (chaque bras d'une pièce d'angle, sur sa face) et sur chaque métope ; largeur réglée, **via = reste** (refus si ≤ 0) ; dos = saillie de la bande (face verticale), face `mutules.face`, dessus sous le ressaut de la bande suivante, dessous à la cote du pied de la bande, sous-face horizontale (EN2) : un dos en retrait sur la bande du dessous y pose (L14) ; **sous sous-face inclinée (EN22, lot N3a) : prisme d'un parallélogramme** (d0, z0 + rise), (d1, z0), (d1, z1), (d0, z1 + rise), plaque verticale de hauteur constante, dos sans pose sur la bande du dessous (L14 absente) ; **pas de mutule d'angle** (angle nu, précédent de Vignole, `classical_colonnade.h:68-69`) ; refus si le mutule extrême atteint le plan des dos du côté voisin. Pour le monument (N2b) : dos lu à face − profondeur, 19,949 in (D3-61) ; sous sous-face inclinée (N3b) : face − L cos θ (D3-67, amende D3-61). **Valeurs pour le README (lot N2b, 2026-11-13 ; dos, pose et dégagement remplacés au lot N3b, colonne « Écart »)** : `mutules.band` = 6 (plaque, 2,35 in) ; largeur 1 ft 8,75 in = 0,52705 m ; face 35,825 in = 0,909955 m ; dos (35,825 − 15,876) in = 19,949 in = **0,5067046 m**, calculé en pouces puis multiplié une fois par le pouce, saillie des deux bords de la bande 6 ; le mutule pose sur le listel du chapiteau du triglyphe (20,659 in) sur 0,018034 m (L14) ; bout du mutule du bras d'angle à 0,4948936 m au-delà de l'axe, dégagement 0,011811 m ; vias courantes 0,11995 m (façade) et 0,11820 m (flanc) ; 21 mutules par façade, 49 par flanc, 140 en tout | README l. 273 (règle, largeur), l. 274 (profondeur, dos, « sous une sous-face horizontale »), l. 217 et 237 (hauteur, face) | via lue 4,75 in non confrontée (MS-55, **B15**) ; mutule d'angle (**B15**) ; pente et gouttes : EN22 à EN24 (lot N3a). **Valeurs pour le README (lot N3b, 2026-11-14, D3-67)** : dos à face − L cos θ = (35,825 − 15,876 cos 14°) in = 20,420585 in = **0,5186829 m**, calculé en pouces puis multiplié une fois par le pouce, saillie des deux bords de la bande 6 ; **aucune pose sur le listel** (L14 absente) : l'anneau monte du listel (20,659 in) au dos par un ressaut de 0,238415 in = 6,0557 mm ; dégagement d'angle 0,0237893 m ; largeur, face, vias et nombre de mutules inchangés ; le contrôle `mutuleBack` (19,949 in, « sous une sous-face horizontale ») reste lu, confronté à face − profondeur seulement | `classical_greek_entablature.cpp` (`LayoutGreekOrnaments`) |
| TE5 (étendue, lot N2a, 2026-11-13) | **Contacts des ornements**, ajoutés à la liste fermée : **L8** dessus d'une regula sous le ressaut de la ténia ; **L9** dos d'une regula sur la face de la bande `friezeBand − 2` (deux faces pour une regula d'angle) ; **L10** tête d'une goutte sous le dessous de sa regula ; **L11** dessus d'un mutule sous le ressaut de la bande du dessus ; **L12** dos d'un mutule sur la face de sa bande ; **L13** (révisée par D3-64) sous une stratégie d'angle, les coins (R, R) et (dos, dos) du dessus de la regula d'angle sont, au bit près, deux sommets d'onglet de l'anneau, et la diagonale (dos, dos)–(R, R) de son couvercle, que contient la découpe d'oreilles de cet ordre de sommets (et toute triangulation si F − w + R < 2 dos, ce qui vaut pour le jeu synthétique et pour le README), est l'arête d'onglet du ressaut de la ténia : après soudure, **une `NonManifoldEdge`** (4 faces : 2 de l'anneau, 2 du couvercle de la regula) et **une composante de moins** par regula d'angle, aucun `NonManifoldVertex` ; **L14** dessous d'un mutule sur le dessus de la bande sous la sienne, là où le dos est en retrait sur elle | aucune ; constats de mesure (sonde du plan 3 §22.18, CT2) | contacts sans traversée, comme L1 à L7 ; L7 et L8 sont aussi des noms de classes retirées au §19.7 du plan 3 | `classical_temple.cpp` (`BuildTempleMesh`) ; CT2 |
| EN22 (lot N3a, 2026-11-14) | **Sous-face inclinée du géison** : pivot à l'arête avant-basse du mutule (face des mutules, cote du pied de leur bande) ; **montée réglée** (`mutules.rise`, une longueur, aucune trigonométrie dans l'anneau ni dans le mutule, D3-66) ; dans la section de l'anneau, le sommet du haut de la bande des mutules est remplacé par (Db, z[k+1] + rise) puis (P, z[k+1]), le ressaut vers la bande suivante restant à z[k+1] : face verticale de l'anneau au dos, du pied de la bande au fond des vias ; fond des vias parallèle à la sous-face, à la hauteur de la plaque au-dessus ; bec = bande suivante (lèvre verticale, dessous horizontal de la face des mutules à la saillie du bec) ; z[k+1] + rise est la même expression `double` dans l'anneau et dans le mutule ; refus si la section n'est pas simple (`IsSimple` strict). **Valeurs pour le README (lot N3b, 2026-11-14)** : pente 14° (`soffitSlope`, `mesuré` [S], unité « ° », R9), convertie en radians par θ = pente × π/180 ; pivot sur la face des mutules, 35,825 in ; montée 15,876 sin θ = 3,840752 in = 0,0975551 m ; cotes depuis le stylobate : pivot 7,37743 m, arête avant-haute 7,43712 m, dessous au dos 7,47499 m, haut du bec 7,46887 m | README `0faba870…` l. 276 (pente `mesuré`, pivot, plaque et bec `déduit`), l. 217 (bec) | gorge du bec non modélisée (B18) ; gorge du joint plaque / gouttes non construite (l. 238) ; ressaut lu 3,9 in contre 3,84 in, dans la plage de ± 1° ; contrôles de la l. 276 (7,377, 7,437, 7,475 et 7,469 m ; 3,84 in ; 35,825 in) tenus à leur demi-unité (J22) | `classical_greek_entablature.cpp` (`SectionOf`, `ValidateOrnaments`) ; `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN23 (lot N3a, 2026-11-14 ; lot N3b) | **Gouttes des mutules en rangs depuis l'arête avant**, le long de la sous-face ; **Ø lu = Ø de tête**, vides entre têtes (D3-76) ; l'avant de la tête de la goutte avant est sur l'arête ; bornes cumulées sur {Ø0, vide0, …, Ø(r−1)} depuis 0, `along` au milieu ; marge arrière = reste ; en largeur, axes extrêmes à `edgeAxis` des bords, pas égal (`EqualParts`) ; nombre par rang **lu** (`perRow`, clé `mutuleGuttaPerRow`, D3-77) ; diviseur du pas en contrôle ; les pieds (Ø + 2 c) sont jugés contre les bords et entre eux. **Valeurs pour le README (lot N3b)** : 3 rangs de 6 ; têtes 1,46 / 1,55 / 1,5 in, vides 4,4 et 4,466 in ; axes à 0,73 / 6,635 / 12,626 in de l'arête ; marge arrière des têtes 2,5 in ; axes extrêmes à 1,775 in des bords, pas (20,75 − 2 × 1,775) / 5 = 3,44 in ; 2 520 gouttes, 18 par mutule | README `0faba870…` l. 274, 275 | aucun | `classical_greek_entablature.cpp` (`OrnamentsOf`) ; `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| EN24 (lot N3a, 2026-11-14 ; lot N3b) | **Goutte de mutule : tronc de cône droit d'axe normal à la sous-face**, tête sur elle, de diamètre Ø, pied à `height` sous elle (hauteur normale), de diamètre Ø + 2 `conicity` (évasement, le même aux trois rangs) ; 8 côtés (format) ; **au README (lot N3b, D3-78)** : hauteur 1,0 in, pied plus large de 0,5 in en diamètre (`mutuleGuttaFootFlare`, `déduit` [S]), soit un **évasement radial de 0,25 in** (`conicity` = la moitié, exacte en binaire) ; sommets latéraux exacts en a ; **garde du pied conservatrice** (D3-70) : l'arrière du pied de la dernière goutte reste devant la plus forte saillie de ce qui est sous les mutules (bandes, face des triglyphes, face des regulae), et le point le plus bas des pieds au-dessus du pied de l'entablement (revue de N3a, M1), évaluées par `StrictCrossDifference` ; garde `float` des mêmes contacts au maillage | README `0faba870…` l. 275 (forme `déduit` [S]), l. 238 | avant du pied à 35,8257 in contre 35,826 écrit (l. 275, à la demi-unité) et 35,825 lu à la l. 238 (+0,0166 mm, hors de la demi-unité, non confronté) ; haut lu à 35,773 in non reproduit (tête sur l'arête, gorge du joint non construite, −1,3 mm) ; flancs concaves de la source remplacés par un tronc droit (convention de l'auteur) | `classical_greek_entablature.cpp` (`AppendInclinedGutta`) ; `classical_monument.cpp` (`MonumentEntablatureSettings`) |
| TE5 (étendue, lot N3a, 2026-11-14) | **Contacts sous sous-face inclinée** : **L11 révisée**, dessus d'un mutule sous la face inclinée de l'anneau, coplanaire au bit près (mêmes couples (d, z) `float` aux deux pièces) ; **L14** absente sous sous-face inclinée (le dessous du mutule monte vers le dos) ; **L15**, tête d'une goutte de mutule sur le dessous de son mutule, coplanaire à l'arrondi `float` près (écart mesuré 0,605 t à la tolérance par défaut du temple synthétique) ; contact ponctuel, sans enregistrement, du sommet avant de la tête avant sur l'arête avant-basse (RN30). Validé à la tolérance explicite de **1e-4 m** (D3-80), sous la garde : mêmes sommets soudés, composantes, défauts hors `SelfIntersection` et paires d'unités en contact qu'à la tolérance par défaut | aucune ; sonde du plan 3 §23.21, CT3 | contacts sans traversée ; à 1e-4, des recouvrements coplanaires minces ne sont plus signalés (`mesh_validation.h:30-32`) ; sur le temple du README (lot N3b), la même liste et la même garde, écart de la sonde 0,882 t à la tolérance par défaut | `classical_temple.cpp` (`BuildTempleMesh`) ; CT3 ; J21 (lot N3b) |

### 10.11 Fronton (lot P0a, `plan_implementation_03_revision.md` §24)

Conventions du module prises le 2026-11-14 au lot P0a, sous les décisions D3-88 (P0a sur réglages synthétiques), D3-89
(rive au dessous, coupe d'angle verticale), D3-90 (hauteurs normales, facteur s), D3-91 (comble pentagonal), D3-94
(section polygonale donnée) et D3-95 (aides strictes dans `classical::detail`). Aucune valeur n'est fixée : le jeu
d'essai `SyntheticPedimentSettings` (`test/greek_doric_test_support.h`) ne décrit aucun monument. La colonne « Sources »
cite le README du temple d'Héphaïstos `42ecd4e1…` (numéros de ligne de cette version). PE6 (bandes à faces droites) des
§24.7 et §24.21.7 est retirée par D3-94 : la section est le polygone de l'auteur (l. 402). Les choix de format (champ
`std::optional` du temple, énumérateurs `RakingCornice` et `Roof` en dernier, ordre des pièces, un seul jeu de réglages
pour les deux façades) restent dans l'en-tête et le plan (D3-17).

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| PE1 | **Corniche rampante en Λ** : section vraie **polygonale donnée** (`rakingSection`), u depuis le nu du tympan vers l'extérieur, h normale à la pente depuis la ligne de rive ; premier sommet (0, 0), dernier (0, T), T = h maximale ; dos sur u = 0 ; balayée le long de la ligne de rive, dessous (h = 0) sur la rive ; **face de bout par découpe d'oreilles** (`detail::EarClip`, polygone concave) ; onglet au plan de symétrie (section de faîte commune aux deux bras), sans couvercle. 3n sommets et 6n − 4 triangles par façade | README l. 401 (hauteurs normales, de haut en bas, `lu` [S]), l. 402 (polygone de 18 sommets, `mesuré` [S] pl. VII B ; 20,85 et 7,7 in `lu`) | quart-de-rond du filet du bas non tracé ; gorge en six cordes (celles de la source) | `classical_greek_pediment.h` (`GreekPedimentSettings`, `BuildGreekPedimentPieceMeshes`) |
| PE2 (remplacée par PE2′, lot P1) | **Ligne de rive** = dessous de la corniche rampante, de (±X_e, z11) à (0, z11 + H) ; **X_e = `halfWidth` + `bands.back ().topProjection`**, l'avant du couronnement, dérivé de l'anneau (aucun champ) ; pente sans angle : seules deux longueurs, `apexRise` et X_e ; s = `hypot (X_e, H) / X_e` (1 / cos θ), calculé une fois, pour les positions seulement (cote d'un sommet h·s au-dessus de la rive) ; toute cote partagée entre deux pièces est calculée une fois, stockée et relue | l. 399 (H, du dessus de la corniche horizontale au **dessous** de la corniche rampante dans l'axe, `lu` [S] pl. III), l. 400 (rive au dessous, égout à l'avant du couronnement, 7,273 m, `déduit` [S] pl. III) | — | idem |
| PE3 (remplacée par PE3′, lot P1) | **Coupe d'angle verticale** par le plan x = ±X_e ; face de bout dans ce plan ; **contact par une arête** : l'arête basse (h = 0) de chaque face de bout est sur l'arête haute extérieure du flanc de l'anneau, mêmes x et z au bit près (mêmes expressions) | l. 401 (« contact par une arête, sans traversée, et aucune coupe horizontale n'est nécessaire », `déduit` [S]) | — | idem |
| PE4 (remplacée par PE4′, lot P1) | **Comble plein pentagonal** de 10 sommets et 16 triangles : base sur z11 de −X_e à X_e ; murs verticaux à x = ±X_e jusqu'à l'égout z11 + V, V = T·s ; pans par l'arête haute du dos de la corniche rampante, coplanaires à son dessus ; abouts = tympans, au nu du tympan (y = ∓(halfDepth + u_t)) ; ses sommets hauts sont les cotes stockées des Λ (`eave[T]`, `ridge[T]`) ; ni sima, ni acrotères, ni tuiles (les murs sont la place de la sima) | l. 406 (« prisme de section pentagonale, soit dix sommets » ; convention de l'auteur, la toiture étant `manquant` ; ligne coupée en cinq cellules, B29) | toiture sans cote : convention de l'auteur ; ne lit pas la l. 406 tant que B29 est ouverte (P0b) ; **B29 levée le 2026-11-14, l. 406 lue au lot P0b (`pediment.sima`, `roofEaveRise`)** | idem |
| PE5 | **Deux frontons identiques**, Front (Y < 0) et Back, un seul jeu de réglages ; le Back est le miroir exact du Front (y opposé au bit près, faces retournées) | le README ne décrit qu'un fronton | — | idem |
| TE5 (étendue, lot P0a, 2026-11-14) | **Contacts du fronton**, ajoutés à la liste fermée, mesurés par la sonde (plan 3 §24.25.4) avant le code : **L16**, dessous du comble sur le dessus de l'anneau (coplanaires à `float (zFoot + z11)`, signalé) ; **L17**, dos (u = 0) de chaque bras sur l'about du comble (plan y = ∓`float (halfDepth + u_t)`, signalé) ; **L18**, arête basse de la face de bout de chaque bras sur l'arête haute extérieure du flanc de l'anneau, mêmes x et z au bit près : **contact linéaire, non signalé** (`mesh_validation.h:28-41`). **8 arêtes non manifold** par temple : par fronton, les deux arêtes haut-dos (faîte → bout) et les deux arêtes verticales de coin d'about, chacune portée par 4 faces (Λ et comble) ; **0 `NonManifoldVertex`** ; le comble et les deux Λ ne font qu'**une composante**. Non signalés (aire nulle) : pans du comble et dessus de la corniche rampante ; murs du comble, faces de bout et face du couronnement du flanc. Sonde : synthétique et README, L16 12, L17 7 par façade, 0 hors liste, sous MSVC et sous Clang FMA | construction (§24.25.4) | sans traversée | `classical_temple.h` |

Le lot P0b (2026-11-14, plan 3 §24.28, décisions D3-94 et D3-96, Q3-86 → b, §24.27) applique PE1 à PE5 au temple du
README (document `monument/v7` : 8 clés et 14 contrôles des l. 399 à 402 et 406) et étend MO3 (R11) ; citations au
README `85f00b96…` (458 lignes, mêmes numéros que `42ecd4e1…` ; seule la l. 406 diffère, B29 répondue).

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| MO3 (étendue, lot P0b, 2026-11-14) | **R11**, liste de sommets : dans la cellule de valeur, une amorce unique (« Ordre : depuis le bas du dos, vers l'avant, puis par le dessus : ») et une fin unique (« , puis retour au premier sommet. »), entre elles des sommets « u NUM h NUM » séparés par « → », chacun lu en entier ; **l'unité est donnée par la phrase « en <unité>. » qui précède immédiatement l'amorce** (« en pouces. » → « in »), au lieu de suivre la valeur ; nombre de sommets exigé (18) ; statut par l'étiquette R1 « section en polygone » de la cellule de statut. Valeurs recopiées en texte, ordre conservé, aucune somme ; le document les range en deux listes `u` et `h` | README l. 402 (« … en pouces. Ordre : … : u 0 h 0 → … → u 0 h 7,7, puis retour au premier sommet. » ; « Section en polygone : mesuré [S] pl. VII B ») | une unité reformulée (« en millimètres. »), un sommet de moins, un sommet sans « h », un statut ou une étiquette changés font **refuser** le script, clé `pediment.rakingSection` nommée (témoins NC68 à NC72, plan 3 §24.28) ; deux sommets permutés (NC73) ou un talon déplacé (NC75) sont lus sans refus : la simplicité est jugée par la bibliothèque (`ValidateGreekPedimentSettings`), les cotes par les tests (J17, J19, J23) | `src/cgarchi/data/temple_hephaistos_from_readme.py` |
| PE1 à PE5 (valeurs du README, lot P0b, 2026-11-14) | **Fronton du temple du README** (`MonumentPedimentSettings`, posé par `MonumentTempleSettings`) : H = (5 × 12 + 9,75) in × pouce = 1,771650 m ; u_t = 18,125 in × pouce = 0,460375 m ; section = les 18 sommets lus, (u × pouce, h × pouce), ordre conservé ; X_e dérivé de l'anneau (7,272965 m) ; pente atan (H / X_e) = 13,6903° ; V = T s = 0,201299 m (7,9252 in), égout à z11 + V. **PE4 ne vaut que pour une sima manquante** : la conversion refuse une `pediment.sima` donnée, comme EN1 pour la face intérieure (une sima cotée demanderait sa propre construction). La suite des hauteurs (l. 401) n'entre pas dans la géométrie : elle n'est que confrontée au polygone (J17) | README l. 399 (H, `lu` [S]), l. 401 (hauteurs, `lu` [S]), l. 402 (u_t `déduit` [S] ; polygone `mesuré` [S] ; 20,85 et 7,7 in `lu`), l. 406 (sima, acrotères, tuiles `manquant` ; comble de dix sommets, convention de l'auteur) | quart-de-rond du filet du bas non tracé, gorge en six cordes (source) ; borne cumulée 6,30 in (filet au nu du larmier) absente des h du polygone ; contrôles 7,93 in et 0,201 m (l. 401, l. 406) tenus à leur demi-unité, le premier à 0,00016 in près en double (RN35) | `classical_monument.cpp` (`MonumentPedimentSettings`, `MonumentTempleSettings`) |

**Lot P1 (2026-11-17, plan 3 §26 ; décisions Q1 à Q6 de l'utilisateur).** Reprise de Dinsmoor 1941 ([O]) : priorité
[O] > [D] > [S] > [W] > [L] pour l'existant, [S] seule source des détails. PE2, PE3 et PE4 sont **remplacées** par
PE2′, PE3′ et PE4′ ; KR3 est amendée ; TE5 L18 est réécrite ; MO3 reçoit R12 ; MO reçoit l'unité `ratio`. Document
`monument/v8`. Citations au README `a40192f8…` (§ 3.2 l. 86 à 93, § 9 l. 608 à 616 : hauteur l. 608, pente l. 609, comble l. 616).

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| PE2′ | **Pente donnée par r** (`rakingRun`, pente 1 : r) et **hauteur au haut** : `apexRise` = H_t, de z11 au **haut** de la corniche rampante (h = T) dans l'axe ; s = `hypot (r, 1) / r` ; H_b = H_t − T s ; rise[k] = H_b + h_k s ; ridge[k] = zBase + rise[k] ; foot[k] = r rise[k] ; xTop = foot[T], calculés une fois dans le repère et relus ; X_e (avant du couronnement) n'est plus qu'une **borne** (refus `'pediment.rakingRun'` si xTop > X_e) ; refus `'pediment.apexRise'` si H_b ≤ 0 | README § 9 l. 609 (« 1 : 4,0846 », `lu` [O] p. 84, n. 181) ; l. 608 (5 ft 9,75 in jusqu'au **haut**, `lu` [S], tranché le 2026-11-17) | xTop du README 7,23648 m (r × 1,77165, mesuré au maillage, J23) contre 7,238 imprimé pour 1,772 × 4,0846 : écart de 1,5 mm, sous la demi-unité de 1,772 propagée par r (J23) | `classical_greek_pediment.h` |
| PE3′ | **Coupe d'angle horizontale** par le plan z = zBase (dessus de l'anneau, même expression `zFoot + z11`) ; **face de pied** de chaque bras dans ce plan, découpe d'oreilles de la section (image affine), normale −z ; sommets de pied (±foot[k], y_k, zBase) au bit près ; aucune coupe verticale | README § 9 l. 610 (« coupe horizontale », option (a) de Q3-78, `déduit` de [O]) | la face de pied n'a pas de station (réserve R-P1, `debt_cgarchi.md`) : écart mesuré au README raffiné 7,1e-5 m < τ_R / 2 | idem |
| PE4′ | **Comble triangulaire** de 6 sommets et 8 triangles : base sur z11 de −xTop à xTop, pans au faîte (0, ridge[T]) ; abouts = tympans ; ses six sommets sont, au bit près, les sommets T (pieds et faîte) des deux Λ ; ni sima, ni acrotères, ni tuiles | README § 9 l. 616 (« comble plein, prisme de section triangulaire, soit six sommets », côtés verticaux de 0 m) | le contrôle `roofEaveRise` (0 m) n'est plus lu : il ne contrôle plus rien | idem |
| TE5 L18 (réécrite, lot P1) | **L18 : face de pied de chaque bras à plat sur le dessus de l'anneau**, coplanaires au bit près, **signalé** par `ValidateMesh` comme L16 ; remplace le contact linéaire de PE3. **4 arêtes non manifold** par temple (par fronton, les deux arêtes haut-dos, 4 faces chacune) au lieu de 8 ; 2(m + 2) avec m stations sur la trajectoire T | construction | mesuré : 4 (CT4, J23), 36 au synthétique raffiné (m = 16, CT5) | `classical_temple.h` |
| KR3 (amendée, lot P1, Q6) | **Euthyntéria = assise pleine** de `euthynteriaHeight` (0,37 m) à **ressaut constant** `euthynteriaTread` (0,11 m) sur toute sa hauteur et sur les deux axes, dernier élément de `lowerSteps` ; soumise au champ de courbure du § 8 comme les degrés ; pied de la krépis à son pied | [O] p. 31 (hauteur), p. 33 (ressaut) ; **[O] ne relève le ressaut que sur la partie haute dressée** (0,145 à 0,16 m), la partie basse brute saillant de 0,01 à 0,08 m de plus | partie basse : convention de l'utilisateur (Q6), en dette | `classical_monument.cpp` (`MonumentKrepisSettings`) |
| MO3 (étendue, lot P1) | **R12**, rapport « 1 : nombre » : unité `ratio`, le motif écrit « 1 : » juste devant son unique groupe décimal, la valeur recopiée est le nombre de droite | README § 9 l. 609 (« **1 : 4,0846** », en tête de cellule) | une unité `ratio` dont le motif ne précède pas son groupe de « 1 : » fait refuser la table des sélecteurs | `temple_hephaistos_from_readme.py` |
| MO (unité, lot P1) | **Unité `ratio`** dans la liste fermée (sept unités) : réel > 0, réservée aux rapports (`MonumentRatio`) ; refusée sur une longueur, et toute autre unité refusée sur un rapport | — | — | `classical_monument.h` |
| RF5 (amendée, lot P1) | **Seconde famille, fronton** : les ±foot[k] des n trajectoires, écart `kRefinementClearance` max (1, r), remplacent les deux bouts ±X_e ; refus nommant `'pediment.rakingRun'` quand ils seuls allongent le plus long bloc | aucune : conséquence de PE3′ (un nœud de la trajectoire k à dx de son pied n'est qu'à dx / r au-dessus du dessus de l'anneau) | bornes basses calculées relevées : synthétique 7,891e-7, CT6 6,594e-7, README raffiné 7,931e-7 (2,40e-7 avant) ; CT8 n'échantillonne qu'au-dessus | `classical_temple.cpp` (`StationClearanceOf`) |

### 10.12 Raffinements (lot R0a, `plan_implementation_03_revision.md` §25)

Conventions prises le 2026-11-15 au lot R0a, sous les décisions D3-97 à D3-103 (§25.18, ratifiées au §25.22.3) : courbure
par un champ vertical (D3-98), inclinaison par cisaillement à pied fixe (D3-99), champ sur toutes les pièces (D3-100),
stations des pièces longues (D3-101), contacts sous raffinement (D3-102), API (D3-103). Aucune valeur n'est fixée : le
jeu d'essai `SyntheticRefinementSettings` (`test/greek_doric_test_support.h`) ne décrit aucun monument ; le temple du
README n'est pas raffiné avant R0b. La colonne « Sources » cite le README du temple d'Héphaïstos `1fe65d8a…` (mêmes
numéros de ligne que `85f00b96…`). RF6 (contacts) et l'extension de TE5 sont écrites après la sonde P2 (§25.24) ; RF7
et RF8 appartiennent à R0b.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| RF1 | **Courbure** : champ vertical Δz(X, Y), carreau de Coons des quatre paraboles de bord, paramétré par les flèches, nul aux angles du stylobate (X = ±a, Y = ±b, demi-côtés du stylobate), défini et prolongé sur tout le plan | l. 64, 381-384, **391** (convention de l'auteur, formule écrite, Q3-90) ; [D] p. 120 (arc) | parabole au lieu de l'arc de [D] (≤ 5,75e-8 m, MS-112), adoptée par la l. 391 ; formule de la l. 391 dans le repère du README, ramenée au repère D2-5 par MO2 (MS-123) ; prolongement négatif au-delà du stylobate (−4,7 mm au coin de la corniche, MS-115), accepté par la l. 391 | `classical_temple_refinement.h` (`TempleRefinementLift`) |
| RF2 | **Portée** : le même champ pour toutes les pièces, krépis entière, colonnes, entablement et ornements, fronton, comble | **l. 385** (règle `lu` [D] p. 119 ; portée en convention, Q3-92) | — | `classical_temple.cpp` (carte des sommets) |
| RF3 | **Ordre** : translation, cisaillement (colonnes), puis champ évalué sur la position horizontale finale, un seul arrondi `float` | l. 66, 391 | — | idem |
| RF4 | **Inclinaison** : cisaillement horizontal à pied fixe, rapporté à la hauteur de la colonne (H = `float (height)` ; 5,715 m au README, l. 93) ; sommet de l'abaque déplacé de i vers l'intérieur, perpendiculairement à la rangée (Front : +Y ; Back : −Y ; Right : −X ; Left : +X) ; angle : i sur les deux axes (i√2 en diagonale) ; lits extrêmes horizontaux après le cisaillement, puis déplacés par le champ | **l. 129, 136, 388, 389** (conventions de l'auteur, Q3-91) ; [D] p. 119-120, 123 | la l. 136 écrit la base 5,715 ; la bibliothèque prend `column.height`, égale au README ; joints de tambours non représentés (l. 389) ; « horizontaux » de la l. 389 lu avant le champ (B37) ; TE4 élargie de \|i\| | `classical_temple.{h,cpp}` (`TemplePlacement::inclinationX`, `inclinationY`) |
| RF5 | **Stations** : krépis, anneau, Λ et comble re-tessellés par des plans verticaux X = s, Y = s, à pas uniforme p = h √(4 e\* / s_max) par axe, N = ⌈ℓ / p⌉ sur ℓ = largeur du degré le plus bas ; deux familles S et S′ (milieux), déplacées dans ±p/4 à plus de p/16 des coordonnées structurelles (axes des colonnes, contours des degrés, section d'origine de l'anneau, polygone rampant, bords des triglyphes, regulae et mutules, faîte x = 0), S′ après S. **Anneau** : arêtes horizontales de la section coupées dans leur largeur au pas min (p_X, p_Y), sauf aux cotes des pièces d'angle ; sommets insérés déplacés hors des coordonnées interdites en u (section, faces des ornements, polygone rampant, nœuds de S′, sommets diagonaux de la calotte d'angle) ; chaque boucle prend les stations de S à plus de p/16 de son coin ; fermeture éclair à clés entre boucles voisines | construction (§25.4, §25.26.2) | écart de corde ≤ e\* le long des stations, ≈ 2e\* dans une cellule ; calottes des colonnes non raffinées (MS-117) ; sous-faces inclinées de l'anneau non coupées (≤ 2,6e-5) ; au-delà du degré le plus bas, aucune station (E4, RN46) | `classical_temple_refinement.{h,cpp}`, `classical_krepis.cpp`, `classical_greek_entablature.cpp`, `classical_greek_pediment.cpp` |
| RF6 | **Contacts sous raffinement** : verticaux et arêtes exacts ; horizontaux et inclinés validés à τ_R par jeu (synthétique 1e-3, README 5e-4, valeurs de P2), écart max \|z_dessus − z_dessous\| ≤ τ_R / 2 ; garde : sommets soudés, composantes et défauts égaux au même maillage champ retiré, ensemble des paires d'unités égal au temple sans raffinement | construction, sondes P2 et P3 (§25.24.7, §25.26) | — | `classical_temple.h` (contrat), `test/tu_cgarchi_classical_temple.cpp` (CT5, CT6) |
| TE5 (étendue) (lot R0a, 2026-11-15, après P2′, plan 3 §25.28) | Sous raffinement : NME = 4m + 4 au fronton (36 au synthétique, 52 au README), plus 8 L7 / L13 au README ; 0 NMV ; contacts des seules classes du §25.5 | P2 | — | `classical_temple.h` |
| RF5 (complétée, revue de R0a, 2026-11-15, plan 3 §25.29) | **Déplacement total des stations** : sans candidat à plus de p/16 des coordonnées interdites, repli sur le point de la fenêtre ±p/4 le plus éloigné d'elles (milieu d'un intervalle libre ou bord de fenêtre) ; **plancher** de ce repli δ_min = 8 FLT_EPSILON max\|coordonnée\| (au moins 8 ulp `float` à cette échelle, et deux fois le terme en ulp de `DefaultWeldTolerance`, 4 FLT_EPSILON max\|x\| : une station ne se confond avec une coordonnée ni à l'arrondi `float` ni à la soudure par défaut ; 4,2e-6 m au synthétique, 1,6e-5 m au README) ; sous le plancher, refus `'refinement.chordDeviation'` dans `LayoutTemple`. **Sommets diagonaux** du dessus de l'abaque d'angle (U) à h / 512 près, h la demi-largeur du dessus de l'abaque (1,3e-3 et 1,1e-3 m, la plus grande tolérance de D3-102 à l'échelle des deux jeux). **Majorant des faces** des pièces à stations comparé à `kTempleMaxFaces` dans `LayoutTemple` | construction (revue de R0a, H1, H2, L4) | le repli rapproche une station d'une coordonnée structurelle (jusqu'au plancher) ; un e\* dix fois plus grossier que le jeu synthétique donne des écarts de contact au-delà de 1e-3 (§25.29) | `classical_temple_refinement.cpp`, `classical_temple.cpp` |
| RF5 et RF6 (bornes, Q3-99 → a2, 2026-11-15, plan 3 §25.31 à §25.37) | **τ_R,max = 1e-3 m** (`kRefinementMaxTolerance`, le plus haut palier de D3-102 : au-delà, la soudure de `ValidateMesh` confond les petites pièces, 192 paires perdues à 1,5e-3 sur un maillage sain, §25.31). Deux bornes des réglages en dérivent, refusées avant tout maillage : **`chordDeviation` ≤ τ_R,max / 10 = 1e-4 m** (l'écart des contacts vaut jusqu'à 4,4 e\*, §25.33 : 2 × 4,4 × 1e-4 ≤ 1e-3 ; `ValidateTempleRefinementSettings`) ; **calotte de l'abaque, 1,1 F2 ≤ τ_R,max / 2 = 5e-4 m**, F2 = (\|f_XX\| + \|f_YY\| + 2 \|f_XY\|) h² / 8, h demi-côté du dessus de l'abaque, courbures du carreau de Coons majorées sur l'emprise des abaques, cisaillement compris (F2 majore la calotte mesurée à 0,96 – 0,995, §25.35 ; 10 % de marge ; `LayoutTemple`, champs des quatre flèches) ; décisions par produits arrondis (lecture `volatile`) | construction, sondes `probe_tau`, `probe_bound`, `probe_cap` | synthétique 1,1 F2 = 3,57e-4, README 5,9e-5 ; flèches synthétiques × 2 refusées (l'écart mesuré, 5,96e-4, violerait la règle) | `classical_temple_refinement.{h,cpp}`, `classical_temple.cpp` |
| RF5 (complétée, revue ciblée des bornes, 2026-11-15, plan 3 §25.38) | **Pas à courbure effective** : s_X = max (s_F, s_B) + \|s_R − s_L\| a / b, s_Y = max (s_R, s_L) + \|s_F − s_B\| b / a (le terme croisé f_XY ramené à l'axe ; sans lui, une seule flèche laisse un axe sans station sous un champ tordu, écart jusqu'à 1,55e-3) ; **borne basse** p / 16 ≥ τ_R,max sur chaque axe à pas fini (`'refinement.chordDeviation'`) ; **borne de calotte** : \|f_XX\| et \|f_YY\| pris aux bouts de l'emprise, max (\|(1 − v) s_F + (1 + v) s_B\|, \|(1 + v) s_F + (1 − v) s_B\|) / a² ; **coordonnées interdites** complétées par tous les points du profil des triglyphes (bornes des divisions, creux des canaux, bras de retour) et, avec des pièces d'angle, F − w et la face de la regula ; plancher des coupes de l'anneau à l'échelle de ses sommets (demi-distance + max\|u\|) | revue ciblée (N1, N2, LOW 1, LOW 2), sonde `r0a/n1/probe_n1.cpp` sur jeux asymétriques, `probe_weld.cpp` | coefficient 1 du terme croisé : mesuré (c = 0,5 suffisait aussi) ; écart max des jeux asymétriques à la limite 3,36e-4 | `classical_temple_refinement.cpp`, `classical_temple.cpp` |
| RF5 (complétée, R0a′, 2026-11-15, plan 3 §25.41 et §25.42) | **Seconde famille d'interdites, garde construite** : x et y des sommets du pied (z minimal) et du dessus de l'abaque (z maximal, décalé de l'inclinaison) de chaque colonne, arrondis comme `AppendMapped`, sur l'axe de sa rangée (Front et Back : X ; Right et Left : Y ; angle : les deux). Un nœud ne se soude à un sommet que s'il en est proche sur x **et** sur y : écarter la station d'un seul axe suffit. **Seuil** `kRefinementClearance` = 2 τ_R,max = 2e-3 m, au-delà de toute tolérance de D3-102 et distinct de δ = p/16. **Contrainte dure** sur S (S′ porte les dessous des boîtes, sous aucune colonne) : candidat et repli hors des blocs fermés [g − θ, g + θ] fusionnés ; repli dans les seuls morceaux de la fenêtre hors des blocs, rentré de min (plancher, longueur / 4) ; sans morceau, refus `'refinement.chordDeviation'`. **Borne basse calculée**, dans `LayoutTemple` après la calotte : p/2 > plus long bloc fusionné de la famille, sur chaque axe à pas fini. Une fenêtre plus longue que tout bloc contient un point à plus de θ de la famille. Les deux maillages de colonne y sont construits, pour que `ValidateTempleSettings` refuse ce que la construction refuserait | décision de l'utilisateur (§25.41) ; sonde `r0a/p2d/probe_free.cpp` : fenêtres pleines pour la seconde famille sous 1e-6 seulement, non monotones (§25.42) | plus long bloc : 1,745e-2 m au synthétique, 1,224e-2 pour CT6, 2,178e-2 au README ; bornes basses calculées, retrouvées par CT8 : 4,25e-7 au synthétique, 2,09e-7 pour CT6, 2,40e-7 au README (§25.42) ; la première famille reste molle (repli jusqu'au plancher `float`, dette §2.14) | `classical_temple_refinement.{h,cpp}`, `classical_temple.cpp` |
| RF5 (complétée, Q2 de la troisième revue ciblée, 2026-11-15, plan 3 §25.44) | **Bouts de l'égout dans la seconde famille** : sur X, ±x_Eave (x_Eave = halfWidth + topProjection de la dernière bande), écart **θ max (1, x_Eave / apexRise)**. Un nœud du Λ à Δx du bout n'est qu'à Δx · apexRise / x_Eave au-dessus du nœud de la base du comble à la même station (pente 0,255 au synthétique : 7,8 mm d'écart exigé) ; le facteur 1 couvre aussi le coin du comble, à Δx. Écart propre à chaque coordonnée (`ClearanceCoordinate {at, distance}`) ; sans fronton, pas de bout d'égout ; la pente n'est jamais au dénominateur (apexRise > 0, validé par le fronton). L'emprise entre dans la **borne basse calculée** | revue ciblée Q2 (paires Λ / comble à 8,85e-4 au synthétique à 5e-7) ; sonde `r0a/q2/probe_q2.cpp`, compteur de paires sur toutes les parties jusqu'à 2e-3 | bornes basses : voir §25.44 ; aucun autre couple de pièces dans la sonde | `classical_temple_refinement.{h,cpp}`, `classical_temple.cpp` |
| RF5 (complétée, quatrième revue ciblée, 2026-11-15, plan 3 §25.45) | **Plancher dur du repli des stations** = max (plancher `float`, kRefinementMaxTolerance) ; refus `'refinement.chordDeviation'` « … a plus de kRefinementMaxTolerance ». Les coupes de l'anneau gardent le plancher `float`. **Borne basse calculée des deux familles** : le plus long bloc fusionne la seconde famille et la première, celle-ci à rayon kRefinementMaxTolerance. Quand seul le bout de l'égout allonge le plus long bloc, le refus nomme aussi `'pediment.apexRise'` (« fronton trop plat ») | LOW 1 et LOW 2 de la quatrième revue ciblée ; sonde `r0a/l1/probe_l1.cpp`, 46 points acceptés : distance minimale à la première famille 1,99e-3 | bornes basses inchangées (4,25e-7, 3,44e-7, 2,40e-7) ; plancher à 2 τ_R,max écarté, il refuserait CT6 à sa borne (dette §2.14) | `classical_temple_refinement.cpp`, `classical_temple.cpp` |
| KR1 (amendée, lot R0a) | « ni courbure (R0) » devient : **dessus horizontal sans raffinement ; sous raffinement, RF1 et RF2** (dessus et dessous en grilles sur S et S′, côtés en bandes) | l. 385 | — | `classical_krepis.h` |

Le sous-lot R0p (2026-11-16, plan 3 §25.48, exécution §25.50, décisions D3-123 à D3-132) n'inscrit **aucune
convention** : la géométrie est identique au bit près à celle de `ef564ce` (seize empreintes C8 et trois raffinées), et
les refus du temple, leurs messages et leur ordre sont ceux du contrat de `LayoutTemple` (`classical_temple.h`), figés
par TP1 (entablement, puis EN10, puis bornes des bandes, puis fronton, puis raffinement). Ses choix sont
d'implémentation et restent dans le plan (D3-17) : preuves de validation internes (`classical_greek_entablature_detail.h`,
`classical_greek_pediment_detail.h`), compteurs `thread_local` de validations lus par les tests (Q3-102 a, D3-128,
dette §2.15), tableaux de travail des ornements réutilisés d'une pièce à la suivante (D3-127).

### 10.13 Mode partiel (lots B1 à B6, `plan_implementation_03_revision.md` §27)

Conventions prises le 2026-11-18, sous les décisions Q1 à Q10 de l'utilisateur (§27.1) : le moteur ne construit que les
parties d'un monument complètement renseignées ; le temple d'Arès (krépis seule) rejoint le temple d'Héphaïstos.
Format `monument/v9` (`src/cgarchi/schemas/monument-v9.schema`) ; API `MissingMonumentKeys`, `BuildMonumentMesh`,
`EmbeddedMonument` (`classical_monument.h`). Rien n'est inventé (option a) : une partie sans valeur reste absente.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| MO-P1 | **Complétude** : une partie est complète quand son bloc est présent et que toutes les clés que sa conversion **consomme** sont renseignées, et non quand aucune valeur ne manque. Table explicite : krépis 10 (toutes sauf `cornerMargin`), péristyle 6, colonne 21 (toutes sauf `grooveDepth`), entablement 45 (toutes sauf `architraveHeight`, `corniceHeight`, `innerFace`, `metopeWidth`, `channelDepth`), fronton 4 (`apexRise`, `rakingRun`, `tympanumFace`, `rakingSection`), cotes en pieds et pouces comptées pour une clé chacune depuis `monument/v10` (§10.14 ; 48 et 5 sous `monument/v9`). Une clé que la conversion exige **manquante** (gardes GD3d, PE4 : `columns.grooveDepth`, `pediment.sima` ; ~~EN1, `entablature.innerFace`~~, clé facultative à repli depuis le lot C4a, CE-32) n'est pas consommée ; parties EastCeiling (15 clés) et FlankCeilings (8) depuis le lot C4d (CE-33) | conception validée du 2026-11-18 ; table mesurée par le test croisé P1 dans les deux sens (84 clés listées mises à `manquant` : refus nommé ; 6 non listées : réglages identiques au bit près ; 88 et 10 sous `monument/v9`) | une partie complète dont une garde n'est pas manquante est refusée par sa conversion (dette) | `classical_monument.{h,cpp}` (`MissingMonumentKeys`) |
| MO-P2 | **Blocs** : `krepis`, `peristyle`, `columns`, `entablature`, `pediment` facultatifs ; bloc absent = partie non documentée (valeurs au défaut, manquantes ; `MonumentData::present`) ; bloc présent = toutes ses clés, `manquant` permis, règles v8 inchangées. `columnControls`, `entablatureControls`, `pedimentControls`, `krepisControls` refusés sans leur partie ; `controls` présent si et seulement si `peristyle` l'est, axes exigés, sommes `frontSum`/`flankSum` facultatives ; nouveau bloc `krepisControls` (emprise de l'euthyntéria, `footprintWidth`, `footprintLength`) | conception validée du 2026-11-18 | `controls` sans péristyle refusé : choix pris faute de règle écrite (les sommes portent sur le péristyle) | `classical_monument.cpp` (`ReadDocument`), `monument_from_readme.py` (`block_errors`) |
| MO-P3 | **Dépendances** : krépis ← péristyle (axes) ← colonnes ← entablement ← fronton. Une partie n'est construite que si elle est complète et que toutes ses dépendances le sont ; sinon `Blocked`, `blockedBy` = première dépendance non complète depuis la krépis. Un bloc absent reste `Absent`, un bloc incomplet `Incomplete`, quelles que soient ses dépendances | conception validée du 2026-11-18 | — | `BuildMonumentMesh` |
| MO-P4 | **Péristyle sans géométrie propre** : construit avec les colonnes (`Built`) ; sans elles, aucune pièce | conception validée du 2026-11-18 | — | idem |
| MO-P5 | **Péristyle validé par `LayoutPeristyle` seul** sans colonnes (`CompleteNotBuilt`) ; TE3 et TE4 (`LayoutTemple`) ne sont contrôlés qu'avec les colonnes | conception validée du 2026-11-18 | TE3/TE4 non contrôlés pour Arès (dette) | idem |
| MO-P6 | **Aiguillage** : colonnes construites → `TempleSettings` des parties construites puis `BuildTempleMesh`, exactement `BuildTempleMesh (MonumentTempleSettings (data))` si les cinq le sont (Héphaïstos au bit près) ; sinon krépis construite → `BuildKrepisMesh`, une partie `KrepisStep` par degré ; sinon maillage vide | conception validée du 2026-11-18 | préfixe krépis comparé en `float ==` (RP1 : `-0.0f + 0.0` → `+0.0`) | idem |

### 10.14 Unités de longueur (lots U1 à U3, `plan_implementation_03_revision.md` §28)

Conventions prises le 2026-11-18 sous la décision de l'utilisateur : le document garde chaque valeur dans l'unité de sa
source, le moteur la convertit en mètres, qui restent la référence interne ; le moteur n'est pas passé en millimètres.
Format `monument/v10` (`src/cgarchi/schemas/monument-v10.schema`) ; API `MonumentLength`, `MonumentMetres`
(`classical_monument.h`).

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| U-1 | **Ensemble fermé des unités** : `m`, `mm`, `in`, `ft`, `ft+in` pour une longueur ; `1` (compte), `°` (angle), `ratio` (rapport) hors longueurs. `unit` reste obligatoire sur chaque valeur, manquante comprise (unité attendue) | décision de l'utilisateur du 2026-11-18 | — | schéma `lengthUnit`, `length` ; `ReadUnit` |
| U-2 | **Référence interne en mètres** : toute longueur est convertie en mètres à sa lecture par une conversion (`MonumentLength`, entrée unique) ; jamais dans le script, jamais dans le document | décision de l'utilisateur du 2026-11-18 | option « moteur en mm » écartée par l'utilisateur | `MonumentLength`, `MonumentMetres` |
| U-3 | **`ft` décimal** : unité de longueur ordinaire, lue partout où une longueur l'est (valeurs, suites, polygones, contrôles, profils) ; 1 ft = 12 in = 0,3048 m, un seul produit | README d'Héphaïstos l. 31 (pied anglais = 12 pouces de 0,0254 m) | **pied de Penrose pris pour le pied international** : les pieds de Penrose et de Stuart et Revett sont des pieds anglais, que la définition de 1959 a fixés à 0,3048 m ; l'écart avec le pied anglais d'avant 1959 (≈ 9e-7 en relatif **[E]**, d'après le yard britannique de 0,9143992 m) est ignoré | `MonumentMetres` |
| U-4 | **`ft+in`** : une cote en pieds et pouces est **une** valeur, `"value": [pieds, pouces]`, `"unit": "ft+in"` ; pieds entiers ≥ 1, pouces finis ≥ 0 ; conversion `(pieds × 12 + pouces) × 0,0254`, le produit par 12 exact et le produit par le pouce en dernier (au bit près, insensible à la contraction FMA, plan 3 MS-40). Réservée aux valeurs de longueur : ni suite, ni polygone, ni contrôle, ni profil (un nombre par élément). Remplace les 8 paires `…Ft`/`…In` de `monument/v9` | règle R6 du script (« 2 ft 8,55 in ») ; conception validée par l'architecte | restriction aux valeurs : choix de format, faute de cote en pieds et pouces dans une suite des README | `DocumentReader::FeetAndInches`, `WrittenLength` ; `monument_from_readme.py` (`FEET_INCHES`) |
| U-5 | **Calcul dans l'unité commune** : un calcul sur plusieurs longueurs (aujourd'hui le dos et la montée des mutules, `MonumentEntablatureSettings`) se mène dans l'unité des opérandes s'ils ont la même ; deux cotes `ft+in` se calculent en pouces ; le facteur de conversion ne s'applique qu'une fois, en dernier (`MonumentMetres`). Opérandes d'unités différentes : chacun d'abord converti en mètres. Les tolérances restent des longueurs de référence en mètres (`kCapitalPartitionTolerance` = 1e-9 in) | conception validée par l'architecte | des opérandes d'unités mixtes ne donnent pas le résultat au bit près de l'unité unique (deux arrondis de plus, < 1e-12 m mesuré, test des mutules) — dette §2.17 | `InCommonUnit` |

### 10.15 Temple du Parthénon (krépis), `plan_implementation_03_revision.md` §29

Conventions prises le 2026-11-18 pour `data/temple_parthenon.json` (README `Parthenon/README.md`, source de référence
Penrose 1888, [P]), monument partiel du type d'Arès : krépis construite, péristyle validé, colonne, entablement et
fronton absents. La PA-1 est portée par les données (statut « convention ») ; elle figure ici sur demande de
l'utilisateur, avec le choix qui la fonde.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| PA-1 | **Euthyntéria de hauteur uniforme, 0.858 ft** (0,262 m) : la valeur de l'angle nord-est, retenue sur tout le pourtour ; ligne « hauteur retenue » ajoutée au § 3.2 du README, statut « convention », la ligne lue (variable) restant « lu » | [P] p. 17, 19, 31 : .858 ft à l'angle nord-est ; .99 à .89 ft à l'ouest, .585 en tournant sur la façade est, .43 au sud | hauteur réelle de −0,131 m (sud) à +0,040 m (ouest) autour de la valeur retenue ; la hauteur des quatre assises, 6.058 ft = 1,846 m, est celle de [P] à l'angle nord-est | README § 3.2 ; `krepis.euthynteriaHeight` |
| PA-2 | **Débord du stylobate au-delà du fût, 0,046 m, emprunté à [H]** (Dinsmoor 1968, p. 176), [P] ne le donnant pas ; il fixe les entraxes d'angle déduits du README (12.081 et 12.145 ft) | README § 3.2 et § 11, point 2 | contrôle par le segment d'angle de Lloyd : 12.127 ft contre 12.081, 1,4 cm (README § 12) ; `cornerMargin` n'est pas consommé sans colonnes (MO-P1) : aucun effet sur la géométrie d'aujourd'hui | `krepis.cornerMargin`, `peristyle.frontCornerSpacing`, `flankCornerSpacing` |
| PA-3 | **Cotes de [P] lues en pieds décimaux**, la valeur entre parenthèses (« (101,341 ft », règle R2, motif non ancré), jamais le mètre en gras, qui en est la conversion ; sur les lignes lues, point décimal des pieds changé en virgule (`NUM` du moteur) ; le reste du README garde le point | décision de l'utilisateur (« écris la valeur en ft de [P] ») ; U-3 | typographie mixte du README (dette §2.18) | `temple_parthenon_from_readme.py` (`FEET`) |
| PA-4 | **Giron de l'euthyntéria déduit de la saillie totale** : 4.985 − 2 × 2.325 = 0.335 ft, la saillie totale de [P] (p. 13) étant prise pour celle des trois ressauts (deux degrés et euthyntéria) ; ligne ajoutée au § 3.2 du README, statut « déduit » | [P] p. 12, 13 | la saillie de Lloyd (4.997 ft, p. 114) donnerait 0.347 ft, +3,7 mm par côté | README § 3.2 ; `krepis.euthynteriaTread` |
| PA-5 | **Emprise de l'euthyntéria au § 12 du README** (`krepisControls`), ajoutée en pieds puis convertie au mm : 111.311 ft = 33,928 m, 238.111 ft = 72,576 m. Contrôle de la conversion, **par construction** pour les données (la saillie totale est la somme des girons, PA-4) | — | contre 30,889 + 2 × 1,519 et 69,537 + 2 × 1,519 m (trois arrondis au mm) : +0,6 et +1,2 mm | README § 12 ; `krepisControls` |

### 10.16 Cella, porches et intérieur de l'Héphaïsteion (lots C1, C2), `plan_implementation_03_revision.md` §30, §31

Conventions prises le 2026-11-18 pour la cella et les porches (README d'Héphaïstos § 7.8, `e99e2001…`), sous les
décisions D1 à D3 et D8 de l'utilisateur et la conception du lot C1. Format `monument/v11`
(`src/cgarchi/schemas/monument-v11.schema`) ; API `CellaSettings`, `LayoutGreekCella`, `BuildGreekCellaPieces`
(`classical_greek_cella.h`), `TempleSettings::cella`, `MonumentCellaSettings`, `MonumentPorchSettings`,
`MonumentPartDependencies` (`classical_monument.h`). CE-10 à CE-12 sont portées par les données (statut
« convention », une ligne chacune ajoutée au § 7.8 du README) ; elles figurent ici avec le choix qui les fonde.

**Révision du lot C2 (2026-11-18, `plan_implementation_03_revision.md` §31).** Intérieur du naos (README § 7.9,
`419838e3…`, lu ligne par ligne, seule source des valeurs), sous les décisions de l'utilisateur du 18 novembre 2026
(calage D1 sur le nu est à 5,0815 ; 16 cannelures ; chapiteaux de 0,367 et 0,275, abaque supérieur de 0,60 lu [B] ;
assise XVII de 0,409 × 0,410 ; corniche de 0,207, saillie 0,016 ; poche à 8,073, nus élargis de 0,410, tympans
fermés ; nef non abaissée, stylobate de la colonnade non construit ; blocs lisses ; intérieur et raffinement
refusés). Format `monument/v12` (`src/cgarchi/schemas/monument-v12.schema`, v11 supprimé) ; API
`NaosInteriorSettings`, `LayoutGreekNaosInterior`, `BuildGreekNaosInteriorPieces`
(`classical_greek_naos_interior.h`), `TempleSettings::interior`, `MonumentInteriorSettings`, `ReportMonumentParts`
(`classical_monument.h`). Le README n'a reçu aucune ligne : la l. 567 seule est corrigée (« p. 93 » en « p. 92,
n. 193 », lecture de 0,367 m). CE-15 à CE-22 sont des conventions du module, sans valeur portée par les données.

**Révision du lot C3 (2026-11-18, `plan_implementation_03_revision.md` §32).** Assise XVII extérieure (épicranitis des
murs et couronnement de la frise des porches) et prolongement est de l'entablement des porches (README § 7.10,
`9fc951da…`), sous les décisions D-C3-1 à D-C3-12 de l'utilisateur. Format `monument/v13`
(`src/cgarchi/schemas/monument-v13.schema`, v12 supprimé) ; API `EpicranitisSettings`, `LayoutGreekEpicranitis`,
`BuildGreekEpicranitisPieces`, `PorchExtensionSettings`, `LayoutGreekPorchExtension`,
`BuildGreekPorchExtensionPieces` (`classical_greek_cella_crown.h`), `CellaPorchEntablature` (`CellaLayout`),
`detail::StripMesh` (`classical_greek_prism_detail.h`), `TempleSettings::epicranitis` et `::porchExtension`,
`MonumentEpicranitisSettings`, `MonumentPorchExtensionSettings` (`classical_monument.h`). Le README a reçu le § 7.10
(une valeur par ligne ; conventions après leur ligne « manquant ») et des renvois aux § 1.1, 7.5, 7.8 et 11.
CE-24, CE-25, CE-27, CE-28 et CE-29 sont portées par les données (statut « convention » au § 7.10) ; CE-26, CE-30 et
CE-31 sont des conventions du module.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| CE-1 | **Format et parties** : `monument/v11`, blocs facultatifs `cella` (32 longueurs de segments, sans signe, dans l'unité de la source, depuis les faces et bords nommés par le README) et `cellaControls` (20 contrôles sans statut, coordonnées signées et cotes en mètres, règle R13 du script, refusés sans `cella`). Deux parties sur un bloc : **Cella** (maçonnerie, 30 clés : toutes sauf `porchColumnDiameter` et `porchColumnOffset`) dépend de la krépis ; **Porches** (les quatre colonnes, 3 clés : diamètre, hauteur, écart) dépend de la krépis, du péristyle, des colonnes et de la cella. MO-P3 passe d'une chaîne par indice à une **table** (`MonumentPartDependencies`), qui redonne la chaîne pour les cinq parties existantes. L'architrave et la frise des porches appartiennent à la Cella : elles portent sur les antes et les murs, et le pied de l'architrave (hauteur des colonnes des porches) fixe le haut des antes et le ressaut des murs longs | conception de l'utilisateur du 2026-11-18 | la hauteur des colonnes des porches est consommée par les deux parties ; les axes des porches par la Cella seule | `classical_monument.{h,cpp}`, schéma, `monument_from_readme.py` |
| CE-2 | **Remplissage plein sous le sol du naos** : un prisme du dessus du toichobate (0,098) au sol du naos (0,300), entre les nus intérieurs des murs longs et des murs de refend ; le sol des porches est le dessus du toichobate | README § 7.8 (sols lus et déduits [O]) ; conception C1 | ni dallage, ni assise II distincte, ni fondations : un bloc | `BuildGreekCellaPieces` |
| CE-3 | **Parties enfouies non construites** : le toichobate part du dessus du stylobate (z = 0) et ne monte que de sa partie visible (0,098 m), sa hauteur de 0,358 m restant un contrôle (dessous à z = −0,260) ; les murs, les antes, le sol du naos et les colonnes des porches posent sur le dessus du toichobate, leurs pieds à float (stylobateTop + 0,098). **Seuil arasé au sol du naos** : du dessus du toichobate à z = 0,300, la butée des vantaux (≈ 0,07 m, dessus de [O] à ≈ 0,370) et le creux de 0,188 m dans l'assise I omis ; centré sur la porte (bouts à ±1,905 contre −1,903 et +1,907 lus [O], 2 mm) | README § 7.8 (« Toichobate : hauteur », « Seuil … : hauteur ») | seuil 7 cm sous la restitution de [O] ; la hauteur 0,46 m n'est pas portée au document (aucune valeur de cellule ne donne le dessus) | idem |
| CE-4 | **Comble plein, aucune pièce au-dessus de z11** (étendue au couronnement par CE-30, lot C3) : la cella s'arrête au dessus des murs (7,358) et de la frise des porches (7,357), sous le dessus de l'anneau (7,694) ; refus `Temple : 'cella'` si la cote la plus haute de la cella dépasse le dessus de l'anneau, ou si son emprise sort de la face intérieure de l'anneau ou touche une colonne du péristyle | README § 7.8 (« L'intérieur s'arrête sous z = 7,694 ») ; décision de l'utilisateur | plafonds, poutres et caissons non construits ; le comble n'est pas creusé (dette, lot C2) | `LayoutCellaInTemple` |
| CE-5 | **Moulures omises** : base moulurée des murs et des antes (assise II), chapiteaux des antes (0,326 m), couronnement de l'architrave, moulures de la porte ; toutes les pièces sont des prismes droits | conception C1 | antes en prismes lisses du toichobate au pied de l'architrave ; `antaShaft` et `antaCapital` du README non portés | idem |
| CE-6 | **Colonnes des porches** : la colonne courante du péristyle (`MonumentColumnSettings`), diamètre inférieur 1,006 et hauteur 5,611 remplacés ; chapiteau et diamètre supérieur inchangés (fût seul raccourci) ; cannelures au même rapport profondeur / corde ; entasis mise à l'échelle du fût, flèche et cote du maximum multipliées par le rapport des hauteurs de fût (0,980) | README § 7.3, § 11 point 4 (convention existante) ; § 7.8, ligne « Cannelures, entasis » manquante | aucune source ne distingue les cannelures et l'entasis des porches | `MonumentPorchSettings` |
| CE-7 | **Architrave et frise des porches lisses** (le prolongement est et le couronnement sont construits au lot C3, CE-29 et CE-25) : deux blocs, sans partage de l'architrave en face et couronnement, d'une face extérieure d'ante à l'autre (y = ±3,962), profondeur 0,999 ; à l'est, la frise n'est pas prolongée au-dessus des portiques latéraux jusqu'à l'entablement du péristyle | README § 7.8 (partage manquant ; étendue lue [D], [W]) ; décision de l'utilisateur | frise sculptée et prolongement est absents (dette) | `BuildGreekCellaPieces` |
| CE-8 | **Pièces fermées concaténées, contacts listés** : 15 prismes (toichobate, sol du naos, 2 murs longs à profil (Y, z), 2 murs de refend, seuil, 4 antes, architrave et frise de chaque porche) puis 4 colonnes, après le comble ; liste fermée des recouvrements, tous coplanaires : C1 toichobate / stylobate ; C2 pieds sur le toichobate ; C3 sol du naos contre murs et seuil ; C4 murs de refend / murs longs ; C5 seuil dans l'échancrure ; C6 antes / bouts des murs longs ; C7 architrave sur antes, murs, abaques, et dos contre les murs ; C8 frise sur l'architrave (faces confondues : `DuplicateFace`) et dos contre les murs | TE5 étendue | les murs longs montent au dos des architraves au-dessus du pied de l'architrave, et au dos des antes en dessous | `classical_temple.h` (TE5), test CE8 |
| CE-9 | **Cella sans raffinements** : `TempleSettings` à cella et raffinement refusé, `Temple : 'cella' et 'refinement' ne se combinent pas : la cella n'a pas de raffinements (README section 7.8, decision D8) …`, avant tout refus du raffinement ; le champ de courbure du § 8 n'est pas défini pour la cella | décision D8 de l'utilisateur ; README § 7.7 (murs verticaux, [O] p. 83, 92) | R0b (`monument/v12`) devra trancher la combinaison | `LayoutWithColumns` |
| CE-10 | **Porte est centrée sur l'axe longitudinal** (y = 0) : jambage sud à 1,7625 m du nu intérieur du mur sud, = (6,230 − 2,705) / 2, clé `cella.doorJambOffset` | [O] p. 54 : seuil presque centré (1,212 et 1,208 m des nus) ; ligne « Porte est : position en plan retenue » du § 7.8 | 2 mm d'asymétrie du seuil de [O] ignorés | README § 7.8 ; `doorJambOffset` |
| CE-11 | **Profondeur du seuil = épaisseur du mur de refend est (0,808)** : dos du seuil au nu ouest du mur, la saillie de 0,369 en avant du nu est s'y ajoutant (1,177 m en tout) ; refus `'cella.thresholdDepth'` si elle diffère de l'épaisseur (le seuil traverse le mur, que l'échancrure perce de part en part) | [O] p. 54 : dos au plan des orthostates ; ligne « Seuil … : profondeur retenue » | le plan des orthostates (0,828) est 1 cm plus loin que le nu | README § 7.8 ; `thresholdDepth` |
| CE-12 | **Section de 0,999 m de l'architrave des porches centrée sur la ligne des axes des colonnes des porches** : face extérieure à 0,4995 m de cette ligne | ligne « Architrave et frise … : position en plan retenue » | le dos de l'architrave passe 9,4 cm (est) et 3,4 cm (ouest) au-delà du dos des antes : il porte sur le mur long, qui s'arrête au dos de l'architrave au-dessus de son pied | README § 7.8 ; `porchEntablatureFront` |
| CE-13 | **Repère des antes : le nu du mur construit** : débord intérieur 0,104 m compté depuis le nu intérieur des murs longs de [O] (y = ±3,115), et non 0,094 depuis le plan des orthostates (y = ±3,105), que le modèle ne construit pas ; faces intérieures à y = ±3,011 dans les deux repères. Contrôle : 0,104 + 0,761 + 0,086 = 0,951, largeur de face lue [S] | README § 7.8, ligne « Antes : débord intérieur » (lu par la règle R2) | — | `antaInnerProjection` |
| CE-14 | **Poche du comble** (lot C2a) : paramètre facultatif `RoofPocket {x0, x1, y0, y1, zTop}` de `BuildGreekPedimentPieceMeshes`, le fronton ignorant ce qui occupe la poche ; boîte ouverte par le bas, de la base du comble (zBase = zFoot + z11) à zTop, sur [x0, x1] × [y0, y1], dans le repère du temple (zTop est une cote absolue, comme zBase ; x en façade, y en profondeur) ; comble de 14 sommets (les 6 du prisme d'abord, au bit près) et 24 triangles ; refus `'roofPocket'`, `'roofPocket.x'`, `'roofPocket.y'`, `'roofPocket.zTop'` : borne non finie, emprise vide ou inversée, zTop ≤ zBase, contact ou traversée d'un bord de la base, d'un tympan ou d'un pan (marge strictement positive, critère en coordonnées, produits arrondis en double sans contraction FMA), contact une fois arrondie en float, combinaison avec les stations du raffinement | conception du lot C2a (2026-11-18) | aucune source ne décrit le comble au-dessus de la cella : l'emprise et le plafond sont fournis par l'appelant (lot suivant) ; le comble raffiné n'a pas de poche | `classical_greek_pediment.{h,cpp}` |
| CE-15 | **Format et partie de l'intérieur** : `monument/v12`, blocs facultatifs `interior` (25 longueurs de segments depuis les nus du naos de la section 7.8 et cotes z depuis le dessus du stylobate, et le compte `fluteCount`) et `interiorControls` (20 contrôles sans statut, règle R13 : nu est, cinq axes des flancs, axe et nu ouest, longueur du naos, axes latéraux, cinq dessus, base et emprise de la poche) ; `interior` exige `cella` (schéma `dependentRequired`, lecteur `requires block "cella"`, moteur `REQUIRES`), `interiorControls` exige `interior`. Partie **Interior** (26 clés, toutes) : dépend de la krépis, du péristyle, des colonnes (ses colonnes dérivent de celle du péristyle) et de la cella, pas du fronton (sans fronton, l'intérieur est construit sans poche). Le rapport des parties est une fonction pure, `ReportMonumentParts`, que `BuildMonumentMesh` rend ; MO-P3 éprouvée sur les 256 combinaisons de complétude. **Nef non abaissée** : le pied des colonnes est `naveLevel` et doit égaler le sol du naos, sinon refus `'interior.naveLevel'` ; le stylobate de la colonnade (0,267 de [O]) n'est pas lu | décisions de l'utilisateur du 2026-11-18 (format, dépendances, nef, stylobate) | `interiorControls` n'est relu que par les tests ; R0b passe en `monument/v13` | `classical_monument.{h,cpp}`, schéma, `monument_from_readme.py`, `temple_hephaistos_from_readme.py` |
| CE-16 | **Colonnes intérieures** : colonne courante du péristyle (`MonumentColumnSettings`), diamètres et hauteur de chaque étage lus ; 16 cannelures au rapport profondeur / corde du péristyle (section homothétique, comme CE-6) ; entasis : flèche × (diamètre inférieur lu / 1,016), cote du maximum × (fût de l'étage / fût du péristyle), même position relative (0,5005) ; 5,06 mm à 1,774 m et 3,84 mm à 1,147 m **[M]** (test N8) | README § 7.7, ligne « Chapiteaux, cannelures, profils » (convention : colonne du péristyle réduite au rapport des diamètres inférieurs) ; § 7.9 | aucune source ne donne l'entasis intérieure ; la colonne réduite au seul rapport des diamètres (hauteurs comprises) ne donnerait pas les hauteurs lues | `MonumentInteriorSettings` (`InteriorColumn`) |
| CE-17 | **Chapiteaux intérieurs, membres et raccord** : profil du péristyle ; hauteurs de tous les membres (rainure, gorgerin, annelets, droite et lèvre de l'échine, abaque) × kh = hauteur lue / chapiteau du modèle du péristyle (0,50983 m, rainure comprise, GD3c), si bien que le chapiteau vaut 0,367 et 0,275 à 1e-12 ; **saillies depuis le sommet du fût (GD5) × kp = (abaque lu / 2 − d′/2) / saillie de l'abaque du péristyle**, si bien que l'abaque vaut 0,824 et 0,60 à 1e-12 et contient l'échine réduite ; lèvre × rapport des demi-cordes jonction-tête (arc semblable). kp 0,69218 et 0,41859, kh 0,71985 et 0,53940 **[M]** (test N8). **Raccord** : le gorgerin est au rayon du sommet du fût par construction (GD2b), **sans marche**, ce que le README laisse ouvert (gorgerin réduit de 0,562 m, 4,5 mm en retrait par côté) | README § 7.9, lignes « Chapiteau … : membres » (inférieur : convention de la source ; supérieur : **manquant**, la convention du § 7.7 appliquée par décision de l'utilisateur) ; abaques et hauteurs lus | **variantes refusées par `ValidateGreekDoricColumnSettings`** à l'étage (test N8) : (a) saillies × diamètres inférieurs (GD5, abaque ≈ 0,835 en bas) puis abaque de 0,60 imposé : l'échine (≈ 0,095 de saillie, calcul à la main) déborde de l'abaque (0,0765) ; (b) rayons depuis l'axe × 0,60 / 1,140 (comme GD5a) : le premier annelet passe 4 mm sous le sommet du fût. Au rez-de-chaussée, kp (0,692) est 4 % sous le rapport des diamètres (0,722) ; à l'étage, kp / kh = 0,78 aplatit l'échine | idem |
| CE-18 | **Rainure des colonnes intérieures** : GD3d sur 16 cannelures et le nouveau fût, R (1 − fond des cannelures au sommet) + hauteur de rainure ; 0,0998 et 0,0993 du rayon du sommet du fût **[M]** (test N8) | aucune (GD3d, limite de représentation) | ≈ 7,1 % prévu par l'architecte : la prévision oubliait le sagitta de la corde (1 − cos π/16 = 1,9 %) et la hauteur de rainure (1,2 %) | idem |
| CE-19 | **Plan et pièces de la colonnade** : Π ouvert vers l'est, flancs aux axes X = ±(nu − 0,771), colonnes d'axe Y = nu est + 1,296 + k × 2,344 ; nombre de travées des flancs (4) et du fond (2) entier, fermé à 1e-9 près de la longueur (sinon refus `'interior.backAxisOffset'`, `'interior.wallAxisOffset'`), 1,473 n'étant qu'un contrôle de fermeture ; colonne du fond à X = −a + j (2a / m) (0 au bit près) ; produits k × entraxe lus en volatile (sans FMA). Épistyles **centrés sur la ligne des axes**, trois prismes en Π (Left, Right, Back), **onglets à 45 degrés** au retour ouest (faces d'onglet coplanaires, I3), partant du nu du mur de refend est ; **blocs lisses**, bande côté mur (0,097 × 0,011), redent de 0,029 et profils non construits | décisions de l'utilisateur ; README § 7.9 (entraxe, axes, sous-faces) | 11 colonnes par étage, comme le schéma A1 ; aucune source ne place les sous-faces par rapport aux axes | `classical_greek_naos_interior.cpp` |
| CE-20 | **Corniche intérieure** : trois prismes en Π sur l'épistyle supérieur, de son dessus (somme des hauteurs) à + 0,207, **de la face côté nef moins 0,016 à la face côté mur** (0,530 de profondeur), onglets à 45 degrés, partant du nu est ; elle pose sur le seul épistyle | README § 7.9 (hauteur, saillie, position côté nef) ; profondeur : aucune | couvre la bande altérée de 0,08 à 0,09 m où posaient les planches des bas-côtés ([H] p. 168-170) ; poser sur l'assise XVII aussi ferait dépendre le contact d'un ulp (dessus de l'épistyle et de l'assise par deux sommes) | idem |
| CE-21 | **Assise XVII intérieure** : anneau de 4 prismes **dans l'épaisseur des murs**, du nu intérieur vers l'extérieur sur 0,410, du dessus des murs (7,358) à + 0,409 ; Left et Right sur toute la longueur, coins compris, Front et Back entre eux (joints droits, comme C4) ; refus `'interior.courseThickness'` si l'assise sort d'un mur | README § 7.9 (« depuis le nu intérieur des murs, sur les quatre murs du naos » ; épaisseur U2) | l'épistyle supérieur, qui part du nu est, touche l'assise est par son about (I4) | idem |
| CE-22 | **Poche et contacts de l'intérieur** : poche du comble (CE-14) aux nus du naos élargis de `pocketMargin`, plafond stylobateTop + 8,073 ; tympans fermés ; refus `'interior.pocketMargin'` si elle est plus étroite que l'assise, `'interior.pocketCeiling'` si le plafond n'est pas au-dessus de la cote la plus haute de l'intérieur ; refus du comble au maillage. Avec une marge égale à l'épaisseur, les faces extérieures de l'assise sont les parois de la poche (I7). Liste fermée (TE5) : I1 pieds inférieurs sur le sol du naos ; I2 abaques sous les épistyles, pieds supérieurs sur l'épistyle inférieur ; I3 onglets ; I4 abouts contre le mur de refend est (inférieur) et l'assise est (supérieur) ; I5 corniche sur l'épistyle ; I6 assise sur les murs, coins ; I7 assise contre la poche. **Intérieur et raffinement refusés** (CE-9 étendue), avant la cella : `Temple : 'interior' et 'refinement' …` | conception du lot C2 ; décisions de l'utilisateur | le dessous de l'épistyle supérieur et le dessus des murs coïncident en float : son about ne touche le mur que par une arête (contact absent de la liste) ; mutation (plafond à 7,9) : 28 traversées comble / corniche hors liste | idem, `classical_temple.{h,cpp}` |
| CE-23 | **Format et parties du couronnement** : `monument/v13`, blocs facultatifs `epicranitis` (6 longueurs : `height` en m, `innerOffset` en m, `listelHeight`, `filletHeight`, `listelProjection`, `bandProjection` en pouces de [S]) et `epicranitisControls` (7 contrôles R13 : `topZ`, `porchCrownBottomZ`, `porchCrownHeight`, `innerFaceY`, `wallBandY`, `eastBandX`, `westBandX`), `porchExtension` (`eastSpan`) et `porchExtensionControls` (`ringInnerY`, `architraveFootZ`) ; aucune clé ajoutée à `cella` ni à `interior`. `epicranitis` exige `cella`, `porchExtension` exige `epicranitis`, chaque bloc de contrôle sa partie (schéma `dependentRequired`, lecteur `requires block`, moteur `REQUIRES`). Parties **Epicranitis** (6 clés ; dépend de la krépis et de la cella) et **PorchExtension** (1 clé ; dépend de la krépis, du péristyle, des colonnes, de l'entablement, de la cella et de l'épicranitis) ; `kMonumentPartCount` = 10, MO-P3 éprouvée sur les 1 024 combinaisons. Sans colonnes, l'épicranitis est construite après la cella, sur la krépis, sans contrôle contre l'anneau. `opus --exterieur` retire aussi les quatre blocs, par `MonumentExterior` (révision de la revue : `MonumentWithoutPart` retire une partie et, de proche en proche, les blocs qui l'exigent ; `ReportMonumentParts` et `BuildMonumentMesh` refusent des drapeaux `present` contraires à la table MO-P2 du lecteur, une seule table pour les trois) | décisions D-C3-11 et D-C3-12 de l'utilisateur (2026-11-18) | R0b passe en `monument/v14` ; les pièces nouvelles suivent les colonnes de l'intérieur, si bien que le maillage du lot C2 est un préfixe exact (test K3) | `classical_monument.{h,cpp}`, schéma, `monument_from_readme.py`, `temple_hephaistos_from_readme.py`, `opus/main.cpp` |
| CE-24 | **Face intérieure de l'épicranitis = face de l'assise XVII intérieure** : `naos.inner + innerOffset`, même expression que `n.inner + s.courseThickness` de CE-21, coplanaires au bit près (test K5) ; 0,410 m depuis le nu intérieur, reste 0,351 m d'épicranitis ; refus `Temple : 'epicranitis.innerOffset'` si un intérieur est présent et que `courseThickness` ≠ `innerOffset` ; refus `Couronnement : 'epicranitis.innerOffset'` si la face sort d'un mur long ou d'un mur de refend | README § 7.10, « face intérieure retenue » : convention D-C3-6 dans la fourchette lue de [O] p. 75 (0,335/0,365 pour l'épicranitis, 0,395/0,425 pour la face intérieure) | sans intérieur, une autre épaisseur est acceptée (test K7) | `classical_greek_cella_crown.cpp`, `classical_temple.cpp` |
| CE-25 | **Profil et dessus commun** : une seule assise, `top = naos.wallTop + height` (7,358 + 0,207 de [O], D-C3-2), calculé une fois, sur les murs et sur la frise des porches (7,357 : le couronnement y fait 0,208 = 8,2 in de [S], contrôle) ; de bas en haut, ordre **lu** sur [S] pl. VII : bande à méandre (saillie 1,9 in lue), filet (hauteur 0,4 in déduite, **saillie manquante, prise égale à celle du listel**, D-C3-4), listel (1,15 in, saillie 1,8 in lues) ; filet et listel à la même saillie ne font qu'un gradin ; **le millimètre d'écart (0,207 contre 0,208) est absorbé par la bande** (D-C3-5) : 0,1676 m sur les murs, 0,1686 m sur la frise. Refus `'epicranitis.height'` si le dessus ne passe pas la frise, `'epicranitis.listelHeight'` si listel et filet ne laissent pas de bande | README § 7.10 (lignes « Couronnement : … ») ; [S] pl. VII, vérifiée le 2026-11-18 | **incohérence lue, gardée** : avec le listel en haut, la bande saille de 0,1 in (2,5 mm) de plus que le listel qui la couronne ; le dessin les montre presque à l'aplomb ; cotes reprises telles quelles | `classical_greek_cella_crown.cpp` (`Profile`) |
| CE-26 | **Corps lisses et bandes moulurées séparées, onglets** : chaque assise est un corps lisse (boîte) ; chaque face moulurée porte une bande, prisme du profil le long de la face (`detail::StripMesh`, 12 sommets, 20 faces), à bout d'onglet à 45 degrés aux angles rentrants (pronaos, opisthodome, dos des couronnements contre les nus extérieurs des murs), à bout droit ailleurs ; le dessous de la bande, en saillie sur le nu, ne porte sur rien (encorbellement). Sens de saillie `detail::StripOutward` ; saillie négative et profil non simple refusés (`IsSimple`, révision de la revue) ; les gardes de `LayoutGreekEpicranitis` comparent les bouts d'onglet **en float**, par les expressions de `StripMesh`, si bien qu'un cas limite donne un refus nommé et non le `logic_error` de la bande. Faces moulurées (D-C3-8) : nus extérieurs des murs longs ; les deux faces des couronnements de la frise ; les deux faces des murs longs des porches ; face côté porche des murs de refend | README § 7.10, « faces moulurées » : [O] p. 75 lu pour la construction séparée des deux faces des porches ; profil de la face intérieure le même, et murs de refend moulurés, par convention (D-C3-8) | méandre peint non représenté ; onglets : la moulure d'un angle rentrant n'est décrite par aucune source ; aux onglets entre la bande d'un couronnement (pied à 7,357) et celle d'un mur (pied à 7,358), les faces d'onglet se recouvrent sans être confondues | `classical_greek_cella_crown.cpp`, `classical_greek_prism.cpp` |
| CE-27 | **Bouts des couronnements coupés, sans retour** : au droit des faces extérieures des antes (X = ±3,962), à l'ouest, et à l'est sans prolongement ; au dos des couronnements, hors des murs, la bande court du nu extérieur des murs (onglet) à la face des antes (0,086 m) | README § 7.10, « bouts ouest » : convention D-C3-8 | un retour de la moulure sur les bouts serait l'autre lecture ; aucune source | idem |
| CE-28 | **Assise XVII des murs des porches et des murs de refend** : sur les murs longs des porches, toute l'épaisseur, en deux corps (face intérieure de 0,410, épicranitis de 0,351 commune aux murs du naos), du dos de l'architrave des porches à la face de l'assise XVII intérieure du mur de refend ; sur les murs de refend, côté porche, de la face du mur à la face de l'assise XVII intérieure (0,410 depuis le nu côté naos), entre les nus intérieurs des murs longs | README § 7.10 : [O] p. 75 et n. 159 lus (trois séries de blocs, dessus au même niveau) ; partage et murs de refend : conventions D-C3-6 et D-C3-7 | sans intérieur, la face intérieure du naos (x de 3,115 à 3,525) reste nue sur le dessus des murs | idem |
| CE-29 | **Prolongement est** : de chaque côté, architrave, frise et corps du couronnement (boîtes de la section des porches, 0,999 m, aux mêmes dessus 6,549 et 7,357), et deux bandes (deux faces) à bouts droits, de la face extérieure des antes à la face intérieure de l'anneau, celle-ci par **l'expression de `SweepRectangularRing`** (`halfWidth + (−innerFace)`) ; `eastSpan` (1,827) n'est qu'un **contrôle de fermeture**, à 5e-4 (−0,26 mm mesuré, test K6), refus `'porchExtension.eastSpan'` ; **dessous de l'architrave au pied de l'anneau, 5,715** (D-C3-10), sur l'abaque de la 3ᵉ colonne de chaque flanc, ressaut de 6 mm sur l'architrave des porches (5,709) | README § 7.10 : étendue lue [D] p. 126, section lue [S] pl. VIII fig. 1, longueur déduite ; dessous : convention D-C3-10 ([S] pl. V et [O] p. 77 mettent les chapiteaux au même niveau) | à 5,709, l'architrave traverserait les deux abaques (mutation M2) ; bouts contre les porches : les sections égales ne donnent des faces confondues que pour les bandes (mêmes triangles), les boîtes ayant des diagonales croisées | idem |
| CE-30 | **Contacts et refus du couronnement dans le temple** : CE-4 étendue, `Temple : 'epicranitis'` si l'emprise des pièces (bandes comprises) n'est pas strictement dans la face intérieure de l'anneau ou si le dessus n'est pas sous le dessus de l'anneau ; les bouts du prolongement y sont égaux. Couronnement et prolongement refusés avec un raffinement (CE-9 étendue), le couronnement sans cella, le prolongement sans couronnement ou sans entablement. Liste fermée (TE5), tous coplanaires sauf les onglets : K1 assises sur le dessus des murs ; K2 couronnements sur la frise, et leur dos contre le millimètre des murs ; K3 contre l'assise XVII intérieure ; K4 bouts des assises contre le dos des couronnements ; K5 joints, dos des bandes sur les corps, onglets ; K6 architrave prolongée sur les abaques, bouts contre l'anneau ; K7, K8 bouts du prolongement contre ceux des porches ; K9 aucun contact avec le comble | conception du lot C3 ; décisions de l'utilisateur | l'emprise de l'entablement des porches elle-même n'est pas contrôlée par CE-4 (seul le toichobate l'est) : écart hérité du lot C1, non corrigé ici | `classical_temple.{h,cpp}` |
| CE-31 | **Corniche à anthémion (0,232) et bandeau (0,089) non construits** : renvoyés au lot C4, avec les plafonds. Constat : la poutre du plafond du ptéron (0,525 m) pose sur le listel du couronnement ([S] pl. VII), dessus des poutres à 8,090 contre 8,073 chez [O], 1,7 cm | décision D-C3-1 ; [S] pl. VII vérifiée le 2026-11-18 | l'alerte d'une poutre à 8,41 était fausse | — |

**Révision du lot C4 (2026-11-19, `plan_implementation_03_revision.md` §33).** Plafonds du péristyle et couverture
(README d'Héphaïstos § 7.11, `b15888ff…`, 210 700 octets, en fin de lot ; décisions C4-1 à C4-9 de l'utilisateur, conception Q-C4-1 à Q-C4-14,
Q-C4-A option C, Q-C4-B option a). Format `monument/v14` (`src/cgarchi/schemas/monument-v14.schema`) ; API
`EastCeilingSettings`, `LayoutGreekEastCeiling`, `BuildGreekEastCeilingPieces`, `FlankCeilingSettings`,
`LayoutGreekFlankCeilings`, `BuildGreekFlankCeilingPieces` (`classical_greek_ceiling.h`), `detail::GridSolidMesh`
(`classical_greek_prism_detail.h`), `detail::PocketMeetsRoof` (`classical_greek_pediment_detail.h`),
`TempleSettings::eastCeiling` et `::flankCeilings`, `MonumentEastCeilingSettings`, `MonumentFlankCeilingSettings`
(`classical_monument.h`). CE-32 à CE-42 viennent de la conception du lot ; CE-43 à CE-50 ont été prises par le
developer à l'exécution, CE-51 à CE-53 à la revue du lot, CE-54 au correctif des coutures du comble
(`plan_implementation_03_revision.md` § 33.6).

| # | Convention | Fondement | Limites | Où |
|---|---|---|---|---|
| CE-32 | **EN1 en repli** : face intérieure de l'entablement lue si elle est donnée (Héphaïstos : 19,891 in = 0,5052314 m, déduit [S] pl. VI et VIII, C4-9), au nu du pied de l'architrave sinon ; clé facultative, non consommée (MO-P1), l'entablement garde ses 45 clés | décision C4-9 ; Q-C4-1 | la face de façade est prise égale à celle du flanc (convention (b) du § 7.11) | `MonumentEntablatureSettings` |
| CE-33 | **Format v14 et parties** : blocs `eastCeiling` (15 clés), `eastCeilingControls` (15, R13), `flankCeilings` (8), `flankCeilingsControls` (7, R13) ; parties EastCeiling ← krépis, péristyle, colonnes, entablement, cella, épicranitis, prolongement est ; FlankCeilings ← les mêmes et EastCeiling ; `kMonumentPartCount` = 12 ; MO-P2 de 12 à 16 lignes ; MO-P3 sur 4 096 combinaisons (N7, 0,045 s) ; R0b passe en `monument/v15` | Q-C4-4, Q-C4-5 | aucune dépendance au fronton ni à l'intérieur : sans fronton, pas de poche | `classical_monument.{h,cpp}`, schéma v14 |
| CE-34 | **Comble en tranches** : chaque poche est l'enveloppe des pièces au-dessus de la base du comble, son plafond le dessus des pièces (contact coplanaire au bit près) ; aiguillage : aucune poche, le prisme ; une, le comble à poche inchangé ; plusieurs, une tranche par intervalle des bornes Y {−yTympanum, y0 et y1 des poches, +yTympanum}, section sous les pans et au-dessus du profil des poches actives (zTop sur une poche, zBase ailleurs), bouts réduits à la différence des sections voisines (CE-54) ; adresse Roof, Front, rang de la tranche par Y croissants | Q-C4-6 | l'index des tranches dépend des longueurs (écart à DG-4) ; Héphaïstos : 9 tranches, 154 sommets, 160 faces | `classical_greek_pediment.cpp` (`RoofSliceMeshes`), `classical_temple.cpp` |
| CE-35 | **Poutres est centrées sur l'axe** : axes (k − (n − 1)/2) × pas, un seul produit | Q-C4-8 : une longueur nulle n'est pas une valeur du format ; README « position en plan » (0 m, mesuré [S]) en contrôle | — | `LayoutGreekEastCeiling` |
| CE-36 | **Corniche à anthémion et bandeau rectangulaires** : corniche de 0,232 m en saillie de 2,634 in sur le nu de la frise, cymaise non représentée ; bandeau de 0,089 m au nu ; profondeur 0,4995 m ; une corniche et un bandeau par travée à l'est, d'une ante à l'autre à l'ouest | Q-C4-10 ; conventions (c), (d), (e) du § 7.11 | profil de la cymaise et anthémion peint absents | `BuildGreekEastCeilingPieces` |
| CE-37 | **Caissons** : ouverture carrée centrée dans la case (l'asymétrie 3,57 / 3,0 in de [S] n'est pas reprise), perles omises, deux ressauts de depth / 2 (fond = ouverture − 2 cadres) | Q-C4-11 ; README § 7.11 (grille lue [S] pl. VIII fig. 2) | nervures perlées absentes | `SlabMesh` |
| CE-38 | **Une dalle = un solide par travée** (`GridSolidMesh`) ; aux flancs, la dalle n'est portée que par les faces des poutres, sans corniche le long des murs : vide de 0,321 m entre l'épicranitis et les dalles | Q-C4-7 ; C4-7 | vide ouvert sous les dalles des flancs (dette § 2.22) | `BuildGreek*CeilingPieces` |
| CE-39 | **Garde des poutres nord et sud contre les dos des entablements des porches** (et non contre les faces des antes, contrôle du README), en float | Q-C4-12 | plus stricte que le README : 7 et 8 poutres tiennent (dernières faces à −8,9075 et +10,2375 contre −9,7605 et +10,5505) | `LayoutGreekFlankCeilings` |
| CE-40 | **Massif de bout de 0,391 m non construit** : corniche, bandeau et dalles s'arrêtent à son plan (\|X\| = 5,387), poche est bornée au même plan | README « nature et hauteur » manquantes | dessous du comble visible au-dessus du massif | `LayoutGreekEastCeiling` |
| CE-41 | **Pannes non construites** ; la contradiction 8,090 (dessus des poutres est) / 8,073 (dessous des pannes de [O]) reste au § 12 du README, sans effet sur le maillage | Q-C4-B, option a | pannes, faîtière, chevrons absents | — |
| CE-42 | **Bout côté péristyle des plafonds des flancs** (option C, remplace la convention (h)) : poutres et dalles des ptérons nord et sud s'arrêtent à `peristyleGap` = 0,170 m de la face intérieure de l'entablement du flanc (\|X\| = 5,608), poutres en console sur l'épicranitis ; dessus (8,090) sous le pan, qui l'atteint à \|X\| = 5,620 : marge minimale mesurée 2,405 mm (PL12) ; 4 caissons sur 1,732 m. Refus nommé `Temple : 'flankCeilings.peristyleGap'` si une poche des flancs touche un pan, en double puis en float | Q-C4-A, option C ; aucune source ([S] pl. X dessine les poutres jusqu'à l'entablement, [O] fig. 35 des dalles sans poutres) | jeu de 0,170 m ouvert sous le comble ; poutres appuyées sur le seul mur | `LayoutFlankCeilingsInTemple` |
| CE-43 | **Rangs des poches dans les refus** : « Fronton : 'roofPocket' : les poches k et l se recouvrent », k < l rangs dans la liste à partir de 0 ; le verdict en double vaut pour les bornes arrondies en float (l'arrondi est monotone : deux emprises disjointes ou au contact le restent), si bien qu'aucun second contrôle en float n'est fait | exécution C4b ; le plan demandait « en double puis en float » | — | `RequireSeparatePockets` |
| CE-44 | **Ordre des poches** (`RoofPocketsOf`) : intérieur du naos, poche est, poche ouest, flancs Left puis Right, chacune si sa partie est présente | ordre du § 33.1.2 du plan | l'ordre fixe les rangs de CE-43 | `classical_temple.cpp` |
| CE-45 | **Tranches formées et vérifiées avant toute pièce** : bornes, recouvrements et sommets confondus sont refusés avant la première pièce du fronton ; `afterEach` ne voit aucune pièce d'une construction refusée | exécution C4b | — | `BuildPedimentPieces` |
| CE-46 | **Critères de `GridSolidMesh`** : diagonale de chaque face du premier coin au troisième ; sommets numérotés par (k, j, i) croissants, seuls ceux des faces émises ; refus d'une arête seule (bloc 2 × 2 d'un plan, diagonale occupée, l'autre vide) et d'un sommet seul (deux cellules occupées opposées d'un bloc 2 × 2 × 2, les six autres vides, ou deux cellules vides opposées, les six autres occupées) | spécification du plan ; exécution C4c ; revue du lot (sommet des cellules vides) | d'autres configurations non manifold d'un sommet ne sont pas refusées (aucune dans les dalles) | `classical_greek_prism.cpp` |
| CE-47 | **Origine de la grille des caissons** : `count` caissons centrés dans [a, b] : début a + ((b − a) − count × pas) / 2, centres au rang k + 0,5, produits en `volatile` ; à l'est, en travers de la travée et le long de [eastEnd, porch.front] ; aux flancs, le long de [nu extérieur des murs, bout côté péristyle] et en travers de la travée entre deux poutres | conventions C4-5 du § 7.11 (centrage) | — | `CofferAxisOf` |
| CE-48 | **Préfixes et gardes nommées** : « Plafond est : 'eastCeiling.<champ>' », « Plafonds des flancs : 'flankCeilings.<champ>' » (noms du document, est/ouest), « Temple : 'eastCeiling.bandHeight' » pour la poche ouest, vérifiée la première (avec la géométrie du README, un bandeau assez haut pour toucher le pan au droit des antes met déjà les poutres au-dessus du pan au massif), puis « Temple : 'eastCeiling.beamHeight' » pour la poche est ; « 'eastCeiling.corniceProjection' » si la saillie atteint la longueur libre ; garde 6 des flancs : cotes du pied des dalles, des deux ressauts et du dessus des poutres strictement croissantes en float (CE-52) ; refus « 'eastCeiling' demande le prolongement est, qui demande un entablement » (pas de refus séparé sans entablement, inatteignable) | exécution C4c, C4e | — | `classical_greek_ceiling.cpp`, `classical_temple.cpp` |
| CE-49 | **Blocs d'essai des flancs, au seul lot C4d** : tant que le document d'Héphaïstos ne portait pas `flankCeilings`, P1, P4 et N7 les ajoutaient avec des valeurs d'essai (conventions sans source) ; retirés au lot C4e, le document les portant ; les plafonds des flancs étaient alors refusés par le temple | exécution C4d | — | tests |
| CE-50 | **Conventions appuyées sur [S]** : `eastCeiling.cofferRows`, `flankCeilings.beamSpacing` et `flankCeilings.beamWidth` sont « convention » avec la source S, que cite la première clause du statut au README (mesure de [S] pl. IV ou X, largeur lue pl. VII) | règle du moteur : sources lues dans la première clause | — | `temple_hephaistos_from_readme.py` |
| CE-51 | **Travée centrale des flancs** : `firstBeamOffset` doit dépasser strictement la demi-largeur des poutres ; la travée la plus étroite est min (beamSpacing, 2 firstBeamOffset) − beamWidth, la travée centrale ayant la largeur 2 firstBeamOffset − beamWidth ; `cofferRows` fois le pas ne doit pas la dépasser | revue du lot C4 (constat M1) | — | `LayoutGreekFlankCeilings` |
| CE-52 | **Gardes des caissons en float** : couches de la dalle (pied, deux ressauts, dessus) strictement croissantes une fois arrondies en float ('eastCeiling.cofferDepth' à l'est, 'flankCeilings.beamHeight' aux flancs) ; lignes de la grille des caissons de chaque axe (en travers des travées, le long des courses) strictement croissantes en float : 'eastCeiling.cofferFrame' si un fond ou un cadre s'écrase, 'eastCeiling.cofferOpening' si un plein s'écrase, 'flankCeilings.cofferRows' aux flancs (champ du plafond est nommé dans le message) ; `GridSolidMesh` ne lève plus après une mise en place acceptée | revue du lot C4 (constat m1) | — | `classical_greek_ceiling.cpp` |
| CE-53 | **Plafonds de deux poches au contact confondus en float** : deux poches d'une même tranche qui se touchent par une face, de plafonds distincts en double et égaux en float, sont refusées (« Fronton : sommets confondus en float : plafonds de deux poches au contact ») : la paroi entre elles serait dégénérée | revue du lot C4 (constat m2) | — | `RoofSliceMeshes` |
| CE-54 | **Coutures du comble en tranches** : une pièce par tranche (Q-C4-6 conservée), mais aucune face là où deux tranches voisines se recouvrent dans leur plan de coupe ; une tranche n'y émet que les colonnes où son profil est plus bas que celui de sa voisine, de la plus basse à la plus haute des deux hauteurs ; aux tympans, la section entière. Tout bord de face dans un plan de coupe est subdivisé par les bornes des deux profils et les hauteurs des colonnes qui les bordent (bandes en fermeture éclair, `AppendZipper`) : la réunion des tranches, sommets confondus par position, est fermée sans T-jonction. Refus de plus dans chaque plan de coupe, mêmes messages que CE-53 et que les bornes d'une tranche : bornes X des deux profils, ou hauteurs au droit d'une même borne, distinctes en double et confondues en float ; et « Fronton : 'roofPocket' : deux poches de tranches voisines ne se touchent que par une arête » si, à une borne, une cote existe où chaque tranche n'est pleine que d'un côté, et pas du même (l'arête serait portée par quatre faces) | correctif des lignes sombres au faîtage (z-fighting des bouts internes coplanaires, opposés, jusqu'au plan des pans) ; alternative écartée : un seul solide, qui défaisait Q-C4-6 et les rangs des pièces ; refus de l'arête seule : relecture du correctif (m1) | les tranches ne sont plus fermées une à une ; contact de deux poches par une arête seule refusé, alors que CE-34 admettait tout contact sans recouvrement (Héphaïstos non refusé **[M]**, PP2) | `RoofSliceMeshes`, `PlaneOf`, `SliceMesh` |

### 10.17 Acanthe corinthienne (lots A0 à A6, §11)

Conventions du chapiteau corinthien sculpté, prises sans appui des sources sous les décisions P1 à P9 de
l'utilisateur (2026-11-21) et la conception de l'architecte (§11). La phase des rangs (P1) suit les sources
et n'y figure pas. Les valeurs numériques des conventions AC-8 à AC-15 sont fixées à l'exécution des lots
A1 à A5 et reportées ici à ce moment ; la ligne existe dès A0 pour que le choix soit visible avant sa valeur.

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| AC-1 | **Feuille `Olive`** (lobes arrondis en folioles d'olivier) pour F et E, et **`OliveMinor`** pour D (style dérivé : contour et relief d'Olive, budget propre, AC-55) ; `LeafStyle {Olive, OliveMinor}` dans `detail` (`classical_acanthus_template.h`) ; le chargeur refuse tout autre style (« style inconnu ») ; un style dérivé du dessin (`derivedFrom`) ne remplace que `source`, `budget`, `minAngle` et `stem` | Vignole pl. XXVI | **part Acute caduque** (décision de l'utilisateur du 2026-11-23, lot A1e) : la décision P2 (Acute par défaut) et sa source (rendus d'un .blend, AC-35) sont retirées ; `AcanthusOptions::style` retiré (aucun lecteur) | `classical_acanthus_template.h`, `data/acanthus_drawing.json` |
| AC-2 | **Borne du feuillage = max (noyau, min (cône de révolution, contour en plan de l'abaque))** sur toute la hauteur du chapiteau (décisions P4 et D2), **portée réduite aux tiges des caulicoles (C), aux volutes et aux hélices** : F et E sont bornés par le cône seul (AC-40) ; **les feuilles D aussi** (décision Q3″ de l'utilisateur, 2026-11-25) : posées sur leurs cotes (AC-53), garde stricte sur le cône, écart à l'abaque mesuré sans contrainte (+1,68 / +1,69 p **[M]**, le D intérieur dépassant le contour des données sous l'hélice, comme le grave le plan de la pl. XXV) ; `EnvelopeExcess` par pièce mesure F, E et D contre le cône (AC-48) ; le fleuron G, posé sur l'abaque, est hors de ce contrôle (−∞) | JSON : cône seul (`enveloppe`, « cône de la règle de Vignole ») ; plan de la pl. XXV : la bande hachurée qui longe le contour de l'abaque gravé (creux moins profond que celui des données) atteint les cornes : contour gravé à relever en tête d'A4 | **[M]** sans le noyau, la borne passait sous la lèvre de la corbeille (20,5 p à h = 36) dès h = 30,85 sur l'axe de face et h = 34,45 à 22,5° ; le max avec le noyau y rend la borne égale au noyau (test `the_envelope_is_the_abacus_raised_to_the_bell`) | `classical_acanthus_detail.h` (`CapitalEnvelope::EnvelopeRadius`) |
| AC-3 | **Borne stricte** : aucun sommet hors de AC-2, pas de débord des calottes (décision P5) | le modèle cgtrader déborde ; Vignole ne cote pas de tolérance | — | détecteur `EnvelopeExcess` (seuil des lots A2 à A5) |
| AC-4 | **Contour de l'abaque = anneau du membre `B · abaco`** (carré concave, cornes 32, faces 17,6), et non celui du cimaise A (cornes 36) | « contour en plan de l'abaque » sans membre désigné ; « A et B insieme vengono detto abaco » | le cimaise A borne plus large de 4 p aux cornes **[V]** (JSON) | `ReadCapitalEnvelope` |
| AC-5 | **Hauteur des instruments comptée du pied du premier membre du chapiteau** ; hors de la bande des membres d'ornement (0 à 36 p), noyau et cône prolongent la bande extrême | les tables du JSON cotent « depuis le bas du chapiteau » ; rien au-dessus de 36 | aucun dans la bande | `CapitalEnvelope::CoreRadius`, `ConeRadius` |
| AC-6 | **Instruments aux sommets** : `CoreClearance` et `EnvelopeExcess` lisent les sommets, pas les faces (majorant du jeu, minorant du débord) | — | une face qui traverse la corbeille entre ses sommets n'est pas vue | `classical_acanthus_detail.h` |
| AC-7 | **Couverture de la corbeille**, deux grandeurs. (1) **Vue depuis l'axe** (`BellCoverage`, grille (θ, h) de 360 × 48 cellules sur h ∈ [1, 23], projection angulaire, rayon ignoré, `CoreClearance` bornant la position radiale à part) : F et E seuls dans [0,60 ; 0,80] (0,710 / 0,712) ; **ornement complet : plancher 0,78**, mesuré 0,876 (Light) / 0,880 (Heavy). (2) **En élévation de face** (`VisibleBellFraction` : projection orthographique, fenêtre \|y\| ≤ 10 p, h 1 à 23, cellules de 0,05 p, une face cache la corbeille si elle passe devant le noyau), **comparée à V-1**, part de corbeille visible tracée à la main sur la pl. XXVI (deux tracés, 0,0613 et 0,0500, V-1 = 0,0557, dispersion 0,011) ; critère : écart ≤ max (5 points, dispersion) (décision Q5 de l'utilisateur, 2026-11-25) ; **rouge connu, en journal** (décision Q10) : 0,119 / 0,117 ; le seuil de 85 % est abandonné | V-1 tracé sur la pl. XXVI **[M]a** ; deux tracés du même opérateur (dispersion minorée) | K24′ +0,061 à +0,064 au-dessus de V-1 : causes nommées en dette (bas de F et E h 1 à 11, jours de part et d'autre des tiges, dessous des becs de E h 21 à 23) | `BellCoverage`, `VisibleBellFraction` |
| AC-8 | **Épaisseur t = 0,2 p** de la coque (faces à S + (t/2 + H) n_S et S − (t/2 + Hb) n_S) ; **garde de courbure** de la porteuse (S4bis-2, L1) : rayon de courbure du méridien **en forme close** (dérivées de la Bézier, de l'arc, des rampes et de la pente du noyau dans sa bande) ≥ 2 t + reliefScale · H, H maximal du gabarit par tranche de y (64 tranches sur [0, 2], voisines comprises) ; corps aux méridiens 0 à u_corps par quarts, arc à 0, u_cap / 2, u_cap ; sur chaque intervalle lisse (tranches de H, début de rampe, crête), 16 échantillons puis section dorée autour du maximum de courbure ; un coin (tangentes écartées de plus de 1e-6 rad) au départ du bec ou, avec bande, au pied du feuillage vaut un rayon nul ; rayon du bec ≥ 2 t + H (y = 1) | non cotée | marge minimale **[M]** F +0,188 (Light) / +0,188 (Heavy), E +0,536 / +0,535 p, à u_cap, y ≈ 1,25 ; identique à 1e-6 entre 4 et 64 échantillons ; rayon égal à 1e-3 près au cercle par trois points à 1e-4 ; les cassures de pente du noyau entre bandes ne sont pas vues (la seule dans les feuilles est à h ≈ 24, crête de E, méridien horizontal) | `LeafSpec::thickness`, `LeafBend::CurvatureMargin` |
| AC-9 | **Gouttière** : l'écart off du méridien est multiplié par 1 − w g (1 − ue²)², **g = 0,2**, ue = \|u\| / u_corps (\|u\| maximal du gabarit sur le corps) ; w = 1 jusqu'à y_d = max (1, fade[1]), puis 1 − smootherstep (y_d, y_crête, y), 0 au-delà de la crête ; rampe confondue avec celle du dôme, distincte du départ du bec (S4bis-2 : le décalage `gutterFadeShift` de 0,02, posé contre la sensibilité des sondes à trois points, est retiré) | gravure sans coupe | g max admissible **[M]** 0,463 à 0,526 sur 6 poses (garde en forme close) ; marge de F +0,188 sans décalage, +0,161 avec 0,02 (prototype) ; côtes, gabarit LeafEnvelope et `droop` retirés | `LeafSpec::gutter`, `LeafBend::Gutter` |
| AC-10 | ~~Contour fonction de v seul (lobes, œillets, dents, cisaillement)~~ — **remplacé au lot A1b** par AC-24 à AC-27 (lobes en éventail, digitations, œillets, contour de lame). Le gabarit (pédoncule, ventre, pointe) reste, en AC-9 et AC-24 | pl. XXV et XXVI dessinent, ne cotent pas | — | — |
| AC-11 | ~~Demi-largeur au ventre = 0,35 rad vue de l'axe~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) ; la largeur est hw du rang (AC-41), `halfAngle` retiré | — | — | — |
| AC-12 | **Jeu c = 0,05 p** entre la face intérieure et le noyau ; retrait sous la borne m = 0,05 p ; c = −0,5 sert de cas négatif | aucun | — | `LeafSpec::clearance`, `margin` (A1) |
| AC-13 | **Ordre radial** des rangs, tenu par la pose seule, sans déformation d'évitement : **F devant E** (A2) ; **F devant la tige du caulicole, tige dans le jour entre deux E** ; **D derrière E** (décision Q8 de l'utilisateur, 2026-11-25 : D extérieur tenu le long de la corbeille, AC-58) ; pieds des D sur le dessus de la bague ; F–F, E–E, D–D disjointes | élévation sans coupe ; profil de la pl. XXV (trait C/D derrière la face de E sous h 24) | marge de E derrière F +0,855 / +0,753 p (K7) ; jour C/E 0,81 p ; D derrière E +2,55 / +2,10 p ; D intérieur à 1,18° de l'axe de face, D extérieur à 0,86° de la corne ; D / tige 0,059 p ; ornement et colonne sans auto-intersection (K9, K10) **[M]** | `tu_cgarchi_classical_acanthus_{ornament,cauliculus,minor_ornament}.cpp` |
| AC-14 | **Méridien d'axe de la porteuse** (plan (off, h), off = r − Base (h), Base = noyau + c + t/2) : bande de tige de E verticale (off = 0) de hStem à hFoot ; **corps en Bézier cubique** du pied (0, hFoot) incliné de footLean (**F 25°, E 0°**, tangent à sa bande) au départ du bec (off_b, h_b), vertical, contrôles à 0,4 (h_b − hFoot) ; **bec en arc** (AC-40) ; une seule directrice, enroulement θ + u hw / r ; abscisse développée y : bande [−1, 0), corps [0, 1] (abscisse curviligne), arc [1, 2] (angle) | profil gravé, sans construction | corps plus gras que la lame gravée de la pl. XXV : face − noyau de F +1,25 à +2,28 p au-dessus des cotes lues, lame 1,59 p contre 0,74 à 0,88 p gravés **[M]** ; corps hybride (S4bis) abandonné par décision de l'utilisateur (dette) ; E à 25° plie au coin bande-corps (faute 14) | `LeafBend`, `DefaultLeafPlacement` |
| AC-15 | **Volutes et hélices en rubans balayés sur spirale** (réutilisation de `BuildTwelveCentreVolute` / `SampleCurve` à vérifier) ; **fleuron** en rosace de largeur `fleuron_largeur` (4 p, lu) sur la face de l'abaque | spirale des caulicoles non construite par Vignole ; forme de la rosace non donnée | — | A4, A5 |
| AC-16 | **D = calice, deux feuilles par caulicole, différentes** (décision P3, appuyée par le relevé de la pl. XXV, plan et profil, et de la pl. XXVI) : **D intérieur** vers l'axe de face, sous l'hélice (pointe r 19,0, h 25,5, à 17 à 19,5° de la tige), **D extérieur** vers la corne, sous la volute (crête h 28, saillie r 27,4, pointe r 26,2 h 25,8 au profil) ; pied commun au haut de la bague | plan de la pl. XXV (quart q3 déroulé) **[M]a** ; profil de la pl. XXV (AC-44) ; pl. XXVI **[Q]** pour les folioles | le D intérieur posé à 17,0° de la tige (bord du relevé) : à 18,2°, il touche son miroir sur l'axe de face | `DefaultMinorLeafPlacement` |
| AC-17 | **« Volutes » décoché coupe tout l'ornement sculpté du chapiteau** corinthien (décision P7) ; réglage `capitalOrnaments` {None, Light, Heavy} (paramètre « Capital ornaments », après « Volutes », colonne et colonnade) avancé en A0 à None (décision D1) ; **défaut Light en vigueur depuis le lot A2, porté par les formes paramétrées** (colonne et colonnade neuves ; structure et JSON à None, AC-47, décision Q2 (B)) ; `opus --colonne-corinthienne` règle Light | choix d'interface | la colonnade corinthienne atteint la bannière de 300 000 faces vers **7 colonnes** au défaut Light, dès 2 en Heavy **[E]** (arithmétique sur 176 292 triangles **[M]** à 4 colonnes ; 10 colonnes estimées avec v0) | `parameterized_classical.cpp`, `opus/main.cpp` |
| AC-18 | ~~**Modèle v0**~~ — **caduque au lot A2** (2026-11-24, décision Q1 (a) : v0 retiré en entier, D et caulicoles absents jusqu'à A3). Énoncé d'origine : feuille = nervure tubulaire + 11 folioles à coque fermée, F à θ = 0 mod 45° (phase inversée), D en couronne de 8, tiges de caulicoles en tube à crosse ; toutes ses constantes de forme (0,82 / 0,57, 1,35, 1,45, 0,96, 2,2 …) sont sans source ; `segmentsPerLeaf` = max (6, `Curve segments`) | aucune | défauts mesurés au §11 (A0) | `classical_acanthus.cpp` |
| AC-19 | **`capitalOrnaments` facultatif dans `classical-column/v1`** : "none" \| "light" \| "heavy", absent vaut "none" quel que soit le défaut de la structure, écrit seulement hors de "none" (un document sans ornement reste lisible par un lecteur antérieur, et les fixtures gelées restent canoniques) ; pas de nouvelle version du schéma | aucune | un document Light ou Heavy est refusé par un lecteur antérieur (champ inconnu) | `parameterized_classical_json.cpp`, `schemas/classical-column-v1.schema` |
| AC-20 | ~~**Bornes de `AcanthusOptions::segmentsPerLeaf`**~~ — **caduque au lot A2** (champ retiré : aucun lecteur après le retrait de v0, même règle que `directrixScale` en A1e). Énoncé d'origine : [2, 32] (`kMaxSegmentsPerLeaf` = `kClassicalCurveSegmentsMax`, seul appelant de production) ; la borne porte sur l'ornement entier, 641 280 triangles à 32 **[M]** (modèle v0), au lieu d'environ 19,5 M à 1024 ; `None` valide le profil comme les autres niveaux | aucune | — | `classical_acanthus.h` |
| AC-21 | **Document gelé du catalogue (Q3)** : le test à l'octet retire du document courant, et de lui seul, les objets de paramètre d'une liste fermée de clés postérieures à Q3 (`kKeysAfterQ3` = {« Capital ornaments »}), leur nombre étant figé (2 : colonne et colonnade) ; la fixture `shape_parameters_before_q3.json` n'est pas régénérée (décision de l'utilisateur, option b). Le passage du défaut à Light au lot A2 changera la valeur de la clé écartée, sans effet sur ce test | règle du fichier de test (l. 34-38) | une clé ajoutée plus tard doit entrer dans la liste par décision, sinon le test rougit | `test/tu_cggraph_parameter_id.cpp` |
| AC-22 | ~~Feuille en 1 + 2L coques fermées~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) ; une coque par feuille | — | — | — |
| AC-23 | **Repère du gabarit** : X = u · hw du rang ; Y développé par morceaux (bande, corps, arc) ; demi-feuille u ≥ 0, miroir u → −u au plaquage, sommets de l'axe partagés | aucune | distorsion hors de l'axe : arc latéral exact (J17, X à 1e-6 p) | `LeafBend::At`, `BuildLeafSheet` |
| AC-24 | ~~Éventail des lobes~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) | — | — | — |
| AC-25 | ~~Paume et doigts~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) | — | — | — |
| AC-26 | ~~Œillets~~ — **retiré au lot A1c** : les lobes d'un même côté se recouvrent (AC-30) ; seuls les côtés opposés restent disjoints, à stackGap | pl. XXVI : œillets | œillets absents (dette) | — |
| AC-27 | ~~Contour de la lame~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) | — | — | — |
| AC-28 | ~~Relief des lobes~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) ; relief lu dans le dessin (AC-37) | — | — | — |
| AC-29 | **Maillages et budget** : coque F (feuille seule) Light 1 800 triangles / 902 sommets, Heavy 10 296 / 5 150 ; coque E (feuille et bande) Light 1 912 / 958, Heavy 11 016 / 5 510 ; **tige C** Light 720 / 362, Heavy 3 072 / 1 538 ; **coque D** (`OliveMinor`, feuille seule) Light **1 300** / 652, Heavy **2 996** / 1 500 (décision Q9′ : 600 / 3 000 visés, 600 impossible : plus de point intérieur sous 852 triangles, et la crosse du D extérieur s'auto-intersecte à 860) ; **ornement (16 F et E, 8 C, 16 D)** Light 56 256 triangles / 28 208 sommets, Heavy 243 008 / 121 584 ; **colonne corinthienne par défaut** Light 69 904, Heavy 256 656 triangles ; seuils des invariants J1 à J21′ (§11.10), K1 à K14 (§11.11), K15 à K28 (§11.13) | aucune ; budgets du dessin | — | `data/acanthus_leaves.json`, tests `tu_cgarchi_classical_acanthus*.cpp` |
| AC-30 | ~~Ordre d'empilement~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) ; l'ordre des couches est une donnée du dessin (`layer` des régions) | — | — | — |
| AC-31 | ~~Contour de la feuille W (y)~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) ; le contour est dessiné (AC-36) | — | — | — |
| AC-32 | ~~Drapé~~ — **caduque** depuis A1e (code A1b/A1c retiré au S5, 2026-11-23) : la feuille est un gabarit plan plaqué (AC-34 à AC-44) | — | — | — |
| AC-33 | **Pli** : chaque triangle de chaque face doit garder, par rapport à la normale n_S moyenne de ses sommets, l'orientation du même triangle pris sur S (`CheckFaceFold`, « pli de la face extérieure / intérieure ») | aucune | la règle A1c (\|Δm\| / \|Δn_M\| ≥ t/2 par arête) refusait des rampes de marche légitimes (F Olive Heavy 0,0997 p) **[M]** ; avec le plafond de Hb côté noyau et la garde de courbure (2 t + H précède t/2 + H), aucun gabarit valide ne la déclenche : testée sur une géométrie synthétique, et vue en relais sous la faute « garde de courbure neutralisée » | `CheckFaceFold` |
| AC-34 | **Format `cgarchi-acanthus-leaves/2`** : gabarit plan compilé hors ligne (`data/acanthus_leaves_from_drawing.py`) et embarqué ; par style et par niveau : sommets [u, y, H, Hb, région], triangles [a, b, c, région] en sens trigonométrique, préfixe de la feuille seule (F) et tout (E, avec la bande), comptes des coques ; le chargeur refuse les indices hors plage (vérifiés avant conversion), le relief négatif ou Hb non nul sous fade[0], un triangle retourné, une arête de plus de deux triangles ou parcourue deux fois dans le même sens, un sommet inutilisé, un bord qui n'est pas une boucle, des comptes faux | aucune | — | `classical_acanthus_template.{h,cpp}` |
| AC-35 | ~~Dessin Acute~~ — **caduque** (décision de l'utilisateur du 2026-11-23 : Acute retiré) | — | — | — |
| AC-36 | **Dessin Olive** : demi-feuille décalquée sur la pl. XXVI (vue XXVI-E calée sur la tige de E), feston en lobes ronds profonds (encoches de 0,20 à 0,26 en y), crête du repère à y = 1,53, bande de tige (u 0,26, rétrécissement 0,92, rangs 4 / 12) | Vignole pl. XXVI | tiers bas décalqué sur les lobes de F ; crête non redécalquée (0,13 en y) (dette) | `data/acanthus_drawing.json` |
| AC-37 | **Relief lu dans le dessin** (H face extérieure, Hb revers) : couche 0,2, cuvette 0,4, nervure 0,12 (rayon 0,35), nerville 0,12, **côte : hauteur 1,25 + arrondi 0,15 (H à l'axe 1,40 p)**, cannelure 0,1, calotte 0,12, revers 0,25 (capBack), arête 0,22 ; effacement du relief extérieur sur y ∈ [1,0 ; 1,2] ; un seul style | la côte n'a pas de source ; la pl. XXV grave une lame de 0,9 p (t + H_axe) **[M]a** | côte à 1,40 gardée (décision de l'utilisateur, S4bis abandonné) : lame 1,59 p (dette) | `data/acanthus_drawing.json` (`relief`) |
| AC-38 | **Maillage du gabarit** : Delaunay contraint (Bowyer-Watson, prédicats exacts, Sloan puis Lawson), pas par bissection sur le budget (1 800 / 10 300 ± 2 %), angle minimal du gabarit et des faces plaquées 1° (Light) / 3° (Heavy) ; J11 mesure la qualité du maillage (pas d'aiguilles), pas la forme | aucune | — | script du gabarit ; test `the_faces_keep_their_angles` |
| AC-39 | **Plafond bilatéral** : H ≤ ψ Free (+n_S), Hb ≤ min (ψ Free (−n_S), CoreRoom (−n_S)), ψ = 0,8 ; Free : place sous LeafBoundRadius − margin − t/2 le long de la normale ; CoreRoom : place au-dessus de CoreRadius + c + t/2 ; recherche par pas de 0,02 p puis 40 bissections, arrêtée à la valeur qui peut encore plafonner (reliefScale H / ψ, reliefScale Hb), au plus 40 p (`kMaxReach`) ; le relief voulu est rendu tel quel quand la place atteint la portée | aucune | — | `LeafBend::Free`, `LeafBend::CoreRoom`, `BuildLeafSheet` |
| AC-40 | **Bec en arc borné par le cône** : arc de courbure constante R_S = bendRadius − t/2 − Hb_ref (Hb_ref = capBack 0,25 p) de la verticale montante à φ_t, crête à hTop ; R_extrados F 2,44 / E 3,23 p, φ_t 169,6° / 172,3° (ajustement sur 4 cotes de la pl. XXV) ; off_b plus grand écart tel que l'extrados de l'arc entier reste sous LeafBoundRadius − margin, R_S réduit par pas de 2 % sinon ; **borne des feuilles F et E = max (noyau, cône)**, sans l'abaque | pl. XXV (cotes corrigées AC-44) | J18 : résidus de −0,23 à +0,21 p ; contact de la face extérieure −0,27 à −0,28 p (J16′) **[M]** | `LeafBend`, `CapitalEnvelope::LeafBoundRadius` |
| AC-41 | **Métrique par rang** : hw F 4,85 p, E 5,25 p ; rapport de longueur du corps F 1,006, E 1,293 (contrôle J17) ; hFoot de E 9,40 p, hTop F 12, E 24 ; ~~Acute : mêmes valeurs~~ (caduc) | pl. XXVI (hw), pl. XXV (hauteurs) | hw_F lue sur la pl. XXVI seule (dette) | `DefaultLeafPlacement` |
| AC-42 | **Bande de tige de E** : rangs de quadrilatères sous le bord de pied (u ≤ stem.u), H de la tige, Hb nul ; budget de E = F + bande | pl. XXVI | — | script du gabarit ; `BuildLeafSheet` |
| AC-43 | **Dôme de calotte** sur l'arc : déplacement κ_d u_cap hw (u / u_cap)² vers le centre de l'arc, **κ_d = 0,2**, rampe **smootherstep (C²)** de max (1, fade[1]) à la crête ; l'effacement du relief du gabarit (fade[0] à fade[1]) est lui aussi un smootherstep (`data/acanthus_leaves_from_drawing.py`, S4bis-2) | aucune | κ_d sans source (dette) | `LeafBend::Dome`, `_smootherstep` du générateur |
| AC-44 | **Correction affine des rayons de la pl. XXV** : r_donnée = 0,9375 r_image + 2,06 (le nu du fût 13,8 p gravé contre 15 p au JSON) | calage du plan de la pl. XXV | hypothèse **[H]** ; le bord gravé de la corbeille tombe 3 à 5 px (0,25 à 0,4 p) plus loin que le noyau corrigé (dette) | instruments de plan (scratchpad) |
| AC-45 | **Pose et copie par rotation** : F et tiges C à θ = π/8 + kπ/4, E à θ = kπ/4, k = 0..7 (`detail::RankTheta`, `kLeavesPerRank` = 8) ; une pièce représentante par rang et par niveau, copiée par rotation autour de l'axe en double, avant la conversion en float ; **D : quatre représentantes** (D intérieur et D extérieur des tiges 0 et 1, images miroir l'une de l'autre), copiées par rotation de **π/2** (groupe C4 de l'abaque) ; l'ornement a la symétrie D4, pas celle de la rotation de π/4 | phase P1 : pl. XXV et XXVI | copie et construction identiques (K3 2,8e-16 p ; K17 0) ; K18 : D4 ≤ 8,3e-15 p, rotation de π/4 2,11 / 1,92 p (contrôle négatif) **[M]** | `classical_acanthus.cpp` (`BuildCorinthianOrnament`, `RotateAboutAxis`) |
| AC-46 | **Ordre des pièces de `CapitalOrnament`** : 8 F, 8 E, 8 C (θ croissant), puis 16 D (tige k croissante, D intérieur puis D extérieur) ; une pièce = une coque fermée ; sommets et faces dans l'ordre de sa construction | aucune | — | `classical_acanthus.h` ; K1, K3, K16, K17 |
| AC-47 | **Défaut des ornements porté par les formes** : colonne et colonnade paramétrées neuves à « Capital ornaments » = Light ; `ClassicalColumnSettings ()` et un document `classical-column/v1` sans le champ à None (AC-19 inchangé) | décision Q2 (B) de l'utilisateur, 2026-11-24 | un lien profond ou un document cggraph sans le paramètre ouvre une colonne Light ; un document JSON sans le champ, et `cgarchi-cli`, restent sans ornement | `parameterized_classical.cpp` (constructeurs) ; K14 |
| AC-48 | **`EnvelopeExcess` par pièce, borne du rang** : F, E et D contre `LeafBoundRadius` (cône) ; autres rangs (C, volutes, hélices) contre `EnvelopeRadius` (cône et abaque) ; fleuron −∞ | AC-40 ; Q3″ pour D | tiges C −0,91 p contre l'abaque des données (le plus grand débord est sur le flanc tourné vers l'axe de face) ; D −2,63 / −2,62 p contre le cône **[M]** | `classical_acanthus.cpp` ; test de l'instrument à 22,5° (F, E, D −0,1 ; C > 3 p) |
| AC-49 | **Pied des feuilles sur l'astragale** : F à hStem = hFoot = **0,70 p** (majorant arrondi de (t/2 + H_pied) sin γ + c = (0,1 + 1,40) sin 25° + 0,05 = 0,684, H_pied pris à la côte) ; E : bande de tige à partir de hStem = **c** (0,05 p) ; garde : aucun sommet de feuille sous h = c (« pied sous le plancher du chapiteau ») | pl. XXV et XXVI : les feuilles reposent sur le haut de l'astragale, sans jour ni recouvrement visibles (±1 p) | sans relèvement, 760 / 1 520 auto-intersections avec l'astragale (Light / Heavy) **[M]** ; plus bas sommet de F 0,1685 p, de E 0,0500 p **[M]** ; corps de F raccourci : J17 0,787 contre 0,842 (référence du décalque 0,84 ± 0,10) | `DefaultLeafPlacement`, `BuildLeafSheet` ; K10 |
| AC-50 | **Tige du caulicole** : tube droit d'axe vertical à r_C **16,9 p**, θ = π/8 + kπ/4, **Ø 2,0 p**, **8 cannelures** (corde 0,55 p, flèche 0,15 p, fond en arc de cercle, centrées sur le plan radial), effacées sur 0,3 p sous la bague (smootherstep) ; **bague à deux cordons** en demi-tores aplatis (exposant 3) : 15,3 à 16,6 (rayon 1,2), gorge à 16,6 (rayon 1,0), 16,6 à 17,8 (rayon 1,55) ; bouts en disques ; pas de torsion (décision Q1) | pl. XXVI **[M]a** (axe, Ø, bague) ; nombre de cannelures 6 à 10 **[Q]**, N = 8 convention | diamètre du tuto (2,5 à 3,3 p) réfuté : c'est celui de la bague | `CauliculusSpec`, `BuildCauliculusSheet` |
| AC-51 | **Ordre radial de C et D** : voir AC-13 ; marge ≥ c sur chaque paire voisine | pl. XXV et XXVI | — | K21 |
| AC-52 | **Naissance de la tige** à h_C0 = **12,40 p** (crête extrados de F, 12,35, + c) ; garde « pied de tige sous la crête de F » | pl. XXVI : tige visible dès 11,2 / 12,25 **[M]a** | 0,15 à 1,2 p de tige visible chez Vignole couverts par la lame de F, plus épaisse que la gravure (dette S4bis) | `BuildCauliculusShell` |
| AC-53 | **Pose explicite du bec des D** (`LeafPlacement::explicitBend`, `bendCenter`) : centre de l'arc posé sur les cotes au lieu du contact avec la borne ; garde « pose explicite au-dessus de la borne » (cône) ; D intérieur : R_extrados 1,5, centre (18,1 ; 25,0), φ_t 160,5° ; D extérieur : R_extrados 2,0, centre (24,7 ; 26,0), φ_t 187,6° ; **bec en arc** (AC-40), à 0,70 p au mieux des trois cotes du profil (crosse gravée en spirale : dette) ; tolérance des cotes de D ±1 p | profil et plan de la pl. XXV (AC-44) | — | `LeafBend`, `DefaultMinorLeafPlacement` |
| AC-54 | **Pied des D sur le dessus de la bague** : h **17,87 p** (haut de la bague 17,8 + c + 0,02 pour le relief du bord), `footOffset` **0,5 p** (corps décollé de la corbeille au pied) ; pied décalé de la tige vers son côté, **calculé** pour que le contour du corps (y ≤ 1) reste à c / r du plan axial de la tige : −5,59° (D intérieur), +4,36° (D extérieur) **[M]** | pl. XXVI : bases des D au nœud | les bases des D ne couvrent pas le tour de la bague, elles naissent sur son dessus (invisible de face) | `DefaultMinorLeafPlacement` |
| AC-55 | **Gabarit D = `OliveMinor`** (dérivé d'Olive, AC-1), budget 1 300 / 2 996 (AC-29), angle minimal 6,66° / 10,57° ; relief réduit (`reliefScale`) : D intérieur **0,35**, D extérieur **0,30** (garde de courbure du petit bec et du coude du corps tenu) ; demi-largeur hw : D intérieur **2,4 p** (largeur projetée de la pl. XXVI), D extérieur **2,3 p** | pl. XXVI : 4 à 6 folioles par côté **[Q]**, compatible avec les 5 lobes d'Olive (décision Q2) | relief du D extérieur ramené de 0,44 à 0,30 (dette) | `DefaultMinorLeafSpec` |
| AC-56 | **Échantillonnage des tiges** : n_φ = N s, s = 3 (Light) / 6 (Heavy) ; n_z = 2 + rampe (3 / 6) + 2 cordons (5 / 12 anneaux chacun) ; une coque par tige (bague comprise), 2 n_φ n_z triangles, n_φ n_z + 2 sommets | aucune | — | `DefaultCauliculusSpec`, `CauliculusRings` |
| AC-57 | **Axe des D incliné par cisaillement** (décision Q6′, méthode ; Q6″, angle) : méridien u enroulé à θ + tan α S (y) / r_0 (y) + X / (cos α r), ancré au pied (S (0) = 0) ; **α déduit** du pied (AC-54) et de la pointe relevée (D intérieur à θ_C ∓ 17,0°, D extérieur à θ_C ± 19,0°) par un point fixe sur une seule porteuse : α = **20,2°** (D intérieur), **19,3°** (D extérieur), journalisé | pointes : plan de la pl. XXV **[M]a** | lecture D-7 de 30 à 35° (**[Q]**, sans grille) contredite (dette) ; contour cisaillé (lobes inclinés), non discernable sur les planches | `LeafPlacement::axisLean`, `LeafBend::At` |
| AC-58 | **Corps tenu du D extérieur** (`LeafPlacement::bodyHold` = 23,1 p) : droite verticale à `footOffset` jusqu'à 23,1, coude d'un quart de tour vers l'extérieur, palier horizontal, coude d'un quart de tour vers le haut jusqu'au départ du bec (rayons (h_b − h_t) / 2 = 1,45 p) ; au plus 0,905 p de la corbeille sous h 24,3 **[M]** ; garde « corps tenu » (pose explicite, footLean nul, palier ≥ 0) ; sans effet par défaut (F et E identiques à l'octet) | profil de la pl. XXV : D le long de la corbeille, derrière E, sous h 24, dégagé au-dessus de 24,3 | **marche d'escalier** au profil, contre la lame diagonale gravée : acceptée par l'utilisateur (Q8) et liée à la dette S4bis (épaisseur de E) | `LeafBend::PlaneJet` |
| AC-59 | **Courbe des volutes et des hélices** : contour extérieur de la construction de Vignole à 12 centres (`BuildTwelveCentreVolute`, raccord `CentreLine`) pour un œil de diamètre 2 k et un sommet à 9 k du centre (homothétie des cotes ioniques) ; **volutes k = 0,470**, **hélices k = 0,36** ; pas de bascule vers une spirale logarithmique (décision Q2 : résidus ≤ 1 p, tours à ¼ près) | volute : profil de la pl. XXV **[M]a** (k 0,470 ; résidus œil 0,34, H +0,02, V −0,02 p ; 3 tours) et pl. XXVI (k 0,475) ; hélices : pl. XXVI **[M]a** (V 5,5 à 6,0, k 0,344 à 0,375 ; milieu retenu) | la construction ionique n'est pas celle des caulicoles (Vignole ne construit pas leur spirale) ; le ruban s'arrête avant le 3e tour (2,58 / 2,66 tours pour les volutes, 2,50 / 2,56 pour les hélices, AC-60) | `classical_acanthus_volute.cpp` (`BuildVoluteRibbon`), `DefaultVoluteSpec`, `DefaultHelixSpec` |
| AC-60 | **Ruban** : bord extérieur sur le contour (AC-59) ; bord intérieur décalé vers le centre de l'arc de w (φ) = min (loi des tours, \|O\| − k − c), loi linéaire en angle entre les largeurs au sommet des tours 1, 2, 3 (**volutes 1,43 / 0,70 / 0,60 p**, **hélices 0,90 / 0,44 / 0,38 p**), constante ensuite ; arrêt à la première station où w < 0,15 p ; garde w < rayon local − c ; **profondeur b = 1,0 p** ; section : dos plat à −b/2, face vue à +b/2 portant **deux bourrelets** (demi-sinus de hauteur 0,175 w sur 0,35 de la largeur chacun) et une **rainure** centrale (0,30 de la largeur, profondeur 0,15 p), parts liées par 2 × bourrelet + rainure = 1 (garde) ; échantillonnage Light 3 stations par quart, face vue 2 + 2 + 2 segments ; Heavy 8, 4 + 4 + 4 | largeurs des volutes : coupes au-dessus de l'œil, profil de la pl. XXV **[M]a** (35,8 → 34,37 ; 33,9 → 33,2 ; 32,65 → 32,05) ; largeur du 1er tour des hélices : pl. XXVI **[Q]** (0,8 à 1,0 p) ; tours 2 et 3 des hélices au rapport des volutes **[E]** ; b : bande de 3,4 p de la volute voisine vue de chant **[E]** (b ≈ 0,94) ; bourrelets et rainure : convention **[Q]** (la gravure ne les cote pas) | borne de l'œil acceptée par l'utilisateur : sans elle, le bord intérieur touche l'œil avant le 3e tour (jour 0,01 p) **[M]** ; le listel construit (0,47 p) n'est pas employé | `VoluteSpec` (`turnWidths`, `minWidth`, `depth`, `bead*`, `groove*`) |
| AC-61 | **Œil** : disque de rayon k, épaisseur b + 0,1 p, **coque à part** dans la pièce (une pièce de volute ou d'hélice = ruban + œil) ; jour avec le ruban ≥ c par la borne de AC-60 ; 12 segments (Light), 32 (Heavy) | œil Ø 1,0 ± 0,2 au profil de la pl. XXV **[M]a** (construction 0,94) | pas d'union booléenne dans le dépôt : deux coques | `BuildVoluteRibbon` (`eyeSegments`, `eyeExtraDepth`) |
| AC-62 | **Tige du ruban**, née cachée derrière la feuille D de son côté (décisions Q7 et Q7′ de l'utilisateur) : sur la corbeille à l'azimut θ_b, centre à noyau + c + b/2 + `stemLift`, à plat (normale de la face vue = normale extérieure du noyau) jusqu'à `stemBend`, puis Hermite cubique G1 jusqu'à la première station de la spirale ; repère à rotation minimale depuis `stemBend`, torsion répartie par smootherstep ; gardes : rayon de courbure de la ligne centrale ≥ demi-diagonale de la section + c, vitesse de torsion ≤ 0,5 rad/p, rampe d'épaisseur (`stemThin` < 1) pas plus longue que la tige. **Volutes** : θ_b = θ_C + 6° (**28,5°** pour la tige à 22,5°, écart acté par l'utilisateur), **plus bas point à h 24,5**, coude à h 27,5, tangentes 0,5 × la corde, `stemLift` 0 ; **pied aminci** : épaisseur 0,8 b au pied, face intérieure fixe à noyau + c, rendue à b par smootherstep (C²) sur 1,0 p de longueur développée ; pied **« en partie caché »** : vu radialement, 0,649 des nœuds de la tige entre h 24,5 et 24,6 ont une pièce F, E, C ou D devant eux (borne 0,63, part d'avant l'amincissement 0,636), aucun à h 24,75 et 25,0 ; jour au D extérieur 0,122 (Light) / 0,125 (Heavy) p. **Hélices** : θ_b = **12°** de l'axe de face, plus bas point à h 25,5, coude à h 27,0, tangentes 0,8, **`stemLift` 0,03 p**, pied non aminci | pl. XXVI : tige des volutes visible dès h 24,5, des hélices dès h 25,5 **[Q]** ; profil de la pl. XXV : tige au-dessus de la crête de D (h 28) **[Q]** ; place derrière les D mesurée sur le maillage (`channel_fine.py`) **[M]** | naissance à h 23,5 (Q7) impossible : 0,51 à 0,62 p de place pour 1,10 nécessaires **[M]** ; pied visible entre D intérieur et D extérieur de h 24,75 à 25 (dette) ; θ_b, coude, tangentes et `stemLift` sont des valeurs de faisabilité (balayages par sonde), pas des cotes | `VoluteSpec` (`stemTheta`, `stemFoot`, `stemBend`, `stemTangents`, `stemLift`, `stemThin`, `stemThinLength`, `maxTwistRate`) ; tests `the_stem_foot_is_partly_hidden_behind_the_leaves`, K36 |
| AC-63 | **Pose des spirales** : **volutes** dans un plan vertical à **22,5° du plan de la face** (décision Q6′), œil à **R 27,6, θ = corne ∓ 6°** (côté de sa tige), **h 31,5** ; paire **disjointe en dièdre de 45°** sous chaque corne (Q6), jour de la paire 1,97 p ; **hélices** dans un plan vertical à **45° de la face**, œil à **3,45 p de l'axe de face**, **h 30,05**, à **21,5 p en avant de l'axe** ; jour de la paire sur l'axe de face 2,12 p (2,2 ± 0,6 relevé), réservé à la tige du fleuron (A5) ; représentantes des tiges 0 et 1, images miroir par le plan θ = π/4 | volutes : œil du profil de la pl. XXV et de la pl. XXVI réunis **[M]a** (R cos (θ − 45°) = 27,5, R sin θ = 17,4) ; h 31,5 relevé au profil (écart accepté : à h 31,3 la garde du cône rate de 0,019 p) ; raccourci vu de face 0,98 (relevé 1,0 ± 0,1) ; hélices : x et h de l'œil sur la pl. XXVI **[M]a**, raccourci 0,85 (relevé 0,6 à 0,9) ; profondeur de l'œil des hélices **non relevée** | profondeur 21,5 au lieu de ≈ 18,0 **[E]** du plan : en deçà de 19,4, la tige se courbe sous sa garde ; de 19,4 à 21,0, elle recoupe la spirale (balayage par sonde **[M]**) ; les hélices sortent de l'abaque des données de +6,3 p (AC-2) | `DefaultVoluteSpec`, `DefaultHelixSpec` (`eyeRadius`, `eyeTheta`, `eyeHeight`, `planeAngle`) |
| AC-64 | **Contour de l'abaque : celui des données** (décision Q4 (a) de l'utilisateur) : le creux construit (milieu de face 18,64 p pour A) est celui du texte de la pl. XXV et de l'arc tracé au compas sur le plan (18,60) ; le second contour hachuré du plan (milieu de face 20,72 p), non décrit par le texte, n'est pas modélisé ; les instruments de planche calent le plan de la pl. XXV sur les faces coupées des cornes (centre (602,1 ; 1 152,2) px, 12,164 / 12,274 px/p) | plan de la pl. XXV **[M]a** (calage A1e : faces coupées à 37,1 / 37,5 p en x ; calage des cornes : B 31,86 à 32,00, listel 33,46 à 33,64) | nature du second contour **[H]** (lecture concurrente : B ≈ 18,6, A ≈ 20,7 au milieu de la face) ; en dette | instruments de planche (scratchpad) ; tour inchangé |

**Conventions modifiées au lot A4** (2026-11-25 et 26 ; les lignes ci-dessus restent, ces compléments prévalent) :

| # | Complément | Appui |
|---|---|---|
| AC-2 | Portée réduite : **volutes et hélices bornées par le cône** (garde stricte), comme F, E et D ; leur écart à l'abaque des données est journalisé : **exceptions** du type de celle du D intérieur, volutes +3,65 p, **hélices +6,3 p** **[M]** | décision Q4 (a) ; texte de la pl. XXV (« foglie, et caulicoli » bornés par la droite de la corne) |
| AC-4 | Contour de l'abaque des données conservé (AC-64) | Q4 (a) |
| AC-13 | **D extérieur devant la tige de la volute, D intérieur devant la tige de l'hélice** ; spirales au-dessus des crêtes de D ; marges **[M]** : tige derrière le D extérieur +0,54 / +0,35 p, derrière le D intérieur +0,15 / +0,12 p ; jour volute / autres pièces ≥ 0,122 p, hélice / autres pièces ≥ 0,13 p (sommets contre triangles) | AC-62 |
| AC-15 | Volutes et hélices construites (AC-59 à AC-63) ; fleuron toujours en A5 | — |
| AC-17 | La colonnade corinthienne par défaut (4 colonnes, Light) dépasse la bannière de 300 000 faces (≈ 4 × 84 224 + entablement **[E]**) : avertissement accepté (décision Q5), budgets revus en A6 | Q5 |
| AC-29 | Volute Light 904 triangles / 456 sommets, Heavy 3 600 / 1 804 ; hélice Light 886 / 447, Heavy 3 510 / 1 759 ; **ornement Light 70 576 / 35 432, Heavy 299 888 / 150 088** ; **colonne 84 224 / 313 536** triangles **[M]** | tests K29 |
| AC-45 | Volutes et hélices : **deux représentantes** (tiges 0 et 1, miroir par θ = π/4), copiées par rotation de π/2 ; symétrie D4 (0 p), rotation de π/4 > 7,9 p (contrôle négatif) **[M]** | K31, K32 |
| AC-46 | Ordre des pièces : 8 F, 8 E, 8 C, 16 D, **8 V, 8 H** (tige k croissante) ; une pièce V ou H = **deux coques** (ruban, œil) | `classical_acanthus.h` |
| AC-48 | `EnvelopeExcess` par pièce : **volutes et hélices contre le cône**, comme F, E et D | AC-2 |
| AC-53 | Pose des D : les quatre poses sont calculées une fois et gardées en cache (`DefaultMinorLeafPlacements`, clé = contenu de l'enveloppe en parties, sous verrou) ; l'enveloppe est validée à chaque appel, avant la lecture du cache ; sorties identiques à l'octet | S1′, revue S7 |

**Conventions du lot A5 (fleuron G, 2026-11-26 et 27)** ; fautes numérotées 5xxx au rejeu S5 (`a5s5_faults.py`), 48 sur 48 rouges ; après la relecture S7 : **55 sur 55** (51156 retirée, la hauteur du fleuron étant un invariant vérifié par `assert` ; 5301 à 5308 ajoutées, une par garde de `BuildFleuron` et de `ReadCapitalTour`) :

| # | Convention | Appui | Écart | Code |
|---|---|---|---|---|
| AC-65 | **Pétales** : 16 cuillers, bout circulaire de rayon **0,582** (dérivé du jour), flancs tangents issus du centre, **jour 0,15 p** entre bouts voisins, encoche **1,55** ; une pointe en haut et en bas | N = 16 : pl. XXVI **[Q]** (déroulé polaire, 16 ± 2) ; forme, jour, encoche : aucune | plan de la pl. XXV : 5 à 6 lobes cannelés (dette) ; fautes 51150 (14 pétales), 5161 (disque), 5184 (pas de jour) rouges | `PetalAt`, `FleuronSpec` |
| AC-66 | **Cotes** : Ø **7,9** (cercle), centre au haut du chapiteau − c − 3,95 (h 38,0), face avant r **21,66**, bouton Ø **2,5** concentrique de flèche **0,45** (décision K50 : débord sur B 4,51 ≤ 4,56) | profil XXV et pl. XXVI **[M]a** (Ø 7,70 à 7,96 vertical, centre 37,77 à 38,43, face 21,69) ; flèche : règle R3 | la pl. XXVI met le bouton 1,0 p plus bas, sous une languette (dette) ; Ø horizontal relevé 8,4 à 8,8 (Q-G2 : cercle) ; fautes 5172 (flèche 0,6), 5155 (face + 0,2), 5153 (phase), 5156 (centre relevé : K46 et K50) rouges | `BuildFleuron` |
| AC-67 | **Borne du fleuron** : carré des cornes de B (x ≤ 22,58), débord sur la face concave de B dans [3,06 ; 4,56], h ≤ haut − c ; **jour ≥ c au maillage réel du tour** (`TourDepthMax` : plus grand x du tour sur une boîte (h, y), chaque triangle découpé ; corde au réglage de la colonne, 8 à 256 secteurs ou grille des cannelures) | profil XXV (+4,06) ; décision de l'utilisateur (corde réelle) | `Ring::RadiusAt` passe derrière la corde (0,19 p à 8 secteurs, 0,012 à 48) ; le critère « sous le contour de l'abaque » est impossible (Vignole grave le débord) ; fautes 51155, 5170, 5171, 5177, 5154 rouges | `ReadCapitalTour`, `TourDepthMax` |
| AC-68 | **Relief** : calotte du bouton 0,45 ; cuiller : creux 0,15 pondéré par 1 − (δ / flanc)², bout reculé de 0,3 ; épaisseur minimale **0,3** | aucune | fautes 5162, 51162 et 55162 (test contre la cote 0,3, plus contre la spec) rouges | `FleuronSpec` |
| AC-69 | **Dos** : **noyau** ρ ≤ 1,0 (caché derrière le bouton Ø 2,5) conforme au tour à c ; **pétales en tambour plan** à 21,66 − **2,35**, ramené au tour + c devant le cône ; **une seule coque** (deux coques se recouperaient ou se toucheraient) | tambour : milieu du relevé 2,17 à 2,51 (profil XXV **[M]a**) ; noyau : aucune | flanc du tambour crénelé de profil (dette) ; fautes 5180 (pignon), 5181 (noyau décollé), 5182 / 51182 (noyau hors du bouton), 5183 (tambour sans relève) rouges | `BuildFleuron` |
| AC-70 | **Largeur du fleuron prise au relevé** (≈ 8 p), et non à `fleuron_largeur` « 4 » du JSON, non lu par le module (décision Q-G0 (a)) | trois vues de Vignole **[M]a** | le « 4 » cote vraisemblablement le pan coupé de la corne **[H]** ; JSON non corrigé (dette) | — |
| AC-71 | **Tige** : demi-jonc à dos plat sur la corbeille, axe de face, sections horizontales de h **24,0** (Q-G4 (a′), pied caché derrière la pointe de E) à **33,0** ; largeur **0,67**, évasée par smootherstep de 30,8 à 33,0 jusqu'à **0,94** ; flèche **0,35** ; dos à `TourDepthMax` + c sur la boîte de l'anneau, chaque facette vérifiée sur 8 sous-intervalles ; sommet au milieu du dos (diagonales et éventails symétriques) ; deuxième coque de la pièce G, jour à la rosace 1,41 / 1,15 p | largeur 0,69, pointe du cornet 30,78, haut 33,02, cornet 0,94 : pl. XXVI **[M]a** ; flèche, dos : aucune | plan : « dos à noyau + c » (jour normal 0,044 sur la pente du cône) et « haut à c sous la rosace » (spécification validée : 33,0) ; nœuds absents (dette) ; fautes 5158, 5159, 5160 / 51160, 5190 à 5192, 5194 à 5197, 5301 à 5307 (gardes de la tige et des valeurs négatives), **55179** (contrôle des facettes, test `the_stem_back_clears_a_tour_bulging_between_its_rings`) rouges | `AppendStem` |
| AC-72 | **Échantillonnage** : rosace Light 8 échantillons par pétale × 4 anneaux, Heavy 12 × 9 ; tige Light 6 × 10, Heavy 12 × 24 | aucune | — | `DefaultFleuronSpec` |
| AC-73 | **Mode enveloppe d'ornement** (`ColumnOptions::ornamentEnvelope`) : `ReadCapitalTour` lit les anneaux circulaires des bandes d'ornement du chapiteau au noyau de `env` ; rosace et tige sont celles du mode noyau, à l'égalité, et recouvrent le volume schématique comme les feuilles | décision de l'utilisateur (S2) | sans cette lecture, `BuildFleuron` levait (épaisseur −11,8) dans 4 des 48 configurations ; fautes 5178 et **55178** (enveloppe lue à 1 % de son écart au noyau, rouge sur l'assertion) | `ReadCapitalTour` |
| AC-74 | **`AcanthusOptions::radialSegments`** (défaut 48) : réglage du tour qui portera l'ornement ; `BuildClassicalColumnMesh` le transmet | décision de l'utilisateur (corde réelle) | faute 5177 (ornement lu à 48 secteurs) rouge | `classical_acanthus.h` |

**Conventions modifiées au lot A5** (les lignes ci-dessus restent, ces compléments prévalent) :

| # | Complément | Appui |
|---|---|---|
| AC-2 | Le fleuron est hors de la borne max (noyau, min (cône, abaque)) : sa borne est AC-67 | AC-67 |
| AC-15 | Fleuron construit (AC-65 à AC-71) ; la largeur « 4 » du JSON n'est pas employée (AC-70) | Q-G0 (a) |
| AC-17 | La colonnade corinthienne par défaut (4 colonnes, Light) dépasse la bannière de 300 000 faces (≈ 4 × 92 880 + entablement **[E]**) | Q5, Q-G6 |
| AC-29 | Rosace Light 2 048 / 1 026, Heavy 6 912 / 3 458 ; tige 116 / 60, 572 / 288 ; pièce G 2 164 / 1 086, 7 484 / 3 746 ; **ornement 79 232 / 39 776 (Light), 329 824 / 165 072 (Heavy)** ; **colonne 92 880 / 343 472** triangles **[M]** | Q-G6 ; tests K45 |
| AC-45 | Fleuron : une représentante (face θ = 0), copiée par rotation de k π/2 ; symétrie D4 à 6,6e-15 p ; contrôle négatif : la rotation de π/4 envoie un sommet des G à **9,21 p** de tout sommet de l'ornement (plus proche voisin exact, S7) **[M]** ; fautes 5151, 5152, 5175, 5176 rouges | K47, K48 |
| AC-46 | Ordre des pièces : 8 F, 8 E, 8 C, 16 D, 8 V, 8 H, **4 G** (indices 56 à 59) ; une pièce G = **deux coques** (rosace, tige) ; préfixe F à H identique à `5729265` (K56 : topologie et somme pondérée partout, octets sous `_MSC_VER`) ; fautes 5163, 5164 rouges, **55163** (copies des H × (1 + 1e-9)) rouge sur les octets seulement | `classical_acanthus.h` |
| AC-48 | `EnvelopeExcess` d'un G : −∞ ; `CoreClearance` de l'ornement entier (K5, instruments) mesurée sur F à H : au-dessus des bandes, `CoreRadius` prolonge le cône (≈ 21,6 à h 38) ; la borne des G est le tour (K49′) | décision de l'utilisateur (S2) |

**Conventions nouvelles du lot A6** :

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| AC-75 | **Bannière de la colonnade** : seuil 300 000 faces inchangé ; le conseil sur `Capital ornaments` descend d'un niveau (Heavy : « a Light », Light : « a None »), rien hors du corinthien orné ; dépassement accepté, sans plafond (décision (f2)) | décisions Q-A6-3 (a) et (f2) de l'utilisateur | dépassement dès **N = 4** (Light, 374 436 faces) et **N = 1** (Heavy, 344 588) **[M]** (K61) ; sans ornement de chapiteau construit (ionique cannelé à volutes, 12 colonnes, 1 360 540 faces ; corinthien cannelé à « Volutes » décoché, 12 colonnes, 1 409 508), bannière sans conseil sur `Capital ornaments` **[M]** (K61) ; fautes 6201 (seuil 3 000 000), 6202 (conseil sans le niveau), 6501 et 6502 (chaque condition de la garde retirée) rouges | `parameterized_classical.cpp` (`CapitalOrnamentsAdvice`) |
| AC-76 | **Seuils d'acceptation WASM** (Node, cache chaud, médiane de 21) : colonne ≤ 50 ms (Light), ≤ 150 ms (Heavy) ; colonnade N = 4 Light ≤ 200 ms | aucune (décision Q-A6-4) | **[M]** 21,1 / 55,6 / 42,1 ms (Node 22.19, Emscripten 6.0.8) ; machine de mesure unique | sonde `probe_a6.mjs` (hors dépôt, scratchpad `a6\s4\`) |
| AC-77 | *Non prise* : refus nommé de la colonnade Heavy au-delà de k colonnes | décision (f2) : bannière seule | colonnade N = 12 Heavy sous Node 338 ms, tas 247 Mo ; affichée sans incident dans Firefox **[M]** | — |
| AC-78 | **Débord de l'abaque d'un rang** (mesure de P4) : max sur les sommets du rang de r − `EnvelopeRadius` (h, θ), borne max (noyau, min (cône, contour de l'abaque des données)) d'AC-2 ; mesure figée à ±0,05 p par rang et par niveau (K60), pas une garde : la borne du rang reste le cône (AC-40) | P4 réfutée par la planche (décision Q-A6-2 (a)) ; tolérance sans appui | G (+1,98 / +1,99) est mesuré contre le noyau relevé (h ≥ 30,85 sur l'axe de face) ; contre le seul contour en plan, il déborde de +4,51 **[M]** | `tu_cgarchi_classical_acanthus_measures.cpp` |
| AC-79 | **Instrument par rang** (`DISABLED_ornament_measures_by_rank`) : représentantes (F, E, C : la pièce 0 ; D : les quatre poses ; V, H : tiges 0 et 1 ; G : face 0), médianes de 21 après chauffe ; froid = premier appel d'un processus, 5 processus ; couverture sur h 1 à 23 et \|y\| ≤ 10 (AC-7) ; jour plafonné à 3 p ; CSV et maillage dans `CG_ACANTHUS_MEASURES_DIR` | aucune | biais : F, E, C, D comptent la conversion en `Mesh` (majorant) ; `CoreClearance` de G lu contre le noyau prolongé au-dessus de la bande : sans objet | idem |

**Compléments du lot A6** (les lignes ci-dessus restent, ces compléments prévalent) :

| # | Complément | Appui |
|---|---|---|
| AC-2 | **P4 mesurée par rang et réfutée par la planche** : débord de l'abaque des données F +2,99 / +3,03, E **+9,34 / +9,40**, C −0,91, D +1,68 / +1,69 (D extérieur +1,33), V +3,65, H +6,29 / +6,31, G +1,98 / +1,99 p (Light / Heavy) **[M]** (K60) ; sur le plan de la pl. XXV, les pointes de E sont à r 27,0 à 28,3 **[M]a** contre 17,6 pour l'abaque des données ; la borne de toutes les pièces de la corbeille est le cône (AC-40) | décision Q-A6-2 (a) ; K60 |
| AC-17 | Colonnade corinthienne : E (N) = 516 + 600 N faces (entablement et ornements) ; bannière dès N = 4 (Light) et N = 1 (Heavy) | K61 |
| AC-29 | Colonnade : Light N = 1, 3, 4, 12 → 93 996, 280 956, 374 436, 1 122 276 ; Heavy N = 1, 4, 12 → 344 588, 1 376 804, 4 129 380 triangles **[M]** | K61, instrument de coût |
| AC-69 | Dos de la rosace : le plus grand des deux dos lus (boîte du sommet, boîte du symétrique) est gardé ; une lecture unique (réunion des boîtes) est réfutée : le maillage du tour n'est pas son propre miroir **[M]** (borne 0,089 / 0,060 / 0,020 / 0,022 p à 8, 48, 256 secteurs et cannelé) | K63 ; option (a) de l'utilisateur |

**Conventions nouvelles du lot A7** :

| # | Convention | Sources | Écart | Où |
|---|---|---|---|---|
| AC-80 | **Borne en plan des volutes** : tout sommet de V à r ≤ R_A (θ) − c, R_A l'anneau de l'arête supérieure de l'abaque construit (sommet le plus haut de « A · cimatio dell'abaco (talon) » dans le profil, `CapitalEnvelope::abacusOutline` : carré concave, corne 36 p, milieu de face 18,64 p) ; garde du ruban (« hors de l'abaque en plan »), après celle du cône ; `ReadCapitalEnvelope` lève si le talon manque ou n'a pas de sommet ; hélices exemptées (`VoluteSpec::planBound` faux) | décisions Q-A7-0 (a) et Q-A7-2 de l'utilisateur ; pl. XXV (plan, profil) et pl. XXVI : volute sous la corne, en deçà du bord gravé **[M]a** | V reste hors de l'abaque des données B de +2,44 p **[M]** (K60) : la planche montre le ruban sur la bande du cimaise | `BuildVoluteRibbon`, `ReadCapitalEnvelope`, `DefaultVoluteSpec`, `DefaultHelixSpec` |
| AC-81 | **Pose des volutes C1** : plan vertical à 22,5° de la face (inchangé), œil à **R 27,2 p, θ = corne ∓ 5°** (côté de sa tige ; `eyeTheta` 40° pour la tige 0), h 31,5 ; prévaut sur la pose des volutes d'AC-63 (R 27,6, corne ∓ 6°) | balayage `DISABLED_volute_pose_sweep` (A7 S1) ; choix de l'utilisateur à P-A7-1, validé sur les planches à P-A7-2 | œil R cos (θ − 45°) 27,10 (référence 27,1, AC-63), R sin θ 17,48 (17,4) ; raccourci vu de face 0,982 ; portée au profil 30,76 contre 31,2 gravé (−0,44) ; tourner le plan vers la diagonale aggrave le débord (+0,13 p par degré **[M]**) | `DefaultVoluteSpec` |
| AC-82 | Marge de la borne en plan = c (0,05 p) | aucune | tolérance sans appui | `BuildVoluteRibbon` |
| AC-83 | **Écart au ruban gravé** : V borné par l'arête supérieure de A, alors que le ruban gravé de la pl. XXV va 0,6 à 0,9 p plus loin vers la corne (bout à r 30,8 / 31,5, bord extérieur entre A et A + 1 p) | décision Q-A7-0 (a), confirmée à P-A7-1, acceptée à P-A7-2 | V rentré de 0,6 à 0,9 p en plan par rapport à la gravure **[M]a** | — |

**Compléments du lot A7** (les lignes ci-dessus restent, ces compléments prévalent) :

| # | Complément | Appui |
|---|---|---|
| AC-2 | Volutes bornées par le cône **et**, en plan, par l'arête supérieure de A (AC-80) ; débord de l'abaque des données de V : +3,65 → **+2,44 p** ; contre A : **−0,108 / −0,106 p** (Light / Heavy) **[M]** ; hélices inchangées, hors de l'abaque construit (+5,25 / +5,27 p contre A), en dette | décisions Q-A7-0 (a) et Q-A7-2 ; K60, K35 |
| AC-63 | Référence de l'œil des volutes au profil : **R cos (θ − 45°) = 27,1** (relevé calé, A7 S0) au lieu de 27,5 ; R sin θ = 17,4 et h 31,5 inchangés ; la pose des volutes est celle d'AC-81 | relevé calé de la pl. XXV ; décision P-A7-1 |
| AC-78 | K60 porte deux colonnes par rang et par niveau, figées à ±0,05 p : contre l'abaque des données B (`EnvelopeRadius`) et contre l'arête supérieure de A (`abacusOutline`). Light B / A : F +2,99 / +1,87, E +9,34 / +8,30, C −0,91 / −2,10, D +1,68 / +0,63, **V +2,44 / −0,11**, H +6,29 / +5,25, G +1,98 / +3,47 ; Heavy B / A : F +3,03 / +1,88, E +9,40 / +8,36, C −0,91 / −2,10, D +1,69 / +0,64, V +2,44 / −0,11, H +6,31 / +5,27, G +1,99 / +3,47 **[M]** ; contrôles de l'instrument : sommet à 17,7 p sur l'axe de face → +0,1 contre B ; sommet à R_A (40°) + 0,1 → +0,1 contre A ; anneau lu = carré concave (36 ; 18,64) | décision Q-A7-3 ; K60 |

## 11. Acanthe corinthienne (lots A0 à A6)

Plan de l'architecte du 2026-11-21, décisions P1 à P9 de l'utilisateur du même jour. Remplace la génération
des feuilles d'acanthe du chapiteau corinthien (`src/cgarchi/classical_acanthus.{h,cpp}`, appelée par
`BuildClassicalColumnMesh` et par `opus --colonne-corinthienne`). Conventions : registre §10.17. Dette :
`debt_cgarchi.md` §2.2. Statuts de ce document : **[L]** du plan de l'architecte (« lu ») est noté **[V]**
(vérifié par lecture), légende en tête.

**Sources.** Vignole, planches XXV et XXVI (`C:\home\doc\papers\Architecture\Antique\Ordres\planches\vignola-p49.jpg`,
`vignola-p51.jpg`) ; référence visuelle : modèle cgtrader
(`C:\Users\c_lau\Downloads\roman-corinthian-column-3d-model-0686bf1730.jpg`).

### 11.1 Décisions de l'utilisateur (2026-11-21)

- **P1** Phase : celle des sources — E sur l'axe de face (θ = 0 mod 45°), F à 22,5° mod 45°, caulicoles à
  22,5° mod 45°. Appliquée en A2.
- **P2** ~~Feuille `Acute` par défaut (style cgtrader), `Olive` en option (Vignole pl. XXVI)~~ — **remplacée le 2026-11-23** (décision de l'utilisateur, lot A1e) : Olive seul style, Acute retiré — AC-1.
- **P3** D = calice, deux par caulicole, et non couronne — AC-16.
- **P4** Référence Vignole : enveloppe = min (cône de révolution du JSON, contour en plan de l'abaque) sur
  toute la hauteur, lue sur le plan de la pl. XXV (le feuillage reste dans le contour de l'abaque) ;
  interprétation, mesure prévue en A4 — AC-2.
- **P5** L'enveloppe est une borne stricte (pas de débord des calottes comme chez cgtrader) — AC-3.
- **P6** Réglage `capitalOrnaments` {None, Light, Heavy}, défaut Light, colonnade comprise (dépassement de la
  bannière maker de 300 000 faces vers 10 colonnes accepté) ; champ avancé en A0 à None (D1), défaut Light en A2 — AC-17.
- **P7** `volutes = false` coupe aussi l'acanthe — AC-17 ; appliqué en A0.
- **P9** Script de rendu de contrôle : `tools/render_glb.py` (`python -I tools/render_glb.py <glb> <dossier>`,
  écrit `cap_front.png` et `cap_diag.png`).
- **D1** (passe de correction, même jour) : `capitalOrnaments` avancé en A0, défaut None jusqu'au lot A2 ;
  `opus --colonne-corinthienne` règle Light — AC-17, AC-19.
- **D2** Borne = max (noyau, min (cône, abaque)) sur toute la hauteur ; fleuron exclu — AC-2.

### 11.2 Constats de départ

- **[V]** Les cotes de Vignole sont dans `src/cgarchi/data/cinq_ordres.json`, membres d'ornement du chapiteau
  (saillie `CoreEnvelope` : `main` = noyau, `envelope` = enveloppe) : F tige 0–9, F retombée 9–12, E tige 12–21,
  E retombée 21–24, D 24–28, C 28–36, B 36–39, A 39–42 ; enveloppe de 126/43 à 792/43 entre h = 0 et 36,
  pente 37/86. Avant A0, le code ne les lisait pas : constantes 15 + 2,5 + 37/86 recopiées à la main.
- **[M]** `colonne.glb` (`opus --colonne-corinthienne`) : 147 408 triangles = 13 648 (tour, référence de
  `test/tu_cgarchi_classical_column.cpp:149`) + 133 760 (acanthe).
- **[V]** 296 `appendMesh` par colonne, chacun recopiant le maillage destination
  (`src/cgmesh/polygon_extrude.cpp:140`).
- **[V]** Phase inversée (F sur θ = 0) ; D en couronne de 8.

### 11.3 Modèle cible

- **Feuille** : coque fermée unique sur une grille Nv × Nu doublée à ±t/2, plus la couture
  (T = 4·Nv·Nu + 4·(Nv + Nu) triangles) ; contour lobé et digité en (u, v) ; directrice corps → crête tangente
  à l'enveloppe → retombée ; enroulement cylindrique sur la corbeille ; section en gouttière, côtes en Heavy.
- **Caulicoles** : tubes balayés. **Calices D** : la même feuille dans un repère à rotation minimale.
- **Volutes et hélices** : rubans balayés sur spirale (réutilisation de `BuildTwelveCentreVolute` /
  `SampleCurve` de `classical_volute.h` à vérifier). **Fleuron** : rosace sur la face de l'abaque, largeur
  `fleuron_largeur` du JSON.
- **API** (`classical_acanthus.h`) : `OrnamentDetail {None, Light, Heavy}`, `CapitalPieceKind`,
  `AcanthusOptions {detail, segmentsPerLeaf, scale}`, `CapitalPiece {kind, faceBegin, faceEnd,
  vertexBegin, vertexEnd}`, `CapitalOrnament {mesh, pieces}`,
  `CapitalOrnament BuildCorinthianOrnament (const OrderData &, const ColumnProfile &, const AcanthusOptions &)`.
- **En-tête interne** `classical_acanthus_detail.h` (modèle : `classical_greek_entablature_detail.h`) :
  `ReadCapitalEnvelope`, `CoreClearance` = min (r − noyau (h)), `EnvelopeExcess` = max (r − enveloppe (h, θ)),
  `BellCoverage` par rastérisation (θ, h) 360 × 48.
- **Feuille (lot A1e)**, en-têtes internes : `classical_acanthus_template.h` (`LeafStyle {Olive}`,
  `LeafTemplate`, `ParseLeafTemplates`, `LeafTemplateOf`, gabarit embarqué `AcanthusLeavesJson`) et
  `classical_acanthus_detail.h` (`LeafSpec`, `LeafPlacement`, `DefaultLeafPlacement`, porteuse `LeafBend`,
  `LeafSheet`, `BuildLeafSheet (env, spec, placement, tpl)`, `CheckFaceFold`). `LeafStyle` n'est plus dans
  l'API publique ; `AcanthusOptions::style` est retiré.

### 11.4 Lots

| Lot | Contenu | Prédictions et critères |
|---|---|---|
| A0 | Socle et instruments, géométrie inchangée : renommage `classical_canthus` → `classical_acanthus`, `ReadCapitalEnvelope` et détecteurs, sortie en un maillage et intervalles de pièces, un seul `appendMesh`, P7 | journal 11.5 |
| A1 | Générateur de feuille seul | 584 / 10 688 triangles par feuille (Light / Heavy), grilles 20 × 6 / 80 × 32 ; cas négatifs : `closeRim = false`, jeu c = −0,5, directrice dilatée de 10 %, courbure < 2t → `invalid_argument` |
| A1b | Feuille en lame et lobes digités (1 + 2L coques), calotte, cache des directrices, passe de relecture de A1 | Light 760 / Heavy 10 392 triangles par feuille ; invariants I1 à I11 vus rouges sur faute injectée (journal 11.5, écarts 11.8) |
| A1c | Remplissage de la feuille : lobes en paume et doigts, empilement drapé, relief, col et calotte ; passe de relecture de A1b | Light 872 / 1 096 (Olive, D = 3), Heavy 10 296 / 10 104 ; invariants I1 à I18 vus rouges sur faute injectée (journal 11.5, écarts 11.9) |
| A2 | Rangs F et E à la phase P1, retrait du modèle v0, défaut `capitalOrnaments` à Light (champ en place depuis A0, D1), reprise des empreintes corinthiennes | ~~ornement 9 344 / 171 008 ; couverture ≥ 85 %~~ (cibles d'A1, caduques) ; **révisé (Q4, Q5, 2026-11-24)** : ornement 29 696 / 170 496, colonne 43 344 / 184 144 ; couverture de la corbeille h ∈ [1, 23] par F et E dans [0,60 ; 0,80], le seuil de 85 % reporté à A3 (mesuré avec C et D) ; ornement valide, 16 composantes ; colonne valide, 17 ; fautes 40 à 53 rouges. **Fait le 2026-11-24** (§11.11) |
| A3 | Caulicoles et calices D | ~~+4 064 / +29 184 ; couverture ≥ 85 %~~ (cibles v0, caduques) ; **révisé (S0b, Q5, Q9′, 2026-11-25)** : ornement 56 256 / 243 008 ; colonne 69 904 / 256 656 ; couverture angulaire ≥ 0,78 ; couverture en élévation contre V-1 en journal (rouge connu) ; ornement valide, 40 composantes ; colonne valide, 41 ; fautes 70 à 104 rouges ; durée de la colonne ≤ 15 / 40 ms. **Fait le 2026-11-25** (§11.13) |
| A4 | Volutes et hélices | ~~≈ +4 000 / ≈ +46 000 (± 30 %) ; mesure de P4~~ ; **révisé (plan A4, Q1 à Q8, 2026-11-25)** : relevé S0, optimisation S1′ à sorties identiques, contour de l'abaque des données (Q4 (a)), 8 volutes puis 8 hélices ; ornement 70 576 / 299 888 ; colonne 84 224 / 313 536 ; ornement valide, 72 composantes ; colonne valide, 73 ; fautes 110 à 142 et 210 à 215 rouges (sauf 4126) ; durée de la colonne ≤ 15 / 40 ms (9,2 / 20,5). **Fait le 2026-11-26** (§11.14) |
| A5 | Fleuron | ~~≈ 800 / ≈ 12 000~~ ; **révisé (plan A5, Q-G0 à Q-G8, 2026-11-26 et 27)** : relevé S0 (fleuron ≈ 8 p, pas 4), rosace en coque unique (cuillers, tambour, noyau), tige en demi-jonc, dos sur la corde réelle du tour ; ornement 79 232 / 329 824 ; colonne 92 880 / 343 472 ; ornement valide, 80 composantes ; colonne valide, 81 ; fautes 5xxx rouges (55 sur 55 après S7) ; durée de la colonne ≤ 15 / 40 ms (12,3 à 12,6 / 26,9 à 27,2 après S7). **Fait le 2026-11-27** (§11.15) |
| A6 | Mesures par niveau, WASM (≈ 50 / ≈ 330 ms **[E]**), colonnade | ~~colonne complète ≈ 31 600 (Light) / ≈ 272 000 (Heavy)~~ : la cible Light est dépassée dès A2 (43 344, F et E seuls) ; **révisé (plan A6, Q-A6-0 à Q-A6-6, 2026-11-27)** : instrument rang × niveau et K60 (P4 mesurée, réfutée) ; colonnade corinthienne (K61, bannière par niveau, AC-75), assemblage linéaire à sortie identique (K62) ; module WASM : comptes = natif, colonne ≤ 50 / 150 ms sous Node (AC-76) ; dos de la rosace sur un tour asymétrique (K63) ; fautes 6xxx rouges ; GLB de la colonne inchangés. **Exécuté le 2026-11-27** (§11.16), validation des documents (P-A6-4) et relecture S8 en attente |
| A7 | Volutes sous les cornes de l'abaque (dette d'A6, P-A6-1) | **plan A7, Q-A7-0 à Q-A7-4, 2026-11-28** : V contre l'arête supérieure de A ≤ −c (garde du produit et K60), garde du cône, jeux ≥ c, œil à 0,5 p des relevés, raccourci ≥ 0,90 ; F, E, C, D, H, G identiques à l'octet ; comptes inchangés ; fautes 7101 à 7108 rouges ; module WASM reconstruit ; bilan 11.17 |

**Risques.** R1 bord de feuille fonction de v (pas de digitation enroulée) ; R2 grille uniforme en Heavy
(≈ 40 % des triangles au centre) ; R3 GCC ci-linux contracte en FMA : aucune comparaison au bit près entre
unités de traduction ; R4 seuils estimés.

### 11.5 Journal de prédictions

Les lignes A1 à A1c ci-dessous citent du code, des tests et un style (Acute) retirés au lot A1e (S5 et décision
de l'utilisateur du 2026-11-23) ; elles restent comme journal. Le bilan d'A1e est au §11.10.

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| A0 | `opus --colonne-corinthienne` : 147 408 triangles et 106 906 sommets inchangés | `opus.exe` reconstruit avant (13:11, à jour) et après (17:17) ; `cmp` et `sha256sum` des GLB | **[M]** 147 408 faces, 74 298 sommets du modèle, 106 906 sommets exportés ; GLB identiques à l'octet (sha256 `6a852fdc…`), aussi identiques à `colonne.glb` de la racine | aucun | ordre des sommets et des faces conservé par l'assemblage unique ; le cône lu diffère de l'ancienne formule de moins d'un ulp `float` aux points utilisés |
| A0 | `ConeRadius` = 15 + 2,5 + (37/86)(y − origine + 1) à 1e-12 sur 100 valeurs | test `the_envelope_is_read_from_the_capital_members` | **[M]** écart maximal 7,1e-15 p sur h ∈ [0, 36] ; même formule **[V]** : 126/43 = 2,5 + 37/86 et (792 − 126)/43/36 = 37/86 | aucun | — |
| A0 | Détecteurs vus rouges sur le modèle v0 : `CoreClearance` des nervures F et E ≈ −0,45 p, D ≈ −0,2 p **[E]** (architecte) | test `the_current_leaves_are_measured_against_the_envelope` (pièce 0 de chaque feuille = nervure) | **[M]** F −0,45, E −0,45, D −0,2 (vus rouges) ; folioles jamais plus bas ; tiges des caulicoles +10,59 p (détachées de la corbeille) | aucun | nervure de rayon 0,075 h centrée à 0,025 h du nu : 0,05 h sous le nu (h = 9 et 4) |
| A0 | — (pas de prédiction) | même test, `EnvelopeExcess` contre la borne P4 puis contre le cône seul | **[M]** F 4,71 / E 7,85 / D 12,60 / C 14,31 p ; cône seul 0,51 / 0,51 / 0,23 / 1,50 p (vus rouges) | — | la borne de l'abaque domine (17,6 p sur la face) |
| A0 | — (constat) | test `the_envelope_is_the_abacus_raised_to_the_bell` | **[M]** abaque B : 17,6 p (face), 19,62 p (22,5°), 32 p (corne) ; la borne P4 passe sous le noyau dès h = 30,85 (face) et 34,45 (22,5°), jamais aux cornes | — | lèvre de la corbeille (noyau des caulicoles, 20,5 p à h = 36, mesurée au JSON) plus large que la face concave de l'abaque. À faire trancher (AC-2, dette §2.2) |
| A0 | `BellCoverage` du modèle v0, h ∈ [1, 23], entre 40 et 70 % **[E]** | même test, rangs F et E | **[M]** 72,5 % (F + E) ; 72,7 % toutes pièces | +2,5 points au-dessus de la borne haute | fourchette estimée sans calcul ; folioles ouvertes en éventail (composante tangentielle 0,82) **[H]** |
| A0 | Instruments vus verts et rouges sur des surfaces connues | test `the_instruments_read_known_surfaces` | **[M]** `CoreClearance` +0,25 / −0,5 ; `EnvelopeExcess` −0,1 et cône seul = cône (36) − abaque (0) ; `BellCoverage` 1,0 (bande pleine), 0,5 (demi-tour), 0,5 (demi-hauteur), 0 (aucune pièce) | aucun | — |
| A0 | Temps de `BuildClassicalColumnMesh` corinthien en baisse ≥ 30 % **[H]** | test `DISABLED_corinthian_column_build_time` (Release MSVC, 21 mesures, médiane, 3 lancements) | **[M]** avant (TU 16:51) 248,7 / 252,7 / 253,9 ms ; après (TU 17:18) 10,3 / 9,9 / 10,1 ms, **−96 %** ; `Regenerate` du corinthien complet (piédestal, entablement, une mesure) 311,5 → 14,5 ms | baisse 3 fois plus forte que le seuil | 296 `appendMesh` recopiant chacun le maillage destination : coût quadratique, devenu une recopie unique |
| A0 | `reference_columns_keep_their_counts_and_volume` échoue avec les sources d'avant A0 **[H]** ; repasse après P7 | TU reconstruit avant toute modification (16:51, sources du matin plus le seul test de durée), filtre `*classical*`, lancé depuis une copie de `test/data` et `src/cgarchi/schemas` | **[M]** avant : rouge (corinthien 147 408 faces au lieu de 13 648, 74 298 sommets au lieu de 6 826, 8 745 composantes, non valide) ; **292 verts, 3 rouges**. Après : vert ; **300 verts, 2 rouges** (sept tests ajoutés) | les deux autres rouges restent | `the_classical_column_keeps_its_fingerprint_in_all_48_configurations` (8 configurations corinthiennes, « comptes ») et `the_parameterized_column_follows_its_parameters` (corinthien complet non valide : 17 952 faces dégénérées, 25 796 auto-intersections) construisent avec « Volutes » coché, défaut : rouges avant et après A0, à l'identique ; ils attendent le modèle valide (A2) |
| A0 | — (révélé) | test `TEST_cgarchi_classical_ceiling.*` et `opus --monument hephaistos` | **[M]** avec le nom `OrnamentPiece` du plan : corruption du tas (`0xC0000374`) à la construction de l'Héphaïsteion, le test plante (segfault, boîte d'erreur qui bloque la suite) ; après renommage en `CapitalPiece` : Héphaïsteion construit (1 639 140 faces), suite verte | — | violation de l'ODR : `cgarchi::classical::OrnamentPiece` existe déjà (`classical_greek_entablature.h:435`), les instanciations de `std::vector<OrnamentPiece>` des deux unités de traduction ont été fusionnées à l'édition des liens. Ni MSVC ni le lieur ne le signalent |
| A0 | Rendu identique à l'état d'avant | `python -I tools/render_glb.py` sur les GLB avant et après, `cmp` des PNG | **[M]** `cap_front.png` et `cap_diag.png` identiques à l'octet | aucun | GLB identiques |
| A0 (revue) | D1 : avec `capitalOrnaments` = None par défaut, tous les tests `*classical*` verts | TU 18:00:43, filtre `*classical*:*cggraph_nodes_shapes*`, depuis `scratchpad\tu_run` (copies rafraîchies de `test/data` et `src/cgarchi/schemas`) | **[M]** **328 verts, 0 rouge sous ce filtre** (`*classical*:*cggraph_nodes_shapes*`), dont 303 du filtre `*classical*` ; **hors filtre, `TEST_cggraph_parameter_id.the_catalogue_document_is_written_byte_for_byte_as_before` était rouge** (deux blocs « Capital ornaments » de plus dans le document du catalogue), vu par la seconde revue et corrigé ensuite (ligne « A0 (revue 2) ») ; les deux rouges d'avant (`the_classical_column_keeps_its_fingerprint_in_all_48_configurations`, `the_parameterized_column_follows_its_parameters`) repassent, corinthien complet 29 104 triangles | aucun | à HEAD l'acanthe n'existe pas : rouges de HEAD non mesurés (pas de construction de HEAD), sans objet puisque aucun rouge ne reste |
| A0 (revue) | GLB inchangé avec Light réglé par opus | `opus.exe` 18:00:49, sha256 | **[M]** `6a852fdc…` identique ; PNG de `render_glb.py` identiques à l'octet | aucun | — |
| A0 (revue) | D2 : `EnvelopeExcess` du modèle v0 contre max (noyau, min (cône, abaque)) | test `the_current_leaves_are_measured_against_the_envelope` | **[M]** F 4,71 / E 7,85 / D 12,60 / C **14,24** p (14,31 contre l'ancienne borne) ; cône seul inchangé 0,51 / 0,51 / 0,23 / 1,50 | — | seules les tiges C montent au-dessus de h = 34,45 hors des cornes, où le noyau relève la borne |
| A0 (revue) | D2 vu rouge et vert sur l'axe de face au-dessus de h = 30,85 | test `the_envelope_is_the_abacus_raised_to_the_bell` (bande θ ∈ [−0,04 ; 0,04], h 31 à 36) | **[M]** surface à noyau + 0,1 : excès +0,1 (rouge) ; surface au noyau : \|excès\| ≤ 1e-5 (vert) ; borne 20,5 à h = 36 sur la face, 32 aux cornes, 17,6 sur la face à h = 9 ; fleuron −∞ | aucun | — |
| A0 (revue) | M1 : l'échelle est exercée | test `the_instruments_read_known_surfaces` (bande à 0,01 unité par partie, ornement à l'échelle 2) ; faute injectée dans le produit : `/ env.scale` remplacé par `* env.scale` dans `VertexAt`, TU reconstruit (18:00:25), puis retirée | **[M]** sain : +0,25 et −0,45 ; faute : **rouges** −14,998 (au lieu de +0,25) et −478,675 (au lieu de −0,45), seul ce test rougit, les tests à l'échelle 1 restent verts | aucun | avant la revue, aucune mesure n'était faite hors de l'échelle 1 : la faute serait passée |
| A0 (revue) | M2 : chaque garde de `ReadCapitalEnvelope` vue | test `the_capital_envelope_rejects_what_it_cannot_read`, fragment du message vérifié | **[M]** vues : scale, ordre sans membre d'ornement, membre absent, membre en double, hauteur nulle, membres non contigus, abaque absent, anneau d'abaque variable, abaque sans sommet ; `BuildCorinthianOrnament` : profil refusé à None, `segmentsPerLeaf` 1025 refusé (1024 accepté à None), `detail` hors énumération, scale 0, ordre ionique | aucun | — |
| A0 (revue) | L1 : couture θ = π | même test, bande sur [3π/4, 5π/4] | **[M]** `BellCoverage` = 0,25 exactement | aucun | — |
| A0 (revue) | L7 : `render_glb.py` refuse explicitement ce qu'il ne lit pas | script `render_negatives.py` (scratchpad) : GLB de opus dont le bloc JSON est modifié | **[M]** 6 cas vus (byteStride entrelacé, indices uint8, primitive sans indices, mode 1, accesseur sparse, POSITION absent), chacun en `GlbUnsupported` nommant la clé ; positif : GLB de opus lu (106 906 sommets, 147 408 triangles) | — | les transformations des nœuds (rotation et échelle de opus) sont signalées sur stderr, pas refusées : les refuser rendrait le script inutilisable sur les GLB de opus |
| A0 (revue) | Durée, Light | `DISABLED_corinthian_column_build_time` (une série de 21) | **[M]** médiane 11,7 ms (10,0 avant la passe) | +1,7 ms | une seule série ; validation du profil à chaque appel ajoutée **[H]** |
| A0 (revue 2) | H1 : test à l'octet du catalogue vert après retrait de « Capital ornaments », et toujours capable d'échouer | `the_catalogue_document_is_written_byte_for_byte_as_before` ; faute injectée dans le produit : défaut de « Volutes » passé à false (`parameterized_classical.h`), TU 19:03:14, puis retirée | **[M]** sain : vert, 2 objets retirés ; faute : **rouge** (comparaison à l'octet, l. 257) | aucun | — |
| A0 (revue 2) | M1 : recadrage de `capitalOrnaments` dans Regenerate (5 → 2, −1 → 0), colonne et colonnade | `the_settings_are_clamped_to_the_bounds_of_their_parameters` ; faute injectée : ligne de recadrage supprimée (`parameterized_classical.cpp:31`), même TU, puis rétablie | **[M]** sain : vert ; faute : **rouge** (5 au lieu de 2, −1 au lieu de 0, et `LastError` non vide, colonne et colonnade) | aucun | — |
| A0 (revue 2) | L1, L2 : `render_glb.py` en plongée, indices contrôlés | `render_negatives.py` (scratchpad), rendu de `colonne.glb` | **[M]** vue diagonale à +15° : le dessus de l'abaque est vu ; 8 cas refusés en `GlbUnsupported` (dont 442 223 indices non multiples de 3, indice 106 905 hors des 100 sommets) ; vue de face identique à l'octet | — | la vue diagonale à −15° était une contre-plongée |
| A0 (revue 2) | TU complet, sans filtre | `TU.exe` 19:03:39, depuis `scratchpad\tu_run`, 291 s | **[M]** 3 023 tests lancés : **3 011 verts, 0 rouge**, 12 sautés (`ImportScad`, OpenSCAD absent), 14 désactivés | — | — |
| A1 | Comptes exacts : 584 (Light 20 × 6) et 10 688 (Heavy 80 × 32) triangles par feuille ; grilles inchangées | test `a_leaf_is_one_closed_shell_on_its_grid` (F et E, Acute et Olive) | **[M]** 584 et 10 688 ; sommets 294 et 5 346 | aucun | — |
| A1 | Feuille seule valide, une composante, test d'auto-intersection non tronqué | même test, `ValidateMesh` par défaut | **[M]** 8 feuilles valides, 1 composante, aucun défaut, non tronqué (après correction, ci-dessous) | 1re version : 26 à 41 auto-intersections | cause [M] par les enregistrements : normales par différences sur la grille, que les œillets et le cisaillement tordent ; cisaillement 0,5 qui retourne la grille ; à la pointe, côtes et gouttière plus profondes que la feuille n'est large. Corrigé : surface porteuse S (xi, v) indépendante du contour, garde du cisaillement, relief proportionnel à l'enveloppe |
| A1 | CoreClearance ≥ −τ ; EnvelopeExcess ≤ τ ; contact de crête ∈ [−0,25 ; 0] ; pointe dans la bande de retombée | même test (τ = 1e-4 p) | **[M]** CoreClearance 0,05 p (= c) partout ; contact F −0,117 (Light) / −0,133 (Heavy), E −0,017 / −0,039 à −0,045 p ; pointe F à h = 9,09, E à 22,45 | aucun | 1re version : contact −0,94 (F) : place prise au plus étroit de toute la plage angulaire et gouttière réduisant la crête ; corrigé (place par méridien, gouttière nulle au bord) |
| A1 | Cas négatifs vus rouges | test `each_defect_is_seen_by_its_instrument` | **[M]** closeRim = false : 104 arêtes de bord (Light) ; c = −0,5 : CoreClearance −0,5 ; directrice × 1,1 : EnvelopeExcess +0,404 p ; t = 1 : `invalid_argument` « rayon de courbure » ; cisaillement 0,5, detail None, hTop = hFoot : refusés | aucun | — |
| A1 | Planche (F, E × Light, Heavy × Acute, Olive, devant un tronçon du noyau) | test `DISABLED_leaf_sheet`, `render_glb.py --full --side`, rendus par case (`crop.py`, scratchpad) | **[M]** GLB de 81 952 triangles ; crosse et retombée visibles de profil ; F à 22,5° s'avance de 4,3 p, E à l'axe de face de 2,3 p seulement (abaque à 17,6) | voir écarts A1 | — |
| A1 | TU complet vert ; GLB de opus inchangé | `TU.exe` 19:25:57 sans filtre, depuis `scratchpad\tu_run`, 285 s ; `opus.exe` 19:26:04, sha256 | **[M]** 3 025 tests lancés : **3 013 verts, 0 rouge**, 12 sautés (`ImportScad`), 15 désactivés ; GLB `6a852fdc…` identique | aucun | le générateur n'est pas branché |
| A1b S0 | Base : tests `*acanthus*` verts ; durée F Heavy Acute > 5 ms (S1 obligatoire) **[H]** | TU 23:30, filtre `*acanthus*` avec les désactivés ; `DISABLED_leaf_build_time` (21 mesures, médiane) | **[M]** 13 tests verts ; **médiane 47,3 ms**, min 44,6 ms (10 688 faces) | — | S1 obligatoire |
| A1b S0 | Relecture A1, point 4 : avec `belly = 0`, la feuille sort de la borne | sonde temporaire (`EnvelopeExcess`, 8 réglages F et E), TU 23:30 puis 23:31 avec cisaillement nul | **[M]** F Light Acute **+0,0445 p**, F Light Olive +0,0269 p, autres ≤ −0,037 ; cisaillement nul : tous ≤ −0,053 p | — | le relief était lu au rang cisaillé v', la largeur au rang v : ue = xi / enveloppe (v') dépasse 1 où l'enveloppe décroît, le facteur de gouttière passe au-dessus de 1 |
| A1b S8 | TU complet vert ; GLB de opus inchangé | `TU.exe` (build 00:13) sans filtre, depuis `scratchpad\tu_run`, 290 s ; `opus.exe` reconstruit 00:14, `--colonne-corinthienne`, sha256 | **[M]** 3 032 tests lancés : **3 020 verts, 0 rouge**, 12 sautés (`ImportScad`), 16 désactivés ; GLB `6a852fdc…` identique | aucun | le générateur n'est pas branché (lot A2) |
| A1b (prédiction) | Comptes : Light 760 triangles et 390 sommets, Heavy 10 392 et 5 210 ; lame 200 / 4 416, lobe 140 / 996 ; arêtes de bord sans couture 248 / 1 112 | `a_leaf_is_one_closed_shell_per_piece`, `the_open_shells_have_their_boundary_edges` (littéraux) | **[M]** 760 / 390 et 10 392 / 5 210 ; lame 200 / 102 et 4 416 / 2 210 ; lobe 140 / 72 et 996 / 500 ; bords 248 / 1 112 ; 1 + 2L composantes (5 / 7), chaque pièce valide, volume > 0 | aucun | — |
| A1b (prédiction) | Contact de crête à ±0,02 p de A1 (F ≈ −0,12 / −0,13, E ≈ −0,02 / −0,04) ; CoreClearance ≥ −τ | même test (`EnvelopeExcess` sur la plage de la lame) | **[M]** F −0,105 (Light) / −0,066 (Heavy) ; E −0,088 / −0,062 ; EnvelopeExcess de la feuille ≤ τ ; CoreClearance 0,059 à 0,080 p | F Light dans la fourchette ; F Heavy +0,067, E Light −0,071, E Heavy −0,022 | **[H]** la crête est portée par la calotte (xi jusqu'à ±1,1, gouttière effacée, retombée des bords) et non plus par toute la feuille ; seuils fixés par rang : F ≥ −0,15, E ≥ −0,12 (AC-29) |
| A1b (prédiction) | Angle de pointe : Acute Light ≈ 38°, Olive Light ≈ 140° **[E]** ; seuils < 60° et ≥ 120° | `the_tip_is_acute_or_rounded` (centre du dernier rang, bords du rang au quart de la crosse avant la pointe, en 3D) | **[M]** exposant 3 : Acute F 52,6°, **E 73,4° (rouge)** ; exposant 4 retenu : Acute F 33,8°, E 49,0° ; Olive F 130,6°, E 171,6° (Light et Heavy à 0,05° près) | E Acute +35° avec l'exposant 3 | crosse de E courte (rayon 0,92 p contre 1,7 p en F) : à fraction égale de la crosse, la pointe est plus trapue ; exposant 4 au registre (AC-27) |
| A1b (prédiction) | Digitations obliques : direction racine → pointe entre 10° et 110° de la verticale ; dispersion par lobe ≥ 0,5 Φ | `the_fingers_fan_out_obliquely` (en (w, h) sur le maillage) | **[M]** directions de 26,7° à 105,0° ; dispersion minimale 25,1° (F Light Olive, Φ = 48°, soit 0,52 Φ) à 36,2° | — | Olive Light (D = 2) est à 1,1° du seuil |
| A1b (prédiction) | Calotte : max \|w\| de la lame sur la crosse ≥ 1,0 hw | `the_blade_widens_into_a_cap` | **[M]** 1,085 hw (Light), 1,100 hw (Heavy) ; corps 0,30 hw | aucun | Light : le rang de crête n'est pas un rang de la grille |
| A1b S1 | Directrices en cache : écart ≤ 1e-3 p, durée F Heavy ≤ 4,7 ms | vidage des sommets avant et après (`cmp_mid.py`, scratchpad), surface de A1 ; `DISABLED_leaf_build_time` | **[M]** cache 2·64 + 1 : surface moyenne 2,7e-4 p ; faces 2,5e-3 p (normales) ; **47,3 → 4,70 ms (×10,1)** ; 4·64 : 2,1e-3 p aux faces ; 16·64 : 1,1e-3 p aux faces, 9,05 ms | écart aux faces 2,5 fois le seuil | l'écart des faces vient des normales par différences (pente de S en xi par morceaux, coudée aux échantillons de place) et décroît en O(Δxi) seulement ; la surface moyenne est dans le seuil. Seuil appliqué à la surface moyenne |
| A1b S8 | Durée F Heavy Acute après A1b | `DISABLED_leaf_build_time`, 3 séries de 21 | **[M]** médianes 22,30 / 22,37 / 22,32 ms (10 392 faces) | 4,7× la durée S1 | gardes de A1b (courbure sur 129 × 257 échantillons, contours, disjonction) et 5 évaluations de S par sommet **[H]**, non profilé ; réserve R-perf |
| A1b | Plan : courbure φ' = φ − σβρ² sans retournement | prototype 2D (`layout2d.py`, scratchpad, mêmes formules que le produit) | **[M]** F Light Acute : 12 triangles retournés au lobe 0 (Φ = 70°, β = 0,25) | refusé | la rotation dépend du rang ρ, pas du rayon : sur un rang, pointe et creux sont à des rayons différents et tournent d'angles différents. Retenu : φ' = φ − β sin φ q², q = (r − r_in) / (R − r_in), avec β < 1 (AC-25) |
| A1b | Plan : portée par 3 itérations de point fixe | même prototype | **[M]** lobe haut F Light Acute : portée 0,60 p (oscillation vers la pointe du gabarit) | refusé | remplacé par une recherche par pas puis bissection du premier contact, gabarit lu sur le corps (Y / Y_c) (AC-24) |
| A1b | Plan : Φ = 70° / 80° autour d'axes de 65° à 35° | même prototype | **[M]** chevauchement des lobes d'un même côté (distance 0) dans les 8 réglages ; axe haut 35° : φ' < 0 | refusé | deux éventails dont les bords convergent se croisent ; retenu : secteurs qui se recouvrent de ±10° (Light Φ 48°, axes 81° → 54° ; Heavy Φ 38°, 86° → 49°), bord haut ≥ 30° (AC-24) |
| A1b | Garde de courbure | `BuildLeafSheet`, sonde temporaire sur les 8 réglages | **[M]** (1) rééchantillonnage en v : rayons 0,35 p sur un arc de 0,92 p (coudes des cordes) ; (2) gouttière `max(0, 1 − ue²)` : rayon 0,385 p à xi ≈ ±0,5 (coude en ue = 1 ; sans gouttière, 8 réglages verts) ; (3) pointe ronde Olive : 0,376 p à xi ≈ −0,1 (ue bascule quand le gabarit tend vers 0) | — | corrigés : courbure lue aux échantillons de chaque directrice ; gouttière (1 − ue²)², C¹ ; gouttière et côtes effacées sur la crosse (AC-9). Marge : à t = 0,22 les 8 réglages passent, à 0,25 F Olive échoue (calotte) |
| A1b | Coques sans auto-intersection | `a_leaf_is_one_closed_shell_per_piece` | **[M]** (1) Light : 14 auto-intersections lame / lobe haut ; (2) Heavy : 4 à 48 aux flancs des dents, indépendantes de t (0,1 donne autant) et du roll | — | (1) la lame passait de la couche pleine à la rampe entre deux rangs distants de 1,3 p : la rampe part maintenant du premier rang de lame au-dessus de fullTop ; (2) cellules en aiguille : sur le flanc raide d'une dent (0,35 de la digitation, 1,1° entre colonnes en Heavy), arête de rang et arête de colonne sont toutes deux radiales. κ × 0,5 : 0 ; × 0,7 : 4 (Olive) ; retenu × 0,6 en Heavy (Acute 0,30, Olive 0,21) et profil Olive en sin (πu) au lieu de sin^½ (AC-25) ; diagonale choisie par cellule (sans ce choix, faute 6 : auto-intersections) |
| A1b | Relecture A1 : relief borné, `belly = 0` | `the_relief_stays_under_the_bound` (8 réglages, belly 0 et 0,55) | **[M]** EnvelopeExcess ≤ τ partout | — | avec `belly = 0` le gabarit est plein dès le pied : une paire de lobes étroits (Φ 20°) pour que la disposition reste valide |
| A1b fautes | I1 à I11 vus rouges sur une faute injectée dans le produit (commutée par une variable d'environnement temporaire, un seul build, posée et retirée par `faults.py` avec détecteur), puis retirée : suite verte, aucun `FAULTINJ` restant | tests de la feuille, TU 00:12 | **[M]** I1a λ = 0 : 319 à 340 auto-intersections ; I1b couture du côté C retirée : comptes 978 / 134 et arêtes de bord ; I2 un rang de lobe de plus : 1 096 / 550 au lieu de 996 / 500 ; I3 couche pleine sur la crosse : contact −0,28 à −0,36 p (seuils −0,15 / −0,12) et Acute 62,7° / 63,0° ; I4 roll de signe inversé : CoreClearance −0,12 à −0,23 p ; I5 racine du lobe à l'origine : faces dégénérées et doublons ; I6 garde de retournement désactivée : β = 0,5 non refusé (« retourne » absent) et feuilles par défaut avec auto-intersections (diagonale fixe) ; I7 garde de chevauchement désactivée : lobes à la même origine acceptés, 330 auto-intersections ; I8 occupation sur la lame et \|xi\| ≤ 1 : E Light Acute t = 0,3 accepté (la garde lève sinon à xi = −1,048) ; I9 éventail resserré à 0,4 Φ : dispersion 15,3 à 16,8° (seuils 19° / 24°) ; I10 calotte ignorée : 0,894 hw ; I11 profil Olive remplacé par Acute : Olive 33,8° à 49,1° | les bornes de direction 10° et 110° de I9 n'ont été vues que vertes | 1re pose : les fautes I2 (colonne de plus) et I9 (axe à 90°, Φ ≈ 0) rougissaient par une garde (grille retournée), pas par l'instrument : remplacées |
| A1c S0 | Base A1b : tests `*acanthus*` verts ; chronomètres par phase de F Heavy Acute ventilant ≥ 80 % des 22,3 ms | TU 09:10, filtre `*acanthus*` ; sonde temporaire `PHASEPROBE` (chronomètre entre les phases de `BuildLeafParts`, posée et retirée par `phaseprobe.py`, fichier rendu identique), `DISABLED_leaf_build_time`, 3 séries de 21, médianes | **[M]** 16 verts ; médianes 22,53 / 22,48 / 22,39 ms ; porteuse 0,58, disposition 0,09, `SplitCells` et domaine 0,73, `CoverLobes` 0,09, `CheckDisjoint` 3,61, `CheckCurvature` 15,55, `LiftPiece` 1,28, assemblage 0,45 ms : somme 22,4 ms (100 %) | — | la courbure fait 69 % ; part des messages construits à chaque échantillon (relecture M2) à mesurer en S2 |
| A1c (prédiction) | Comptes par feuille : Light Acute 872 / 446, **Light Olive (D = 3, m = 2) 1 096 / 558**, Heavy Acute 10 296 / 5 162, Heavy Olive 10 104 / 5 066 ; lobe 168 / 86, 224 / 114, 980 / 492, 948 / 476 ; lame 200 / 102 et 4 416 / 2 210 inchangée ; arêtes de bord sans couture 296 (Light Acute), 1 304 (Heavy Acute) | littéraux de `a_leaf_is_one_closed_shell_per_piece` et `the_open_shells_have_their_boundary_edges` ; formules T = 2 T2 + 2 B, V = 2 V2 | **[M]** exacts dans les 8 réglages (TU 10:24) ; 1 + 2L composantes, chaque pièce valide, volume > 0 | aucun | — |
| A1c (prédiction) | Angle 2D minimal des triangles de lobe ≥ 2,5° sur les 8 réglages (prototype : 2,53° à 8,28°, Olive Light D = 3 : 4,04° et 4,27°) ; I16 (angle 3D des triangles extérieurs) ≥ 2° | garde d'angle minimal ; `the_lobe_triangles_are_not_needles` | **[M]** garde 2D à 1° passée ; I16 de 2,90° (F Heavy Olive) à 7,69° (F Light Acute) | 2D non mesuré isolément (garde à 1°) | les triangles extérieurs 3D restent au-dessus de 2,9° |
| A1c (prédiction) | Remplissage I13 de 77 à 93 % (A1b : 35 à 50 %) ; col I12 de 0,54 à 0,65 hw (A1b : 0,20) ; I12 et I13 rouges sur A1b | `the_leaf_fills_its_contour`, `the_neck_is_wide` (trame (w, h) de 0,05 p) | **[M]** sur A1b (S1) : col 0,187 à 0,190 hw, remplissage 34,9 à 50,6 % (rouges) ; A1c : remplissage 75,0 % (F Light Acute) à 87,6 % (E Heavy Olive), col 0,479 à 0,621 hw | Light Acute −2 points sous la fourchette (75,0 et 75,4 %) ; col Light 0,48 à 0,53 sous 0,54 | lobe du haut plafonné sous Y_c (écart 11.9 n° 4) et col évasé dès 0,5 Y_c (n° 5) au lieu de lobes jusqu'à Y_c |
| A1c (prédiction) | Profondeur d'empilement ≤ 3, ≤ 2 au-dessus de 0,85 Y_c ; relief du rang 0 ≥ 0,8 p ; ε_s = 0,04 sans auto-intersection | `the_lobes_stand_out` (I14), `the_layers_are_ordered` (I15), `ValidateMesh` | **[M]** profondeur 3 ; 2 au-dessus de 0,85 Y_c (1 en E Light) ; ε_s = 0,04 : 0 auto-intersection dans les 8 réglages ; relief du rang 0 (mesure différentielle) **0,25 à 0,81 p** ; ordre : écart minimal 0,038 à 0,048 p | relief : prédiction réfutée sauf Light Acute (0,81) | le rang 0 est en grande partie sous le rang 1, où son relief est effacé (δ (1 − C)) ; seuil I14 ramené à 0,2 p (écart 11.9 n° 9) |
| A1c (prédiction) | Contact de crête dans les seuils A1b (F ≥ −0,15 p, E ≥ −0,12 p) ; CoreClearance ≥ −τ, EnvelopeExcess ≤ τ | `a_leaf_is_one_closed_shell_per_piece` | **[M]** contact F −0,105 (Light) / −0,066 (Heavy), E −0,088 / −0,062 ; CoreClearance 0,050 à 0,052 p ; EnvelopeExcess ≤ τ | aucun | la lame n'est plus relevée sur la crosse (lobe du haut plafonné) |
| A1c (prédiction) | Durée F Heavy Acute : A1b ≤ 15 ms après S2 ; A1c ≤ 20 ms (critère dur ≤ 22,3 ms) | `DISABLED_leaf_build_time`, médiane de 21 | **[M]** A1b après S2 : **6,94 / 6,88 ms** ; A1c : **35,82 / 35,68 / 36,65 ms** (3 séries) | A1c +79 % sur la prédiction, critère dur **non tenu** (+60 %) | empilement 25,4 ms (drapé : distance étoile-contour et maximum des triangles voisins), courbure 4,8, domaine et côtés opposés 1,8, disposition 1,3, repères 1,2, porteuse 0,6 ms (sonde `PHASEPROBE`). 1re version 115 ms ; ramenée sans changer la sortie (GLB identique à l'octet) par tri des triangles par pic, grille de voisinage, `sqrt` au lieu de `std::hypot`, arêtes de bord de l'étoile seules |
| A1c étape 1 | Relecture A1b : M4, M5, L1 à L6 corrigés ; tests `*acanthus*` verts | TU 09:16 à 09:19 | **[M]** 19 verts (dont 3 tests nouveaux : `the_envelope_is_guarded`, `every_nan_is_refused_by_its_guard`, `the_layout_guards_are_seen`) ; L6 : 16 sommets de bord de calotte déplacés, écart maximal 7,4e-3 p (`glbdiff.py`, planche avant et après) | 1re entrée de la garde de rampe (from / to) : levait d'abord la portée ou le retournement | entrée retenue : `layerGap` 5 p |
| A1c S2 | M2 : messages des gardes construits seulement en cas d'échec ; écart ≤ 1e-3 p, médiane ≤ 15 ms | planche avant et après (`glbdiff.py`) ; `DISABLED_leaf_build_time` ; sonde `PHASEPROBE` | **[M]** géométrie identique à l'octet ; 21,2 → 6,94 ms ; courbure 15,55 → 1,85 ms, disjonction 3,61 → 2,51 ms | les autres caches du plan (repère, index de `Directrix::At`, relief par méridien) non faits | critère atteint par M2 seul : la relecture estimait 10 à 20 ms de messages, mesuré 14,3 ms |
| A1c S3 | `PlanarPiece` en maillage plan, `MeshShell` générique (faces, couture, orientation) ; comptes A1b identiques | tests de la feuille ; `glbdiff.py` | **[M]** comptes identiques, positions identiques à l'octet, seul l'ordre des faces change | aucun | — |
| A1c S4 à S6 | Disposition, empilement, lame (plan §1) | tests de la feuille, sonde `DISABLED_probe_cases` (temporaire) | **[M]** 8 réglages valides après les écarts 11.9 n° 1 à 8 | voir §11.9 | chaque écart mesuré avant d'être retenu (rayons, décalages, angles au §11.9) |
| A1c fautes | I1 à I18 vus rouges sur une faute injectée dans le produit (variable `CG_LEAF_FAULT`, une seule construction, posées et retirées par `faults.py` avec détecteur), puis retirées : suite verte, aucun `FAULTINJ` restant | tests de la feuille, TU 10:12 à 10:22 | **[M]** faute 0 : 18 verts. I1 drapé nul : 8 réglages en auto-intersection, ordre −0,23 à −0,28 p ; I2 rang de doigt de plus : comptes Light 216 / 110 au lieu de 168 / 86 (Heavy : garde de pli) ; I3 lame au décalage maximal partout : contact −0,74 à −1,05 p en Light (Heavy : garde de pli) ; I4 relief de signe inversé : CoreClearance −0,72 p, ordre −0,72 à −1,11 p ; I5 domaine de κ retiré : κ = 0 n'est plus refusé par sa garde (le message devient « retourne ») ; I6, I7, I17, I18 gardes retirées : `the_stacking_guards_are_seen` rouge ; I8 décalage ignoré par la garde de courbure : rouge ; I9 éventail à 0,4 : dispersion 39,9 à 49,1° (seuils 60 et 70°) ; 92 (I9, borne basse, gardes de domaine et de côtés opposés retirées) : −8,8 à −30,2° (seuil −5 / −3°) ; I10 : `the_blade_widens_into_a_cap` rouge ; I11 : `the_tip_is_acute_or_rounded` rouge ; I12 gabarit sans col : col 0,19 à 0,28 hw ; I13 angles de type A1b (55° à 105°) : remplissage 63,2 et 69,9 % (Heavy : garde de pli) ; I14 relief nul : 0 p ; I15 ordre des lobes inversé : ordre −1,76 à −2,40 p, seul ce test rougit ; I16 rangs tassés à la pointe : 1,70 à 1,82° (autres : garde de pli) | I8 « occupation restreinte à la lame » (faute 81) reste verte ; borne haute de I9 (down + 15°) jamais vue rouge | 81 : dans les cas trouvés, le décalage qui fait refuser la courbure est celui de la lame ; borne haute : une direction au-delà de downAngle + 15° pointe sous le pied et lève la garde de portée (faute 91 : « portée du lobe 0 ») |
| A1c S8 | Planche et jugement visuel | `DISABLED_leaf_sheet` (81 600 triangles), `render_glb.py` (sans option, `--side`, `--full`), `crop.py`, `montage_a1.py` | **[M]** `planche_feuilles_A1c.png` (session) | voir §11.9 n° 12 | — |
| A1c S9 | TU complet vert ; GLB de opus inchangé | `TU.exe` (build 10:23) sans filtre, depuis `scratchpad\tu_run` ; `opus.exe` reconstruit 10:24, `--colonne-corinthienne --out`, sha256 | **[M]** GLB `6a852fdc…` identique ; TU : ligne suivante | aucun | le générateur n'est pas branché (lot A2) |
| A1c S9 (TU) | TU complet sans filtre vert | `TU.exe` (build 10:23), depuis `scratchpad\tu_run`, 292 s | **[M]** 3 041 tests lancés : **3 029 verts, 0 rouge**, 12 sautés (`ImportScad`, OpenSCAD absent), 16 désactivés | aucun | 9 tests de plus qu'en A1b (instruments I12 à I16, gardes) |
| A1e S0 | hw_E du feuillage dans [5,85 ; 7,85] p | grille sur la pl. XXVI | **[M]** 5,1 ± 0,5 p (minorant) ; plan de la pl. XXV 4,2 à 4,4 p | −0,75 à −1,6 p | largeur déduite d'un λ isotrope, non mesurée ; métrique par rang (AC-41) |
| A1e S4 | J11, angle 3D minimal : Heavy F ≥ 4,5°, Light ≥ 8,2° | `the_faces_keep_their_angles` | **[M]** Heavy F 10,18°, E 8,98° ; Light F 4,70° (bec, y 1,68), E 6,40° (bec) | Light −3,5° et −1,8° | minima au bec, triangles du gabarit et non du plaquage ; prédiction révisée Light ≥ 4° (architecte), seuils du test 1° / 3° inchangés |
| A1e S4 | J16′ : contact dans [−0,30 ; −0,05] p | `the_sheet_follows_its_template_and_carrier` | **[M]** −0,274 à −0,283 p | aucun | — |
| A1e S4bis | Corps hybride G2 tenant J18, J20 (±0,6 p), J20b et J21′ | prototype Python hors dépôt (`hybrid.py`), constructeur validé sur J18, J11 et J21′ du C++ | **[M]p** G2 infaisable (β ≈ 142°) ; G1 sans solution sous la garde de courbure ; J21′ −0,80 | réfutée | tout point de l'arc est à off ≥ off_b ; le début de la rampe de gouttière (rayon ≈ 0,7 p) tombe dans le relief plein dès que β > 0 ; une lame qui longe la corbeille jusqu'à h ≈ 9 ne laisse pas passer E. Modèle actuel gardé (décision de l'utilisateur), S4bis en dette |
| A1e S7 | Durée F Heavy : médiane ≤ 8 ms (critère dur 15 ms) | sonde temporaire par phase (`probe.py`), `DISABLED_leaf_build_time`, 3 séries de 21, Release MSVC | **[M]** avant : 44,1 ms, `Free` 97 % (H 15 %, Hb et CoreRoom 82 %) ; après la portée bornée : **1,79 / 1,86 / 1,88 ms** (E Heavy 2,11 / 2,20 / 2,18 ms) ; sommets des 4 feuilles identiques à l'octet | prédiction ×5,5 avant, tenue après | la prédiction ventilait A1c sans `Free` ; `Free` (−n_S) avançait vers l'axe jusqu'à 40 p (2 000 pas) |
| A1e S7 | TU complet ; GLB de opus inchangé | TU en deux moitiés (`TEST_cgarchi*`, `-TEST_cgarchi*`), Release, un processus à la fois ; `opus --colonne-corinthienne` | voir §11.10 | — | — |
| A2 conception | Couverture F + E ≥ 85 % (A1) | intégration à la main par secteur | 0,70 ± 0,10 **[E]** | −15 points | affirmation sur la forme cible non vérifiée contre la pl. XXVI (corbeille visible entre les E) ; seuil revu (Q5) |
| A2 S0 | Couverture F + E dans [0,60 ; 0,80] | `BellCoverage` sur 16 feuilles `BuildLeafSheet` aux θ_k (sonde) | **[M]** 0,7106 (Light), 0,7125 (Heavy) | +0,01 | — |
| A2 S0 | Tour + 16 feuilles : 0 auto-intersection | `ValidateMesh` avec enregistrements classés par membre du tour | **[M]** 760 (Light), 1 520 (Heavy), toutes contre l'astragale : mur de pied de E coplanaire à h = 0, face extérieure et mur de F plongés dans le tore jusqu'à −0,53 p ; aucune entre feuilles | réfutée | risque nommé par l'architecte (constat 7), non mesuré avant S0 ; d'où AC-49 et l'étape S1b |
| A2 S0 | Tour seul (réserve RA3) | sonde, 3 × 21, Release | **[M]** 0,59 à 0,60 ms | — | — |
| A2 S1 | `BuildLeafShell` extrait sans changement de sortie | vidage des 4 feuilles aux échelles 1 et 2 (`%a`), sha256 | **[M]** identique à l'octet | aucun | — |
| A2 S1b | AC-49 : plus bas sommet de F ≈ 0,17 p ; J17 de F ≈ 0,80 ; J21′ et K7 en baisse | J19 étendu, J17, J21′, sonde K10 | **[M]** F 0,1685 p, E 0,0500 p ; J17 0,787 ; **J21′ et K7 inchangés** (+0,876 / +0,765, minimum sur le bec de F, que le pied ne déplace pas) ; K10 valide | J21′ : réfutée | hypothèse sur le lieu du minimum non vérifiée (il est au bec, fixé par hTop et le cône) |
| A2 S2 | K3 ≤ 2e-6 p ; K7 +0,88 / +0,77 ; K4 ≤ 4e-6 p | tests K2 à K9, K12 | **[M]** K3 2,8e-16 p (flottants identiques) ; K7 +0,8762 / +0,7650 sur les 8 paires ; K4 2,0e-6 / 2,1e-6 p (1 ulp à r ≈ 27) ; K8 0,7097 / 0,7116 ; K9 valide | aucun | — |
| A2 S3 | T5, T6, T8 verts sans modification ; seuls T1 à T8 rougissent | passe de repérage avant adaptation | **[M]** T5, T6, T8 verts ; un test hors liste rouge, `the_colonnade_entablature_rests_on_the_top_level_of_its_column` (devenu T9) | 1 test | fixture : colonne de référence à None, colonnade neuve à Light (même cause que T3) |
| A2 S4 | GLB à 43 344 triangles, déterministe | `opus --colonne-corinthienne`, deux exécutions | **[M]** 43 344 faces, 21 706 sommets, 41 394 exportés ; sha256 `1019d12c…` identique ; 48 empreintes à None inchangées | aucun | — |
| A2 S4 | K12 : pointes de E à 27,4 ± 0,3 p (critère 27,6 ± 0,6) | max r par pièce | **[M]** E 26,94 (Light) / 27,00 (Heavy) ; F 22,14 / 22,18 | E −0,46 : réfutée ; critère révisé à [26,4 ; 28,9] | la pointe de E n'est pas son point le plus éloigné de l'axe (cause 1) ; tolérance plus étroite que la dispersion de la lecture du plan, 27,0 à 28,3 (cause 2) |
| A2 S5 | Faute 44 (centre de rotation décalé de 0,2 p en x) rougit K6 | `faults_a2s5.py` | **[M]** K6 vert (−0,274 p) ; la variante 144 (−0,2 p) rougit K6 (+0,117 p) | réfutée | géométriquement impossible : un centre décalé tire chaque copie vers l'axe, composante radiale 0,2 (cos α − 1) ≤ 0 |
| A2 S5 | Faute 47 (`Free` sur `EnvelopeRadius (h, θ)`) rougit K12 | `faults_a2s5.py` | **[M]** K12 vert (E 26,91 p), **K3 rouge** ; la variante 147 (crête de E à 21 p) rougit K12 | réfutée | `Free` ne plafonne que le relief, la pointe est posée par off_b sur `LeafBoundRadius` ; sur des copies tournées K12 est constant par rang : seul K3 voit une borne non axisymétrique |
| A2 S5 | K2 et K3 voient une faute dans `RankTheta` | `faults_a2s5.py` | **[M]** avant durcissement : 41 et 143 vertes sur K2 et K3 (qui lisaient `RankTheta`) ; après : K2, K3, K7 sur des axes littéraux (`PlateAxis`), toutes rouges | instrument corrigé | détecteur qui lisait la grandeur fautive (cause 7) |
| A2 S5 | Fautes 40 à 53 (sans 49) : chacune rougit son ensemble attendu, avec au plus les voisins nommés ; faute 0 verte ; fautes 1 à 39 (lots A1e et antérieurs) toujours rouges | `faults_a2s5.py` (test-engineer) : `CG_LEAF_FAULT` dans 3 sources, sha256 avant et après, assertion rouge relevée par invariant (fichier:ligne) sur 91 tests (acanthe, colonne, ordres, colonnade) | **[M]** après durcissement de K2, K3 et K7 (axes littéraux `PlateAxis`, taille du vecteur des pièces assertée, K1 et K10 en littéraux) : 0 verte ; 1 à 39, 40 à 43, 45, 46, 48, 50 à 53 rouges à l'assertion visée ; 48 arrêtée par la garde du pied (148, garde neutralisée : K10 rouge) ; 52 arrêtée par la garde gabarit / forme (152 : K1 rouge) ; 154 (échelle non transmise par la colonne) : T4 rouge ; 44 et 47 vertes sur leur invariant prévu (lignes ci-dessus) ; sources restaurées à l'octet, aucun `FAULTINJ` | 2 fautes sur 13 sans le rouge prévu | voir les lignes 44 et 47 : prédictions fausses, pas instruments aveugles (144 et 147 rougissent K6 et K12) |
| A2 S6 | K11 : colonne corinthienne Light ≈ tour + 1 ms (≤ 12), Heavy ≈ tour + 6 ms (≤ 20) ; critères durs 15 / 40 ms | sonde, 3 séries de 21, Release MSVC | **[M]** Light 3,23 à 3,32 ms, Heavy 15,9 à 16,5 ms, tour 0,54 ms | Light ×2,1, Heavy ×2,5 sur « tour + » ; critères tenus | la prédiction comptait deux constructions de feuille et des copies, sans la recopie de `appendMesh` ni la conversion float (non mesurées séparément) [H] |
| A2 S6 | Contrôles finaux sur `87b5a18` : TU 1 012 + 2 030 verts, 12 sautés ; GLB `1019d12c…` | TU en deux moitiés, Release, un processus à la fois ; `opus --colonne-corinthienne` reconstruit par MSBuild | **[M]** 1 012 + 2 030 verts, 12 sautés, 0 rouge ; 43 344 faces, sha256 `1019d12c…c3fbc1` | aucun | — |
| A2 S7 | Faute « ordre des pièces inversé » vue par K7 sur l'assertion de rang ajoutée à la revue | faute injectée dans `BuildCorinthianOrnament`, test K7 | **[M]** K7 rouge à l'assertion de rang de E ; K1 rouge aussi ; source restaurée (sha256) | aucun | — |
| S4bis-2 P0 | Instruments Python (porteuse, coque, J18, J20, garde, J7, K7, K8) au C++ à 0,02 près | `s4bis2_proto\p0.py` contre `TU.exe` de `445f578` | **[M]** 80 / 82 dans la tolérance (marge +0,097 d'AC-8 = Heavy ; égalité symétrique de θ en K7) ; cas négatifs rouges ; garde en forme close de F Heavy **+0,071** | aucun | la garde à trois sondes manquait le pic au bord de la rampe du dôme |
| S4bis-2 P1 | L1 : marge ≥ 0,3 ; forme inchangée hors fenêtres | prototype, garde en forme close et à trois sondes (128, 256) | **[M]** F +0,161, E +0,485 ; écart hors fenêtres 0 ; écart trois sondes / forme close 0,1 à 0,7 % (C¹ : 3,6 à 14,5 %) | F : réfutée | la rampe de la gouttière reste courte devant le critère physique (1,92 contre 2,40 p) |
| S4bis-2 P2 | Un corps qui longe la corbeille (L2) tient J20, J18 et la garde | calage (β, γ) sur trois côtes, avec L3 et rampes prolongées | **[M]** aucune variante ; β ≥ 33° (F) et ≥ 54° (E) imposés par le départ sur la corbeille, crosse C12 incompatible ; rampes comprimées (garde −0,02 à −0,8) ou J18 perdu (−0,83 / −1,95 p) | réfutée | arrêt prévu ; dette S4bis gardée, L1 seul intégré (décision de l'utilisateur) |
| S4bis-2 I1 | Forme hors fenêtres inchangée ; fenêtres 0,05 à 0,09 p | vidage des 4 feuilles (`DISABLED_dump_sheets`) avant et après, sommet par sommet | **[M]** hors fenêtres 0 ; fenêtres F 0,068 p, **E 0,126 p** | E +0,04 | la prédiction supposait `gutterFadeShift` gardé ; son retrait avance la rampe de la gouttière |
| S4bis-2 I1 | Marges C++ = prototype | `the_curvature_guard_keeps_its_margin_at_any_sampling` | **[M]** F +0,1880 / +0,1875, E +0,5362 / +0,5347 (prototype +0,1880 / +0,1876, +0,5362 / +0,5347) | aucun | — |
| S4bis-2 I3 | Fautes 60 à 64 rouges, chacune sur son test ; 1 à 154 rejouées inchangées | `faults_s4b2.py` | **[M]** 60 à 64 rouges ; 44 et 47 manquent comme en A2 ; faute 0 verte. Au premier passage, 60 rougissait aussi la comparaison aux trois points ; depuis le balayage de la revue, qui écarte les coupures, elle n'a plus de voisin | aucun | — |
| S4bis-2 revue | Faute 65 (courbure de la Bézier du corps × 1,1) vue par la comparaison aux trois points | `faults_s4b2.py 65`, test avant et après le balayage des 8 méridiens | **[M]** verte avec l'ancien test (seul point le plus serré, sur l'arc) ; rouge avec le balayage (voir §11.12) | ancien test : réfutée | le minimum de la garde est sur l'arc : un défaut de la forme close du corps ne le déplace pas |
| S4bis-2 I4 | GLB : comptes inchangés, empreinte nouvelle et déterministe | `opus --colonne-corinthienne`, deux exécutions | **[M]** 43 344 faces, 21 706 sommets, 41 434 exportés (41 394 avant) ; sha256 `46e4b373…d608` deux fois | sommets exportés +40 | normales à seuil de 22° dans les fenêtres de rampe |
| A3 S0 | Tige visible dès h ≈ 13, bague 17,5 à 18, Ø 2,5 à 3,3 p (analyse du tuto) | relevé quadrillé de la pl. XXVI (profils de gris) | **[M]a** visible dès 11,2 / 12,25 ; bague 15,3 à 17,8, deux cordons ; tige Ø 2,0 ± 0,3 | Ø réfuté (−0,5 à −1,3 p) | le tuto mesurait la bague, pas la tige [H] |
| A3 S0 | P3 : deux D par caulicole | plan de la pl. XXV déroulé, profil, pl. XXVI | **[M]a** deux D par caulicole, pied commun, D intérieur et D extérieur différents | appuyée, forme différente du schéma symétrique | appui non relu en A0 |
| A3 S0 | V-1 par seuillage du hachuré | deux instruments (anisotropie, seuil de gris) avec contrôles | **[M]** les deux échouent à leurs contrôles (rappel 0,06 à 0,33 ; lobes sombres) | réfutée | V-1 tracé à la main en S1 (0,0557) |
| A3 S0b | Bec du D extérieur : un arc tient les trois cotes à ±0,6 p | ajustement minimax | **[M]p** 0,70 p au mieux ; aucun cercle ne tient les trois cotes | +0,10 p | la crosse gravée est une spirale (dette) |
| A3 S1 | Bande en U du plan de la pl. XXV = rubans des hélices et des volutes | plan déroulé jusqu'à r 38 | **[M]a** la bande monte jusqu'aux cornes | réfutée | identification posée sans contrôle aux extrémités ; bord de l'abaque gravé [H], relevé en A4 |
| A3 S2 | K20 des tiges −1,07 p | `EnvelopeExcess` | **[M]** −0,913 / −0,911 | +0,16 | prédiction lue à 22,5° ; le plus grand débord est sur le flanc |
| A3 S3 | Budget D 600 / 3 000 | générateur, balayage | **[M]** Light : 852 sans point intérieur, 860 sous 1°, crosse du D extérieur auto-intersectée à 860 ; 1 300 retenu ; Heavy 2 996 | Light ×2,2 | points du contour Olive non comptés |
| A3 S4a | Garde de courbure du D intérieur ≥ 0,1 p à R_extrados 1,2 | `CurvatureMargin` | **[M]** refusée à 1,3 (dôme du bec) ; +0,083 à 1,5 avec hw 3,1, +0,143 avec hw 2,4 | — | dôme omis dans l'estimation |
| A3 S4a | Pointes posées par une inclinaison de 35° / 30° (D-7) | sonde | **[M]** pointes à −22,4° / +20,8° de la tige ; α déduit 20,2° / 19,3° (pied ancré, AC-57) | 15° / 11° | lecture [Q] sans grille |
| A3 S4a | Couverture en élévation 0,18 à 0,22 avec les D | `VisibleBellFraction` | **[M]** 0,117 / 0,114 | −0,06 à −0,10 | D intérieurs plus couvrants qu'estimé |
| A3 S4a | D et tige, D et E sans auto-intersection (pose S4a) | sonde, `ValidateMesh` | **[M]** 5 136 / 5 492 auto-intersections : pieds dans la bague, pieds recouverts (largeur du contour Olive au pied), D extérieur au travers des bords de E | réfutée | seconde pose S4a′ : pieds sur la bague, décalage calculé sur le contour, corps tenu (AC-54, AC-58) |
| A3 S4a′ | Pieds décalés de 2,5° / 2,3° disjoints | sonde | **[M]** 568 auto-intersections D/D à h 18,9 à 19,7 | réfutée | le contour Olive s'élargit (u 0,45 → 0,83 dès y 0,23) plus vite que le cisaillement n'écarte les axes ; décalage calculé (−5,59° / +4,36°) |
| A3 S4b | Colonne Light ≈ 5 ms, Heavy ≈ 22 ms (K27) | `BuildClassicalColumnMesh`, 3 × 21, Release | **[M]** première intégration 81 ms (Light) ; après le point fixe sur une seule porteuse : Light 13,8 à 14,1 ms, Heavy 33,7 à 34,2 ms | ×2,8 / ×1,5 | le point fixe reconstruisait deux porteuses (garde de courbure comprise) par itération ; le reste : 16 coques D |
| A3 S6 | Chaque faute rougit son ensemble attendu | `s6_faults.py` (test-engineer) | **[M]** fautes 70 à 104 rouges ; 1 à 65 rejouées ; 44 et 47 manquent comme en A2 ; 6 détecteurs faibles corrigés et prouvés | — | détecteurs écrits sans faute qui les vise (cause 4) |
| A3 S7 | GLB : comptes 69 904 / 256 656, déterministes | `opus --colonne-corinthienne`, deux exécutions | **[M]** Light 69 904 faces, 72 122 sommets exportés, sha256 `070c48f6…015e` ; Heavy 256 656, 199 346, `f612024a…0b66` | aucun | — |
| A4 plan | +4 000 / +46 000 triangles (± 30 %, §11.4) puis +15 000 / +69 300 (formules du plan A4) | K29 | **[M]** +14 320 (Light) / +56 880 (Heavy) | §11.4 : ×3,6 / +24 % ; plan A4 : −4,5 % / −18 % | cible du §11.4 posée avant toute section de ruban ; échantillonnage Heavy plus sobre que prévu |
| A4 S0 | Creux de l'abaque gravé moins profond de 1 à 3 p | plan de la pl. XXV déroulé, cercles ajustés sur les traits suivis | **[M]a** deux contours : arc du compas = données (18,60 contre 18,64) ; bord hachuré 20,52 à 20,84 | dans la plage, mais pas le creux | identification d'A3 [H] ; données conservées (Q4 (a)) |
| A4 S0 | Calage A1e du plan juste aux cornes (contrôle C-U) | traits sur les diagonales | **[M]** faces coupées à 37,1 / 37,5 p en x | réfutée (1,1 à 1,5 p) | calage refait sur les cornes ; B et listel à ±0,15 |
| A4 S0 | Volutes dans le plan diagonal, hélices parallèles à l'axe de face | pl. XXVI, raccourcis | **[M]a** volutes sans raccourci (1,0 ± 0,1 contre 0,71 attendu) ; hélices raccourcies 0,6 à 0,9 | réfutée | plans à 22,5° et 45° (Q6′) |
| A4 S0 | 12 centres à l'échelle dans ±1 p (Q2) | `volute12.py`, profil de la pl. XXV | **[M]a** résidus ≤ 0,34 p, 3 tours | aucun | — |
| A4 S0 | La pose des D domine les 14 ms (Light) | sonde de ventilation (T-1) | **[M]** D 53 % en Light ; en Heavy, copies et fusion 56 % | Heavy : réfutée | — |
| A4 S1′ | Test du cache : un abaque différent donne une autre pose | test, faute 126 | **[M]** la pose ne lit pas l'abaque : clé sans l'abaque verte | réfutée | critère posé sans lire les entrées du point fixe ; faute redéfinie (clé sans les bandes) |
| A4 S1′ | Colonne Light ≈ 10,6 ms, Heavy ≤ 24 ms | 3 × 21, Release | **[M]** 8,43 à 8,56 / 19,04 à 19,28 ms ; sorties identiques à l'octet | −2,1 / −4,8 ms | copies et fusion sous-estimées |
| A4 S2 | Tige de la volute née à h 23,5 derrière le D extérieur (Q7) | rayon minimal des pièces devant la corbeille (`channel_fine.py`) | **[M]** place 0,51 à 0,62 p pour 1,10 nécessaires ; 1,12 à h 24,45 | réfutée | corps tenu du D extérieur (AC-58) ; naissance à h 24,5 (Q7′) |
| A4 S2 | Garde du cône tenue avec l'œil de la volute à h 31,3 | prototype | **[M]** +0,019 p hors du cône ; −0,067 à h 31,5 | réfutée | h 31,5 (profil relevé) |
| A4 S3 | Œil des hélices à r ≈ 18,0 (noyau + c + b/2) | balayage par sonde | **[M]** x < 19,4 : courbure de la tige refusée ; 19,4 à 21,0 : tige contre spirale (12 à 34 auto-intersections) ; valide dès 21,5 | +3,5 p | estimation posée sans la tige |
| A4 S5 | K36 vert (V et H à ≥ c des autres pièces) | K36 sommets contre triangles (`PieceGap`) | **[M]** pied de la volute à 0,0056 p du D extérieur en Heavy (0,064 Light) | réfutée | l'instrument sommets contre sommets voyait ≥ 0,072 p (faux vert) |
| A4 S5′ | Le pied se dégage en le reculant vers l'intérieur, ou en déplaçant θ_b | sonde (jour) et `cover.py` (part cachée) | **[M]** recul impossible : face intérieure déjà à noyau + c ; θ_b de 24 à 34° : jour ≥ c seulement sous 25°, où le pied est découvert (1/11 caché) | réfutée | pied aminci à 0,8 b (décision de l'utilisateur) : jour 0,122 / 0,125 p |
| A4 S5 | Les fautes 211 et 212 (pied décalé) rougissent K34 | rejeu S5 (test-engineer) | **[M]** rouges sur K36 seulement | réfutée | la face intérieure ne se rapproche pas du noyau |
| A4 S6 | Colonne ≤ 15 / 40 ms ; prédiction A4 15 à 16 ms (Light) | 3 × 21, Release (K42) | **[M]** Light 9,15 à 9,22 ms, Heavy 20,34 à 20,93 ms | −6 ms | S1′ |
| A4 S6 | GLB déterministes | `opus --colonne-corinthienne`, deux exécutions | **[M]** Light 84 224 faces, sha256 `5c464d3d…1f77` ; Heavy 313 536, `06faba3e…e35e` | aucun | — |
| A5 plan | Fleuron ≈ 8 p, pas 4 (JSON) | trois vues de Vignole | **[M]a** vertical 7,70 à 7,96 ; horizontal 8,37 à 8,83 | JSON « 4 » contredit | affirmation sur l'existant non vérifiée (repris par AC-15 et le tuto) |
| A5 S0 | Contrôles des calages ; le plan de la pl. XXV est une rosace rabattue (charnière 18,85) | traits sous-pixel, plan | **[M]a** pl. XXV tournée de 1,0° ; abaque gravé à 12,4 px/p en vertical ; bas de B lu 35,82 (et non 36,00) ; le trait du plan est à r 19,95 (lèvre) | calage : 0,3 à 0,4 p ; charnière réfutée | contrôles posés sur une seule colonne de pixels |
| A5 S0 | Couloir de la tige sous les D intérieurs (0,66 à 0,78 p) n'admet pas 0,77 p | `PieceGap` sur une tige d'essai, Light et Heavy, contrôles positifs E8 et H48 | **[M]** couloir en élévation seulement ; jour ≥ 1 p en volume de h 23 à 34 ; tige cachée par E jusqu'à h 24,3 / 24,45 | réfutée | estimation angulaire sans la profondeur des pièces |
| A5 S1 | Débord sur B 4,2 à 4,6 (au bord de K50) | K50 | **[M]** 4,66 avec la flèche 0,6 (rouge) ; 4,51 avec 0,45 (décision K50) | +0,1 | sommet du bouton compté en plus de la face avant |
| A5 S1 | Jour au tour ≈ c par construction du dos | K49′, 8 à 256 secteurs, cannelé | **[M]** 0,0500 partout ; dos recopié du miroir avec des diagonales non symétriques : 0,021 (256 secteurs) et 0,012 (cannelé) avant correction | défaut trouvé | diagonales non symétriques par y → −y |
| A5 S1bis | Épaisseur minimale 0,9 à 1,1 p devant la lèvre | garde, test | **[M]** 0,885 (Light), 0,936 (Heavy) | dans la plage | — |
| A5 S2 | Rosace / hélices ≥ 0,5 p | K49 (`PieceGap`) | **[M]** 1,62 à 1,70 p (H48) ; avec la tige, 1,20 / 1,21 p (D24) | +1,1 p | — |
| A5 S2 | +3 536 / +18 672 triangles (formules §3.6 du plan) | K45 | **[M]** +8 656 (Light) / +29 936 (Heavy) | ×2,4 / +60 % | cuillers larges et noyau (S1bis), tige |
| A5 S3 | Durée de la colonne 9,3 à 9,8 / 20,7 à 22,0 ms | 3 × 21, Release (K42) | **[M]** 10,8 à 10,9 / 23,6 à 23,7 ms | +1 à +1,5 ms | `TourDepthMax` par sommet du dos et de la tige |
| A5 S5 | Chaque faute rougit son ensemble attendu | `a5s5_faults.py` (test-engineer) | **[M]** 48 sur 48 ; rejeu antérieur 164 sur 168 (44, 46, 47, 4126 connus) ; 55179, 55178, 55163, 55162 ajoutées | — | — |
| A5 S6 | GLB déterministes ; suite ≈ 3 101 verts | `opus --colonne-corinthienne`, deux exécutions ; TU par tranches | **[M]** Light 92 880 faces, sha256 `958b9425…b207b0` ; Heavy 343 472, `3889c595…39b3df` ; **3 118 verts**, 12 ignorés | +17 tests | tests de la tige et du mode enveloppe |
| A5 S7 | Contrôle négatif de D4 : rotation de π/4 des G à ≥ 1,00 p | plus proche voisin exact (grille de 0,5 p parcourue en couronnes) | **[M]** 9,21 p (Light et Heavy) | « 1,00 » était le plafond de la recherche (27 cellules de 1e-3 p) | instrument cité sans son plafond |
| A5 S7 | Dos de la rosace au max des deux boîtes symétriques : GLB et durée inchangés | GLB, K42 (3 × 21, Release) | **[M]** GLB identiques (`958b9425…`, `3889c595…`) ; K42 12,3 à 12,6 / 26,9 à 27,2 ms | +1,5 / +3,3 ms | deux lectures du tour par sommet du dos |
| A5 S7 | Gardes de `BuildFleuron` et de `ReadCapitalTour` vues par leurs tests | fautes 5301 à 5308 ; rejeu d'A5 | **[M]** 55 sur 55 rouges ; 5176 (diagonales) ne rougit plus le jour au tour, seulement la symétrie | — | le dos couvre les deux boîtes |
| A5 S7 | Suite | TU par tranches | **[M]** 3 118 verts, 12 ignorés | aucun (cas ajoutés dans les tests existants) | — |
| A6 conception | §11.4 « ≈ 50 / ≈ 330 ms WASM [E] » et « colonne ≈ 31 600 / ≈ 272 000 » | comptes [M] d'A5 ; aucun WASM | **[M]** colonne 92 880 / 343 472 (×2,9 / +26 %) ; WASM non mesuré | cibles caduques dès A2 | 1 (cibles posées sur le modèle v0, avant toute forme) |
| A6 conception | bannière de la colonnade « vers 10 colonnes » (P6) | arithmétique sur les comptes [M] | N = 4 (Light), N = 1 (Heavy) [E] | ×2,5 plus tôt | 1 |
| A6 conception | bilans A2 à A5 dans l'arbre de travail | `git status`, `git show stash@{0}` | **[M]** absents ; dans `stash@{0}` seulement | — | 3 (état de l'outillage git non vérifié avant la tâche) |
| A6 conception | module WASM publié porte l'acanthe | `grep -c` sur `maker.wasm` (contrôle positif « Classical Colonnade ») | **[M]** « Capital ornaments » absent | — | 1 |
| A6 S1 | ventilation ≥ 80 % de K42 ; D (poses comprises) premier poste en Light | `DISABLED_ornament_measures_by_rank` | **[M]** représentantes seules 74,6 % (Light) / 62,4 % (Heavy) de la colonne ; par différence : assemblage 4,6 / 5,9 %, tour et versement dans le maillage 20,8 / 31,7 % ; D premier poste en Light (3,74 ms, 41 % des représentantes), G en Heavy (6,78 ms, 37 %) | ventilation par représentantes −5 / −18 points | 1 (le versement de 165 000 sommets et 330 000 faces dans le `Mesh`, 9,3 ms en Heavy, n'était pas compté) |
| A6 S1 | cache froid : K42 + 3 à 8 ms | 5 processus | **[M]** premier appel 30,0 ms (Light, chaud 12,5) / 47,1 ms (Heavy, chaud 29,5) : **+17,5 / +17,6 ms** ; poses des D seules 14,1 ms au premier appel, 0,000 ms ensuite | ×2 à ×6 | 1 (A4 S0 mesurait la part de D avec ses coques, pas le point fixe seul ; reste ≈ 3,4 ms : gabarits et premières lectures **[H]**) |
| A6 S1 | débord de l'abaque : C −0,91, D +1,68, V +3,65, H +6,3 (contrôles) ; E ≈ +9 p sur l'axe de face | K60 | **[M]** Light F +2,99, E +9,34, C −0,91, D +1,68 (D i. ; D e. +1,33), V +3,65, H +6,29, G +1,98 ; Heavy F +3,03, E +9,40, C −0,91, D +1,69, V +3,65, H +6,31, G +1,99 | aucun | — |
| A6 S2 | colonnade N = 4 Light 374 436 faces ; seuil à N = 4 (Light), 1 (Heavy) | K61 | **[M]** 374 436 ; E (N) = 516 + 600 N (1 116, 2 316, 2 916 ; 7 716 à N = 12) ; bannière dès N = 4 (Light) et N = 1 (Heavy) | aucun | — |
| A6 S2 | natif N = 4 : 25 à 45 / 70 à 150 ms ; N = 12 : 80 à 210 ms / 0,27 à 0,75 s ; part de l'assemblage 65 à 80 % à N = 12 | `DISABLED_corinthian_colonnade_cost` | **[M]** N = 4 : 52,4 / 178,1 ms ; N = 12 : 191,3 / 696,8 ms ; part de l'assemblage à N = 12 : 58,8 / 61,1 % ; crête du processus 112 / 278 Mo (N = 12) | N = 4 : +16 % / +19 % au-dessus de la fourchette ; N = 12 dans la fourchette ; part −6 / −4 points ; crête sous 0,5 à 1 Go [E] | 1 (à N = 4, normales et copies pèsent plus que T_app seul ; la crête était estimée sans mesure de la taille d'un `Mesh`) |
| A6 S3 | assemblage linéaire : sortie identique, N = 12 Light ×3 à ×5 plus rapide | K62, instrument sur `AssembleColonnadeMesh` | **[M]** 21 empreintes identiques ; assemblage N = 12 ÷ 7,3 (Light, 150,0 → 20,5 ms) et ÷ 7,0 (Heavy, 578,0 → 82,6 ms) ; Regenerate N = 12 ÷ 3,3 (191,6 → 58,6 ms) et ÷ 3,5 (697,6 → 196,6 ms) | au-dessus de ×3 à ×5 pour l'assemblage | 1 (la part réelle de l'assemblage, entablement et ornements compris, était 78 / 83 %, pas 59 / 61 % : la boucle rejouée de S2 omettait les versements de l'entablement et des ornements) |
| A6 S4 | Node, colonne chaude 15 à 35 / 35 à 80 ms ; froide + 4 à 16 ms | `probe_a6.mjs` | **[M]** chaude 21,1 / 55,6 ms ; premier appel 68,5 / 111,1 ms (médianes de 5 processus) : **+47 / +55 ms** | chaud dans la fourchette ; froid ×3 à ×12 | 1 (prédiction fondée sur le « +3 à 8 ms » natif, mesuré +17,5 en S1, puis multiplié par f) |
| A6 S4 | navigateur, ligne de statut 25 à 60 / 50 à 120 ms | lecture de l'utilisateur (Firefox, captures d'écran `a6\s4\captures\`) | **[M]** colonne Light 46 602 sommets, 92 880 faces, 30,0 ms ; Heavy 171 898 sommets, 343 472 faces, 96,0 ms ; colonnade N = 4 Light 374 436 faces, bannière « a None » (elle remplace la ligne de statut : durée non lue) ; N = 12 Heavy 4 129 380 faces, affichée sans incident, bannière « a Light » | aucun | — |
| A6 S4 | Node, colonnade N = 12 Heavy 0,35 à 1,5 s ; tas 300 à 450 Mo | `probe_a6.mjs` | **[M]** 338 ms ; tas 247 Mo (N = 12 Light : 96 ms, 83 Mo) | sous la fourchette (assemblage linéaire de S3, postérieur à la prédiction) | 4 (S3 a changé l'objet mesuré) |
| A6 S4 | comptes WASM = natif | K64 | **[M]** égaux sur les 8 cas (colonne 92 880 / 343 472 ; colonnade N = 1, 4, 12 Light et Heavy = K61 et S2) ; stderr muet | aucun | — |
| A6 S5 | L2 : GLB identiques, K42 −1,5 / −3,3 ms | sha256, K42, K63 | **[M]** lecture unique non sûre : le maillage du tour réel n'est pas son propre miroir sur la bande du dos (borne 0,089 / 0,060 / 0,020 / 0,022 p) ; variante retirée (option (a)) ; deux lectures gardées, GLB identiques, K42 inchangé (12,27 à 12,30 / 27,4 à 29,7 ms) | gain nul | 1 (symétrie du maillage du tour supposée, jamais mesurée) |
| A6 S5 | 47 rouge sur K3 | rejeu de 47 seule, `TEST_cgarchi_classical*` (410 tests) | **[M]** 13 tests rouges : K3 (copie / construction 1,18 p contre 2,8e-16), K2 (phase 1,12e-4 rad > 1e-4), symétries de la feuille, de l'octogone, du carré et de l'abaque, préfixes inchangés (fleurons, hélices, volutes), dôme, relief du revers, K60 ; **K12 vert** (pointes de E 26,9052 p, comme sous la faute 0) | aucun | 2 (le rejeu d'A5 exigeait K3 **et** K12 (`a5s5_faults.py:868`) ; K12 est constant par rang sur des copies tournées : la faute était vue depuis A2, l'ensemble attendu était faux ; « aucune assertion ne rougit » de la dette l. 106 est réfuté) |
| A6 S5 | filtre vert sous GCC ; 4126 rouge sous TSan | K66, K65 | **retirés** (décision Q-A6-6 (c) : la CI fait foi pour GCC) ; l'image Docker de développement n'a pas de compilateur C++ GCC **[M]** (S0) | — | — |
| A6 S6 | chaque faute rougit son ensemble attendu | `a6s6_faults.py` (test-engineer) | **[M]** A6 11 sur 11 (6403 ajoutée en S6 : gardes de taille de K63) ; A5 55 sur 55 ; antérieures 163 sur 166 (44 → 144, 46 → 1046, 4126 sans TSan) ; 47 rouge sur K3 | — | — |
| A6 S7 | suite ≈ 3 120 à 3 123 verts | tranches (S6, sources inchangées depuis, sha256) | **[M]** **3 122 verts**, 12 ignorés, 0 rouge ; /W4 : 0 avertissement | dans la plage | — |
| A7 conception (dette d'A6 l. 102) | cause **[H]** : volute trop loin, ou pointe vive des données contre corne tronquée | `excess_rings.py` ; données de B | **[M]** le débord est à 41° (flancs, pas pointe) ; la troncature ne joue qu'au-delà de 44° | 2e candidate réfutée ; 1re réfutée contre la planche, confirmée contre l'abaque construit | 1 (contour de référence non choisi : AC-2 mesurait contre B seul) |
| A7 conception | V contre A (abaque construit), maillages d'A6 | `excess_rings.py`, contrôle positif K60 | **[M]** +1,22 / +1,23 p ; second contour −0,44 | — | — |
| A7 S0 | lectures [E]a de la pl. XXV confirmées à ±1 p | relevé calé (calage AC-64) | **[M]a** profil : portée de la spirale 31,2 (± 0,2) à h ≈ 31,5 ; œil ≈ 27,1, h ≈ 31,2 ; plan : ruban à bout arrondi, bout à r 30,8 et 31,5, bord extérieur entre A et A + 1 p ; modèle d'A6 : portée 31,11, V au-delà de A de 0,6 à 0,7 p en latéral | confirmées, dispersion entre les deux cornes ≈ 0,7 p | — |
| A7 S1 | point réalisable par le plan (`planeAngle` 28 à 34°) complété par `eyeRadius` | `DISABLED_volute_pose_sweep` (175 points par niveau) | **[M]** tourner le plan vers la diagonale aggrave V contre A (+0,13 p par degré) : 0 point réalisable sur la grille du plan ; 16 (Light) / 15 (Heavy) réalisables avec `eyeTheta` 40 à 40,5° et `eyeRadius` 27,2 à 27,4, `planeAngle` ≤ 22,5° | **réfutée** (sens du levier) | 1 (effet du plan estimé sur la latéralité, sans calcul) |
| A7 S2 | comptes inchangés ; autres rangs identiques à l'octet ; V contre A −0,05 à −0,30, contre B +2,2 à +2,6 | K60, `DISABLED_rank_fingerprints` | **[M]** comptes égaux (92 880 / 343 472) ; F, E, C, D, H, G identiques à l'octet ; V contre A −0,108 / −0,106, contre B +2,44 ; cône −0,459 / −0,453 ; paire 0,940 ; pied caché 0,649 ; raccourci 0,982 | dans les fourchettes | — |
| A7 S4 | 6 à 8 fautes nouvelles, toutes rouges | `a7s4_faults.py` (dérivé d'A6 S6) | **[M]** 7101 à 7106 rouges ; rejeu A6 (14) et A4 familles V et H rouges après re-pose de 1111, 1117 et 215 (variantes 1215, 2215) | 3 fautes d'A4 mal posées à la pose C1 | 2 (1111 et 1117 retirent la garde du cône pour atteindre un détecteur aval : la garde en plan les arrête d'abord ; à θ_b 25°, la tige entre dans la corbeille) |
| A7 S5 | 3 124 à 3 126 verts ; K42 ±5 % | tranches ; K42 (S3) | **[M]** 3 122 verts, 12 ignorés, 0 rouge ; K42 12,28 à 12,29 / 26,80 à 27,20 ms | **réfutée** sur le compte | 2 (les cas nouveaux sont des assertions dans des tests existants ; les tests nouveaux sont des instruments désactivés) |
| A7 S6 | corrections de la relecture sans effet sur les sorties | `DISABLED_rank_fingerprints` avant / après | **[M]** empreintes des 7 rangs identiques (Light et Heavy) ; 3 122 verts, 12 ignorés ; fautes 7107, 7108 (refus du talon) rouges, isolées | aucun | — |
| A7 S7 | WASM : comptes inchangés ; V contre A égal au natif à 1e-3 | `probe_a7.mjs` (secteur des cornes, 5,75 à 14 p sous le sommet), `probe_a6.mjs` | **[M]** −0,1085 / −0,1062 p (natif −0,1085 / −0,1062) ; module d'A6 +1,219 / +1,225 (rouge) ; comptes égaux sur 10 cas | aucun | — |

### 11.6 Écarts au plan (lot A0)

1. **Noms de l'API** : `OrnamentPiece` et `OrnamentPieceKind` du plan deviennent `CapitalPiece` et
   `CapitalPieceKind`. `cgarchi::classical::OrnamentPiece` existe déjà (ornements de l'entablement grec,
   `classical_greek_entablature.h:435`) ; la collision compile, se lie, et corrompt le tas (journal 11.5).
2. **`LeafStyle` et `AcanthusOptions::style` non introduits** : le modèle v0 n'a qu'une feuille, un champ
   ignoré surprendrait. Ils entrent au lot A1 avec le générateur. `segmentsPerLeaf` reste jusqu'au retrait du
   modèle v0 (A2). `OrnamentDetail` est en place : `None` rend un maillage vide sans pièce, `Light` et
   `Heavy` rendent le modèle v0.
3. **Instruments** : hauteurs en parties depuis le pied du chapiteau (AC-5) ; `CapitalEnvelope` porte
   l'échelle du maillage mesuré ; `BellCoverage` prend le maillage, les pièces retenues (rangs F et E pour le
   critère A2), la bande [hLo, hHi] et la grille (360 × 48 par défaut). Surcharges par plage de sommets et par
   pièce pour `CoreClearance` et `EnvelopeExcess`. Définitions dans `classical_acanthus.cpp`.
4. **Abaque de la borne P4 = membre `B · abaco`** (AC-4), le plan ne désignant pas le membre.
5. ~~**Borne P4 sous le noyau**~~ — **résolu par la décision D2** (borne relevée au noyau). Énoncé d'origine : **Borne P4 sous le noyau** près de la lèvre de la corbeille, hors des cornes (journal 11.5, dette §2.2) : la
   borne stricte P5 ne peut pas être tenue par une pièce posée sur la corbeille au-dessus de h = 30,85 sur
   l'axe de face et de h = 34,45 à 22,5°. À trancher avant A3 (caulicoles à 22,5°).
6. **Couverture du modèle v0 72,5 %**, au-dessus de la fourchette estimée (40 à 70 %) : le seuil de 85 % du
   lot A2 reste plus haut que l'existant, de 12,5 points seulement.
7. ~~**Deux tests rouges avant A0 le restent**~~ — **résolu par la décision D1** (`capitalOrnaments` à None par défaut : 0 rouge). Énoncé d'origine : **Deux tests rouges avant A0 le restent** (journal 11.5) ; A0 ne les touche pas, leur correction dépend du
   modèle valide (A2) et de la reprise des empreintes corinthiennes.
8. **`capitalOrnaments` avancé de A2 à A0** (D1), défaut None ; le passage à Light reste au lot A2, avec la reprise
   des empreintes corinthiennes de `the_classical_column_keeps_its_fingerprint_in_all_48_configurations` (à None,
   elles sont celles de la colonne sans ornement).
9. **`BellCoverage` reste une projection angulaire** (L6) : le filtre r ≥ noyau − τ est écarté, il ajouterait une
   convention (τ) sans effet sur le modèle v0 ; le contrat de l'en-tête le dit, la mesure 72,5 % est inchangée.
10. **`render_glb.py` signale les transformations de nœud sans les refuser** (L7), voir le journal.

### 11.7 Écarts au plan et à la référence (lot A1)

*État : le générateur de ce lot (`LeafCarrier`, `BuildLeafSheet` sans gabarit) et ses tests sont retirés au lot A1e (S5) ; Acute retiré le 2026-11-23. Section gardée comme journal.*


1. **`BuildLeafSheet (env, spec, placement, closeRim)`** : l'enveloppe lue (`CapitalEnvelope`) est un argument de
   plus que le plan ; `LeafGridOf`, `DefaultLeafSpec` et `LeafStyle` (dans `AcanthusOptions`, défaut Acute) ajoutés.
2. **Surface porteuse indépendante du contour** et **garde du cisaillement** (refus d'une grille retournée), non
   prévues : sans elles, la coque se replie aux œillets (journal A1).
3. **Ressemblance aux références, décrite honnêtement.** Ce que la feuille rend : un corps qui épouse la corbeille,
   une gouttière, des lobes séparés par des œillets, une crosse qui monte, touche la borne et retombe en rentrant ;
   en Heavy, des côtes droites et des dents (Acute) ou des festons (Olive). Écarts restants :
   - **R1 avéré** : le bord est une fonction de v ; les dents et les œillets sont des marches horizontales du
     contour, qui se lisent comme des gradins (« écailles ») et non comme les digitations obliques et détachées
     de cgtrader ou les grappes de folioles de Vignole ; les franges latérales de cgtrader (folioles en éventail
     hors du plan de la feuille) sont hors de portée de ce modèle ;
   - **calotte** : la crosse est un arc de cercle de rayon 0,4 × place, sans le chapeau élargi de cgtrader (la
     feuille y est plus large en haut qu'au corps) ni le crochet de Vignole qui pend devant la feuille ; la pointe
     reste sous la crosse, peu visible de face ;
   - **E très plat** : à l'axe de face, la borne D2 vaut l'abaque (17,6 p) : 2,3 p d'avancée, contre ≈ 4,3 p pour F ;
     conséquence directe de P4, à juger sur la colonne en A2 ;
   - **rendu** : le tri du peintre de `render_glb.py` laisse des artefacts (dents du noyau, plis) ; feuille et noyau
     de même teinte.
4. **Planche hors du chemin du dépôt** : le test écrit dans `CG_LEAF_SHEET_DIR` (le répertoire courant sinon), pas
   dans un chemin de session codé en dur.

### 11.8 Écarts au plan et à la référence (lot A1b)

*État : lame et lobes (AC-22 à AC-28) retirés au lot A1e (S5), avec `tu_cgarchi_classical_acanthus_leaf.cpp`. Section gardée comme journal.*


Plan de l'architecte (A1b), décisions de l'utilisateur : Q1 option A (Light 760, Heavy 10 392), Q2 pas de
ramification des digitations (dette §2.2). Passe de correction de la relecture de A1 faite en tête (étape 0).

1. **Relecture de A1** : comptes comparés à des littéraux et grilles vérifiées (`the_grids_are_the_registered_ones`) ;
   chaque refus vérifie le fragment de sa garde (`ThrowsWith`) ; chaque double de `LeafSpec`, `LeafBladeSpec`,
   `LeafLobeSpec` et `LeafPlacement` gardé fini et dans un domaine nommé, NaN vu refusé pour chacun ; retournement
   jugé sur le sinus de l'angle (seuil 1e-6), plus sur un signe ; relief borné (AC-9), EnvelopeExcess ≤ τ avec
   `belly = 0` ; narration retirée des commentaires (`classical_acanthus_detail.h`, `classical_acanthus.h:41`,
   `directrixScale`) ; registre complété (exposants 1,5 et 0,9, relèvement 0,4, place sur la moitié haute, borne π/4,
   grilles, seuils de contact par rang : AC-9, AC-14, AC-27, AC-29) ; message « grille retournée ou dégénérée au
   rang … ». Le cisaillement, les dents et les œillets de A1 n'existent plus.
2. **Interface** : `BuildLeafSheet` rend une `LeafSheet` (maillage et plages de pièces) ; `LeafGridsOf` remplace
   `LeafGridOf` ; `from` et `to` en fraction du corps (Y / Y_c) et non de L0 ; `bend` borné à [0, 1[ ; `LeafLayout`,
   `PlanarPiece`, `LeafCarrier`, `LeafPart` internes (espace de noms anonyme) ; noms publics vérifiés absents de
   `src` avant création (garde ODR, contrôle positif : `OrnamentPiece` trouvé).
3. **Géométrie modifiée après mesure** (journal 11.5) : courbure des digitations fonction du rayon (sin φ q²) et non
   du rang ; portée par bissection sur un gabarit lu sur le corps ; ouvertures et axes recalculés (secteurs recouverts
   de ±10°) ; κ Heavy ramené à 0,6 fois la valeur du plan et profil Olive en sin (πu) ; diagonale choisie par
   cellule ; exposant de la pointe aiguë 4 ; retombée en xi absolu ; gouttière (1 − ue²)² effacée sur la crosse ;
   rampe de la couche de la lame alignée sur un rang ; courbure lue aux échantillons des directrices.
4. **Invariants** : I1 à I11 en place ; I3 est vu rouge par le seuil bas du contact (la couche pleine sur la crosse
   abaisse la crête : sur la crosse la normale de la surface porteuse regarde vers le centre de l'arc), non par
   EnvelopeExcess > 0 comme le plan le prévoyait ; I6 et I7 rougissent le test de garde et, garde retirée, le
   maillage (auto-intersections) ; I8 : cas E Light Acute, t = 0,3, refusé à xi = −1,048 (calotte seule).
5. **Durée** : S1 divise la durée de A1 par 10,1 (47,3 → 4,70 ms) ; la feuille A1b complète coûte 22,3 ms en
   Heavy F (gardes et évaluations de S), non profilée : réserve R-perf.
6. **Ressemblance aux références, décrite honnêtement** (planche `planche_feuilles_A1b.png`, session) : la feuille
   se lit maintenant comme une nervure étroite portant des éventails de digitations détachés, obliques, et une
   calotte large qui retombe en champignon sur la crosse, pointue (Acute) ou arrondie (Olive) — la structure de
   cgtrader. Écarts restants : lobes plats et peu saillants (roll ≤ 0,3 p, à peine visibles de face sous l'éclairage
   du rendu), espacés d'une bande vide le long de la nervure, sans recouvrement des folioles ; pédoncule nu entre le
   pied et le premier lobe ; digitations Light grossières (3 rangs) ; E toujours très plat (2,3 p d'avancée) ; tri du
   peintre de `render_glb.py` inchangé (dents du noyau, feuille et noyau de même teinte).
7. **`tools/render_glb.py` non modifié** : la planche se rend avec le script de A1 ; la différenciation des teintes
   feuille / noyau demanderait des primitives séparées dans le GLB de la planche (point ouvert).

### 11.9 Écarts au plan et à la référence (lot A1c)

*État : paume, doigts, drapé et empilement (AC-25, AC-28, AC-30 à AC-33 de A1c) retirés au lot A1e (S5) ; la feuille est un gabarit plan plaqué (§11.10). Section gardée comme journal.*


Plan de l'architecte (A1c), décisions de l'utilisateur : Light 872 (Acute) / 808 (Olive), Np = 2 ; **Olive Light
D = 3** (comptes refigés : 1 096 / 558) ; P4 inchangée (question de E reportée à A2). Passe de correction de la
relecture de A1b faite en tête (étape 1). Chaque écart ci-dessous est mesuré (journal 11.5).

1. **Drapé sur l'étoile du sommet, non sur h_v** (AC-32) : le terme « distance − plus grande arête incidente » du
   plan relevait la lame Light, dont les arêtes transversales atteignent 5,8 p sur la calotte, de 1,29 p jusqu'à la
   crête (garde de courbure à xi = −1,03). La distance est prise entre les triangles incidents et le contour de Q, et
   `top` est le maximum des pics des triangles de Q, pondérés par leur distance à l'étoile (le rayon fixe du plan
   faisait varier `top` par sauts et relevait la lame de 1,5 p au départ de la crosse). Rampe 2 Δ au lieu de 4 Δ (la
   crosse de E, crête à 1,45 p de Y_c, gardait 0,37 p de relèvement).
2. **Flèche de corde ajoutée au drapé** : la lame Light (triangles de 3,7 p de large) passait par sa corde sous S et
   coupait le lobe +1/1 (12 auto-intersections).
3. **Relèvement lissé majorant** (maximum sur disque de rayon Δ, deux moyennes sur Δ / 2) : sans lissage, plis de la
   lame Heavy (rayon 0,128 à 0,196 p, décalages voisins 0,647 / 0,558 p sur 0,15 p). Effacement du relief (1 − C)
   calculé au point et lissé (rayon 2 Δ), q porté par chaque sommet le long des doigts.
4. **Lobe du haut plafonné sous Y_c** (Y_c − pas d'un rang du corps de la lame − (t + 2 Δ)) et **portée bornée au
   départ de crosse de chaque méridien** : les méridiens de bord commencent leur crosse plus bas (crête de E à
   Y = 11,96 au lieu de 12,5 sur l'axe) ; le lobe du haut et le relèvement de la lame s'y posaient sur la crosse
   (rayon 0,87 p sous 0,89 p requis). Heavy : origine haute 0,68 au lieu de 0,74 (le lobe du haut, privé de 1 p,
   se retournait en Olive).
5. **Col évasé dès 0,5 Y_c** (flare 0,5 au lieu de 0,8) : avec le lobe du haut plafonné, le col tombait à
   0,39 hw (F Light Olive). Pédoncule de la lame 0,45 hw (valeur du prototype).
6. **Faces décalées le long de n_S** (et non de n_M) : avec n_M, la face intérieure du lobe 2 de F Heavy Olive
   recoupait sa couture (14 auto-intersections) ; le long de n_S, l'écart t + stackGap du drapé se lit exactement
   d'une face à l'autre. Les coques sont plus minces sur les pentes du relief (t cos θ).
7. **Garde de pli** (AC-33) : critère par arête |Δm| / |Δn_M| ≥ t/2 (condition exacte de non-retournement d'une arête
   décalée de t/2), au lieu de hauteur / (2 sin (θ/2)) ≥ t, qui refusait les triangles minces des pointes Heavy à
   0,09–0,19 p sans défaut de maillage.
8. **Angle minimal à 1° pour les lobes seulement** (relecture M1) : la pointe ronde de la lame Heavy Olive porte une
   aiguille de 0,129° au dernier rang (déjà présente en A1b, dette) ; la lame garde la garde de retournement seule.
9. **Relief du rang 0** : 0,25 à 0,81 p mesurés (différence avec la feuille sans relief), 0,8 p prédit ; le rang 0
   est en grande partie sous le rang 1. Seuil de I14 à 0,2 p (faute roll = cup = 0 : 0 p). Une hausse de roll
   (Olive 0,9 / 1,1) rendait 0,52 à 0,87 p, mais une transition d'effacement plus courte relevait l'empilement au-delà
   de la place libre : valeurs du plan conservées.
10. **Gardes** : κ = 0,65 est hors domaine ]0 ; 0,6] et κ = 0,6 lève la portée ; I6 vu avec β = 0,9 (retournement
    réel). I7 avec upAngle 0,01 seul (innerRadius 0,05 levait d'abord l'angle minimal). I18 avec roll 3 et
    reliefShare 1 (stackGap 2 abaisse le plafond du lobe du haut et lève la portée). I8 : aucune entrée trouvée où un
    décalage de lobe fait refuser la courbure ; la faute « occupation restreinte à la lame » reste verte ; vue rouge :
    « décalage ignoré ». `CheckDomain` est atteinte (contour plein, calotte 1).
11. **Durée** : 35,7 ms en F Heavy Acute (critère dur 22,3 ms non tenu) ; ventilation au journal. Le drapé domine.
12. **Ressemblance aux références, décrite honnêtement** (`planche_feuilles_A1c.png`, session, comparée à A1b et à
    cgtrader) : la feuille est **remplie** — le vide de A1b le long de la nervure a disparu, le col est large, les
    lobes se recouvrent et saillent. Mais elle se lit comme une **touffe hérissée** plus que comme une feuille
    d'acanthe : en Heavy Acute, les dents nombreuses et fines, en relief, partent dans toutes les directions sans la
    hiérarchie de cgtrader (lobes en grappes de folioles autour de nervures marquées, tiges cannelées) ; en Heavy
    Olive, les lobes ronds forment des rosaces. En Light, les lobes sont des facettes planes et anguleuses. La lame est
    un large tronc plat sans nervure visible ; la calotte reste le meilleur élément (Olive Heavy surtout). E reste
    plat (2,3 p). Rendu : tri du peintre inchangé (dents du noyau), feuille et noyau de même teinte
    (`render_glb.py` non modifié : le GLB ne porte pas l'appartenance des triangles).

### 11.10 Bilan du lot A1e (feuille en gabarit plaqué)

Plan de l'architecte A1e (étapes S0 à S7), décisions de l'utilisateur du 2026-11-22 et du 2026-11-23. La feuille
F ou E est un gabarit plan Olive (`data/acanthus_leaves.json`, AC-34 à AC-38) plaqué sur une porteuse
(`LeafBend` : bande de tige, corps en Bézier, bec en arc borné par le cône, gouttière, dôme ; AC-9, AC-14, AC-40,
AC-43), avec plafond bilatéral du relief (AC-39). Le générateur n'est pas branché : il le sera au lot A2.

**Décisions de l'utilisateur.** Olive seul style, Acute retiré (AC-1, AC-35) ; borne des feuilles = cône seul ;
gouttière en variante B décalée (AC-9) ; modèle actuel gardé (Bézier + arc, côte à 1,40 p), corps hybride
S4bis abandonné (dette §2.2) ; les planches de Vignole priment sur le dossier `tuto`.

**Invariants et tests** (`tu_cgarchi_classical_acanthus_{template,bend,sheet}.cpp` : 23 tests actifs, 5 désactivés) :

| Invariant | Mesure | Test |
|---|---|---|
| J1, J2 | comptes de la coque (AC-29) ; coque valide, une composante (indices et soudure), volume > 0 | `the_sheet_is_a_sound_shell` |
| J3, J4, J5, J16′, J17, J19 | bord sur le contour dessiné (2e-6) ; crête de S à hTop (±0,05) ; jeu ≥ c ; contact de la borne dans [−0,35 ; 0] p ; X = u hw (1e-3 p), corps et bec dans ±0,10 et ±0,15 du nominal ; bande de E au pied hStem | `the_sheet_follows_its_template_and_carrier` |
| J6 | relief exagéré (H × 4 sur y ∈ [0,3 ; 0,8], Hb × 10) sous la borne, plafond mordant sur H et sur Hb | `the_relief_stays_under_the_bound_when_exaggerated` |
| J7, J8 | marches ≥ 0,15 p (≥ 80 %) ; couches ordonnées (≥ 80 %, probant en Olive : 19 et 44 échantillons) | `the_steps_and_layers_hold_on_the_leaf_alone` |
| J9 | garde de courbure sur reliefScale · H | `the_curvature_guard_reads_the_scaled_relief`, `the_guards_are_seen` |
| J11 | **qualité du maillage** (pas d'aiguilles), et non forme : angle 3D minimal des faces ≥ 1° (Light), 3° (Heavy) ; faute de référence 32b (arc du gabarit comprimé × 0,1 au plaquage : 0,63° / 1,83°) | `the_faces_keep_their_angles` |
| J12′ (a), (b) | dôme de calotte ; demi-largeur de S dans la bande de crête | `the_dome_lowers_the_cap_edges_and_stays_inside`, `the_cap_keeps_its_width_at_the_crest` |
| J13′, J18 | flexion du bec, tangente finale, R_S ; cotes corrigées de la pl. XXV (±0,6 / ±0,5 p) | `the_axis_meridian_meets_plate_XXV` |
| J14, J15 | symétrie miroir ; Hb gardé sur l'axe du bec | `the_sheet_is_mirror_symmetric`, `the_back_keeps_its_relief_on_the_cap` |
| J21′ | ordre radial : là où E (θ = 0) et F (±π/8) couvrent la même cellule (θ, h), face extérieure de E ≤ face intérieure de F − c ; marge mesurée +0,77 p | `the_middle_leaf_passes_behind_the_lower_leaves` |
| gardes | chargeur, porteuse, plaquage, pli (synthétique), plafond côté noyau, portée bornée de la recherche de place | `every_guard_is_seen`, `the_free_room_is_bounded`, `the_indices_reliefs_and_edges_are_guarded`, `the_guards_are_seen`, `the_sheet_guards_are_seen`, `the_fold_guard_refuses_a_folded_face`, `the_back_relief_stops_at_the_core` |
| J20, J20b | face − noyau et épaisseur de lame contre la pl. XXV : **journalisés seulement** (dette S4bis) | `DISABLED_blade_against_plate_XXV` |

**Fautes injectées dans le produit** (`CG_LEAF_FAULT`, posées et retirées par script, sources restaurées à
l'octet près, aucun `FAULTINJ` restant) : fautes 1 à 39, faute 0 verte (30 tests du filtre `*acanthus*`) ;
**toutes rouges**. 13 rougissent leur seul test attendu (1, 2, 12, 16, 17, 18, 20, 22, 25, 26, 31, 38, 39) ; 26
rougissent aussi des voisins, le plus souvent parce que la porteuse refuse la construction (fautes 4, 7, 9, 11, 13,
14, 24, 27 : 9 à 17 tests). Faute 32 (arc × 0,3 au plaquage) : rougit J16′ (contact) et J12′ (b) (bande de crête
vide), **J11 non concerné** ; faute 32b (= 36, arc × 0,1) : J11 rouge. Faute 38 (portée de `Free` coupée de
moitié) : J7 rouge. Faute 39 (portée de la recherche de place non bornée) : `the_free_room_is_bounded` rouge.

**Durées** (`DISABLED_leaf_build_time`, Release MSVC, médianes de 21, 3 séries) :

| Cas | Avant S7 | Après S7 |
|---|---|---|
| F Light | 7,87 ms | 0,36 ms |
| F Heavy | 44,1 ms | **1,79 à 1,88 ms** (critère 15 ms tenu) |
| E Light | 10,8 ms | 0,42 à 0,49 ms |
| E Heavy | 61,8 ms | 2,11 à 2,20 ms |

Ventilation par sonde (F Heavy, couverture 99 % avant, 86 % après) : avant, `Free` (H) 6,5 ms et `Free` (Hb) +
`CoreRoom` 36,2 ms sur 44,1 ; après, sommets (`At` × 5, `BackNormal`) 0,61, `Free` (H) 0,64, `Free` (Hb) +
`CoreRoom` 0,18, assemblage 0,31, garde de courbure 0,10 ms sur 2,2. Correctif : la recherche s'arrête à la valeur
qui peut encore plafonner (portée bornée à 40 p, `kMaxReach`), et le relief voulu est rendu tel quel quand la place
atteint la portée ; `planar` (H et Hb en double) identique au bit près au calcul à portée 40 sur les quatre feuilles,
sommets des maillages identiques à l'octet.

**Prédictions révisées.** J11 en Light ≥ 4° (mesuré 4,70° et 6,40°) ; J16′ tenu (−0,27 à −0,28 p) ; durée
tenue après correction. **Causes des écarts** (journal du plan A1e) : hypothèse non mesurée (hw_E isotrope, P4
contre la pl. XXV) ; instrument biaisé (prédiction de durée sans `Free`, J7 à J8 en Acute sur 1 et 8 échantillons,
J21 comparant l'axe de E à l'intrados de F) ; lecture contestée de la planche (trait A ou B de E, tranché : B).

**Contrôles finaux.** GLB de `opus --colonne-corinthienne` identique (sha256 `6a852fdc…`) ; /W4 sans
avertissement ; TU complet en deux moitiés (journal du plan A1e et rapport de S7).

### 11.11 Bilan du lot A2 (rangs F et E posés, modèle v0 retiré, défaut Light)

Plan de l'architecte A2 (étapes S0 à S7), décisions de l'utilisateur du 2026-11-24 : **Q1 (a)** v0 retiré en entier,
chapiteau réduit à F et E jusqu'à A3 ; **Q2 (B)** défaut Light porté par les formes, structure et JSON à None ;
**Q3** « Ornament envelope » ne coupe pas l'acanthe (AC-50 non créée, dette) ; **Q4** cibles de triangles révisées ;
**Q5** couverture F + E dans [0,60 ; 0,80], 85 % reporté à A3. Planches S4 validées par l'utilisateur.

**Construction.** `BuildCorinthianOrnament` pose 8 F à θ = π/8 + kπ/4 et 8 E à θ = kπ/4 (AC-45) : une feuille
représentante par rang (`BuildLeafShell`, la construction de `BuildLeafSheet` en double), copiée par rotation autour de
l'axe avant l'unique conversion en float. Pièces : 8 F puis 8 E, une coque chacune (AC-46). Pieds relevés sur
l'astragale (AC-49 : F à 0,70 p, bande de E à c), avec une garde dans `BuildLeafSheet`. `EnvelopeExcess` par pièce
mesure F et E contre le cône (AC-48). Retirés : `MakeLeaflet`, `MakeRachis`, `MakeStem`,
`AcanthusOptions::segmentsPerLeaf`, `kMaxSegmentsPerLeaf` (AC-18 et AC-20 caduques). Défaut « Capital ornaments »
Light dans les constructeurs de `ParameterizedClassicalColumn` et `ParameterizedClassicalColonnade` (AC-47).

**Invariants de l'assemblage** (`tu_cgarchi_classical_acanthus_ornament.cpp`, Light et Heavy, échelles 1 et 2 ;
K1 et K14 dans `tu_cgarchi_classical_acanthus.cpp`) :

| Invariant | Mesure **[M]** | Test |
|---|---|---|
| K1 | ornement 29 696 / 14 880 (Light), 170 496 / 85 280 (Heavy) ; 8 F puis 8 E, comptes de coque par pièce | `the_ornament_is_one_mesh_tiled_by_its_pieces` |
| K2 | azimut du barycentre de chaque pièce sur l'axe des planches à 1,2e-10 rad | `the_leaves_stand_on_the_phase_of_the_plates` |
| K3 | copie = construction à θ_k : 2,8e-16 p (flottants identiques) | `each_copy_is_the_leaf_built_at_its_place` |
| K4 | symétrie D8 sans les indices : réflexion 6,6e-15 p, rotation de π/4 2,0e-6 / 2,1e-6 p | `the_ornament_has_the_symmetry_of_the_octagon` |
| K5, K6 | jeu 0,05000 p ; borne des feuilles −0,2765 / −0,2741 p | `the_leaves_keep_off_the_bell_and_under_the_cone` |
| K7 | E derrière F sur les 8 paires : +0,8762 / +0,7650 p | `each_middle_leaf_passes_behind_its_lower_neighbours` |
| K8 | couverture h ∈ [1, 23] : 0,7097 / 0,7116 | `the_two_ranks_cover_most_of_the_bell` |
| K9 | ornement valide, 16 composantes, test non tronqué | `the_ornament_is_a_sound_mesh` |
| K10 | colonne valide, 17 composantes, défaut et complet, Light et Heavy (enveloppe d'ornement décochée) | `the_column_and_its_leaves_are_a_sound_mesh` |
| K12 | pointes F 22,14 / 22,18 p (22,3 ± 0,6), E 26,94 / 27,00 p ([26,4 ; 28,9]) | `the_tips_lie_on_the_circles_of_the_plan` |
| K14 | structure à None, colonne et colonnade neuves à Light | `capital_ornaments_and_volutes_carry_the_corinthian_ornament` |

Les tests de la feuille (J1 à J21′) restent calculés sur les représentants (F à π/8, E à 0) ; J19 mesure en plus le
plancher (plus bas sommet F 0,1685 p, E 0,0500 p) et `the_sheet_guards_are_seen` voit la garde du pied. K2, K3 et K7
lisent des axes littéraux (`PlateAxis`, test), pas `RankTheta` (produit).

**Fautes injectées dans le produit** (`CG_LEAF_FAULT`, script `faults_a2s5.py` du scratchpad, sources restaurées et
vérifiées par sha256, aucun `FAULTINJ` restant) : 1 à 39 rejouées, 40 à 53 (sans 49) et variantes, **toutes
rouges** ; faute 0 verte. Deux prédictions du plan réfutées (§11.5) : la faute 44 ne peut pas faire monter K6 (la
variante 144 le fait), la faute 47 est vue par K3 et non par K12 (la variante 147 rougit K12). Avant durcissement, 41
et 143 (faute dans `RankTheta`) passaient K2 et K3, qui lisaient `RankTheta` : instruments corrigés.

**Tests hors acanthe** (liste fermée du plan, T1 à T9) : T1 (surface des paramètres, deux littéraux), T2 (48
empreintes mesurées à None, **inchangées** ; 8 lignes corinthiennes à Light en comptes), T3 et T9 (colonne de
référence aux ornements de la colonnade), T4 (17 composantes au corinthien complet) ; T5 à T8 verts sans
modification.

**Mesures.**

| Grandeur | Valeur **[M]** |
|---|---|
| `BuildClassicalColumnMesh` corinthien (Release, médianes de 3 séries de 21) | Light 3,23 à 3,32 ms, Heavy 15,9 à 16,5 ms (critères 15 / 40 ms), tour seul 0,54 ms |
| Colonnade corinthienne, 4 colonnes, défaut | 176 292 triangles ; bannière de 300 000 faces vers 7 colonnes (Light), dès 2 (Heavy) **[E]** |
| GLB de `opus --colonne-corinthienne` | 43 344 faces, 21 706 sommets, 41 394 sommets exportés ; sha256 `1019d12cb2d610a4e4ee1c0eb540f37586c750e0ac584e38804b14897bc3fbc1`, identique sur deux exécutions : **empreinte de non-régression pour A3** (remplace `6a852fdc…`) |

**Planches** (S4, validées par l'utilisateur) : rendu du chapiteau, élévation de face sur la pl. XXVI, section par
θ = π/4 sur le profil de la pl. XXV, plan sur le plan de la pl. XXV (cercles des pointes, abaque). Elles montrent F et
E seuls : D, caulicoles, volutes et fleuron manquent par construction jusqu'à A5.

**Écarts au plan** : AC-49 ajoutée après la mesure de S0 (étape S1b) ; T9 ajouté à la liste fermée ; K12 de E
révisé, mesure gardée ; littéral J17 de F gardé à 0,84 (référence du décalque ; mesure 0,787, dans la plage) ;
prédictions réfutées au §11.5 (J21′ qui ne baisse pas, K12 de E, fautes 44 et 47, durées « tour + »).

**Contrôles finaux** (code du commit `87b5a18`, Release MSVC) : /W4 sans avertissement sur les 8 sources touchées ;
suite `*acanthus*` 38 / 38 ; TU complet en deux moitiés, un processus à la fois : `TEST_cgarchi*` 1 012 verts, le
reste 2 030 verts et 12 sautés (ImportScad, OpenSCAD absent), 0 rouge ; `opus --colonne-corinthienne` reconstruit :
43 344 faces, 21 706 sommets, 41 394 exportés, sha256 `1019d12c…c3fbc1` inchangé. Les fautes ne sont pas rejouées
en S6 : S5 les a rejouées sur ce même code (sha256 des trois sources identiques à `87b5a18`).

**Revue S7** (code-reviewer) : un constat moyen et deux faibles, tous dans les tests, aucun dans le produit.
`PiecesOf` et `#include <map>` retirés de `tu_cgarchi_classical_acanthus.cpp` (fonction sans appelant, signalée par
GCC `-Wunused-function`) ; K7 vérifie le rang des pièces qu'il rastérise (`ASSERT_EQ` sur `kind`, F et E) ; tampons
`float p[3]` initialisés à zéro (`acanthus_test_support.h`, `Parts`). /W4 sans avertissement sur les trois unités
recompilées, `*acanthus*` 38 / 38. Faute « ordre des pièces inversé » (E avant F dans `BuildCorinthianOrnament`) :
K7 rouge sur l'assertion de rang (ligne 266), K1 rouge aussi ; source restaurée à l'octet (sha256).

**Suite** : reprise de S4bis (dette, corps de F et E) sur le chapiteau posé, puis A3 (caulicoles, calices D,
couverture ≥ 85 %). L'empreinte `1019d12c…` changera avec S4bis : c'est attendu.

### 11.12 Bilan de S4bis-2 (rampes C² et garde de courbure en forme close)

Base : `445f578`. Le prototype hors dépôt (P0 à P2, §11.5) s'arrête en P2 : aucune côte ne tient J20 de F, J18 et la
garde avec un corps qui longe la corbeille. **Décision de l'utilisateur (2026-11-25)** : dette S4bis gardée, causes
consignées (`debt_cgarchi.md` §2.2) ; seul le levier L1 est intégré, sans changer le corps.

**Intégration.**
- Rampes du dôme et de la gouttière en smootherstep (`Smootherstep`, valeur et deux dérivées) ; effacement du relief
  du gabarit en smootherstep (`_smootherstep`, générateur) ; `acanthus_leaves.json` recompilé (`--write`, `--check`
  conforme), comptes et maillage inchangés, H changé dans la fenêtre [1 ; 1,2] seulement : sha256 `cb65fd87…bc325`
  (avant `4b04380d…f1859`).
- Garde de courbure en forme close (`LeafBend::CurvatureMargin`, `LeafCurvatureMargin`), registre AC-8 amendé ; coins
  aux raccords vus comme rayon nul (la jonction bande-corps de E penchée reste refusée à y = 0).
- `LeafSpec::gutterFadeShift` retiré (AC-9) : sans décalage, la marge de F passe de +0,161 à +0,188 p.

**Mesures** **[M]** : marges F +0,188 / +0,188 p, E +0,536 / +0,535 p (Light / Heavy), contre +0,071 en forme close
et +0,097 aux trois sondes avant ; identiques à 1e-6 entre 4 et 64 échantillons ; g max 0,463 à 0,526 ; écart de forme
contre `445f578` nul hors des fenêtres de rampe, 0,068 p (F) et 0,126 p (E) dedans ; K7 +0,855 / +0,753 p ; K8,
J7, J17, J18, J20 inchangés.

**Tests** (`tu_cgarchi_classical_acanthus_bend.cpp`) : `the_curvature_guard_keeps_its_margin_at_any_sampling` (marge
≥ 0,1 p, même marge à 4, 16 et 64 échantillons), `the_curvature_guard_matches_three_close_points` (courbure en forme
close, `LeafBend::MeridianCurvature`, contre le cercle par trois points à 1e-4 sur les 8 méridiens de la garde, y au pas
de 1e-3 hors de ±2 pas des coupures, écart relatif ≤ 1e-3 : mesuré 5,0e-4 pour F, 2,3e-4 pour E, 7 989 points), `the_curvature_guard_requires_the_relief` (marge sans relief > marge
avec relief + 0,05 p) ; `DISABLED_gutter_margin` rend la marge et g max, sans décalage ; garde de `gutterFadeShift`
retirée de `the_guards_are_seen`.

**Fautes injectées dans le produit** (`CG_LEAF_FAULT`, `faults_s4b2.py` dérivé de `faults_a2s5.py`, ancres de 11, 17,
19, 21, 22, 26, 30 et 31 adaptées) : 60 (rampes en smoothstep), 61 (garde des méridiens neutralisée), 62 (garde sans
H), 63 (section dorée retirée), 64 (dérivée seconde du dôme omise), 65 (courbure de la Bézier du corps × 1,1 ;
verte contre la comparaison au seul point le plus serré, ajoutée à la revue) **rouges**, chacune sur son test (65 rougit aussi
`the_relief_stays_under_the_bound_when_exaggerated`, dont le relief exagéré passe par la garde) ; 1 à 39 et 40 à 154
rejouées :
toutes rouges sauf 44 et 47, qui manquent K6 et K12 comme en A2 (variantes 144 et 147 rouges, §11.5) ; faute 0 verte ;
sources restaurées à l'octet (sha256), aucun `FAULTINJ`.

**GLB** de `opus --colonne-corinthienne` (opus reconstruit par MSBuild) : 43 344 faces, 21 706 sommets (inchangés),
41 434 sommets exportés (41 394 avant : les normales à seuil se découpent autrement dans les fenêtres) ; sha256
`46e4b3736f8093495c45f800cf517f2ff956a71df6070a74793d57888199d608`, identique sur deux exécutions : **nouvelle
empreinte de non-régression** (remplace `1019d12c…`).

**Contrôles** (Release MSVC) : /W4 sans avertissement (cgarchi et TU) ; suite `*acanthus*` 41 / 41 ; TU complet en
deux moitiés, un processus à la fois : `TEST_cgarchi*` 1 015 verts, le reste 2 030 verts et 12 sautés (ImportScad,
OpenSCAD absent), 0 rouge ; garde ODR sur les nouveaux noms (`LeafCurvatureMargin`, `CurvatureMargin`,
`kCurvatureSamples`, `MeridianCurvature`, `Smootherstep`, `Ramp`, `PlaneJet`, `MeridianJet`, `CoreSlope`, `ReliefAt`, `Jet`) : aucune
occurrence hors de `classical_acanthus.{h,cpp}` et `classical_acanthus_detail.h` (contrôle positif : `LeafBend`
trouvé dans les mêmes fichiers seulement).

### 11.13 Bilan du lot A3 (caulicoles et feuilles D)

Base : `2b53a06`. Plan de l'architecte (scratchpad `a3_plan.md`), décisions de l'utilisateur du 2026-11-25 : Q1
(tiges droites), Q2 (gabarit dérivé d'Olive), Q6 (découpage A3a / A3b), Q3″ (D posées sur leurs cotes, garde sur le
cône), Q5 (V-1 tracé, critère en élévation), Q6′ / Q6″ (cisaillement, angle déduit), Q8 (D derrière E, corps tenu),
Q9′ (budget D 1 300 / 2 996), Q10 (K24′ rouge connu).

**Relevé quadrillé** (pl. XXV et XXVI, calages A1e et A2) : axe des tiges à |x| 6,48 p (22,5° à r_C 16,9) ; Ø 2,0 ± 0,3 ;
bague 15,3 à 17,8 à deux cordons ; tige visible dès 11,2 / 12,25 ; deux D par caulicole, pied commun au haut de la
bague, D intérieur (pointe r 19,0, h 25,5) et D extérieur (crête h 28, saillie r 27,4, pointe r 26,2) ; D derrière E sous
h 24. V-1 tracé à la main : 0,0557 (deux tracés, dispersion 0,011).

**Intégration.**
- Tiges (A3a) : `CauliculusSpec`, `DefaultCauliculusSpec`, `CauliculusRadius`, `CauliculusRings`,
  `BuildCauliculusSheet` (AC-50, AC-52, AC-56) ; `RankTheta` étendu aux tiges.
- Gabarit `OliveMinor` (AC-1, AC-55) : générateur étendu aux styles dérivés (`derivedFrom`), bloc Olive de
  `acanthus_leaves.json` identique à l'octet (sha256 `1a6b979b…`), `--check` conforme.
- Porteuse : `footOffset` (AC-54), pose explicite du bec (AC-53), axe cisaillé (AC-57), corps tenu (AC-58), à leurs
  défauts sans effet sur F et E (préfixe de l'ornement identique à l'octet, FNV-1a, Light et Heavy, échelles 1 et 2).
- Feuilles D (A3b) : `MinorLeafSide`, `DefaultMinorLeafPlacement (env, côté, θ_C)`, `DefaultMinorLeafSpec` ; quatre
  représentantes copiées par π/2 (AC-45), ordre des pièces AC-46 ; `EnvelopeExcess` par pièce : D contre le cône
  (AC-48) ; `VisibleBellFraction` (AC-7).

**Mesures** (Light / Heavy) **[M]** : ornement 56 256 / 243 008 triangles, 28 208 / 121 584 sommets ; colonne 69 904 /
256 656 ; tiges : axe 8,9e-9 rad, r_C 16,9, Ø 1,96 / 2,00, jeu au noyau 0,35, abaque −0,91, jour C/E 0,81 ; D : pointes
à 2,8e-8 rad de leur azimut, jeu au noyau 0,45, cône −2,63, abaque +1,68 (journal), D derrière E +2,55 / +2,10, D / tige
0,059, D intérieur à 1,18° de l'axe de face, D extérieur à 0,86° de la corne, gardes +0,143 / +0,108 ; symétrie D4 ≤
8,3e-15 p ; couverture angulaire 0,876 / 0,880 ; corbeille visible en élévation 0,119 / 0,117 contre V-1 0,0557 (rouge
connu) ; durée de la colonne 13,8 à 14,1 ms (Light), 33,7 à 34,2 ms (Heavy), médianes de 3 × 21, Release.

**Tests.** `tu_cgarchi_classical_acanthus_cauliculus.cpp` (K15 à K21, K24, K25, K28 des tiges),
`tu_cgarchi_classical_acanthus_minor.cpp` (pied, pose explicite, cisaillement et corps tenu, AC-53, AC-54, AC-57, AC-58 ; cotes des D, gardes de pose, extensions sans effet sur F et E),
`tu_cgarchi_classical_acanthus_minor_ornament.cpp` (K16 à K21 et K24 des D) ; tests existants aux nouveaux comptes (K1,
K9, K10, T2, T4, gabarit).

**Fautes injectées** : 70 à 104 rouges, chacune sur son invariant ; 1 à 65 rejouées ; 44 et 47 manquent comme en A2 ;
6 détecteurs faibles corrigés et prouvés (S6) ; faute 0 verte ; sources restaurées à l'octet, aucun `FAULTINJ`.

**GLB** de `opus --colonne-corinthienne` : Light 69 904 faces, sha256
`070c48f6bcc4c5dcfc08315337be9d0073ce8e9cfd011cf0ebab88ce79bc015e` ; Heavy (`--ornements heavy`) 256 656 faces,
`f612024a64fe8e350a17c1b4d1b6a218b4a03e86f08618567feb83bd0c060b66` ; identiques sur deux exécutions : **nouvelles
empreintes de non-régression** (remplacent `46e4b373…`).

**Contrôles** (Release MSVC) : /W4 sans avertissement ; suite `*acanthus*` verte ; TU complet en deux moitiés, un
processus à la fois, 0 rouge (12 sautés, ImportScad) ; garde ODR sur les nouveaux noms (contrôle positif `BuildLeafShell`).

**Dette** (`debt_cgarchi.md` §2.2) : escalier du D extérieur lié à S4bis ; K24′ rouge connu ; relief du D extérieur 0,30 ;
α déduit contre D-7 ; bec du D extérieur en arc ; contour de l'abaque gravé (A4) ; budget de la colonnade ; générateur
sans test propre.

### 11.14 Bilan du lot A4 (volutes et hélices)

Base : `3fbfd84`. Plan de l'architecte (scratchpad `a4_plan.md`), décisions de l'utilisateur du 2026-11-25 et 26 :
- Q1 : découpage A4-0, A4a, A4b ;
- Q2 : 12 centres à l'échelle, sans bascule ;
- Q5 : bannière de la colonnade dépassée, révision en A6 ;
- Q8 : optimisation S1′ avant A4a ;
- Q4 (a) : contour de l'abaque des données ;
- Q6 / Q6′ : volutes en paire disjointe, dièdre de 45°, plans à 22,5° ; hélices à 45° ;
- Q7 / Q7′ : tiges nées cachées, à h 24,5 (volutes) ;
- écarts actés : œil des volutes à h 31,5, largeur bornée par l'œil, θ_b 28,5° ;
- correctif du pied : aminci à 0,8 b, pied « en partie caché » inscrit en dette.

**Relevé S0** (pl. XXV et XXVI) **[M]a** :
- calage du plan sur les faces coupées des cornes (A1e ratait le contrôle C-U de 1,1 à 1,5 p) ;
- deux contours de l'abaque (arc du compas = données ; bord hachuré 2 p plus saillant au milieu de la face) ;
- volute : 12 centres × 0,470, résidus ≤ 0,34 p, 3 tours ;
- hélices : V 5,5 à 6,0, plan tourné de 30 à 52° sur la face ;
- tiges visibles dès h 24,5 (volutes) et 25,5 (hélices).

**S1′, optimisation à sorties identiques.**
- `DefaultMinorLeafPlacements` : les quatre poses des D calculées une fois, en cache sous verrou ; clé = contenu de l'enveloppe en parties (AC-53).
- `OrnamentAssembly::AddRotated` (copie tournée écrite dans les tampons) et `Reserve` ; `detail::BuildCorinthianOrnamentGeometry`.
- `BuildClassicalColumnMesh` : l'ornement est versé dans le tour sans maillage intermédiaire.
- Preuve : empreintes FNV-1a de l'ornement (Light, Heavy, échelles 1 et 2, maillage et pièces) et de la colonne (niveaux 0 à 2, modules 1 et 2, enveloppe cochée ou non) identiques avant et après ; GLB `070c48f6…` / `f612024a…` inchangés.
- Colonne Light 13,2 → 8,5 ms, Heavy 32,4 → 19,2 ms **[M]**.

**Intégration** (`classical_acanthus_volute.cpp`, nouveau) :
- `VoluteSpec`, `DefaultVoluteSpec`, `DefaultHelixSpec`, `VolutePiece`, `BuildVoluteRibbon` (AC-59 à AC-63) ;
- 8 volutes puis 8 hélices après les 16 D (AC-46) ;
- `EnvelopeExcess` par pièce : V et H contre le cône (AC-48) ;
- gardes de la construction : domaine (parts du bourrelet et de la rainure de somme 1, reliefs finis), largeur < rayon local − c, moins d'un tour de ruban, courbure et torsion de la tige, rampe d'épaisseur pas plus longue que la tige, sommets au-dessus de l'abaque − c, hors du cône, dans la corbeille ;
- revue S7 : test du cache durci (point fixe direct à l'échelle 2, enveloppe invalide après une mise en cache), boîte de K40 à 4 ulp float ; chaque garde et chaque test nouveaux vus rouges sur faute ; sorties identiques (K40, GLB).

**Mesures** (Light / Heavy) **[M]** :
- **ornement 70 576 / 299 888 triangles**, 35 432 / 150 088 sommets, 72 composantes ; **colonne 84 224 / 313 536**, 73 composantes ;
- **volutes** : œil ≤ 1,8e-7 p de ses cotes, plan ≤ 2,6e-7 rad ; H 6,58, V 7,52 p ; vue de face 0,98 ; largeurs 1,430 / 0,697 à 0,700 / 0,532 à 0,558 ; jour entre tours 0,28 / 0,22 ; jeu au noyau 0,0506 ; cône −0,077 / −0,072, abaque +3,65 (journal) ; tige derrière le D extérieur +0,54 / +0,35 ; jour au D extérieur 0,122 / 0,125 ; paire 1,97 p ; jonction tige / spirale 0,23° ;
- **pied des volutes caché à 0,649** (h 24,5 à 24,6), 0 à h 24,75 et 25,0 ;
- **hélices** : œil ≤ 4,6e-6 p ; V 5,76 ; vue de face 0,85 ; largeurs 0,900 / 0,438 à 0,440 / 0,380 ; jour entre tours 0,33 / 0,29 ; jeu au noyau 0,071 ; cône −6,6, abaque +6,3 (journal) ; tige derrière le D intérieur +0,15 / +0,12 ; jour sur l'axe de face 2,12 p ;
- symétrie D4 0 p, rotation de π/4 > 7,9 p ;
- **K42** : colonne 9,15 à 9,22 ms (**≈ 9,2**, Light), 20,34 à 20,93 ms (**≈ 20,5**, Heavy), médianes de 3 × 21, Release.

**Défaut trouvé en S5 et correction.**
- Durci pour mesurer sommets contre triangles (`acanthus_test::PieceGap`), K36 a vu le pied de la tige de la volute à **0,0056 p** du D extérieur en Heavy (0,064 p en Light). L'instrument précédent, sommets contre sommets, voyait ≥ 0,072 p.
- Reculer le pied était impossible : sa face intérieure est déjà à noyau + c.
- Déplacer θ_b ne donnait le jour c que sous 25°, avec un pied découvert.
- Décision de l'utilisateur : **amincir le pied** (0,8 b au pied, face intérieure fixe, rampe smootherstep sur 1,0 p), et inscrire en dette la part visible.
- Jour 0,0640 / 0,0056 → **0,122 / 0,125 p** ; part cachée 0,636 → 0,649, figée par le test `the_stem_foot_is_partly_hidden_behind_the_leaves` (borne 0,63 ; faute θ_b 25° : 0,027, rouge).

**Tests.**
- `tu_cgarchi_classical_acanthus_volute.cpp` et `tu_cgarchi_classical_acanthus_helix.cpp` : K29 à K44 ;
- test du cache dans `tu_cgarchi_classical_acanthus_minor.cpp` ;
- tests existants aux nouveaux comptes (K1, K9, K10, T2, T4).
- K40 (préfixe inchangé) : topologie et somme pondérée portables, octets sous MSVC seulement.

**Fautes injectées** (S2, S3, S5, test-engineer) :
- fautes d'A4 110 à 142 et du correctif 210 à 215, toutes rouges, sauf 4126 (verrou du cache retiré : course de données, invisible sans TSan) ;
- les fautes arrêtées par une garde du produit ont leurs variantes qui atteignent l'assertion ;
- **147 rejouées, 144 rouges** ; 44 (K6) et 47 (K12) manquent comme en A3, 46 (K8) est arrêtée par la taille de l'ornement (variante 1046 rouge), 96 par la garde de convergence (variante 1096 rouge sur X3).
- **Détecteurs faibles corrigés**, chacun vu rouge : clé du cache ; colonne = tour + ornement à l'octet ; cassure de la spirale du ruban (K33) ; K40 portable ; K36 sommets contre triangles ; couverture du pied.
- Sources restaurées à l'octet, aucun `FAULTINJ`.

**GLB** de `opus --colonne-corinthienne` :
- Light 84 224 faces, sha256 `5c464d3d41aa1dac858460a21ac832c31b97f48b1c3386ea8947fb553acf1f77` ;
- Heavy 313 536 faces, `06faba3e6bb003fa49b03c451eb45c748e0e6d5f20cebfdfe1a2725d8cade35e` ;
- identiques sur deux exécutions : **nouvelles empreintes de non-régression** (remplacent `070c48f6…` / `f612024a…`).

**Contrôles** (Release MSVC) :
- /W4 sans avertissement, appliqué aux tests par `_CL_` (D9025 : /W1 remplacé) ;
- TU par tranches, un processus à la fois : 3 089 verts, 12 ignorés, 0 rouge ;
- garde ODR sur les nouveaux noms (contrôle positif `BuildLeafShell`).

**Dette** (`debt_cgarchi.md` §2.2) :
- second contour de l'abaque ;
- exceptions à AC-2 (hélices +6,3 p, volutes +3,65 p) ;
- valeurs de faisabilité de la tige (θ_b, coude, tangentes, `stemLift`) ;
- œil des hélices à x 21,5 ;
- pied des volutes visible de h 24,75 à 25 ;
- faute 4126 sans TSan ;
- fautes 44 et 47 ;
- conventions d'A4 sans appui des planches.

### 11.15 Bilan du lot A5 (fleuron G)

Base : `5729265` (parent stable `5ce6d8c`). Plan de l'architecte (scratchpad `a5_plan.md`), décisions de l'utilisateur
du 2026-11-26 et 27 :
- Q-G0 (a) : largeur au relevé, « 4 » du JSON non lu (AC-70) ;
- Q-G1 : 16 pétales en une couronne ; Q-G2 : cercle Ø 7,9 ; Q-G3 : dos qui suit le tour ; Q-G5 : languette absente ;
- Q-G4 (a′) : tige née à h 24,0, cachée derrière la pointe de E ; cornet 0,94 au haut ;
- dos calé sur la corde réelle du tour, au réglage de la colonne (AC-67, AC-74) ;
- K50 : flèche du bouton 0,6 → 0,45, borne 4,56 et face 21,66 inchangées ;
- S1bis : pétales en tambour de 2,35 et noyau derrière le bouton (une coque), cuillers larges ;
- Q-G6 : budget +8 192 / +27 648 (rosaces) accepté ; Q-G7 : A5a puis A5b ; Q-G8 : saillie brute 21,66 ;
- S2 : mode enveloppe (rosace du mode noyau, AC-73) et K5 limité à F…H (AC-48).

**Relevé S0** (pl. XXV et XXVI) **[M]a** :
- fleuron Ø 7,70 à 7,96 (vertical), 8,37 à 8,83 (horizontal), centre h 37,77 à 38,43, face avant r 21,69 ;
- bouton Ø 2,50, 1,0 p sous le milieu de la fleur ; 16 ± 2 pétales (déroulé polaire) ;
- tige 0,69, visible dès h 24,86 ; cornet de h 30,78 à 33,02, 0,94 au haut (V sombre) ;
- calages : pl. XXV tournée de 1,0°, abaque gravé à 12,4 px/p en vertical ;
- sonde S0-3 : couloir de la tige en élévation seulement, jour ≥ 1 p en volume, tige cachée par E jusqu'à h 24,3 / 24,45.

**Construction** (`classical_acanthus_fleuron.cpp`, `classical_acanthus_detail.h`) :
- `ReadCapitalTour`, `TourDepthMax` : tour lu comme `LatheColumnProfile` le maille ;
- `BuildFleuron` : rosace (grille polaire, cuillers, bouton, noyau conforme au tour, tambour), puis tige
  (`AppendStem`) ; symétrie y → −y par construction, sommets et triangles ;
- assemblage : 4 G après les H, représentante θ = 0 copiée par rotation de k π/2 ;
  `AcanthusOptions::radialSegments`, transmis par `BuildClassicalColumnMesh`.

**Mesures** :
- pièce G 2 164 / 1 086 (Light), 7 484 / 3 746 (Heavy) ; ornement 79 232 / 329 824 ; **colonne 92 880 / 343 472** faces ;
- débord sur B 4,51 ; épaisseur minimale 0,885 / 0,936 ; tambour 2,35 ;
- jour au tour de la colonne 0,0500 à 8, 48, 256 secteurs et cannelé ; jour des G aux autres pièces 1,20 / 1,21 (D24) ;
- K42 : 10,8 à 10,9 ms (Light), 23,6 à 23,7 ms (Heavy) en S3 ; **12,3 à 12,6 / 26,9 à 27,2 ms** après S7 (dos de la rosace lu sur
  les deux boîtes symétriques) ;
- préfixe F à H : identique à la colonne de `5729265` (K56 ; GLB : 252 672 / 940 608 coins de faces à l'octet).

**GLB** (`opus --colonne-corinthienne`, `--ornements heavy`) :
- Light 92 880 faces, `958b9425899f2fe4df5e92ee26f3b172c6bb353854e7293368956204b11207b0` ;
- Heavy 343 472 faces, `3889c5954743beb6468c8a6a31b286d46190414982b93637815aa3fa9d39b3df` ;
- identiques sur deux exécutions : **nouvelles empreintes de non-régression** (remplacent `5c464d3d…` / `06faba3e…`).

**Contrôles** (Release MSVC) :
- /W4 sans avertissement (D9025 : /W1 remplacé) ;
- TU par tranches : **3 118 verts**, 12 ignorés, 0 rouge ;
- fautes d'A5 (préfixe 5) : 48 sur 48 rouges en S5, **55 sur 55** après S7 (5301 à 5308 ajoutées, 51156 retirée) ; détecteurs
  renforcés en S5 : 55179 (facettes de la tige, test
  `the_stem_back_clears_a_tour_bulging_between_its_rings`), 55178 (mode enveloppe), 55163 (octets de K56, sous
  `_MSC_VER` seulement), 55162 (épaisseur contre la cote 0,3) ; rejeu antérieur 164 sur 168 ;
- garde ODR sur les nouveaux noms (contrôle positif `BuildLeafShell`).

**Dette** (`debt_cgarchi.md` §2.2) :
- flanc du tambour crénelé de profil ;
- recouvrement du volume schématique en mode enveloppe ;
- G latéral vu de chant en vue de face ;
- jour de 1 p entre cornet et rosace, sans bande de lèvre ;
- nœuds de la tige et languette centrale absents ;
- durées au-dessus de la prédiction ;
- opus construit par MSBuild (MSB1009) ;
- « 4 » de `fleuron_largeur` non lu ;
- K56 portable aveugle à un dernier bit ;
- faute 47 (K12).

### 11.16 Bilan du lot A6 (mesures par niveau, WASM, colonnade)

Base : `01dd762` (parent stable `5ce6d8c`). Plan de l'architecte (scratchpad `a6_plan.md`), décisions de l'utilisateur
du 2026-11-27 :
- Q-A6-0 (a) : ce document rendu depuis `stash@{0}` (bilans A2 à A5, registre AC-45 à AC-74), stash gardé ;
- Q-A6-1 (a) : instrument rang × niveau hors suite (AC-79) et K60 permanent ;
- Q-A6-2 (a) : P4 réfutée par la planche, borne = cône (AC-2, AC-40, AC-78) ;
- Q-A6-3 (a) et (d) : dépassement de la bannière accepté, seuil 300 000, conseil par niveau (AC-75) ; assemblage linéaire
  (part de l'assemblage ≥ 30 % à N = 12) ; sous-décision (f2) : bannière seule (AC-77 non prise) ;
- Q-A6-4 (a) : build WASM en S4 ; module d'origine restauré ; module reconstruit sur le code final après la relecture
  S8, en commit séparé ;
- Q-A6-6 (c) : pas de GCC ni de TSan dans A6 (K65, K66 retirés), la CI fait foi pour GCC ;
- critère de S3 : assemblage ÷ 3 au moins, `Regenerate` N = 12 Light ≤ 120 ms, 21 empreintes FNV-1a identiques ;
- dos de la rosace : option (a), deux lectures gardées.

**Mesures par rang** (`DISABLED_ornament_measures_by_rank`, Release MSVC) **[M]** :

| Rang | Faces L / H | Représentantes (ms) L / H | Débord de l'abaque L / H (K60) | Couverture cumulée L | Jour min L |
|---|---|---|---|---|---|
| F | 14 400 / 82 368 | 1,11 / 2,91 | +2,99 / +3,03 | 0,282 | 0,523 |
| E | 15 296 / 88 128 | 1,20 / 3,16 | +9,34 / +9,40 | 0,710 | 0,581 |
| C | 5 760 / 24 576 | 0,02 / 0,08 | −0,91 / −0,91 | 0,752 | 0,059 |
| D | 20 800 / 47 936 | 3,74 / 4,92 | +1,68 / +1,69 | 0,876 | 0,059 |
| V | 7 232 / 28 800 | 0,08 / 0,24 | +3,65 / +3,65 | 0,876 | 0,122 |
| H | 7 088 / 28 080 | 0,07 / 0,23 | +6,29 / +6,31 | 0,876 | 0,144 |
| G | 8 656 / 29 936 | 2,86 / 6,78 | +1,98 / +1,99 | 0,876 | 1,205 |

- représentantes 9,07 / 18,32 ms, soit 74,6 / 62,4 % de la colonne (12,17 / 29,37 ms) ; le reste, par différence :
  assemblage 0,56 / 1,73 ms, tour et versement dans le `Mesh` 2,53 / 9,32 ms ; normales 2,0 / 7,6 ms ;
- premier appel d'un processus (5 processus) : colonne 30,0 / 47,1 ms contre 12,5 / 29,5 à chaud (**+17,5 / +17,6 ms**) ;
  poses des D seules 14,1 ms ;
- planches de P-A6-1 (plan de la pl. XXV, dessus et dessous ; rayon maximal par azimut) : scratchpad `a6\s1\`.

**Colonnade** (K61, K62, `AssembleColonnadeMesh`) **[M]** :

| N | Light avant / après (ms) | Heavy avant / après (ms) | Faces Light / Heavy |
|---|---|---|---|
| 1 | 20,6 / 16,3 | 57,2 / 41,0 | 93 996 / 344 588 |
| 4 | 52,2 / 28,1 | 177,1 / 83,7 | 374 436 / 1 376 804 |
| 12 | 191,6 / **58,6** | 697,6 / 196,6 | 1 122 276 / 4 129 380 |

- `Regenerate` médian ; assemblage (code du produit) à N = 12 : 150,0 → 20,5 ms (÷ 7,3, Light), 578,0 → 82,6 ms (÷ 7,0,
  Heavy) ; part de l'assemblage avant : 78 / 83 % ; crête mémoire du processus à N = 12 : 110 / 267 Mo ;
- assemblage : réserve unique, N copies translatées, entablement et familles d'ornements écrits en place, un seul
  `SetVertices` / `SetFaces` ; sortie identique à l'octet aux `appendMesh` successifs (K62, 21 cas ; 21 empreintes
  FNV-1a égales à celles d'avant) ;
- bannière : conseil par niveau (AC-75) ; rendus de P-A6-2 : scratchpad `a6\s2\`.

**WASM** (Emscripten 6.0.8, Node 22.19 ; module reconstruit sur le code final d'A6 et publié dans `maker/web/` : `maker.js` `c527b813…`, `maker.wasm` `83120f12…`, identiques à l'octet au module contrôlé en S4) **[M]** :
- `check_module.mjs` : 29 formes, 2 gothiques, stderr muet ;
- K64 : comptes égaux au natif sur 8 cas ; colonne 21,1 / 55,6 ms (seuils 50 / 150) ; colonnade N = 4 Light 42,1 ms
  (seuil 200), N = 12 Light 96,3 ms, Heavy 338,3 ms ; tas 17 à 247 Mo ; premier appel 68,5 / 111,1 ms (+47 / +55 ms) ;
  facteur WASM / natif 1,48 à 1,72 ;
- module publié, sonde rejouée : colonne 20,7 / 54,8 ms ; colonnade N = 4 Light 41,8 ms, N = 12 Heavy 336,7 ms ; colonnades sans ornement de chapiteau (ionique cannelé, corinthien à « Volutes » décoché, 12 colonnes) : bannière sans conseil sur `Capital ornaments` ;
- contrôle négatif sur le module publié avant A6 (`cd95f1c5…` / `18057a6d…`, antérieur à A0) : rouge (« Capital ornaments » absent, comptes du tour seul) ;
- navigateur (Firefox, captures de l'utilisateur) : colonne 30,0 / 96,0 ms ; colonnade N = 12 Heavy affichée sans
  incident, bannières conformes.

**Fleuron** : K63 (`the_rosette_back_holds_on_an_asymmetric_tour`) : deux tours bombés de 0,1 p d'un seul côté, jour ≥ c,
dos miroir ; fautes 6401 (une seule boîte) et 6402 (plus petit des deux dos) rouges, vues par K63 seul. La lecture
unique du dos (variante « réunion ») est réfutée : le maillage du tour réel n'est pas son propre miroir (AC-69).

**GLB** (`opus --colonne-corinthienne`) : inchangés, `958b9425…` (Light) et `3889c595…` (Heavy) ; K42 12,1 à 12,3 ms
(Light), 26,9 à 29,7 ms (Heavy).

**Contrôles** (Release MSVC) :
- /W4 sans avertissement, mesuré à la relecture S8 en recompilant les unités d'A6 (produit et tests) ; avant correction : 3 C4018 (comparaisons signé / non signé) et 1 C4996 (`sscanf`) dans les tests d'A6, invisibles au niveau d'avertissement par défaut du projet ;
- TU par tranches : **3 122 verts**, 12 ignorés, 0 rouge ;
- fautes d'A6 : 14 sur 14 rouges (6101, 6102, 6201 à 6203, 6301, 6302, 6304, 6401 à 6403 ; 6501 à 6503 à la relecture S8 : garde du conseil de la bannière, sensibilité de K63) ; A5 : 55 sur 55 ; antérieures :
  163 sur 166 (44 → 144, 46 → 1046, 4126 sans TSan) ; **47 rouge sur K3** : l'ensemble attendu (K3 et K12) était faux,
  K12 est constant par rang sur des copies tournées.

**Dette** (`debt_cgarchi.md` §2.2) :
- volutes hors du contour de l'abaque près des cornes (r 31,2 contre ≈ 29) ;
- durées de la colonne au-dessus de la prédiction (variante « réunion » réfutée) ;
- surcoût du premier appel (cache froid) ;
- `LastError` sans liaison Embind ;
- 4126 sans TSan, vérification GCC locale ;
- soldées : faute 47, K56 portable (limite acceptée), fautes 44 et 46 (variantes 144 et 1046), module WASM publié antérieur à l'acanthe.

### 11.17 Bilan du lot A7 (volutes sous les cornes de l'abaque)

Base : `ba60d5e` (parent stable `5ce6d8c`). Plan de l'architecte (scratchpad `a7_plan.md`), décisions de l'utilisateur
du 2026-11-28 :
- Q-A7-0 (a) : V borné en plan par l'arête supérieure de l'abaque construit A, marge c, garde du produit (AC-80, AC-82) ;
  confirmé à P-A7-1 malgré l'écart de 0,6 à 0,9 p au ruban gravé (AC-83) ;
- Q-A7-1 : pose C1 (AC-81), choisie sur le balayage de S1 ;
- Q-A7-2 : hélices hors d'A7, mesurées contre A dans K60, en dette ;
- Q-A7-3 : K60 en deux colonnes, contre B et contre A (AC-78) ;
- Q-A7-4 : module WASM reconstruit après la relecture, commit séparé ;
- P-A7-1 : référence de l'œil au profil 27,1 (AC-63) ; balayage gardé comme instrument désactivé ;
- P-A7-2 : C1 validé sur les planches (dessus, plan et profil de la pl. XXV, face de la pl. XXVI).

**Diagnostic** (dette d'A6 l. 102) **[M]** : sur les maillages d'A6, V sortait de l'arête supérieure de A de +1,22 / +1,23 p,
sur les flancs de la corne (41°), pas à la pointe ; l'abaque des données B (creux 17,6 p) ne peut pas borner V sans
contredire les planches (+2,4 p d'écart A − B à 41°).

**Produit** :
- `CapitalEnvelope::abacusOutline` : anneau du sommet le plus haut de « A · cimatio dell'abaco (talon) », lu par
  `ReadCapitalEnvelope` (refus si le talon manque ou n'a pas de sommet) ; dans la clé du cache des poses des D ;
- `VoluteSpec::planBound` (vrai pour les volutes, faux pour les hélices) et garde « hors de l'abaque en plan » de
  `BuildVoluteRibbon`, après celle du cône ;
- pose C1 : œil R 27,2 p, `eyeTheta` 40°, plan à 22,5° (inchangé).

**Mesures** (Release MSVC, Light / Heavy) **[M]** :

| Grandeur | A6 | A7 |
|---|---|---|
| V contre l'arête supérieure de A (K60, K35) | +1,22 / +1,23 | **−0,108 / −0,106** |
| V contre l'abaque des données B (K60) | +3,65 / +3,65 | +2,44 / +2,44 |
| V contre le cône (K35) | −0,077 / −0,072 | −0,459 / −0,453 |
| jour de la paire (K36) | 1,97 | 0,940 |
| part du pied caché (K39, borne 0,63) | 0,649 | 0,649 |
| raccourci vu de face (≥ 0,90) | 0,98 | 0,982 |
| portée de la spirale au profil (gravé 31,2) | 31,11 | 30,76 |

- F, E, C, D, H, G identiques à l'octet (empreinte par rang, `DISABLED_rank_fingerprints`), V seul change ; comptes
  inchangés : ornement 79 232 / 329 824, colonne 92 880 / 343 472 faces ;
- K60, deux colonnes : registre AC-78 (compléments d'A7) ;
- K42 12,28 à 12,29 / 26,80 à 27,20 ms ; GLB d'`opus --colonne-corinthienne` : Light `1d250f45…`, Heavy `17dcd1f7…`
  (stables sur 2 tirages ; V a changé) ; planches avant / après : scratchpad `a7\s3\`.

**Fautes** (`CG_LEAF_FAULT`, lots de 3 au plus, sources restaurées par sha256) **[M]** :
- A7 : 7101 (garde en plan retirée, K43 seule), 7102 (pose d'A6, garde en place), 7103 (pose d'A6, garde retirée : K60,
  K35, K30), 7104 (anneau lu sur le listel), 7105 (borne en plan aussi pour H), 7106 (anneau lu à θ + π/4) ; 7107
  (talon absent toléré) et 7108 (refus « talon sans sommet » retiré), vues par le seul test des refus de
  `ReadCapitalEnvelope` : 8 sur 8 rouges ;
- rejeu d'A6 : 14 sur 14 ; rejeu d'A4, familles V et H (110 à 125, 123, 130 à 142, 200, 201, 210 à 215, 126 et
  variantes) : toutes rouges après re-pose de trois fautes mal posées à la pose C1 : 1111 et 1117 retirent aussi la
  garde en plan (1111 reprend la pose d'A6, pour laquelle elle était écrite) ; 215 (θ_b 25°) fait entrer la tige dans
  la corbeille, doublée de 1215 (26,5°) et 2215 (27,5°), rouges sur K39 ;
- non injectable : l'anneau de A retiré de la clé du cache des D (mutant équivalent : les poses des D ne le lisent pas).

**WASM** (Emscripten, Node 22.19 ; module reconstruit sur le code final d'A7) **[M]** :
- `maker.js` `c527b813…` inchangé à l'octet, `maker.wasm` `7e5cee47…` ;
- `check_module.mjs` : 29 formes, 2 gothiques ; `probe_a6.mjs` : comptes égaux au natif sur 10 cas, colonne 20,7 /
  54,8 ms, colonnade N = 4 Light 41,8 ms, N = 12 Heavy 330,9 ms, bannières conformes ; stderr muet ;
- `probe_a7.mjs` (saillie en plan de la colonne dans le secteur des cornes, 5,75 à 14 p sous le sommet, contre le carré
  concave lu sur le sommet de l'abaque) : −0,1085 / −0,1062 p, égaux au natif ; contrôle négatif sur le module d'A6
  (`c527b813…` / `83120f12…`) : +1,219 / +1,225, rouge.

**Contrôles** (Release MSVC) :
- TU par tranches : **3 122 verts**, 12 ignorés, 0 rouge ;
- /W4 : 0 avertissement sur les unités recompilées (produit et tests de l'acanthe, journal horodaté) ;
- relecture S6 : 1 MEDIUM (refus du talon non testés : deux cas ajoutés, fautes 7107 et 7108), 3 LOW (références
  AC-63 / AC-81 des commentaires, `eyeRadius` écrit une seule fois dans `DefaultVoluteSpec`, contrôle de l'anneau du
  balayage contre ses cotes) ; sorties identiques à l'octet après correction.

**Dette** (`debt_cgarchi.md` §2.2) :
- hélices hors de l'arête supérieure de l'abaque construit (+5,25 / +5,27 p), avec la profondeur de leur œil ;
- anneau de A dans la clé du cache des D sans détecteur ;
- marge mince du pied caché des volutes (0,649 pour 0,63) ;
- soldée : volutes hors du contour de l'abaque de part et d'autre des cornes (l. 102 d'A6).
