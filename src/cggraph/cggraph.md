# `cggraph` — graphe de calcul

Documentation de développement. La première partie décrit **ce qui fonctionne
aujourd'hui** ; la seconde, §7 et suivantes, rassemble ce qui touche à
l'implémentation : limitations, améliorations possibles, points d'attention.

---

## 1. Rôle du module

`cggraph` évalue un **graphe orienté acyclique d'opérations** sur des données du
dépôt — maillages, polices, contours, champs scalaires. Un nœud décrit un calcul,
un lien transporte une valeur, et l'évaluation d'un nœud déclenche celle de tout
ce dont il dépend.

Le module répond à trois usages, sans en privilégier aucun :

| Usage | Ce qu'il consomme |
|---|---|
| Édition interactive d'un graphe | les quatre couches, un hôte à fenêtre |
| Rejeu d'un document sans interface | le moteur, le catalogue, `runner` |
| Empilement linéaire d'opérations | le moteur seul |

Trois propriétés le définissent :

- **Le moteur ne connaît aucun domaine.** Il manipule des `Value` opaques
  identifiées par un `TypeDesc*`. Il ne compile pas contre `cgmesh`, `cgimg` ni
  `cgmath`.
- **Le moteur ne connaît aucune interface.** Aucune structure de dessin, aucune
  dépendance à une bibliothèque de fenêtrage.
- **Un résultat calculé est réutilisé** tant que rien de ce dont il dépend n'a
  changé, l'identité étant portée par une signature.

---

## 2. Architecture

### 2.1 Les quatre couches

    core  <-  nodes  <-  ui  <-  canvas

| Répertoire | Cible CMake | Espace de noms | Rôle | Dépendances propres |
|---|---|---|---|---|
| `core/` | `cggraph` | `cggraph` | moteur | **aucune** (hors `Threads`) |
| `nodes/` | `cggraph_nodes` | `cggraph_nodes` | adaptateurs vers le domaine | `cgmesh`, `cgmath` |
| `ui/` | `cggraph_ui` | `cggraph_ui` | modèle d'éditeur, palette, inspecteur | aucune tierce |
| `canvas/` | `cggraph_canvas` | `cggraph_canvas` | dessin ImGui | `imgui`, `imgui-node-editor` |

La chaîne est **strictement linéaire** : pas de losange, pas de cycle. `ui`
n'atteint `nodes` que par un en-tête, `catalog.h`, sur deux sites d'appel.

Volumes : `core` 19 fichiers / 2 856 lignes, `nodes` 55 / 5 133, `ui` 8 / 1 038,
`canvas` 4 / 1 393.

### 2.2 Les deux contraintes vérifiables

```
grep -rn "cgmesh\|cgimg\|cgmath" src/cggraph/core        -> 0
grep -rn cggraph src/cgmesh src/cgmath src/cgimg         -> 0
```

Le premier grep porte sur le **texte**, commentaires compris : `core/` ne doit
nommer aucune bibliothèque de domaine, fût-ce dans une phrase. Le second interdit
la dépendance inverse — aucune bibliothèque de domaine ne connaît le graphe.

### 2.3 Consommateurs

| Cible | Lie | Bâtie |
|---|---|---|
| `TU` | `core`, `nodes`, `ui` | sans condition |
| `maker` (WebAssembly) | `core`, `nodes`, `ui`, `canvas` | sous `ENABLE_MAKER` |
| `cggraph-boilerplate` (natif, GLFW + OpenGL 3) | les quatre | sous `ENABLE_CGGRAPH_BOILERPLATE`, défaut `Off` |

`TU` ne lie pas `canvas` : `canvas_layout.h` et `canvas_style.h` sont
entièrement `inline` et sans ImGui, la suite les inclut sans lier la cible.

Les deux hôtes partagent le **même** canvas ; ils ne diffèrent que par le fichier
qui ouvre la fenêtre et pompe la frame.

---

## 3. Le moteur — `core/`

### 3.1 Valeurs et types

`TypeDesc` décrit un type transporté : un nom, un `clone`, un `sizeHint`, et une
mutabilité `Forkable` ou `Immutable`. `TypeRegistry` les enregistre par nom et
rend des pointeurs stables.

`Value` est un couple `(TypeDesc*, shared_ptr<const void>)`. Trois conséquences :

- la charge est **partagée et en lecture seule** — un aval ne peut pas modifier
  ce qu'un amont lui a passé ;
- `Get<T>` et `Share<T>` exigent le `TypeDesc*` attendu et rendent `nullptr` en
  cas de discordance : pas de `reinterpret_cast` implicite ;
- `GetSizeHint()` délègue au type, et c'est ce qui alimente la comptabilité
  mémoire du cache.

### 3.2 Nœuds

```cpp
struct PortDesc  { std::string name; const TypeDesc *type; bool optional; };
struct NodeDesc  { std::string typeName; int version; 
                   std::vector<PortDesc> inputs, outputs; bool sideEffect; };
```

`Node` expose `GetDesc()`, `Compute(EvalContext&, const ValueList& in, ValueList& out)`,
et porte son propre `ParamSet`.

- `version` est la version **du type de nœud**. Un document en porte une copie ;
  `IsVersionCompatible()` compare les deux.
- `sideEffect` marque les puits — un nœud qui écrit un fichier. Ils ne sont
  jamais mis en cache et ne s'exécutent que sur demande nommée.
- `RefreshExternalState()` relève l'état extérieur d'une source (date et taille
  d'un fichier, par exemple). Appelé **uniquement** pendant la pré-passe.
- `SetSubgraphReference()` désigne un document délégué.

### 3.3 Graphe

`Graph` détient les nœuds par valeur dans des `Slot` (identifiant, nœud,
position `x`/`y`, référence de sous-graphe) et une liste de `Link`.

`Connect` rend un `ConnectStatus` nommé : `Ok`, `UnknownNode`, `UnknownPort`,
`TypeMismatch`, `InputAlreadyConnected`, `Cycle`. **Le refus des cycles est fait
à la connexion**, ce qui permet aux parcours en aval de récurser sans garde.

Une entrée n'accepte **qu'un** lien ; une sortie en alimente autant qu'on veut.

`AddNodeWithId` sert au chargement : les identifiants du document sont repris
tels quels, jamais renumérotés.

### 3.4 Paramètres

`ParamSet` est une liste d'entrées nommées. Chacune porte quatre axes :

| Axe | Valeurs | Effet |
|---|---|---|
| `ParamType` | `Int`, `Float`, `Bool`, `String` | type de la donnée |
| `ParamKind` | `Literal`, `Driven` | valeur posée, ou expression à évaluer |
| `ParamRole` | `Semantic`, `NonSemantic` | **entre ou non dans la signature** |
| `ParamVisibility` | `Public`, `Internal` | lisible par un adaptateur, ou réservé |

`NonSemantic` désigne ce qui ne change pas le résultat — une couleur d'affichage.
`Internal` désigne ce que `RefreshExternalState` écrit : les accesseurs
`GetInt`/`GetFloat`/`GetBool`/`GetString` de `nodes/` **refusent** de lire un
paramètre interne, parce qu'il est écrit pendant la pré-passe, éventuellement
depuis un autre fil.

### 3.5 Signature

`Signature(graph, id, memo)` produit un `Hash` 64 bits (FNV-1a) qui agrège :

1. le **nom du type** du nœud ;
2. son `ParamSet`, **entrées sémantiques seulement** ;
3. sa **référence de sous-graphe** ;
4. pour chaque entrée : la signature de l'amont, ou une constante `kUnconnected` ;
5. l'**indice du port lu** sur cet amont.

Le point 5 est ce qui distingue deux sorties d'un même nœud amont. Le `memo`
évite de recalculer une branche partagée. Une valeur nulle est remplacée par `1`,
`kNoSignature == 0` étant réservé à l'absence.

### 3.6 Évaluation

`Evaluator::Evaluate(id, outputs, ctx)` procède en deux temps.

**Pré-passe, sous verrou exclusif du graphe** (`Graph::LockPrePass()`) :

1. `RefreshBranch` appelle `RefreshExternalState()` sur toute la branche ;
2. `Signature(graph, id, memo)` **fige** les signatures de la branche entière.

Les deux sont indissociables : la première écrit ce que la seconde lit. Les
séparer laisserait une seconde évaluation réécrire l'identité entre les deux, et
le cache servirait un résultat qui n'est pas celui qu'il annonce.

**Descente**, hors verrou : `EvaluateNode` ne relit plus aucun paramètre en
dehors de `Compute` — toutes les signatures sont dans le memo.

L'ordre des refus, par nœud : version incompatible, puis annulation, puis cache,
puis paramètres.

`EvalStatus` compte huit valeurs : `Ok`, `UnknownNode`, `IncompatibleVersion`,
`MissingInput`, `DrivenParameter`, `ComputeFailed`, `Aborted`, `Busy`.

### 3.7 Cache

Indexé par signature. `CacheStats` expose `hits`, `misses`, `evictions`,
`entries`, `bytes`.

- Budget par défaut **256 Mio**, réglable par `SetMemoryBudget`.
- Éviction **LRU**, et **l'entrée la plus récente n'est jamais victime** :
  évincer ce qu'on vient de calculer reviendrait à ne rien mettre en cache. Une
  entrée seule plus grosse que le budget survit donc, et le budget est dépassé.
- `Pin`/`Unpin` protègent une entrée de l'éviction.
- Les nœuds `sideEffect` ne sont **pas** mis en cache.

### 3.8 Annulation et progression

`EvalContext` porte deux choses, toutes deux optionnelles :

- un `std::atomic<bool>*` d'annulation, interrogé par `IsAborted()` ;
- un `ProgressSink`, `std::function<void(float t, const char *label)>`.

Côté domaine, `nodes/node_support.h` fabrique un `GraphContext` — implémentation
de l'interface `Context` de `cgmath` — que les algorithmes de `cgmesh` reçoivent
en dernier paramètre optionnel. `Context::IsAborted()` est virtuel : il ne
s'interroge que depuis les boucles **externes** des algorithmes.

### 3.9 Évaluation asynchrone

`AsyncEvaluator` porte un fil et **coalesce** les demandes : une seule demande en
attente, jamais une file — une file ferait calculer les états intermédiaires d'un
curseur qu'on déplace. Fenêtre de coalescence par défaut **150 ms**, horloge
**injectable** pour que les tests décident seuls de son écoulement.

Le fil appelant ne touche jamais l'évaluateur ni le graphe pendant un calcul : il
pose une demande, il retire des `Completed`.

⚠ Cet objet exige des fils. Sous Emscripten sans `-pthread`, il compile et il
lie, mais son constructeur **lève** `std::system_error`.

### 3.10 Validation

`ValidateNode` rend un `NodeValidation` : la disponibilité (`Ready`,
`MissingRequiredInput`, `UnknownNode`) et l'état de chaque entrée (`Connected`,
`MissingRequired`, `MissingOptional`). `ValidateBranch` fait de même sur toute la
branche amont. Une entrée `optional` non alimentée n'empêche pas l'évaluation.

### 3.11 Document

JSON, versionné à **deux étages** : `formatVersion` pour le format, `version` par
nœud pour le type.

```json
{
  "format": "cggraph", "formatVersion": 1,
  "nodes": [ { "id": 1, "type": "profile.chamfer", "version": 1,
               "x": 20.0, "y": 40.0, "params": [] } ],
  "links": [ { "from": 1, "fromPort": 0, "to": 2, "toPort": 0 } ]
}
```

Le document porte les identifiants, les positions d'écran, la version par nœud,
les paramètres avec leur rôle et leur nature, et la référence de sous-graphe.
`LoadGraph` exige un graphe **vide** et rend un `LoadResult` portant la liste des
nœuds `incompatible` — ils sont chargés, mais l'évaluateur les refusera par un
statut nommé. `SerializeStatus` compte onze valeurs.

L'analyse JSON s'appuie sur `nlohmann/json` sous `extern/`, incluse par le seul
`serialize.cpp` et en `PRIVATE` : la dépendance ne fuit pas vers les
consommateurs.

---

## 4. Les adaptateurs — `nodes/`

### 4.1 Catalogue

`Catalog()` rend une liste de `CatalogEntry` — nom de type, libellé, catégorie,
fabrique, et un champ **`caveat`** portant la limite connue de l'algorithme
sous-jacent. `MakeNode(typeName)` instancie.

**22 types**, en cinq familles :

| Famille | Types |
|---|---|
| `flow.` | `foreach`, `repeat`, `subgraph` |
| `mesh.` | `io.load`, `io.load_parts`, `io.save`, `smooth.laplacian`, `simplify`, `hull.convex`, `curvature`, `color.map`, `thickness`, `ambient_occlusion`, `align.icp` |
| `profile.` | `chamfer`, `cavetto`, `bar.keel`, `bar.ogee`, `bar.roll` |
| `shape.` | `gothic.window` |
| `text.` | `font.load`, `extrude` |

Catégories d'affichage déclarées : Analyse, Formes, Texte, et les familles.

### 4.2 Types de valeurs

`value_types.h` enregistre les `TypeDesc` du domaine : `mesh`, `meshArray`,
`font`, `selection`, `scalarField`, `glyphContours` (courbes, unités de police),
`extrudeContours` (aplatis, unités monde), `splayProfile` (ouvert, depuis
l'origine), `barProfile` (fermé, section balayée).

La distinction `glyphContours` / `extrudeContours` et `splayProfile` /
`barProfile` est portée par le **type**, pas par une convention : deux formes
incompatibles ne peuvent pas se connecter.

### 4.3 Écriture de fichiers

`FileSink::GetWrittenPaths()` rend la liste des chemins écrits, **par valeur** —
une référence sur un vecteur qu'un autre fil fait croître serait invalidée sans
que personne le voie.

Le chemin d'un puits accepte un jeton `{hash}`, remplacé par la signature du
nœud. Deux exécutions du même document écrivent donc le même nom de fichier.

### 4.4 Rejeu

`runner.h` porte tout ce qui charge, vérifie et évalue un document :

- `Check(document)` — relit et rend un `RunReport` : nombre de nœuds et de liens,
  puits, versions incompatibles ;
- `Run(RunRequest)` — évalue, `targets` nommées ou `allSinks` ;
- `FindSinks(graph)` — les nœuds sans aval.

**La cible est toujours nommée.** Un nœud à effet de bord écrit un fichier ;
exécuter « tout ce qui n'a pas d'aval » sans demande produirait des écritures que
le document seul ne justifie pas — d'où le drapeau explicite `allSinks`.

---

## 5. Le modèle d'éditeur — `ui/`

Aucune dépendance à une bibliothèque graphique. C'est de la logique d'édition,
pas du dessin.

**`EditorModel`** détient le graphe, la palette, l'inspecteur, la sélection et le
pilote d'évaluation. Il expose l'édition (`AddNode`, `Connect`, `Disconnect`,
`RemoveNode`, `Select`), la validation, et l'évaluation sous deux formes :
`Evaluate` (synchrone) ou `RequestEvaluation` + `Poll` + `Pump`.

**`Palette`** liste les entrées du catalogue par catégorie.

**`Inspector`** rend, pour le nœud sélectionné, ses `ParamField` (publics
seulement) et ses `PortField` avec leur état de connexion.

**`EvalDriver`** est l'interface d'évaluation, avec deux implémentations et une
fabrique :

| Implémentation | Comportement |
|---|---|
| `ThreadedEvalDriver` | enveloppe `AsyncEvaluator` ; `SetProgressSink` rend **`false`** — il refuse |
| `InlineEvalDriver` | calcule dans `Pump()`, sur le fil appelant |

`MakeEvalDriver(graph, EvalMode)` choisit : `Auto` (un fil si l'hôte en a),
`Threaded`, `Inline`. `HostHasThreads()` répond d'après la **configuration de
compilation**, ce n'est pas une mesure.

`Pump()` doit être appelé **hors frame** par tout hôte. Sur un hôte à fils il ne
fait rien ; sur la cible WebAssembly, c'est le **seul** lieu de calcul.

---

## 6. Le dessin — `canvas/`

**`NodeCanvas::Draw(EditorModel&)`** dessine tout : le graphe plein cadre via
`imgui-node-editor`, la palette et l'inspecteur en panneaux flottants.
`RequestFitToContent()` demande un cadrage au prochain rendu.

Deux en-têtes sont **`inline` et sans ImGui**, donc testables sans lier la cible :

- **`canvas_layout.h`** — `ComputeLayout(displayWidth, displayHeight)`, fonction
  pure. Marge 10, palette 280, inspecteur 320, largeur minimale de graphe 260.
- **`canvas_style.h`** — `NameTint()` dérive une teinte stable d'un nom par
  FNV-1a puis HSV→RGB ; `Luminance()` et `Scaled()` complètent. Les huit cas
  vérifient les vecteurs FNV-1a publiés, l'indépendance au signe de `char`, la
  stabilité de la teinte d'un nom donné, que toute teinte reste dans la bande
  lisible, et qu'aucun couple de types enregistrés ne partage sa teinte — la
  séparation RGB minimale sur les **neuf types de valeurs** vaut **0,172**.

Aucun appel OpenGL dans `canvas/` : l'API graphique n'est choisie que par l'hôte.

---

## 7. Couverture

**257 cas** répartis en 15 suites :

| Suite | Cas | Suite | Cas |
|---|---:|---|---:|
| `flow` | 39 | `nodes_shapes` | 18 |
| `ui` | 31 | `nodes_analysis` | 17 |
| `graph` | 25 | `signature` | 13 |
| `nodes` | 22 | `validate` | 12 |
| `evaluator` | 20 | `concurrency` | 9 |
| `serialize` | 18 | `runner` | 9 |
| `async` | 8 | `canvas_layout` | 8 |
| `canvas_style` | 8 | | |

Trois propriétés ne se prouvent que par `runner`, une fois le document relu : un
puits ne s'exécute que sur demande nommée, un nom de fichier produit est stable
d'une exécution à l'autre, et une source relue réinterroge son fichier avant que
sa signature ne serve d'index.

---

## 8. Limitations connues

### 8.1 Moteur

**Le budget de cache est une valeur unique.** 256 Mio par défaut ne convient à
aucun des deux hôtes : trop pour la cible WebAssembly, arbitraire pour l'hôte
natif. Rien dans le code ne le rappelle au moment de construire un évaluateur.

**Le budget peut être dépassé.** Une entrée seule plus grosse que le budget
survit, par construction (§3.7). Il n'existe aucun signalement de ce dépassement.

**`RemoveNode` laisse des identifiants creux.** La suppression retire le nœud et
ses liens ; l'identifiant n'est pas réattribué.

**Le paramètre `Driven` est déclaré, jamais évalué.** `ParamKind::Driven` et
`EvalStatus::DrivenParameter` existent, et l'évaluateur refuse un nœud qui en
porte un. Aucun évaluateur d'expression n'est branché.

**Aucun contrôle de réentrance sur le cache.** L'exclusion porte sur la pré-passe
(§3.6). Le reste du parcours suppose qu'un seul évaluateur travaille sur un
graphe donné.

### 8.2 Concurrence

**ThreadSanitizer ne tourne pas.** La chaîne Linux ne le pose pas. Les 9 cas de
`concurrency` exercent le contrat, pas les courses réelles.

**`HostHasThreads()` n'est pas une mesure.** Elle rend la configuration de
compilation. Un hôte compilé avec `-pthread` mais incapable de démarrer un fil au
lancement obtiendrait `true` puis une exception.

### 8.3 Interface

**`imgui-node-editor` 0.9.3 laisse son rectangle de découpe en coordonnées
locales.** Le canvas reçoit correctement la taille de sa fenêtre ; le défaut est
en amont, dans la copie versionnée. Aucun correctif n'est appliqué localement.

**Le dessin n'a aucun test.** `canvas_layout` et `canvas_style` sont couverts (16
cas) parce qu'ils sont `inline` et purs. `node_canvas.cpp`, 1 014 lignes, ne
l'est pas — aucune suite ne lie `cggraph_canvas`.

**`ThreadedEvalDriver::SetProgressSink` refuse.** Il rend `false` : la progression
n'est pas relayée depuis un fil. Seul `InlineEvalDriver` l'accepte.

### 8.4 Adaptateurs

**Les `caveat` du catalogue sont des limites d'algorithme, pas des bogues.**
Ils sont portés à l'écran par la palette. Exemples relevés : `mesh.io.load` — le
code de retour d'`mesh_io_obj.cpp` ne suffit pas à décider du succès ;
`mesh.smooth.laplacian` — une passe, bord préservé sans condition ;
`mesh.simplify` — `maxError` est un proxy du coût QEM, pas une borne de
Hausdorff ; `mesh.ambient_occlusion` — une face d'aire nulle garde une occlusion
de 0 que le corps ne distingue pas d'une face non occluse ; `mesh.align.icp` —
une entrée inutilisable rend un résultat par défaut.

**Ces limites viennent de `cgmesh`, pas des adaptateurs.** Les corriger relève de
la bibliothèque de domaine.

### 8.5 Construction

**`cggraph-boilerplate` a pour défaut `Off`.** Il n'est bâti que par le job
`ci-linux-boilerplate` et par les machines qui posent l'option. Le canvas n'a
donc qu'un hôte bâti dans la configuration par défaut.

**Aucun job CI ne bâtit la cible WebAssembly.** `maker` se construit à la main.

---

## 9. Améliorations possibles

Par ordre de rapport valeur / coût estimé.

### 9.1 Peu coûteuses

**Un budget de cache par hôte.** Poser la valeur au point de construction de
l'évaluateur plutôt qu'un défaut unique. Le point de décision existe déjà —
`MakeEvalDriver` connaît le mode, donc l'hôte.

**Signaler le dépassement de budget.** Ajouter un compteur à `CacheStats` quand
une entrée survit parce qu'elle est seule. Aujourd'hui le dépassement est
silencieux.

**Un job CI pour la cible WebAssembly.** Le second hôte du canvas n'est vérifié
par aucune chaîne.

**ThreadSanitizer sur la chaîne Linux.** Le contrat de concurrence est décrit et
testé ; les courses ne le sont pas.

### 9.2 Coût moyen

**Tester le dessin.** `node_canvas.cpp` est la plus grosse unité non couverte du
module. Une approche possible : extraire davantage de logique pure vers des
en-têtes `inline` — c'est ce qui rend `canvas_layout` et `canvas_style` testables
sans ImGui.

**Relayer la progression depuis un fil.** `ThreadedEvalDriver::SetProgressSink`
refuse aujourd'hui. Une file de messages entre le fil de calcul et le fil
d'interface lèverait la limite ; le type `AsyncEvaluator::Progress` existe déjà.

**Corriger le découpage d'`imgui-node-editor` en amont.** Le défaut est dans la
copie versionnée sous `extern/`. Un correctif local en ferait un fork à
réappliquer à chaque montée de version — d'où le choix actuel de ne rien toucher.

### 9.3 Ouvertures de conception

**Évaluer les paramètres `Driven`.** Toute la déclaration est en place : le type,
le statut de refus, la sérialisation de l'expression. Il manque l'évaluateur
d'expression et la définition de ce qu'une expression peut lire.

**Étendre `Context` sans rouvrir les signatures.** L'interface est dans `cgmath`
et ne porte qu'`IsAborted()`. Une graine déterministe s'ajouterait par une
méthode virtuelle **non pure**, `virtual unsigned int GetSeed() const { return 0; }`,
sans toucher aucune des signatures qui prennent un `const Context*`. Deux
réserves : le contexte est optionnel, donc le déterminisme ne vaudrait que
lorsqu'un contexte est fourni ; et une méthode **pure** casserait les
implémenteurs existants.

**Réattribuer les identifiants libérés.** Suppose de décider ce qu'un document
déjà écrit devient — les identifiants y sont repris tels quels.

---

## 10. Points d'attention pour le développement

### 10.1 Ce qu'il ne faut pas faire

**Ne jamais nommer `cgmesh`, `cgimg` ou `cgmath` dans `core/`**, y compris dans un
commentaire ou un exemple. L'instrument du critère de couche est un grep de
texte : un exemple de `TypeDesc::name` valant `"cgmesh.Mesh"` le fait échouer.

**Ne pas lire un paramètre `Internal` depuis `Compute`.** Il est écrit par
`RefreshExternalState`, pendant la pré-passe, éventuellement depuis un autre fil.
Ce serait la seule lecture de paramètre concurrente d'une écriture. Un nœud qui a
besoin de son état extérieur pendant le calcul le relève lui-même.

**Ne pas séparer le relevé de l'état extérieur du figeage des signatures.** Les
deux forment la pré-passe et sont sous le même verrou (§3.6).

**Ne pas ajouter de champ au document sans ses trois consommateurs** : le format
qui l'écrit et le relit, la signature qui le prend en compte s'il change le
calcul, et l'interprète qui l'utilise. Un champ écrit et jamais lu est un piège.

**Ne pas mettre un nœud `sideEffect` en cache**, et ne pas l'exécuter sans demande
nommée.

**Ne pas appeler `Pump()` pendant une frame.** Sur l'hôte natif l'appel ne fait
rien ; sur la cible WebAssembly, c'est le seul lieu de calcul. L'omettre donne un
hôte qui marche en natif et ne calcule jamais sur le web.

### 10.2 Ajouter un type de nœud

1. Écrire l'adaptateur sous `nodes/<famille>/`, avec son `NodeDesc` — nom de
   type, version, ports typés, `sideEffect` si le nœud écrit.
2. Poser les paramètres dans le constructeur, avec leur `ParamRole` et leur
   `ParamVisibility` explicites.
3. Enregistrer l'entrée dans `nodes/catalog.cpp`, avec son `caveat` si
   l'algorithme sous-jacent en porte un.
4. Si le nœud transporte une forme nouvelle, enregistrer son `TypeDesc` dans
   `value_types.h` plutôt que de réutiliser un type voisin.

⚠ `ParamSet::SetInt` et ses variantes **reposent le rôle à chaque appel**, avec
`Semantic` par défaut. Un appel ultérieur qui omet le rôle écrase un
`NonSemantic` posé à la construction.

### 10.3 Ajouter un hôte

Un hôte fournit la fenêtre, le contexte graphique et la boucle. Il ne dessine
aucun widget. `cggraph-boilerplate/main.cpp` est le gabarit ; deux de ses choix
ne se transportent pas et sont signalés sur place :

- `IniFilename = nullptr` — justifié par le fait que les positions appartiennent
  au document du graphe ;
- l'appel à `Pump()` — sans effet sur cet hôte, indispensable sur l'autre.

L'hôte lie l'implémentation OpenGL lui-même : `find_package(OpenGL REQUIRED)` puis
`OpenGL::GL`, cible importée qui vaut `opengl32` sous Windows et `libGL` sous
Linux. Le backend `imgui_impl_opengl3` ne suffit pas — il charge ses propres
pointeurs de fonction et n'exige rien à l'édition de liens.

### 10.4 Vérifications avant de livrer

```
# contraintes de couche
grep -rn "cgmesh\|cgimg\|cgmath" src/cggraph/core        # doit rendre 0
grep -rn cggraph src/cgmesh src/cgmath src/cgimg         # doit rendre 0

# suite complete
cd build/<preset>/test/test-run && ../Release/TU.exe

# le second hote du canvas
cmake --preset ci-linux-boilerplate && cmake --build --preset ci-linux-boilerplate
```

Les tests écrivent leurs sorties sous des noms relatifs nus et lisent leurs
entrées sous `./test/data/…` : ils doivent être lancés depuis `test-run`, sinon
les sorties se déversent dans le répertoire courant du lanceur.
