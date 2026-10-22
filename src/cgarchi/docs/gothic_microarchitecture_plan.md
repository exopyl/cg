# Plan d’implémentation — microarchitecture gothique 3D

## Objectif

Intégrer dans `cgarchi` la méthode de Segall et al. (SIGGRAPH 2025) : reconstruire un réseau de courbes gothiques 3D à partir d’un plan 2D paramétré, de ses symétries et de règles de construction de la vue latérale. Le premier résultat doit être le réseau 3D lui-même. La conversion en maillage est une étape distincte et optionnelle.

L’article décrit des ogives latérales comme des biarcs composés de deux arcs circulaires raccordés tangentiellement et de courbures opposées. Le plan et la vue latérale sont paramétrés séparément, puis combinés pour obtenir les courbes spatiales. Il distingue aussi les croisements visibles en plan des véritables jonctions 3D. Référence : `C:\home\Doc\papers\Gothic\2025_Computational Modeling of Gothic Microarchitecture.pdf`, sections 4.1–4.4, pp. 5–8 ; résultats et limites, pp. 8–10.

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

## Jalons et critères d’acceptation

1. **Données et JSON** — Un fichier décrit courbes de base, symétries, points au sol et paramètres. Les données invalides produisent des erreurs localisées.
2. **Profil latéral** — Une courbe simple produit une ogive biarc `G1` satisfaisant les contraintes. Les cas dégénérés sont signalés.
3. **Courbe spatiale** — Les projections de la sortie correspondent au plan et au profil latéral dans une tolérance spécifiée.
4. **Symétries et topologie** — Les courbes répliquées sont cohérentes en 3D ; les faux croisements ne sont pas transformés en jonctions.
5. **Visualisation** — L’export du réseau fonctionne indépendamment du maillage ; la visualisation tubulaire est optionnelle et documente ses limites.
6. **Maker** — Le générateur est accessible sans confondre le réseau de courbes avec son maillage d’affichage.

## Décisions à confirmer avant le développement

- Représentation canonique des courbes : arcs exacts, polylignes échantillonnées, ou prise en charge des deux.
- Paramétrisation par longueur d’arc obligatoire dès le premier jalon ou ajoutée après un prototype plus simple.
- Format de sélection explicite du PRC en cas de plusieurs candidats.
- Premier consommateur attendu : export de courbes, visualisation Maker ou édition interactive.
- Prise en charge des niveaux dans le premier schéma JSON ou extension ultérieure.

## Références internes

- `src/cgarchi/parameterized_gothic.h` et `src/cgarchi/parameterized_gothic.cpp` — fenêtre gothique existante.
- `src/cgmesh/parameterized.h` — interface paramétrique.
- `src/cgarchi/gothic_parametric_catalog.{h,cpp}` — formes paramétriques gothiques ; `src/cggraph/nodes/shapes/shape_catalog.{h,cpp}` — réunion avec le catalogue de `cgmesh`.
- `src/cgmesh/surface_basic.h` — génération de tubes depuis des polylignes.
- `src/cgarchi/CMakeLists.txt` — sources de `cgarchi` (glob).
