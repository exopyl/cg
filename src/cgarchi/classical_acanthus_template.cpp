#include <cgarchi/classical_acanthus_template.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace cgarchi::classical::detail
{

namespace
{

using json = nlohmann::json;

constexpr const char *kFormat = "cgarchi-acanthus-leaves/2";
// Un sommet hors de l'axe en est au moins a cette distance en u.
constexpr double kSeamGap = 1e-9;

[[noreturn]] void Fail (const std::string &path, const std::string &what)
{
	throw std::invalid_argument ("ParseLeafTemplates : " + (path.empty () ? what : path + " : " + what));
}

const json &At (const json &obj, const char *key, const std::string &path)
{
	if (!obj.is_object ())
		Fail (path, "objet attendu");
	const auto it = obj.find (key);
	if (it == obj.end ())
		Fail (path, std::string ("cle ") + key + " absente");
	return *it;
}

double Number (const json &v, const std::string &path)
{
	if (!v.is_number ())
		Fail (path, "nombre attendu");
	const double x = v.get<double> ();
	if (!std::isfinite (x))
		Fail (path, "nombre non fini");
	return x;
}

long long Integer (const json &v, const std::string &path)
{
	if (!v.is_number_integer ())
		Fail (path, "entier attendu");
	return v.get<long long> ();
}

// Indice de region dans [0, regionCount), plage verifiee sur l'entier lu avant
// sa conversion.
int RegionIndex (const json &v, int regionCount, const std::string &path)
{
	const long long r = Integer (v, path);
	if (r < 0 || r >= regionCount)
		Fail (path, "region " + std::to_string (r) + " hors de [0, " + std::to_string (regionCount) + ")");
	return static_cast<int> (r);
}

unsigned Count (const json &v, const std::string &path)
{
	const long long n = Integer (v, path);
	if (n < 0 || n > 0x7fffffffLL)
		Fail (path, "compte hors plage");
	return static_cast<unsigned> (n);
}

// Bord d'une variante (sommets [0, vertexCount), triangles [0, triangleCount)) :
// chaque sommet sur un triangle au moins, chaque arete d'au plus deux
// triangles, parcourue une fois dans chaque sens si elle en a deux ; bord en
// boucles (chaque sommet de bord sur deux aretes) ; couture = aretes de bord
// dont les deux sommets sont sur l'axe.
LeafTemplateRim RimOf (const LeafTemplate &t, unsigned vertexCount, unsigned triangleCount, const std::string &path)
{
	std::map<std::pair<unsigned, unsigned>, int> uses;
	std::map<std::pair<unsigned, unsigned>, LeafTemplateEdge> oriented;
	std::vector<bool> used (vertexCount, false);
	for (unsigned k = 0; k < triangleCount; ++k)
	{
		const auto &tri = t.triangles[k];
		for (int e = 0; e < 3; ++e)
		{
			const unsigned a = tri[static_cast<std::size_t> (e)], b = tri[static_cast<std::size_t> ((e + 1) % 3)];
			used[a] = true;
			const auto key = std::minmax (a, b);
			if (++uses[key] > 2)
				Fail (path, "arete " + std::to_string (key.first) + "-" + std::to_string (key.second) +
				                " partagee par plus de deux triangles");
			if (uses[key] == 2 && oriented[key] == LeafTemplateEdge {a, b})
				Fail (path, "arete " + std::to_string (a) + "-" + std::to_string (b) + " parcourue deux fois dans le meme sens");
			oriented[key] = {a, b};
		}
	}
	for (unsigned v = 0; v < vertexCount; ++v)
		if (!used[v])
			Fail (path, "sommet " + std::to_string (v) + " sur aucun triangle");
	LeafTemplateRim rim;
	std::map<unsigned, int> valence;
	std::vector<unsigned> seam;
	for (const auto &[key, n] : uses)
	{
		if (n != 1)
			continue;
		const LeafTemplateEdge edge = oriented[key];
		++valence[edge[0]];
		++valence[edge[1]];
		const bool onAxis = t.vertices[edge[0]].u == 0.0 && t.vertices[edge[1]].u == 0.0;
		if (onAxis)
		{
			seam.push_back (edge[0]);
			seam.push_back (edge[1]);
		}
		else
			rim.edges.push_back (edge);
	}
	for (const auto &[v, n] : valence)
		if (n != 2)
			Fail (path, "couture : le bord n'est pas une boucle au sommet " + std::to_string (v));
	std::sort (seam.begin (), seam.end ());
	seam.erase (std::unique (seam.begin (), seam.end ()), seam.end ());
	rim.seam = std::move (seam);
	return rim;
}

struct ShellCounts
{
	unsigned vertices = 0, triangles = 0, shellVertices = 0, shellTriangles = 0;
};

ShellCounts CountsOf (const LeafTemplate &t, unsigned vertexCount, unsigned triangleCount, const LeafTemplateRim &rim)
{
	unsigned axis = 0;
	for (unsigned i = 0; i < vertexCount; ++i)
		if (t.vertices[i].u == 0.0)
			++axis;
	const unsigned rimEdges = static_cast<unsigned> (rim.edges.size ());
	return {vertexCount, triangleCount, 2 * (2 * vertexCount - axis), 4 * triangleCount + 4 * rimEdges};
}

void CheckCounts (const json &counts, const ShellCounts &c, const std::string &path)
{
	const std::pair<const char *, unsigned> expected[] = {{"vertices", c.vertices},
	                                                      {"triangles", c.triangles},
	                                                      {"shellVertices", c.shellVertices},
	                                                      {"shellTriangles", c.shellTriangles}};
	for (const auto &[key, value] : expected)
		if (Count (At (counts, key, path), path + "." + key) != value)
			Fail (path + "." + key, "comptes du document " + std::to_string (Count (At (counts, key, path), path)) +
			                            ", maillage lu " + std::to_string (value));
}

LeafTemplate ParseLevel (const json &style, const json &level, LeafStyle styleId, OrnamentDetail detail,
                         const std::string &stylePath, const std::string &path)
{
	LeafTemplate t;
	t.style = styleId;
	t.detail = detail;

	const json &regions = At (style, "regions", stylePath);
	if (!regions.is_array () || regions.empty ())
		Fail (stylePath + ".regions", "tableau non vide attendu");
	for (std::size_t r = 0; r < regions.size (); ++r)
	{
		const std::string rp = stylePath + ".regions[" + std::to_string (r) + "]";
		const json &id = At (regions[r], "id", rp);
		if (!id.is_string ())
			Fail (rp + ".id", "texte attendu");
		t.regions.push_back ({id.get<std::string> (), Number (At (regions[r], "layer", rp), rp + ".layer"),
		                      Number (At (regions[r], "cup", rp), rp + ".cup")});
	}
	const int regionCount = static_cast<int> (t.regions.size ());

	const json &outline = At (style, "outline", stylePath);
	if (!outline.is_array () || outline.size () < 3)
		Fail (stylePath + ".outline", "au moins trois points attendus");
	for (std::size_t k = 0; k < outline.size (); ++k)
	{
		const std::string op = stylePath + ".outline[" + std::to_string (k) + "]";
		if (!outline[k].is_array () || outline[k].size () != 2)
			Fail (op, "couple (u, y) attendu");
		t.outline.push_back ({Number (outline[k][0], op), Number (outline[k][1], op)});
	}

	const json &stem = At (style, "stem", stylePath);
	t.stemU = Number (At (stem, "u", stylePath + ".stem"), stylePath + ".stem.u");
	t.stemTaper = Number (At (stem, "taper", stylePath + ".stem"), stylePath + ".stem.taper");
	t.stemRegion = RegionIndex (At (stem, "region", stylePath + ".stem"), regionCount, stylePath + ".stem.region");
	if (!(t.stemU > 0.0) || !(t.stemTaper > 0.0 && t.stemTaper <= 1.0))
		Fail (stylePath + ".stem", "u > 0 et taper dans ]0, 1] attendus");

	const json &nominal = At (style, "nominal", stylePath);
	t.nominalHalfWidth = Number (At (nominal, "hw", stylePath + ".nominal"), stylePath + ".nominal.hw");
	t.nominalBody = Number (At (nominal, "body", stylePath + ".nominal"), stylePath + ".nominal.body");
	t.nominalCurl = Number (At (nominal, "curl", stylePath + ".nominal"), stylePath + ".nominal.curl");
	if (!(t.nominalHalfWidth > 0.0 && t.nominalBody > 0.0 && t.nominalCurl > 0.0))
		Fail (stylePath + ".nominal", "hw, body et curl > 0 attendus");
	const json &relief = At (style, "relief", stylePath);
	t.backHeight = Number (At (relief, "backHeight", stylePath + ".relief"), stylePath + ".relief.backHeight");
	const json &fade = At (relief, "fade", stylePath + ".relief");
	if (!fade.is_array () || fade.size () != 2)
		Fail (stylePath + ".relief.fade", "couple attendu");
	t.fade = {Number (fade[0], stylePath + ".relief.fade[0]"), Number (fade[1], stylePath + ".relief.fade[1]")};
	if (!(t.backHeight >= 0.0) || !(t.fade[1] > t.fade[0]))
		Fail (stylePath + ".relief", "backHeight >= 0 et fade croissant attendus");

	const json &vertices = At (level, "vertices", path);
	if (!vertices.is_array () || vertices.empty ())
		Fail (path + ".vertices", "tableau non vide attendu");
	t.vertices.reserve (vertices.size ());
	for (std::size_t i = 0; i < vertices.size (); ++i)
	{
		const std::string vp = path + ".vertices[" + std::to_string (i) + "]";
		const json &v = vertices[i];
		if (!v.is_array () || v.size () != 5)
			Fail (vp, "[u, y, H, Hb, region] attendu");
		LeafTemplateVertex x {Number (v[0], vp + "[0]"), Number (v[1], vp + "[1]"), Number (v[2], vp + "[2]"),
		                      Number (v[3], vp + "[3]"), RegionIndex (v[4], regionCount, vp + "[4]")};
		if (x.H < 0.0 || x.Hb < 0.0)
			Fail (vp, "relief negatif (H " + std::to_string (x.H) + ", Hb " + std::to_string (x.Hb) + ")");
		if (x.Hb != 0.0 && x.y < t.fade[0])
			Fail (vp + "[3]", "Hb non nul sous fade[0] (y " + std::to_string (x.y) + ")");
		if (x.u < 0.0 || (x.u != 0.0 && x.u < kSeamGap))
			Fail (vp + "[0]", "couture : u negatif ou a moins de 1e-9 de l'axe sans y etre");
		t.vertices.push_back (x);
	}
	const unsigned vertexCount = static_cast<unsigned> (t.vertices.size ());

	const json &triangles = At (level, "triangles", path);
	if (!triangles.is_array () || triangles.empty ())
		Fail (path + ".triangles", "tableau non vide attendu");
	t.triangles.reserve (triangles.size ());
	for (std::size_t k = 0; k < triangles.size (); ++k)
	{
		const std::string tp = path + ".triangles[" + std::to_string (k) + "]";
		const json &tri = triangles[k];
		if (!tri.is_array () || tri.size () != 4)
			Fail (tp, "[a, b, c, region] attendu");
		std::array<unsigned, 3> abc {};
		for (std::size_t e = 0; e < 3; ++e)
		{
			const long long i = Integer (tri[e], tp);
			if (i < 0 || i >= static_cast<long long> (vertexCount))
				Fail (tp, "indice de sommet " + std::to_string (i) + " hors de [0, " + std::to_string (vertexCount) + ")");
			abc[e] = static_cast<unsigned> (i);
		}
		const int region = RegionIndex (tri[3], regionCount, tp);
		const LeafTemplateVertex &a = t.vertices[abc[0]], &b = t.vertices[abc[1]], &c = t.vertices[abc[2]];
		const double cross = (b.u - a.u) * (c.y - a.y) - (b.y - a.y) * (c.u - a.u);
		if (!(cross > 0.0))
			Fail (tp, "triangle retourne ou plat dans (u, y)");
		t.triangles.push_back (abc);
		t.triangleRegion.push_back (region);
	}
	const unsigned triangleCount = static_cast<unsigned> (t.triangles.size ());

	t.leafVertices = Count (At (level, "leafVertices", path), path + ".leafVertices");
	t.leafTriangles = Count (At (level, "leafTriangles", path), path + ".leafTriangles");
	if (t.leafVertices == 0 || t.leafVertices > vertexCount || t.leafTriangles == 0 || t.leafTriangles > triangleCount)
		Fail (path, "prefixe hors bornes : leafVertices " + std::to_string (t.leafVertices) + " / " +
		                std::to_string (vertexCount) + ", leafTriangles " + std::to_string (t.leafTriangles) + " / " +
		                std::to_string (triangleCount));
	for (unsigned i = 0; i < vertexCount; ++i)
		if ((i < t.leafVertices) != (t.vertices[i].y >= 0.0))
			Fail (path + ".vertices[" + std::to_string (i) + "]", "prefixe : sommet de bande (y < 0) dans la feuille, ou "
			                                                         "sommet de feuille dans la bande");
	for (unsigned k = 0; k < t.leafTriangles; ++k)
		for (unsigned v : t.triangles[k])
			if (v >= t.leafVertices)
				Fail (path + ".triangles[" + std::to_string (k) + "]", "prefixe : triangle de la feuille sur un sommet de bande");

	t.leafRim = RimOf (t, t.leafVertices, t.leafTriangles, path + " (feuille seule)");
	t.stemRim = RimOf (t, vertexCount, triangleCount, path + " (feuille et bande)");
	CheckCounts (At (level, "counts", path), CountsOf (t, t.leafVertices, t.leafTriangles, t.leafRim), path + ".counts");
	CheckCounts (At (level, "countsWithStem", path), CountsOf (t, vertexCount, triangleCount, t.stemRim),
	             path + ".countsWithStem");

	for (unsigned i = 0; i < t.leafVertices; ++i)
		if (t.vertices[i].y > 1.0)
			t.capHalfWidth = std::max (t.capHalfWidth, t.vertices[i].u);
	return t;
}

} // namespace

std::vector<LeafTemplate> ParseLeafTemplates (std::string_view text)
{
	json doc;
	try
	{
		doc = json::parse (text.begin (), text.end ());
	}
	catch (const json::exception &e)
	{
		Fail ("", std::string ("document illisible : ") + e.what ());
	}
	const json &format = At (doc, "format", "");
	if (!format.is_string () || format.get<std::string> () != kFormat)
		Fail ("format", std::string ("\"") + kFormat + "\" attendu");
	const json &styles = At (doc, "styles", "");
	if (!styles.is_object () || styles.empty ())
		Fail ("styles", "objet non vide attendu");
	std::vector<LeafTemplate> out;
	for (auto it = styles.begin (); it != styles.end (); ++it)
	{
		const std::string sp = "styles." + it.key ();
		if (it.key () != "olive" && it.key () != "oliveMinor")
			Fail (sp, "style inconnu");
		const LeafStyle style = it.key () == "olive" ? LeafStyle::Olive : LeafStyle::OliveMinor;
		for (const auto &[name, detail] : {std::make_pair ("light", OrnamentDetail::Light),
		                                   std::make_pair ("heavy", OrnamentDetail::Heavy)})
			out.push_back (ParseLevel (it.value (), At (it.value (), name, sp), style, detail, sp, sp + "." + name));
	}
	return out;
}

const LeafTemplate &LeafTemplateOf (LeafStyle style, OrnamentDetail detail)
{
	static const std::vector<LeafTemplate> templates = ParseLeafTemplates (AcanthusLeavesJson ());
	if (detail == OrnamentDetail::None)
		throw std::invalid_argument ("LeafTemplateOf : aucun gabarit au niveau None");
	for (const LeafTemplate &t : templates)
		if (t.style == style && t.detail == detail)
			return t;
	throw std::invalid_argument ("LeafTemplateOf : style ou niveau absent du document embarque");
}

} // namespace cgarchi::classical::detail
