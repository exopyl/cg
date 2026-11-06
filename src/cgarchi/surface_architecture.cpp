#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <cgarchi/surface_architecture.h>
#include <cgmath/cgmath.h>

namespace cgarchi
{

namespace
{

float toFloat (double v) { return static_cast<float> (v); }

void requireFinite (float value, const char *function, const char *name)
{
	if (!std::isfinite (value))
		throw std::invalid_argument (std::string (function) + ": " + name + " is not finite");
}

// Refuse npts = 1 : un echantillonnage d'arc exige ses deux extremites.
void requireTwoSamplesOrNone (unsigned npts, const char *function)
{
	if (npts == 1)
		throw std::invalid_argument (std::string (function) + ": npts must be 0 or at least 2");
}

// Angle de l'echantillon i sur count echantillons repartis de start a end,
// extremites comprises ; un echantillon unique est pris en start.
double sampleAngle (double start, double end, unsigned i, unsigned count)
{
	if (count < 2)
		return start;
	return start + (end - start) * i / (count - 1);
}

ArcSamples makeSamples (unsigned npts)
{
	ArcSamples s;
	s.points.resize (npts);
	s.tangents.resize (npts);
	return s;
}

// Cercle echantillonne de angle0 a angle1 inclus.
std::vector<Vector2f> sampleCircle (const Vector2f &center, float radius,
                                    float angle0, float angle1, unsigned npts)
{
	std::vector<Vector2f> points (npts);
	for (unsigned i = 0; i < npts; ++i)
	{
		const float alpha = (npts < 2) ? angle0
			: angle0 + (angle1 - angle0) * static_cast<float> (i) / static_cast<float> (npts - 1);
		points[i] = Vector2f (center[0] + radius * std::cos (alpha),
		                      center[1] + radius * std::sin (alpha));
	}
	return points;
}

} // namespace

std::unique_ptr<Mesh> CreateBlock (float _width, float _height, float _depth, float bevel)
{
	auto mesh = std::make_unique<Mesh> (24, 26);

	const float width = _width / 2.f;
	const float height = _height / 2.f;
	const float depth = _depth / 2.f;

	// vertices
	float vertices[3*24] = {width-bevel,    depth-bevel,   -height,
				width-bevel,   -(depth-bevel), -height,
				-(width-bevel), -(depth-bevel), -height,
				-(width-bevel),  depth-bevel,   -height,
				width-bevel,    depth-bevel,    height,
				-(width-bevel),  depth-bevel,    height,
				-(width-bevel), -(depth-bevel),  height,
				width-bevel,   -(depth-bevel),  height,
				width,          depth-bevel,   -(height-bevel),
				width,          depth-bevel,    height-bevel,
				width,         -(depth-bevel),  height-bevel,
				width,         -(depth-bevel), -(height-bevel),
				width-bevel,   -depth,         -(height-bevel),
				width-bevel,   -depth,           height-bevel,
				-(width-bevel), -depth,           height-bevel,
				-(width-bevel), -depth,         -(height-bevel),
				-width,         -(depth-bevel), -(height-bevel),
				-width,         -(depth-bevel),  height-bevel,
				-width,          depth-bevel,    height-bevel,
				-width,          depth-bevel,   -(height-bevel),
				width-bevel,    depth,          height-bevel,
				width-bevel,    depth,         -(height-bevel),
				-(width-bevel),  depth,         -(height-bevel),
				-(width-bevel),  depth,          height-bevel};
	mesh->SetVertices (24, vertices);

	// faces
	mesh->SetFace (0, 0, 1, 2, 3);
	mesh->SetFace (1, 6, 7, 4, 5);
	mesh->SetFace (2, 10, 11, 8, 9);
	mesh->SetFace (3, 14, 15, 12, 13);
	mesh->SetFace (4, 18, 19, 16, 17);
	mesh->SetFace (5, 22, 23, 20, 21);
	mesh->SetFace (6, 0, 8, 11, 1);
	mesh->SetFace (7, 15, 2, 1, 12);
	mesh->SetFace (8, 21, 0, 3, 22);
	mesh->SetFace (9, 19, 3, 2, 16);
	mesh->SetFace (10, 23, 5, 4, 20);
	mesh->SetFace (11, 5, 18, 17, 6);
	mesh->SetFace (12, 7, 10, 9, 4);
	mesh->SetFace (13, 6, 14, 13, 7);
	mesh->SetFace (14, 20, 9, 8, 21);
	mesh->SetFace (15, 10, 13, 12, 11);
	mesh->SetFace (16, 14, 17, 16, 15);
	mesh->SetFace (17, 18, 23, 22, 19);
	mesh->SetFace (18, 0, 21, 8);
	mesh->SetFace (19, 1, 11, 12);
	mesh->SetFace (20, 2, 15, 16);
	mesh->SetFace (21, 3, 19, 22);
	mesh->SetFace (22, 4, 9, 20);
	mesh->SetFace (23, 7, 13, 10);
	mesh->SetFace (24, 6, 17, 14);
	mesh->SetFace (25, 5, 23, 18);

	return mesh;
}

std::unique_ptr<Mesh> CreateArch ()
{
	const unsigned int n = 40;
	auto mesh = std::make_unique<Mesh> (16+2*(n-1), 11 + 3 + 3*(n-2) + 2 + 3);

	const float door_width = 3.f;
	const float door_height = 6.f;
	const float column_thickness = 2.f;
	const float column_height = 2.f;

	unsigned int vn = 0;
	unsigned int fn = 0;

	//
	// vertices
	//
	// bottom left
	mesh->SetVertex (vn++, -door_width-column_thickness, -column_thickness/2.f, 0.f);
	mesh->SetVertex (vn++, -door_width-column_thickness,  column_thickness/2.f, 0.f);
	mesh->SetVertex (vn++, -door_width,                   column_thickness/2.f, 0.f);
	mesh->SetVertex (vn++, -door_width,                  -column_thickness/2.f, 0.f);

	// bottom right
	mesh->SetVertex (vn++, door_width+column_thickness, -column_thickness/2.f, 0.f);
	mesh->SetVertex (vn++, door_width+column_thickness,  column_thickness/2.f, 0.f);
	mesh->SetVertex (vn++, door_width,                   column_thickness/2.f, 0.f);
	mesh->SetVertex (vn++, door_width,                  -column_thickness/2.f, 0.f);

	// top
	mesh->SetVertex (vn++,  -door_width-column_thickness, -column_thickness/2.f, door_height);
	mesh->SetVertex (vn++,  -door_width-column_thickness,  column_thickness/2.f, door_height);
	mesh->SetVertex (vn++,  door_width+column_thickness,  column_thickness/2.f, door_height);
	mesh->SetVertex (vn++,  door_width+column_thickness, -column_thickness/2.f, door_height);

	// middle
	mesh->SetVertex (vn++, -door_width, -column_thickness/2.f, column_height);
	mesh->SetVertex (vn++, -door_width,  column_thickness/2.f, column_height);
	mesh->SetVertex (vn++,  door_width,  column_thickness/2.f, column_height);
	mesh->SetVertex (vn++,  door_width, -column_thickness/2.f, column_height);

	//
	// faces
	//

	// bottom
	mesh->SetFace (fn++, 0, 1, 2, 3);
	mesh->SetFace (fn++, 4, 7, 6, 5);

	// up
	mesh->SetFace (fn++, 8, 11, 10, 9);

	// side left
	mesh->SetFace (fn++, 0, 8, 9, 1);

	// side right
	mesh->SetFace (fn++, 4, 5, 10, 11);

	// front left
	mesh->SetFace (fn++, 12, 8, 0, 3);

	// front right
	mesh->SetFace (fn++, 15, 7, 4, 11);

	// back left
	mesh->SetFace (fn++, 13, 2, 1, 9);

	// back right
	mesh->SetFace (fn++, 14, 10, 5, 6);

	// side interior left
	mesh->SetFace (fn++, 2, 13, 12, 3);

	// side interior right
	mesh->SetFace (fn++, 6, 7, 15, 14);

	//
	// arch
	//
	// link extremities
	for (unsigned int i=1; i<n; i++)
	{
		const double angle = 3.14159-i*3.14159/n;
		const float x = toFloat (door_width*std::cos (angle));
		const float z = toFloat (column_height + door_width*std::sin (angle));
		mesh->SetVertex (vn,   x, -column_thickness/2.f, z);
		mesh->SetVertex (vn+1, x,  column_thickness/2.f, z);
		if (i==1)
		{
			// under
			mesh->SetFace (fn++, 12, 13, vn+1, vn);

			// front
			mesh->SetFace (fn++, 8, 12, vn);

			// back
			mesh->SetFace (fn++, 13, 9, vn+1);
		}
		else
		{
			// under
			mesh->SetFace (fn++, vn-2, vn-1, vn+1, vn);

			// front
			mesh->SetFace (fn++, vn-2, vn, (i-1<n/2)?8:11);
			if ((i-1<n/2) != (i-2<n/2))
				mesh->SetFace (fn++, 11, 8, vn-2);

			// back
			mesh->SetFace (fn++, vn+1, vn-1, (i-1<n/2)?9:10);
			if ((i-1<n/2) != (i-2<n/2))
				mesh->SetFace (fn++, 9, 10, vn-1);
		}
		if (i==n-1)
		{
			mesh->SetFace (fn++, vn, vn+1, 14, 15);

			// front
			mesh->SetFace (fn++, 10, 14, vn+1);

			// back
			mesh->SetFace (fn++, 15, 11, vn);
		}
		vn+=2;
	}

	return mesh;
}

std::unique_ptr<Mesh> CreateArch2 ()
{
	const unsigned int nVertices = 360;
	auto mesh = std::make_unique<Mesh> (nVertices, 0);

	const float fRadius = 4.f;
	for (unsigned int i=0; i<nVertices; i++)
	{
		const float angle = static_cast<float> (i);
		mesh->SetVertex (i, fRadius*std::cos (angle), fRadius*std::sin (angle), 0.f);
	}

	return mesh;
}

ArcSamples sampleArcBrise (float a, float b, unsigned npts, float offset)
{
	static const char *fn = "sampleArcBrise";
	requireFinite (a, fn, "a");
	requireFinite (b, fn, "b");
	requireFinite (offset, fn, "offset");
	requireTwoSamplesOrNone (npts, fn);

	a += offset;
	b += offset;
	if (!(a > 0.f))
		throw std::invalid_argument ("sampleArcBrise: a + offset must be positive");
	if (!(b > a))
		throw std::invalid_argument ("sampleArcBrise: b + offset must exceed a + offset");

	ArcSamples s = makeSamples (npts);
	if (npts == 0)
		return s;

	// Centre du cercle de la moitie droite, sur l'axe des pieds : il passe par
	// le pied (a, 0) et par le sommet (0, b).
	const Vector2f C (toFloat ((a*a - b*b)/(2.*a)), 0.f);
	const float r = a-C[0];
	const float alpha_max = std::atan (-b/C[0]);

	const unsigned hnpts = npts/2;
	const bool odd = (npts % 2) != 0;
	for (unsigned i=0; i<hnpts; i++)
	{
		// npts pair : la moitie atteint le sommet ; impair : le sommet est
		// l'echantillon du milieu, la moitie s'arrete avant.
		const unsigned last = odd ? hnpts : hnpts - 1;
		const float alpha = (last == 0) ? 0.f : alpha_max * static_cast<float> (i) / static_cast<float> (last);
		const float x = C[0]+r*std::cos (alpha);
		const float y = C[1]+r*std::sin (alpha);
		const float sa = std::sin (alpha);
		const float ca = std::cos (alpha);

		s.points[i] = Vector2f (x, y);
		s.tangents[i] = Vector2f (-sa, ca);

		// symetrie x -> -x, parcourue en sens inverse
		s.points[npts-1-i] = Vector2f (-x, y);
		s.tangents[npts-1-i] = Vector2f (-sa, -ca);
	}
	if (odd)
	{
		s.points[hnpts] = Vector2f (0.f, b);
		s.tangents[hnpts] = Vector2f (-1.f, 0.f);
	}

	return s;
}

// http://ens.math.univ-montp2.fr/SPIP/irem/archi/mathtxt/arcaccolade.php
ArcSamples sampleArcAccolade (float /*a*/, float /*b*/, float /*e*/, unsigned npts)
{
	requireTwoSamplesOrNone (npts, "sampleArcAccolade");

	ArcSamples s = makeSamples (npts);
	if (npts == 0)
		return s;

	const float a = 5.f; // Ox
	const float b = 3.f; // Oy
	const float e = 0.3f; // 0. (0) -> 1. (A)

	const Vector2f E (a*e, b*(1.f-e));
	const Vector2f O1 (a*e, toFloat (0.5*(b*b-a*a)*(1-e)/b));
	const Vector2f O2 (a*e, toFloat (0.5*(a*a-b*b)*e/b+b));

	const float r1 = (E - O1).getLength ();
	const float r2 = (E - O2).getLength ();

	const unsigned n1 = npts/2;

	const float alpha1 = -std::atan (O1[1]/a);
	for (unsigned i=0; i<n1; i++)
	{
		const float alpha = toFloat (sampleAngle (alpha1, .5*3.14159, i, n1));
		s.points[i] = Vector2f (O1[0]+r1*std::cos (alpha), O1[1]+r1*std::sin (alpha));
		s.tangents[i] = Vector2f (-std::sin (alpha), std::cos (alpha));
	}

	// Second arc : n1 echantillons de alpha2 a 3pi/2, comme pour npts pair ;
	// npts impair ajoute le dernier echantillon en 3pi/2 (D12).
	const float alpha2 = toFloat (3.14159 + std::atan ((O2[1]-b)/O2[0]));
	const double alpha2End = .5*3.*3.14159;
	const auto setSecondArc = [&] (unsigned k, float alpha) {
		s.points[k] = Vector2f (O2[0]+r2*std::cos (alpha), O2[1]+r2*std::sin (alpha));
		s.tangents[k] = Vector2f (-std::sin (alpha), std::cos (alpha));
	};
	for (unsigned i=0; i<n1; i++)
		setSecondArc (n1+i, toFloat (sampleAngle (alpha2, alpha2End, i, n1)));
	if ((npts % 2) != 0)
		setSecondArc (npts-1, toFloat (alpha2End));

	return s;
}

// http://ens.math.univ-montp2.fr/SPIP/irem/archi/mathtxt/arcanse.php
ArcSamples sampleArcAnseDePanier (float a, float e, unsigned npts)
{
	static const char *fn = "sampleArcAnseDePanier";
	requireFinite (a, fn, "a");
	requireFinite (e, fn, "e");
	requireTwoSamplesOrNone (npts, fn);
	if (!(a > 0.f))
		throw std::invalid_argument ("sampleArcAnseDePanier: a must be positive");
	if (!(e >= 0.f && e < 1.f))
		throw std::invalid_argument ("sampleArcAnseDePanier: e must be in [0, 1)");

	ArcSamples s = makeSamples (npts);
	if (npts == 0)
		return s;

	const double tan60 = std::tan (3.14159/3.);
	const Vector2f O1 ( a*e, 0.f);
	const Vector2f O2 (-a*e, 0.f);
	const Vector2f O0 (0.f, toFloat (-a*e*tan60));
	const float r = toFloat (a*(1.-e));
	const float rc = toFloat (r+a*e*std::sqrt (1+tan60*tan60));

	// Les trois arcs se raccordent tangentiellement a pi/3 et 2pi/3 : la portion
	// est choisie par l'angle de l'echantillon.
	const float alphaRight = toFloat (3.14159/3.);
	const float alphaLeft = toFloat (2.*3.14159/3.);
	for (unsigned i=0; i<npts; i++)
	{
		const float alpha = toFloat (3.14159 * i / (npts - 1));
		const float ca = std::cos (alpha);
		const float sa = std::sin (alpha);
		if (alpha < alphaRight) // portion du pied droit
			s.points[i] = Vector2f (O1[0]+r*ca, O1[1]+r*sa);
		else if (alpha > alphaLeft) // portion du pied gauche
			s.points[i] = Vector2f (O2[0]+r*ca, O2[1]+r*sa);
		else // portion centrale
			s.points[i] = Vector2f (O0[0]+rc*ca, O0[1]+rc*sa);

		s.tangents[i] = Vector2f (-sa, ca);
	}

	return s;
}

// http://ens.math.univ-montp2.fr/SPIP/irem/archi/mathtxt/arcram.php
ArcSamples sampleArcRampant (float /*a*/, float /*t*/, unsigned npts)
{
	requireTwoSamplesOrNone (npts, "sampleArcRampant");

	ArcSamples s = makeSamples (npts);
	if (npts == 0)
		return s;

	const float a = 1.f;
	const float t = 0.6f;

	const Vector2f O1 (0.f, 0.f);
	const Vector2f O2 (0.f, t*a);

	const unsigned nh = npts/2;

	// premiere portion : pi >= alpha > pi/2, parcourue a angle decroissant
	for (unsigned vi=0; vi<nh; vi++)
	{
		const float alpha = toFloat (3.14159 * (1. - static_cast<float> (vi)/(2*nh)));
		s.points[vi] = Vector2f (O1[0]+a*std::cos (alpha), O1[1]+a*std::sin (alpha));
		s.tangents[vi] = Vector2f (std::sin (alpha), -std::cos (alpha));
	}

	// seconde portion : pi/2 >= alpha > 0, parcourue a angle decroissant
	for (unsigned vi=0; vi<npts-nh; vi++)
	{
		const float alpha = toFloat (0.5 * 3.14159 * (1. - static_cast<float> (vi)/npts));
		s.points[nh+vi] = Vector2f (toFloat (O2[0]+a*(1.-t)*std::cos (alpha)),
		                            toFloat (O2[1]+a*(1.-t)*std::sin (alpha)));
		s.tangents[nh+vi] = Vector2f (std::sin (alpha), -std::cos (alpha));
	}

	return s;
}

void extrudeMoulureAlongCurve (const ArcSamples &curve, const Profile2D &moulure, Mesh &out)
{
	static const char *fn = "extrudeMoulureAlongCurve";
	if (moulure.points.size () < 3)
		throw std::invalid_argument ("extrudeMoulureAlongCurve: moulure must have at least 3 points");
	if (curve.points.size () < 2)
		throw std::invalid_argument ("extrudeMoulureAlongCurve: curve must have at least 2 samples");
	if (curve.tangents.size () != curve.points.size ())
		throw std::invalid_argument ("extrudeMoulureAlongCurve: curve points and tangents differ in size");
	for (const Vector2d &q : moulure.points)
		if (!std::isfinite (q[0]) || !std::isfinite (q[1]))
			throw std::invalid_argument ("extrudeMoulureAlongCurve: moulure point is not finite");
	for (std::size_t i = 0; i < curve.points.size (); ++i)
	{
		requireFinite (curve.points[i][0], fn, "curve point");
		requireFinite (curve.points[i][1], fn, "curve point");
		requireFinite (curve.tangents[i][0], fn, "curve tangent");
		requireFinite (curve.tangents[i][1], fn, "curve tangent");
		if (curve.tangents[i][0] == 0.f && curve.tangents[i][1] == 0.f)
			throw std::invalid_argument ("extrudeMoulureAlongCurve: curve tangent is null");
	}

	const std::size_t n = curve.points.size ();
	const std::size_t m = moulure.points.size ();

	std::vector<float> vertices;
	vertices.reserve (3 * n * m);
	for (std::size_t i = 0; i < n; ++i)
	{
		const double cx = curve.points[i][0];
		const double cz = curve.points[i][1];
		double tx = curve.tangents[i][0];
		double tz = curve.tangents[i][1];
		const double len = std::sqrt (tx*tx + tz*tz);
		tx /= len;
		tz /= len;

		// B = (0, -1, 0), T = (tx, 0, tz), N = B x T = (-tz, 0, tx)
		const double nx = -tz;
		const double nz = tx;
		for (std::size_t j = 0; j < m; ++j)
		{
			const double u = moulure.points[j][0];
			const double v = moulure.points[j][1];
			vertices.push_back (toFloat (cx + v*nx));
			vertices.push_back (toFloat (-u));
			vertices.push_back (toFloat (cz + v*nz));
		}
	}

	std::vector<unsigned int> faces;
	faces.reserve (6 * m * (n - 1));
	for (std::size_t i = 0; i + 1 < n; ++i)
		for (std::size_t j = 0; j < m; ++j)
		{
			const std::size_t j1 = (j + 1) % m;
			const auto v00 = static_cast<unsigned int> (i       * m + j);
			const auto v10 = static_cast<unsigned int> ((i + 1) * m + j);
			const auto v11 = static_cast<unsigned int> ((i + 1) * m + j1);
			const auto v01 = static_cast<unsigned int> (i       * m + j1);
			faces.insert (faces.end (), {v00, v10, v11, v00, v11, v01});
		}

	Mesh result;
	result.SetVertices (static_cast<unsigned int> (n * m), vertices.data ());
	result.SetFaces (static_cast<unsigned int> (faces.size () / 3), 3, faces.data ());

	// L'affectation recopie aussi la revision de `result` ; celle de `out` doit
	// rester croissante, car elle sert de cle aux caches indexes sur le maillage.
	const uint64_t revisionBefore = out.GetRevision ();
	out = result;
	while (out.GetRevision () <= revisionBefore)
		out.IncrementRevision ();
}

ArcBrise::ArcBrise ()
{
	m_fAltitude = -1.;
	m_fWidth = -1.;
	m_fHeight = -1.;
	m_fAltitude2 = -1.;
	m_fWidth2 = -1.;
	m_fHeight2 = -1.;
}

ArcBrise::~ArcBrise ()
{
}

void ArcBrise::SetPrincipalArc (float altitude, float width, float height)
{
	m_fAltitude = altitude;
	m_fWidth = width;
	m_fHeight = height;
}

void ArcBrise::SetSecondArc (float altitude2, float width2, float height2)
{
	m_fAltitude2 = altitude2;
	m_fWidth2 = width2;
	m_fHeight2 = height2;
}


Rosace::Rosace ()
{
	m_nFoils = 3;
	m_fRadius = 1.f;
}

Rosace::~Rosace ()
{
}

Polygon2* Rosace::Generate (void)
{
	const unsigned int resolution = 20;
	Polygon2 *pol = new Polygon2 ();
	float *p = pol->add_contour (0, m_nFoils*resolution, nullptr);

	const float alpha = toFloat (2.*M_PI/m_nFoils);
	const float halpha = toFloat (M_PI/m_nFoils);
	const float radius = toFloat (std::sin (halpha) / (1. + std::sin (halpha)));
	const Vector2f center (toFloat ((1.-radius)*std::cos (halpha)), radius);

	for (unsigned int i=0; i<resolution; i++)
	{
		const float a = toFloat (-.5*M_PI + i*(M_PI+alpha)/resolution);
		p[2*i]   = center[0] + radius*std::cos (a);
		p[2*i+1] = center[1] + radius*std::sin (a);
	}

	for (unsigned int i=1; i<m_nFoils; i++)
	{
		const float theta = static_cast<float> (i) * alpha;
		Matrix2f rot (std::cos (theta), -std::sin (theta),
			      std::sin (theta),  std::cos (theta));
		for (unsigned int j=0; j<resolution; j++)
		{
			Vector2f v = rot * Vector2f (p[2*j], p[2*j+1]);
			p[2*resolution*i + 2*j]   = v[0];
			p[2*resolution*i + 2*j+1] = v[1];
		}
	}

	return pol;
}

// Altitudes : decalages verticaux des arcs. Le cercle central n'est juste que
// pour des altitudes nulles.
Polygon2* ArcBrise::Generate (void)
{
	const unsigned int npts = 32;

	if (m_fWidth2 > m_fWidth/2.f)
		return nullptr;

	const ArcSamples principal = sampleArcBrise (m_fWidth/2.f, m_fHeight, npts, 0.f);
	const ArcSamples second = sampleArcBrise (m_fWidth2/2.f, m_fHeight2, npts, 0.f);

	auto pol = std::make_unique<Polygon2> ();

	// principal arc
	pol->add_contour (0, 3*npts, nullptr);
	for (unsigned int i=0; i<npts; i++)
		pol->set_point (0, i, principal.points[i][0], m_fAltitude + principal.points[i][1]);

	// second arc, twice, in reverse order
	for (unsigned int i=0; i<npts; i++)
	{
		const Vector2f &q = second.points[npts-1-i];
		pol->set_point (0, npts+i, q[0]-m_fWidth/4.f, m_fAltitude2 + q[1]);
		pol->set_point (0, 2*npts+i, q[0]+m_fWidth/4.f, m_fAltitude2 + q[1]);
	}

	//
	// rosace
	//
	// evaluate the locus of points equidistant from two circles => ellipse
	//
	// references :
	// http://www.mathokay.com/equidistant_locus.htm
	// http://www.geogebra.org/forum/viewtopic.php?f=2&t=23035
	//

	// principal circle
	float a = m_fWidth/2.f;
	float b = m_fHeight;
	const Vector2f c_principal (toFloat ((a*a - b*b)/(2.*a)), m_fAltitude);
	const float r_principal = a-c_principal[0];

	// second circle
	Vector2f c_second;
	a = m_fWidth2/2.f;
	b = m_fHeight2;
	c_second.Set (toFloat ((a*a - b*b)/(2.*a)), m_fAltitude2);
	const float r_second = a-c_second[0];
	c_second[0] = toFloat (c_second[0] - m_fWidth/4.);

	// middle
	Vector2f c_mid = c_principal + c_second;
	c_mid = c_mid * 0.5f;

	// ellipse equation : x^2/a^2 + y^2/b^2 = 1 (centered on c_mid)
	a = toFloat ((r_principal + r_second) / 2.);

	Vector2f tmp = c_principal - c_second;
	const float l = toFloat (tmp.getLength ()/2.);
	b = std::sqrt (a*a - l*l);

	// and the translation is :
	const float x_trans = c_mid[0];

	// solve the equation with x = x_trans
	const float rosace_y = toFloat (b * std::sqrt (1. - x_trans*x_trans/(a*a)));

	tmp.Set (c_second[0], c_second[1]-rosace_y);
	const float rosace_radius = tmp.getLength () - r_second;

	const std::vector<Vector2f> circle =
		sampleCircle (Vector2f (0.f, rosace_y), rosace_radius, 0.f, toFloat (2.*3.14159), npts);
	pol->add_contour (1, npts, nullptr);
	for (unsigned int i=0; i<npts; i++)
		pol->set_point (1, i, circle[npts-i-1][0], circle[npts-i-1][1]);

	return pol.release ();
}

} // namespace cgarchi
