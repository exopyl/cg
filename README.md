## cg

[![Build](https://github.com/exopyl/cg/actions/workflows/build.yml/badge.svg)](https://github.com/exopyl/cg/actions)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=exopyl_cg&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=exopyl_cg)

## Compilation (Build Instructions)

Ce projet utilise CMake. Sur Windows, il est recommandé d'utiliser Visual Studio 2022.

### Environnement
- **CMake** : Rechercher dans `C:\home\bin\cmake-3.26.2-windows-x86_64\bin\cmake.exe` ou dans le PATH.
- **Compilateur** : Visual Studio 2022 (MSVC). Utiliser le "Developer Command Prompt" ou "vcvars64.bat".

### 1. Tests Unitaires (TU)
Les tests unitaires sont compilés par défaut avec le projet principal.
- **Debug** :
  ```bash
  cmake -B build/debug -S . -DCMAKE_BUILD_TYPE=Debug
  cmake --build build/debug --target TU
  ```
- **Release** :
  ```bash
  cmake -B build/release -S . -DCMAKE_BUILD_TYPE=Release
  cmake --build build/release --target TU
  ```

### 2. Sinaia (Visualiseur OpenGL)
Nécessite `wxWidgets`.
- **Build dédié** (dans `build_sinaia`) :
  ```bash
  cmake -B build_sinaia -S . -DENABLE_SINAIA=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build build_sinaia --target sinaia --config Release
  ```

### 3. Vecna (Visualiseur Vulkan)
Nécessite le SDK Vulkan et un compilateur C++20.
- **Localisation du SDK Vulkan** :
  Si le SDK n'est pas dans le PATH, spécifiez sa racine (contenant `bin`, `include`, `lib`) via `Vulkan_ROOT`.
- **Build dédié** (dans `build_vecna`) :
  ```bash
  # Recherche du SDK à la racine du projet ou dans C:/home/dev
  cmake -B build_vecna -S . -DENABLE_VECNA=ON -DCMAKE_BUILD_TYPE=Release -DVulkan_ROOT="C:/home/dev/VulkanSDK/1.x.x.x"
  cmake --build build_vecna --target vecna --config Release
  ```


### Résumé des options CMake
| Option | Description | Par défaut |
| :--- | :--- | :--- |
| `ENABLE_SINAIA` | Compile l'application Sinaia (wxWidgets) | `OFF` |
| `ENABLE_VECNA` | Compile l'application Vecna (Vulkan) | `OFF` |
| `ENABLE_COVERAGE` | Active l'instrumentation pour la couverture de code | `OFF` |
| `EXTERN_GOOGLETEST` | Chemin vers GoogleTest | (Variable d'environnement ou cache) |
| `EXTERN_WXWIDGETS` | Chemin vers wxWidgets (pour Sinaia) | (Variable d'environnement ou cache) |

## Génération de la solution Visual Studio (.sln)

Pour obtenir une solution `.sln` classique permettant de basculer entre **Debug** et **Release** directement dans l'IDE Visual Studio :

1.  **Générer la solution** (dans le répertoire `build_msvc`) :
    ```bash
    cmake -B build_msvc -S . -G "Visual Studio 17 2022" -A x64 -DEXTERN_GOOGLETEST="C:/home/dev/extern/googletest-1.15.2"
    ```
2.  **Ouvrir la solution** :
    Ouvrez le fichier `build_msvc/cg.sln` avec Visual Studio.
3.  **Utilisation** :
    Utilisez la liste déroulante des configurations dans la barre d'outils de Visual Studio pour choisir entre `Debug` et `Release`.
