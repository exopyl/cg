#include <cgarchi/classical_temple.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
                                        double ec)
{
	const double sum = (xs.size () >= 3 ? en : ec) + (ys.size () >= 3 ? en : ec);
	const double dxs[2] = {xs[1] - xs[0], xs[xs.size () - 1] - xs[xs.size () - 2]};
	const double dys[2] = {ys[1] - ys[0], ys[ys.size () - 1] - ys[ys.size () - 2]};
	for (const double dx : dxs)
		for (const double dy : dys)
			if (!(dx > sum || dy > sum))
				Fail ("peristyle.frontCornerSpacing",
				      "et 'peristyle.flankCornerSpacing' ne separent pas les deux voisines d'un angle");
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

Pieces BuildPieces (const TempleSettings &settings, const TempleLayout &layout)
{
	Pieces p;
	p.krepis = BuildKrepisMesh (settings.krepis);
	p.column = BuildGreekDoricColumnMesh (settings.column);
	p.cornerColumn = BuildGreekDoricColumnMesh (settings.cornerColumn);
	p.Add (*p.krepis);
	for (const TemplePlacement &placement : layout.columns)
		p.Add (placement.corner ? *p.cornerColumn : *p.column);
	if (settings.entablature && layout.frieze)
	{
		const GreekEntablatureSettings &e = *settings.entablature;
		p.ring = SweepRectangularRing (GreekEntablatureSection (e), layout.peristyle.halfWidth,
		                               layout.peristyle.halfDepth, layout.entablatureFoot);
		p.Add (*p.ring);
		// Borne des faces comparee apres chaque piece construite, une seule
		// validation des reglages pour toutes.
		static const OrnamentLayout none;
		const OrnamentLayout &ornaments = layout.ornaments ? *layout.ornaments : none;
		p.entablature = BuildEntablaturePieceMeshes (e, layout.peristyle, *layout.frieze, ornaments, layout.entablatureFoot,
		                                             [&p] (const Mesh &piece) { p.Add (piece); });
	}
	return p;
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

} // namespace

TempleLayout LayoutTemple (const TempleSettings &s)
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
		ValidateGreekEntablatureSettings (*s.entablature);
		// EN10 : l'anneau pose sur le sommet float des abaques, a la cote que
		// Accumulator::Append donne au sommet d'un tour (float (height) plus
		// stylobateTop, en double).
		const float top = static_cast<float> (s.column.height);
		if (!(static_cast<float> (s.cornerColumn.height) == top))
			Fail ("cornerColumn.height", "differe en float de 'column.height' : l'entablement ne peut poser sur deux hauteurs");
		layout.entablatureFoot = static_cast<double> (top) + layout.krepis.stylobateTop;
		layout.frieze = LayoutGreekFrieze (*s.entablature, layout.peristyle);
		if (s.entablature->regulae || s.entablature->mutules)
			layout.ornaments = LayoutGreekOrnaments (*s.entablature, layout.peristyle);
	}
	return layout;
}

void ValidateTempleSettings (const TempleSettings &settings)
{
	static_cast<void> (LayoutTemple (settings));
}

std::size_t CountTempleMeshFaces (const TempleSettings &settings)
{
	return BuildPieces (settings, LayoutTemple (settings)).faces;
}

std::unique_ptr<Mesh> BuildTempleMesh (const TempleSettings &settings, TempleLayout *layout,
                                       std::vector<TempleMeshPart> *parts)
{
	TempleLayout built = LayoutTemple (settings);
	const Pieces pieces = BuildPieces (settings, built);

	Accumulator acc;
	acc.vertices.reserve (3 * pieces.vertices);
	acc.faces.reserve (3 * pieces.faces);
	std::vector<TempleMeshPart> built_parts;
	const TempleMeshPart whole = acc.Append (*pieces.krepis, 0.0, 0.0, 0.0, {});
	built_parts = SplitKrepis (whole, built.krepis.steps.size (), acc.faces);
	built_parts.reserve (built.krepis.steps.size () + built.columns.size () + (pieces.ring ? 1 : 0) +
	                     pieces.entablature.size ());
	for (const TemplePlacement &p : built.columns)
		built_parts.push_back (acc.Append (p.corner ? *pieces.cornerColumn : *pieces.column, p.x, p.y, p.z, p.address));
	// Entablement : pieces deja dans le repere du temple, translation nulle
	// (l'arrondi float d'un float plus 0 le rend inchange).
	if (pieces.ring)
	{
		built_parts.push_back (acc.Append (*pieces.ring, 0.0, 0.0, 0.0, {TempleElement::Entablature, PeristyleSide::Front, 0}));
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
			built_parts.push_back (acc.Append (*pieces.entablature[k], 0.0, 0.0, 0.0, addresses[k]));
	}

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
