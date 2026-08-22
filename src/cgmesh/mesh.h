#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

#include "../cgmath/cgmath.h"
#include "material.h"
#include "tensor.h"
#include "bounding_box.h"

//
// class Mesh
//
typedef int (*funcptr_v)(float x, float y, float z);
class Mesh : public Geometry
{
public:
	Mesh ();
	Mesh (unsigned int nVertices, unsigned int nFaces);
	~Mesh();

	// Copie PROFONDE : le maillage copie ne partage rien de mutable avec
	// l'original. `= default` suffit, chaque membre etant un type valeur -- ne pas
	// enumerer les membres ici, ce serait une chose de plus a maintenir.
	//
	// ⚠ Seule exception : l'image d'une MaterialTexture est partagee par comptage
	// de references, donc traitee comme IMMUABLE.
	Mesh (const Mesh &) = default;
	Mesh& operator= (const Mesh &) = default;

	void Dump();

	// from class Geometry
	bool GetIntersectionBboxWithRay (const Vector3f &o, const Vector3f &d);

	// Chemin NON ACCELERE : tous les triangles testes, sans structure spatiale.
	// Existe parce que Geometry la declare virtuelle pure -- la retirer rendrait
	// Mesh abstraite -- et sert les appelants qui ne voient qu'un `Geometry*`.
	// Pour du lancer de rayon repete, utiliser mesh_raycast.h : un octree detenu
	// par l'appelant et la fonction libre qui l'exploite.
	virtual int GetIntersectionWithRay (const Vector3f &o, const Vector3f &d, float *_t, Vector3f &i, Vector3f &n);
	virtual int GetIntersectionWithSegment (const Vector3f &vStart, const Vector3f &vEnd, float *_t, Vector3f &i, Vector3f &n);
	virtual const void* GetMaterial (void) const;

private:
	void DeleteFaces (void);

	// Init
protected:
	void InitVertices (unsigned int nVertices);
	void InitFaces (unsigned int nFaces);
public:
	void Init (void);
	void Init (unsigned int nVertices, unsigned int nFaces);
	void InitVertexColors (float r = 0., float g = 0., float b = 0.);
	void InitVertexColorsFromCurvatures (CurvatureType curvature);
	void InitVertexColorsFromArray (float *array, char *defined = nullptr);

	// Couleurs par sommet : 3 flottants par sommet, ou VIDE (pas de couleurs).
	// Mixtes par nature -- importees d'un fichier ou derivees des courbures.
	//
	// Ecrire une couleur NE TOUCHE PAS la revision de geometrie, et ce n'est pas
	// un choix de confort : InitVertexColorsFromCurvatures lit AreTensorsValid()
	// pour avertir d'un cache perime. Si colorier incrementait la revision, un
	// second coloriage depuis les memes tenseurs avertirait a tort -- or c'est
	// exactement ce que fait le recoloriage a la volee de l'interface.
	const std::vector<float>& GetVertexColors (void) const { return m_vertexColors; }
	int SetVertexColor (unsigned int i, float r, float g, float b);
	// Une SEULE composante (dim = 0, 1 ou 2), pour les lecteurs de fichiers qui
	// les recoivent une par une.
	int SetVertexColorComponent (unsigned int i, unsigned int dim, float value);
	// Remplace le tableau entier. Taille acceptee : 3 * GetNVertices(), ou 0.
	int SetVertexColors (std::vector<float> colors);

	// Revision
	uint64_t GetRevision() const;
	void IncrementRevision();

	// Curvature tensors validity
	// The tensors are computed for a given geometry; a later geometry edit
	// bumps the revision and leaves the tensors stale. MarkTensorsComputed()
	// stamps them against the current revision; AreTensorsValid() reports
	// whether they still match it.
	void MarkTensorsComputed();
	bool AreTensorsValid() const;

	// Replace this mesh's per-vertex curvature tensors with a deep copy of
	// src's, and stamp them valid against the current geometry revision.
	// Used to bring tensors computed on a Mesh_half_edge working copy back
	// onto the rendered mesh, so the curvature colour can later be re-derived
	// (InitVertexColorsFromCurvatures) for a different CurvatureType without
	// recomputing the tensor field. src must share this mesh's vertex order
	// and count (it is the half-edge copy of *this*); on a size mismatch the
	// tensors are cleared instead of copied.
	void AdoptTensorsFrom(const Mesh& src);

	// Getters / Setters
	std::vector<unsigned int> GetTriangles (void);

	// Triangulation utility: returns a flat triangle-index list covering
	// every face. Triangles emit as-is; quads fan-triangulate from vertex
	// 0; faces with N>=5 or detected concave are routed through glutess.
	// Returned vector size is 3 * (sum of (N-2) over all faces). Useful
	// for any code that needs raw triangle topology; the rendering path
	// uses BuildPolygonRenderData() instead, which preserves polygon
	// identity (one normal per polygon).
	std::vector<unsigned int> BuildTriangulation();

	// Hybrid render data layout:
	//   - Triangle faces (N==3) index directly into the shared topology
	//     slots (positions = m_pVertices, normals = m_vertexNormals) →
	//     no duplication, smooth shading preserved.
	//   - Polygon faces (N>=4) append N fresh "expansion" slots that
	//     duplicate the corner positions but carry the face's Newell
	//     polygon normal uniformly → eliminates fan-diagonal kinks on
	//     non-planar n-gons.
	// A mesh of pure triangles emits exactly m_nVertices render-vertices
	// (same as the topology); only n-gons pay the duplication cost.
	// A contiguous run of 'indices' that shares one material. Lets the VBO
	// path draw a multi-material mesh as one buffer + several glDrawElements
	// calls (activating each material in turn) instead of falling back to slow
	// immediate mode.
	struct MaterialRange
	{
		unsigned int materialId; // mesh material index, or MATERIAL_NONE
		unsigned int offset;     // first index into 'indices'
		unsigned int count;      // number of indices (multiple of 3)
	};

	struct PolygonRenderData
	{
		std::vector<float>        positions; // 3 floats per render-vertex
		std::vector<float>        normals;   // 3 floats per render-vertex
		std::vector<float>        texCoords; // 2 floats per render-vertex (empty if absent)
		std::vector<float>        colors;    // 3 floats per render-vertex (empty if absent)
		std::vector<unsigned int> indices;   // triangle indices into the above,
		                                     // grouped by material
		std::vector<MaterialRange> materialRanges; // one run per material
	};
	// flat=false (smooth): triangles share topology slots and use per-vertex
	// normals. flat=true: every face (triangles included) is expanded into its
	// own non-shared corners carrying the face normal, so each triangle is
	// uniformly shaded (true flat shading, independent of vertex welding).
	PolygonRenderData BuildPolygonRenderData(bool flat = false);

	// Replace every N-gon face (N>=4) with (N-2) triangle Face objects
	// (fan for convex, glutess for concave). Triangles are kept as-is.
	// Material ids are propagated to each emitted sub-triangle. After
	// triangulation, ComputeNormals() is called and the revision is
	// bumped so render caches refresh on the next draw.
	void Triangulate();
	inline int GetVertex (unsigned int i, float v[3]) const {
		if (i>=m_nVertices) return -1;
		i*=3;
		v[0] = m_pVertices[i];
		v[1] = m_pVertices[i+1];
		v[2] = m_pVertices[i+2];
		return 0;
	};
	// Vector3f overload (migration vers TVector3 ; la version vec3 reste le temps de la transition)
	inline int GetVertex (unsigned int i, Vector3f &v) const {
		if (i>=m_nVertices) return -1;
		i*=3;
		v.Set (m_pVertices[i], m_pVertices[i+1], m_pVertices[i+2]);
		return 0;
	};
	inline int GetFaceVertex (unsigned int fi, unsigned int vi) const { return FaceAt (fi)->GetVertex (vi); };

	// Remplace toutes les faces par nFaces faces neuves. Contrairement a
	// Init (nv, nf), NE TOUCHE PAS aux sommets. Incremente la revision.
	void SetNFaces (unsigned int nFaces);

	// Supprime la face fi et laisse un TROU : le compte ne change pas, mais
	// FaceAt (fi) devient invalide. Incremente la revision.
	void RemoveFace (unsigned int fi);

	//
	// Reference sur une face : (maillage, indice), et non un pointeur vers le
	// stockage. Classe imbriquee, donc elle atteint le prive de Mesh sans
	// declaration friend.
	//
	// operator-> renvoie l'objet lui-meme, ce qui permet d'ecrire
	// mesh.FaceAt (i)->GetVertex (0).
	//
	// ⚠ Une reference devient caduque des que le nombre de faces change
	// (InitFaces, SetNFaces, Append...), comme un iterateur de vector apres un
	// push_back.
	class ConstFaceRef
	{
	public:
		ConstFaceRef (void) : m_mesh (nullptr), m_index (0) {}
		ConstFaceRef (const Mesh *mesh, unsigned int index)
			: m_mesh (mesh), m_index (index) {}

		// Faux pour un indice hors bornes ET pour un TROU (cf. Mesh::RemoveFace).
		bool IsValid (void) const
			{ return m_mesh && m_mesh->FaceExists (m_index); }
		explicit operator bool (void) const { return IsValid (); }
		unsigned int GetIndex (void) const { return m_index; }

		const ConstFaceRef *operator-> (void) const { return this; }

		int GetNVertices (void) const
			{ return IsValid () ? (int)m_mesh->FaceArity (m_index) : 0; }

		int GetVertex (unsigned int i) const
			{
				if (!IsValid () || i >= m_mesh->FaceArity (m_index)) return -1;
				return (int)m_mesh->m_faceVertices[m_mesh->FaceBegin (m_index) + i];
			}

		int GetTexCoordIndex (unsigned int i) const
			{
				if (!IsValid () || i >= m_mesh->FaceArity (m_index)) return -1;
				if (m_mesh->m_faceTexIndices.empty ()) return -1;
				return (int)m_mesh->m_faceTexIndices[m_mesh->FaceBegin (m_index) + i];
			}
		int GetTexCoord (unsigned int i, float uv[2]) const
			{
				if (!IsValid () || i >= m_mesh->FaceArity (m_index)) return -1;
				if (m_mesh->m_faceTexCoords.empty ()) return -1;
				const unsigned int k = 2 * (m_mesh->FaceBegin (m_index) + i);
				uv[0] = m_mesh->m_faceTexCoords[k];
				uv[1] = m_mesh->m_faceTexCoords[k+1];
				return 0;
			}

		// Propriete du MAILLAGE, non de la face : les UV sont optionnels, et ces
		// tableaux sont vides quand le maillage n'en porte pas.
		bool HasTexCoordIndices (void) const
			{ return m_mesh && !m_mesh->m_faceTexIndices.empty (); }
		bool HasCornerTexCoords (void) const
			{ return m_mesh && !m_mesh->m_faceTexCoords.empty (); }

		// Drapeau PAR FACE, independant de la presence des tableaux.
		bool UsesTextureCoordinates (void) const
			{
				if (!IsValid () || m_mesh->m_faceHasTex.empty ()) return false;
				return m_mesh->m_faceHasTex[m_index] != 0;
			}

		int GetMaterialId (void) const
			{
				if (!IsValid ()) return (int)MATERIAL_NONE;
				return (int)m_mesh->m_faceMaterial[m_index];
			}

	protected:
		const Mesh *m_mesh;
		unsigned int m_index;
	};

	class FaceRef : public ConstFaceRef
	{
	public:
		FaceRef (void) {}
		FaceRef (Mesh *mesh, unsigned int index) : ConstFaceRef (mesh, index) {}

		const FaceRef *operator-> (void) const { return this; }

		int SetNVertices (unsigned int n) const
			{
				if (!IsValid ()) return -1;
				M ()->SetFaceArity (m_index, n);
				return 0;
			}

		int SetVertex (unsigned int i, unsigned int vi) const
			{
				if (!IsValid () || i >= m_mesh->FaceArity (m_index)) return -1;
				M ()->m_faceVertices[m_mesh->FaceBegin (m_index) + i] = vi;
				return 1;
			}

		void SetTriangle (unsigned int a, unsigned int b, unsigned int c) const
			{
				SetNVertices (3);
				SetVertex (0, a); SetVertex (1, b); SetVertex (2, c);
			}
		void SetQuad (unsigned int a, unsigned int b, unsigned int c, unsigned int d) const
			{
				SetNVertices (4);
				SetVertex (0, a); SetVertex (1, b); SetVertex (2, c); SetVertex (3, d);
			}

		void Flip (void) const
			{
				if (!IsValid ()) return;
				const unsigned int n = m_mesh->FaceArity (m_index);
				const unsigned int b = m_mesh->FaceBegin (m_index);
				Mesh *m = M ();
				for (unsigned int i = 0; i < n/2; i++)
					std::swap (m->m_faceVertices[b+i], m->m_faceVertices[b+n-1-i]);
			}

		// Alloues a la demande, dimensionnes sur le pool entier : activer pour une
		// face active pour tout le maillage.
		int ActivateTextureCoordinatesIndices (void) const
			{ if (!IsValid ()) return -1; M ()->EnsureTexIndices (); return 1; }
		int ActivateTextureCoordinates (void) const
			{ if (!IsValid ()) return -1; M ()->EnsureTexCoords (); return 1; }

		bool InitTexCoord (void) const
			{
				if (!IsValid ()) return false;
				Mesh *m = M ();
				m->EnsureTexIndices ();
				const unsigned int n = m_mesh->FaceArity (m_index);
				const unsigned int b = m_mesh->FaceBegin (m_index);
				for (unsigned int i = 0; i < n; i++)
					m->m_faceTexIndices[b+i] = m->m_faceVertices[b+i];
				return true;
			}

		int SetTexCoord (unsigned int i, unsigned int ti) const
			{
				if (!IsValid () || i >= m_mesh->FaceArity (m_index)) return -1;
				Mesh *m = M ();
				m->EnsureTexIndices ();
				m->m_faceTexIndices[m_mesh->FaceBegin (m_index) + i] = ti;
				return 1;
			}
		int SetTexCoord (unsigned int i, float u, float v) const
			{
				if (!IsValid () || i >= m_mesh->FaceArity (m_index)) return -1;
				Mesh *m = M ();
				m->EnsureTexCoords ();
				const unsigned int k = 2 * (m_mesh->FaceBegin (m_index) + i);
				m->m_faceTexCoords[k]   = u;
				m->m_faceTexCoords[k+1] = v;
				return 1;
			}

		void SetUsesTextureCoordinates (bool b) const
			{
				if (!IsValid ()) return;
				// Un drapeau faux sur un maillage qui n'en a aucun n'a rien a ecrire.
				if (!b && m_mesh->m_faceHasTex.empty ()) return;
				Mesh *m = M ();
				m->EnsureHasTexFlags ();
				m->m_faceHasTex[m_index] = b ? 1 : 0;
			}

		void SetMaterialId (unsigned int mi) const
			{
				if (!IsValid ()) return;
				M ()->m_faceMaterial[m_index] = mi;
			}

	private:
		// Le const_cast est confine ici : FaceRef ne se construit que depuis un
		// Mesh* non const, donc le maillage vise n'est pas const.
		Mesh *M (void) const { return const_cast<Mesh *>(m_mesh); }
	};

	// Hors bornes ou sur un trou : reference invalide, jamais un comportement
	// indefini.
	FaceRef FaceAt (unsigned int fi)
		{ return FaceExists (fi) ? FaceRef (this, fi) : FaceRef (); }
	ConstFaceRef FaceAt (unsigned int fi) const
		{ return FaceExists (fi) ? ConstFaceRef (this, fi) : ConstFaceRef (); }

	inline int GetFaceNVertices (unsigned int fi) const { return FaceAt (fi)->GetNVertices (); };
	inline int GetFaceMaterialId (unsigned int fi) { return FaceAt (fi)->GetMaterialId (); };
	inline void SetFaceMaterialId (unsigned int fi, unsigned int mi) { FaceAt (fi)->SetMaterialId (mi); };
	void GetFaceBarycenter (unsigned int fi, Vector3f &bar)
		{
			auto f = FaceAt (fi);
			if (f->GetNVertices () == 3)
			{
				unsigned int a = (unsigned int)f->GetVertex (0);
				unsigned int b = (unsigned int)f->GetVertex (1);
				unsigned int c = (unsigned int)f->GetVertex (2);
				bar.Set (
					   (m_pVertices[3*a] + m_pVertices[3*b] + m_pVertices[3*c]) / 3.,
					   (m_pVertices[3*a+1] + m_pVertices[3*b+1] + m_pVertices[3*c+1]) / 3.,
					   (m_pVertices[3*a+2] + m_pVertices[3*b+2] + m_pVertices[3*c+2]) / 3. );
			}
		};

	int SetVertices(unsigned int nVertices, const float* pVertices);
	int SetVertexNormals(unsigned int nVerticesNormals, const float* pVerticesNormals);
	// Remplace le tableau entier. La taille doit valoir 3 * GetNVertices().
	int SetVertexNormals(std::vector<float> normals);

	// Normales par sommet : DERIVATION mise en cache, 3 flottants par sommet,
	// donc de taille 3 * GetNVertices() une fois remplies et VIDE avant. Comme la
	// bbox et les normales de face, rien ne detecte qu'elles sont perimees.
	//
	// Ecrire une normale NE TOUCHE PAS la revision de geometrie : une normale
	// n'est pas de la geometrie, elle s'en derive.
	const std::vector<float>& GetVertexNormals (void) const { return m_vertexNormals; }
	int SetVertexNormal (unsigned int i, float x, float y, float z);
	// Une SEULE composante (dim = 0, 1 ou 2). Existe pour les lecteurs de fichiers
	// qui recoivent les composantes une par une (rappel RPly).
	int SetVertexNormalComponent (unsigned int i, unsigned int dim, float value);
	// 3 * GetNVertices() zeros.
	void InitVertexNormals (void);
	// Remplace TOUTES les faces par nFaces faces de meme arite, en liberant les
	// anciennes. Incremente la revision.
	//
	// ⚠ pTextureCoordinates est IGNORE, et l'a toujours ete -- le corps ne le lit
	// nulle part. Aucun appelant ne le renseigne. Conserve pour ne pas casser les
	// signatures ; a supprimer, pas a implementer, faute de contrat clair sur ce
	// qu'il designerait (indices par coin ? par face ?).
	int SetFaces (unsigned int nFaces, unsigned int nVerticesPerFace,
		      unsigned int *pFaces, unsigned int *pTextureCoordinates=nullptr);
	//int SetTextureCoordinates (unsigned int nTextureCoordinates, float *pTextureCoordinates);

	int SetVertex (unsigned int i, float x, float y, float z);
	// Une SEULE composante (dim = 0, 1 ou 2), pour les lecteurs de fichiers qui
	// les recoivent une par une (rappels RPly).
	int SetVertexComponent (unsigned int i, unsigned int dim, float value);
	// Vector3f overload (migration vers TVector3 ; les versions scalaires restent)
	inline int SetVertex (unsigned int i, const Vector3f &v) { return SetVertex (i, v.x, v.y, v.z); }
	int SetFace (unsigned int i,
		     unsigned int a, unsigned int b, unsigned int c);
	int SetFace (unsigned int i,
		     unsigned int a, unsigned int b, unsigned int c, unsigned int d);

	//
	// Curvature tensors
	//
	// Vertex-parallel derived cache: one slot per vertex, a null slot meaning
	// "no tensor at this vertex" (border / non-manifold vertex). The cache is
	// stamped against the geometry revision (MarkTensorsComputed /
	// AreTensorsValid), which the tensors only ever READ: a tensor is not
	// geometry, so writing one leaves the revision alone -- were it bumped, the
	// stamp would never match and the cache could never be valid.
	//
	// Stockage par valeur : vector<optional<Tensor>>. L'option VIDE est
	// l'emplacement vide, ce qu'un vector<Tensor> nu ne saurait pas exprimer.
	//
	// SetTensor prend possession de `t` : la valeur est copiee dans l'emplacement,
	// l'argument est libere. nullptr vide l'emplacement -- c'est ainsi que les
	// estimateurs marquent un sommet de bord.
	unsigned int GetNTensors () const { return (unsigned int)m_tensors.size (); }
	Tensor* GetTensor (unsigned int index);
	const Tensor* GetTensor (unsigned int index) const;
	// An index outside the current slot count deletes `t` instead of growing the
	// array: the slots are vertex-parallel, growing them would break that.
	void SetTensor (unsigned int index, Tensor *t);
	// One null slot per vertex.
	void InitTensors (void);
	void ClearTensors (void);

	// Treatments on vertices
//	bool ColorizeVerticesDensity_Traverse (Octree &o, void *data);
	int ColorizeVerticesDensity (float k);

	void FlipFaces (void)
		{
			for (unsigned int i=0; i<GetNFaces (); i++)
				FaceAt (i)->Flip();
		}

	// edit
	int DeleteVertices (funcptr_v func);

	// Split vertices along UV seams so that UVs become VERTEX-PARALLEL (one UV
	// per vertex, m_texCoords sized 2*m_nVertices). A topological
	// vertex carrying several UVs (a seam / "wedge") is duplicated into one
	// vertex per distinct UV; positions (and per-vertex colours/normals) are
	// copied, faces are reindexed. This is the standard "explode UV wedges"
	// operation — general-purpose (rendering, decimation that must carry UVs,
	// export). No-op if the mesh has no UVs or its UVs are already
	// vertex-parallel. Bumps the geometry revision.
	void SplitVerticesByUVSeams (void);
	// Weld vertices closer than `tolerance`. The weld is geometric: vertex
	// normals are NOT a criterion (callers recompute them afterwards), so all
	// coincident vertices weld — including across facet boundaries of an STL —
	// and the operation is idempotent. UV seams and per-vertex colour islands
	// are preserved (coincident vertices whose UV/colour differ are kept).
	int MergeVertices (float tolerance = 1e-6f);

	// noise
	void add_gaussian_noise (float variance);

	void GetTopologicIssues(std::vector<unsigned int>& nonManifoldBorders, std::vector<unsigned int>& borders) const;

	// IO
	// The format-specific import/export helpers now live in class MeshIO
	// (mesh_io.h). The public entry points below are thin delegators.
public:
	int load (const char *filename);
	int save (const char *filename);
	int export_stl_binary (const char *filename);   // Binary STL (caller chooses format)

	// bbox
	//
	// ⚠ Derivation MISE EN CACHE, et sans detection de peremption : bbox() rend
	// la derniere valeur calculee, telle quelle. C'est a l'appelant d'appeler
	// computebbox() apres toute modification des positions, faute de quoi les
	// trois lectures ci-dessous decrivent une geometrie qui n'existe plus. Sur un
	// maillage jamais passe par computebbox(), la boite est vide.
	int computebbox (void);
	const BoundingBox& bbox() const;
	float bbox_diagonal_length (void) const;
	float GetLargestLength(void) const;

	// area
	float GetFaceArea (unsigned int fi);
	float GetArea (void);
	float* GetAreas (void);
	float* GetCumulativeAreas (void);

	//
	// normals
	//
	void ComputeNormals (void);

	// Normales par face : DERIVATION mise en cache, 3 flottants par face, dans
	// l'ordre des faces, donc de taille 3 * GetNFaces() des que ComputeNormals()
	// est passe -- et VIDE avant. Comme la bbox, rien ne detecte qu'elles sont
	// perimees : c'est a l'appelant d'appeler ComputeNormals() apres une
	// modification de geometrie.
	//
	// La vue est const et le restera : la seule ecriture legitime est par face,
	// via SetFaceNormal.
	const std::vector<float>& GetFaceNormals (void) const { return m_faceNormals; }
	int SetFaceNormal (unsigned int fi, float x, float y, float z);
	
	//
	// stats
	//
	int stats_vertices_in_faces (int *verticesinfaces, int n);

	unsigned int GetNVertices() const;
	// Positions : 3 flottants par sommet. Vue en LECTURE ; l'ecriture passe par
	// SetVertex / SetVertexComponent / SetVertices, qui incrementent la revision
	// de geometrie.
	const std::vector<float>& GetVertices (void) const { return m_pVertices; }
	unsigned int GetNFaces() const;
	// Segments de ligne ('l' d'un OBJ) : les indices vont PAR PAIRES, une
	// polyligne de N sommets donnant N-1 segments. AddLine est la seule ecriture
	// par element, et c'est elle qui garantit la parite -- un push_back nu sur le
	// tableau ne la garantit pas, et un index orphelin serait silencieusement
	// perdu par la division de GetNLines().
	//
	// Ces indices designent des sommets : ils deviennent faux si la numerotation
	// des sommets change, et rien ne le detecte.
	unsigned int GetNLines()  const { return (unsigned int)(m_lines.size() / 2); }
	const std::vector<unsigned int>& GetLines (void) const { return m_lines; }
	void AddLine (unsigned int a, unsigned int b);
	// La taille doit etre PAIRE ; un dernier index impair serait ignore.
	void SetLines (std::vector<unsigned int> lines);
	// Points isoles ('p' d'un OBJ) : un indice de sommet par point. Memes
	// reserves que les segments de ligne -- ces indices designent des sommets et
	// deviennent faux si la numerotation change, sans que rien ne le detecte.
	unsigned int GetNPoints() const { return (unsigned int)m_points.size(); }

	// Coordonnees de texture du MAILLAGE : 2 flottants par entree. Les faces y
	// renvoient par leurs indices de coin (cf. Face), donc leur nombre n'est PAS
	// necessairement celui des sommets.
	//
	// ⚠ LE COMPTE ET LE TABLEAU SONT INDEPENDANTS, et c'est un defaut connu :
	// import_3ds remplit le tableau en laissant le compte a zero, si bien que la
	// verification de coherence de MeshIO desactive les UV par face alors que les
	// donnees sont la. C'est pourquoi SetTextureCoordinates exige les deux
	// explicitement -- pour que le desaccord se voie au lieu d'etre subi.
	unsigned int GetNTextureCoordinates (void) const { return m_nTexCoords; }
	const std::vector<float>& GetTextureCoordinates (void) const { return m_texCoords; }
	int SetTextureCoordinate (unsigned int i, float u, float v);
	int SetTextureCoordinates (std::vector<float> uv, unsigned int nTexCoords);
	const std::vector<unsigned int>& GetPoints (void) const { return m_points; }
	void AddPoint (unsigned int v);
	void SetPoints (std::vector<unsigned int> points);
	bool IsTriangleMesh() const;

	// triangulation
	void triangulate_regular_heightfield (unsigned int width, unsigned int height);

	//
	// Materials
	//
	// Ownership: Mesh owns its materials via unique_ptr. SetMaterial and
	// Material_Add take a raw pointer to a heap-allocated Material that the
	// caller has just `new`ed — ownership transfers to Mesh.
	//
	unsigned int GetNMaterials () const { return (unsigned int)m_materials.size(); }

	Material* GetMaterial (unsigned int id)
		{
			if (id < m_materials.size())
				return m_materials[id].get();
			return nullptr;
		}
	const Material* GetMaterial (unsigned int id) const
		{
			if (id < m_materials.size())
				return m_materials[id].get();
			return nullptr;
		}
	int GetMaterialId (const std::string & material_name)
		{
			for (size_t i = 0; i < m_materials.size(); ++i)
				if (m_materials[i] && m_materials[i]->GetName() == material_name)
					return (int)i;
			return -1;
		}
	// Les materiaux ne sont pas de la geometrie, mais le RENDU en depend, et les
	// caches de rendu s'indexent sur la revision : ces deux ecritures
	// l'incrementent donc. En pratique elles n'ont lieu qu'a l'import ou a la
	// generation, avant qu'aucun accelerateur ni tenseur n'existe.
	void SetMaterial (unsigned int id, Material *pMaterial)
	{
		if (id >= m_materials.size())
			m_materials.resize(id + 1);
		m_materials[id].reset(pMaterial);
		IncrementRevision ();
	};

	unsigned int Material_Add (Material *pMaterial)
	{
		m_materials.emplace_back(pMaterial);
		IncrementRevision ();
		return (unsigned int)m_materials.size() - 1;
	};

	void ApplyMaterial (unsigned int id)
	{
		if (id >= m_materials.size())
			return;
		for (unsigned int i=0; i<GetNFaces (); i++)
			FaceAt (i)->SetMaterialId (id);
	}

	//
	// Transformations
	//
	void centerize (void);
	void scale (float s);
	void scale_xyz (float sx, float sy, float sz);
	void translate (float tx, float ty, float tz);
	void transform (float mrot[9]);
	void transform (const Matrix3f &m);

	//
	unsigned int CountEdges (void);
	int Append (Mesh *m);

	// Nom du maillage. Init () le remet a "#NoName#".
	void SetName (const std::string &name) { m_name = name; };
	const std::string& GetName (void) const { return m_name; };

private:
	std::string m_name;

	// =====================================================================
	//  Stockage des faces -- tableaux plats
	// =====================================================================
	//
	// m_faceVertices est le POOL de coins. Les coins de la face i occupent
	// [FaceBegin (i), FaceBegin (i) + FaceArity (i)).
	//
	// DEUX FORMES, decrites par m_faceArity :
	//
	//  - ARITE UNIFORME (m_faceArity >= 1) : m_faceOffsets et m_faceCorners sont
	//    VIDES et FaceBegin (i) vaut i * m_faceArity. Un maillage tout triangles ne
	//    paie donc aucun tableau d'offsets.
	//
	//  - MIXTE (m_faceArity == 0) : m_faceOffsets[i] et m_faceCorners[i] decrivent
	//    la tranche de la face i.
	//
	//    ⚠ Les offsets NE SONT PAS MONOTONES dans cette forme : changer l'arite
	//    d'une face ajoute sa tranche en fin de pool, ce qui garde SetNVertices en
	//    temps constant amorti au prix de tranches orphelines. Ne rien supposer de
	//    la forme offsets[i+1] == offsets[i] + arite.
	//
	// La forme uniforme est un DESCRIPTEUR, pas un second stockage : derivable du
	// pool a tout moment, donc verifiable (AssertFaceStorage).
	std::vector<unsigned int> m_faceVertices;
	std::vector<unsigned int> m_faceOffsets;
	std::vector<unsigned int> m_faceCorners;
	unsigned int m_faceArity = 0;

	// Un par face, TOUJOURS present : c'est lui qui porte le nombre de faces. Il
	// n'existe aucun membre de comptage separe, qui pourrait en diverger.
	std::vector<unsigned int> m_faceMaterial;

	// VIDE quand le maillage n'a aucun trou. Mesh_half_edge::edge_contract
	// supprime des faces sans changer le compte.
	std::vector<uint8_t> m_faceRemoved;

	// UV par face : OPTIONNELS, au niveau du MAILLAGE. Ces trois tableaux sont
	// VIDES tant que personne n'ecrit d'UV.
	//
	// Les deux representations coexistent sans fusion : des indices dans le
	// tableau d'UV du maillage (modele OBJ), et des valeurs par coin.
	std::vector<unsigned int> m_faceTexIndices;  // 1 par coin, indexe comme m_faceVertices
	std::vector<float>        m_faceTexCoords;   // 2 par coin
	std::vector<uint8_t>      m_faceHasTex;      // 1 par face

	// --- Indexation. Le seul endroit qui connait la disposition du pool. ---
	inline unsigned int FaceBegin (unsigned int i) const
		{ return m_faceOffsets.empty () ? i * m_faceArity : m_faceOffsets[i]; }
	inline unsigned int FaceArity (unsigned int i) const
		{ return m_faceCorners.empty () ? m_faceArity : m_faceCorners[i]; }
	inline bool FaceExists (unsigned int i) const
		{
			return i < m_faceMaterial.size ()
			       && (m_faceRemoved.empty () || !m_faceRemoved[i]);
		}

	// Passe de la forme uniforme a la forme mixte, sans changer le contenu.
	// Appelee des qu'une arite par face doit differer des autres.
	void MaterialiseFaceOffsets (void);
	// Change l'arite de la face fi. Ajoute une tranche en fin de pool si elle
	// grandit ; se contente de reduire le compte si elle rapetisse.
	void SetFaceArity (unsigned int fi, unsigned int n);
	// Alloue a la demande les tableaux d'UV, dimensionnes sur le pool.
	void EnsureTexIndices (void);
	void EnsureTexCoords (void);
	void EnsureHasTexFlags (void);
	// Verification du descripteur d'arite contre le pool. Vide en release.
	// Invariants de taille, en temps CONSTANT : appelable par face.
	void AssertFaceStorage (void) const;
	// Verification complete, LINEAIRE : reservee aux operations en masse.
	void AssertFaceStorageFull (void) const;
	// Remet la face a l'etat neuf : triangle, sans materiau, sans UV, vivante.
	void ResetFace (unsigned int fi);
	// Repasse a la forme uniforme quand toutes les arites coincident.
	void CompactFaceArity (void);
	// Ne garde que les faces listees, dans cet ordre, et compacte le pool.
	// Les trous et les tranches orphelines disparaissent.
	void KeepFaces (const std::vector<unsigned int> &keep);

private:
	unsigned int m_nVertices;
	std::vector<float> m_pVertices;

	unsigned int m_nTexCoords;
	std::vector<float> m_texCoords;

	std::vector<float> m_vertexNormals;
	std::vector<float> m_vertexColors;

	std::vector<MaterialPtr>               m_materials;

	// Points isoles, un indice de sommet par point. Voir GetPoints().
	std::vector<unsigned int> m_points;

	// Segments de ligne, deux indices de sommet par segment. Voir GetLines().
	std::vector<unsigned int> m_lines;

	std::vector<float> m_faceNormals;

	BoundingBox m_bbox;

	std::vector<std::optional<Tensor>>     m_tensors;

	uint64_t m_revision = 0;
	// Geometry revision at which m_tensors was last computed; (uint64_t)-1
	// means "never computed / invalid".
	uint64_t m_tensorsRevision = (uint64_t)-1;
};
