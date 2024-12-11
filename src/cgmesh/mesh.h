#ifndef __MESH_H__
#define __MESH_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../cgmath/cgmath.h"
#include "material.h"
#include "tensor.h"

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
				delete m_pTextureCoordinatesIndices;
			m_pTextureCoordinatesIndices = new unsigned int[2*m_nVertices];
			return 1;
		}

	inline int ActivateTextureCoordinates ()
		{
			if (m_pTextureCoordinates)
				delete m_pTextureCoordinates;
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

	// from class Geometry
	bool GetIntersectionBboxWithRay (vec3 o, vec3 d);
	int GetIntersectionWithRayInOctree (vec3 o, vec3 d, float *_t, vec3 i, vec3 n, Octree *pOctree);
	virtual int GetIntersectionWithRay (vec3 o, vec3 d, float *_t, vec3 i, vec3 n);
	virtual int GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n);
	virtual void* GetMaterial (void);

private:
	void DeleteFaces (void);
public:
	~Mesh ();

	void Dump ();

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
	void InitVertexColorsFromArray (float *array, char *defined = NULL);

	// Getters / Setters
	unsigned int* GetTriangles (void);
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

	int SetVertices (unsigned int nVertices, float *pVertices);
	int SetFaces (unsigned int nFaces, unsigned int nVerticesPerFace,
		      unsigned int *pFaces, unsigned int *pTextureCoordinates=NULL);
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

	// noise
	void add_gaussian_noise (float variance);

	// IO
private:
	int import_mtl (char *filename, char *path);
	int import_obj (char *filename);
	int import_objnm (char *filename);
	int export_obj (char *filename);
	int import_3ds (char *filename);
	int export_3ds (char *filename);
	int import_asc (char*filename);
	int export_asc (char*filename);
	int import_pset (char *filename);
	int export_pset (char *filename);
	int export_dae (char *filename);
	int export_cpp (char *filename);
	int export_gts (char *filename);
	int import_ifs (char *filename);
	int import_lwo (char *filename);
	int import_off (char *filename);
	int export_off (char *filename);
	int import_pgm (char *filename);
	int import_pts (char *filename);
	int export_pts (char *filename);
	int import_ply (char *filename);
	int export_ply (char *filename);
	int import_stl (char *filename);
	int export_stl (char *filename);
	int import_u3d (char *filename);
	int export_u3d (char *filename);
public:
	int load (char *filename);
	int save (char *filename);

	// bbox
	int computebbox (void);
	int bbox (float min[3], float max[3]);
	float bbox_diagonal_length (void);

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

	// triangulation
	void triangulate_regular_heightfield (unsigned int width, unsigned int height);

	//
	// Materials
	//
	Material* GetMaterial (unsigned int id)
		{
			if (id < m_nMaterials)
				return m_pMaterials[id];
			return NULL;
		}
	int GetMaterialId (char *material_name)
		{
			for (unsigned int i=0; i<m_nMaterials; i++)
				if (strcmp (m_pMaterials[i]->GetName(), material_name) == 0)
					return i;
			return -1;
		}
	void SetMaterial (unsigned int id, Material *pMaterial)
	{
		if (m_pMaterials == NULL)
		{
			m_pMaterials = new Material*[id+1];
			m_nMaterials = id+1;
		}
		else if (id > m_nMaterials)
		{
			delete m_pMaterials;
			m_pMaterials = new Material*[id+1];
			m_nMaterials = id+1;
		}
		m_pMaterials[id] = pMaterial;
	};

	unsigned int Material_Add (Material *pMaterial)
	{
		Material **pMaterials = new Material*[m_nMaterials+1];
		if (pMaterials == NULL)
			return 0;
		
		for (unsigned int i=0; i<m_nMaterials; i++)
			pMaterials[i] = m_pMaterials[i];
		pMaterials[m_nMaterials] = pMaterial;
		delete[] m_pMaterials;
		m_pMaterials = pMaterials;
		m_nMaterials++;
		return m_nMaterials-1;
	};

	void ApplyMaterial (unsigned int id)
	{
		if (id > m_nMaterials)
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
	int Append (Mesh *m);

public:
	unsigned int m_nVertices;
	unsigned int m_nFaces;
	float *m_pVertices;
	float *m_pVertexNormals;
	float *m_pVertexColors;
	Face **m_pFaces;
	float *m_pFaceNormals;
	unsigned int m_nTextureCoordinates;
	float *m_pTextureCoordinates;
	unsigned int m_nMaterials;
	Material **m_pMaterials;
	Tensor **m_pTensors;

	float bbox_min[3], bbox_max[3];

	Octree *m_pOctree;
};

#endif // __MESH_H__
