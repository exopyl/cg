#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include <cgarchi/classical_acanthus.h>
#include <cgarchi/classical_acanthus_template.h>

using namespace cgarchi::classical;

// ===========================================================================
//  Gabarits plans de la feuille d'acanthe (format cgarchi-acanthus-leaves/2)
// ===========================================================================

namespace
{

const nlohmann::json &Document ()
{
	static const nlohmann::json doc = nlohmann::json::parse (cgarchi::AcanthusLeavesJson ());
	return doc;
}

// Leve-t-il std::invalid_argument dont le message contient `fragment` ?
bool ThrowsWith (const std::function<void ()> &f, const std::string &fragment)
{
	try
	{
		f ();
	}
	catch (const std::invalid_argument &e)
	{
		if (std::string (e.what ()).find (fragment) != std::string::npos)
			return true;
		ADD_FAILURE () << "message sans « " << fragment << " » : " << e.what ();
		return false;
	}
	ADD_FAILURE () << "aucun refus, « " << fragment << " » attendu";
	return false;
}

// Document embarque modifie par `change`, relu par ParseLeafTemplates.
std::function<void ()> Mutated (const std::function<void (nlohmann::json &)> &change)
{
	return [change] {
		nlohmann::json doc = Document ();
		change (doc);
		detail::ParseLeafTemplates (doc.dump ());
	};
}

nlohmann::json &OliveLight (nlohmann::json &doc)
{
	return doc["styles"]["olive"]["light"];
}

struct Counts
{
	unsigned shellTriangles, shellVertices;
};

// Coque fermee d'une variante : deux faces, murs du bord hors couture.
Counts ShellOf (const detail::LeafTemplate &t, bool withStem)
{
	const unsigned v = withStem ? static_cast<unsigned> (t.vertices.size ()) : t.leafVertices;
	const unsigned tris = withStem ? static_cast<unsigned> (t.triangles.size ()) : t.leafTriangles;
	const detail::LeafTemplateRim &rim = withStem ? t.stemRim : t.leafRim;
	unsigned axis = 0;
	for (unsigned i = 0; i < v; ++i)
		axis += t.vertices[i].u == 0.0 ? 1u : 0u;
	return {4 * tris + 4 * static_cast<unsigned> (rim.edges.size ()), 2 * (2 * v - axis)};
}

} // namespace

TEST (TEST_cgarchi_classical_acanthus_template, the_counts_are_the_compiled_ones)
{
	struct Expected
	{
		detail::LeafStyle style;
		OrnamentDetail detail;
		Counts leaf, withStem;
	};
	const Expected cases[] = {
		{detail::LeafStyle::Olive, OrnamentDetail::Light, {1800, 902}, {1912, 958}},
		{detail::LeafStyle::Olive, OrnamentDetail::Heavy, {10296, 5150}, {11016, 5510}},
		{detail::LeafStyle::OliveMinor, OrnamentDetail::Light, {1300, 652}, {1380, 692}},
		{detail::LeafStyle::OliveMinor, OrnamentDetail::Heavy, {2996, 1500}, {3428, 1716}},
	};
	for (const Expected &c : cases)
	{
		const detail::LeafTemplate &t = detail::LeafTemplateOf (c.style, c.detail);
		const Counts leaf = ShellOf (t, false), stem = ShellOf (t, true);
		EXPECT_EQ (leaf.shellTriangles, c.leaf.shellTriangles);
		EXPECT_EQ (leaf.shellVertices, c.leaf.shellVertices);
		EXPECT_EQ (stem.shellTriangles, c.withStem.shellTriangles);
		EXPECT_EQ (stem.shellVertices, c.withStem.shellVertices);
	}
}

TEST (TEST_cgarchi_classical_acanthus_template, the_prefix_and_the_rims_are_consistent)
{
	for (detail::LeafStyle style : {detail::LeafStyle::Olive, detail::LeafStyle::OliveMinor})
		for (OrnamentDetail level : {OrnamentDetail::Light, OrnamentDetail::Heavy})
		{
			const detail::LeafTemplate &t = detail::LeafTemplateOf (style, level);
			ASSERT_LT (t.leafVertices, t.vertices.size ());
			ASSERT_LT (t.leafTriangles, t.triangles.size ());
			for (unsigned i = 0; i < t.vertices.size (); ++i)
				EXPECT_EQ (i < t.leafVertices, t.vertices[i].y >= 0.0) << i;
			for (unsigned k = t.leafTriangles; k < t.triangles.size (); ++k)
				EXPECT_EQ (t.triangleRegion[k], t.stemRegion);
			// Couture : sommets d'axe seulement, du pied a l'apex pour la feuille, de -1 a l'apex avec la bande.
			for (const detail::LeafTemplateRim *rim : {&t.leafRim, &t.stemRim})
			{
				ASSERT_FALSE (rim->seam.empty ());
				for (unsigned v : rim->seam)
					EXPECT_EQ (t.vertices[v].u, 0.0);
			}
			double lowLeaf = 1.0, lowStem = 1.0;
			for (unsigned v : t.leafRim.seam)
				lowLeaf = std::min (lowLeaf, t.vertices[v].y);
			for (unsigned v : t.stemRim.seam)
				lowStem = std::min (lowStem, t.vertices[v].y);
			EXPECT_EQ (lowLeaf, 0.0);
			EXPECT_EQ (lowStem, -1.0);
			EXPECT_GT (t.capHalfWidth, 0.3);
			EXPECT_EQ (t.fade[0], 1.0);
			EXPECT_EQ (t.fade[1], 1.2);
		}
	EXPECT_NEAR (detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::Heavy).capHalfWidth, 0.625, 1e-9);
}

TEST (TEST_cgarchi_classical_acanthus_template, every_guard_is_seen)
{
	EXPECT_TRUE (ThrowsWith ([] { detail::LeafTemplateOf (detail::LeafStyle::Olive, OrnamentDetail::None); }, "None"));
	EXPECT_TRUE (ThrowsWith ([] { detail::ParseLeafTemplates ("{"); }, "illisible"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { d["format"] = "cgarchi-acanthus-leaves/1"; }), "format"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { d["styles"]["laurier"] = d["styles"]["olive"]; }),
	                         "styles.laurier : style inconnu"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["triangles"][0][1] = 1000000; }),
	                         "indice de sommet"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &t = OliveLight (d)["triangles"][0];
		                         std::swap (t[0], t[1]);
	                         }),
	                         "triangle retourne"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["vertices"][3][4] = 99; }), "region"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["triangles"][0][3] = -1; }), "region"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         d["styles"]["olive"]["stem"]["region"] = 42;
	                         }),
	                         "region"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         OliveLight (d)["vertices"][5][2] = std::numeric_limits<double>::quiet_NaN ();
	                         }),
	                         "nombre attendu"));
	EXPECT_TRUE (ThrowsWith ([] {
		                         std::string text = Document ().dump ();
		                         const std::string from = "\"backHeight\":0.25";
		                         const std::size_t at = text.find (from);
		                         ASSERT_NE (at, std::string::npos);
		                         text.replace (at, from.size (), "\"backHeight\":1e999");
		                         detail::ParseLeafTemplates (text);
	                         },
	                         "illisible"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         l["leafVertices"] = l["vertices"].size () + 1;
	                         }),
	                         "prefixe hors bornes"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         l["leafVertices"] = l["leafVertices"].get<unsigned> () - 1;
	                         }),
	                         "prefixe : sommet"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         l["leafTriangles"] = l["leafTriangles"].get<unsigned> () + 1;
	                         }),
	                         "prefixe : triangle"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         l["triangles"].push_back (l["triangles"][0]);
	                         }),
	                         "plus de deux triangles"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &v = OliveLight (d)["vertices"];
		                         for (nlohmann::json &x : v)
			                         if (x[0].get<double> () == 0.0 && x[1].get<double> () > 0.5)
			                         {
				                         x[0] = 1e-12;
				                         break;
			                         }
	                         }),
	                         "couture"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         l["triangles"].erase (l["triangles"].begin () + l["leafTriangles"].get<int> () - 1);
		                         l["leafTriangles"] = l["leafTriangles"].get<unsigned> () - 1;
	                         }),
	                         "comptes"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &c = OliveLight (d)["countsWithStem"];
		                         c["shellTriangles"] = c["shellTriangles"].get<unsigned> () + 4;
	                         }),
	                         "countsWithStem.shellTriangles"));
}

TEST (TEST_cgarchi_classical_acanthus_template, the_indices_reliefs_and_edges_are_guarded)
{
	// Indice de region hors de int : refuse sur l'entier lu, aux trois sites.
	static constexpr long long kWide = 4294967296LL;
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { d["styles"]["olive"]["stem"]["region"] = kWide; }),
	                         "stem.region : region 4294967296 hors de [0,"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["vertices"][3][4] = kWide; }),
	                         "vertices[3][4] : region 4294967296 hors de [0,"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["triangles"][0][3] = kWide; }),
	                         "triangles[0] : region 4294967296 hors de [0,"));
	// Relief : H et Hb positifs ou nuls, Hb nul sous fade[0].
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["vertices"][3][2] = -0.1; }),
	                         "vertices[3] : relief negatif"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) { OliveLight (d)["vertices"][3][3] = -0.1; }),
	                         "vertices[3] : relief negatif"));
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         for (nlohmann::json &v : OliveLight (d)["vertices"])
			                         if (v[1].get<double> () >= 0.0 && v[1].get<double> () < 0.5)
			                         {
				                         v[3] = 0.1;
				                         break;
			                         }
	                         }),
	                         "Hb non nul sous fade[0]"));
	// Arete de bord parcourue une seconde fois dans le meme sens, par un triangle ajoute du bon cote.
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         const nlohmann::json &vs = l["vertices"];
		                         nlohmann::json &ts = l["triangles"];
		                         std::map<std::pair<long long, long long>, int> uses;
		                         for (const nlohmann::json &t : ts)
			                         for (int e = 0; e < 3; ++e)
				                         ++uses[std::minmax (t[e].get<long long> (), t[(e + 1) % 3].get<long long> ())];
		                         const auto edgeOf = [&] (long long a, long long b) { return uses.count (std::minmax (a, b)) != 0; };
		                         for (const nlohmann::json &t : ts)
			                         for (int e = 0; e < 3; ++e)
			                         {
				                         const long long a = t[e].get<long long> (), b = t[(e + 1) % 3].get<long long> ();
				                         if (uses[std::minmax (a, b)] != 1)
					                         continue;
				                         const double au = vs[a][0], ay = vs[a][1], bu = vs[b][0], by = vs[b][1];
				                         for (long long c = 0; c < static_cast<long long> (vs.size ()); ++c)
				                         {
					                         const double cu = vs[c][0], cy = vs[c][1];
					                         if (c == a || c == b || edgeOf (a, c) || edgeOf (b, c) ||
					                             !((bu - au) * (cy - ay) - (by - ay) * (cu - au) > 0.0))
						                         continue;
					                         ts.push_back ({a, b, c, t[3]});
					                         return;
				                         }
			                         }
	                         }),
	                         "parcourue deux fois dans le meme sens"));
	// Sommet de bande sur aucun triangle.
	EXPECT_TRUE (ThrowsWith (Mutated ([] (nlohmann::json &d) {
		                         nlohmann::json &l = OliveLight (d);
		                         l["vertices"].push_back ({0.1, -0.5, 0.0, 0.0, d["styles"]["olive"]["stem"]["region"]});
	                         }),
	                         "sur aucun triangle"));
}
