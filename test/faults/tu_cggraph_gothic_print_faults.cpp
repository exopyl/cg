#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../src/cggraph/core/eval_context.h"
#include "../../src/cggraph/core/node.h"
#include "../../src/cggraph/nodes/catalog.h"
#include "../../src/cggraph/nodes/value_types.h"
#include "../../src/cgmesh/mesh.h"
#include "../../src/cgmesh/parameterized_gothic.h"
#include "thread_allocations.h"

// ===========================================================================
//  Noeud shape.gothic.window : la garde d'impression, comptee par allocations
// ===========================================================================
//
// La garde (CheckGothicExportAndPrintAtWidth) commence par CheckGothicExport,
// qui alloue des centaines de milliers de fois sur la fenetre par defaut : un
// seul appel se voit donc au compte des allocations du thread appelant, sans
// crochet dans le code de production. Le noeud est calcule directement
// (Compute) sur ce thread.

using namespace cggraph;
using namespace cggraph_nodes;

namespace
{

struct Computed
{
    bool ok = false;
    bool hasMesh = false;
    std::map<std::string, double> stats;
};

Computed ComputeNode (Node &node)
{
    EvalContext ctx;
    ValueList in (node.GetDesc ().inputs.size ());
    ValueList out (node.GetDesc ().outputs.size ());
    Computed c;
    c.ok = node.Compute (ctx, in, out);
    c.hasMesh = !out.empty () && out[0].Get<Mesh> (Types ().mesh) != nullptr;
    std::vector<NodeStat> stats;
    node.PublishStats (stats);
    for (const NodeStat &s : stats)
        c.stats[s.name] = s.value;
    return c;
}

std::unique_ptr<Node> Window (bool printCheck)
{
    std::unique_ptr<Node> node = MakeNode ("shape.gothic.window");
    node->GetParams ().SetBool ("Print check", printCheck);
    return node;
}

long AllocationsOfCheckGothicExport ()
{
    ParameterizedGothicWindow shape;
    shape.Regenerate ();
    const Mesh &mesh = *shape.GetMesh ();
    return CountThreadAllocations ([&] { CheckGothicExport (shape, mesh); });
}

long AllocationsOfTheShapeAlone ()
{
    return CountThreadAllocations ([] {
        ParameterizedGothicWindow shape;
        shape.Regenerate ();
        std::unique_ptr<Mesh> mesh (shape.TakeMesh ());
    });
}

} // namespace

// C2 : booleen faux, la garde n'est jamais appelee. Tout appel de la garde paie
// au moins un CheckGothicExport ; le seuil est la moitie de ce compte. Booleen
// faux, le surcout du noeud sur la forme seule reste dessous ; temoin, booleen
// vrai sans largeur, le surcout sur le booleen faux le depasse. Un premier
// calcul, non compte, pose les initialisations statiques.
TEST (TEST_cggraph_gothic_print_faults, print_check_off_never_runs_the_export_guard)
{
    const long geometry = AllocationsOfCheckGothicExport ();
    const long shape = AllocationsOfTheShapeAlone ();
    ASSERT_GT (geometry, 1000);

    std::unique_ptr<Node> warmUp = Window (true);
    ComputeNode (*warmUp);
    std::unique_ptr<Node> off = Window (false);
    std::unique_ptr<Node> on = Window (true);
    Computed computedOff, computedOn;
    const long allocOff = CountThreadAllocations ([&] { computedOff = ComputeNode (*off); });
    const long allocOn = CountThreadAllocations ([&] { computedOn = ComputeNode (*on); });
    RecordProperty ("allocations_check_gothic_export", std::to_string (geometry));
    RecordProperty ("allocations_shape_alone", std::to_string (shape));
    RecordProperty ("allocations_node_check_off", std::to_string (allocOff));
    RecordProperty ("allocations_node_check_on_unset", std::to_string (allocOn));
    ASSERT_TRUE (computedOff.ok);
    ASSERT_TRUE (computedOn.ok);

    EXPECT_LT (allocOff - shape, geometry / 2);
    EXPECT_GT (allocOn - allocOff, geometry / 2);
    EXPECT_EQ (computedOff.stats.count ("print.scope"), 0u);
    EXPECT_EQ (computedOn.stats.count ("print.scope"), 1u);
}

// Une exception sortie de la garde (ici pendant son verdict geometrique, que
// CheckGothicExportAndPrint laisse passer) ne fait pas echouer le noeud : le
// maillage sort, print.error vaut 1.
TEST (TEST_cggraph_gothic_print_faults, an_exception_in_the_print_check_never_fails_the_node)
{
    if (DebugStandardLibrary ())
        GTEST_SKIP () << "debug standard library: noexcept operations allocate";
    const long geometry = AllocationsOfCheckGothicExport ();
    std::unique_ptr<Node> off = Window (false);
    const long allocOff = CountThreadAllocations ([&] { ComputeNode (*off); });
    ASSERT_GT (geometry, 1000);

    std::unique_ptr<Node> on = Window (true);
    on->GetParams ().SetBool ("Print width set", true);
    on->GetParams ().SetFloat ("Printed width mm", 50.0f);
    Computed computed;
    bool threw = false;
    ArmThreadAllocationFailure (allocOff + geometry / 2);
    try
    {
        computed = ComputeNode (*on);
    }
    catch (...)
    {
        threw = true;
    }
    const bool fired = DisarmThreadAllocationFailure ();
    ASSERT_TRUE (fired);
    ASSERT_FALSE (threw);
    EXPECT_TRUE (computed.ok);
    EXPECT_TRUE (computed.hasMesh);
    EXPECT_EQ (computed.stats["print.error"], 1.0);
    EXPECT_EQ (computed.stats["print.scope"], 0.0);
    EXPECT_EQ (computed.stats["print.evaluated"], 0.0);
    EXPECT_EQ (computed.stats["print.printable"], 0.0);
}
