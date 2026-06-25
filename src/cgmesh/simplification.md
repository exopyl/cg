# Simplification de maillages 3D triangulés — Décimation géométrique

> Document structuré en deux parties, à ne pas fusionner :
> - **[Partie 1 — État de l'art](#partie-1--état-de-lart)** (agent recherche) — cartographie des familles de méthodes ; le contrat d'interface est la section **[§5 Méthodes retenues](#5-méthodes-retenues)**.
> - **[Partie 2 — Faisabilité](#partie-2--faisabilité)** (agent architecte) — instruit, sur le code existant de `src/cgmesh`, **exactement** les méthodes retenues en §5 (cohérence Partie 1 ↔ Partie 2 vérifiée).
>
> **Définition opérationnelle fixée** : décimation géométrique sous 3 contraintes — topologie préservée (manifold), arêtes vives/features, attributs d'apparence.
> **Date de consultation des sources** : 2026-06-24.

---

# Partie 1 — État de l'art

## 0. Cadrage et définition opérationnelle

**Sujet** : *décimation géométrique de maillages triangulés* — réduire le nombre de triangles/sommets d'un maillage triangulé tout en préservant au mieux la forme.

**Contraintes cibles** (fixées, non rediscutées ; elles structurent l'analyse et le choix des « méthodes retenues ») :

1. **Topologie préservée** — rester *manifold*, ne pas fusionner de composantes connexes distinctes, conserver le genre (genus) de la surface.
2. **Préservation des features** — plis (creases), arêtes vives (sharp edges), silhouettes, bords (boundaries).
3. **Préservation des attributs d'apparence** — normales, coordonnées de texture (UV), couleurs/textures, et plus généralement champs définis sur la surface.

**Contexte d'implémentation aval** (non sur-spécialisant) : structure de données **half-edge**, donc préférence opérationnelle pour les méthodes compatibles **edge-collapse / half-edge collapse** avec **file de priorité**. L'état de l'art reste néanmoins général.

**Questions structurantes opérationnelles** :
- Q1 — Quelles familles de décimation respectent *intrinsèquement* la contrainte de topologie (1), et lesquelles la violent par construction ?
- Q2 — Comment les métriques d'erreur encodent-elles features (2) et attributs (3) ?
- Q3 — Quelles bornes d'erreur géométrique (Hausdorff, enveloppes) sont garanties par quelles familles ?
- Q4 — Quels sont les apports récents (≈2018–2026 : parallèle/GPU, méthodes apprises, simplification pilotée par l'apparence/rendu) et sont-ils compatibles avec une pipeline half-edge ?

### Convention d'étiquetage (Règle d'Or)

Chaque affirmation est marquée :
- **[fait sourcé]** — rattachée à une source primaire vérifiable (lien/DOI/arXiv ci-dessous).
- **[extrapolation]** — déduction allant au-delà de ce que la source affirme littéralement, source de départ indiquée.
- **[opinion]** — lecture personnelle de l'analyste, n'engageant pas l'état de la science.

**Profondeur de lecture** : les sources lues *en intégralité* (texte extrait) sont signalées « (lu intégralement) ». Les autres reposent sur abstract + page de garde + fiches DBLP/éditeur, signalées « (abstract/métadonnées) ». Aucun résumé « de confiance » n'est donné pour un paper paywallé non lu.

---

## 1. Cartographie des approches

On distingue les grandes familles suivantes (terminologie consacrée). La taxonomie de base suit la revue classique de Cignoni, Montani & Scopigno (1998), enrichie des familles récentes. **[fait sourcé]** : la catégorisation par stratégie de réduction « tout en maintenant la topologie » est attribuée à [Cignoni et al. 1998] par la revue récente de Kulkarni & Narayanan (2025), p. 3 (lu intégralement).

| # | Famille | Principe en une phrase | Topologie (contrainte 1) |
|---|---|---|---|
| A | **Décimation itérative locale par edge-collapse / half-edge collapse** | Effondrer itérativement une arête (deux sommets → un), priorisé par une fonction de coût ; cœur des méthodes modernes. | Préservable (contrôlable par tests de validité) |
| A.1 | — sous-famille **Quadric Error Metrics (QEM)** | Coût = forme quadratique (matrice 4×4) sommant les distances au carré aux plans des faces incidentes. | idem |
| A.2 | — sous-famille **memoryless / volume-based (Lindstrom–Turk)** | Coût géométrique local (volume, aire) sans historique de la géométrie initiale. | idem |
| A.3 | — sous-famille **QEM étendue aux attributs** (apparence) | Quadrique en dimension n>3 intégrant normales/couleurs/UV. | idem |
| B | **Décimation de sommets (vertex removal) — Schroeder** | Retirer un sommet, re-trianguler le trou localement. | Préservée par construction (classification topologique) |
| C | **Vertex clustering (Rossignac–Borrel) et dérivés** | Regrouper les sommets par cellules d'une grille, un représentant par cellule. | **Violée** (peut fusionner composantes / changer le genre) |
| C.1 | — **out-of-core / quadrique de cluster (Lindstrom)** | Clustering + quadrique pour placer le représentant ; passe unique, très gros modèles. | **Violée** (même limite que C) |
| D | **Méthodes à erreur bornée** (enveloppes / Hausdorff) | Garantir que le maillage simplifié reste à ≤ ε de l'original (et réciproquement). | Préservée (contrainte explicite des méthodes) |
| E | **Remaillage / retiling** (Turk 1992) | Ré-échantillonner la surface puis re-trianguler. | Variable ; pas conçu pour les attributs |
| F | **Cadre multi-résolution : Progressive Meshes (Hoppe 1996)** | Représentation continue (suite d'edge-collapse réversibles) — *cadre*, pas une métrique. | Préservable |
| G | **Méthodes apprises / neuronales** (depuis 2022) | Réseau (souvent géométrie profonde) qui propose la décimation ou une représentation LOD. | Variable, souvent non garantie |
| H | **Simplification pilotée par l'apparence / le rendu** (depuis 2021) | Optimiser géométrie+shading pour minimiser la différence *image* rendue. | Variable, souvent non garantie |
| I | **Erreur intrinsèque** (Liu et al. 2023) | Décimation guidée par une triangulation intrinsèque, garanties de qualité d'élément. | Préservée (carte bijective fin↔grossier) |

**Conséquence directe pour le contrat aval** : les familles **C / C.1** (vertex clustering) **violent la contrainte 1** (fusion possible de composantes, modification du genre). Elles sont incluses **pour contraste** mais **ne peuvent pas être « retenues » telles quelles**. **[fait sourcé]** : le caractère « compromettant le détail et l'exactitude topologique » du vertex clustering est explicitement noté par Kulkarni & Narayanan (2025), p. 3 (lu intégralement) ; **[fait sourcé]** : Rossignac & Borrel (1993) revendiquent eux-mêmes la capacité à joindre des régions non connectées comme un avantage de débit, ce qui est précisément la propriété incompatible avec la contrainte 1 (abstract/métadonnées).

---

## 2. Références clés par famille

### Famille A — Décimation itérative locale par edge-collapse

#### A.1 — Quadric Error Metrics (QEM) — *fondateur*

- **Garland & Heckbert, SIGGRAPH 1997, « Surface Simplification Using Quadric Error Metrics »**, pp. 209–216 (abstract/métadonnées + page projet auteur).
  - **[fait sourcé]** Principe : contractions itératives de *paires* de sommets ; l'erreur de surface est approximée par des matrices quadriques 4×4 (une par sommet), le coût d'une contraction étant évalué via cette quadrique.
  - **[fait sourcé]** En contractant des paires *arbitraires* (pas seulement des arêtes), l'algorithme peut joindre des régions non connectées — propriété qui, activée, violerait la contrainte 1. **[extrapolation, à partir de Garland & Heckbert 1997]** : restreindre aux contractions d'arêtes (edge-collapse) suffit à éviter cette fusion ; c'est la pratique standard dans les implémentations préservant la topologie.
  - Niveau de preuve : article de conférence séminal, abondamment répliqué et réimplémenté (QSlim, CGAL, meshoptimizer — voir section Outils).

#### A.1bis — QEM appliqué : couleur et texture — *extension fondatrice*

- **Garland & Heckbert, Visualization 1998, « Simplifying Surfaces with Color and Texture Using Quadric Error Metrics »**, pp. 263–269 (abstract/métadonnées).
  - **[fait sourcé]** Étend le coût de contraction à des attributs de sommet à valeur réelle quelconques (couleur, texture). Première extension QEM vers la contrainte 3 (apparence).

#### A.3 — QEM pour attributs d'apparence — *référence centrale pour la contrainte 3*

- **Hoppe, Visualization 1999, « New Quadric Metric for Simplifying Meshes with Appearance Attributes »**, pp. 59–66 (abstract/métadonnées + page projet auteur + fiche DBLP).
  - **[fait sourcé]** Nouvelle métrique quadrique fondée sur une correspondance géométrique en 3D : moins de stockage, évaluation plus rapide, maillages simplifiés plus précis que l'extension de 1998.
  - **[fait sourcé]** Gère les *discontinuités d'attributs* (creases de surface, frontières de matériau) qui imposent plusieurs vecteurs d'attributs par sommet, via une structure « wedge-based » permettant l'optimisation simultanée de ces vecteurs.
  - **[fait sourcé]** La quadrique mesure à la fois l'exactitude géométrique de la surface et la fidélité des champs d'apparence (normales, couleurs) définis dessus.
  - **[extrapolation]** : référence pivot pour satisfaire conjointement C2 (creases/discontinuités sont des features) et C3 (attributs) dans un cadre edge-collapse.
- **Référence de suivi** : « Efficient Minimization of New Quadric Metric for Simplifying Meshes with Appearance Attributes » (rapport MSR / Cornell), optimise la métrique de Hoppe 1999 (abstract/métadonnées).

#### A.2 — Memoryless / volume-based — *fondateur alternatif*

- **Lindstrom & Turk, Visualization 1998, « Fast and Memory Efficient Polygonal Simplification »**, pp. 279–286 (abstract/métadonnées + PDF auteur référencé).
  - **[fait sourcé]** Suite d'edge-collapse *sans* conserver l'historique de la géométrie de l'original (« memoryless »).
  - **[fait sourcé]** La position du nouveau sommet est choisie pour *préserver le volume* et minimiser le changement de volume par triangle ; préservation de l'aire près des bords et minimisation du changement d'aire par triangle.
  - **[fait sourcé]** Adapté aux grands modèles et aux applications visant une faible erreur géométrique moyenne.
  - **[fait sourcé]** Évaluation comparative ultérieure : « Evaluation of Memoryless Simplification », IEEE TVCG 1999 (abstract/métadonnées).

#### A — *récent* (≈2018–2026)

- **Kulkarni & Narayanan, arXiv:2512.19959 (23 déc. 2025), « A Comprehensive Guide to Mesh Simplification using Edge Collapse »** (lu intégralement, 46 pages).
  - **[fait sourcé]** Guide implémentation-orienté : dérive QEM, les critères de Lindstrom–Turk, le coût attribute-aware des variantes QEM, et la méthode énergétique de Hoppe (progressive meshes). p. 1, pp. 3–4.
  - **[fait sourcé]** Structures de données (pp. 4–5) : la **half-edge** (attribuée à McGuire 2000) est « largement utilisée », « idéale pour l'edge collapse » (requêtes rapides, mises à jour locales) ; le Corner Table (Rossignac 2002) est l'autre option compacte. Les deux offrent O(degré(v)) pour les requêtes nécessaires (Table 1, p. 6).
  - **[fait sourcé]** Insiste sur les *garde-fous* contre dégénérescences, normales inversées, mauvaise gestion des bords (§4, p. 6+). Pertinent pour C1 et C2.
  - **[opinion]** : *guide de synthèse*, non revue à comité affiché ; valeur pédagogique, pas de résultat empirique nouveau.
- **Bhosikar, Savalia, Tiwari & Bhowmick, arXiv:2605.14029 (13 mai 2026), « Fast and Robust Mesh Simplification for Generated and Real-World 3D Assets »** (abstract/métadonnées via fetch).
  - **[fait sourcé]** Propose **FA-QEM** : formulation quadrique multi-termes encodant déviation géométrique, courbure de bord et cohérence des normales ; vise la préservation des features sous décimation agressive et un meilleur transfert d'apparence pour le texture mapping.
  - **[fait sourcé]** Évalué sur Thingi10K et « Real-World Textured Things » ; revendique erreur plus faible, meilleure fidélité visuelle, runtimes plus rapides.
  - **[à vérifier]** Préservation topologique non explicitée dans l'abstract ; non lu intégralement.

### Famille B — Décimation par suppression de sommets (Schroeder) — *fondateur*

- **Schroeder, Zarge & Lorensen, SIGGRAPH 1992, « Decimation of Triangle Meshes »**, Computer Graphics 26(2) (abstract/métadonnées).
  - **[fait sourcé]** Élimination itérative de sommets ; faces adjacentes supprimées et voisinage local re-triangulé dans le plan local du sommet.
  - **[fait sourcé]** Revendique explicitement la **préservation de la topologie originale** et une bonne approximation ; opérations locales, indépendant de l'application.
  - **[extrapolation]** : préservation topologique comme invariant *de conception* (classification des sommets). Aligné sur C1/C2, mais ne traite pas nativement C3.

### Famille C — Vertex clustering — *fondateur, NON retenable (contraste)*

- **Rossignac & Borrel, 1993, « Multi-resolution 3D approximations for rendering complex scenes »**, in *Modeling in Computer Graphics* (Springer), pp. 455–465, DOI 10.1007/978-3-642-78114-8_29 (abstract/métadonnées).
  - **[fait sourcé]** Grille uniforme ; tous les sommets d'une cellule remplacés par un représentant unique. Pas de graphe d'adjacence à maintenir : faible coût, fort taux de réduction.
  - **[fait sourcé]** Conséquence : ne préserve PAS la topologie (peut fusionner des régions). **→ exclu (violation C1).**

#### C.1 — Out-of-core quadrique de cluster — *NON retenable (contraste)*

- **Lindstrom, SIGGRAPH 2000, « Out-of-Core Simplification of Large Polygonal Models »**, pp. 259–262, DOI 10.1145/344779.344912 (abstract/métadonnées).
  - **[fait sourcé]** Étend le vertex clustering via l'information de quadrique pour placer le représentant de chaque cluster ; meilleure préservation des détails, faible erreur moyenne, une seule passe, temps linéaire.
  - **[extrapolation, à partir de Lindstrom 2000]** : hérite de la limite topologique du clustering. **→ contraste seulement.**
- **Garland & Zhou, « Quadric-based simplification in any dimension », ACM TOG 2005**, DOI 10.1145/1061347.1061350 (abstract/métadonnées) — généralisation des quadriques à toute dimension (utile pour attributs). **[à vérifier]** non lu intégralement.

### Famille D — Méthodes à erreur bornée — *fondateur*

- **Cohen, Varshney, Manocha, Turk, Weber, Agarwal, Brooks, Wright, SIGGRAPH 1996, « Simplification Envelopes »** (abstract/métadonnées + page projet UNC).
  - **[fait sourcé]** Hiérarchie de LOD garantissant que tout point de l'approximation est à distance ≤ ε de l'original, et réciproquement (deux maillages offset englobant). Erreur bornée explicite.
- **Klein, Liebich & Straßer, IEEE Visualization 1996, « Mesh Reduction with Error Control »** (abstract/métadonnées + PDF auteur Uni Bonn).
  - **[fait sourcé]** Utilise la **distance de Hausdorff** entre maillage original et simplifié comme erreur géométriquement significative ; réduit le nombre de triangles sans dépasser un seuil de Hausdorff défini par l'utilisateur.
- **Outil de mesure associé** : Aspert, Santa-Cruz & Ebrahimi, « MESH: Measuring Errors between Surfaces Using the Hausdorff Distance », ICME 2002 (abstract/métadonnées) — métrique d'évaluation, pas une méthode de décimation.

### Famille E — Remaillage / retiling — *fondateur, périphérique*

- **Turk, SIGGRAPH 1992, « Re-tiling Polygonal Surfaces »** (cité par Kulkarni & Narayanan 2025, pp. 3–4 ; paper original non lu).
  - **[fait sourcé, via Kulkarni & Narayanan 2025 pp. 3–4]** Place aléatoirement un nombre réduit de nouveaux sommets sur la surface, ajustés selon la courbure, puis re-triangule. Efficace en réduction mais **ne supporte pas les attributs par sommet** (C3). Citation « d'après X cité par Y ».

### Famille F — Cadre Progressive Meshes — *cadre fondateur*

- **Hoppe, SIGGRAPH 1996, « Progressive Meshes »**, pp. 99–108 (abstract/métadonnées + PDF auteur).
  - **[fait sourcé]** Représentation multi-résolution : maillage de base + suite d'opérations *vertex split* (inverse *edge collapse*) ; raffinement progressif, résolution continue, compression.
  - **[fait sourcé, via Kulkarni & Narayanan 2025 p. 3]** Antérieur : Hoppe et al. 1993 (« Mesh Optimization »), méthode énergétique globale (collapse/swap/split), moins utilisée pour sa complexité.
  - **[extrapolation]** : *cadre* (sérialisation LOD) s'appuyant sur une métrique de coût (souvent QEM ou énergie de Hoppe) ; orthogonal aux familles A.1/A.3. Compatible half-edge (l'edge-collapse en est l'atome).

### Famille G — Méthodes apprises / neuronales — *récent*

- **Potamias, Ploumpis & Zafeiriou, CVPR 2022, « Neural Mesh Simplification »** (abstract/métadonnées ; IEEE Xplore doc 9878835).
  - **[fait sourcé]** Apprentissage géométrique profond pour un modèle de décimation *différentiable*, plus rapide que les algorithmes classiques ; simplification *en une passe* : échantillonne un sous-ensemble de sommets, puis un réseau d'attention sparse propose des triangles candidats selon la connectivité d'arêtes. Généralisable sans réentraînement par maillage.
  - **[à vérifier]** Préservation topologique / manifoldness en sortie non garantie par construction (ré-échantillonnage + re-triangulation apprise). Non lu intégralement.
- **Chen, Kim, Aigerman & Jacobson, SIGGRAPH 2023, « Neural Progressive Meshes »** (abstract/métadonnées + cité par Kulkarni & Narayanan 2025 p. 4).
  - **[fait sourcé, via Kulkarni & Narayanan 2025 p. 4]** Maillage de base grossier par QEM, puis remaillage neuronal par face splits ; représentation latente par face décodée côté client → LODs multiples.
- **Chen et al., Computational Intelligence 2024, « A progressive mesh simplification algorithm based on neural implicit representation »** (abstract/métadonnées). **[à vérifier]** non lu.

### Famille H — Simplification pilotée par l'apparence / le rendu — *récent*

- **Hasselgren, Munkberg, Lehtinen, Aittala & Laine, EGSR 2021 (arXiv:2104.03989), « Appearance-Driven Automatic 3D Model Simplification »** (abstract/métadonnées + page NVIDIA + dépôt nvdiffmodeling).
  - **[fait sourcé]** Optimisation conjointe maillage + modèle de shading pour reproduire l'apparence d'une scène de référence, pilotée *uniquement* par la différence en espace-image via un rendu différentiable (analysis-by-synthesis). Supporte normal mapping, BRDF spatialement variables, displacement mapping.
  - **[extrapolation]** : approche orientée *rendu* (LOD temps réel), non conçue pour garantir manifoldness/topologie ; complémentaire d'une décimation QEM, pas substitut sous nos contraintes.
- **Liu, Zhang & Yuksel, SIGGRAPH Asia 2025 (arXiv:2409.15458), « Simplifying Textured Triangle Meshes in the Wild »** (abstract/métadonnées via fetch ; ACM TOG 10.1145/3763277).
  - **[fait sourcé]** Décimation de *2-complexes simpliciaux* pour traiter collectivement plusieurs composantes non-manifold ; **quadrique d'erreur modifiée** qui *converge vers la QEM classique pour les maillages manifold étanches*. Cible la qualité visuelle des maillages texturés.
  - **[extrapolation]** : généralise la QEM ; sous entrée manifold imposée, se réduit asymptotiquement à la QEM standard. Traitement texture/UV instructif.

### Famille I — Erreur intrinsèque — *récent*

- **Liu, Gillespie, Chislett, Sharp, Jacobson & Crane, SIGGRAPH 2023 (arXiv:2305.06410), « Surface Simplification using Intrinsic Error Metrics »**, ACM TOG 42(4) (abstract/métadonnées via fetch).
  - **[fait sourcé]** Décimation gloutonne guidée par une métrique *intrinsèque* : construit une triangulation intrinsèque grossière du domaine ; substitue aux quadriques extrinsèques des tangentes intrinsèques suivant la dérive de courbure.
  - **[fait sourcé]** Garanties *dures* sur la qualité des éléments via retriangulation intrinsèque ; **carte bijective entre maillage fin et grossier** (multigrille géométrique, géodésiques).
  - **[opinion]** : paradigme distinct ; pertinent si l'objectif aval est le calcul sur surface, moins central pour le rendu/LOD avec attributs extrinsèques (UV/textures) que l'erreur intrinsèque ne mesure pas directement. Complément, pas base unique.

---

## 3. Comparaison transversale

Critères : **principe** · **qualité géométrique** (Hausdorff/QEM) · **topologie (C1)** · **features (C2)** · **attributs d'apparence (C3)** · **coût/complexité** · **pré-requis (structure de données)** · **implémentation de référence**.

| Famille | Principe | Qualité géom. | Topologie (C1) | Features (C2) | Attributs (C3) | Coût | Pré-requis | Implém. réf. |
|---|---|---|---|---|---|---|---|---|
| **A.1 QEM** (G&H 1997) | Quadrique 4×4/face, edge-collapse priorisé | Très bonne ; standard de fait | Préservable si **edge-collapse only** + tests de validité | Bonne (quadriques de plan capturent les plis) | Non native (cf. A.1bis/A.3) | O(n log n) (file de priorité) | Adjacence + file de priorité ; half-edge adapté | QSlim, CGAL, meshoptimizer (outils) |
| **A.2 Memoryless** (Lindstrom–Turk 1998) | Coût volume/aire local, sans historique | Bonne erreur moyenne ; faible mémoire | Préservable (edge-collapse) | Moyenne–bonne (préserve bords/volume) | Non native | Faible mémoire, rapide | half-edge adapté | réimplémentations diverses |
| **A.3 QEM+attributs** (Hoppe 1999 ; G&H 1998) | Quadrique dim n>3 (pos+attributs) | Très bonne, incl. champs d'apparence | Préservable (edge-collapse) | **Bonne** (discontinuités/creases via wedges) | **Oui (cible)** : normales, couleurs, UV | Plus coûteux (dim. + haute) | Structure « wedge »/coins + half-edge | Hoppe ; variantes |
| **B Vertex removal** (Schroeder 1992) | Retrait sommet + re-triangulation locale | Bonne | **Préservée par conception** | Bonne (classification de sommets) | Non native | Faible | Adjacence locale | VTK (outil) |
| **C Vertex clustering** (Rossignac–Borrel 1993) | Grille, 1 représentant/cellule | Variable (dépend grille) | **VIOLÉE** | Faible | Faible | Très faible, débit élevé | Grille spatiale (pas d'adjacence) | MeshLab (outil) |
| **C.1 OoC quadrique** (Lindstrom 2000) | Clustering + quadrique, 1 passe | Meilleure que C | **VIOLÉE** (hérite de C) | Faible–moyenne | Non | Linéaire, très gros modèles | Grille + quadriques | OoCS |
| **D Erreur bornée** (Cohen 1996 ; Klein 1996) | Enveloppes ε / seuil Hausdorff | **Garantie ε** | Préservée (contrainte) | Bonne | Non native | Élevé | Structures de distance | SIMPLE (UNC) |
| **F Progressive Meshes** (Hoppe 1996) | Cadre LOD (vsplit/ecol) | = métrique sous-jacente | Préservable | = métrique | = métrique | Modéré | half-edge naturel | PM |
| **G Neuronal** (Potamias 2022 ; Chen 2023) | Réseau différentiable / latents | Variable, dépend données | **Non garantie** | Variable | Variable | Inférence rapide, entraînement lourd | GPU, framework DL | dépôts associés |
| **H Apparence/rendu** (Hasselgren 2021 ; Liu 2025) | Optimisation image-space / 2-complexes | Orientée perception visuelle | **Non garantie** (H21) / converge vers QEM si manifold (Liu25) | Bonne (visuelle) | **Oui (cible visuelle)** | Élevé (rendu différentiable) | Rendu diff. / GPU | nvdiffmodeling |
| **I Erreur intrinsèque** (Liu 2023) | Triangulation intrinsèque + décimation | Garanties qualité d'élément ; carte bijective | **Préservée** | Intrinsèque (courbure) | Pas la cible (extrinsèque non mesurée) | Modéré | Triangulation intrinsèque | repo associé |

> Note : « topologie préservable » = *contrôlable par tests de validité de collapse* (link condition, anti-flip de normale, anti-fold, vérif non-manifold), pas garantie automatique. **[fait sourcé]** : la nécessité de ces garde-fous est documentée par Kulkarni & Narayanan 2025, §4 (lu intégralement).

### Convergences et désaccords

- **Convergence forte** (méthodes différentes, même conclusion) : l'edge-collapse priorisé par une métrique d'erreur est le paradigme dominant pour préserver simultanément forme et topologie — confirmé indépendamment par les familles A.1, A.2, A.3, F et la pratique outillée. **[fait sourcé, multi-sources]**.
- **Convergence sur la structure de données** : half-edge (et corner table) sont les supports recommandés pour l'edge-collapse. **[fait sourcé : Kulkarni & Narayanan 2025, pp. 4–6]**.
- **Désaccord de scope** (pas un vrai désaccord) : vertex clustering vs edge-collapse ne visent pas le même compromis (débit/échelle vs fidélité+topologie). Choix d'objectif, pas contradiction.
- **Débat ouvert** : place des méthodes neuronales (G) face aux méthodes classiques. **[opinion]** : à ce jour, aucune source lue intégralement ne démontre que les méthodes apprises garantissent manifoldness/genre en sortie ; prometteuses en vitesse/généralisation mais non alignées sur C1 sans post-traitement.

---

## 4. Frontières actuelles

### Consensus
- **[fait sourcé, multi-sources]** L'**edge-collapse priorisé par QEM** (Garland–Heckbert 1997) est le socle de la décimation préservant la forme ; ses extensions attributs (Hoppe 1999) sont la voie établie pour C3.
- **[fait sourcé]** La **half-edge** est une structure adaptée et largement utilisée pour l'edge-collapse (Kulkarni & Narayanan 2025).
- **[fait sourcé]** La **distance de Hausdorff** (Klein 1996 ; outil MESH 2002) est la mesure d'erreur géométrique de référence pour *évaluer* une décimation.

### Débats actifs
- Méthodes **apprises** (G) : vitesse/généralisation vs garanties topologiques. Représentants : Potamias 2022, Chen 2023, Chen 2024.
- Simplification **pilotée par l'apparence/rendu** (H) : Hasselgren 2021 (image-space), Liu 2025 (2-complexes, QEM modifiée). Tendance forte côté pipelines temps réel / actifs générés / champs de Gaussiennes. **[à vérifier]** transposition aux contraintes manifold strictes (papers non lus intégralement).
- Décimation **parallèle/GPU** : recherche active (≈2018–2022) sur l'edge-collapse parallèle par QEM (régions indépendantes, synchronisation par mutex/priorité ; ex. ParallelQSlim ; revue QEM MDPI 2023). **[à vérifier]** attributions précises (synthèses web non vérifiées sur le texte primaire).

### Angles morts (justifiés)
- **[opinion]** Peu de travaux récents lus intégralement combinent *simultanément* les trois contraintes (manifold strict + features + UV/normales) avec **garanties d'erreur bornée** : les méthodes à erreur bornée (D) sont anciennes et ne traitent pas nativement les attributs ; les méthodes attributs (A.3) ne donnent pas de borne Hausdorff garantie. Intersection peu explorée.
- **[opinion]** L'évaluation comparative *standardisée* de la préservation d'attributs (métrique d'erreur d'apparence reproductible) reste hétérogène d'un papier à l'autre.

---

## 5. Méthodes retenues

> Contrat d'interface vers l'aval (Partie 2 — Faisabilité). Toutes cohérentes avec les 3 contraintes et exécutables sur structure **half-edge** avec **file de priorité**.

- **QEM par edge-collapse / half-edge collapse (base)** — *Garland & Heckbert 1997*.
  - **Pourquoi retenue** : socle dominant, qualité géométrique éprouvée. **C1** : préservée si l'on restreint aux *edge-collapse* (jamais paires arbitraires) + tests de validité (link condition, anti-flip de normale, anti-fold, vérif non-manifold). **C2** : les quadriques de plan capturent naturellement les plis ; renforçable par quadriques de contraintes de bord/feature (plans virtuels). **C3** : à compléter par l'extension attributs ci-dessous.
  - **Pré-requis** : half-edge ; file de priorité (min-heap) sur le coût de collapse ; quadrique 4×4 par sommet ; garde-fous topologiques avant chaque collapse.

- **QEM étendue aux attributs d'apparence (extension obligatoire pour C3)** — *Hoppe 1999 (« New Quadric Metric… »)*, repère historique *Garland & Heckbert 1998*.
  - **Pourquoi retenue** : couvre directement **C3** (normales, UV, couleurs) et **C2** via la gestion des *discontinuités d'attributs* (creases, frontières de matériau) par représentation en coins (« wedges »). S'intègre dans le même cadre edge-collapse.
  - **Pré-requis** : extension de la quadrique en dimension n>3 (position + attributs) ; structure de coins/wedges au-dessus du half-edge pour porter plusieurs vecteurs d'attributs par sommet ; gestion des seams UV comme features à préserver.

- **Préservation explicite des features par quadriques de contraintes (renfort C2)** — *Garland & Heckbert 1997 (contraintes de bord)*, esprit repris par *FA-QEM, Bhosikar et al. 2026* (récent, à confirmer par lecture intégrale avant adoption de détails).
  - **Pourquoi retenue** : ajoute des quadriques planaires perpendiculaires le long des arêtes vives/bords pour pénaliser leur déplacement → satisfait **C2** sans nuire à **C1**. Compatible edge-collapse/half-edge.
  - **Pré-requis** : détection d'arêtes vives (seuil d'angle dièdre), marquage des bords et des seams UV ; pondération des quadriques de contrainte.

- **(Optionnel) Critère memoryless volume/aire (variante de coût)** — *Lindstrom & Turk 1998*.
  - **Pourquoi mentionnée** : alternative de fonction de coût à faible empreinte mémoire, **C1** préservable (edge-collapse), bonne préservation volume/bords (**C2** partiel). À considérer si la mémoire des quadriques pose problème ; **ne couvre pas C3 nativement**.
  - **Pré-requis** : identiques (half-edge + file de priorité), sans stockage de quadriques.

- **(Évaluation, non décimation) Distance de Hausdorff comme métrique de contrôle qualité** — *Klein 1996 ; outil MESH 2002*.
  - **Pourquoi retenue** : pour *mesurer* et *borner* l'erreur géométrique post-décimation (lien famille D). Non un algorithme de décimation, mais un critère d'arrêt / validation.

> **Explicitement NON retenues** (incompatibles C1) : **vertex clustering** (Rossignac–Borrel 1993) et **out-of-core quadrique de cluster** (Lindstrom 2000) — fusion de composantes / modification du genre. **Décimation non-manifold / 2-complexes** (Liu et al. 2025) — sous entrée manifold elle converge vers la QEM standard, sans apport additionnel sous nos contraintes. **Méthodes neuronales** (G) et **apparence image-space** (Hasselgren 2021) — pas de garantie manifold/topologie démontrée dans les sources lues ; hors périmètre half-edge déterministe.

---

## 6. Limites de cet état de l'art

- **Profondeur de lecture** : seuls **Kulkarni & Narayanan 2025 (arXiv:2512.19959)** ont été lus *intégralement* (texte extrait, 46 pages). Les autres références reposent sur abstract + métadonnées éditeur/DBLP + pages projet auteurs, parfois complétées par fetch d'abstract (Liu 2023, Liu 2025, Bhosikar 2026). Les claims correspondants sont étiquetés et, en cas de doute, marqués **[à vérifier]**. Aucun résumé « de confiance » n'a été produit pour un paper paywallé non lu.
- **Périmètre temporel** : fondateurs 1992–2005 ; récents 2021–2026 (preprints inclus). Recherche active menée pour chaque famille sur la fenêtre ≈2018–2026.
- **Biais de sélection** : sources majoritairement anglophones (SIGGRAPH / Visualization / TOG / CVPR / arXiv) ; littérature non anglophone et brevets non couverts (brevets apparus dans les recherches écartés). Les **outils industriels** (§7) sont *exclus du quota académique* et ne fondent aucune affirmation théorique.
- **Attributions « récent parallèle/GPU »** : certaines attributions précises proviennent de synthèses de recherche web non vérifiées sur le texte primaire → **[à vérifier]** avant citation dans un livrable final.
- **Date de consultation** : **2026-06-24**.
- **[opinion] de l'analyste** : pour la cible (half-edge, manifold strict, features, attributs), le couple **QEM edge-collapse + métrique attributs de Hoppe 1999 + quadriques de contraintes de bord** est le chemin le mieux étayé par la littérature ; les approches récentes (neuronales, image-space) sont à surveiller mais n'offrent pas, dans les sources lues, les garanties topologiques requises.

---

## 7. Outils industriels / bibliothèques (HORS QUOTA — ne fondent aucune affirmation théorique)

Étiquetés *outils*. Référencés pour orientation d'implémentation uniquement.

- **QSlim** — implémentation de référence de Garland–Heckbert (QEM). (outil)
- **CGAL** — module *Triangulated Surface Mesh Simplification* (edge-collapse, half-edge / `Surface_mesh`). (outil)
- **meshoptimizer** (zeux) — `simplify` basé QEM, orienté temps réel/jeux. (outil)
- **libigl** — fonctions de décimation (qslim / edge-collapse) au-dessus de structures half-edge. (outil)
- **MeshLab / VCGlib** — quadric edge collapse decimation, vertex clustering. (outil)
- **Blender** — *Decimate modifier* (collapse / planar / un-subdivide). (outil)
- **Open3D** — `simplify_quadric_decimation`, `simplify_vertex_clustering`. (outil)
- **Simplygon** (Microsoft) — solution industrielle LOD / décimation. (outil)
- **Draco** (Google) — compression de maillages (≠ décimation géométrique stricto sensu). (outil)
- **VTK** — `vtkDecimatePro` (basé Schroeder 1992). (outil)
- **nvdiffmodeling** (NVIDIA) — implémentation de Hasselgren et al. 2021 (apparence / rendu différentiable). (outil)

---

## 8. Sources

### Papers académiques — fondateurs

1. **Garland, M., Heckbert, P. (1997).** *Surface Simplification Using Quadric Error Metrics.* SIGGRAPH '97, pp. 209–216. https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf — page projet : https://mgarland.org/research/quadrics.html  *(abstract/métadonnées)*
2. **Garland, M., Heckbert, P. (1998).** *Simplifying Surfaces with Color and Texture Using Quadric Error Metrics.* Visualization '98, pp. 263–269. https://www.semanticscholar.org/paper/68164aef267b6deda9e375aad0e351bdf511b11b  *(abstract/métadonnées)*
3. **Hoppe, H. (1999).** *New Quadric Metric for Simplifying Meshes with Appearance Attributes.* IEEE Visualization '99, pp. 59–66. https://hhoppe.com/proj/newqem/ — DBLP : https://dblp.org/rec/conf/visualization/Hoppe99.html — ACM : https://dl.acm.org/doi/10.5555/832273.834119  *(abstract/métadonnées)*
4. **Hoppe, H. (1996).** *Progressive Meshes.* SIGGRAPH '96, pp. 99–108. https://hhoppe.com/pm.pdf — ACM : https://dl.acm.org/doi/10.1145/237170.237216  *(abstract/métadonnées)*
5. **Schroeder, W., Zarge, J., Lorensen, W. (1992).** *Decimation of Triangle Meshes.* SIGGRAPH '92, Computer Graphics 26(2). https://dl.acm.org/doi/10.1145/133994.134010  *(abstract/métadonnées)*
6. **Lindstrom, P., Turk, G. (1998).** *Fast and Memory Efficient Polygonal Simplification.* IEEE Visualization '98, pp. 279–286. http://mesh.brown.edu/DGP/pdfs/Lindstrom-vis98.pdf — ACM : https://dl.acm.org/doi/10.5555/288216.288288  *(abstract/métadonnées)*
7. **Lindstrom, P., Turk, G. (1999).** *Evaluation of Memoryless Simplification.* IEEE TVCG. https://faculty.cc.gatech.edu/~turk/my_papers/memless_tvcg99.pdf — https://ieeexplore.ieee.org/document/773803/  *(abstract/métadonnées)*
8. **Rossignac, J., Borrel, P. (1993).** *Multi-resolution 3D Approximations for Rendering Complex Scenes.* In *Modeling in Computer Graphics*, Springer, pp. 455–465. DOI 10.1007/978-3-642-78114-8_29. https://link.springer.com/chapter/10.1007/978-3-642-78114-8_29  *(abstract/métadonnées)* — **NON retenable (viole C1), contraste seulement.**
9. **Lindstrom, P. (2000).** *Out-of-Core Simplification of Large Polygonal Models.* SIGGRAPH '00, pp. 259–262. DOI 10.1145/344779.344912. https://dl.acm.org/doi/10.1145/344779.344912  *(abstract/métadonnées)* — **NON retenable (viole C1).**
10. **Cohen, J., Varshney, A., Manocha, D., Turk, G., et al. (1996).** *Simplification Envelopes.* SIGGRAPH '96. http://gamma.cs.unc.edu/SIMPLE/  *(abstract/métadonnées)*
11. **Klein, R., Liebich, G., Straßer, W. (1996).** *Mesh Reduction with Error Control.* IEEE Visualization '96. https://cg.cs.uni-bonn.de/backend/v1/files/publications/klein-1996-mesh.pdf — https://ieeexplore.ieee.org/document/568124/  *(abstract/métadonnées)*

### Papers académiques — récents (≈2018–2026)

12. **Potamias, R., Ploumpis, S., Zafeiriou, S. (2022).** *Neural Mesh Simplification.* CVPR 2022. https://ieeexplore.ieee.org/document/9878835/  *(abstract/métadonnées)* — **[à vérifier]** garanties topologiques.
13. **Chen, Z., Kim, V., Aigerman, N., Jacobson, A. (2023).** *Neural Progressive Meshes.* ACM SIGGRAPH 2023. *(abstract/métadonnées + cité par réf. 18, p. 4)*
14. **Liu, H.-T. D., Gillespie, M., Chislett, B., Sharp, N., Jacobson, A., Crane, K. (2023).** *Surface Simplification using Intrinsic Error Metrics.* ACM TOG 42(4), SIGGRAPH 2023. arXiv:2305.06410. https://arxiv.org/abs/2305.06410  *(abstract/métadonnées via fetch)*
15. **Hasselgren, J., Munkberg, J., Lehtinen, J., Aittala, M., Laine, S. (2021).** *Appearance-Driven Automatic 3D Model Simplification.* EGSR 2021. arXiv:2104.03989. https://arxiv.org/abs/2104.03989 — https://research.nvidia.com/labs/rtr/publication/hasselgren2021diffmodeling/  *(abstract/métadonnées)*
16. **Liu, H.-T. D., Zhang, X., Yuksel, C. (2025).** *Simplifying Textured Triangle Meshes in the Wild.* SIGGRAPH Asia 2025 ; ACM TOG, DOI 10.1145/3763277. arXiv:2409.15458. https://arxiv.org/abs/2409.15458  *(abstract/métadonnées via fetch)*
17. **Chen, et al. (2024).** *A progressive mesh simplification algorithm based on neural implicit representation.* Computational Intelligence (Wiley). https://onlinelibrary.wiley.com/doi/abs/10.1111/coin.12605  *(abstract/métadonnées)* — **[à vérifier]**
18. **Kulkarni, P., Narayanan, A. S. (2025).** *A Comprehensive Guide to Mesh Simplification using Edge Collapse.* arXiv:2512.19959 (23 déc. 2025). https://arxiv.org/abs/2512.19959  ***(LU INTÉGRALEMENT, 46 pages)*** — guide de synthèse (non revue à comité affiché).
19. **Bhosikar, K., Savalia, P., Tiwari, L., Bhowmick, B. (2026).** *Fast and Robust Mesh Simplification for Generated and Real-World 3D Assets (FA-QEM).* arXiv:2605.14029 (13 mai 2026). https://arxiv.org/abs/2605.14029  *(abstract/métadonnées via fetch)* — **[à vérifier]** topologie.
20. **Garland, M., Zhou, Y. (2005).** *Quadric-Based Simplification in Any Dimension.* ACM TOG 24(2). DOI 10.1145/1061347.1061350. https://dl.acm.org/doi/10.1145/1061347.1061350  *(abstract/métadonnées)* — **[à vérifier]**

### Méta-références / surveys

21. **Cignoni, P., Montani, C., Scopigno, R. (1998).** *A comparison of mesh simplification algorithms.* Computers & Graphics. *(cité comme base taxonomique par réf. 18 ; non lu directement → « d'après Kulkarni & Narayanan 2025 »)*
22. **Revue QEM / bibliométrie (2023).** *Review of Three-Dimensional Model Simplification Algorithms Based on Quadric Error Metrics and Bibliometric Analysis by Knowledge Map.* MDPI Mathematics 11(23):4815. https://www.mdpi.com/2227-7390/11/23/4815  *(abstract/métadonnées ; bibliométrie de 128 études 1998–2022)*
23. **Aspert, N., Santa-Cruz, D., Ebrahimi, T. (2002).** *MESH: Measuring Errors between Surfaces Using the Hausdorff Distance.* ICME 2002. http://dsanta.users.ch/research/mesh.pdf  *(abstract/métadonnées — outil de mesure d'erreur)*

### Outils industriels / bibliothèques (HORS QUOTA — voir §7)

QSlim · CGAL (*Surface Mesh Simplification*) · meshoptimizer · libigl · MeshLab/VCGlib · Blender (*Decimate*) · Open3D · Simplygon · Draco · VTK (`vtkDecimatePro`) · nvdiffmodeling. *Référencés pour implémentation ; ne fondent aucune affirmation théorique.*

---

<!-- ================================================================= -->
<!-- Partie 2 - Faisabilite (ajoutee a la suite de la Partie 1).       -->
<!-- ================================================================= -->

# Partie 2 — Faisabilité

> Analyse de faisabilité sur le code existant du module `src/cgmesh` (C++). Périmètre confirmé : implémentation de `Mesh_half_edge::simplify()` (stub `simplification.cpp:3`). **Aucun code d'implémentation produit ici** — uniquement structure, briques réutilisables, manques, plan incrémental et trade-offs.
>
> **Discipline d'étiquetage** (reprise de la Règle d'Or de la Partie 1) :
> - **[VÉRIFIÉ]** = constaté par lecture du code, avec `chemin:ligne`.
> - **[HYPOTHÈSE→EXÉC]** = supposition de comportement/bug à confirmer **par exécution** avant toute correction.
> - **[OPINION]** = jugement d'architecte (trade-off).
> - Aucune affirmation de bug n'est posée comme fait sans reproduction par exécution.

---

## 0. Cadrage et confirmation

- **Module** : `C:/home/perso/cg/src/cgmesh`. **Langage dominant** : C++ (les skills C++ `cpp-coding-standards` et `cmake-patterns` s'appliquent pleinement). **[VÉRIFIÉ]** `CMakeLists.txt` fait un `file(GLOB *.cpp)` (`CMakeLists.txt:5,16`) : `simplification.cpp` est donc **déjà compilé** dans la lib `cgmesh`, sans modification du build à prévoir pour le MVP.
- **Entrée API** : `void Mesh_half_edge::simplify(void)` — `mesh_half_edge.h:60`, corps vide `simplification.cpp:3-6`. **[OPINION]** signature à enrichir (cf. section 3) d'un budget cible (ratio ou nb de faces) et d'options, via un struct d'options à valeurs par défaut pour ne pas casser l'existant.
- **Fichiers réellement lus** : `mesh_half_edge.h/.cpp`, `half_edge.h/.cpp`, `mesh.h`, `mesh.cpp` (extraits : GetTriangles 407-417, GetTopologicIssues 1322-1386, MergeVertices 1543-1662), `simplification.cpp`, `normals.cpp`, `orientation_edges.h`, `orientation_edges2.h`, `topology.h`, `regions_vertices_ridges_valleys_belyaev.cpp` (début), `cgmath/quadric.h/.cpp`, `CMakeLists.txt`.
- **NON examiné en détail** (déclaré) : `bvh.*`, `octree.*` (repérés ; pertinents pour Hausdorff seulement), `thickness.*`, l'intégralité de `mesh.cpp` (~49 ko ; 3 segments lus), les chemins d'I/O (`mesh_io*`), et le détail des évaluateurs de courbure (`DiffParamEvaluator*`) au-delà du constat que Belyaev s'appuie sur des tenseurs de courbure.

---

## 1. Cartographie de l'existant par méthode retenue

### Brique transverse n.1 — Métrique QEM 3D : DÉJÀ PRÉSENTE (réutilisable telle quelle)

**[VÉRIFIÉ]** `src/cgmath/quadric.{h,cpp}` fournit une boîte à outils QEM complète et autonome, encodée comme `typedef double quadric_t[10]` (matrice 4x4 symétrique compacte) :
- `plane_init(plane, v1, v2, v3)` — plan d'un triangle, normale normalisée (`quadric.cpp:9-18`).
- `plane_quadric(plane, q)` — produit externe plan x plan vers quadrique (`quadric.cpp:20-32`).
- `quadric_zero / quadric_copy / quadric_add / quadric_scale` — accumulation des quadriques par sommet (`quadric.cpp:34-62`).
- `quadric_eval(q, v)` — coût v^T A v + 2 b^T v + c (`quadric.cpp:124-131`).
- `quadric_minimize(q, vnew, err)` — position optimale par inversion 3x3, garde-fou `fabs(fdet)<=EPSILON` et `isnan` (`quadric.cpp:76-122`).
- `quadric_minimize_edge` — optimum contraint au segment [v0,v1], a clampé dans [0,1] (`quadric.cpp:133-184`).
- `quadric_minimize2` — fallback en cascade : optimum libre, puis optimum sur arête, puis meilleur des trois points v0, v1, milieu (`quadric.cpp:186-234`). C'est la robustesse attendue d'un QEM industriel (Garland-Heckbert, section 5 de la Partie 1).

**Réutilisable** : tout le cœur QEM 3D position de la méthode 1 (G&H 1997). **[VÉRIFIÉ]** non câblé à la décimation aujourd'hui : seul `surface_implicit_tandem.cpp` consomme `quadric_` (grep sur tout `src`). Aucun lien CMake à ajouter : `quadric.cpp` est dans `cgmath`, déjà liée transitivement (cgmesh vers cgimg vers cgmath ; include `../cgmath/cgmath.h` présent dans `mesh.h:10`).

**Manque (méthode 1)** : (a) calcul/stockage d'une quadrique par sommet (accumulation des `plane_quadric` des faces incidentes) ; (b) file de priorité des arêtes par coût ; (c) garde-fous topologiques par collapse (cf. brique n.3) ; (d) compaction finale du maillage.

### Brique transverse n.2 — Structure half-edge : edge-collapse partiellement outillé

La structure (`half_edge.h:11-78`) est une SoA indexée par entiers (`m_pair`, `m_he_next`, `m_v_begin/end`, `m_face`, `m_valid`), avec maps `m_map_edges` (paire de sommets vers edge), `map_edges_vertex`, `map_edges_face`. Composition `Mesh_half_edge` = géométrie `Mesh*` + topologie `Che_mesh` (`mesh_half_edge.h:38,79`).

**[VÉRIFIÉ] Accès incident disponibles** :
- Itération 1-anneau autour d'un sommet : `Citerator_half_edges_vertex` (`half_edge.h:86-122`) et le pattern de parcours répété partout (ex. `mesh_half_edge.cpp:321-324`).
- Recherche d'arête par couple de sommets : `Che_mesh::get_edge` (`half_edge.cpp:294-298`) et `Mesh_half_edge::get_edge` (`mesh_half_edge.cpp:226-250`).
- Longueur d'arête, voisins, cotangentes : `edge_length`, `get_n_neighbours`, `cotangent_weight_formula` (`mesh_half_edge.cpp:346-460`).

**[VÉRIFIÉ] Opération de collapse DÉJÀ écrite, en deux variantes** :
- `Che_mesh::edge_contract(ei)` (`half_edge.cpp:466-512`) — collapse topologique brut : recâble les paires des arêtes survivantes, réaffecte iv2 vers iv1 sur e1p/e4p, invalide les 6 demi-arêtes des 2 faces, met `m_edges_vertex[iv2]=-1`, `m_edges_face[f1/f2]=-1`. Ne met PAS à jour `m_map_edges` ni les sommets iv3/iv4 autour. **[HYPOTHÈSE→EXÉC]** cette variante laisse `m_map_edges` désynchronisée (clés obsolètes) ; à confirmer par exécution ; **[OPINION]** ne pas la retenir pour la décimation.
- `Che_mesh::edge_contract2(ei)` + garde `is_edge_contract2_valid(ei)` (`half_edge.cpp:514-641`) — variante avec link condition : `is_edge_contract2_valid` parcourt les 1-anneaux de iv1 et iv2 et refuse le collapse si un voisin commun autre que iv3/iv4 existe (`half_edge.cpp:533-552`) — c'est la link condition qui garantit la préservation manifold/topologie (C1, méthode 1). `edge_contract2` remappe iv2 vers iv1 autour du sommet ET met à jour `m_map_edges`, `map_edges_vertex` (iv1, iv3, iv4) et `map_edges_face` (`half_edge.cpp:582-640`).

**[VÉRIFIÉ] Couche géométrique du collapse** : `Mesh_half_edge::edge_contract(ei)` (`mesh_half_edge.cpp:297-337`) déplace iv1 au milieu de l'arête (point fixe, pas QEM), réécrit les indices de sommet iv2 vers iv1 dans toutes les `Face` incidentes, puis fait `delete` des deux `Face*` des triangles effondrés et met `m_pFaces[f1/f2]=nullptr`, puis appelle `Che_mesh::edge_contract` (la variante brute, pas `edge_contract2`).

**Manques / limites importantes [VÉRIFIÉ]** :
1. **Incohérence de variante** : la couche `Mesh_half_edge::edge_contract` appelle `Che_mesh::edge_contract` (brute) et non `edge_contract2` (link condition + maj maps). **[OPINION]** la décimation doit s'appuyer sur `edge_contract2` (ou une fusion des deux) ; la couche géométrique actuelle est à reprendre.
2. **m_pFaces[f]=nullptr crée des trous** : `Mesh::m_pFaces` devient un tableau à trous (`mesh_half_edge.cpp:331,333`). **[HYPOTHÈSE→EXÉC]** beaucoup de boucles for sur m_nFaces déréférencent `m_pFaces[i]` sans test nul (ex. GetTriangles `mesh.cpp:410-414`, EvalOnFaces `normals.cpp:164-168`) : crash possible si appelées après collapse sans compaction. À reproduire par exécution. Conséquence de conception : prévoir une passe de compaction finale (suppression des faces/sommets invalides + réindexation).
3. **Position de collapse = milieu** (`mesh_half_edge.cpp:306-308`), pas l'optimum QEM. Pour la méthode 1 il faut brancher `quadric_minimize2`.
4. **m_edges ne se compacte jamais** : les demi-arêtes invalidées restent dans le vector (m_valid=0). Acceptable pendant la boucle (skip sur m_valid), à compacter à la fin.
5. **Itérateur fragile sur les bords** : `Citerator_half_edges_vertex::next()` (`half_edge.h:107-113`) déréférence `m_pair` sans tester -1. Les boucles manuelles testent e_walk>=0, pas l'itérateur. **[HYPOTHÈSE→EXÉC]** itération autour d'un sommet de bord via cet itérateur peut indexer `edge(-1)` ; à reproduire.

### Méthode 2 — QEM étendue aux attributs (Hoppe 1999) : modèle d'attributs hétérogène, c'est le point dur

**[VÉRIFIÉ] Stockage des attributs dans le modèle actuel** :
- **Positions** : `Mesh::m_pVertices` (3 floats/sommet) — `mesh.h:402`. Par sommet.
- **Normales** : `Mesh::m_pVertexNormals` (3 floats/sommet) — `mesh.h:403`. Par sommet (recalculables via `Normals::EvalOnVertices`, `normals.cpp:16`).
- **Couleurs** : `Mesh::m_pVertexColors` (3 floats/sommet) — `mesh.h:404`. Par sommet.
- **UV / texcoords** : par coin de face, pas par sommet. `Face::m_pTextureCoordinates` (2 floats par coin) et `Face::m_pTextureCoordinatesIndices` (`mesh.h:99-100`, accès `SetTexCoord` 59-74). Il existe aussi `Mesh::m_pTextureCoordinates` global (`mesh.h:408`) mais `MergeVertices` ne traite l'UV parallèle au sommet que si `size()==2*nVertices` (`mesh.cpp:1564-1565`) : deux conventions coexistent selon la provenance du fichier.

**Conséquence de faisabilité (méthode 2 / C3)** :
- Normales et couleurs étant par sommet, une quadrique nD position+normale+couleur à la Hoppe est implémentable sans structure de wedges tant que les attributs sont continus sur la surface.
- Mais les UV par-coin et les discontinuités d'attributs (seams UV, frontières de matériau via `Face::m_iMaterialId` `mesh.h:101`, plis de normale) exigent la notion de wedge (coin) de Hoppe : un sommet topologique peut porter plusieurs vecteurs d'attributs selon le secteur de faces. **[VÉRIFIÉ]** cette structure de wedges n'existe pas ; `Che_edge` a un `void *m_data` (`half_edge.h:24`) et `int m_flag` libres, mais aucun conteneur de wedge.
- **[OPINION]** Hoppe 1999 pur (quadrique nD unifiée) est la cible la plus fidèle mais coûteuse (quadrique jusqu'à 8 dimensions : 3 position + 3 normale + 2 UV) et impose la couche wedge. Alternative pragmatique (à arbitrer en section 3) : garder la quadrique 3D géométrique pour position+coût, et réinterpoler les attributs au point fusionné (barycentrique le long de l'arête) avec détection de seam traitée comme une feature (méthode 3) qui bloque le collapse à travers la couture. Couvre la préservation C3 sans la quadrique nD. Trade-off : moins optimal sur la fidélité d'attribut, beaucoup plus simple et robuste.

**Manques (méthode 2)** : (a) couche wedge OU stratégie d'interpolation+blocage de seam ; (b) extension `quadric_t` en nD (le [10] est figé en 3D) si Hoppe pur ; (c) normalisation des UV par-coin vers une représentation exploitable pendant la décimation ; (d) recalcul/projection des attributs après collapse.

### Méthode 3 — Préservation des features par quadriques de contraintes de bord : détection à écrire

**[VÉRIFIÉ] Détection de bord disponible** :
- `Che_mesh::is_border(vi)` (`half_edge.cpp:249-270`) et `Mesh_half_edge::is_border` + `check_border` (`mesh_half_edge.cpp:64-117`, vecteur `m_border`).
- `Mesh::GetTopologicIssues` (`mesh.cpp:1322-1386`) renvoie les bords (arête vue 1 fois) et les arêtes non-manifold (vue 3 fois ou plus) — utile pour marquer les contraintes de bord et refuser les collapses non-manifold (C1).
- Un bord se détecte aussi directement par `Che_edge::m_pair < 0` (`half_edge.cpp:415,468`).

**[VÉRIFIÉ] Détection d'arêtes vives (angle dièdre) : ABSENTE.**
- `orientation_edges*.{h,cpp}` ne détectent PAS les creases : ce sont des estimateurs d'orientation globale par accumulateur de Hough (`orientation_edges.h:12-33`, `orientation_edges2.h`), sans rapport avec un seuil d'angle dièdre par arête.
- `regions_vertices_ridges_valleys_belyaev.cpp` détecte ridges/ravines mais via les tenseurs de courbure (`DiffParamEvaluator*`), pas un seuil d'angle dièdre, et la partie cœur est commentée (belyaev.cpp:36-45). Non réutilisable directement comme détecteur de crease léger.
- `Mesh::m_pFaceNormals` (`mesh.h:406`) et `EvalOnFaces` (`normals.cpp:157-186`) fournissent les normales de face : brique suffisante pour calculer l'angle dièdre sur chaque arête interne via le `m_pair` (faces adjacentes = `edge(e).m_face` et `edge(e.m_pair).m_face`).

**Manque (méthode 3)** : (a) un détecteur de crease (angle entre normales de faces supérieur à un seuil = feature) ; (b) marquage des seams UV / frontières de matériau comme features ; (c) ajout, pour les sommets touchant une feature/bord, d'une quadrique de plan virtuel perpendiculaire pondérée (G&H boundary constraint, section 5 Partie 1), via `plane_init`/`plane_quadric`.

### Méthode 4 (optionnelle) — Memoryless Lindstrom-Turk : rien de spécifique présent

**[VÉRIFIÉ]** aucune brique volume/aire dédiée au coût LT. `Mesh::GetFaceArea/GetArea/GetAreas` (`mesh.h:319-322`) donnent les aires ; pas de coût LT. **[OPINION]** option de repli si l'empreinte mémoire des quadriques pose problème (peu probable aux tailles visées) ; faible priorité. Ne couvre pas C3.

### Méthode 5 (évaluation) — Distance de Hausdorff : briques spatiales présentes, métrique à écrire

**[VÉRIFIÉ]** `bvh.{h,cpp}` et `octree.{h,cpp}` existent (requêtes spatiales / plus-proche-triangle). `Mesh::GetIntersectionWithRayInOctree` (`mesh.cpp:1407+`) montre l'usage octree. NON lus en détail (déclaré). Manque : échantillonnage + plus-proche-point sur maillage cible, puis max des distances (one-sided + symétrique). **[OPINION]** réservé à l'évaluation/critère d'arrêt qualité ; pas sur le chemin critique du MVP.

### Brique transverse n.3 — File de priorité : ABSENTE dans cgmesh

**[VÉRIFIÉ]** aucune file de priorité réutilisable dans `cgmesh`. Seul `cgimg/image_geodesic.cpp:96` utilise un `std::priority_queue` local. Manque : un min-heap d'arêtes par coût supportant l'invalidation paresseuse (version stamp par arête/sommet, car `std::priority_queue` n'offre pas de decrease-key). **[OPINION]** patron standard : `std::priority_queue` + numéro de version ; on dépile, on revérifie la validité (arête encore m_valid, version à jour), sinon on jette. Simple et éprouvé.

---

## 2. Synthèse vérifié / hypothèse / manque

| Élément | Statut | Référence |
|---|---|---|
| Cœur QEM 3D (plane vers quadric, add, minimize, fallback) | [VÉRIFIÉ] présent, réutilisable | `cgmath/quadric.cpp:9-234` |
| Half-edge : accès incident, 1-anneau, get_edge | [VÉRIFIÉ] présent | `half_edge.h:86-122`, `mesh_half_edge.cpp:226-250` |
| Collapse avec link condition (préserve manifold) | [VÉRIFIÉ] présent (`edge_contract2`) | `half_edge.cpp:514-641` |
| Collapse géométrique (positions + faces) | [VÉRIFIÉ] présent mais à reprendre (milieu, trous m_pFaces, variante brute) | `mesh_half_edge.cpp:297-337` |
| Détection de bord / non-manifold | [VÉRIFIÉ] présent | `half_edge.cpp:249-270`, `mesh.cpp:1322-1386` |
| Normales de face (pour angle dièdre) | [VÉRIFIÉ] présent | `normals.cpp:157-186`, `mesh.h:406` |
| Détection d'arêtes vives par seuil dièdre | MANQUE | — |
| Couche wedges (Hoppe attributs/UV par-coin) | MANQUE | UV par-coin `mesh.h:99-100` |
| Quadrique nD (position+attributs) | MANQUE (`quadric_t` figé en 3D) | `quadric.h:8` |
| File de priorité avec invalidation | MANQUE dans cgmesh | modèle `cgimg/image_geodesic.cpp:96` |
| Compaction post-décimation (faces/sommets/edges) | MANQUE | — |
| Anti-flip de normale par collapse | MANQUE | — |
| Hausdorff (échantillonnage + plus-proche-point) | MANQUE (BVH/octree présents) | `bvh.*`, `octree.*` (non lus en détail) |

**Hypothèses à reproduire par exécution AVANT toute correction** (non traitées comme bugs établis) :
- **[HYPOTHÈSE→EXÉC]** `m_pFaces[f]=nullptr` (`mesh_half_edge.cpp:331,333`) provoque un déréférencement nul dans les boucles for sur m_nFaces ultérieures (GetTriangles, EvalOnFaces, rendu).
- **[HYPOTHÈSE→EXÉC]** `Che_mesh::edge_contract` (variante brute) laisse `m_map_edges` désynchronisée.
- **[HYPOTHÈSE→EXÉC]** `Citerator_half_edges_vertex::next()` déréférence `edge(-1)` sur un sommet de bord (`half_edge.h:107-113`).
- **[HYPOTHÈSE→EXÉC]** `GetCheMesh()` reconstruit le half-edge à partir de `GetTriangles()` qui ne lit que 3 sommets/face (`mesh.cpp:410-414`) : sur un mesh non purement triangulaire, la topologie half-edge est fausse ; exiger `Triangulate()` (`mesh.h:212`) en pré-condition de `simplify()`.

---

## 3. Plan d'implémentation incrémental (MVP vers cible) et trade-offs

> Chaque étape est livrable et testable indépendamment. Pré-condition globale : maillage triangulé et manifold-friendly. **[OPINION]** appeler `Triangulate()` puis `MergeVertices()` en entrée de `simplify()` (idempotent, soude les doublons en préservant seams UV/couleurs — `mesh.cpp:1554+`), et invalider le cache half-edge via `create_half_edge()` (`mesh_half_edge.cpp:161-166`).

### Étape 0 — Reproduction par exécution des hypothèses (préalable, pas du code de feature)
- Écrire/lancer un petit harnais : charger un mesh, forcer `Triangulate()`, appeler `GetCheMesh()`, exécuter un `edge_contract2` puis un parcours/rendu, pour confirmer ou infirmer les 4 hypothèses ci-dessus. Sortie : liste des bugs réellement reproduits, routée vers implémentation.
- Trade-off : coût amont, mais évite de corriger du code sain et fixe le contrat réel de `edge_contract2` vs `edge_contract`.

### Étape 1 — MVP : QEM edge-collapse manifold, sans attributs (méthode 1)
- Briques à créer : (1) quadrique par sommet = somme des `plane_quadric` des faces incidentes (réutilise `quadric.cpp`) ; (2) file de priorité d'arêtes avec invalidation paresseuse ; (3) coût + position via `quadric_minimize2` ; (4) boucle de collapse appuyée sur `is_edge_contract2_valid` + `edge_contract2` ; (5) passe de compaction finale (réindexer sommets/faces valides, reconstruire le `Mesh`, recalculer normales via `ComputeNormals`).
- Points de contact existant : `quadric.cpp` (cœur), `half_edge.cpp:514-641` (collapse+link), `mesh_half_edge.cpp:297-337` (couche géométrique à réécrire : position = optimum QEM ; pas de m_pFaces=nullptr à la volée mais marquage + compaction).
- Garde-fous : link condition (déjà dans `is_edge_contract2_valid`) ; anti-flip de normale (à créer : refuser le collapse si une face incidente inverse sa normale, en comparant `m_pFaceNormals` avant/après la position candidate) ; refus si arête non-manifold (`GetTopologicIssues`).
- Critère d'arrêt : budget (ratio/nb faces) en paramètre.
- Trade-offs : (a) position optimale QEM (meilleure fidélité) vs sur l'arête / endpoint (`quadric_minimize_edge`, plus stable, garde les sommets d'origine, utile pour ne pas dériver les features) ; `quadric_minimize2` couvre déjà la cascade. (b) Marquage-puis-compaction vs édition immédiate : le marquage évite la fragilité des trous m_pFaces mais demande une passe finale.

### Étape 2 — Préservation des bords et features (méthode 3, renfort C2)
- Briques à créer : (1) détecteur de crease par angle dièdre sur `m_pFaceNormals` + `m_pair` (seuil paramétrable) ; (2) marquage des arêtes de bord (m_pair<0 / `GetTopologicIssues`), seams UV et frontières de matériau (`Face::m_iMaterialId`) comme features ; (3) ajout aux quadriques des sommets de feature de quadriques de plan virtuel perpendiculaire pondérées (réutilise `plane_init`/`plane_quadric`) ; (4) option : interdire purement le collapse d'une arête feature (mode conservateur).
- Points de contact : `normals.cpp` (normales de face), `half_edge.cpp` (m_pair), `mesh.cpp:1322-1386`.
- Trade-offs : pénalité quadrique (souple, déplace peu les features) vs interdiction stricte (sûr mais peut bloquer la réduction le long de longues arêtes vives). **[OPINION]** offrir les deux via le poids de contrainte.

### Étape 3 — Préservation des attributs d'apparence (méthode 2, C3)
- Voie A (pragmatique, recommandée en premier) : quadrique 3D pour position/coût ; interpolation barycentrique des normales/couleurs par-sommet au point fusionné ; UV par-coin reprojetées par face ; seams UV / frontières de matériau traitées comme features (étape 2) qui bloquent le collapse transverse. Couvre la préservation C3 sans quadrique nD ni wedges.
  - Trade-off : simple, robuste, réutilise tout l'existant ; fidélité d'attribut sous-optimale là où l'arête traverse un fort gradient d'attribut.
- Voie B (cible Hoppe 1999 fidèle) : couche wedges au-dessus du half-edge (un sommet topologique vers plusieurs vecteurs d'attributs selon secteur) + quadrique nD (nouveau type ; `quadric_t[10]` insuffisant) intégrant position+normale+UV+couleur.
  - Trade-off : meilleure fidélité (recommandation [OPINION] de la Partie 1), mais coût mémoire (quadrique jusqu'à 8x8), complexité de la couche wedge, et réécriture de la mécanique de collapse pour propager les wedges. **[OPINION]** ne pas l'engager avant que la Voie A soit en place et mesurée (Hausdorff + inspection visuelle).
- Points de contact : `mesh.h:99-100,403,404,408` (attributs), `MergeVertices` (logique seam déjà présente, `mesh.cpp:1642-1654`, réutilisable comme référence de gating seam).

### Étape 4 — Critère d'arrêt / qualité par Hausdorff (méthode 5)
- Briques à créer : échantillonnage du maillage + requête plus-proche-point via `bvh`/`octree` (à lire en détail à ce stade), distance one-sided puis symétrique ; arrêt quand l'erreur dépasse un seuil relatif à la diagonale bbox (`Mesh::bbox_diagonal_length`).
- Trade-off : qualité contrôlée vs surcoût de calcul ; n'activer qu'en mode erreur bornée.

### Étape 5 (optionnelle) — Coût memoryless Lindstrom-Turk (méthode 4)
- Fonction de coût alternative sans stockage de quadriques, branchable dans la même boucle/heap. **[OPINION]** faible priorité ; uniquement si la mémoire des quadriques devient un problème mesuré. Ne couvre pas C3.

### Évolution de l'API
- **[OPINION]** Étendre `simplify()` (`mesh_half_edge.h:60`) sans casser l'existant via une struct d'options à valeurs par défaut : budget cible, seuil d'angle de crease, préserver bords/seams/matériaux (bool), mode position (optimum/segment/endpoints), erreur de Hausdorff max optionnelle. Conserver `void simplify()` comme surcharge appelant les défauts.

---

## 4. Conclusion de faisabilité

**Faisable, avec un socle déjà bien avancé.** La métrique QEM 3D (`cgmath/quadric.cpp`) et un edge-collapse avec link condition (`edge_contract2`) sont déjà présents : le MVP (méthode 1, C1) est essentiellement un assemblage (quadriques par sommet + heap + boucle + compaction + anti-flip), pas une écriture ab initio. Les renforts features (méthode 3) reposent sur des briques présentes (normales de face, détection de bord/non-manifold) plus un détecteur de crease simple à écrire. Le point dur réel est C3 / Hoppe 1999, à cause du modèle d'attributs hétérogène (normales/couleurs par-sommet, UV par-coin) et de l'absence de wedges ; d'où la stratégie incrémentale Voie A (blocage de seam) avant Voie B (wedges + quadrique nD).

**Risques principaux** : (1) cohérence d'état du `Mesh`/half-edge après collapse (trous m_pFaces, maps désynchronisées), à verrouiller par exécution avant tout codage ; (2) coût/complexité de la couche wedge si Voie B engagée ; (3) robustesse numérique des collapses dégénérés (atténuée par `quadric_minimize2`).

---

## 5. Note de handoff

- **Frontière respectée** : aucun code d'implémentation produit ; ce document est un livrable de conception (cartographie + plan + trade-offs).
- **Vers l'implémentation** : commencer par l'Étape 0 (reproduction par exécution des 4 hypothèses [HYPOTHÈSE→EXÉC]) AVANT de corriger quoi que ce soit dans `edge_contract*` ou la couche géométrique. Un bug non reproduit par exécution reste une hypothèse, pas une tâche de correction.
- **Vers la revue de code** : invariants à vérifier en revue : (a) maillage manifold préservé (link condition à chaque collapse), (b) cohérence `m_map_edges`/`map_edges_*` après chaque opération, (c) aucune face/sommet invalide laissé au rendu (compaction), (d) attributs : aucun collapse ne traverse un seam/feature en Voie A.

---

# Partie 3 — État d'implémentation

> Journal d'implémentation (mise à jour au fil de l'eau). Ne remplace pas la conception ci-dessus.

## MVP (Étape 1) — IMPLÉMENTÉ et validé par exécution — 2026-06-24

- **Code** : `simplification.cpp` implémente `Mesh_half_edge::simplify(float target_ratio = 0.5f)` (signature étendue dans `mesh_half_edge.h:60`, non cassante via argument par défaut).
- **Architecture retenue** : décimation sur la topologie `Che_mesh` + tableau de quadriques/positions par sommet, puis **reconstruction d'un `Mesh` compacté** depuis la topologie survivante. Le chemin géométrique `Mesh_half_edge::edge_contract` (milieu + `m_pFaces=nullptr`) n'est **pas** utilisé → l'hypothèse des trous `m_pFaces` est **contournée par conception**, pas corrigée.
- **Briques** : quadrique/sommet = somme des `plane_quadric` des faces incidentes (`cgmath/quadric.cpp`) ; file de priorité `std::priority_queue` avec **invalidation paresseuse** (versions par sommet) ; coût/position via `quadric_minimize2` ; collapse via `Che_mesh::edge_contract2`.
- **Garde-fous** : link condition **réécrite localement** (`link_condition_ok` : intersection des 1-anneaux == exactement les 2 apex) car `is_edge_contract2_valid` en l'état n'inspecte qu'un voisin (hypothèse confirmée par lecture) ; **anti-flip** de normale ; **bords gelés** (`is_border`) pour le MVP ; caps anti-boucle sur tous les parcours d'anneau.
- **Validation par exécution** (test `test/tu_cgmesh_simplification.cpp`, 3 cas, tous PASS) : `rabbit.obj` 902 f / 453 v → **0.5** : 450 f / 227 v ; **0.1** : 90 f / 47 v ; **1.0** : no-op (902 f). Résultats vérifiés : maillage triangulé, **sans face nulle ni dégénérée**, indices valides, half-edge reconstructible. Aucune régression sur `TEST_cgmesh_he.*`.
- **Limites assumées du MVP** : pas d'attributs d'apparence (Étape 3), pas de préservation de features par quadriques de contraintes (Étape 2 ; les bords sont seulement gelés), poids de quadrique non pondéré par l'aire, position optimale pouvant s'écarter sur géométries plates (atténué par la cascade `quadric_minimize2`).

## Étape 2 — Préservation des features — IMPLÉMENTÉ et validé par exécution — 2026-06-24

- **API** : `simplify(float target_ratio = 0.5f, bool preserve_features = true, float feature_angle_deg = 45.0f)`. La préservation des features est donc un **paramètre booléen** de l'algorithme (avec seuil d'angle de crease associé). Bordures **toujours** préservées (la primitive `edge_contract2` ne peut pas retirer une arête de bord), indépendamment du flag.
- **Mécanisme** (méthode 3, Garland-Heckbert) : pour chaque arête intérieure détectée comme feature, on ajoute aux deux extrémités une **quadrique de contrainte** = plan perpendiculaire à la face adjacente contenant l'arête, pondéré (`FEATURE_WEIGHT = 1000`). Effet : l'optimum de collapse reste sur la ligne de feature et collapser à travers une feature est fortement pénalisé (préservation *douce*, recommandée par la Partie 1).
- **Détection de feature** : (a) **crease** = angle dièdre entre normales de faces adjacentes > seuil (`feature_angle_deg`) ; (b) **seam de matériau** = `Face::m_iMaterialId` différents de part et d'autre de l'arête. (Les bords sont gelés, donc traités séparément.)
- **Validation par exécution** (tests dans `tu_cgmesh_simplification.cpp`, tous PASS) : grille plane avec **seam de matériau** le long de x=0.5 (plan géométriquement sans feature, donc seul le seam peut retenir la ligne) → décimation à 0.3 : **12** sommets conservés sur la ligne avec `preserve_features=true` contre **6** sans. Modes true/false également valides sur `rabbit.obj`. Aucune régression (`TEST_cgmesh_he.*`, `TEST_cgmesh_merge*` : 24 tests verts).
- **Limites assumées** : préservation *douce* (pas de mode strict « interdire tout collapse de feature ») ; les bords sont gelés et non simplifiés le long de leur courbe (la primitive de collapse ne gère pas les demi-arêtes de bord, `m_pair<0`) ; pas encore de pondération par l'aire.

## Étape 3 (Voie A) — Préservation des attributs d'apparence — IMPLÉMENTÉ et validé par exécution — 2026-06-24

- **API** : la préservation des attributs est un **paramètre booléen**, regroupé avec les options de features dans une **structure d'options** (cf. « API consolidée » ci-dessous).
- **Mécanisme (Voie A pragmatique)** : la quadrique reste **3D** (position/coût). À chaque collapse, les attributs **par-sommet** (couleurs `m_pVertexColors`, normales `m_pVertexNormals`) sont **interpolés** au point fusionné via le paramètre d'arête `t = clamp(<p−pu, pv−pu>/‖pv−pu‖², 0, 1)` (normales renormalisées), puis **transportés** dans le maillage reconstruit (au lieu d'être recalculés/perdus). Les normales de face sont recalculées (cohérence rendu) ; les normales par-sommet préservées les surchargent.
- **Périmètre assumé** : attributs **par-sommet** uniquement. Les **UV par-coin** (`Face::m_pTextureCoordinates`) ne sont **pas transportés** — leur prise en charge fidèle exige la couche **wedges** (Voie B), car un seam UV donne plusieurs vecteurs UV par sommet topologique. Les seams de **matériau** restent traités comme features (Étape 2) ; les seams UV ne sont pas détectés ici (inutile tant que les UV ne sont pas transportés).
- **Validation par exécution** (tests `tu_cgmesh_simplification.cpp`, 7 PASS) : grille plane avec **champ de couleur linéaire** `couleur=(x,y,0)` → après décimation à 0.3, **déviation max = 0** (un champ linéaire est préservé exactement par l'interpolation d'arête) et `m_pVertexColors` conservé (taille 3·nV). Avec `preserve_attributes=false`, les couleurs sont abandonnées. Aucune régression (`TEST_cgmesh_he.*`, `merge*` : 24 tests verts précédemment).
- **Limite** : si la géométrie change beaucoup, des normales par-sommet préservées peuvent diverger de la géométrie (pour une normale purement géométrique, passer `preserve_attributes=false`).

## Étape 3 (Voie B) — Quadrique nD (position + couleur) — IMPLÉMENTÉ et validé par exécution — 2026-06-24

- **Mécanisme** : quadrique **généralisée Garland-Heckbert 1998** sur **R⁶ = position(3) + couleur(3)** (cœur mathématique de Hoppe), implémentée de façon autonome (type `Q6`, solveur 6×6 par élimination de Gauss avec pivot partiel) sans toucher à `cgmath/quadric.cpp`. La couleur ET la position optimales **tombent directement de la minimisation** ; l'erreur d'attribut pilote donc **coût ET position**, de sorte qu'une discontinuité de couleur est préservée par la métrique elle-même (sans flag de feature). Les contraintes de features (Étape 2) sont injectées dans le bloc position de la quadrique R⁶.
- **Paramètres `SimplifyOptions`** : `attribute_metric` (bool, active la Voie B ; nécessite des couleurs, sinon repli Voie A) et `attribute_weight` (float, poids de l'erreur couleur vs géométrie). En Voie B les normales sont recalculées (non transportées).
- **Périmètre / limite assumée** : Voie B couvre **position + couleur**. Les **UV par-coin** exigent encore la **couche wedges** (un sommet → plusieurs vecteurs UV selon le secteur), **non implémentée** — c'est le morceau restant de Hoppe fidèle. Honnêteté : ce qui est livré est la *quadrique nD* (la moitié « métrique » de la Voie B), pas la couche wedges UV.
- **Validation par exécution** (tests, 9 PASS) : (a) champ de couleur **linéaire** `(x,y,0)` reproduit **exactement** par la quadrique R⁶ (déviation max = 0) → solveur correct ; (b) **step de couleur** sur grille plane (géométrie sans feature) : à `attribute_weight=1000`, ratio 0.5, la Voie B conserve **12** sommets sur la frontière de couleur contre **9** pour la Voie A (QEM géométrique) → métrique réellement *attribute-aware*. Aucune régression (`he`/`merge` : 24 tests verts).

## API consolidée — 2026-06-24

Options regroupées dans `Mesh_half_edge::SimplifyOptions` (valeurs par défaut = résultat le plus fidèle), conformément à Partie 2 §3 :

```cpp
struct SimplifyOptions {
    bool  preserve_features   = true;   // creases (angle dièdre) + seams de matériau (Étape 2)
    float feature_angle_deg   = 45.0f;  // seuil de crease (si preserve_features)
    bool  preserve_attributes = true;   // couleurs + normales par-sommet (Voie A)
    bool  attribute_metric    = false;  // quadrique R⁶ position+couleur (Voie B)
    float attribute_weight    = 1.0f;   // poids couleur vs géométrie (si attribute_metric)
};
void simplify(float target_ratio = 0.5f, const SimplifyOptions &options = SimplifyOptions());
```

Exemples : `simplify(0.3f)` (Voie A, tout préservé) ; `simplify(0.3f, {false})` (features off) ; `simplify(0.5f, {false,45.f,true,true,1000.f})` (Voie B, couleur fortement pondérée). `target_ratio` reste l'argument primaire.

## Analyse — Voie B : couche wedges UV (avant implémentation) — 2026-06-24

### Modèle de données UV réel (VÉRIFIÉ par lecture)
Deux conventions UV coexistent dans `Mesh` :
- **Face-indexée** (produite par l'import OBJ, `mesh_io.cpp:354-377`) : `Mesh::m_pTextureCoordinates` = **pool global** des `vt`, et `Face::m_pTextureCoordinatesIndices[coin]` = index dans ce pool, par coin. C'est le vrai modèle « par-coin ».
- **Parallèle au sommet** : `m_pTextureCoordinates.size() == 2*nVertices`, indexé par id de sommet (`MergeVertices` ne traite l'UV que dans ce cas, `mesh.cpp:1564`).

**Fait décisif** [VÉRIFIÉ] : le **chemin de rendu GL réel** (`Mesh::BuildPolygonRenderData`, `mesh.cpp:682-758`, appelé par `src/cgre/vertex_buffer_manager.cpp:204` et `sulina/.../CgreQuickItem.cpp:782`) lit, pour les **triangles**, `m_pTextureCoordinates[2*vi]` **indexé par sommet** et **ignore** `m_pTextureCoordinatesIndices`. Aucun rendu immédiat per-coin. Les index per-coin ne servent qu'à l'import/export (OBJ/DAE).

### Conséquence pour le wedge
Dans ce moteur, la représentation native d'un wedge pour les triangles **est le sommet dupliqué** : pour afficher une couture UV, le sommet doit être **scindé** (un sommet par UV distinct), chaque sommet portant un seul UV (parallèle au sommet). `MergeVertices` est d'ailleurs *seam-aware* exactement ainsi (refuse de souder deux sommets coïncidents d'UV différents, `mesh.cpp:1642-1647`). Un seam scindé devient un **bord topologique** dans le half-edge (arêtes sans paire) → donc **déjà gelé** par la logique de bord existante de `simplify`.

### Deux approches
- **Approche 1 — wedge = sommet dupliqué + UV parallèle au sommet (RECOMMANDÉE)**
  - Pré-passe : convertir l'UV face-indexée → parallèle au sommet en **scindant** les sommets multi-UV (un sommet par wedge), positions dupliquées, coins de faces réindexés. (≈ construction de wedges, exprimée en duplication de sommets.)
  - L'UV devient alors **un attribut par-sommet** comme la couleur : transport par interpolation (réutilise la Voie A) et/ou intégration à la quadrique nD (Voie B : étendre R⁶→R⁸ = position+couleur+UV). Aucune nouvelle mécanique de collapse.
  - Seams = bords → **préservés automatiquement** (gelés). Rendu : correct car parallèle au sommet (ce que `BuildPolygonRenderData` attend).
  - Coût : pré-passe de split + émettre `m_pTextureCoordinates`/`m_nTextureCoordinates` au rebuild (aujourd'hui `SetFaces` les abandonne) + extension nD.
- **Approche 2 — vrais wedges per-coin au-dessus du half-edge (DÉCONSEILLÉE)**
  - Garder l'UV face-indexée, bookkeeping de wedges, transport per-coin au collapse, ré-émission d'index per-coin.
  - **Ne matche pas le render path** (qui ignore les index per-coin) → il faudrait aussi réécrire le rendu. Beaucoup plus complexe, pour un résultat non affichable sans autres changements.

### Recommandation
**Approche 1.** Elle colle au modèle de rendu réel, réutilise tout l'acquis (transport per-sommet Voie A, quadrique nD), rend les seams gratuitement préservés (bords gelés), et réduit « la couche wedges » à **une pré-passe de split + l'UV comme attribut**.

### Plan incrémental (Approche 1)
1. Pré-passe `split_uv_seams()` : face-indexé → parallèle-sommet avec duplication aux seams (no-op si déjà parallèle ou sans UV).
2. UV comme attribut par-sommet : transport Voie A (interp) — livrable et testable seul (champ UV linéaire préservé exactement).
3. UV dans la quadrique nD (R⁶→R⁸) sous `attribute_metric`.
4. Rebuild : émettre `m_pTextureCoordinates` (taille 2·nV) + `m_nTextureCoordinates`.
5. **Validation par exécution** : décimer un mesh texturé, vérifier via `BuildPolygonRenderData` que les UV sont cohérents et que les seams sont conservés.

### Hypothèses à lever par exécution [HYPOTHÈSE→EXÉC]
- Qu'un mesh OBJ texturé réel passe bien par la pré-passe de split sans exploser le compte de sommets de façon pathologique (dépend du nombre de seams).
- Que `BuildPolygonRenderData` affiche correctement le résultat parallèle-sommet (pas d'autre chemin de rendu actif qui lirait les index per-coin).
- Disponibilité d'un mesh de test texturé dans `test/data` (à vérifier ; sinon en construire un synthétique avec seam UV).

### Limites assumées
- Seams gelés (pas de simplification le long de la couture) — même limite que les bords, cohérente avec l'état actuel.
- La pré-passe augmente le nombre de sommets d'entrée (duplication aux seams) avant décimation.

## Analyse — Étape 4 : erreur de Hausdorff (avant implémentation) — 2026-06-24

### Briques spatiales réelles (VÉRIFIÉ par lecture)
- `BVH` (`bvh.h`) : **rayon uniquement** — `nearest(orig, dir, tMin)` renvoie la distance au plus proche triangle *le long d'un rayon*. Pas de requête point→surface.
- `Octree` (`octree.h`) : requêtes de **voisinage par rayon** sur nuage de points (`GetClosestPoints(pt, distance, …)`, `GetClosestIndicesPoints`) ; stockage triangles (`BuildForTriangles`) mais **aucune** requête « triangle le plus proche » exposée.
- `cgmath` : `closest_point` existe pour **lignes/segments** (`line.h`, `linesegment.h`), **pas pour triangles**.
- `Mesh::bbox_diagonal_length()` (`mesh.h:315`) et `computebbox()` : disponibles (échelle de référence pour un seuil relatif).

**Conclusion** [VÉRIFIÉ] : il n'existe **aucune** primitive distance point→triangle ni requête plus-proche-triangle. Il faut **écrire** : (a) la distance point↔triangle (projection + clamp barycentrique, algo standard), (b) une recherche du triangle le plus proche (force brute, ou descente BVH à écrire).

### Deux usages distincts à ne pas confondre
1. **Mesure de qualité** (Hausdorff symétrique entre maillage original et décimé) — pour *rapporter/valider* l'erreur réelle. Coûteux mais hors boucle.
2. **Critère d'arrêt** dans la boucle de décimation — doit être **bon marché** (évalué à chaque collapse). Le vrai Hausdorff par collapse est prohibitif.

### Options
- **(A) Fonction de mesure Hausdorff** `mesh_hausdorff(A, B)` :
  - Écrire `point_triangle_distance2(p, a, b, c)` (closest-point-on-triangle).
  - Échantillonnage déterministe (sommets + barycentres de faces ; densification pondérée par l'aire en option) — **pas de RNG** pour des tests reproductibles.
  - Recherche plus-proche-triangle : **force brute** O(S·F) pour une v1 correcte (OK sur maillages modérés), **descente BVH closest-point** (à ajouter au BVH : pruning par distance AABB↔point) pour passer à l'échelle.
  - `d(A→B)=max_p min_t dist(p, t)`, symétrique `max(d(A→B), d(B→A))`, normalisé par la diagonale bbox.
- **(B) Critère d'arrêt par erreur**, option `SimplifyOptions::max_error` (fraction de la diagonale bbox) :
  - **Proxy QEM bon marché** (recommandé) : le coût QEM ≈ somme des distances² aux plans supports ≈ déviation². Arrêter quand le meilleur candidat a un coût > `(max_error · diag)²`. Réutilise l'existant, aucun calcul spatial. **Borne approximative**, à étiqueter comme telle (≠ vrai Hausdorff).
  - **Vrai Hausdorff borné** (Klein / simplification envelopes) : suivre la distance réelle au maillage **original** par collapse → exact mais coûteux et intrusif. Hors v1.
  - Note : le proxy QEM est défini pour le **coût géométrique** (chemin 3D). En mode `attribute_metric` (Voie B), le coût mélange géométrie+attributs → le seuil `max_error` y serait mal défini ; le restreindre au chemin géométrique ou documenter.

### Recommandation
- **(B) avec proxy QEM** comme critère d'arrêt (`max_error`), combiné au `target_ratio` existant (arrêt au premier atteint : « décime jusqu'à 30 % mais jamais au-delà de 1 % d'erreur »).
- **(A) fonction de mesure Hausdorff** (force brute d'abord) pour *rapporter l'erreur réelle obtenue* et alimenter les tests — découplée de la boucle. Descente BVH closest-point en optimisation ultérieure si besoin de passage à l'échelle.

### Plan incrémental
1. `point_triangle_distance2()` (+ tests unitaires sur cas connus).
2. `mesh_hausdorff(A, B)` force brute + normalisation diagonale (test : plan vs plan décalé → distance = décalage ; grille plane décimée → 0).
3. `SimplifyOptions::max_error` + arrêt par proxy QEM ; rapport de l'erreur réelle via (2) après décimation.
4. (Option) descente BVH closest-point si la force brute est trop lente sur gros maillages.

### Hypothèses à lever par exécution [HYPOTHÈSE→EXÉC]
- Corrélation proxy QEM ↔ vrai Hausdorff suffisante pour que `max_error` soit utile (à mesurer : décimer à divers seuils, comparer coût-proxy et Hausdorff réel).
- Performances de la force brute acceptables sur les tailles visées (sinon BVH closest-point).
- Unités du coût QEM (quadriques à normale unitaire ⇒ coût = distance² aux plans) cohérentes avec `(max_error·diag)²` — à confirmer par mesure.

### Limites assumées
- v1 : mesure approximée par échantillonnage (sommets+barycentres) — sous-estime sur grandes faces ; densifier si besoin.
- Critère d'arrêt = proxy QEM, pas une borne de Hausdorff garantie.

## Étape 4 — Hausdorff (mesure + arrêt borné) — IMPLÉMENTÉ et validé par exécution — 2026-06-24

> Primitives écrites de façon **réutilisable** (rien de spécifique à la simplification) et accélérées par le **BVH existant**.

- **`cgmath` (géométrie pure, réutilisable)** : `point_triangle_distance2(p,a,b,c[,closest_out])` — closest-point-on-triangle (Ericson), `cgmath/geometry.{h,cpp}`. Accepte des `const vec3`, sans dépendance Mesh.
- **`cgmesh/BVH` (structure d'accélération)** : nouvelle requête `BVH::closest_distance2(p[,closest_out])` — point le plus proche de la surface par **descente à élagage distance AABB↔point** (enfant le plus proche d'abord). Réutilise le BVH split-médian déjà construit ; utilisable par tout algo (SDF, snapping, collision…). La requête rayon existante (`nearest`) est intacte (régression OK).
- **`cgmesh/mesh_metrics.{h,cpp}` (comparaison de maillages, réutilisable)** : `mesh_hausdorff(A,B)` (one-sided + symétrique) et `mesh_hausdorff_relative(A,B)` (normalisé par la diagonale bbox). Échantillonnage déterministe (sommets + barycentres), BVH pour le plus-proche-point.
- **Arrêt borné dans `simplify`** : `SimplifyOptions::max_error` (fraction de la diagonale, 0 = désactivé), avec **deux modes** sélectionnés par `SimplifyOptions::exact_error` :
  - `exact_error=false` (défaut) — **proxy QEM** : le tas étant un min-heap, on s'arrête dès que le collapse vivant le moins cher dépasse `(max_error·diag)²`. Gratuit, conservateur, **borne non garantie** (distance aux plans supports, pas à la surface). Chemin géométrique uniquement (ignoré si `attribute_metric`).
  - `exact_error=true` — **borne surfacique fiable** : un `BVH` est construit sur le maillage **original** (snapshot, car `simplify` mute les positions en place) ; tout collapse dont le nouveau sommet dépasse `max_error·diag` de la surface d'origine est **rejeté** (distance point↔surface réelle via `BVH::closest_distance2`). Coût : une requête plus-proche-point par collapse tenté. **Portée honnête** : borne la déviation des **sommets** décimés vs la surface d'origine — **pas** une enveloppe de Hausdorff bilatérale (intérieurs de triangles et direction inverse non bornés).
  - Combiné à `target_ratio` (premier atteint / plus aucun collapse admissible).
- **Validation par exécution** (tests `tu_cgmesh_metrics.cpp` + `tu_cgmesh_simplification.cpp`, 63 tests verts au total) :
  - point↔triangle : intérieur/arête/sommet sur cas connus.
  - `BVH::closest_distance2` : point au-dessus d'un plan → distance = hauteur ; point le plus proche sur le plan.
  - `mesh_hausdorff` : maillages identiques → 0 ; plan vs plan décalé de 0.3 → **0.3 exact** ; grille plane décimée → **5.4e-6** (≈0).
  - `max_error` proxy QEM : rabbit, cible 2 % mais borne 0.5 % → arrêt à **776 faces** (au lieu de ~18), **erreur réelle obtenue 0.254 % ≤ 0.5 % demandé** → proxy conservateur et bien corrélé ici.
  - `exact_error` : rabbit, borne 2 % → **pire sommet décimé à 0.0508 ≤ borne 0.0523** (gate respecté) ; le Hausdorff échantillonné inclut des barycentres à 0.182 (intérieurs sur zones courbes, **non bornés** — limite assumée et affichée par le test).
- **Limites assumées** : mesure par échantillonnage (sous-estime sur grandes faces — densifier si besoin) ; proxy QEM = borne non garantie ; mode exact = borne **sur les sommets** vs surface d'origine, pas une enveloppe Hausdorff bilatérale.

## Voie B — couche wedges UV (Approche 1) — IMPLÉMENTÉ et validé par exécution — 2026-06-24

> Implémentation de l'Approche 1 de l'analyse ci-dessus, activée par un booléen.

- **Opération réutilisable** : `Mesh::SplitVerticesByUVSeams()` (`mesh.h`/`mesh.cpp`) — « explose » les coutures UV : un sommet topologique portant plusieurs UV (un *wedge*) est **dupliqué** en un sommet par UV distinct ; positions et attributs par-sommet (couleurs/normales) recopiés, faces réindexées, UV rendu **parallèle au sommet** (`m_pTextureCoordinates` taille 2·nV). No-op si pas d'UV ou déjà parallèle ; idempotent. Générale (rendu, export, tout algo devant porter les UV).
- **Paramètre** `SimplifyOptions::preserve_uv` (bool). Quand vrai : `simplify` appelle d'abord `SplitVerticesByUVSeams()` (après triangulation), puis transporte l'UV comme **attribut par-sommet** (interpolation au paramètre d'arête, dans les deux modes A et B) et l'**émet** parallèle-sommet au rebuild.
- **Seams gratuitement préservés** : après split, les sommets de couture sont coïncidents mais topologiquement séparés → arêtes de couture sans paire → **bords gelés** par la logique existante. Cohérent avec le render path (`BuildPolygonRenderData` lit l'UV par-sommet).
- **Validation par exécution** (tests, 70 verts au total) : `uv_seam_split` (quad avec couture sur v0 → 5 sommets, UV parallèle-sommet, idempotent) ; `preserve_uv_linear_exact` (grille UV linéaire `uv=(x,y)` décimée à 0.3 → **déviation max = 0**, UV émis parallèle-sommet). Aucune régression (`io`, `merge`, `triangulation`, `he`, `bvh`).
- **Transport** : par défaut l'UV est transporté par interpolation (Voie A) avec seams gelés. Conversion UV face-indexée→parallèle supposée sur maillage triangulé.

## Voie B — UV dans la quadrique nD (R⁶→R⁸) — IMPLÉMENTÉ et validé par exécution — 2026-06-24

- **Généralisation** : le quadrique nD `Q6` (fixe R⁶) est devenu `QN` de **dimension variable ≤ 8** (`QN_MAX=8`), partagée par tous les sommets d'un run, matrice dans un buffer fixe (pas d'alloc par sommet). Layout du vecteur augmenté : `[position(3)] [couleur(3) si nd_color] [UV(2) si nd_uv]`.
- **Sélection** : quand `attribute_metric` est actif, les attributs **présents** sont repliés dans la métrique — couleur (si `m_pVertexColors`), UV (si `preserve_uv`). Dimensions possibles : R⁵ (pos+UV), R⁶ (pos+couleur), **R⁸ (pos+couleur+UV)**. La couleur et l'UV optimaux tombent de la minimisation ; les attributs hors métrique restent interpolés.
- **Validation par exécution** (tests, 72 verts au total) : `attribute_metric_uv_linear_exact` (R⁵, champ UV linéaire → **déviation 0**) ; `attribute_metric_uv_and_color_exact` (R⁸, couleur **et** UV linéaires → **déviation 0**). Aucune régression.
- **Portée** : Voie B « Hoppe fidèle » désormais complète sur position+couleur+UV (par-sommet, seams gelés). Les normales restent recalculées en mode nD (non intégrées à la métrique — choix volontaire, une normale n'est pas une quantité QEM linéaire naturelle).

## Étape 5 — coût memoryless (Lindstrom-Turk) — NON RETENUE — 2026-06-24

Écartée volontairement (décision 2026-06-24). Le coût memoryless n'apporte essentiellement qu'une économie de mémoire (pas de quadrique par sommet), qui **n'est pas un goulot mesuré** ici, et il aurait deux régressions : (a) perte du sens d'erreur implicite vers la surface d'origine (le QEM accumulé + `exact_error` le fournissent), (b) **géométrie seule** → perte de l'intégration des attributs (Voie A/B couleur+UV). À reconsidérer uniquement en cas de **contrainte mémoire avérée** sur de très gros maillages — et même là, réduire la dimension nD (R³/R⁶) serait un levier plus simple.

## Robustesse manifold (debug viewer) — 2026-06-24

Symptôme : sur des STL soudés (Starman, Dragon articulé) la décimation produisait des arêtes non-manifold. Diagnostic + corrections (validés par exécution) :

- **L'algo ne crée pas de non-manifold sur entrée manifold** : sur maillage manifold propre (rabbit, bunny STL soudé), la sortie est **0 arête non-manifold** (asserté). `Base_Starman.stl` est non-manifold *en entrée* (2 M+ arêtes) → l'edge-collapse ne répare pas, il transporte ; un **avertissement** est affiché dans le viewer (`ApplyDecimation`).
- **Link condition complétée** (créait des non-manifold sur Dragon, entrée manifold-par-arêtes) : (1) voisins partagés = **exactement les apex** {iv3,iv4} ; (2) l'arête (iv3,iv4) ne doit pas déjà exister ; (3) doublon d'arête testé via `get_edge` (map maintenue) ; (4) anneaux de u et v = **boucles fermées propres**.
- **Sommets bowtie gelés** : détectés **une fois** sur l'entrée (analyse du *link* de chaque sommet, indépendante du half-edge dont `edge_contract2` maintient imparfaitement les structures) et gelés comme des bords. (Tentative via `m_map_edges` sur-rejetait → abandonnée.)
- **Résultat** : Dragon_v2.stl décime désormais à **0 arête non-manifold** (décimation moindre là où le modèle soudé a des bowties — compromis assumé : garantie manifold > taux). Maillages propres : décimation pleine + 0 non-manifold. **73 tests verts**.

## État de la feature
Décimation QEM **complète** : MVP (Étape 1) + features/bords (Étape 2) + attributs Voie A (Étape 3) + Voie B nD position+couleur+UV avec couche wedges + Hausdorff (mesure réutilisable + arrêt borné proxy/exact). Étape 5 non retenue. Optimisations possibles restantes (non requises) : échantillonnage Hausdorff densifiable ; poids d'attribut distinct couleur/UV ; descente BVH closest-point déjà en place.
- **Standards** : respecter `cpp-coding-standards` et `cmake-patterns` ; aucune dépendance externe nouvelle requise (tout réutilise `cgmath`/`cgmesh` ; build déjà couvert par le GLOB du `CMakeLists.txt`).

