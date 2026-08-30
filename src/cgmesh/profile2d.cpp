#include "profile2d.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

#include "mesh.h"
#include "polygon2.h"

namespace
{

const double PI = 3.14159265358979323846;

double signedAreaShoelace (const std::vector<Vector2d> &polygon)
{
	double sum = 0.0;
	size_t n = polygon.size();
	for (size_t i = 0; i < n; ++i)
	{
		size_t j = (i + 1) % n;
		sum += polygon[i].x * polygon[j].y - polygon[j].x * polygon[i].y;
	}
	return 0.5 * sum;
}

// Normale « vers la pierre » par sommet, pour un contour range matiere a GAUCHE
// (exterieur CCW, trous CW). = normalize(moyenne des rot90(arete)) avec
// rot90(dx,dy) = (-dy,dx), qui pointe a gauche de la marche.
std::vector<Vector2d> intoStoneNormals (const std::vector<Vector2d> &pts)
{
	const size_t n = pts.size();
	std::vector<Vector2d> en(n);   // normale par arete (arete i -> i+1)
	for (size_t i = 0; i < n; ++i)
	{
		const Vector2d &a = pts[i], &b = pts[(i + 1) % n];
		double dx = b.x - a.x, dy = b.y - a.y;
		double L = std::sqrt(dx*dx + dy*dy);
		en[i] = (L < 1e-12) ? Vector2d(0,0) : Vector2d(-dy / L, dx / L);
	}
	std::vector<Vector2d> vn(n);
	for (size_t i = 0; i < n; ++i)
	{
		Vector2d s = en[(i + n - 1) % n] + en[i];
		double L = std::sqrt(s.x*s.x + s.y*s.y);
		vn[i] = (L < 1e-9) ? en[i] : Vector2d(s.x / L, s.y / L);
	}
	return vn;
}

} // namespace

// ---------------------------------------------------------------------------
//  Fabriques
// ---------------------------------------------------------------------------

Profile2D chamferSplayProfile (double width, double depth)
{
	Profile2D p;
	p.points.push_back (Vector2d (0.0, 0.0));
	p.points.push_back (Vector2d (depth, width));
	return p;
}

Profile2D cavettoSplayProfile (double width, double depth, int segments)
{
	if (segments < 1) segments = 1;
	Profile2D p;
	// Quart de cercle CONCAVE, au sens ou l'entend deja ce depot (cf. le
	// cavettoProfile de la bibliotheque de sections) : la courbe passe du cote
	// de la PIERRE par rapport a la corde, donc elle creuse davantage qu'un
	// chanfrein de memes largeur et profondeur. Elle part de la face avant
	// tangente au plan de celle-ci et arrive au fond tangente a la paroi
	// droite. u et v croissent tous deux, donc l'anneau decale du dernier point
	// est bien le plus rentre -- c'est ce que suppose le garde-fou
	// d'auto-intersection.
	for (int k = 0; k <= segments; ++k)
	{
		const double t = PI * 0.5 * (double)k / (double)segments;
		p.points.push_back (Vector2d (depth * (1.0 - std::cos (t)), width * std::sin (t)));
	}
	// Le premier point vaut (0,0) par construction ; on l'y pose exactement --
	// le contrat de extrudeProfiledToMesh ne doit pas dependre de l'exactitude
	// de sin(0) et cos(0).
	p.points[0] = Vector2d (0.0, 0.0);
	return p;
}

Profile2D rollBarProfile (double radius, int segments)
{
	if (segments < 2) segments = 2;
	Profile2D p;
	for (int k = 0; k <= segments; ++k)
	{
		const double th = PI * (double)k / (double)segments;
		p.points.push_back (Vector2d (radius * std::sin (th), radius * std::cos (th)));
	}
	return p;
}

Profile2D keelBarProfile (double radius)
{
	Profile2D p;
	p.points.push_back (Vector2d (0.0, radius));
	p.points.push_back (Vector2d (1.2 * radius, 0.0));
	p.points.push_back (Vector2d (0.0, -radius));
	return p;
}

Profile2D ogeeBarProfile (double radius, int segments)
{
	if (segments < 1) segments = 1;
	Profile2D p;
	for (int k = 0; k <= segments; ++k)
	{
		const double s = (double)k / (double)segments;
		const double h = radius * (3*s*s - 2*s*s*s);
		p.points.push_back (Vector2d (h,  radius * (1.0 - s)));
	}
	for (int k = segments - 1; k >= 0; --k)
	{
		const double s = (double)k / (double)segments;
		const double h = radius * (3*s*s - 2*s*s*s);
		p.points.push_back (Vector2d (h, -radius * (1.0 - s)));
	}
	return p;
}

Profile2D translatedInU (const Profile2D &profile, double du)
{
	Profile2D p = profile;
	for (Vector2d &q : p.points)
		q.x += du;
	return p;
}

// ---------------------------------------------------------------------------
//  Extrusion profilee
// ---------------------------------------------------------------------------

void extrudeProfiledToMesh (const Polygon2 &polygon, const Profile2D &profile, Mesh &out,
                            double zBottom, double zTop)
{
	const int nc = polygon.get_n_contours();
	if (nc <= 0) throw std::runtime_error("extrudeProfiledToMesh: empty polygon");

	const std::vector<Vector2d> &pp = profile.points;
	if (pp.size() < 2)
		throw std::runtime_error("extrudeProfiledToMesh: profile needs at least two points");
	if (pp[0].x != 0.0 || pp[0].y != 0.0)
		throw std::runtime_error("extrudeProfiledToMesh: profile must start at (0, 0)");
	for (const Vector2d &q : pp)
		if (q.x < 0.0 || q.y < 0.0)
			throw std::runtime_error("extrudeProfiledToMesh: profile has a negative coordinate");

	// Etendues du profil : la largeur commande le garde-fou d'auto-intersection,
	// la profondeur est bornee par l'epaisseur de la piece.
	const double profW = pp.back().y;
	double profD = pp.back().x;
	double depthScale = 1.0;
	if (profD > (zTop - zBottom))
	{
		// Le profil ne peut pas etre plus profond que la piece. On le comprime
		// en u plutot que de le tronquer : tronquer ferait disparaitre le
		// dernier point, donc l'anneau sur lequel s'appuie le capot arriere.
		depthScale = (zTop - zBottom) / profD;
		profD = zTop - zBottom;
	}

	// Anneaux de base (v = 0) et decale (v = profW, vers la pierre) par contour.
	// Les petites ouvertures -- foils de rosette, barres de fillet -- recoivent
	// un profil proportionnellement plus etroit, pour que la moulure ne mange
	// pas la barre de pierre ni ne fasse fusionner deux foils voisins ; les
	// grandes ouvertures gardent le profil entier. La largeur est bornee par le
	// rayon caracteristique du contour, sqrt(|aire|/pi).
	//
	// ⚠ Cette reduction porte sur la LARGEUR seule. La profondeur reste celle du
	// profil, la meme pour tous les contours : la version d'origine calculait
	// bien une profondeur par contour (`chamD_c`) mais ne la lisait nulle part
	// -- elle la rangeait dans un tableau `zChfC` qu'aucune ligne ne relisait.
	// Le comportement est conserve tel quel : ceci est une extraction, pas une
	// correction.
	std::vector<std::vector<Vector2d>> base(nc);
	std::vector<std::vector<Vector2d>> nrm(nc);
	std::vector<double> widthScale(nc);
	for (int ci = 0; ci < nc; ++ci)
	{
		const unsigned int n = (unsigned int)polygon.get_n_points(ci);
		const float *p = polygon.get_points(ci);
		base[ci].resize(n);
		for (unsigned int i = 0; i < n; ++i) base[ci][i] = Vector2d(p[2*i], p[2*i+1]);
		const double area  = signedAreaShoelace(base[ci]);
		const double charR = std::sqrt(std::fabs(area) / PI);
		double profW_c = std::min(profW, 0.32 * charR);
		if (profW_c < 0.3) profW_c = std::min(profW, 0.3);

		nrm[ci] = intoStoneNormals(base[ci]);

		// Sauter le profil (paroi verticale) quand il produirait des cuvettes ou
		// des « gouttes » : (a) contours PETITS -- foils, tetes folies -- ou la
		// moulure est proportionnellement trop grosse et chevauche la pierre
		// mince ; (b) contours a REDENTS (fleur de rosette, tetes folies) dont
		// l'offset interieur se recoupe aux pointes concaves. Seules les grandes
		// ouvertures convexes (lancettes, arc principal) gardent la moulure.
		//
		// (b) se mesure par la courbure concave TOTALE : la somme des angles de
		// virage par sommet qui vont a l'ENVERS de l'orientation d'ensemble du
		// contour (sommets rentrants). C'est invariant par echantillonnage --
		// couper une arete en deux n'ajoute aucun virage --, donc cela attrape
		// les redents POINTUS finement tesselles (fleur pointue) que l'ancien
		// test par arete « offset replie » manquait (l'offset de chaque arete
		// courte ne s'inversait jamais completement). Une ouverture convexe a
		// une courbure concave quasi nulle.
		bool skip = (charR < std::max(24.0, 4.0 * profW));
		if (!skip)
		{
			const double orient = (area >= 0.0) ? 1.0 : -1.0;
			double concaveTurn = 0.0;
			for (unsigned int i = 0; i < n; ++i)
			{
				const Vector2d &a = base[ci][(i + n - 1) % n];
				const Vector2d &b = base[ci][i];
				const Vector2d &c = base[ci][(i + 1) % n];
				const double e1x = b.x - a.x, e1y = b.y - a.y;
				const double e2x = c.x - b.x, e2y = c.y - b.y;
				const double cross = e1x*e2y - e1y*e2x;
				const double dot   = e1x*e2x + e1y*e2y;
				const double turn  = std::atan2(cross, dot);   // virage signe en b
				if (turn * orient < 0.0) concaveTurn += -turn * orient;
			}
			if (concaveTurn > 0.8) skip = true;                // ~45 deg de redents
		}
		// Paroi verticale = profil d'echelle NULLE en largeur : l'anneau decale
		// retombe sur l'anneau de base, exactement comme l'ancien `off = base`.
		widthScale[ci] = skip ? 0.0 : ((profW > 0.0) ? (profW_c / profW) : 0.0);
	}

	std::vector<float>        V;
	std::vector<unsigned int> F;
	auto addV = [&](double x, double y, double z) -> unsigned int
	{ unsigned int id = (unsigned int)(V.size() / 3); V.push_back((float)x); V.push_back((float)y); V.push_back((float)z); return id; };

	// --- capot avant : face de pierre a zTop (v = 0) ---
	{
		float *pV=nullptr; unsigned int nV=0,*pF=nullptr,nF=0;
		polygon.tesselate(&pV,&nV,&pF,&nF);
		if (!nV||!nF){ if(pV)free(pV); if(pF)free(pF); throw std::runtime_error("extrudeProfiledToMesh: front tess empty"); }
		unsigned int b=(unsigned int)(V.size()/3);
		for (unsigned int i=0;i<nV;++i) addV(pV[3*i],pV[3*i+1],zTop);
		for (unsigned int i=0;i<nF;++i){ F.push_back(b+pF[3*i]); F.push_back(b+pF[3*i+1]); F.push_back(b+pF[3*i+2]); }
		free(pV); free(pF);
	}

	// --- moulure laterale : le profil balaye le long de chaque contour ---
	// Un niveau de sommets par point du profil, puis un dernier a zBottom sur le
	// dernier anneau. Deux points redonnent exactement le chanfrein d'origine.
	std::vector<std::vector<Vector2d>> lastRing(nc);
	const double zLast = zTop - profD;
	for (int ci = 0; ci < nc; ++ci)
	{
		const size_t n = base[ci].size();
		if (n < 2) continue;
		const double s = widthScale[ci];

		auto strip = [&](unsigned int A, unsigned int B){   // A = niveau avant, B = niveau arriere
			for (size_t i=0;i<n;++i){
				unsigned int j=(unsigned int)((i+1)%n);
				unsigned int ai=A+(unsigned int)i, aj=A+j, bi=B+(unsigned int)i, bj=B+j;
				F.push_back(ai); F.push_back(bi); F.push_back(bj);
				F.push_back(ai); F.push_back(bj); F.push_back(aj);
			}
		};

		unsigned int previous = 0;
		for (size_t k = 0; k < pp.size(); ++k)
		{
			const double v = s * pp[k].y;
			const double z = zTop - depthScale * pp[k].x;
			unsigned int level = (unsigned int)(V.size()/3);
			for (size_t i=0;i<n;++i)
				addV(base[ci][i].x + v * nrm[ci][i].x, base[ci][i].y + v * nrm[ci][i].y, z);
			if (k > 0) strip(previous, level);
			previous = level;
		}

		// Partie droite jusqu'au fond, sur le dernier anneau du profil.
		lastRing[ci].resize(n);
		const double vEnd = s * pp.back().y;
		for (size_t i=0;i<n;++i)
			lastRing[ci][i] = Vector2d(base[ci][i].x + vEnd * nrm[ci][i].x,
			                           base[ci][i].y + vEnd * nrm[ci][i].y);
		if (zLast > zBottom + 1e-6)
		{
			unsigned int bottom = (unsigned int)(V.size()/3);
			for (size_t i=0;i<n;++i) addV(lastRing[ci][i].x, lastRing[ci][i].y, zBottom);
			strip(previous, bottom);
		}
	}

	// --- capot arriere : face de pierre decalee, a zBottom ---
	Polygon2 backPoly;
	backPoly.alloc_contours(nc);
	std::vector<std::vector<float>> packed(nc);
	for (int ci=0; ci<nc; ++ci){
		packed[ci].resize(lastRing[ci].size()*2);
		for (size_t i=0;i<lastRing[ci].size();++i){ packed[ci][2*i]=(float)lastRing[ci][i].x; packed[ci][2*i+1]=(float)lastRing[ci][i].y; }
		backPoly.add_contour(ci, (unsigned int)lastRing[ci].size(), packed[ci].data());
	}
	{
		float *pV=nullptr; unsigned int nV=0,*pF=nullptr,nF=0;
		backPoly.tesselate(&pV,&nV,&pF,&nF);
		if (nV&&nF){
			unsigned int b=(unsigned int)(V.size()/3);
			for (unsigned int i=0;i<nV;++i) addV(pV[3*i],pV[3*i+1],zBottom);
			for (unsigned int i=0;i<nF;++i){ F.push_back(b+pF[3*i]); F.push_back(b+pF[3*i+2]); F.push_back(b+pF[3*i+1]); } // inverse -> -z
		}
		if(pV)free(pV); if(pF)free(pF);
	}

	out.SetVertices((unsigned int)(V.size()/3), V.data());
	out.SetFaces   ((unsigned int)(F.size()/3), 3, F.data());
}

void extrudeProfiledToMesh (Polygon2 &polygon, Mesh &out,
                            double zBottom, double zTop, double chamW, double chamD)
{
	extrudeProfiledToMesh (polygon, chamferSplayProfile (chamW, chamD), out, zBottom, zTop);
}
