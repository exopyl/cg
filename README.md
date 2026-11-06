## cg

[![Build](https://github.com/exopyl/cg/actions/workflows/build.yml/badge.svg)](https://github.com/exopyl/cg/actions)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=exopyl_cg&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=exopyl_cg)

## Compilation (Build Instructions)

Le projet utilise CMake **avec des presets** (`CMakePresets.json`, version 6, CMake >= 3.25).
Les presets sont la voie normale de compilation : ils fixent le générateur, le type de
build et les options. Cette section documente, pour chaque cible, le preset à utiliser,
les dépendances à fournir et ce qui est réellement produit.

### Vue d'ensemble

| Cible      | Binaire produit           | Preset                                                                                         | Plateforme                   |
| :--------- | :------------------------ | :--------------------------------------------------------------------------------------------- | :--------------------------- |
| `TU`       | `TU.exe` / `TU`           | preset utilisateur héritant de `local-windows`, `ci-windows` (Windows) ; `ci-linux` (Linux)    | Windows, Linux               |
| `sinaia`   | `sinaia.exe`              | preset utilisateur héritant de `local-windows`                                                 | Windows (wxWidgets + OpenGL) |
| `maker`    | `maker.js` + `maker.wasm` | `maker-wasm` (NMake, Windows), `ci-maker-wasm` (Ninja)                                         | navigateur (Emscripten)      |
| `cg_bench` | `cg_bench.exe` / `cg_bench` | aucun preset ne l'active : `-DENABLE_BENCHMARKS=ON` par-dessus un preset (§10)               | Windows, Linux               |
| `sulina`   | `sulina.exe`              | *(aucun preset — configuration manuelle)*                                                      | Windows (Qt 6 + Vulkan)      |
| `vecna`    | `vecna.exe`               | *(projet CMake autonome)*                                                                      | Windows, Linux (Vulkan)      |

Les presets écrivent tous dans `build/<nom-du-preset>/`.

> **CMake.** Sous Windows, le `cmake` de référence est celui de Visual Studio (4.3.1 sous
> VS 18 2026), dans `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/` du répertoire
> d'installation, par exemple
> `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.
> Il n'est **pas** sur le `PATH` d'un shell ordinaire : l'appeler par ce chemin, ou depuis
> une « Developer PowerShell for VS ». Toute installation CMake >= 3.25 convient par
> ailleurs. `ctest` est livré à côté de `cmake`.

### Bibliothèques du dépôt

Chaque bibliothèque du socle a son guide : responsabilité, dépendances et options, usage
minimal tiré d'un test, commande de test.

| Bibliothèque | Guide                                          |
| :----------- | :--------------------------------------------- |
| `cgmath`     | [`src/cgmath/README.md`](src/cgmath/README.md) |
| `cgimg`      | [`src/cgimg/README.md`](src/cgimg/README.md)   |
| `cgmesh`     | [`src/cgmesh/README.md`](src/cgmesh/README.md) |
| `cgarchi`    | [`src/cgarchi/README.md`](src/cgarchi/README.md) |

### Presets

Presets versionnés dans `CMakePresets.json` :

| Preset                 | Rôle                                                                                                                                                                                             |
| :--------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `_base`                | masqué ; socle commun (Release, OpenNURBS et OCCT à `ON`, sinaia et démo cgnet à `OFF`)                                                                                                          |
| `local-windows`        | masqué ; **gabarit** Windows (Visual Studio 18 2026, x64, sinaia, lib3mf) dont les quatre `EXTERN_*` valent `"..."`. Il ne se configure pas tel quel : il est fait pour être hérité (§1.2)        |
| `ci-linux`             | Linux, Ninja, GCC, Release ; tierces sous `.deps/cg_binaries/` et OCCT par apt ; `cgre` et Poisson à `ON` (§4)                                                                                  |
| `ci-linux-asan`        | hérite de `ci-linux` ; Debug, `CG_ENABLE_SANITIZERS=ON` (ASan + UBSan)                                                                                                                           |
| `ci-linux-clang`       | hérite de `ci-linux` ; compilateurs `clang` / `clang++`, le reste inchangé                                                                                                                       |
| `ci-linux-boilerplate` | hérite de `ci-linux` ; `ENABLE_CGGRAPH_BOILERPLATE=ON`, son buildPreset ne construit que `cggraph-boilerplate`                                                                                   |
| `ci-windows`           | Windows ; hérite de `local-windows` et renseigne ses `EXTERN_*` sous `.deps/cg_binaries/` ; Poisson à `ON` ; sinaia, `cgre`, sulina, ONNX, `reconstruction-cli` et canvas ImGui à `OFF` ; wxWidgets non requis |
| `maker-wasm`           | module `maker` sous Emscripten, générateur NMake (§5)                                                                                                                                            |
| `ci-maker-wasm`        | hérite de `maker-wasm` ; générateur Ninja, pour un runner Linux ; son buildPreset ne construit que `maker`                                                                                        |

Presets de build : `ci-linux`, `ci-linux-asan`, `ci-linux-clang`, `ci-linux-boilerplate`,
`ci-windows-debug` et `ci-windows-release` (tous deux sur le configurePreset `ci-windows`,
multi-configuration), `maker-wasm`, `ci-maker-wasm`.
Presets de test : `ci-linux`, `ci-linux-asan`, `ci-linux-clang`, `ci-windows-debug`,
`ci-windows-release` ; tous affichent la sortie des tests en échec et échouent si aucun test
n'est découvert.

Les presets `ci-linux*` et `ci-windows` attendent sous `.deps/cg_binaries/` les binaires
que seule l'étape de téléchargement des workflows dépose : ils sont faits pour la CI. Chacun
est restreint à son système par une condition, `ci-linux*` à Linux et `ci-windows` à
Windows. Pour les rejouer sur un poste, il faut d'abord reproduire cette étape : §4 pour
Linux ; sous Windows, même extraction depuis l'archive `cgbinaries-*-windows-x86_64.zip`
(étape « Download cg_binaries archive » du job `ci-windows` de `build.yml`).

Sur un poste Windows, on ne configure pas `local-windows` lui-même mais un preset
**personnel**, déclaré dans `CMakeUserPresets.json` (§1.2).

### 1. Windows — premier build

#### 1.1 Dépendances externes

Quatre dépendances binaires sont attendues. Trois viennent de la release `cg_binaries`
(archive `cgbinaries-<version>-windows-x86_64.zip` ; le tag consommé par la CI est dans
`CG_BINARIES_VERSION`, `.github/workflows/build.yml`), déjà dans la disposition attendue :

| Dépendance                    | Contenu                    | Utilisée par           |
| :---------------------------- | :------------------------- | :--------------------- |
| `opennurbs-v8.27.26019.16021` | `include/`, `lib/`         | cgmesh, `TU`, `sinaia` |
| `OCCT-8_0_0`                  | `include/`, `lib/`, `bin/` | cgmesh, `TU`, `sinaia` |
| `lib3mf-2.5.0`                | `include/`, `lib/`, `bin/` | cgmesh, `TU`, `sinaia` |

La quatrième, wxWidgets 3.3.2, se récupère séparément ; elle ne sert qu'à `sinaia`.

GoogleTest (1.15.2) et Google Benchmark (1.10.0) ne sont **pas** à fournir : ils sont
versionnés sous `extern/` et construits avec le dépôt (§2 et §10).

#### 1.2 `CMakeUserPresets.json` — obligatoire

Le preset `local-windows` de `CMakePresets.json` laisse ses quatre `EXTERN_*`
(`EXTERN_OPENNURBS`, `EXTERN_OCCT`, `EXTERN_WXWIDGETS`, `EXTERN_LIB3MF`) à la valeur de
gabarit `"..."` : **il ne configure pas tel quel**, et il est masqué pour cette raison.
C'est voulu — les chemins des dépendances sont propres à chaque machine et n'ont pas à être
versionnés.

`CMakeUserPresets.json` est le fichier que CMake lit à côté de `CMakePresets.json` pour les
presets **personnels** : il peut hériter des presets versionnés, et il est listé dans
`.gitignore`. Créez-en un à la racine du dépôt, qui hérite de `local-windows` et renseigne
les quatre chemins.
Le nom du preset est libre ; c'est lui qui détermine le répertoire de build
(`build/<nom>/`) :

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "windows-perso",
      "displayName": "Local (Windows) -- chemins de cette machine",
      "inherits": "local-windows",
      "cacheVariables": {
        "EXTERN_OPENNURBS":  "C:/chemin/vers/opennurbs-v8.27.26019.16021",
        "EXTERN_OCCT":       "C:/chemin/vers/OCCT-8_0_0",
        "EXTERN_WXWIDGETS":  "C:/chemin/vers/wxWidgets-3.3.2",
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

> **`EXTERN_GOOGLETEST` est supprimé.** Un preset personnel qui le renseigne encore
> configure, mais `test/CMakeLists.txt` émet un avertissement : la variable est ignorée,
> googletest est construit depuis `extern/googletest-1.15.2`. Retirez-la du preset.

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

Un preset personnel n'a pas de testPreset, sauf à en déclarer un dans
`CMakeUserPresets.json` — les testPresets versionnés visent les configurations de CI
(`ci-linux*`, `ci-windows-{debug,release}`) — d'où la commande `ctest` explicite ci-dessus,
avec `-C Release` puisque le générateur est multi-configuration.

`ctest` lance trois exécutables : `TU` (la suite), `TU_faults` (injection d'échecs
d'allocation, absent sous `CG_ENABLE_SANITIZERS`) et `TU_glutess_main_probe`. Les tests de
l'import `.scad` exigent l'exécutable OpenSCAD : sans lui, ils sont **sautés**
(`GTEST_SKIP`), sauf si la variable d'environnement `CG_REQUIRE_OPENSCAD` est non vide et
différente de `0`, auquel cas ils échouent (`test/scad_test_support.h`). La recherche essaie
`CG_OPENSCAD`, puis le `PATH`, puis `EXTERN_OPENSCAD` (variable CMake, reprise de
l'environnement à la configuration si elle est vide).

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

> **GoogleTest.** Il est versionné sous `extern/googletest-1.15.2` (BSD-3, contenu décrit
> dans son `FORK.md`) et construit par `add_subdirectory` sur **toutes** les plateformes et
> tous les compilateurs : ni paquet système, ni `find_package(GTest)`, ni chemin à fournir.
> Le chemin est la variable de cache `GOOGLETEST_DIR` ; la configuration s'arrête sur un
> `FATAL_ERROR` si `gtest.h` n'y est pas. Les tests se lient à `GTest::gtest_main` seul ;
> `GTest::gmock` reste disponible pour un test qui en aurait besoin. Sous MSVC, gtest est
> compilé avec le même CRT `/MD` que le dépôt (`gtest_force_shared_crt`).

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
Ninja, GCC, Release, sans application (`ENABLE_SINAIA` reste `OFF`) ; `cgre` est construit
et testé sans sinaia (`ENABLE_CGRE=ON`), et la reconstruction Poisson est active.
`ci-linux-asan` et `ci-linux-clang` en héritent et se lancent de la même façon (§11).

Paquets système :

```bash
sudo apt-get update
sudo apt-get install -y \
  ninja-build \
  zlib1g-dev \
  libocct-foundation-dev \
  libocct-modeling-data-dev \
  libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev \
  libtbb-dev
# facultatif : sans OpenSCAD, les tests de l'import .scad sont sautés (§2)
sudo apt-get install -y --no-install-recommends openscad
```

GoogleTest n'est pas un paquet à installer : il est construit depuis `extern/` (§2).

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

Hors Visual Studio — sur Linux notamment, comme dans le job CI `maker-wasm` — le preset
`ci-maker-wasm` en hérite et ne change que le générateur (Ninja) :

```bash
source "$EMSDK/emsdk_env.sh"
cmake --preset ci-maker-wasm
cmake --build --preset ci-maker-wasm
```

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

Il configure dans `build_coverage`, en Debug avec clang, avec `-DENABLE_COVERAGE=On
-DENABLE_OPENNURBS=On -DENABLE_OCCT=On -DENABLE_POISSON=On -DENABLE_CGRE=On`, lance `TU`
**depuis son `TU_RUN_DIR`** (§2), puis produit `coverage.txt` et `coverage.info` via
`llvm-profdata-18` et `llvm-cov-18`. `coverage.info` n'est écrit que si l'export a réussi et
décrit au moins un fichier source. Le script rend le code de sortie de `TU`, après avoir
produit la couverture : un test en échec le fait échouer. `EXTERN_OPENNURBS` est lu dans
l'environnement (il n'est pas passé en ligne de commande).

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

### 10. Bancs de performance (`cg_bench`)

Les bancs vivent dans `bench/` et s'appuient sur Google Benchmark 1.10.0, versionné sous
`extern/benchmark-1.10.0` (chemin : variable de cache `BENCHMARK_DIR`). Ils couvrent le
`Regenerate` gothique, les barres fusionnées à plusieurs nombres de fils, `CreateBlock`, la
tessellation et les extrusions de la région de pierre, et les fonctions de
`surface_architecture`. Chaque banc rapporte aussi ses allocations (compteurs `allocs` et
`alloc_bytes`), par un `operator new` global remplacé.

- Option **`ENABLE_BENCHMARKS`**, à `OFF` par défaut : désactivée, elle n'ajoute ni cible
  ni sous-répertoire. Aucun preset ne l'active, et la CI ne construit pas `cg_bench`.
- **Exclue sous `CG_ENABLE_SANITIZERS`** : la configuration émet un avertissement et ne
  construit pas `cg_bench`. Le remplacement de l'`operator new` retire à ASan une partie de
  ses contrôles (la même raison écarte `TU_faults`), et une mesure sous sanitizer ne vaut
  rien.
- Jamais construite sous Emscripten.
- La cible `cg_bench` lie `cgarchi` et `cgmesh` ; sous Windows, les DLL d'OpenNURBS, de
  lib3mf et d'OCCT sont copiées à côté d'elle, comme pour `TU`.

Sous Windows, avec un preset personnel (§1.2), dans un répertoire de build à part :

```powershell
cmake --preset windows-perso -B build/windows-perso-bench -DENABLE_BENCHMARKS=ON
cmake --build build/windows-perso-bench --config Release --target cg_bench
build\windows-perso-bench\bench\Release\cg_bench.exe --benchmark_repetitions=21 --benchmark_display_aggregates_only=true
```

Le protocole, la machine de référence et les mesures sont dans
[`baseline_performance.md`](baseline_performance.md) §8. Une passe complète enchaîne tous
les bancs, et un banc peut dépendre de l'état du tas laissé par ceux qui le précèdent.
**Pour une mesure à retenir, lancez une passe isolée par famille** avec
`--benchmark_filter`, par exemple `--benchmark_filter=FusedBars`,
`--benchmark_filter=StonePolygon`, `--benchmark_filter=SampleArc` ou
`--benchmark_filter=ExtrudeMoulure`.

### 11. Intégration continue

Deux workflows, déclenchés par un push sur `main` et par les pull requests (`build.yml` :
celles qui visent `main`, et aussi à la main par `workflow_dispatch` ; `sq.yml` : toutes, à
l'ouverture, à la mise à jour et à la réouverture). Tous les jobs consomment la même release de `cg_binaries`,
fixée par `CG_BINARIES_VERSION` dans chacun des deux fichiers, qui doivent rester alignés.

`.github/workflows/build.yml` :

| Job                       | Runner                | Compilateur, configuration                   | Preset                                    | Ce qui est vérifié                                                                                                   | Ce qui ne l'est pas                                                                                          |
| :------------------------ | :-------------------- | :------------------------------------------- | :---------------------------------------- | :------------------------------------------------------------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------- |
| `ci-linux`                | `ubuntu-latest`       | GCC, Release, Ninja                          | `ci-linux`                                | build complet, `ctest` (`TU`, `TU_faults`, sonde glutess), `cgre` compris ; OpenSCAD installé et **exigé**           | applications, `cg_bench`                                                                                     |
| `ci-linux-asan`           | `ubuntu-latest`       | GCC, Debug, ASan + UBSan                     | `ci-linux-asan`                           | `ctest` sous sanitizers, arrêt au premier rapport ; OpenSCAD exigé                                                   | fuites (`detect_leaks=0`), `TU_faults` et `cg_bench` (non construits sous sanitizers)                        |
| `ci-linux-boilerplate`    | `ubuntu-latest`       | GCC, Release                                 | `ci-linux-boilerplate`                    | édition de liens du canvas ImGui et de l'exécutable `cggraph-boilerplate`                                            | aucun test lancé                                                                                             |
| `ci-linux-clang`          | `ubuntu-latest`       | Clang, Release                               | `ci-linux-clang`                          | mêmes build et tests que `ci-linux` ; OpenSCAD exigé                                                                 | pas de `-Werror` : les warnings se lisent dans le journal                                                    |
| `ci-windows (Debug)`, `ci-windows (Release)` | `windows-2025-vs2026` | MSVC (Visual Studio 18 2026), Debug et Release | `ci-windows`, puis `ci-windows-{debug,release}` | build et `ctest` des deux configurations, sans `fail-fast` | **tests `.scad` sautés** (OpenSCAD absent du runner) ; sinaia, `cgre`, sulina, ONNX ; pas d'ASan MSVC |
| `maker-wasm`              | `ubuntu-latest`       | Emscripten 6.0.8, Release, Ninja             | `ci-maker-wasm`                           | `maker` compilé et lié ; le module produit se charge et rend un maillage non vide pour chaque forme du catalogue (`maker/tools/check_module.mjs`, cardinalités attendues dans le workflow) | aucun test googletest ; les fichiers `maker.js` et `maker.wasm` produits ne sont pas commités                |

`.github/workflows/sq.yml`, job `Build` (`ubuntu-latest`) :

- **Couverture** : `coverage.sh` (§8), clang, Debug instrumenté, avec OpenSCAD installé et
  exigé. Seul `TU` est lancé, directement et non par `ctest` : `TU_faults` et la sonde glutess
  n'y tournent pas. Un test en échec fait échouer le job ; la couverture est tout de même
  envoyée à SonarCloud si `coverage.info` a été produit.
- **Analyse SonarCloud** : une seconde configuration, GCC, hors preset, compilée sous le
  `build-wrapper` de Sonar. Elle n'est que compilée, pas testée.

Aucun job ne construit `sinaia`, `sulina`, `vecna` ni `cg_bench`, et `cgnet` n'est testé que
sous Windows (il n'est construit que sous `WIN32`).

### Résumé des options CMake

Les colonnes donnent la valeur **effective** dans chaque preset, héritage compris.
« — » signifie « non renseigné par le preset », donc valeur par défaut. `maker-wasm`
n'hérite pas de `_base`. Les presets dérivés ne sont pas répétés : `ci-linux-asan` est
`ci-linux` plus `CMAKE_BUILD_TYPE=Debug` et `CG_ENABLE_SANITIZERS=ON`, `ci-linux-clang` est
`ci-linux` avec Clang, `ci-linux-boilerplate` est `ci-linux` plus
`ENABLE_CGGRAPH_BOILERPLATE=ON`, `ci-maker-wasm` est `maker-wasm` sous Ninja.

| Option                       | Description                                                    | Défaut | `_base`  | `local-windows` | `ci-linux` | `ci-windows` | `maker-wasm` |
| :--------------------------- | :------------------------------------------------------------- | :----- | :------- | :-------------- | :--------- | :----------- | :----------- |
| `ENABLE_SINAIA`              | Application sinaia, et `src/cgre` avec elle                     | `OFF`  | `OFF`    | **`ON`**        | `OFF`      | `OFF`        | `OFF`        |
| `ENABLE_SINAIA_TREATMENTS`   | Dock « Treatments » de sinaia (option de `sinaia/`)             | `ON`   | —        | —               | —          | —            | —            |
| `ENABLE_CGRE`                | Bibliothèque `cgre` et ses tests, sans sinaia                   | `OFF`  | —        | — (impliqué par sinaia) | **`ON`** | `OFF`   | —            |
| `ENABLE_SULINA`              | sulina (Qt 6 + Vulkan), et `src/cgre2` avec lui                 | `OFF`  | —        | —               | —          | `OFF`        | `OFF`        |
| `ENABLE_MAKER`               | Module WebAssembly `maker` (Emscripten uniquement)              | `OFF`  | —        | —               | —          | —            | **`ON`**     |
| `ENABLE_CGGRAPH_BOILERPLATE` | Canvas nodal ImGui et son hôte natif `cggraph-boilerplate`      | `OFF`  | —        | —               | —          | `OFF`        | —            |
| `ENABLE_OPENNURBS`           | Import `.3dm` (requiert `EXTERN_OPENNURBS`)                     | `ON`   | `ON`     | `ON`            | `ON`       | `ON`         | `OFF`        |
| `ENABLE_OCCT`                | Import STEP/IGES (`EXTERN_OCCT` sous Windows ; apt sous Linux)  | `OFF`  | **`ON`** | `ON`            | `ON`       | `ON`         | `OFF`        |
| `ENABLE_LIB3MF`              | Import `.3mf` (requiert `EXTERN_LIB3MF`)                        | `OFF`  | —        | **`ON`**        | **`ON`**   | `ON`         | —            |
| `ENABLE_ZLIB`                | zlib, requis par le parseur NBT (Minecraft)                     | `ON`   | —        | —               | —          | —            | `OFF`        |
| `ENABLE_POISSON`             | Reconstruction de surface Poisson (`POISSONRECON_DIR`)          | `OFF`  | —        | —               | **`ON`**   | **`ON`**     | `OFF`        |
| `ENABLE_ONNX`                | Source de profondeur ONNX (requiert `EXTERN_ONNXRUNTIME`)       | `OFF`  | —        | —               | —          | `OFF`        | `OFF`        |
| `ENABLE_RECONSTRUCTION_CLI`  | Exécutable `reconstruction-cli` (implique `ENABLE_ONNX`)        | `OFF`  | —        | —               | —          | `OFF`        | `OFF`        |
| `ENABLE_CGNET_DEMO`          | Exécutable de démo cgnet (Windows)                              | `OFF`  | `OFF`    | `OFF`           | `OFF`      | `OFF`        | `OFF`        |
| `ENABLE_BENCHMARKS`          | Bancs `cg_bench` (§10), ignorée sous sanitizers                 | `OFF`  | —        | —               | —          | —            | —            |
| `CG_ENABLE_SANITIZERS`       | ASan (+ UBSan hors MSVC)                                        | `OFF`  | —        | —               | —          | —            | —            |
| `ENABLE_COVERAGE`            | Instrumentation pour la couverture de code                      | `OFF`  | —        | —               | —          | —            | —            |

Chemins des dépendances :

| Variable             | Désigne                                                  | Requise quand                                                  |
| :------------------- | :------------------------------------------------------- | :------------------------------------------------------------- |
| `EXTERN_OPENNURBS`   | Racine OpenNURBS (`include/`, `lib/`)                     | `ENABLE_OPENNURBS=ON`, sur les deux plateformes                 |
| `EXTERN_OCCT`        | Racine OCCT (`include/`, `lib/`, `bin/`)                  | `ENABLE_OCCT=ON`, **Windows uniquement**                        |
| `EXTERN_LIB3MF`      | Racine lib3mf (`include/`, `lib/`, `bin/`)                | `ENABLE_LIB3MF=ON`                                              |
| `EXTERN_WXWIDGETS`   | Racine wxWidgets                                          | `ENABLE_SINAIA=ON`                                              |
| `EXTERN_ONNXRUNTIME` | Racine ONNX Runtime                                       | `ENABLE_ONNX=ON`                                                |
| `POISSONRECON_DIR`   | Source PoissonRecon (défaut `extern/PoissonRecon-18.76`)  | `ENABLE_POISSON=ON`                                             |
| `ZLIB_DIR`           | Source zlib (défaut `extern/zlib-1.3.2`)                  | `ENABLE_ZLIB=ON`, Windows uniquement                            |
| `GOOGLETEST_DIR`     | Source GoogleTest (défaut `extern/googletest-1.15.2`)     | toujours, hors Emscripten (tests)                               |
| `BENCHMARK_DIR`      | Source Google Benchmark (défaut `extern/benchmark-1.10.0`) | `ENABLE_BENCHMARKS=ON`                                          |
| `EXTERN_OPENSCAD`    | Exécutable OpenSCAD ou son répertoire                     | jamais : dernier recours des tests `.scad` (§2)                 |

> `EXTERN_OPENNURBS`, `EXTERN_OCCT`, `EXTERN_LIB3MF`, `EXTERN_ONNXRUNTIME` et
> `EXTERN_OPENSCAD` acceptent un repli par variable d'environnement de même nom, utilisé si
> la variable CMake est vide. `EXTERN_WXWIDGETS` n'en a **pas** : elle doit être passée à
> CMake. `EXTERN_GOOGLETEST` n'existe plus (§1.2).

## Génération de la solution Visual Studio

La configuration par preset produit déjà une solution dans le répertoire de build. Avec le
générateur `Visual Studio 18 2026`, c'est **`cg.slnx`** — le format de VS 2026 — et non
`cg.sln` : `build/<preset>/cg.slnx`. Ouvrez-la et utilisez la liste déroulante des
configurations pour basculer entre `Debug` et `Release`.

Le répertoire de travail de débogage de `TU` est déjà réglé sur `TU_RUN_DIR` (§2) : les
tests lancés depuis Visual Studio lisent `test/data` au bon endroit.
