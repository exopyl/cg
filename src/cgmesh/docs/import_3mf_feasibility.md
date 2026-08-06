# Faisabilité — import 3MF dans cgmesh via lib3mf

**Date** : 2026-08-06
**Périmètre** : `src/cgmesh` (cœur IO). Consommateurs visés : `sinaia`, `sulina`,
`reconstruction-cli`. **`maker` (WASM) hors périmètre** — décision utilisateur, cf. §4.
**Méthode retenue en entrée** : lib3mf (3MF Consortium), v2.5.0 du 2025-02-24.
**Nature** : analyse seule, aucun code d'implémentation écrit.

---

## 1. Cadrage

Aucun support 3MF n'existe aujourd'hui, ni en import ni en export. Les deux seules
mentions dans le dépôt sont des intentions :

- `src/cgmesh/zip_manager.h:12` — « le 3MF, qui EST une archive OPC (plusieurs parties
  XML), quand il viendra »
- `src/cgmesh/CMakeLists.txt:34` — « […] et servira au 3MF »

Le catalogue actuel (`sinaia/SupportedFormats.h`) compte 18 extensions : `3dm`, `3ds`,
`asc`, `glb`, `gltf`, `iges`, `igs`, `kvx`, `nbt`, `npts`, `obj`, `off`, `ply`, `pset`,
`pts`, `step`, `stl`, `stp`.

**Décisions utilisateur actées avant rédaction du plan** (cf. §4) :
1. **Desktop seulement** — `maker` reste inchangé.
2. **Parité avec l'import 3dm** — objets maillés + build items + transformations +
   composants. Matières et couleurs reportées.

---

## 2. Ce qu'apporte lib3mf — vérifié sur les artefacts téléchargés

Constats établis en inspectant `lib3mf-2.5.0-Windows.zip`, `lib3mf_sdk_v2.5.0.zip` et
`lib3mf-wasm-2.5.0.zip`, **pas** par lecture de documentation.

| Point | Constat |
|---|---|
| Licence | **BSD-2-Clause** — permissive, compatible |
| Windows | `bin/lib3mf.dll` (2,53 Mo) + `lib/lib3mf.lib` (189 Ko) |
| SDK multiplateforme | `Bin/lib3mf.dll`, `Bin/lib3mf.so` (5,3 Mo), `Bin/lib3mf.dylib` (7,9 Mo) |
| Statique | **Aucune** — pas de `.a`/`.lib` statique publié, uniquement des bibliothèques partagées |
| Bindings C++ | En-têtes autogénérés au-dessus d'une API C, en deux variantes : `Cpp/lib3mf_implicit.hpp` (lien sur l'import lib) et `CppDynamic/lib3mf_dynamic.hpp` (chargement par `LoadLibrary`/`dlopen`) |
| Dépendances embarquées | libzip 1.10.1, **zlib 1.3.1**, cpp-base64, fast-float |

### API de lecture

```cpp
PModel model = wrapper->CreateModel();
PReader reader = model->QueryReader("3mf");
reader->ReadFromFile(path);              // ou ReadFromBuffer(vector<uint8>)

PMeshObjectIterator it = model->GetMeshObjects();
while (it->MoveNext()) {
    PMeshObject o = it->GetCurrentMeshObject();
    std::vector<sPosition> verts;   o->GetVertices(verts);
    std::vector<sTriangle> tris;    o->GetTriangleIndices(tris);
}
PBuildItemIterator bi = model->GetBuildItems();   // placement + transformations
```

**Le mapping vers cgmesh est quasi gratuit** — c'est le point fort de cette piste :

| lib3mf | cgmesh | Conversion |
|---|---|---|
| `sPosition { float m_Coordinates[3] }` | `std::vector<float> Mesh::m_pVertices` (3 flottants/sommet, contigu) | **layout identique → `memcpy`** |
| `sTriangle { uint32 m_Indices[3] }` | `Face` via `SetNVertices(3)` + `SetVertex` | boucle triviale ; patron existant à `mesh_io_3dm.cpp:215-238` |
| `GetVertices` / `GetTriangleIndices` | — | accès **en bloc**, pas sommet par sommet |

---

## 3. Cartographie : réutilisable / manquant

### Réutilisable tel quel — l'essentiel de l'ossature existe

| Brique | Emplacement | Rôle |
|---|---|---|
| Point de dispatch | `vmeshes_io.cpp:71-73` (`ext == "3dm"`) | ajouter le bloc `"3mf"` juste après |
| Déclaration | `vmeshes_io.h:24` (`import_3dm`) | ajouter `import_3mf` à côté |
| Patron de dépendance optionnelle | `vmeshes_io_occt.cpp:35` / `:308-324` | `#ifdef CG_HAS_OCCT` … `#else` stub renvoyant `false` + message. Modèle exact à reproduire |
| Patron CMake | `src/cgmesh/CMakeLists.txt:112-129` (OpenNURBS) | `ENABLE_*` + `EXTERN_*` + `FATAL_ERROR` de garde + `CG_HAS_*` + lien par plateforme |
| Copie de DLL | `test/CMakeLists.txt` (OpenNURBS, OCCT, ONNX) | `copy_if_different` post-build |
| Accumulation des maillages | `vmeshes.h:15` `AddMesh(Mesh*)` | un `Mesh` par objet 3MF |
| Catalogue applicatif | `sinaia/SupportedFormats.h` | ajouter `3mf` |

Trois dépendances externes suivent déjà ce patron (OpenNURBS, OCCT, ONNX Runtime) : lib3mf
est un quatrième cas **strictement identique**, ce qui est l'argument principal de
faisabilité.

### Manquant — à écrire

1. **`vmeshes_io_3mf.cpp`** (nouveau) : `VMeshesIO::import_3mf`, gardé `CG_HAS_LIB3MF`,
   avec stub de repli. Estimation : ~150-250 lignes, dont l'essentiel en résolution de
   build items / composants.
2. **Application d'une transformation affine à un `Mesh`** — *le seul vrai manque*.
   `mesh.h:396-397` n'offre que :
   ```cpp
   void transform (float mrot[9]);
   void transform (const Matrix3f &m);
   ```
   Aucune surcharge ne porte de **translation**, alors que `sLib3MFTransform` est un
   `float[4][3]` = partie linéaire 3×3 **+ ligne de translation**. Il faut soit ajouter
   `Mesh::transform(const Matrix3f&, const Vector3f&)`, soit translater les sommets dans
   l'importeur. La première option est préférable (réutilisable, testable isolément), mais
   elle touche `mesh.h` — donc recompile les 50 fichiers qui l'incluent.
3. **Wiring CMake** : `ENABLE_LIB3MF` / `EXTERN_LIB3MF` / `CG_HAS_LIB3MF` + copies de
   bibliothèque partagée pour `TU`, `sinaia`, `sulina`.
4. **Tests** : fichiers `.3mf` de référence dans `test/data/`, avec oracles (nombre
   d'objets, de sommets, de faces, volume signé pour valider une transformation appliquée).

### Risques identifiés

**🔴 Collision zlib — hypothèse à vérifier par exécution, pas un fait constaté.**
lib3mf embarque zlib 1.3.1 ; `cgmesh` vendorise zlib 1.3.2. La note projet du dépôt
documente que cette zlib **doit** être compilée avec `Z_PREFIX`, faute de quoi les tests
3dm *segfaultent sur la CI Linux* par interposition de symboles. Le mécanisme est
exactement le même ici.

- Sur **Windows**, `lib3mf.dll` n'exporte que ses propres symboles : risque faible.
- Sur **Linux**, `lib3mf.so` peut exposer les symboles zlib et entrer en interposition
  avec la copie de `cgmesh` : **c'est le scénario déjà rencontré dans ce dépôt**.
- **Protocole de vérification** : après intégration, lancer la suite complète sous Linux
  (`ctest --preset ci-linux`) et vérifier en particulier les tests 3dm. En cas de
  segfault, inspecter `nm -D lib3mf.so | grep -E ' T (inflate|deflate|crc32)'`.

**🟡 Aucune bibliothèque statique publiée.** Impose de livrer `.dll`/`.so`/`.dylib` à côté
de chaque exécutable. Sans conséquence : c'est déjà le régime d'OpenNURBS, OCCT (≈48 DLL)
et ONNX Runtime, et l'outillage de copie existe.

**🟡 Version des artefacts.** v2.5.0 date de février 2025. Les binaires Windows sont
construits avec une toolchain MSVC donnée ; à valider avec MSVC 2026 (v14.50) utilisé ici.
Non vérifié — aucune compilation n'a été tentée dans cette analyse.

---

## 4. Pistes d'intégration explorées

Conformément à la discipline de la recette, les pistes écartées sont conservées.

### 4.1 Cible WASM / `maker`

| Piste | Principe | Décision |
|---|---|---|
| **A. Desktop seulement** | 3MF dans cgmesh, `maker` inchangé | ✅ **RETENUE** |
| B. Module WASM séparé | Charger `lib3mf.js` + `lib3mf.wasm` en JS à côté de `maker.wasm`, marshalling des sommets/triangles vers le tas | ❌ écartée |
| C. lib3mf statique sous emcc | Compiler lib3mf + libzip + zlib avec emsdk, lier dans `maker.wasm` | ❌ écartée |

**Justification du choix.** Trois faits ont porté la décision :
1. **`maker` n'a aujourd'hui aucune page d'import de maillage** — ses seuls
   `<input type="file">` acceptent images, SVG et JSON. L'app *génère* des maillages.
2. L'artefact `lib3mf-wasm-2.5.0.zip` est un **module Emscripten autonome à API embind**
   (`lib3mf.js` 153 Ko + `lib3mf.wasm` 2,33 Mo), **pas** une bibliothèque linkable. La
   piste B triplerait le poids téléchargé (`maker.wasm` fait 1,09 Mo aujourd'hui) et
   imposerait deux runtimes Emscripten en mémoire plus une frontière de données en JS.
3. La piste C est un chantier de build à part entière (3 dépendances sous emcc, aucune
   `.a` publiée donc build maison à maintenir à chaque montée de version) et c'est là que
   la collision zlib serait la plus probable — `ENABLE_ZLIB` vaut **OFF** sur le preset
   `maker-wasm`.

Le code étant gardé par `CG_HAS_LIB3MF`, rouvrir la piste B ou C plus tard ne coûte rien
de ce qui aura été fait.

### 4.2 Mode de liaison desktop

| Piste | Principe | Décision |
|---|---|---|
| **Implicite** (`Cpp/lib3mf_implicit.hpp`) | Lien sur `lib3mf.lib` ; la bibliothèque partagée doit être présente au lancement | ✅ **RETENUE** |
| Dynamique (`CppDynamic/lib3mf_dynamic.hpp`) | `LoadLibrary`/`dlopen` au premier usage ; l'absence de la lib est rattrapable à l'exécution | ❌ écartée |

**Justification.** L'implicite est le régime des trois dépendances existantes : cohérence
et zéro code de chargement. La piste dynamique a un avantage réel — 3MF deviendrait
optionnel *à l'exécution*, sans recompilation — mais elle introduit un mode de défaillance
nouveau dans un dépôt qui n'en a aucun de ce type, pour un bénéfice que `CG_HAS_LIB3MF`
couvre déjà à la compilation. **À reconsidérer** si l'on veut un jour distribuer sinaia
sans imposer la présence de `lib3mf.dll`.

### 4.3 Point d'entrée dans cgmesh

| Piste | Principe | Décision |
|---|---|---|
| **`VMeshes` / `vmeshes_io.cpp`** | Un `Mesh` par objet 3MF, multi-objets | ✅ **RETENUE** |
| `MeshIO` / `mesh_io.cpp` | Un seul `Mesh` fusionné | ❌ écartée |

**Justification.** Le 3MF est multi-objets par nature (build items + composants), et c'est
le chemin qu'empruntent déjà `sinaia` et `sulina`, qui manipulent des `VMeshes`. C'est
aussi le chemin de `import_3dm`, dont on vise la parité. `MeshIO` imposerait une fusion
destructive et perdrait le placement.

---

## 5. Plan incrémental

Chaque étape est livrable et vérifiable seule.

### Étape 1 — Câblage et squelette (fondation)
- `ENABLE_LIB3MF` / `EXTERN_LIB3MF` / `CG_HAS_LIB3MF` dans `src/cgmesh/CMakeLists.txt`, en
  copiant le bloc OpenNURBS (`:112-129`), gardes `FATAL_ERROR` comprises.
- Copies de bibliothèque post-build pour `TU`, `sinaia`, `sulina`.
- `vmeshes_io.h` : déclarer `import_3mf`. `vmeshes_io_3mf.cpp` : squelette avec `#ifdef` et
  **stub de repli** calqué sur `vmeshes_io_occt.cpp:308-324`.
- Dispatch `ext == "3mf"` dans `vmeshes_io.cpp`, après le bloc 3dm.
- **Critère de sortie** : le projet compile avec `ENABLE_LIB3MF` ON *et* OFF ; les 1 098
  tests restent verts dans les deux configurations.

### Étape 2 — MVP géométrie
- `ReadFromFile` + itération `GetMeshObjects()` ; par objet : `GetVertices` → `memcpy` dans
  `m_pVertices`, `GetTriangleIndices` → `Face`.
- **Critère de sortie** : un 3MF mono-objet s'ouvre dans sinaia avec le bon nombre de
  sommets et de faces. Premier test avec oracles chiffrés.

### Étape 3 — Parité 3dm (l'objectif retenu)
- Ajouter `Mesh::transform(const Matrix3f&, const Vector3f&)` (ou équivalent), **testé
  isolément avant usage**.
- Parcourir `GetBuildItems()`, résoudre `GetObjectResource()` (maillage **ou** composants,
  récursivement), composer et appliquer les transformations.
- **Critère de sortie** : un assemblage multi-pièces s'ouvre correctement placé. Oracle
  recommandé : volume signé invariant par transformation rigide, et barycentre attendu
  après translation.

### Étape 4 — Vérification multiplateforme (non optionnelle)
- Build + tests **Linux** : c'est là que la collision zlib se manifesterait.
- Build `sulina` et `sinaia` sous Windows.
- **Ne pas se fier aux seuls tests** : `sinaia`, `sulina`, `cgre` et `maker` ne sont
  couverts par aucun test et ne sont pas construits en CI. Les construire explicitement.

### Reporté hors périmètre
Matières, couleurs et textures (le modèle de propriétés 3MF est **par triangle**, cgmesh
porte des couleurs **par sommet** : la correspondance demande une vraie conception) ;
export 3MF (`ZipManager` est en écriture seule et sans compression — il conviendrait pour
produire l'archive OPC, mais c'est un autre sujet) ; extensions 3MF (beam lattice,
production, slice).

---

## 6. Trade-offs à assumer

| Décision | Gain | Coût / risque |
|---|---|---|
| lib3mf plutôt qu'un lecteur maison | Implémentation de référence du consortium, spec couverte, BSD-2 | 4ᵉ dépendance binaire externe ; ~2,5 Mo par plateforme ; risque zlib sur Linux |
| Desktop seulement | Aucun impact sur le poids du bundle web ni sur la chaîne emcc | `maker` ne lira pas de 3MF sans rouvrir le sujet |
| Liaison implicite | Cohérent avec l'existant, zéro code de chargement | La bibliothèque partagée doit être présente au lancement, sinon échec au démarrage |
| Parité 3dm (pas MVP seul) | Les assemblages sont correctement placés | Impose de toucher `mesh.h` (translation), donc recompilation large |
| Matières reportées | Périmètre tenable | Les 3MF colorés s'ouvriront sans leurs couleurs |

---

## 7. Non examiné — déclaration explicite

- **Aucune compilation n'a été tentée.** Ni lib3mf, ni son intégration. Tout ce document
  repose sur la lecture du code du dépôt et sur l'**inspection du contenu des artefacts
  téléchargés**. La compatibilité binaire de `lib3mf.lib` v2.5.0 avec MSVC 2026 (v14.50)
  est **non vérifiée**.
- **La collision zlib est une hypothèse**, fondée sur un précédent documenté dans ce dépôt,
  pas sur une reproduction. À router vers le sous-agent `debugger` si elle se manifeste.
- **Le contenu du `.so` Linux n'a pas été inspecté** (seuls les artefacts Windows et WASM
  ont été décompressés) : la question de l'exposition des symboles zlib reste ouverte.
- **Les performances n'ont pas été évaluées.** L'accès en bloc (`GetVertices`) et
  l'identité de layout suggèrent un import rapide, mais aucune mesure n'a été faite.
- **La spec 3MF n'a pas été auditée** au-delà des besoins de la géométrie et du placement.
  La couverture réelle de lib3mf pour les fichiers produits par les trancheurs courants
  (PrusaSlicer, Bambu Studio, Cura) n'a pas été testée — ces outils émettent des
  extensions propriétaires dont on ignore ici l'impact sur la lecture du cœur.
- **L'export 3MF n'a pas été instruit.**

---

## 8. Handoff

Analyse seule, aucun code d'implémentation écrit (frontière `architect` / `developer`).
L'implémentation passe par le sous-agent `developer`, puis `/code-review` obligatoire avant
conclusion. Les défaillances supposées (collision zlib) passent d'abord par `debugger`
pour reproduction.
