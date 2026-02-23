# Testing Guide - Vecna

## Framework

**Google Test v1.14.0** avec **Google Mock** pour les tests unitaires et les mocks.

## Structure des tests

```
tests/
├── CMakeLists.txt                    # Configuration CMake des tests
├── Core/
│   ├── LoggerTest.cpp                # Tests du Logger
│   └── WindowConfigTest.cpp          # Tests de Window::Config
└── Renderer/
    └── QueueFamilyIndicesTest.cpp    # Tests de QueueFamilyIndices
```

## Compilation des tests

### Prérequis

- CMake 3.20+
- Compilateur C++20 (MSVC 19.29+, GCC 10+, Clang 10+)
- Vulkan SDK (pour les dépendances)

### Configuration

```bash
# Configurer le projet avec les tests activés
cmake -B build -DVECNA_BUILD_TESTS=ON

# Pour un build Debug (recommandé pour les tests)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DVECNA_BUILD_TESTS=ON
```

### Compilation

```bash
# Compiler uniquement les tests
cmake --build build --target vecna_tests

# Compiler tout (application + tests)
cmake --build build
```

## Exécution des tests

### Via CTest (recommandé)

```bash
# Exécuter tous les tests
ctest --test-dir build -C Debug

# Avec affichage détaillé en cas d'échec
ctest --test-dir build -C Debug --output-on-failure

# Exécuter un test spécifique par nom
ctest --test-dir build -C Debug -R "LoggerTest"

# Liste des tests disponibles
ctest --test-dir build -C Debug -N
```

### Via l'exécutable directement

```bash
# Exécuter tous les tests
./build/tests/Debug/vecna_tests.exe

# Filtrer les tests par pattern
./build/tests/Debug/vecna_tests.exe --gtest_filter="LoggerTest.*"

# Afficher les tests disponibles
./build/tests/Debug/vecna_tests.exe --gtest_list_tests
```

## Tests disponibles

### LoggerTest (14 tests)

| Test | Description |
|------|-------------|
| `DebugIsLowestLevel` | Vérifie l'ordre des niveaux de log |
| `InfoIsBelowWarn` | Vérifie l'ordre des niveaux de log |
| `WarnIsBelowError` | Vérifie l'ordre des niveaux de log |
| `SetAndGetMinLevel` | Test get/set du niveau minimum |
| `MessagesBelowMinLevelAreFiltered` | Messages sous le seuil sont filtrés |
| `MessagesAtOrAboveMinLevelAreShown` | Messages au-dessus du seuil sont affichés |
| `InfoMessagesGoToStdoutWithModulePrefix` | Format [MODULE] Message sur stdout |
| `ErrorMessagesGoToStderr` | Erreurs sur stderr |
| `WarnMessagesGoToStderr` | Warnings sur stderr |
| `DebugMessagesGoToStdout` | Debug sur stdout |
| `HandlesEmptyModuleName` | Gestion module vide |
| `HandlesEmptyMessage` | Gestion message vide |
| `HandlesSpecialCharactersInMessage` | Caractères spéciaux (backslash, etc.) |
| `HandlesNewlinesInMessage` | Retours à la ligne dans messages |

### WindowConfigTest (19 tests)

| Test | Description |
|------|-------------|
| `MinWidthIsReasonable` | MIN_WIDTH dans une plage raisonnable |
| `MinHeightIsReasonable` | MIN_HEIGHT dans une plage raisonnable |
| `MaxWidthIsLargeEnough` | MAX_WIDTH supporte 4K+ |
| `MaxHeightIsLargeEnough` | MAX_HEIGHT supporte 4K+ |
| `MinValuesAreLessThanMaxValues` | MIN < MAX |
| `DefaultWidthIs1280` | Largeur par défaut = 1280 |
| `DefaultHeightIs720` | Hauteur par défaut = 720 |
| `DefaultTitleIsVecna` | Titre par défaut = "Vecna" |
| `DefaultIsResizable` | Redimensionnable par défaut |
| `CustomDimensions` | Dimensions personnalisées |
| `CustomTitle` | Titre personnalisé |
| `NonResizableWindow` | Fenêtre non-redimensionnable |
| `DefaultConfigIsWithinValidRange` | Config par défaut valide |
| `Resolution720pIsWithinValidRange` | 720p supporté |
| `Resolution1080pIsWithinValidRange` | 1080p supporté |
| `Resolution4KIsWithinValidRange` | 4K supporté |
| `EmptyTitleIsAllowed` | Titre vide autorisé |
| `LongTitleIsAllowed` | Titre long autorisé |
| `UnicodeTitle` | Titre Unicode supporté |

### QueueFamilyIndicesTest (8 tests)

| Test | Description |
|------|-------------|
| `DefaultConstructedIsIncomplete` | Structure par défaut est incomplète |
| `WithOnlyGraphicsFamilyIsIncomplete` | Avec seulement graphicsFamily, isComplete() retourne false |
| `WithOnlyPresentFamilyIsIncomplete` | Avec seulement presentFamily, isComplete() retourne false |
| `WithBothFamiliesIsComplete` | Avec les deux families définies, isComplete() retourne true |
| `FamiliesCanBeAnyValidIndex` | Supporte index 0, 5, UINT32_MAX pour les deux families |
| `ResetToIncomplete` | Peut être remis à nullopt |
| `CopyBehavior` | Copie indépendante de l'original |
| `DefaultConstructedFamiliesAreNullopt` | graphicsFamily et presentFamily sont nullopt par défaut |

## Composants non testés (tests d'intégration)

Les composants suivants nécessitent un environnement runtime complet et ne sont pas couverts par les tests unitaires :

| Composant | Raison | Parties testables |
|-----------|--------|-------------------|
| `Window` (création) | Nécessite GLFW initialisé et un display | `Window::Config` testé |
| `GLFWContext` | Singleton avec effets de bord globaux | - |
| `VulkanInstance` | Nécessite Vulkan runtime et GPU | - |
| `VulkanDevice` | Nécessite Vulkan runtime et GPU | `QueueFamilyIndices` testé |
| `Application` | Dépend de Window et VulkanInstance | - |

Ces composants sont validés manuellement ou via des tests d'intégration.

## Ajouter de nouveaux tests

### 1. Créer le fichier de test

```cpp
// tests/Module/MyClassTest.cpp
#include <gtest/gtest.h>
#include "Vecna/Module/MyClass.hpp"

using namespace Vecna::Module;

TEST(MyClassTest, TestName) {
    // Arrange
    MyClass obj;

    // Act
    auto result = obj.doSomething();

    // Assert
    EXPECT_EQ(result, expectedValue);
}
```

### 2. Ajouter au CMakeLists.txt

```cmake
# tests/CMakeLists.txt
set(TEST_SOURCES
    Core/LoggerTest.cpp
    Core/WindowConfigTest.cpp
    Module/MyClassTest.cpp  # Nouveau
)
```

### 3. Ajouter les sources si nécessaire

```cmake
target_sources(vecna_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Module/MyClass.cpp  # Si le test en a besoin
)
```

## Conventions

### Nommage des tests

- **Test suite** : `NomClasseTest` (ex: `LoggerTest`, `WindowConfigTest`)
- **Test case** : Description en PascalCase (ex: `DefaultWidthIs1280`)

### Structure AAA

```cpp
TEST(Suite, TestName) {
    // Arrange - Setup

    // Act - Exécution

    // Assert - Vérification
}
```

### Fixtures

Utiliser `TEST_F` avec une classe fixture pour partager le setup/teardown :

```cpp
class MyFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup avant chaque test
    }

    void TearDown() override {
        // Cleanup après chaque test
    }
};

TEST_F(MyFixture, TestName) {
    // Test utilisant la fixture
}
```

## CI/CD

Les tests sont exécutés automatiquement dans le pipeline CI (`.github/workflows/ci.yml`) sur :
- Windows (MSVC)
- Linux (GCC)
- macOS (Clang)

Pour activer les tests dans CI, la variable `VECNA_BUILD_TESTS=ON` est passée à CMake.
