#pragma once

// ============================================================================
//  Chainage des segments d'une coupe plan/maillage
// ============================================================================
//
// Troisieme etape du slicing de mesh_slicing.h (conception et etiquettes
// D1-D8 : docs/mesh_slicing.md) :
//   - SliceSegment : un segment d'intersection entre une face et le plan ;
//   - SliceLinker  : chainage des segments par cle d'arete, avec des
//                    reparations sur une couche non variete ;
//   - traceChains : parcours des chaines en chemins.
//
// Le code ne depend que de la STL et de cgmath. Les limites connues sont
// signalees "(D...)" en commentaire, la ou elles naissent.
//
// Les types vivent dans cgmesh::slicing : TPoint, TSegment et TPath existent
// deja dans l'espace global (image_vectorization.h, avec un using namespace std
// dans l'en-tete), et un nom court comme "Segment" n'y resisterait pas.
//
// ============================================================================

#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

#include <cgmath/cgmath.h>

namespace cgmesh {
namespace slicing {

// Un segment d'intersection entre UNE face triangulaire et le plan de coupe.
//
// edges[k] designe l'arete (paire d'indices de sommets) qui porte l'extremite
// points[k]. Une extremite confondue avec un sommet porte la paire degeneree
// (v, v). Ces paires servent de CLE de chainage : deux segments se touchent
// quand ils partagent une arete, et non quand leurs points coincident -- c'est
// ce qui rend le chainage exact sur un maillage soude.
//
// Les pointeurs `linked` designent des elements du MEME std::vector : celui-ci
// ne doit plus etre redimensionne une fois le chainage commence.
struct SliceSegment
{
	int edges[2][2] = {{-1, -1}, {-1, -1}};
	int faceId = -1;                 // indice de face du Mesh tranche
	Vector3f points[2];              // extremites, en repere plateau
	Vector3f faceNormal;
	SliceSegment* linked[2] = {nullptr, nullptr};   // voisin a l'extremite k
	bool visited = false;            // consomme (chaine, ou ecarte par une reparation)

	// Faux pour un segment emis INVALIDE : present pour le chainage, jamais
	// point de depart d'un chemin (traceChains). Cas d'une arete dans le plan
	// a une ou plus de deux entrees (intersectTriangles).
	bool valid = true;

	float length2 () const
	{
		const float dx = points[1].x - points[0].x;
		const float dy = points[1].y - points[0].y;
		const float dz = points[1].z - points[0].z;
		return dz * dz + dy * dy + dx * dx;
	}

	// Retourne le segment : extremites, aretes ET voisins.
	void inverse ()
	{
		const Vector3f p = points[0];
		points[0] = points[1];
		points[1] = p;

		const int e0 = edges[0][0], e1 = edges[0][1];
		edges[0][0] = edges[1][0];
		edges[0][1] = edges[1][1];
		edges[1][0] = e0;
		edges[1][1] = e1;

		SliceSegment* l = linked[0];
		linked[0] = linked[1];
		linked[1] = l;
	}
};

// Chaine les segments entre eux (champ `linked`), en reparant au passage une
// partie des defauts de topologie d'une couche.
//
// Chemin nominal : la couche est "manifold sur indices" (chaque extremite
// partagee par exactement un autre segment, aucune arete en double) et le
// chainage se fait par cle d'arete. Sinon, une batterie de reparations
// s'applique avant ce meme chainage -- aretes dupliquees, "poils", sommets
// non-varietes issus d'une arete nulle, sommets non-varietes --, suivie d'un
// comblement des trous par proximite XY (0,005).
//
// (D1) le test de variete porte sur TOUTE la couche : un seul defaut local --
// par exemple un sommet classe "sur" le plan par la tolerance absolue
// +-FLT_EPSILON -- envoie la couche entiere dans le chemin de reparation.
//
// (D2) Le retrait des segments de longueur nulle (removeZeroLengthIntersections,
// seuil absolu length2 < FLT_EPSILON) n'a PAS lieu sur le chemin nominal : il
// effondrerait les contours faits de segments courts. Il est fait en tete du
// chemin de reparation, dont les heuristiques supposent une couche sans
// segment nul (voir le .cpp).
class SliceLinker
{
public:
	explicit SliceLinker (std::vector<SliceSegment>& segments);

	// useTopology = false : chainage par comparaison de points seulement
	// (tolerance 1e-6), sans reparation. Rend toujours true.
	//
	// LIMITE CONNUE (D2, residuel) : une couche NON variete -- il suffit d'un
	// defaut local, n'importe ou dans le plan -- passe par le retrait
	// des segments de longueur nulle (seuil absolu length2 < FLT_EPSILON) avant
	// les reparations. Un contour tres facette (segments < 3,45e-4) de cette
	// couche peut donc toujours s'y effondrer, et le retrait y reste en
	// O(k^2). Seul le chemin nominal en est exempt.
	bool execute (bool useTopology = true);

	// Diagnostic : vrai quand la couche n'a ni arete non-variete, ni bord, ni
	// sommet non-variete (comptes sur les segments non consommes).
	// Rien n'est imprime : les comptes sont rendus a l'appelant.
	bool checkManifold (unsigned int* nonManifoldEdges = nullptr,
	                    unsigned int* borders = nullptr,
	                    unsigned int* nonManifoldVertices = nullptr) const;

	// Le test par lequel execute() choisit le chemin nominal.
	bool isManifoldOnIndices () const;

private:
	uint32_t howManySimilarEdge (const SliceSegment& s) const;
	uint32_t howManySimilarVertex (const SliceSegment& s, bool startPoint = true) const;
	uint32_t howManySimilarVertex (const SliceSegment& s, std::vector<SliceSegment*>& found,
	                               bool startPoint = true);
	template <bool Collect>
	uint32_t howManySimilarVertexImpl (const SliceSegment& s, std::vector<SliceSegment*>* found,
	                                   bool startPoint) const;

	bool removeDuplicateEdges ();
	bool removeHairs ();
	bool removeNonManifoldVerticesFromNullEdge ();
	bool removeNonManifoldVertices ();
	bool removeZeroLengthIntersections ();
	bool linkByComparingIndices ();
	bool linkByComparingPoints ();
	bool linkToFillHoles ();

	void insertIntoMap (uint32_t index);
	void updateIntoMap (uint32_t index);
	void buildMap ();

	// Cle d'arete independante de l'ordre : edgeKey (a, b) == edgeKey (b, a).
	static uint64_t edgeKey (int v1, int v2);

	// arete -> indices des segments qui la portent. Un std::set d'ENTIERS : son
	// parcours est deterministe, independant des adresses et de la plateforme.
	std::unordered_map<uint64_t, std::set<uint32_t>> m_map;
	std::vector<SliceSegment>& m_segments;
};

// Parcourt les segments chaines et rend les chemins -- des boucles fermees
// quand le chainage l'est, le premier point etant alors REPETE en fin de
// chemin --, avec la liste des faces traversees par chacun.
//
// Deux particularites, retenues telles quelles :
//   - la validite teste le segment de DEPART et non le segment courant ;
//   - en marche arriere, les faces sont ajoutees en queue (push_back) alors
//     que les points sont inseres en tete : `faces` n'est donc pas aligne sur
//     les points (il reste l'ensemble des faces traversees).
// La coordonnee z des points est conservee ; l'appelant l'abandonne.
//
// `closed`, optionnel (D8) : pour chaque chemin, vrai quand il est FERME
// au sens topologique -- la marche avant est revenue sur le segment de depart
// et la marche arriere n'a rien ajoute (liens asymetriques d'une reparation).
// Le dernier point repete alors le premier, mais calcule par le segment qui
// ARRIVE a cette jonction, alors que le premier vient du segment de depart :
// les deux copies peuvent differer d'un ulp. Aucun test de coordonnees n'est
// donc fait.
bool traceChains (std::vector<SliceSegment>& segments,
                    std::vector<std::vector<Vector3f>>& paths,
                    std::vector<std::vector<int>>& faces,
                    std::vector<bool>* closed = nullptr);

} // namespace slicing
} // namespace cgmesh
