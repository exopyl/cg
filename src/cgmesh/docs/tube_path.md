# Tube le long d'un path 3D & jointures — SOTA + faisabilité (`src/cgmesh`)

> Document produit par `/devagents:sota-and-feasibility-analysis`.
> Question d'origine : *création d'un tube le long d'un path 3D ; la proposition d'une sphère (aux nœuds) est coûteuse — quelles sont les alternatives ?*
> Périmètre : `src/cgmesh` (`CreateTube`/`CreateTubes` dans `surface_basic.{h,cpp}`, appelé par `ParameterizedLSystem`).
> Date : 2026-07-16.

---

# Partie 1 — État de l'art : tube le long d'un path 3D & jointures

## Introduction — cadrage du problème

Le problème est celui du **balayage d'une section (généralement un n-gone régulier) le long d'un squelette 1D plongé dans R³**, produisant un maillage triangulé fermé (« generalized cylinder » / sweep surface). Trois sous-problèmes se distinguent nettement, et la littérature les traite dans des communautés différentes :

1. **Le repère de balayage** (comment orienter la section le long de la courbe sans vrille parasite) — communauté géométrie différentielle / animation.
2. **Les jointures de degré 2** (coins d'une polyligne) — transposition des *line joins* de la 2D vectorielle.
3. **Les jointures de degré ≥ 3** (nœuds de branchement des L-systèmes / arbres) — communauté modélisation d'arbres, surfaces implicites, skeleton-to-mesh.

À cela s'ajoute une contrainte transverse pour l'impression 3D : l'**étanchéité** (manifold / watertight), qui départage fortement les approches.

Le contexte de `src/cgmesh` (signatures `CreateTube(polyline, radius, nSides, caps)` et `CreateTubes(polylines, ...)`, `surface_basic.cpp`/`.h`) impose deux invariants forts qui pilotent la sélection : **régénération interactive jusqu'à ~60 000 segments** et **entrée sous forme de polylignes séparées** (les branches ne sont pas données comme un graphe : les nœuds de degré ≥ 3 sont **implicites**, matérialisés par des extrémités de coordonnées partagées).

**Note de méthode.** Distinction systématique entre ce que les sources *affirment* (marqué par citation) et les *analyses de coût* (marquées *Extrapolation* / *Opinion*). Les estimations O(·) sont des analyses, pas des mesures publiées.

---

## Famille 1 — Repères de balayage le long d'une courbe

Le défaut du code actuel (« repère de section arbitraire par segment via u = a×d, v = d×u ») est que chaque segment choisit une base indépendante : la section « tourne » d'un segment à l'autre, et comme surtout **chaque segment est tubé indépendamment** (2 anneaux/segment, non partagés), on obtient les décrochements observés. La solution amont est un **repère cohérent le long de toute la polyligne**, avec **un anneau partagé par sommet** (n+1 anneaux pour n segments, au lieu de 2n).

### 1.1 Repère de Frenet (fondateur, mais instable)

Le repère de Frenet (tangente, normale principale, binormale) est le véhicule historique d'analyse d'une courbe. Sa faiblesse est bien documentée : la normale principale est **définie à partir de la dérivée seconde**, donc **indéfinie ou discontinue là où la courbure s'annule** (segments rectilignes, points d'inflexion), provoquant des sauts brusques d'orientation de la section.

- **Fait (sourcé).** Bishop, R. L. (1975), « There Is More than One Way to Frame a Curve », *The American Mathematical Monthly*, 82(3), 246–251. DOI 10.1080/00029890.1975.11993807.
- **Fait (sourcé).** Hanson, A. J. & Ma, H. (1995), « Parallel Transport Approach to Curve Framing », Indiana University Tech. Rep. TR 425 : le repère de Frenet « souffre d'ambiguïté et de changements soudains d'orientation quand la courbe se redresse momentanément ».

Pour une polyligne (dérivée seconde nulle *à l'intérieur* de chaque segment, non définie aux sommets), Frenet est **inutilisable tel quel**. **Verdict : à écarter.**

### 1.2 Parallel Transport Frame / Bishop frame (le bon défaut discret)

Le repère de transport parallèle (« relatively parallel adapted frame » de Bishop) transporte la base d'un point au suivant **en minimisant la rotation autour de la tangente** ; il reste **bien défini même quand la courbure s'annule**.

- **Fait (sourcé).** Bishop 1975 : fondation mathématique du repère parallèle.
- **Fait (sourcé).** Hanson & Ma 1995 : algorithme concret de génération d'un repère mobile par transport parallèle pour « rubans, tubes et orientations de caméra », fondation « mathématiquement saine ».

En pratique discrète : on transporte la base d'un segment au suivant par la **rotation minimale amenant la tangente précédente sur la tangente courante** (rotation autour de t_i × t_{i+1}). Coût **O(n)**.

### 1.3 Rotation Minimizing Frames (RMF) et méthode de double réflexion

- **Fait (sourcé).** Wang, W., Jüttler, B., Zheng, D. & Liu, Y. (2008), « Computation of Rotation Minimizing Frames », *ACM TOG*, 27(1), art. 2. DOI 10.1145/1330511.1330513 — La **double réflexion** calcule chaque repère à partir du précédent via **deux réflexions**, approchant un RMF exact ; **plus précise que les méthodes de projection/rotation**. Coût **O(n)**.
- *Extrapolation.* Pour une **polyligne**, transport parallèle discret (1.2) et double réflexion coïncident essentiellement ; la double réflexion reste le choix le plus robuste et numériquement propre.

### 1.4 Cas des polylignes fermées (Koch, îles)

Un repère transporté le long d'une **boucle fermée ne se referme pas** (holonomie). Il faut **répartir l'écart de fermeture** uniformément sur les anneaux.

- **Fait (sourcé).** Wang et al. 2008 traitent explicitement la fermeture d'un RMF sur courbe fermée par distribution de l'écart de rotation.

### Synthèse Famille 1

| Repère | Défini si courbure=0 | Coût | Adéquation polyligne |
|---|---|---|---|
| Frenet | Non | O(n) | À écarter (sauts aux inflexions/segments droits) |
| Parallel Transport / Bishop | Oui | O(n) | Excellente |
| RMF double réflexion | Oui | O(n) | Excellente, la plus robuste |

**Point-clé coût.** *Extrapolation.* Passer d'un tubage per-segment (2n anneaux, gaps) à un **anneau partagé par sommet** (n+1 anneaux) **réduit** la géométrie d'environ ×2 **tout en supprimant** les gaps de degré 2. Amélioration à coût négatif.

---

## Famille 2 — Jointures aux coins (degré 2)

Aux sommets où la direction change, deux anneaux consécutifs ne sont pas coplanaires : c'est le problème des *line joins* de la 2D vectorielle, transposé en 3D.

- **Fait (documentation).** W3C, SVG — `stroke-linejoin` : **`miter`**, **`round`**, **`bevel`**, et **`stroke-miterlimit`** qui borne l'onglet pour éviter les pointes aux angles aigus.

Transposition 3D :

- **Miter (onglet).** Un seul anneau dans le **plan bissecteur** des deux segments, ouvert de 1/cos(θ/2). Maillage minimal, manifold naturel. *Défaut :* pincement/auto-intersection aux angles aigus (1/cos(θ/2) → ∞). *Parade :* miter-limit → basculer en bevel/round.
- **Bevel.** Deux anneaux distincts reliés par une **couronne de triangles**. Robuste aux angles vifs, coin facetté.
- **Round.** Plusieurs anneaux intermédiaires (arc). Plus lisse, plus coûteux.

*Opinion.* Pour du temps réel avec `nSides` faible (6), **miter + miter-limit → bevel** = meilleur compromis : coût quasi nul, watertight par construction, dégradation contrôlée.

*Incertitude.* Pas de référence académique canonique du *miter 3D pour tubes* : transposition d'ingénierie dérivée de la spec 2D.

---

## Famille 3 — Jointures aux branches (degré ≥ 3) — cœur du problème

### 3.1 La baseline « sphère au nœud » et pourquoi elle coûte cher

*Extrapolation (analyse de coût).*
- **Géométrie fixe et non partagée :** O(lat×long) triangles **par nœud**, indépendamment de `nSides`, **sans se souder** aux anneaux. Sur un arbre L-système à milliers de nœuds, ce surcoût domine.
- **Pas d'étanchéité gratuite :** la sphère **interpénètre** les tubes → soupe auto-intersectante **non manifold** → il faut encore une **union booléenne** pour un STL watertight. La sphère déplace le problème.
- **Qualité :** bosse sphérique visible (discontinuité C¹).

Donc *cher* **et** insuffisant : d'où la question.

### 3.2 Generalized cylinders / ramiform (fondateur)

- **Fait (sourcé).** Bloomenthal, J. (1985), « Modeling the Mighty Maple », *SIGGRAPH '85*, 19(3), 305–311 — branches = **generalized cylinders**, **surface ramiform** connectant les limbes au branchement. Le raccord est **une surface soudée**, pas un solide surajouté.

### 3.3 Surfaces de convolution / implicites (branchements lisses)

- **Fait (sourcé).** Bloomenthal, J. & Shoemake, K. (1991), « Convolution Surfaces », *SIGGRAPH '91*, 25(4), 251–256 — iso-surface d'un champ ⇒ **jonctions sans couture**, watertight natif.
- *Extrapolation (coût).* Évaluation de champ + polygonisation O(résolution³), **découplé de `nSides`** : **incompatible** régénération interactive 60k + contrôle exact de `nSides`. Qualité max, hors budget.

### 3.4 Skeleton-to-mesh : B-Mesh et Sphere-Meshes

- **Fait (sourcé).** Ji, Z., Liu, L. & Wang, Y. (2010), « B-Mesh », *CGF (Pacific Graphics)*, 29(7), 2169–2177 — marche une section le long des arêtes du squelette (un tube par arête), puis **aux nœuds collecte les sections incidentes et construit une enveloppe convexe** soudée aux tubes. **Valide la stratégie hull-des-anneaux-incidents**, manifold, sans solide surajouté ni booléen global.
- **Fait (sourcé).** Thiery, J.-M., Guy, E. & Boubekeur, T. (2013), « Sphere-Meshes », *ACM TOG (SIGGRAPH Asia)*, 32(6), art. 178 — surface interpolant des sphères (SQEM). *Interprétation :* travail d'**approximation**, pas un générateur de tube ; pertinence **conceptuelle** (hull de sphères = branchement lisse « soudé »), pas une brique temps-réel directe.

### 3.5 Solutions économiques (vrais candidats temps-réel)

1. **Interpénétration simple (overlap).** Tubes qui se recouvrent au nœud. **Coût nul.** Rendu opaque **acceptable**. **Non manifold** ; imprimabilité slicer-dépendante (*extrapolation*).
2. **Hull des anneaux incidents (B-Mesh, 3.4).** Enveloppe convexe des premiers anneaux incidents, soudée. **Coût O(k log k)/nœud**, géométrie ∝ `nSides` (pas une sphère fixe). **Manifold.** Meilleur rapport qualité/coût/étanchéité.
3. **Union booléenne** (à l'export STL seulement). Watertight garanti, coûteux, **one-shot** — pas en régénération interactive. *Incertitude :* brique non sourcée dans cette session.

---

## Famille 4 — Étanchéité (watertight) pour l'impression 3D

- **Per-segment indépendant (actuel) :** non manifold (gaps + anneaux non soudés). Non imprimable proprement.
- **Sweep anneau partagé + miter/bevel :** **manifold/watertight** par construction. *Fait géométrique.*
- **Sphère au nœud :** non manifold → booléen requis.
- **Interpénétration :** non manifold ; slicer-dépendant.
- **Hull anneaux incidents :** **manifold** si soudure correcte → watertight sans booléen.
- **Convolution/implicite :** watertight natif.
- **Union booléenne export :** watertight garanti si robuste ; coûteux.

---

## Tableau comparatif

| Méthode | Coût mém./temps | Qualité coins (deg 2) | Branches (deg ≥3) | Étanchéité STL | Complexité | Temps-réel 60k |
|---|---|---|---|---|---|---|
| Per-segment indépendant (actuel) | Élevé (2n anneaux) | Mauvaise (gaps) | Non | Non manifold | Faible (fait) | Oui mais faux |
| Sweep + Frenet | O(n) | Sauts inflexions | — | Manifold si OK | Faible | Oui |
| **Sweep + PTF/RMF (double réflexion)** | **O(n), ~2× moins d'anneaux** | **Bonne** | — | **Manifold** | Moyenne | **Oui** |
| Join **miter** (+limit → bevel) | ~nul (1 anneau/coin) | Très bonne, pincement borné | — | Manifold | Faible-moy. | Oui |
| Join round | k anneaux/coin | Excellente | — | Manifold | Moyenne | Oui |
| **Sphère au nœud** (baseline) | **Élevé (O(lat·long)/nœud fixe)** | — | Masque seulement | **Non manifold** → booléen | Faible | Dégrade si nombreux nœuds |
| Interpénétration (overlap) | **Nul** | — | Oui (opaque) | Non manifold (slicer) | Très faible | **Oui** |
| **Hull anneaux incidents (B-Mesh)** | **O(k log k)/nœud, ∝ nSides** | — | **Oui** | **Manifold** | Moyenne | **Oui** |
| Sphere-Meshes (2013) | Élevé (approx globale) | — | Oui (lisse) | Manifold | Élevée | Non (pas générateur) |
| Convolution/implicite (1991) | Élevé (O(rés³)) | Lisse | Oui (fluide) | Watertight natif | Élevée | Non |
| Union booléenne (export) | Élevé, one-shot | — | Oui | **Garanti** | Élevée | Export seulement |

---

## Méthodes retenues (CONTRAT pour la Partie 2)

Priorisation pour `src/cgmesh` sous {perf interactive 60k, branches L-système, STL souhaitablement étanche, `nSides` faible}.

### R1 — Sweep à anneau partagé avec repère RMF (double réflexion) — *base obligatoire*
- **Principe.** Balayage unique par polyligne connexe : **un anneau par sommet**, orienté par repère cohérent (double réflexion, Wang 2008) ; répartition de l'écart de fermeture pour polylignes fermées.
- **E/S.** In : `polyline` (vector<Vector3f>), `radius`, `nSides`, flag fermé. Out : bande de tube manifold (n+1 anneaux) + caps aux extrémités ouvertes.
- **Pourquoi > sphère.** Aucune géométrie de jonction ; **~×2 moins d'anneaux** que per-segment ; supprime les gaps degré 2. O(n).

### R2 — Join miter (miter-limit → bevel) aux coins (degré 2) — *base obligatoire*
- **Principe.** Anneau unique dans le plan bissecteur (ouvert 1/cos(θ/2)) ; bevel au-delà d'un angle seuil (transposition SVG `stroke-linejoin`/`stroke-miterlimit`).
- **E/S.** In : tangentes entrante/sortante par sommet, `radius`, seuil. Out : jointure manifold sans auto-intersection.
- **Pourquoi.** Coût quasi nul, watertight par construction ; résout les décrochements de coins.

### R3 — Nœuds de branchement : hull des anneaux incidents (B-Mesh) — *option qualité + étanchéité*
- **Principe.** Détecter les nœuds implicites (extrémités à coordonnées partagées → construire le **graphe**), collecter le premier anneau de chaque branche, **enveloppe convexe** triangulée et **soudée** aux anneaux (Ji 2010).
- **E/S.** In : position du nœud, anneaux incidents (k directions), `radius`, `nSides`. Out : coque de jonction **manifold** → watertight sans booléen.
- **Pourquoi > sphère.** O(k)·∝`nSides` (petit, adaptatif) au lieu d'une sphère fixe non soudée ; **manifold** ; pas de booléen.

### R4 — Interpénétration simple (overlap) — *fallback économique*
- **Principe.** Tubes qui se recouvrent, aucune géométrie de jonction.
- **Rôle.** Chemin le moins cher (rendu opaque) et **filet de sécurité** quand R3 dégénère (branches quasi-colinéaires, k=2, rayons très différents). Imprimable via tolérance slicer, non garanti watertight.

### Combinaison recommandée par défaut : **R1 + R2 + R3**, avec **R4 en repli**
1. **R1** partout (gain net : moins de triangles, plus de gaps degré 2).
2. **R2** sur les sommets internes de degré 2.
3. **R3** sur les nœuds de degré ≥ 3 (via le graphe) → étanchéité STL sans booléen.
4. **R4** en repli sur nœuds dégénérés + mode « draft » ultra-rapide.
5. **Union booléenne** hors cœur de pipeline, éventuellement en passe d'export STL (à évaluer séparément).

*Justification (Opinion argumentée).* Trajectoire validée par la littérature — repère minimisant la rotation (Bishop 1975 ; Hanson & Ma 1995 ; Wang 2008) + jonctions soudées plutôt que solides surajoutés (Bloomenthal 1985) + hull local aux nœuds (Ji 2010) — dans un budget O(n) compatible 60k interactif, avec étanchéité sans le coût des implicites (Bloomenthal & Shoemake 1991) ni de la simplification globale (Thiery 2013).

## Sources
- Bishop (1975), *Amer. Math. Monthly* 82(3):246–251.
- Hanson & Ma (1995), IU TR 425.
- Wang, Jüttler, Zheng & Liu (2008), *ACM TOG* 27(1):art.2.
- Bloomenthal (1985), *SIGGRAPH '85* 19(3):305–311.
- Bloomenthal & Shoemake (1991), *SIGGRAPH '91* 25(4):251–256.
- Ji, Liu & Wang (2010), *CGF* 29(7):2169–2177.
- Thiery, Guy & Boubekeur (2013), *ACM TOG* 32(6):art.178.
- W3C SVG — `stroke-linejoin`, `stroke-miterlimit`.

**Limites.** Périmètre anglophone ; transposition 3D des joins et coûts O(·) = analyses d'ingénierie ; brique booléenne non sourcée ; consulté le 2026-07-16.

---

# Partie 2 — Faisabilité dans `src/cgmesh`

> Analyse produite après lecture ciblée de `surface_basic.{h,cpp}`, `parameterized_shapes.cpp`, `mesh.h`, `chull.{h,cpp}`. Aucun fichier modifié.
> Convention : **[vérifié]** = lu dans le code ; **[hypothèse]** = déduction non confirmée par exécution.

## 0. Faits d'ancrage (état actuel vérifié)

- **[vérifié]** `CreateTubes(polylines, radius, nSides=6, caps=true)` tube **chaque segment indépendamment** : `nv = nSeg*2*K` (2 anneaux non partagés/segment), repère de section arbitraire recalculé par segment (`a`=Z ou X selon `|d.z|<0.9`, `u=a×d`, `v=d×u`). Aucune continuité de repère → vrille et décrochements.
- **[vérifié]** Faces : `nf = nSeg*2*K` latéraux + `nCapped*2*(K-2)` (fans de cap). Triangulé, via `SetVertices`/`SetFaces`.
- **[vérifié]** `isClosed(pl)` = `size>=3 && dist²(front,back) < (0.5*r)²` ; une fermée n'est pas capuchonnée mais reste tubée par segments (anneaux extrêmes non soudés → trou résiduel).
- **[vérifié]** Segments dégénérés ignorés ; mesh vide → triangle de secours.
- **[vérifié]** Appelants de `CreateTube`/`CreateTubes` dans tout le dépôt : **uniquement** `ParameterizedLSystem::Regenerate()` + wrapper interne → **signature modifiable à faible risque**.
- **[vérifié]** `ParameterizedLSystem::Regenerate()` découpe le walk en `chains` (rupture `m_bDrawable==false`), applique le **cap 60000 segments dans l'appelant**, normalise (centre + échelle diag→4), puis `CreateTubes(chains, m_thickness, 6)` + `ComputeNormals()`. Branches partageant des coordonnées **exactes** (même transformée affine) → nœuds implicites détectables par position.
- **[vérifié]** `chull` = `Chull3D(float* v,int n)` ; `compute()` ; `get_convex_hull(float**,int*,int**,int*)` (arrays `malloc`, triangles CCW extérieur, à `free`) ; `get_convex_hull_indices(int**,int*)` renvoie **l'index d'entrée d'origine** de chaque sommet du hull (essentiel pour ressouder) ; dégénérescences colinéaire/coplanaire → `printf` + retour `1` (égalité flottante **exacte**).
- **[vérifié]** Étanchéité : `Mesh::MergeVertices(1e-6)`, `Mesh::Append(Mesh*)`, `Mesh::GetTopologicIssues(nonManifoldBorders, borders)`.

## 1. Cartographie existant / manquant (R1 → R4)

### R1 — Sweep anneau partagé + RMF (double réflexion)
**Déjà là [vérifié]** : parcours par polyligne, tangente par segment, anneau `K`-gone, caps, détection fermée, skip dégénérés, sortie `SetVertices`/`SetFaces`.
**Manquant [vérifié]** : (1) **anneau partagé** (1/sommet, `n+1` anneaux au lieu de `2n`) ; (2) **continuité de repère** (init au 1ᵉʳ segment puis **double réflexion** `t_i→t_{i+1}`) ; (3) **repère au sommet** (bissectrice) ; (4) **fermeture (holonomie)** : souder anneau 0↔n + répartir l'écart de rotation. Écart = refonte du corps de sweep (indices + repère).

### R2 — Join miter (miter-limit → bevel), degré 2
**Déjà là [vérifié]** : rien (l'indépendance par segment *est* l'absence de join).
**Manquant [hypothèse]** : anneau au sommet dans le **plan bissecteur**, rayon `1/cos(θ/2)` ; **miter-limit** → bascule **bevel** (2 anneaux + couronne). R1 supprime déjà les *trous* degré 2 (manifold) ; R2 = **raffinement géométrique** (position/rayon), pas correction de topologie.

### R3 — Hull des anneaux incidents (degré ≥3, B-Mesh)
**Déjà là [vérifié]** : `Chull3D` réutilisable + `get_convex_hull_indices` pour ressouder — atout majeur, pas de hull à écrire.
**Manquant** : (1) **reconstruction du graphe** (clusteriser positions partagées avec **tolérance**) — la signature `CreateTubes(polylines,...)` **suffit** (toutes les polylignes présentes) ; (2) **[hypothèse]** un nœud peut être **interne** à une polyligne (parent traversant) et pas seulement une extrémité — le `]` (pop) tend à couper le parent → détection par extrémités *souvent* suffisante, **non garanti** → prévoir repli sur tous les sommets ; (3) trim/hull/stitch soudé ; (4) **robustesse chull** (colinéaire/coplanaire → échec → déclenche R4).

### R4 — Interpénétration (overlap) — repli
**Déjà là [vérifié]** : c'est le comportement par branche une fois R1 en place (tubes se recouvrent au nœud, coût nul). **Manquant** : ne **pas capuchonner** les extrémités internes à un nœud ; `MergeVertices` final optionnel. Sert de **filet** quand R3 dégénère.

## 2. Décisions d'architecture / points d'intégration
- **RMF (R1)** : **dans `CreateTubes`, par polyligne** (repère = propriété de la courbe, garde le découplage). Extraire un `sweepPolyline(...)` interne. Caps → anneau unique début/fin. Fermée → souder n↔0 + holonomie.
- **Joins (R2)** : dans le sweep, au sommet interne degré 2 ; `sweepPolyline` peut émettre 1 (miter) ou 2 (bevel) anneaux. Paramétrer via `struct TubeOptions { float miterLimit; enum JoinMode; ... }`.
- **Graphe/nœuds (R3)** : **dans `CreateTubes`** (reçoit déjà toutes les polylignes ; garde le graphe implicite dans la primitive). Hachage spatial + `tolEps` (défaut ≈ `0.1*radius`). Après normalisation les coords sont exactement partagées **[hypothèse forte]** donc `tolEps` quasi nul suffirait, mais paramétrable par robustesse. **Point ouvert** : nœud interne (§1 R3-2).
- **chull (R3)** : **réutiliser `Chull3D`** (ne pas réimplémenter) ; remapper via `get_convex_hull_indices` ; **enrober `compute()`** (échec → R4) ; gérer `free`/`printf` ; trimmer le tube à l'anneau utilisé.
- **Compat** : caps conservés degré 1, désactivés aux extrémités internes ; `isClosed` réutilisé (R1 doit souder) ; `nSides` inchangé (hull ∝ `nSides`) ; **export STL/GLB/OBJ inchangé** (toujours `SetFaces`) ; cap 60000 reste dans l'appelant ; signature via `TubeOptions` (defaults iso-comportement) pour ne rien casser.

## 3. Plan incrémental
- **(a) R1** — sweep anneau partagé + RMF. Fichiers : `surface_basic.{cpp,h}`. Effort **M**, risque moyen. Validation : `nVertices` ~×2 moins, disparition vrille/gaps degré 2, tube ouvert **watertight** (`GetTopologicIssues`→0 bord), boucle fermée sans trou.
- **(b) R2** — miter→bevel. Fichiers : `surface_basic.{cpp,h}` (+`TubeOptions`). Effort **S–M**, risque faible-moyen. Validation : coins nets sans pincement (60/90°), bevel au-delà du seuil, toujours watertight.
- **(c) R3** — graphe + hull. Fichiers : `surface_basic.{cpp,h}` (+réutilise `chull`). Effort **L**, risque élevé. Validation : nœuds degré ≥3 cohérents, **watertight sans booléen** (`GetTopologicIssues`=0 aux jonctions), jonctions soudées sans bosse.
- **(d) R4** — repli overlap. Fichiers : `surface_basic.cpp` (+`MergeVertices` optionnel). Effort **S**, risque faible. Validation : aucun crash sur nœuds dégénérés, mode draft plus rapide.

## 4. Trade-offs & risques
- Rétro-compat signature : **faible** (2 appelants internes) ; préférer `TubeOptions`.
- Graphe : `O(N)` avec hachage ; vrai enjeu = **`tolEps`** (trop grand → fusion abusive ; trop petit → nœuds manqués).
- RMF sur angles vifs L-système : défini (contrairement à Frenet) mais **miter agressif explose** → **miter-limit obligatoire** + bevel.
- Dégénérescences : segments nuls déjà filtrés ; **chull échoue colinéaire/coplanaire** (float exact, `printf`) → **router vers R4** ; neutraliser le `printf`.
- Perf 60k : R1 réduit ~×2 les sommets ; R3 = `O(nœuds × hull(k·nSides))`, `k·nSides`≈18 petit mais **milliers de nœuds** → prévoir mode draft (R4) interactif, R3 à l'export.
- Étanchéité STL : R1 rend chaque tube manifold ; **branches non soudées sans R3** (T-junctions ; `MergeVertices` recolle géométriquement mais ne crée pas les faces de jonction) → **R3 est la vraie condition du watertight aux branches**.
- `Chull3D` : `malloc`/`free` manuel → risque de fuite, encapsuler.

## 5. Recommandation par défaut
**Implémenter (a) R1 puis (b) R2, livrer/valider ce socle avant R3.** R1+R2 = l'essentiel du bénéfice visuel (fin de la vrille, des gaps degré 2, coins propres) + chaque tube manifold, effort M+S, risque contenu, sans toucher aux appelants. **Engager (c) R3 ensuite pour l'étanchéité STL aux branches** (réutilise `Chull3D`), avec **(d) R4 en repli systématique** (dégénérescences garanties). Pas d'union booléenne (hors périmètre temps-réel).

**À valider avec l'utilisateur (choix ouverts) :**
1. **Miter-limit par défaut** (angle miter→bevel) — impacte les angles 60/90°.
2. **`tolEps`** (défaut ≈ `0.1*radius`) et surtout : détecter les nœuds sur **extrémités seules** ou **tous les sommets** ? (dépend du nœud-interne, **[hypothèse]** à confirmer sur les L-systèmes réels).
3. **R3 maintenant ou plus tard** : livrer R1+R2 comme jalon autonome, ou viser R1+R2+R3+R4 d'une traite ?
4. **`struct TubeOptions` vs surcharges** (préférence : `TubeOptions`).
</content>
