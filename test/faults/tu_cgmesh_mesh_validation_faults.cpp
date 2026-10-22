#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/cgmesh/mesh.h"
#include "../../src/cgmesh/mesh_validation.h"
#include "thread_allocations.h"

// ===========================================================================
//  Memoire de ValidateMesh (option A, jalon A2)
// ===========================================================================
//
// Pic des octets demandes et des blocs vivants pendant ValidateMesh, test
// d'auto-intersection compris, sur un tore ferme de F = 2^17 + 1 024 triangles :
// juste au-dela d'une puissance de 2, ou une croissance par doublement gaspille
// le plus. Les seuils sont calibres sous MSVC Release (croissance des vecteurs
// x 1,5) ; libstdc++ croit par doublement (plan d'execution de la
// microarchitecture, §15.5). Sous une bibliotheque standard de debogage, les
// allocations ne sont pas celles du code mesure : test saute.

namespace
{
    constexpr unsigned kRings = 258, kSides = 256;   // F = 2 x 258 x 256 = 132 096

    // Seuils au pic, rapportes a F. Octets demandes : 1,10 x la mesure du validateur (177,3) ;
    // sans la reserve de la liste de triangles, le pic monte a 206. Blocs vivants : la grille de
    // soudure en tient deux par sommet, soit F sur une surface fermee (V = F / 2) ; une copie des
    // faces en vecteurs en ajoute F / 2 au pic.
    constexpr double kMaxBytesPerTriangle = 195.0;
    constexpr double kMaxBlocksPerTriangle = 1.25;

    Mesh torus()
    {
        const double pi = std::acos(-1.0), R = 3.0, r = 1.0;
        std::vector<float> v;
        v.reserve(3u * kRings * kSides);
        for (unsigned i = 0; i < kRings; ++i)
            for (unsigned j = 0; j < kSides; ++j)
            {
                const double u = 2 * pi * i / kRings, w = 2 * pi * j / kSides;
                v.push_back(static_cast<float>((R + r * std::cos(w)) * std::cos(u)));
                v.push_back(static_cast<float>((R + r * std::cos(w)) * std::sin(u)));
                v.push_back(static_cast<float>(r * std::sin(w)));
            }
        std::vector<unsigned> f;
        f.reserve(6u * kRings * kSides);
        auto at = [](unsigned i, unsigned j) { return (i % kRings) * kSides + (j % kSides); };
        for (unsigned i = 0; i < kRings; ++i)
            for (unsigned j = 0; j < kSides; ++j)
            {
                const unsigned a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
                f.insert(f.end(), {a, b, c, a, c, d});
            }
        Mesh m;
        m.SetVertices(static_cast<unsigned>(v.size() / 3), v.data());
        m.SetFaces(static_cast<unsigned>(f.size() / 3), 3, f.data());
        return m;
    }
}

TEST(TEST_cgmesh_mesh_validation_faults, peak_memory_per_triangle_is_bounded)
{
    if (DebugStandardLibrary())
        GTEST_SKIP() << "debug standard library: allocations differ from the measured code";
    const Mesh m = torus();
    const double F = static_cast<double>(m.GetNFaces());
    ASSERT_EQ(m.GetNFaces(), 2u * kRings * kSides);

    MeshValidationReport report;
    const ThreadAllocationPeak peak = MeasureThreadAllocationPeak([&] { report = ValidateMesh(m); });
    ASSERT_TRUE(report.IsValid());
    ASSERT_TRUE(report.selfIntersectionChecked);
    ASSERT_FALSE(report.selfIntersectionTruncated);

    const double bytesPerTriangle = static_cast<double>(peak.bytes) / F;
    const double blocksPerTriangle = static_cast<double>(peak.blocks) / F;
    RecordProperty("peak_bytes_per_triangle", std::to_string(bytesPerTriangle));
    RecordProperty("peak_blocks_per_triangle", std::to_string(blocksPerTriangle));
    std::printf("ValidateMesh peak: %zu bytes (%.1f per triangle), %zu blocks (%.3f per triangle), F = %.0f\n",
                peak.bytes, bytesPerTriangle, peak.blocks, blocksPerTriangle, F);
    EXPECT_LE(bytesPerTriangle, kMaxBytesPerTriangle);
    EXPECT_LT(blocksPerTriangle, kMaxBlocksPerTriangle);
}
