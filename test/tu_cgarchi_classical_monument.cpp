#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <cgarchi/classical_flutes.h>
#include <cgarchi/classical_greek_doric_column.h>
#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_peristyle.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Document du monument "monument/v6" : lecteur, schema, conversions
// ===========================================================================
//
// Donnee : le document embarque, derive du README du temple d'Hephaistos. Les
// documents refuses en sont tires par une modification ciblee, pour que chaque
// cas ne differe du valide que par la propriete eprouvee.
//
// Tolerances :
//   - kPrintedHalfUnit (5e-4 m) : demi-unite du dernier chiffre d'un resultat
//     imprime par le README (millimetre), calcule depuis les memes valeurs
//     arrondies ; precision d'affichage (J5, J6, J11) ;
//   - kInchHalfUnit (5e-4 in) et kHundredthInchHalfUnit (5e-3 in) : demi-unite
//     d'un resultat en pouces imprime a trois ou deux decimales (J11) ;
//   - kFrontMarginTolerance et kFlankMarginTolerance (registre MO1) : somme des
//     demi-unites du dernier chiffre affiche de chaque terme d'un controle qui
//     confronte deux sources (J8) ; kTopFluteDepthTolerance, MO1 etendue a un
//     produit et un quotient : somme des demi-unites relatives des facteurs,
//     rapportee a la valeur (J11) ;
//   - kProfileTolerance (0,1 in) : precision annoncee du profil de l'echine,
//     trace au pixel (README l. 186) (J13) ;
//   - kChannelDepthTolerance (0,1 in) : precision annoncee de la profondeur des
//     canaux, mesuree au trace (README l. 254) (J17) ;
//   - kDecimalSumTolerance (1e-9 in) : egalite de sommes decimales ecrites par le
//     README, au-dessus du bruit d'arrondi (1e-15) et en deca du plus petit ecart
//     ecrivable (1e-3 in) (J17) ;
//   - kReadMetopeTolerance (registre MO1) : metope lue contre metope courante du
//     modele (J19) ;
//   - au bit pres la ou la conversion n'est qu'une moitie (J7), ou recopie un
//     reglage (J12, J15, J18), et pour une cote en pieds et pouces dont la seule
//     multiplication inexacte vient en dernier (J10). Aucune comparaison au bit
//     pres a une expression a * b +- c calculee ici avec un produit inexact : le
//     job ci-linux contracte ces expressions en FMA (plan 3 section 20.7).

using namespace cgarchi::classical;
using nlohmann::json;

namespace {

constexpr double kPrintedHalfUnit = 5e-4;
constexpr double kInchHalfUnit = 5e-4 * kMonumentInch;
constexpr double kHundredthInchHalfUnit = 5e-3 * kMonumentInch;
// MO1 etendue, README l. 155 : 1,1 x 4,81 / 6,25 = 0,85 in ; demi-unites relatives
// 0,05 / 1,1 + 0,005 / 4,81 + 0,005 / 6,25, rapportees a 0,85 in.
constexpr double kTopFluteDepthTolerance = 0.85 * (0.05 / 1.1 + 0.005 / 4.81 + 0.005 / 6.25) * kMonumentInch;
constexpr double kProfileTolerance = 0.1 * kMonumentInch;
constexpr double kChannelDepthTolerance = 0.1 * kMonumentInch;
constexpr double kDecimalSumTolerance = 1e-9 * kMonumentInch;
// MO1, metope lue (README l. 251-252) : n_f / 2 (0,25 mm), largeur du triglyphe
// 8,3 in (0,05 in), metope lue 6,475 in (0,0005 in).
constexpr double kReadMetopeTolerance = 0.25e-3 + 0.05 * kMonumentInch + 0.0005 * kMonumentInch;

// Mots non ASCII des documents, en octets UTF-8 : independants du jeu de caracteres
// d'execution du compilateur.
constexpr const char *kMeasured = "mesur\xC3\xA9";
constexpr const char *kHypothesis = "hypoth\xC3\xA8" "se";
constexpr const char *kSection = "\xC2\xA7";
constexpr const char *kDegree = "\xC2\xB0";
// MO1, facade : W / 2 (0,25 mm), axe d'angle 1,5 n + a (0,75 + 0,5 mm), D_a / 2
// (0,25 mm), debord (0,5 mm).
constexpr double kFrontMarginTolerance = 2.25e-3;
// MO1, flanc : L / 2 (0,25 mm), axe d'angle 5 n_f + a_f (2,5 + 0,5 mm), D_a / 2
// (0,25 mm), debord (0,5 mm).
constexpr double kFlankMarginTolerance = 4.0e-3;

json EmbeddedDocument ()
{
	const std::string_view text = cgarchi::TempleHephaistosJson();
	return json::parse(text.begin(), text.end());
}

// Message du refus de `text`, ou "" si le document est accepte.
std::string RefusalOf (const std::string &text)
{
	try
	{
		ParseMonument(text);
	}
	catch (const std::runtime_error &e)
	{
		return e.what();
	}
	return std::string();
}

// Le document est refuse, et le message porte "monument/v6 <path> : <motif>".
void ExpectRefused (const std::string &text, const std::string &path, const std::string &motif)
{
	const std::string message = RefusalOf(text);
	const std::string expected = std::string(kMonumentSchema) + " " + path + " : " + motif;
	EXPECT_NE(message.find(expected), std::string::npos) << "attendu : " << expected << "\nrecu : " << message;
}

void ExpectRefused (const json &doc, const std::string &path, const std::string &motif)
{
	ExpectRefused(doc.dump(), path, motif);
}

// Le document modifie par `edit` est refuse, message et chemin nommes.
void ExpectEditRefused (const std::function<void (json &)> &edit, const std::string &path, const std::string &motif)
{
	json doc = EmbeddedDocument();
	edit(doc);
	ExpectRefused(doc, path, motif);
}

std::vector<std::pair<std::string, const MonumentValue *>> AllValues (const MonumentData &d)
{
	const MonumentKrepis &k = d.krepis;
	const MonumentPeristyle &p = d.peristyle;
	const MonumentColumns &c = d.columns;
	const MonumentEntablature &e = d.entablature;
	return {
		{"krepis.stylobateWidth", &k.stylobateWidth},
		{"krepis.stylobateLength", &k.stylobateLength},
		{"krepis.stepCount", &k.stepCount},
		{"krepis.stylobateHeight", &k.stylobateHeight},
		{"krepis.secondStepHeight", &k.secondStepHeight},
		{"krepis.thirdStepHeight", &k.thirdStepHeight},
		{"krepis.secondStepTread", &k.secondStepTread},
		{"krepis.thirdStepTread", &k.thirdStepTread},
		{"krepis.cornerMargin", &k.cornerMargin},
		{"krepis.euthynteria", &k.euthynteria},
		{"peristyle.frontColumns", &p.frontColumns},
		{"peristyle.flankColumns", &p.flankColumns},
		{"peristyle.frontSpacing", &p.frontSpacing},
		{"peristyle.flankSpacing", &p.flankSpacing},
		{"peristyle.frontCornerSpacing", &p.frontCornerSpacing},
		{"peristyle.flankCornerSpacing", &p.flankCornerSpacing},
		{"columns.lowerDiameter", &c.lowerDiameter},
		{"columns.cornerLowerDiameter", &c.cornerLowerDiameter},
		{"columns.upperDiameter", &c.upperDiameter},
		{"columns.height", &c.height},
		{"columns.fluteCount", &c.fluteCount},
		{"columns.fluteDepth", &c.fluteDepth},
		{"columns.entasisSagitta", &c.entasisSagitta},
		{"columns.entasisHeight", &c.entasisHeight},
		{"columns.abacusHeight", &c.abacusHeight},
		{"columns.abacusHalfWidth", &c.abacusHalfWidth},
		{"columns.echinusHeight", &c.echinusHeight},
		{"columns.echinusJunctionDepth", &c.echinusJunctionDepth},
		{"columns.echinusJunctionRetreat", &c.echinusJunctionRetreat},
		{"columns.echinusTopRetreat", &c.echinusTopRetreat},
		{"columns.echinusLipRadius", &c.echinusLipRadius},
		{"columns.annuletCount", &c.annuletCount},
		{"columns.annuletsHeight", &c.annuletsHeight},
		{"columns.annuletsTopRetreat", &c.annuletsTopRetreat},
		{"columns.annuletsBottomRetreat", &c.annuletsBottomRetreat},
		{"columns.hypotrachelionHeight", &c.hypotrachelionHeight},
		{"columns.grooveHeight", &c.grooveHeight},
		{"columns.grooveDepth", &c.grooveDepth},
		{"entablature.architraveHeightFt", &e.architraveHeightFt},
		{"entablature.architraveHeightIn", &e.architraveHeightIn},
		{"entablature.taeniaHeight", &e.taeniaHeight},
		{"entablature.regulaHeight", &e.regulaHeight},
		{"entablature.guttaeHeight", &e.guttaeHeight},
		{"entablature.architraveFaceHeight", &e.architraveFaceHeight},
		{"entablature.friezeHeightFt", &e.friezeHeightFt},
		{"entablature.friezeHeightIn", &e.friezeHeightIn},
		{"entablature.triglyphCapitalHeight", &e.triglyphCapitalHeight},
		{"entablature.corniceHeightFt", &e.corniceHeightFt},
		{"entablature.corniceHeightIn", &e.corniceHeightIn},
		{"entablature.mutulePlateHeight", &e.mutulePlateHeight},
		{"entablature.dripHeight", &e.dripHeight},
		{"entablature.coronaHeight", &e.coronaHeight},
		{"entablature.filletHeight", &e.filletHeight},
		{"entablature.crownHeight", &e.crownHeight},
		{"entablature.architraveTopProjection", &e.architraveTopProjection},
		{"entablature.architraveFootProjection", &e.architraveFootProjection},
		{"entablature.taeniaProjection", &e.taeniaProjection},
		{"entablature.triglyphFace", &e.triglyphFace},
		{"entablature.triglyphCapitalBandProjection", &e.triglyphCapitalBandProjection},
		{"entablature.triglyphCapitalListelProjection", &e.triglyphCapitalListelProjection},
		{"entablature.triglyphCapitalListelHeight", &e.triglyphCapitalListelHeight},
		{"entablature.triglyphCapitalBandHeight", &e.triglyphCapitalBandHeight},
		{"entablature.metopeFace", &e.metopeFace},
		{"entablature.mutulePlateProjection", &e.mutulePlateProjection},
		{"entablature.dripProjection", &e.dripProjection},
		{"entablature.coronaProjection", &e.coronaProjection},
		{"entablature.filletProjection", &e.filletProjection},
		{"entablature.crownProjection", &e.crownProjection},
		{"entablature.innerFace", &e.innerFace},
		{"entablature.triglyphWidthFt", &e.triglyphWidthFt},
		{"entablature.triglyphWidthIn", &e.triglyphWidthIn},
		{"entablature.metopeWidthFt", &e.metopeWidthFt},
		{"entablature.metopeWidthIn", &e.metopeWidthIn},
		{"entablature.channelDepth", &e.channelDepth},
		{"entablature.regulaProjection", &e.regulaProjection},
		{"entablature.regulaGuttaCount", &e.regulaGuttaCount},
		{"entablature.regulaGuttaDiameter", &e.regulaGuttaDiameter},
		{"entablature.regulaGuttaGap", &e.regulaGuttaGap},
		{"entablature.regulaGuttaTopProjection", &e.regulaGuttaTopProjection},
		{"entablature.regulaGuttaFootProjection", &e.regulaGuttaFootProjection},
		{"entablature.mutuleWidthFt", &e.mutuleWidthFt},
		{"entablature.mutuleWidthIn", &e.mutuleWidthIn},
		{"entablature.mutuleDepth", &e.mutuleDepth},
		{"entablature.soffitSlope", &e.soffitSlope},
		{"entablature.mutuleGuttaFrontDiameter", &e.mutuleGuttaFrontDiameter},
		{"entablature.mutuleGuttaMiddleDiameter", &e.mutuleGuttaMiddleDiameter},
		{"entablature.mutuleGuttaRearDiameter", &e.mutuleGuttaRearDiameter},
		{"entablature.mutuleGuttaFrontGap", &e.mutuleGuttaFrontGap},
		{"entablature.mutuleGuttaRearGap", &e.mutuleGuttaRearGap},
		{"entablature.mutuleGuttaHeight", &e.mutuleGuttaHeight},
		{"entablature.mutuleGuttaEdgeAxis", &e.mutuleGuttaEdgeAxis},
		{"entablature.mutuleGuttaPerRow", &e.mutuleGuttaPerRow},
		{"entablature.mutuleGuttaFootFlare", &e.mutuleGuttaFootFlare},
	};
}

// Cle de pieds d'une cote en pieds et pouces : "<nom>Ft", seules valeurs d'unite "ft".
bool IsFeetKey (const std::string &key)
{
	const std::string suffix = "Ft";
	return key.size() > suffix.size() && key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Cote en pieds et pouces du document, en metres.
double FeetAndInches (const MonumentValue &feet, const MonumentValue &inches)
{
	return MonumentFeetAndInches(feet, inches, "cote");
}

// Cles des comptes du document : les seules valeurs d'unite "1".
const std::set<std::string> &CountKeys ()
{
	static const std::set<std::string> keys = {"krepis.stepCount", "peristyle.frontColumns", "peristyle.flankColumns",
	                                           "columns.fluteCount", "columns.annuletCount",
	                                           "entablature.regulaGuttaCount", "entablature.mutuleGuttaPerRow"};
	return keys;
}

double Metres (const MonumentControlValue &c)
{
	return MonumentMetres(c.value, c.unit, "controle");
}

double Length (const MonumentValue &v, const std::string &key = "valeur")
{
	return MonumentLength(v, key);
}

constexpr double kPi = 3.14159265358979323846;

double Value (const MonumentValue &v)
{
	if (!v.value)
		throw std::logic_error("valeur manquante dans le document embarque");
	return *v.value;
}

// --- Validateur de schema de test -----------------------------------------
//
// Mots-cles connus : type, const, enum, properties, required,
// additionalProperties (booleen ou schema), $defs, $ref ("#/$defs/<nom>"),
// items, exclusiveMinimum, pattern ; annotations sans effet : $schema, $id,
// title, description, $comment. Tout autre mot-cle est un ecart : un schema
// qui en emploierait un ne serait pas verifie en entier.

std::string Join (const std::string &parent, const std::string &key)
{
	return parent.empty() ? key : parent + "." + key;
}

const json &Resolve (const json &schema, const json &root)
{
	if (!schema.contains("$ref"))
		return schema;
	const std::string ref = schema["$ref"].get<std::string>();
	const std::string prefix = "#/$defs/";
	if (ref.compare(0, prefix.size(), prefix) != 0 || !root["$defs"].contains(ref.substr(prefix.size())))
		throw std::invalid_argument("$ref non resolu : " + ref);
	return root["$defs"][ref.substr(prefix.size())];
}

bool HasType (const json &v, const std::string &type)
{
	if (type == "object")  return v.is_object();
	if (type == "array")   return v.is_array();
	if (type == "string")  return v.is_string();
	if (type == "boolean") return v.is_boolean();
	if (type == "number")  return v.is_number();
	if (type == "integer")
		return v.is_number() && std::isfinite(v.get<double>()) && std::floor(v.get<double>()) == v.get<double>();
	throw std::invalid_argument("type inconnu : " + type);
}

// Vide si `v` respecte `schema` ; sinon le premier ecart, chemin nomme.
std::string Violation (const json &v, const json &schemaIn, const json &root, const std::string &path)
{
	static const std::set<std::string> kKeywords = {
		"type", "const", "enum", "properties", "required", "additionalProperties", "$defs", "$ref", "items",
		"exclusiveMinimum", "pattern", "$schema", "$id", "title", "description", "$comment"};
	for (auto it = schemaIn.begin(); it != schemaIn.end(); ++it)
		if (kKeywords.count(it.key()) == 0)
			return path + " : mot-cle inconnu du validateur : " + it.key();
	if (schemaIn.contains("$ref"))
	{
		const std::string inner = Violation(v, Resolve(schemaIn, root), root, path);
		if (!inner.empty())
			return inner;
	}
	const json &s = schemaIn;
	if (s.contains("type") && !HasType(v, s["type"].get<std::string>()))
		return path + " : type " + s["type"].get<std::string>() + " attendu";
	if (s.contains("const") && v != s["const"])
		return path + " : valeur differente de const";
	if (s.contains("enum") && std::find(s["enum"].begin(), s["enum"].end(), v) == s["enum"].end())
		return path + " : valeur hors enum";
	if (s.contains("exclusiveMinimum") && !(v.get<double>() > s["exclusiveMinimum"].get<double>()))
		return path + " : valeur <= exclusiveMinimum";
	if (s.contains("pattern") && !std::regex_search(v.get<std::string>(), std::regex(s["pattern"].get<std::string>())))
		return path + " : motif non respecte";
	if (s.contains("required"))
		for (const json &name : s["required"])
			if (!v.contains(name.get<std::string>()))
				return Join(path, name.get<std::string>()) + " : requis absent";
	if (v.is_object() && (s.contains("properties") || s.contains("additionalProperties")))
		for (auto it = v.begin(); it != v.end(); ++it)
		{
			const std::string sub = Join(path, it.key());
			std::string inner;
			if (s.contains("properties") && s["properties"].contains(it.key()))
				inner = Violation(it.value(), s["properties"][it.key()], root, sub);
			else if (s.contains("additionalProperties") && s["additionalProperties"].is_boolean())
				inner = s["additionalProperties"].get<bool>() ? std::string() : sub + " : champ hors schema";
			else if (s.contains("additionalProperties"))
				inner = Violation(it.value(), s["additionalProperties"], root, sub);
			if (!inner.empty())
				return inner;
		}
	if (v.is_array() && s.contains("items"))
		for (std::size_t i = 0; i < v.size(); ++i)
		{
			const std::string inner = Violation(v[i], s["items"], root, path + "[" + std::to_string(i) + "]");
			if (!inner.empty())
				return inner;
		}
	return std::string();
}

// Chemins de chaque cle requise par le schema, presente dans `doc`, en suivant
// les proprietes presentes.
void RequiredPaths (const json &doc, const json &schemaIn, const json &root, std::vector<std::string> &keys,
                    std::vector<std::vector<std::string>> &out)
{
	const json &s = Resolve(schemaIn, root);
	if (s.contains("required"))
		for (const json &name : s["required"])
		{
			keys.push_back(name.get<std::string>());
			out.push_back(keys);
			keys.pop_back();
		}
	if (doc.is_object() && s.contains("properties"))
		for (auto it = doc.begin(); it != doc.end(); ++it)
			if (s["properties"].contains(it.key()))
			{
				keys.push_back(it.key());
				RequiredPaths(it.value(), s["properties"][it.key()], root, keys, out);
				keys.pop_back();
			}
}

json SchemaFile ()
{
	const std::filesystem::path path = std::filesystem::path("src") / "cgarchi" / "schemas" / "monument-v6.schema";
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return json();
	std::ostringstream text;
	text << in.rdbuf();
	return json::parse(text.str());
}

// Colonnes d'une rangee, par cote.
std::vector<const PeristyleColumn *> Row (const PeristyleLayout &layout, PeristyleSide side)
{
	std::vector<const PeristyleColumn *> row;
	for (const PeristyleColumn &c : layout.columns)
		if (c.side == side)
			row.push_back(&c);
	return row;
}

} // namespace

// J1
TEST(TEST_cgarchi_classical_monument, the_embedded_document_is_read_and_records_its_source_fingerprint)
{
	const MonumentData *data = nullptr;
	ASSERT_NO_THROW(data = &HephaistosMonument());
	EXPECT_EQ(EmbeddedDocument()["schema"], kMonumentSchema);
	EXPECT_EQ(data->monument, "temple-hephaistos");
	EXPECT_TRUE(std::regex_match(data->source.sha256, std::regex("[0-9a-f]{64}"))) << data->source.sha256;
	EXPECT_GT(data->source.bytes, 0u);
	const std::string &doc = data->source.document;
	ASSERT_FALSE(doc.empty());
	EXPECT_NE(doc.front(), '/') << doc;
	EXPECT_EQ(doc.find('\\'), std::string::npos) << doc;
	EXPECT_FALSE(doc.size() > 1 && doc[1] == ':') << doc;
	EXPECT_EQ(data->source.derivedBy, "src/cgarchi/data/temple_hephaistos_from_readme.py");
}

// J2
TEST(TEST_cgarchi_classical_monument, every_value_has_a_closed_status_a_readme_place_and_known_sources)
{
	const MonumentData &data = HephaistosMonument();
	std::set<MonumentStatus> seen;
	std::set<std::string> cited;
	for (const auto &[key, v] : AllValues(data))
	{
		SCOPED_TRACE(key);
		seen.insert(v->status);
		EXPECT_FALSE(v->readme.empty());
		EXPECT_EQ(v->value.has_value(), v->status != MonumentStatus::Missing);
		if (v->status == MonumentStatus::Read || v->status == MonumentStatus::Measured)
			EXPECT_FALSE(v->sources.empty());
		if (v->status == MonumentStatus::Missing)
			EXPECT_TRUE(v->sources.empty());
		for (const std::string &s : v->sources)
		{
			EXPECT_EQ(data.sources.count(s), 1u) << s;
			cited.insert(s);
		}
	}
	for (const auto &entry : data.sources)
		EXPECT_EQ(cited.count(entry.first), 1u) << entry.first;
	for (MonumentStatus s : {MonumentStatus::Read, MonumentStatus::Deduced, MonumentStatus::Measured,
	                         MonumentStatus::Convention, MonumentStatus::Missing})
		EXPECT_EQ(seen.count(s), 1u) << static_cast<int>(s);
	// Q3-31 : le giron du 3e degre est une convention de la source, celui du 2e est lu.
	EXPECT_EQ(data.krepis.secondStepTread.status, MonumentStatus::Read);
	EXPECT_EQ(data.krepis.thirdStepTread.status, MonumentStatus::Convention);
}

// J3
TEST(TEST_cgarchi_classical_monument, the_reader_refuses_a_value_without_a_valid_status_or_source)
{
	const std::string h = "krepis.stylobateHeight";
	const auto at = [](json &d) -> json & { return d["krepis"]["stylobateHeight"]; };
	const auto eu = [](json &d) -> json & { return d["krepis"]["euthynteria"]; };

	ExpectEditRefused([&](json &d) { at(d).erase("status"); }, h + ".status", "required field is missing");
	ExpectEditRefused([&](json &d) { at(d)["status"] = kHypothesis; }, h + ".status",
	                  std::string("unknown value \"") + kHypothesis + "\"");
	ExpectEditRefused([&](json &d) { at(d)["status"] = nullptr; }, h + ".status", "expected a string, not null");
	ExpectEditRefused([&](json &d) { at(d)["value"] = nullptr; }, h + ".value", "expected a number, not null");
	ExpectEditRefused([&](json &d) { at(d).erase("value"); }, h + ".value", "required field is missing");
	ExpectEditRefused([&](json &d) { eu(d)["value"] = 1.0; }, "krepis.euthynteria.value",
	                  "status \"manquant\" admits no value");
	ExpectEditRefused([&](json &d) { at(d)["sources"] = {"X"}; }, h + ".sources[0]", "unknown source \"X\"");
	ExpectEditRefused([&](json &d) { d["sources"]["Z"] = "absente du README"; }, "sources.Z",
	                  "listed but never cited");
	ExpectEditRefused([&](json &d) { at(d)["sources"] = json::array(); }, h + ".sources",
	                  "status \"lu\" requires a source");
	ExpectEditRefused([&](json &d) { at(d)["status"] = kMeasured; at(d)["sources"] = json::array(); }, h + ".sources",
	                  std::string("status \"") + kMeasured + "\" requires a source");
	ExpectEditRefused([&](json &d) { eu(d)["sources"] = {"S"}; }, "krepis.euthynteria.sources",
	                  "status \"manquant\" admits no source");
	ExpectEditRefused([&](json &d) { at(d)["readme"] = ""; }, h + ".readme", "empty place");
	ExpectEditRefused([&](json &d) { d["extra"] = 1; }, "extra", "unknown field");
	ExpectEditRefused([&](json &d) { d["krepis"]["extra"] = 1; }, "krepis.extra", "unknown field");
	ExpectEditRefused([&](json &d) { at(d)["extra"] = 1; }, h + ".extra", "unknown field");
	// Version : seule monument/v6 est lue ; v1 a v5 et v7 sont refusees (D2-8).
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v1"; }, "schema", "unsupported schema \"monument/v1\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v2"; }, "schema", "unsupported schema \"monument/v2\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v3"; }, "schema", "unsupported schema \"monument/v3\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v4"; }, "schema", "unsupported schema \"monument/v4\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v5"; }, "schema", "unsupported schema \"monument/v5\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v7"; }, "schema", "unsupported schema \"monument/v7\"");
	// Un document de forme v5 (sans sous-face inclinee ni gouttes des mutules) sous l'etiquette v6 est refuse, cles
	// nommees.
	ExpectEditRefused([&](json &d) { d["entablature"].erase("soffitSlope"); }, "entablature.soffitSlope",
	                  "required field is missing");
	ExpectEditRefused([&](json &d) { d["entablatureControls"].erase("mutuleGuttaRows"); },
	                  "entablatureControls.mutuleGuttaRows", "required field is missing");
	// Un document de forme v3 (sans les controles d'angle) sous l'etiquette v6 est refuse, cle nommee.
	ExpectEditRefused([&](json &d) { d["entablatureControls"].erase("cornerTriglyphOffset"); },
	                  "entablatureControls.cornerTriglyphOffset", "required field is missing");
	// De meme un document de forme v4 (sans regulae ni mutules) : premieres cles absentes nommees.
	ExpectEditRefused([&](json &d) { d["entablature"].erase("regulaProjection"); },
	                  "entablature.regulaProjection", "required field is missing");
	ExpectEditRefused([&](json &d) { d["entablatureControls"].erase("mutuleBack"); },
	                  "entablatureControls.mutuleBack", "required field is missing");
	ExpectEditRefused([&](json &d) { at(d)["value"] = 0.0; }, h + ".value", "0 outside (0, inf]");
	ExpectEditRefused([&](json &d) { at(d)["value"] = -1.0; }, h + ".value", "-1 outside (0, inf]");
	ExpectEditRefused([&](json &d) { d["krepis"]["stepCount"]["value"] = 2.5; }, "krepis.stepCount.value",
	                  "expected an integer, not number");
	ExpectEditRefused([&](json &d) { d["source"]["sha256"] = d["source"]["sha256"].get<std::string>().substr(1); },
	                  "source.sha256", "expected 64 lowercase hexadecimal digits");
	ExpectEditRefused([&](json &d) { d["source"]["bytes"] = 0; }, "source.bytes", "0 outside [1, ");
	ExpectEditRefused([&](json &d) { std::swap(d["controls"]["frontAxesY"]["values"][0],
	                                           d["controls"]["frontAxesY"]["values"][1]); },
	                  "controls.frontAxesY.values[1]", "not strictly greater than the previous coordinate");

	ExpectEditRefused([&](json &d) { d["controls"]["frontAxesY"]["values"] = json::array(); },
	                  "controls.frontAxesY.values", "empty array");
	{
		// 1e400 deborde en double : nlohmann le refuse des l'analyse (out_of_range 406), avant
		// le controle de finitude de ReadAxes, qu'aucun texte JSON n'atteint. Le texte est
		// fabrique a la main : json::dump ecrirait null.
		const std::string text = EmbeddedDocument().dump();
		const std::string last = ",15.309]";
		ASSERT_EQ(text.find(last), text.rfind(last));
		ASSERT_NE(text.find(last), std::string::npos);
		std::string overflow = text;
		overflow.replace(text.find(last), last.size(), ",1e400]");
		ExpectRefused(overflow, "", "unreadable text");
		EXPECT_NE(RefusalOf(overflow).find("1e400"), std::string::npos);
	}

	// Cle en double : la seconde valeur de "monument" est nommee par son chemin.
	const std::string text = EmbeddedDocument().dump();
	const std::string key = "\"monument\":\"temple-hephaistos\"";
	const std::size_t at0 = text.find(key);
	ASSERT_NE(at0, std::string::npos);
	ExpectRefused(text.substr(0, at0) + key + "," + text.substr(at0), "monument", "duplicate key");
	ExpectRefused(std::string("{\"schema\":"), "", "unreadable text");

	// Une valeur mesuree avec sa source est acceptee.
	json measured = EmbeddedDocument();
	at(measured)["status"] = kMeasured;
	EXPECT_EQ(RefusalOf(measured.dump()), "");
	EXPECT_EQ(ParseMonument(measured.dump()).krepis.stylobateHeight.status, MonumentStatus::Measured);
}

// J4
TEST(TEST_cgarchi_classical_monument, the_document_conforms_to_its_schema_file_and_every_required_key_is_required_by_the_reader)
{
	const json schema = SchemaFile();
	ASSERT_TRUE(schema.is_object()) << "src/cgarchi/schemas/monument-v6.schema illisible depuis le repertoire courant";
	const json doc = EmbeddedDocument();
	EXPECT_EQ(Violation(doc, schema, schema, std::string()), "");
	// Controle positif : le document complet est accepte, si bien que chaque refus
	// ci-dessous tient a la cle retiree.
	EXPECT_EQ(RefusalOf(doc.dump()), "");

	// Controles negatifs du validateur : un mot-cle inconnu, puis une donnee hors schema.
	json unknownKeyword = schema;
	unknownKeyword["$defs"]["length"]["properties"]["value"]["minimum"] = 0;
	EXPECT_NE(Violation(doc, unknownKeyword, unknownKeyword, std::string()).find("mot-cle inconnu"), std::string::npos);
	json offSchema = doc;
	offSchema["krepis"]["stylobateHeight"]["status"] = kHypothesis;
	EXPECT_EQ(Violation(offSchema, schema, schema, std::string()), "krepis.stylobateHeight.status : valeur hors enum");

	std::vector<std::string> keys;
	std::vector<std::vector<std::string>> paths;
	RequiredPaths(doc, schema, schema, keys, paths);
	// 11 a la racine, 4 dans source, 38 + 36 + 9 + 10 cles de valeur (angle compris) et 4 par valeur, 4 controles
	// du peristyle et 2 par controle, 11 controles de la colonne et 3 par controle, un profil et ses 4 cles, une
	// suite de divisions et ses 6 cles, 10 + 9 + 4 + 18 controles de l'entablement (dont ceux de l'angle, des
	// gouttes, du dos des mutules, de la sous-face inclinee et des gouttes des mutules) et 3 par controle.
	EXPECT_EQ(paths.size(),
	          11u + 4u + (38u + 36u + 9u + 10u) * 5u + 4u * 3u + 11u * 4u + 5u + 7u + (10u + 9u + 4u + 18u) * 4u);
	for (const std::vector<std::string> &path : paths)
	{
		std::string dotted, pointer;
		for (const std::string &k : path)
		{
			dotted = Join(dotted, k);
			pointer += "/" + k;
		}
		SCOPED_TRACE(dotted);
		json deprived = doc;
		const json::json_pointer ptr(pointer);
		deprived[ptr.parent_pointer()].erase(path.back());
		ExpectRefused(deprived, dotted, "required field is missing");
	}
}

// J5
TEST(TEST_cgarchi_classical_monument, the_facade_and_flank_sums_of_the_readme_recompute_from_the_values)
{
	const MonumentData &d = HephaistosMonument();
	const double margin = Value(d.krepis.cornerMargin);
	const double corner = Value(d.columns.cornerLowerDiameter);
	const MonumentPeristyle &p = d.peristyle;
	const double front = 2.0 * margin + corner + 2.0 * Value(p.frontCornerSpacing)
	                   + (Value(p.frontColumns) - 3.0) * Value(p.frontSpacing);
	const double flank = 2.0 * margin + corner + 2.0 * Value(p.flankCornerSpacing)
	                   + (Value(p.flankColumns) - 3.0) * Value(p.flankSpacing);
	EXPECT_NEAR(front, d.controls.frontSum, kPrintedHalfUnit);
	EXPECT_NEAR(flank, d.controls.flankSum, kPrintedHalfUnit);
	EXPECT_EQ(d.controls.frontSum, 13.718);
	EXPECT_EQ(d.controls.flankSum, 31.77);
}

// J6
TEST(TEST_cgarchi_classical_monument, the_peristyle_axes_are_those_of_the_readme_in_the_temple_frame)
{
	const MonumentData &d = HephaistosMonument();
	const PeristyleLayout layout = LayoutPeristyle(MonumentPeristyleSettings(d));
	ASSERT_EQ(layout.columns.size(), 34u);
	EXPECT_EQ(layout.front, 6);
	EXPECT_EQ(layout.flank, 13);
	EXPECT_NEAR(layout.halfWidth, 6.283, kPrintedHalfUnit);
	EXPECT_NEAR(layout.halfDepth, 15.309, kPrintedHalfUnit);

	// Selection d'une rangee : un axe d'angle et la demi-distance sont deux
	// calculs, egaux a l'arrondi pres.
	const double kAxisTolerance = 1e-12 * layout.halfDepth;
	const std::vector<double> &ys = d.controls.frontAxesY;
	const std::vector<double> &xs = d.controls.flankAxesX;
	ASSERT_EQ(ys.size(), 6u);
	ASSERT_EQ(xs.size(), 13u);

	// Rangee de facade (Y = -halfDepth) : ses X sont les y du README.
	std::vector<const PeristyleColumn *> front = Row(layout, PeristyleSide::Front);
	ASSERT_EQ(front.size(), 5u);   // l'angle qui ferme la facade ouvre le flanc droit
	std::vector<double> frontX;
	for (const PeristyleColumn &c : layout.columns)
		if (std::abs(c.y + layout.halfDepth) <= kAxisTolerance)
			frontX.push_back(c.x);
	ASSERT_EQ(frontX.size(), ys.size());
	std::sort(frontX.begin(), frontX.end());
	for (std::size_t i = 0; i < ys.size(); ++i)
		EXPECT_NEAR(frontX[i], ys[i], kPrintedHalfUnit) << i;

	// Flanc droit (X = +halfWidth) : ses Y sont les -x du README.
	std::vector<double> rightY, minusX;
	for (const PeristyleColumn &c : layout.columns)
		if (std::abs(c.x - layout.halfWidth) <= kAxisTolerance)
			rightY.push_back(c.y);
	for (double x : xs)
		minusX.push_back(-x);
	std::sort(rightY.begin(), rightY.end());
	std::sort(minusX.begin(), minusX.end());
	ASSERT_EQ(rightY.size(), minusX.size());
	for (std::size_t i = 0; i < minusX.size(); ++i)
		EXPECT_NEAR(rightY[i], minusX[i], kPrintedHalfUnit) << i;

	// Les 34 axes du README (section 4.3), tournes dans le repere du temple,
	// sont les 34 axes du peristyle, un pour un.
	std::vector<MonumentFramePoint> readme;
	for (double x : {xs.front(), xs.back()})
		for (double y : ys)
			readme.push_back(TempleFrameFromMonument(x, y, 0.0, 0.0));
	for (double y : {ys.front(), ys.back()})
		for (std::size_t i = 1; i + 1 < xs.size(); ++i)
			readme.push_back(TempleFrameFromMonument(xs[i], y, 0.0, 0.0));
	ASSERT_EQ(readme.size(), 34u);
	std::vector<bool> used(layout.columns.size(), false);
	for (const MonumentFramePoint &q : readme)
	{
		std::size_t match = layout.columns.size();
		for (std::size_t i = 0; i < layout.columns.size(); ++i)
			if (!used[i] && std::abs(layout.columns[i].x - q.x) <= kPrintedHalfUnit
			    && std::abs(layout.columns[i].y - q.y) <= kPrintedHalfUnit)
				match = i;
		ASSERT_LT(match, layout.columns.size()) << "(" << q.x << ", " << q.y << ") sans axe";
		used[match] = true;
	}

	// MO2 : la facade est du README (x > 0) est la facade du temple (Y < 0), le flanc
	// nord (y > 0) son flanc droit (X > 0).
	EXPECT_LT(TempleFrameFromMonument(xs.back(), 0.0, 0.0, 0.0).y, 0.0);
	EXPECT_GT(TempleFrameFromMonument(0.0, ys.back(), 0.0, 0.0).x, 0.0);

	// Les quatre angles aux quatre coins.
	int corners = 0;
	for (const PeristyleColumn &c : layout.columns)
		if (c.corner)
		{
			++corners;
			EXPECT_NEAR(std::abs(c.x), layout.halfWidth, kAxisTolerance);
			EXPECT_NEAR(std::abs(c.y), layout.halfDepth, kAxisTolerance);
		}
	EXPECT_EQ(corners, 4);
}

// J7
TEST(TEST_cgarchi_classical_monument, the_krepis_is_the_readme_krepis_with_its_origin_on_top_of_the_stylobate)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentKrepis &k = d.krepis;
	const KrepisLayout layout = LayoutKrepis(MonumentKrepisSettings(d));
	ASSERT_EQ(static_cast<double>(layout.steps.size()), Value(k.stepCount));
	EXPECT_EQ(layout.steps[0].halfWidth, 0.5 * Value(k.stylobateWidth));
	EXPECT_EQ(layout.steps[0].halfDepth, 0.5 * Value(k.stylobateLength));
	EXPECT_LT(layout.steps[0].halfWidth, layout.steps[0].halfDepth);

	const double tol = 1e-12 * layout.stylobateTop;
	EXPECT_NEAR(layout.steps[0].zTop - layout.steps[0].zBottom, 0.365, tol);
	EXPECT_NEAR(layout.steps[1].zTop - layout.steps[1].zBottom, 0.347, tol);
	EXPECT_NEAR(layout.steps[2].zTop - layout.steps[2].zBottom, 0.35, tol);
	const double treads[] = {0.372, 0.372};   // README l. 77 et 78
	for (std::size_t i = 1; i < 3; ++i)
	{
		const double dtol = 1e-12 * layout.steps[2].halfDepth;
		EXPECT_NEAR(layout.steps[i].halfWidth - layout.steps[i - 1].halfWidth, treads[i - 1], dtol) << i;
		EXPECT_NEAR(layout.steps[i].halfDepth - layout.steps[i - 1].halfDepth, treads[i - 1], dtol) << i;
	}
	const double sum = Value(k.stylobateHeight) + Value(k.secondStepHeight) + Value(k.thirdStepHeight);
	EXPECT_NEAR(layout.stylobateTop, sum, 1e-12);
	EXPECT_NEAR(layout.stylobateTop, 1.062, 1e-12);

	// z = 0 du README (dessus du stylobate) est la cote stylobateTop du temple.
	const MonumentFramePoint origin = TempleFrameFromMonument(0.0, 0.0, 0.0, layout.stylobateTop);
	EXPECT_EQ(origin.z, layout.stylobateTop);
}

// J8
TEST(TEST_cgarchi_classical_monument, the_corner_margin_of_the_readme_is_the_stylobate_beyond_the_corner_column)
{
	const MonumentData &d = HephaistosMonument();
	const KrepisSettings krepis = MonumentKrepisSettings(d);
	const PeristyleLayout peristyle = LayoutPeristyle(MonumentPeristyleSettings(d));
	const double halfCorner = 0.5 * Value(d.columns.cornerLowerDiameter);
	const double margin = Value(d.krepis.cornerMargin);
	const double front = krepis.stylobateHalfWidth - peristyle.halfWidth - halfCorner;
	const double flank = krepis.stylobateHalfDepth - peristyle.halfDepth - halfCorner;
	EXPECT_NEAR(front, margin, kFrontMarginTolerance);
	EXPECT_NEAR(flank, margin, kFlankMarginTolerance);
	::testing::Test::RecordProperty("front_margin_gap_m", std::to_string(front - margin));
	::testing::Test::RecordProperty("flank_margin_gap_m", std::to_string(flank - margin));
}

// J9
TEST(TEST_cgarchi_classical_monument, a_missing_value_is_refused_where_a_conversion_needs_it)
{
	const auto expectThrowNaming = [](const std::function<void ()> &f, const std::string &what) {
		try
		{
			f();
			ADD_FAILURE() << "aucun refus, attendu : " << what;
		}
		catch (const std::invalid_argument &e)
		{
			const std::string message = e.what();
			EXPECT_EQ(message.rfind("Monument : ", 0), 0u) << message;
			EXPECT_NE(message.find(what), std::string::npos) << message;
		}
	};
	const MonumentData base = HephaistosMonument();

	MonumentData third = base;
	third.krepis.thirdStepHeight.value.reset();
	third.krepis.thirdStepHeight.status = MonumentStatus::Missing;
	third.krepis.thirdStepHeight.sources.clear();
	expectThrowNaming([&] { MonumentKrepisSettings(third); }, "'krepis.thirdStepHeight'");

	json doc = EmbeddedDocument();
	doc["peristyle"]["frontSpacing"] = {{"unit", "m"}, {"status", "manquant"}, {"sources", json::array()},
	                                    {"readme", std::string(kSection) + "4.2"}};
	const MonumentData spacing = ParseMonument(doc.dump());
	expectThrowNaming([&] { MonumentPeristyleSettings(spacing); }, "'peristyle.frontSpacing'");

	json given = EmbeddedDocument();
	given["krepis"]["euthynteria"] = {{"value", 0.3}, {"unit", "m"}, {"status", "lu"}, {"sources", {"S"}},
	                                  {"readme", std::string(kSection) + "3.2"}};
	const MonumentData euthynteria = ParseMonument(given.dump());
	expectThrowNaming([&] { MonumentKrepisSettings(euthynteria); }, "'krepis.euthynteria'");

	json four = EmbeddedDocument();
	four["krepis"]["stepCount"]["value"] = 4;
	const MonumentData steps = ParseMonument(four.dump());
	expectThrowNaming([&] { MonumentKrepisSettings(steps); }, "'krepis.stepCount'");

	MonumentData notCount = base;
	notCount.peristyle.frontColumns.value = 6.5;
	expectThrowNaming([&] { MonumentPeristyleSettings(notCount); }, "'peristyle.frontColumns'");

	// Colonne : une valeur employee manquante, pour les deux jeux et pour le temple.
	MonumentData lip = base;
	lip.columns.echinusLipRadius.value.reset();
	lip.columns.echinusLipRadius.status = MonumentStatus::Missing;
	lip.columns.echinusLipRadius.sources.clear();
	for (const bool corner : {false, true})
		expectThrowNaming([&] { MonumentColumnSettings(lip, corner); }, "'columns.echinusLipRadius'");
	expectThrowNaming([&] { MonumentTempleSettings(lip); }, "'columns.echinusLipRadius'");

	// GD3d ne vaut que pour une profondeur de rainure manquante.
	json depth = EmbeddedDocument();
	depth["columns"]["grooveDepth"] = {{"value", 0.002}, {"unit", "m"}, {"status", "convention"},
	                                   {"sources", json::array()}, {"readme", std::string(kSection) + "5.2"}};
	const MonumentData given_depth = ParseMonument(depth.dump());
	expectThrowNaming([&] { MonumentColumnSettings(given_depth, false); }, "'columns.grooveDepth'");

	// Un compte de listels nul n'est pas un compte.
	MonumentData noStep = base;
	noStep.columns.annuletCount.value = 0.0;
	expectThrowNaming([&] { MonumentColumnSettings(noStep, false); }, "'columns.annuletCount'");

	// Au-dela de kGreekDoricMaxAnnulets, la conversion refuse avant d'allouer les listels.
	for (const double count : {static_cast<double>(kGreekDoricMaxAnnulets + 1), 1000.0, 2e9})
	{
		SCOPED_TRACE(count);
		MonumentData many = base;
		many.columns.annuletCount.value = count;
		expectThrowNaming([&] { MonumentColumnSettings(many, false); }, "'columns.annuletCount'");
	}
	MonumentData eight = base;
	eight.columns.annuletCount.value = static_cast<double>(kGreekDoricMaxAnnulets);
	EXPECT_EQ(MonumentColumnSettings(eight, false).annulets.size(), static_cast<std::size_t>(kGreekDoricMaxAnnulets));

	// Entablement. Controle positif : le document embarque se convertit, si bien que chaque refus
	// ci-dessous tient a la modification.
	EXPECT_NO_THROW(MonumentEntablatureSettings(base));
	// EN1 ne vaut que pour une face interieure manquante.
	json inner = EmbeddedDocument();
	inner["entablature"]["innerFace"] = {{"value", 0.494}, {"unit", "m"}, {"status", "convention"},
	                                     {"sources", json::array()}, {"readme", std::string(kSection) + "6.2"}};
	const MonumentData givenInner = ParseMonument(inner.dump());
	expectThrowNaming([&] { MonumentEntablatureSettings(givenInner); }, "'entablature.innerFace'");
	expectThrowNaming([&] { MonumentTempleSettings(givenInner); }, "'entablature.innerFace'");
	// Partage du chapiteau du triglyphe incoherent avec sa hauteur : listel de 1,1 in.
	MonumentData partition = base;
	partition.entablature.triglyphCapitalListelHeight.value = 1.1;
	expectThrowNaming([&] { MonumentEntablatureSettings(partition); }, "'entablature.triglyphCapitalBandHeight'");
	// Disposition des divisions : demi-canal interieur, canal a un bout, canaux inegaux.
	MonumentData innerHalf = base;
	innerHalf.entablature.triglyphDivisions.kinds[1] = MonumentDivisionKind::HalfChannel;
	expectThrowNaming([&] { MonumentEntablatureSettings(innerHalf); }, "'entablature.triglyphDivisions'");
	MonumentData channelAtEnd = base;
	channelAtEnd.entablature.triglyphDivisions.kinds[0] = MonumentDivisionKind::Channel;
	expectThrowNaming([&] { MonumentEntablatureSettings(channelAtEnd); }, "'entablature.triglyphDivisions'");
	MonumentData unequal = base;
	unequal.entablature.triglyphDivisions.values[2] = 3.9;
	expectThrowNaming([&] { MonumentEntablatureSettings(unequal); }, "'entablature.triglyphDivisions'");
	// Sans canal, EN11 n'a pas de largeur de canal a lire.
	MonumentData noChannel = base;
	noChannel.entablature.triglyphDivisions.kinds = {
		MonumentDivisionKind::HalfChannel, MonumentDivisionKind::Band, MonumentDivisionKind::Band,
		MonumentDivisionKind::Band,        MonumentDivisionKind::Band, MonumentDivisionKind::Band,
		MonumentDivisionKind::HalfChannel};
	expectThrowNaming([&] { MonumentEntablatureSettings(noChannel); }, "'entablature.triglyphDivisions'");
	// Pieds non entiers.
	MonumentData halfFoot = base;
	halfFoot.entablature.friezeHeightFt.value = 2.5;
	expectThrowNaming([&] { MonumentEntablatureSettings(halfFoot); }, "'entablature.friezeHeightFt'");
	// Une valeur employee manquante.
	MonumentData noFace = base;
	noFace.entablature.triglyphFace.value.reset();
	noFace.entablature.triglyphFace.status = MonumentStatus::Missing;
	noFace.entablature.triglyphFace.sources.clear();
	expectThrowNaming([&] { MonumentEntablatureSettings(noFace); }, "'entablature.triglyphFace'");
	expectThrowNaming([&] { MonumentTempleSettings(noFace); }, "'entablature.triglyphFace'");
}

// J10
TEST(TEST_cgarchi_classical_monument, every_value_has_a_unit_from_the_closed_list)
{
	const MonumentData &data = HephaistosMonument();
	std::set<MonumentUnit> seen;
	for (const auto &[key, v] : AllValues(data))
	{
		SCOPED_TRACE(key);
		seen.insert(v->unit);
		EXPECT_EQ(v->unit == MonumentUnit::One, CountKeys().count(key) == 1u);
		EXPECT_EQ(v->unit == MonumentUnit::Foot, IsFeetKey(key));
		EXPECT_EQ(v->unit == MonumentUnit::Degree, key == "entablature.soffitSlope");
	}
	EXPECT_EQ(seen.size(), 6u);
	// Chacune telle qu'ecrite au README ; une valeur manquante porte l'unite attendue (l. 31).
	EXPECT_EQ(data.columns.lowerDiameter.unit, MonumentUnit::Metre);         // l. 90, « 1,016 m »
	EXPECT_EQ(data.columns.abacusHalfWidth.unit, MonumentUnit::Inch);        // l. 165, « 22,451 in »
	EXPECT_EQ(data.columns.entasisSagitta.unit, MonumentUnit::Millimetre);   // l. 156, « 7 mm »
	EXPECT_EQ(data.columns.grooveDepth.unit, MonumentUnit::Metre);
	const MonumentColumnControls &cc = data.columnControls;
	for (const MonumentControlValue *c : {&cc.shaftHeight, &cc.footFluteChord, &cc.fluteCentralRadius,
	                                      &cc.fluteLateralRadius, &cc.topFluteDepth, &cc.cornerFluteChord,
	                                      &cc.cornerFluteDepth, &cc.capitalHeight, &cc.annuletsAndRelief,
	                                      &cc.upperRadius, &cc.echinusLipRetreat})
		EXPECT_NE(c->unit, MonumentUnit::One);
	// Controles d'angle de la frise (l. 255-256) : en metres tels qu'ecrits, y compris les deux
	// metopes interieures de la seconde solution, dont le README ecrit l'unite apres la seconde (R8).
	const MonumentEntablatureControls &ec = data.entablatureControls;
	for (const MonumentControlValue *c : {&ec.cornerTriglyphOffset, &ec.frontCornerDistance, &ec.frontCornerMetope,
	                                      &ec.flankCornerMetope, &ec.intermediateShift, &ec.frontMidBayInnerMetope,
	                                      &ec.frontMidBayOuterMetope, &ec.flankMidBayInnerMetope,
	                                      &ec.flankMidBayOuterMetope})
		EXPECT_EQ(c->unit, MonumentUnit::Metre);
	ExpectEditRefused([&](json &d) { d["entablatureControls"]["frontMidBayInnerMetope"]["unit"] = "1"; },
	                  "entablatureControls.frontMidBayInnerMetope.unit", "unit \"1\" on a length");

	// Conversion en metres : 1 in = 0,0254 m, 1 mm = 0,001 m.
	EXPECT_EQ(MonumentLength(data.columns.entasisSagitta, "columns.entasisSagitta"), 0.007);
	EXPECT_EQ(MonumentLength(data.columns.abacusHeight, "columns.abacusHeight"), 7.85 * 0.0254);
	EXPECT_EQ(MonumentLength(data.columns.height, "columns.height"), 5.715);
	json inches = EmbeddedDocument();
	inches["columns"]["entasisSagitta"]["unit"] = "in";
	EXPECT_EQ(MonumentLength(ParseMonument(inches.dump()).columns.entasisSagitta, "s"), 7.0 * 0.0254);
	EXPECT_EQ(MonumentMetres(1.0, MonumentUnit::Foot, "pied"), 0.3048);

	// Cote en pieds et pouces (README l. 209, 2 ft 8,9 in), calculee en pouces : 2 x 12 est exact
	// et le produit par le pouce vient en dernier, donc l'egalite au bit pres tient avec ou sans
	// contraction FMA ; ft x 0,3048 + in x 0,0254 en differe ici (plan 3, MS-40).
	const MonumentEntablature &ent = data.entablature;
	EXPECT_EQ(Value(ent.architraveHeightFt), 2.0);
	EXPECT_EQ(Value(ent.architraveHeightIn), 8.9);
	EXPECT_EQ(MonumentFeetAndInches(ent.architraveHeightFt, ent.architraveHeightIn, "entablature.architraveHeight"),
	          (2.0 * 12.0 + 8.9) * kMonumentInch);
	for (const MonumentValue *feet : {&ent.architraveHeightFt, &ent.friezeHeightFt, &ent.corniceHeightFt,
	                                  &ent.triglyphWidthFt, &ent.metopeWidthFt})
		EXPECT_EQ(feet->unit, MonumentUnit::Foot);

	// MonumentLength : refus d'une valeur manquante et d'une valeur sans dimension.
	const auto expectRefusalNaming = [](const MonumentValue &v, const std::string &key) {
		try
		{
			MonumentLength(v, key);
			ADD_FAILURE() << "aucun refus : " << key;
		}
		catch (const std::invalid_argument &e)
		{
			const std::string message = e.what();
			EXPECT_EQ(message.rfind("Monument : ", 0), 0u) << message;
			EXPECT_NE(message.find("'" + key + "'"), std::string::npos) << message;
		}
	};
	expectRefusalNaming(data.krepis.euthynteria, "krepis.euthynteria");
	expectRefusalNaming(data.columns.grooveDepth, "columns.grooveDepth");
	expectRefusalNaming(data.krepis.stepCount, "krepis.stepCount");
	expectRefusalNaming(data.columns.fluteCount, "columns.fluteCount");
	// Des pieds seuls ne sont pas une longueur : une cote en pieds et pouces passe par MonumentFeetAndInches.
	expectRefusalNaming(data.entablature.friezeHeightFt, "entablature.friezeHeightFt");

	// Refus du lecteur, nommes.
	const std::string a = "columns.abacusHeight";
	const auto ab = [](json &d) -> json & { return d["columns"]["abacusHeight"]; };
	ExpectEditRefused([&](json &d) { ab(d).erase("unit"); }, a + ".unit", "required field is missing");
	ExpectEditRefused([&](json &d) { ab(d)["unit"] = "yd"; }, a + ".unit", "unknown value \"yd\"");
	ExpectEditRefused([&](json &d) { ab(d)["unit"] = "ft"; }, a + ".unit", "unit \"ft\" outside a feet key");
	ExpectEditRefused([&](json &d) { d["entablature"]["friezeHeightFt"]["unit"] = "in"; },
	                  "entablature.friezeHeightFt.unit", "a feet value requires unit \"ft\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["entablature"]["friezeHeightFt"]["value"] = 2.5; },
	                  "entablature.friezeHeightFt.value", "expected an integer, not number");
	ExpectEditRefused([&](json &d) { d["entablature"]["friezeHeightFt"]["value"] = 0; },
	                  "entablature.friezeHeightFt.value", "0 outside [1, ");
	ExpectEditRefused([&](json &d) { ab(d)["unit"] = nullptr; }, a + ".unit", "expected a string, not null");
	ExpectEditRefused([&](json &d) { ab(d)["unit"] = "1"; }, a + ".unit", "unit \"1\" on a length");
	ExpectEditRefused([&](json &d) { d["columns"]["grooveDepth"]["unit"] = "1"; }, "columns.grooveDepth.unit",
	                  "unit \"1\" on a length");
	ExpectEditRefused([&](json &d) { d["columns"]["fluteCount"]["unit"] = "m"; }, "columns.fluteCount.unit",
	                  "a count requires unit \"1\", not \"m\"");
	ExpectEditRefused([&](json &d) { d["krepis"]["stepCount"]["unit"] = "in"; }, "krepis.stepCount.unit",
	                  "a count requires unit \"1\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["columnControls"]["capitalHeight"]["unit"] = "1"; },
	                  "columnControls.capitalHeight.unit", "unit \"1\" on a length");
	ExpectEditRefused([&](json &d) { d["columnControls"]["capitalHeight"]["value"] = 0.0; },
	                  "columnControls.capitalHeight.value", "0 outside (0, inf]");
	ExpectEditRefused([&](json &d) { d["columnControls"]["echinusProfile"]["unit"] = "1"; },
	                  "columnControls.echinusProfile.unit", "unit \"1\" on a length");
	ExpectEditRefused([&](json &d) { d["columnControls"]["echinusProfile"]["retreats"].erase(std::size_t{6}); },
	                  "columnControls.echinusProfile.retreats", "not as many retreats as depths");
	ExpectEditRefused([&](json &d) { std::swap(d["columnControls"]["echinusProfile"]["depths"][1],
	                                           d["columnControls"]["echinusProfile"]["depths"][2]); },
	                  "columnControls.echinusProfile.depths[2]", "not strictly greater than the previous depth");
	ExpectEditRefused([&](json &d) { d["columnControls"]["echinusProfile"]["depths"] = json::array();
	                                 d["columnControls"]["echinusProfile"]["retreats"] = json::array(); },
	                  "columnControls.echinusProfile.depths", "empty array");

	// Regulae, gouttes et mutules (l. 212, 231, 232, 273, 274, 277) : nombre de gouttes en "1", largeur des
	// mutules en pieds et pouces (R6), le reste en pouces, tels qu'ecrits.
	EXPECT_EQ(ent.regulaGuttaCount.unit, MonumentUnit::One);
	EXPECT_EQ(ent.mutuleWidthFt.unit, MonumentUnit::Foot);
	for (const MonumentValue *v : {&ent.regulaProjection, &ent.regulaGuttaDiameter, &ent.regulaGuttaGap,
	                               &ent.regulaGuttaTopProjection, &ent.regulaGuttaFootProjection, &ent.mutuleWidthIn,
	                               &ent.mutuleDepth})
		EXPECT_EQ(v->unit, MonumentUnit::Inch);
	const MonumentEntablatureControls &ornaments = data.entablatureControls;
	EXPECT_EQ(ornaments.regulaGuttaCountCheck.unit, MonumentUnit::One);
	for (const MonumentControlValue *c : {&ornaments.regulaGuttaMargin, &ornaments.regulaGuttaShortfall,
	                                      &ornaments.mutuleBack})
		EXPECT_EQ(c->unit, MonumentUnit::Inch);
	EXPECT_EQ(MonumentFeetAndInches(ent.mutuleWidthFt, ent.mutuleWidthIn, "entablature.mutuleWidth"),
	          (1.0 * 12.0 + 8.75) * kMonumentInch);
	ExpectEditRefused([&](json &d) { d["entablature"]["regulaGuttaCount"]["value"] = 6.5; },
	                  "entablature.regulaGuttaCount.value", "expected an integer, not number");
	ExpectEditRefused([&](json &d) { d["entablature"]["regulaGuttaCount"]["unit"] = "in"; },
	                  "entablature.regulaGuttaCount.unit", "a count requires unit \"1\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["entablature"]["regulaGuttaCount"]["value"] = 0; },
	                  "entablature.regulaGuttaCount.value", "0 outside [1, ");
	ExpectEditRefused([&](json &d) { d["entablature"]["mutuleWidthFt"]["unit"] = "in"; },
	                  "entablature.mutuleWidthFt.unit", "a feet value requires unit \"ft\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["entablature"]["mutuleDepth"]["unit"] = "1"; },
	                  "entablature.mutuleDepth.unit", "unit \"1\" on a length");
	ExpectEditRefused([&](json &d) { d["entablatureControls"]["regulaGuttaCountCheck"]["unit"] = "in"; },
	                  "entablatureControls.regulaGuttaCountCheck.unit", "a count requires unit \"1\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["entablatureControls"]["mutuleBack"]["value"] = 0.0; },
	                  "entablatureControls.mutuleBack.value", "0 outside (0, inf]");

	// Sous-face inclinee et gouttes des mutules (l. 275, 276) : pente en "°" (R9), gouttes par rang en "1", le reste
	// en pouces ; le degre n'est l'unite d'aucune longueur, et un angle n'en a pas d'autre.
	const std::string degree = kDegree;
	EXPECT_EQ(ent.soffitSlope.unit, MonumentUnit::Degree);
	EXPECT_EQ(ent.mutuleGuttaPerRow.unit, MonumentUnit::One);
	for (const MonumentValue *v : {&ent.mutuleGuttaFrontDiameter, &ent.mutuleGuttaMiddleDiameter,
	                               &ent.mutuleGuttaRearDiameter, &ent.mutuleGuttaFrontGap, &ent.mutuleGuttaRearGap,
	                               &ent.mutuleGuttaHeight, &ent.mutuleGuttaEdgeAxis, &ent.mutuleGuttaFootFlare})
		EXPECT_EQ(v->unit, MonumentUnit::Inch);
	EXPECT_EQ(ornaments.mutuleGuttaRows.unit, MonumentUnit::One);
	EXPECT_EQ(ornaments.mutuleGuttaPitchCount.unit, MonumentUnit::One);
	expectRefusalNaming(ent.soffitSlope, "entablature.soffitSlope");
	// Le refus dit pourquoi : un angle, et non une unite hors de l'enumeration.
	try
	{
		MonumentLength(ent.soffitSlope, "entablature.soffitSlope");
		ADD_FAILURE() << "aucun refus : un angle lu comme une longueur";
	}
	catch (const std::invalid_argument &x)
	{
		EXPECT_NE(std::string(x.what()).find("angle"), std::string::npos) << x.what();
	}
	// La conversion refuse une pente hors de ]0, 90[ degres, cle nommee : 374 rendrait la geometrie de 14, 194 et 90
	// une sous-face qui ne descend pas vers la face ; 89 est convertie.
	for (const double slope : {90.0, 194.0, 374.0, 0.0, -14.0})
	{
		SCOPED_TRACE(slope);
		MonumentData changed = data;
		changed.entablature.soffitSlope.value = slope;
		try
		{
			MonumentEntablatureSettings(changed);
			ADD_FAILURE() << "aucun refus de la pente";
		}
		catch (const std::invalid_argument &x)
		{
			const std::string message = x.what();
			EXPECT_NE(message.find("'entablature.soffitSlope'"), std::string::npos) << message;
			EXPECT_NE(message.find("]0, 90["), std::string::npos) << message;
		}
	}
	{
		MonumentData changed = data;
		changed.entablature.soffitSlope.value = 89.0;
		EXPECT_NO_THROW(MonumentEntablatureSettings(changed));
	}
	ExpectEditRefused([&](json &d) { d["entablature"]["soffitSlope"]["unit"] = "in"; }, "entablature.soffitSlope.unit",
	                  "an angle requires unit \"" + degree + "\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["entablature"]["soffitSlope"]["value"] = 0.0; },
	                  "entablature.soffitSlope.value", "0 outside (0, inf]");
	ExpectEditRefused([&](json &d) { d["entablature"]["mutuleGuttaHeight"]["unit"] = degree; },
	                  "entablature.mutuleGuttaHeight.unit", "unit \"" + degree + "\" on a length");
	ExpectEditRefused([&](json &d) { d["entablatureControls"]["soffitRise"]["unit"] = degree; },
	                  "entablatureControls.soffitRise.unit", "unit \"" + degree + "\" on a length");
	ExpectEditRefused([&](json &d) { d["entablature"]["mutuleGuttaPerRow"]["unit"] = degree; },
	                  "entablature.mutuleGuttaPerRow.unit", "a count requires unit \"1\", not \"" + degree + "\"");
	ExpectEditRefused([&](json &d) { d["entablature"]["mutuleGuttaPerRow"]["value"] = 6.5; },
	                  "entablature.mutuleGuttaPerRow.value", "expected an integer, not number");
	ExpectEditRefused([&](json &d) { d["entablature"]["mutuleWidthFt"]["unit"] = degree; },
	                  "entablature.mutuleWidthFt.unit", "a feet value requires unit \"ft\", not \"" + degree + "\"");
}

// J11
TEST(TEST_cgarchi_classical_monument, the_column_controls_of_the_readme_recompute_from_the_values)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentColumns &c = d.columns;
	const MonumentColumnControls &cc = d.columnControls;
	const GreekDoricColumnSettings s = MonumentColumnSettings(d, false);
	const GreekDoricColumnSettings corner = MonumentColumnSettings(d, true);

	// Chapiteau du README (l. 449) : abaque, echine, annelets, gorgerin ; la rainure en dehors.
	const double capital = Length(c.abacusHeight) + Length(c.echinusHeight) + Length(c.annuletsHeight)
	                     + Length(c.hypotrachelionHeight);
	EXPECT_NEAR(capital, Metres(cc.capitalHeight), kInchHalfUnit);
	// Fut du README (l. 149) : hauteur de colonne moins chapiteau, rainure comprise.
	const double shaft = Length(c.height) - capital;
	EXPECT_NEAR(shaft, Metres(cc.shaftHeight), kPrintedHalfUnit);
	// Le fut du modele, plus la rainure que porte son chapiteau (GD3c), est le fut du README.
	double modelCapital = s.hypotrachelionHeight + s.echinusHeight + s.abacusHeight;
	for (const GreekDoricAnnulet &a : s.annulets)
		modelCapital += a.height;
	EXPECT_NEAR(s.height - modelCapital + s.grooveHeight, shaft, 1e-12);
	// Maximum de l'entasis a mi-hauteur du fut du README (l. 157) : zf est lu, il n'est pas
	// recalcule sur le fut du modele.
	EXPECT_NEAR(s.entasisHeight, 0.5 * shaft, kPrintedHalfUnit);
	RecordProperty("zf_minus_half_readme_shaft_mm", std::to_string(1e3 * (s.entasisHeight - 0.5 * shaft)));
	RecordProperty("zf_minus_half_model_shaft_mm",
	               std::to_string(1e3 * (s.entasisHeight - 0.5 * (s.height - modelCapital))));
	EXPECT_NEAR(s.entasisSagitta, 0.007, 1e-15);   // l. 156, 7 mm

	// Corde au pied (l. 151, « 6,25 in = 0,159 m ») : le modele la deduit de D de [D], le README
	// la lit sur [S]. MO1 : demi-unite de 6,25 in (0,005 in) plus celle de D (0,5 mm) portee sur
	// la corde, D sin (pi / N).
	const double halfAngle = kPi / s.fluteCount;
	const double chord = s.lowerDiameter * std::sin(halfAngle);
	const double footChordTolerance = kHundredthInchHalfUnit + kPrintedHalfUnit * std::sin(halfAngle);
	EXPECT_EQ(cc.footFluteChord.unit, MonumentUnit::Inch);
	EXPECT_NEAR(chord, Metres(cc.footFluteChord), footChordTolerance);
	RecordProperty("foot_chord_tolerance_mm", std::to_string(1e3 * footChordTolerance));
	RecordProperty("foot_chord_gap_mm", std::to_string(1e3 * (chord - Metres(cc.footFluteChord))));
	// Fausse ellipse (l. 154) sur la corde et la profondeur du README : Rc = 6,37 in, arrondi au
	// centieme de 6,3652, et Rc / 3 = 2,12 in, chacun a la demi-unite (0,005 in).
	const ThreeCentredFlute readme = SolveThreeCentredFlute(Length(c.fluteDepth) / Metres(cc.footFluteChord));
	const double rc = readme.centralRadiusRatio * Metres(cc.footFluteChord);
	EXPECT_EQ(cc.fluteCentralRadius.unit, MonumentUnit::Inch);
	EXPECT_NEAR(rc, Metres(cc.fluteCentralRadius), kHundredthInchHalfUnit);
	EXPECT_NEAR(rc / 3.0, Metres(cc.fluteLateralRadius), kHundredthInchHalfUnit);
	RecordProperty("central_radius_in", std::to_string(rc / kMonumentInch));
	RecordProperty("readme_central_radius_in", std::to_string(cc.fluteCentralRadius.value));
	const double modelRc = SolveThreeCentredFlute(s.fluteDepthRatio).centralRadiusRatio * chord;
	RecordProperty("model_lateral_radius_in", std::to_string(modelRc / 3.0 / kMonumentInch));

	// Cannelures d'angle (l. 130) : section homothetique, meme rapport profondeur / corde.
	const double cornerChord = corner.lowerDiameter * std::sin(halfAngle);
	EXPECT_NEAR(cornerChord, Metres(cc.cornerFluteChord), kPrintedHalfUnit);
	EXPECT_NEAR(corner.fluteDepthRatio * cornerChord, Metres(cc.cornerFluteDepth), kPrintedHalfUnit);
	RecordProperty("corner_flute_depth_gap_mm",
	               std::to_string(1e3 * (corner.fluteDepthRatio * cornerChord - Metres(cc.cornerFluteDepth))));
	// Profondeur au sommet (l. 155), MO1 etendue au produit et au quotient.
	const double topDepth = s.fluteDepthRatio * s.upperDiameter * std::sin(halfAngle);
	EXPECT_NEAR(topDepth, Metres(cc.topFluteDepth), kTopFluteDepthTolerance);
	RecordProperty("top_flute_depth_in", std::to_string(topDepth / kMonumentInch));

	// Rayon superieur de [S] (l. 451) : demi-cote de l'abaque moins annelets et ressaut.
	EXPECT_NEAR(Length(c.abacusHalfWidth) - Metres(cc.annuletsAndRelief), Metres(cc.upperRadius), kInchHalfUnit);
	EXPECT_GT(Metres(cc.annuletsAndRelief), Length(c.annuletsBottomRetreat));
	RecordProperty("gd5a_upper_radius_gap_mm", std::to_string(1e3 * (Metres(cc.upperRadius) - 0.5 * s.upperDiameter)));
	// Pied de l'echine au profil (l. 190) : le sommet des annelets (l. 167), au fond de l'echine.
	const MonumentProfileControl &p = cc.echinusProfile;
	ASSERT_FALSE(p.depths.empty());
	EXPECT_EQ(MonumentMetres(p.retreats.back(), p.unit, "retrait"), Length(c.annuletsTopRetreat));
	EXPECT_EQ(MonumentMetres(p.depths.back(), p.unit, "profondeur"), Length(c.echinusHeight));
}

// J12
TEST(TEST_cgarchi_classical_monument, the_two_column_settings_differ_only_by_their_lower_diameter)
{
	const MonumentData &d = HephaistosMonument();
	const GreekDoricColumnSettings s = MonumentColumnSettings(d, false);
	const GreekDoricColumnSettings c = MonumentColumnSettings(d, true);
	EXPECT_EQ(s.lowerDiameter, Length(d.columns.lowerDiameter));
	EXPECT_EQ(c.lowerDiameter, Length(d.columns.cornerLowerDiameter));
	EXPECT_NE(s.lowerDiameter, c.lowerDiameter);

	EXPECT_EQ(s.height, c.height);
	EXPECT_EQ(s.upperDiameter, c.upperDiameter);
	EXPECT_EQ(s.entasisSagitta, c.entasisSagitta);
	EXPECT_EQ(s.entasisHeight, c.entasisHeight);
	EXPECT_EQ(s.fluteCount, c.fluteCount);
	EXPECT_EQ(s.fluteSection, c.fluteSection);
	EXPECT_EQ(s.fluteDepthRatio, c.fluteDepthRatio);
	EXPECT_EQ(s.hypotrachelionHeight, c.hypotrachelionHeight);
	EXPECT_EQ(s.grooveHeight, c.grooveHeight);
	EXPECT_EQ(s.grooveDepth, c.grooveDepth);
	ASSERT_EQ(s.annulets.size(), c.annulets.size());
	for (std::size_t k = 0; k < s.annulets.size(); ++k)
	{
		EXPECT_EQ(s.annulets[k].height, c.annulets[k].height) << k;
		EXPECT_EQ(s.annulets[k].projection, c.annulets[k].projection) << k;
	}
	EXPECT_EQ(s.echinusHeight, c.echinusHeight);
	EXPECT_EQ(s.echinusFootProjection, c.echinusFootProjection);
	EXPECT_EQ(s.echinusJunctionHeight, c.echinusJunctionHeight);
	EXPECT_EQ(s.echinusJunctionProjection, c.echinusJunctionProjection);
	EXPECT_EQ(s.echinusTopProjection, c.echinusTopProjection);
	EXPECT_EQ(s.echinusLipRadius, c.echinusLipRadius);
	EXPECT_EQ(s.abacusHeight, c.abacusHeight);
	EXPECT_EQ(s.abacusProjection, c.abacusProjection);
	// Resolution : valeurs par defaut des reglages, qui ne sont pas des valeurs du domaine.
	const GreekDoricColumnSettings defaults;
	for (const GreekDoricColumnSettings *x : {&s, &c})
	{
		EXPECT_EQ(x->curveSegments, defaults.curveSegments);
		EXPECT_EQ(x->shaftSegments, defaults.shaftSegments);
		EXPECT_EQ(x->fluteSamples, defaults.fluteSamples);
		EXPECT_EQ(x->radialSegments, defaults.radialSegments);
	}
}

// J13
TEST(TEST_cgarchi_classical_monument, the_echinus_of_the_settings_passes_through_the_engraved_points)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentColumns &c = d.columns;
	const MonumentProfileControl &p = d.columnControls.echinusProfile;
	const double A = Length(c.abacusHalfWidth);
	const double H = Length(c.echinusHeight);
	const double junctionDepth = Length(c.echinusJunctionDepth);

	// Oracle : cercle de la levre par la jonction et la tete, de rayon echinusLipRadius, centre du
	// cote rentrant ; coordonnees (rayon depuis l'axe, hauteur depuis le pied de l'echine).
	const double jx = A - Length(c.echinusJunctionRetreat), jy = H - junctionDepth;
	const double tx = A - Length(c.echinusTopRetreat), ty = H;
	const double lip = Length(c.echinusLipRadius);
	const double mx = 0.5 * (jx + tx), my = 0.5 * (jy + ty);
	const double chord = std::hypot(tx - jx, ty - jy);
	const double offset = std::sqrt(lip * lip - 0.25 * chord * chord);
	const double nx = (ty - jy) / chord, ny = -(tx - jx) / chord;   // normale vers les rayons croissants
	const double cx = mx - offset * nx, cy = my - offset * ny;
	ASSERT_LT(cx, jx);
	ASSERT_LT(cx, tx);
	const double footRetreat = Length(c.annuletsTopRetreat);
	const auto retreatAt = [&](double depth) {
		if (depth < junctionDepth)
			return A - (cx + std::sqrt(lip * lip - (H - depth - cy) * (H - depth - cy)));
		const double t = (depth - junctionDepth) / (H - junctionDepth);
		return Length(c.echinusJunctionRetreat) + t * (footRetreat - Length(c.echinusJunctionRetreat));
	};

	for (const bool corner : {false, true})
	{
		SCOPED_TRACE(corner ? "angle" : "courante");
		const ColumnProfile profile = BuildGreekDoricColumnProfile(MonumentColumnSettings(d, corner));
		std::size_t echinus = profile.members.size();
		for (std::size_t i = 0; i < profile.members.size(); ++i)
			if (profile.members[i].name == "echine")
				echinus = i;
		ASSERT_LT(echinus, profile.members.size());
		const ProfileMember &m = profile.members[echinus];
		// Sommets du profil de l'echine en (profondeur, retrait), pied compris (fusionne avec le
		// haut du dernier listel).
		std::vector<std::pair<double, double>> points;
		for (const ProfileVertex &v : profile.vertices)
			if (v.ring.shape == RingShape::Circle && v.y >= m.y0 - 1e-12 && v.y <= m.y1 + 1e-12)
			{
				points.push_back({H - (v.y - m.y0), A - v.ring.radius});
				if (v.y - m.y0 > jy + 1e-12)
					EXPECT_NEAR(std::hypot(v.ring.radius - cx, v.y - m.y0 - cy), lip, 1e-9) << v.y;
			}
		ASSERT_GE(points.size(), 4u);

		double worstMeasured = 0.0;
		for (std::size_t i = 0; i < p.depths.size(); ++i)
		{
			const double depth = MonumentMetres(p.depths[i], p.unit, "profondeur");
			const double retreat = MonumentMetres(p.retreats[i], p.unit, "retrait");
			SCOPED_TRACE(p.depths[i]);
			const bool engraved = depth == 0.0 || depth == junctionDepth || depth == H;
			if (engraved)
			{
				const auto hit = std::find_if(points.begin(), points.end(), [&](const auto &q) {
					return std::fabs(q.first - depth) <= 1e-12 && std::fabs(q.second - retreat) <= 1e-12;
				});
				EXPECT_NE(hit, points.end()) << "point grave absent du profil";
			}
			else
			{
				const double gap = retreatAt(depth) - retreat;
				EXPECT_NEAR(gap, 0.0, kProfileTolerance);
				if (depth < junctionDepth)
					worstMeasured = std::max(worstMeasured, std::fabs(gap));
				RecordProperty("gap_in_at_depth_" + std::to_string(p.depths[i]), std::to_string(gap / kMonumentInch));
			}
		}
		RecordProperty("lip_worst_gap_in", std::to_string(worstMeasured / kMonumentInch));

		// Saillie maximale (l. 197) : le sommet le plus saillant de l'echine.
		double least = points.front().second;
		for (const auto &q : points)
			least = std::min(least, q.second);
		EXPECT_NEAR(least, Metres(d.columnControls.echinusLipRetreat), kProfileTolerance);
		RecordProperty("lip_least_retreat_in", std::to_string(least / kMonumentInch));
	}
}

// J14
TEST(TEST_cgarchi_classical_monument, the_temple_of_the_readme_is_laid_out_and_meshed)
{
	const TempleSettings t = MonumentTempleSettings(HephaistosMonument());
	TempleLayout layout;
	ASSERT_NO_THROW(layout = LayoutTemple(t));
	ASSERT_EQ(layout.columns.size(), 34u);
	int corners = 0;
	for (const TemplePlacement &placement : layout.columns)
		corners += placement.corner ? 1 : 0;
	EXPECT_EQ(corners, 4);

	std::size_t faces = 0;
	ASSERT_NO_THROW(faces = CountTempleMeshFaces(t));
	std::unique_ptr<Mesh> mesh;
	ASSERT_NO_THROW(mesh = BuildTempleMesh(t));
	EXPECT_EQ(static_cast<std::size_t>(mesh->GetNFaces()), faces);
	RecordProperty("temple_faces", std::to_string(faces));

	// Une colonne de chaque jeu : un temple entier coute plusieurs secondes a ValidateMesh.
	for (const GreekDoricColumnSettings *s : {&t.column, &t.cornerColumn})
	{
		SCOPED_TRACE(s == &t.column ? "courante" : "angle");
		const std::unique_ptr<Mesh> column = BuildGreekDoricColumnMesh(*s);
		EXPECT_TRUE(ValidateMesh(*column).IsValid());
		RecordProperty(s == &t.column ? "column_faces" : "corner_column_faces", std::to_string(column->GetNFaces()));
	}
}

// J15
TEST(TEST_cgarchi_classical_monument, the_capital_of_the_readme_has_four_steps_a_deep_groove_and_no_null_ring)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentColumns &c = d.columns;
	const double inch = kMonumentInch;
	// Oracle en pouces, valeurs du document telles qu'ecrites.
	const int steps = static_cast<int>(Value(c.annuletCount));
	const double A = Value(c.abacusHalfWidth), top = Value(c.annuletsTopRetreat), bottom = Value(c.annuletsBottomRetreat);
	ASSERT_EQ(steps, 4);

	for (const bool corner : {false, true})
	{
		SCOPED_TRACE(corner ? "angle" : "courante");
		const GreekDoricColumnSettings s = MonumentColumnSettings(d, corner);
		const ColumnProfile profile = BuildGreekDoricColumnProfile(s);
		const double R = 0.5 * s.upperDiameter;

		// Membres, de bas en haut, et leurs hauteurs.
		const std::vector<std::pair<std::string, double>> members = {
			{"rainure", Value(c.grooveHeight) * inch},
			{"gorgerin", Value(c.hypotrachelionHeight) * inch},
			{"annelet", Value(c.annuletsHeight) / steps * inch},
			{"annelet", Value(c.annuletsHeight) / steps * inch},
			{"annelet", Value(c.annuletsHeight) / steps * inch},
			{"annelet", Value(c.annuletsHeight) / steps * inch},
			{"echine", Value(c.echinusHeight) * inch},
			{"abaque", Value(c.abacusHeight) * inch},
		};
		ASSERT_EQ(profile.members.size(), members.size());
		for (std::size_t i = 0; i < members.size(); ++i)
		{
			SCOPED_TRACE(i);
			EXPECT_EQ(profile.members[i].name, members[i].first);
			EXPECT_NEAR(profile.members[i].y1 - profile.members[i].y0, members[i].second, 1e-12);
		}

		// Listels en gradins, face puis lit (GD3a, phase B) : saillies depuis d/2.
		ASSERT_EQ(s.annulets.size(), static_cast<std::size_t>(steps));
		for (int k = 0; k < steps; ++k)
		{
			const double expected = ((A - top) - (steps - 1 - k) * (bottom - top) / steps) * inch - R;
			EXPECT_NEAR(s.annulets[static_cast<std::size_t>(k)].projection, expected, 1e-12) << k;
		}
		EXPECT_EQ(s.annulets.back().projection, s.echinusFootProjection);

		// Anneaux du chapiteau : rainure 2, gorgerin 2, 2 par listel, echine (jonction, levre,
		// extremum interieur), abaque 2 ; le pied de l'echine fusionne avec le haut du dernier listel.
		std::size_t capital = 0;
		for (const ProfileVertex &v : profile.vertices)
			capital += v.member >= 0 ? 1u : 0u;
		EXPECT_EQ(capital, 2u + 2u + 2u * static_cast<std::size_t>(steps) + (1u + static_cast<std::size_t>(s.curveSegments) + 1u) + 2u);
		RecordProperty("capital_vertices", std::to_string(capital));
		// Aucune couronne de largeur nulle : deux sommets voisins a la meme cote ont des rayons
		// distincts d'au moins un micrometre.
		for (std::size_t i = 1; i < profile.vertices.size(); ++i)
			if (profile.vertices[i].y == profile.vertices[i - 1].y)
				EXPECT_GT(std::fabs(profile.vertices[i].ring.radius - profile.vertices[i - 1].ring.radius), 1e-6) << i;

		// Rainure (GD3d) : fond sous celui des cannelures au sommet du fut, d'une hauteur de rainure,
		// par l'oracle du support de test.
		const double flutesBottom = R * cgarchi_test::FluteBottomRatio(s.fluteCount, s.fluteSection, s.fluteDepthRatio);
		EXPECT_NEAR(flutesBottom - (R - s.grooveDepth), s.grooveHeight, 1e-12);
		for (const ProfileVertex &v : profile.vertices)
			if (v.member == 0)
			{
				EXPECT_EQ(v.ring.shape, RingShape::Circle);
				EXPECT_NEAR(v.ring.radius, R - s.grooveDepth, 1e-12);
			}
		RecordProperty("groove_depth_mm", std::to_string(1e3 * s.grooveDepth));

		// Abaque (GD5a) : demi-cote A du README depuis l'axe, soit une saillie A - R depuis d/2 ;
		// anneau carre du profil a ce demi-cote.
		EXPECT_NEAR(R + s.abacusProjection, A * inch, 1e-12);
		std::size_t abacus = 0;
		for (const ProfileVertex &v : profile.vertices)
			if (v.member == static_cast<int>(members.size()) - 1)
			{
				++abacus;
				EXPECT_EQ(v.ring.shape, RingShape::Square);
				EXPECT_NEAR(v.ring.radius, A * inch, 1e-12);
			}
		EXPECT_EQ(abacus, 2u);
	}
}

// J16
TEST(TEST_cgarchi_classical_monument, the_entablature_values_are_read_with_their_units_statuses_and_places)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablature &e = d.entablature;
	const std::vector<std::string> s = {"S"};
	std::size_t values = 0, read = 0;
	for (const auto &[key, v] : AllValues(d))
	{
		if (key.rfind("entablature.", 0) != 0)
			continue;
		SCOPED_TRACE(key);
		++values;
		if (v->status == MonumentStatus::Read)
		{
			++read;
			EXPECT_EQ(v->sources, s);
		}
	}
	EXPECT_EQ(values, 55u);
	EXPECT_EQ(read, 47u);
	// Partage du chapiteau du triglyphe : convention de la source, sans reference (l. 235).
	for (const MonumentValue *v : {&e.triglyphCapitalListelHeight, &e.triglyphCapitalBandHeight})
	{
		EXPECT_EQ(v->status, MonumentStatus::Convention);
		EXPECT_TRUE(v->sources.empty());
	}
	EXPECT_EQ(Value(e.triglyphCapitalListelHeight), 1.0);
	EXPECT_EQ(Value(e.triglyphCapitalBandHeight), 3.6);
	EXPECT_EQ(e.metopeFace.status, MonumentStatus::Deduced);
	EXPECT_EQ(e.metopeFace.sources, s);
	EXPECT_EQ(e.channelDepth.status, MonumentStatus::Measured);
	EXPECT_EQ(e.channelDepth.sources, s);
	EXPECT_EQ(e.innerFace.status, MonumentStatus::Missing);
	EXPECT_FALSE(e.innerFace.value.has_value());
	EXPECT_EQ(e.innerFace.unit, MonumentUnit::Metre);
	// Lieux de cinq cles temoins.
	const std::string section = kSection;
	EXPECT_EQ(e.friezeHeightFt.readme, section + "6.1, l. 214");
	EXPECT_EQ(e.friezeHeightIn.readme, section + "6.1, l. 214");
	EXPECT_EQ(e.triglyphCapitalBandHeight.readme, section + "6.2, l. 235");
	EXPECT_EQ(e.innerFace.readme, section + "6.2, l. 243");
	EXPECT_EQ(e.triglyphDivisions.readme, section + "6.3, l. 253");
	EXPECT_EQ(e.channelDepth.readme, section + "6.3, l. 254");

	// Division du triglyphe (l. 253), telle qu'ecrite, mots dans l'ordre.
	using K = MonumentDivisionKind;
	const MonumentDivisions &dv = e.triglyphDivisions;
	EXPECT_EQ(dv.values, (std::vector<double>{2.0, 2.8, 3.95, 2.8, 3.95, 2.8, 2.0}));
	EXPECT_EQ(dv.kinds, (std::vector<K>{K::HalfChannel, K::Band, K::Channel, K::Band, K::Channel, K::Band,
	                                    K::HalfChannel}));
	EXPECT_EQ(dv.unit, MonumentUnit::Inch);
	EXPECT_EQ(dv.status, MonumentStatus::Read);
	EXPECT_EQ(dv.sources, s);

	// Controles de l'entablement, lus avec leur unite.
	const MonumentEntablatureControls &c = d.entablatureControls;
	for (const MonumentControlValue *count : {&c.metopeCount, &c.frontTriglyphs, &c.flankTriglyphs})
		EXPECT_EQ(count->unit, MonumentUnit::One);
	EXPECT_EQ(c.metopeCount.value, 68.0);
	EXPECT_EQ(c.frontTriglyphs.value, 11.0);
	EXPECT_EQ(c.flankTriglyphs.value, 25.0);
	EXPECT_EQ(c.total.unit, MonumentUnit::Metre);
	EXPECT_EQ(c.corniceSum.unit, MonumentUnit::Inch);

	// Controles d'angle de la frise, lus tels qu'ecrits, avec leur lieu : l. 255 pour le centre du
	// triglyphe d'angle, l. 256 pour les deux solutions. Le lieu est controle par le lecteur sans
	// etre conserve : il se lit dans le document.
	const json document = EmbeddedDocument();
	const std::pair<const char *, std::pair<const MonumentControlValue *, double>> corner[] = {
		{"cornerTriglyphOffset", {&c.cornerTriglyphOffset, 0.231}},
		{"frontCornerDistance", {&c.frontCornerDistance, 2.632}},
		{"frontCornerMetope", {&c.frontCornerMetope, 0.801}},
		{"flankCornerMetope", {&c.flankCornerMetope, 0.802}},
		{"intermediateShift", {&c.intermediateShift, 0.116}},
		{"frontMidBayInnerMetope", {&c.frontMidBayInnerMetope, 0.685}},
		{"frontMidBayOuterMetope", {&c.frontMidBayOuterMetope, 0.916}},
		{"flankMidBayInnerMetope", {&c.flankMidBayInnerMetope, 0.686}},
		{"flankMidBayOuterMetope", {&c.flankMidBayOuterMetope, 0.918}},
	};
	for (const auto &[key, read] : corner)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(read.first->value, read.second);
		EXPECT_EQ(read.first->unit, MonumentUnit::Metre);
		const std::string line = std::string(key) == "cornerTriglyphOffset" ? "255" : "256";
		EXPECT_EQ(document["entablatureControls"][key]["readme"].get<std::string>(), section + "6.3, l. " + line);
	}

	// Refus du lecteur sur la suite.
	const std::string p = "entablature.triglyphDivisions";
	const auto at = [](json &doc) -> json & { return doc["entablature"]["triglyphDivisions"]; };
	ExpectEditRefused([&](json &doc) { at(doc)["values"] = json::array(); at(doc)["kinds"] = json::array(); },
	                  p + ".values", "empty array");
	ExpectEditRefused([&](json &doc) { at(doc)["kinds"].erase(std::size_t{6}); }, p + ".kinds",
	                  "not as many kinds as values");
	ExpectEditRefused([&](json &doc) { at(doc)["kinds"][0] = "glyphe"; }, p + ".kinds[0]",
	                  "unknown division kind \"glyphe\"");
	ExpectEditRefused([&](json &doc) { at(doc)["values"][1] = 0.0; }, p + ".values[1]", "not a finite width > 0");
	ExpectEditRefused([&](json &doc) { at(doc)["status"] = "manquant"; at(doc)["sources"] = json::array(); },
	                  p + ".status", "a division series is never \"manquant\"");
	ExpectEditRefused([&](json &doc) { at(doc)["unit"] = "1"; }, p + ".unit", "unit \"1\" on a length");
	// Refus du lecteur sur les controles.
	ExpectEditRefused([&](json &doc) { doc["entablatureControls"]["metopeCount"]["unit"] = "m"; },
	                  "entablatureControls.metopeCount.unit", "a count requires unit \"1\", not \"m\"");
	ExpectEditRefused([&](json &doc) { doc["entablatureControls"]["total"]["unit"] = "1"; },
	                  "entablatureControls.total.unit", "unit \"1\" on a length");
	ExpectEditRefused([&](json &doc) { doc["entablatureControls"]["metopeCount"]["value"] = 68.5; },
	                  "entablatureControls.metopeCount.value", "expected an integer, not number");
	ExpectEditRefused([&](json &doc) { doc["entablatureControls"]["intermediateShift"]["value"] = 0.0; },
	                  "entablatureControls.intermediateShift.value", "0 outside (0, inf]");
	ExpectEditRefused([&](json &doc) { doc["entablatureControls"]["flankMidBayOuterMetope"]["readme"] = ""; },
	                  "entablatureControls.flankMidBayOuterMetope.readme", "empty place");

	// Regulae, gouttes et mutules : les neuf valeurs telles qu'ecrites, lues [S], avec leur lieu.
	const std::tuple<const char *, const MonumentValue *, double, const char *> ornaments[] = {
		{"regulaProjection", &e.regulaProjection, 20.859, "6.2, l. 231"},
		{"regulaGuttaCount", &e.regulaGuttaCount, 6.0, "6.1, l. 212"},
		{"regulaGuttaDiameter", &e.regulaGuttaDiameter, 1.466, "6.1, l. 212"},
		{"regulaGuttaGap", &e.regulaGuttaGap, 2.25, "6.1, l. 212"},
		{"regulaGuttaTopProjection", &e.regulaGuttaTopProjection, 20.809, "6.2, l. 232"},
		{"regulaGuttaFootProjection", &e.regulaGuttaFootProjection, 20.859, "6.2, l. 232"},
		{"mutuleWidthFt", &e.mutuleWidthFt, 1.0, "6.4, l. 273"},
		{"mutuleWidthIn", &e.mutuleWidthIn, 8.75, "6.4, l. 273"},
		{"mutuleDepth", &e.mutuleDepth, 15.876, "6.4, l. 274"},
	};
	for (const auto &[key, v, written, place] : ornaments)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(Value(*v), written);
		EXPECT_EQ(v->status, MonumentStatus::Read);
		EXPECT_EQ(v->sources, s);
		EXPECT_EQ(v->readme, section + place);
	}
	// Controles des gouttes et du dos des mutules, lus tels qu'ecrits, avec leur lieu.
	const std::tuple<const char *, const MonumentControlValue *, double, const char *> checks[] = {
		{"regulaGuttaMargin", &c.regulaGuttaMargin, 0.127, "6.1, l. 212"},
		{"regulaGuttaShortfall", &c.regulaGuttaShortfall, 0.254, "6.1, l. 212"},
		{"regulaGuttaCountCheck", &c.regulaGuttaCountCheck, 6.0, "6.4, l. 277"},
		{"mutuleBack", &c.mutuleBack, 19.949, "6.4, l. 274"},
	};
	for (const auto &[key, control, written, place] : checks)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(control->value, written);
		EXPECT_EQ(document["entablatureControls"][key]["readme"].get<std::string>(), section + place);
	}

	// Sous-face inclinee et gouttes des mutules : les dix valeurs telles qu'ecrites, avec leur statut et leur lieu
	// (l. 275, 276) ; toutes citent [S].
	const MonumentStatus lu = MonumentStatus::Read, mesure = MonumentStatus::Measured, deduit = MonumentStatus::Deduced;
	const std::tuple<const char *, const MonumentValue *, double, MonumentStatus, const char *> soffit[] = {
		{"soffitSlope", &e.soffitSlope, 14.0, mesure, "6.4, l. 276"},
		{"mutuleGuttaFrontDiameter", &e.mutuleGuttaFrontDiameter, 1.46, lu, "6.4, l. 275"},
		{"mutuleGuttaMiddleDiameter", &e.mutuleGuttaMiddleDiameter, 1.55, lu, "6.4, l. 275"},
		{"mutuleGuttaRearDiameter", &e.mutuleGuttaRearDiameter, 1.5, lu, "6.4, l. 275"},
		{"mutuleGuttaFrontGap", &e.mutuleGuttaFrontGap, 4.4, lu, "6.4, l. 275"},
		{"mutuleGuttaRearGap", &e.mutuleGuttaRearGap, 4.466, lu, "6.4, l. 275"},
		{"mutuleGuttaHeight", &e.mutuleGuttaHeight, 1.0, mesure, "6.4, l. 275"},
		{"mutuleGuttaEdgeAxis", &e.mutuleGuttaEdgeAxis, 1.775, lu, "6.4, l. 275"},
		{"mutuleGuttaPerRow", &e.mutuleGuttaPerRow, 6.0, lu, "6.4, l. 275"},
		{"mutuleGuttaFootFlare", &e.mutuleGuttaFootFlare, 0.5, deduit, "6.4, l. 275"},
	};
	for (const auto &[key, v, written, status, place] : soffit)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(Value(*v), written);
		EXPECT_EQ(v->status, status);
		EXPECT_EQ(v->sources, s);
		EXPECT_EQ(v->readme, section + place);
	}
	// Les dix-huit controles des l. 274 a 276, lus tels qu'ecrits, avec leur unite et leur lieu.
	const MonumentUnit in = MonumentUnit::Inch, m = MonumentUnit::Metre, one = MonumentUnit::One;
	const std::tuple<const char *, const MonumentControlValue *, double, MonumentUnit, const char *> soffitChecks[] = {
		{"mutuleGuttaRows", &c.mutuleGuttaRows, 3.0, one, "6.4, l. 275"},
		{"mutuleGuttaPitchCount", &c.mutuleGuttaPitchCount, 5.0, one, "6.4, l. 275"},
		{"mutuleGuttaMiddleAxis", &c.mutuleGuttaMiddleAxis, 6.635, in, "6.4, l. 275"},
		{"mutuleGuttaRearAxis", &c.mutuleGuttaRearAxis, 12.626, in, "6.4, l. 275"},
		{"mutuleGuttaPitch", &c.mutuleGuttaPitch, 3.44, in, "6.4, l. 275"},
		{"mutuleRearMargin", &c.mutuleRearMargin, 2.5, in, "6.4, l. 274"},
		{"mutuleHorizontalDepth", &c.mutuleHorizontalDepth, 15.404, in, "6.4, l. 274"},
		{"mutuleInclinedBack", &c.mutuleInclinedBack, 20.421, in, "6.4, l. 274"},
		{"soffitPivotZ", &c.soffitPivotZ, 7.377, m, "6.4, l. 276"},
		{"mutuleFrontTopZ", &c.mutuleFrontTopZ, 7.437, m, "6.4, l. 276"},
		{"soffitBackZ", &c.soffitBackZ, 7.475, m, "6.4, l. 276"},
		{"soffitRise", &c.soffitRise, 3.84, in, "6.4, l. 276"},
		{"geisonBackStep", &c.geisonBackStep, 3.9, in, "6.4, l. 276"},
		{"dripTopZ", &c.dripTopZ, 7.469, m, "6.4, l. 276"},
		{"soffitPivotProjection", &c.soffitPivotProjection, 35.825, in, "6.4, l. 276"},
		{"mutuleGuttaRadialFlare", &c.mutuleGuttaRadialFlare, 0.25, in, "6.4, l. 275"},
		{"mutuleGuttaRadialFlareExact", &c.mutuleGuttaRadialFlareExact, 0.249, in, "6.4, l. 275"},
		{"mutuleGuttaFrontFootProjection", &c.mutuleGuttaFrontFootProjection, 35.826, in, "6.4, l. 275"},
	};
	for (const auto &[key, control, written, unit, place] : soffitChecks)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(control->value, written);
		EXPECT_EQ(control->unit, unit);
		EXPECT_EQ(document["entablatureControls"][key]["readme"].get<std::string>(), section + place);
	}
}

// J17
TEST(TEST_cgarchi_classical_monument, the_entablature_controls_of_the_readme_recompute_from_the_values)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablature &e = d.entablature;
	const MonumentEntablatureControls &c = d.entablatureControls;
	const GreekEntablatureSettings s = MonumentEntablatureSettings(d);
	const double architrave = FeetAndInches(e.architraveHeightFt, e.architraveHeightIn);
	const double frieze = FeetAndInches(e.friezeHeightFt, e.friezeHeightIn);
	const double cornice = FeetAndInches(e.corniceHeightFt, e.corniceHeightIn);

	// Sommes ecrites par le README, egales en pouces : a kDecimalSumTolerance.
	// Architrave (l. 209-213) : face lisse, gouttes, regula, tenia = 2 ft 8,9 in.
	EXPECT_NEAR(Length(e.architraveFaceHeight) + Length(e.guttaeHeight) + Length(e.regulaHeight) +
	                Length(e.taeniaHeight),
	            architrave, kDecimalSumTolerance);
	// Corniche (l. 217 et 446) : cinq membres = 1 ft 0,45 in = 12,45 in.
	const double members = Length(e.mutulePlateHeight) + Length(e.dripHeight) + Length(e.coronaHeight) +
	                       Length(e.filletHeight) + Length(e.crownHeight);
	EXPECT_NEAR(members, cornice, kDecimalSumTolerance);
	EXPECT_NEAR(members, Metres(c.corniceSum), kDecimalSumTolerance);
	// Partage du chapiteau du triglyphe (l. 235) = sa hauteur (l. 215).
	EXPECT_NEAR(Length(e.triglyphCapitalBandHeight) + Length(e.triglyphCapitalListelHeight),
	            Length(e.triglyphCapitalHeight), kDecimalSumTolerance);
	// Divisions (l. 253 et 445) = largeur du triglyphe (l. 250) = 20,3 in.
	double divisions = 0.0;
	for (const double v : e.triglyphDivisions.values)
		divisions += MonumentMetres(v, e.triglyphDivisions.unit, "division");
	EXPECT_NEAR(divisions, FeetAndInches(e.triglyphWidthFt, e.triglyphWidthIn), kDecimalSumTolerance);
	EXPECT_NEAR(divisions, Metres(c.triglyphDivisionSum), kDecimalSumTolerance);
	// Ressaut de la metope (l. 236) : face du triglyphe moins nu des metopes = 2,8 in ; pas au bit
	// pres, 19,259 - 16,459 valant 2,8000000000000007 en double.
	EXPECT_NEAR(Length(e.triglyphFace) - Length(e.metopeFace), Metres(c.metopeRecess), kDecimalSumTolerance);

	// Confrontations aux nombres imprimes en metres, a la demi-unite.
	EXPECT_NEAR(architrave + frieze + cornice, Metres(c.total), kPrintedHalfUnit);   // l. 218, 1,979
	const double foot = Length(d.columns.height);                                     // l. 209, 5,715
	EXPECT_NEAR(foot + architrave, 6.551, kPrintedHalfUnit);                          // l. 209
	const double friezeTop = foot + architrave + frieze;
	EXPECT_NEAR(friezeTop, 7.377, kPrintedHalfUnit);                                  // l. 214
	// La somme des nombres en gras donnerait 7,378 ; la l. 214 l'ecarte, et le modele aussi.
	EXPECT_GT(std::fabs(friezeTop - 7.378), kPrintedHalfUnit);
	EXPECT_NEAR(friezeTop + cornice, 7.694, kPrintedHalfUnit);                        // l. 216
	RecordProperty("frieze_top_m", std::to_string(friezeTop));
	RecordProperty("entablature_height_gap_mm", std::to_string(1e3 * (architrave + frieze + cornice - Metres(c.total))));

	// Canaux (l. 254) : profondeur par l'angle droit contre la mesure, a +- 0,1 in ; fond des
	// canaux contre 0,439 m ; les deux fonds devant le nu des metopes.
	const GreekTriglyphSettings &t = s.triglyph;
	EXPECT_NEAR(t.channelDepth, Length(e.channelDepth), kChannelDepthTolerance);
	EXPECT_NEAR(t.face - t.channelDepth, Metres(c.channelBottom), kPrintedHalfUnit);
	const double metopes = Length(e.metopeFace);
	EXPECT_GT(t.face - t.channelDepth, metopes);
	EXPECT_GT(t.face - t.halfChannelDepth, metopes);
	RecordProperty("channel_depth_in", std::to_string(t.channelDepth / kMonumentInch));
	RecordProperty("channel_bottom_ahead_mm", std::to_string(1e3 * (t.face - t.channelDepth - metopes)));
	RecordProperty("half_channel_bottom_ahead_mm", std::to_string(1e3 * (t.face - t.halfChannelDepth - metopes)));

	// L'abaque deborde du dessous de l'anneau, des deux cotes de la ligne des axes (MS-33).
	const double abacus = Length(d.columns.abacusHalfWidth);
	EXPECT_GT(abacus - s.bands[0].projection, 0.0);
	EXPECT_GT(abacus - s.innerFace, 0.0);
	RecordProperty("abacus_overhang_m", std::to_string(abacus - s.bands[0].projection));

	// Gouttes des regulae (l. 212, 277), recalculees depuis les valeurs lues, a kInchHalfUnit (resultats
	// imprimes a trois decimales du pouce) : reste de la largeur de la regula, celle du triglyphe (20,3 in),
	// apres six diametres et cinq vides ; marge de chaque bout, la moitie ; nombre.
	const double guttae = Value(e.regulaGuttaCount);
	const double shortfall = FeetAndInches(e.triglyphWidthFt, e.triglyphWidthIn) -
	                         (guttae * Length(e.regulaGuttaDiameter) + (guttae - 1.0) * Length(e.regulaGuttaGap));
	EXPECT_NEAR(shortfall, Metres(c.regulaGuttaShortfall), kInchHalfUnit);
	EXPECT_NEAR(shortfall / 2.0, Metres(c.regulaGuttaMargin), kInchHalfUnit);
	EXPECT_EQ(guttae, c.regulaGuttaCountCheck.value);
	// Dos des mutules (l. 274) : face de la plaque (l. 237) moins la profondeur, contre le dos imprime.
	const double back = Length(e.mutulePlateProjection) - Length(e.mutuleDepth);
	EXPECT_NEAR(back, Metres(c.mutuleBack), kInchHalfUnit);
	RecordProperty("regula_gutta_shortfall_in", std::to_string(shortfall / kMonumentInch));
	RecordProperty("mutule_back_in", std::to_string(back / kMonumentInch));

	// Gouttes des mutules (l. 275), recalculees depuis les valeurs lues, a kInchHalfUnit : axes des rangs du milieu
	// et de l'arriere depuis les diametres de tete et les vides ; pas en largeur, axes extremes a 1,775 in des bords,
	// par le diviseur lu ; marge arriere des tetes (l. 274) ; un rang par diametre ; gouttes par rang = diviseur + 1.
	const double head0 = Length(e.mutuleGuttaFrontDiameter), head1 = Length(e.mutuleGuttaMiddleDiameter),
	             head2 = Length(e.mutuleGuttaRearDiameter);
	const double gap0 = Length(e.mutuleGuttaFrontGap), gap1 = Length(e.mutuleGuttaRearGap);
	EXPECT_NEAR(head0 + gap0 + 0.5 * head1, Metres(c.mutuleGuttaMiddleAxis), kInchHalfUnit);
	EXPECT_NEAR(head0 + gap0 + head1 + gap1 + 0.5 * head2, Metres(c.mutuleGuttaRearAxis), kInchHalfUnit);
	EXPECT_NEAR((FeetAndInches(e.mutuleWidthFt, e.mutuleWidthIn) - 2.0 * Length(e.mutuleGuttaEdgeAxis)) /
	                c.mutuleGuttaPitchCount.value,
	            Metres(c.mutuleGuttaPitch), kInchHalfUnit);
	EXPECT_NEAR(Length(e.mutuleDepth) - (head0 + gap0 + head1 + gap1 + head2), Metres(c.mutuleRearMargin), kInchHalfUnit);
	ASSERT_TRUE(s.mutules.has_value() && s.mutules->guttae.has_value());
	EXPECT_EQ(static_cast<double>(s.mutules->guttae->rowDiameters.size()), c.mutuleGuttaRows.value);
	EXPECT_EQ(Value(e.mutuleGuttaPerRow), c.mutuleGuttaPitchCount.value + 1.0);
	// Forme (l. 275) : evasement en rayon, la moitie de l'evasement lu en diametre ; sa valeur non arrondie,
	// H tan theta ; avant du pied de la goutte avant, P + (evasement / 2) cos theta - H sin theta, a la demi-unite
	// seulement (expression contractable).
	const double slope = Value(e.soffitSlope) * kPi / 180.0;
	const double flare = 0.5 * Length(e.mutuleGuttaFootFlare), height = Length(e.mutuleGuttaHeight);
	EXPECT_NEAR(flare, Metres(c.mutuleGuttaRadialFlare), kInchHalfUnit);
	EXPECT_NEAR(height * std::tan(slope), Metres(c.mutuleGuttaRadialFlareExact), kInchHalfUnit);
	const double frontFoot = Length(e.mutulePlateProjection) + flare * std::cos(slope) - height * std::sin(slope);
	EXPECT_NEAR(frontFoot, Metres(c.mutuleGuttaFrontFootProjection), kInchHalfUnit);
	RecordProperty("mutule_gutta_rear_margin_in",
	               std::to_string((Length(e.mutuleDepth) - (head0 + gap0 + head1 + gap1 + head2)) / kMonumentInch));
	RecordProperty("mutule_gutta_front_foot_in", std::to_string(frontFoot / kMonumentInch));
}

// J18
TEST(TEST_cgarchi_classical_monument, the_entablature_settings_of_the_readme_are_its_eleven_bands_and_its_triglyph)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablature &e = d.entablature;
	GreekEntablatureSettings s;
	ASSERT_NO_THROW(s = MonumentEntablatureSettings(d));
	ASSERT_EQ(s.bands.size(), 11u);

	// Saillies du pied et du haut de chaque bande, de bas en haut, au bit pres : un seul produit
	// par le pouce, dans la bibliotheque des deux cotes (EN2, EN5, EN8, EN9) ; la bande des mutules a part.
	const std::pair<const MonumentValue *, const MonumentValue *> projections[] = {
		{&e.architraveFootProjection, &e.architraveTopProjection},
		{&e.architraveTopProjection, &e.architraveTopProjection},
		{&e.taeniaProjection, &e.taeniaProjection},
		{&e.metopeFace, &e.metopeFace},
		{&e.triglyphCapitalBandProjection, &e.triglyphCapitalBandProjection},
		{&e.triglyphCapitalListelProjection, &e.triglyphCapitalListelProjection},
		{nullptr, nullptr},
		{&e.dripProjection, &e.dripProjection},
		{&e.coronaProjection, &e.coronaProjection},
		{&e.filletProjection, &e.filletProjection},
		{&e.crownProjection, &e.crownProjection},
	};
	for (std::size_t k = 0; k < s.bands.size(); ++k)
	{
		SCOPED_TRACE(k);
		if (!projections[k].first)
			continue;
		EXPECT_EQ(s.bands[k].projection, Length(*projections[k].first));
		EXPECT_EQ(s.bands[k].topProjection, Length(*projections[k].second));
	}
	// Bande des mutules (EN21, EN22, D3-67) : au dos des mutules sous la sous-face a 14 degres, face moins L cos
	// theta, en pouces ; a 1e-12, jamais au bit pres (expression contractable, cos de la libm).
	const double back = (35.825 - 15.876 * std::cos(14.0 * kPi / 180.0)) * 0.0254;
	EXPECT_NEAR(s.bands[6].projection, back, 1e-12);
	EXPECT_NEAR(s.bands[6].topProjection, back, 1e-12);
	EXPECT_NEAR(s.bands[6].projection, 0.5186828603, 1e-9);   // 20,420585 in
	EXPECT_TRUE(s.regulae.has_value());
	ASSERT_TRUE(s.mutules.has_value());
	// Montee de la sous-face, L sin theta (D3-67) ; gouttes des mutules : diametres de tete, du devant vers le dos,
	// et conicite radiale, la moitie de l'evasement de 0,5 in (D3-76, D3-78).
	EXPECT_NEAR(s.mutules->rise, 0.0975551012, 1e-9);   // 3,840752 in
	ASSERT_TRUE(s.mutules->guttae.has_value());
	const GreekMutuleGuttaSettings &guttae = *s.mutules->guttae;
	ASSERT_EQ(guttae.rowDiameters.size(), 3u);
	EXPECT_NEAR(guttae.rowDiameters[0], 1.46 * 0.0254, 1e-12);
	EXPECT_NEAR(guttae.rowDiameters[1], 1.55 * 0.0254, 1e-12);
	EXPECT_NEAR(guttae.rowDiameters[2], 1.5 * 0.0254, 1e-12);
	EXPECT_NEAR(guttae.conicity, 0.00635, 1e-12);
	// Vides entre tetes, du devant vers le dos ; axe des gouttes extremes depuis le bord ; hauteur ; compte par rang ;
	// cotes (EN23, EN24, format).
	ASSERT_EQ(guttae.rowGaps.size(), 2u);
	EXPECT_NEAR(guttae.rowGaps[0], 4.4 * 0.0254, 1e-12);
	EXPECT_NEAR(guttae.rowGaps[1], 4.466 * 0.0254, 1e-12);
	EXPECT_NEAR(guttae.edgeAxis, 1.775 * 0.0254, 1e-12);
	EXPECT_NEAR(guttae.height, 1.0 * 0.0254, 1e-12);
	EXPECT_EQ(guttae.perRow, 6);
	EXPECT_EQ(guttae.sides, 8);
	// Hauteurs d'un seul membre, au bit pres.
	const std::pair<std::size_t, const MonumentValue *> single[] = {
		{0, &e.architraveFaceHeight}, {2, &e.taeniaHeight},  {4, &e.triglyphCapitalBandHeight},
		{5, &e.triglyphCapitalListelHeight}, {6, &e.mutulePlateHeight}, {7, &e.dripHeight},
		{8, &e.coronaHeight}, {9, &e.filletHeight}, {10, &e.crownHeight},
	};
	for (const auto &[k, v] : single)
		EXPECT_EQ(s.bands[k].height, Length(*v)) << k;
	// Zone des regulae et des gouttes, corps de la frise, total : sommes et difference, a 1e-12 m
	// (une contraction FMA peut fusionner un produit avec l'addition, ici ou dans la bibliotheque).
	EXPECT_NEAR(s.bands[1].height, Length(e.regulaHeight) + Length(e.guttaeHeight), 1e-12);
	EXPECT_NEAR(s.bands[3].height,
	            FeetAndInches(e.friezeHeightFt, e.friezeHeightIn) - Length(e.triglyphCapitalHeight), 1e-12);
	double total = 0.0;
	for (const EntablatureBand &band : s.bands)
		total += band.height;
	EXPECT_NEAR(total,
	            FeetAndInches(e.architraveHeightFt, e.architraveHeightIn) +
	                FeetAndInches(e.friezeHeightFt, e.friezeHeightIn) +
	                FeetAndInches(e.corniceHeightFt, e.corniceHeightIn),
	            1e-12);
	EXPECT_EQ(s.friezeBand, 3);
	EXPECT_LT(s.bands[0].topProjection, s.bands[0].projection);   // face lisse inclinee vers l'axe (l. 229)
	EXPECT_EQ(s.innerFace, Length(e.architraveFootProjection));   // EN1

	// Triglyphe : largeur en pieds et pouces, face, divisions lues, profondeurs par l'angle droit (EN11).
	const GreekTriglyphSettings &t = s.triglyph;
	const MonumentDivisions &dv = e.triglyphDivisions;
	EXPECT_EQ(t.width, FeetAndInches(e.triglyphWidthFt, e.triglyphWidthIn));
	EXPECT_EQ(t.face, Length(e.triglyphFace));
	ASSERT_EQ(t.divisionWidths.size(), 7u);
	for (std::size_t i = 0; i < t.divisionWidths.size(); ++i)
		EXPECT_EQ(t.divisionWidths[i], MonumentMetres(dv.values[i], dv.unit, "division")) << i;
	EXPECT_EQ(t.divisionIsChannel, (std::vector<bool>{true, false, true, false, true, false, true}));
	EXPECT_EQ(t.channelDepth, t.divisionWidths[2] / 2.0);
	EXPECT_EQ(t.halfChannelDepth, t.divisionWidths[0]);

	EXPECT_NO_THROW(ValidateGreekEntablatureSettings(s));
	std::vector<RingSectionPoint> section;
	ASSERT_NO_THROW(section = GreekEntablatureSection(s));
	// 2 x 11 + 2 sommets, moins la jonction a saillies egales du haut de l'architrave (face
	// inclinee sous une face verticale : sommet garde), plus un : le haut de la bande des mutules
	// remplace par le dos du fond des vias et le pivot (EN22).
	EXPECT_EQ(section.size(), 24u);
	RecordProperty("section_vertices", std::to_string(section.size()));
}

// J19
TEST(TEST_cgarchi_classical_monument, the_temple_of_the_readme_has_a_corner_triglyph_at_each_corner)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablature &e = d.entablature;
	const MonumentEntablatureControls &c = d.entablatureControls;
	const TempleSettings t = MonumentTempleSettings(d);
	ASSERT_TRUE(t.entablature.has_value());
	const GreekEntablatureSettings &s = *t.entablature;
	EXPECT_EQ(s.corner, DoricCornerStrategy::CornerTriglyphEqualMetopes);   // EN15
	TempleLayout layout;
	ASSERT_NO_THROW(layout = LayoutTemple(t));
	ASSERT_TRUE(layout.frieze.has_value());
	const FriezeLayout &f = *layout.frieze;
	const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};

	// 68 triglyphes (l. 249), dont 4 pieces d'angle en L (EN14) ; par cote de n colonnes, 2n - 2
	// possedes, et 2n - 1 visibles en comptant le bras de retour de la piece d'angle du cote suivant.
	EXPECT_EQ(f.triglyphs.size(), 68u);
	std::size_t cornerPieces = 0, metopeCount = 0;
	double frontCurrent = 0.0, flankCurrent = 0.0;
	for (const PeristyleSide side : sides)
	{
		SCOPED_TRACE(static_cast<int>(side));
		const bool front = side == PeristyleSide::Front || side == PeristyleSide::Back;
		const std::size_t n = static_cast<std::size_t>(front ? layout.peristyle.front : layout.peristyle.flank);
		std::vector<FriezePiece> triglyphs, metopes;
		for (const FriezePiece &q : f.triglyphs)
			if (q.side == side)
				triglyphs.push_back(q);
		for (const FriezePiece &q : f.metopes)
			if (q.side == side)
				metopes.push_back(q);
		ASSERT_EQ(triglyphs.size(), 2 * n - 2);
		EXPECT_EQ(static_cast<double>(triglyphs.size() + 1), (front ? c.frontTriglyphs : c.flankTriglyphs).value);
		for (const FriezePiece &q : triglyphs)
		{
			EXPECT_EQ(q.kind, q.rank == 0 ? FriezeElement::CornerTriglyph : FriezeElement::Triglyph) << q.rank;
			cornerPieces += q.kind == FriezeElement::CornerTriglyph ? 1u : 0u;
		}
		// 2n - 2 metopes, aucune demi-metope d'angle ; travees courantes (rangs 2 a 2n - 5 apres tri)
		// a la metope courante imprimee, a la demi-unite.
		ASSERT_EQ(metopes.size(), 2 * n - 2);
		metopeCount += metopes.size();
		std::sort(metopes.begin(), metopes.end(), [](const FriezePiece &x, const FriezePiece &y) { return x.a0 < y.a0; });
		for (std::size_t k = 2; k + 2 < metopes.size(); ++k)
		{
			const double width = metopes[k].a1 - metopes[k].a0;
			EXPECT_NEAR(width, Metres(front ? c.frontMetope : c.flankMetope), kPrintedHalfUnit) << k;
			(front ? frontCurrent : flankCurrent) = width;
		}
		// Travees d'angle aux deux bouts du cote (rangs 0 et 1, 2n - 4 et 2n - 3 apres tri), celle de
		// l'angle qui ouvre le cote comme celle de l'angle qui le ferme : deux metopes egales a 1e-12,
		// a la metope d'angle imprimee a la demi-unite (EN15).
		for (const std::size_t k : {std::size_t{0}, metopes.size() - 2})
		{
			SCOPED_TRACE(k);
			const double first = metopes[k].a1 - metopes[k].a0, second = metopes[k + 1].a1 - metopes[k + 1].a0;
			EXPECT_NEAR(first, second, 1e-12);
			EXPECT_NEAR(first, Metres(front ? c.frontCornerMetope : c.flankCornerMetope), kPrintedHalfUnit);
			EXPECT_NEAR(second, Metres(front ? c.frontCornerMetope : c.flankCornerMetope), kPrintedHalfUnit);
		}
	}
	EXPECT_EQ(cornerPieces, 4u);
	EXPECT_EQ(static_cast<double>(metopeCount), c.metopeCount.value);
	// Metope lue (l. 251) : a MO1 de la metope courante de flanc, hors de MO1 en facade (l. 252).
	const double readMetope = FeetAndInches(e.metopeWidthFt, e.metopeWidthIn);
	EXPECT_LE(std::fabs(flankCurrent - readMetope), kReadMetopeTolerance);
	EXPECT_GT(std::fabs(frontCurrent - readMetope), kReadMetopeTolerance);
	RecordProperty("flank_read_metope_gap_mm", std::to_string(1e3 * (flankCurrent - readMetope)));
	RecordProperty("front_read_metope_gap_mm", std::to_string(1e3 * (frontCurrent - readMetope)));

	// Faces : l'anneau (8 triangles par sommet de section, 24 sommets), 64 prismes droits de 76 et
	// 4 pieces d'angle de 80 (4P - 4, P = 21), et les ornements (96 992, J21).
	TempleSettings bare = t;
	bare.entablature.reset();
	std::size_t faces = 0, bareFaces = 0;
	ASSERT_NO_THROW(faces = CountTempleMeshFaces(t));
	ASSERT_NO_THROW(bareFaces = CountTempleMeshFaces(bare));
	EXPECT_EQ(faces - bareFaces, 8u * 24u + 64u * 76u + 4u * 80u + 96992u);
	RecordProperty("temple_faces", std::to_string(faces));
	RecordProperty("bare_temple_faces", std::to_string(bareFaces));

	// Pieces seules valides : l'anneau, chaque piece d'angle, un prisme droit par cote.
	const std::unique_ptr<Mesh> ring = SweepRectangularRing(GreekEntablatureSection(s), layout.peristyle.halfWidth,
	                                                        layout.peristyle.halfDepth, layout.entablatureFoot);
	EXPECT_TRUE(ValidateMesh(*ring).IsValid());
	EXPECT_EQ(static_cast<std::size_t>(ring->GetNFaces()), 8u * 24u);
	std::vector<std::unique_ptr<Mesh>> cornerMeshes;
	for (const FriezePiece &q : f.triglyphs)
	{
		if (q.rank > 1)
			continue;
		SCOPED_TRACE(static_cast<int>(q.side));
		SCOPED_TRACE(q.rank);
		std::unique_ptr<Mesh> prism = BuildTriglyphMesh(s, layout.peristyle, q, layout.entablatureFoot);
		EXPECT_TRUE(ValidateMesh(*prism).IsValid());
		EXPECT_EQ(static_cast<std::size_t>(prism->GetNFaces()), q.rank == 0 ? 80u : 76u);
		if (q.rank == 0)
			cornerMeshes.push_back(std::move(prism));
	}
	ASSERT_EQ(cornerMeshes.size(), 4u);

	// Contact L7 (registre TE5) sur le README : l'anneau et les quatre pieces d'angle, reunis, ont
	// exactement quatre aretes non manifold, chacune aux deux cotes de la frise d'un angle, a
	// (+-float (H + b), +-float (H' + b)) ; une composante au lieu de cinq ; aucun autre defaut que
	// ces aretes et les contacts (auto-intersections L4).
	std::vector<float> vertices = ring->GetVertices();
	std::vector<unsigned> triangles;
	for (unsigned face = 0; face < ring->GetNFaces(); ++face)
		for (unsigned j = 0; j < 3; ++j)
			triangles.push_back(static_cast<unsigned>(ring->GetFaceVertex(face, j)));
	for (const std::unique_ptr<Mesh> &m : cornerMeshes)
	{
		const unsigned offset = static_cast<unsigned>(vertices.size() / 3);
		vertices.insert(vertices.end(), m->GetVertices().begin(), m->GetVertices().end());
		for (unsigned face = 0; face < m->GetNFaces(); ++face)
			for (unsigned j = 0; j < 3; ++j)
				triangles.push_back(offset + static_cast<unsigned>(m->GetFaceVertex(face, j)));
	}
	Mesh assembly;
	assembly.SetVertices(static_cast<unsigned>(vertices.size() / 3), vertices.data());
	assembly.SetFaces(static_cast<unsigned>(triangles.size() / 3), 3, triangles.data());
	MeshValidationOptions options;
	options.maxRecordsPerKind = assembly.GetNFaces();
	const MeshValidationReport r = ValidateMesh(assembly, options);
	ASSERT_TRUE(r.selfIntersectionChecked);
	ASSERT_FALSE(r.selfIntersectionTruncated);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect>(kind) != MeshDefect::SelfIntersection &&
		    static_cast<MeshDefect>(kind) != MeshDefect::NonManifoldEdge)
			EXPECT_EQ(r.counts[kind], 0u) << MeshDefectName(static_cast<MeshDefect>(kind));
	EXPECT_EQ(r.Count(MeshDefect::NonManifoldEdge), 4u);
	EXPECT_EQ(r.components, 1u);
	const double b = s.bands[static_cast<std::size_t>(s.friezeBand)].projection;
	const float xCorner = static_cast<float>(layout.peristyle.halfWidth + b);
	const float yCorner = static_cast<float>(layout.peristyle.halfDepth + b);
	std::set<std::pair<bool, bool>> quadrants;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::NonManifoldEdge)
			continue;
		EXPECT_EQ(rec.faces.size(), 4u);
		ASSERT_EQ(rec.vertices.size(), 2u);
		for (const unsigned v : rec.vertices)
		{
			float q[3];
			assembly.GetVertex(v, q);
			EXPECT_EQ(std::fabs(q[0]), xCorner);
			EXPECT_EQ(std::fabs(q[1]), yCorner);
			quadrants.insert({q[0] > 0.0f, q[1] > 0.0f});
		}
	}
	EXPECT_EQ(quadrants.size(), 4u);
	RecordProperty("l7_non_manifold_edges", std::to_string(r.Count(MeshDefect::NonManifoldEdge)));

	// Regression EN4 : les memes reglages sous AxisTriglyph rendent 72 prismes et 8 demi-metopes
	// d'angle de b - w / 2 = 0,1602486 m.
	GreekEntablatureSettings axis = s;
	axis.corner = DoricCornerStrategy::AxisTriglyph;
	const FriezeLayout fa = LayoutGreekFrieze(axis, layout.peristyle);
	EXPECT_EQ(fa.triglyphs.size(), 72u);
	EXPECT_EQ(fa.metopes.size(), 76u);
	std::size_t halfMetopes = 0;
	for (const FriezePiece &q : fa.metopes)
		if (std::fabs(q.a1 - q.a0 - 0.1602486) <= 1e-12)
			++halfMetopes;
	EXPECT_EQ(halfMetopes, 8u);
}

// J20
TEST(TEST_cgarchi_classical_monument, the_corner_of_the_readme_meets_its_controls_and_reports_its_contraction)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablatureControls &c = d.entablatureControls;
	const GreekEntablatureSettings s = MonumentEntablatureSettings(d);
	EXPECT_EQ(s.corner, DoricCornerStrategy::CornerTriglyphEqualMetopes);   // EN15
	const PeristyleLayout peristyle = LayoutPeristyle(MonumentPeristyleSettings(d));

	// La contraction relevee pilote (D3-22) : le peristyle du temple est celui du document, au bit
	// pres, entraxes d'angle compris.
	const TempleSettings t = MonumentTempleSettings(d);
	const PeristyleSettings p = MonumentPeristyleSettings(d);
	EXPECT_EQ(t.peristyle.front, p.front);
	EXPECT_EQ(t.peristyle.flank, p.flank);
	EXPECT_EQ(t.peristyle.frontSpacing, p.frontSpacing);
	EXPECT_EQ(t.peristyle.flankSpacing, p.flankSpacing);
	EXPECT_EQ(t.peristyle.frontCornerSpacing, p.frontCornerSpacing);
	EXPECT_EQ(t.peristyle.flankCornerSpacing, p.flankCornerSpacing);
	EXPECT_EQ(p.frontCornerSpacing, Length(d.peristyle.frontCornerSpacing));
	EXPECT_EQ(p.flankCornerSpacing, Length(d.peristyle.flankCornerSpacing));

	// Premiere solution (l. 255-256), a la demi-unite des nombres imprimes.
	const double offset = CornerTriglyphOffset(s);
	EXPECT_NEAR(offset, Metres(c.cornerTriglyphOffset), kPrintedHalfUnit);
	EXPECT_NEAR(offset, 0.2313686, 1e-12);
	const std::array<DoricCornerReport, 4> reports = ReportDoricCorners(s, peristyle);
	for (const DoricCornerReport &r : reports)
	{
		SCOPED_TRACE(static_cast<int>(r.side));
		const bool front = r.side == PeristyleSide::Front || r.side == PeristyleSide::Back;
		EXPECT_NEAR(r.cornerTriglyphOffset, offset, 1e-12);
		if (front)
			EXPECT_NEAR(r.cornerSpacing + r.cornerTriglyphOffset, Metres(c.frontCornerDistance), kPrintedHalfUnit);
		// Deux metopes egales dans la travee d'angle, a 1e-12 (pas au bit pres : 3,55e-15 en flanc).
		// 0,801 est l'arrondi de la valeur non arrondie (0,80056), pas le quotient des operandes
		// imprimes ((2,632 - 2 x 0,516) / 2 = 0,800) : marge de 0,064 mm a la demi-unite.
		EXPECT_NEAR(r.innerMetope, r.outerMetope, 1e-12);
		EXPECT_NEAR(r.outerMetope, Metres(front ? c.frontCornerMetope : c.flankCornerMetope), kPrintedHalfUnit);
		EXPECT_NEAR(r.outerMetope, front ? 0.8005643 : 0.8020643, 1e-12);
		EXPECT_NEAR(r.intermediateShift, Metres(c.intermediateShift), kPrintedHalfUnit);
		// Les valeurs de la seconde solution ne tiennent pas sous la premiere.
		EXPECT_GT(std::fabs(r.innerMetope - Metres(front ? c.frontMidBayInnerMetope : c.flankMidBayInnerMetope)),
		          kPrintedHalfUnit);

		// Contraction simple calculee (EN16), rapportee contre la relevee : a* = s - c, ecart a - a*.
		ASSERT_TRUE(r.computedCornerSpacing.has_value());
		ASSERT_TRUE(r.cornerSpacingGap.has_value());
		EXPECT_NEAR(*r.computedCornerSpacing, front ? 2.3566314 : 2.3496314, 1e-12);
		EXPECT_NEAR(*r.cornerSpacingGap, front ? 0.0443686 : 0.0543686, 1e-12);
	}
	RecordProperty("front_contraction_gap_mm", std::to_string(1e3 * *reports[0].cornerSpacingGap));
	RecordProperty("flank_contraction_gap_mm", std::to_string(1e3 * *reports[1].cornerSpacingGap));
	RecordProperty("front_corner_metope_m", std::to_string(reports[0].outerMetope));
	RecordProperty("flank_corner_metope_m", std::to_string(reports[1].outerMetope));

	// Seconde solution (intermediaire au milieu de la travee) : 0,685 / 0,916 et 0,686 / 0,918 ;
	// la premiere n'y tient plus.
	GreekEntablatureSettings midBay = s;
	midBay.corner = DoricCornerStrategy::CornerTriglyphMidBay;
	for (const DoricCornerReport &r : ReportDoricCorners(midBay, peristyle))
	{
		SCOPED_TRACE(static_cast<int>(r.side));
		const bool front = r.side == PeristyleSide::Front || r.side == PeristyleSide::Back;
		EXPECT_NEAR(r.innerMetope, Metres(front ? c.frontMidBayInnerMetope : c.flankMidBayInnerMetope),
		            kPrintedHalfUnit);
		EXPECT_NEAR(r.outerMetope, Metres(front ? c.frontMidBayOuterMetope : c.flankMidBayOuterMetope),
		            kPrintedHalfUnit);
		EXPECT_NEAR(r.innerMetope, front ? 0.68488 : 0.68638, 1e-12);
		EXPECT_NEAR(r.outerMetope, front ? 0.9162486 : 0.9177486, 1e-12);
		EXPECT_NEAR(r.intermediateShift, 0.0, 1e-12);
		EXPECT_GT(std::fabs(r.outerMetope - Metres(front ? c.frontCornerMetope : c.flankCornerMetope)),
		          kPrintedHalfUnit);
		EXPECT_FALSE(r.computedCornerSpacing.has_value());
	}
}

// J21
TEST(TEST_cgarchi_classical_monument, the_temple_of_the_readme_has_regulae_guttae_and_mutules)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablature &e = d.entablature;
	const MonumentEntablatureControls &c = d.entablatureControls;
	const TempleSettings t = MonumentTempleSettings(d);
	ASSERT_TRUE(t.entablature.has_value());
	const GreekEntablatureSettings &s = *t.entablature;
	ASSERT_TRUE(s.regulae.has_value());
	ASSERT_TRUE(s.mutules.has_value());
	TempleLayout layout;
	ASSERT_NO_THROW(layout = LayoutTemple(t));
	ASSERT_TRUE(layout.frieze.has_value());
	ASSERT_TRUE(layout.ornaments.has_value());
	const OrnamentLayout &o = *layout.ornaments;
	const PeristyleLayout &p = layout.peristyle;
	const double hx = p.halfWidth, hy = p.halfDepth, foot = layout.entablatureFoot;
	const std::vector<double> z = GreekEntablatureBandBounds(s);
	const std::size_t fb = static_cast<std::size_t>(s.friezeBand), mb = static_cast<std::size_t>(s.mutules->band);
	const PeristyleSide sides[] = {PeristyleSide::Front, PeristyleSide::Right, PeristyleSide::Back, PeristyleSide::Left};
	auto alongX = [](PeristyleSide side) { return side == PeristyleSide::Front || side == PeristyleSide::Back; };

	// Une regula par placement de triglyphe (l. 212), dont 4 d'angle ; 6 gouttes par regula droite et par
	// bras d'une regula d'angle (l. 212, 277) ; un mutule par triglyphe visible et par metope (l. 273),
	// 4n - 3 par cote de n colonnes, aucun a l'angle (EN21).
	ASSERT_EQ(o.regulae.size(), 68u);
	std::size_t cornerRegulae = 0;
	for (const OrnamentPiece &r : o.regulae)
	{
		const bool corner = r.kind == OrnamentElement::CornerRegula;
		cornerRegulae += corner ? 1u : 0u;
		EXPECT_EQ(r.guttaCount, corner ? 12 : 6);
	}
	EXPECT_EQ(cornerRegulae, 4u);
	EXPECT_EQ(o.guttae.size(), 432u);
	EXPECT_EQ(o.mutules.size(), 140u);

	// Regula (l. 211, 229, 231) : du nu du haut de l'architrave (19,259 in) a la face lue (20,859 in),
	// sous la tenia, de la hauteur lue.
	for (const OrnamentPiece &r : o.regulae)
	{
		EXPECT_EQ(r.d0, Length(e.architraveTopProjection));
		EXPECT_EQ(r.d1, Length(e.regulaProjection));
		EXPECT_EQ(r.z1, z[fb - 1]);
		EXPECT_NEAR(r.z1 - r.z0, Length(e.regulaHeight), 1e-12);
	}
	EXPECT_NEAR(o.regulae.front().d0, 0.4891786, 1e-12);
	EXPECT_NEAR(o.regulae.front().d1, 0.5298186, 1e-12);

	// Gouttes : avant du pied au nu de la regula, 20,859 in (l. 231, 232) ; avant de la tete contre 20,809 in
	// (l. 232), a la demi-unite ; marge de chaque bout d'une regula droite contre 0,127 in (l. 212).
	const GreekGuttaSettings &g = s.regulae->gutta;
	const double headDiameter = g.footDiameter - 2.0 * (g.footProjection - g.topProjection);
	for (const GuttaPlacement &q : o.guttae)
	{
		EXPECT_NEAR(q.axis + 0.5 * g.footDiameter, 0.5298186, 1e-12);
		EXPECT_NEAR(q.axis + 0.5 * headDiameter, 20.809 * kMonumentInch, kInchHalfUnit);
	}
	double margin = 0.0;
	for (const OrnamentPiece &r : o.regulae)
	{
		if (r.kind != OrnamentElement::Regula)
			continue;
		const GuttaPlacement &first = o.guttae[static_cast<std::size_t>(r.firstGutta)];
		const GuttaPlacement &last = o.guttae[static_cast<std::size_t>(r.firstGutta + r.guttaCount - 1)];
		margin = (first.a - 0.5 * g.footDiameter) - r.a0;
		EXPECT_NEAR(margin, Metres(c.regulaGuttaMargin), kInchHalfUnit);
		EXPECT_NEAR(r.a1 - (last.a + 0.5 * g.footDiameter), Metres(c.regulaGuttaMargin), kInchHalfUnit);
	}
	RecordProperty("regula_gutta_margin_in", std::to_string(margin / kMonumentInch));

	// Mutules (l. 237, 273, 274, 276) : dos a 20,420585 in sous la sous-face a 14 degres (D3-67), face de la
	// plaque, dans la bande 6, montee de la sous-face ; aucune pose sur le listel du chapiteau du triglyphe (L14
	// absente) : l'anneau monte du listel au dos par un ressaut ; vias courantes n/4 - largeur, sans confrontation
	// a la via lue (B15) ; degagement d'angle contre le plan des dos du cote voisin.
	const double width = s.mutules->width, back = s.bands[mb].projection;
	EXPECT_EQ(width, FeetAndInches(e.mutuleWidthFt, e.mutuleWidthIn));
	EXPECT_NEAR(back, 0.5186828603, 1e-9);
	for (const OrnamentPiece &m : o.mutules)
	{
		EXPECT_EQ(m.d0, back);
		EXPECT_EQ(m.d1, Length(e.mutulePlateProjection));
		EXPECT_EQ(m.z0, z[mb]);
		EXPECT_EQ(m.z1, z[mb + 1]);
		EXPECT_EQ(m.rise, s.mutules->rise);
	}
	const double listelStep = s.bands[mb - 1].projection - back;
	EXPECT_NEAR(listelStep, 0.0060557397, 1e-9);
	RecordProperty("listel_step_mm", std::to_string(1e3 * listelStep));
	double cornerVia = 1.0;
	for (const PeristyleSide side : sides)
	{
		SCOPED_TRACE(static_cast<int>(side));
		const bool front = alongX(side);
		const double along = front ? hx : hy;
		const double spacing = front ? t.peristyle.frontSpacing : t.peristyle.flankSpacing;
		const double cornerSpacing = front ? t.peristyle.frontCornerSpacing : t.peristyle.flankCornerSpacing;
		std::vector<const OrnamentPiece *> row;
		for (const OrnamentPiece &m : o.mutules)
			if (m.side == side)
				row.push_back(&m);
		ASSERT_EQ(row.size(), static_cast<std::size_t>(4 * (front ? p.front : p.flank) - 3));
		std::sort(row.begin(), row.end(), [](const OrnamentPiece *x, const OrnamentPiece *y) { return x->a0 < y->a0; });
		// Travees courantes : les deux centres entre les axes des deuxiemes colonnes, quatre vias par
		// entrecolonnement courant, (n - 3) x 4 par cote.
		const double inner = along - cornerSpacing;
		std::size_t currentVias = 0;
		for (std::size_t k = 0; k + 1 < row.size(); ++k)
		{
			const double via = row[k + 1]->a0 - row[k]->a1;
			const double c0 = 0.5 * (row[k]->a0 + row[k]->a1), c1 = 0.5 * (row[k + 1]->a0 + row[k + 1]->a1);
			EXPECT_GT(via, 0.0) << k;
			if (c0 >= -inner - 1e-9 && c1 <= inner + 1e-9)
			{
				EXPECT_NEAR(via, 0.25 * spacing - width, 1e-12) << k;
				++currentVias;
			}
			else
				cornerVia = std::min(cornerVia, via);
		}
		EXPECT_EQ(currentVias, front ? 12u : 40u);
		RecordProperty(front ? "front_mutule_via_mm" : "flank_mutule_via_mm", std::to_string(1e3 * (0.25 * spacing - width)));
		EXPECT_NEAR(row.front()->a0 + along + back, 0.0237892603, 1e-9);
		EXPECT_NEAR(along + back - row.back()->a1, 0.0237892603, 1e-9);
	}
	EXPECT_GT(cornerVia, 0.0);
	RecordProperty("corner_mutule_via_mm", std::to_string(1e3 * cornerVia));

	// Faces : 64 regulae droites de 12 et 4 d'angle de 20, 432 gouttes de 32, 140 mutules de 12 portant chacun
	// 18 gouttes de 32 ; sans mutules, l'anneau perd le sommet de la sous-face inclinee, soit 8 faces.
	TempleSettings plain = t;
	plain.entablature->regulae.reset();
	plain.entablature->mutules.reset();
	std::size_t faces = 0, plainFaces = 0;
	ASSERT_NO_THROW(faces = CountTempleMeshFaces(t));
	ASSERT_NO_THROW(plainFaces = CountTempleMeshFaces(plain));
	EXPECT_EQ(faces - plainFaces - 8u, 64u * 12u + 4u * 20u + 432u * 32u + 140u * (12u + 18u * 32u));
	RecordProperty("ornament_faces", std::to_string(faces - plainFaces - 8u));

	// L'anneau et les 208 pieces d'ornements, reunis, valides a 1e-4 m (D3-80) : seuls recouvrements L8 a L12 (L11
	// sous la face inclinee de l'anneau, memes couples (d, z)) et L15 (tete d'une goutte de mutule sur le dessous de
	// son mutule), registre TE5 ; aucun L14 ; une arete non manifold L13 par regula d'angle ; aucun sommet non
	// manifold. Garde de D3-80 : a la tolerance par defaut et a 1e-4, memes sommets soudes, composantes, defauts hors
	// SelfIntersection et paires d'unites en contact (une unite : l'anneau, une piece, ou une goutte).
	std::vector<std::unique_ptr<Mesh>> pieces;
	ASSERT_NO_THROW(pieces = BuildEntablaturePieceMeshes(s, p, *layout.frieze, o, foot));
	const std::size_t nt = layout.frieze->triglyphs.size(), nr = o.regulae.size(), nm = o.mutules.size();
	const std::size_t ng = o.guttae.size(), nmg = o.mutuleGuttae.size();
	ASSERT_EQ(pieces.size(), nt + nr + nm);
	EXPECT_EQ(nmg, 18u * nm);
	const std::unique_ptr<Mesh> ring = SweepRectangularRing(GreekEntablatureSection(s), hx, hy, foot);
	std::vector<float> vertices = ring->GetVertices();
	std::vector<unsigned> triangles;
	std::vector<std::size_t> owner(ring->GetNFaces(), 0);   // 0 : anneau ; 1 + k : regula k ; 1 + nr + k : mutule k
	std::vector<std::size_t> unit(ring->GetNFaces(), 0);    // owner, ou 1 + nr + nm + goutte, ou 1 + nr + nm + ng + goutte de mutule
	std::vector<unsigned> firstFace = {0};
	for (unsigned face = 0; face < ring->GetNFaces(); ++face)
		for (unsigned j = 0; j < 3; ++j)
			triangles.push_back(static_cast<unsigned>(ring->GetFaceVertex(face, j)));
	for (std::size_t k = nt; k < pieces.size(); ++k)
	{
		const Mesh &m = *pieces[k];
		const std::size_t piece = k - nt + 1;
		const unsigned offset = static_cast<unsigned>(vertices.size() / 3);
		firstFace.push_back(static_cast<unsigned>(owner.size()));
		vertices.insert(vertices.end(), m.GetVertices().begin(), m.GetVertices().end());
		for (unsigned face = 0; face < m.GetNFaces(); ++face)
		{
			owner.push_back(piece);
			std::size_t u = piece;
			if (piece <= nr)
			{
				const OrnamentPiece &reg = o.regulae[piece - 1];
				const unsigned body = reg.kind == OrnamentElement::CornerRegula ? 20u : 12u;
				if (face >= body)
					u = 1 + nr + nm + static_cast<std::size_t>(reg.firstGutta) + (face - body) / 32u;
			}
			else if (face >= 12u)
				u = 1 + nr + nm + ng + static_cast<std::size_t>(o.mutules[piece - 1 - nr].firstGutta) + (face - 12u) / 32u;
			unit.push_back(u);
			for (unsigned j = 0; j < 3; ++j)
				triangles.push_back(offset + static_cast<unsigned>(m.GetFaceVertex(face, j)));
		}
	}
	Mesh assembly;
	assembly.SetVertices(static_cast<unsigned>(vertices.size() / 3), vertices.data());
	assembly.SetFaces(static_cast<unsigned>(triangles.size() / 3), 3, triangles.data());
	MeshValidationOptions byDefault, options;
	byDefault.maxRecordsPerKind = assembly.GetNFaces();
	options.maxRecordsPerKind = assembly.GetNFaces();
	options.weldTolerance = 1e-4;
	const MeshValidationReport r0 = ValidateMesh(assembly, byDefault), r = ValidateMesh(assembly, options);
	ASSERT_TRUE(r0.selfIntersectionChecked && r.selfIntersectionChecked);
	ASSERT_FALSE(r0.selfIntersectionTruncated || r.selfIntersectionTruncated);
	RecordProperty("default_weld_tolerance", std::to_string(r0.weldTolerance));

	// Garde de D3-80.
	auto contacts = [&](const MeshValidationReport &report) {
		std::set<std::pair<std::size_t, std::size_t>> pairs;
		for (const MeshDefectRecord &rec : report.records)
			if (rec.kind == MeshDefect::SelfIntersection)
				pairs.insert({std::min(unit[rec.faces[0]], unit[rec.faces[1]]), std::max(unit[rec.faces[0]], unit[rec.faces[1]])});
		return pairs;
	};
	EXPECT_EQ(r0.weldedVertices, r.weldedVertices);
	EXPECT_EQ(r0.components, r.components);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect>(kind) != MeshDefect::SelfIntersection)
			EXPECT_EQ(r0.counts[kind], r.counts[kind]) << MeshDefectName(static_cast<MeshDefect>(kind));
	const std::set<std::pair<std::size_t, std::size_t>> pairs0 = contacts(r0), pairs = contacts(r);
	EXPECT_TRUE(pairs0 == pairs) << "paires d'unites en contact : " << pairs0.size() << " a la tolerance par defaut, "
	                             << pairs.size() << " a 1e-4";
	RecordProperty("contact_pairs", std::to_string(pairs.size()));

	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect>(kind) != MeshDefect::SelfIntersection &&
		    static_cast<MeshDefect>(kind) != MeshDefect::NonManifoldEdge)
			EXPECT_EQ(r.counts[kind], 0u) << MeshDefectName(static_cast<MeshDefect>(kind));
	EXPECT_EQ(r.Count(MeshDefect::NonManifoldEdge), 4u);
	EXPECT_EQ(r.components, 1u + nr + nm + ng + nmg - 4u);
	RecordProperty("components", std::to_string(r.components));

	const float zTaenia = static_cast<float>(foot + z[fb - 1]);
	const double dos = s.bands[fb - 2].projection;
	auto horizontal = [&](unsigned face, float zz) {
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			assembly.GetVertex(static_cast<unsigned>(assembly.GetFaceVertex(face, j)), q);
			if (q[2] != zz)
				return false;
		}
		return true;
	};
	auto perpendicular = [&](PeristyleSide side, double dd) {
		return static_cast<float>(side == PeristyleSide::Front   ? -(hy + dd)
		                          : side == PeristyleSide::Back  ? hy + dd
		                          : side == PeristyleSide::Right ? hx + dd
		                                                         : -(hx + dd));
	};
	auto onPlane = [&](unsigned face, PeristyleSide side, double dd) {
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			assembly.GetVertex(static_cast<unsigned>(assembly.GetFaceVertex(face, j)), q);
			if ((alongX(side) ? q[1] : q[0]) != perpendicular(side, dd))
				return false;
		}
		return true;
	};
	// Chaque sommet de la face sur l'un des couples (d, z) du cote, au bit pres.
	auto onCouples = [&](unsigned face, PeristyleSide side, double d0, double z0, double d1, double z1) {
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			assembly.GetVertex(static_cast<unsigned>(assembly.GetFaceVertex(face, j)), q);
			const float across = alongX(side) ? q[1] : q[0];
			if (!((across == perpendicular(side, d0) && q[2] == static_cast<float>(foot + z0)) ||
			      (across == perpendicular(side, d1) && q[2] == static_cast<float>(foot + z1))))
				return false;
		}
		return true;
	};
	auto previousOf = [](PeristyleSide side) { return static_cast<PeristyleSide>((static_cast<int>(side) + 3) % 4); };

	// L13 : chaque arete non manifold portee par 2 faces de l'anneau et 2 du dessus d'une regula d'angle.
	std::set<std::size_t> l13;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::NonManifoldEdge)
			continue;
		ASSERT_EQ(rec.faces.size(), 4u);
		std::size_t onRing = 0, piece = 0;
		for (const unsigned face : rec.faces)
		{
			if (owner[face] == 0)
				++onRing;
			else
			{
				piece = owner[face];
				EXPECT_TRUE(horizontal(face, zTaenia));
			}
		}
		EXPECT_EQ(onRing, 2u);
		ASSERT_TRUE(piece >= 1 && piece <= nr);
		EXPECT_EQ(o.regulae[piece - 1].kind, OrnamentElement::CornerRegula);
		l13.insert(piece);
	}
	EXPECT_EQ(l13.size(), 4u);

	std::vector<int> l8(nr, 0), l9(nr, 0), l11(nm, 0), l12(nm, 0), l10(ng, 0), l15(nmg, 0);
	std::array<std::size_t, 16> classes{};
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection)
			continue;
		ASSERT_EQ(rec.faces.size(), 2u);
		unsigned f0 = rec.faces[0], f1 = rec.faces[1];
		if (owner[f0] > owner[f1] || (owner[f0] == owner[f1] && f0 > f1))
			std::swap(f0, f1);
		const std::size_t p0 = owner[f0], p1 = owner[f1];
		auto both = [&](float zz) { return horizontal(f0, zz) && horizontal(f1, zz); };
		std::size_t cls = 0;
		if (p0 == 0 && p1 >= 1 && p1 <= nr)
		{
			const OrnamentPiece &reg = o.regulae[p1 - 1];
			if (both(zTaenia))
			{
				cls = 8;
				++l8[p1 - 1];
			}
			else if ((onPlane(f0, reg.side, dos) && onPlane(f1, reg.side, dos)) ||
			         (reg.kind == OrnamentElement::CornerRegula && onPlane(f0, previousOf(reg.side), dos) &&
			          onPlane(f1, previousOf(reg.side), dos)))
			{
				cls = 9;
				++l9[p1 - 1];
			}
		}
		else if (p0 == p1 && p0 >= 1 && p0 <= nr)
		{
			// Goutte (faces apres celles du corps de la regula) sous le dessous de sa regula.
			const OrnamentPiece &reg = o.regulae[p0 - 1];
			const unsigned body = reg.kind == OrnamentElement::CornerRegula ? 20u : 12u;
			const unsigned local0 = f0 - firstFace[p0], local1 = f1 - firstFace[p0];
			if (local0 < body && local1 >= body && both(static_cast<float>(foot + reg.z0)))
			{
				cls = 10;
				++l10[static_cast<std::size_t>(reg.firstGutta) + (local1 - body) / 32u];
			}
		}
		else if (p0 == 0 && p1 > nr)
		{
			const std::size_t k = p1 - 1 - nr;
			const OrnamentPiece &mut = o.mutules[k];
			if (onCouples(f0, mut.side, mut.d0, mut.z1 + mut.rise, mut.d1, mut.z1) &&
			    onCouples(f1, mut.side, mut.d0, mut.z1 + mut.rise, mut.d1, mut.z1))
			{
				cls = 11;
				++l11[k];
			}
			else if (onPlane(f0, mut.side, back) && onPlane(f1, mut.side, back))
			{
				cls = 12;
				++l12[k];
			}
			else if (both(static_cast<float>(foot + mut.z0)))
				cls = 14;   // classe de N2, affirmee absente ci-dessous
		}
		else if (p0 == p1 && p0 > nr)
		{
			// Tete d'une goutte (triangle d'eventail de tete, 4 i + 3) sur le dessous de son mutule.
			const std::size_t k = p0 - 1 - nr;
			const OrnamentPiece &mut = o.mutules[k];
			const unsigned local0 = f0 - firstFace[p0], local1 = f1 - firstFace[p0];
			if (local0 < 12u && local1 >= 12u && (local1 - 12u) % 4u == 3u &&
			    onCouples(f0, mut.side, mut.d0, mut.z0 + mut.rise, mut.d1, mut.z0))
			{
				cls = 15;
				++l15[static_cast<std::size_t>(mut.firstGutta) + (local1 - 12u) / 32u];
			}
		}
		if (cls == 0)
			ADD_FAILURE() << "recouvrement hors liste : pieces " << p0 << " et " << p1 << ", faces " << f0 << " et " << f1;
		++classes[cls];
	}
	EXPECT_EQ(classes[14], 0u);
	for (std::size_t k = 0; k < nr; ++k)
	{
		EXPECT_GE(l8[k], 1) << "regula " << k << " sans L8";
		EXPECT_GE(l9[k], 1) << "regula " << k << " sans L9";
	}
	for (std::size_t q = 0; q < l10.size(); ++q)
		EXPECT_GE(l10[q], 1) << "goutte " << q << " sans L10";
	for (std::size_t k = 0; k < nm; ++k)
	{
		EXPECT_GE(l11[k], 1) << "mutule " << k << " sans L11";
		EXPECT_GE(l12[k], 1) << "mutule " << k << " sans L12";
	}
	for (std::size_t q = 0; q < l15.size(); ++q)
		EXPECT_GE(l15[q], 1) << "goutte de mutule " << q << " sans L15";
	for (std::size_t cl = 8; cl < classes.size(); ++cl)
		if (classes[cl])
			RecordProperty("l" + std::to_string(cl) + "_records", std::to_string(classes[cl]));

	// Sous AxisTriglyph (regression EN4), les mutules d'angle chevauchent : refus 'mutules.width' (MS-56b).
	GreekEntablatureSettings axis = s;
	axis.corner = DoricCornerStrategy::AxisTriglyph;
	std::string refusal;
	try
	{
		LayoutGreekOrnaments(axis, p);
	}
	catch (const std::invalid_argument &x)
	{
		refusal = x.what();
	}
	EXPECT_EQ(refusal.rfind("Entablement : 'mutules.width'", 0), 0u) << refusal;

	// Le dos des mutules se calcule en pouces : la face de la plaque ou la profondeur dans une autre unite, ou
	// une profondeur manquante, sont refusees, cle nommee (contrat de MonumentEntablatureSettings).
	auto conversionRefusal = [&](const std::function<void (MonumentEntablature &)> &edit) {
		MonumentData changed = d;
		edit(changed.entablature);
		try
		{
			MonumentEntablatureSettings(changed);
		}
		catch (const std::invalid_argument &x)
		{
			return std::string(x.what());
		}
		return std::string();
	};
	const std::string plateInMetres =
		conversionRefusal([](MonumentEntablature &m) { m.mutulePlateProjection.unit = MonumentUnit::Metre; });
	EXPECT_NE(plateInMetres.find("'entablature.mutulePlateProjection'"), std::string::npos) << plateInMetres;
	const std::string depthInMetres =
		conversionRefusal([](MonumentEntablature &m) { m.mutuleDepth.unit = MonumentUnit::Metre; });
	EXPECT_NE(depthInMetres.find("'entablature.mutuleDepth'"), std::string::npos) << depthInMetres;
	const std::string depthMissing = conversionRefusal([](MonumentEntablature &m) {
		m.mutuleDepth.value.reset();
		m.mutuleDepth.status = MonumentStatus::Missing;
		m.mutuleDepth.sources.clear();
	});
	EXPECT_NE(depthMissing.find("valeur manquante 'entablature.mutuleDepth'"), std::string::npos) << depthMissing;
}

// J22
TEST(TEST_cgarchi_classical_monument, the_mutules_of_the_readme_hang_eighteen_guttae_under_a_fourteen_degree_soffit)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentEntablature &e = d.entablature;
	const MonumentEntablatureControls &c = d.entablatureControls;
	const TempleSettings t = MonumentTempleSettings(d);
	ASSERT_TRUE(t.entablature.has_value());
	const GreekEntablatureSettings &s = *t.entablature;
	ASSERT_TRUE(s.mutules.has_value() && s.mutules->guttae.has_value());
	const GreekMutuleSettings &mu = *s.mutules;
	const GreekMutuleGuttaSettings &gs = *mu.guttae;
	const std::vector<double> z = GreekEntablatureBandBounds(s);
	const std::size_t mb = static_cast<std::size_t>(mu.band);
	const double back = s.bands[mb].projection;

	// Cotes de la l. 276 depuis le dessus du stylobate, le pied de l'entablement a la hauteur de colonne (l. 209),
	// a la demi-unite : pivot (arete avant-basse du mutule), arete avant-haute, dessous au dos, haut du bec.
	const double foot = Length(d.columns.height);
	EXPECT_NEAR(foot + z[mb], Metres(c.soffitPivotZ), kPrintedHalfUnit);
	EXPECT_NEAR(foot + z[mb + 1], Metres(c.mutuleFrontTopZ), kPrintedHalfUnit);
	EXPECT_NEAR(foot + (z[mb] + mu.rise), Metres(c.soffitBackZ), kPrintedHalfUnit);
	EXPECT_NEAR(foot + z[mb + 2], Metres(c.dripTopZ), kPrintedHalfUnit);
	// Profondeur sous la pente (l. 274) : dos et projection horizontale a kInchHalfUnit ; montee (l. 276), imprimee
	// a deux decimales du pouce, a kHundredthInchHalfUnit.
	EXPECT_NEAR(back, Metres(c.mutuleInclinedBack), kInchHalfUnit);
	EXPECT_NEAR(mu.face - back, Metres(c.mutuleHorizontalDepth), kInchHalfUnit);
	EXPECT_NEAR(mu.rise, Metres(c.soffitRise), kHundredthInchHalfUnit);
	// Pivot sur la face de la plaque (l. 276) : un seul produit par le pouce des deux cotes.
	EXPECT_EQ(Metres(c.soffitPivotProjection), mu.face);
	// Ressaut vertical au dos (l. 276) : dans la plage de la montee a 14 +- 1 degres.
	const double depth = Length(e.mutuleDepth);
	EXPECT_GT(Metres(c.geisonBackStep), depth * std::sin(13.0 * kPi / 180.0));
	EXPECT_LT(Metres(c.geisonBackStep), depth * std::sin(15.0 * kPi / 180.0));
	RecordProperty("soffit_pivot_z_m", std::to_string(foot + z[mb]));
	RecordProperty("soffit_back_z_m", std::to_string(foot + (z[mb] + mu.rise)));

	// 2 520 gouttes, 18 par mutule (l. 275) ; pied plus large de 0,5 in en diametre que la tete, aux trois rangs :
	// litteral, independant de la conversion.
	TempleLayout layout;
	ASSERT_NO_THROW(layout = LayoutTemple(t));
	ASSERT_TRUE(layout.ornaments.has_value() && layout.frieze.has_value());
	const OrnamentLayout &o = *layout.ornaments;
	EXPECT_EQ(o.mutuleGuttae.size(), 2520u);
	for (const OrnamentPiece &m : o.mutules)
		EXPECT_EQ(m.guttaCount, 18);
	EXPECT_NEAR(2.0 * gs.conicity, 0.0127, 1e-12);

	// Avant de la tete de chaque goutte du rang avant sur l'arete avant-basse du mutule (D3-76), en double, par les
	// placements : d1 - (along - O / 2) cos, a 1e-9 m.
	const double slant = std::hypot(mu.face - back, mu.rise), cosine = (mu.face - back) / slant;
	std::size_t frontGuttae = 0;
	for (const OrnamentPiece &m : o.mutules)
		for (int k = 0; k < m.guttaCount; ++k)
		{
			const MutuleGuttaPlacement &g = o.mutuleGuttae[static_cast<std::size_t>(m.firstGutta + k)];
			if (g.row != 0)
				continue;
			++frontGuttae;
			EXPECT_NEAR(m.d1 - (g.along - 0.5 * gs.rowDiameters[0]) * cosine, mu.face, 1e-9) << k;
		}
	EXPECT_EQ(frontGuttae, 840u);

	// Placements de la bibliotheque contre les controles de la l. 275, a kInchHalfUnit : axes des rangs du milieu et
	// de l'arriere, le long de la sous-face depuis l'arete avant (`along` est l'axe meme, EN23) ; en largeur, ecart
	// des axes voisins d'un rang contre le pas, axes extremes a edgeAxis lu des bords du mutule.
	const double middleAxis = Metres(c.mutuleGuttaMiddleAxis), rearAxis = Metres(c.mutuleGuttaRearAxis);
	const double pitch = Metres(c.mutuleGuttaPitch), edge = Length(e.mutuleGuttaEdgeAxis);
	std::size_t rowsChecked = 0;
	for (const OrnamentPiece &m : o.mutules)
	{
		std::array<std::vector<double>, 3> across;
		for (int k = 0; k < m.guttaCount; ++k)
		{
			const MutuleGuttaPlacement &g = o.mutuleGuttae[static_cast<std::size_t>(m.firstGutta + k)];
			ASSERT_TRUE(g.row >= 0 && g.row < 3);
			if (g.row == 1)
				EXPECT_NEAR(g.along, middleAxis, kInchHalfUnit);
			else if (g.row == 2)
				EXPECT_NEAR(g.along, rearAxis, kInchHalfUnit);
			across[static_cast<std::size_t>(g.row)].push_back(g.a);
		}
		for (std::vector<double> &row : across)
		{
			ASSERT_EQ(row.size(), static_cast<std::size_t>(gs.perRow));
			std::sort(row.begin(), row.end());
			EXPECT_NEAR(row.front() - m.a0, edge, kInchHalfUnit);
			EXPECT_NEAR(m.a1 - row.back(), edge, kInchHalfUnit);
			for (std::size_t j = 0; j + 1 < row.size(); ++j)
				EXPECT_NEAR(row[j + 1] - row[j], pitch, kInchHalfUnit);
			++rowsChecked;
		}
	}
	EXPECT_EQ(rowsChecked, 3u * o.mutules.size());

	// Sommets maillés, en float : avant du pied de chaque goutte du rang avant contre 35,826 in (l. 275), a la
	// demi-unite (35,825652 en double) ; avant de sa tete, rapporte (35,825). Par goutte : `sides` sommets du pied,
	// l'avant en premier, puis ceux de la tete, puis les deux centres, apres les 8 sommets du prisme du mutule.
	std::vector<std::unique_ptr<Mesh>> pieces;
	ASSERT_NO_THROW(pieces = BuildEntablaturePieceMeshes(s, layout.peristyle, *layout.frieze, o, layout.entablatureFoot));
	const std::size_t nt = layout.frieze->triglyphs.size(), nr = o.regulae.size();
	ASSERT_EQ(pieces.size(), nt + nr + o.mutules.size());
	const double hx = layout.peristyle.halfWidth, hy = layout.peristyle.halfDepth;
	auto distance = [&](PeristyleSide side, const float q[3]) {
		return side == PeristyleSide::Front   ? -static_cast<double>(q[1]) - hy
		       : side == PeristyleSide::Back  ? static_cast<double>(q[1]) - hy
		       : side == PeristyleSide::Right ? static_cast<double>(q[0]) - hx
		                                      : -static_cast<double>(q[0]) - hx;
	};
	const unsigned perGutta = 2u * static_cast<unsigned>(gs.sides) + 2u;
	double firstHead = 0.0, firstFoot = 0.0, worstFoot = 0.0;
	std::size_t meshed = 0;
	for (std::size_t k = 0; k < o.mutules.size(); ++k)
	{
		const OrnamentPiece &m = o.mutules[k];
		const Mesh &mesh = *pieces[nt + nr + k];
		ASSERT_EQ(mesh.GetNVertices(), 8u + static_cast<unsigned>(m.guttaCount) * perGutta);
		for (int j = 0; j < m.guttaCount; ++j)
		{
			if (o.mutuleGuttae[static_cast<std::size_t>(m.firstGutta + j)].row != 0)
				continue;
			const unsigned base = 8u + static_cast<unsigned>(j) * perGutta;
			float footFront[3], headFront[3];
			mesh.GetVertex(base, footFront);
			mesh.GetVertex(base + static_cast<unsigned>(gs.sides), headFront);
			const double footD = distance(m.side, footFront), headD = distance(m.side, headFront);
			EXPECT_NEAR(footD, Metres(c.mutuleGuttaFrontFootProjection), kInchHalfUnit) << k << " " << j;
			worstFoot = std::max(worstFoot, std::fabs(footD - Metres(c.mutuleGuttaFrontFootProjection)));
			if (meshed++ == 0)
			{
				firstHead = headD;
				firstFoot = footD;
			}
		}
	}
	EXPECT_EQ(meshed, 840u);
	RecordProperty("front_gutta_head_in", std::to_string(firstHead / kMonumentInch));
	RecordProperty("front_gutta_foot_in", std::to_string(firstFoot / kMonumentInch));
	RecordProperty("front_gutta_foot_worst_gap_in", std::to_string(worstFoot / kMonumentInch));

	// La pente se convertit en degres : une autre unite, ou une pente manquante, sont refusees, cle nommee
	// (contrat de MonumentEntablatureSettings).
	auto conversionRefusal = [&](const std::function<void (MonumentEntablature &)> &edit) {
		MonumentData changed = d;
		edit(changed.entablature);
		try
		{
			MonumentEntablatureSettings(changed);
		}
		catch (const std::invalid_argument &x)
		{
			return std::string(x.what());
		}
		return std::string();
	};
	const std::string slopeInMetres =
		conversionRefusal([](MonumentEntablature &m) { m.soffitSlope.unit = MonumentUnit::Metre; });
	EXPECT_NE(slopeInMetres.find("'entablature.soffitSlope'"), std::string::npos) << slopeInMetres;
	const std::string slopeMissing = conversionRefusal([](MonumentEntablature &m) {
		m.soffitSlope.value.reset();
		m.soffitSlope.status = MonumentStatus::Missing;
		m.soffitSlope.sources.clear();
	});
	EXPECT_NE(slopeMissing.find("valeur manquante 'entablature.soffitSlope'"), std::string::npos) << slopeMissing;
}
