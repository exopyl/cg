# PoissonRecon — fork vendorisé pour cgmesh

Copie vendorisée de **PoissonRecon** (Misha Kazhdan, *Adaptive Multigrid Solvers*),
intégrée pour la reconstruction de surface de `cgmesh`
(`recon::PoissonReconstructor`, étape 8 du pipeline de reconstruction — voir
`src/cgmesh/reconstruction.md`).

## Version

- **Upstream : `ADAPTIVE_SOLVERS_VERSION "18.76"`** (constante dans `Src/PreProcessor.h`),
  d'où le nom du dossier `PoissonRecon-18.76`.
- Source amont : <https://github.com/mkazhdan/PoissonRecon>.
- Licence : voir `LICENSE` (conservé).

## Mode d'intégration

**In-process, sans exécutable et sans bibliothèque image.** On n'utilise QUE l'API
d'en-têtes `Src/Reconstructors.h` (autonome, sous `namespace PoissonRecon`), depuis
`src/cgmesh/recon_surface_poisson.cpp` (calqué sur `Src/Reconstruction.example.cpp` :
flux de points orientés en mémoire → `Reconstructor::Poisson::Solver::Solve` →
`extractLevelSet` → `Mesh`).

Activation : option CMake `ENABLE_POISSON` (+ `POISSONRECON_DIR`), qui définit
`CG_HAS_POISSON` (PUBLIC) et ajoute `Src/` aux include dirs **PRIVATE** de cgmesh
(les en-têtes PoissonRecon ne fuient pas hors du `.cpp`).

## Ce qui a été nécessaire pour l'intégrer

### 1. Suppressions (fichiers/répertoires inutiles)

La reconstruction in-process n'a besoin que des **en-têtes** de `Src/`. Ont été supprimés :

- **Fichiers de build amont** (on compile via le CMake de cgmesh, pas via ces projets) :
  `*.sln`, `*.vcxproj`, `*.vcxproj.filters`, `Makefile`, `test.bat`, ainsi qu'un
  `CMakeLists.txt` ajouté temporairement (tentative de build d'un exe, abandonnée).
- **Sources des outils CLI** `Src/*.cpp` (`PoissonRecon.cpp`, `SSDRecon.cpp`,
  `PointInterpolant.cpp`, `Reconstruction.example.cpp`, `ImageStitching.cpp`,
  `SurfaceTrimmer.cpp`, etc.) — non utilisées (on n'inclut que les en-têtes).
- **En-têtes image** `Src/Image.h`, `Src/PNG.h`, `Src/JPEG.h`, `Src/PNG.inl`,
  `Src/JPEG.inl` — inclus uniquement par les outils CLI, **jamais** par
  `Reconstructors.h`. Les retirer supprime toute dépendance PNG/JPEG/ZLIB.
  *(Cette copie ne contenait de toute façon pas les répertoires sources PNG/JPEG/ZLIB.)*

**Conservés** : `LICENSE`, `README.md`, et les en-têtes de `Src/` (`*.h`, `*.inl`,
`*.imp.h`, `*.imp.inl`) nécessaires à la compilation de `Reconstructors.h`.

### 2. Modifications du code source amont : **AUCUNE**

Le code 18.76 compile **tel quel** (C++17, MSVC). Aucune retouche des sources n'a
été faite.

> **À propos du `hash_map`** : l'ancienne version **8.0** (essayée d'abord,
> `extern/PoissonRecon-8.0`) incluait l'en-tête **`<hash_map>`** (extension
> SGI/Microsoft, **supprimée des MSVC modernes**) dans `Src/Hash.h`, et aurait exigé
> un patch `<hash_map>` → `<unordered_map>`. La **18.76 n'a plus ce problème** (elle
> utilise les conteneurs standard) : **c'est l'une des raisons du passage à cette
> version** — aucune modification `hash_map` n'a été nécessaire ici.

### 3. Réglages de compilation (côté cgmesh, pas dans le fork)

Dans `src/cgmesh/CMakeLists.txt`, sous `ENABLE_POISSON` (MSVC) :

- **`_HAS_STD_BYTE=0`** — PoissonRecon fait des `using namespace std;` et inclut des
  en-têtes Windows ; sans ce define, `std::byte` (C++17) entre en conflit avec le
  `byte` du SDK Windows → erreur *« 'byte' : symbole ambigu »*.
- **`/bigobj`** — les templates `FEMTree` génèrent un objet volumineux.
- **`NOMINMAX`**, **`_USE_MATH_DEFINES`** — usuels avec les en-têtes Windows.
- C++17 (déjà la norme de cgmesh).

## Remarques

- **Performance** : PoissonRecon est orienté Release ; il est **très lent en Debug**.
  Préférer une configuration Release pour les reconstructions réelles.
- **Prérequis** : Poisson exige un nuage à **normales orientées** (étape 7 du
  pipeline). `recon::PoissonReconstructor::reconstruct` renvoie `nullptr` si le nuage
  n'a pas de normales.
