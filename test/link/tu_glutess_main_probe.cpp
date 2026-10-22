#include <gtest/gtest.h>

#include "../../src/cgmesh/mesh.h"

// Programme lie a cgmesh PUIS a gtest_main (test/CMakeLists.txt) : la bibliotheque cgmesh ne
// fournit pas de main. Si elle en fournissait un, l'editeur de liens le prendrait a la place de
// celui de gtest_main, ce test ne s'executerait pas, et la ligne « [  PASSED  ] 1 test » que
// CTest attend (PASS_REGULAR_EXPRESSION) manquerait, quel que soit le code de sortie.
TEST(TEST_glutess_main_probe, gtest_main_runs_the_tests)
{
    Mesh mesh;
    EXPECT_EQ(mesh.GetNVertices(), 0u);
}
