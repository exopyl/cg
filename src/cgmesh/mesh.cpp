#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include "mesh.h"
#include "../cgmath/cgmath.h"
#include "../cgimg/cgimg.h"
#include "octree.h"

//
// Face
//
void Face::Init (void)
{
	m_nVertices = 0;
	m_pVertices = nullptr;
	m_bUseTextureCoordinates = false;
	m_pTextureCoordinatesIndices = nullptr;
	m_pTextureCoordinates = nullptr;
	m_iMaterialId = MATERIAL_NONE;
};

Face::Face ()
{
	Init ();
	SetNVertices (3);
}

Face::Face (const Face &f)
{
	Init ();
	SetNVertices(f.m_nVertices);

	m_nVertices = f.m_nVertices;
	memcpy (m_pVertices, f.m_pVertices, f.m_nVertices*sizeof(unsigned int));

	m_bUseTextureCoordinates = f.m_bUseTextureCoordinates;
	if (m_bUseTextureCoordinates)
	{
		m_pTextureCoordinatesIndices = new unsigned int[2*f.m_nVertices];
		memcpy (m_pTextureCoordinatesIndices, f.m_pTextureCoordinatesIndices, 2*m_nVertices*sizeof(float));

		m_pTextureCoordinates = new float[2*m_nVertices];
		memcpy (m_pTextureCoordinates, f.m_pTextureCoordinates, 2*m_nVertices*sizeof(float));
	}
	else
		m_pTextureCoordinates = nullptr;

	m_iMaterialId = f.m_iMaterialId;
}

Face::~Face ()
{
	if (m_pVertices)
		delete[] m_pVertices;
	if (m_pTextureCoordinatesIndices)
		delete[] m_pTextureCoordinatesIndices;
	if (m_pTextureCoordinates)
		delete[] m_pTextureCoordinates;
};

int Face::SetNVertices (unsigned int n)
{
	if (m_nVertices == n)
		return 0;

	m_nVertices = n;
	if (m_pVertices)
		delete[] m_pVertices;
	m_pVertices = new unsigned int[m_nVertices];

	if (m_pTextureCoordinatesIndices)
		delete[] m_pTextureCoordinatesIndices;
	m_pTextureCoordinatesIndices = new unsigned int[2*m_nVertices];

	if (m_pTextureCoordinates)
		delete[] m_pTextureCoordinates;
	m_pTextureCoordinates = new float[2*m_nVertices];

	return 0;
}

void Face::SetTriangle (unsigned int a, unsigned int b, unsigned int c)
{
	SetNVertices (3);
	m_pVertices[0] = a;
	m_pVertices[1] = b;
	m_pVertices[2] = c;
}

void Face::SetQuad (unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
	SetNVertices (4);
	m_pVertices[0] = a;
	m_pVertices[1] = b;
	m_pVertices[2] = c;
	m_pVertices[3] = d;
}

void Face::dump (void)
{
	printf ("%d vertices stored in %p : ", m_nVertices, (void*)m_pVertices);
	for (unsigned int i=0; i<m_nVertices; i++)
		printf ("%d ", m_pVertices[i]);
	printf ("\n");
}

//
// Mesh
//
void Mesh::Init ()
{
	m_name = std::string("#NoName#");
	m_nVertices = 0;
	m_nFaces = 0;
	m_pVertices.clear();
	m_pVertexNormals.clear();
	m_pVertexColors.clear();
	m_pFaces = nullptr;
	m_pFaceNormals.clear();
	m_pMaterials.clear();
	m_nTextureCoordinates = 0;
	m_pTextureCoordinates.clear();
	m_pTensors.clear();
	m_pOctree = nullptr;
	m_revision = 0;
}

void Mesh::InitVertexColors (float r, float g, float b)
{
	m_pVertexColors.clear();

	if (m_nVertices > 0)
	{
		m_pVertexColors.resize(3*m_nVertices);
		for (int i=0; i<m_nVertices; i++)
		{
			m_pVertexColors[3*i]   = r;
			m_pVertexColors[3*i+1] = g;
			m_pVertexColors[3*i+2] = b;
		}
	}
}

void Mesh::InitVertexColorsFromCurvatures (Tensor::eCurvature curvature)
{
	if (m_pTensors.empty())
		return;

	if (m_nVertices > 0)
	{
		float *array = (float*)malloc(m_nVertices*sizeof(float));
		char *defined = (char*)malloc(m_nVertices*sizeof(char));
		for (int i=0; i<m_nVertices; i++)
		{
			if (m_pTensors[i])
			{
				defined[i] = 1;
				float kappa1 = m_pTensors[i]->GetKappaMax ();
				float kappa2 = m_pTensors[i]->GetKappaMin ();
				switch (curvature)
				{
				case Tensor::CURVATURE_MAX:
					array[i] = kappa1;
					break;
				case Tensor::CURVATURE_MIN:
					array[i] = kappa2;
					break;
				case Tensor::CURVATURE_GAUSSIAN:
					array[i] = kappa1*kappa2;
					break;
				case Tensor::CURVATURE_MEAN:
					array[i] = (kappa1+kappa2)/2.0;
					break;
				default:
					printf ("type unknown\n");
					break;
				}
			}
			else
				defined[i] = 0;
		}

		InitVertexColorsFromArray (array, defined);

		free (defined);
		free (array);
	}
}

void Mesh::InitVertexColorsFromArray (float *array, char *defined)
{
	if (!array)
		return;

	m_pVertexColors.clear();

	if (m_nVertices > 0)
	{
		m_pVertexColors.resize(3*m_nVertices);

		float min = array[0];
		float max = array[0];
		for (int i=1; i<m_nVertices; i++)
		{
			if (defined && !defined[i])
				continue;

			if (min > array[i]) min = array[i];
			if (max < array[i]) max = array[i];
		}
		/*if (fabs(min) > fabs(max))
		{
			min = -fabs(min);
			max =  fabs(min);
		}
		else
		{
			min =  fabs(max);
			max = -fabs(max);
		}*/

		float r, g, b;
		if (min == max)
		{
			//color_jet(0.5, &r, &g, &b);
			InitVertexColors (0., 0., 0.);
		}
		else
		{
			m_nTextureCoordinates = m_nVertices;
			m_pTextureCoordinates.assign(2 * m_nTextureCoordinates, 0.0f);

			float invdenom = 1./(max-min);
			for (int i=0; i<m_nVertices; i++)
			{
				m_pTextureCoordinates[2 * i] = (array[i] - min)*invdenom;
				m_pTextureCoordinates[2 * i + 1] = .5f;

				if (defined && !defined[i])
				{
					m_pVertexColors[3*i]   = 0.;
					m_pVertexColors[3*i+1] = 0.;
					m_pVertexColors[3*i+2] = 0.;
				}
				color_jet((array[i]-min)*invdenom, &m_pVertexColors[3*i], &m_pVertexColors[3*i+1], &m_pVertexColors[3*i+2]);

			}

			
			for (int i = 0; i < m_nFaces; i++)
			{
				Face* pFace = m_pFaces[i];
				pFace->ActivateTextureCoordinatesIndices();
				pFace->InitTexCoord();
			}
		}
	}	
}

void Mesh::InitVertices (unsigned int nVertices)
{
	m_pVertices.clear();
	m_pVertexColors.clear();
	m_pVertexNormals.clear();

	m_nVertices = nVertices;
	if (m_nVertices)
	{
		m_pVertices.assign(3*nVertices, 0.0f);
		m_pVertexNormals.assign(3*nVertices, 0.0f);
		m_pVertexColors.assign(3*nVertices, 0.5f);
	}
}

void Mesh::InitFaces (unsigned int nFaces)
{
	m_pFaces = nullptr;
	m_pFaceNormals.clear();

	m_nFaces = nFaces;
	if (m_nFaces)
	{
		m_pFaces = new Face*[m_nFaces];
		for (int  i=0; i<m_nFaces; i++)
			m_pFaces[i] = new Face ();
		m_pFaceNormals.assign(3*m_nFaces, 0.0f);
	}
}

void Mesh::InitTensors (void)
{
	m_pTensors.clear();
	if (m_nVertices)
		m_pTensors.resize(m_nVertices); // unique_ptr default-constructs to nullptr
}

Tensor* Mesh::GetTensor (unsigned int index)
{
	if (index >= m_pTensors.size()) return nullptr;
	return m_pTensors[index].get();
}

void Mesh::Init (unsigned int nVertices, unsigned int nFaces)
{
	Init ();

	InitVertices (nVertices);
	InitFaces (nFaces);

	m_pMaterials.clear();

	IncrementRevision();
}

Mesh::Mesh () : Geometry()
{
	Init ();
}

Mesh::Mesh (unsigned int nVertices, unsigned int nFaces)
{
	Init (nVertices, nFaces);
}

Mesh::Mesh (Mesh &m)
{
}

void Mesh::DeleteFaces (void)
{
	if (m_pFaces)
	{
		for (unsigned int i=0; i<m_nFaces; i++)
		{
			Face *pFace = m_pFaces[i];
			delete pFace;
		}
		delete[] m_pFaces;
	}
	m_pFaces = nullptr;
	m_nFaces = 0;
}

Mesh::~Mesh ()
{
	// vector + unique_ptr members destruct themselves
	DeleteFaces ();
	if (m_pOctree) delete m_pOctree;
}

void Mesh::Dump ()
{
	printf ("nVertices : %d\n", m_nVertices);
	printf("pVertices : %p\n", (void*)m_pVertices.data());
	printf ("pVertexNormals : %p\n", (void*)m_pVertexNormals.data());
	printf("pVertexColors : %p\n", (void*)m_pVertexColors.data());
	printf ("nFace : %d\n", m_nFaces);
	printf ("pFaces : %p\n", (void*)m_pFaces);
	printf ("pTextureCoordinates : %p\n", (void*)m_pTextureCoordinates.data());
	printf ("nMaterials : %zu\n", m_pMaterials.size());
	for (const auto& mat : m_pMaterials)
		if (mat) mat->Dump();
}

uint64_t Mesh::GetRevision() const
{
	return m_revision;
}

void Mesh::IncrementRevision()
{ 
	m_revision++;
}

unsigned int* Mesh::GetTriangles (void)
{
	unsigned int *pFaces = (unsigned int*)malloc(3*m_nFaces*sizeof(unsigned int));
	for (unsigned int i=0; i<m_nFaces; i++)
	{
		pFaces[3*i]   = m_pFaces[i]->GetVertex(0);
		pFaces[3*i+1] = m_pFaces[i]->GetVertex(1);
		pFaces[3*i+2] = m_pFaces[i]->GetVertex(2);
	}
	return pFaces;
}

int Mesh::SetVertices (unsigned int nVertices, float *pVertices)
{
	m_nVertices = nVertices;
	m_pVertices.assign(pVertices, pVertices + 3*nVertices);
	return 0;
}

int Mesh::SetVertexNormals(unsigned int nVertexNormals, float* pVertexNormals)
{
	if (nVertexNormals != m_nVertices)
	{
		return -1;
	}

	m_pVertexNormals.assign(pVertexNormals, pVertexNormals + 3*nVertexNormals);

	return 0;

}

int Mesh::SetFaces (unsigned int nFaces, unsigned int nVerticesPerFace, unsigned int *pFaces, unsigned int *pTextureCoordinates)
{
	if (m_pFaces) delete m_pFaces;
	m_pFaces = new Face*[nFaces];
	m_nFaces = nFaces;
	for (unsigned int i=0; i<nFaces; i++)
	{
		Face *pFace = new Face ();
		pFace->SetNVertices (nVerticesPerFace);
		for (unsigned int j=0; j<nVerticesPerFace; j++)
			pFace->m_pVertices[j] = pFaces[nVerticesPerFace*i+j];

		m_pFaces[i] = pFace;
	}
	return 1;
}

int Mesh::SetVertex (unsigned int i, float x, float y, float z)
{
	if (m_pVertices.empty() || i>=m_nVertices)
		return -1;

	m_pVertices[3*i+0] = x;
	m_pVertices[3*i+1] = y;
	m_pVertices[3*i+2] = z;

	return 0;
}

int Mesh::SetFace (unsigned int i,
		   unsigned int a, unsigned int b, unsigned int c)
{
	if (m_pFaces[i])
		delete m_pFaces[i];
	m_pFaces[i] = new Face ();
	m_pFaces[i]->SetNVertices (3);
	m_pFaces[i]->m_pVertices[0] = a;
	m_pFaces[i]->m_pVertices[1] = b;
	m_pFaces[i]->m_pVertices[2] = c;

	return 0;
}

int Mesh::SetFace (unsigned int i,
		   unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
	if (m_pFaces[i])
		delete m_pFaces[i];
	m_pFaces[i] = new Face ();
	m_pFaces[i]->SetQuad (a, b, c, d);
	return 0;
}


int Mesh::computebbox (void)
{
	/*
	for (int i = 0; i < m_nFaces; i++)
	{
		auto pFace = m_pFaces[i];
		for (int j = 0; j < pFace->GetNVertices(); j++)
		{
			int vindex = pFace->GetVertex(j);
			m_bbox.AddPoint(m_pVertices[3 * vindex], m_pVertices[3 * vindex + 1], m_pVertices[3 * vindex + 2]);
		}
	}
	*/

	for (int i = 0; i < m_nVertices; i++)
		m_bbox.AddPoint(m_pVertices[3 * i], m_pVertices[3 * i + 1], m_pVertices[3 * i + 2]);
	
	return 0;
}

const BoundingBox& Mesh::bbox() const
{
	return m_bbox;
}

float Mesh::bbox_diagonal_length(void) const
{
	return m_bbox.GetDiagonalLength();
}

float Mesh::GetLargestLength(void) const
{
	return m_bbox.GetLargestLength();
}

//
// area
//
float Mesh::GetFaceArea (unsigned int fi)
{
	unsigned int vi1 = 3*m_pFaces[fi]->m_pVertices[0];
	unsigned int vi2 = 3*m_pFaces[fi]->m_pVertices[1];
	unsigned int vi3 = 3*m_pFaces[fi]->m_pVertices[2];
	vec3 v1, v2, v3;

	vec3_init (v1, m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2]);
	vec3_init (v2, m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2]);
	vec3_init (v3, m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
	
	return vec3_triangle_area (v1, v2, v3);
}

float Mesh::GetArea (void)
{
	float area = 0.;
	for (unsigned int i=0; i<m_nFaces; i++)
		area += GetFaceArea(i);
	return area;
}

float* Mesh::GetAreas (void)
{
	float *areas = (float*)malloc(m_nFaces*sizeof(float));
	for (unsigned int i=0; i<m_nFaces; i++)
		areas[i] = GetFaceArea(i);
	return areas;
}

float* Mesh::GetCumulativeAreas (void)
{
	float *areas = GetAreas ();
	for (unsigned int i=1; i<m_nFaces; i++)
		areas[i] += areas[i-1];
	return areas;
}

int Mesh::stats_vertices_in_faces (int *verticesinfaces, int n)
{	
	memset (verticesinfaces, 0, n*sizeof(int));
	for (int i=0; i<m_nFaces; i++)
	{
		Face *f = m_pFaces[i];
		if (f == nullptr)
			continue;
		if (f->GetNVertices() >= n)
			(verticesinfaces[n-1])++;
		else
			(verticesinfaces[f->GetNVertices()])++;
	}

	return 0;
}

unsigned int Mesh::GetNVertices() const
{
	return m_nVertices;
}

unsigned int Mesh::GetNFaces() const
{
	return m_nFaces;
}

bool Mesh::IsTriangleMesh() const
{
	for (int i = 0; i < m_nFaces; i++)
	{
		Face* pFace = m_pFaces[i];
		if (pFace->GetNVertices() != 3)
			return false;
	}
	return true;
}

//
//
//
void Mesh::ComputeNormals (void)
{
	m_pVertexNormals.assign(3*m_nVertices, 0.0f);
	m_pFaceNormals.assign(3*m_nFaces, 0.0f);
	

	//
	int *nfaces = new int[m_nVertices];
	memset (nfaces, 0, m_nVertices*sizeof(int));
	
	for (int i=0; i<m_nFaces; i++)
	{
		Face *pFace = m_pFaces[i];
		int k1 = pFace->m_pVertices[0];
		int k2 = pFace->m_pVertices[1];
		int k3 = pFace->m_pVertices[2];
		vec3 p1p2, p1p3, n;
		vec3_init (p1p2,
			   m_pVertices[3*k2]   - m_pVertices[3*k1],
			   m_pVertices[3*k2+1] - m_pVertices[3*k1+1],
			   m_pVertices[3*k2+2] - m_pVertices[3*k1+2]);
		vec3_init (p1p3,
			   m_pVertices[3*k3]   - m_pVertices[3*k1],
			   m_pVertices[3*k3+1] - m_pVertices[3*k1+1],
			   m_pVertices[3*k3+2] - m_pVertices[3*k1+2]);
		vec3_cross_product (n, p1p2, p1p3);
		vec3_normalize (n);
		m_pFaceNormals[3*i]   = n[0];
		m_pFaceNormals[3*i+1] = n[1];
		m_pFaceNormals[3*i+2] = n[2];
		
		for (int j=0; j<pFace->m_nVertices; j++)
		{
			int k = pFace->m_pVertices[j];
			
			m_pVertexNormals[3*k]   += m_pFaceNormals[3*i];
			m_pVertexNormals[3*k+1] += m_pFaceNormals[3*i+1];
			m_pVertexNormals[3*k+2] += m_pFaceNormals[3*i+2];
			
			nfaces[k]++;
		}
	}

	for (int i=0; i<m_nVertices; i++)
	{
		m_pVertexNormals[3*i]   /= nfaces[i];
		m_pVertexNormals[3*i+1] /= nfaces[i];
		m_pVertexNormals[3*i+2] /= nfaces[i];
	}

	// cleaning
	delete[] nfaces;
}

unsigned int Mesh::CountEdges (void)
{
	std::set<std::pair<unsigned int, unsigned int>> edges;
	for (unsigned int f = 0; f < m_nFaces; f++)
	{
		unsigned int nv = m_pFaces[f]->GetNVertices();
		for (unsigned int e = 0; e < nv; e++)
		{
			unsigned int a = m_pFaces[f]->GetVertex(e);
			unsigned int b = m_pFaces[f]->GetVertex((e + 1) % nv);
			if (a > b) std::swap(a, b);
			edges.insert({a, b});
		}
	}
	return (unsigned int)edges.size();
}

int Mesh::Append (Mesh *m)
{
	if (!m)
		return 0;

	// vertices
	unsigned int res_nv = m_nVertices + m->m_nVertices;
	m_pVertices.insert(m_pVertices.end(),
	                   m->m_pVertices.begin(),
	                   m->m_pVertices.begin() + 3 * m->m_nVertices);

	// faces
	unsigned int res_nf = m_nFaces + m->m_nFaces;
	Face **res_f = new Face*[res_nf];

	for (unsigned int i=0; i<m_nFaces; i++)
		res_f[i] = m_pFaces[i];

	const unsigned int matOffset = (unsigned int)m_pMaterials.size();

	for (unsigned int i=0; i<m->m_nFaces; i++)
	{
		res_f[m_nFaces+i] = new Face (*m->m_pFaces[i]);
		for (unsigned int j=0; j<res_f[m_nFaces+i]->m_nVertices; j++)
			res_f[m_nFaces+i]->m_pVertices[j] += m_nVertices;
		res_f[m_nFaces+i]->SetMaterialId(matOffset + m->m_pFaces[i]->GetMaterialId());
	}

	// Materials: transfer ownership from `m` to `this`. After Append, `m`'s
	// material array is emptied (it no longer owns them). This is a
	// behavior change from the previous raw-pointer code which shared
	// pointers between the two meshes — a guaranteed double-free at
	// destruction. Append currently has no callers so the swap is safe.
	for (auto& mat : m->m_pMaterials)
		m_pMaterials.push_back(std::move(mat));
	m->m_pMaterials.clear();

	m_nVertices = res_nv;

	m_nFaces = res_nf;
	m_pFaces = res_f;

	return 1;
}

//
// edit
//
int Mesh::DeleteVertices (funcptr_v func)
{
	unsigned int nVertices = 0;
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		float x = m_pVertices[3*i];
		float y = m_pVertices[3*i+1];
		float z = m_pVertices[3*i+2];
		if (!func (x, y, z))
		{
			m_pVertices[3*nVertices]   = m_pVertices[3*i];
			m_pVertices[3*nVertices+1] = m_pVertices[3*i+1];
			m_pVertices[3*nVertices+2] = m_pVertices[3*i+2];

			m_pVertexNormals[3*nVertices]   = m_pVertexNormals[3*i];
			m_pVertexNormals[3*nVertices+1] = m_pVertexNormals[3*i+1];
			m_pVertexNormals[3*nVertices+2] = m_pVertexNormals[3*i+2];
			if (!m_pVertexColors.empty())
			{
				m_pVertexColors[3*nVertices]   = m_pVertexColors[3*i];
				m_pVertexColors[3*nVertices+1] = m_pVertexColors[3*i+1];
				m_pVertexColors[3*nVertices+2] = m_pVertexColors[3*i+2];
			}

			nVertices++;
		}
	}
	m_nVertices = nVertices;
	return 0;
}

/*
bool Mesh::ColorizeVerticesDensity_Traverse (Octree &o, void *_data)
{
	if (o.m_nIndices)
	{
		float *data = (float*)_data;
		vec3 v0, v;
		vec3_init (v0, data[0], data[1], data[2]);
		float k = data[3];
		for (int i=0; i<o.m_nIndices; i++)
		{
			GetVertex (i, v);
			if (vec3_distance (v0, v) < k)
				data[4+i]++;
		}
		
	}
	return true;
}
*/
// Treatments on vertices
int Mesh::ColorizeVerticesDensity (float k)
{
	if (m_pVertexColors.empty())
		InitVertexColors (m_nVertices);

	unsigned int *neighbours = (unsigned int*)malloc(m_nVertices*sizeof(unsigned int));
	//unsigned int neighbours[m_nVertices];
	memset (neighbours, 0, m_nVertices*sizeof(unsigned int));
/*
	vec3 v, vtmp;
	for (int i=0; i<m_nVertices; i++)
	{
		printf ("%d / %d\n", i, m_nVertices);
		GetVertex (i, v);
		for (int j=0; j<m_nVertices; j++)
		{
			GetVertex (j, vtmp);
			if (vec3_distance (v, vtmp) < k)
				neighbours[i]++;
		}
	}
*/

	Octree *pOctree = new Octree ();
	pOctree->Build (m_pVertices.data(), m_nVertices, 200, 20);
	vec3 pt;
	for (int i=0; i<m_nVertices; i++)
	{
		//printf ("%d %d\n", i, m_nVertices);
		GetVertex (i, pt);
		neighbours[i] = pOctree->GetKNeighbours (pt, k);
	}

	unsigned int max = 0;
	for (int i=0; i<m_nVertices; i++)
		if (neighbours[i] > max)
			max = neighbours[i];

	for (int i=0; i<m_nVertices; i++)
	{

		color_jet ((float)neighbours[i]/max,
			   &m_pVertexColors[3*i], &m_pVertexColors[3*i+1], &m_pVertexColors[3*i+2]);
		
		//m_pVertexColors[3*i]   = (float)neighbours[i]/max;
		//m_pVertexColors[3*i+1] = (float)neighbours[i]/max;
		//m_pVertexColors[3*i+2] = (float)neighbours[i]/max;
	}

	// cleaning
	delete pOctree;
	free (neighbours);

	return 0;
}

// noise
void Mesh::add_gaussian_noise (float variance)
{
	for (int i=0; i<m_nVertices; i++)
	{
		static long idum = -247;
		vec3 disp;
		vec3_init (disp, gasdev(&idum), gasdev(&idum), gasdev(&idum));
		m_pVertices[3*i]   += variance*disp[0];
		m_pVertices[3*i+1] += variance*disp[1];
		m_pVertices[3*i+2] += variance*disp[2];
	}
	IncrementRevision();
}

// topology
void Mesh::GetTopologicIssues(vector<unsigned int>& nonManifoldEdges, vector<unsigned int>& borders) const
{
	nonManifoldEdges.clear();
	borders.clear();

	std::map<unsigned int, std::map<unsigned int, unsigned int>> occurences;
	for (int i = 0; i < m_nFaces; i++)
	{
		Face* pFace = m_pFaces[i];
		unsigned int nVertices = pFace->GetNVertices();
		for (int j = 0; j < nVertices; j++)
		{
			unsigned int v1 = pFace->GetVertex(j);
			unsigned int v2 = pFace->GetVertex((j + 1) % nVertices);
			if (v1 > v2) // v1 should be the smallest index
			{
				unsigned int tmp = v1;
				v1 = v2;
				v2 = tmp;
			}
			auto it = occurences.find(v1);
			if (it == occurences.end())
			{
				std::map<unsigned int, unsigned int> newOccurence;
				newOccurence.insert(std::pair<unsigned int, unsigned int>(v2, 1));
				occurences.insert(std::pair<unsigned int, std::map<unsigned int, unsigned int>>(v1, newOccurence));
			}
			else
			{
				auto it2 = it->second.find(v2);
				if (it2 != it->second.end())
				{
					//if (std::find(it->second.begin(), it->second.end(), v2) != it->second.end())
					it2->second++;
				}
				else
				{
					it->second.insert(std::pair<unsigned int, unsigned int>(v2, 1));
				}
			}
		}
	}

	for (auto occurence : occurences)
	{
		auto v1 = occurence.first;
		for (auto links : occurence.second)
		{
			auto v2 = links.first;
			auto n = links.second;
			if (n == 1)
			{
				borders.push_back(v1);
				borders.push_back(v2);
			}
			else if (n >= 3)
			{
				nonManifoldEdges.push_back(v1);
				nonManifoldEdges.push_back(v2);
			}
		}
	}

	occurences.clear();
}

//
// from class Geometry
//
bool Mesh::GetIntersectionBboxWithRay (vec3 o, vec3 d)
{
	float bbox_min[3];
	float bbox_max[3];
	m_bbox.GetMinMax(bbox_min, bbox_max);
	AABox *m_pAABox = new AABox (bbox_min[0], bbox_min[1], bbox_min[2]);
	if (m_pAABox)
	{
		m_pAABox->AddVertex (bbox_max[0], bbox_max[1], bbox_max[2]);
		Ray r (Vector3 (o[0], o[1], o[2]), Vector3 (d[0], d[1], d[2]));
		return m_pAABox->intersection (r, 0., 100.);
	}
	else
		return true;
}

int Mesh::GetIntersectionWithRayInOctree (vec3 vOrig, vec3 vDirection, float *_t, vec3 vIntersection, vec3 vNormal, Octree *pOctree)
{
	if (pOctree->IsLeaf ())
	{
		unsigned int nTriangles = pOctree->GetNTriangles();
		unsigned int *pTriangles = pOctree->GetTriangles();
		float fTCurrent, fT = -1.;
		Triangle tri;
		for (unsigned int i=0; i<nTriangles; i++)
		{
			float vIntersectionCurrent[3], vNormalCurrent[3];

			unsigned int vi1, vi2, vi3;
			vi1 = 3*pTriangles[3*i];
			vi2 = 3*pTriangles[3*i+1];
			vi3 = 3*pTriangles[3*i+2];
			tri.Init (m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2],
						m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2],
						m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
			if (tri.GetIntersectionWithRay (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent))
			{
				if (fT < 0. || fTCurrent < fT)
				{
					fT = fTCurrent;
					vec3_copy (vIntersection, vIntersectionCurrent);
					vec3_copy (vNormal, vNormalCurrent);
				}
			}
		}

		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}
	else
	{
		Ray ray (vOrig[0], vOrig[1], vOrig[2], vDirection[0], vDirection[1], vDirection[2]);
		float fTCurrent, fT = -1.;
		for (int i=0; i<8; i++)
		{
			Octree *pChild = pOctree->GetChildren()[i];
			if (!pChild)
				continue;
			AABox AABox(pChild->m_vecMin[0], pChild->m_vecMin[1], pChild->m_vecMin[2]);
			AABox.AddVertex (pChild->m_vecMax[0], pChild->m_vecMax[1], pChild->m_vecMax[2]);
			if (AABox.intersection (ray, 0., 10000.))
			{
				vec3 vIntersectionCurrent, vNormalCurrent; 
				if (GetIntersectionWithRayInOctree (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent, pChild))
				{
					if (fT < 0. || fTCurrent < fT)
					{
						fT = fTCurrent;
						vec3_copy (vIntersection, vIntersectionCurrent);
						vec3_copy (vNormal, vNormalCurrent);
					}
				}
			}
		}
		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}
}

int Mesh::GetIntersectionWithRay (vec3 vOrig, vec3 vDirection, float *_t, vec3 vIntersection, vec3 vNormal)
{
	/*
	// test the bbox
	bool bGotBBox = GetIntersectionBboxWithRay (vOrig, vDirection);
	if (bGotBBox == false)
		return 0;
	*/

	if (1 && !m_pOctree)
	{
		m_pOctree = new Octree ();
		m_pOctree->BuildForTriangles (m_pVertices.data(), m_nVertices,
									  100, // maxTriangles
									  5,  // maxDepth
								      GetTriangles (), m_nFaces);
	}

	if (m_pOctree)
	{
		return GetIntersectionWithRayInOctree (vOrig, vDirection, _t, vIntersection, vNormal, m_pOctree);
	}
	else // test alls the triangles
	{
		float fTCurrent, fT = -1.;
		Triangle *pTri = new Triangle ();
		for (unsigned int i=0; i<m_nFaces; i++)
		{
			float vIntersectionCurrent[3], vNormalCurrent[3];

			unsigned int vi1, vi2, vi3;
			vi1 = 3*m_pFaces[i]->GetVertex (0);
			vi2 = 3*m_pFaces[i]->GetVertex (1);
			vi3 = 3*m_pFaces[i]->GetVertex (2);
			pTri->Init (m_pVertices[vi1], m_pVertices[vi1+1], m_pVertices[vi1+2],
						m_pVertices[vi2], m_pVertices[vi2+1], m_pVertices[vi2+2],
						m_pVertices[vi3], m_pVertices[vi3+1], m_pVertices[vi3+2]);
			if (pTri->GetIntersectionWithRay (vOrig, vDirection, &fTCurrent, vIntersectionCurrent, vNormalCurrent))
			{
				if (fT < 0. || fTCurrent < fT)
				{
					fT = fTCurrent;
					vec3_copy (vIntersection, vIntersectionCurrent);
					vec3_copy (vNormal, vNormalCurrent);
				}
			}
		}
		delete pTri;
		*_t = fT;
		return (fT < 0.)? 0 : 1;
	}
}

int Mesh::GetIntersectionWithSegment (vec3 vStart, vec3 vEnd, float *_t, vec3 i, vec3 n)
{
	return 0;
}

void* Mesh::GetMaterial (void)
{
	if (m_pMaterials.empty())
	{
		// add a default material
		MaterialColorExt *pMaterial;
		pMaterial = new MaterialColorExt();
		pMaterial->Init_From_Library (MaterialColorExt::EMERALD);
		unsigned int id = Material_Add (pMaterial);
		ApplyMaterial (id);
	}
	return GetMaterial (0);
}

//
// MergeVertices
// Merge vertices that are closer than the given tolerance.
// Face indices are remapped accordingly. Degenerate faces are removed.
//
// Implementation : spatial hash grid of cell size = tolerance. Each vertex is
// checked only against vertices in the 3x3x3 = 27 neighbouring cells, giving
// average-case O(N) time instead of the naive O(N^2).
//
// If tolerance <= 0, the function only merges exact duplicates (cell size set
// to a tiny epsilon so each unique coord triple goes in its own cell).
//
int Mesh::MergeVertices (float tolerance)
{
	if (m_nVertices == 0)
		return 0;

	unsigned int *remap = new unsigned int[m_nVertices];
	float *newVertices = new float[3 * m_nVertices];
	unsigned int nNewVertices = 0;
	const float tol  = std::fabs(tolerance);
	const float tol2 = tol * tol;
	// Cell size : tolerance (or a tiny epsilon when tolerance == 0 so we still
	// catch exact duplicates without exploding the grid).
	const float cell = (tol > 0.0f) ? tol : 1e-12f;
	const float invCell = 1.0f / cell;

	struct CellKey { int x, y, z; };
	struct CellKeyHash {
		size_t operator() (const CellKey &k) const noexcept {
			// Mix 3 ints into a 64-bit hash. Splitmix-style.
			uint64_t h = (uint64_t)(uint32_t)k.x;
			h ^= ((uint64_t)(uint32_t)k.y + 0x9E3779B97F4A7C15ULL + (h<<6) + (h>>2));
			h ^= ((uint64_t)(uint32_t)k.z + 0x9E3779B97F4A7C15ULL + (h<<6) + (h>>2));
			return (size_t)h;
		}
	};
	struct CellKeyEq {
		bool operator() (const CellKey &a, const CellKey &b) const noexcept {
			return a.x == b.x && a.y == b.y && a.z == b.z;
		}
	};
	std::unordered_map<CellKey, std::vector<unsigned int>, CellKeyHash, CellKeyEq> grid;
	grid.reserve(m_nVertices);

	for (unsigned int i = 0; i < m_nVertices; i++)
	{
		const float xi = m_pVertices[3 * i];
		const float yi = m_pVertices[3 * i + 1];
		const float zi = m_pVertices[3 * i + 2];

		const int cx = (int)std::floor(xi * invCell);
		const int cy = (int)std::floor(yi * invCell);
		const int cz = (int)std::floor(zi * invCell);

		// Probe the 3x3x3 neighbourhood. A vertex within tolerance can fall
		// in an adjacent cell when sitting close to a cell boundary.
		bool found = false;
		unsigned int hit = 0;
		for (int dz = -1; dz <= 1 && !found; ++dz)
		for (int dy = -1; dy <= 1 && !found; ++dy)
		for (int dx = -1; dx <= 1 && !found; ++dx)
		{
			CellKey k { cx + dx, cy + dy, cz + dz };
			auto it = grid.find(k);
			if (it == grid.end()) continue;
			for (unsigned int j : it->second)
			{
				float ex = newVertices[3 * j]     - xi;
				float ey = newVertices[3 * j + 1] - yi;
				float ez = newVertices[3 * j + 2] - zi;
				if (ex * ex + ey * ey + ez * ez <= tol2)
				{
					hit = j;
					found = true;
					break;
				}
			}
		}

		if (found)
		{
			remap[i] = hit;
		}
		else
		{
			newVertices[3 * nNewVertices]     = xi;
			newVertices[3 * nNewVertices + 1] = yi;
			newVertices[3 * nNewVertices + 2] = zi;
			remap[i] = nNewVertices;
			grid[CellKey{cx, cy, cz}].push_back(nNewVertices);
			nNewVertices++;
		}
	}

	// Replace vertex array
	m_pVertices.assign(newVertices, newVertices + 3 * nNewVertices);
	delete[] newVertices;

	// Rebuild vertex normals
	if (!m_pVertexNormals.empty())
	{
		m_pVertexNormals.assign(3 * nNewVertices, 0.0f);
	}

	// Rebuild vertex colors
	if (!m_pVertexColors.empty())
	{
		std::vector<float> newColors(3 * nNewVertices, 0.0f);
		// Copy color from first occurrence
		for (unsigned int i = 0; i < m_nVertices; i++)
		{
			unsigned int ni = remap[i];
			newColors[3 * ni]     = m_pVertexColors[3 * i];
			newColors[3 * ni + 1] = m_pVertexColors[3 * i + 1];
			newColors[3 * ni + 2] = m_pVertexColors[3 * i + 2];
		}
		m_pVertexColors = std::move(newColors);
	}

	unsigned int nOldVertices = m_nVertices;
	m_nVertices = nNewVertices;

	// Remap face indices and remove degenerate faces
	unsigned int nNewFaces = 0;
	for (unsigned int i = 0; i < m_nFaces; i++)
	{
		Face *f = m_pFaces[i];
		for (unsigned int v = 0; v < f->m_nVertices; v++)
			f->m_pVertices[v] = remap[f->m_pVertices[v]];

		// Check for degenerate face (duplicate vertex indices)
		bool degenerate = false;
		for (unsigned int a = 0; a < f->m_nVertices && !degenerate; a++)
			for (unsigned int b = a + 1; b < f->m_nVertices && !degenerate; b++)
				if (f->m_pVertices[a] == f->m_pVertices[b])
					degenerate = true;

		if (!degenerate)
		{
			m_pFaces[nNewFaces] = m_pFaces[i];
			nNewFaces++;
		}
		else
		{
			delete m_pFaces[i];
		}
	}
	m_nFaces = nNewFaces;

	delete[] remap;

	IncrementRevision();

	return (int)(nOldVertices - nNewVertices);
}

