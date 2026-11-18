#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
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
#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_krepis.h>
#include <cgarchi/classical_monument.h>
#include <cgarchi/classical_monument_data.h>
#include <cgarchi/classical_peristyle.h>
#include <cgarchi/classical_temple.h>
#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/mesh_validation.h"
#include "greek_doric_test_support.h"

// ===========================================================================
//  Document du monument "monument/v9" : lecteur, schema, conversions, mode partiel
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
//     trace au pixel (README l. 228) (J13) ;
//   - kChannelDepthTolerance (0,1 in) : precision annoncee de la profondeur des
//     canaux, mesuree au trace (README l. 296) (J17) ;
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
// MO1 etendue, README l. 197 : 1,1 x 4,81 / 6,25 = 0,85 in ; demi-unites relatives
// 0,05 / 1,1 + 0,005 / 4,81 + 0,005 / 6,25, rapportees a 0,85 in.
constexpr double kTopFluteDepthTolerance = 0.85 * (0.05 / 1.1 + 0.005 / 4.81 + 0.005 / 6.25) * kMonumentInch;
constexpr double kProfileTolerance = 0.1 * kMonumentInch;
constexpr double kChannelDepthTolerance = 0.1 * kMonumentInch;
constexpr double kDecimalSumTolerance = 1e-9 * kMonumentInch;
// MO1, metope lue (README l. 293-294) : n_f / 2 (0,25 mm), largeur du triglyphe
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

// Le document est refuse, et le message porte "monument/v9 <path> : <motif>".
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
		{"krepis.euthynteriaHeight", &k.euthynteriaHeight},
		{"krepis.euthynteriaTread", &k.euthynteriaTread},
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
		{"pediment.apexRiseFt", &d.pediment.apexRiseFt},
		{"pediment.apexRiseIn", &d.pediment.apexRiseIn},
		{"pediment.rakingRun", &d.pediment.rakingRun},
		{"pediment.rakingProjectionFt", &d.pediment.rakingProjectionFt},
		{"pediment.rakingProjectionIn", &d.pediment.rakingProjectionIn},
		{"pediment.tympanumFace", &d.pediment.tympanumFace},
		{"pediment.sima", &d.pediment.sima},
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
// items, exclusiveMinimum, pattern, dependentRequired ; annotations sans effet : $schema, $id,
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
		"exclusiveMinimum", "pattern", "dependentRequired", "$schema", "$id", "title", "description", "$comment"};
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
	if (s.contains("dependentRequired") && v.is_object())
		for (auto it = s["dependentRequired"].begin(); it != s["dependentRequired"].end(); ++it)
			if (v.contains(it.key()))
				for (const json &name : it.value())
					if (!v.contains(name.get<std::string>()))
						return Join(path, name.get<std::string>()) + " : requis par " + it.key();
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
	const std::filesystem::path path = std::filesystem::path("src") / "cgarchi" / "schemas" / "monument-v9.schema";
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
	// Valeur manquante du document : la face interieure de l'architrave (EN1).
	const auto eu = [](json &d) -> json & { return d["entablature"]["innerFace"]; };

	ExpectEditRefused([&](json &d) { at(d).erase("status"); }, h + ".status", "required field is missing");
	ExpectEditRefused([&](json &d) { at(d)["status"] = kHypothesis; }, h + ".status",
	                  std::string("unknown value \"") + kHypothesis + "\"");
	ExpectEditRefused([&](json &d) { at(d)["status"] = nullptr; }, h + ".status", "expected a string, not null");
	ExpectEditRefused([&](json &d) { at(d)["value"] = nullptr; }, h + ".value", "expected a number, not null");
	ExpectEditRefused([&](json &d) { at(d).erase("value"); }, h + ".value", "required field is missing");
	ExpectEditRefused([&](json &d) { eu(d)["value"] = 1.0; }, "entablature.innerFace.value",
	                  "status \"manquant\" admits no value");
	ExpectEditRefused([&](json &d) { at(d)["sources"] = {"X"}; }, h + ".sources[0]", "unknown source \"X\"");
	ExpectEditRefused([&](json &d) { d["sources"]["Z"] = "absente du README"; }, "sources.Z",
	                  "listed but never cited");
	ExpectEditRefused([&](json &d) { at(d)["sources"] = json::array(); }, h + ".sources",
	                  "status \"lu\" requires a source");
	ExpectEditRefused([&](json &d) { at(d)["status"] = kMeasured; at(d)["sources"] = json::array(); }, h + ".sources",
	                  std::string("status \"") + kMeasured + "\" requires a source");
	ExpectEditRefused([&](json &d) { eu(d)["sources"] = {"S"}; }, "entablature.innerFace.sources",
	                  "status \"manquant\" admits no source");
	ExpectEditRefused([&](json &d) { at(d)["readme"] = ""; }, h + ".readme", "empty place");
	ExpectEditRefused([&](json &d) { d["extra"] = 1; }, "extra", "unknown field");
	ExpectEditRefused([&](json &d) { d["krepis"]["extra"] = 1; }, "krepis.extra", "unknown field");
	ExpectEditRefused([&](json &d) { at(d)["extra"] = 1; }, h + ".extra", "unknown field");
	// Version : seule monument/v9 est lue ; v1 a v8 sont refusees (D2-8).
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v1"; }, "schema", "unsupported schema \"monument/v1\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v2"; }, "schema", "unsupported schema \"monument/v2\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v3"; }, "schema", "unsupported schema \"monument/v3\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v4"; }, "schema", "unsupported schema \"monument/v4\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v5"; }, "schema", "unsupported schema \"monument/v5\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v6"; }, "schema", "unsupported schema \"monument/v6\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v7"; }, "schema", "unsupported schema \"monument/v7\"");
	ExpectEditRefused([&](json &d) { d["schema"] = "monument/v8"; }, "schema", "unsupported schema \"monument/v8\"");
	// Un document de forme v7 (euthynteria manquante d'une cle, sans pente) sous l'etiquette v9 est refuse, cles
	// nommees.
	ExpectEditRefused([&](json &d) { d["pediment"].erase("rakingRun"); }, "pediment.rakingRun", "required field is missing");
	ExpectEditRefused([&](json &d) { d["krepis"]["euthynteria"] = d["krepis"]["euthynteriaHeight"]; }, "krepis.euthynteria",
	                  "unknown field");
	ExpectEditRefused([&](json &d) { d["pedimentControls"]["roofEaveRise"] = d["pedimentControls"]["eaveHalfSpan"]; },
	                  "pedimentControls.roofEaveRise", "unknown field");
	ExpectEditRefused([&](json &d) { d["krepis"].erase("euthynteriaTread"); }, "krepis.euthynteriaTread",
	                  "required field is missing");
	// Blocs facultatifs (MO-P2) : un controle sans sa partie est refuse, partie nommee ; le peristyle exige ses
	// axes. Le fronton retire seul laisse ses controles orphelins.
	ExpectEditRefused([&](json &d) { d.erase("pediment"); }, "pedimentControls", "requires block \"pediment\"");
	ExpectEditRefused([&](json &d) { d.erase("columns"); }, "columnControls", "requires block \"columns\"");
	ExpectEditRefused([&](json &d) { d.erase("entablature"); }, "entablatureControls", "requires block \"entablature\"");
	ExpectEditRefused([&](json &d) { d.erase("peristyle"); }, "controls", "requires block \"peristyle\"");
	ExpectEditRefused([&](json &d) { d.erase("controls"); }, "controls", "required field is missing");
	ExpectEditRefused([&](json &d) { d["krepisControls"] = {{"footprintWidth", {{"value", 1.0}, {"readme", "l"}}},
	                                                          {"footprintLength", {{"value", 1.0}, {"readme", "l"}}}};
	                                   d.erase("krepis"); },
	                  "krepisControls", "requires block \"krepis\"");
	ExpectEditRefused([&](json &d) { d["controls"].erase("frontAxesY"); }, "controls.frontAxesY", "required field is missing");
	// Un document de forme v5 (sans sous-face inclinee ni gouttes des mutules) sous l'etiquette v9 est refuse, cles
	// nommees.
	ExpectEditRefused([&](json &d) { d["entablature"].erase("soffitSlope"); }, "entablature.soffitSlope",
	                  "required field is missing");
	ExpectEditRefused([&](json &d) { d["entablatureControls"].erase("mutuleGuttaRows"); },
	                  "entablatureControls.mutuleGuttaRows", "required field is missing");
	// Un document de forme v3 (sans les controles d'angle) sous l'etiquette v9 est refuse, cle nommee.
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
	ASSERT_TRUE(schema.is_object()) << "src/cgarchi/schemas/monument-v9.schema illisible depuis le repertoire courant";
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
	// 4 a la racine (blocs facultatifs, MO-P2), 4 dans source, 39 + 36 + 9 + 10 cles de valeur (angle compris) et 4
	// par valeur, 2 axes du peristyle et 2 par axe, 2 sommes facultatives presentes et 2 cles chacune, 11 controles
	// de la colonne et 3 par controle, un profil et ses 4 cles, une
	// suite de divisions et ses 6 cles, 10 + 9 + 4 + 18 controles de l'entablement (dont ceux de l'angle, des
	// gouttes, du dos des mutules, de la sous-face inclinee et des gouttes des mutules) et 3 par controle ; fronton :
	// 7 cles de valeur (rapport compris) et 4 par valeur, une suite de hauteurs et un polygone et leurs 6 cles,
	// 13 controles (pente comprise) et 3 par controle.
	EXPECT_EQ(paths.size(), 4u + 4u + (39u + 36u + 9u + 10u) * 5u + (2u * 3u + 2u * 2u) + 11u * 4u + 5u + 7u +
	                            (10u + 9u + 4u + 18u) * 4u + 7u * 5u + 2u * 7u + 13u * 4u);
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
	ASSERT_TRUE(d.controls.frontSum && d.controls.flankSum);
	EXPECT_NEAR(front, *d.controls.frontSum, kPrintedHalfUnit);
	EXPECT_NEAR(flank, *d.controls.flankSum, kPrintedHalfUnit);
	EXPECT_EQ(*d.controls.frontSum, 13.718);
	EXPECT_EQ(*d.controls.flankSum, 31.77);
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
	// Trois degres et l'euthynteria (KR3, convention Q6), assise pleine sous le 3e degre.
	ASSERT_EQ(static_cast<double>(layout.steps.size()), Value(k.stepCount) + 1.0);
	EXPECT_EQ(layout.steps[0].halfWidth, 0.5 * Value(k.stylobateWidth));
	EXPECT_EQ(layout.steps[0].halfDepth, 0.5 * Value(k.stylobateLength));
	EXPECT_LT(layout.steps[0].halfWidth, layout.steps[0].halfDepth);

	const double tol = 1e-12 * layout.stylobateTop;
	EXPECT_NEAR(layout.steps[0].zTop - layout.steps[0].zBottom, 0.364, tol);
	EXPECT_NEAR(layout.steps[1].zTop - layout.steps[1].zBottom, 0.346, tol);
	EXPECT_NEAR(layout.steps[2].zTop - layout.steps[2].zBottom, 0.35, tol);
	EXPECT_NEAR(layout.steps[3].zTop - layout.steps[3].zBottom, 0.37, tol);
	EXPECT_EQ(layout.steps[3].zBottom, 0.0);
	const double treads[] = {0.372, 0.372, 0.11};   // giron des 2e et 3e degres, ressaut de l'euthynteria
	for (std::size_t i = 1; i < 4; ++i)
	{
		const double dtol = 1e-12 * layout.steps[3].halfDepth;
		EXPECT_NEAR(layout.steps[i].halfWidth - layout.steps[i - 1].halfWidth, treads[i - 1], dtol) << i;
		EXPECT_NEAR(layout.steps[i].halfDepth - layout.steps[i - 1].halfDepth, treads[i - 1], dtol) << i;
	}
	const double sum = Value(k.stylobateHeight) + Value(k.secondStepHeight) + Value(k.thirdStepHeight) +
	                   Value(k.euthynteriaHeight);
	EXPECT_NEAR(layout.stylobateTop, sum, 1e-12);
	EXPECT_NEAR(layout.stylobateTop, 1.43, 1e-12);

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

	// Euthynteria (KR3, convention Q6) : un ressaut manquant est refuse, nomme.
	json given = EmbeddedDocument();
	given["krepis"]["euthynteriaTread"] = {{"unit", "m"}, {"status", "manquant"}, {"sources", json::array()},
	                                       {"readme", std::string(kSection) + "3.2"}};
	const MonumentData euthynteria = ParseMonument(given.dump());
	expectThrowNaming([&] { MonumentKrepisSettings(euthynteria); }, "'krepis.euthynteriaTread'");
	json noHeight = EmbeddedDocument();
	noHeight["krepis"]["euthynteriaHeight"] = {{"unit", "m"}, {"status", "manquant"}, {"sources", json::array()},
	                                           {"readme", std::string(kSection) + "3.2"}};
	const MonumentData euthynteriaHeight = ParseMonument(noHeight.dump());
	expectThrowNaming([&] { MonumentKrepisSettings(euthynteriaHeight); }, "'krepis.euthynteriaHeight'");

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
		EXPECT_EQ(v->unit == MonumentUnit::Ratio, key == "pediment.rakingRun");
	}
	EXPECT_EQ(seen.size(), 7u);
	// Chacune telle qu'ecrite au README ; une valeur manquante porte l'unite attendue (l. 36).
	EXPECT_EQ(data.columns.lowerDiameter.unit, MonumentUnit::Metre);         // l. 121, « 1,016 m »
	EXPECT_EQ(data.columns.abacusHalfWidth.unit, MonumentUnit::Inch);        // l. 207, « 22,451 in »
	EXPECT_EQ(data.columns.entasisSagitta.unit, MonumentUnit::Millimetre);   // l. 198, « 7 mm »
	EXPECT_EQ(data.columns.grooveDepth.unit, MonumentUnit::Metre);
	const MonumentColumnControls &cc = data.columnControls;
	for (const MonumentControlValue *c : {&cc.shaftHeight, &cc.footFluteChord, &cc.fluteCentralRadius,
	                                      &cc.fluteLateralRadius, &cc.topFluteDepth, &cc.cornerFluteChord,
	                                      &cc.cornerFluteDepth, &cc.capitalHeight, &cc.annuletsAndRelief,
	                                      &cc.upperRadius, &cc.echinusLipRetreat})
		EXPECT_NE(c->unit, MonumentUnit::One);
	// Controles d'angle de la frise (l. 297-298) : en metres tels qu'ecrits, y compris les deux
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

	// Cote en pieds et pouces (README l. 251, 2 ft 8,9 in), calculee en pouces : 2 x 12 est exact
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
	// Un rapport n'est pas une longueur.
	expectRefusalNaming(data.pediment.rakingRun, "pediment.rakingRun");
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

	// Regulae, gouttes et mutules (l. 254, 273, 274, 315, 316, 319) : nombre de gouttes en "1", largeur des
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

	// Sous-face inclinee et gouttes des mutules (l. 317, 318) : pente en "°" (R9), gouttes par rang en "1", le reste
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

	// Fronton (l. 608 a 611, 616) : hauteur et saillie en pieds et pouces (R6), nu du tympan, hauteurs et section en
	// pouces, sima manquante en metres (unite attendue) ; controles en metres ou en pouces tels qu'ecrits, pente en
	// « ° » (R9).
	const MonumentPediment &f = data.pediment;
	const MonumentPedimentControls &fc = data.pedimentControls;
	EXPECT_EQ(f.apexRiseFt.unit, MonumentUnit::Foot);
	EXPECT_EQ(f.rakingProjectionFt.unit, MonumentUnit::Foot);
	for (const MonumentValue *v : {&f.apexRiseIn, &f.rakingProjectionIn, &f.tympanumFace})
		EXPECT_EQ(v->unit, MonumentUnit::Inch);
	EXPECT_EQ(f.rakingHeights.unit, MonumentUnit::Inch);
	EXPECT_EQ(f.rakingSection.unit, MonumentUnit::Inch);
	EXPECT_EQ(f.sima.unit, MonumentUnit::Metre);
	EXPECT_EQ(fc.rakingSlope.unit, MonumentUnit::Degree);
	for (const MonumentControlValue *c : {&fc.apexRise, &fc.eaveHalfSpan, &fc.rakingHeightM, &fc.rakingVerticalHeightM,
	                                      &fc.rakingProjection, &fc.tympanumFaceM})
		EXPECT_EQ(c->unit, MonumentUnit::Metre);
	// Pente 1 : r (R12) : unite "ratio", pour elle seule, dans les deux sens ; MonumentRatio la refuse sur une longueur.
	EXPECT_EQ(f.rakingRun.unit, MonumentUnit::Ratio);
	EXPECT_EQ(MonumentRatio(f.rakingRun, "pediment.rakingRun"), 4.0846);
	ExpectEditRefused([&](json &d) { d["pediment"]["rakingRun"]["unit"] = "m"; }, "pediment.rakingRun.unit",
	                  "a ratio requires unit \"ratio\", not \"m\"");
	ExpectEditRefused([&](json &d) { d["pediment"]["tympanumFace"]["unit"] = "ratio"; }, "pediment.tympanumFace.unit",
	                  "unit \"ratio\" outside a ratio");
	ExpectEditRefused([&](json &d) { d["pediment"]["rakingRun"]["value"] = 0.0; }, "pediment.rakingRun.value",
	                  "0 outside (0, inf]");
	ExpectEditRefused([&](json &d) { d["pediment"]["rakingRun"]["value"] = -4.0846; }, "pediment.rakingRun.value",
	                  "-4.0846 outside (0, inf]");
	try
	{
		MonumentRatio(f.tympanumFace, "pediment.tympanumFace");
		ADD_FAILURE() << "aucun refus : un pouce n'est pas un rapport";
	}
	catch (const std::invalid_argument &x)
	{
		EXPECT_EQ(std::string(x.what()).rfind("Monument : 'pediment.tympanumFace' n'est pas un rapport", 0), 0u) << x.what();
	}
	for (const MonumentControlValue *c : {&fc.rakingHeight, &fc.rakingVerticalHeight, &fc.rakingListelProjection,
	                                      &fc.rakingCoronaProjection, &fc.rakingBottomFilletProjection,
	                                      &fc.rakingCoronaGap})
		EXPECT_EQ(c->unit, MonumentUnit::Inch);
	EXPECT_EQ(MonumentFeetAndInches(f.apexRiseFt, f.apexRiseIn, "pediment.apexRise"), (5.0 * 12.0 + 9.75) * kMonumentInch);
	ExpectEditRefused([&](json &d) { d["pedimentControls"]["rakingSlope"]["unit"] = "m"; },
	                  "pedimentControls.rakingSlope.unit", "an angle requires unit \"" + degree + "\", not \"m\"");
	ExpectEditRefused([&](json &d) { d["pedimentControls"]["eaveHalfSpan"]["unit"] = degree; },
	                  "pedimentControls.eaveHalfSpan.unit", "unit \"" + degree + "\" on a length");
	ExpectEditRefused([&](json &d) { d["pediment"]["rakingSection"]["unit"] = "1"; }, "pediment.rakingSection.unit",
	                  "unit \"1\" on a length");
	ExpectEditRefused([&](json &d) { d["pediment"]["rakingHeights"]["unit"] = "ft"; }, "pediment.rakingHeights.unit",
	                  "unit \"ft\" outside a feet key");
	ExpectEditRefused([&](json &d) { d["pediment"]["apexRiseFt"]["unit"] = "in"; }, "pediment.apexRiseFt.unit",
	                  "a feet value requires unit \"ft\", not \"in\"");
	ExpectEditRefused([&](json &d) { d["pediment"]["sima"]["unit"] = "1"; }, "pediment.sima.unit",
	                  "unit \"1\" on a length");
}

// J11
TEST(TEST_cgarchi_classical_monument, the_column_controls_of_the_readme_recompute_from_the_values)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentColumns &c = d.columns;
	const MonumentColumnControls &cc = d.columnControls;
	const GreekDoricColumnSettings s = MonumentColumnSettings(d, false);
	const GreekDoricColumnSettings corner = MonumentColumnSettings(d, true);

	// Chapiteau du README (l. 742) : abaque, echine, annelets, gorgerin ; la rainure en dehors.
	const double capital = Length(c.abacusHeight) + Length(c.echinusHeight) + Length(c.annuletsHeight)
	                     + Length(c.hypotrachelionHeight);
	EXPECT_NEAR(capital, Metres(cc.capitalHeight), kInchHalfUnit);
	// Fut du README (l. 191) : hauteur de colonne moins chapiteau, rainure comprise.
	const double shaft = Length(c.height) - capital;
	EXPECT_NEAR(shaft, Metres(cc.shaftHeight), kPrintedHalfUnit);
	// Le fut du modele, plus la rainure que porte son chapiteau (GD3c), est le fut du README.
	double modelCapital = s.hypotrachelionHeight + s.echinusHeight + s.abacusHeight;
	for (const GreekDoricAnnulet &a : s.annulets)
		modelCapital += a.height;
	EXPECT_NEAR(s.height - modelCapital + s.grooveHeight, shaft, 1e-12);
	// Maximum de l'entasis a mi-hauteur du fut du README (l. 199) : zf est lu, il n'est pas
	// recalcule sur le fut du modele.
	EXPECT_NEAR(s.entasisHeight, 0.5 * shaft, kPrintedHalfUnit);
	RecordProperty("zf_minus_half_readme_shaft_mm", std::to_string(1e3 * (s.entasisHeight - 0.5 * shaft)));
	RecordProperty("zf_minus_half_model_shaft_mm",
	               std::to_string(1e3 * (s.entasisHeight - 0.5 * (s.height - modelCapital))));
	EXPECT_NEAR(s.entasisSagitta, 0.007, 1e-15);   // l. 198, 7 mm

	// Corde au pied (l. 193, « 6,25 in = 0,159 m ») : le modele la deduit de D de [D], le README
	// la lit sur [S]. MO1 : demi-unite de 6,25 in (0,005 in) plus celle de D (0,5 mm) portee sur
	// la corde, D sin (pi / N).
	const double halfAngle = kPi / s.fluteCount;
	const double chord = s.lowerDiameter * std::sin(halfAngle);
	const double footChordTolerance = kHundredthInchHalfUnit + kPrintedHalfUnit * std::sin(halfAngle);
	EXPECT_EQ(cc.footFluteChord.unit, MonumentUnit::Inch);
	EXPECT_NEAR(chord, Metres(cc.footFluteChord), footChordTolerance);
	RecordProperty("foot_chord_tolerance_mm", std::to_string(1e3 * footChordTolerance));
	RecordProperty("foot_chord_gap_mm", std::to_string(1e3 * (chord - Metres(cc.footFluteChord))));
	// Fausse ellipse (l. 196) sur la corde et la profondeur du README : Rc = 6,37 in, arrondi au
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

	// Cannelures d'angle (l. 161) : section homothetique, meme rapport profondeur / corde.
	const double cornerChord = corner.lowerDiameter * std::sin(halfAngle);
	EXPECT_NEAR(cornerChord, Metres(cc.cornerFluteChord), kPrintedHalfUnit);
	EXPECT_NEAR(corner.fluteDepthRatio * cornerChord, Metres(cc.cornerFluteDepth), kPrintedHalfUnit);
	RecordProperty("corner_flute_depth_gap_mm",
	               std::to_string(1e3 * (corner.fluteDepthRatio * cornerChord - Metres(cc.cornerFluteDepth))));
	// Profondeur au sommet (l. 197), MO1 etendue au produit et au quotient.
	const double topDepth = s.fluteDepthRatio * s.upperDiameter * std::sin(halfAngle);
	EXPECT_NEAR(topDepth, Metres(cc.topFluteDepth), kTopFluteDepthTolerance);
	RecordProperty("top_flute_depth_in", std::to_string(topDepth / kMonumentInch));

	// Rayon superieur de [S] (l. 744) : demi-cote de l'abaque moins annelets et ressaut.
	EXPECT_NEAR(Length(c.abacusHalfWidth) - Metres(cc.annuletsAndRelief), Metres(cc.upperRadius), kInchHalfUnit);
	EXPECT_GT(Metres(cc.annuletsAndRelief), Length(c.annuletsBottomRetreat));
	RecordProperty("gd5a_upper_radius_gap_mm", std::to_string(1e3 * (Metres(cc.upperRadius) - 0.5 * s.upperDiameter)));
	// Pied de l'echine au profil (l. 232) : le sommet des annelets (l. 209), au fond de l'echine.
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

		// Saillie maximale (l. 239) : le sommet le plus saillant de l'echine.
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
	// Partage du chapiteau du triglyphe : convention de la source, sans reference (l. 277).
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
	EXPECT_EQ(e.friezeHeightFt.readme, section + "6.1, l. 256");
	EXPECT_EQ(e.friezeHeightIn.readme, section + "6.1, l. 256");
	EXPECT_EQ(e.triglyphCapitalBandHeight.readme, section + "6.2, l. 277");
	EXPECT_EQ(e.innerFace.readme, section + "6.2, l. 285");
	EXPECT_EQ(e.triglyphDivisions.readme, section + "6.3, l. 295");
	EXPECT_EQ(e.channelDepth.readme, section + "6.3, l. 296");

	// Division du triglyphe (l. 295), telle qu'ecrite, mots dans l'ordre.
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

	// Controles d'angle de la frise, lus tels qu'ecrits, avec leur lieu : l. 297 pour le centre du
	// triglyphe d'angle, l. 298 pour les deux solutions. Le lieu est controle par le lecteur sans
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
		const std::string line = std::string(key) == "cornerTriglyphOffset" ? "297" : "298";
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
		{"regulaProjection", &e.regulaProjection, 20.859, "6.2, l. 273"},
		{"regulaGuttaCount", &e.regulaGuttaCount, 6.0, "6.1, l. 254"},
		{"regulaGuttaDiameter", &e.regulaGuttaDiameter, 1.466, "6.1, l. 254"},
		{"regulaGuttaGap", &e.regulaGuttaGap, 2.25, "6.1, l. 254"},
		{"regulaGuttaTopProjection", &e.regulaGuttaTopProjection, 20.809, "6.2, l. 274"},
		{"regulaGuttaFootProjection", &e.regulaGuttaFootProjection, 20.859, "6.2, l. 274"},
		{"mutuleWidthFt", &e.mutuleWidthFt, 1.0, "6.4, l. 315"},
		{"mutuleWidthIn", &e.mutuleWidthIn, 8.75, "6.4, l. 315"},
		{"mutuleDepth", &e.mutuleDepth, 15.876, "6.4, l. 316"},
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
		{"regulaGuttaMargin", &c.regulaGuttaMargin, 0.127, "6.1, l. 254"},
		{"regulaGuttaShortfall", &c.regulaGuttaShortfall, 0.254, "6.1, l. 254"},
		{"regulaGuttaCountCheck", &c.regulaGuttaCountCheck, 6.0, "6.4, l. 319"},
		{"mutuleBack", &c.mutuleBack, 19.949, "6.4, l. 316"},
	};
	for (const auto &[key, control, written, place] : checks)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(control->value, written);
		EXPECT_EQ(document["entablatureControls"][key]["readme"].get<std::string>(), section + place);
	}

	// Sous-face inclinee et gouttes des mutules : les dix valeurs telles qu'ecrites, avec leur statut et leur lieu
	// (l. 317, 318) ; toutes citent [S].
	const MonumentStatus lu = MonumentStatus::Read, mesure = MonumentStatus::Measured, deduit = MonumentStatus::Deduced;
	const std::tuple<const char *, const MonumentValue *, double, MonumentStatus, const char *> soffit[] = {
		{"soffitSlope", &e.soffitSlope, 14.0, mesure, "6.4, l. 318"},
		{"mutuleGuttaFrontDiameter", &e.mutuleGuttaFrontDiameter, 1.46, lu, "6.4, l. 317"},
		{"mutuleGuttaMiddleDiameter", &e.mutuleGuttaMiddleDiameter, 1.55, lu, "6.4, l. 317"},
		{"mutuleGuttaRearDiameter", &e.mutuleGuttaRearDiameter, 1.5, lu, "6.4, l. 317"},
		{"mutuleGuttaFrontGap", &e.mutuleGuttaFrontGap, 4.4, lu, "6.4, l. 317"},
		{"mutuleGuttaRearGap", &e.mutuleGuttaRearGap, 4.466, lu, "6.4, l. 317"},
		{"mutuleGuttaHeight", &e.mutuleGuttaHeight, 1.0, mesure, "6.4, l. 317"},
		{"mutuleGuttaEdgeAxis", &e.mutuleGuttaEdgeAxis, 1.775, lu, "6.4, l. 317"},
		{"mutuleGuttaPerRow", &e.mutuleGuttaPerRow, 6.0, lu, "6.4, l. 317"},
		{"mutuleGuttaFootFlare", &e.mutuleGuttaFootFlare, 0.5, deduit, "6.4, l. 317"},
	};
	for (const auto &[key, v, written, status, place] : soffit)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(Value(*v), written);
		EXPECT_EQ(v->status, status);
		EXPECT_EQ(v->sources, s);
		EXPECT_EQ(v->readme, section + place);
	}
	// Les dix-huit controles des l. 316 a 318, lus tels qu'ecrits, avec leur unite et leur lieu.
	const MonumentUnit in = MonumentUnit::Inch, m = MonumentUnit::Metre, one = MonumentUnit::One;
	const std::tuple<const char *, const MonumentControlValue *, double, MonumentUnit, const char *> soffitChecks[] = {
		{"mutuleGuttaRows", &c.mutuleGuttaRows, 3.0, one, "6.4, l. 317"},
		{"mutuleGuttaPitchCount", &c.mutuleGuttaPitchCount, 5.0, one, "6.4, l. 317"},
		{"mutuleGuttaMiddleAxis", &c.mutuleGuttaMiddleAxis, 6.635, in, "6.4, l. 317"},
		{"mutuleGuttaRearAxis", &c.mutuleGuttaRearAxis, 12.626, in, "6.4, l. 317"},
		{"mutuleGuttaPitch", &c.mutuleGuttaPitch, 3.44, in, "6.4, l. 317"},
		{"mutuleRearMargin", &c.mutuleRearMargin, 2.5, in, "6.4, l. 316"},
		{"mutuleHorizontalDepth", &c.mutuleHorizontalDepth, 15.404, in, "6.4, l. 316"},
		{"mutuleInclinedBack", &c.mutuleInclinedBack, 20.421, in, "6.4, l. 316"},
		{"soffitPivotZ", &c.soffitPivotZ, 7.377, m, "6.4, l. 318"},
		{"mutuleFrontTopZ", &c.mutuleFrontTopZ, 7.437, m, "6.4, l. 318"},
		{"soffitBackZ", &c.soffitBackZ, 7.475, m, "6.4, l. 318"},
		{"soffitRise", &c.soffitRise, 3.84, in, "6.4, l. 318"},
		{"geisonBackStep", &c.geisonBackStep, 3.9, in, "6.4, l. 318"},
		{"dripTopZ", &c.dripTopZ, 7.469, m, "6.4, l. 318"},
		{"soffitPivotProjection", &c.soffitPivotProjection, 35.825, in, "6.4, l. 318"},
		{"mutuleGuttaRadialFlare", &c.mutuleGuttaRadialFlare, 0.25, in, "6.4, l. 317"},
		{"mutuleGuttaRadialFlareExact", &c.mutuleGuttaRadialFlareExact, 0.249, in, "6.4, l. 317"},
		{"mutuleGuttaFrontFootProjection", &c.mutuleGuttaFrontFootProjection, 35.826, in, "6.4, l. 317"},
	};
	for (const auto &[key, control, written, unit, place] : soffitChecks)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(control->value, written);
		EXPECT_EQ(control->unit, unit);
		EXPECT_EQ(document["entablatureControls"][key]["readme"].get<std::string>(), section + place);
	}

	// Fronton (l. 608, 610, 611, 616) : six valeurs telles qu'ecrites, avec leur statut et leur lieu ; la sima est
	// manquante, sans valeur ni source.
	const MonumentPediment &f = d.pediment;
	const std::tuple<const char *, const MonumentValue *, double, MonumentStatus, const char *> pediment[] = {
		{"apexRiseFt", &f.apexRiseFt, 5.0, lu, "9, l. 608"},
		{"apexRiseIn", &f.apexRiseIn, 9.75, lu, "9, l. 608"},
		{"rakingProjectionFt", &f.rakingProjectionFt, 1.0, lu, "9, l. 611"},
		{"rakingProjectionIn", &f.rakingProjectionIn, 8.85, lu, "9, l. 611"},
		{"tympanumFace", &f.tympanumFace, 18.125, deduit, "9, l. 611"},
	};
	for (const auto &[key, v, written, status, place] : pediment)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(Value(*v), written);
		EXPECT_EQ(v->status, status);
		EXPECT_EQ(v->sources, s);
		EXPECT_EQ(v->readme, section + place);
	}
	EXPECT_EQ(f.sima.status, MonumentStatus::Missing);
	EXPECT_FALSE(f.sima.value.has_value());
	EXPECT_TRUE(f.sima.sources.empty());
	EXPECT_EQ(f.sima.readme, section + "9, l. 616");
	// Hauteurs de la corniche rampante (l. 610), de haut en bas, mots dans l'ordre (R5).
	using R = MonumentRakingMember;
	EXPECT_EQ(f.rakingHeights.values, (std::vector<double>{1.05, 0.35, 4.2, 1.45, 0.65}));
	EXPECT_EQ(f.rakingHeights.kinds, (std::vector<R>{R::Listel, R::Fillet, R::Corona, R::Drip, R::Fillet}));
	EXPECT_EQ(f.rakingHeights.status, lu);
	EXPECT_EQ(f.rakingHeights.sources, s);
	EXPECT_EQ(f.rakingHeights.readme, section + "9, l. 610");
	// Section (l. 611, R11) : 18 sommets dans l'ordre du README, mesures [S].
	EXPECT_EQ(f.rakingSection.u, (std::vector<double>{0, 0.85, 0.85, 2.2, 2.2, 11.2, 14.1, 15.9, 17.0, 17.6, 18.2, 18.8,
	                                                  19.36, 19.36, 19.65, 20.85, 20.85, 0}));
	EXPECT_EQ(f.rakingSection.h, (std::vector<double>{0, 0, 0.65, 0.65, 2.1, 2.1, 2.0, 1.67, 1.36, 1.16, 0.87, 0.35, 0.35,
	                                                  6.65, 7.3, 7.4, 7.7, 7.7}));
	EXPECT_EQ(f.rakingSection.status, mesure);
	EXPECT_EQ(f.rakingSection.sources, s);
	EXPECT_EQ(f.rakingSection.readme, section + "9, l. 611");
	// Pente 1 : r (R12) : lue [O] en tete de la cellule « Pente des rampants », avec son lieu.
	EXPECT_EQ(Value(f.rakingRun), 4.0846);
	EXPECT_EQ(f.rakingRun.status, lu);
	EXPECT_EQ(f.rakingRun.sources, (std::vector<std::string>{"O"}));
	EXPECT_EQ(f.rakingRun.readme, section + "9, l. 609");
	// Les treize controles du fronton, tels qu'ecrits, avec leur unite et leur lieu.
	const MonumentPedimentControls &fc = d.pedimentControls;
	const MonumentUnit deg = MonumentUnit::Degree;
	const std::tuple<const char *, const MonumentControlValue *, double, MonumentUnit, const char *> pedimentChecks[] = {
		{"apexRise", &fc.apexRise, 1.772, m, "9, l. 608"},
		{"rakingSlope", &fc.rakingSlope, 13.76, deg, "9, l. 609"},
		{"eaveHalfSpan", &fc.eaveHalfSpan, 7.238, m, "9, l. 609"},
		{"rakingHeight", &fc.rakingHeight, 7.70, in, "9, l. 610"},
		{"rakingHeightM", &fc.rakingHeightM, 0.196, m, "9, l. 610"},
		{"rakingVerticalHeight", &fc.rakingVerticalHeight, 7.93, in, "9, l. 610"},
		{"rakingVerticalHeightM", &fc.rakingVerticalHeightM, 0.201, m, "9, l. 610"},
		{"rakingProjection", &fc.rakingProjection, 0.530, m, "9, l. 611"},
		{"tympanumFaceM", &fc.tympanumFaceM, 0.460, m, "9, l. 611"},
		{"rakingListelProjection", &fc.rakingListelProjection, 20.85, in, "9, l. 611"},
		{"rakingCoronaProjection", &fc.rakingCoronaProjection, 19.36, in, "9, l. 611"},
		{"rakingBottomFilletProjection", &fc.rakingBottomFilletProjection, 0.85, in, "9, l. 611"},
		{"rakingCoronaGap", &fc.rakingCoronaGap, 0.11, in, "9, l. 611"},
	};
	for (const auto &[key, control, written, unit, place] : pedimentChecks)
	{
		SCOPED_TRACE(key);
		EXPECT_EQ(control->value, written);
		EXPECT_EQ(control->unit, unit);
		EXPECT_EQ(document["pedimentControls"][key]["readme"].get<std::string>(), section + place);
	}

	// Refus du lecteur sur la suite des hauteurs et sur le polygone.
	const std::string hp = "pediment.rakingHeights";
	const auto heights = [](json &doc) -> json & { return doc["pediment"]["rakingHeights"]; };
	ExpectEditRefused([&](json &doc) { heights(doc)["kinds"][3] = "bec"; }, hp + ".kinds[3]",
	                  "unknown raking member \"bec\"");
	ExpectEditRefused([&](json &doc) { heights(doc)["values"][2] = 0.0; }, hp + ".values[2]", "not a finite height > 0");
	ExpectEditRefused([&](json &doc) { heights(doc)["kinds"].erase(std::size_t{4}); }, hp + ".kinds",
	                  "not as many kinds as values");
	ExpectEditRefused([&](json &doc) { heights(doc)["status"] = "manquant"; heights(doc)["sources"] = json::array(); },
	                  hp + ".status", "a raking height series is never \"manquant\"");
	// Les mots d'une suite ne valent que pour elle : « bande » n'est pas un membre de la corniche rampante.
	ExpectEditRefused([&](json &doc) { heights(doc)["kinds"][0] = "bande"; }, hp + ".kinds[0]",
	                  "unknown raking member \"bande\"");
	const std::string sp = "pediment.rakingSection";
	const auto poly = [](json &doc) -> json & { return doc["pediment"]["rakingSection"]; };
	ExpectEditRefused([&](json &doc) { poly(doc)["h"].erase(std::size_t{17}); }, sp + ".h", "not as many h as u");
	ExpectEditRefused([&](json &doc) { poly(doc)["u"] = {0.0, 1.0}; poly(doc)["h"] = {0.0, 1.0}; }, sp + ".u",
	                  "fewer than 3 vertices");
	ExpectEditRefused([&](json &doc) { poly(doc)["u"] = json::array(); poly(doc)["h"] = json::array(); }, sp + ".u",
	                  "empty array");
	ExpectEditRefused([&](json &doc) { poly(doc)["h"][4] = -2.1; }, sp + ".h[4]", "not a finite coordinate >= 0");
	ExpectEditRefused([&](json &doc) { poly(doc)["u"][1] = "0,85"; }, sp + ".u[1]", "expected a number, not string");
	ExpectEditRefused([&](json &doc) { poly(doc)["status"] = "manquant"; poly(doc)["sources"] = json::array(); },
	                  sp + ".status", "a polygon is never \"manquant\"");
	ExpectEditRefused([&](json &doc) { poly(doc).erase("h"); }, sp + ".h", "required field is missing");
	ExpectEditRefused([&](json &doc) { poly(doc)["sources"] = json::array(); }, sp + ".sources",
	                  std::string("status \"") + kMeasured + "\" requires a source");
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
	// Architrave (l. 251-255) : face lisse, gouttes, regula, tenia = 2 ft 8,9 in.
	EXPECT_NEAR(Length(e.architraveFaceHeight) + Length(e.guttaeHeight) + Length(e.regulaHeight) +
	                Length(e.taeniaHeight),
	            architrave, kDecimalSumTolerance);
	// Corniche (l. 259 et 739) : cinq membres = 1 ft 0,45 in = 12,45 in.
	const double members = Length(e.mutulePlateHeight) + Length(e.dripHeight) + Length(e.coronaHeight) +
	                       Length(e.filletHeight) + Length(e.crownHeight);
	EXPECT_NEAR(members, cornice, kDecimalSumTolerance);
	EXPECT_NEAR(members, Metres(c.corniceSum), kDecimalSumTolerance);
	// Partage du chapiteau du triglyphe (l. 277) = sa hauteur (l. 257).
	EXPECT_NEAR(Length(e.triglyphCapitalBandHeight) + Length(e.triglyphCapitalListelHeight),
	            Length(e.triglyphCapitalHeight), kDecimalSumTolerance);
	// Divisions (l. 295 et 738) = largeur du triglyphe (l. 292) = 20,3 in.
	double divisions = 0.0;
	for (const double v : e.triglyphDivisions.values)
		divisions += MonumentMetres(v, e.triglyphDivisions.unit, "division");
	EXPECT_NEAR(divisions, FeetAndInches(e.triglyphWidthFt, e.triglyphWidthIn), kDecimalSumTolerance);
	EXPECT_NEAR(divisions, Metres(c.triglyphDivisionSum), kDecimalSumTolerance);
	// Ressaut de la metope (l. 278) : face du triglyphe moins nu des metopes = 2,8 in ; pas au bit
	// pres, 19,259 - 16,459 valant 2,8000000000000007 en double.
	EXPECT_NEAR(Length(e.triglyphFace) - Length(e.metopeFace), Metres(c.metopeRecess), kDecimalSumTolerance);

	// Confrontations aux nombres imprimes en metres, a la demi-unite.
	EXPECT_NEAR(architrave + frieze + cornice, Metres(c.total), kPrintedHalfUnit);   // l. 260, 1,979
	const double foot = Length(d.columns.height);                                     // l. 251, 5,715
	EXPECT_NEAR(foot + architrave, 6.551, kPrintedHalfUnit);                          // l. 251
	const double friezeTop = foot + architrave + frieze;
	EXPECT_NEAR(friezeTop, 7.377, kPrintedHalfUnit);                                  // l. 256
	// La somme des nombres en gras donnerait 7,378 ; la l. 256 l'ecarte, et le modele aussi.
	EXPECT_GT(std::fabs(friezeTop - 7.378), kPrintedHalfUnit);
	EXPECT_NEAR(friezeTop + cornice, 7.694, kPrintedHalfUnit);                        // l. 258
	RecordProperty("frieze_top_m", std::to_string(friezeTop));
	RecordProperty("entablature_height_gap_mm", std::to_string(1e3 * (architrave + frieze + cornice - Metres(c.total))));

	// Canaux (l. 296) : profondeur par l'angle droit contre la mesure, a +- 0,1 in ; fond des
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

	// Gouttes des regulae (l. 254, 319), recalculees depuis les valeurs lues, a kInchHalfUnit (resultats
	// imprimes a trois decimales du pouce) : reste de la largeur de la regula, celle du triglyphe (20,3 in),
	// apres six diametres et cinq vides ; marge de chaque bout, la moitie ; nombre.
	const double guttae = Value(e.regulaGuttaCount);
	const double shortfall = FeetAndInches(e.triglyphWidthFt, e.triglyphWidthIn) -
	                         (guttae * Length(e.regulaGuttaDiameter) + (guttae - 1.0) * Length(e.regulaGuttaGap));
	EXPECT_NEAR(shortfall, Metres(c.regulaGuttaShortfall), kInchHalfUnit);
	EXPECT_NEAR(shortfall / 2.0, Metres(c.regulaGuttaMargin), kInchHalfUnit);
	EXPECT_EQ(guttae, c.regulaGuttaCountCheck.value);
	// Dos des mutules (l. 316) : face de la plaque (l. 279) moins la profondeur, contre le dos imprime.
	const double back = Length(e.mutulePlateProjection) - Length(e.mutuleDepth);
	EXPECT_NEAR(back, Metres(c.mutuleBack), kInchHalfUnit);
	RecordProperty("regula_gutta_shortfall_in", std::to_string(shortfall / kMonumentInch));
	RecordProperty("mutule_back_in", std::to_string(back / kMonumentInch));

	// Gouttes des mutules (l. 317), recalculees depuis les valeurs lues, a kInchHalfUnit : axes des rangs du milieu
	// et de l'arriere depuis les diametres de tete et les vides ; pas en largeur, axes extremes a 1,775 in des bords,
	// par le diviseur lu ; marge arriere des tetes (l. 316) ; un rang par diametre ; gouttes par rang = diviseur + 1.
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
	// Forme (l. 317) : evasement en rayon, la moitie de l'evasement lu en diametre ; sa valeur non arrondie,
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

	// Fronton (l. 608 a 611, 616), recalcule depuis les valeurs lues. Hauteur (l. 608) : 5 ft 9,75 in contre 1,772 m.
	const MonumentPediment &f = d.pediment;
	const MonumentPedimentControls &fc = d.pedimentControls;
	EXPECT_NEAR(FeetAndInches(f.apexRiseFt, f.apexRiseIn), Metres(fc.apexRise), kPrintedHalfUnit);
	// Hauteurs des membres (l. 610) : leur somme est le total ecrit, 7,70 in, et la hauteur maximale du polygone de la
	// l. 611 ; 0,196 m a la demi-unite.
	const MonumentPolygon &section = f.rakingSection;
	ASSERT_EQ(section.u.size(), section.h.size());
	ASSERT_FALSE(section.u.empty());
	const auto metres = [&](double v) { return MonumentMetres(v, section.unit, "pediment.rakingSection"); };
	double heights = 0.0;
	std::vector<double> bounds;   // bornes cumulees des membres, de bas en haut
	for (auto it = f.rakingHeights.values.rbegin(); it != f.rakingHeights.values.rend(); ++it)
	{
		heights += MonumentMetres(*it, f.rakingHeights.unit, "pediment.rakingHeights");
		bounds.push_back(heights);
	}
	const double hMax = metres(*std::max_element(section.h.begin(), section.h.end()));
	const double uMax = metres(*std::max_element(section.u.begin(), section.u.end()));
	EXPECT_NEAR(heights, Metres(fc.rakingHeight), kDecimalSumTolerance);
	EXPECT_NEAR(heights, hMax, kDecimalSumTolerance);
	EXPECT_NEAR(heights, Metres(fc.rakingHeightM), kPrintedHalfUnit);
	const auto amongH = [&](double z) {
		return std::any_of(section.h.begin(), section.h.end(), [&](double h) { return std::fabs(metres(h) - z) <= kDecimalSumTolerance; });
	};
	const auto amongU = [&](double x) {
		return std::any_of(section.u.begin(), section.u.end(), [&](double u) { return std::fabs(metres(u) - x) <= kDecimalSumTolerance; });
	};
	// Bornes 0,65, 2,10, 6,65 et 7,70 in : des hauteurs du polygone ; 6,30 (dessus du larmier, filet au nu du
	// larmier) n'en est pas une (MS-103).
	ASSERT_EQ(bounds.size(), 5u);
	for (const std::size_t k : {std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{4}})
		EXPECT_TRUE(amongH(bounds[k])) << bounds[k] / kMonumentInch;
	EXPECT_FALSE(amongH(bounds[2])) << bounds[2] / kMonumentInch;
	// Saillies (l. 611) : u maximal = listel 20,85 in = 1 ft 8,85 in = 0,530 m ; larmier 19,36 in et filet du bas
	// 0,85 in sont des saillies du polygone.
	EXPECT_NEAR(uMax, Metres(fc.rakingListelProjection), kDecimalSumTolerance);
	EXPECT_NEAR(uMax, FeetAndInches(f.rakingProjectionFt, f.rakingProjectionIn), kDecimalSumTolerance);
	EXPECT_NEAR(uMax, Metres(fc.rakingProjection), kPrintedHalfUnit);
	EXPECT_TRUE(amongU(Metres(fc.rakingCoronaProjection)));
	EXPECT_TRUE(amongU(Metres(fc.rakingBottomFilletProjection)));
	// Nu du tympan (l. 611) : couronnement de la corniche horizontale (l. 284, 38,975 in) moins la saillie, 18,125 in ;
	// 0,460 m a la demi-unite ; ecart des larmiers 18,125 + 19,36 - 37,375 = 0,11 in, a la demi-unite du centieme.
	const double tympanum = Length(f.tympanumFace);
	EXPECT_NEAR(Length(e.crownProjection) - FeetAndInches(f.rakingProjectionFt, f.rakingProjectionIn), tympanum,
	            kDecimalSumTolerance);
	EXPECT_NEAR(tympanum, Metres(fc.tympanumFaceM), kPrintedHalfUnit);
	EXPECT_NEAR(tympanum + Metres(fc.rakingCoronaProjection) - Length(e.coronaProjection), Metres(fc.rakingCoronaGap),
	            kHundredthInchHalfUnit);
	RecordProperty("raking_heights_in", std::to_string(heights / kMonumentInch));
	RecordProperty("raking_corona_gap_in",
	               std::to_string((tympanum + Metres(fc.rakingCoronaProjection) - Length(e.coronaProjection)) / kMonumentInch));
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
	EXPECT_LT(s.bands[0].topProjection, s.bands[0].projection);   // face lisse inclinee vers l'axe (l. 271)
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

	// 68 triglyphes (l. 291), dont 4 pieces d'angle en L (EN14) ; par cote de n colonnes, 2n - 2
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
	// Metope lue (l. 293) : a MO1 de la metope courante de flanc, hors de MO1 en facade (l. 294).
	const double readMetope = FeetAndInches(e.metopeWidthFt, e.metopeWidthIn);
	EXPECT_LE(std::fabs(flankCurrent - readMetope), kReadMetopeTolerance);
	EXPECT_GT(std::fabs(frontCurrent - readMetope), kReadMetopeTolerance);
	RecordProperty("flank_read_metope_gap_mm", std::to_string(1e3 * (flankCurrent - readMetope)));
	RecordProperty("front_read_metope_gap_mm", std::to_string(1e3 * (frontCurrent - readMetope)));

	// Faces : l'anneau (8 triangles par sommet de section, 24 sommets), 64 prismes droits de 76 et
	// 4 pieces d'angle de 80 (4P - 4, P = 21), les ornements (96 992, J21) et le fronton (deux Lambda de
	// 6n - 4 = 104 triangles, n = 18, et le comble de 8, J23). Le fronton demande un entablement : le temple
	// nu n'a ni l'un ni l'autre.
	TempleSettings bare = t;
	bare.pediment.reset();
	bare.entablature.reset();
	std::size_t faces = 0, bareFaces = 0;
	ASSERT_NO_THROW(faces = CountTempleMeshFaces(t));
	ASSERT_NO_THROW(bareFaces = CountTempleMeshFaces(bare));
	EXPECT_EQ(faces - bareFaces, 8u * 24u + 64u * 76u + 4u * 80u + 96992u + 2u * 104u + 8u);
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

	// Premiere solution (l. 297-298), a la demi-unite des nombres imprimes.
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

	// Une regula par placement de triglyphe (l. 254), dont 4 d'angle ; 6 gouttes par regula droite et par
	// bras d'une regula d'angle (l. 254, 319) ; un mutule par triglyphe visible et par metope (l. 315),
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

	// Regula (l. 253, 271, 273) : du nu du haut de l'architrave (19,259 in) a la face lue (20,859 in),
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

	// Gouttes : avant du pied au nu de la regula, 20,859 in (l. 273, 274) ; avant de la tete contre 20,809 in
	// (l. 274), a la demi-unite ; marge de chaque bout d'une regula droite contre 0,127 in (l. 254).
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

	// Mutules (l. 279, 315, 316, 318) : dos a 20,420585 in sous la sous-face a 14 degres (D3-67), face de la
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

	// Cotes de la l. 318 depuis le dessus du stylobate, le pied de l'entablement a la hauteur de colonne (l. 251),
	// a la demi-unite : pivot (arete avant-basse du mutule), arete avant-haute, dessous au dos, haut du bec.
	const double foot = Length(d.columns.height);
	EXPECT_NEAR(foot + z[mb], Metres(c.soffitPivotZ), kPrintedHalfUnit);
	EXPECT_NEAR(foot + z[mb + 1], Metres(c.mutuleFrontTopZ), kPrintedHalfUnit);
	EXPECT_NEAR(foot + (z[mb] + mu.rise), Metres(c.soffitBackZ), kPrintedHalfUnit);
	EXPECT_NEAR(foot + z[mb + 2], Metres(c.dripTopZ), kPrintedHalfUnit);
	// Profondeur sous la pente (l. 316) : dos et projection horizontale a kInchHalfUnit ; montee (l. 318), imprimee
	// a deux decimales du pouce, a kHundredthInchHalfUnit.
	EXPECT_NEAR(back, Metres(c.mutuleInclinedBack), kInchHalfUnit);
	EXPECT_NEAR(mu.face - back, Metres(c.mutuleHorizontalDepth), kInchHalfUnit);
	EXPECT_NEAR(mu.rise, Metres(c.soffitRise), kHundredthInchHalfUnit);
	// Pivot sur la face de la plaque (l. 318) : un seul produit par le pouce des deux cotes.
	EXPECT_EQ(Metres(c.soffitPivotProjection), mu.face);
	// Ressaut vertical au dos (l. 318) : dans la plage de la montee a 14 +- 1 degres.
	const double depth = Length(e.mutuleDepth);
	EXPECT_GT(Metres(c.geisonBackStep), depth * std::sin(13.0 * kPi / 180.0));
	EXPECT_LT(Metres(c.geisonBackStep), depth * std::sin(15.0 * kPi / 180.0));
	RecordProperty("soffit_pivot_z_m", std::to_string(foot + z[mb]));
	RecordProperty("soffit_back_z_m", std::to_string(foot + (z[mb] + mu.rise)));

	// 2 520 gouttes, 18 par mutule (l. 317) ; pied plus large de 0,5 in en diametre que la tete, aux trois rangs :
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

	// Placements de la bibliotheque contre les controles de la l. 317, a kInchHalfUnit : axes des rangs du milieu et
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

	// Sommets maillés, en float : avant du pied de chaque goutte du rang avant contre 35,826 in (l. 317), a la
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

// J23
TEST(TEST_cgarchi_classical_monument, the_pediments_of_the_readme_rest_on_their_cornice)
{
	const MonumentData &d = HephaistosMonument();
	const MonumentPediment &f = d.pediment;
	const MonumentPedimentControls &c = d.pedimentControls;
	const TempleSettings t = MonumentTempleSettings(d);
	ASSERT_TRUE(t.entablature.has_value());
	ASSERT_TRUE(t.pediment.has_value());
	const GreekEntablatureSettings &e = *t.entablature;
	const GreekPedimentSettings &p = *t.pediment;

	// Reglages : un seul produit par grandeur (D3-67), au bit pres ; la section dans l'ordre du README, u depuis le nu
	// du tympan, z la hauteur normale (PE1), du sommet (0, 0) au sommet (0, 7,7 in).
	EXPECT_EQ(p.apexRise, (5.0 * 12.0 + 9.75) * kMonumentInch);
	EXPECT_EQ(p.tympanumFace, 18.125 * kMonumentInch);
	ASSERT_EQ(f.rakingSection.u.size(), 18u);
	ASSERT_EQ(p.rakingSection.size(), 18u);
	for (std::size_t k = 0; k < p.rakingSection.size(); ++k)
	{
		SCOPED_TRACE(k);
		EXPECT_EQ(p.rakingSection[k].u, f.rakingSection.u[k] * kMonumentInch);
		EXPECT_EQ(p.rakingSection[k].z, f.rakingSection.h[k] * kMonumentInch);
	}
	EXPECT_EQ(p.rakingSection.front().u, 0.0);
	EXPECT_EQ(p.rakingSection.front().z, 0.0);
	EXPECT_EQ(p.rakingSection.back().u, 0.0);
	EXPECT_EQ(p.rakingSection.back().z, 7.7 * kMonumentInch);
	EXPECT_EQ(p.rakingRun, 4.0846);

	// Le temple, et le meme sans fronton : 2 x 104 + 8 = 216 faces de plus, trois parties apres les mutules.
	TempleLayout l;
	std::vector<TempleMeshPart> parts;
	std::unique_ptr<Mesh> m;
	ASSERT_NO_THROW(m = BuildTempleMesh(t, &l, &parts));
	TempleSettings plain = t;
	plain.pediment.reset();
	const std::size_t with = CountTempleMeshFaces(t), without = CountTempleMeshFaces(plain);
	EXPECT_EQ(with, static_cast<std::size_t>(m->GetNFaces()));
	EXPECT_EQ(with - without, 2u * 104u + 8u);
	RecordProperty("pediment_faces", std::to_string(with - without));
	ASSERT_GE(parts.size(), 4u);
	const std::size_t roof = parts.size() - 1, back = roof - 1, front = roof - 2;
	EXPECT_EQ(parts[front].address.element, TempleElement::RakingCornice);
	EXPECT_EQ(parts[front].address.side, PeristyleSide::Front);
	EXPECT_EQ(parts[back].address.element, TempleElement::RakingCornice);
	EXPECT_EQ(parts[back].address.side, PeristyleSide::Back);
	EXPECT_EQ(parts[roof].address.element, TempleElement::Roof);
	for (const std::size_t k : {front, back})
	{
		EXPECT_EQ(parts[k].vertexCount, 54u) << k;   // 3n
		EXPECT_EQ(parts[k].faceCount, 104u) << k;    // 6n - 4
	}
	EXPECT_EQ(parts[roof].vertexCount, 6u);
	EXPECT_EQ(parts[roof].faceCount, 8u);

	// Pente 1 : r (PE2') : atan (1 / r) contre 13,76°, a la demi-unite du centieme de degre imprime.
	constexpr double kHundredthDegreeHalfUnit = 0.005;
	ASSERT_EQ(c.rakingSlope.unit, MonumentUnit::Degree);
	const double slope = std::atan(1.0 / p.rakingRun) * 180.0 / kPi;
	EXPECT_NEAR(slope, c.rakingSlope.value, kHundredthDegreeHalfUnit);
	RecordProperty("raking_slope_deg", std::to_string(slope));

	// V = T s, s = hypot (r, 1) / r, hauteur verticale de la corniche rampante, en double : contre 7,93 in et
	// 0,201 m, a leur demi-unite.
	const double s = std::hypot(p.rakingRun, 1.0) / p.rakingRun;
	const double rise = p.rakingSection.back().z * s;
	EXPECT_NEAR(rise, Metres(c.rakingVerticalHeight), kHundredthInchHalfUnit);
	EXPECT_NEAR(rise, Metres(c.rakingVerticalHeightM), kPrintedHalfUnit);
	RecordProperty("raking_vertical_height_in", std::to_string(rise / kMonumentInch));

	// Au maillage, sorties de la bibliotheque seulement. Dessus de l'anneau lu dans l'anneau.
	std::size_t ring = parts.size();
	for (std::size_t k = 0; k < parts.size(); ++k)
		if (parts[k].address.element == TempleElement::Entablature)
			ring = k;
	ASSERT_LT(ring, parts.size());
	const float zTop = static_cast<float>(l.entablatureFoot + GreekEntablatureBandBounds(e).back());
	float ringX = 0.0f, ringY = 0.0f, ringTop = std::numeric_limits<float>::lowest();
	for (unsigned v = parts[ring].firstVertex; v < parts[ring].firstVertex + parts[ring].vertexCount; ++v)
	{
		float q[3];
		m->GetVertex(v, q);
		ringTop = std::max(ringTop, q[2]);
		if (q[2] == zTop)
		{
			ringX = std::max(ringX, std::fabs(q[0]));
			ringY = std::max(ringY, std::fabs(q[1]));
		}
	}
	EXPECT_EQ(ringTop, zTop);
	const double xEave = l.peristyle.halfWidth + e.bands.back().topProjection;
	EXPECT_EQ(ringX, static_cast<float>(xEave));

	// Comble : pieds du haut xTop (base hors de l'axe) et faite (dans l'axe). Coherence du README : r x 1,772
	// contre 7,238, a la demi-unite imprimee. Sortie du modele : xTop contre r x apexRise (cote contractable, a
	// 1e-6) ; xTop <= X_e. Haut au faite, au-dessus du dessus du stylobate, contre z = 9,466 : demi-unites de 7,694 et
	// de 1,772 (MO1).
	constexpr double kApexHeightTolerance = 2.0 * kPrintedHalfUnit;
	float roofBase = std::numeric_limits<float>::max(), xTop = 0.0f, ridge = std::numeric_limits<float>::lowest();
	for (unsigned v = parts[roof].firstVertex; v < parts[roof].firstVertex + parts[roof].vertexCount; ++v)
	{
		float q[3];
		m->GetVertex(v, q);
		roofBase = std::min(roofBase, q[2]);
		ridge = std::max(ridge, q[2]);
		xTop = std::max(xTop, std::fabs(q[0]));
	}
	EXPECT_EQ(roofBase, zTop);
	EXPECT_NEAR(p.rakingRun * Metres(c.apexRise), Metres(c.eaveHalfSpan), kPrintedHalfUnit);
	EXPECT_NEAR(xTop, p.rakingRun * p.apexRise, 1e-6);
	EXPECT_LE(xTop, ringX);
	const double apex = static_cast<double>(ridge) - l.krepis.stylobateTop;
	EXPECT_NEAR(apex, 9.466, kApexHeightTolerance);
	RecordProperty("eave_half_span_m", std::to_string(xTop));
	RecordProperty("apex_z_m", std::to_string(apex));

	// Faces de pied (PE3', L18) : les 2n sommets de pied de chaque Lambda a la cote du dessus de l'anneau, au bit
	// pres, dans l'emprise de son dessus ; en y, le listel (18,125 + 20,85 = 38,975 in) affleure le couronnement, a
	// un ulp float pres.
	const std::size_t n = p.rakingSection.size();
	const float yLimit = std::nextafter(ringY, std::numeric_limits<float>::infinity());
	float yFoot = 0.0f;
	for (const std::size_t k : {front, back})
	{
		std::size_t feet = 0;
		for (unsigned v = parts[k].firstVertex; v < parts[k].firstVertex + parts[k].vertexCount; ++v)
		{
			float q[3];
			m->GetVertex(v, q);
			EXPECT_GE(q[2], zTop);
			if (q[2] != zTop)
				continue;
			++feet;
			EXPECT_LE(std::fabs(q[0]), xTop);
			EXPECT_LE(std::fabs(q[1]), yLimit);
			yFoot = std::max(yFoot, std::fabs(q[1]));
		}
		EXPECT_EQ(feet, 2u * n) << k;
	}
	RecordProperty("listel_minus_crown_ulp", std::to_string(yFoot == ringY ? 0 : (yFoot > ringY ? 1 : -1)));

	// Sous-assemblage anneau + fronton, tolerance par defaut (TE5) : recouvrements L16 (comble sur l'anneau), L17
	// (dos de chaque Lambda sur l'about du comble) et L18 (faces de pied sur l'anneau) seuls, chacun present ;
	// 4 aretes non manifold, aucun sommet non manifold, 2 composantes, aucun autre defaut.
	std::vector<float> vertices;
	std::vector<unsigned> faces;
	std::vector<std::size_t> owner;
	for (const std::size_t k : {ring, front, back, roof})
	{
		const TempleMeshPart &part = parts[k];
		const unsigned base = static_cast<unsigned>(vertices.size() / 3);
		vertices.insert(vertices.end(), m->GetVertices().begin() + 3 * part.firstVertex,
		                m->GetVertices().begin() + 3 * (part.firstVertex + part.vertexCount));
		for (unsigned face = part.firstFace; face < part.firstFace + part.faceCount; ++face)
		{
			for (unsigned j = 0; j < 3; ++j)
				faces.push_back(base + static_cast<unsigned>(m->GetFaceVertex(face, j)) - part.firstVertex);
			owner.push_back(k);
		}
	}
	Mesh sub;
	sub.SetVertices(static_cast<unsigned>(vertices.size() / 3), vertices.data());
	sub.SetFaces(static_cast<unsigned>(faces.size() / 3), 3, faces.data());
	MeshValidationOptions options;
	options.maxRecordsPerKind = sub.GetNFaces();
	const MeshValidationReport r = ValidateMesh(sub, options);
	ASSERT_TRUE(r.selfIntersectionChecked);
	ASSERT_FALSE(r.selfIntersectionTruncated);
	const float yTympanum = static_cast<float>(l.peristyle.halfDepth + p.tympanumFace);
	const auto allAt = [&](unsigned face, int axis, float value) {
		for (unsigned j = 0; j < 3; ++j)
		{
			float q[3];
			sub.GetVertex(static_cast<unsigned>(sub.GetFaceVertex(face, j)), q);
			if (q[axis] != value)
				return false;
		}
		return true;
	};
	std::size_t l16 = 0, l17Front = 0, l17Back = 0, l18Front = 0, l18Back = 0, records = 0;
	for (const MeshDefectRecord &rec : r.records)
	{
		if (rec.kind != MeshDefect::SelfIntersection)
			continue;
		++records;
		ASSERT_EQ(rec.faces.size(), 2u);
		unsigned f0 = rec.faces[0], f1 = rec.faces[1];
		if (owner[f0] > owner[f1])
			std::swap(f0, f1);
		const std::size_t p0 = owner[f0], p1 = owner[f1];
		if (p0 == ring && p1 == roof && allAt(f0, 2, zTop) && allAt(f1, 2, zTop))
			++l16;
		else if (p0 == front && p1 == roof && allAt(f0, 1, -yTympanum) && allAt(f1, 1, -yTympanum))
			++l17Front;
		else if (p0 == back && p1 == roof && allAt(f0, 1, yTympanum) && allAt(f1, 1, yTympanum))
			++l17Back;
		else if (p0 == ring && (p1 == front || p1 == back) && allAt(f0, 2, zTop) && allAt(f1, 2, zTop))
			++(p1 == front ? l18Front : l18Back);
		else
			ADD_FAILURE() << "recouvrement hors liste : parties " << p0 << " et " << p1 << ", faces " << f0 << " et " << f1;
	}
	EXPECT_EQ(records, r.Count(MeshDefect::SelfIntersection));
	EXPECT_GE(l16, 1u) << "L16 absente";
	EXPECT_GE(l17Front, 1u) << "L17 absente en facade Front";
	EXPECT_GE(l17Back, 1u) << "L17 absente en facade Back";
	EXPECT_GE(l18Front, 2u) << "L18 absente d'un bras en facade Front";
	EXPECT_GE(l18Back, 2u) << "L18 absente d'un bras en facade Back";
	EXPECT_EQ(r.Count(MeshDefect::NonManifoldEdge), 4u);
	EXPECT_EQ(r.Count(MeshDefect::NonManifoldVertex), 0u);
	EXPECT_EQ(r.components, 2u);
	for (std::size_t kind = 0; kind < kMeshDefectCount; ++kind)
		if (static_cast<MeshDefect>(kind) != MeshDefect::SelfIntersection &&
		    static_cast<MeshDefect>(kind) != MeshDefect::NonManifoldEdge &&
		    static_cast<MeshDefect>(kind) != MeshDefect::NonManifoldVertex)
			EXPECT_EQ(r.counts[kind], 0u) << MeshDefectName(static_cast<MeshDefect>(kind));
	RecordProperty("l16_records", std::to_string(l16));
	RecordProperty("l17_records", std::to_string(l17Front + l17Back));
	RecordProperty("l18_records", std::to_string(l18Front + l18Back));

	// Refus de la conversion, nommes : une sima donnee (PE4 ne vaut que manquante), une section de listes inegales.
	const auto refusalOf = [](const MonumentData &data) {
		try
		{
			MonumentPedimentSettings(data);
		}
		catch (const std::invalid_argument &x)
		{
			return std::string(x.what());
		}
		return std::string();
	};
	MonumentData sima = d;
	sima.pediment.sima.value = 0.2;
	sima.pediment.sima.status = MonumentStatus::Read;
	sima.pediment.sima.sources = {"S"};
	EXPECT_EQ(refusalOf(sima).rfind("Monument : 'pediment.sima'", 0), 0u) << refusalOf(sima);
	MonumentData uneven = d;
	uneven.pediment.rakingSection.h.pop_back();
	EXPECT_EQ(refusalOf(uneven).rfind("Monument : 'pediment.rakingSection'", 0), 0u) << refusalOf(uneven);
	MonumentData noHeight = d;
	noHeight.pediment.apexRiseIn.value.reset();
	noHeight.pediment.apexRiseIn.status = MonumentStatus::Missing;
	noHeight.pediment.apexRiseIn.sources.clear();
	EXPECT_NE(refusalOf(noHeight).find("'pediment.apexRiseIn'"), std::string::npos) << refusalOf(noHeight);
	EXPECT_EQ(refusalOf(d), "");
}

// ===========================================================================
//  Mode partiel (registre MO-P1 a MO-P5) : cles consommees, aiguillage, Ares
// ===========================================================================

namespace {

constexpr std::array<MonumentPart, 5> kParts = {MonumentPart::Krepis, MonumentPart::Peristyle, MonumentPart::Columns,
                                                MonumentPart::Entablature, MonumentPart::Pediment};

MonumentPart PartOfKey (const std::string &key)
{
	const std::string block = key.substr(0, key.find('.'));
	if (block == "krepis")      return MonumentPart::Krepis;
	if (block == "peristyle")   return MonumentPart::Peristyle;
	if (block == "columns")     return MonumentPart::Columns;
	if (block == "entablature") return MonumentPart::Entablature;
	if (block == "pediment")    return MonumentPart::Pediment;
	throw std::invalid_argument("cle hors des parties : " + key);
}

// Reglages de la conversion de `part`, aplatis en nombres dans un ordre fixe : deux conversions sont
// identiques si et seulement si leurs suites le sont (au bit pres, aucun NaN).
std::vector<double> ConvertedPart (const MonumentData &d, MonumentPart part)
{
	std::vector<double> out;
	switch (part)
	{
	case MonumentPart::Krepis:
	{
		const KrepisSettings s = MonumentKrepisSettings(d);
		out = {s.stylobateHalfWidth, s.stylobateHalfDepth, s.stylobateHeight};
		for (const KrepisStep &k : s.lowerSteps)
			out.insert(out.end(), {k.height, k.outsetX, k.outsetY});
		break;
	}
	case MonumentPart::Peristyle:
	{
		const PeristyleSettings s = MonumentPeristyleSettings(d);
		out = {double(s.front), double(s.flank), s.frontSpacing, s.flankSpacing, s.frontCornerSpacing,
		       s.flankCornerSpacing};
		break;
	}
	case MonumentPart::Columns:
		// La colonne courante ne lit pas cornerLowerDiameter : les deux jeux sont convertis. Seuls les refus
		// sont eprouves pour la colonne, aucune de ses cles hors table n'etant renseignee.
		MonumentColumnSettings(d, false);
		MonumentColumnSettings(d, true);
		break;
	case MonumentPart::Entablature:
	{
		const GreekEntablatureSettings s = MonumentEntablatureSettings(d);
		for (const EntablatureBand &b : s.bands)
			out.insert(out.end(), {b.height, b.projection, b.topProjection});
		out.insert(out.end(), {s.innerFace, double(s.friezeBand), double(static_cast<int>(s.corner)), s.triglyph.width,
		                       s.triglyph.face, s.triglyph.channelDepth, s.triglyph.halfChannelDepth});
		out.insert(out.end(), s.triglyph.divisionWidths.begin(), s.triglyph.divisionWidths.end());
		for (bool c : s.triglyph.divisionIsChannel)
			out.push_back(c ? 1.0 : 0.0);
		out.push_back(s.regulae ? 1.0 : 0.0);
		if (s.regulae)
			out.insert(out.end(), {s.regulae->height, s.regulae->face, double(s.regulae->guttaCount), s.regulae->guttaGap,
			                       s.regulae->gutta.footDiameter, s.regulae->gutta.footProjection,
			                       s.regulae->gutta.topProjection, double(s.regulae->gutta.sides)});
		out.push_back(s.mutules ? 1.0 : 0.0);
		if (s.mutules)
		{
			out.insert(out.end(), {double(s.mutules->band), s.mutules->width, s.mutules->face, s.mutules->rise,
			                       s.mutules->guttae ? 1.0 : 0.0});
			if (const auto &g = s.mutules->guttae)
			{
				out.insert(out.end(), g->rowDiameters.begin(), g->rowDiameters.end());
				out.insert(out.end(), g->rowGaps.begin(), g->rowGaps.end());
				out.insert(out.end(), {double(g->perRow), g->edgeAxis, g->height, g->conicity, double(g->sides)});
			}
		}
		break;
	}
	case MonumentPart::Pediment:
	{
		const GreekPedimentSettings s = MonumentPedimentSettings(d);
		out = {s.apexRise, s.rakingRun, s.tympanumFace};
		for (const RingSectionPoint &p : s.rakingSection)
			out.insert(out.end(), {p.u, p.z});
		break;
	}
	}
	return out;
}

// Message du refus de la conversion de `part`, ou "" si elle reussit.
std::string ConversionRefusal (const MonumentData &d, MonumentPart part)
{
	try
	{
		ConvertedPart(d, part);
	}
	catch (const std::invalid_argument &e)
	{
		return e.what();
	}
	return std::string();
}

// Retire de la table des sources celles qu'aucune valeur ne cite plus.
void PruneSources (json &doc)
{
	std::set<std::string> cited;
	const std::function<void (const json &)> walk = [&](const json &v) {
		if (!v.is_object())
			return;
		for (auto it = v.begin(); it != v.end(); ++it)
		{
			if (it.key() == "sources" && it.value().is_array())
				for (const json &s : it.value())
					cited.insert(s.get<std::string>());
			else
				walk(it.value());
		}
	};
	for (auto it = doc.begin(); it != doc.end(); ++it)
		if (it.key() != "sources")
			walk(it.value());
	json kept = json::object();
	for (auto it = doc["sources"].begin(); it != doc["sources"].end(); ++it)
		if (cited.count(it.key()))
			kept[it.key()] = it.value();
	doc["sources"] = kept;
}

// Document embarque d'Hephaistos, la valeur `key` ("bloc.cle") mise a manquant.
MonumentData WithMissing (const std::string &key)
{
	json doc = EmbeddedDocument();
	json &v = doc[key.substr(0, key.find('.'))][key.substr(key.find('.') + 1)];
	v.erase("value");
	v["status"] = "manquant";
	v["sources"] = json::array();
	PruneSources(doc);
	return ParseMonument(doc.dump());
}

const MonumentPartReport &ReportOf (const MonumentBuild &b, MonumentPart part)
{
	return b.report[static_cast<std::size_t>(part)];
}

} // namespace

// P1 (lot B2) : la table des cles consommees est exacte dans les deux sens. Cle listee mise a manquant : la
// partie est incomplete de cette seule cle, et sa conversion la nomme en refusant ; cle non listee mise a
// manquant : reglages identiques au bit pres.
TEST(TEST_cgarchi_classical_monument, the_consumed_keys_of_each_part_are_exactly_those_its_conversion_reads)
{
	const MonumentData &heph = HephaistosMonument();
	std::map<MonumentPart, std::vector<double>> reference;
	std::set<std::string> consumed;
	const std::array<std::size_t, 5> counts = {10u, 6u, 21u, 48u, 5u};
	for (MonumentPart part : kParts)
	{
		// Bloc absent : toutes les cles consommees manquent.
		const std::vector<std::string> all = MissingMonumentKeys(MonumentData{}, part);
		EXPECT_EQ(all.size(), counts[static_cast<std::size_t>(part)]);
		consumed.insert(all.begin(), all.end());
		EXPECT_TRUE(MissingMonumentKeys(heph, part).empty());
		reference[part] = ConvertedPart(heph, part);
	}

	int listed = 0, unlisted = 0;
	for (const auto &[key, value] : AllValues(heph))
	{
		if (value->status == MonumentStatus::Missing)
			continue;   // gardes GD3d, EN1, PE4 : manquantes au document, hors table
		SCOPED_TRACE(key);
		const MonumentPart part = PartOfKey(key);
		const MonumentData d = WithMissing(key);
		if (consumed.count(key))
		{
			++listed;
			EXPECT_EQ(MissingMonumentKeys(d, part), std::vector<std::string>{key});
			const std::string refusal = ConversionRefusal(d, part);
			EXPECT_NE(refusal.find("'" + key + "'"), std::string::npos) << refusal;
		}
		else
		{
			++unlisted;
			EXPECT_TRUE(MissingMonumentKeys(d, part).empty());
			const std::string refusal = ConversionRefusal(d, part);
			EXPECT_EQ(refusal, "");
			if (refusal.empty())
				EXPECT_TRUE(ConvertedPart(d, part) == reference[part]);
		}
	}
	// 90 cles consommees moins la suite et le polygone ; non listees et renseignees : cornerMargin, 7 de
	// l'entablement, 2 du fronton.
	EXPECT_EQ(listed, 88);
	EXPECT_EQ(unlisted, 10);

	// Suite et polygone, jamais manquants au document : rendus manquants dans le C++.
	MonumentData noDivisions = heph;
	noDivisions.entablature.triglyphDivisions.status = MonumentStatus::Missing;
	noDivisions.entablature.triglyphDivisions.values.clear();
	noDivisions.entablature.triglyphDivisions.kinds.clear();
	EXPECT_EQ(MissingMonumentKeys(noDivisions, MonumentPart::Entablature),
	          std::vector<std::string>{"entablature.triglyphDivisions"});
	EXPECT_NE(ConversionRefusal(noDivisions, MonumentPart::Entablature).find("'entablature.triglyphDivisions'"),
	          std::string::npos);
	MonumentData noSection = heph;
	noSection.pediment.rakingSection.status = MonumentStatus::Missing;
	noSection.pediment.rakingSection.u.clear();
	noSection.pediment.rakingSection.h.clear();
	EXPECT_EQ(MissingMonumentKeys(noSection, MonumentPart::Pediment), std::vector<std::string>{"pediment.rakingSection"});
	EXPECT_NE(ConversionRefusal(noSection, MonumentPart::Pediment).find("'pediment.rakingSection'"), std::string::npos);
}

// P2 (lot B3) : aiguillage. Tout complet : exactement BuildTempleMesh (MonumentTempleSettings). Hauteur de colonne
// manquante : la krepis seule, prefixe du temple (positions comparees en float, -0.0f et +0.0 confondus, RP1).
TEST(TEST_cgarchi_classical_monument, the_monument_build_is_the_temple_mesh_or_its_krepis_prefix)
{
	const MonumentData &heph = HephaistosMonument();
	std::vector<TempleMeshPart> parts;
	const std::unique_ptr<Mesh> temple = BuildTempleMesh(MonumentTempleSettings(heph), nullptr, &parts);
	const MonumentBuild all = BuildMonumentMesh(heph);
	ASSERT_TRUE(all.mesh);
	EXPECT_EQ(all.mesh->GetNVertices(), 514846);
	EXPECT_EQ(all.mesh->GetNFaces(), 1016552u);
	ASSERT_EQ(all.mesh->GetNVertices(), temple->GetNVertices());
	ASSERT_EQ(all.mesh->GetNFaces(), temple->GetNFaces());
	EXPECT_EQ(std::memcmp(all.mesh->GetVertices().data(), temple->GetVertices().data(),
	                      temple->GetVertices().size() * sizeof(float)), 0);
	bool sameFaces = true;
	for (unsigned f = 0; sameFaces && f < temple->GetNFaces(); ++f)
		for (unsigned j = 0; j < 3; ++j)
			sameFaces = sameFaces && all.mesh->GetFaceVertex(f, j) == temple->GetFaceVertex(f, j);
	EXPECT_TRUE(sameFaces);
	ASSERT_EQ(all.parts.size(), parts.size());
	for (MonumentPart part : kParts)
	{
		EXPECT_EQ(ReportOf(all, part).state, MonumentPartState::Built) << static_cast<int>(part);
		EXPECT_TRUE(ReportOf(all, part).missingKeys.empty());
	}

	MonumentData noHeight = heph;
	noHeight.columns.height.value.reset();
	noHeight.columns.height.status = MonumentStatus::Missing;
	noHeight.columns.height.sources.clear();
	const MonumentBuild krepis = BuildMonumentMesh(noHeight);
	ASSERT_TRUE(krepis.mesh);
	EXPECT_EQ(krepis.mesh->GetNVertices(), 64);
	EXPECT_EQ(krepis.mesh->GetNFaces(), 48u);
	const std::vector<float> &kv = krepis.mesh->GetVertices();
	const std::vector<float> &tv = temple->GetVertices();
	ASSERT_LE(kv.size(), tv.size());
	for (std::size_t i = 0; i < kv.size(); ++i)
		EXPECT_TRUE(kv[i] == tv[i]) << i << " : " << kv[i] << " / " << tv[i];
	for (unsigned f = 0; f < krepis.mesh->GetNFaces(); ++f)
		for (unsigned j = 0; j < 3; ++j)
			EXPECT_EQ(krepis.mesh->GetFaceVertex(f, j), temple->GetFaceVertex(f, j));
	ASSERT_EQ(krepis.parts.size(), 4u);
	for (std::size_t k = 0; k < 4; ++k)
	{
		EXPECT_EQ(krepis.parts[k].address.element, TempleElement::KrepisStep);
		EXPECT_EQ(krepis.parts[k].address.index, parts[k].address.index);
		EXPECT_EQ(krepis.parts[k].firstVertex, parts[k].firstVertex);
		EXPECT_EQ(krepis.parts[k].vertexCount, parts[k].vertexCount);
		EXPECT_EQ(krepis.parts[k].firstFace, parts[k].firstFace);
		EXPECT_EQ(krepis.parts[k].faceCount, parts[k].faceCount);
	}
	EXPECT_EQ(ReportOf(krepis, MonumentPart::Krepis).state, MonumentPartState::Built);
	EXPECT_EQ(ReportOf(krepis, MonumentPart::Peristyle).state, MonumentPartState::CompleteNotBuilt);
	EXPECT_EQ(ReportOf(krepis, MonumentPart::Columns).state, MonumentPartState::Incomplete);
	EXPECT_EQ(ReportOf(krepis, MonumentPart::Columns).missingKeys, std::vector<std::string>{"columns.height"});
	for (MonumentPart part : {MonumentPart::Entablature, MonumentPart::Pediment})
	{
		EXPECT_EQ(ReportOf(krepis, part).state, MonumentPartState::Blocked);
		EXPECT_TRUE(ReportOf(krepis, part).blockedBy == std::optional<MonumentPart>(MonumentPart::Columns));
	}

	// Colonnes construites, entablement ou fronton non constructible : BuildTempleMesh des reglages complets
	// prives des parties sautees, au bit pres (meme chemin).
	const auto withMissing = [&](auto member) {
		MonumentData d = heph;
		MonumentValue &v = member(d);
		v.value.reset();
		v.status = MonumentStatus::Missing;
		v.sources.clear();
		return d;
	};
	const auto expectTempleOf = [](const MonumentBuild &b, const TempleSettings &settings) {
		const std::unique_ptr<Mesh> ref = BuildTempleMesh(settings);
		ASSERT_TRUE(b.mesh);
		ASSERT_EQ(b.mesh->GetNVertices(), ref->GetNVertices());
		ASSERT_EQ(b.mesh->GetNFaces(), ref->GetNFaces());
		EXPECT_EQ(std::memcmp(b.mesh->GetVertices().data(), ref->GetVertices().data(),
		                      ref->GetVertices().size() * sizeof(float)), 0);
		bool same = true;
		for (unsigned f = 0; same && f < ref->GetNFaces(); ++f)
			for (unsigned j = 0; j < 3; ++j)
				same = same && b.mesh->GetFaceVertex(f, j) == ref->GetFaceVertex(f, j);
		EXPECT_TRUE(same);
		RecordProperty("faces", std::to_string(ref->GetNFaces()));
	};
	TempleSettings noPedimentSettings = MonumentTempleSettings(heph);
	noPedimentSettings.pediment.reset();
	TempleSettings columnsSettings = noPedimentSettings;
	columnsSettings.entablature.reset();

	const MonumentBuild noTympanum =
		BuildMonumentMesh(withMissing([](MonumentData &d) -> MonumentValue & { return d.pediment.tympanumFace; }));
	expectTempleOf(noTympanum, noPedimentSettings);
	EXPECT_LT(noTympanum.mesh->GetNFaces(), temple->GetNFaces());
	EXPECT_EQ(ReportOf(noTympanum, MonumentPart::Pediment).state, MonumentPartState::Incomplete);
	EXPECT_EQ(ReportOf(noTympanum, MonumentPart::Pediment).missingKeys, std::vector<std::string>{"pediment.tympanumFace"});
	for (MonumentPart part : {MonumentPart::Krepis, MonumentPart::Peristyle, MonumentPart::Columns, MonumentPart::Entablature})
		EXPECT_EQ(ReportOf(noTympanum, part).state, MonumentPartState::Built) << static_cast<int>(part);

	const MonumentBuild noDrip =
		BuildMonumentMesh(withMissing([](MonumentData &d) -> MonumentValue & { return d.entablature.dripHeight; }));
	expectTempleOf(noDrip, columnsSettings);
	EXPECT_LT(noDrip.mesh->GetNFaces(), noTympanum.mesh->GetNFaces());
	EXPECT_EQ(ReportOf(noDrip, MonumentPart::Entablature).state, MonumentPartState::Incomplete);
	EXPECT_EQ(ReportOf(noDrip, MonumentPart::Entablature).missingKeys,
	          std::vector<std::string>{"entablature.dripHeight"});
	EXPECT_EQ(ReportOf(noDrip, MonumentPart::Pediment).state, MonumentPartState::Blocked);
	EXPECT_TRUE(ReportOf(noDrip, MonumentPart::Pediment).blockedBy ==
	            std::optional<MonumentPart>(MonumentPart::Entablature));
	for (MonumentPart part : {MonumentPart::Krepis, MonumentPart::Peristyle, MonumentPart::Columns})
		EXPECT_EQ(ReportOf(noDrip, part).state, MonumentPartState::Built) << static_cast<int>(part);
	std::printf("faces : complet %u, sans fronton %u, sans entablement %u\n", temple->GetNFaces(),
	            noTympanum.mesh->GetNFaces(), noDrip.mesh->GetNFaces());
}

// P4 (revue des lots B) : le document d'Ares, a blocs absents, respecte le schema v9 ; dependentRequired lie
// chaque bloc de controle a sa partie, et le lecteur refuse ce que le schema refuse.
TEST(TEST_cgarchi_classical_monument, the_ares_document_conforms_to_the_schema_file_and_its_block_dependencies)
{
	const json schema = SchemaFile();
	ASSERT_TRUE(schema.is_object()) << "src/cgarchi/schemas/monument-v9.schema illisible depuis le repertoire courant";
	const std::string_view text = cgarchi::TempleAresJson();
	const json ares = json::parse(text.begin(), text.end());
	EXPECT_EQ(Violation(ares, schema, schema, std::string()), "");
	EXPECT_EQ(RefusalOf(ares.dump()), "");

	json noControls = ares;
	noControls.erase("controls");
	EXPECT_EQ(Violation(noControls, schema, schema, std::string()), "controls : requis par peristyle");
	EXPECT_NE(RefusalOf(noControls.dump()).find(std::string(kMonumentSchema) + " controls : required field is missing"),
	          std::string::npos);
	json noKrepis = ares;
	noKrepis.erase("krepis");
	EXPECT_EQ(Violation(noKrepis, schema, schema, std::string()), "krepis : requis par krepisControls");
	json heph = EmbeddedDocument();
	heph.erase("columns");
	EXPECT_EQ(Violation(heph, schema, schema, std::string()), "columns : requis par columnControls");
}

// P3 (lot B4) : le temple d'Ares, krepis seule. Emprise de l'euthynteria et hauteur de la krepis du README
// (sections 3.3 et 12), axes du peristyle de la section 4.3, au millimetre imprime.
TEST(TEST_cgarchi_classical_monument, the_temple_of_ares_builds_its_krepis_and_validates_its_peristyle)
{
	const MonumentData &ares = EmbeddedMonument("temple-ares");
	EXPECT_EQ(ares.monument, "temple-ares");
	EXPECT_EQ(ares.source.derivedBy, "src/cgarchi/data/temple_ares_from_readme.py");
	EXPECT_TRUE(ares.present.krepis && ares.present.peristyle && ares.present.controls && ares.present.krepisControls);
	EXPECT_FALSE(ares.present.columns || ares.present.entablature || ares.present.pediment);
	EXPECT_THROW(EmbeddedMonument("temple-zeus"), std::invalid_argument);
	EXPECT_EQ(&EmbeddedMonument("temple-hephaistos"), &HephaistosMonument());

	const MonumentBuild b = BuildMonumentMesh(ares);
	ASSERT_TRUE(b.mesh);
	EXPECT_EQ(b.mesh->GetNVertices(), 64);
	EXPECT_EQ(b.mesh->GetNFaces(), 48u);
	EXPECT_EQ(b.parts.size(), 4u);
	EXPECT_EQ(ReportOf(b, MonumentPart::Krepis).state, MonumentPartState::Built);
	EXPECT_EQ(ReportOf(b, MonumentPart::Peristyle).state, MonumentPartState::CompleteNotBuilt);
	for (MonumentPart part : {MonumentPart::Columns, MonumentPart::Entablature, MonumentPart::Pediment})
		EXPECT_EQ(ReportOf(b, part).state, MonumentPartState::Absent) << static_cast<int>(part);

	const std::vector<float> &v = b.mesh->GetVertices();
	double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
	for (std::size_t i = 0; i < v.size(); ++i)
	{
		lo[i % 3] = std::min(lo[i % 3], double(v[i]));
		hi[i % 3] = std::max(hi[i % 3], double(v[i]));
	}
	EXPECT_NEAR(hi[0] - lo[0], 16.125, kPrintedHalfUnit);
	EXPECT_NEAR(hi[1] - lo[1], 34.941, kPrintedHalfUnit);
	EXPECT_NEAR(hi[2] - lo[2], 1.371, kPrintedHalfUnit);
	EXPECT_NEAR(hi[0] - lo[0], ares.krepisControls.footprintWidth, kPrintedHalfUnit);
	EXPECT_NEAR(hi[1] - lo[1], ares.krepisControls.footprintLength, kPrintedHalfUnit);

	// Axes : facade (Y = -halfDepth) le long de X, aux y du README ; flanc droit (X = +halfWidth) aux -x.
	const PeristyleLayout layout = LayoutPeristyle(MonumentPeristyleSettings(ares));
	ASSERT_EQ(layout.columns.size(), 34u);
	const double kAxisTolerance = 1e-12 * layout.halfDepth;
	std::vector<double> frontX, rightY, minusX;
	for (const PeristyleColumn &c : layout.columns)
	{
		if (std::abs(c.y + layout.halfDepth) <= kAxisTolerance)
			frontX.push_back(c.x);
		if (std::abs(c.x - layout.halfWidth) <= kAxisTolerance)
			rightY.push_back(c.y);
	}
	for (double x : ares.controls.flankAxesX)
		minusX.push_back(-x);
	std::sort(frontX.begin(), frontX.end());
	std::sort(rightY.begin(), rightY.end());
	std::sort(minusX.begin(), minusX.end());
	ASSERT_EQ(frontX.size(), ares.controls.frontAxesY.size());
	ASSERT_EQ(rightY.size(), minusX.size());
	for (std::size_t i = 0; i < frontX.size(); ++i)
		EXPECT_NEAR(frontX[i], ares.controls.frontAxesY[i], kPrintedHalfUnit) << i;
	for (std::size_t i = 0; i < rightY.size(); ++i)
		EXPECT_NEAR(rightY[i], minusX[i], kPrintedHalfUnit) << i;

	// Cas negatif : une cle de la krepis manquante, rien n'est construit.
	MonumentData noStylobate = ares;
	noStylobate.krepis.stylobateHeight.value.reset();
	noStylobate.krepis.stylobateHeight.status = MonumentStatus::Missing;
	const MonumentBuild none = BuildMonumentMesh(noStylobate);
	ASSERT_TRUE(none.mesh);
	EXPECT_EQ(none.mesh->GetNVertices(), 0);
	EXPECT_TRUE(none.parts.empty());
	EXPECT_EQ(ReportOf(none, MonumentPart::Krepis).state, MonumentPartState::Incomplete);
	EXPECT_EQ(ReportOf(none, MonumentPart::Krepis).missingKeys, std::vector<std::string>{"krepis.stylobateHeight"});
	EXPECT_EQ(ReportOf(none, MonumentPart::Peristyle).state, MonumentPartState::Blocked);
	EXPECT_TRUE(ReportOf(none, MonumentPart::Peristyle).blockedBy == std::optional<MonumentPart>(MonumentPart::Krepis));
}
