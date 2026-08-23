## cg

[![Build](https://github.com/exopyl/cg/actions/workflows/build.yml/badge.svg)](https://github.com/exopyl/cg/actions)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=exopyl_cg&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=exopyl_cg)

## Compilation (Build Instructions)

Le projet utilise CMake **avec des presets** (`CMakePresets.json`, version 6, CMake >= 3.25).
Les presets sont la voie normale de compilation : ils fixent le générateur, le type de
build et les options. Cette section documente, pour chaque cible, le preset à utiliser,
les dépendances à fournir et ce qui est réellement produit.

### Vue d'ensemble

| Cible    | Binaire produit           | Preset                                        | Plateforme                   |
| :------- | :------------------------ | :-------------------------------------------- | :--------------------------- |
| `TU`     | `TU.exe` / `TU`           | `local-windows` (Windows), `ci-linux` (Linux) | Windows, Linux               |
| `sinaia` | `sinaia.exe`              | `local-windows`                               | Windows (wxWidgets + OpenGL) |
| `maker`  | `maker.js` + `maker.wasm` | `maker-wasm`                                  | navigateur (Emscripten)      |
| `sulina` | `sulina.exe`              | *(aucun preset — configuration manuelle)*     | Windows (Qt 6 + Vulkan)      |
| `vecna`  | `vecna.exe`               | *(projet CMake autonome)*                     | Windows, Linux (Vulkan)      |

Les presets écrivent tous dans `build/<nom-du-preset>/`.

> **CMake.** La version fournie par Visual Studio suffit (4.3.1 sous VS 18 2026, dans
> `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`). Toute installation
> CMake >= 3.25 convient. `ctest` est livré à côté de `cmake`.

### 1. Windows — premier build

#### 1.1 Dépendances externes

Cinq dépendances binaires sont attendues. Trois viennent de la release `cg_binaries`
(archive `cgbinaries-<version>-windows-x86_64.zip` ; le tag consommé par la CI est dans
`CG_BINARIES_VERSION`, `.github/workflows/build.yml`), déjà dans la disposition attendue :

| Dépendance                    | Contenu                    | Utilisée par           |
| :---------------------------- | :------------------------- | :--------------------- |
| `opennurbs-v8.27.26019.16021` | `include/`, `lib/`         | cgmesh, `TU`, `sinaia` |
| `OCCT-8_0_0`                  | `include/`, `lib/`, `bin/` | cgmesh, `TU`, `sinaia` |
| `lib3mf-2.5.0`                | `include/`, `lib/`, `bin/` | cgmesh, `TU`, `sinaia` |

Les deux autres se récupèrent séparément :

| Dépendance                    | Utilisée par |
| :---------------------------- | :----------- |
| GoogleTest (arbre **source**) | `TU`         |
| wxWidgets 3.3.2               | `sinaia`     |

#### 1.2 `CMakeUserPresets.json` — obligatoire

Le preset `local-windows` de `CMakePresets.json` laisse ses cinq `EXTERN_*` à la valeur
de gabarit `"..."` : **il ne configure pas tel quel**. C'est voulu — les chemins des
dépendances sont propres à chaque machine et n'ont pas à être versionnés.

Créez donc à la racine du dépôt un `CMakeUserPresets.json` — **non versionné**, il est
listé dans `.gitignore` — qui hérite de `local-windows` et renseigne les cinq chemins.
Le nom du preset est libre ; c'est lui qui détermine le répertoire de build
(`build/<nom>/`) :

```json
{
  "version": 4,
  "cmakeMinimumRequired": { "major": 3, "minor": 23, "patch": 0 },
  "configurePresets": [
    {
      "name": "windows-perso",
      "displayName": "Local (Windows) -- chemins de cette machine",
      "inherits": "local-windows",
      "cacheVariables": {
        "EXTERN_OPENNURBS":  "C:/chemin/vers/opennurbs-v8.27.26019.16021",
        "EXTERN_OCCT":       "C:/chemin/vers/OCCT-8_0_0",
        "EXTERN_WXWIDGETS":  "C:/chemin/vers/wxWidgets-3.3.2",
        "EXTERN_GOOGLETEST": "C:/chemin/vers/googletest-1.15.2",
        "EXTERN_LIB3MF":     "C:/chemin/vers/lib3mf-2.5.0"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "windows-perso",
      "configurePreset": "windows-perso",
      "configuration": "Release"
    }
  ]
}
```

> `"configuration": "Release"` dans le **buildPreset** n'est pas décoratif : le générateur
> Visual Studio est multi-configuration, et sans cette ligne le build sort en `Debug`.

#### 1.3 Configurer et compiler

```powershell
cmake --preset windows-perso
cmake --build --preset windows-perso --target TU
cmake --build --preset windows-perso --target sinaia
```

Ce que `local-windows` fixe (`_base` compris) : générateur `Visual Studio 18 2026`,
architecture `x64`, `CMAKE_BUILD_TYPE=Release`, `ENABLE_OPENNURBS=ON`, `ENABLE_OCCT=ON`,
`ENABLE_LIB3MF=ON`, `ENABLE_SINAIA=ON`, `ENABLE_CGNET_DEMO=OFF`.

Sorties :

| Cible    | Chemin                                          |
| :------- | :---------------------------------------------- |
| `TU`     | `build/windows-perso/test/Release/TU.exe`       |
| `sinaia` | `build/windows-perso/sinaia/Release/sinaia.exe` |

Les DLL nécessaires (wxWidgets, `OpenNURBS.dll`, `lib3mf.dll`, et l'intégralité du `bin/`
d'OCCT — une cinquantaine de DLL interdépendantes) sont copiées à côté de chaque
exécutable par des étapes post-build.

### 2. Tests unitaires (`TU`)

**Lancez les tests par `ctest`, et non en exécutant `TU.exe` à la main.**

```powershell
ctest --test-dir build/windows-perso -C Release --output-on-failure
```

Il n'existe pas de testPreset pour Windows — seul `ci-linux` en a un (§4) — d'où la
commande `ctest` explicite ci-dessus, avec `-C Release` puisque le générateur est
multi-configuration.

Raison de la consigne : les tests lisent leurs entrées via `./test/data/...` et écrivent
leurs sorties sous des noms relatifs nus (une cinquantaine de fichiers, plus environ 155
SVG produits par les L-systèmes). Aucun code de test ne choisit son répertoire courant :
il est hérité du lanceur. `test/CMakeLists.txt` définit donc un répertoire de travail
dédié,

    TU_RUN_DIR = build/<preset>/test/test-run

y copie `test/data` au build, et l'impose à la fois à `ctest`
(paramètre `WORKING_DIRECTORY` de `gtest_discover_tests`) et au débogueur
Visual Studio (`VS_DEBUGGER_WORKING_DIRECTORY`).
Lancer `TU.exe` depuis la racine du dépôt « marche » — `./test/data` s'y trouve — mais y
déverse deux cents fichiers de sortie.

`gtest_discover_tests` porte `DISCOVERY_TIMEOUT 60` : la découverte lance `TU.exe` juste
après le dépôt d'une cinquantaine de DLL à côté de lui, et à froid leur chargement dépasse
les 5 s du défaut.

> **Piège `EXTERN_GOOGLETEST`.** Sous Windows, si `EXTERN_GOOGLETEST` est vide,
> `test/CMakeLists.txt` ne lie **aucune** bibliothèque GoogleTest et **n'émet aucune
> erreur de configuration** : l'échec n'apparaît qu'à l'édition de liens. La variable
> désigne un arbre **source** (elle est passée à `add_subdirectory`), pas un binaire
> pré-compilé, et elle n'a **pas** de repli par variable d'environnement. Sous Linux elle
> est ignorée : GoogleTest est trouvé par `find_package`.

> **Build « tests seuls ».** On peut passer `-DENABLE_SINAIA=OFF` par-dessus le preset,
> mais `EXTERN_OPENNURBS`, `EXTERN_OCCT` et `EXTERN_LIB3MF` restent requis — les trois
> options correspondantes sont `ON` dans les presets, et `cgmesh` s'arrête sur un
> `FATAL_ERROR` si le chemin manque. Les désactiver explicitement
> (`-DENABLE_OPENNURBS=OFF` etc.) est l'autre issue.

### 3. sinaia (wxWidgets + OpenGL)

Piloté par `ENABLE_SINAIA` : l'option ajoute `src/cgre` **et** `sinaia` au projet racine.
Elle vaut `OFF` dans `_base` et `ON` dans `local-windows` — un build Windows par preset la
construit donc sans rien de plus (§1.3).

Import `.3dm` (OpenNURBS), STEP/IGES (OCCT) et `.3mf` (lib3mf) selon les `ENABLE_*`
correspondants.

`sinaia/CMakeLists.txt` expose une option propre, `ENABLE_SINAIA_TREATMENTS` (`ON` par
défaut) : la désactiver retire le rattachement du dock « Treatments » à l'AUI, les widgets
restant construits.

### 4. Linux (preset `ci-linux`)

C'est la configuration exécutée par la CI (`.github/workflows/build.yml`) : générateur
Ninja, `TU` seul (`ENABLE_SINAIA` reste `OFF`).

Paquets système :

```bash
sudo apt-get update
sudo apt-get install -y \
  ninja-build \
  zlib1g-dev \
  libgtest-dev libgmock-dev \
  libocct-foundation-dev \
  libocct-modeling-data-dev \
  libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev \
  libtbb-dev
```

OpenNURBS et lib3mf n'ont pas d'équivalent apt : le preset les attend sous
`.deps/cg_binaries/`, extrait de la release `cg_binaries` :

```bash
mkdir -p .deps/cg_binaries
cd .deps
gh release download "$CG_BINARIES_VERSION" \
  --repo exopyl/cg_binaries \
  --pattern 'cgbinaries-*-linux-x86_64.zip'
unzip -q cgbinaries-*-linux-x86_64.zip
mv cgbinaries-*-linux-x86_64/* cg_binaries/
```

OCCT, lui, vient d'apt : sous Unix `cgmesh` le trouve par `find_package(OpenCASCADE)` et
`EXTERN_OCCT` n'est pas utilisé. zlib est également celui du système — le zlib embarqué
d'`extern/` n'est compilé que sous Windows.

Configurer, compiler, tester :

```bash
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --preset ci-linux -j "$(nproc)"
```

Le testPreset `ci-linux` active `--output-on-failure` et échoue si aucun test n'est
découvert. Sortie : `build/ci-linux/test/TU`.

### 5. maker (WebAssembly)

Module WASM exposant la géométrie de `cgmesh` au navigateur via Embind ; les pages de
`maker/web/` le chargent et embarquent Online3DViewer comme moteur de rendu.

Prérequis :

- **`EMSDK`** défini dans l'environnement — le preset en dérive le fichier toolchain
  (`$env{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`). Si la
  variable est vide, CMake résout un chemin tronqué et la configuration échoue sans
  message explicite.
- **`cmake`** et **`nmake`** sur le `PATH` : le preset utilise le générateur
  `NMake Makefiles`, et `nmake` est fourni par Visual Studio.

```powershell
emcmake cmake --preset maker-wasm
cmake --build --preset maker-wasm
```

(`cmake --preset maker-wasm` seul suffit si `emsdk_env` a déjà été appliqué au shell.)

`maker/build.ps1` automatise ce montage : il fixe `EMSDK` et le `PATH`, puis appelle les
deux commandes ci-dessus. Les trois chemins en tête du script (emsdk, cmake, MSVC) sont à
ajuster à votre installation.

**La sortie va dans l'arbre source, pas dans `build/`** : `maker/web/maker.js` et
`maker/web/maker.wasm`, là où `maker/web/index.html` les référence en `./maker.js`. Le
build y copie aussi les ressources par défaut des pages (descripteur gothique, image,
police), depuis `test/data/` — le serveur ne servant que `maker/web/`.

Les modules ES et le `.wasm` exigent HTTP ; `file://` ne fonctionne pas. Un serveur de
développement est fourni :

```powershell
.\maker\serve.ps1        # http://127.0.0.1:8099/
```

Il passe par `maker/serve.py`, qui force `Cache-Control: no-store` : `python -m http.server`
nu resert le `maker.js` de la build précédente après une recompilation.

Le preset `maker-wasm` met toutes les dépendances lourdes à `OFF`, et le `CMakeLists.txt`
racine les force en plus défensivement sous `EMSCRIPTEN`. Les tests ne sont pas construits
pour cette cible.

### 6. sulina (Qt 6 + Vulkan)

**Aucun preset ne couvre sulina** : `ENABLE_SULINA` n'est activé nulle part dans
`CMakePresets.json`. La configuration se fait donc à la main. L'option ajoute `src/cgre2`
et `sulina` au projet racine ; OpenNURBS et OCCT ne sont pas requis.

```powershell
cmake -G "Visual Studio 18 2026" -A x64 `
  -DENABLE_SULINA=ON `
  -DENABLE_OPENNURBS=OFF `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 `
  -S . -B build/sulina

cmake --build build/sulina --config Release --target sulina
```

Exige Qt 6.11 (`find_package(Qt6 6.11 ...)`) et le Vulkan SDK : la variable
d'environnement `VULKAN_SDK` doit être définie, elle fournit `Vulkan::Vulkan` et `glslc`
(compilation des shaders en SPIR-V, avec `FATAL_ERROR` si `glslc` est introuvable).

### 7. vecna (Vulkan)

vecna est un **projet CMake autonome** (`vecna/CMakeLists.txt`) — il ne passe **pas** par
le projet racine, et aucune option `ENABLE_VECNA` n'existe. Il réintègre lui-même
`cgmath`, `cgimg`, `cgmesh` et `cgre2` par `add_subdirectory`.

Prérequis :

- **C++20** (contre C++17 pour le reste du dépôt).
- **Vulkan SDK** : `find_package(Vulkan REQUIRED)`, plus **`glslc`** pour compiler les
  shaders — `FATAL_ERROR` explicite s'il est introuvable.
- **Accès réseau à la première configuration** : GLFW, VMA, Dear ImGui et
  portable-file-dialogs sont récupérés par `FetchContent`.

```powershell
cmake -G "Visual Studio 18 2026" -A x64 -S vecna -B build_vecna
cmake --build build_vecna --config Release --target vecna
```

Si le SDK Vulkan n'est pas trouvé automatiquement, ajouter
`-DVulkan_ROOT="C:/VulkanSDK/<version>"` à la configuration.

Sortie : `build_vecna/Release/vecna.exe`, avec le répertoire `shaders/` (SPIR-V compilé)
copié à côté par une étape post-build — l'application se lance donc depuis n'importe quel
répertoire courant.

vecna a ses propres options, notamment `VECNA_BUILD_TESTS` (`OFF` par défaut).

### 8. Couverture de code

`coverage.sh` (Linux, clang + LLVM) enchaîne configuration, compilation et rapport :

```bash
./coverage.sh
```

Il configure dans `build_coverage` avec `-DENABLE_COVERAGE=On -DENABLE_OPENNURBS=On
-DENABLE_OCCT=On -DENABLE_POISSON=On`, lance `TU` **depuis son `TU_RUN_DIR`** (§2), puis
produit `coverage.txt` et `coverage.info` via `llvm-profdata-18` et `llvm-cov-18`.

`ENABLE_COVERAGE` ajoute par ailleurs une cible CMake `coverage` (`ctest -T Coverage`
suivi de `gcovr`).

### 9. Cas particulier : OCCT hors disposition attendue

`cgmesh` attend `EXTERN_OCCT` pointant vers un dossier contenant `include/`, `lib/` et
`bin/`. C'est **déjà** la disposition livrée par la release `cg_binaries` : dans ce cas il
n'y a rien à préparer.

Certains binaires OCCT récupérés directement en amont ont une autre arborescence
(`install/inc`, `install/win64/vc14/{lib,bin}`). On peut alors fabriquer un dossier de
jonctions, qui ne nécessite pas les droits administrateur :

```powershell
$root   = 'C:\chemin\vers\OCCT-8_0_0_binaries\install'
$layout = 'C:\chemin\vers\occt_layout'
New-Item -ItemType Directory -Force -Path $layout | Out-Null
New-Item -ItemType Junction -Path "$layout\include" -Target "$root\inc"            | Out-Null
New-Item -ItemType Junction -Path "$layout\lib"     -Target "$root\win64\vc14\lib" | Out-Null
New-Item -ItemType Junction -Path "$layout\bin"     -Target "$root\win64\vc14\bin" | Out-Null
```

puis faire pointer `EXTERN_OCCT` sur `occt_layout`.

### Résumé des options CMake

Les colonnes donnent la valeur **effective** dans chaque preset, héritage de `_base`
compris. « — » signifie « non renseigné par le preset », donc valeur par défaut.
`maker-wasm` n'hérite pas de `_base`.

| Option                      | Description                                                    | Défaut | `_base`  | `local-windows` | `ci-linux` | `maker-wasm` |
| :-------------------------- | :------------------------------------------------------------- | :----- | :------- | :-------------- | :--------- | :----------- |
| `ENABLE_SINAIA`             | Application sinaia, et `src/cgre` avec elle                     | `OFF`  | `OFF`    | **`ON`**        | `OFF`      | `OFF`        |
| `ENABLE_SINAIA_TREATMENTS`  | Dock « Treatments » de sinaia (option de `sinaia/`)             | `ON`   | —        | —               | —          | —            |
| `ENABLE_SULINA`             | sulina (Qt 6 + Vulkan), et `src/cgre2` avec lui                 | `OFF`  | —        | —               | —          | `OFF`        |
| `ENABLE_MAKER`              | Module WebAssembly `maker` (Emscripten uniquement)              | `OFF`  | —        | —               | —          | **`ON`**     |
| `ENABLE_OPENNURBS`          | Import `.3dm` (requiert `EXTERN_OPENNURBS`)                     | `ON`   | `ON`     | `ON`            | `ON`       | `OFF`        |
| `ENABLE_OCCT`               | Import STEP/IGES (`EXTERN_OCCT` sous Windows ; apt sous Linux)  | `OFF`  | **`ON`** | `ON`            | `ON`       | `OFF`        |
| `ENABLE_LIB3MF`             | Import `.3mf` (requiert `EXTERN_LIB3MF`)                        | `OFF`  | —        | **`ON`**        | **`ON`**   | —            |
| `ENABLE_ZLIB`               | zlib, requis par le parseur NBT (Minecraft)                     | `ON`   | —        | —               | —          | `OFF`        |
| `ENABLE_POISSON`            | Reconstruction de surface Poisson (`POISSONRECON_DIR`)          | `OFF`  | —        | —               | **`ON`**   | `OFF`        |
| `ENABLE_ONNX`               | Source de profondeur ONNX (requiert `EXTERN_ONNXRUNTIME`)       | `OFF`  | —        | —               | —          | `OFF`        |
| `ENABLE_RECONSTRUCTION_CLI` | Exécutable `reconstruction-cli` (implique `ENABLE_ONNX`)        | `OFF`  | —        | —               | —          | `OFF`        |
| `ENABLE_CGNET_DEMO`         | Exécutable de démo cgnet (Windows)                              | `OFF`  | `OFF`    | `OFF`           | `OFF`      | `OFF`        |
| `ENABLE_COVERAGE`           | Instrumentation pour la couverture de code                      | `OFF`  | —        | —               | —          | —            |

Chemins des dépendances :

| Variable             | Désigne                                                  | Requise quand                                                  |
| :------------------- | :------------------------------------------------------- | :------------------------------------------------------------- |
| `EXTERN_OPENNURBS`   | Racine OpenNURBS (`include/`, `lib/`)                     | `ENABLE_OPENNURBS=ON`, sur les deux plateformes                 |
| `EXTERN_OCCT`        | Racine OCCT (`include/`, `lib/`, `bin/`)                  | `ENABLE_OCCT=ON`, **Windows uniquement**                        |
| `EXTERN_LIB3MF`      | Racine lib3mf (`include/`, `lib/`, `bin/`)                | `ENABLE_LIB3MF=ON`                                              |
| `EXTERN_WXWIDGETS`   | Racine wxWidgets                                          | `ENABLE_SINAIA=ON`                                              |
| `EXTERN_GOOGLETEST`  | Arbre **source** GoogleTest                               | Build de `TU`, **Windows uniquement** (Linux : `find_package`)  |
| `EXTERN_ONNXRUNTIME` | Racine ONNX Runtime                                       | `ENABLE_ONNX=ON`                                                |
| `POISSONRECON_DIR`   | Source PoissonRecon (défaut `extern/PoissonRecon-18.76`)  | `ENABLE_POISSON=ON`                                             |
| `ZLIB_DIR`           | Source zlib (défaut `extern/zlib-1.3.2`)                  | `ENABLE_ZLIB=ON`, Windows uniquement                            |

> `EXTERN_OPENNURBS`, `EXTERN_OCCT`, `EXTERN_LIB3MF` et `EXTERN_ONNXRUNTIME` acceptent un
> repli par variable d'environnement de même nom, utilisé si la variable CMake est vide.
> `EXTERN_WXWIDGETS` et `EXTERN_GOOGLETEST` n'en ont **pas** : elles doivent être passées
> à CMake.

## Génération de la solution Visual Studio

La configuration par preset produit déjà une solution dans le répertoire de build. Avec le
générateur `Visual Studio 18 2026`, c'est **`cg.slnx`** — le format de VS 2026 — et non
`cg.sln` : `build/<preset>/cg.slnx`. Ouvrez-la et utilisez la liste déroulante des
configurations pour basculer entre `Debug` et `Release`.

Le répertoire de travail de débogage de `TU` est déjà réglé sur `TU_RUN_DIR` (§2) : les
tests lancés depuis Visual Studio lisent `test/data` au bon endroit.
