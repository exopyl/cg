#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <new>
#include <optional>
#include <string>

#include "../../src/cgmesh/parameterized_gothic.h"
#include "../../src/cgmesh/print_validation.h"

// ===========================================================================
//  Garde d'export face a une exception HORS DOMAINE
// ===========================================================================
//
// Une std::bad_alloc est injectee au milieu du controle d'impression, sans
// crochet dans le code de production : l'operator new global du programme de
// test est remplace, et ne leve que s'il est ARME sur le thread appelant, a la
// n-ieme allocation de ce thread, puis, en mode persistant, a toutes les
// suivantes. Desarme, il se comporte comme malloc / free. Les autres threads
// (fils de calcul) ne sont jamais touches. Ce remplacement vaut pour tout le
// programme : d'ou l'executable a part (TU_faults, test/CMakeLists.txt).
//
// Le rang d'injection est mesure sur la configuration courante ; sous une
// bibliotheque standard de debogage (MSVC _ITERATOR_DEBUG_LEVEL > 0,
// _GLIBCXX_DEBUG), des operations noexcept allouent (proxies d'iterateurs) et
// un echec y finirait en std::terminate : les tests sont alors sautes.

namespace
{
    thread_local long tlCountdown = 0;     // > 0 : l'allocation qui le fait passer a 0 leve
    thread_local bool tlPersistent = false; // apres elle, toutes les allocations levent
    thread_local bool tlFailing = false;
    thread_local long tlCount = 0;         // allocations comptees
    thread_local bool tlCounting = false;

    void disarm()
    {
        tlCountdown = 0;
        tlPersistent = false;
        tlFailing = false;
    }

#if defined(_DEBUG) || defined(_GLIBCXX_DEBUG) || (defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL > 0)
    constexpr bool kDebugLibrary = true;
#else
    constexpr bool kDebugLibrary = false;
#endif
}

void *operator new (std::size_t size)
{
    if (tlCounting)
        ++tlCount;
    if (tlFailing)
        throw std::bad_alloc();
    if (tlCountdown > 0 && --tlCountdown == 0)
    {
        tlFailing = tlPersistent;
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void operator delete (void *p) noexcept { std::free(p); }
void operator delete (void *p, std::size_t) noexcept { std::free(p); }

namespace
{
    template <class F>
    long allocationsDuring(F &&f)
    {
        tlCount = 0;
        tlCounting = true;
        f();
        tlCounting = false;
        return tlCount;
    }

    struct Window
    {
        ParameterizedGothicWindow shape;
        std::unique_ptr<Mesh>     mesh;

        Window()
        {
            shape.Regenerate();
            mesh.reset(shape.TakeMesh());
        }
    };

    PrintConstraints at(double mmPerUnit)
    {
        PrintConstraints c;
        c.mmPerUnit = mmPerUnit;
        return c;
    }
}

TEST(TEST_cgmesh_gothic_print_faults, internal_error_during_the_print_check_never_blocks_an_allowed_export)
{
    if (kDebugLibrary)
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    const Window w;
    const PrintConstraints c = at(0.1);
    const long geometry = allocationsDuring([&] { CheckGothicExport(w.shape, *w.mesh); });
    const long all = allocationsDuring([&] { CheckGothicExportAndPrint(w.shape, *w.mesh, c); });
    const long again = allocationsDuring([&] { CheckGothicExportAndPrint(w.shape, *w.mesh, c); });
    RecordProperty("allocations_geometry", std::to_string(geometry));
    RecordProperty("allocations_export_and_print", std::to_string(all));
    ASSERT_EQ(all, again) << "allocation count of the calling thread is not reproducible";
    ASSERT_GT(all, geometry + 100);

    // Echec au milieu de la mesure d'impression.
    tlCountdown = geometry + (all - geometry) / 2;
    std::optional<GothicPrintVerdict> v;
    bool threw = false;
    try
    {
        v = CheckGothicExportAndPrint(w.shape, *w.mesh, c);
    }
    catch (...)
    {
        threw = true;
    }
    const bool fired = (tlCountdown == 0);
    disarm();
    ASSERT_TRUE(fired);
    ASSERT_FALSE(threw);
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(v->ExportAllowed());
    EXPECT_EQ(v->check.scope, GothicPrintScope::NotRun);
    EXPECT_EQ(v->check.reason, "print check internal error");
    EXPECT_NE(v->printError.find("bad_alloc"), std::string::npos) << v->printError;
    EXPECT_FALSE(v->check.print.evaluated);
    EXPECT_FALSE(v->IsPrintable());
}

TEST(TEST_cgmesh_gothic_print_faults, internal_error_while_judging_the_geometry_propagates)
{
    // Sans verdict geometrique, l'export n'est pas juge : l'exception sort.
    if (kDebugLibrary)
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    const Window w;
    const long geometry = allocationsDuring([&] { CheckGothicExport(w.shape, *w.mesh); });
    ASSERT_GT(geometry, 2);
    tlCountdown = geometry / 2;
    EXPECT_THROW(CheckGothicExportAndPrint(w.shape, *w.mesh, at(0.1)), std::bad_alloc);
    disarm();
}

TEST(TEST_cgmesh_gothic_print_faults, memory_exhausted_during_the_print_check_still_returns_a_verdict)
{
    // A partir du milieu de la mesure, TOUTE allocation du thread echoue,
    // celles du gestionnaire d'exception comprises : le verdict sort quand
    // meme, avec le repli litteral.
    if (kDebugLibrary)
        GTEST_SKIP() << "debug standard library: noexcept operations allocate";
    const Window w;
    const PrintConstraints c = at(0.1);
    const long geometry = allocationsDuring([&] { CheckGothicExport(w.shape, *w.mesh); });
    const long all = allocationsDuring([&] { CheckGothicExportAndPrint(w.shape, *w.mesh, c); });
    ASSERT_GT(all, geometry + 100);

    std::optional<GothicPrintVerdict> v;
    bool threw = false;
    tlPersistent = true;
    tlCountdown = geometry + (all - geometry) / 2;
    try
    {
        v = CheckGothicExportAndPrint(w.shape, *w.mesh, c);
    }
    catch (...)
    {
        threw = true;
    }
    const bool fired = tlFailing;
    disarm();
    ASSERT_TRUE(fired);
    ASSERT_FALSE(threw);
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(v->ExportAllowed());
    EXPECT_EQ(v->check.scope, GothicPrintScope::NotRun);
    EXPECT_EQ(v->printError, "out of memory");
    EXPECT_FALSE(v->check.print.evaluated);
    EXPECT_FALSE(v->IsPrintable());
}

// main propre : la bibliotheque cgmesh embarque un main (demo de glutess,
// extern/glutess/main.c) qui prendrait la place de celui de gtest_main.
int main (int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
