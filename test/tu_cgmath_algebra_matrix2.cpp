#include <gtest/gtest.h>
#include "../src/cgmath/algebra_matrix2.h"
#include "../src/cgmath/TMatrix2.h"

TEST(TEST_cgmath_matrix2, EigenSystem) {
    // Legacy mat2 style
    mat2 mm2 = {{1.0f, 2.0f}, {3.0f, 2.0f}};
    float eigenvectors[2][2];
    float eigenvalues[2];
    mat2_solve_eigensystem(mm2, eigenvectors[0], eigenvectors[1], eigenvalues);
}
