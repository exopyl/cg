#include <cgarchi/classical_orders.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <locale>
#include <sstream>
#include <set>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include <cgarchi/classical_orders_data.h>

namespace cgarchi
{
namespace classical
{

namespace
{

using nlohmann::json;

[[noreturn]] void Fail (const std::string &path, const std::string &what)
{
	throw std::runtime_error ("cinq_ordres.json " + path + " : " + what);
}

const json &At (const json &object, const char *key, const std::string &path)
{
	if (!object.is_object ())
		Fail (path, "objet attendu");
	const auto it = object.find (key);
	if (it == object.end ())
		Fail (path + "/" + key, "champ manquant");
	return *it;
}

std::string Text (const json &value, const std::string &path)
{
	if (!value.is_string ())
		Fail (path, "chaine attendue");
	return value.get<std::string> ();
}

// Nombre {v, f} : la valeur exacte est celle de f.
double Number (const json &value, const std::string &path)
{
	const json &f = At (value, "f", path);
	try
	{
		return ParseExactNumber (Text (f, path + "/f"));
	}
	catch (const std::invalid_argument &)
	{
		Fail (path + "/f", "nombre illisible");
	}
}

std::set<std::string> Keys (const json &object)
{
	std::set<std::string> keys;
	for (auto it = object.begin (); it != object.end (); ++it)
		keys.insert (it.key ());
	return keys;
}

PlanForm ParsePlan (const json &plan, const std::string &path)
{
	const std::string form = Text (At (plan, "forme", path), path + "/forme");
	if (form == "revolution")      return PlanForm::Revolution;
	if (form == "carre")           return PlanForm::Square;
	if (form == "carre_concave")   return PlanForm::ConcaveSquare;
	if (form == "oriente_ionique") return PlanForm::IonicOriented;
	if (form == "lineaire")        return PlanForm::Linear;
	Fail (path + "/forme", "plan inconnu \"" + form + "\"");
}

MouldingForm ParseForm (const std::string &form, const std::string &path)
{
	if (form == "listel")   return MouldingForm::Fillet;
	if (form == "face")     return MouldingForm::Face;
	if (form == "ornement") return MouldingForm::Ornament;
	if (form == "ove")      return MouldingForm::Ovolo;
	if (form == "cavet")    return MouldingForm::Cavetto;
	if (form == "doucine")  return MouldingForm::CymaRecta;
	if (form == "talon")    return MouldingForm::CymaReversa;
	if (form == "tore")     return MouldingForm::Torus;
	if (form == "scotie")   return MouldingForm::Scotia;
	if (form == "conge")    return MouldingForm::Conge;
	Fail (path, "forme de moulure inconnue \"" + form + "\"");
}

// Une coupe : {s} ou {s_bas, s_haut}.
Span ParseCut (const json &cut, const std::string &path, bool allowConstant)
{
	if (!cut.is_object ())
		Fail (path, "objet attendu");
	const std::set<std::string> keys = Keys (cut);
	if (allowConstant && keys == std::set<std::string> {"s"})
	{
		const double s = Number (cut.at ("s"), path + "/s");
		return {s, s};
	}
	if (keys == std::set<std::string> {"s_bas", "s_haut"})
		return {Number (cut.at ("s_bas"), path + "/s_bas"), Number (cut.at ("s_haut"), path + "/s_haut")};
	Fail (path, "cles de saillie inattendues");
}

Projection ParseProjection (const json &saillie, const std::string &path)
{
	if (!saillie.is_object ())
		Fail (path, "objet attendu (aucune saillie nulle n'est admise)");
	const std::set<std::string> keys = Keys (saillie);
	Projection p;
	if (keys == std::set<std::string> {"s"} || keys == std::set<std::string> {"s_bas", "s_haut"})
	{
		p.kind = keys.size () == 1 ? ProjectionKind::Constant : ProjectionKind::Range;
		p.main = ParseCut (saillie, path, true);
	}
	else if (keys == std::set<std::string> {"attache", "s_max"})
	{
		p.kind = ProjectionKind::Torus;
		const double attach = Number (saillie.at ("attache"), path + "/attache");
		p.main = {attach, attach};
		p.extremum = Number (saillie.at ("s_max"), path + "/s_max");
	}
	else if (keys == std::set<std::string> {"s_bas", "fond", "s_haut"})
	{
		p.kind = ProjectionKind::Scotia;
		p.main = {Number (saillie.at ("s_bas"), path + "/s_bas"), Number (saillie.at ("s_haut"), path + "/s_haut")};
		p.extremum = Number (saillie.at ("fond"), path + "/fond");
	}
	else if (keys == std::set<std::string> {"diagonale", "face"})
	{
		p.kind = ProjectionKind::TwoCuts;
		p.main = ParseCut (saillie.at ("diagonale"), path + "/diagonale", true);
		p.face = ParseCut (saillie.at ("face"), path + "/face", true);
	}
	else if (keys == std::set<std::string> {"noyau", "enveloppe"})
	{
		p.kind = ProjectionKind::CoreEnvelope;
		p.main     = ParseCut (saillie.at ("noyau"), path + "/noyau", false);
		p.envelope = ParseCut (saillie.at ("enveloppe"), path + "/enveloppe", false);
	}
	else
		Fail (path, "jeu de cles de saillie inconnu");
	return p;
}

Block ParseBlock (const std::string &key, const json &b, const std::string &path)
{
	Block block;
	block.key = key;
	block.axisDistance = Number (At (b, "distance_axe", path), path + "/distance_axe");
	block.plan = ParsePlan (At (b, "plan", path), path + "/plan");
	const json &total = At (b, "hauteur_totale", path);
	if (!total.is_null ())
		block.totalHeight = Number (total, path + "/hauteur_totale");
	if (b.contains ("saillie_totale"))
		block.totalProjection = Number (b.at ("saillie_totale"), path + "/saillie_totale");
	if (b.contains ("hors_module"))
	{
		const json &hm = b.at ("hors_module");
		if (!hm.is_array ())
			Fail (path + "/hors_module", "liste attendue");
		for (const json &name : hm)
			block.outsideModule.push_back (Text (name, path + "/hors_module"));
	}
	const json &members = At (b, "membres", path);
	if (!members.is_array () || members.empty ())
		Fail (path + "/membres", "liste non vide attendue");
	for (std::size_t i = 0; i < members.size (); ++i)
	{
		const std::string mp = path + "/membres[" + std::to_string (i) + "]";
		const json &m = members[i];
		Member member;
		member.name = Text (At (m, "nom", mp), mp + "/nom");
		member.form = ParseForm (Text (At (m, "forme", mp), mp + "/forme"), mp + "/forme");
		member.h = Number (At (m, "h", mp), mp + "/h");
		if (!(member.h > 0.0))
			Fail (mp + "/h", "hauteur non positive");
		member.projection = ParseProjection (At (m, "saillie", mp), mp + "/saillie");
		member.plan = block.plan;
		if (m.contains ("plan"))
		{
			member.plan = ParsePlan (m.at ("plan"), mp + "/plan");
			member.planOverridden = true;
		}
		block.members.push_back (std::move (member));
	}
	for (const std::string &name : block.outsideModule)
	{
		const bool found = std::any_of (block.members.begin (), block.members.end (),
		                                [&] (const Member &m) { return m.name == name; });
		if (!found)
			Fail (path + "/hors_module", "membre inconnu \"" + name + "\"");
	}
	return block;
}

DiminutionMethod ParseDiminution (const std::string &method, const std::string &path)
{
	if (method == "methode_1") return DiminutionMethod::Cosine;
	if (method == "methode_2") return DiminutionMethod::Conchoid;
	Fail (path, "methode de diminution inconnue \"" + method + "\"");
}

// Valeur d'un champ texte « a/b » ou « a ».
double TextNumber (const json &object, const char *key, const std::string &path)
{
	const std::string at = path + "/" + key;
	const std::string text = Text (At (object, key, path), at);
	try
	{
		return ParseExactNumber (text);
	}
	catch (const std::invalid_argument &)
	{
		Fail (at, "nombre illisible");
	}
}

IonicVoluteData ParseVolute (const json &v, const std::string &path)
{
	if (!v.is_object ())
		Fail (path, "objet attendu");
	IonicVoluteData d;
	d.height           = TextNumber (v, "hauteur", path);
	d.eyeDiameter      = TextNumber (v, "oeil_diametre", path);
	d.centreFromTop    = TextNumber (v, "centre_depuis_haut", path);
	d.centreFromBottom = TextNumber (v, "centre_depuis_bas", path);
	const std::string dp = path + "/distances_au_centre";
	const json &distances = At (v, "distances_au_centre", path);
	d.top    = TextNumber (distances, "haut", dp);
	d.right  = TextNumber (distances, "droite", dp);
	d.bottom = TextNumber (distances, "bas", dp);
	d.left   = TextNumber (distances, "gauche", dp);
	d.catheteSpacing  = TextNumber (v, "cathetes_entraxe", path);
	d.eyeFromAxis     = TextNumber (v, "centre_oeil_depuis_axe", path);
	d.topBelowCapital = TextNumber (v, "haut_volute_sous_sommet_chapiteau", path);
	d.faceSpacing     = TextNumber (v, "faces_des_volutes_en_plan", path);
	d.cushionWidth    = TextNumber (v, "largeur_coussinet", path);
	d.listelFace      = TextNumber (v, "listel_face", path);
	return d;
}

FluteData ParseFlutes (const json &c, const OrderGeneral &general, const std::string &path)
{
	if (!c.is_object ())
		Fail (path, "objet attendu");
	FluteData f;
	const json &count = At (c, "nombre", path);
	if (!count.is_number_integer () || count.get<int> () <= 0)
		Fail (path + "/nombre", "entier positif attendu");
	// Le tour suppose une section a symetrie d'ordre 4.
	if (count.get<int> () % 4 != 0)
		Fail (path + "/nombre", "nombre de cannelures non multiple de 4");
	f.count = count.get<int> ();
	Text (At (c, "facultatives", path), path + "/facultatives");
	const json &ends = At (c, "extremites", path);
	Text (At (ends, "haut", path + "/extremites"), path + "/extremites/haut");
	Text (At (ends, "bas", path + "/extremites"), path + "/extremites/bas");

	// Deux ecritures du meme fait : « listels » booleen (dorique) ou
	// « separees_par » (autres ordres).
	if (c.contains ("listels"))
	{
		if (!c.at ("listels").is_boolean ())
			Fail (path + "/listels", "booleen attendu");
		f.listels = c.at ("listels").get<bool> ();
	}
	else
	{
		const std::string by = Text (At (c, "separees_par", path), path + "/separees_par");
		if (by != "listels")
			Fail (path + "/separees_par", "valeur inconnue \"" + by + "\"");
		f.listels = true;
	}

	if (c.contains ("A") || c.contains ("B"))
	{
		Text (At (At (c, "A", path), "section", path + "/A"), path + "/A/section");
		Text (At (At (c, "B", path), "section", path + "/B"), path + "/B/section");
		f.sections = {FluteSection::Segment60, FluteSection::Semicircle};
	}
	else
	{
		const std::string bottom = Text (At (c, "fond", path), path + "/fond");
		if (bottom != "demi-cercle")
			Fail (path + "/fond", "fond inconnu \"" + bottom + "\"");
		f.sections = {FluteSection::Semicircle};
	}

	if (f.listels)
	{
		const std::string fp = path + "/au_pied";
		const json &foot = At (c, "au_pied", path);
		f.footListel = TextNumber (foot, "listel", fp);
		f.footFlute = TextNumber (foot, "cannelure", fp);
		f.footDepth = TextNumber (foot, "profondeur", fp);
		Text (At (foot, "pas", fp), fp + "/pas");
		const double pitch = 3.14159265358979323846 * general.lowerDiameter_p / f.count;
		if (!(f.footListel > 0.0 && f.footListel < pitch))
			Fail (fp + "/listel", "listel hors du pas");
	}
	return f;
}

// Premier nombre ecrit « a/b » ou « a » apres `prefix` dans `text`.
std::optional<double> NumberAfter (const std::string &text, const std::string &prefix)
{
	const std::size_t at = text.find (prefix);
	if (at == std::string::npos)
		return std::nullopt;
	std::size_t i = at + prefix.size ();
	while (i < text.size () && text[i] == ' ')
		++i;
	std::size_t j = i;
	while (j < text.size () && (std::isdigit (static_cast<unsigned char> (text[j])) || text[j] == '/'))
		++j;
	if (j == i)
		return std::nullopt;
	try
	{
		return ParseExactNumber (text.substr (i, j - i));
	}
	catch (const std::invalid_argument &)
	{
		return std::nullopt;
	}
}

double RequireAfter (const std::string &text, const std::string &prefix, const std::string &path)
{
	const std::optional<double> v = NumberAfter (text, prefix);
	if (!v)
		Fail (path, "nombre introuvable apres \"" + prefix + "\"");
	return *v;
}

void RequireText (const json &object, const char *key, const std::string &expected, const std::string &path)
{
	const std::string value = Text (At (object, key, path), path + "/" + key);
	if (value != expected)
		Fail (path + "/" + key, "valeur non prise en charge \"" + value + "\" (attendu \"" + expected + "\")");
}

// Le membre de corniche lie a un ornement : present une seule fois, de forme
// « ornement », de saillie {s}.
const Member &RequireOrnamentMember (const OrderData &order, EntablatureOrnament ornament, const std::string &path)
{
	const OrnamentMember *link = EntablatureOrnamentMember (order.key, ornament);
	if (!link)
		Fail (path, "ornement sans membre lie dans la table");
	const Block *block = order.FindBlock (link->block);
	if (!block)
		Fail (path, std::string ("bloc lie absent : ") + link->block);
	const Member *found = nullptr;
	int count = 0;
	for (const Member &m : block->members)
		if (m.name == link->member)
		{
			++count;
			found = &m;
		}
	if (count != 1)
		Fail (path, std::string (count == 0 ? "membre lie absent : " : "membre lie ambigu : ") + link->block + "/" +
		              link->member);
	if (found->form != MouldingForm::Ornament || found->projection.kind != ProjectionKind::Constant)
		Fail (path, std::string ("membre lie de forme ou de saillie inattendue : ") + link->member);
	return *found;
}

// Hauteurs exactes (fractions) : egalite a l'arrondi double pres.
void RequireSameHeight (double ornament, double member, const std::string &path, const std::string &what)
{
	if (std::fabs (ornament - member) > 1e-12 * std::max (1.0, std::fabs (member)))
		Fail (path, "hauteur differente de celle du membre lie (" + what + ")");
}

TriglyphData ParseTriglyph (const OrderData &order, const json &o, const std::string &path)
{
	const std::string tp = path + "/triglyphe";
	const json &t = o.at ("triglyphe");
	TriglyphData d;
	d.width = TextNumber (t, "largeur", tp);
	d.height = TextNumber (t, "hauteur", tp);
	d.capitalHeight = TextNumber (t, "chapiteau_h", tp);
	d.spacing = TextNumber (t, "entraxe", tp);
	d.projection = TextNumber (t, "saillie_sur_metope", tp);
	RequireText (t, "canaux", "en V", tp);
	const json &divisions = At (t, "divisions", tp);
	if (!divisions.is_array () || divisions.empty ())
		Fail (tp + "/divisions", "liste attendue");
	double sum = 0.0;
	for (std::size_t i = 0; i < divisions.size (); ++i)
	{
		const std::string dp = tp + "/divisions[" + std::to_string (i) + "]";
		const std::string text = Text (divisions[i], dp);
		const std::size_t space = text.rfind (' ');
		if (space == std::string::npos)
			Fail (dp, "division illisible");
		const std::string kind = text.substr (0, space);
		if (kind != "canal" && kind != "1/2 canal" && kind != "bande")
			Fail (dp, "nature de division inconnue \"" + kind + "\"");
		double w = 0.0;
		try
		{
			w = ParseExactNumber (text.substr (space + 1));
		}
		catch (const std::invalid_argument &)
		{
			Fail (dp, "largeur illisible");
		}
		d.divisionWidths.push_back (w);
		d.divisionIsChannel.push_back (kind != "bande");
		sum += w;
	}
	if (std::fabs (sum - d.width) > 1e-12)
		Fail (tp + "/divisions", "somme des divisions differente de la largeur");
	const std::string cp = tp + "/canaux_detail";
	const json &channels = At (t, "canaux_detail", tp);
	d.channelAngleDeg = RequireAfter (Text (At (channels, "angle", cp), cp + "/angle"), "", cp + "/angle");
	d.channelDepth = TextNumber (channels, "profondeur", cp);
	d.halfChannelDepth = TextNumber (channels, "profondeur_demi_canal", cp);
	// L'angle du V d'un canal plein est fixe par sa largeur et sa profondeur :
	// 2 atan((w/2)/p). Les demi-canaux des bords (profondeur_demi_canal) ne
	// s'y rapportent pas : largeur 1 et profondeur 1/2 donneraient un V a
	// 126,9 degres (anomalie des donnees, consignee).
	for (std::size_t i = 1; i + 1 < d.divisionWidths.size (); ++i)
	{
		if (!d.divisionIsChannel[i])
			continue;
		const double angle = 2.0 * std::atan (0.5 * d.divisionWidths[i] / d.channelDepth) * 180.0 / 3.14159265358979323846;
		if (std::fabs (angle - d.channelAngleDeg) > 1e-9)
			Fail (cp + "/angle", "angle incompatible avec la largeur et la profondeur des canaux");
	}
	const json &metope = At (o, "metope", path);
	d.metopeWidth = TextNumber (metope, "largeur", path + "/metope");
	if (std::fabs (d.width + d.metopeWidth - d.spacing) > 1e-12)
		Fail (tp + "/entraxe", "triglyphe + metope differe de l'entraxe");
	// Hauteur = frise ; chapiteau = membre du bas de chaque corniche.
	const Block *frieze = order.FindBlock ("frise");
	if (!frieze || frieze->members.size () != 1)
		Fail (tp + "/hauteur", "frise d'un seul membre attendue");
	RequireSameHeight (d.height, frieze->members.front ().h, tp + "/hauteur", "frise");
	for (const Block &b : order.blocks)
		if (b.key.rfind ("corniche", 0) == 0)
			RequireSameHeight (d.capitalHeight, b.members.back ().h, tp + "/chapiteau_h", b.key + "/" + b.members.back ().name);
	return d;
}

GuttaeData ParseGuttae (const OrderData &, const json &o, const std::string &path, double triglyphWidth)
{
	const std::string gp = path + "/gouttes";
	const json &g = At (o, "gouttes", path);
	GuttaeData d;
	d.regulaHeight = TextNumber (g, "regula_h", gp);
	d.guttaHeight = TextNumber (g, "gouttes_h", gp);
	const json &count = At (g, "nombre", gp);
	if (!count.is_number_integer () || count.get<int> () <= 0)
		Fail (gp + "/nombre", "entier positif attendu");
	d.count = count.get<int> ();
	RequireText (g, "forme", "tronconique", gp);
	d.diameterBottom = TextNumber (g, "diametre_bas", gp);
	d.diameterTop = TextNumber (g, "diametre_haut", gp);
	d.spacing = TextNumber (g, "entraxe", gp);
	if (std::fabs (d.spacing * d.count - triglyphWidth) > 1e-12)
		Fail (gp + "/entraxe", "gouttes : entraxe x nombre differe de la largeur du triglyphe");
	return d;
}

DentilData ParseDentils (const OrderData &order, const json &o, const std::string &path)
{
	const std::string dp = path + "/denticules";
	const json &t = o.at ("denticules");
	DentilData d;
	d.width = TextNumber (t, "largeur", dp);
	d.gap = TextNumber (t, "intervalle", dp);
	d.height = TextNumber (t, "hauteur", dp);
	d.depth = TextNumber (t, "recul_du_fond", dp);
	d.back = TextNumber (t, "fond_depuis_nu_frise", dp);
	if (t.contains ("entraxe") && std::fabs (TextNumber (t, "entraxe", dp) - d.width - d.gap) > 1e-12)
		Fail (dp + "/entraxe", "entraxe different de largeur + intervalle");
	const Member &m = RequireOrnamentMember (order, EntablatureOrnament::Dentils, dp);
	RequireSameHeight (d.height, m.h, dp + "/hauteur", m.name);
	return d;
}

MutuleData ParseMutules (const OrderData &order, const json &o, const std::string &path)
{
	const std::string mp = path + "/mutules";
	const json &t = o.at ("mutules");
	MutuleData d;
	d.width = TextNumber (t, "largeur", mp);
	d.height = TextNumber (t, "hauteur", mp);
	RequireText (t, "axe", "triglyphe", mp);
	const std::string guttae = Text (At (t, "gouttes", mp), mp + "/gouttes");
	d.rows = static_cast<int> (RequireAfter (guttae, "", mp + "/gouttes"));
	d.perRow = static_cast<int> (RequireAfter (guttae, "rangs de", mp + "/gouttes"));
	d.guttaPitch = RequireAfter (guttae, "au pas de", mp + "/gouttes");
	d.guttaDiameterBottom = RequireAfter (guttae, "du bas", mp + "/gouttes");
	d.guttaHeight = RequireAfter (guttae, "; h", mp + "/gouttes");
	d.listelHeight = RequireAfter (guttae, "sous un listel de", mp + "/gouttes");
	d.guttaDiameterTop = TextNumber (t, "gouttes_diametre_haut", mp);
	const std::string plan = Text (At (t, "plan", mp), mp + "/plan");
	d.face = RequireAfter (plan, "face \xC3\xA0", mp + "/plan");
	d.back = RequireAfter (plan, "vers", mp + "/plan");
	if (std::fabs (d.guttaPitch * d.perRow - d.width) > 1e-12)
		Fail (mp + "/gouttes", "gouttes : pas x nombre par rang differe de la largeur");
	const Member &m = RequireOrnamentMember (order, EntablatureOrnament::Mutules, mp);
	RequireSameHeight (d.height, m.h, mp + "/hauteur", m.name);
	return d;
}

SoffitGuttaeData ParseSoffitGuttae (const json &o, const std::string &path)
{
	const std::string sp = path + "/plafond_corniche_denticules";
	const json &t = o.at ("plafond_corniche_denticules");
	SoffitGuttaeData d;
	const std::string guttae = Text (At (t, "gouttes", sp), sp + "/gouttes");
	d.rows = static_cast<int> (RequireAfter (guttae, "", sp + "/gouttes"));
	d.perRow = static_cast<int> (RequireAfter (guttae, "rangs de", sp + "/gouttes"));
	d.pitchAlong = RequireAfter (guttae, "au pas de", sp + "/gouttes");
	// « 2 2/5 en profondeur » : nombre fractionnaire mixte.
	const std::size_t depthAt = guttae.find (" en profondeur");
	const std::size_t mixed = depthAt == std::string::npos ? std::string::npos : guttae.rfind (' ', depthAt - 1);
	const std::size_t whole = mixed == std::string::npos ? std::string::npos : guttae.rfind (' ', mixed - 1);
	if (whole == std::string::npos)
		Fail (sp + "/gouttes", "pas en profondeur introuvable");
	try
	{
		d.pitchDepth = ParseExactNumber (guttae.substr (whole + 1, mixed - whole - 1)) +
		               ParseExactNumber (guttae.substr (mixed + 1, depthAt - mixed - 1));
	}
	catch (const std::invalid_argument &)
	{
		Fail (sp + "/gouttes", "pas en profondeur illisible");
	}
	const std::size_t between = guttae.find ("entre ");
	if (between == std::string::npos)
		Fail (sp + "/gouttes", "bande de la sous-face introuvable");
	d.from = RequireAfter (guttae.substr (between), "entre", sp + "/gouttes");
	d.to = RequireAfter (guttae.substr (between), " et", sp + "/gouttes");
	d.diameterBottom = TextNumber (t, "diametre_bas", sp);
	return d;
}

ModillionData ParseModillions (const OrderData &order, const json &o, const std::string &path)
{
	const std::string mp = path + "/modillons";
	const json &t = o.at ("modillons");
	ModillionData d;
	d.width = TextNumber (t, "largeur", mp);
	d.coffer = TextNumber (t, "caisson", mp);
	d.spacing = TextNumber (t, "entraxe", mp);
	d.length = TextNumber (t, "longueur", mp);
	RequireText (t, "axe", "colonne", mp);
	if (std::fabs (d.width + d.coffer - d.spacing) > 1e-12)
		Fail (mp + "/entraxe", "largeur + caisson differe de l'entraxe");
	RequireOrnamentMember (order, EntablatureOrnament::Modillions, mp);
	return d;
}

BlockRowData ParseBlocks (const OrderData &order, const json &o, const std::string &path)
{
	const std::string bp = path + "/blocs_composite";
	const json &t = o.at ("blocs_composite");
	BlockRowData d;
	d.width = TextNumber (t, "largeur", bp);
	d.gap = TextNumber (t, "intervalle", bp);
	d.height = TextNumber (t, "hauteur", bp);
	d.depth = TextNumber (t, "profondeur", bp);
	RequireText (t, "fond", "nu de la frise", bp);
	const Member &m = RequireOrnamentMember (order, EntablatureOrnament::Blocks, bp);
	RequireSameHeight (d.height, m.h, bp + "/hauteur", m.name);
	return d;
}

EntablatureOrnaments ParseEntablatureOrnaments (const OrderData &order, const json &o, const std::string &path)
{
	EntablatureOrnaments e;
	if (o.contains ("triglyphe"))
	{
		e.triglyph = ParseTriglyph (order, o, path);
		e.guttae = ParseGuttae (order, o, path, e.triglyph->width);
	}
	if (o.contains ("denticules"))
		e.dentils = ParseDentils (order, o, path);
	if (o.contains ("mutules"))
		e.mutules = ParseMutules (order, o, path);
	if (o.contains ("plafond_corniche_denticules"))
		e.soffitGuttae = ParseSoffitGuttae (o, path);
	if (o.contains ("modillons"))
		e.modillions = ParseModillions (order, o, path);
	if (o.contains ("blocs_composite"))
		e.blocks = ParseBlocks (order, o, path);
	return e;
}

OrderData ParseOrder (Order order, const json &o, const std::string &path)
{
	OrderData data;
	data.order = order;
	data.key = OrderKey (order);
	const json &ppm = At (o, "parties_par_module", path);
	if (!ppm.is_number_integer () || ppm.get<int> () <= 0)
		Fail (path + "/parties_par_module", "entier positif attendu");
	data.partsPerModule = ppm.get<int> ();

	const std::string gp = path + "/general";
	const json &g = At (o, "general", path);
	OrderGeneral &gen = data.general;
	gen.column_M               = Number (At (g, "colonne_M", gp), gp + "/colonne_M");
	gen.base_M                 = Number (At (g, "base_M", gp), gp + "/base_M");
	gen.shaft_M                = Number (At (g, "fut_M", gp), gp + "/fut_M");
	gen.capital_M              = Number (At (g, "chapiteau_M", gp), gp + "/chapiteau_M");
	gen.lowerDiameter_p        = Number (At (g, "diam_inf_p", gp), gp + "/diam_inf_p");
	gen.upperDiameter_p        = Number (At (g, "diam_sup_p", gp), gp + "/diam_sup_p");
	gen.entablature_M          = Number (At (g, "entablement_M", gp), gp + "/entablement_M");
	gen.pedestal_M             = Number (At (g, "piedestal_M", gp), gp + "/piedestal_M");
	gen.totalWithoutPedestal_M = Number (At (g, "total_sans_piedestal_M", gp), gp + "/total_sans_piedestal_M");
	gen.totalWithPedestal_M    = Number (At (g, "total_avec_piedestal_M", gp), gp + "/total_avec_piedestal_M");
	gen.intercolumniation_M    = Number (At (g, "entrecolonnement_M", gp), gp + "/entrecolonnement_M");
	gen.diminution = ParseDiminution (Text (At (g, "diminution", gp), gp + "/diminution"), gp + "/diminution");

	const json &blocks = At (o, "blocs", path);
	if (!blocks.is_object ())
		Fail (path + "/blocs", "objet attendu");
	const std::string op = path + "/ordre_des_blocs_de_haut_en_bas";
	const json &sequence = At (o, "ordre_des_blocs_de_haut_en_bas", path);
	if (!sequence.is_array () || sequence.size () != blocks.size ())
		Fail (op, "liste des blocs attendue, un nom par bloc");
	std::set<std::string> seen;
	for (const json &name : sequence)
	{
		const std::string key = Text (name, op);
		if (!seen.insert (key).second)
			Fail (op, "bloc cite deux fois \"" + key + "\"");
		data.blocks.push_back (ParseBlock (key, At (blocks, key.c_str (), path + "/blocs"), path + "/blocs/" + key));
	}

	const json &ornaments = At (o, "ornements", path);
	if (ornaments.is_object () && ornaments.contains ("cannelures") && !ornaments.at ("cannelures").is_null ())
		data.flutes = ParseFlutes (ornaments.at ("cannelures"), data.general, path + "/ornements/cannelures");
	if (ornaments.is_object ())
		data.entablatureOrnaments = ParseEntablatureOrnaments (data, ornaments, path + "/ornements");
	if (ornaments.is_object () && ornaments.contains ("volute"))
		data.volute = ParseVolute (ornaments.at ("volute"), path + "/ornements/volute");
	return data;
}

bool Near (double a, double b)
{
	return std::fabs (a - b) <= 1e-9 * std::max (1.0, std::fabs (b));
}

double MaxProjection (const Member &m)
{
	const Projection &p = m.projection;
	double s = std::max (p.main.bottom, p.main.top);
	if (p.kind == ProjectionKind::Torus)
		s = std::max (s, p.extremum);
	if (p.kind == ProjectionKind::TwoCuts)
		s = std::max ({s, p.face.bottom, p.face.top});
	if (p.kind == ProjectionKind::CoreEnvelope)
		s = std::max ({s, p.envelope.bottom, p.envelope.top});
	return s;
}

} // namespace

const OrnamentMember *EntablatureOrnamentMember (const std::string &orderKey, EntablatureOrnament ornament)
{
	struct Row { const char *order; EntablatureOrnament ornament; OrnamentMember member; };
	static const Row rows[] = {
		{ "dorique",    EntablatureOrnament::Dentils,    { "corniche_denticules", "B \xC2\xB7 dentello (denticules)" } },
		{ "dorique",    EntablatureOrnament::Mutules,    { "corniche_mutules", "B \xC2\xB7 modiglione (mutule)" } },
		{ "ionique",    EntablatureOrnament::Dentils,    { "corniche", "denticules" } },
		{ "corinthien", EntablatureOrnament::Dentils,    { "corniche", "denticules" } },
		{ "corinthien", EntablatureOrnament::Modillions, { "corniche", "modillons" } },
		{ "composite",  EntablatureOrnament::Blocks,     { "corniche", "blocs \xC3\xA0 pomme de pin" } },
	};
	for (const Row &r : rows)
		if (orderKey == r.order && ornament == r.ornament)
			return &r.member;
	return nullptr;
}

const Member &LinkedOrnamentMember (const OrderData &order, EntablatureOrnament ornament)
{
	const OrnamentMember *link = EntablatureOrnamentMember (order.key, ornament);
	const Block *block = link ? order.FindBlock (link->block) : nullptr;
	if (block)
		for (const Member &m : block->members)
			if (m.name == link->member)
				return m;
	throw std::invalid_argument ("membre lie absent pour " + order.key);
}

double AxisSpacing (const OrderData &order)
{
	return order.general.intercolumniation_M * order.partsPerModule + order.general.lowerDiameter_p;
}

const char *OrderKey (Order order)
{
	switch (order)
	{
	case Order::Tuscan:     return "toscan";
	case Order::Doric:      return "dorique";
	case Order::Ionic:      return "ionique";
	case Order::Corinthian: return "corinthien";
	case Order::Composite:  return "composite";
	}
	return "";
}

bool Block::IsOutsideModule (const Member &member) const
{
	return std::find (outsideModule.begin (), outsideModule.end (), member.name) != outsideModule.end ();
}

const Block *OrderData::FindBlock (std::string_view k) const
{
	for (const Block &b : blocks)
		if (b.key == k)
			return &b;
	return nullptr;
}

double ParseExactNumber (std::string_view text)
{
	const std::string s (text);
	if (s.empty ())
		throw std::invalid_argument ("nombre vide");
	const std::size_t slash = s.find ('/');
	// Locale "C" imposee : le separateur decimal du JSON est le point, quelle
	// que soit la locale globale du programme.
	auto parse = [] (const std::string &part) {
		std::istringstream in (part);
		in.imbue (std::locale::classic ());
		double v = 0.0;
		in >> std::noskipws >> v;
		if (part.empty () || in.fail () || in.peek () != std::char_traits<char>::eof () || !std::isfinite (v))
			throw std::invalid_argument ("nombre illisible : " + part);
		return v;
	};
	if (slash == std::string::npos)
		return parse (s);
	const double num = parse (s.substr (0, slash));
	const double den = parse (s.substr (slash + 1));
	if (den == 0.0)
		throw std::invalid_argument ("denominateur nul : " + s);
	return num / den;
}

OrdersData ParseClassicalOrders (std::string_view jsonText)
{
	const json root = json::parse (jsonText.begin (), jsonText.end (), nullptr, false);
	if (root.is_discarded ())
		Fail ("/", "JSON illisible");

	OrdersData data;
	data.version = Text (At (root, "version", "/"), "/version");
	{
		const json &fillets = At (At (root, "conventions", "/"), "conges_du_fut", "/conventions");
		const std::string fp = "/conventions/conges_du_fut";
		data.shaftFilletRule = Text (At (fillets, "regle", fp), fp + "/regle");
		const std::string bp = "/conventions/plans/face_arriere_entablement";
		const json &back = At (At (At (root, "conventions", "/"), "plans", "/conventions"), "face_arriere_entablement",
		                       "/conventions/plans");
		data.symmetricEntablatureRule =
			Text (At (At (back, "cas_colonnade_isolee", bp), "coupe_symetrique", bp + "/cas_colonnade_isolee"),
			      bp + "/cas_colonnade_isolee/coupe_symetrique");
		Text (At (fillets, "place", fp), fp + "/place");
		Text (At (fillets, "cannelures", fp), fp + "/cannelures");
	}

	const json &orders = At (root, "ordres", "/");
	for (int i = 0; i < kOrderCount; ++i)
	{
		const Order order = static_cast<Order> (i);
		const std::string key = OrderKey (order);
		data.orders[i] = ParseOrder (order, At (orders, key.c_str (), "/ordres"), "/ordres/" + key);
	}

	const json &controls = At (root, "controles", "/");
	if (!controls.is_array ())
		Fail ("/controles", "liste attendue");
	for (std::size_t i = 0; i < controls.size (); ++i)
	{
		const std::string cp = "/controles[" + std::to_string (i) + "]";
		const json &c = controls[i];
		Control control;
		control.order    = Text (At (c, "ordre", cp), cp + "/ordre");
		control.block    = Text (At (c, "bloc", cp), cp + "/bloc");
		control.label    = Text (At (c, "controle", cp), cp + "/controle");
		control.expected = Text (At (c, "attendu", cp), cp + "/attendu");
		control.obtained = Text (At (c, "obtenu", cp), cp + "/obtenu");
		const json &ok = At (c, "ok", cp);
		if (!ok.is_boolean ())
			Fail (cp + "/ok", "booleen attendu");
		control.ok = ok.get<bool> ();
		data.controls.push_back (std::move (control));
	}

	const std::string m2 = "/diminution/methode_2";
	const json &method2 = At (At (root, "diminution", "/"), "methode_2", "/diminution");
	data.conchoidCDPlate_M = Number (At (method2, "CD_planche", m2), m2 + "/CD_planche");
	const json &computed = At (method2, "CD_calcule", m2);
	if (!computed.is_object ())
		Fail (m2 + "/CD_calcule", "objet attendu");
	for (auto it = computed.begin (); it != computed.end (); ++it)
	{
		const json &v = At (it.value (), "v", m2 + "/CD_calcule/" + it.key ());
		if (!v.is_number ())
			Fail (m2 + "/CD_calcule/" + it.key () + "/v", "nombre attendu");
		data.conchoidCDComputed_M.emplace_back (it.key (), v.get<double> ());
	}
	return data;
}

const OrdersData &ClassicalOrders ()
{
	static const OrdersData data = ParseClassicalOrders (ClassicalOrdersJson ());
	return data;
}

std::vector<ControlCheck> RecomputeControls (const OrdersData &data)
{
	static const std::string kHeights = "somme des hauteurs";
	static const std::string kProjection = "saillie maximale";
	static const std::string kWidth = "largeur en plan : ";

	std::vector<ControlCheck> checks;
	checks.reserve (data.controls.size ());
	for (const Control &control : data.controls)
	{
		ControlCheck check;
		check.control = &control;

		const OrderData *order = nullptr;
		for (const OrderData &o : data.orders)
			if (o.key == control.order)
				order = &o;
		const Block *block = order ? order->FindBlock (control.block) : nullptr;

		if (block && control.label == kHeights)
		{
			check.computable = true;
			for (const Member &m : block->members)
				if (!block->IsOutsideModule (m))
					check.recomputed += m.h;
		}
		else if (block && control.label == kProjection)
		{
			check.computable = true;
			for (const Member &m : block->members)
				check.recomputed = std::max (check.recomputed, MaxProjection (m));
		}
		else if (block && control.label.compare (0, kWidth.size (), kWidth) == 0)
		{
			const std::string name = control.label.substr (kWidth.size ());
			for (const Member &m : block->members)
				if (m.name == name && m.projection.kind == ProjectionKind::Constant)
				{
					check.computable = true;
					check.recomputed = 2.0 * (block->axisDistance + m.projection.main.bottom);
				}
		}

		if (check.computable)
		{
			check.expected = ParseExactNumber (control.expected);
			check.matches = Near (check.recomputed, check.expected);
		}
		checks.push_back (check);
	}
	return checks;
}

} // namespace classical
} // namespace cgarchi
