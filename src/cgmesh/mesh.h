#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <memory>
#include <cstdint>

#include "../cgmath/cgmath.h"
#include "material.h"
#include "tensor.h"
#include "bounding_box.h"

class Octree;

class Face
{
public:
	Face ();
	Face (const Face &f); // constructor of copy
	~Face ();

	void Init (void);
	int SetNVertices (unsigned int n);
	int GetNVertices (void) { return m_nVertices; };

	// vertices
	inline int GetVertex (unsigned int i) { if (i>=m_nVertices) return -1; return m_pVertices[i]; };
	inline int SetVertex (unsigned int i, unsigned int vi) { if (i>=m_nVertices) return -1; m_pVertices[i] = vi; return 1; };

	// tex coord
	inline int ActivateTextureCoordinatesIndices ()
		{
			if (m_pTextureCoordinatesIndices)
				delete[] m_pTextureCoordinatesIndices;
			m_pTextureCoordinatesIndices = new unsigned int[2*m_nVertices];
			return 1;
		}

	inline int ActivateTextureCoordinates ()
		{
			if (m_pTextureCoordinates)
				delete[] m_pTextureCoordinates;
			m_pTextureCoordinates = new float[2*m_nVertices];

			return 1;
		}

	inline bool InitTexCoord()
	{
		if (!m_pTextureCoordinatesIndices)
			return false;
		for (int i = 0; i < m_nVertices; i++)
			m_pTextureCoordinatesIndices[i] = GetVertex(i);
		return true;
	}

	inline int SetTexCoord (unsigned int i, unsigned int ti)
		{
			if (i>=m_nVertices)
				return -1;
			m_pTextureCoordinatesIndices[i] = ti;
			return 1;
		};
	
	inline int SetTexCoord (unsigned int i, float u, float v)
		{
			if (i>=m_nVertices)
				return -1;
			m_pTextureCoordinates[2*i]   = u;
			m_pTextureCoordinates[2*i+1] = v;
			return 1;
		};

	void SetTriangle (unsigned int a, unsigned int b, unsigned int c);
	void SetQuad (unsigned int a, unsigned int b, unsigned int c, unsigned int d);

	inline void Flip (void)
		{
			for (unsigned int i=0; i<m_nVertices/2; i++)
			{
				unsigned int j = m_pVertices[i];
				m_pVertices[i] = m_pVertices[m_nVertices-1-i];
				m_pVertices[m_nVertices-1-i] = j;
			}
		}

	// material
	inline int GetMaterialId (void) { return m_iMaterialId; };
	inline void SetMaterialId (unsigned int mi) { m_iMaterialId = mi; };

	void dump (void);

public:
	unsigned int m_nVertices;
	unsigned int *m_pVertices;
	bool m_bUseTextureCoordinates;
	unsigned int *m_pTextureCoordinatesIndices;
	float *m_pTextureCoordinates;
	unsigned int m_iMaterialId;
};

//
// class Mesh
//
typedef int (*funcptr_v)(float x, float y, float z);
class Mesh : public Geometry
{
public:
	Mesh ();
	Mesh (unsigned int nVertices, unsigned int nFaces);
	Mesh (Mesh &m);
	~Mesh();

	void Dump();

	// from class Geometry
	bool GetIntersectionBboxWithRay (vec3 o, vec3 d);
	int GetIntersectionWithRayInOctree (vec3 o, vec3 d, float *_t, vec3 i, vec3 n, Octree *pOctree);
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);
	virtual void* GetMaterial (void);

private:
	void DeleteFaces (void);

	// Init
protected:
	void InitVertices (unsigned int nVertices);
	void InitFaces (unsigned int nFaces);
	void InitTensors (void);
public:
	void Init (void);
	void Init (unsigned int nVertices, unsigned int nFaces);
	void InitVertexColors (float r = 0., float g = 0., float b = 0.);
	void InitVertexColorsFromCurvatures (Tensor::eCurvature curvature);
	void InitVertexColorsFromArray (float *array, char *defined = nullptr);

	// Revision
	uint64_t GetRevision() const;
	void IncrementRevision();

	// Getters / Setters
	unsigned int* GetTriangles (void);

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
	//     slots (positions = m_pVertices, normals = m_pVertexNormals) →
	//     no duplication, smooth shading preserved.
	//   - Polygon faces (N>=4) append N fresh "expansion" slots that
	//     duplicate the corner positions but carry the face's Newell
	//     polygon normal uniformly → eliminates fan-diagonal kinks on
	//     non-planar n-gons.
	// A mesh of pure triangles emits exactly m_nVertices render-vertices
	// (same as the topology); only n-gons pay the duplication cost.
	struct PolygonRenderData
	{
		std::vector<float>        positions; // 3 floats per render-vertex
		std::vector<float>        normals;   // 3 floats per render-vertex
		std::vector<float>        texCoords; // 2 floats per render-vertex (empty if absent)
		std::vector<float>        colors;    // 3 floats per render-vertex (empty if absent)
		std::vector<unsigned int> indices;   // triangle indices into the above
	};
	PolygonRenderData BuildPolygonRenderData();

	// Replace every N-gon face (N>=4) with (N-2) triangle Face objects
	// (fan for convex, glutess for concave). Triangles are kept as-is.
	// Material ids are propagated to each emitted sub-triangle. After
	// triangulation, ComputeNormals() is called and the revision is
	// bumped so render caches refresh on the next draw.
	void Triangulate();
	inline int GetVertex (unsigned int i, float v[3]) {
		if (i>=m_nVertices) return -1;
		i*=3;
		v[0] = m_pVertices[i];
		v[1] = m_pVertices[i+1];
		v[2] = m_pVertices[i+2];
		return 0;
	};
	inline int GetFaceVertex (unsigned int fi, unsigned int vi) { return m_pFaces[fi]->GetVertex (vi); };

	Face *GetFace (unsigned int fi) { return m_pFaces[fi]; };
	inline int GetFaceNVertices (unsigned int fi) { return m_pFaces[fi]->GetNVertices (); };
	inline int GetFaceMaterialId (unsigned int fi) { return m_pFaces[fi]->GetMaterialId (); };
	inline void SetFaceMaterialId (unsigned int fi, unsigned int mi) { m_pFaces[fi]->SetMaterialId (mi); };
	void GetFaceBarycenter (unsigned int fi, vec3 bar)
		{
			Face *f=m_pFaces[fi];
			if (f->m_nVertices == 3)
			{
				unsigned int a = f->m_pVertices[0];
				unsigned int b = f->m_pVertices[1];
				unsigned int c = f->m_pVertices[2];
				vec3_init (bar,
					   (m_pVertices[3*a] + m_pVertices[3*b] + m_pVertices[3*c]) / 3.,
					   (m_pVertices[3*a+1] + m_pVertices[3*b+1] + m_pVertices[3*c+1]) / 3.,
					   (m_pVertices[3*a+2] + m_pVertices[3*b+2] + m_pVertices[3*c+2]) / 3. );
			}
		};

	int SetVertices(unsigned int nVertices, float* pVertices);
	int SetVertexNormals(unsigned int nVerticesNormals, float* pVerticesNormals);
	int SetFaces (unsigned int nFaces, unsigned int nVerticesPerFace,
		      unsigned int *pFaces, unsigned int *pTextureCoordinates=nullptr);
	//int SetTextureCoordinates (unsigned int nTextureCoordinates, float *pTextureCoordinates);

	int SetVertex (unsigned int i, float x, float y, float z);
	int SetFace (unsigned int i,
		     unsigned int a, unsigned int b, unsigned int c);
	int SetFace (unsigned int i,
		     unsigned int a, unsigned int b, unsigned int c, unsigned int d);

	Tensor* GetTensor (unsigned int index);

	// Treatments on vertices
//	bool ColorizeVerticesDensity_Traverse (Octree &o, void *data);
	int ColorizeVerticesDensity (float k);

	void FlipFaces (void)
		{
			for (unsigned int i=0; i<m_nFaces; i++)
				m_pFaces[i]->Flip();
		}

	// edit
	int DeleteVertices (funcptr_v func);
	int MergeVertices (float tolerance = 1e-6f);

	// noise
	void add_gaussian_noise (float variance);

	void GetTopologicIssues(std::vector<unsigned int>& nonManifoldBorders, std::vector<unsigned int>& borders) const;

	// IO
private:
	int import_mtl (const char *filename, const char *path);
	int import_obj (const char *filename);
	int import_objnm (const char *filename);
	int export_obj (const char *filename);
	int import_3ds (const char *filename);
	int export_3ds (const char *filename);
	int import_asc (const char *filename);
	int export_asc (const char *filename);
	int import_pset (const char *filename);
	int export_pset (const char *filename);
	int export_dae (const char *filename);
	int export_cpp (const char *filename);
	int export_gts (const char *filename);
	int import_ifs (const char *filename);
	int import_lwo (const char *filename);
	int import_off (const char *filename);
	int export_off (const char *filename);
	int import_pgm (const char *filename);
	int import_pts (const char *filename);
	int export_pts (const char *filename);
	int import_ply (const char *filename);
	int export_ply (const char *filename);
	int import_stl (const char *filename);
	int export_stl (const char *filename);          // ASCII STL (called by save() for .stl)
	int import_u3d (const char *filename);
	int export_u3d (const char *filename);
public:
	int load (const char *filename);
	int save (const char *filename);
	int export_stl_binary (const char *filename);   // Binary STL (caller chooses format)

	// bbox
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
	
	//
	// stats
	//
	int stats_vertices_in_faces (int *verticesinfaces, int n);

	unsigned int GetNVertices() const;
	unsigned int GetNFaces() const;
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
	unsigned int GetNMaterials () const { return (unsigned int)m_pMaterials.size(); }

	Material* GetMaterial (unsigned int id)
		{
			if (id < m_pMaterials.size())
				return m_pMaterials[id].get();
			return nullptr;
		}
	int GetMaterialId (const std::string & material_name)
		{
			for (size_t i = 0; i < m_pMaterials.size(); ++i)
				if (m_pMaterials[i] && m_pMaterials[i]->GetName() == material_name)
					return (int)i;
			return -1;
		}
	void SetMaterial (unsigned int id, Material *pMaterial)
	{
		if (id >= m_pMaterials.size())
			m_pMaterials.resize(id + 1);
		m_pMaterials[id].reset(pMaterial);
	};

	unsigned int Material_Add (Material *pMaterial)
	{
		m_pMaterials.emplace_back(pMaterial);
		return (unsigned int)m_pMaterials.size() - 1;
	};

	void ApplyMaterial (unsigned int id)
	{
		if (id >= m_pMaterials.size())
			return;
		for (unsigned int i=0; i<m_nFaces; i++)
			m_pFaces[i]->m_iMaterialId = id;
	}

	//
	// Transformations
	//
	void centerize (void);
	void scale (float s);
	void scale_xyz (float sx, float sy, float sz);
	void translate (float tx, float ty, float tz);
	void transform (float mrot[9]);
	void transform (mat3 m);
	void transform4 (mat4 m);

	//
	unsigned int CountEdges (void);
	int Append (Mesh *m);

public:
	std::string m_name;
	unsigned int m_nVertices;
	unsigned int m_nFaces;
	std::vector<float> m_pVertices;
	std::vector<float> m_pVertexNormals;
	std::vector<float> m_pVertexColors;
	Face **m_pFaces;
	std::vector<float> m_pFaceNormals;
	unsigned int m_nTextureCoordinates;
	std::vector<float> m_pTextureCoordinates;
	std::vector<std::unique_ptr<Material>> m_pMaterials;
	std::vector<std::unique_ptr<Tensor>>   m_pTensors;

	BoundingBox m_bbox;

	Octree *m_pOctree;

private:
	uint64_t m_revision = 0;
};
