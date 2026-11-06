# googletest — copie vendorisée pour `test/`

## Version

- Amont : <https://github.com/google/googletest>, étiquette **`v1.15.2`** (commit
  `b514bdc898e2951020cbdca1304b75f5950d1f59`, du 2024-07-31). Même version et même commit que ceux
  que le job `ci-windows` téléchargeait et contrôlait avant cette copie.
- Licence : BSD-3-Clause, voir `LICENSE` (conservé).

## Contenu conservé

Ce qui sert à construire `gtest`, `gtest_main`, `gmock` et `gmock_main` : `CMakeLists.txt` racine, et
pour `googletest/` comme pour `googlemock/` : `CMakeLists.txt`, `cmake/`, `include/`, `src/`.
Retirés : `.git`, `.github`, `ci/`, `docs/`, `googletest/samples/`, `googletest/test/`,
`googlemock/test/`, les `README.md`, `CONTRIBUTING.md`, `CONTRIBUTORS`, les fichiers Bazel
(`BUILD.bazel`, `MODULE.bazel`, `WORKSPACE*`, `*.bzl`) et les fichiers de configuration d'outils.
Les `CMakeLists.txt` ne lisent `test/` et `samples/` que sous `gtest_build_tests`,
`gtest_build_samples` et `gmock_build_tests`, tous à `OFF` par défaut.

## Intégration

`test/CMakeLists.txt` : `add_subdirectory(${GOOGLETEST_DIR} ${CMAKE_BINARY_DIR}/extern/googletest
EXCLUDE_FROM_ALL)` sur toutes les plateformes, avec `INSTALL_GTEST` à `OFF`, et
`gtest_force_shared_crt` à `ON` (CRT `/MD`, celui du dépôt sous MSVC). Les cibles se lient par
leurs alias `GTest::gtest_main`, `GTest::gmock_main`, etc. Elles exposent leurs en-têtes en `SYSTEM` :
les warnings posés par cible sur le dépôt ne s'y appliquent pas.

## Modification locale

Aucune.

## Comportement amont à connaître

Sous MSVC, `googletest/cmake/internal_utils.cmake` impose `-W4 -WX` à gtest, sans option pour le
désactiver ; sous GCC et Clang, aucun `-Werror`. Un nouvel avertissement d'une mise à jour de MSVC
dans `gtest-all.cc` fait donc échouer le build Windows sans changement du dépôt. Le lever demande
une modification locale de ce fichier, à consigner ci-dessus.
