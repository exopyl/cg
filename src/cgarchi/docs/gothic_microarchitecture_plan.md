# Plan d’implémentation — microarchitecture gothique 3D

## Objectif

Intégrer dans `cgarchi` la méthode de Segall et al. (SIGGRAPH 2025) : reconstruire un réseau de courbes gothiques 3D à partir d’un plan 2D paramétré, de ses symétries et de règles de construction de la vue latérale. Le premier résultat doit être le réseau 3D lui-même. La conversion en maillage est une étape distincte et optionnelle.

L’article décrit des ogives latérales comme des biarcs composés de deux arcs circulaires raccordés tangentiellement et de courbures opposées. Le plan et la vue latérale sont paramétrés séparément, puis combinés pour obtenir les courbes spatiales. Il distingue aussi les croisements visibles en plan des véritables jonctions 3D. Référence : `C:\home\Doc\papers\Architecture\Gothic\2025_Computational Modeling of Gothic Microarchitecture.pdf`, sections 4.1–4.4, pp. 5–8 ; résultats et limites, pp. 8–10.

## État du dépôt et contraintes d’intégration

- `src/cgarchi/parameterized_gothic.{h,cpp}` génère une fenêtre de remplage et son maillage. Garder ce périmètre séparé du réseau de microarchitecture.
- `src/cgmesh/parameterized.h` définit `IParameterized`, dont la sortie courante est principalement un `Mesh`. Un réseau de courbes ne doit pas être présenté comme s’il était équivalent à ce maillage.
- Les formes paramétriques gothiques sont déclarées par `GothicParametricShapes()` (`src/cgarchi/gothic_parametric_catalog.cpp`), pas par `src/cgmesh/parametric_catalog.cpp` ; Maker et la couche nodale les lisent par la réunion `cggraph_nodes::CatalogShapes()` (`src/cggraph/nodes/shapes/shape_catalog.h`), et la page de Maker les présente via le catalogue de `maker/wasm_api.cpp`.
- `src/cgmesh/surface_basic.h` expose `CreateTubes` pour visualiser des polylignes 3D. Cette conversion ne garantit pas une coque unique ou manifold aux jonctions.
- Les nouveaux fichiers vont dans `src/cgarchi/`, sous `namespace cgarchi`. `src/cgarchi/CMakeLists.txt` globe ses sources sans branche Emscripten : maker liant `cgarchi`, toute source y doit aussi compiler sous Emscripten.

Ne pas ajouter Clipper2 à la génération des courbes : les opérations booléennes 2D de la fenêtre ne sont pas requises par la méthode de reconstruction décrite dans l’article.

## Architecture proposée

Créer un sous-système autonome, sans modifier le générateur de fenêtre :

- `src/cgarchi/gothic_microarchitecture.h/.cpp` : structures de données, transformations de symétrie, validation, reconstruction et représentation du réseau 3D.
- `src/cgarchi/gothic_microarchitecture_json.h/.cpp` : lecture et écriture d’un format JSON versionné.
- `src/cgarchi/parameterized_gothic_microarchitecture.h/.cpp` : adaptateur Maker éventuel, une fois stabilisé le contrat de sortie du réseau.

La représentation du réseau doit distinguer les courbes, les extrémités, les jonctions validées et les niveaux. Elle doit permettre l’échantillonnage ou l’export sans imposer la génération de triangles.

## Modèle d’entrée initial

Le format initial décrit explicitement :

- les courbes de base du plan, d’abord segments et arcs circulaires ;
- les symétries de rotation (centre et ordre) et de réflexion (axe) ;
- les points marqués comme étant au sol ;
- les paramètres de reconstruction de la vue latérale, dont les hauteurs de contrôle ;
- les éventuels niveaux, identifiés séparément même si le premier jalon n’en génère qu’un.

Les répliques obtenues par symétrie ne sont pas dupliquées dans l’entrée. Le premier jalon ne fait ni vectorisation d’image, ni reconnaissance de courbes dans un scan : l’article prend le plan paramétré comme entrée.

## Algorithme

### 1. Valider et développer le plan

Vérifier que les courbes sont finies et non dégénérées, que les transformations de symétrie sont valides et que les paramètres restent cohérents après transformation. Développer les courbes de base en répliques tout en conservant leur identité et leur paramétrisation.

### 2. Déterminer les points de rebroussement (PRC)

Pour chaque courbe de base, calculer les intersections avec ses répliques. Une intersection entre courbes répliquées est candidate au PRC lorsque les paramètres des deux courbes au point d’intersection coïncident, conformément à la méthode de l’article. En présence de plusieurs candidates, choisir par défaut celle qui est la plus proche du milieu de la courbe ; permettre une sélection explicite pour lever l’ambiguïté.

Les croisements entre courbes de base distinctes peuvent être de véritables intersections, mais doivent eux aussi être validés en 3D avant d’être enregistrés comme jonctions.

### 3. Construire le profil latéral

Construire une ogive latérale sous forme de deux arcs circulaires raccordés avec continuité tangentielle `G1` et de courbures opposées. Respecter les contraintes du modèle :

- les points marqués reposent au sol ;
- les tangentes aux extrémités sont verticales dans la vue latérale ;
- le PRC correspond à la projection du point identifié dans le plan ;
- une hauteur de contrôle utilisateur fixe le degré de liberté restant.

Détecter et rapporter les profils impossibles, dégénérés ou incompatibles avec ces contraintes. Ne pas remplacer silencieusement un profil invalide par une courbe d’un autre type.

### 4. Reconstituer chaque courbe 3D

Pour une courbe de plan `ℓt(u) = (x(u), y(u))` et une courbe latérale `ℓs(v) = (g(v), h(v))`, calculer la courbe spatiale à partir de la hauteur `h(g⁻¹(p(u)))`, où `p(u)` établit la correspondance entre les paramètres des deux vues. Privilégier la longueur d’arc du plan comme paramétrisation indépendante de la direction de vue ; toute option plus simple doit être documentée avec ses limites.

Garanties à préserver :

- la projection XY de la courbe spatiale reproduit la courbe d’entrée ;
- sa projection latérale reproduit le profil calculé ;
- les symétries conservent les hauteurs ;
- deux courbes ne deviennent une jonction que si leurs positions 3D coïncident selon la tolérance définie.

### 5. Construire le réseau

Appliquer les symétries aux courbes reconstruites et créer les jonctions uniquement après validation spatiale. Conserver les niveaux séparés. Un croisement projeté en XY avec des hauteurs différentes reste un croisement visuel, pas une jonction topologique.

## Sorties et maillage

La sortie principale est un réseau de courbes 3D, accompagné des diagnostics de génération et sérialisable indépendamment du rendu.

Dans une étape ultérieure, proposer une visualisation tubulaire via `CreateTubes`. Cette sortie est une approximation de visualisation : les jonctions tubulaires peuvent s’interpénétrer et ne garantissent pas une coque fermée ou manifold. Le remplissage des faces délimitées par le réseau, la fusion des jonctions et la production d’un maillage imprimable sont hors du premier périmètre.

## Intégration Maker

Commencer par un import/export JSON : un réseau de courbes avec symétries et contrôles géométriques est mal représenté par une série de sliders. Après stabilisation de l’API, décider si Maker peut exposer directement le réseau ou s’il faut un adaptateur qui conserve le réseau comme résultat principal et fournit en plus un maillage tubulaire pour l’affichage.

Ajouter l’entrée au catalogue paramétrique (`GothicParametricShapes()`, réuni par `cggraph_nodes::CatalogShapes()`) uniquement si cette distinction entre réseau et maillage reste claire pour les consommateurs existants.

Contrainte mémoire pour J6 : construire le réseau, sérialiser son JSON puis le **libérer**, et seulement ensuite construire les tubes. Ainsi le pic reste d’environ 520 Mo de tas WASM (tubes au plafond M20, mesuré), contre environ 890 Mo si le réseau, son JSON et les tubes sont gardés ensemble (somme des pics mesurés de M16-bis et M20 ; plan d’exécution, §12.6). Ce pic suppose l’affichage par `meshData` seul : l’amorçage d’o3dv par `regenerate` (OBJ texte du maillage entier) le portait à 1 489 Mo (mesuré, §13.7). `regenerate` rend donc une chaîne vide pour la microarchitecture, et la page s’amorce sans OBJ. Depuis M42, un import qui remplace une microarchitecture la garde pendant le chargement de la suivante : le pic de l'import devient la somme des deux objets : 1 176 Mo mesurés au pire sur D4, résiduel d'un ancien STL compris.

Contrainte mémoire pour J7-d (M29 point 5, M33, M36) : le solide imprimable est construit au clic par `exportMicroarchStl`, vérifié, sérialisé en STL puis libéré ; les octets sont copiés dans le tas JS et le tampon WASM est rendu avant le retour. Le seuil de vérification vaut **2²² triangles**, le plafond de M29 (M36, tranché en A5) : tout solide accepté est vérifié, et le chemin « non vérifié » ne se joue plus que par un seuil injecté (M38, Q-A6). Pire cas mesuré en A5 (plan d'exécution, §15.9), F au moins 0,98 × 2²², tubes d'affichage au plafond de M20, tampons `g_pos` et `g_idx` de `meshData` et 100 Mio d'un ancien `g_exportBytes` vivants : **1 597 et 1 600 Mio** sur deux réseaux denses, sous les 2 Gio du tas ; 1 028 à 1 036 Mio avec les tubes par défaut. Historique : avant A2, le seuil était de 2²¹ (M34), dernier F avant que `ValidateMesh` ne double ses tableaux ; A2 les réserve exactement.

## Jalons et critères d’acceptation

1. **Données et JSON** — Un fichier décrit courbes de base, symétries, points au sol et paramètres. Les données invalides produisent des erreurs localisées.
2. **Profil latéral** — Une courbe simple produit une ogive biarc `G1` satisfaisant les contraintes. Les cas dégénérés sont signalés.
3. **Courbe spatiale** — Les projections de la sortie correspondent au plan et au profil latéral dans une tolérance spécifiée.
4. **Symétries et topologie** — Les courbes répliquées sont cohérentes en 3D ; les faux croisements ne sont pas transformés en jonctions.
5. **Visualisation** — L’export du réseau fonctionne indépendamment du maillage ; la visualisation tubulaire est optionnelle et documente ses limites.
6. **Maker** — Le générateur est accessible sans confondre le réseau de courbes avec son maillage d’affichage.

## Décisions (2026-10-22)

| # | Sujet | Décision | Source |
|---|---|---|---|
| M1 | Représentation canonique des courbes | Segments et arcs circulaires **exacts** en entrée et dans le modèle ; polylignes échantillonnées uniquement en sortie (export, tubes) | utilisateur |
| M2 | Paramétrisation | Longueur d’arc du plan **dès J1** (analytique pour segments et arcs) | utilisateur |
| M3 | Premier consommateur | **Export de courbes** : JSON du réseau et polylignes 3D ; Maker plus tard | utilisateur |
| M4 | Périmètre lancé | **J1 à J4** ; J5 (visualisation) et J6 (Maker) décidés ensuite | utilisateur |
| M5 | Sélection du PRC | Par défaut le candidat le plus proche du milieu de la courbe ; champ JSON optionnel pour désigner explicitement le candidat | défaut de l’orchestrateur, révisable |
| M6 | Niveaux | Champ `level` présent dès la v1 du schéma JSON ; un seul niveau généré au premier jalon | défaut de l’orchestrateur, révisable |
| M7 | Filtre `t = t'` des candidats PRC (Algo. 1, ambigu) | **Filtrer `t = t'`**, comme le code des auteurs (`llorz/SIG25_goldschmiedrisse` @ `c48eae8`, `web/js/utils/intersect.js` l. 151-180 : `|t − t'| < 1e-2`, extrémités exclues ; MATLAB : `1e-6`). Moulinet C4 : s = 2,5. Les auteurs n'appliquent pas le palier 2 (`state.js` l. 300-305, commenté) : le garder relève de la lecture de l'article [H]. Leur sélection explicite du PRC (clic sur intersection filtrée, ou glissement libre) diffère de M5, qui reste un choix projet | utilisateur ; code lu le 2026-10-22 |
| M8 | Profil latéral non monotone (`k < 1`) | **Avertissement** : la courbe est produite, le rapport le signale | utilisateur |
| M9 | Plusieurs niveaux déclarés en v1 | **Erreur localisée** « plusieurs niveaux non pris en charge » | utilisateur |
| M10 | Export du réseau (JSON + OBJ de polylignes) | Avancé de J5 à **J4-b** ; les tubes restent en J5 | utilisateur |
| M11 | Unité de l’axe miroir dans le modèle | **Degrés** (`SymmetrySpec::reflection_axis_deg`), conversion en radians au calcul : l’aller-retour JSON est exact par construction (revue J1, M2) | utilisateur |
| M12 | Arc presque plat (limite L2) | **Refus** au-delà de `r/D > 1e6` (`SpecLimits::max_arc_radius_ratio`) : erreur `nearly_flat_arc` sur `curves[i]`, seule (plus de `arc_endpoint_mismatch` trompeur), même contrôle pour le lecteur JSON et la validation C++. Seuil : facteur 10 sous la mesure de J2-b, où l’erreur de représentation atteint ~τ vers r/D = 1e7 | utilisateur, 2026-11-01 |
| M13 | PRC près d’un bout (« aiguille » de profil) | Le candidat est **gardé** ; avertissement `prc_near_endpoint` sur `curves[i]` quand le PRC retenu **automatiquement** a min(u, 1 − u) < 0,05 (`PrcLimits::near_endpoint_u`). La sélection explicite n’avertit pas | défaut de l’orchestrateur, révisable (2026-11-01) |
| M14 | Abscisse d’un point commun à une extrémité (réserve R1) | Dans `Intersect` et `IntersectAxis`, un point à τ d’une extrémité (`at_end1` / `at_end2`) a s = 0 ou L **exactement**. Au voisinage d’une tangente latérale verticale, 1 ulp d’erreur sur s en donnait √(2 r ulp) sur z | utilisateur, 2026-11-01 |
| M15 | Volume de `SampleNetwork` | Plafond global de **1e7 points** (`SamplingLimits::max_network_points`), vérifié par un comptage préalable sans allocation des points ; au-delà, erreur `sample_limit_exceeded` et résultat vide | utilisateur, 2026-11-01 |
| M16 | Coût de `BuildNetwork` | (1) Au plus **16 384 répliques** (courbes de base × éléments du groupe, dédoublonnages compris, `SpecLimits::max_replicas`), contrôlé par la validation partagée avec le lecteur JSON : erreur `too_many_replicas` sur `curves`. (2) Au plus **8 388 608 points de contact** pendant le balayage (`NetworkLimits::max_contacts`, 2,35 fois le pire cas mesuré à 16 384 répliques) : erreur `too_many_contacts` et réseau vide | utilisateur, 2026-11-01 |
| M16-bis | Mémoire de `BuildNetwork` et de sa sortie (révise M16.2) | `NetworkLimits::max_contacts` abaissé à **131 072 = 2¹⁷** contacts. Mesures (cordes denses sous D64) : réseau accepté **au plafond exact** (131 072 contacts) à **415 Mo** de pic natif après `NetworkToJson` et **328 Mo** de tas WASM ; refus à ≤ 23 Mo (natif) et 19 Mo (WASM). Ce seul plafond borne aussi la sortie JSON, qui domine la mémoire (≈ 3 Ko de pic par croisement) : cible < 1 Go tenue | utilisateur, 2026-11-02 |
| M17 | Amas tangent des grands rayons | Option B : une tangence dont la portion commune à τ près, d’un côté du contact, dépasse 1e-3·D (`NetworkLimits::tangent_zone_ratio`) devient `plan_overlap`. Conséquence : les **moulures quasi concentriques** (deux arcs tangents intérieurement de rayons voisins, par exemple ≈ D et 1,001·D) sont refusées | utilisateur, 2026-11-01 |
| M18 | Recouvrement co-circulaire partiel | Sous la précondition d ≤ 2τ et \|Δr\| ≤ 2τ, toute portion angulaire commune plus longue que τ est un recouvrement, sans contrôle d’écart par portion (les cercles sont à 4τ l’un de l’autre partout) | utilisateur, 2026-11-01 |
| M19 | Hauteurs aux jonctions | Le chaînage des groupes de hauteur (§6.5) est gardé ; une jonction dont les incidences s’étalent en z de plus de `tol.height` donne l’avertissement `junction_height_spread` | utilisateur, 2026-11-01 |
| M20 | Plafond du maillage tubulaire (J5) | `TubeLimits::max_triangles` = **2²³ = 8 388 608**, réglable ; au-delà, `tube_limit_exceeded` sur `tubes`, refusé avant toute allocation. Pic mesuré au plafond (F = 8 388 544) : **485 Mo** natif, **517 Mo** de tas WASM (§12.6) | utilisateur, 2026-11-02 |
| M21 | Rayon des tubes | `radius_ratio` relatif à D, **0,005** par défaut, borné à (0 ; 0,1] ; hors bornes, `tube_invalid_parameter` | utilisateur, 2026-11-02 |
| M22 | Pas d’échantillonnage des tubes | `max_angle` > **π/8** refusé (`tube_invalid_parameter`) : au-delà, le biseau de `CreateTubes` rendrait les comptes et le plafond inexacts | utilisateur, 2026-11-02 |
| M23 | Sphères aux jonctions | **Non** : les extrémités ont une tangente verticale (§12.2). À revoir seulement si un rendu montre une encoche | utilisateur, 2026-11-02 |
| M24 | Défauts de `CreateTubes` (`cgmesh`) | Consignés dans `debt_cgmesh.md` §2.1, sans modifier `cgmesh` ; J5 les contourne par son repère canonique et ses contrôles | utilisateur, 2026-11-02 |
| M25 | Courbes plus courtes que le rayon des tubes | `CreateTubes` soude en boucle une polyligne d’au moins 4 points dont les bouts sont à moins de r/2. `BuildTubeMesh` coupe une telle courbe en morceaux ouverts (coupe récursive au milieu, échantillon partagé) ; `CountTubeTriangles` applique la même découpe et le même préfiltre, et devient **exact**. Cas du relecteur (D = 1000, r = 5, arc de bouts à 1,04) : 18 morceaux, F = 552, compte = construit | utilisateur, 2026-11-02 |
| M26 | Origine locale du maillage tubulaire | `TubeMesh::origin` (double) = centre de la boîte 3D des échantillons ; les sommets `float` du `Mesh` sont relatifs à cette origine, que l’appelant ajoute pour l’affichage. Écart des sommets, référence décalée de 1e6 : 0 ; de 1e8 : 1,5e-5·r (133 % et 13 600 % du rayon avant) | utilisateur, 2026-11-02 |
| M27 | Intégration Maker (J6) | Adaptateur `IParameterized` **hors catalogue** (`ParameterizedGothicMicroarchitecture`), créé seulement par `createMicroarchFromJson` ; trois curseurs d'affichage (rayon en ‰ de D, côtés, pas angulaire en degrés). Le résultat reste le réseau : exports JSON du réseau, OBJ des courbes au pas courant, OBJ des tubes avec l'origine appliquée (2²¹ triangles au plus) ; OBJ, GLB et STL génériques refusés. Pas de gabarit au catalogue. Texte d'entrée borné à 4 Mio (`json_too_large`, contrôlé avant le parsing). Une page dédiée de Maker, `microarch.html` (M41). MSVC Debug en local ; GCC par la CI, lancé par l'utilisateur | utilisateur, 2026-11-02 |
| M28 | Lecture A-1 de l'Algo. 1 (R3) et palier 2 du PRC | R3 levée pour le filtre t = t' (tranché par M7 sur le code des auteurs). Le palier 2 (`prc_from_other_basic_curve`) est gardé, explicitement accepté ; son avertissement est visible dans Maker | utilisateur, 2026-11-02 |
| M29 | Maillage imprimable (J7, §14 du plan d'exécution) | Tout §14.9 adopté. (1) Option (b) : isosurface de f = min des distances aux courbes − r, coupée à la base, marching cubes par une sous-classe d'`ImplicitSurface`, `set_orientation(0)`. (2) Pas de socle par défaut, verdict par composante. (3) Raccords francs ; congé éventuel en J7-e. (4) Seuils `PrintConstraints` réutilisés ; sans échelle, jamais déclaré imprimable. (5) Maker : export STL au clic, solide ni affiché ni gardé ; lien de `maker` autorisé en J7-d avec sauvegarde préalable, pas avant. (6) Plafonds : 2²² triangles, 2²⁸ nœuds, q ∈ [2 ; 8] (défaut 3), 2 048 nœuds par axe. (7) Défauts d'`ImplicitSurface` consignés dans `debt_cgmesh.md`, sans modifier `cgmesh` (M24 maintenu). (8) Faces de l'article hors J7. (9) Surplombs et supports hors critères | utilisateur, 2026-11-02 |
| M30 | Paroi aux plis de jonction (revue de J7-a, MEDIUM-3) | « Mesurer, seuil au verdict ». Le verdict d'imprimabilité de J7-c mesure la paroi partout, plis de jonction compris. Si le col passe sous `minWallMm`, l'objet n'est pas déclaré imprimable, et un avertissement localise le pli. La géométrie ne change pas : raccords francs (M29, point 3), congé possible en J7-e. Rien n'est implémenté avant J7-c | utilisateur, 2026-11-02 |
| M31 | Verdict de J7-c : épaisseur et cellules (révise M30 ; contre-revue de J7-a, MEDIUM-N1 et LOW-N2) | (a) « Par tube, col en info ». Le verdict exige 2r ≥ `minWallMm`, condition exacte et indépendante de q. La paroi est mesurée hors des zones de fourche (§14.3) et doit tenir ce seuil. Le col des fourches, qui dépend de q et tend vers 0, est rapporté comme information avec sa position, jamais comme un refus. (b) « Information localisée ». Une cellule du réseau fermée par le solide (rayon inscrit sous ~r) donne un avertissement avec sa position et n'empêche pas « imprimable ». Seule une fusion de composantes que le graphe sépare est bloquante (`solid_unplanned_fusion`) : P6 distingue le nombre de composantes (écart bloquant) de Σχ (écart informatif). Rien n'est implémenté avant J7-c | utilisateur, 2026-11-02 |
| M32 | Validation et refus au plafond (revue de J7-b) | (a) Validation bornée dans Maker : `ValidateMesh` reçoit un budget fixe de paires d'auto-intersection, réglé pour un pic sous ~1 Gio de tas WASM au plafond ; au-delà, « auto-intersection vérifiée partiellement ». P1-P3, P5, P6, P9 restent complets ; en natif, validation complète. (b) Estimateur préalable F ≈ 3·aire/h², avant extraction : refus au-delà d'une marge (fixée à 2 par les mesures, §14.13) ; le plafond exact reste le contrôle qui fait foi ; l'estimateur ne doit jamais refuser un cas qui passe | utilisateur, 2026-11-03 |
| M33 | Verdict dans Maker (remplace M32 a ; revue de J7-c) | Dans Maker, le verdict est complet jusqu'à un seuil de triangles mesuré pour tenir sous ~1,2 Go de tas WASM ; au-delà, le STL est exporté quand même, marqué « non vérifié ». En natif, verdict complet. `cgmesh` n'est pas modifié. Seuil mesuré en J7-d ; API prête en J7-c (`SolidPrintOptions::max_verified_triangles`, `SolidPrintVerdict::verified`, `solid_unverified`). M32 (b) est révisé par la même revue : l'estimation 3·A/h² majore F sans borne (5,7 F mesuré), elle n'est plus qu'une information (`solid_triangle_estimate_high`) et ne refuse jamais | utilisateur, 2026-11-03 |
| M34 | Seuil de vérification dans Maker (Q-J7d-1, revue de J7-d) | **2²¹ triangles, gardé.** Pic de l'export vérifié mesuré au seuil : 1 067 Mio (F = 2²¹ − 576) avec les tubes d'affichage par défaut, saut à 1 323 Mio dès 2²¹ + 352. Pire cas mesuré, tubes au plafond de M20 (8 354 816 triangles) : 1 384 Mio, 1 516 Mio avec les tampons `g_pos` et `g_idx` de `meshData`, 1 628 Mio avec en plus 100 Mio d'un ancien `g_exportBytes` (désormais rendu en tête de `exportMicroarchStl`). Sous les 2 Gio du tas ; au-dessus de 1,2 Go, accepté | utilisateur, 2026-11-03 |
| M35 | Exception à M24 : option A, quatre correctifs dans `cgmesh` (plan : §15 du plan d'exécution) | M24 est levée pour **quatre** correctifs, et pour eux seuls, dans cet ordre : (A1) `main` de la démo glutess retiré de la bibliothèque `cgmesh` ; (A2) mémoire de `ValidateMesh` ; (A3) `ImplicitSurface` (indices entiers, compteur remis à zéro, allocations contrôlées, normales seulement si un rappel les lit) ; (A4) `CreateTubes` (clé de regroupement sans comportement indéfini, entrée vide, `nSides`, commentaires, tests directs). Chaque correctif garde des sorties **identiques au bit près** pour les appelants existants, sauf changement décidé : `evaluated_nodes` de `cgarchi` baisse (A3), entrée vide de `CreateTubes` (M37). Chacun se termine avec la suite complète verte et des détecteurs vus échouer, avant le correctif ou sous mutation. Hors de ces quatre correctifs, M24 reste en vigueur | utilisateur, 2026-11-04 |
| M36 | Seuil M33 relevé, sous condition (révise M34 si la condition tient) | Après A2, le seuil de vérification dans Maker passe de 2²¹ à **2²²** (plafond de M29), **à condition** que le pire cas mesuré au plafond exact reste sous **2 Gio** de tas WASM. Ce pire cas : export avec verdict pendant que les tubes d'affichage sont à leur plafond M20, plus `g_pos` et `g_idx` de `meshData`, plus un ancien `g_exportBytes` (protocole de M34). Si la condition ne tient pas, le seuil reste à 2²¹ (M34). Mesure et lien de `maker`, avec sauvegarde préalable, en A5 | utilisateur, 2026-11-04 |
| M37 | `CreateTubes` sur une entrée vide | Rend un maillage **vide** (0 sommet, 0 face), et non plus un triangle factice. Préalable : vérifier le comportement de `ParameterizedLSystem` et de Maker avec 0 face, et adapter l'aval si nécessaire (A4) | utilisateur, 2026-11-04 |
| M38 | Questions de l'option A (§15.8 du plan d'exécution) | Tout adopté. (Q-A1) `compute` et `get_triangulation` rendent un `bool`, sans `[[nodiscard]]`. (Q-A2) `nSides` documenté dans [3 ; 1 024] et ramené aux bornes. (Q-A3) `GridIsoExtractor` : l'essai d'allocation est remplacé par le retour de `compute` ; le plafond de 2 048 nœuds par axe et le refus d'un second `Run` sont gardés. (Q-A4) Une polyligne contenant un point non fini est écartée entière. (Q-A5) `-fsanitize=float-cast-overflow` en CI : décision à part, hors option A. (Q-A6) Le chemin « non vérifié » est inatteignable dans Maker si M36 passe, et reste testé par un seuil injecté. (Q-A7) `vertexFaces` n'est pas aplati. (Q-A8) Si M36 échoue, le seuil reste à 2²¹ | utilisateur, 2026-11-04 |
| M39 | Banc d'équivalence et défaut de `ImplicitSurfaceTandem` (revue de A0-A1) | (a) Le banc d'équivalence va dans le dépôt, sous `tools/equivalence_bench/` : sources, scripts, fixtures, étalon et README (commande, durée). Il reste hors de toute cible CMake et se lance depuis la racine du dépôt. (b) Le dépassement de `ImplicitSurfaceTandem` au-delà de 100 000 faces entre dans l'exception M35 et se corrige en A3 : une seule définition de l'accumulateur (fin de la violation de l'ODR), `farea`, `varea`, `q` et `fplane` agrandis correctement, allocations contrôlées | utilisateur, 2026-11-04 |
| M40 | Forme vide dans Maker (revue de A4) | Les deux comportements muets de Maker sur une forme vide (M37) sont corrigés en A5 : (1) un OBJ d'amorçage sans face est traité comme `""` : la page s'amorce sur un triangle, comme pour la microarchitecture, puis affiche par `meshData` ; (2) l'export GLB d'un maillage vide affiche « rien à exporter » au lieu de ne rien télécharger. La sonde Chrome couvre les deux cas | utilisateur, 2026-11-04 |
| M41 | Page de Maker (révise M27 et Q-J6-8) | La microarchitecture quitte `gothic.html` pour une page séparée, « Micro architecture gothique » (`maker/web/microarch.html`, `js/pages/microarch.js`), reliée par une carte de l'accueil. Elle reprend toute la section : import (4 Mio), trois curseurs, bannière des deux rapports, exports réseau JSON, courbes OBJ, tubes OBJ et solide STL avec son verdict, refus des exports génériques, amorçage sans OBJ texte (M40). La page n'a pas de catalogue (l'adaptateur est hors catalogue, M27) et s'ouvre sans modèle. Les contrôles de l'imprimante (`print-constraints/v1`, même clé du navigateur) sont partagés avec `gothic.html` par `js/printer_panel.js` : le solide en lit la paroi minimale et le volume utile. `gothic.html` redevient la page d'avant J6, correctif `setProvenance` gardé. `maker.wasm` inchangé | utilisateur, 2026-11-04 |
| M42 | Import refusé dans `microarch.html` (revue de M41, MEDIUM-1) | **Un import refusé garde le modèle précédent.** `createMicroarchFromJson` est appelé avant toute destruction ; un refus (spécification, réseau, exception) ne change que la bannière : modèle affiché, curseurs, exports et solide restent actifs. Accepté : l'état de l'ancienne microarchitecture est oublié, puis `setShape` détruit l'ancien objet. Coût : pendant le chargement, le tas porte l'ancien objet et le nouveau, **tubes compris** (`Load` appelle `Regenerate`, `parameterized_gothic_microarchitecture.cpp:94`). Pic mesuré (contre-revue, deux objets au plafond de M20, D4) : 976 Mo, **1 176 Mo** avec le résiduel d'un ancien STL au plafond de 2²² (`g_exportBytes`, 200 Mio, non rendu à l'import) ; réseau dense estimé à +0,1 à 0,2 Gio ; sous les 2 Gio du tas. Épuisement : `bad_alloc` rattrapé, bannière « échec », modèle précédent gardé (plan d'exécution, §13.8) | utilisateur, 2026-11-04 |

Plan d’exécution détaillé : `gothic_microarchitecture_implementation.md`.

## Références internes

- `src/cgarchi/parameterized_gothic.h` et `src/cgarchi/parameterized_gothic.cpp` — fenêtre gothique existante.
- `src/cgmesh/parameterized.h` — interface paramétrique.
- `src/cgarchi/gothic_parametric_catalog.{h,cpp}` — formes paramétriques gothiques ; `src/cggraph/nodes/shapes/shape_catalog.{h,cpp}` — réunion avec le catalogue de `cgmesh`.
- `src/cgmesh/surface_basic.h` — génération de tubes depuis des polylignes.
- `src/cgarchi/CMakeLists.txt` — sources de `cgarchi` (glob).
