## cg

[![Build](https://github.com/exopyl/cg/actions/workflows/build.yml/badge.svg)](https://github.com/exopyl/cg/actions)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=exopyl_cg&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=exopyl_cg)

## Compilation (Build Instructions)

Le projet utilise CMake. Cette section documente les commandes **exactes** validées
sur la machine de dev Windows pour compiler les quatre cibles : **tests (TU)**,
**Sinaia**, **sulina** et **vecna**.

### Environnement (Windows)

Les outils et dépendances ne sont **pas** dans le `PATH` — on utilise des chemins absolus.

- **CMake 4.2.0** : `C:\home\bin\cmake-4.2.0-windows-x86_64\bin\cmake.exe`
- **Générateur** : `Visual Studio 18 2026` (MSVC v14.5x). Multi-config : on choisit
  `Debug`/`Release` au moment du build via `--config`, pas à la configuration.
- **Qt** : `C:/Qt/6.11.1/msvc2022_64` (pour sulina)
- **Vulkan SDK** : `C:/VulkanSDK/1.4.309.0` — la variable d'environnement `VULKAN_SDK`
  doit être définie (fournit `Vulkan::Vulkan` et `glslc`) pour sulina et vecna.

Dépendances pré-compilées sous `C:\home\devthirdparties\` :

| Dépendance   | Chemin                                                        | Utilisé par        |
| :----------- | :------------------------------------------------------------ | :----------------- |
| GoogleTest   | `C:/home/devthirdparties/googletest-1.15.2`                   | tests (TU)         |
| wxWidgets    | `C:/home/devthirdparties/wxWidgets-3.3.2`                     | Sinaia             |
| OpenNURBS    | `C:/home/devthirdparties/opennurbs-v8.27.26019.16021`         | Sinaia, tests      |
| OCCT         | `C:/home/devthirdparties/occt_layout` *(voir §4)*             | Sinaia, tests      |

Les exemples ci-dessous (PowerShell) définissent d'abord un raccourci vers cmake :

```powershell
$cmake = "C:\home\bin\cmake-4.2.0-windows-x86_64\bin\cmake.exe"
```

### 1. Tests unitaires (TU) + Sinaia

Le projet racine compile à la fois les tests (cible `TU`) et l'application Sinaia
(`ENABLE_SINAIA=ON`), avec import `.3dm` (OpenNURBS) et STEP/IGES (OCCT).

```powershell
# Configuration (une seule fois)
& $cmake -G "Visual Studio 18 2026" -A x64 `
  -DENABLE_SINAIA=ON `
  -DENABLE_OPENNURBS=ON -DEXTERN_OPENNURBS=C:/home/devthirdparties/opennurbs-v8.27.26019.16021 `
  -DENABLE_OCCT=ON -DEXTERN_OCCT=C:/home/devthirdparties/occt_layout `
  -DEXTERN_WXWIDGETS=C:/home/devthirdparties/wxWidgets-3.3.2 `
  -DEXTERN_GOOGLETEST=C:/home/devthirdparties/googletest-1.15.2 `
  -S C:/home/perso/cg -B C:/home/perso/cg/build_sinaia_release

# Build des deux cibles
& $cmake --build C:/home/perso/cg/build_sinaia_release --config Release --target TU
& $cmake --build C:/home/perso/cg/build_sinaia_release --config Release --target sinaia
```

Exécuter les tests :

```powershell
& C:/home/perso/cg/build_sinaia_release/test/Release/TU.exe
# ou, via ctest :
& $cmake -E chdir C:/home/perso/cg/build_sinaia_release ctest -C Release --output-on-failure
```

Sortie : `build_sinaia_release/sinaia/Release/sinaia.exe` (les DLLs wxWidgets,
OpenNURBS et OCCT sont copiées à côté de l'exécutable par les étapes post-build).

> Pour un build **tests seuls**, on peut omettre `ENABLE_SINAIA`, `ENABLE_OCCT` et
> `EXTERN_WXWIDGETS`, mais il faut garder `EXTERN_OPENNURBS` (OpenNURBS est activé
> par défaut et `cgmesh` échoue sinon), ou passer `-DENABLE_OPENNURBS=OFF`.

### 2. sulina (Qt 6 + Vulkan)

Piloté par le projet racine via `ENABLE_SULINA=ON` (ajoute `src/cgre2` et `sulina`).
Nécessite Qt 6.11 et le Vulkan SDK. OpenNURBS/OCCT ne sont pas requis pour cette cible.

```powershell
& $cmake -G "Visual Studio 18 2026" -A x64 `
  -DENABLE_SULINA=ON `
  -DENABLE_OPENNURBS=OFF `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64 `
  -S C:/home/perso/cg -B C:/home/perso/cg/build_sulina

& $cmake --build C:/home/perso/cg/build_sulina --config Release --target sulina
```

### 3. vecna (Vulkan)

vecna est un **projet CMake autonome** (`vecna/CMakeLists.txt`) — il ne passe **pas**
par le root. Il récupère ses dépendances (GLFW, VMA, ImGui) via FetchContent (accès
réseau requis à la première configuration) et nécessite le Vulkan SDK + C++20.

```powershell
& $cmake -G "Visual Studio 18 2026" -A x64 `
  -S C:/home/perso/cg/vecna -B C:/home/perso/cg/build_vecna

& $cmake --build C:/home/perso/cg/build_vecna --config Release --target vecna
```

Si le SDK Vulkan n'est pas trouvé automatiquement, ajouter
`-DVulkan_ROOT="C:/VulkanSDK/1.4.309.0"` à la configuration.

### 4. Préparation d'OCCT (pour Sinaia/tests)

`cgmesh/CMakeLists.txt` attend `EXTERN_OCCT` pointant vers un dossier contenant
`include/`, `lib/` et `bin/`. Les binaires livrés ont une autre arborescence
(`install/inc`, `install/win64/vc14/{lib,bin}`). On crée donc un dossier `occt_layout`
de jonctions (ne nécessite pas les droits admin) :

```powershell
$root   = 'C:\home\devthirdparties\OCCT-8_0_0_binaries\install'
$layout = 'C:\home\devthirdparties\occt_layout'
New-Item -ItemType Directory -Force -Path $layout | Out-Null
New-Item -ItemType Junction -Path "$layout\include" -Target "$root\inc"            | Out-Null
New-Item -ItemType Junction -Path "$layout\lib"     -Target "$root\win64\vc14\lib" | Out-Null
New-Item -ItemType Junction -Path "$layout\bin"     -Target "$root\win64\vc14\bin" | Out-Null
```

### Résumé des options CMake

| Option              | Description                                          | Par défaut |
| :------------------ | :--------------------------------------------------- | :--------- |
| `ENABLE_SINAIA`     | Compile l'application Sinaia (wxWidgets + OpenGL)    | `OFF`      |
| `ENABLE_SULINA`     | Compile sulina (Qt 6 + Vulkan, via `cgre2`)          | `OFF`      |
| `ENABLE_OPENNURBS`  | Import `.3dm` (requiert `EXTERN_OPENNURBS`)          | `ON`       |
| `ENABLE_OCCT`       | Import STEP/IGES (requiert `EXTERN_OCCT`)            | `OFF`      |
| `ENABLE_CGNET_DEMO` | Compile l'exécutable de démo cgnet (Windows)         | `OFF`      |
| `ENABLE_COVERAGE`   | Instrumentation pour la couverture de code           | `OFF`      |
| `EXTERN_GOOGLETEST` | Chemin vers GoogleTest                               | (cache/env)|
| `EXTERN_WXWIDGETS`  | Chemin vers wxWidgets (Sinaia)                       | (cache/env)|
| `EXTERN_OPENNURBS`  | Chemin vers OpenNURBS                                | (cache/env)|
| `EXTERN_OCCT`       | Chemin vers OCCT (`include`/`lib`/`bin`)             | (cache/env)|

> vecna a ses propres options (`vecna/CMakeLists.txt`), notamment
> `VECNA_BUILD_TESTS` (`OFF` par défaut) ; il n'y a **pas** d'option `ENABLE_VECNA`
> dans le projet racine.

## Génération de la solution Visual Studio (.sln)

La configuration racine (§1) produit déjà une solution `cg.sln` dans le répertoire de
build (ici `build_sinaia_release/cg.sln`). Ouvrez-la dans Visual Studio et utilisez la
liste déroulante des configurations pour basculer entre `Debug` et `Release`.
