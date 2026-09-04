# Faisabilité — module « Texte 3D imprimable » du maker (cible : parité stltext.com)

**Date** : 2026-09-03
**Périmètre** : `maker/` (page + gabarit), `src/cggraph/nodes` (catalogue), `src/cgmesh`
(primitives 2D→solide). Cible d'exécution : **WASM navigateur**, tout local.
**Nature** : analyse seule, aucun code d'implémentation écrit.
**Référence externe** : `https://stltext.com/` — page **récupérée et lue**
(2026-09-03). La récupération automatique rendait d'abord un HTTP 403 : filtrage
anti-bot sur l'en-tête `User-Agent`, aucune authentification en jeu ; un `User-Agent` de
navigateur rend 200. Les contrôles ci-dessous sont donc **relevés sur la page**, pas
déduits d'une description commerciale.

**Panneau « Parameters » de l'outil, tel qu'il est rendu** — unité affichée : `MM / STL`

| Contrôle | Valeur relevée |
|---|---|
| `Text` | champ **une ligne**, compteur `5 / 30` — **30 caractères maximum** |
| `Typeface` | **10 entrées** = 5 familles × Regular/Bold : Helvetiker, Optimer, Gentilis, Droid Sans, Droid Serif |
| `Letter height` | 24 mm |
| `Extrusion depth` | 5 mm |
| `Letter spacing` | 1 mm |
| `Edge bevel` | 0,6 mm — **un seul scalaire** |
| `Origin alignment` | Left / Center / Right |
| `Build style` | **Text / Keychain / Plate** — un préréglage, pas une épaisseur réglable |
| Sortie | `Download STL` (binaire) |
| Affichage | `Model envelope 0.0 × 0.0 × 0.0 mm` |

Deux relevés qui changent la lecture du problème :

- **Les dix polices sont au format `typeface` JSON** (les cinq familles sont exactement
  celles livrées avec three.js), et l'import **TTF/OTF est un outil SÉPARÉ**
  (`/font-to-...`, route distincte), pas un réglage du générateur. La chaîne de polices du
  dépôt — stb_truetype, `glyf` **et** CFF, collections `.ttc`, crénage, 4515 glyphes sur
  Noto — est donc **strictement plus riche**, sur l'outil principal.
- **Le biseau est un scalaire unique en mm**, et le socle un **préréglage** sans épaisseur
  exposée. Cela borne le périmètre de parité bien plus bas que ce qu'un port `Profile2D`
  sait faire : un chanfrein droit suffit (cf. §3.2 G2).

Le générateur proprement dit vit dans un chunk JS chargé à la demande et minifié
(`assets/index-*.js` est un routeur Vite) : **son code n'a pas été lu**. Ce qu'il produit,
en revanche, a été mesuré — trois exports STL disséqués au §1 bis, qui répondent à la
place du code.

---

## 1. Cadrage — la question est plus étroite qu'elle n'en a l'air

Le dépôt n'est pas devant une feuille blanche : **le module existe déjà**, sous le nom
« Texte 3D » (`maker/web/text.html`, carte publiée par `maker/web/index.html:50`). Il est
piloté par un graphe cggraph décrit dans `maker/web/data/templates/text3d.json` :

```
file.ref → text.font.load → text.contours → shape.extrude → (viewer, export)
```

La chaîne native derrière est complète et **déjà portée sous Emscripten** :
`src/cgmath/font.cpp` (stb_truetype : TTF/OTF/TTC), `text_layout.cpp`,
`bezier_flatten.cpp`, `src/cgmesh/text_extrude.cpp`, `extrude_contours.cpp`, plus
Clipper2 vendoré (`extern/clipper2`, cf. `src/cgmesh/CMakeLists.txt:112`). Quinze polices
redistribuables sont embarquées avec leurs licences
(`test/data/fonts/catalogue.json`, recopiées par `maker/CMakeLists.txt:173-183`).

La question n'est donc pas « peut-on faire un générateur de texte 3D ? » mais **« que
manque-t-il à celui qui existe pour être un outil d'impression 3D ? »** — et la réponse
tient en **quatre** lacunes retenues, dont une seule est structurante — le porte-clés de la
référence étant volontairement laissé de côté pour ce premier lot (§3.4).

### Confrontation fonction par fonction

| Contrôle stltext | État dans le dépôt | Verdict |
|---|---|---|
| `Text`, 1 ligne ≤ 30 car. | `text.contours/text`, **multi-ligne**, sans limite de longueur | ✅ (mieux) |
| `Typeface` — 10 `typeface` JSON | **15 polices** TTF/OTF/CFF + import `.ttf/.otf/.ttc` de l'utilisateur **dans le même outil** (port `file.ref`) | ✅ (mieux) |
| `Origin alignment` | `align` (gauche/centre/droite) + `lineSpacing`, crénage `kern` — sans équivalent en face | ✅ (mieux) |
| `Letter spacing` (mm) | `letterSpacing`, en unités monde — sémantique à fixer (G3) | ✅ |
| `Extrusion depth` | `shape.extrude/depth` | ✅ |
| Reconstruction locale à chaque réglage | évaluation WASM sur le thread UI, cache par nœud, annulation | ✅ |
| Prévisualisation orbitale | Online3DViewer (`maker/web/js/viewer.js`) | ✅ |
| `Build style: Plate` | `support` = plaque / bandeau / cadre — **mais à la même profondeur que les lettres** ; deux formes visées, rectangulaire **et silhouette décalée** (§3.2 G1) | ⚠️ **G1** |
| `Build style: Keychain` | absent — **retiré du périmètre** (décision du 2026-09-03), noté en amélioration (§3.4) | ⏸ hors lot |
| `Edge bevel` (scalaire, mm) | absent | ❌ **G2** |
| `Letter height` en mm + `Model envelope` | `size` = corps em en unités monde abstraites, aucune cote à l'écran | ❌ **G3** |
| `Download STL` (binaire) | l'encodeur existe (`maker/web/js/exporters.js:43`) mais n'est pas câblé à cette page (bouton OBJ seul, `template.js:488`) | ❌ **G4** |

Soit **quatre lacunes retenues** — une seule structurante (G1), une seule non triviale
(G2) — plus une **écartée de ce lot** : le porte-clés. Le relevé avait par ailleurs **retiré**
deux avantages que je prêtais à tort à la référence : elle est mono-ligne et ne lit pas les
TTF dans son outil principal. La dissection des STL (§1 bis) a ensuite **abaissé le coût de
G1** : elle n'emploie aucun booléen.

**En sens inverse, le périmètre s'élargit sur deux points**, et ce sont les seuls endroits
où ce module vise plus haut que ce qu'il copie :

- le réglage d'arête ne se limite pas au scalaire de la référence — **chanfrein, biseau et
  congé** sont tous trois autorisés (§3.2 G2) ;
- la plaque ne se limite pas au rectangle — une **plaque silhouette**, contour du texte
  décalé d'un offset, est demandée en plus (§3.2 G1).

---

## 1 bis. Mesure du STL de référence — trois exports disséqués

**Méthode** : les trois `Build style` exportés depuis l'app (texte « MANGO », Helvetiker
Bold, `Letter height` 24 mm, `Extrusion depth` 5 mm, `Letter spacing` 1 mm, `Edge bevel`
0,6 mm, alignement centré), **octets interceptés et analysés** — soudure des sommets à
1e-4 mm, comptage des arêtes par multiplicité, composantes connexes, faces dupliquées et
dégénérées. Le fichier n'a **pas** pu être écrit sur disque : Chrome est réglé sur
`prompt_for_download`, donc une boîte « Enregistrer sous » native intercepte chaque export,
et une requête page → `127.0.0.1` est refusée par la politique d'accès au réseau local.
Cela n'ôte rien à la mesure — elle porte sur les octets mêmes du STL.

| | `Text` | `Plate` | `Keychain` |
|---|---|---|---|
| Octets | 230 684 | 231 284 | 308 084 |
| Triangles | 4 612 | 4 624 | 6 160 |
| Sommets bruts → soudés | 13 836 → 2 312 | 13 872 → 2 320 | 18 480 → 3 088 |
| Encombrement (mm) | 124,98 × 26,86 × 6,20 | 136,98 × 27,96 × 7,65 | 149,78 × 27,96 × 7,65 |
| Arêtes de bord / non-manifold | **0 / 0** | **0 / 0** | **0 / 0** |
| Faces dupliquées / dégénérées | 0 / 0 | 0 / 0 | 0 / 0 |
| **Coques** | **5** | **6** | **7** |

### Ce que les coques disent de la construction

**Cinq coques pour cinq lettres** : `M`, `A`, `N`, `G`, `O` sortent en solides **séparés**,
jamais fusionnés. Chacune est fermée — aucune arête de bord sur l'ensemble du fichier.
À comparer au dépôt, dont `text_to_contours` fusionne **toujours** les glyphes en une
région unique (`src/cgmesh/text_extrude.h:154`) : notre sortie serait **une** coque là où la
leur en compte cinq. Ce n'est pas un manque, c'est l'inverse — mais c'est une différence
de nature à connaître avant de comparer deux fichiers.

**`Plate` = les cinq mêmes coques, inchangées, plus une sixième de 12 triangles** — donc une
**boîte** (aucun congé, aucun biseau) de 136,98 × 27,84 × **2,20 mm**, placée en
z ∈ [−4,55 ; −2,35] quand les lettres occupent z ∈ [−3,10 ; +3,10].

C'est **exactement la construction G1-a** (§3.2) : le socle **interpénètre** les lettres sur
**0,75 mm**, et l'assemblage est livré tel quel. La référence ne fait **aucun booléen**, ni
2D ni 3D — elle empile des coques fermées et laisse le slicer les unifier. Deux conséquences
directes sur le plan :

1. **G1-a suffit à la parité**, et l'étape 3 n'a donc pas à attendre le booléen 2D. Ce que
   je présentais comme un compromis assumé est en réalité ce que fait le concurrent.
2. **G1-b (socle percé, coque unique manifold) DÉPASSE la référence.** À garder comme
   objectif de qualité, pas comme condition de parité.

**`Keychain` = `Plate` plus un anneau de 1 536 triangles**, 12 × 12 × 2,20 mm, exactement à
la cote du socle (z ∈ [−4,55 ; −2,35]) : l'anneau est accroché au **socle**, pas aux lettres.
La réserve que j'avais inscrite (« vérifier que l'anneau touche réellement une lettre »)
**tombe** : la référence contourne le problème en ne l'accrochant jamais à une lettre. C'est
plus simple que ce que j'envisageais, et c'est la solution à reprendre — **le jour où le
porte-clés reviendra au périmètre**, dont il est sorti par décision (§3.4, A1).

### Deux mesures qui corrigent des choix de conception

**Le biseau DILATE, il ne chanfreine pas.** Z total = 6,20 mm pour une profondeur demandée
de 5 mm, soit **`depth` + 2 × `bevel`** ; en XY, les lettres à sommet plat (`M`, `N`)
mesurent 25,51 mm pour une hauteur demandée de 24 mm. Le biseau est donc appliqué **vers
l'extérieur, symétriquement sur les deux faces** — un `InflatePaths` **positif**, et non le
décalage intérieur que décrit G2. Les deux sémantiques se défendent : la mienne préserve
l'emprise nominale (24 mm demandés = 24 mm mesurés), la leur préserve l'âme des déliés.
**Tranché en faveur du décalage intérieur** (D1, §8) : les cotes affichées et la hauteur de
lettre en millimètres ne souffriraient pas qu'un réglage de 24 mm rende 26,86. Le revers est
retenu et non nié — la dilatation aurait annulé le risque « déliés mangés » de G2, que ce
défaut ramène ; elle reste donc disponible en option, et la détection de l'anneau dégénéré
entre dans l'étape 4.

**Le modèle est centré sur z = 0**, pas posé sur le plateau (z ∈ [−4,55 ; +3,10] pour
`Plate`). `shape.extrude` place ses solides en z ∈ [0 ; depth] : **la convention du dépôt est
la meilleure des deux**, et c'est un point à ne pas « corriger » par mimétisme.

### Coût en triangles — la comparaison est flatteuse pour le dépôt

Les deux lettres courbes pèsent **2 048 et 2 028 triangles** à elles seules, contre 204, 176
et 156 pour `M`, `A`, `N` : l'échantillonnage des courbes est **fixe et généreux**, multiplié
par les anneaux du biseau. Le dépôt expose `flattenTol`, tolérance de corde en unités monde
(`src/cgmesh/text_extrude.h:48`), donc un réglage de finesse que la référence n'offre pas —
à condition de le rendre relatif au corps (G3).

### Pile technique de la référence — relevée dans les artefacts servis

Le chunk du générateur (`assets/text-to-stl-studio-*.js`) déclare ses dépendances lazy dans
un tableau `__vite__mapDeps` : c'est la liste exacte, pas une déduction.

| Couche | Techno | Preuve |
|---|---|---|
| Géométrie + rendu | **three.js r185** | `__THREE__='185'` ; `three.module-*.js` |
| Contours de glyphes | **`FontLoader`** (typeface JSON) | `FontLoader-*.js` ; `moveTo`/`lineTo`/`quadraticCurveTo`/`bezierCurveTo` dans le studio |
| Solide extrudé | **`TextGeometry`** (donc `ExtrudeGeometry`) | `TextGeometry-*.js` |
| Export | **`STLExporter`** | `STLExporter-*.js` |
| Prévisualisation | `WebGLRenderer`, `PerspectiveCamera(38,…)`, `Fog`, **`OrbitControls`** | lus littéralement dans le studio |
| Polices de l'utilisateur | **opentype.js** | `opentype-*.js`, chargé par la route « Custom Font » seulement |
| Interface | **React** + **TanStack Router** + **Vite** | `react-dom-*.js`, `matchContext-*.js`, `getParentRoute`, `__vite__mapDeps` |
| Styles | **Tailwind CSS v4** | 1 068 × `--tw-`, 97 × `oklch`, `@layer` |
| Icônes | **lucide-react**, un chunk par icône | `check-*`, `chevron-right-*`, `key-round-*`, `sparkles-*`, `upload-*`, `x-*` |
| i18n | **Paraglide JS** (inlang) | 1 362 × `paraglide`, `baseLocale` ; 387 Ko de messages |
| Hébergement | **Cloudflare** | `Server: cloudflare`, `CF-RAY: …-CDG`, `cf-nel` ; filtrage par `User-Agent` |
| Mesure | Plausible + `gtag` | dans le bundle principal |

**Trois conséquences pour ce document.**

1. **Le « biseau qui dilate » (§1 bis) n'est pas un choix de conception : c'est le
   comportement de `ExtrudeGeometry`.** three.js extrude de 0 à `depth` puis **ajoute** le
   biseau au-delà des deux faces (`bevelThickness`) et l'étend vers l'extérieur en XY
   (`bevelSize`) — d'où exactement les 6,20 mm mesurés pour 5 demandés. Ce n'est pas une
   sémantique arbitrée par la référence, c'est celle de sa bibliothèque. **Argument
   supplémentaire pour D1** : rien à respecter là-dedans.
2. **Les cinq coques par cinq lettres et les 13 836 sommets bruts** s'expliquent de même :
   `TextGeometry` rend une soupe de triangles non indexée, et `STLExporter` l'écrit telle
   quelle. La qualité mesurée (zéro arête de bord) est celle de la bibliothèque, pas d'un
   travail de maillage.
3. **L'import TTF/OTF de la référence passe par opentype.js**, sur une route séparée. Le
   dépôt fait la même chose avec stb_truetype **dans l'outil principal**, ce qui confirme
   l'avantage noté au §1.

**Ce que trois.js leur donne et que le dépôt n'a pas** : rien, sur ce périmètre. Le dépôt a
son propre chemin complet (police → mise en page → aplatissement → contours → tessellation →
solide), déjà porté en WASM. La comparaison n'est donc pas « une bibliothèque contre une
autre » mais « leur pile JS contre la nôtre en C++ compilé ».

### Ce que la mesure ne dit pas — et pourquoi ce n'est plus une question

La règle qui fixe l'épaisseur du socle à **2,20 mm** reste inconnue : un seul jeu de
paramètres a été exporté, donc impossible de distinguer une constante d'une fonction de
`depth`. **D3 (§8) rend la question sans objet** : l'épaisseur de plaque devient un paramètre
en millimètres, là où la référence l'enferme dans son préréglage. Il n'y a donc rien à
deviner — et c'est précisément ce que G1 apporte.

---

## 2. Approche nodale, ou autre ?

**Nodale — et ce n'est pas un choix à faire, c'est celui qui est déjà en vigueur.** La
page est un gabarit JSON au-dessus du catalogue de nœuds ; son propre en-tête
(`maker/web/text.html:12-20`) rappelle ce qui a été supprimé pour y arriver :
`js/pages/text.js` et la chaîne C++ dédiée `createTextExtrusion` / `ParameterizedText3D`.
La raison est explicite : page et éditeur nodal passant par les mêmes nœuds, **ils ne
peuvent plus diverger**, et « Enregistrer le document » rend un document cggraph ordinaire
qui s'ouvre dans `graph.html`.

Trois voies restent ouvertes pour combler les lacunes. Elles ne se valent pas.

**(a) Étendre le catalogue avec des nœuds GÉNÉRAUX — recommandé.**
Les manques (socle indépendant, biseau, texte creusé, trou de porte-clés) se ramènent tous
à trois briques réutilisables : un **booléen 2D** sur jeux de contours, une **extrusion
placée en Z** (et non forcément posée à z=0), une **extrusion profilée** consommant le type
`Profile2D` qui existe déjà. Aucune n'est spécifique au texte : le booléen 2D sert
identiquement à `svg.contours` et à `img.relief.layers`, l'extrusion profilée est
littéralement annoncée par `src/cggraph/nodes/shapes/profile.h:11` — « alimentera le
**biseau d'un texte** ou une section balayée le long d'un chemin ». Le coût est réparti sur
des nœuds que d'autres chantiers attendent déjà.

**(b) Un nœud monolithique `text.stltext` qui rend le solide fini.** Plus rapide à écrire,
et c'est son seul avantage. Il réintroduit exactement la faute que l'étape « texte 3D »
avait corrigée : une deuxième implémentation du socle et du biseau, invisible aux autres
producteurs de contours, et un nœud dont les quinze paramètres ne se recomposent pas.
À écarter.

**(c) Une page dédiée non nodale (JS + une fonction C++ `createTextForPrint`).** C'est le
retour au monde d'avant, avec la divergence page/éditeur pour prix. Le seul argument
sérieux en sa faveur serait un besoin d'interface que le gabarit ne sait pas exprimer
(§3, G3 : `ParamSet` ne porte ni bornes ni unités) — mais cette limite se corrige dans le
gabarit, pas en supprimant le graphe. À écarter.

**Conclusion** : (a). Le module « stltext » devient alors *un gabarit JSON de plus* plus
trois nœuds génériques, et il reste, gratuitement, un document nodal que l'utilisateur peut
ouvrir et détourner.

---

## 3. Cartographie : réutilisable / manquant

### 3.1 Réutilisable tel quel

| Brique | Où | Ce qu'elle donne |
|---|---|---|
| Chargement de police | `src/cgmath/font.h` | TTF (`glyf`), OTF/CFF (Type 2), collections `.ttc`, contours en unités de police **en courbes** ; WOFF/WOFF2, Type 1, bitmap refusés **avec diagnostic** |
| Mise en page UTF-8 | `src/cgmath/text_layout.h` | glyphes placés, crénage `kern` (tri-état `KerningStatus`), alignement, interligne |
| Aplatissement | `src/cgmath/bezier_flatten.h` | tolérance appliquée **après** mise à l'échelle → finesse indépendante de l'em de la police |
| Texte → contours | `text_to_contours`, `src/cgmesh/text_extrude.h:154` | union Clipper2 NonZero de tous les glyphes, mémoïsation par glyphe distinct (« MISSISSIPPI » : 11 placés, 4 aplatis) |
| Contours → solide | `src/cgmesh/extrude_contours.h` | tessellation glutess + capots + parois, **`zBottom`/`zTop` déjà paramétrables**, `emitWalls`, `wallFilter` par arête, `materialId` |
| Offset polygonal robuste | Clipper2 `InflatePaths`, précédents en `architecture_gothic.cpp:529` et `image_region_pipeline.cpp:128` ; emballage `stroke_contours.h` | rétrécissement/dilatation avec union terminale — c'est la brique du biseau |
| Profils de moulure | `src/cgmesh/profile2d.h`, nœuds `profile.chamfer` / `profile.cavetto` | type valeur, port nodal, cinq producteurs |
| Fusion de maillages | `Mesh::Append`, `src/cgmesh/mesh.h:701` | concaténation — la moitié d'un nœud `mesh.merge` |
| Sortie vers le navigateur | `graphMeshData`, `maker/wasm_api.cpp:775` | positions/normales/indices groupés par matériau |
| Encodeur STL binaire | `maker/web/js/exporters.js:43-84` | STL 84 + 50·n octets, normales recalculées |
| Gabarit de page | `maker/web/js/template.js` | widgets, bornes, énumérations, catalogues de fichiers, **vérification de type au chargement** |

Tout cela est dans la liste `EMSCRIPTEN` de `src/cgmesh/CMakeLists.txt` (y compris
`profile2d.cpp:84` et `extrude_contours.cpp:65`) : **rien à porter**.

### 3.2 Manquant

#### G1 — Socle d'épaisseur indépendante des lettres (**la seule lacune structurante**)

C'est la contrainte que `src/cgmesh/text_extrude.h:70` énonce sans l'adoucir : support et
texte **partagent** `depth`, parce que l'union est **2D** et rend une région plane unique.
Conséquence mesurée, écrite dans le même en-tête : `Plate` contient l'emprise du texte,
donc à profondeur égale l'union rend la **silhouette de la plaque** et les lettres
disparaissent dedans. Seuls `Bar` et `Frame` restent lisibles — précisément parce qu'ils ne
couvrent pas les lettres.

Or « socle de 2 mm + lettres de 5 mm en relief » est le cas d'usage majoritaire d'un
générateur de plaque de porte ou d'étiquette. **Sans lui, le module n'est pas un
concurrent de stltext.**

L'en-tête conclut qu'il faudrait « un booléen 3D, c'est-à-dire une capacité nouvelle ».
**C'est vrai de la voie qu'il envisageait, et faux du problème** : la pièce est
2,5D — un empilement de régions planes à des Z différents. Deux constructions y suffisent,
toutes deux au-dessus de primitives existantes :

- **G1-a, deux coques (zéro géométrie nouvelle)** : `Append(plaque, z ∈ [0, t])` puis
  `Append(texte, z ∈ [t, t+h])` sur le **même** `ExtrudedMeshBuilder`. Deux solides fermés
  qui se touchent sur une face coplanaire : c'est ce que produit tout slicer quand on pose
  deux corps l'un sur l'autre, et Cura/PrusaSlicer/Bambu l'unifient sans broncher. Faces
  internes conservées, donc **non manifold au sens strict** — acceptable pour l'impression,
  discutable pour un export propre.
- **G1-b, socle percé (manifold)** : `plaqueDessus = Difference(plaque, texte)` (Clipper2),
  puis capot supérieur du socle tessellé sur cette région, parois du texte montant de `t` à
  `t+h`. Aucune face interne, **une seule coque étanche**. Coût : le nœud booléen 2D de G3
  ci-dessous, et le paramétrage `emitWalls` / `wallFilter` que `extrude_contours.h` expose
  déjà.

**Mesuré sur la référence (§1 bis)** : c'est **G1-a qu'elle applique**, avec un socle de
2,20 mm qui interpénètre les lettres de 0,75 mm. G1-a est donc la parité, et G1-b un
dépassement.

Côté nodal, G1 se joue en **deux nœuds** : `shape.extrude` gagne un paramètre `zBottom`
(aujourd'hui codé en dur à 0, `src/cggraph/nodes/shapes/extrude.cpp:52`) et un nœud
`mesh.merge` apparaît (≈40 lignes sur `Mesh::Append`). Le graphe devient :

```
text.contours ──┬─────────────────────────► shape.extrude (z: t → t+h) ─┐
                │                                                        ├─ mesh.merge → sortie
                └─ <forme de plaque> ────── shape.extrude (z: 0 → t) ────┘
```

##### Deux formes de plaque, dont une que la référence n'a pas

**G1-R — plaque RECTANGULAIRE** : l'emprise du texte + une marge, coins vifs ou arrondis.
C'est la forme de la référence (12 triangles, §1 bis), et celle que `text_extrude.h` sait
déjà décrire (`Support::Plate`). Rien à inventer, seulement à sortir de l'union 2D.

Dans les deux formes, **l'épaisseur est un paramètre en millimètres** (D3), et non un
préréglage caché comme chez la référence.

**G1-S — plaque SILHOUETTE : le contour du texte décalé** (ajout du 2026-09-03). La plaque
suit la forme des lettres à distance constante, comme le fond d'un autocollant découpé. Une
seule opération la produit :

```
plaque = InflatePaths(contours du texte, +offset, JoinType::Round, EndType::Polygon)
```

`InflatePaths` **termine par une union** (cf. `stroke_contours.h`), donc les halos de lettres
voisines fusionnent d'eux-mêmes en une région unique — pas de recollage à écrire. Le dépôt
appelle déjà cette fonction en trois endroits, mais **toujours en décalage négatif et sur des
chemins ouverts** (`architecture_gothic.cpp:529`, `image_region_pipeline.cpp:128`,
`stroke_contours.cpp:51`) : le cas « polygone fermé, delta positif » demande un emballage de
~20 lignes en cgmesh (`offsetContours(contours, delta, join, miterLimit)`), sur le modèle
exact de `strokeToContours`. **Aucune primitive géométrique nouvelle**, et l'énumération
`StrokeJoin{Round, Miter, Bevel}` existe déjà pour dire la forme des angles — Round pour le
halo classique, Miter pour des pointes (avec sa limite), Bevel pour l'entre-deux.

Nodalement, c'est **un nœud générique de plus**, `shape.contours.offset`, qui sert
identiquement à `svg.contours` et aux régions d'image :

```
text.contours ──┬──────────────────────────────► shape.extrude (z: t → t+h) ─┐
                │                                                             ├─ mesh.merge
                └─ shape.contours.offset (+o) ── shape.extrude (z: 0 → t) ────┘
```

**Bénéfice d'ordonnancement** : ce même emballage d'offset est le cœur de l'étape 4 (les
bandes du chanfrein, du biseau et du congé sont des `InflatePaths` successifs). L'écrire ici
paie deux fois, et c'est un argument pour garder l'étape 3 avant l'étape 4.

**Trois conséquences à connaître, et elles sont toutes mesurables :**

1. **La connexité n'est pas garantie — c'est LE piège de cette forme.** Le halo ne relie deux
   lettres que si `offset` vaut au moins la moitié de l'espace qui les sépare. En dessous, la
   région décalée compte **une composante par lettre** : la pièce sort en lettres libres, ce
   qui est correct géométriquement et inutilisable en pratique. Le nombre de composantes de
   premier niveau est **directement lisible** dans un `PolyTreeD` Clipper2 : le vérifier, et
   soit refuser, soit relever l'offset, soit le dire. À ne pas laisser découvrir à
   l'impression.
2. **Les contre-formes se REFERMENT.** Décaler la matière vers l'extérieur rétrécit les trous
   d'autant : le centre du `o` perd `offset` de chaque côté et **disparaît** dès que l'offset
   atteint sa demi-largeur. C'est **souhaitable** quand la silhouette est un fond sur lequel
   les lettres se posent en relief (le trou du `o` doit être bouché, sinon on voit à travers
   la plaque), et rédhibitoire si la silhouette était la pièce entière. **Tranché le
   2026-09-03 (D2) : la silhouette est un FOND, et rien d'autre, dans ce lot** — donc les
   contre-formes refermées sont le comportement voulu, à documenter et non à corriger. La
   silhouette lue seule est un autre produit, proche du pochoir (A5).
3. **Les cous sont fragiles.** Là où deux lettres se relient tout juste, le halo forme un
   étranglement ; plus fin que quelques passes de buse, il casse. Même remède que le point 1 :
   l'offset minimal de connexité plus une marge.

⚠ **Ne pas ranger G1-S dans `TextExtrudeOptions::Support`.** Cette énumération est soudée à
l'union 2D à profondeur partagée — c'est la contrainte même que G1 lève. La silhouette
appartient à la composition nodale, où elle est une plaque comme une autre, extrudée sur sa
propre plage de Z. L'y ajouter recréerait le défaut qu'on corrige.

#### G2 — Arêtes profilées : **chanfrein, biseau, congé** (décision du 2026-09-03)

**Périmètre arrêté** : trois formes d'arête sont autorisées, et non le seul réglage scalaire
de la référence. Conséquence directe sur la conception — **le port `splayProfile` n'est plus
une généralisation « pour plus tard », il est la forme même du nœud dès la première
version**. Ce que couvrent les producteurs de `src/cgmesh/profile2d.h` :

| Forme demandée | Producteur | État |
|---|---|---|
| **Chanfrein** — coupe droite à 45° | `chamferSplayProfile(w, d)` avec `w == d` | ✅ existe |
| **Biseau** — coupe droite à angle libre | `chamferSplayProfile(w, d)` avec `w ≠ d` | ✅ existe |
| **Congé** — arête roulée, **arrondi convexe** | `cavettoSplayProfile(w, d, segments)` | ✅ existe — ⚠ voir ci-dessous |

**« Congé » = arrondi CONVEXE — arbitré le 2026-09-03.** Le mot est ambigu et il fallait
choisir : dans le registre de l'architecture — celui qu'épouse `profile2d.h` — un congé est
un raccord **concave**, et c'est `cavettoSplayProfile` ; dans le registre CAO (le « fillet »
de Fusion ou SolidWorks), un congé posé sur une arête **saillante** rend un arrondi
**convexe**. C'est le second qui est retenu pour ce module : sur une lettre, il **roule
l'arête** au lieu de **creuser le flanc**.

⚠ **CORRIGÉ PAR L'IMPLÉMENTATION (étape 4).** Ce paragraphe annonçait un producteur
`roundoverSplayProfile` à écrire, en raisonnant sur les tangences du cavet. **La mesure dit
le contraire** : sous la lecture du consommateur qui a finalement été écrit — l'anneau du
dessus rentré de `v_max`, la section pleine atteinte à `u_max` — le cavet **roule l'arête**
au lieu de la creuser. Le test le tranche par le volume : à largeur et profondeur égales, le
chanfrein est la corde, et `volume(cavetto) > volume(chanfrein)` prouve que la courbe bombe
au-dehors. **Aucun producteur n'a été ajouté.** Ce qui suit reste vrai du reste :

- la leçon générale : **la même courbe rend un congé ou une gorge selon le SENS où le
  consommateur la lit**. C'est pourquoi la forme est désormais nommée par un test
  (`the_cavetto_profile_reads_as_a_CONVEX_roundover`) et non par un raisonnement dans un
  en-tête.
- **Et il ne s'applique pas depuis la même face** — question réglée par D1, dont il est le
  même arbitrage de signe. La convention d'ébrasement de `profile2d.h` (u = enfoncement sous
  la face de référence, v = décalage **vers la matière**, tous deux ≥ 0) décrit une ouverture
  qui se **resserre** en s'enfonçant, et c'est exactement ce que le décalage intérieur
  retenu demande : l'anneau du haut est **rentré** de `r`, la section pleine est atteinte à
  `u = r`. Le profil s'applique donc **depuis la face vers le fond**, sans entorse au type ni
  signe négatif. En mode dilatation (l'option), le même profil se lit dans l'autre sens,
  depuis l'anneau le plus large : **un seul drapeau, côté consommateur, jamais dans le
  profil**.
- `cavettoSplayProfile` **reste au catalogue** et devient une quatrième forme offerte
  gratuitement, mais elle n'est pas ce que l'interface appelle « congé ».
- **Nommage** : la page offre deux entrées — « chanfrein / biseau » et « congé » — pour deux
  producteurs existants, `profile.chamfer` et `profile.cavetto`. La collision de vocabulaire
  redoutée n'a pas lieu d'être : il n'y a qu'un seul producteur courbe.

Trois conséquences sur l'implémentation, toutes en faveur de la voie générale :

1. **Le nombre de bandes n'est plus 1.** Chanfrein et biseau tiennent en une bande d'offset ;
   un congé en demande *n* (le `segments` du producteur). La primitive doit donc être
   **générale dès le départ** — ce qui retire de la table le « MVP à une seule bande » que
   j'y avais mis.
2. **Le profil devient un port, pas un paramètre.** Un nœud `shape.extrude.profiled` avec une
   entrée `splayProfile` : les trois formes sont alors **trois nœuds de profil déjà au
   catalogue** (`profile.chamfer` pour deux d'entre elles, `profile.cavetto` pour la
   troisième), et non trois positions d'un menu. C'est ce que `src/cggraph/nodes/shapes/profile.h:11` annonçait.
3. **Le coût en triangles devient réglable**, là où la référence l'impose : `segments` du
   congé × contours du glyphe. À exposer dans le gabarit avec une borne basse honnête
   (4 à 6 segments suffisent visuellement à cette échelle).

**Reste le piège principal, et il n'est pas atténué par ce périmètre élargi — il empire.**
Le réflexe serait de réutiliser `extrudeProfiledToMesh` (`src/cgmesh/profile2d.h:101`) —
elle consomme déjà un `Profile2D`, donc les trois formes ci-dessus. Mais elle décale les
anneaux **par sommet**, et se protège de l'auto-intersection en **retombant sur une paroi
verticale** dès que la courbure concave cumulée d'un contour dépasse ~45°
(`profile2d.cpp:232`, `concaveTurn > 0.8`) ou que le rayon caractéristique est trop petit
(`charR`, ligne 194). Un `E`, un `M`, la moindre empattement franchissent ce seuil : la
majorité des lettres sortiraient **sans arête profilée, en silence**. Le garde-fou est
correct pour les remplages gothiques auxquels il était destiné ; il ne transpose pas — et
plus le profil compte de points (le congé), plus l'offset par sommet a d'occasions de se
recouper.

✅ **SENS DU DÉCALAGE — tranché le 2026-09-03 : décalage INTÉRIEUR par défaut**, dilatation
exposée en option (D1, §8). La mesure (§1 bis) montre que la référence **dilate** : Z total =
`depth` + 2 × `bevel`, et l'emprise XY croît d'autant — c'est pourquoi elle affiche 26,86 mm
pour 24 demandés. Le décalage intérieur préserve au contraire l'emprise nominale, seule
convention compatible avec les cotes de l'étape 1 et la hauteur de lettre de l'étape 2 :
**24 mm demandés = 24 mm mesurés**. Les deux s'implémentent avec la **même** primitive, au
signe près, donc l'option ne coûte qu'un paramètre — et le défaut choisi ramène le risque de
fin de section, qu'il faut donc traiter et non plus contourner.

La voie robuste est celle que Clipper2 rend praticable : **bandes d'offset**. Pour un profil
échantillonné en *n* pas (u_i, v_i), on calcule les anneaux `R_i = InflatePaths(texte, −v_i,
JoinType::Round, EndType::Polygon)`, puis on tessellle chaque **couronne**
`A_i = Difference(R_i, R_{i+1})` en attribuant à chaque sommet le Z de l'anneau dont il
provient. `InflatePaths` gère nativement l'auto-intersection, les scissions de contour et
la disparition d'un contour trop mince — les trois cas où l'offset par sommet échoue.

Le même code sert donc les trois formes : `n = 1` pour le chanfrein et le biseau, `n =
segments` pour le congé. **Aucune branche par forme** — c'est le profil échantillonné qui
décide, et un profil de plus (l'arrondi convexe) n'ajoute pas une ligne au consommateur.

Risque à porter au plan : sur une cursive à déliés fins (`GreatVibes.ttf`, signalée « cas
dur » dans le catalogue), un biseau plus large que la moitié du délié **fait disparaître la
partie fine**. Comportement correct au sens géométrique, surprenant à l'usage : il faut le
borner (biseau ≤ une fraction de l'épaisseur de trait minimale mesurée) ou le dire.

#### G3 — Unités millimétriques, hauteur de lettre, cotes affichées

Trois écarts distincts, tous petits, tous visibles :

1. **`size` est le corps em, pas la hauteur de lettre.** stltext règle une hauteur de
   lettre ; l'em est ~1,4 × la hauteur de capitale selon la police. Deux polices au même
   `size` ne rendent pas des lettres de même hauteur. `src/cgmath/font.h` expose
   `unitsPerEm`, `vMetrics`, mais **pas la hauteur de capitale** ; elle se dérive de la
   bbox du glyphe `'H'` (via `glyphContours`) — quelques lignes, sans dépendance nouvelle.
   **Tranché (D4) : la page expose la HAUTEUR DE LETTRE**, le corps em restant l'unité
   interne. Et `letterSpacing` s'exprime en **millimètres**, non en fraction de cadratin —
   ce sont les unités d'un atelier.
2. **Aucune convention d'unité.** Les bornes du gabarit (`size` ≤ 10, `depth` ≤ 5,
   `text3d.json:64-77`) sont des unités abstraites. Passer en mm est un changement de
   **bornes et de défauts**, pas de moteur — mais il faut y penser pour `flattenTol`
   (défaut 0,01, exprimé en **unités monde** par conception,
   `src/cgmesh/text_extrude.h:48`) : à `size` = 30 mm, 0,01 mm de tolérance de corde
   produit des contours **inutilement denses**. Le défaut doit devenir relatif au corps
   (p. ex. corps/1000) ou la finesse se règle en « fraction du corps ».
3. **Pas de cotes à l'écran.** Le pied de page n'affiche que l'état du calcul
   (`template.js:172`). La bbox se calcule côté JS depuis `graphMeshData` (positions) ou se
   lit dans o3dv. Petit, et c'est ce que l'utilisateur regarde avant d'imprimer.

Limite de moteur à connaître, déjà documentée comme dette : `ParamSet` ne porte **ni
bornes ni unité** — d'où les métadonnées dans le gabarit (`text3d.json:6-18`, renvoyant à
`debt_cggraph.md`). Tant qu'elle tient, une unité affichée est une chaîne du gabarit, pas
une propriété du paramètre.

#### G4 — Export STL binaire depuis la page gabarit

L'encodeur est écrit et commenté (`exporters.js:43`), mais il consomme `Module.meshData(id)`
— le chemin des **formes**, qui suppose un objet natif identifié. La page gabarit n'en a
pas : elle a un graphe et un port de sortie, lu par `Module.graphMeshData(0)`
(`wasm_api.cpp:775`, déjà appelé par `template.js:263`). **C'est une dizaine de lignes** :
une variante `downloadStlFromGraph`, un bouton dans `text.html`, une entrée dans le
gabarit. Le commentaire d'en-tête de `exporters.js:11` note d'ailleurs que
`MeshIO::export_stl_binary` existe côté C++ si l'on préfère y déplacer la sérialisation.

### 3.4 Améliorations notées, hors du premier lot

Rien ici ne bloque la livraison ; tout y est consigné pour ne pas avoir à le redécouvrir.

#### A1 — Porte-clés (`Build style: Keychain`) — **retiré du périmètre le 2026-09-03**

Écarté de ce lot par décision, alors qu'il figure comme troisième préréglage chez la
référence. Ce n'est **pas** un renoncement coûteux : la mesure (§1 bis) a montré que l'anneau
y est une **coque indépendante de 1 536 triangles**, à la cote du socle, accrochée au
**socle** et non aux lettres. Donc, quand il reviendra :

- un contour d'anneau (deux cercles concentriques, le perçage étant le contour intérieur —
  un simple trou au sens NonZero, sans aucun booléen) ;
- `shape.extrude` à la cote du socle, puis `mesh.merge` ;
- soit **exactement les nœuds de l'étape 3**, plus un générateur de **contours primitifs**
  (le cercle) — la même brique que réclame A6, et la seule à écrire pour les deux. Le report
  ne crée aucune dette : rien à prévoir aujourd'hui pour le rendre possible demain.

La réserve que j'avais d'abord inscrite — vérifier que l'anneau **touche réellement** une
lettre — tombe d'elle-même : accroché au socle, il n'a pas à toucher de lettre.

#### A2 — Texte creusé (gravé) dans une plaque

**Absent de la référence.** J'avais écrit ici « `Difference(plaque, texte)`, le nœud booléen
2D de l'étape 4 » : **c'est faux, et le booléen n'est pas nécessaire.** Vérification faite,
la soustraction est déjà faite par le **tessellateur**, via la règle de remplissage — pas par
un booléen de polygones.

**Ce que le creusement engendre, plan par plan.** Une plaque d'épaisseur `t` gravée d'une
profondeur `h` n'est pas un solide de plus : c'est le **même empilement 2,5D**, à un étage
près.

| Élément | Comment il est produit |
|---|---|
| Fond de plaque, z ∈ [0 ; t−h] | `Append(plaque, …)` — capot bas, capot haut, parois extérieures |
| **Fond des cuvettes** | le capot haut du même Append, dans la partie couverte par les lettres : il regarde déjà vers le haut, **rien à produire** |
| Dalle supérieure, z ∈ [t−h ; t] | `Append(plaque + lettres marquées TROUS, …)` — le capot est la région « plaque moins lettres », et les **parois des cuvettes sont les parois des contours-trous** |

Les deux mécanismes qui rendent cela gratuit existent et sont en production :

- **`ExtrudeContour::isHole` + `normalizeOrientation`** : un contour marqué trou est réalimenté
  en sens inverse avant tessellation (`extrude_contours.cpp:195-199`), le remplissage NonZero
  le soustrait, et `Append` émet **quand même ses parois** — c'est exactement ainsi que
  `region_append_wall` (`image_region_pipeline.cpp:372`) fabrique son cadre rectangulaire
  aujourd'hui. Un anneau et une cuvette sont la même construction.
- **La règle de remplissage est déjà paramétrable** : `ExtrudeWinding::EvenOdd` est câblée sur
  `GLU_TESS_WINDING_ODD` (`extrude_contours.cpp:167-169`).

**Le seul point qui demande de l'attention — les contre-formes.** `text_to_contours` **ne
renseigne pas `isHole`** : il s'appuie sur l'orientation que Clipper2 donne à sa sortie
(extérieurs dans un sens, contre-formes dans l'autre), ce que `shape.extrude` documente
explicitement en refusant toute renormalisation (`shapes/extrude.cpp:56-60`). Donc
l'imbrication **est déjà connue**, encodée dans le signe de l'aire — et
`image_region_pipeline.cpp:146` fait déjà exactement cette dérivation
(`isHole = (area > 0)`). Pour graver, il faut **inverser le rôle** de chaque contour de
texte : les extérieurs de glyphe deviennent des trous dans la plaque, les contre-formes
redeviennent de la matière (l'îlot du `o` repose alors sur le fond de la cuvette). C'est une
transformation d'une ligne sur la liste de contours, pas une analyse d'imbrication.

**Ce qu'il reste à écrire** : un nœud de composition de contours — deux entrées (région,
évidements), concaténation et attribution des rôles `isHole` par signe d'aire, le second jeu
inversé. Une quarantaine de lignes, **aucune géométrie**. Plus `zBottom` et `mesh.merge`,
déjà à l'étape 3.

**Le défaut résiduel est celui de G1-a, à l'identique** : le capot haut du fond et le capot
bas de la dalle coïncident sur la région « plaque moins lettres » — une membrane interne.
`Append` sait déjà supprimer des **parois** internes (`wallFilter`, utilisé par les blocs
pixelisés) mais pas des **capots**. Deux drapeaux `emitBottomCap` / `emitTopCap`, sur le
modèle de `emitWalls`, donneraient une coque unique — et **profiteraient aussi au relief et
aux blocs pixelisés**, qui portent la même membrane aujourd'hui.

**Deux garde-fous à écrire, parce que rien ne les tient :**
1. `h < t` **strictement**. `h == t` ne rate pas, il produit un **pochoir** (découpe
   traversante) — voir A5, c'est un produit, pas un bug. `h > t` n'a pas de sens : refuser.
2. **Lettres contenues dans la plaque.** Un texte qui déborde du bord (marge trop faible)
   fait déboucher la cuvette sur le flanc et peut trancher la plaque en deux. Un test
   d'inclusion suffit, et Clipper2 le rend en une ligne.

**Ce que le creusement change à l'usage, et qui n'est pas géométrique** : une gravure se lit
par l'ombre, pas par la silhouette. Un sillon plus étroit qu'une passe de buse (~0,4 mm)
**disparaît** au tranchage, et une buse ronde **arrondit tous les angles rentrants** — l'angle
de l'`A` gravé s'émousse. À taille et police égales, une gravure est donc **moins lisible
qu'un relief**, et demande des lettres plus grandes. À dire dans l'interface plutôt qu'à
laisser découvrir. Choisir de plus `h` multiple de la hauteur de couche (0,2 mm) évite un
fond de cuvette qui tombe au milieu d'une couche.

#### A3 — Miroir (moules, tampons)
Un `flip_x` sur les contours, ou un nœud de transformation 2D — absent du catalogue.
**Absent de la référence.**

#### A4 — Socle percé, coque unique manifold (G1-b)
Décrit en G1. **Dépasse la référence**, qui livre des coques interpénétrées.

### 3.3 Risques identifiés

| Risque | Nature | Atténuation |
|---|---|---|
| Étanchéité de G1-a | faces internes entre socle et lettres ; STL multi-coques | acceptable pour slicer ; viser G1-b pour un export propre |
| Déliés fins mangés par l'arête profilée | géométrique, pas un bug — et **actif par défaut** depuis D1 (décalage intérieur) | mesurer l'aire de l'anneau : elle tombe à zéro, donc la disparition est **détectable** ; borner le réglage, le signaler, ou proposer le mode dilatation qui l'annule |
| `flattenTol` en unités monde | passage aux mm ⇒ explosion du nombre de segments | défaut relatif au corps ; garde-fou sur le nombre de points |
| Fonte variable (`fvar`) | stb_truetype lit **toujours** l'instance par défaut (`catalogue.json:8-12`) | déjà documenté, ne pas promettre le Bold |
| Pas de booléen 3D en WASM | OCCT n'est lié qu'en natif (`src/cgmesh/CMakeLists.txt:246`, exclu de la liste Emscripten) | rester en 2,5D — ce que fait tout ce plan |
| Congé = *n* bandes | triangles × *n*, et *n* offsets Clipper2 par glyphe | `segments` exposé et borné bas (4-6 suffisent à cette échelle) ; chanfrein et biseau restent à 1 bande |
| Plaque silhouette non connexe | offset < demi-espace entre lettres ⇒ une composante par lettre, pièce en morceaux | compter les composantes du `PolyTreeD` ; refuser, relever l'offset, ou le dire (§3.2 G1) |
| Contre-formes refermées par l'offset | le centre du `o` se bouche dès que l'offset atteint sa demi-largeur | **voulu** : D2 fait de la silhouette un fond sous des lettres en relief, où le trou doit être bouché — à documenter, pas à corriger |
| Sens d'application du congé | **réglé** : D1 retient le décalage intérieur, donc la convention d'ébrasement s'applique telle quelle (§3.2 G2) | un drapeau côté consommateur pour le mode dilatation, jamais dans le profil |
| Coût de l'union Clipper2 | union systématique de tous les glyphes (`text_extrude.cpp:133`) | déjà en place et mesuré à l'usage ; les anneaux du biseau la multiplient par *n* pas ⇒ garder *n* petit |

#### A5 — Pochoir (découpe traversante)

Le cas `h == t` du précédent, et **le plus propre de tous** : un seul `Append` sur
z ∈ [0 ; t] avec les lettres en trous, donc **une coque unique, sans membrane interne, sans
booléen**. Une limite réelle en revanche, et classique : les contre-formes deviennent des
**îlots détachés** (le centre du `o`, du `a`, du `e` tombe). Il faut les détecter — un contour
de profondeur d'imbrication paire, entièrement entouré de vide — et les **relier par des
ponts**, ce qui est un travail de conception à part entière, sans rapport avec le reste de ce
document.

#### A6 — Fixation murale (perçages, fraisures, trous de serrure, aimants)

**Consigné le 2026-09-03, hors lot ; le BANDEAU À OREILLES PERCÉES est livré le 2026-09-04**
(`mesh.mounts`, voir plus bas). Rendre le modèle accrochable à un mur : c'est une famille
entière, et elle se dit **tout entière dans le vocabulaire 2,5D déjà établi**, sans une
primitive géométrique de plus. Le tableau ci-dessous décrit la famille ; ce qui en est fait,
et pourquoi la forme livrée n'est aucune de ses lignes prise isolément, suit le tableau.

| Forme de fixation | Construction | Mécanisme déjà décrit |
|---|---|---|
| **Trou de vis traversant** | un contour de cercle en rôle `isHole` dans la région de la plaque | A5 (découpe traversante) |
| **Fraisure** (tête de vis affleurante) | le même trou, **arête profilée** : un chanfrein sur un contour-trou l'évase vers la face | étape 4, signe inversé par le rôle du contour |
| **Trou de serrure** (passage de tête + coulisse) | cuvette en face ARRIÈRE — disque ∪ oblong — plus un perçage traversant plus petit | A2 mirroré en Z ; l'union vient du **remplissage NonZero**, pas d'un booléen |
| **Pattes / oreilles de fixation** | contours ajoutés à la région de la plaque, mêmes orientations | remplissage NonZero |
| **Logement d'aimant** | cuvette cylindrique en face arrière, profondeur = épaisseur de l'aimant | A2 mirroré en Z |

**La seule brique à écrire est un générateur de contours primitifs** — cercle, rectangle,
rectangle arrondi, oblong (« stade ») — soit un nœud `shape.contours.primitive` d'une
quarantaine de lignes, sans dépendance. Le dépôt n'a rien de tel côté 2D : `region_make_rect`
(`image_region_pipeline.cpp:347`) ne fait que le rectangle et vit à l'intérieur de la chaîne
image. **C'est la même brique que A1** (le porte-clés a besoin du cercle) : les deux
améliorations se débloquent d'un seul coup.

Ce qui est **déjà acquis** pour cette famille : l'ajout de matière par contours de même
orientation et le retrait par rôle `isHole` (A2), la cuvette et son fond (A2), l'arête
profilée sur un trou (étape 4), la composition et la fusion (étape 3).

##### Livré le 2026-09-04 — `mesh.mounts`, un bandeau à deux oreilles percées

Demande initiale : des **pastilles** (disque troué) ou des **trous aux extrémités**, dans le
module de texte, mais conçus pour servir à tout autre modèle.

**La première conception a été écartée par deux objections, et elles étaient justes.** Elle
posait la fixation *avant* l'extrusion, sur les contours, et laissait le placement se déduire
de l'emprise. D'où :

1. *« ça ne vient pas au début de la génération, mais à la fin, justement dans un souci de
   généralisation »* — une fixation posée sur les contours ne sert que les producteurs de
   contours. Les nœuds qui rendent directement un maillage (formes paramétriques, baie
   gothique, fichier importé) en restaient exclus : exactement le contraire du but ;
2. *« si le modèle est plus bas à gauche et plus haut à droite, les deux points de fixation
   ne seront pas forcément horizontaux »* — et c'est décisif. **Les vis d'un mur sont de
   niveau.** Deux points pris « aux extrémités » du contour sont à des hauteurs quelconques ;
   la pièce pendrait de travers.

**Ce que ces deux objections imposent ensemble.** La ligne de fixation doit être **imposée**,
jamais déduite : une hauteur, et deux centres sur cette hauteur. Mais la matière n'est pas
forcément là à cette hauteur-là — ce qui relie les deux centres au corps est donc une bande
horizontale. Il n'y a alors pas deux objets à concevoir, un « support » et des « pastilles » :
**la fixation EST un bandeau dont les extrémités sont percées.**

Et ce bandeau **AJOUTE** de la matière. Ajouter ne demande aucun booléen 3D — la fixation est
un solide autonome, ses trous sont intérieurs à son propre contour, et elle se colle par
concaténation comme le socle. Elle s'applique donc à **n'importe quel maillage**, ce que la
première conception ne pouvait pas promettre. C'est le point 1 de l'objection, résolu par la
géométrie et non par une couche de plus.

| | Première conception (écartée) | `mesh.mounts` (livré) |
|---|---|---|
| Où | avant l'extrusion, sur les contours | **après tout**, sur le maillage |
| Portée | les producteurs de contours seulement | **tout maillage**, quelle qu'en soit l'origine |
| Horizontalité | déduite de l'emprise, donc fausse sur une forme oblique | **imposée** par le bandeau |
| Booléen | 2D (union + différence sur la pièce) | aucun sur la pièce : union et différence **de la fixation avec elle-même** |

**Le nœud.** `mesh.mounts`, catégorie « Maillage », une entrée et une sortie de maillage. Six
réglages : hauteur de ligne (**fraction** de la pièce, 0,5 par défaut — une fraction et non
des millimètres, pour qu'un changement de taille du texte ne déplace pas la fixation), hauteur
du bandeau, épaisseur, débord, diamètre d'oreille, diamètre de trou.

Sa géométrie tient en cinq lignes : `roundedRectContour` pour le bandeau tendu **de centre à
centre** — ce qui rend la soudure oreille/bandeau acquise par construction, quel que soit le
débord —, deux `circleContour` pour les oreilles, `unionContours`, puis `differenceContours`
pour les deux trous. **L'ordre n'est pas indifférent** : le bandeau va jusqu'aux centres, donc
il recouvre les trous ; les poser comme contours inversés dans la même région les reboucherait
là où il passe, le compte NonZero y redevenant non nul. Extrusion du **bas** de la pièce vers
le haut, sur l'épaisseur demandée — c'est ce qui la fait mordre dans le socle quand il y en a
un, et dans les lettres sinon, les deux commençant au plateau.

**L'entraxe est publié** (`Node::PublishStats`), et c'est la seule chose que le nœud mesure :
il ne se règle pas — il tombe de l'emprise et du débord —, et c'est pourtant le seul chiffre
dont on ait besoin devant le mur, perceuse en main. La page l'affiche dès que la fixation est
demandée.

**Ce qu'il refuse plutôt que de le rendre** : un trou aussi large que son oreille, un bandeau
sans hauteur, une épaisseur nulle, un débord si négatif que les deux centres se croisent.
Chacun donnerait une pièce qu'on ne découvrirait qu'après impression.

⚠ **La limite, dite plutôt que masquée.** Le nœud lit une **emprise**, pas une forme : il pose
son bandeau en travers de la boîte englobante sans savoir si de la matière s'y trouve. Sur une
pièce ordinaire — un texte, une plaque, un relief — il la traverse et se soude. Sur un modèle
en **anneau**, il passerait dans le vide central et sortirait en pièce libre. Le vérifier
demanderait un test de recouvrement entre solides, qui est un autre chantier.

⚠ **Il ne perce que ce qu'il apporte.** Percer la pièce existante retirerait de la matière,
donc demanderait ses contours (chaîne 2,5D) ou un booléen 3D que le dépôt n'a pas. Le reste de
la famille du tableau ci-dessus — trou de serrure, logement d'aimant — est donc encore à faire
au niveau des contours ; c'est là que `shape.contours.primitive` garde son emploi.

##### Où s'arrêtent les bouts — mesuré À LA HAUTEUR, pas sur l'emprise (2026-09-04)

Première version : les deux bouts venaient de la **boîte englobante** du modèle. C'est faux
dès que la pièce n'est pas un rectangle. Un L va jusqu'à la pointe de son pied en bas, et pas
plus loin que son montant à mi-hauteur ; une fixation posée à mi-hauteur d'après l'emprise
totale débordait de plusieurs millimètres **dans le vide**, oreille comprise.

Les bouts sont donc pris sur ce que le maillage occupe **dans la tranche du bandeau** — la
tranche étant exactement celle que le bandeau va occuper (`cgmesh/mesh_slab.h`).

**Pourquoi la surface suffit, et qu'il n'y a rien à reconstruire.** Le point le plus à gauche
de (solide ∩ tranche) est sur le bord de cette intersection, donc sur la **peau** du solide :
une section plane a son bord sur la surface. Parcourir les triangles et retenir, pour chacun,
ses sommets dans la tranche plus ses traversées des deux plans donne l'étendue **exacte**, sans
calculer aucune section. O(faces), quelques dixièmes de milliseconde sur une pièce de 7 000
triangles.

**Ce que la nouvelle mesure gagne** : la fixation se resserre sur la matière, et surtout ses
deux extrémités tombent **sur** de la matière puisqu'elles sont prises dessus — ce que l'emprise
totale ne garantissait pas. Une tranche vide (un bandeau mince tombant entre deux lignes de
texte) est refusée : c'est la seule réponse honnête.

⚠ **La limite change de nature sans disparaître.** Connaître les deux bouts n'est pas connaître
ce qu'il y a entre eux : sur un modèle en **anneau**, les deux bouts sont bien sur la matière,
mais le bandeau traverse le vide central. Le vérifier demande un test de recouvrement entre
solides — un autre chantier.

**Le refus qu'il faut savoir lire — signalé à l'usage comme « la fixation ne se met plus à
jour ».** Sur un texte de **plusieurs lignes**, le bandeau à mi-hauteur tombe dans le blanc
entre deux lignes : la tranche est vide, il ne se souderait à rien, et le nœud refuse. Or un
refus, dans la page, **gèle la vue sur le modèle précédent** — ce qui se lit exactement comme
« rien ne s'est mis à jour », d'autant que le seul indice était un « calcul refusé ·
mesh.mounts » sans explication.

Le refus est le bon comportement : l'accepter produirait une barre libre dans le même STL,
qu'on ne découvrirait qu'au démoulage. Ce qui manquait est le **conseil de panne**
(`failureHints`, nœud 15), le mécanisme prévu pour cela par le gabarit — un nœud ne peut pas
motiver son refus, `Compute` ne rendant qu'un booléen. Il nomme la cause, les trois issues
(déplacer la ligne sur une ligne de texte, élargir le bandeau jusqu'aux lettres, poser un
socle) et le fait que la vue est celle d'avant. Un cas de test garde la géométrie qui provoque
le refus **et** vérifie que les deux issues qu'il recommande fonctionnent — un conseil qui
enverrait dans le mur serait pire que pas de conseil.

##### Ce que la vérification a fait tomber : une MESURE QUI MENTAIT

L'entraxe est devenu variable avec la hauteur de la ligne, et il s'est mis à mentir en page :
il restait figé sur le dernier réglage **neuf**. Le défaut était antérieur et général, pas
propre à la fixation — il ne se voyait pas parce qu'aucun compteur ne changeait aussi souvent.

**La cause, en deux temps.** `Node::PublishStats` publie les mesures du **dernier calcul**. Or :

1. un **succès de cache** ne calcule pas : le nœud garde alors les mesures d'une autre
   signature, exactes et sans rapport avec ce qu'on regarde ;
2. pire, un succès de cache **retourne tôt** — la branche sous le nœud n'est pas descendue. La
   page évaluant la *sortie*, un retour de cache à la racine laissait toute la chaîne, fixation
   comprise, sans passer par aucun site d'enregistrement. Sur une page à graphe fixe, où l'on
   revient sans cesse sur des réglages déjà vus, c'était le cas **ordinaire**.

**Le correctif** suit la voie que les vignettes empruntaient déjà (`TakeRunPreviews`, dont
l'en-tête notait exactement ce piège pour son propre compte) : les mesures accompagnent
l'entrée de cache, sont republiées quand elle ressert, et un relevé de **toute la branche** après
le calcul rattrape les nœuds qu'un retour anticipé n'a pas visités. Elles voyagent ensuite dans
`Completed` jusqu'au modèle, et `graphNodeInfo` les lit **du parcours** et non du nœud.

Deux cas de test le tiennent, dont un qui montre le mécanisme plutôt que de le décrire : à
l'instant du succès de cache, le nœud porte encore, et légitimement, la mesure d'une autre
hauteur.

##### La FRAISURE — livrée le 2026-09-04, et construite à l'envers

Une tête de vis fraisée doit **affleurer**, sinon la pièce porte sur la tête et non sur le mur.
Le creux qu'elle demande est un **cône**, et le dépôt n'a aucun booléen 3D pour le retirer.

**Le retournement.** On ne creuse pas : on perce trop, puis on remet. La pièce fraisée est
l'union de **trois solides fermés**, tous ajoutés :

1. le bandeau, percé au diamètre de la **bouche** (le grand), de part en part — donc trop
   large, et c'est voulu ;
2. deux **bagues** (`cgmesh/countersink.h`), une par trou : pleines en bas jusqu'au diamètre
   du trou de vis, évidées en cône vers le haut. Elles remettent la matière qu'il ne fallait
   pas retirer.

C'est le même procédé que le socle sous les lettres — des coques qui s'interpénètrent et que
le trancheur unifie — à ceci près qu'ici il sert à **retirer** de la matière sans jamais
soustraire. C'est, à ma connaissance, le seul endroit du dépôt où ce renversement est utilisé,
et c'est ce qui rend la fraisure possible sans booléen.

**Le recouvrement (`bite`) n'est pas une précaution décorative.** Sans lui, la paroi extérieure
de la bague et celle du perçage seraient exactement confondues — deux surfaces opposées au même
endroit, qu'un trancheur arbitre comme il peut. La bague mord donc de quelques centièmes dans
la plaque. Le prix se lit dans les tests : un volume signé étant additif, ce chevauchement est
compté **deux fois** dans la somme des coques, alors que la pièce imprimée ne le contient
qu'une. Le cas de test le nomme et le retranche, plutôt que d'élargir la tolérance jusqu'à ce
que ça passe.

**La profondeur n'est pas un réglage.** Diamètre de bouche, diamètre de trou et angle au
sommet la déterminent entièrement ; l'offrir en quatrième serait offrir de décrire un cône qui
n'existe pas. Deux réglages seulement, donc : le **diamètre** (0 = aucune fraisure, et la pièce
est alors exactement celle d'avant) et l'**angle** (90° métrique, 82° pouce).

**Ce qui est refusé plutôt que rendu** : une bouche plus étroite que le trou, une bouche plus
large que l'oreille, un angle hors de [30°, 170°], et surtout une fraisure **traversante** —
qui ne laisserait aucune portée cylindrique pour guider la vis. Sur la plaque de 2 mm par
défaut, un Ø9 pour vis de 4 est donc refusé : il faut passer à 3 ou 4 mm d'épaisseur, ce que
la page dit dans l'infobulle du réglage.

**Ce que cela coûte** : 320 triangles par bague, soit 640 sur la pièce. Le solide est une
révolution à cinq anneaux — fond, paroi extérieure, liston du dessus, portée cylindrique,
cône — dont l'orientation est vérifiée par le volume signé : une bague retournée se
soustrairait de la pièce, et rien à l'écran ne le dirait.

**Dans le graphe du gabarit** : deux nœuds de plus (14 → 16). `mesh.mounts` et le
`flow.select.mesh` qui le rend facultatif — une page à graphe FIXE ne débranche pas, elle
choisit entre deux branches (D9). Placés **avant** `mesh.color`, sans quoi le bandeau
sortirait gris pendant que le reste est peint.

**Dans la page** : un bloc « Fixation murale », replié comme les autres, sept réglages.

**Ce qui a été écrit pour cela** : `circleContour` dans `contour_ops` (la première pièce du
générateur de contours primitifs qu'attendent A1 et A5), le nœud (~110 lignes), quatre cas de
test — dont un qui **pèse** la fixation par volume signé, additif sur des coques fermées même
imbriquées, et prouve ainsi que les trous sont réellement percés et non seulement dessinés.

**Quatre points d'ingénierie à ne pas découvrir à l'impression :**

1. **Le trou doit tomber dans la matière, et à distance du bord.** Sur une plaque
   rectangulaire c'est évident ; sur une **plaque silhouette** (G1-S) la matière suit les
   lettres et il n'y a pas forcément de place. Garde-fou du même type que celui de la gravure :
   dilater le trou de la paroi minimale voulue et vérifier que le résultat reste inclus dans
   la plaque — une ligne de Clipper2.
2. **Jeu de perçage.** Un trou imprimé sort sous-coté (l'extrusion mord vers l'intérieur) :
   prévoir Ø nominal + 0,4 à 0,6 mm, ou l'exposer comme réglage plutôt que de le coder.
3. **Épaisseur suffisante.** Une fraisure demande `t ≥ hauteur de tête + ~1 mm` de matière
   restante, un logement d'aimant `t > épaisseur de l'aimant`. À vérifier contre l'épaisseur
   de plaque (D3), qui est désormais un paramètre — donc vérifiable.
4. **Le ligament du trou de serrure est le point faible**, et il travaille en traction là où
   les couches se délaminent le plus volontiers. À dimensionner large, ou à préférer deux
   perçages traversants — plus laids, bien plus solides.

---

## 4. Plan incrémental

Chaque étape est **livrable seule** et améliore la page existante ; aucune ne dépend d'une
étape ultérieure.

### Étape 1 — Utile, imprimable, sans une ligne de C++ — ✅ **FAITE le 2026-09-03**

Livré : `downloadStlFromGraph` + `meshExtent` (`maker/web/js/exporters.js`), bouton STL et
ligne de cotes (`maker/web/text.html`, `js/template.js`), bornes et défauts du gabarit en
millimètres, `letterSpacing` compris (D4). Les documents existants ne sont pas migrés (D5).

**Vérifié dans le navigateur** sur le WASM prébuild, texte « Texte 3D », Blooming Grove,
corps 30 mm :

| Contrôle | Résultat |
|---|---|
| Taille du fichier | 177 284 o = **84 + 3 544 × 50** exactement — STL binaire bien formé |
| Triangles | 3 544, identique au compteur de la page |
| Cotes lues dans le STL | 116,22 × 23,88 × 3 mm — **identiques à l'affichage** |
| Profondeur réglée à 3 puis 8 mm | z ∈ [0 ; 3] puis [0 ; 8] — le réglage veut bien dire des millimètres |
| **Pièce posée sur le plateau** | z commence à **0**, pas à −d/2 : l'avantage noté au §1 bis est réel |
| Étanchéité | **0 arête de bord, 0 arête non-manifold, 0 face dupliquée** |
| Coques | 7, une par glyphe non jointif — cohérent avec l'union 2D du dépôt |

⚠ **Un écart mesuré en notre défaveur : 4 triangles DÉGÉNÉRÉS** (aire nulle), là où les trois
exports de la référence en comptaient zéro. Ils viennent de la tessellation (glutess émet des
esquilles sur certains contours) et non de l'encodeur STL, que la vérification ci-dessus
valide au bit près. Sans conséquence au tranchage — un triangle d'aire nulle ne contribue à
aucun contour —, mais c'est un vrai défaut de propreté, à traiter là où il naît : un filtre
d'aire minimale dans `ExtrudedMeshBuilder::Build`, donc du C++, donc **pas à cette étape**.

⚠ **`flattenTol` reste ABSOLU** (défaut porté de 0,01 à 0,05 mm, bornes 0,01–1 mm). Le rendre
relatif au corps demande de toucher le nœud, c'est-à-dire du C++ : reporté à l'étape 2, qui
en touche déjà. Le libellé le dit à l'utilisateur plutôt que de le laisser deviner.
*Touche* : `maker/web/js/exporters.js`, `js/template.js`, `web/text.html`,
`data/templates/text3d.json`.
**Résultat** : la page devient un outil d'impression. Le reste du lot se réduit à
« socle indépendant » et « arêtes profilées ».

### Étape 2 — Hauteur de lettre — ✅ **FAITE le 2026-09-03**

Livré :
- `Font::capHeight()` (`src/cgmath/font.h/.cpp`) — constatée sur la bbox d'une capitale à
  sommet **plat** (`'H'`, puis `'E'`, `'I'`, `'X'`, `'T'`), mémoïsée, invalidée au
  rechargement. **Pas** lue dans `OS/2` : stb_truetype n'expose pas `sCapHeight`, et la
  valeur déclarée y est souvent absente ou fausse ;
- `TextExtrudeOptions::SizeMode { Em, CapHeight }` + la conversion `emSizeFor()` dans
  `text_extrude.cpp`. **La conversion vit là et pas dans `text_layout`** : celui-ci ne
  connaît que `IGlyphMetrics`, contrat minimal qu'un test implémente aussi — l'y ajouter
  aurait cassé un implémenteur pour un besoin qui n'est pas le sien ;
- `flattenTol == 0` ⇒ **automatique** (`size / 600`), la dette laissée par l'étape 1. Zéro
  était la seule valeur sans signification (une tolérance nulle subdivise sans fin), d'où sa
  réaffectation plutôt qu'un paramètre de plus. `ParameterizedText3D`, seul autre appelant,
  fixe `flattenTol` explicitement : personne en production ne dépendait du défaut ;
- paramètre `sizeMode` sur `text.contours`, exposé au gabarit sous « Mesure de la cote ».
  **Défaut du nœud : `Em`** — un document existant ne doit pas changer de cote en changeant
  de version — mais **défaut du gabarit : `CapHeight`** (D4).

**7 tests ajoutés** (`tu_cgmath_font.cpp`, `tu_cgmesh_text_extrude.cpp`), dont deux témoins
qui échouent si la mesure cesse de prouver quelque chose : le `'O'` doit déborder la hauteur
de capitale, et le mode `Em` doit faire **diverger** deux polices. **158 tests** des suites
touchées passent.

**Vérifié dans le navigateur** — « HEIL », hauteur de lettre demandée **30 mm** :

| Police | em | Mode hauteur de capitale | Mode corps em |
|---|---|---|---|
| DejaVu Sans | 2048 | **30,00** | 21,87 |
| Bebas Neue | 1000 | **30,00** | 21,00 |
| Fira Sans (CFF) | 1000 | **30,00** | 20,67 |
| Cinzel | 1000 | 30,77 | 21,54 |

La promesse tient : **quatre polices, quatre ems, une seule hauteur**. Le mode `Em`, lui,
donne quatre hauteurs différentes dont aucune ne vaut 30 — c'est ce qui justifie le mode.

**L'écart de Cinzel est expliqué, pas subi** : le `'H'` seul y mesure **30,00 mm exactement**
— la conversion est juste. Les 0,77 mm viennent d'une autre capitale de cette fonte
lapidaire, dessinée légèrement au-dessus de la ligne du `H`. À retenir pour l'interface : la
cote est honorée **sur la lettre de référence**, l'encombrement d'un mot peut la dépasser de
1 à 3 % selon la fonte. C'est la définition même d'une hauteur de capitale, pas un défaut.

### Étape 3 — Socle d'épaisseur indépendante, deux formes de plaque (G1) — ✅ **FAITE le 2026-09-03**

Livré, et **portable** (tout est dans la liste EMSCRIPTEN, vérifié dans `maker.wasm`) :

- **`src/cgmesh/contour_ops.h/.cpp`** — les primitives, obtenues en **extrayant** ce que
  `text_extrude.cpp` avait écrit en local plutôt qu'en le réécrivant : `contourSignedArea`,
  `contoursBBox`, `roundedRectContour`, et le nouveau `offsetContours` (Clipper2,
  `EndType::Polygon`, delta signé, `StrokeJoin` réutilisé de `stroke_contours.h`).
  `text_extrude.cpp` consomme désormais les mêmes — il n'y a **qu'une** implémentation ;
- **`shape.extrude` + `zBottom`** : `depth` reste une **épaisseur**, le solide occupe
  `[zBottom, zBottom + depth]`. C'est ce qui rend l'empilement indépendant — déplacer le
  socle ne change pas l'épaisseur des lettres ;
- **`mesh.merge`** (`mesh/merge.h/.cpp`) : concaténation par `Mesh::Append`, **seconde entrée
  optionnelle**. Sans second maillage, l'entrée est **repartagée** et non copiée ;
- **`shape.contours.plate`** (G1-R) : emprise + marge + coins arrondis, sens de tracé aligné
  sur le plus grand contour d'entrée ;
- **`shape.contours.offset`** (G1-S) : la plaque silhouette, avec le **compte de morceaux**
  publié (`GetPieceCount`) — le garde-fou de connexité que le §3.2 réclamait.

**15 tests ajoutés**, deux fichiers : `tu_cgmesh_contour_ops.cpp` (9 — dont le seuil de
connexité **encadré** par deux cas, et la contre-forme qui se referme) et
`tu_cggraph_nodes_stack.cpp` (6 — la chaîne socle + lettres + fusion, le passe-plat sans
second maillage, le refus d'un offset qui consomme la matière). **1 596 tests** au total,
aucune régression.

Les deux tests de **recensement du catalogue** ont rougi, ce qui est leur fonction : mis à
jour explicitement (55 entrées portables, 62 au total, 6 nœuds de maillage).

#### Ce que l'étape a révélé, et comment c'est réglé : **D9**

Le moteur livré, le câblage de la page a buté sur un fait que le plan n'avait pas vu : **un
gabarit ne peut pas exprimer une branche OPTIONNELLE.** Une page pilotée par un graphe fixe
ne débranche rien, elle règle des paramètres — or « avec ou sans socle » et « rectangle ou
silhouette » sont des **variantes**. Trois faits fermaient les échappatoires :

1. `PortDesc::optional` ne couvre que le port **non alimenté** ; un port alimenté par un nœud
   qui **échoue** fait échouer l'évaluation entière, et c'est bien ainsi qu'il faut que ça se
   comporte ;
2. une épaisseur nulle n'éteint pas la branche — l'extrudeur rendrait un solide plat ;
3. rien dans `flow/` ne choisissait entre deux valeurs de même type.

**D9 tranchée le 2026-09-03 : `flow.select`**, livré le jour même — la voie qui garde les
nœuds généraux **et** le document comme source unique.

- **`flow/select.h/.cpp`** : un corps commun, trois entrées (la première obligatoire), un
  paramètre `index` borné, et la valeur **repartagée** telle quelle — le sélecteur ne
  fabrique rien, il choisit ;
- **deux variantes au catalogue**, `flow.select.mesh` et `flow.select.contours`. Et ce n'est
  pas un renoncement à la généricité : **les liens d'un document sont validés à la
  RELECTURE**, donc des ports dont le type dépendrait d'un paramètre seraient encore à leur
  type par défaut au moment de la validation, et un document valide serait refusé. Le seul
  point où un nœud republie ses ports est `RefreshExternalState`, appelé à l'**évaluation** —
  trop tard. Une variante de plus est un constructeur de plus ;
- ⚠ **réserve tenue par un test** (`the_selector_does_not_shield_a_failing_unselected_branch`) :
  **toutes les branches sont évaluées**, y compris celle qu'on ne garde pas. Le sélecteur
  choisit un résultat, il ne rattrape pas une panne. Un gabarit doit donc **borner ses
  branches** pour qu'aucune ne puisse échouer — c'est pourquoi le décalage de la silhouette
  est borné à 0 minimum dans le gabarit.

**5 tests de plus** (11 dans `tu_cggraph_nodes_stack.cpp`), **1 601 au total**.

#### La page offre le socle — vérifié dans le navigateur

Le gabarit passe de **4 à 10 nœuds** : les deux formes de plaque, le sélecteur de contours
qui les arbitre, l'extrusion du socle sur sa propre plage de Z, la fusion, et le sélecteur
« avec ou sans socle » en sortie. Le socle et les lettres **partagent la même valeur** — celle
qui donne l'épaisseur du socle donne la cote de pose des lettres ; deux paramètres distincts
se désynchroniseraient et les lettres flotteraient.

| Réglage | Cotes mesurées | Triangles |
|---|---|---|
| Sans socle | 146,74 × 30,15 × **3** mm | 3 928 |
| Plaque rectangulaire (marge 4, congé 2) | 154,74 × 38,15 × **5** mm | 4 036 |
| Plaque silhouette (décalage 4) | 154,74 × 38,15 × **5** mm | 8 708 |

Les trois lectures se vérifient à la main : `+2 × 4` de marge en X et Y, et `2 + 3` en Z —
**le socle a bien son épaisseur propre**, ce que l'union 2D de `text.contours` ne pouvait pas
faire. La silhouette coûte le double de triangles, ce qui est le prix d'un contour qui épouse
les lettres au lieu de les encadrer.

**Et le piège de la connexité s'est montré tout seul** : à « Texte 3D », les halos relient les
lettres de « Texte » entre elles mais **pas** au « 3D » — deux pièces dans un même STL. C'est
exactement ce que §3.2 G1 annonçait, et le nœud le SAIT (`GetPieceCount`).

⚠ **Ce qui reste, et c'est petit** : le compte de morceaux **n'atteint pas encore
l'interface**. `graphNodeInfo` (`maker/graph_api.cpp:221`) publie type, ports et paramètres,
mais **aucune statistique de nœud** — le même manque prive déjà la page des compteurs de
glyphes de `text.contours`. Le garde-fou existe donc dans le moteur et pas dans l'écran : une
liaison WASM à ajouter, utile à plusieurs nœuds à la fois, et à faire avant de considérer
G1-S livrée pour un utilisateur.

### Étape 4 — Arêtes profilées : chanfrein, biseau, congé (G2) — ✅ **FAITE le 2026-09-03**

La seule étape non triviale du lot, et elle a corrigé **deux affirmations** de ce dossier.

**Livré :**

- **`src/cgmesh/extrude_profiled.h/.cpp`** — `extrudeProfiledContours`. Un anneau par point
  du profil (`offsetContours`), une **couronne** tessellée entre deux anneaux consécutifs,
  chaque sommet recevant la cote de l'anneau dont il provient. Le fond, la paroi d'aplomb et
  les deux capots sont cousus autour ;
- **`shape.extrude.profiled`** — un nœud à part de `shape.extrude`, et non un paramètre de
  plus : ce qui le distingue est une **entrée**, un profil. Un port laisse la famille des
  formes ouverte là où une énumération l'aurait figée ;
- **`flow.select.profile`** — la troisième variante du sélecteur, pour que le gabarit
  choisisse la forme d'arête ;
- **`differenceContours`** dans `contour_ops` — écrit pour les couronnes, finalement **pas
  utilisé par elles** (voir plus bas), mais gardé : c'est la brique de A2 (texte gravé).

**12 + 3 tests** (`tu_cgmesh_extrude_profiled.cpp`, `tu_cggraph_nodes_stack.cpp`).
**1 616 tests** au total, aucune régression.

#### Correction 1 — **le congé ne demandait aucun producteur de plus**

G2 et D7 annonçaient un `roundoverSplayProfile` « requis », au motif que `cavettoSplayProfile`
est concave. **C'est faux sous la lecture de ce consommateur**, et le test le tranche par la
mesure plutôt que par le raisonnement : à largeur et profondeur égales, le chanfrein est la
**corde** ; ce qui bombe au-delà retire **moins** de matière. Or

```
volume(cavetto) > volume(chanfrein)
```

— donc le cavet **roule l'arête** au lieu de la creuser : c'est le congé convexe demandé.
La cause de mon erreur est instructive : la même courbe rend un congé ou une gorge **selon le
sens où le consommateur la lit**, et j'avais raisonné sur la lecture du gothique (l'ébrasement
d'une ouverture) au lieu de celle-ci (l'arête d'un solide en relief). D'où le cas
`the_cavetto_profile_reads_as_a_CONVEX_roundover`, qui **nomme la forme par une mesure**.

Conséquence : **D7 tombe** pour sa moitié « producteur à écrire ». Le nommage reste :
`profile.chamfer` porte chanfrein **et** biseau (largeur = profondeur, ou non),
`profile.cavetto` porte le congé.

#### Correction 2 — **la couronne ne passe pas par un booléen**

Première version : `differenceContours(anneau_k, anneau_k−1)`. Résultat mesuré sur les quatre
polices difficiles du catalogue : **4 à 12 arêtes non partagées** — une peau trouée. Cause :
la seconde passe Clipper2 recalcule la frontière extérieure et **recolle au passage des
micro-arêtes** que la paroi, elle, avait gardées.

La couronne est donc obtenue par la **règle de remplissage**, comme le reste du dépôt :
l'anneau intérieur est fourni **à l'envers**, et le NonZero du tessellateur le retire. Les
sommets rendus sont alors, mot pour mot, ceux des deux anneaux. Trois polices sur quatre
réparées d'un coup.

#### Ce que la quatrième a appris : **trou ≠ pincement**

Cinzel gardait 4 arêtes anormales — mais de multiplicité **4**, pas 1. Ce n'est pas un trou,
c'est un **pincement** : le décalage vers l'intérieur referme un empattement sur lui-même, la
surface se touche le long d'une arête. La peau reste **fermée** ; elle cesse seulement d'être
une variété — ce qu'un slicer traite comme il traite deux coques qui s'interpénètrent (§1 bis).

Le test distingue donc les deux : **zéro trou** est une exigence dure, les pincements sont
comptés et rapportés. Sans cette distinction, on aurait soit un test qui échoue sur une
géométrie correcte, soit une tolérance muette qui aurait laissé passer un vrai trou.

#### Deux détails de conception qui ont payé

- **Largeur nulle = paroi droite.** La primitive accepte `v_max == 0` comme la limite du
  profil. C'est ainsi qu'une page à graphe fixe offre « arête vive » — en mettant un curseur à
  zéro, puisqu'elle ne sait pas débrancher un nœud. **Vérifié dans le navigateur : 3 928
  triangles, exactement le compte de l'extrusion droite.**
- **Sens du décalage exposé** (D1) : rentrant par défaut, dilatant en option — un paramètre du
  nœud, un seul signe dans le code.

#### La page, mesurée — « Texte 3D », lettres de 30 mm, profondeur 3 mm

| Forme d'arête | Cotes | Triangles |
|---|---|---|
| Arête vive (largeur 0) | 146,74 × 30,15 × 3 mm | 3 928 |
| Chanfrein 0,4 mm | 146,74 × 30,15 × 3 mm | 6 866 |
| Congé 0,4 mm (6 segments) | 146,74 × 30,15 × 3 mm | 22 396 |

**Les cotes ne bougent pas d'un centième** entre les trois : c'est exactement ce que le
décalage intérieur de D1 promettait, et ce que la référence ne tient pas (26,86 mm pour 24
demandés). Le prix est visible dans la dernière colonne — un congé à six segments coûte six
couronnes.

Le gabarit passe de **10 à 13 nœuds** et expose 25 réglages.

⚠ **Garde-fou non adouci** : un profil plus profond que les lettres est **refusé**, pas raboté.
Les bornes des curseurs (3 mm) et les libellés le disent, mais une combinaison extrême reste
possible et rend alors une page en erreur plutôt qu'un modèle faux. C'est le bon sens de
l'échange.

### Étape 5 — Booléen 2D (`shape.boolean2d`) — ✅ **FAITE le 2026-09-03**, hors parité

Livrée en dernier, et c'est sa place : la mesure du §1 bis avait **retiré la parité de cette
étape** — la référence n'emploie aucun booléen, ni pour son socle ni pour son porte-clefs.
C'est donc un **dépassement**, et il ne débloque rien du lot déjà livré.

**Livré :**

- **`unionContours` et `intersectionContours`** dans `contour_ops`, aux côtés de
  `differenceContours` — la famille est complète. Pour normaliser une région *seule*,
  `offsetContours (in, 0)` faisait déjà le travail, et l'en-tête le dit plutôt que d'ajouter
  une quatrième fonction qui ferait la même chose ;
- **`shape.boolean2d`** — un nœud, un paramètre `op`, et non trois nœuds : ici seule
  l'*opération* change, les opérandes sont du même type. C'est l'inverse du cas
  `flow.select.*`, où c'est le TYPE qui change et où le catalogue de types impose donc une
  variante par type ;
- **ports NOMMÉS** « matière (A) » et « outil (B) », parce que l'ordre porte un sens pour la
  différence et pour elle seule. Deux ports anonymes l'auraient caché.

**7 tests** (`tu_cgmesh_contour_ops.cpp`, `tu_cggraph_nodes_stack.cpp`), dont l'invariant qui
lie les trois opérations — `|A ∪ B| = |A| + |B| − |A ∩ B|` : si l'une change de sens sans
qu'on le dise, il tombe. **1 623 tests** au total.

#### Deux décisions de contrat, et leurs raisons

**Un résultat vide est REFUSÉ par le nœud**, pas propagé. Deux formes disjointes n'ont pas
d'intersection, un emporte-pièce plus grand que sa matière ne laisse rien : c'est un
*résultat*, pas une panne — mais ce n'est pas une région extrudable. Le laisser passer ferait
échouer l'extrudeur trois nœuds plus loin, là où la cause n'est plus lisible. Le refus est
donc **ici**, et depuis l'étape 6 la page nomme le nœud qui refuse.

**Intersecter avec rien rend RIEN**, et non « tout ». La différence, elle, admet un outil
vide (retirer rien rend la matière). Le raccourci symétrique aurait été faux, et c'est le
genre d'asymétrie qu'un test garde mieux qu'un commentaire.

#### Ce qu'il ouvre, et qui reste à faire

| Amélioration | Ce qu'il en manque encore |
|---|---|
| **A4** — socle à coque unique étanche | les drapeaux `emitBottomCap` / `emitTopCap` sur `ExtrudedMeshBuilder` (D6, hors lot) |
| **A2** — texte gravé | rien de plus côté booléen : la gravure passe par les rôles `isHole` et la règle de remplissage (§3.4) ; le booléen sert le capot |
| **A5** — pochoir, emporte-pièce | un générateur de contours primitifs (le cercle), partagé avec A1 et A6 |
| **A6** — perçages, fraisures, trou de serrure | le même générateur de contours primitifs — mais le **bandeau à oreilles percées est livré** (`mesh.mounts`, 2026-09-04), et il ne passe par aucun d'eux |

**La brique suivante est donc la même pour A1, A5 et A6** : `shape.contours.primitive`
(cercle, rectangle, rectangle arrondi, oblong), une quarantaine de lignes sans dépendance.
Trois améliorations se débloquent d'un coup. **Son premier morceau est écrit** — `circleContour`
dans `contour_ops`, posé là par la fixation murale et non dans le nœud qui s'en sert, parce
qu'un cercle n'appartient à aucun d'eux. Restent le rectangle, le rectangle arrondi et
l'oblong, et l'emballage en nœud.

### Étape 6 — Publier le module — ✅ **FAITE le 2026-09-03**

**Enrichissement du gabarit `text3d` existant** (D8), sans septième carte. Mais « publier »
demandait plus que d'exposer des paramètres : trois manques rendaient le module inutilisable
par quelqu'un d'autre que celui qui l'a écrit.

#### 1. Les compteurs n'atteignaient aucun écran

Depuis l'étape 3, des nœuds comptent ce que le réglage a coûté — morceaux d'une silhouette
qui ne s'est pas refermée, formes qu'une arête a mangées, glyphes placés. **Chacun derrière
un accesseur qui lui était propre, aucun visible nulle part.** Un garde-fou invisible n'en
est pas un.

- **`cggraph::Node::PublishStats`** (+ `struct NodeStat`) — le chemin générique qui manquait
  entre un compteur de nœud et une interface. Trois nœuds le servent : `text.contours`,
  `shape.contours.offset`, `shape.extrude.profiled` ;
- **`graphNodeInfo` publie une section `stats`** — à part des `params`, et la distinction est
  de fond : rien ne *règle* une mesure, elle se *constate* ;
- **le gabarit déclare ce qu'il surveille** (`watch`), avec une garde `onlyIf` : la
  silhouette ne se signale que si le socle est demandé **et** que la forme choisie est bien
  elle. Sans cette garde, la page crierait juste et hors sujet — donc on apprendrait à
  l'ignorer.

**Mesuré dans la page**, texte « Texte 3D », lettres de 30 mm, Blooming Grove :

| Réglage | Ce que la page dit |
|---|---|
| Silhouette, décalage 1 mm | « socle silhouette : **7 morceaux séparés** — augmente le décalage » |
| Silhouette, décalage 9 mm | *(rien : les halos se sont rejoints)* |
| Chanfrein 0,6 mm | *(rien)* |
| Chanfrein 0,8 mm | « **2 forme(s) mangée(s)** par l'arête » |
| Chanfrein 1,0 mm | « **4 forme(s) mangée(s)** par l'arête » |

#### 2. Un refus s'affichait « compute-failed », c'est-à-dire rien

À 1,5 mm de chanfrein sur cette cursive, **toutes** les formes disparaissent : le nœud refuse
— correctement — et la page n'en disait rien d'exploitable. Or `graphEvaluate` **rendait déjà
l'identifiant du nœud fautif** ; la page n'en faisait simplement rien.

- le statut nomme désormais le nœud : « **calcul refusé · shape.extrude.profiled** » ;
- le gabarit peut porter un **conseil par nœud** (`failureHints`), affiché à la place du
  cul-de-sac : « l'arête est trop large ou trop profonde pour ces lettres […] réduis la
  largeur, augmente la profondeur des lettres, ou passe le sens en *dilatant* ».

Ce gain vaut pour **toute** page gabarit, pas seulement celle-ci.

#### 3. Vingt-cinq réglages à la file ne se lisent pas

Champ `group` sur chaque paramètre exposé, intertitre rendu quand il change : **Texte ·
Arête des lettres · Socle · Mise en page · Support fondu (hérité)**. L'ordre du gabarit
portait déjà le sens, il ne le montrait pas. Un gabarit qui ne groupe rien rend la liste
d'avant.

Le dernier groupe **nomme la dette** plutôt que de la cacher : le `support` de
`text.contours` est un contour fondu aux lettres, donc forcément à leur profondeur — la
limite même que le socle lève. Il reste (un bandeau, un cadre servent encore), avec une
infobulle qui dit lequel choisir.

#### 4. Second passage (2026-09-04) : les blocs ne se VOYAIENT pas

Le regroupement existait dans les données, pas à l'écran — un intertitre gris dans la même
colonne que les réglages, indiscernable des libellés de section de la page. Retour
utilisateur, et il était juste.

- **Une rampe bleue à quatre marches**, déclarée en variables CSS : bandeau du titre le plus
  sombre (`--block-head`), corps du bloc plus CLAIR que le panneau (`--block-body`, c'est lui
  qui sépare un bloc de son voisin), champs renfoncés dans le corps (`--field`), titre en
  bleu clair (`--block-title`). L'accent (`#4c9aff`) reste réservé aux **valeurs** et aux
  positions actives ; le titre en est un parent désaturé (`#8ab4f8`), même famille, rôle
  différent ;
- **contrôles segmentés** à la place des menus pour deux ou trois positions — alignement,
  forme d'arête, forme de socle, sens. Un menu cache ce qu'on *pourrait* choisir ;
- **bloc « Police »** : les sources de fichier deviennent un bloc comme les autres, et son
  titre vient du **gabarit** (`sourcesGroup`) — ce qu'on y dépose dépend du document, et
  « Sources » ne dit rien à personne ;
- valeurs affichées à la précision du **pas** du curseur (« 30 » et non « 30.000 »), zone de
  texte pleine largeur, panneau élargi à 360 px, bouton STL en action principale.

**Deux défauts trouvés en vérifiant, tous deux invisibles à la lecture du code :**

1. le bloc des sources se **réduisait à un trait de deux pixels**. C'est un enfant DIRECT du
   panneau, lequel est un flex colonne : il se laissait comprimer dès que le contenu
   débordait. Les blocs de paramètres, eux, vivent dans `#params` et ne le voyaient pas.
   `flex: 0 0 auto` sur `.block` ;
2. la position **active** d'un contrôle segmenté ressortait **éteinte** pendant que les
   inactives paraissaient allumées : la règle qui renfonce les champs dans un bloc
   l'emportait par spécificité sur `[aria-pressed="true"]`. Corrigé en la restreignant aux
   enfants directs du corps.

#### 4 bis. Troisieme passage : ce que la couleur seule ne faisait pas

Trois corrections apres un second retour, et la premiere est la lecon :

- **les blocs se DILUAIENT dans le panneau.** Le premier jeu de tons les posait
  juste au-dessus de lui (#2a3444 contre #26282c) : une nuance, pas une
  separation. Il a fallu ECARTER franchement les deux -- le corps des blocs
  monte a #3a4762 --, ajouter une ombre portee courte qui les POSE sur le
  panneau, et relever d une marche les textes secondaires, que le fond eclairci
  avalait (`--block-value`, `--block-muted`). Nuancer ne suffisait pas ;
  **Deuxieme reprise** : encore trop proche. Le corps monte a #4b5b7d, soit pres
  de trente points de clarte au-dessus du panneau -- l ecart doit etre FRANC, pas
  nuance, et les champs restent sombres pour se designer comme creux ;
- **l intertitre << Parametres >> disparait** : chaque bloc porte son nom, et un
  titre qui chapeaute des titres n ajoute qu une ligne a lire ;
- **tous les blocs s ouvrent REPLIES**, y compris ceux ecrits a la main. Le panneau
  presente alors une table des matieres -- neuf en-tetes qu on lit d un coup d oeil
  -- au lieu d une colonne de vingt-cinq reglages qu il faut parcourir pour savoir
  ce qu elle contient. Le drapeau du gabarit a change de sens : un groupe demande
  desormais `open: true` pour faire exception, le defaut du moteur etant replie ;
- **l EXPORT devient un bloc**, et le GRAPHE aussi -- laisser l un encadre et l
  autre nu se serait lu comme un oubli. Le graphe est replie : on enregistre le
  document une fois, on exporte a chaque essai.

#### 5. La couleur devient un NŒUD (`mesh.color`)

Elle était un réglage de page, une pastille dans « Vue », à côté du fil de fer et du
recadrage. Elle n'en était pas un : le fil de fer et la caméra *regardent* la pièce, la
couleur lui **appartient** — on la choisit une fois, on veut la retrouver en rouvrant le
document, et pouvoir la piloter depuis le graphe.

- nouveau nœud **`mesh.color`**, paramètre `color` au format `#rrggbb` — une **chaîne**, pour
  que le document enregistré reste lisible : « #b4bec8 » se reconnaît, « 11845832 » se
  décode ;
- **il ne repeint pas ce qui est déjà peint.** Seules les faces sans matériau reçoivent la
  couleur ; un relief coloré branché là garde sa palette au lieu de s'aplatir. Repeindre
  demanderait de désigner QUELLES faces, c'est-à-dire une sélection, qui n'existe pas. Les
  deux comptes (`paintedFaces`, `keptFaces`) sont publiés, donc constatables ;
- une chaîne qui n'est pas une couleur est **refusée**, pas remplacée par un gris ;
- nouveau type de widget `color` dans le gabarit, et la pastille disparaît de « Vue » —
  `template.js` tolère désormais son absence, `template.html` la garde.

⚠ **Ce que la couleur ne traverse pas, et il faut le dire** : ni l'un ni l'autre des exports.
Le STL binaire n'a pas de champ de couleur, et `graphExportObj` rend un OBJ **minimal sans
`mtllib`**. Elle sert donc l'aperçu et le document — un export coloré demanderait un chemin
qui porte les matières, ce que le §7 déclare déjà comme hors périmètre.

**Un piège de portabilité, attrapé par le build WASM** : l'`#include` du nouveau nœud avait
atterri dans le bloc `#ifndef __EMSCRIPTEN__` du catalogue, celui des nœuds d'ANALYSE. Le
build natif passait, le build web échouait sur « unknown type name ». C'est exactement la
dérive que `nodes/CMakeLists.txt` annonce comme « bruyante » — elle l'a été.

**1 626 tests** passent, dont trois nouveaux sur la couleur : les faces vierges sont peintes,
celles qui portent un matériau sont **gardées** (vérifié en chaînant le nœud sur lui-même),
et cinq chaînes fautives sont refusées quand `#B4BEC8` passe.

**La carte de `index.html`** dit enfin ce que le module fait : cotes en millimètres, hauteur
de lettre honorée, arête chanfreinée ou congée, socle rectangulaire ou en silhouette, export
STL binaire.

**1 616 tests** passent.
---

## 5. Trade-offs à assumer

- **2,5D, pas de CSG.** Tout le plan tient parce que la pièce est un empilement de régions
  planes. Un texte sur surface courbe, une lettre en révolution, un chanfrein sur l'arête
  *inférieure* du socle sortiraient du cadre et demanderaient une capacité nouvelle. Ce
  n'est pas ce que fait stltext.
- **Nœuds généraux plutôt qu'un nœud « produit ».** Quatre étapes au lieu de deux, et un
  gabarit à écrire à la fin. Le retour : `mesh.merge`, `extrude.profiled`, `zBottom` (puis
  `boolean2d`) servent l'extrusion SVG, le relief d'image et la baie gothique, et le module
  reste un document que l'utilisateur peut ouvrir dans l'éditeur nodal.
- **G1-a et rien de plus.** Un STL multi-coques, parfaitement imprimable, et c'est ce que
  livre la référence (§1 bis). La coque unique manifold est notée en A4, pas dans le lot.
- **Porte-clés écarté (A1), arêtes élargies (G2).** Le lot renonce à un préréglage que la
  référence offre, et ajoute deux formes d'arête qu'elle n'offre pas. Assumé : le porte-clés
  ne coûtera rien à rattraper (mêmes nœuds que l'étape 3), tandis que le congé exige la
  primitive générale en *n* bandes — donc l'échange déplace l'effort vers la seule pièce qui
  ait une valeur de réemploi.
- **Unités millimétriques = rupture de valeurs, assumée (D5).** Les documents `text3d.json`
  déjà enregistrés (racine du dépôt : `text3d.json`, `text3d (1).json`) portent des valeurs
  en unités abstraites. Changer les bornes ne les casse pas mais les rend absurdes (corps
  1,0 mm) : ce sont des fichiers de travail, ils seront refaits plutôt que versionnés.

---

## 6. Ce que ça coûte, en ordre de grandeur

| Étape | Nature | Ampleur |
|---|---|---|
| 1 — STL + cotes + mm | JS + JSON | la plus petite du lot, aucun C++ |
| 2 — hauteur de lettre | cgmath + nœud | petite |
| 3 — socle, plaque rectangulaire **et** silhouette (G1-a) | 3 nœuds + 1 paramètre + 1 emballage Clipper2 (~20 lignes) | petite/moyenne — l'emballage d'offset est réutilisé par l'étape 4 |
| 4 — arêtes chanfrein / biseau / congé | 1 primitive cgmesh + 2 nœuds (l'extrudeur profilé, le sélecteur de profil) ; **aucun producteur de profil** | **la seule non triviale** — bandes d'offset, classification des sommets par anneau, cas dégénérés ; le congé est ce qui interdit de s'en tirer avec une bande unique |
| *hors lot* — `boolean2d` (étape 5) | 1 nœud sur Clipper2 | moyenne |

---

## 7. Non examiné — déclaration explicite

- **Le corps du générateur de la référence reste minifié**, donc ses formules exactes ne
  sont pas lues. En revanche sa **pile est établie** (§1 bis, « Pile technique ») : le
  tableau `__vite__mapDeps` nomme three.js r185, `FontLoader`, `TextGeometry`,
  `STLExporter`, `OrbitControls` et opentype.js. Ce qui reste inconnu est donc le
  **paramétrage**, pas la mécanique.
- **La règle qui fixe l'épaisseur du socle de la référence** (2,20 mm relevés) reste
  inconnue — un seul jeu de paramètres exporté. **Sans conséquence** : D3 en fait un
  paramètre réglable de notre côté, donc il n'y a plus rien à deviner (§8.3).
- **Les fichiers ne sont pas sur disque** : Chrome est réglé sur `prompt_for_download` (boîte
  « Enregistrer sous » native, hors d'atteinte depuis la page) et la requête page →
  `127.0.0.1` est refusée par la politique d'accès au réseau local. L'analyse a porté sur les
  octets du `Blob` dans la page, ce qui est équivalent pour toutes les mesures rapportées,
  mais **aucun STL de référence n'est conservé dans le dépôt** pour rejouer la comparaison.
- **Aucune mesure de performance** : ni le coût de l'union Clipper2 à l'échelle
  millimétrique, ni celui de *n* anneaux d'offset, ni le budget mémoire WASM
  (`graphSetMemoryBudget` existe et n'a pas été sollicité ici).
- **Aucune vérification de slicer** : l'acceptation d'un STL multi-coques (G1-a) par
  Cura/PrusaSlicer/Bambu est affirmée par pratique courante, pas testée dans ce dépôt.
- **Le rendu texturé / matériaux multiples** : `graphExportObj` rend un OBJ minimal sans
  `mtllib` (`template.js:19-24`) et le STL ne porte pas la couleur. Sans conséquence pour
  un solide à matériau unique ; hors périmètre ici.
- **Le chemin de l'éditeur nodal** (`graph.html`, worker + OffscreenCanvas) : les nœuds
  ajoutés y apparaîtront par le catalogue, mais l'ergonomie de leur usage à la main n'a pas
  été examinée.

---

---

## 8. Registre des décisions — **closes**

**Aucune décision n'est pendante au 2026-09-04.** D9, ouverte puis close le même jour par
l'implémentation de l'étape 3, est la preuve que ce journal sert : le plan avait supposé
qu'un gabarit pouvait exprimer une branche optionnelle, l'implémentation a montré que non,
et `flow.select` a été livré en conséquence. C'est le journal, à contredire explicitement
quand l'implémentation révèle une erreur.

### 8.1 Périmètre

| # | Décision | Effet |
|---|---|---|
| — | **Porte-clés retiré** du premier lot | consigné en A1 ; aucune dette, il consomme les nœuds de l'étape 3 |
| — | **Trois formes d'arête** : chanfrein, biseau, congé | la primitive est générale en *n* bandes dès le départ ; le port `splayProfile` est la forme du nœud |
| — | **« Congé » = arrondi CONVEXE** (registre CAO) | rendu par `cavettoSplayProfile`, qui **est** cet arrondi sous la lecture retenue — vérifié par le volume, cf. étape 4 |
| — | **Plaque silhouette** (contour du texte décalé) en plus du rectangle | emballage `offsetContours` à l'étape 3, réutilisé par l'étape 4 |

### 8.2 Conception

**D1 — Sens du décalage des arêtes profilées : DÉCALAGE INTÉRIEUR par défaut**, dilatation
exposée en option. C'était la seule décision bloquante — elle fixe le contrat de la primitive
de l'étape 4 et, du même coup, le sens d'application du congé.
*Raison* : l'étape 1 affiche des cotes et l'étape 2 promet une hauteur de lettre en
millimètres ; une dilatation ferait mentir les deux, comme chez la référence qui affiche
26,86 mm pour 24 demandés. **24 mm demandés = 24 mm mesurés.**
*Prix assumé, et il est actif* : sur une cursive, un décalage intérieur plus large que la
demi-épaisseur du trait fait disparaître le délié. C'est **détectable** (l'aire de l'anneau
tombe à zéro) — donc à signaler, à borner, ou à contourner par le mode dilatation. La
détection de l'anneau dégénéré entre de ce fait dans l'étape 4.

**D2 — La plaque silhouette est un FOND, et rien d'autre, dans ce lot.** Les contre-formes
refermées par l'offset sont donc le comportement **voulu** — le trou du `o` doit être bouché,
sinon on voit à travers la plaque. La silhouette lue seule est un autre produit, proche du
pochoir (A5).

**D3 — L'épaisseur de plaque est un paramètre en millimètres**, pas un préréglage. C'est ce
que G1 apporte, et cela **dissout la seule question ouverte du §7** : la règle qui fixe le
socle de la référence à 2,20 mm n'a plus à être devinée.

**D4 — La page expose la HAUTEUR DE LETTRE** (hauteur de capitale, dérivée de la bbox du
`'H'`), le corps em restant l'unité interne ; **`letterSpacing` en millimètres**, non en
fraction de cadratin. Ce sont les unités d'un atelier.

**D5 — La rupture d'unités est assumée.** Les `text3d.json` de la racine du dépôt portent des
valeurs abstraites que le passage aux millimètres rendra absurdes (corps 1,0 mm). Ce sont des
fichiers de travail, pas des documents d'utilisateur : les versionner coûterait plus que de
les refaire.

**D6 — `emitBottomCap` / `emitTopCap` restent HORS du lot**, avec A4. La référence livre la
membrane interne, le relief du dépôt aussi : ce n'est pas ce qui distingue ce lot. Les dix
lignes se justifieront quand la coque unique manifold sera visée — et elles profiteront
alors aussi au relief et aux blocs pixelisés.

**D7 — Nommage au catalogue.** ⚠ **Révisée par l'étape 4** : elle prévoyait un
`profile.roundover` à écrire, au motif que le cavet serait concave. La mesure a montré
l'inverse (§ étape 4, correction 1) — **`profile.cavetto` EST le congé convexe** sous la
lecture de ce consommateur, et aucun producteur n'a été ajouté. La page offre donc deux
entrées, « chanfrein / biseau » et « congé », pour deux producteurs existants.

**D8 — Le module enrichit la page « Texte 3D » existante**, sans septième carte : deux pages
du même métier finiraient par diverger, ce que l'architecture de gabarits a coûté d'efforts
à éviter.

**D9 — Un gabarit à graphe FIXE offre ses variantes par un SÉLECTEUR : `flow.select`.**
Ouverte et close le 2026-09-03 par l'étape 3. Deux variantes typées
(`flow.select.mesh`, `flow.select.contours`) plutôt qu'un port génériquement typé, parce que
les liens sont validés à la relecture et qu'un port dont le type dépendrait d'un paramètre
serait encore à son défaut à ce moment-là. Réserve assumée et testée : **toutes les branches
sont évaluées**, donc un gabarit borne ses branches au lieu de compter sur le sélecteur pour
masquer celle qui échouerait.

**D10 — La fixation murale est un BANDEAU À OREILLES PERCÉES, ajouté au MAILLAGE, à ligne
réglable et à mi-hauteur par défaut.** Ouverte et close le 2026-09-04, contre la conception
que j'avais d'abord écrite (des pastilles posées sur les contours, avant l'extrusion), et
c'est la seconde fois que ce journal sert à consigner une erreur plutôt qu'un choix.
*Deux objections l'ont retournée* : une fixation posée sur les contours ne sert que les
producteurs de contours, alors que le but était la généralité ; et deux points pris « aux
extrémités » d'un modèle plus bas à gauche qu'à droite ne sont **pas de niveau**, alors que
les vis d'un mur le sont.
*Conséquence* : la ligne est **imposée** — une hauteur, deux centres dessus — et ce qui relie
ces centres au corps est une bande. La fixation EST donc un bandeau percé, et non un support
plus des pastilles. Comme elle n'AJOUTE que de la matière, aucun booléen 3D n'entre en jeu et
elle s'applique à **tout maillage**, ce que la première conception ne pouvait pas promettre.
*Hauteur* : réglable, en **fraction** de la pièce, 0,5 par défaut — une fraction et non des
millimètres, pour qu'un changement de taille du texte ne déplace pas la fixation.
*Largeur* : les deux bouts sont pris sur la matière présente **à la hauteur de la fixation** —
une tranche, pas l'emprise totale. Corrigé le même jour : une fixation posée à mi-hauteur d'un
L d'après l'emprise totale débordait de plusieurs millimètres dans le vide.
*Limite assumée et publiée* : connaître les deux bouts n'est pas connaître ce qu'il y a entre
eux ; sur un modèle en anneau, le bandeau traverse le vide central. Et il ne perce **que ce
qu'il apporte**.

### 8.3 Ce qui n'est plus une question

- **La règle d'épaisseur du socle de la référence** — dissoute par D3.
- **Le besoin d'un booléen** pour le socle, le porte-clés ou la gravure — écarté par la mesure
  (§1 bis) et par la vérification de la règle de remplissage (§3.4, A2).
- **Le format d'export** — STL binaire, encodeur déjà écrit (`exporters.js:43`).
- **Le sens d'application du congé** — le même que D1, donc réglé avec lui.
- **Comment garantir que deux vis murales sont de niveau** — réglé par D10 : en ne le
  *déduisant* pas. La ligne est imposée par le bandeau, la forme de la pièce n'y entre pas.

---

## 9. Réponse en une ligne

Oui, et **par l'approche nodale, qui est déjà celle du module existant** : sur les onze
contrôles relevés dans l'outil de référence, sept sont acquis — trois avec un cran d'avance,
puisque le dépôt lit de vraies TTF/OTF, gère le crénage et le multi-ligne là où la référence
offre dix polices `typeface` JSON sur une seule ligne de 30 caractères.

La dissection de ses STL (§1 bis) a **simplifié le reste**, en établissant que la référence
n'emploie **aucun booléen** : elle empile des coques fermées qui s'interpénètrent, et laisse
le slicer les unifier. Restent donc, pour ce premier lot : **un socle en deux formes** —
rectangulaire comme la référence, et **silhouette** (le contour du texte décalé, un
`InflatePaths` positif) qu'elle n'a pas — soit `Append` sur deux plages de Z, `mesh.merge` et
un emballage d'offset de vingt lignes ; **les arêtes profilées — chanfrein, biseau, congé** (la
seule primitive à écrire : des bandes d'offset Clipper2, générale en *n* dès le départ, en
refusant le garde-fou du chanfrein gothique qui neutraliserait la plupart des lettres, et en
tranchant le sens du décalage), et **deux finitions d'interface sans une ligne de C++** :
l'export STL binaire et les millimètres.

Le **porte-clés est écarté du lot** et consigné en A1 : la mesure a montré qu'il ne demande
que les nœuds de l'étape 3, donc le report ne crée aucune dette.

Le dépôt n'a pas à rattraper un retard. Il a à **livrer trois nœuds et une primitive**, à
choisir sur deux points où il fait déjà mieux (glyphes fusionnés, pièce posée sur z = 0)
entre la parité et sa propre convention — et il sortira du lot avec **deux réglages que la
référence n'a pas** : les trois formes d'arête, et la plaque silhouette.
