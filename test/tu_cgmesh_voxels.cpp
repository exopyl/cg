#include <gtest/gtest.h>
#include "../src/cgmesh/voxels.h"

TEST(TEST_cgmesh_voxels, Initialization) {
    Voxels v(10, 20, 30);
    EXPECT_EQ(v.get_nx(), 10);
    EXPECT_EQ(v.get_ny(), 20);
    EXPECT_EQ(v.get_nz(), 30);

    for (unsigned int i = 0; i < 10; ++i) {
        for (unsigned int j = 0; j < 20; ++j) {
            for (unsigned int k = 0; k < 30; ++k) {
                EXPECT_FALSE(v.is_activated(i, j, k));
                EXPECT_EQ(v.get_data(i, j, k), 0.0f);
                EXPECT_EQ(v.get_label(i, j, k), 0);
            }
        }
    }
}

TEST(TEST_cgmesh_voxels, Activation) {
    Voxels v(5, 5, 5);
    v.activate(1, 2, 3);
    EXPECT_TRUE(v.is_activated(1, 2, 3));
    EXPECT_FALSE(v.is_activated(0, 0, 0));

    v.inverse_activation();
    EXPECT_FALSE(v.is_activated(1, 2, 3));
    EXPECT_TRUE(v.is_activated(0, 0, 0));
}

TEST(TEST_cgmesh_voxels, DataAndLabels) {
    Voxels v(5, 5, 5);
    v.set_data(1, 1, 1, 12.5f);
    EXPECT_EQ(v.get_data(1, 1, 1), 12.5f);

    v.set_label(2, 2, 2, 100);
    EXPECT_EQ(v.get_label(2, 2, 2), 100);

    v.reset_labels();
    EXPECT_EQ(v.get_label(2, 2, 2), 0);
}

TEST(TEST_cgmesh_voxels, Threshold) {
    Voxels v(5, 5, 5);
    v.activate(1, 1, 1);
    v.set_data(1, 1, 1, 5.0f);
    
    v.activate(2, 2, 2);
    v.set_data(2, 2, 2, 15.0f);

    v.threshold_data(10.0f);
    
    EXPECT_FALSE(v.is_activated(1, 1, 1));
    EXPECT_TRUE(v.is_activated(2, 2, 2));
}

TEST(TEST_cgmesh_voxels, ExtremalValues) {
    Voxels v(5, 5, 5);
    v.activate(1, 1, 1);
    v.set_data(1, 1, 1, 5.0f);
    v.activate(2, 2, 2);
    v.set_data(2, 2, 2, 15.0f);
    v.activate(3, 3, 3);
    v.set_data(3, 3, 3, -10.0f);

    float min, max;
    v.get_extremal_values(&min, &max);
    EXPECT_EQ(min, -10.0f);
    EXPECT_EQ(max, 15.0f);
}

TEST(TEST_cgmesh_voxels, Dilation) {
    Voxels v(10, 10, 10);
    v.activate(5, 5, 5);
    v.set_data(5, 5, 5, 1.0f);
    
    v.dilation();
    
    // Check 6-neighbors
    EXPECT_TRUE(v.is_activated(4, 5, 5));
    EXPECT_TRUE(v.is_activated(6, 5, 5));
    EXPECT_TRUE(v.is_activated(5, 4, 5));
    EXPECT_TRUE(v.is_activated(5, 6, 5));
    EXPECT_TRUE(v.is_activated(5, 5, 4));
    EXPECT_TRUE(v.is_activated(5, 5, 6));
    
    // Check non-neighbor
    EXPECT_FALSE(v.is_activated(0, 0, 0));
}
