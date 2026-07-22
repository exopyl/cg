# `maker` — Analyse d'une interface web pour cgmesh (Online3DViewer + Emscripten)

> Statut : analyse de faisabilité / conception. Aucun code produit.
> Date : 2026-07-15. Cible : nouveau répertoire `maker/` à la racine du dépôt.

## 1. Objectif

Créer une application **web** dans `maker/`, sur le modèle fonctionnel de **sinaia**, mais dont
le moteur de rendu 3D est **[Online3DViewer](https://github.com/kovacsv/Online3DViewer)** (o3dv,
MIT, basé sur three.js) embarqué dans une page HTML. L'interface offre :

1. un **panneau de paramètres** pour générer/éditer en direct des **surfaces paramétriques** et
   des **L-systèmes**, exactement comme sinaia le fait aujourd'hui via son `PropertyPanel` ;
2. l'**import de fichiers SVG** et leur **extrusion** paramétrable (hauteur, tolérance), comme
   `ParameterizedSvgExtrusion` dans sinaia.

Le cœur géométrique est **cgmesh compilé en WebAssembly via Emscripten**. Le navigateur ne fait
que piloter des paramètres et afficher le maillage résultant ; toute la géométrie est calculée
par le C++ existant.

---

## 2. Ce qui existe déjà et se réutilise tel quel

L'analyse du dépôt montre que **la quasi-totalité de la logique métier existe** et est déjà
découplée de l'UI. C'est le point décisif de faisabilité.

| Brique | Fichier(s) | Réutilisable en WASM ? |
|---|---|---|
| Interface de paramétrage | `src/cgmesh/parameterized.h` (`IParameterized`, `Parameter`) | ✅ tel quel |
| Formes & surfaces paramétriques | `src/cgmesh/parameterized_shapes.{h,cpp}` — **27 classes** (cube, sphère, tore, Klein, hélicoïde, seashell, Möbius, nœuds, Menger, …) | ✅ tel quel |
| Import + extrusion SVG | `src/cgmesh/import_svg.{h,cpp}` (`import_svg_extruded`, `SvgExtrudeOptions`) + `ParameterizedSvgExtrusion` | ✅ tel quel |
| L-systèmes | `src/cgmesh/lsystem.{h,cpp}`, `lsrule.*`, `lsysteminit.*`, `lsystems_def.txt` | ✅ (wrapper à ajouter, cf. §7) |
| Tesselation 2D | `extern/glutess/*.c` (C vendored) | ✅ compile en WASM |
| Parsing SVG | `extern/nanosvg` (header-only) | ✅ |
| Maillage & export | `src/cgmesh/mesh.{h,cpp}`, `mesh_io.{h,cpp}` → OBJ, STL (ASCII/bin), PLY, OFF, DAE, 3DS… | ✅ (adaptation IO, cf. §9.3) |
| Modèle de panneau (référence UI) | `sinaia/PropertyPanel.cpp` | ↦ à **réécrire en JS/HTML** |

**Chaîne de dépendances du cœur** (à compiler) :
`cgmath` → `cgimg` → `cgmesh`. Toutes les dépendances lourdes et problématiques pour le web
(**OpenNURBS, OCCT, ONNX Runtime, PoissonRecon, zlib**) sont derrière des flags `ENABLE_*`
**OFF par défaut** : on ne les active pas pour `maker`. Restent uniquement des dépendances
header-only / sources vendored (glutess, nanosvg, stb, nlohmann, tinygltf) — toutes
compatibles Emscripten.

### Le pattern `IParameterized` (clé de voûte)

```cpp
class IParameterized {
  virtual std::vector<Parameter> GetParameters() = 0; // liste typée (INT/FLOAT/BOOL/ENUM + min/max/choices)
  virtual void Regenerate() = 0;                       // reconstruit le Mesh depuis les valeurs courantes
  virtual std::string GetName() const = 0;
  virtual Mesh* TakeMesh();                            // transfert du maillage produit
};
```

Dans sinaia, `PropertyPanel::Rebuild()` génère un widget par `Parameter`
(INT/FLOAT→spinner, BOOL→checkbox, ENUM→combo), et `OnPropertyChanged()` pousse la nouvelle
valeur puis appelle `Regenerate()` et rafraîchit la vue. **`maker` reproduit exactement ce
flux, mais le panneau est en HTML et `Regenerate()` s'exécute dans le WASM.**

⚠️ **Point d'attention** : `Parameter` stocke des **pointeurs bruts** (`int*`, `float*`, `bool*`)
vers les membres de l'objet C++. Ces pointeurs **ne traversent pas** la frontière JS↔WASM. Le
pont (§4) sérialise la liste en JSON pour le JS et applique les modifications *côté C++* par
nom de paramètre — la couche JS ne touche jamais un pointeur.

---

## 3. Architecture cible

```
┌──────────────────────────── Navigateur ────────────────────────────┐
│                                                                     │
│  index.html  ─────────────────────────────────────────────────┐    │
│   ┌─────────────────────┐   ┌───────────────────────────────┐ │    │
│   │  Panneau paramètres │   │   Online3DViewer (o3dv)       │ │    │
│   │  (HTML/JS)          │   │   EmbeddedViewer + three.js   │ │    │
│   │  - catalogue formes │   │   canvas WebGL                │ │    │
│   │  - sliders / combos │   │                               │ │    │
│   │  - import SVG        │   │   ▲ charge un Blob mesh        │ │    │
│   └─────────┬───────────┘   └───────────────┬───────────────┘ │    │
│             │ set(param,val)                 │ File/Blob (OBJ/GLB)  │
│             ▼                                │                 │    │
│   ┌──────────────────────────────────────────┴─────────────┐ │    │
│   │        maker.js  (glue)  ⇄  cgmesh.wasm / cgmesh.js     │ │    │
│   │   Embind : listShapes / create / getParams / setParam  │ │    │
│   │            / regenerate / exportMesh(format) → bytes    │ │    │
│   └────────────────────────────────────────────────────────┘ │    │
│                          Emscripten module                    │    │
│                                                               │    │
└───────────────────────────────────────────────────────────────────┘
        cgmesh (C++) ── cgimg ── cgmath   [compilés statiquement en .wasm]
```

Trois couches nettes :

1. **cgmesh.wasm** — le C++ existant, exposé par une fine API Embind.
2. **maker.js** — colle : appelle le WASM, construit le panneau, transfère le maillage au viewer.
3. **Online3DViewer** — moteur de rendu embarqué (on utilise `EmbeddedViewer`, pas l'appli
   complète 3dviewer.net), affiche le maillage et gère caméra/lumières/orbit.

---

## 4. Le pont WASM (Embind)

Un unique fichier `maker/wasm_api.cpp` (dans `maker/`, **hors** de `src/cgmesh` pour ne pas
polluer la lib) expose une façade. Esquisse d'API (non implémentée) :

```cpp
// Catalogue : renvoie ["Cube","Sphere",...,"SVG extrusion","L-system"]
std::string listShapes();

// Fabrique un IParameterized par nom, le garde dans un registre interne (id).
int createShape(const std::string& name);

// Sérialise GetParameters() en JSON : [{name,type,value,min,max,choices[]}]
std::string getParams(int id);

// Applique une valeur par nom, sans exposer les pointeurs bruts de Parameter.
void setParam(int id, const std::string& paramName, double value);

// Regenerate() puis renvoie le maillage encodé (OBJ texte ou GLB binaire).
emscripten::val regenerate(int id, const std::string& format /*"obj"|"glb"|"stl"*/);

// Import SVG : les octets du fichier arrivent depuis un <input type=file> JS.
int createSvgFromBytes(const std::string& svgText); // écrit en MEMFS puis import_svg_extruded
```

Marshalling :
- **Paramètres** : JSON (via `nlohmann/json` déjà vendored) dans les deux sens → aucun
  pointeur brut ne franchit la frontière.
- **Maillage** : renvoyé en **octets** (typed array JS) — cf. §5 pour le format.
- **Registre d'objets** : une `std::map<int, std::unique_ptr<IParameterized>>` côté C++ ; le JS
  ne manipule que des `id` entiers (pas de pointeurs).

---

## 5. Transfert du maillage vers Online3DViewer

Online3DViewer sait charger un modèle depuis des `File`/`Blob` en mémoire
(`viewer.LoadModelFromInputFiles([...])` / `LoadModelFromFileList`) sans requête réseau.
Le pont produit donc les octets d'un format qu'o3dv importe nativement.

Options, par ordre de préférence :

| Format | Avantages | Inconvénients | Recommandation |
|---|---|---|---|
| **OBJ (texte)** | `export_obj` **existe déjà** ; trivial à générer ; o3dv le lit | verbeux, pas de normales compactes, plus lourd en octets | ✅ **v1** (chemin le plus court) |
| **GLB (glTF binaire)** | compact, normales/matériaux, rapide à parser | export glTF **à écrire** (tinygltf est vendored → faisable) | ✅ **v2** (perf/qualité) |
| **STL binaire** | `export_stl_binary` existe ; compact | pas de normales lissées ni couleurs | fallback |

**Recommandation** : démarrer en **OBJ** (réutilise `export_obj` immédiatement), migrer vers
**GLB** via tinygltf quand la fluidité du live-edit l'exige. Un flux « regenerate → bytes →
`new File([bytes],'m.obj')` → `LoadModelFromInputFiles` » redessine à chaque changement de
slider.

> Optimisation ultérieure possible : bypasser l'IO fichier et pousser directement des
> `BufferGeometry` three.js. Cela couple `maker` aux internes d'o3dv/three — **non recommandé**
> pour la v1 ; le passage par un format standard garde le découplage.

---

## 6. Panneau de paramètres (réécriture JS du `PropertyPanel`)

Miroir direct de `sinaia/PropertyPanel.cpp`, piloté par le JSON de `getParams` :

- `INT` / `FLOAT` → `<input type="range">` + champ numérique (avec `min`/`max` du `Parameter`).
- `BOOL` → `<input type="checkbox">`.
- `ENUM` → `<select>` peuplé par `choices[]`.

Boucle d'édition (identique en esprit au `OnPropertyChanged` de sinaia) :

```
onInput(param, value) → wasm.setParam(id, param, value)
                      → bytes = wasm.regenerate(id, "obj")
                      → viewer.LoadModelFromInputFiles([File(bytes)])
```

Un **débounce** (~30–60 ms) sur les sliders évite de régénérer à chaque pixel. Le temps de
régénération (déjà mesuré dans sinaia via `m_lastRegenMs`) peut être affiché.

Menu « catalogue » = liste renvoyée par `listShapes()` (les 27 formes + SVG + L-system),
transposant les entrées `ID_GEOMETRY_NEW_PARAM_*` du menu wxWidgets de `SinaiaFrame`.

---

## 7. Pipeline SVG → extrusion

Déjà entièrement implémenté (`import_svg_extruded` + `ParameterizedSvgExtrusion`) :
nanosvg parse → flatten des béziers → tesselation glutess (règle NONZERO, trous gérés) →
extrusion sur +Z → recentrage/normalisation.

Paramètres exposés (déjà définis) : **hauteur d'extrusion** (`m_height`) et **tolérance de
flatten** (`m_flattenTol`). Exactement le comportement demandé (« importer des SVG et les
extruder avec des paramètres de hauteur »).

Adaptation web : le fichier n'est pas lu par `wxFileDialog` mais fourni par un
`<input type="file" accept=".svg">`. Les octets sont écrits dans **MEMFS** (système de fichiers
virtuel Emscripten) puis passés à `import_svg_extruded`, qui attend un chemin. Alternative :
ajouter une surcharge `import_svg_extruded_from_string(const std::string&)` (petit refactor).

---

## 8. Pipeline L-systèmes

`LSystem` (nom, angle, longueur, règles, axiome, `Next()` pour itérer, puis
`ComputeGraphicalInterpretation2D/3D()` produisant un « walk » de points). Le catalogue vit
dans `lsystems_def.txt` (format : nom / axiome / angle / itérations / règles). Les nombreux
`data_generated_*.svg` à la racine (Hilbert, Koch, Dragon, Gosper, Peano, Sierpinski…) sont
des sorties de L-systèmes → **le lien L-système → courbe 2D → SVG → extrusion existe déjà en
pratique** et constitue un pipeline naturel à offrir dans `maker`.

Travail à faire (petit) : `LSystem` **n'implémente pas encore `IParameterized`**. Ajouter un
wrapper `ParameterizedLSystem : ParameterizedMesh` :
- paramètres exposés : choix du système (ENUM depuis `lsystems_def.txt`), nombre d'itérations
  (INT), angle (FLOAT), longueur (FLOAT), et pour la 3D une **épaisseur de tube** (extrusion de
  la courbe en volume affichable) ;
- `Regenerate()` : `Init(axiome)` → N × `Next()` → interprétation graphique → conversion du
  walk en `Mesh` (tube/ruban le long de la polyligne, ou extrusion Z de la courbe fermée).

C'est le seul composant fonctionnel réellement **à ajouter** (tout le reste existe).

---

## 9. Build Emscripten — faisabilité et points durs

### 9.1 Verdict global
**Faisable sans obstacle majeur.** Le cœur est du C++ standard + du C (glutess) + des headers.
Aucune dépendance système native n'est requise une fois les `ENABLE_*` laissés à OFF.

### 9.2 Toolchain
- Installer **emsdk** (`emcc`/`em++`), utiliser le toolchain file
  `emscripten/cmake/Modules/Platform/Emscripten.cmake` via un **preset CMake** dédié
  (`CMakePresets.json` a déjà des presets — en ajouter un `maker-wasm`).
- Un `maker/CMakeLists.txt` construit `cgmath`/`cgimg`/`cgmesh` en objets WASM + `wasm_api.cpp`,
  et lie avec `-lembind`.
- Flags typiques : `-s MODULARIZE=1 -s EXPORT_ES6=1 -s ALLOW_MEMORY_GROWTH=1
  -s FORCE_FILESYSTEM=1 --bind`, `-fexceptions` si le code lève (STL, parsing).

### 9.3 Points à adapter
1. **IO fichier** (`mesh_io` et `import_svg` prennent des `const char* filename`) : sous WASM,
   soit on écrit/lit via **MEMFS** (`FS.writeFile`), soit on ajoute des surcharges
   **string/stream** (`std::ostringstream`). MEMFS = zéro refactor pour la v1 ; surcharges
   stream = plus propre pour la v2. **Reco : MEMFS d'abord.**
2. **Threads** : la mémoire mentionne une parallélisation `std::thread` (travail « épaisseur »
   sur mesh + BVH). Emscripten supporte pthreads (SharedArrayBuffer + en-têtes COOP/COEP), mais
   c'est une complexité inutile pour `maker`. **Reco : compiler en single-thread** (les
   surfaces paramétriques et l'extrusion SVG ne dépendent pas du multithread ; le code doit
   juste tourner mono-thread — vérifier qu'aucun chemin obligatoire ne bloque sans threads).
3. **Exceptions** : activer `-fexceptions` (nanosvg/STL). Coût de taille acceptable.
4. **Libs lourdes** : garder `ENABLE_OPENNURBS/OCCT/ONNX/POISSON/ZLIB = OFF`. Donc **pas** de
   `.3dm`, STEP/IGES, depth-ONNX, Poisson ni NBT/`.nbt` dans `maker` — non requis par le
   périmètre demandé.
5. **cgimg** : compilé avec `PNG`/`JPG` → **stb** (header-only) : OK en WASM. Utile seulement si
   on veut des textures ; sinon on peut même le réduire.
6. **`-fpermissive`** (posé sous UNIX dans `cgmesh/CMakeLists`) : Clang/emcc l'accepte ;
   à vérifier qu'aucun warning promu en erreur ne casse le build web.

### 9.4 Taille & perf
Sans les libs lourdes, le `.wasm` reste modéré (cœur géométrique). Le live-edit est
CPU-géométrie côté WASM (rapide) ; le goulet éventuel = re-parsing du maillage exporté par
o3dv à chaque frame → mitigé par le débounce (§6) et le passage à GLB (§5).

---

## 10. Ce qui ne passe pas / à ne pas faire

- **Pointeurs bruts de `Parameter`** à travers JS → contournés par le marshalling JSON (§4).
- **Rendu OpenGL natif de sinaia** (`wxOpenGLCanvas`, `DrawingArea`) → **abandonné** : c'est
  Online3DViewer/three.js qui rend. On ne réutilise **que** la logique géométrique de cgmesh.
- **Chemins de fichiers absolus** (dialogues wx) → remplacés par `<input type=file>` + MEMFS.
- **Threads obligatoires** → build mono-thread.
- **Formats CAO (.3dm/STEP/IGES)** et **ONNX/Poisson** → hors périmètre v1.

---

## 11. Plan incrémental proposé

| Étape | Livrable | Contenu | Risque |
|---|---|---|---|
| 0 | Toolchain | emsdk installé ; preset CMake `maker-wasm` ; « hello wasm » qui appelle cgmath | Faible |
| 1 | Build cœur | `cgmath`+`cgimg`+`cgmesh` compilés en WASM, mono-thread, libs lourdes OFF | Faible/moyen |
| 2 | Pont Embind | `wasm_api.cpp` : `listShapes/create/getParams/setParam/regenerate("obj")` | Moyen |
| 3 | Viewer | `index.html` avec Online3DViewer `EmbeddedViewer` affichant un OBJ statique produit par le WASM | Faible |
| 4 | Panneau | Panneau HTML piloté par `getParams` (sliders/combos), live-edit d'une sphère/tore | Moyen |
| 5 | Catalogue | Les 27 formes paramétriques câblées (menu = `listShapes`) | Faible |
| 6 | SVG | `<input file>` → MEMFS → `ParameterizedSvgExtrusion` (hauteur, tolérance) | Moyen |
| 7 | L-systèmes | `ParameterizedLSystem` (wrapper à écrire) + catalogue `lsystems_def.txt` | Moyen |
| 8 | Perf/qualité | Export **GLB** (tinygltf), débounce, normales, export utilisateur (télécharger OBJ/STL) | Moyen |

---

## 12. Décisions à trancher (recommandations)

1. **Format de transfert maillage** → **OBJ en v1**, **GLB en v2**. (§5)
2. **IO fichier sous WASM** → **MEMFS en v1**, surcharges stream ensuite. (§9.3)
3. **Threads** → **mono-thread**. (§9.3)
4. **Intégration o3dv** → **`EmbeddedViewer`** dans page custom (pas l'appli 3dviewer.net
   complète), o3dv servi en asset local (vendored dans `maker/vendor/`). (§7 GitHub, MIT)
5. **Emplacement du pont** → `maker/wasm_api.cpp` (ne pas modifier `src/cgmesh`, sauf ajout du
   petit `ParameterizedLSystem` et éventuelles surcharges IO stream, qui bénéficient à toute la
   lib).

---

## 13. Risques

- **Comportement mono-thread** : s'assurer qu'aucun chemin de génération obligatoire n'exige
  `std::thread` (le travail « épaisseur » en dépend, mais il est hors périmètre `maker`).
- **`-fpermissive` / warnings-as-errors** sous emcc : quelques ajustements de compilation
  possibles.
- **IO par chemin** : nanosvg/mesh_io supposent des fichiers ; MEMFS lève le blocage mais
  demande de la rigueur (nettoyage des fichiers virtuels).
- **Fluidité du live-edit** en OBJ (re-parse o3dv) : mitigé par débounce + GLB.
- **Licence** : Online3DViewer et three.js sont **MIT** → compatibles ; vendored dans le repo.

---

## 14. Conclusion

Le projet est **très favorable** : ~90 % de la logique (27 surfaces paramétriques, import+
extrusion SVG, L-systèmes, tesselation, export maillage) **existe déjà** dans cgmesh et est
**découplée de l'UI** grâce à l'abstraction `IParameterized`. Le travail réel se concentre sur
(a) un **pont Embind** mince, (b) un **panneau HTML** rejouant le `PropertyPanel` de sinaia,
(c) l'**intégration Online3DViewer**, et (d) un **petit wrapper `ParameterizedLSystem`**. La
compilation Emscripten est sans obstacle majeur tant qu'on laisse les dépendances lourdes
(OpenNURBS/OCCT/ONNX/Poisson) désactivées et qu'on compile en mono-thread.
</content>
</invoke>
