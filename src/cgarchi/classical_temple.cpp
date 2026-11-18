#include <cgarchi/classical_temple.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_entablature_detail.h>
#include <cgarchi/classical_greek_pediment_detail.h>
#include "mesh.h"

namespace cgarchi
{
namespace classical
{

namespace
{

[[noreturn]] void Fail (const char *field, const std::string &rule)
{
	throw std::invalid_argument (std::string ("Temple : '") + field + "' " + rule);
}

// Axes places d'une rangee, croissants : facade (X) ou flanc droit (Y). Les
// colonnes d'un cote donnent ses n - 1 premiers axes, la demi-distance le
// dernier.
std::vector<double> PlacedRow (const PeristyleLayout &l, PeristyleSide side, double last)
{
	std::vector<double> axes;
	for (const PeristyleColumn &c : l.columns)
		if (c.side == side)
			axes.push_back (side == PeristyleSide::Front ? c.x : c.y);
	axes.push_back (last);
	return axes;
}

// TE4 sur une rangee, sur les ecarts des axes places : deux colonnes voisines
// (angle aux deux bouts, courantes entre) ; pour une rangee de deux colonnes,
// son unique travee separe aussi les colonnes des deux rangees opposees, qui
// se font face au meme abscisse.
void RequireRowSeparation (const std::vector<double> &axes, int otherN, double en, double ec, const char *spacingField,
                           const char *cornerField)
{
	const std::size_t n = axes.size ();
	auto extent = [&] (std::size_t k) { return k == 0 || k == n - 1 ? ec : en; };
	for (std::size_t k = 0; k + 1 < n; ++k)
	{
		double need = extent (k) + extent (k + 1);
		if (n == 2 && otherN >= 3)
			need = 2.0 * std::max (ec, en);
		const bool cornerGap = k == 0 || k + 2 == n;
		if (!(axes[k + 1] - axes[k] > need))
			Fail (cornerGap ? cornerField : spacingField, "place deux colonnes a moins de la somme de leurs demi-etendues");
	}
}

// TE4 sur les deux voisines d'un angle, l'une sur la facade, l'autre sur le
// flanc, aux quatre angles : separees si l'un des deux decalages places
// depasse la somme de leurs demi-etendues.
void RequireCornerNeighboursSeparation (const std::vector<double> &xs, const std::vector<double> &ys, double en,
                                        double ec, const char *field = "peristyle.frontCornerSpacing",
                                        const char *rule = "et 'peristyle.flankCornerSpacing' ne separent pas les deux voisines d'un angle")
{
	const double sum = (xs.size () >= 3 ? en : ec) + (ys.size () >= 3 ? en : ec);
	const double dxs[2] = {xs[1] - xs[0], xs[xs.size () - 1] - xs[xs.size () - 2]};
	const double dys[2] = {ys[1] - ys[0], ys[ys.size () - 1] - ys[ys.size () - 2]};
	for (const double dx : dxs)
		for (const double dy : dys)
			if (!(dx > sum || dy > sum))
				Fail (field, rule);
}

// Inclinaison d'un placement (RF4) : vers l'interieur, perpendiculaire a la
// rangee ; a l'angle, sur les deux axes.
void Incline (TemplePlacement &p, double i)
{
	if (p.corner)
	{
		p.inclinationX = p.x > 0.0 ? -i : i;
		p.inclinationY = p.y > 0.0 ? -i : i;
		return;
	}
	switch (p.address.side)
	{
	case PeristyleSide::Front: p.inclinationY = i; break;
	case PeristyleSide::Back: p.inclinationY = -i; break;
	case PeristyleSide::Right: p.inclinationX = -i; break;
	case PeristyleSide::Left: p.inclinationX = i; break;
	}
}

// Accumulation des pieces en un bloc (TE5) : sommets et indices ajoutes en
// place, tableaux reserves une fois.
struct Accumulator
{
	std::vector<float> vertices;
	std::vector<unsigned> faces;

	// Ajoute `piece` translatee de (tx, ty, tz) : chaque position en double,
	// arrondie une fois en float. Rend la plage ecrite.
	TempleMeshPart Append (const Mesh &piece, double tx, double ty, double tz, const TempleAddress &address)
	{
		TempleMeshPart part;
		part.address = address;
		part.firstVertex = static_cast<unsigned> (vertices.size () / 3);
		part.firstFace = static_cast<unsigned> (faces.size () / 3);
		const std::vector<float> &v = piece.GetVertices ();
		for (std::size_t i = 0; i + 2 < v.size (); i += 3)
		{
			vertices.push_back (static_cast<float> (static_cast<double> (v[i]) + tx));
			vertices.push_back (static_cast<float> (static_cast<double> (v[i + 1]) + ty));
			vertices.push_back (static_cast<float> (static_cast<double> (v[i + 2]) + tz));
		}
		for (unsigned f = 0; f < piece.GetNFaces (); ++f)
		{
			if (piece.GetFaceNVertices (f) != 3)
				throw std::logic_error ("Temple : piece non triangulee");
			for (unsigned k = 0; k < 3; ++k)
				faces.push_back (part.firstVertex + static_cast<unsigned> (piece.GetFaceVertex (f, k)));
		}
		part.vertexCount = static_cast<unsigned> (vertices.size () / 3) - part.firstVertex;
		part.faceCount = static_cast<unsigned> (faces.size () / 3) - part.firstFace;
		return part;
	}
};

// Carte des sommets sous raffinement (RF3) : translation, cisaillement d'une
// colonne, champ sur la position horizontale finale, un seul arrondi float.
struct VertexMap
{
	const TempleRefinementSettings *refinement = nullptr;
	double a = 0.0, b = 0.0;   // demi-cotes du stylobate
};

struct Shear
{
	double ix = 0.0, iy = 0.0, height = 1.0;   // deplacement du sommet, hauteur H du tour
	bool active = false;
};

TempleMeshPart AppendMapped (Accumulator &acc, const Mesh &piece, double tx, double ty, double tz, const Shear &shear,
                             const VertexMap &map, const TempleAddress &address)
{
	TempleMeshPart part;
	part.address = address;
	part.firstVertex = static_cast<unsigned> (acc.vertices.size () / 3);
	part.firstFace = static_cast<unsigned> (acc.faces.size () / 3);
	const std::vector<float> &v = piece.GetVertices ();
	for (std::size_t i = 0; i + 2 < v.size (); i += 3)
	{
		double x = static_cast<double> (v[i]) + tx, y = static_cast<double> (v[i + 1]) + ty;
		const double z = static_cast<double> (v[i + 2]) + tz;
		if (shear.active)
		{
			const double t = static_cast<double> (v[i + 2]) / shear.height;
			x = x + shear.ix * t;
			y = y + shear.iy * t;
		}
		acc.vertices.push_back (static_cast<float> (x));
		acc.vertices.push_back (static_cast<float> (y));
		acc.vertices.push_back (static_cast<float> (z + TempleRefinementLift (*map.refinement, map.a, map.b, x, y)));
	}
	for (unsigned f = 0; f < piece.GetNFaces (); ++f)
	{
		if (piece.GetFaceNVertices (f) != 3)
			throw std::logic_error ("Temple : piece non triangulee");
		for (unsigned k = 0; k < 3; ++k)
			acc.faces.push_back (part.firstVertex + static_cast<unsigned> (piece.GetFaceVertex (f, k)));
	}
	part.vertexCount = static_cast<unsigned> (acc.vertices.size () / 3) - part.firstVertex;
	part.faceCount = static_cast<unsigned> (acc.faces.size () / 3) - part.firstFace;
	return part;
}

// Plages des degres dans la plage `whole` de la krepis (contrat de
// BuildKrepisMesh : degres dans l'ordre de KrepisLayout::steps, autant de
// sommets et de faces chacun, sommets non partages).
std::vector<TempleMeshPart> SplitKrepis (const TempleMeshPart &whole, std::size_t steps, const std::vector<unsigned> &faces)
{
	if (steps == 0 || whole.vertexCount % steps != 0 || whole.faceCount % steps != 0)
		throw std::logic_error ("Temple : degres de la krepis de tailles inegales");
	const unsigned sv = static_cast<unsigned> (whole.vertexCount / steps);
	const unsigned sf = static_cast<unsigned> (whole.faceCount / steps);
	std::vector<TempleMeshPart> parts;
	for (std::size_t k = 0; k < steps; ++k)
	{
		TempleMeshPart part;
		part.address = {TempleElement::KrepisStep, PeristyleSide::Front, static_cast<int> (k)};
		part.firstVertex = whole.firstVertex + static_cast<unsigned> (k) * sv;
		part.vertexCount = sv;
		part.firstFace = whole.firstFace + static_cast<unsigned> (k) * sf;
		part.faceCount = sf;
		for (std::size_t i = 3 * static_cast<std::size_t> (part.firstFace); i < 3 * static_cast<std::size_t> (part.firstFace + sf); ++i)
			if (faces[i] < part.firstVertex || faces[i] >= part.firstVertex + sv)
				throw std::logic_error ("Temple : degre de la krepis hors de sa plage");
		parts.push_back (part);
	}
	return parts;
}

// Plages des degres d'une krepis a stations, par les tailles rendues par
// BuildKrepisMesh (degres de tailles inegales, sommets non partages).
std::vector<TempleMeshPart> SplitKrepis (const TempleMeshPart &whole, const std::vector<KrepisStepSize> &sizes,
                                         const std::vector<unsigned> &faces)
{
	std::vector<TempleMeshPart> parts;
	unsigned firstVertex = whole.firstVertex, firstFace = whole.firstFace;
	for (std::size_t k = 0; k < sizes.size (); ++k)
	{
		TempleMeshPart part;
		part.address = {TempleElement::KrepisStep, PeristyleSide::Front, static_cast<int> (k)};
		part.firstVertex = firstVertex;
		part.vertexCount = sizes[k].vertices;
		part.firstFace = firstFace;
		part.faceCount = sizes[k].faces;
		for (std::size_t i = 3 * static_cast<std::size_t> (firstFace); i < 3 * static_cast<std::size_t> (firstFace + part.faceCount); ++i)
			if (faces[i] < firstVertex || faces[i] >= firstVertex + part.vertexCount)
				throw std::logic_error ("Temple : degre de la krepis hors de sa plage");
		firstVertex += part.vertexCount;
		firstFace += part.faceCount;
		parts.push_back (part);
	}
	if (firstVertex != whole.firstVertex + whole.vertexCount || firstFace != whole.firstFace + whole.faceCount)
		throw std::logic_error ("Temple : tailles des degres de la krepis differentes du maillage");
	return parts;
}

// Etat de travail d'un temple, rempli par LayoutWithColumns, repris par
// BuildPieces sur les memes reglages et la meme mise en place.
struct TempleWork
{
	// Raffinement : maillages des colonnes construits pour la borne de la seconde
	// famille ; vides sinon.
	std::unique_ptr<Mesh> column, cornerColumn;
	// Avec un entablement : sa preuve de validation et les cotes de ses bandes.
	std::optional<detail::MeasuredEntablature> entablature;
	// Avec un fronton : la preuve de validation de ses champs propres.
	std::optional<detail::CheckedPediment> pediment;
	// Section de l'anneau (SectionOnce) ; vide avant son premier usage.
	std::vector<RingSectionPoint> section;
	// Raffinement : coordonnees structurelles (StructuralCoordinates) et seconde
	// famille sans elles (StationClearanceOf), calculees une fois.
	std::pair<std::vector<double>, std::vector<double>> structural;
	StationClearance clearance;
};

// Section de l'anneau d'un temple a entablement, calculee une fois.
const std::vector<RingSectionPoint> &SectionOnce (TempleWork &work)
{
	if (work.section.empty ())
		work.section = detail::EntablatureSection (*work.entablature);
	return work.section;
}

// Pieces du maillage et leurs comptes : la krepis et les deux tours, construits
// une fois, puis, avec un entablement, l'anneau et ses pieces (triglyphes,
// regulae, mutules) ; les comptes cumulent la krepis, un tour par placement et
// l'entablement, sans debordement (chaque piece comparee a la marge restante).
struct Pieces
{
	std::unique_ptr<Mesh> krepis, column, cornerColumn, ring;
	// Ordre de FriezeLayout::triglyphs, puis OrnamentLayout::regulae, puis
	// OrnamentLayout::mutules (BuildEntablaturePieceMeshes).
	std::vector<std::unique_ptr<Mesh>> entablature;
	// Corniche rampante Front, corniche rampante Back, comble
	// (BuildGreekPedimentPieceMeshes).
	std::vector<std::unique_ptr<Mesh>> pediment;
	// Avec un raffinement : stations deplacees et tailles des degres.
	std::optional<RefinementStations> stations;
	std::vector<KrepisStepSize> krepisSteps;
	// Avec une cella : ses pieces (BuildGreekCellaPieces) et, avec des porches, le
	// tour de la colonne de porche.
	std::vector<CellaPiece> cella;
	std::unique_ptr<Mesh> porchColumn;
	// Avec un interieur : ses pieces (BuildGreekNaosInteriorPieces) et les tours
	// des colonnes inferieure et superieure.
	std::vector<NaosInteriorPiece> interior;
	std::unique_ptr<Mesh> lowerInteriorColumn, upperInteriorColumn;
	// Avec un couronnement : ses pieces (BuildGreekEpicranitisPieces), puis celles du
	// prolongement est (BuildGreekPorchExtensionPieces).
	std::vector<EpicranitisPiece> epicranitis;
	std::vector<PorchExtensionPiece> porchExtension;
	// Avec un plafond est : ses pieces (BuildGreekEastCeilingPieces), puis celles des plafonds des flancs
	// (BuildGreekFlankCeilingPieces).
	std::vector<CeilingPiece> eastCeiling, flankCeilings;
	std::size_t vertices = 0, faces = 0;

	void Add (const Mesh &piece)
	{
		const std::size_t f = piece.GetNFaces (), v = piece.GetNVertices ();
		if (f > kTempleMaxFaces || !TempleMeshFacesWithinBound (faces + f))
			Fail ("peristyle", "donne plus de " + std::to_string (kTempleMaxFaces) + " faces");
		if (v > 3 * kTempleMaxFaces - vertices)
			Fail ("peristyle", "donne plus de " + std::to_string (3 * kTempleMaxFaces) + " sommets");
		faces += f;
		vertices += v;
	}
};

// Coordonnees structurelles de chaque axe (RF5), avec les deux signes : axes
// des colonnes, contours des degres, sections de l'anneau et du polygone
// rampant, bords des triglyphes, regulae et mutules, faite. `section` : celle
// de l'anneau (SectionOnce), lue avec un entablement.
std::pair<std::vector<double>, std::vector<double>> StructuralCoordinates (const TempleSettings &s, const TempleLayout &l,
                                                                           const std::vector<RingSectionPoint> &section)
{
	std::vector<double> xs, ys;
	// Majorant des coordonnees de chaque axe : une reservation.
	std::size_t bound = l.columns.size () + 2 * l.krepis.steps.size () + 1;
	if (s.entablature && l.frieze)
	{
		bound += 2 * s.entablature->bands.size () + 8 + 4 * l.frieze->triglyphs.size () * (2 + 2 * s.entablature->triglyph.divisionWidths.size ());
		if (l.ornaments)
			bound += 4 * (l.ornaments->regulae.size () + l.ornaments->mutules.size ());
		if (s.pediment)
			bound += 2 * s.pediment->rakingSection.size ();
	}
	xs.reserve (bound);
	ys.reserve (bound);
	auto both = [] (std::vector<double> &v, double c) {
		v.push_back (c);
		v.push_back (-c);
	};
	for (const TemplePlacement &c : l.columns)
	{
		xs.push_back (c.x);
		ys.push_back (c.y);
	}
	for (const KrepisBox &box : l.krepis.steps)
	{
		both (xs, box.halfWidth);
		both (ys, box.halfDepth);
	}
	if (s.entablature && l.frieze)
	{
		for (const RingSectionPoint &q : section)
		{
			both (xs, l.peristyle.halfWidth + q.u);
			both (ys, l.peristyle.halfDepth + q.u);
		}
		auto along = [&] (PeristyleSide side, double a0, double a1, double r0, double r1) {
			const bool alongX = side == PeristyleSide::Front || side == PeristyleSide::Back;
			both (alongX ? xs : ys, a0);
			both (alongX ? xs : ys, a1);
			if (r0 != 0.0 || r1 != 0.0)
			{
				both (alongX ? ys : xs, r0);
				both (alongX ? ys : xs, r1);
			}
		};
		for (const FriezePiece &t : l.frieze->triglyphs)
			along (t.side, t.a0, t.a1, t.ra0, t.ra1);
		// Points du profil d'un triglyphe (bornes des divisions, creux des canaux),
		// depuis chaque bout de chaque bras : le dos du prisme porte un sommet a
		// chacun, sur la face de frise de l'anneau.
		const GreekTriglyphSettings &tg = s.entablature->triglyph;
		auto profile = [&] (std::vector<double> &axis, double lo, double hi) {
			for (const bool fromLow : {true, false})
			{
				double at = fromLow ? lo : hi;
				for (std::size_t k = 0; k < tg.divisionWidths.size (); ++k)
				{
					const double w = tg.divisionWidths[k];
					const double next = fromLow ? at + w : at - w;
					if (k < tg.divisionIsChannel.size () && tg.divisionIsChannel[k])
						both (axis, 0.5 * (at + next));
					both (axis, next);
					at = next;
				}
			}
		};
		for (const FriezePiece &t : l.frieze->triglyphs)
		{
			const bool alongX = t.side == PeristyleSide::Front || t.side == PeristyleSide::Back;
			profile (alongX ? xs : ys, t.a0, t.a1);
			if (t.ra0 != 0.0 || t.ra1 != 0.0)
				profile (alongX ? ys : xs, t.ra0, t.ra1);
		}
		// Coordonnees locales des pieces d'angle (L de la frise, hexagone de la
		// regula d'angle), sur les deux axes : dos du bras F - w, face de la regula.
		const bool cornerPieces = std::any_of (l.frieze->triglyphs.begin (), l.frieze->triglyphs.end (),
		                                       [] (const FriezePiece &t) { return t.kind == FriezeElement::CornerTriglyph; });
		if (cornerPieces)
		{
			std::vector<double> local {tg.face - tg.width};
			if (s.entablature->regulae)
				local.push_back (s.entablature->regulae->face);
			for (const double d : local)
			{
				both (xs, l.peristyle.halfWidth + d);
				both (ys, l.peristyle.halfDepth + d);
			}
		}
		if (l.ornaments)
		{
			for (const OrnamentPiece &o : l.ornaments->regulae)
				along (o.side, o.a0, o.a1, o.ra0, o.ra1);
			for (const OrnamentPiece &o : l.ornaments->mutules)
				along (o.side, o.a0, o.a1, 0.0, 0.0);
		}
		if (s.pediment)
		{
			xs.push_back (0.0);
			for (const RingSectionPoint &q : s.pediment->rakingSection)
				both (ys, l.peristyle.halfDepth + (s.pediment->tympanumFace + q.u));
		}
	}
	return {std::move (xs), std::move (ys)};
}

// Seconde famille d'interdites des stations S (registre RF5) : x et y des
// sommets du pied (z minimal du maillage) et du dessus de l'abaque (z maximal,
// decale de l'inclinaison) de chaque colonne, arrondis en float comme par
// AppendMapped, sur l'axe de sa rangee (Front et Back : X ; Right et Left : Y ;
// angle : les deux). Ecart kRefinementClearance. Avec un fronton, sur X, les
// pieds +-foot[k] de chaque trajectoire de la corniche rampante
// (detail::PedimentFeet), ajoutes en dernier : un noeud de la trajectoire k a dx
// de son pied n'est qu'a dx / r au-dessus du noeud du dessus de l'anneau ou de
// la base du comble ; ecart kRefinementClearance max (1, r), sans division par
// la pente.
StationClearance StationClearanceOf (const TempleSettings &s, const TempleLayout &l, const Mesh &column, const Mesh &cornerColumn,
                                     const std::vector<double> &feet)
{
	struct Ends
	{
		std::size_t count = 0;
		float lo = 0.0f, hi = 0.0f;
	};
	auto endsOf = [] (const Mesh &m) {
		const std::vector<float> &v = m.GetVertices ();
		Ends e;
		if (v.empty ())
			return e;
		e.lo = e.hi = v[2];
		for (std::size_t k = 2; k < v.size (); k += 3)
		{
			e.lo = std::min (e.lo, v[k]);
			e.hi = std::max (e.hi, v[k]);
		}
		for (std::size_t k = 2; k < v.size (); k += 3)
			e.count += (v[k] == e.lo || v[k] == e.hi) ? 1 : 0;
		return e;
	};
	const Ends plain = endsOf (column), corner = endsOf (cornerColumn);
	StationClearance c;
	std::size_t nx = 2 * feet.size (), ny = 0;
	for (const TemplePlacement &p : l.columns)
	{
		const std::size_t n = (p.corner ? corner : plain).count;
		const bool front = p.address.side == PeristyleSide::Front || p.address.side == PeristyleSide::Back;
		nx += p.corner || front ? n : 0;
		ny += p.corner || !front ? n : 0;
	}
	c.x.reserve (nx);
	c.y.reserve (ny);
	for (const TemplePlacement &p : l.columns)
	{
		const Mesh &m = p.corner ? cornerColumn : column;
		const Ends &e = p.corner ? corner : plain;
		const bool front = p.address.side == PeristyleSide::Front || p.address.side == PeristyleSide::Back;
		const std::vector<float> &v = m.GetVertices ();
		for (std::size_t k = 0; k + 2 < v.size (); k += 3)
		{
			const bool foot = v[k + 2] == e.lo, top = v[k + 2] == e.hi;
			if (!foot && !top)
				continue;
			if (p.corner || front)
				c.x.push_back ({static_cast<double> (static_cast<float> ((static_cast<double> (v[k]) + p.x) + (top ? p.inclinationX : 0.0))),
				                kRefinementClearance});
			if (p.corner || !front)
				c.y.push_back ({static_cast<double> (static_cast<float> ((static_cast<double> (v[k + 1]) + p.y) + (top ? p.inclinationY : 0.0))),
				                kRefinementClearance});
		}
	}
	if (!feet.empty ())
	{
		const double distance = kRefinementClearance * std::max (1.0, s.pediment->rakingRun);
		for (const double x : feet)
		{
			c.x.push_back ({x, distance});
			c.x.push_back ({-x, distance});
		}
	}
	return c;
}

// Borne basse calculee des deux familles (registre RF5) : sur chaque axe a pas
// fini, la fenetre +-p / 4 d'une station plus longue que le plus long bloc
// fusionne de la seconde famille et de la premiere a kRefinementMaxTolerance ;
// elle contient alors un point hors de tous les blocs, et le deplacement ne la
// refuse ni pour la seconde famille ni pour le plancher dur du repli. Quand
// seuls les pieds de la corniche rampante allongent le plus long bloc, le refus
// nomme aussi 'pediment.rakingRun'. Les deux maillages de colonne, la section
// de l'anneau, les coordonnees structurelles et la seconde famille sont rendus
// dans `work`.
void RequireStationClearance (const TempleSettings &s, const TempleLayout &l, TempleWork &work)
{
	work.column = BuildGreekDoricColumnMesh (s.column);
	work.cornerColumn = BuildGreekDoricColumnMesh (s.cornerColumn);
	const std::vector<double> feet = work.pediment ? detail::PedimentFeet (*work.pediment) : std::vector<double> {};
	work.clearance = StationClearanceOf (s, l, *work.column, *work.cornerColumn, feet);
	if (s.entablature && l.frieze)
		SectionOnce (work);
	work.structural = StructuralCoordinates (s, l, work.section);
	const std::size_t eaves = 2 * feet.size ();
	for (const bool alongX : {true, false})
	{
		const double pitch = alongX ? l.stations->pitchX : l.stations->pitchY;
		if (!std::isfinite (pitch))
			continue;
		// Seconde famille, puis coordonnees structurelles ; work.clearance reste la
		// seconde famille seule.
		const std::vector<ClearanceCoordinate> &second = alongX ? work.clearance.x : work.clearance.y;
		const std::vector<double> &structural = alongX ? work.structural.first : work.structural.second;
		std::vector<ClearanceCoordinate> family;
		family.reserve (second.size () + structural.size ());
		family.insert (family.end (), second.begin (), second.end ());
		const std::size_t own = family.size () - (alongX ? eaves : 0);
		for (const double f : structural)
			family.push_back ({f, kRefinementMaxTolerance});
		const double span = LongestClearanceBlock (family);
		if (pitch / 2.0 > span)
			continue;
		bool eaveDominates = false;
		if (alongX && eaves > 0)
		{
			std::vector<ClearanceCoordinate> without (family.begin (), family.begin () + static_cast<std::ptrdiff_t> (own));
			without.insert (without.end (), family.begin () + static_cast<std::ptrdiff_t> (own + eaves), family.end ());
			eaveDominates = pitch / 2.0 > LongestClearanceBlock (without);
		}
		throw std::invalid_argument (std::string ("Raffinement : 'refinement.chordDeviation'") +
		                             (eaveDominates ? " et 'pediment.rakingRun' donnent" : " donne") + " sur l'axe " + (alongX ? "X" : "Y") +
		                             " une fenetre de station (" + std::to_string (pitch / 2.0) + ") plus courte que l'emprise " +
		                             (eaveDominates ? "des pieds de la corniche rampante, fronton trop plat" :
		                                              "des sommets de pied, d'abaque, des pieds de la corniche rampante et des coordonnees "
		                                              "structurelles") +
		                             " a eviter (" + std::to_string (span) + ")");
	}
}

// Cotes des aretes protegees de la section de l'anneau (RF5, D3-108) : z0 et z1
// des pieces d'angle (triglyphe et regula en L), dont un sommet insere dans la
// largeur ferait une T-jonction sur l'arete d'onglet (L13).
std::vector<double> CornerPieceHeights (const TempleLayout &l)
{
	std::vector<double> z;
	for (const FriezePiece &t : l.frieze->triglyphs)
		if (t.kind == FriezeElement::CornerTriglyph)
		{
			z.push_back (t.z0);
			z.push_back (t.z1);
		}
	if (l.ornaments)
		for (const OrnamentPiece &o : l.ornaments->regulae)
			if (o.kind == OrnamentElement::CornerRegula)
			{
				z.push_back (o.z0);
				z.push_back (o.z1);
			}
	return z;
}

// Coordonnees interdites en u des sommets inseres dans la largeur de l'anneau
// (RF5, D3-108) : u de la section, faces des triglyphes, regulae et mutules,
// polygone rampant, noeuds de S' ramenes en u (|s'| - demi-distance), et
// sommets du dessus de la colonne d'angle sur une diagonale, centre compris,
// decales de l'inclinaison (+-|x| - i). Diagonale a h / 512 pres, h la
// demi-largeur du dessus de l'abaque (registre RF5) : la plus grande tolerance de
// soudure de D3-102 (1e-3) a l'echelle des deux jeux (h 0,685 et 0,570 : 1,3e-3
// et 1,1e-3), sans unite.
std::vector<double> RingWidthForbidden (const TempleSettings &s, const TempleLayout &l, const RefinementStations &stations,
                                        const std::vector<RingSectionPoint> &section, const Mesh &cornerColumn)
{
	const std::vector<float> &v = cornerColumn.GetVertices ();
	float top = v.empty () ? 0.0f : v[2], half = 0.0f;
	for (std::size_t k = 2; k < v.size (); k += 3)
		top = std::max (top, v[k]);
	for (std::size_t k = 0; k + 2 < v.size (); k += 3)
		if (v[k + 2] == top)
			half = std::max (half, std::max (std::fabs (v[k]), std::fabs (v[k + 1])));
	const double tolerance = static_cast<double> (half) / 512.0;
	auto diagonal = [&] (std::size_t k) {
		return v[k + 2] == top && std::fabs (std::fabs (v[k]) - std::fabs (v[k + 1])) <= tolerance;
	};
	std::size_t diagonals = 0;
	for (std::size_t k = 0; k + 2 < v.size (); k += 3)
		diagonals += diagonal (k) ? 1 : 0;
	std::vector<double> u;
	u.reserve (2 * l.frieze->triglyphs.size () + (l.ornaments ? 2 * (l.ornaments->regulae.size () + l.ornaments->mutules.size ()) : 0) +
	           (s.pediment ? s.pediment->rakingSection.size () : 0) + section.size () + stations.yLower.size () + stations.xLower.size () +
	           2 * diagonals);
	for (const FriezePiece &t : l.frieze->triglyphs)
	{
		u.push_back (t.d0);
		u.push_back (t.d1);
	}
	if (l.ornaments)
	{
		for (const OrnamentPiece &o : l.ornaments->regulae)
		{
			u.push_back (o.d0);
			u.push_back (o.d1);
		}
		for (const OrnamentPiece &o : l.ornaments->mutules)
		{
			u.push_back (o.d0);
			u.push_back (o.d1);
		}
	}
	if (s.pediment)
		for (const RingSectionPoint &q : s.pediment->rakingSection)
			u.push_back (s.pediment->tympanumFace + q.u);
	for (const RingSectionPoint &q : section)
		u.push_back (q.u);
	for (const double y : stations.yLower)
		u.push_back (std::fabs (y) - l.peristyle.halfDepth);
	for (const double x : stations.xLower)
		u.push_back (std::fabs (x) - l.peristyle.halfWidth);
	const double i = s.refinement ? s.refinement->inclination : 0.0;
	for (std::size_t k = 0; k + 2 < v.size (); k += 3)
		if (diagonal (k))
		{
			u.push_back (std::fabs (v[k]) - i);
			u.push_back (-std::fabs (v[k]) - i);
		}
	return u;
}

// Marge des boucles de l'anneau : p / 16, 0 sans courbure sur l'axe.
double LoopMargin (double pitch)
{
	return std::isfinite (pitch) ? pitch / 16.0 : 0.0;
}

// Majorant, en double, des faces des pieces a stations (degres de la krepis,
// anneau coupe dans sa largeur, deux Lambda, comble), compare a kTempleMaxFaces
// avant toute allocation. Chaque grille est majoree sur toutes les stations
// nominales ; l'anneau par n' boucles d'au plus 4 + 2 (|S_X| + |S_Y|) sommets,
// n' <= n (1 + ceil (W / p_u)), n <= 2 b + 3 sommets de section (contrat de
// GreekEntablatureSection : 2b + 2 - j - v, un de plus sous sous-face inclinee),
// W = max des saillies + innerFace, son etendue en u. Sans construire la section.
// Produit ou quotient arrondi en double avant toute somme : jamais contracte en
// FMA (lecture volatile, comme StrictCrossDifference).
double Rounded (double value)
{
	const volatile double v = value;
	return v;
}

// Borne de la calotte de l'abaque (registre RF5, plan 3 section 25.36) : le
// dessus de l'abaque est un eventail que les stations ne raffinent pas ; son
// ecart d'interpolation au champ est majore par F2 = (|f_XX| + |f_YY| +
// 2 |f_XY|) h^2 / 8 (h demi-cote du dessus de l'abaque, rayon du sommet du fut +
// abacusProjection), courbures du carreau de Coons majorees sur l'emprise des
// abaques, cisaillement compris : |u| <= min ((|x| + h + i) / a, 1 + h / a),
// |v| <= (|y| + h + i) / b, |f_XX| <= max aux bouts v = +-v_max de
// |(1 - v) s_F + (1 + v) s_B| / a^2 (lineaire en v), de meme |f_YY| en u avec
// s_L, s_R ; |f_XY| <= (|v| |s_L - s_R| +
// |u| |s_F - s_B|) / (a b). Refus si 1,1 F2 > kRefinementMaxTolerance / 2
// (F2 majore la calotte mesuree a 0,96 - 0,995, section 25.35 ; 10 % de marge).
void RequireAbacusCapWithinBound (const TempleSettings &s, const TempleLayout &l)
{
	const TempleRefinementSettings &r = *s.refinement;
	const double a = s.krepis.stylobateHalfWidth, b = s.krepis.stylobateHalfDepth;
	double h = 0.0;
	for (const GreekDoricColumnSettings *c : {&s.column, &s.cornerColumn})
		h = std::max (h, Rounded (c->upperDiameter / 2.0) + c->abacusProjection);
	double u = 0.0, v = 0.0;
	for (const TemplePlacement &p : l.columns)
	{
		u = std::max (u, ((std::fabs (p.x) + h) + r.inclination) / a);
		v = std::max (v, ((std::fabs (p.y) + h) + r.inclination) / b);
	}
	u = std::min (u, 1.0 + h / a);
	// Maximum de |(1 - v) s_F + (1 + v) s_B| sur |v| <= v_max : aux bouts (lineaire).
	auto ends = [] (double t, double s0, double s1) {
		return std::max (std::fabs (Rounded ((1.0 - t) * s0) + Rounded ((1.0 + t) * s1)),
		                 std::fabs (Rounded ((1.0 + t) * s0) + Rounded ((1.0 - t) * s1)));
	};
	const double fxx = Rounded (ends (v, r.sagittaFront, r.sagittaBack) / Rounded (a * a));
	const double fyy = Rounded (ends (u, r.sagittaLeft, r.sagittaRight) / Rounded (b * b));
	const double fxy = Rounded ((Rounded (v * std::fabs (r.sagittaLeft - r.sagittaRight)) + Rounded (u * std::fabs (r.sagittaFront - r.sagittaBack))) /
	                            Rounded (a * b));
	const double curvature = (fxx + fyy) + Rounded (2.0 * fxy);
	const double cap = Rounded (Rounded (curvature * Rounded (h * h)) / 8.0);
	if (!(Rounded (1.1 * cap) <= kRefinementMaxTolerance / 2.0))
		throw std::invalid_argument ("Raffinement : 'refinement.sagittaFront', 'refinement.sagittaRight', 'refinement.sagittaBack' et "
		                             "'refinement.sagittaLeft' courbent le champ au point que la calotte de l'abaque s'en ecarte de " +
		                             std::to_string (cap) + " : 1,1 fois cet ecart depasse kRefinementMaxTolerance / 2");
}

void RequireRefinedFacesWithinBound (const TempleSettings &s, const TempleLayout &l)
{
	// Entiers 64 bits : stations <= kRefinementMaxSegments par axe, degres
	// <= kKrepisMaxLowerSteps + 1 ; la seule grandeur reelle, le nombre de coupes
	// d'une arete, est bornee avant conversion. Aucune decision en double.
	using U = std::uint64_t;
	const RefinementStations &st = *l.stations;
	const U sx = st.x.size (), sy = st.y.size (), lx = st.xLower.size (), ly = st.yLower.size ();
	U faces = static_cast<U> (l.krepis.steps.size ()) *
	          (2 * (sx + 1) * (sy + 1) + 2 * (lx + 1) * (ly + 1) + 2 * (sx + lx + 2) + 2 * (sy + ly + 2));
	if (s.entablature && l.frieze)
	{
		const GreekEntablatureSettings &e = *s.entablature;
		double reach = 0.0;
		for (const EntablatureBand &b : e.bands)
			reach = std::max (reach, std::max (b.projection, b.topProjection));
		const double pu = std::min (st.pitchX, st.pitchY);
		const double quotient = std::isfinite (pu) ? (reach + e.innerFace) / pu : 0.0;
		if (!(quotient <= static_cast<double> (kRefinementMaxSegments)))
			throw std::invalid_argument ("Raffinement : 'refinement.chordDeviation' coupe l'anneau en plus de " +
			                             std::to_string (kRefinementMaxSegments) + " segments dans sa largeur");
		const U cuts = static_cast<U> (std::ceil (quotient));
		const U n = 2 * static_cast<U> (e.bands.size ()) + 3;
		faces += 2 * n * (1 + cuts) * (4 + 2 * (sx + sy));
		if (s.pediment)
		{
			const U nr = s.pediment->rakingSection.size ();
			faces += 2 * (2 * nr * (sx + 2) + 2 * nr);
			faces += 2 * (sx + 1) * (ly + 1) + 2 * (sx + 2) * (ly + 1) + 4 * (ly + 1) + 2 * (2 * sx + 3);
		}
	}
	if (faces > kTempleMaxFaces)
		throw std::invalid_argument ("Raffinement : 'refinement.chordDeviation' donne plus de " + std::to_string (kTempleMaxFaces) +
		                             " faces aux pieces a stations (majorant " + std::to_string (faces) + ")");
}

// Poches du comble (CE-14, CE-34), dans l'ordre des parties du temple qui en ont une : interieur du naos, plafond
// est (pteron est, puis porche ouest), plafonds des flancs (Left, Right).
std::vector<RoofPocket> RoofPocketsOf (const TempleLayout &layout)
{
	std::vector<RoofPocket> pockets;
	if (layout.interior)
		pockets.push_back (layout.interior->pocket);
	if (layout.eastCeiling)
		pockets.insert (pockets.end (), {layout.eastCeiling->eastPocket, layout.eastCeiling->westPocket});
	if (layout.flankCeilings)
		pockets.insert (pockets.end (), layout.flankCeilings->pockets.begin (), layout.flankCeilings->pockets.end ());
	return pockets;
}

Pieces BuildPieces (const TempleSettings &settings, const TempleLayout &layout, TempleWork &work)
{
	Pieces p;
	// Avec un raffinement, stations deplacees apres les seules colonnes (seconde
	// famille), puis coupes de l'anneau avant toute grille : un refus
	// "Raffinement : " arrive avant les grandes allocations.
	p.column = work.column ? std::move (work.column) : BuildGreekDoricColumnMesh (settings.column);
	p.cornerColumn = work.cornerColumn ? std::move (work.cornerColumn) : BuildGreekDoricColumnMesh (settings.cornerColumn);
	if (layout.stations)
		p.stations = DisplaceRefinementStations (*layout.stations, std::move (work.structural.first),
		                                         std::move (work.structural.second), work.clearance);
	std::vector<RingSectionPoint> refined;
	if (p.stations)
	{
		if (settings.entablature && layout.frieze)
		{
			const std::vector<RingSectionPoint> &section = SectionOnce (work);
			double reach = 0.0;
			for (const RingSectionPoint &q : section)
				reach = std::max (reach, std::fabs (q.u));
			refined = RefineRingSectionWidth (section, std::min (p.stations->pitchX, p.stations->pitchY), CornerPieceHeights (layout),
			                                  RingWidthForbidden (settings, layout, *p.stations, section, *p.cornerColumn),
			                                  std::max (layout.peristyle.halfWidth, layout.peristyle.halfDepth) + reach);
		}
		p.krepis = BuildKrepisMesh (settings.krepis, *p.stations, nullptr, &p.krepisSteps);
	}
	else
		p.krepis = BuildKrepisMesh (settings.krepis);
	p.Add (*p.krepis);
	for (const TemplePlacement &placement : layout.columns)
		p.Add (placement.corner ? *p.cornerColumn : *p.column);
	if (settings.entablature && layout.frieze)
	{
		if (p.stations)
			// Section coupee dans la largeur de ses aretes horizontales, puis
			// boucles a stations propres (D3-108) ; stations S inchangees.
			p.ring = SweepRectangularRing (refined, layout.peristyle.halfWidth, layout.peristyle.halfDepth, layout.entablatureFoot,
			                               p.stations->x, p.stations->y, LoopMargin (p.stations->pitchX),
			                               LoopMargin (p.stations->pitchY));
		else
			p.ring = SweepRectangularRing (SectionOnce (work), layout.peristyle.halfWidth,
			                               layout.peristyle.halfDepth, layout.entablatureFoot);
		p.Add (*p.ring);
		// Borne des faces comparee apres chaque piece construite, sur les preuves
		// de validation de la mise en place.
		static const OrnamentLayout none;
		const OrnamentLayout &ornaments = layout.ornaments ? *layout.ornaments : none;
		const detail::CheckedEntablature &checked = work.entablature->Checked ();
		p.entablature = detail::BuildEntablaturePieces (checked, layout.peristyle, *layout.frieze, ornaments, layout.entablatureFoot,
		                                                [&p] (const Mesh &piece) { p.Add (piece); });
		if (settings.pediment && p.stations)
			p.pediment = detail::BuildPedimentPieces (*work.pediment, checked, layout.peristyle, layout.entablatureFoot, *p.stations,
			                                         [&p] (const Mesh &piece) { p.Add (piece); });
		else if (settings.pediment)
			p.pediment = detail::BuildPedimentPieces (*work.pediment, checked, layout.peristyle, layout.entablatureFoot,
			                                         [&p] (const Mesh &piece) { p.Add (piece); }, RoofPocketsOf (layout));
	}
	if (settings.cella && layout.cella)
	{
		p.cella = BuildGreekCellaPieces (*settings.cella, *layout.cella);
		for (const CellaPiece &piece : p.cella)
			p.Add (*piece.mesh);
		if (settings.cella->porches)
		{
			p.porchColumn = BuildGreekDoricColumnMesh (settings.cella->porches->column);
			for (std::size_t k = 0; k < layout.cella->porchColumns.size (); ++k)
				p.Add (*p.porchColumn);
		}
	}
	if (settings.interior && layout.interior)
	{
		p.interior = BuildGreekNaosInteriorPieces (*settings.interior, *layout.interior);
		for (const NaosInteriorPiece &piece : p.interior)
			p.Add (*piece.mesh);
		p.lowerInteriorColumn = BuildGreekDoricColumnMesh (settings.interior->lowerColumn);
		p.upperInteriorColumn = BuildGreekDoricColumnMesh (settings.interior->upperColumn);
		for (const NaosInteriorColumn &c : layout.interior->columns)
			p.Add (c.storey == NaosInteriorStorey::Lower ? *p.lowerInteriorColumn : *p.upperInteriorColumn);
	}
	if (settings.epicranitis && layout.epicranitis)
	{
		p.epicranitis = BuildGreekEpicranitisPieces (*settings.epicranitis, *layout.epicranitis);
		for (const EpicranitisPiece &piece : p.epicranitis)
			p.Add (*piece.mesh);
		if (settings.porchExtension && layout.porchExtension)
		{
			p.porchExtension = BuildGreekPorchExtensionPieces (*settings.epicranitis, *layout.epicranitis, *layout.porchExtension);
			for (const PorchExtensionPiece &piece : p.porchExtension)
				p.Add (*piece.mesh);
		}
	}
	if (settings.eastCeiling && layout.eastCeiling)
	{
		p.eastCeiling = BuildGreekEastCeilingPieces (*settings.eastCeiling, *layout.eastCeiling);
		for (const CeilingPiece &piece : p.eastCeiling)
			p.Add (*piece.mesh);
		if (settings.flankCeilings && layout.flankCeilings)
		{
			p.flankCeilings = BuildGreekFlankCeilingPieces (*settings.flankCeilings, *settings.eastCeiling, *layout.flankCeilings);
			for (const CeilingPiece &piece : p.flankCeilings)
				p.Add (*piece.mesh);
		}
	}
	return p;
}

// Face interieure de l'anneau : demi-largeur et demi-profondeur, par
// l'expression de SweepRectangularRing (halfX + u, u = -innerFace).
struct RingInner
{
	double halfWidth = 0.0, halfDepth = 0.0;
};

RingInner RingInnerOf (const PeristyleLayout &peristyle, const GreekEntablatureSettings &entablature)
{
	const double u = -entablature.innerFace;
	return {peristyle.halfWidth + u, peristyle.halfDepth + u};
}

// Cella dans le temple (CE-4, CE-9) : refus du raffinement, mise en place
// propre, emprise hors des colonnes du peristyle et, avec un entablement, a
// l'interieur de l'anneau et sous son dessus.
CellaLayout LayoutCellaInTemple (const TempleSettings &s, const TempleLayout &l, const TempleWork &work)
{
	CellaLayout cella = LayoutGreekCella (*s.cella, l.krepis);
	for (const TemplePlacement &c : l.columns)
	{
		const double e = c.corner ? l.cornerColumnExtents.overall : l.columnExtents.overall;
		const bool apartX = std::fabs (c.x) - e > cella.halfWidth;
		const bool apartY = c.y + e < cella.front || c.y - e > cella.back;
		if (!(apartX || apartY))
			Fail ("cella", "place le toichobate au contact d'une colonne du peristyle");
	}
	if (s.entablature && work.entablature)
	{
		const RingInner ring = RingInnerOf (l.peristyle, *s.entablature);
		if (!(cella.halfWidth < ring.halfWidth && -cella.front < ring.halfDepth && cella.back < ring.halfDepth))
			Fail ("cella", "sort de la face interieure de l'anneau de l'entablement");
		const double ringTop = l.entablatureFoot + work.entablature->Bounds ().back ();
		if (!(cella.top < ringTop))
			Fail ("cella", "depasse le dessus de l'entablement, sous le comble (registre CE-4)");
	}
	return cella;
}

// Couronnement dans le temple (CE-24, CE-4 etendu) : mise en place propre, face
// interieure commune avec l'assise XVII interieure, et, avec un entablement,
// emprise strictement dans la face interieure de l'anneau et dessus sous le sien.
EpicranitisLayout LayoutEpicranitisInTemple (const TempleSettings &s, const TempleLayout &l, const TempleWork &work)
{
	EpicranitisLayout crown = LayoutGreekEpicranitis (*s.epicranitis, *l.cella);
	if (s.interior && !(s.interior->courseThickness == s.epicranitis->innerOffset))
		Fail ("epicranitis.innerOffset", "differe de 'interior.courseThickness' : la face interieure de l'epicranitis est "
		                                 "celle de l'assise XVII interieure (registre CE-24)");
	if (s.entablature && work.entablature)
	{
		const RingInner ring = RingInnerOf (l.peristyle, *s.entablature);
		if (!(crown.halfWidth < ring.halfWidth && -crown.front < ring.halfDepth && crown.back < ring.halfDepth))
			Fail ("epicranitis", "sort de la face interieure de l'anneau de l'entablement");
		const double ringTop = l.entablatureFoot + work.entablature->Bounds ().back ();
		if (!(crown.top < ringTop))
			Fail ("epicranitis", "depasse le dessus de l'entablement, sous le comble (registre CE-4)");
	}
	return crown;
}

// Prolongement est dans le temple : bouts sur la face interieure de l'anneau
// (RingInnerOf), pied au dessous de l'anneau.
PorchExtensionLayout LayoutPorchExtensionInTemple (const TempleSettings &s, const TempleLayout &l)
{
	return LayoutGreekPorchExtension (*s.porchExtension, *l.epicranitis, RingInnerOf (l.peristyle, *s.entablature).halfWidth,
	                                  l.entablatureFoot);
}

// Plafond est dans le temple : bouts et massif sur la face interieure de l'anneau (RingInnerOf), poche est sous les
// pans du comble (critere du fronton, en double puis en float).
EastCeilingLayout LayoutEastCeilingInTemple (const TempleSettings &s, const TempleLayout &l, const TempleWork &work)
{
	const RingInner ring = RingInnerOf (l.peristyle, *s.entablature);
	EastCeilingLayout east = LayoutGreekEastCeiling (*s.eastCeiling, *l.epicranitis, ring.halfWidth, ring.halfDepth);
	if (s.pediment && work.pediment)
	{
		const detail::CheckedEntablature &checked = work.entablature->Checked ();
		if (detail::PocketMeetsRoof (*work.pediment, checked, l.peristyle, l.entablatureFoot, east.westPocket))
			Fail ("eastCeiling.bandHeight", "met le dessus de la corniche et du bandeau du porche ouest au contact ou "
			                                "au-dessus d'un pan du comble");
		if (detail::PocketMeetsRoof (*work.pediment, checked, l.peristyle, l.entablatureFoot, east.eastPocket))
			Fail ("eastCeiling.beamHeight", "met le dessus des pieces au contact ou au-dessus d'un pan du comble");
	}
	return east;
}

// Plafonds des flancs dans le temple : bout cote peristyle a peristyleGap de la face interieure de l'anneau
// (RingInnerOf), poches des flancs sous les pans du comble (critere du fronton, en double puis en float, CE-42).
FlankCeilingLayout LayoutFlankCeilingsInTemple (const TempleSettings &s, const TempleLayout &l, const TempleWork &work)
{
	const RingInner ring = RingInnerOf (l.peristyle, *s.entablature);
	FlankCeilingLayout flanks = LayoutGreekFlankCeilings (*s.flankCeilings, *s.eastCeiling, *l.eastCeiling, ring.halfWidth);
	if (s.pediment && work.pediment)
		for (const RoofPocket &pocket : flanks.pockets)
			if (detail::PocketMeetsRoof (*work.pediment, work.entablature->Checked (), l.peristyle, l.entablatureFoot, pocket))
				Fail ("flankCeilings.peristyleGap", "laisse le dessus des poutres et des dalles au contact ou au-dessus d'un "
				                                    "pan du comble");
	return flanks;
}

// TE3 : le pied de chaque colonne dans le dessus du stylobate, bord compris.
void RequireFeetOnStylobate (const TempleLayout &layout, const KrepisSettings &krepis)
{
	for (const TemplePlacement &p : layout.columns)
	{
		const double foot = p.corner ? layout.cornerColumnExtents.foot : layout.columnExtents.foot;
		if (!(std::fabs (p.x) + foot <= krepis.stylobateHalfWidth))
			Fail ("krepis.stylobateHalfWidth", "ne contient pas le pied d'une colonne");
		if (!(std::fabs (p.y) + foot <= krepis.stylobateHalfDepth))
			Fail ("krepis.stylobateHalfDepth", "ne contient pas le pied d'une colonne");
	}
}

// Mise en place ; `work` recoit les preuves de validation de l'entablement et
// du fronton et, avec un raffinement, les maillages des colonnes construits
// pour la borne de la seconde famille.
TempleLayout LayoutWithColumns (const TempleSettings &s, TempleWork &work)
{
	TempleLayout layout;
	layout.krepis = LayoutKrepis (s.krepis);
	layout.peristyle = LayoutPeristyle (s.peristyle);
	layout.columnExtents = ComputeGreekDoricColumnHalfExtents (s.column);
	layout.cornerColumnExtents = ComputeGreekDoricColumnHalfExtents (s.cornerColumn);

	const double en = layout.columnExtents.overall, ec = layout.cornerColumnExtents.overall;
	const PeristyleLayout &pl = layout.peristyle;
	const std::vector<double> xs = PlacedRow (pl, PeristyleSide::Front, pl.halfWidth);
	const std::vector<double> ys = PlacedRow (pl, PeristyleSide::Right, pl.halfDepth);
	RequireRowSeparation (xs, pl.flank, en, ec, "peristyle.frontSpacing", "peristyle.frontCornerSpacing");
	RequireRowSeparation (ys, pl.front, en, ec, "peristyle.flankSpacing", "peristyle.flankCornerSpacing");
	RequireCornerNeighboursSeparation (xs, ys, en, ec);

	layout.columns.reserve (layout.peristyle.columns.size ());
	int rank = 0;
	for (std::size_t k = 0; k < layout.peristyle.columns.size (); ++k)
	{
		const PeristyleColumn &c = layout.peristyle.columns[k];
		rank = k > 0 && layout.peristyle.columns[k - 1].side == c.side ? rank + 1 : 0;
		TemplePlacement placement;
		placement.address = {TempleElement::Column, c.side, rank};
		placement.x = c.x;
		placement.y = c.y;
		placement.z = layout.krepis.stylobateTop;
		placement.corner = c.corner;
		layout.columns.push_back (placement);
	}
	RequireFeetOnStylobate (layout, s.krepis);

	if (s.entablature)
	{
		const detail::CheckedEntablature checked = detail::CheckEntablature (*s.entablature);
		// EN10 : l'anneau pose sur le sommet float des abaques, a la cote que
		// Accumulator::Append donne au sommet d'un tour (float (height) plus
		// stylobateTop, en double).
		const float top = static_cast<float> (s.column.height);
		if (!(static_cast<float> (s.cornerColumn.height) == top))
			Fail ("cornerColumn.height", "differe en float de 'column.height' : l'entablement ne peut poser sur deux hauteurs");
		layout.entablatureFoot = static_cast<double> (top) + layout.krepis.stylobateTop;
		// Cotes des bandes apres EN10 : leurs refus suivent le sien.
		work.entablature = detail::MeasureEntablature (checked);
		layout.frieze = detail::PlaceFrieze (*work.entablature, layout.peristyle);
		if (s.entablature->regulae || s.entablature->mutules)
			layout.ornaments = detail::PlaceOrnaments (*work.entablature, layout.peristyle, *layout.frieze);
	}
	if (s.pediment)
	{
		if (!s.entablature)
			Fail ("pediment", "demande un entablement");
		work.pediment = detail::CheckPediment (*s.pediment, work.entablature->Checked ());
		detail::RequirePedimentTopWithinCrown (*work.pediment, layout.peristyle.halfWidth);
	}
	if (s.interior)
	{
		if (s.refinement)
			Fail ("interior", "et 'refinement' ne se combinent pas : l'interieur, comme la cella, n'a pas de raffinements "
			                  "(README section 7.8, decision D8 ; registre CE-9)");
		if (!s.cella)
			Fail ("interior", "demande une cella");
	}
	if (s.epicranitis)
	{
		if (s.refinement)
			Fail ("epicranitis", "et 'refinement' ne se combinent pas : le couronnement, comme la cella, n'a pas de "
			                     "raffinements (README section 7.8, decision D8 ; registre CE-9)");
		if (!s.cella)
			Fail ("epicranitis", "demande une cella");
	}
	if (s.porchExtension)
	{
		if (s.refinement)
			Fail ("porchExtension", "et 'refinement' ne se combinent pas : le prolongement est, comme la cella, n'a pas de "
			                        "raffinements (registre CE-9)");
		if (!s.epicranitis)
			Fail ("porchExtension", "demande le couronnement ('epicranitis')");
		if (!s.entablature)
			Fail ("porchExtension", "demande un entablement");
	}
	if (s.eastCeiling)
	{
		if (s.refinement)
			Fail ("eastCeiling", "et 'refinement' ne se combinent pas : les plafonds, comme la cella, n'ont pas de "
			                     "raffinements (registre CE-9)");
		if (!s.porchExtension)
			Fail ("eastCeiling", "demande le prolongement est ('porchExtension'), qui demande un entablement");
	}
	if (s.flankCeilings)
	{
		if (s.refinement)
			Fail ("flankCeilings", "et 'refinement' ne se combinent pas : les plafonds, comme la cella, n'ont pas de "
			                       "raffinements (registre CE-9)");
		if (!s.eastCeiling)
			Fail ("flankCeilings", "demande le plafond est ('eastCeiling')");
	}
	if (s.cella)
	{
		if (s.refinement)
			Fail ("cella", "et 'refinement' ne se combinent pas : la cella n'a pas de raffinements (README section 7.8, "
			               "decision D8) et le champ de courbure ne s'y applique pas (registre CE-9)");
		layout.cella = LayoutCellaInTemple (s, layout, work);
	}
	if (s.interior)
		layout.interior = LayoutGreekNaosInterior (*s.interior, *layout.cella);
	if (s.epicranitis)
		layout.epicranitis = LayoutEpicranitisInTemple (s, layout, work);
	if (s.porchExtension)
		layout.porchExtension = LayoutPorchExtensionInTemple (s, layout);
	if (s.eastCeiling)
		layout.eastCeiling = LayoutEastCeilingInTemple (s, layout, work);
	if (s.flankCeilings)
		layout.flankCeilings = LayoutFlankCeilingsInTemple (s, layout, work);
	if (s.refinement)
	{
		const TempleRefinementSettings &r = *s.refinement;
		ValidateTempleRefinementSettings (r, layout.krepis);
		// TE4 elargie : chaque somme de deux demi-etendues augmente de |i|.
		const double half = r.inclination / 2.0;
		RequireRowSeparation (xs, pl.flank, en + half, ec + half, "refinement", "refinement");
		RequireRowSeparation (ys, pl.front, en + half, ec + half, "refinement", "refinement");
		RequireCornerNeighboursSeparation (xs, ys, en + half, ec + half, "refinement",
		                                   "place deux voisines d'un angle a moins de la somme de leurs demi-etendues plus l'inclinaison");
		for (TemplePlacement &placement : layout.columns)
			Incline (placement, r.inclination);
		layout.stations = NominalRefinementStations (r, layout.krepis);
		RequireRefinedFacesWithinBound (s, layout);
		RequireAbacusCapWithinBound (s, layout);
		RequireStationClearance (s, layout, work);
	}
	return layout;
}

} // namespace

TempleAddress TempleAddressOf (const CellaPiece &piece)
{
	TempleElement element = TempleElement::Toichobate;
	switch (piece.element)
	{
	case CellaElement::Toichobate:      element = TempleElement::Toichobate; break;
	case CellaElement::NaosFloor:       element = TempleElement::NaosFloor; break;
	case CellaElement::LongWall:        element = TempleElement::LongWall; break;
	case CellaElement::CrossWall:       element = TempleElement::CrossWall; break;
	case CellaElement::Threshold:       element = TempleElement::Threshold; break;
	case CellaElement::Anta:            element = TempleElement::Anta; break;
	case CellaElement::PorchArchitrave: element = TempleElement::PorchArchitrave; break;
	case CellaElement::PorchFrieze:     element = TempleElement::PorchFrieze; break;
	}
	return {element, piece.side, piece.index};
}

TempleAddress TempleAddressOf (const NaosInteriorPiece &piece)
{
	TempleElement element = TempleElement::InteriorCourse;
	switch (piece.element)
	{
	case NaosInteriorElement::Course:        element = TempleElement::InteriorCourse; break;
	case NaosInteriorElement::LowerEpistyle: element = TempleElement::InteriorLowerEpistyle; break;
	case NaosInteriorElement::UpperEpistyle: element = TempleElement::InteriorUpperEpistyle; break;
	case NaosInteriorElement::Cornice:       element = TempleElement::InteriorCornice; break;
	}
	return {element, piece.side, piece.index};
}

TempleAddress TempleAddressOf (const EpicranitisPiece &piece)
{
	TempleElement element = TempleElement::EpicranitisCourse;
	switch (piece.element)
	{
	case EpicranitisElement::Course:      element = TempleElement::EpicranitisCourse; break;
	case EpicranitisElement::PorchCourse: element = TempleElement::EpicranitisPorchCourse; break;
	case EpicranitisElement::CrossCourse: element = TempleElement::EpicranitisCrossCourse; break;
	case EpicranitisElement::Crown:       element = TempleElement::PorchCrown; break;
	case EpicranitisElement::Moulding:    element = TempleElement::CrownMoulding; break;
	}
	return {element, piece.side, piece.index};
}

TempleAddress TempleAddressOf (const CeilingPiece &piece)
{
	TempleElement element = TempleElement::CeilingBeam;
	switch (piece.element)
	{
	case CeilingElement::Beam:    element = TempleElement::CeilingBeam; break;
	case CeilingElement::Cornice: element = TempleElement::PorchCornice; break;
	case CeilingElement::Band:    element = TempleElement::PorchBand; break;
	case CeilingElement::Slab:    element = TempleElement::CeilingSlab; break;
	}
	return {element, piece.side, piece.index};
}

TempleAddress TempleAddressOf (const PorchExtensionPiece &piece)
{
	TempleElement element = TempleElement::ExtensionArchitrave;
	switch (piece.element)
	{
	case PorchExtensionElement::Architrave: element = TempleElement::ExtensionArchitrave; break;
	case PorchExtensionElement::Frieze:     element = TempleElement::ExtensionFrieze; break;
	case PorchExtensionElement::Crown:      element = TempleElement::ExtensionCrown; break;
	case PorchExtensionElement::Moulding:   element = TempleElement::ExtensionMoulding; break;
	}
	return {element, piece.side, piece.index};
}

TempleLayout LayoutTemple (const TempleSettings &s)
{
	TempleWork work;
	return LayoutWithColumns (s, work);
}

void ValidateTempleSettings (const TempleSettings &settings)
{
	static_cast<void> (LayoutTemple (settings));
}

std::size_t CountTempleMeshFaces (const TempleSettings &settings)
{
	TempleWork work;
	const TempleLayout layout = LayoutWithColumns (settings, work);
	return BuildPieces (settings, layout, work).faces;
}

std::unique_ptr<Mesh> BuildTempleMesh (const TempleSettings &settings, TempleLayout *layout,
                                       std::vector<TempleMeshPart> *parts)
{
	TempleWork work;
	TempleLayout built = LayoutWithColumns (settings, work);
	const Pieces pieces = BuildPieces (settings, built, work);

	Accumulator acc;
	acc.vertices.reserve (3 * pieces.vertices);
	acc.faces.reserve (3 * pieces.faces);
	// Sans raffinement, Accumulator::Append ; avec, la carte des sommets (RF3).
	VertexMap map;
	if (settings.refinement)
	{
		map.refinement = &*settings.refinement;
		map.a = settings.krepis.stylobateHalfWidth;
		map.b = settings.krepis.stylobateHalfDepth;
	}
	auto append = [&] (const Mesh &piece, double tx, double ty, double tz, const Shear &shear, const TempleAddress &address) {
		return map.refinement ? AppendMapped (acc, piece, tx, ty, tz, shear, map, address) : acc.Append (piece, tx, ty, tz, address);
	};
	std::vector<TempleMeshPart> built_parts;
	const TempleMeshPart whole = append (*pieces.krepis, 0.0, 0.0, 0.0, {}, {});
	built_parts = pieces.stations ? SplitKrepis (whole, pieces.krepisSteps, acc.faces)
	                              : SplitKrepis (whole, built.krepis.steps.size (), acc.faces);
	built_parts.reserve (built.krepis.steps.size () + built.columns.size () + (pieces.ring ? 1 : 0) +
	                     pieces.entablature.size () + pieces.pediment.size () + pieces.cella.size () +
	                     (built.cella ? built.cella->porchColumns.size () : 0) + pieces.interior.size () +
	                     (built.interior ? built.interior->columns.size () : 0) + pieces.epicranitis.size () +
	                     pieces.porchExtension.size () + pieces.eastCeiling.size () + pieces.flankCeilings.size ());
	for (const TemplePlacement &p : built.columns)
	{
		Shear shear;
		if (map.refinement)
		{
			shear.active = true;
			shear.ix = p.inclinationX;
			shear.iy = p.inclinationY;
			shear.height = static_cast<double> (static_cast<float> (p.corner ? settings.cornerColumn.height : settings.column.height));
		}
		built_parts.push_back (append (p.corner ? *pieces.cornerColumn : *pieces.column, p.x, p.y, p.z, shear, p.address));
	}
	// Entablement : pieces deja dans le repere du temple, translation nulle
	// (l'arrondi float d'un float plus 0 le rend inchange).
	if (pieces.ring)
	{
		built_parts.push_back (append (*pieces.ring, 0.0, 0.0, 0.0, {}, {TempleElement::Entablature, PeristyleSide::Front, 0}));
		std::vector<TempleAddress> addresses;
		addresses.reserve (pieces.entablature.size ());
		for (const FriezePiece &t : built.frieze->triglyphs)
			addresses.push_back ({TempleElement::Triglyph, t.side, t.rank});
		if (built.ornaments)
		{
			for (const OrnamentPiece &o : built.ornaments->regulae)
				addresses.push_back ({TempleElement::Regula, o.side, o.rank});
			for (const OrnamentPiece &o : built.ornaments->mutules)
				addresses.push_back ({TempleElement::Mutule, o.side, o.rank});
		}
		if (addresses.size () != pieces.entablature.size ())
			throw std::logic_error ("Temple : pieces de l'entablement et adresses en nombres differents");
		for (std::size_t k = 0; k < pieces.entablature.size (); ++k)
			built_parts.push_back (append (*pieces.entablature[k], 0.0, 0.0, 0.0, {}, addresses[k]));
		if (!pieces.pediment.empty ())
		{
			// Corniches rampantes Front et Back, puis le comble : une piece, ou ses tranches par Y croissants (CE-34),
			// d'index leur rang.
			const TempleAddress raking[] = {{TempleElement::RakingCornice, PeristyleSide::Front, 0},
			                                {TempleElement::RakingCornice, PeristyleSide::Back, 0}};
			if (pieces.pediment.size () <= std::size (raking))
				throw std::logic_error ("Temple : pieces du fronton et adresses en nombres differents");
			for (std::size_t k = 0; k < pieces.pediment.size (); ++k)
				built_parts.push_back (append (*pieces.pediment[k], 0.0, 0.0, 0.0, {},
				                               k < std::size (raking)
				                                   ? raking[k]
				                                   : TempleAddress {TempleElement::Roof, PeristyleSide::Front,
				                                                    static_cast<int> (k - std::size (raking))}));
		}
	}
	// Cella : pieces deja dans le repere du temple, puis les colonnes des porches.
	for (const CellaPiece &piece : pieces.cella)
		built_parts.push_back (append (*piece.mesh, 0.0, 0.0, 0.0, {}, TempleAddressOf (piece)));
	if (pieces.porchColumn)
		for (const CellaPorchColumn &c : built.cella->porchColumns)
			built_parts.push_back (append (*pieces.porchColumn, c.x, c.y, c.z, {}, {TempleElement::PorchColumn, c.side, c.index}));
	// Interieur : pieces deja dans le repere du temple, puis les colonnes des deux etages.
	for (const NaosInteriorPiece &piece : pieces.interior)
		built_parts.push_back (append (*piece.mesh, 0.0, 0.0, 0.0, {}, TempleAddressOf (piece)));
	if (built.interior && pieces.lowerInteriorColumn)
		for (const NaosInteriorColumn &c : built.interior->columns)
		{
			const bool lower = c.storey == NaosInteriorStorey::Lower;
			built_parts.push_back (append (lower ? *pieces.lowerInteriorColumn : *pieces.upperInteriorColumn, c.x, c.y, c.z, {},
			                               {lower ? TempleElement::InteriorLowerColumn : TempleElement::InteriorUpperColumn, c.side,
			                                c.index}));
		}
	// Couronnement puis prolongement est : pieces deja dans le repere du temple.
	for (const EpicranitisPiece &piece : pieces.epicranitis)
		built_parts.push_back (append (*piece.mesh, 0.0, 0.0, 0.0, {}, TempleAddressOf (piece)));
	for (const PorchExtensionPiece &piece : pieces.porchExtension)
		built_parts.push_back (append (*piece.mesh, 0.0, 0.0, 0.0, {}, TempleAddressOf (piece)));
	// Plafond est, puis plafonds des flancs : pieces deja dans le repere du temple.
	for (const CeilingPiece &piece : pieces.eastCeiling)
		built_parts.push_back (append (*piece.mesh, 0.0, 0.0, 0.0, {}, TempleAddressOf (piece)));
	for (const CeilingPiece &piece : pieces.flankCeilings)
		built_parts.push_back (append (*piece.mesh, 0.0, 0.0, 0.0, {}, TempleAddressOf (piece)));

	auto mesh = std::make_unique<Mesh> ();
	mesh->SetVertices (static_cast<unsigned> (acc.vertices.size () / 3), acc.vertices.data ());
	mesh->SetFaces (static_cast<unsigned> (acc.faces.size () / 3), 3, acc.faces.data ());
	if (layout)
		*layout = std::move (built);
	if (parts)
		*parts = std::move (built_parts);
	return mesh;
}

} // namespace classical
} // namespace cgarchi
