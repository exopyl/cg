#include <gtest/gtest.h>
#include "../src/cgmesh/geodesic_wrapper.h"
#include <vector>
#include <cmath>

// Helper to create a simple flat square mesh (2 triangles)
// V0(0,0,0)---V1(1,0,0)
// |           |
// V2(0,1,0)---V3(1,1,0)
static void setup_simple_mesh(GeodesicWrapper& wrapper) {
    float vertices[] = {
        0.0f, 0.0f, 0.0f, // V0
        1.0f, 0.0f, 0.0f, // V1
        0.0f, 1.0f, 0.0f, // V2
        1.0f, 1.0f, 0.0f  // V3
    };
    unsigned int faces[] = {
        0, 1, 2,
        1, 3, 2
    };
    wrapper.SetMesh(4, vertices, 2, faces);
}

TEST(GeodesicWrapperTest, SetMeshAndInitialization) {
    // Context
    GeodesicWrapper wrapper;
    
    // Action
    setup_simple_mesh(wrapper);
    
    // Expectations
    EXPECT_EQ(wrapper.GetNVertices(), 4);
}

TEST(GeodesicWrapperTest, ComputeDistancesExact) {
    // Context
    GeodesicWrapper wrapper;
    setup_simple_mesh(wrapper);
    wrapper.AddSource(0); // Source at V0
    std::vector<float> distances(4);

    // Action
    bool success = wrapper.ComputeDistances(distances.data(), geodesic::GeodesicAlgorithmBase::EXACT);

    // Expectations
    EXPECT_TRUE(success);
    EXPECT_NEAR(distances[0], 0.0f, 1e-5f); // Distance to self
    EXPECT_NEAR(distances[1], 1.0f, 1e-5f); // Edge length
    EXPECT_NEAR(distances[2], 1.0f, 1e-5f); // Edge length
    EXPECT_NEAR(distances[3], std::sqrt(2.0f), 1e-5f); // Diagonal
}

TEST(GeodesicWrapperTest, ComputeDistancesDijkstra) {
    // Context
    GeodesicWrapper wrapper;
    setup_simple_mesh(wrapper);
    wrapper.AddSource(0); // Source at V0
    std::vector<float> distances(4);

    // Action
    bool success = wrapper.ComputeDistances(distances.data(), geodesic::GeodesicAlgorithmBase::DIJKSTRA);

    // Expectations
    EXPECT_TRUE(success);
    EXPECT_NEAR(distances[0], 0.0f, 1e-5f);
    EXPECT_NEAR(distances[1], 1.0f, 1e-5f);
    EXPECT_NEAR(distances[2], 1.0f, 1e-5f);
    // Dijkstra on vertices only might give 2.0 (0->1->3 or 0->2->3) or 1.414 if diagonal edge exists
    // In our case, edges are (0,1), (1,2), (2,0), (1,3), (3,2), (2,1)
    // Shortest path 0->1->3 has length 2.0. 
    // Wait, the diagonal edge is (1,2). Path 0->1->2 has length 2.0? No, 0->2 is an edge.
    // 0->1, 1->2, 2->0 are edges of F0.
    // 1->3, 3->2, 2->1 are edges of F1.
    // So paths from 0 to 3 are:
    // 0 -> 1 -> 3 (length 2)
    // 0 -> 2 -> 3 (length 2)
    EXPECT_NEAR(distances[3], 2.0f, 1e-5f); 
}

TEST(GeodesicWrapperTest, ComputeDistancesSubdivision) {
    // Context
    GeodesicWrapper wrapper;
    setup_simple_mesh(wrapper);
    wrapper.AddSource(0); // Source at V0
    std::vector<float> distances(4);

    // Action
    bool success = wrapper.ComputeDistances(distances.data(), geodesic::GeodesicAlgorithmBase::SUBDIVISION, 4);

    // Expectations
    EXPECT_TRUE(success);
    EXPECT_NEAR(distances[0], 0.0f, 1e-5f);
    EXPECT_NEAR(distances[1], 1.0f, 1e-5f);
    EXPECT_NEAR(distances[2], 1.0f, 1e-5f);
    // Subdivision algorithm should be closer to EXACT than Dijkstra
    EXPECT_NEAR(distances[3], std::sqrt(2.0f), 0.1f); 
}

TEST(GeodesicWrapperTest, MultipleSources) {
    // Context
    GeodesicWrapper wrapper;
    setup_simple_mesh(wrapper);
    wrapper.AddSource(0); // Source at V0
    wrapper.AddSource(3); // Source at V3
    std::vector<float> distances(4);

    // Action
    bool success = wrapper.ComputeDistances(distances.data(), geodesic::GeodesicAlgorithmBase::EXACT);

    // Expectations
    EXPECT_TRUE(success);
    EXPECT_NEAR(distances[0], 0.0f, 1e-5f); // At source 0
    EXPECT_NEAR(distances[3], 0.0f, 1e-5f); // At source 3
    EXPECT_NEAR(distances[1], 1.0f, 1e-5f); // Distance to V0 or V3 is 1.0
    EXPECT_NEAR(distances[2], 1.0f, 1e-5f); // Distance to V0 or V3 is 1.0
}
