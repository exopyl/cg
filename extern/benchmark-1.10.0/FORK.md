# Google Benchmark — copie vendorisée pour `bench/`

## Version

- Amont : <https://github.com/google/benchmark>, étiquette **`v1.10.0`** (commit
  `a6b4a9f00b36c19af9997d68a1cf6e0b5b3d032c`, publiée le 2026-10-14).
- Licence : Apache 2.0, voir `LICENSE` (conservé, avec `AUTHORS` et `CONTRIBUTORS`).

## Contenu conservé

Ce qui sert à construire la bibliothèque : `CMakeLists.txt`, `cmake/`, `include/`, `src/`.
Retirés : `.git`, `.github`, `test/`, `docs/`, `bindings/`, `tools/`, `bazel/` et fichiers Bazel,
`setup.py`, `pyproject.toml`, fichiers de configuration d'outils.

## Intégration

`CMakeLists.txt` racine, sous `ENABLE_BENCHMARKS` (défaut Off) : `add_subdirectory(${BENCHMARK_DIR}
... EXCLUDE_FROM_ALL)` avec `BENCHMARK_ENABLE_TESTING`, `BENCHMARK_ENABLE_GTEST_TESTS`,
`BENCHMARK_ENABLE_INSTALL`, `BENCHMARK_INSTALL_DOCS` et `BENCHMARK_ENABLE_WERROR` à `OFF`. Seule la cible
`cg_bench` (`bench/`) la consomme.

## Modification locale

Une seule : `cmake/GetGitVersion.cmake` n'interroge git que si `${PROJECT_SOURCE_DIR}/.git` existe.
Sans elle, `git describe` s'exécute dans le dépôt `cg` englobant et rend son étiquette à lui
(`v0.0.1-…`), que la bibliothèque prend pour sa version. Avec elle, la version est celle de
`project()` : `v1.10.0`.
