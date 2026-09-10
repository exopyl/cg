#include "tangents.h"

// Ce que ce fichier utilise, et non le parapluie.
#include "mesh.h"

#include <cmath>
#include <cstddef>

namespace {

// Seuil du determinant de la matrice des derivees UV. En dessous, les deux
// aretes du triangle ont des UV colineaires : la parametrisation y est
// degeneree et l'inverse exploserait. Le triangle est alors ignore -- ses
// sommets recevront la contribution de leurs autres triangles, ou le repli.
constexpr float kUvDetEpsilon = 1e-12f;

// Seuil de longueur au carre en deca duquel un vecteur accumule est tenu pour
// nul. Il faut le meme des deux cotes de l'orthogonalisation : une tangente
// quasi parallele a la normale donne un residu quasi nul, aussi inutilisable
// qu'une accumulation vide.
constexpr float kLengthEpsilon2 = 1e-20f;

struct Vec3 { float x, y, z; };

inline Vec3 operator+ (const Vec3& a, const Vec3& b) { return { a.x+b.x, a.y+b.y, a.z+b.z }; }
inline Vec3 operator- (const Vec3& a, const Vec3& b) { return { a.x-b.x, a.y-b.y, a.z-b.z }; }
inline Vec3 operator* (const Vec3& a, float s)       { return { a.x*s, a.y*s, a.z*s }; }
inline float dot (const Vec3& a, const Vec3& b)      { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 cross (const Vec3& a, const Vec3& b)
	{ return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x }; }

// Tangente de repli, ORTHOGONALE A LA NORMALE, et non l'axe X constant.
//
// vecna (GltfPbrLoader.cpp) ecrit (1,0,0,1) quand le fichier n'a pas de
// TANGENT. Ce repli est degenere des que la normale est elle-meme proche de X :
// la base TBN s'aplatit et le nuanceur echantillonne la carte de normales dans
// un repere sans rang. On construit donc la tangente a partir de l'axe le MOINS
// dominant de la normale, ce qui garantit un produit vectoriel de norme
// substantielle quelle que soit l'orientation.
Vec3 fallbackTangent (const Vec3& n)
{
	const float ax = std::fabs (n.x), ay = std::fabs (n.y), az = std::fabs (n.z);
	const Vec3 axis = (ax <= ay && ax <= az) ? Vec3{1.f, 0.f, 0.f}
	                : (ay <= az)             ? Vec3{0.f, 1.f, 0.f}
	                                         : Vec3{0.f, 0.f, 1.f};
	const Vec3 t = cross (n, axis);
	const float len2 = dot (t, t);
	if (len2 < kLengthEpsilon2)
		return { 1.f, 0.f, 0.f };   // normale nulle : plus rien a orthogonaliser
	return t * (1.f / std::sqrt (len2));
}

} // namespace

bool generateTangents (Mesh& mesh, unsigned int uvSet)
{
	const unsigned int nv = mesh.GetNVertices ();
	// Le jeu 1 est PARALLELE AUX SOMMETS par contrat (cf. mesh.h,
	// GetTextureCoordinates1) : il n'a pas d'indirection par coin, et un
	// uvSet hors {0, 1} n'existe pas dans TextureRef.
	const bool useSet1 = (uvSet == 1u);
	const std::vector<float>& uv = useSet1 ? mesh.GetTextureCoordinates1 ()
	                                       : mesh.GetTextureCoordinates ();

	// CONTRAT : sans UV DANS LE JEU DEMANDE, aucune ecriture. On teste le
	// TABLEAU et non GetNTextureCoordinates(), pour la meme raison que
	// BuildPolygonRenderData -- le compte et le tableau peuvent diverger
	// (import_3ds), et c'est le tableau qui porte les donnees.
	if (nv == 0 || uv.empty ())
		return false;

	const std::vector<float>& pos = mesh.GetVertices ();
	const std::vector<float>& nrm = mesh.GetVertexNormals ();
	const bool hasNormals = (nrm.size () == 3u * (std::size_t)nv);

	std::vector<Vec3> tanU ((std::size_t)nv, Vec3{0.f, 0.f, 0.f});
	std::vector<Vec3> tanV ((std::size_t)nv, Vec3{0.f, 0.f, 0.f});

	// Resolution d'un coin vers sa position et son UV. Meme regle que
	// BuildPolygonRenderData : indice de coin quand la face en porte un, indice
	// de sommet sinon, et (0,0) hors bornes. L'indirection par coin ne vaut que
	// pour le jeu 0 -- le jeu 1 est adresse par indice de sommet, toujours.
	auto cornerUV = [&](Mesh::ConstFaceRef face, unsigned int c, unsigned int vi,
	                    float& u, float& v)
	{
		unsigned int uvIdx = vi;
		if (!useSet1 && face->UsesTextureCoordinates () && face->HasTexCoordIndices ())
		{
			const int ti = face->GetTexCoordIndex (c);
			if (ti >= 0) uvIdx = (unsigned int)ti;
		}
		if (2u * (std::size_t)uvIdx + 1u < uv.size ())
		{
			u = uv[2 * (std::size_t)uvIdx];
			v = uv[2 * (std::size_t)uvIdx + 1];
		}
		else
		{
			u = 0.f; v = 0.f;
		}
	};

	const unsigned int nf = mesh.GetNFaces ();
	for (unsigned int fi = 0; fi < nf; ++fi)
	{
		Mesh::ConstFaceRef face = mesh.FaceAt (fi);
		if (!face) continue;
		const unsigned int n = (unsigned int)face->GetNVertices ();
		if (n < 3) continue;

		// Eventail depuis le coin 0. Il ne sert qu'a l'accumulation : la face
		// n'est pas modifiee, et une eventuelle concavite ne fausse pas le
		// resultat -- seules les derivees de la parametrisation comptent, pas
		// l'aire signee du sous-triangle.
		for (unsigned int k = 1; k + 1 < n; ++k)
		{
			const unsigned int c[3] = { 0u, k, k + 1u };
			unsigned int vi[3];
			bool ok = true;
			for (int j = 0; j < 3; ++j)
			{
				const int v = face->GetVertex (c[j]);
				if (v < 0 || (unsigned int)v >= nv) { ok = false; break; }
				vi[j] = (unsigned int)v;
			}
			if (!ok) continue;

			Vec3 p[3];
			float su[3], sv[3];
			for (int j = 0; j < 3; ++j)
			{
				p[j] = { pos[3 * (std::size_t)vi[j]],
				         pos[3 * (std::size_t)vi[j] + 1],
				         pos[3 * (std::size_t)vi[j] + 2] };
				cornerUV (face, c[j], vi[j], su[j], sv[j]);
			}

			const Vec3 e1 = p[1] - p[0];
			const Vec3 e2 = p[2] - p[0];
			const float du1 = su[1] - su[0], dv1 = sv[1] - sv[0];
			const float du2 = su[2] - su[0], dv2 = sv[2] - sv[0];

			const float det = du1 * dv2 - du2 * dv1;
			if (std::fabs (det) < kUvDetEpsilon)
				continue;
			const float r = 1.f / det;

			const Vec3 t = (e1 * dv2 - e2 * dv1) * r;
			const Vec3 b = (e2 * du1 - e1 * du2) * r;

			for (int j = 0; j < 3; ++j)
			{
				tanU[vi[j]] = tanU[vi[j]] + t;
				tanV[vi[j]] = tanV[vi[j]] + b;
			}
		}
	}

	std::vector<float> out (4u * (std::size_t)nv, 0.f);
	for (unsigned int i = 0; i < nv; ++i)
	{
		const Vec3 n = hasNormals
			? Vec3{ nrm[3 * (std::size_t)i], nrm[3 * (std::size_t)i + 1], nrm[3 * (std::size_t)i + 2] }
			: Vec3{ 0.f, 0.f, 0.f };

		Vec3 t = tanU[i];
		float w = 1.f;

		if (hasNormals)
		{
			// Gram-Schmidt : on retire de la tangente accumulee sa composante
			// le long de la normale. Sans cela la base n'est pas orthogonale et
			// la carte de normales se lit de travers sur les surfaces courbes.
			t = t - n * dot (n, t);
			if (dot (t, t) < kLengthEpsilon2)
				t = fallbackTangent (n);
			else
				t = t * (1.f / std::sqrt (dot (t, t)));

			// Signe de main : +1 si (N, T, B) est direct, -1 si la
			// parametrisation est miroir. C'est ce que le nuanceur multiplie a
			// cross (N, T) pour retrouver la bitangente reelle.
			w = (dot (cross (n, t), tanV[i]) < 0.f) ? -1.f : 1.f;
		}
		else
		{
			const float len2 = dot (t, t);
			if (len2 < kLengthEpsilon2)
				t = { 1.f, 0.f, 0.f };
			else
				t = t * (1.f / std::sqrt (len2));
		}

		out[4 * (std::size_t)i]     = t.x;
		out[4 * (std::size_t)i + 1] = t.y;
		out[4 * (std::size_t)i + 2] = t.z;
		out[4 * (std::size_t)i + 3] = w;
	}

	mesh.SetVertexTangents (std::move (out));
	return true;
}

void clearTangents (Mesh& mesh)
{
	mesh.SetVertexTangents (std::vector<float> ());
}
