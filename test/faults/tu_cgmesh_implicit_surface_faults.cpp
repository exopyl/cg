#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "../../src/cgmesh/half_edge.h"
#include "../../src/cgmesh/surface_implicit.h"
#include "../../src/cgmesh/surface_implicit_tandem.h"
#include "../../src/cgmesh/surface_implicit_triangulation.h"
#include <cgarchi/gothic_microarchitecture_solid.h>
#include "thread_allocations.h"

// ===========================================================================
//  ImplicitSurface face aux echecs d'allocation (option A, jalon A3)
// ===========================================================================
//
// L'operator new du programme (tu_cgarchi_gothic_print_faults.cpp) compte les blocs du
// thread appelant et fait echouer la k-ieme allocation. Les tampons de sortie, pris par
// malloc (l'appelant les libere par free), passent par l'allocateur de sortie
// (surface_implicit_triangulation.h), remplace ici par une sonde qui compte et fait
// echouer de meme. Sous une bibliotheque standard de debogage, des operations noexcept
// allouent : les tests qui injectent un echec dans operator new sont alors sautes.

namespace
{
    float ball (float x, float y, float z, void *)
    {
        return x * x + y * y + z * z - 0.36f;
    }

    std::unique_ptr<ImplicitSurface> makeSurface (bool tandem)
    {
        std::unique_ptr<ImplicitSurface> mc;
        if (tandem)
            mc = std::make_unique<ImplicitSurfaceTandem>();
        else
            mc = std::make_unique<ImplicitSurface>();
        mc->set_bbox(-1.f, -1.f, -1.f, 1.f, 1.f, 1.f);
        mc->set_resolution_per_unit(4);
        mc->set_eval_func(&ball);
        mc->set_value(0.f);
        return mc;
    }

    // Sorties brutes de get_triangulation ; seul cet appel est compte ou arme.
    struct Raw
    {
        bool ok = false;
        int nv = -1, nf = -1;
        float *v = nullptr;
        unsigned int *f = nullptr;
        void run (ImplicitSurface &mc) { ok = mc.get_triangulation(&nv, &v, &nf, &f); }
        bool null () const { return nv == 0 && nf == 0 && v == nullptr && f == nullptr; }
        ~Raw () { std::free(v); std::free(f); }
    };

    struct Output
    {
        std::vector<float> vertices;
        std::vector<unsigned int> faces;
        explicit Output (const Raw &r)
            : vertices(r.v, r.v + 3 * (r.ok ? r.nv : 0)), faces(r.f, r.f + 3 * (r.ok ? r.nf : 0)) {}
    };

    // Sonde de l'allocateur de sortie : compte les allocations, fait echouer la
    // countdown-ieme, suit les blocs qu'elle a rendus et qu'on lui a rendus. Les sorties
    // d'un appel reussi sont liberees par free, hors de la sonde : `live` ne se lit
    // qu'apres un appel en echec, remis a zero avant.
    thread_local long tlOutputCount = 0;
    thread_local long tlOutputCountdown = 0;
    thread_local long tlOutputLive = 0;

    void *probeAllocate (std::size_t bytes)
    {
        ++tlOutputCount;
        if (tlOutputCountdown > 0 && --tlOutputCountdown == 0)
            return nullptr;
        void *block = std::malloc(bytes);
        if (block)
            ++tlOutputLive;
        return block;
    }

    void probeRelease (void *block)
    {
        if (block)
            --tlOutputLive;
        std::free(block);
    }

    class OutputProbe
    {
    public:
        OutputProbe ()
        {
            tlOutputCount = tlOutputCountdown = tlOutputLive = 0;
            ImplicitOutputAllocator probe;
            probe.allocate = &probeAllocate;
            probe.release = &probeRelease;
            m_previous = SetImplicitOutputAllocator(probe);
        }
        ~OutputProbe () { SetImplicitOutputAllocator(m_previous); }
        OutputProbe (const OutputProbe &) = delete;
        OutputProbe &operator= (const OutputProbe &) = delete;

    private:
        ImplicitOutputAllocator m_previous;
    };

    struct FieldFailure {};

    // Boule qui leve FieldFailure a la (remaining + 1)-ieme evaluation.
    float throwingBall (float x, float y, float z, void *data)
    {
        long &remaining = *static_cast<long *>(data);
        if (remaining-- <= 0)
            throw FieldFailure{};
        return ball(x, y, z, nullptr);
    }
}

// Chaque allocation par operator new d'une triangulation, a tour de role, echoue :
// get_triangulation rend faux, des sorties nulles, et aucun bloc pris par operator new
// pendant l'appel ne reste vivant (accumulateur, maillage en demi-aretes, donnees d'aretes
// du tandem) ; un appel suivant redonne la triangulation de reference. Les tampons de
// sortie (malloc) ne sont pas encore pris quand operator new echoue ; leurs propres
// echecs sont l'objet de output_buffer_failures_are_reported.
// Exception : le constructeur de Che_mesh (half_edge.cpp), qui alloue ses trois cartes
// par new nu et perd les premieres si une suivante echoue, hors du perimetre de
// l'option A (debt_cgmesh.md). Ses allocations suivent celles de l'accumulateur du
// tandem et de l'objet Che_mesh ; leur nombre est mesure.
TEST(TEST_cgmesh_implicit_surface_faults, allocation_failures_are_reported)
{
    if (DebugStandardLibrary())
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    for (const bool tandem : {false, true})
    {
        SCOPED_TRACE(tandem ? "ImplicitSurfaceTandem" : "ImplicitSurface");
        std::unique_ptr<ImplicitSurface> reference = makeSurface(tandem);
        Raw first;
        const long allocations = CountThreadAllocations([&] { first.run(*reference); });
        ASSERT_TRUE(first.ok);
        const Output expected(first);
        ASSERT_FALSE(expected.faces.empty());
        ASSERT_GT(allocations, 0) << "no allocation goes through operator new";
        long cheMeshFirst = 0, cheMeshLast = -1;
        if (tandem)
        {
            const long inConstructor = CountThreadAllocations([] { Che_mesh m; });
            cheMeshFirst = 3;   // 1 : accumulateur du tandem, 2 : objet Che_mesh
            cheMeshLast = 2 + inConstructor;
        }

        for (long k = 1; k <= allocations; ++k)
        {
            SCOPED_TRACE("failing allocation " + std::to_string(k) + " of " + std::to_string(allocations));
            std::unique_ptr<ImplicitSurface> mc = makeSurface(tandem);
            Raw failed;
            ArmThreadAllocationFailure(k);
            const ThreadAllocationPeak memory = MeasureThreadAllocationPeak([&] { failed.run(*mc); });
            ASSERT_TRUE(DisarmThreadAllocationFailure());
            EXPECT_FALSE(failed.ok);
            EXPECT_TRUE(failed.null());
            if (k < cheMeshFirst || k > cheMeshLast)
            {
                EXPECT_EQ(memory.liveBytes, 0u);
                EXPECT_EQ(memory.liveBlocks, 0u);
            }
            Raw again;
            again.run(*mc);
            ASSERT_TRUE(again.ok);
            const Output output(again);
            EXPECT_EQ(output.vertices, expected.vertices);
            EXPECT_EQ(output.faces, expected.faces);
        }
    }
}

// Une triangulation reussie ne laisse vivant aucun bloc pris par operator new : ses
// sorties sont des tampons malloc, rendus a l'appelant.
TEST(TEST_cgmesh_implicit_surface_faults, a_triangulation_leaves_no_block_alive)
{
    for (const bool tandem : {false, true})
    {
        SCOPED_TRACE(tandem ? "ImplicitSurfaceTandem" : "ImplicitSurface");
        std::unique_ptr<ImplicitSurface> mc = makeSurface(tandem);
        Raw r;
        const ThreadAllocationPeak memory = MeasureThreadAllocationPeak([&] { r.run(*mc); });
        ASSERT_TRUE(r.ok);
        ASSERT_GT(r.nf, 0);
        EXPECT_GT(memory.blocks, 0u);
        EXPECT_EQ(memory.liveBytes, 0u);
        EXPECT_EQ(memory.liveBlocks, 0u);
    }
}

// Chaque tampon de sortie, a tour de role, n'est pas obtenu : get_triangulation rend
// faux, des sorties nulles, rend le tampon deja pris et ne laisse aucun bloc vivant ;
// un appel suivant redonne la triangulation de reference.
TEST(TEST_cgmesh_implicit_surface_faults, output_buffer_failures_are_reported)
{
    for (const bool tandem : {false, true})
    {
        SCOPED_TRACE(tandem ? "ImplicitSurfaceTandem" : "ImplicitSurface");
        const OutputProbe probe;
        std::unique_ptr<ImplicitSurface> reference = makeSurface(tandem);
        Raw first;
        first.run(*reference);
        ASSERT_TRUE(first.ok);
        const Output expected(first);
        ASSERT_FALSE(expected.faces.empty());
        const long outputs = tlOutputCount;
        ASSERT_EQ(outputs, 2) << "vertex and face buffers";

        for (long k = 1; k <= outputs; ++k)
        {
            SCOPED_TRACE("failing output buffer " + std::to_string(k));
            std::unique_ptr<ImplicitSurface> mc = makeSurface(tandem);
            Raw failed;
            tlOutputLive = 0;
            tlOutputCountdown = k;
            const ThreadAllocationPeak memory = MeasureThreadAllocationPeak([&] { failed.run(*mc); });
            EXPECT_EQ(tlOutputCountdown, 0) << "the armed buffer was not requested";
            tlOutputCountdown = 0;
            EXPECT_FALSE(failed.ok);
            EXPECT_TRUE(failed.null());
            EXPECT_EQ(tlOutputLive, 0);
            EXPECT_EQ(memory.liveBytes, 0u);
            EXPECT_EQ(memory.liveBlocks, 0u);
            Raw again;
            again.run(*mc);
            ASSERT_TRUE(again.ok);
            const Output output(again);
            EXPECT_EQ(output.vertices, expected.vertices);
            EXPECT_EQ(output.faces, expected.faces);
        }
    }
}

// Une exception levee par un rappel (ici, le champ) traverse get_triangulation, qui
// libere d'abord son accumulateur (get_triangulation_post, celui du tandem compris) :
// aucun bloc ne reste vivant, et l'objet triangule ensuite comme avant.
TEST(TEST_cgmesh_implicit_surface_faults, an_exception_from_a_callback_leaves_nothing_alive)
{
    for (const bool tandem : {false, true})
    {
        SCOPED_TRACE(tandem ? "ImplicitSurfaceTandem" : "ImplicitSurface");
        std::unique_ptr<ImplicitSurface> reference = makeSurface(tandem);
        Raw first;
        first.run(*reference);
        ASSERT_TRUE(first.ok);
        const Output expected(first);

        std::unique_ptr<ImplicitSurface> mc = makeSurface(tandem);
        long remaining = 400;   // 729 noeuds : l'exception tombe apres des faces
        mc->set_eval_func(&throwingBall);
        mc->set_eval_data(&remaining);
        Raw failed;
        bool thrown = false;
        const ThreadAllocationPeak memory = MeasureThreadAllocationPeak([&] {
            try
            {
                failed.run(*mc);
            }
            catch (const FieldFailure &)
            {
                thrown = true;
            }
        });
        ASSERT_TRUE(thrown);
        EXPECT_TRUE(failed.null());
        EXPECT_GT(memory.blocks, 0u);
        EXPECT_EQ(memory.liveBytes, 0u);
        EXPECT_EQ(memory.liveBlocks, 0u);

        remaining = 1L << 30;
        Raw again;
        again.run(*mc);
        ASSERT_TRUE(again.ok);
        const Output output(again);
        EXPECT_EQ(output.vertices, expected.vertices);
        EXPECT_EQ(output.faces, expected.faces);
    }
}

// Une couche d'exactement ImplicitSurface::kMaxLayerNodes noeuds (16 384 x 16 384) est
// acceptee : compute demande ses caches. La premiere allocation, armee, echoue ; sans
// elle, les caches prendraient 8 Gio.
TEST(TEST_cgmesh_implicit_surface_faults, a_layer_of_exactly_the_node_ceiling_is_accepted)
{
    if (DebugStandardLibrary())
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    static_assert(ImplicitSurface::kMaxLayerNodes == std::size_t(16384) * 16384, "ceiling");
    ImplicitSurface mc;
    mc.set_bbox(0.f, 0.f, 0.f, 16383.f, 16383.f, 1.f);   // 16 384 x 16 384 noeuds par couche
    mc.set_resolution_per_unit(1);
    mc.set_eval_func(&ball);
    ArmThreadAllocationFailure(1);
    bool computed = true;
    const long allocations = CountThreadAllocations([&] { computed = mc.compute(1); });
    EXPECT_TRUE(DisarmThreadAllocationFailure());
    EXPECT_FALSE(computed);
    EXPECT_EQ(allocations, 1);
}

// Une couche de plus de ImplicitSurface::kMaxLayerNodes noeuds est refusee avant toute
// allocation : la premiere allocation, armee pour echouer, n'a jamais lieu.
TEST(TEST_cgmesh_implicit_surface_faults, an_oversized_resolution_is_refused_before_any_allocation)
{
    if (DebugStandardLibrary())
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    ImplicitSurface mc;
    mc.set_bbox(0.f, 0.f, 0.f, 16384.f, 16384.f, 1.f);   // 16 385 x 16 385 noeuds par couche
    mc.set_resolution_per_unit(1);
    mc.set_eval_func(&ball);
    ArmThreadAllocationFailure(1);
    bool computed = true;
    const long allocations = CountThreadAllocations([&] { computed = mc.compute(1); });
    EXPECT_FALSE(DisarmThreadAllocationFailure());
    EXPECT_FALSE(computed);
    EXPECT_EQ(allocations, 0);
}

// GridIsoExtractor : un echec d'allocation des caches de compute (le premier, valeurs,
// ou le second, indices) donne CacheAllocation, sans evaluer le champ ; un autre objet
// extrait ensuite normalement.
TEST(TEST_cgmesh_implicit_surface_faults, grid_extractor_reports_a_cache_allocation_failure)
{
    if (DebugStandardLibrary())
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    using cgarchi::microarch::GridIsoExtractor;
    auto field = [](float x, float y, float z, void *) {
        return (x - 20.f) * (x - 20.f) + (y - 20.f) * (y - 20.f) + (z - 20.f) * (z - 20.f) - 100.f;
    };
    const int lo[3] = {0, 0, 0}, hi[3] = {40, 40, 40};
    for (const long k : {1L, 2L})
    {
        SCOPED_TRACE(k == 1 ? "value cache refused" : "index cache refused");
        GridIsoExtractor e(field, nullptr, std::size_t(1) << 22, std::size_t(1) << 28);
        ArmThreadAllocationFailure(k);
        const GridIsoExtractor::Status status = e.Run(lo, hi);
        EXPECT_TRUE(DisarmThreadAllocationFailure());
        EXPECT_EQ(status, GridIsoExtractor::Status::CacheAllocation);
        EXPECT_EQ(e.Evaluations(), 0u);
        EXPECT_TRUE(e.Faces().empty());
    }
    GridIsoExtractor e(field, nullptr, std::size_t(1) << 22, std::size_t(1) << 28);
    EXPECT_EQ(e.Run(lo, hi), GridIsoExtractor::Status::Ok);
    EXPECT_FALSE(e.Faces().empty());
}
