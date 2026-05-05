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
Nécessite `wxWidgets`. Activer via l'option `ENABLE_SINAIA`.
- **Debug** :
  ```bash
  cmake -B build/sinaia-debug -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_SINAIA=ON
  cmake --build build/sinaia-debug --target sinaia
  ```
- **Release** :
  ```bash
  cmake -B build/sinaia-release -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_SINAIA=ON
  cmake --build build/sinaia-release --target sinaia
  ```

### 3. Vecna (Visualiseur Vulkan)
Nécessite le SDK Vulkan. Activer via l'option `ENABLE_VECNA`.
Note : `vecna` requiert un compilateur compatible C++20.
- **Debug** :
  ```bash
  cmake -B build/vecna-debug -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_VECNA=ON
  cmake --build build/vecna-debug --target vecna
  ```
- **Release** :
  ```bash
  cmake -B build/vecna-release -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_VECNA=ON
  cmake --build build/vecna-release --target vecna
  ```

### Résumé des options CMake
| Option | Description | Par défaut |
| :--- | :--- | :--- |
| `ENABLE_SINAIA` | Compile l'application Sinaia (wxWidgets) | `OFF` |
| `ENABLE_VECNA` | Compile l'application Vecna (Vulkan) | `OFF` |
| `ENABLE_COVERAGE` | Active l'instrumentation pour la couverture de code | `OFF` |
| `EXTERN_GOOGLETEST` | Chemin vers GoogleTest | (Variable d'environnement ou cache) |
| `EXTERN_WXWIDGETS` | Chemin vers wxWidgets (pour Sinaia) | (Variable d'environnement ou cache) |
