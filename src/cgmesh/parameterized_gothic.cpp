#include "parameterized_gothic.h"

#include "surface_architecture.h"
#include "architecture_gothic.h"
#include "mesh_slicing.h"
#include "mesh_validation.h"
#include <cmath>

#include <algorithm>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

// ===========================================================================
//  Architecture gothique
// ===========================================================================

// --- Gothic Block ----------------------------------------------------------
ParameterizedGothicBlock::ParameterizedGothicBlock() { Regenerate(); }
std::vector<Parameter> ParameterizedGothicBlock::GetParameters()
{
	return {
		Parameter::MakeFloat("Width",  &m_width,  0.1f, 20.f),
		Parameter::MakeFloat("Height", &m_height, 0.1f, 20.f),
		Parameter::MakeFloat("Depth",  &m_depth,  0.1f, 20.f),
		Parameter::MakeFloat("Bevel",  &m_bevel,  0.f,  0.5f),
	};
}
void ParameterizedGothicBlock::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateBlock(m_width, m_height, m_depth, m_bevel);
	m_pMesh->ComputeNormals();
}

// --- Gothic Window (remplage parametrique complet) -------------------------
namespace
{
	// Largeur du repere canonique : toute la chaine 2D/3D calcule a cette
	// largeur, l'echelle monde n'est appliquee qu'en sortie. Les constantes
	// relatives du generateur (grille de Clipper2, gardes de degenerescence)
	// valent ainsi pour toute largeur ce qu'elles valent a la largeur par defaut.
	constexpr double kCanonicalWidth = 200.0;

	Polygon2 scaledPolygon (const Polygon2 &poly, double s)
	{
		Polygon2 out;
		out.alloc_contours(poly.get_n_contours());
		std::vector<float> pts;
		for (int c = 0; c < poly.get_n_contours(); ++c)
		{
			const unsigned n = poly.get_n_points(c);
			const float *p = poly.get_points(c);
			pts.assign(p, p + 2 * n);
			for (float &x : pts) x = static_cast<float>(x * s);
			out.add_contour(static_cast<unsigned>(c), n, pts.data());
		}
		return out;
	}

	void scaleMesh (Mesh &m, double s)
	{
		std::vector<float> v = m.GetVertices();
		for (float &x : v) x = static_cast<float>(x * s);
		m.SetVertices(static_cast<unsigned>(v.size() / 3), v.data());
	}

	Profile2D scaledProfile (const Profile2D &profile, double k)
	{
		Profile2D p = profile;
		for (Vector2d &q : p.points) { q.x *= k; q.y *= k; }
		return p;
	}

	bool allFinite (const Profile2D &profile)
	{
		return std::all_of(profile.points.begin(), profile.points.end(), [](const Vector2d &q) {
			return std::isfinite(q.x) && std::isfinite(q.y);
		});
	}

	// Text of the exception being handled, prefixed by `where`. Call from a
	// catch block only.
	std::string currentExceptionText (const char *where)
	{
		try { throw; }
		catch (const std::exception &e) { return std::string(where) + ": " + e.what(); }
		catch (...) { return std::string(where) + ": unknown exception"; }
	}

	// Records ParameterClamped when the value used, rounded to float like the
	// setting, differs from the setting : a bound the setting already sits on
	// (to float rounding) is not reported.
	void noteClamp (GothicBuildReport *report, const char *name, float setting, double used,
	                const char *rule)
	{
		if (!report || static_cast<float>(used) == setting)
			return;
		std::ostringstream os;
		os << setting << " -> " << used << " (" << rule << ")";
		report->Record(GothicBuildWarning::ParameterClamped, name, os.str());
	}

	void noteClamp (GothicBuildReport *report, const char *name, int setting, int used,
	                const char *rule)
	{
		if (!report || used == setting)
			return;
		std::ostringstream os;
		os << setting << " -> " << used << " (" << rule << ")";
		report->Record(GothicBuildWarning::ParameterClamped, name, os.str());
	}

	// Defauts qui empechent `mesh` d'etre strict, "" s'il l'est : une
	// composante, aucune categorie de ValidateMesh, test d'auto-intersection non
	// tronque, soudure 5e-4 x largeur / 200 (la tolerance suit la baie). Une
	// feuille plane (`sheet`) admet ses aretes de bord.
	std::string strictDefects (const Mesh &mesh, double width, bool sheet)
	{
		MeshValidationOptions options;
		options.weldTolerance      = 5e-4 * width / 200.0;
		options.expectedComponents = 1u;
		const MeshValidationReport r = ValidateMesh(mesh, options);
		std::ostringstream defects;
		if (mesh.GetNFaces() <= 1u)
			defects << " no surface";
		if (r.components != 1u)
			defects << " components=" << r.components;
		for (std::size_t k = 0; k < kMeshDefectCount; ++k)
		{
			const auto kind = static_cast<MeshDefect>(k);
			if (!r.counts[k] || kind == MeshDefect::UnexpectedComponentCount || (sheet && kind == MeshDefect::BoundaryEdge))
				continue;
			defects << ' ' << MeshDefectName(kind) << '=' << r.counts[k];
		}
		if (r.selfIntersectionTruncated)
			defects << " self-intersection test truncated";
		return defects.str();
	}
}

// Garde de completude : GothicWindowSettings est un agregat de exactement
// kSettingsFieldCount champs (initialisable par autant de valeurs, pas une de
// plus). Un champ ajoute ou retire fait echouer la compilation : mettre alors a
// jour operator== ci-dessous, settings_equality_compares_every_field, puis ce
// compte. La taille ne suffit pas : un champ peut se loger dans le bourrage.
namespace
{
	constexpr std::size_t kSettingsFieldCount = 32;

	struct AnyField
	{
		template <class T> operator T () const;
	};

	template <class T, class... Args>
	constexpr auto braceInitializable (int) -> decltype(T{ std::declval<Args>()... }, true) { return true; }
	template <class T, class... Args>
	constexpr bool braceInitializable (...) { return false; }

	template <class T, std::size_t... I>
	constexpr bool initializableFrom (std::index_sequence<I...>)
	{
		return braceInitializable<T, decltype((void)I, AnyField{})...>(0);
	}
}
static_assert(initializableFrom<GothicWindowSettings>(std::make_index_sequence<kSettingsFieldCount>{})
              && !initializableFrom<GothicWindowSettings>(std::make_index_sequence<kSettingsFieldCount + 1>{}),
              "GothicWindowSettings a change : mettre a jour operator== et son test");

bool GothicWindowSettings::operator== (const GothicWindowSettings &o) const
{
	return width == o.width && excess == o.excess && offsetOuter == o.offsetOuter
	    && offsetInner == o.offsetInner && bodyHeight == o.bodyHeight
	    && subCount == o.subCount && subDrop == o.subDrop && subExcess == o.subExcess
	    && gapFraction == o.gapFraction && lancetLayout == o.lancetLayout
	    && recursion == o.recursion && lancetHead == o.lancetHead && lancetHeadFoils == o.lancetHeadFoils
	    && rosette == o.rosette && rosetteFoils == o.rosetteFoils && rosetteFoilCount == o.rosetteFoilCount
	    && rosetteFoilType == o.rosetteFoilType && rosetteFoilPointed == o.rosetteFoilPointed
	    && subFoils == o.subFoils && subFoilCount == o.subFoilCount && subFoilType == o.subFoilType
	    && subFoilPointed == o.subFoilPointed
	    && archTrefoil == o.archTrefoil && trefoilSplit == o.trefoilSplit && trefoilFoilRadius == o.trefoilFoilRadius
	    && mouchettes == o.mouchettes && mouchetteType == o.mouchetteType && mouchetteRadius == o.mouchetteRadius
	    && fillets == o.fillets && zHeight == o.zHeight && maxAngleRad == o.maxAngleRad
	    && profile == o.profile;
}

ParameterizedGothicWindow::ParameterizedGothicWindow() { Regenerate(); }

std::vector<Parameter> ParameterizedGothicWindow::GetParameters()
{
	return {
		// arc + corps
		Parameter::MakeFloat("Excess (arch)", &m_settings.excess, 0.7f, 2.5f),
		Parameter::MakeFloat("Width",         &m_settings.width, 20.f, 600.f),
		Parameter::MakeFloat("Body height",   &m_settings.bodyHeight, 0.f, 600.f),
		Parameter::MakeFloat("Offset outer",  &m_settings.offsetOuter, 0.f, 40.f),
		Parameter::MakeFloat("Offset inner",  &m_settings.offsetInner, 0.f, 40.f),
		// lancettes : figé à 2 pour l'instant (le modèle de la these ne definit la
		// rosette/tangence que pour deux sous-fenetres) -> pas exposé dans l'UI.
		Parameter::MakeEnum ("Lancet head",   &m_settings.lancetHead, {"Plain","Foiled"}),
		Parameter::MakeInt  ("Lancet head foils", &m_settings.lancetHeadFoils, 3, 8),
		Parameter::MakeInt  ("Recursion",     &m_settings.recursion, 0, 2),
		Parameter::MakeFloat("Lancet drop",   &m_settings.subDrop, 0.f, 100.f),
		Parameter::MakeFloat("Lancet excess", &m_settings.subExcess, 0.7f, 2.5f),
		Parameter::MakeFloat("Gap fraction",  &m_settings.gapFraction, 0.01f, 0.2f),
		Parameter::MakeEnum ("Lancet layout", &m_settings.lancetLayout, {"Uniform","Prototype"}),
		// rosette + foils (les foils de LANCETTE ne sont pas exposes : les
		// lancettes sont de simples arcs brises hauts, cf. buildBayStonePolygon)
		Parameter::MakeBool ("Fillets",             &m_settings.fillets),
		Parameter::MakeBool ("Mouchettes",          &m_settings.mouchettes),
		Parameter::MakeEnum ("Mouchette type",      &m_settings.mouchetteType, {"Vesica","Teardrop","Soufflet"}),
		Parameter::MakeBool ("Rosette",             &m_settings.rosette),
		Parameter::MakeBool ("Rosette foils",       &m_settings.rosetteFoils),
		Parameter::MakeInt  ("Rosette foil count",  &m_settings.rosetteFoilCount, 3, 24),
		Parameter::MakeEnum ("Rosette foil type",   &m_settings.rosetteFoilType, {"Round","Pointed"}),
		Parameter::MakeFloat("Rosette pointedness", &m_settings.rosetteFoilPointed, 0.01f, 2.f),
		// extrusion + profil de moulure (trefoil/mouchettes non exposes)
		Parameter::MakeFloat("Extrusion (zHeight)", &m_settings.zHeight, 0.f, 100.f),
		Parameter::MakeEnum ("Profile",             &m_settings.profile, {"Flat","Chamfer","Roll bar","Keel bar","Ogee bar"}),
	};
}

// Correspondance des parametres de la baie : instance cgmath -> geometrie +
// parametres de maillage. Partagee par Regenerate et BuildStonePolygon, pour que
// la region validee soit celle qui est extrudee. Leve sur une combinaison invalide.
//
// Repere canonique : la largeur y vaut exactement kCanonicalWidth et toutes les
// longueurs (offsets, drop, corps, extrusion, seuils) y sont divisees par
// l'echelle s = largeur / kCanonicalWidth. A s = 1, les divisions sont exactes.
double ParameterizedGothicWindow::prepareBay (WindowGeometry &geom, GothicMeshParams &mp,
                                              GothicBuildReport *report) const
{
	WindowInstance wi;

	// --- reglages et seuils flottants : un non-fini est une erreur, avant tout
	// calcul (les clamps ci-dessous laissent passer NaN : std::min / std::max et
	// les comparaisons sont fausses sur NaN). Chaque reglage lu par la baie est
	// controle, qu'il ait un effet ou non dans la configuration courante, et
	// meme s'il n'est jamais lu (pointe des foils de lancette, trefle, rayon de
	// mouchette) : un NaN y rendrait MatchesLastRegenerate faux pour toujours.
	{
		const GothicFeatureThresholds &t = m_thresholds;
		const std::pair<const char *, double> floats[] = {
			{"Width", m_settings.width}, {"Excess (arch)", m_settings.excess},
			{"Body height", m_settings.bodyHeight}, {"Offset outer", m_settings.offsetOuter},
			{"Offset inner", m_settings.offsetInner}, {"Lancet drop", m_settings.subDrop},
			{"Lancet excess", m_settings.subExcess}, {"Gap fraction", m_settings.gapFraction},
			{"Rosette pointedness", m_settings.rosetteFoilPointed},
			{"subFoilPointed", m_settings.subFoilPointed}, {"trefoilSplit", m_settings.trefoilSplit},
			{"trefoilFoilRadius", m_settings.trefoilFoilRadius}, {"mouchetteRadius", m_settings.mouchetteRadius},
			{"Extrusion (zHeight)", m_settings.zHeight}, {"maxAngleRad", m_settings.maxAngleRad},
			{"thresholds.minRosetteRadius", t.minRosetteRadius},
			{"thresholds.minRingWidth", t.minRingWidth},
			{"thresholds.minEyeletInset", t.minEyeletInset},
			{"thresholds.minSubRosetteRadius", t.minSubRosetteRadius},
			{"thresholds.minMouchetteRadius", t.minMouchetteRadius},
			{"thresholds.minFilletInnerOffset", t.minFilletInnerOffset},
			{"thresholds.maxMouchetteInset", t.maxMouchetteInset},
			{"thresholds.chamferWidthAdd", t.chamferWidthAdd},
			{"thresholds.minChamferWidth", t.minChamferWidth},
			{"thresholds.barRadiusAdd", t.barRadiusAdd},
			{"thresholds.minBarRadius", t.minBarRadius},
			{"thresholds.minBand", t.minBand} };
		std::string bad;
		for (const auto &[name, value] : floats)
		{
			if (std::isfinite(value))
				continue;
			if (report)
				report->Record(GothicBuildError::InvalidParameter, name, "non-finite value");
			bad += bad.empty() ? std::string(name) : std::string(", ") + name;
		}
		if (!bad.empty())
			throw std::invalid_argument("ParameterizedGothicWindow: valeur non finie (" + bad + ")");
	}

	// --- arc : pL/pR centres a la base, largeur canonique ---
	const double width = (m_settings.width < 1.0) ? 1.0 : (double)m_settings.width;
	noteClamp(report, "Width", m_settings.width, width, "minimum 1");
	const double s     = width / kCanonicalWidth;
	const double w     = kCanonicalWidth;
	const GothicFeatureThresholds thr = m_thresholds.Scaled(1.0 / s);
	wi.archBasis.pL = Vector2d(-w * 0.5, 0.0);
	wi.archBasis.pR = Vector2d( w * 0.5, 0.0);
	double excess = std::min(std::max((double)m_settings.excess, 0.7), 2.5);
	noteClamp(report, "Excess (arch)", m_settings.excess, excess, "range [0.7, 2.5]");
	wi.archBasis.excess = excess;

	// --- lancettes (calculees AVANT l'offset : elles bornent l'epaisseur inner) ---
	// Figé à 2 pour l'instant (cf. these : rosette/tangence definies pour deux
	// sous-fenetres seulement). Le reglage subCount n'est plus exposé dans l'UI.
	int count = 2;
	wi.subwindowParams.count  = count;
	wi.subwindowParams.drop   = (m_settings.subDrop < 0.f) ? 0.0 : (double)m_settings.subDrop / s;
	noteClamp(report, "Lancet drop", m_settings.subDrop, wi.subwindowParams.drop * s, "minimum 0");
	double subExcess = std::min(std::max((double)m_settings.subExcess, 0.7), 2.5);
	noteClamp(report, "Lancet excess", m_settings.subExcess, subExcess, "range [0.7, 2.5]");
	wi.subwindowParams.excess = subExcess;
	wi.subwindowParams.layout = (m_settings.lancetLayout == 1)
		? SubwindowParams::Layout::Prototype
		: SubwindowParams::Layout::Uniform;
	const bool prototype = wi.subwindowParams.layout == SubwindowParams::Layout::Prototype;

	// --- offset outer : bande du cadre, au moins thresholds.minBand. En
	// Prototype, les pieds des lancettes sont a outer des pieds de l'arc :
	// borne a 0,4 x largeur, il laisse aux lancettes et au meneau 0,2 x largeur.
	double outer = (m_settings.offsetOuter < 0.f) ? 0.0 : (double)m_settings.offsetOuter / s;
	const char *outerRule = "minimum 0";
	if (outer < thr.minBand)
	{
		outer     = thr.minBand;
		outerRule = "minimum thresholds.minBand";
	}
	if (prototype && outer > 0.4 * w)
	{
		outer     = 0.4 * w;
		outerRule = "Prototype maximum 0.4 x Width";
	}
	wi.archOffset.outer = outer;
	noteClamp(report, "Offset outer", m_settings.offsetOuter, outer * s, outerRule);
	wi.subwindowParams.gap.mode = SubwindowParams::Gap::Mode::Fraction;
	double gapMax = 0.6 / (count + 1);   // conservateur : garde >= 40% de largeur aux lancettes
	double gf = (double)m_settings.gapFraction;
	if (gf < 0.001) gf = 0.001;
	if (gf > gapMax) gf = gapMax;
	noteClamp(report, "Gap fraction", m_settings.gapFraction, gf, "range [0.001, 0.6 / (count + 1)]");
	wi.subwindowParams.gap.gapFraction = gf;

	// --- offset inner : borne par l'arc principal ET par les lancettes ---
	// Un inner trop grand fait "collapser" un sous-arc -> contour degenere qui
	// fait BOUCLER la tessellation GLU (non rattrapable par try/catch). On borne
	// donc contre le rayon de lancette (le plus petit), avec marge 0.8.
	// Thesis convention : the concentric inner offset stays a valid arch while
	// inner < openingWidth/2 (main) and < lancetWidth/2 (each lancet). Margin 0.8.
	double lancetWidth    = w * (1.0 - (count + 1) * gf) / (double)count;
	double innerMaxMain   = 0.8 * 0.5 * w;
	double innerMaxLancet = 0.8 * 0.5 * lancetWidth;
	double innerMax = std::min(innerMaxMain, innerMaxLancet);
	const char *innerMaxRule = "maximum 0.8 x half the narrowest opening";
	// Prototype : le meneau vaut l'offset inner et chacune des deux lancettes a
	// (w - 2 outer - inner) / 2 de large ; son propre offset inner reste sous sa
	// demi-largeur (buildArchOffset) tant que inner < (w - 2 outer) / 5. Borne
	// avec marge : (w - 2 outer) / 6. Le meneau est au moins thresholds.minBand.
	if (prototype && (w - 2.0 * outer) / 6.0 < innerMax)
	{
		innerMax     = (w - 2.0 * outer) / 6.0;
		innerMaxRule = "Prototype maximum (Width - 2 x Offset outer) / 6";
	}
	double inner = (m_settings.offsetInner < 0.f) ? 0.0 : (double)m_settings.offsetInner / s;
	const char *innerRule = "minimum 0";
	if (prototype && inner < thr.minBand)
	{
		inner     = thr.minBand;
		innerRule = "Prototype mullion minimum thresholds.minBand";
	}
	if (innerMax > 0.0 && inner > innerMax)
	{
		inner     = innerMax;
		innerRule = innerMaxRule;
	}
	noteClamp(report, "Offset inner", m_settings.offsetInner, inner * s, innerRule);
	wi.archOffset.inner = inner;

	// --- rosette + foils ---
	// REGLE : une rosette n'existe QUE si elle couronne exactement DEUX lancets
	// (elle est construite tangente a ces deux lancets, cf. gothic1.png). Donc
	// pas de rosette a count != 2 : a count==1 la construction degenere / boucle,
	// et a count>=3 il n'y a pas la paire de lancets a laquelle etre tangente.
	// (La case "Rosette" reste sans effet tant qu'on n'a pas 2 lancets.)
	bool wantRosette = m_settings.rosette && (count == 2);
	wi.hasRosette = wantRosette;
	if (m_settings.rosetteFoils && wantRosette)
	{
		wi.hasRosetteFoils = true;
		wi.rosetteFoils.count = std::min(std::max(m_settings.rosetteFoilCount, 3), 24);
		// Always build the cgmath foil ring as ROUND : the maker rendering
		// builds its own pointed petals (pointedFoilPetals), so we only need
		// count + outerCircle from cgmath. Forcing Round avoids buildFoilRing
		// throwing on extreme pointedness (which made the whole window vanish).
		wi.rosetteFoils.type  = FoilType::Round;
		// phi0 = +pi/2 : premier foil mesure depuis l'axe +Y -> un foil en HAUT
		// (trilobe = 1 en haut + 2 en bas, orientation gothique classique).
		wi.rosetteFoils.phi0 = 1.57079632679489661923;
	}
	// Foils de LANCETTE : plus construits ni exposes. Les lancettes sont de
	// simples arcs brises hauts (fut + tete pointue) ; le remplage folie
	// d'une lancette (grand vide + tete festonnee) exigerait un vrai booleen
	// 2D, cf. buildBayStonePolygon. m_settings.subFoils/... restent dans les reglages.
	wi.hasSubwindowFoils = false;

	// L'instance cgmath ne porte ni trefle, ni mouchettes, ni ecoincons : les
	// ecoincons et les mouchettes sont decoupes par buildBayStonePolygon, a
	// partir de GothicMeshParams (mp.fillets, mp.mouchettes, ci-dessous). Le
	// trefle de l'arc principal n'est construit nulle part : m_settings.archTrefoil et
	// ses parametres ne servent qu'a l'aller-retour JSON.

	// --- geometrie -> polygone pierre -> mesh (extrude si zHeight>0) ---
	// Une rosace que cgmath ne sait pas construire (buildRosette, ou son anneau
	// de lobes) est abandonnee : la baie est construite sans elle, et
	// feature_dropped nomme « Rosette ». Si la baie sans rosace leve aussi,
	// l'echec n'etait pas celui de la rosace : son exception est propagee.
	try
	{
		geom = buildGeometryFromInstance(wi);
	}
	catch (const std::exception &)
	{
		if (!wi.hasRosette)
			throw;
		const std::string why = currentExceptionText("rosette");
		WindowInstance plain = wi;
		plain.hasRosette      = false;
		plain.hasRosetteFoils = false;
		geom = buildGeometryFromInstance(plain);
		if (report)
			report->Record(GothicBuildWarning::FeatureDropped, "Rosette", why);
	}

	// La rosette de cgmath est tangente aux arcs de BASE, mais l'ouverture
	// reelle en pierre est l'offset INTERIEUR (mainOffset.inner + sous-arcs
	// offset.inner), plus petit : la rosette de base debordait donc du cadre.
	// On la recalcule tangente a CES arcs interieurs, pour qu'elle touche le
	// lancet principal ET les lancets inferieurs (cf. gothic1.png) au lieu de
	// flotter avec un jour tout autour. Repli sur l'ancien retrecit-a-centre-
	// fixe si la construction tangente degenere.
	if (geom.hasRosette && !geom.subwindows.lancets.empty())
	{
		// Oculus passing through the lancet apexes (touches the drawn lancet
		// tops) and tangent to the main arch. RULE : it exists only if it can
		// REST ON the two lancets -> its centre must be ABOVE their apexes. If
		// the head is too shallow (centre below the apexes, it would hang wedged
		// between the lancets with nothing supporting it), drop the rosette.
		Vector2d apex = geom.subwindows.lancets.back().offset.inner.apex;
		Circle rc = rosetteTangentToLancets(geom.mainOffset.inner, 0.0, apex);
		if (!(rc.radius > thr.minRosetteRadius && rc.center.y > apex.y) && report)
		{
			std::ostringstream os;
			os << "rosette: ";
			if (rc.radius <= 0.0)
				os << "no circle tangent to the arch through the lancet apexes";
			else if (rc.radius <= thr.minRosetteRadius)
				os << "radius " << rc.radius * s << " <= threshold " << m_thresholds.minRosetteRadius;
			else
				os << "centre not above the lancet apexes";
			report->Record(GothicBuildWarning::FeatureDropped, "Rosette", os.str());
		}
		if (rc.radius > thr.minRosetteRadius && rc.center.y > apex.y)
		{
			geom.rosette.center = rc.center;
			geom.rosette.radius = rc.radius;
			geom.rosette.circle = rc;
			if (geom.hasRosetteFoils)
				geom.rosetteFoils = buildFoilRing(rc, wi.rosetteFoils);
		}
		else
		{
			geom.hasRosette      = false;   // ne peut pas prendre appui
			geom.hasRosetteFoils = false;
		}
	}

	mp.maxAngleRad = (m_settings.maxAngleRad > 1e-4) ? m_settings.maxAngleRad : (3.14159265358979323846 / 180.0);
	if (report && mp.maxAngleRad != m_settings.maxAngleRad)
	{
		std::ostringstream os;
		os << m_settings.maxAngleRad << " -> " << mp.maxAngleRad << " (minimum 1e-4, else 1 degree)";
		report->Record(GothicBuildWarning::ParameterClamped, "maxAngleRad", os.str());
	}
	mp.zHeight    = (m_settings.zHeight    < 0.f) ? 0.0 : (double)m_settings.zHeight / s;
	noteClamp(report, "Extrusion (zHeight)", m_settings.zHeight, mp.zHeight * s, "minimum 0");
	mp.bodyHeight = (m_settings.bodyHeight < 0.f) ? 0.0 : (double)m_settings.bodyHeight / s;
	// Le pied des lancettes est a drop sous leur naissance, l'appui des lancettes
	// a corps - offset exterieur (bande d'appui de buildBayStonePolygon) : un drop
	// plus grand ferait sortir les lancettes, et leur sous-remplage, du cadre. Le
	// corps est alors porte a drop + offset exterieur.
	const bool bodyRaised = wi.subwindowParams.drop > mp.bodyHeight - wi.archOffset.outer;
	if (bodyRaised)
		mp.bodyHeight = wi.subwindowParams.drop + wi.archOffset.outer;
	noteClamp(report, "Body height", m_settings.bodyHeight, mp.bodyHeight * s,
	          bodyRaised ? "minimum Lancet drop + Offset outer" : "minimum 0");
	mp.thresholds = thr;
	// Recursion : reuse the same offset + subdivision on each sub-lancet.
	mp.recursionDepth  = std::min(std::max(m_settings.recursion, 0), 2);
	noteClamp(report, "Recursion", m_settings.recursion, mp.recursionDepth, "range [0, 2]");
	mp.recursionOffset = wi.archOffset;
	mp.recursionSub    = wi.subwindowParams;
	// Sub-rosettes use the SAME motif AND foil count as the main rosette
	// (a sub-window gets one only if it subdivides into exactly two sub-lancets,
	// enforced in collectUnitVoids).
	mp.recursionFoils  = std::min(std::max(m_settings.rosetteFoilCount, 3), 24);
	noteClamp(report, "Rosette foil count", m_settings.rosetteFoilCount, mp.recursionFoils, "range [3, 24]");
	// Fillets (Phase 1) : corner fields via Clipper2, inset by the inner offset,
	// floored at thresholds.minFilletInnerOffset (monotone in the inner offset).
	mp.fillets     = m_settings.fillets;
	mp.filletInset = std::max((double)m_settings.offsetInner, m_thresholds.minFilletInnerOffset) / s;
	// Flamboyant : shape the spandrel fields as mouchettes/soufflets (needs fillets on).
	// Use a THIN frame bar (small inset) so the field ≈ the full spandrel and the
	// inscribed mouchette nearly fills it, leaving only a slim stone bar around it.
	mp.mouchettes    = m_settings.mouchettes;
	mp.mouchetteType = std::min(std::max(m_settings.mouchetteType, 0), 2);
	noteClamp(report, "Mouchette type", m_settings.mouchetteType, mp.mouchetteType, "range [0, 2]");
	mp.mouchetteSize = kMouchetteSize;
	if (m_settings.mouchettes) mp.filletInset = std::min(mp.filletInset, thr.maxMouchetteInset);
	// Foiled lancet heads (Phase 3).
	mp.lancetHeadFoiled = (m_settings.lancetHead == 1);
	mp.lancetHeadFoils  = std::min(std::max(m_settings.lancetHeadFoils, 3), 8);
	noteClamp(report, "Lancet head foils", m_settings.lancetHeadFoils, mp.lancetHeadFoils, "range [3, 8]");
	// Rosette foil shape (Round/Pointed) + pointedness of the pointed petals.
	mp.rosetteFoilType    = m_settings.rosetteFoilType;
	mp.rosettePointedness = std::min(std::max((double)m_settings.rosetteFoilPointed, 0.0), 2.0);
	noteClamp(report, "Rosette pointedness", m_settings.rosetteFoilPointed, mp.rosettePointedness, "range [0, 2]");
	return s;
}

Polygon2 ParameterizedGothicWindow::BuildStonePolygon (GothicBuildReport *report) const
{
	WindowGeometry   geom;
	GothicMeshParams mp;
	const double s = prepareBay(geom, mp, report);
	Polygon2 poly = buildBayStonePolygon(geom, mp, report);
	return (s == 1.0) ? poly : scaledPolygon(poly, s);
}

Profile2D ParameterizedGothicWindow::barProfile (const GothicMeshParams &mp, double s) const
{
	if (m_hasBarProfile)
		return scaledProfile(m_barProfile, 1.0 / s);       // le PORT l'emporte sur le menu
	if (m_settings.profile < 2)
		return Profile2D();
	const double innerCanonical = (double)m_settings.offsetInner / s;
	double rb = std::min(0.5 * mp.filletInset,
	                     0.45 * innerCanonical + mp.thresholds.barRadiusAdd);
	if (rb < mp.thresholds.minBarRadius) rb = mp.thresholds.minBarRadius;
	// Trois positions du menu, trois valeurs de la meme famille --
	// bourdon, arete, doucine (Havemann §5.4.1 « french style »,
	// Fig 5.28 d).
	if      (m_settings.profile == 3) return keelBarProfile (rb);
	else if (m_settings.profile == 4) return ogeeBarProfile (rb);
	return rollBarProfile (rb);
}

bool ParameterizedGothicWindow::BuildFusedBay (Mesh &out, FusedBarsResult *result, GothicBuildReport *report) const
{
	out = Mesh();
	if (result)
		*result = FusedBarsResult();
	WindowGeometry   geom;
	GothicMeshParams mp;
	const double s = prepareBay(geom, mp, report);
	if (mp.zHeight <= 0.0 || m_hasSplayProfile || m_settings.profile == 1)
		return false;
	const Profile2D bar = barProfile(mp, s);
	if (bar.empty())
		return false;
	if (m_hasBarProfile && !allFinite(m_barProfile))
		throw std::invalid_argument("ParameterizedGothicWindow: profil de barre non fini");
	const Polygon2 poly = buildBayStonePolygon(geom, mp, report);
	const std::vector<FusedBar> bars = collectBayMouldingOutlines(geom, mp, bar.points, report);
	const FusedBarsResult fused = fuseBarsOnPlate(poly, bars, bar, mp.zHeight, out);
	if (result)
		*result = fused;
	if (fused.failed)
		return false;
	if (s != 1.0)
		scaleMesh(out, s);
	out.ComputeNormals();
	return true;
}

void ParameterizedGothicWindow::Regenerate()
{
	delete m_pMesh;
	m_pMesh = nullptr;
	m_lastReport = GothicBuildReport();
	m_lastRegenerate = {m_settings, m_thresholds, m_hasSplayProfile, m_hasBarProfile, m_splayProfile, m_barProfile};

	auto placeholder = [&]() -> Mesh* {
		std::vector<float>        v = {0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f};
		std::vector<unsigned int> f = {0u,1u,2u};
		Mesh* m = new Mesh();
		m->SetVertices(3, v.data());
		m->SetFaces(1, 3, f.data());
		return m;
	};

	GothicBuildReport report;
	try
	{
		WindowGeometry   geom;
		GothicMeshParams mp;
		// Profils explicites : seul Regenerate les lit ; un point non fini est
		// refuse comme un reglage non fini.
		bool badProfile = false;
		for (const auto &[name, has, profile] : {std::make_tuple("Splay profile", m_hasSplayProfile, &m_splayProfile),
		                                         std::make_tuple("Bar profile",   m_hasBarProfile,   &m_barProfile)})
		{
			if (!has || allFinite(*profile))
				continue;
			report.Record(GothicBuildError::InvalidParameter, name, "non-finite point");
			badProfile = true;
		}
		const double s = prepareBay(geom, mp, &report);
		if (badProfile)
			throw std::invalid_argument("ParameterizedGothicWindow: profil explicite non fini");
		const double innerCanonical = (double)m_settings.offsetInner / s;
		Polygon2 poly = buildBayStonePolygon(geom, mp, &report);
		const bool regionInvalid = report.Count(GothicBuildWarning::RegionInvalid) > 0;
		std::unique_ptr<Mesh> plate;   // copie de la pierre seule, quand la region est invalide

		auto m = std::make_unique<Mesh>();
		if (mp.zHeight > 0.0)
		{
			// Profile "Chamfer" (Phase 2) : bevelled/splayed openings for the 3D
			// carved-stone look. Falls back to a flat extrusion if the per-vertex
			// offset degenerates (thin bars / sharp corners).
			bool profiled = false;
			Profile2D splay;
			if (m_hasSplayProfile)
				splay = scaledProfile(m_splayProfile, 1.0 / s);   // le PORT l'emporte sur le menu
			else if (m_settings.profile == 1)
			{
				// Position « Chamfer » du menu : un chanfrein droit dont la
				// largeur et la profondeur se derivent des offsets de la baie.
				// C'est desormais UNE valeur de profil parmi d'autres, et non
				// plus un cas particulier du code d'extrusion.
				double chamW = std::min(0.6 * mp.filletInset,
				                        0.45 * innerCanonical + mp.thresholds.chamferWidthAdd);
				if (chamW < mp.thresholds.minChamferWidth) chamW = mp.thresholds.minChamferWidth;
				double chamD = std::min(0.5 * mp.zHeight, 3.0 * chamW);
				splay = chamferSplayProfile (chamW, chamD);
			}
			if (!splay.empty())
			{
				std::vector<ProfileReduction> reductions;
				try
				{
					extrudeProfiledToMesh(poly, splay, *m, 0.0, mp.zHeight, &reductions);
					profiled = true;
					for (const ProfileReduction &r : reductions)
					{
						std::ostringstream msg;
						msg << "contour " << r.contour << ": ";
						if (r.widthFactor > 0.0) msg << "width x " << r.widthFactor;
						else                     msg << "vertical wall";
						report.Record(GothicBuildWarning::ProfileReduced, "Profile", msg.str());
					}
				}
				catch (...)
				{
					profiled = false;
					report.Record(GothicBuildWarning::ProfileFallback, "Profile",
					              currentExceptionText("splay profile"));
				}
			}
			// La plaque extrudee n'est construite que si les barres ne seront pas
			// fusionnees (la fusion porte sa propre plaque), ou pour le verdict
			// d'une region invalide, juge sur elle ; sinon au repli sur O1.
			const Profile2D bar = barProfile(mp, s);
			bool platePending = !profiled;
			auto ensurePlate = [&]()
			{
				if (!platePending)
					return;
				extrudeToMesh(poly, *m, 0.0, mp.zHeight);
				platePending = false;
			};
			if (!splay.empty() || bar.empty() || regionInvalid)
				ensurePlate();
			if (regionInvalid)
			{
				plate = std::make_unique<Mesh>();
				appendMesh(*plate, *m);
			}

			// Profile bars (Phase 2b, Havemann §5.4.1 "french style", Fig 5.28 d) : on
			// top of the flat plate, sweep a bar cross-section along the ACTUAL opening
			// outlines (main frame, lancets incl. foiled heads + jambs, recursion sub-
			// tracery, rosette rim) via buildBayMoulding. Profile library : 2 = Roll
			// (half-round bead), 3 = Keel (pointed ridge), 4 = Ogee (symmetric cyma).
			// Corner mitring is deferred (open joints for now).
			// La section de barre est RELATIVE a la face avant ; c'est ici,
			// et non chez son auteur, qu'elle est portee a la cote zF. Le code
			// d'origine l'ecrivait en z absolu, ce qui liait chaque section a la
			// profondeur d'extrusion de CETTE baie et interdisait de la produire
			// ailleurs.
			if (!bar.empty())
			{
				const Profile2D placed = translatedInU (bar, mp.zHeight);
				std::vector<FusedBar> bars;
				bool collected = false;
				bool fused = false;
				try
				{
					bars = collectBayMouldingOutlines(geom, mp, placed.points, &report);
					collected = true;
					// Les barres sont fusionnees a la plaque en une seule coque
					// (fuseBarsOnPlate), sauf avec un ebrasement, que la fusion ne
					// combine pas. Sans fusion, ou si elle echoue (cause nommee par
					// moulding_not_fused), elles sont posees sur la plaque (O1). Des
					// gradins sautes par la garde d'anneau sont nommes par
					// moulding_simplified (recordFusedBarsOutcome).
					if (!bars.empty() && splay.empty())
					{
						Mesh merged;
						FusedBarsResult r;
						std::string thrown;
						try
						{
							r = fuseBarsOnPlate(poly, bars, bar, mp.zHeight, merged);
						}
						catch (...)
						{
							thrown = currentExceptionText("fusion");
						}
						if (!thrown.empty())
						{
							r.failed = true;
							report.Record(GothicBuildWarning::MouldingNotFused, "Profile",
							              "bars laid on the plate instead: " + thrown);
						}
						else
						{
							if (!r.failed && merged.GetNFaces() == 0)
							{
								r.failed  = true;
								r.failure = "empty fused mesh";
							}
							recordFusedBarsOutcome(r, &report);
						}
						fused = !r.failed;
						if (fused)
						{
							*m = Mesh();
							appendMesh(*m, merged);
						}
					}
				}
				catch (...)   // degenerate outline : keep the flat plate only
				{
					report.Record(GothicBuildWarning::MouldingDropped, "Profile",
					              currentExceptionText("moulding"));
				}
				if (!fused)
				{
					// Hors du traitement des moulures : une plaque qui ne s'extrude
					// pas est une erreur de construction, comme sans barre.
					ensurePlate();
					if (collected)
					{
						try
						{
							Mesh beads;
							sweepBayMoulding(bars, placed.points, beads, &report);
							appendMesh(*m, beads);
						}
						catch (...)
						{
							report.Record(GothicBuildWarning::MouldingDropped, "Profile",
							              currentExceptionText("moulding"));
						}
					}
				}
			}
		}
		else tessellateToMesh(poly, *m, 0.0);
		if (s != 1.0) scaleMesh(*m, s);
		if (regionInvalid)
		{
			if (plate && s != 1.0) scaleMesh(*plate, s);
			const std::string defects = strictDefects(plate ? *plate : *m, s * kCanonicalWidth, mp.zHeight <= 0.0);
			if (!defects.empty())
				report.Record(GothicBuildError::MeshNotStrict, std::string(),
				              "level-0 region invalid and stone mesh not strict:" + defects);
		}
		m->ComputeNormals();
		m_pMesh = m.release();
	}
	catch (...)
	{
		// Combinaison de parametres invalide (les buildXxx lancent) -> placeholder.
		// Le rapport garde les avertissements deja enregistres et porte l'erreur ;
		// un reglage deja refuse (invalid_parameter) n'en ajoute pas d'autre.
		if (!report.HasErrors())
			report.Record(GothicBuildError::ConstructionFailed, std::string(),
			              currentExceptionText("ParameterizedGothicWindow"));
		m_pMesh = placeholder();
	}
	m_lastReport = report;
}

void ParameterizedGothicWindow::SetSplayProfile (const Profile2D *profile)
{
	m_hasSplayProfile = (profile != nullptr) && !profile->empty();
	m_splayProfile = m_hasSplayProfile ? *profile : Profile2D();
}

void ParameterizedGothicWindow::SetBarProfile (const Profile2D *profile)
{
	m_hasBarProfile = (profile != nullptr) && !profile->empty();
	m_barProfile = m_hasBarProfile ? *profile : Profile2D();
}

GothicExportCheck CheckGothicExport (const ParameterizedGothicWindow &window, const Mesh &mesh)
{
	GothicExportCheck check;
	const GothicBuildReport &report = window.LastBuildReport();
	if (report.HasErrors())
	{
		check.message = "build error";
		for (const GothicDiagnostic &d : report.entries)
			if (d.error)
			{
				check.message = std::string(GothicBuildErrorName(static_cast<GothicBuildError>(d.code)))
				              + (d.parameter.empty() ? std::string() : " (" + d.parameter + ")") + ": " + d.message;
				break;
			}
		return check;
	}
	check.allowed = true;
	const std::string defects = strictDefects(mesh, std::max(1.0, static_cast<double>(window.Settings().width)), false);
	check.strict = defects.empty();
	if (!check.strict)
		check.message = "mesh not strict:" + defects;
	return check;
}

bool ParameterizedGothicWindow::HasReliefProfile () const
{
	// Memes conditions que Regenerate : relief seulement sur une plaque extrudee
	// (zHeight > 0) ; ebrasement si port ou position 1, barres si port ou
	// position >= 2 (barProfile).
	return m_settings.zHeight > 0.f && (m_hasSplayProfile || m_hasBarProfile || m_settings.profile >= 1);
}

bool ParameterizedGothicWindow::HasBarRelief () const
{
	// Memes conditions que barProfile : port de barre, ou position >= 2.
	return m_settings.zHeight > 0.f && (m_hasBarProfile || m_settings.profile >= 2);
}

double ParameterizedGothicWindow::ReliefOverhang () const
{
	WindowGeometry   geom;
	GothicMeshParams mp;
	const double s = prepareBay(geom, mp, nullptr);
	if (mp.zHeight <= 0.0)
		return 0.0;
	// Profil de barre canonique, relatif a la face avant : u au-dessus d'elle,
	// v de part et d'autre de la silhouette balayee.
	double extent = 0.0;
	for (const Vector2d &q : barProfile(mp, s).points)
		extent = std::max({extent, std::fabs(q.x), std::fabs(q.y)});
	return extent * s;
}

namespace
{
	// Garde de completude : GothicFeatureThresholds est fait de 12 double.
	static_assert(sizeof(GothicFeatureThresholds) == 12 * sizeof(double),
	              "GothicFeatureThresholds a change : mettre a jour sameThresholds");

	bool sameThresholds (const GothicFeatureThresholds &a, const GothicFeatureThresholds &b)
	{
		return a.minRosetteRadius == b.minRosetteRadius && a.minRingWidth == b.minRingWidth
		    && a.minEyeletInset == b.minEyeletInset && a.minSubRosetteRadius == b.minSubRosetteRadius
		    && a.minMouchetteRadius == b.minMouchetteRadius && a.minFilletInnerOffset == b.minFilletInnerOffset
		    && a.maxMouchetteInset == b.maxMouchetteInset && a.chamferWidthAdd == b.chamferWidthAdd
		    && a.minChamferWidth == b.minChamferWidth && a.barRadiusAdd == b.barRadiusAdd
		    && a.minBarRadius == b.minBarRadius && a.minBand == b.minBand;
	}

	bool sameProfile (const Profile2D &a, const Profile2D &b)
	{
		return a.points.size() == b.points.size()
		    && std::equal(a.points.begin(), a.points.end(), b.points.begin(),
		                  [](const Vector2d &p, const Vector2d &q) { return p.x == q.x && p.y == q.y; });
	}
}

bool ParameterizedGothicWindow::MatchesLastRegenerate () const
{
	const Snapshot &s = m_lastRegenerate;
	return m_settings == s.settings && sameThresholds(m_thresholds, s.thresholds)
	    && m_hasSplayProfile == s.hasSplay && sameProfile(m_splayProfile, s.splay)
	    && m_hasBarProfile == s.hasBar && sameProfile(m_barProfile, s.bar);
}

const char *GothicPrintScopeName (GothicPrintScope scope)
{
	switch (scope)
	{
	case GothicPrintScope::NotRun:            return "not_run";
	case GothicPrintScope::Complete:          return "complete";
	case GothicPrintScope::ProfileNotChecked: return "profile_not_checked";
	}
	return "unknown";
}

bool GothicPrintCheck::IsPrintable () const
{
	return geometry.allowed && geometry.strict && scope == GothicPrintScope::Complete && print.IsPrintable();
}

namespace
{
	// Contours de la region de pierre, trous compris, tels quels (orientation
	// NonZero de buildBayStonePolygon).
	std::vector<ExtrudeContour> stoneContours (const Polygon2 &poly)
	{
		std::vector<ExtrudeContour> out(static_cast<std::size_t>(poly.get_n_contours()));
		for (int c = 0; c < poly.get_n_contours(); ++c)
		{
			const float *p = poly.get_points(c);
			const int n = poly.get_n_points(c);
			std::vector<Vector2f> &pts = out[static_cast<std::size_t>(c)].pts;
			pts.reserve(static_cast<std::size_t>(n));
			for (int i = 0; i < n; ++i)
				pts.push_back(Vector2f(p[2 * i], p[2 * i + 1]));
		}
		return out;
	}
}

namespace
{
	// Concordance maillage / region, en fraction de la largeur de la baie.
	constexpr double kExtentTolerance    = 1e-3;
	constexpr double kReliefExtentMargin = 0.05;   // plancher du debord admis d'un relief ; au-dela, ReliefOverhang

	struct Extent
	{
		double lo[3] = {0.0, 0.0, 0.0}, hi[3] = {0.0, 0.0, 0.0};
		bool   empty = true;

		void Add (const double p[3], int dims)
		{
			for (int k = 0; k < dims; ++k)
			{
				lo[k] = empty ? p[k] : std::min(lo[k], p[k]);
				hi[k] = empty ? p[k] : std::max(hi[k], p[k]);
			}
			empty = false;
		}
	};

	// "" si l'emprise du maillage concorde avec la region extrudee, sinon le
	// motif. Elle doit contenir la region extrudee ([0, height] en z) et ne pas
	// la deborder de plus de `reliefMargin`, a `tol` pres. Plaque plate : marge
	// 0, donc emprise egale. Relief : l'ebrasement amincit une face et
	// les barres debordent la region (en y au pied des jambages, en z au-dessus
	// de la face avant) ; l'emprise doit alors contenir celle de la region
	// extrudee, sans la deborder de plus de `reliefMargin` (le debord des
	// barres, ReliefOverhang, absolu en unites monde). Plus lache, sans risque :
	// un relief n'est jamais declare imprimable.
	Extent meshExtent (const Mesh &mesh)
	{
		Extent m;
		const int n = mesh.GetNVertices();
		for (int i = 0; i < n; ++i)
		{
			float v[3];
			mesh.GetVertex(static_cast<unsigned>(i), v);
			const double q[3] = {v[0], v[1], v[2]};
			m.Add(q, 3);
		}
		return m;
	}

	std::string extentMismatch (const Extent &m, const Polygon2 &poly, double height, bool relief,
	                            double tol, double reliefMargin)
	{
		Extent region;
		for (int c = 0; c < poly.get_n_contours(); ++c)
		{
			const float *p = poly.get_points(c);
			for (int i = 0; i < poly.get_n_points(c); ++i)
			{
				const double q[3] = {p[2 * i], p[2 * i + 1], 0.0};
				region.Add(q, 2);
			}
		}
		region.lo[2] = 0.0;
		region.hi[2] = height;
		bool ok = !m.empty && !region.empty;
		for (int k = 0; ok && k < 3; ++k)
		{
			const double below = region.lo[k] - m.lo[k], above = m.hi[k] - region.hi[k];
			ok = below >= -tol && below <= reliefMargin + tol && above >= -tol && above <= reliefMargin + tol;
		}
		if (ok)
			return std::string();
		std::ostringstream os;
		os.imbue(std::locale::classic());
		os << "mesh extent ";
		if (m.empty)
			os << "empty";
		else
			os << '[' << m.lo[0] << ", " << m.hi[0] << "] x [" << m.lo[1] << ", " << m.hi[1] << "] x ["
			   << m.lo[2] << ", " << m.hi[2] << ']';
		os << " does not match the stone region [" << region.lo[0] << ", " << region.hi[0] << "] x ["
		   << region.lo[1] << ", " << region.hi[1] << "] extruded over [0, " << height << "], tolerance " << tol;
		if (relief)
			os << ", relief margin " << reliefMargin;
		return os.str();
	}
}

namespace
{
	// Pile de tranches lue sur le maillage.
	constexpr double kLevelTolerance    = 1e-6;   // fraction de la largeur : deux cotes plus proches sont confondues
	constexpr double kSliceInset        = 1e-4;   // fraction de l'epaisseur de la couche : retrait d'une coupe
	constexpr double kVerticalTolerance = 1e-6;   // |n_z| / |n| d'une face verticale

	// Pile de tranches du maillage, en unites monde. Les NIVEAUX sont les cotes
	// distinctes des sommets (a `levelTol` pres) : face arriere, sommets du
	// profil, face avant. Entre deux niveaux, chaque face laterale est plane,
	// donc la section varie lineairement : ses extremes sont aux deux bouts de la
	// couche. Coupes candidates : bas et haut de chaque couche, en retrait de
	// kSliceInset x son epaisseur (jamais SUR un niveau, ou le maillage, dont
	// capots et parois ne partagent pas leurs sommets, ne se coupe pas
	// proprement). Deux candidates voisines donnent la meme section, et sont
	// confondues en une tranche :
	//   - les deux bouts d'une couche PRISMATIQUE (aucune face non verticale ne
	//     la traverse) ; la coupe est alors au milieu de la couche ;
	//   - le haut d'une couche et le bas de la suivante, si aucune face
	//     horizontale n'est sur le niveau qui les separe.
	// Une tranche couvre de la cote de sa premiere candidate a celle de sa
	// derniere, coupe comprise : [coupe, niveau] pour le haut seul d'une couche
	// inclinee, [niveau, coupe] pour son bas seul. Vide si le maillage a moins
	// de deux niveaux.
	//
	// Lus au passage sur les faces INCLINEES (ni horizontales ni verticales) :
	// s'il y en a, si toutes regardent vers l'arriere (n_z < 0), le sinus de la
	// plus faible inclinaison sur l'horizontale, et le coin CONVEXE le plus aigu
	// de la face avant (niveau le plus haut) que borde une face inclinee : angle
	// 2D b entre ses deux aretes du niveau avant. Convexe si la matiere est dans
	// le petit secteur de ces aretes, donc si la somme des normales sortantes
	// (horizontales) des faces inclinees qui le touchent pointe hors de ce
	// secteur. Un coin rentrant compte pour b = 180.
	struct MeshSliceStack
	{
		std::vector<PrintSlice> slices;
		bool   slanted = false;        // au moins une face inclinee
		bool   nested = true;          // aucune face inclinee ne regarde vers l'avant
		double minIncline = 1.0;       // plus petit |sin| de l'inclinaison d'une face inclinee ; 1 sans
		double minHalfCornerSin = 1.0; // sin (b / 2) du coin convexe le plus aigu ; 1 sans coin
		double cornerX = 0.0, cornerY = 0.0;
	};

	MeshSliceStack meshSliceStack(const Mesh &mesh, double levelTol)
	{
		MeshSliceStack out;
		const std::vector<float> &v = mesh.GetVertices();
		std::vector<double> zs;
		zs.reserve(v.size() / 3);
		for (std::size_t i = 2; i < v.size(); i += 3)
			zs.push_back(v[i]);
		std::sort(zs.begin(), zs.end());
		std::vector<double> levels;
		for (double z : zs)
			if (levels.empty() || z - levels.back() > levelTol)
				levels.push_back(z);
		if (levels.size() < 2)
			return out;
		const std::size_t layers = levels.size() - 1;
		auto levelOf = [&](double z) {
			const auto it = std::upper_bound(levels.begin(), levels.end(), z + levelTol);
			return static_cast<std::size_t>(it - levels.begin()) - 1;
		};

		std::vector<char> prismatic(layers, 1), horizontal(levels.size(), 0);
		const double top = levels.back();
		auto atTop = [&](unsigned i) { return std::fabs(static_cast<double>(v[3 * static_cast<std::size_t>(i) + 2]) - top) <= levelTol; };
		// Sommets du niveau avant bordes par une face inclinee : voisins sur ce
		// niveau, et somme des normales sortantes horizontales de ces faces.
		struct FrontCorner
		{
			std::vector<unsigned> next;
			double nx = 0.0, ny = 0.0;
		};
		std::vector<FrontCorner> front(static_cast<std::size_t>(mesh.GetNVertices()));
		for (unsigned f = 0; f < mesh.GetNFaces(); ++f)
		{
			const int n = mesh.GetFaceNVertices(f);
			if (n < 3)
				continue;
			double nx = 0.0, ny = 0.0, nz = 0.0;
			double zmin = std::numeric_limits<double>::infinity(), zmax = -zmin;
			for (int k = 0; k < n; ++k)
			{
				const float *a = &v[3 * static_cast<std::size_t>(mesh.GetFaceVertex(f, static_cast<unsigned>(k)))];
				const float *b = &v[3 * static_cast<std::size_t>(mesh.GetFaceVertex(f, static_cast<unsigned>((k + 1) % n)))];
				nx += (static_cast<double>(a[1]) - b[1]) * (static_cast<double>(a[2]) + b[2]);
				ny += (static_cast<double>(a[2]) - b[2]) * (static_cast<double>(a[0]) + b[0]);
				nz += (static_cast<double>(a[0]) - b[0]) * (static_cast<double>(a[1]) + b[1]);
				zmin = std::min(zmin, static_cast<double>(a[2]));
				zmax = std::max(zmax, static_cast<double>(a[2]));
			}
			if (zmax - zmin <= levelTol)
			{
				horizontal[levelOf(zmin)] = 1;
				continue;
			}
			const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
			if (len == 0.0 || std::fabs(nz) <= kVerticalTolerance * len)
				continue;
			for (std::size_t k = levelOf(zmin); k < levelOf(zmax); ++k)
				prismatic[k] = 0;
			out.slanted = true;
			for (int k = 0; k < n; ++k)
			{
				const unsigned a = static_cast<unsigned>(mesh.GetFaceVertex(f, static_cast<unsigned>(k)));
				if (!atTop(a))
					continue;
				FrontCorner &c = front[a];
				c.nx += nx;
				c.ny += ny;
				for (int q = 1; q < n; ++q)
				{
					const unsigned b = static_cast<unsigned>(mesh.GetFaceVertex(f, static_cast<unsigned>((k + q) % n)));
					if ((q == 1 || q == n - 1) && atTop(b) && std::find(c.next.begin(), c.next.end(), b) == c.next.end())
						c.next.push_back(b);
				}
			}
			out.nested = out.nested && nz < 0.0;
			const double cz = std::fabs(nz) / len;
			out.minIncline = std::min(out.minIncline, std::sqrt(std::max(0.0, 1.0 - cz * cz)));
		}

		for (std::size_t i = 0; i < front.size(); ++i)
		{
			const FrontCorner &c = front[i];
			const double ax = v[3 * i], ay = v[3 * i + 1];
			for (std::size_t p = 0; p < c.next.size(); ++p)
				for (std::size_t q = p + 1; q < c.next.size(); ++q)
				{
					double e1x = v[3 * static_cast<std::size_t>(c.next[p])] - ax, e1y = v[3 * static_cast<std::size_t>(c.next[p]) + 1] - ay;
					double e2x = v[3 * static_cast<std::size_t>(c.next[q])] - ax, e2y = v[3 * static_cast<std::size_t>(c.next[q]) + 1] - ay;
					const double l1 = std::hypot(e1x, e1y), l2 = std::hypot(e2x, e2y);
					if (l1 == 0.0 || l2 == 0.0)
						continue;
					e1x /= l1; e1y /= l1; e2x /= l2; e2y /= l2;
					// Convexe : les faces inclinees regardent hors du petit secteur.
					if (c.nx * (e1x + e2x) + c.ny * (e1y + e2y) >= 0.0)
						continue;
					const double cosB = std::clamp(e1x * e2x + e1y * e2y, -1.0, 1.0);
					const double halfSin = std::sqrt(0.5 * (1.0 - cosB));
					if (halfSin < out.minHalfCornerSin)
					{
						out.minHalfCornerSin = halfSin;
						out.cornerX = ax;
						out.cornerY = ay;
					}
				}
		}

		// Candidates 2k (bas de la couche k) et 2k + 1 (haut), dans l'ordre des z.
		auto position = [&](std::size_t c) { return levels[c / 2 + c % 2]; };
		auto cut = [&](std::size_t c) {
			const std::size_t k = c / 2;
			const double inset = kSliceInset * (levels[k + 1] - levels[k]);
			return (c % 2) ? levels[k + 1] - inset : levels[k] + inset;
		};
		auto sameSection = [&](std::size_t c) {   // c et c + 1
			return (c % 2 == 0) ? prismatic[c / 2] != 0 : horizontal[c / 2 + 1] == 0;
		};
		std::vector<PrintSlice> &stack = out.slices;
		std::vector<float> cuts;
		for (std::size_t first = 0; first < 2 * layers;)
		{
			std::size_t last = first;
			while (last + 1 < 2 * layers && sameSection(last))
				++last;
			PrintSlice slice;
			slice.zLow = position(first);
			slice.zHigh = position(last);
			slice.zCut = cut(first);
			std::size_t layer = first / 2;
			for (std::size_t c = first; c < last; ++c)
				if (c % 2 == 0)
				{
					layer = c / 2;
					slice.zCut = 0.5 * (levels[layer] + levels[layer + 1]);
					break;
				}
			// Coupe strictement dans sa couche une fois arrondie en float ; sinon au
			// milieu de la couche.
			float zc = static_cast<float>(slice.zCut);
			if (!(zc > levels[layer] && zc < levels[layer + 1]))
				zc = static_cast<float>(0.5 * (levels[layer] + levels[layer + 1]));
			cuts.push_back(zc);
			stack.push_back(std::move(slice));
			first = last + 1;
		}

		cgmesh::slicing::SliceInput input;
		input.mesh = &mesh;
		cgmesh::slicing::SliceOptions options;
		options.nesting = cgmesh::slicing::Nesting::Winding;
		const std::vector<cgmesh::slicing::SliceLayer> sections = cgmesh::slicing::sliceMeshes({input}, cuts, options);
		for (std::size_t i = 0; i < stack.size(); ++i)
		{
			stack[i].zCut = static_cast<double>(cuts[i]);
			stack[i].zLow = std::min(stack[i].zLow, stack[i].zCut);
			stack[i].zHigh = std::max(stack[i].zHigh, stack[i].zCut);
			for (const cgmesh::slicing::SliceRegion &region : sections[i])
				stack[i].region.insert(stack[i].region.end(), region.contours.begin(), region.contours.end());
		}
		return out;
	}

	// Majorant, en mm, de la partie mince d'un coin de la face avant (loi du
	// coin de D3 en 3D : distance du sommet a la boule de rayon r = minWallMm / 2
	// la plus proche, moins r). Triedre face avant + deux pentes d'inclinaison a
	// sur les cotes d'un coin 2D d'angle b : centre de la boule a
	// r cot (a/2) / sin (b/2) du sommet le long de la bissectrice, a r sous la
	// face, donc r (sqrt ((cot (a/2) / sin (b/2))^2 + 1) - 1) ; b = 180 donne la
	// levre (diedre), r (1 / sin (a/2) - 1). La plus faible inclinaison et le coin
	// le plus aigu majorent tous les coins ; le decalage par bissectrice du
	// profil laisse au coin plus de matiere que le triedre.
	double cornerThinLengthMm(double minIncline, double halfCornerSin, const PrintConstraints &constraints)
	{
		const double half = 0.5 * std::asin(std::min(1.0, minIncline));
		if (!(half > 0.0) || !(halfCornerSin > 0.0))
			return std::numeric_limits<double>::infinity();
		const double r = 0.5 * constraints.minWallMm;
		const double k = 1.0 / std::tan(half) / std::min(1.0, halfCornerSin);
		return r * (std::sqrt(k * k + 1.0) - 1.0);
	}
}

namespace
{
	PrintExtent printExtent (const Extent &e)
	{
		PrintExtent p;
		for (std::size_t k = 0; k < 3; ++k)
		{
			p.lo[k] = e.lo[k];
			p.hi[k] = e.hi[k];
		}
		return p;
	}

	// Corps commun de CheckGothicPrint et de CheckGothicExportAndPrint, sur une
	// geometrie deja jugee et permise (check.geometry). `check` est rempli au
	// fil des etapes : une exception laisse ce qui a ete mesure avant elle, le
	// rapport d'enveloppe.
	void measurePrint (const ParameterizedGothicWindow &window, const Mesh &mesh, const Extent &meshBox,
	                   const PrintConstraints &constraints, GothicPrintCheck &check)
	{
		if (!window.MatchesLastRegenerate())
		{
			check.reason = "settings changed since last Regenerate";
			return;
		}

		// Epaisseur de la plaque : l'extrusion de Regenerate, plancher 0.
		const float z = window.Settings().zHeight;
		const double height = (z < 0.f) ? 0.0 : static_cast<double>(z);
		const bool relief = window.HasReliefProfile();
		const Polygon2 poly = window.BuildStonePolygon();
		const double width = std::max(1.0, static_cast<double>(window.Settings().width));
		const double tol = kExtentTolerance * width;
		const double margin = relief ? std::max(kReliefExtentMargin * width, window.ReliefOverhang()) : 0.0;
		check.reason = extentMismatch(meshBox, poly, height, relief, tol, margin);
		if (!check.reason.empty())
			return;

		// Enveloppe : la boite du maillage imprime, jugee avant la mesure 2D.
		const PrintabilityReport envelope = CheckPrintEnvelope(printExtent(meshBox), constraints);
		check.print = envelope;
		const auto withEnvelope = [&envelope](PrintabilityReport r) {
			const std::size_t k = static_cast<std::size_t>(PrintDefect::EnvelopeExceeded);
			r.counts[k] += envelope.counts[k];
			r.records.insert(r.records.end(), envelope.records.begin(), envelope.records.end());
			return r;
		};

		if (!relief)
		{
			check.print = withEnvelope(CheckPrintExtrusion(stoneContours(poly), height, constraints));
			check.scope = GothicPrintScope::Complete;
			return;
		}
		if (window.HasBarRelief())
		{
			check.print = withEnvelope(CheckPrintExtrusion(stoneContours(poly), height, constraints));
			check.scope = GothicPrintScope::ProfileNotChecked;
			check.reason = "bar relief not checked: base plate measured as flat";
			return;
		}

		// Ebrasement seul : la pile de tranches du maillage. Couverte si les
		// sections sont emboitees et les coins de la face avant non fragiles ; sans
		// face inclinee, il n'y a ni levre ni coin.
		const MeshSliceStack stack = meshSliceStack(mesh, kLevelTolerance * width);
		if (stack.slices.empty())
		{
			check.print = withEnvelope(CheckPrintExtrusion(stoneContours(poly), height, constraints));
			check.scope = GothicPrintScope::ProfileNotChecked;
			check.reason = "splay not checked: mesh has fewer than two levels";
			return;
		}
		check.print = withEnvelope(CheckPrintSlices(stack.slices, constraints));
		check.scope = GothicPrintScope::Complete;
		const double thin = cornerThinLengthMm(stack.minIncline, stack.minHalfCornerSin, constraints);
		constexpr double kDegrees = 180.0 / 3.14159265358979323846;
		std::ostringstream os;
		os.imbue(std::locale::classic());
		if (!stack.nested)
			os << "splay not checked: a slanted face looks forward (sections not nested)";
		else if (stack.slanted && thin > constraints.maxTipLengthMm)
		{
			os << "splay not checked: ";
			if (stack.minHalfCornerSin < 1.0)
				os << "front corner of " << 2.0 * std::asin(stack.minHalfCornerSin) * kDegrees << " deg at ("
				   << stack.cornerX << ", " << stack.cornerY << "), ";
			os << "front lip of " << std::asin(std::min(1.0, stack.minIncline)) * kDegrees
			   << " deg, thin over " << thin << " mm > maxTipLengthMm " << constraints.maxTipLengthMm;
		}
		check.reason = os.str();
		if (!check.reason.empty())
			check.scope = GothicPrintScope::ProfileNotChecked;
	}
}

GothicPrintCheck CheckGothicPrint (const ParameterizedGothicWindow &window, const Mesh &mesh,
                                   const PrintConstraints &constraints)
{
	GothicPrintCheck check;
	check.geometry = CheckGothicExport(window, mesh);
	if (!check.geometry.allowed)
	{
		check.reason = "geometry refused";
		return check;
	}
	measurePrint(window, mesh, meshExtent(mesh), constraints, check);
	return check;
}

namespace
{
	// Verdict d'un controle d'impression interrompu : portee NotRun, rien de
	// mesure n'est declare evalue, puis le motif et le texte. Ne leve jamais :
	// sans memoire pour le texte, printError retombe sur un litteral court
	// (tampon interne de std::string, sans allocation sous MSVC et libstdc++).
	void recordPrintError (GothicPrintVerdict &verdict, const char *reason, const char *type,
	                       const char *what) noexcept
	{
		verdict.check.scope = GothicPrintScope::NotRun;
		verdict.check.print.evaluated = false;
		try
		{
			verdict.check.reason = reason;
			verdict.printError = type ? std::string(type) + ": " + what : std::string(what);
		}
		catch (...)
		{
			try
			{
				verdict.printError = "out of memory";
			}
			catch (...)
			{
			}
		}
	}
}

GothicPrintVerdict CheckGothicExportAndPrint (const ParameterizedGothicWindow &window, const Mesh &mesh,
                                              const std::optional<PrintConstraints> &constraints)
{
	GothicPrintVerdict verdict;
	GothicPrintCheck &check = verdict.check;
	check.geometry = CheckGothicExport(window, mesh);
	if (!check.geometry.allowed)
	{
		check.reason = "geometry refused";
		return verdict;
	}
	verdict.constraints = constraints;
	const PrintConstraints c = constraints.value_or(PrintConstraints());
	try
	{
		if (!c.mmPerUnit)
		{
			// Aucune mesure : le rapport ScaleUnset d'une region vide, apres le
			// controle du domaine des contraintes.
			check.reason = "print scale not set";
			check.print = CheckPrintRegion({}, c);
			return verdict;
		}
		const Extent box = meshExtent(mesh);
		if (!box.empty)
		{
			std::array<double, 3> size {};
			bool finite = true;
			for (std::size_t k = 0; k < 3; ++k)
			{
				size[k] = (box.hi[k] - box.lo[k]) * *c.mmPerUnit;
				finite = finite && std::isfinite(size[k]);
			}
			if (finite)
				verdict.printedSizeMm = size;
		}
		measurePrint(window, mesh, box, c, check);
	}
	catch (const std::invalid_argument &e)
	{
		recordPrintError(verdict, "print check error", nullptr, e.what());
	}
	catch (const std::exception &e)
	{
		recordPrintError(verdict, "print check internal error", typeid(e).name(), e.what());
	}
	return verdict;
}

double GothicMmPerUnitForPrintedWidth (const Mesh &mesh, double printedWidthMm)
{
	const Extent box = meshExtent(mesh);
	if (box.empty)
		throw std::invalid_argument("GothicMmPerUnitForPrintedWidth: empty mesh");
	return MmPerUnitForPrintedWidth(printedWidthMm, box.hi[0] - box.lo[0]);
}

GothicPrintVerdict CheckGothicExportAndPrintAtWidth (const ParameterizedGothicWindow &window, const Mesh &mesh,
                                                     const PrintConstraints &printer,
                                                     const std::optional<double> &printedWidthMm)
{
	PrintConstraints c = printer;
	c.mmPerUnit.reset();
	if (!printedWidthMm)
		return CheckGothicExportAndPrint(window, mesh, c);
	try
	{
		c.mmPerUnit = GothicMmPerUnitForPrintedWidth(mesh, *printedWidthMm);
	}
	catch (const std::invalid_argument &e)
	{
		GothicPrintVerdict verdict = CheckGothicExportAndPrint(window, mesh, c);
		if (verdict.ExportAllowed())
			recordPrintError(verdict, "print check error", "printed width", e.what());
		return verdict;
	}
	return CheckGothicExportAndPrint(window, mesh, c);
}
