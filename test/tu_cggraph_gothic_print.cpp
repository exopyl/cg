#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../src/cggraph/core/evaluator.h"
#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/value_types.h"
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/parameterized_gothic.h"
#include "../src/cgmesh/print_validation.h"
#include "../src/cgmesh/profile2d.h"

// ===========================================================================
//  Controle d'impression dans le noeud shape.gothic.window (M4-d3)
// ===========================================================================
//
//   - booleen « Print check » faux (defaut) : aucune stat print.*, maillage et
//     stats gothic.* inchanges ; un document de graphe ecrit avant ces
//     parametres se relit et se calcule a l'identique ;
//   - booleen vrai : CheckGothicExportAndPrintAtWidth sur la baie du noeud,
//     stats egales au verdict natif ;
//   - le controle ne fait jamais echouer le noeud.
// Le compteur « aucun appel de la garde quand le booleen est faux » est dans
// TU_faults (test/faults/tu_cggraph_gothic_print_faults.cpp) : il compte les
// allocations du thread appelant.

using namespace cggraph;
using namespace cggraph_nodes;

namespace
{

const char *kBeforePrintCheck = "./test/data/graphs/gothic_window_before_m4d3.json";

enum class Relief
{
	Flat,
	Chamfer,
	Roll
};

std::string ReadText (const char *path)
{
	std::string text;
	std::FILE *f = std::fopen (path, "rb");
	if (!f)
		return text;
	std::fseek (f, 0, SEEK_END);
	const long size = std::ftell (f);
	std::fseek (f, 0, SEEK_SET);
	if (size > 0)
	{
		text.resize (static_cast<std::size_t> (size));
		if (std::fread (&text[0], 1, text.size (), f) != text.size ())
			text.clear ();
	}
	std::fclose (f);
	return text;
}

std::string WithoutCarriageReturns (std::string text)
{
	std::string out;
	out.reserve (text.size ());
	for (char c : text)
		if (c != '\r')
			out += c;
	return out;
}

std::map<std::string, double> StatsOf (Evaluator &evaluator, NodeId id)
{
	std::map<std::string, double> out;
	const std::map<NodeId, std::vector<NodeStat>> stats = evaluator.TakeRunStats ();
	const auto it = stats.find (id);
	if (it != stats.end ())
		for (const NodeStat &s : it->second)
			out[s.name] = s.value;
	return out;
}

std::size_t CountPrefixed (const std::map<std::string, double> &stats, const std::string &prefix)
{
	std::size_t n = 0;
	for (const auto &s : stats)
		if (s.first.rfind (prefix, 0) == 0)
			++n;
	return n;
}

// Une baie dans un graphe, avec son profil eventuel ; Run () l'evalue et rend
// le maillage et les stats du parcours.
struct WindowGraph
{
	Graph  graph;
	NodeId window = 0;
	NodeId profile = 0;
	Relief relief = Relief::Flat;
	std::shared_ptr<const Mesh> mesh;
	std::map<std::string, double> stats;

	explicit WindowGraph (Relief r) : relief (r)
	{
		window = graph.AddNode (MakeNode ("shape.gothic.window"));
		if (r == Relief::Chamfer)
		{
			profile = graph.AddNode (MakeNode ("profile.chamfer"));
			graph.Connect (profile, 0, window, 0);
		}
		else if (r == Relief::Roll)
		{
			profile = graph.AddNode (MakeNode ("profile.bar.roll"));
			graph.Connect (profile, 0, window, 1);
		}
	}

	ParamSet &Params () { return graph.FindNode (window)->GetParams (); }

	bool Run (Evaluator &evaluator)
	{
		EvalContext ctx;
		ValueList outputs;
		if (!evaluator.Evaluate (window, outputs, ctx).IsOk () || outputs.empty ())
			return false;
		mesh = outputs[0].Share<Mesh> (Types ().mesh);
		stats = StatsOf (evaluator, window);
		return mesh != nullptr;
	}

	bool Run ()
	{
		Evaluator evaluator (graph);
		return Run (evaluator);
	}

	// Le profil tel que le noeud amont le produit.
	std::optional<Profile2D> UpstreamProfile ()
	{
		if (profile == 0)
			return std::nullopt;
		Evaluator evaluator (graph);
		EvalContext ctx;
		ValueList outputs;
		if (!evaluator.Evaluate (profile, outputs, ctx).IsOk () || outputs.empty ())
			return std::nullopt;
		const Profile2D *p = outputs[0].Get<Profile2D> (relief == Relief::Chamfer ? Types ().splayProfile
		                                                                         : Types ().barProfile);
		if (p == nullptr)
			return std::nullopt;
		return *p;
	}
};

// La meme baie hors graphe : forme regeneree par cgmesh seul.
struct ReferenceWindow
{
	ParameterizedGothicWindow shape;
	std::optional<Profile2D>  profile;

	ReferenceWindow (Relief relief, const std::optional<Profile2D> &p, int recursion = 0) : profile (p)
	{
		for (Parameter &param : shape.GetParameters ())
			if (param.GetName () == "Recursion")
				param.SetInt (recursion);
		if (relief == Relief::Chamfer && profile)
			shape.SetSplayProfile (&*profile);
		if (relief == Relief::Roll && profile)
			shape.SetBarProfile (&*profile);
		shape.Regenerate ();
	}

	const Mesh &MeshRef () { return *shape.GetMesh (); }
};

void ExpectSameMesh (const Mesh &a, Mesh &b)
{
	ASSERT_EQ (a.GetNVertices (), b.GetNVertices ());
	ASSERT_EQ (a.GetNFaces (), b.GetNFaces ());
	EXPECT_EQ (a.GetVertices (), b.GetVertices ());
	Mesh &ma = const_cast<Mesh &> (a);
	for (unsigned int f = 0; f < b.GetNFaces (); ++f)
	{
		ASSERT_EQ (ma.GetFaceNVertices (f), b.GetFaceNVertices (f)) << "face " << f;
		for (int k = 0; k < b.GetFaceNVertices (f); ++k)
			ASSERT_EQ (ma.FaceAt (f)->GetVertex (k), b.FaceAt (f)->GetVertex (k)) << "face " << f;
	}
}

double ScopeCode (GothicPrintScope scope)
{
	switch (scope)
	{
	case GothicPrintScope::NotRun:            return 0.0;
	case GothicPrintScope::Complete:          return 1.0;
	case GothicPrintScope::ProfileNotChecked: return 2.0;
	}
	return -1.0;
}

// Les stats print.* qu'un verdict natif doit donner.
std::map<std::string, double> ExpectedPrintStats (const GothicPrintVerdict &v)
{
	std::map<std::string, double> out;
	const std::array<std::size_t, kPrintDefectCount> shown = PrintDisplayCounts (v.check.print);
	for (std::size_t k = 0; k < kPrintDefectCount; ++k)
		out[std::string ("print.") + PrintDefectName (static_cast<PrintDefect> (k))] = static_cast<double> (shown[k]);
	out["print.scope"] = ScopeCode (v.check.scope);
	out["print.printable"] = v.IsPrintable () ? 1.0 : 0.0;
	out["print.evaluated"] = v.check.print.evaluated ? 1.0 : 0.0;
	out["print.error"] = v.printError.empty () ? 0.0 : 1.0;
	return out;
}

std::map<std::string, double> PrintStatsOf (const std::map<std::string, double> &stats)
{
	std::map<std::string, double> out;
	for (const auto &s : stats)
		if (s.first.rfind ("print.", 0) == 0)
			out.insert (s);
	return out;
}

double Stat (const std::map<std::string, double> &stats, const std::string &name)
{
	const auto it = stats.find (name);
	return it == stats.end () ? -1.0 : it->second;
}

const std::size_t kPrintStatCount = kPrintDefectCount + 4;

} // namespace

TEST (TEST_cggraph_gothic_print, print_parameters_are_declared_off_by_default)
{
	std::unique_ptr<Node> node = MakeNode ("shape.gothic.window");
	ASSERT_NE (node, nullptr);
	EXPECT_EQ (node->GetDesc ().version, 1);
	EXPECT_EQ (node->GetParams ().GetCount (), 31u);

	const ParamSet &p = node->GetParams ();
	struct Expected
	{
		const char *name;
		ParamType   type;
		double      value;
	};
	const Expected expected[] = {
		{ "Print check", ParamType::Bool, 0.0 },
		{ "Print width set", ParamType::Bool, 0.0 },
		{ "Printed width mm", ParamType::Float, 0.0 },
		{ "Print min wall mm", ParamType::Float, kDefaultMinWallMm },
		{ "Print min hole mm", ParamType::Float, kDefaultMinHoleMm },
		{ "Print max tip mm", ParamType::Float, kDefaultMaxTipLengthMm },
		{ "Print envelope set", ParamType::Bool, 0.0 },
		{ "Print envelope x mm", ParamType::Float, 0.0 },
		{ "Print envelope y mm", ParamType::Float, 0.0 },
		{ "Print envelope z mm", ParamType::Float, 0.0 },
	};
	for (const Expected &e : expected)
	{
		SCOPED_TRACE (e.name);
		const ParamEntry *entry = p.FindEntry (e.name);
		ASSERT_NE (entry, nullptr);
		EXPECT_EQ (entry->role, ParamRole::Semantic);
		EXPECT_EQ (entry->visibility, ParamVisibility::Public);
		ASSERT_EQ (entry->value.type, e.type);
		if (e.type == ParamType::Bool)
			EXPECT_EQ (entry->value.boolValue, e.value != 0.0);
		else
			EXPECT_EQ (entry->value.floatValue, static_cast<float> (e.value));
	}
}

TEST (TEST_cggraph_gothic_print, print_check_off_publishes_no_print_stat_and_keeps_the_mesh)
{
	for (const Relief relief : { Relief::Flat, Relief::Chamfer, Relief::Roll })
	{
		SCOPED_TRACE (static_cast<int> (relief));
		WindowGraph g (relief);
		// Largeur et enveloppe fixees : sans le booleen, elles ne lancent rien.
		g.Params ().SetBool ("Print width set", true);
		g.Params ().SetFloat ("Printed width mm", 50.0f);
		g.Params ().SetBool ("Print envelope set", true);
		g.Params ().SetFloat ("Print envelope x mm", 10.0f);
		g.Params ().SetFloat ("Print envelope y mm", 10.0f);
		g.Params ().SetFloat ("Print envelope z mm", 10.0f);
		ASSERT_TRUE (g.Run ());

		EXPECT_EQ (CountPrefixed (g.stats, "print."), 0u);
		EXPECT_EQ (CountPrefixed (g.stats, "gothic."), kGothicBuildWarningCount);
		EXPECT_EQ (g.stats.size (), kGothicBuildWarningCount);

		ReferenceWindow ref (relief, g.UpstreamProfile ());
		ExpectSameMesh (*g.mesh, *ref.shape.GetMesh ());
	}
}

// Document ecrit par le code d'avant les parametres d'impression (fbd07c5 +
// M4-a a M4-d2) : profil de chanfrein et barre ronde, quatre baies (plate,
// chanfrein, barre, Recursion 2). Il se relit sans noeud incompatible, se
// calcule a l'identique de la forme cgmesh, ne publie que les stats gothic.*,
// et se re-sauve a l'octet.
TEST (TEST_cggraph_gothic_print, graph_documents_written_before_the_print_check_compute_as_before)
{
	const std::string document = WithoutCarriageReturns (ReadText (kBeforePrintCheck));
	ASSERT_FALSE (document.empty ()) << kBeforePrintCheck << " introuvable";

	Graph graph;
	const CatalogFactory factory;
	const LoadResult loaded = LoadGraph (document, factory, graph);
	ASSERT_TRUE (loaded.IsOk ()) << ToString (loaded.status) << " : " << loaded.detail;
	EXPECT_TRUE (loaded.incompatible.empty ());
	EXPECT_EQ (SaveGraph (graph), document);

	struct Case
	{
		NodeId window;
		Relief relief;
		NodeId profile;
		int    recursion;
	};
	const Case cases[] = { { 3, Relief::Flat, 0, 0 }, { 4, Relief::Chamfer, 1, 0 }, { 5, Relief::Roll, 2, 0 },
		                   { 6, Relief::Flat, 0, 2 } };
	Evaluator evaluator (graph);
	for (const Case &c : cases)
	{
		SCOPED_TRACE ("node " + std::to_string (c.window));
		const Node *node = graph.FindNode (c.window);
		ASSERT_NE (node, nullptr);
		ASSERT_EQ (node->GetDesc ().typeName, "shape.gothic.window");
		EXPECT_EQ (node->GetParams ().GetCount (), 21u);
		EXPECT_EQ (node->GetParams ().Find ("Print check"), nullptr);

		std::optional<Profile2D> profile;
		if (c.profile != 0)
		{
			EvalContext ctx;
			ValueList outputs;
			ASSERT_TRUE (evaluator.Evaluate (c.profile, outputs, ctx).IsOk ());
			const Profile2D *p = outputs[0].Get<Profile2D> (c.relief == Relief::Chamfer ? Types ().splayProfile
			                                                                           : Types ().barProfile);
			ASSERT_NE (p, nullptr);
			profile = *p;
		}

		EvalContext ctx;
		ValueList outputs;
		ASSERT_TRUE (evaluator.Evaluate (c.window, outputs, ctx).IsOk ());
		const std::shared_ptr<const Mesh> mesh = outputs[0].Share<Mesh> (Types ().mesh);
		ASSERT_NE (mesh, nullptr);
		const std::map<std::string, double> stats = StatsOf (evaluator, c.window);
		EXPECT_EQ (stats.size (), kGothicBuildWarningCount);
		EXPECT_EQ (CountPrefixed (stats, "gothic."), kGothicBuildWarningCount);
		if (c.recursion == 2)
		{
			EXPECT_EQ (Stat (stats, "gothic.depth_fallback"), 4.0);
		}

		ReferenceWindow ref (c.relief, profile, c.recursion);
		ExpectSameMesh (*mesh, *ref.shape.GetMesh ());
	}
	EXPECT_EQ (SaveGraph (graph), document);
}

TEST (TEST_cggraph_gothic_print, print_check_reports_the_native_verdict)
{
	struct Case
	{
		Relief relief;
		float  widthMm;
		float  maxTipMm;
		bool   envelope;
		float  side[3];
		double scope;
		double printable;
	};
	const Case cases[] = {
		{ Relief::Flat, 100.0f, 2.0f, true, { 218.0f, 123.0f, 250.0f }, 1.0, 1.0 },
		{ Relief::Chamfer, 50.0f, 0.3f, true, { 60.0f, 60.0f, 60.0f }, 2.0, 0.0 },
		{ Relief::Roll, 50.0f, 0.3f, false, { 0.0f, 0.0f, 0.0f }, 2.0, 0.0 },
	};
	bool sumsDifferSomewhere = false;
	for (const Case &c : cases)
	{
		SCOPED_TRACE (static_cast<int> (c.relief));
		WindowGraph g (c.relief);
		g.Params ().SetBool ("Print check", true);
		g.Params ().SetBool ("Print width set", true);
		g.Params ().SetFloat ("Printed width mm", c.widthMm);
		g.Params ().SetFloat ("Print max tip mm", c.maxTipMm);
		g.Params ().SetBool ("Print envelope set", c.envelope);
		g.Params ().SetFloat ("Print envelope x mm", c.side[0]);
		g.Params ().SetFloat ("Print envelope y mm", c.side[1]);
		g.Params ().SetFloat ("Print envelope z mm", c.side[2]);
		ASSERT_TRUE (g.Run ());

		ReferenceWindow ref (c.relief, g.UpstreamProfile ());
		ExpectSameMesh (*g.mesh, *ref.shape.GetMesh ());
		PrintConstraints printer;
		printer.minWallMm = static_cast<double> (static_cast<float> (kDefaultMinWallMm));
		printer.minHoleMm = static_cast<double> (static_cast<float> (kDefaultMinHoleMm));
		printer.maxTipLengthMm = static_cast<double> (c.maxTipMm);
		if (c.envelope)
			printer.envelopeMm = std::array<double, 3> { c.side[0], c.side[1], c.side[2] };
		const GothicPrintVerdict native =
			CheckGothicExportAndPrintAtWidth (ref.shape, ref.MeshRef (), printer, static_cast<double> (c.widthMm));
		ASSERT_TRUE (native.printError.empty ()) << native.printError;

		EXPECT_EQ (PrintStatsOf (g.stats), ExpectedPrintStats (native));
		EXPECT_EQ (g.stats.size (), kGothicBuildWarningCount + kPrintStatCount);
		EXPECT_EQ (Stat (g.stats, "print.scope"), c.scope);
		EXPECT_EQ (Stat (g.stats, "print.printable"), c.printable);
		EXPECT_EQ (Stat (g.stats, "print.evaluated"), 1.0);
		EXPECT_EQ (Stat (g.stats, "print.error"), 0.0);
		if (c.envelope && c.side[0] == 60.0f)
		{
			EXPECT_EQ (Stat (g.stats, "print.envelope_exceeded"), 1.0);
		}
		if (PrintDisplayCounts (native.check.print) != native.check.print.counts)
			sumsDifferSomewhere = true;
	}
	EXPECT_TRUE (sumsDifferSomewhere) << "aucun cas ne distingue les comptes affiches des comptes sommes";
}

TEST (TEST_cggraph_gothic_print, print_check_without_a_width_reports_the_scale_as_unset)
{
	WindowGraph g (Relief::Flat);
	g.Params ().SetBool ("Print check", true);
	g.Params ().SetFloat ("Printed width mm", 50.0f);   // largeur non fixee : ignoree
	ASSERT_TRUE (g.Run ());

	const std::map<std::string, double> print = PrintStatsOf (g.stats);
	EXPECT_EQ (print.size (), kPrintStatCount);
	EXPECT_EQ (Stat (print, "print.scope"), 0.0);
	EXPECT_EQ (Stat (print, "print.evaluated"), 0.0);
	EXPECT_EQ (Stat (print, "print.printable"), 0.0);
	EXPECT_EQ (Stat (print, "print.error"), 0.0);
	EXPECT_EQ (Stat (print, "print.scale_unset"), 1.0);
	for (std::size_t k = 0; k < kPrintDefectCount; ++k)
	{
		const PrintDefect kind = static_cast<PrintDefect> (k);
		if (kind != PrintDefect::ScaleUnset)
		{
			EXPECT_EQ (Stat (print, std::string ("print.") + PrintDefectName (kind)), 0.0) << PrintDefectName (kind);
		}
	}
}

TEST (TEST_cggraph_gothic_print, invalid_print_inputs_never_fail_the_node)
{
	const float nan = std::numeric_limits<float>::quiet_NaN ();
	struct Case
	{
		const char *what;
		bool  widthSet;
		float width;
		float minWall;
		bool  envelope;
	};
	const Case cases[] = {
		{ "width set without a value", true, 0.0f, 0.4f, false },
		{ "negative width", true, -5.0f, 0.4f, false },
		{ "non finite width", true, nan, 0.4f, false },
		{ "threshold out of domain, scale set", true, 50.0f, nan, false },
		{ "threshold out of domain, scale unset", false, 0.0f, -1.0f, false },
		{ "envelope set without sides", true, 50.0f, 0.4f, true },
	};
	ReferenceWindow ref (Relief::Flat, std::nullopt);
	for (const Case &c : cases)
	{
		SCOPED_TRACE (c.what);
		WindowGraph g (Relief::Flat);
		g.Params ().SetBool ("Print check", true);
		g.Params ().SetBool ("Print width set", c.widthSet);
		g.Params ().SetFloat ("Printed width mm", c.width);
		g.Params ().SetFloat ("Print min wall mm", c.minWall);
		g.Params ().SetBool ("Print envelope set", c.envelope);
		ASSERT_TRUE (g.Run ());
		ExpectSameMesh (*g.mesh, *ref.shape.GetMesh ());

		const std::map<std::string, double> print = PrintStatsOf (g.stats);
		EXPECT_EQ (print.size (), kPrintStatCount);
		EXPECT_EQ (Stat (print, "print.error"), 1.0);
		EXPECT_EQ (Stat (print, "print.scope"), 0.0);
		EXPECT_EQ (Stat (print, "print.evaluated"), 0.0);
		EXPECT_EQ (Stat (print, "print.printable"), 0.0);
	}
}

TEST (TEST_cggraph_gothic_print, a_print_parameter_change_recomputes_the_stats)
{
	WindowGraph g (Relief::Chamfer);
	Evaluator evaluator (g.graph);
	g.Params ().SetBool ("Print check", true);
	g.Params ().SetBool ("Print width set", true);
	g.Params ().SetFloat ("Printed width mm", 50.0f);
	ASSERT_TRUE (g.Run (evaluator));
	EXPECT_EQ (Stat (g.stats, "print.evaluated"), 1.0);
	EXPECT_EQ (Stat (g.stats, "print.scope"), 2.0);

	g.Params ().SetBool ("Print width set", false);
	ASSERT_TRUE (g.Run (evaluator));
	EXPECT_EQ (Stat (g.stats, "print.evaluated"), 0.0);
	EXPECT_EQ (Stat (g.stats, "print.scope"), 0.0);
	EXPECT_EQ (Stat (g.stats, "print.scale_unset"), 1.0);

	g.Params ().SetBool ("Print check", false);
	ASSERT_TRUE (g.Run (evaluator));
	EXPECT_EQ (CountPrefixed (g.stats, "print."), 0u);
}
