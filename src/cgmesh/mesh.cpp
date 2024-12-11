#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	m_pVertices = NULL;
	m_bUseTextureCoordinates = false;
	m_pTextureCoordinatesIndices = NULL;
	m_pTextureCoordinates = NULL;
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
		m_pTextureCoordinates = NULL;

	m_iMaterialId = f.m_iMaterialId;
}

Face::~Face ()
{
	if (m_pVertices)
		delete m_pVertices;
	if (m_pTextureCoordinatesIndices)
		delete m_pTextureCoordinatesIndices;
	if (m_pTextureCoordinates)
		delete m_pTextureCoordinates;
};

int Face::SetNVertices (unsigned int n)
{
	if (m_nVertices == n)
		return 0;

	m_nVertices = n;
	if (m_pVertices)
		delete m_pVertices;
	m_pVertices = new unsigned int[m_nVertices];
/*
	if (m_pTextureCoordinatesIndices)
		delete m_pTextureCoordinatesIndices;
	m_pTextureCoordinatesIndices = new unsigned int[2*m_nVertices];

	if (m_pTextureCoordinates)
		delete m_pTextureCoordinates;
	m_pTextureCoordinates = new float[2*m_nVertices];
*/
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
	printf ("%d vertices stored in %xd : ", m_nVertices, m_pVertices);
	for (unsigned int i=0; i<m_nVertices; i++)
		printf ("%d ", m_pVertices[i]);
	printf ("\n");
}

//
// Mesh
//
void Mesh::Init ()
{
	m_nVertices = 0;
	m_nFaces = 0;
	m_pVertices = NULL;
	m_pVertexNormals = NULL;
	m_pVertexColors = NULL;
	m_pFaces = NULL;
	m_pFaceNormals = NULL;
	m_nMaterials = 0;
	m_pMaterials = NULL;
	m_nTextureCoordinates = 0;
	m_pTextureCoordinates = NULL;
	m_pTensors = NULL;
	m_pOctree = NULL;
}

void Mesh::InitVertexColors (float r, float g, float b)
{
	if (m_pVertexColors)
		delete m_pVertexColors;
	m_pVertexColors = NULL;

	if (m_nVertices > 0)
	{
		m_pVertexColors = new float[3*m_nVertices];
		for (int i=0; i<m_nVertices; i++)
		{
			m_pVertexColors[3*i]   = r;
			m_pVertexColors[3*i+1] = g;
			m_pVertexColors[3*i+2] = b;
		}
		//memset (m_pVertexColors, 0, 3*m_nVertices*sizeof(float));
	}
}

void Mesh::InitVertexColorsFromCurvatures (Tensor::eCurvature curvature)
{
	if (!m_pTensors)
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

	if (m_pVertexColors)
		delete m_pVertexColors;
	m_pVertexColors = NULL;

	if (m_nVertices > 0)
	{
		m_pVertexColors = new float[3*m_nVertices];

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
			m_pTextureCoordinates = new float[2 * m_nTextureCoordinates];

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
	m_pVertices = NULL;
	m_pVertexColors = NULL;
	m_pVertexNormals = NULL;

	m_nVertices = nVertices;
	if (m_nVertices)
	{
		m_pVertices = new float[3*nVertices];
		memset (m_pVertices, 0, 3*nVertices*sizeof(float));
		
		m_pVertexNormals = new float[3*nVertices];
		memset (m_pVertexNormals, 0, 3*nVertices*sizeof(float));
	}
}

void Mesh::InitFaces (unsigned int nFaces)
{
	m_pFaces = NULL;
	m_pFaceNormals = NULL;

	m_nFaces = nFaces;
	if (m_nFaces)
	{
		m_pFaces = new Face*[m_nFaces];
		//memset (m_pFaces, 0, nFaces*sizeof(Face*));
		for (int  i=0; i<m_nFaces; i++)
			m_pFaces[i] = new Face ();
		m_pFaceNormals = new float[3*m_nFaces];
		memset (m_pFaceNormals, 0, 3*m_nFaces*sizeof(float));
	}
}

void Mesh::InitTensors (void)
{
	m_pTensors = NULL;
	if (m_nVertices)
	{
		m_pTensors = new Tensor*[m_nVertices];
		memset (m_pTensors, 0, m_nVertices*sizeof(Tensor*));
	}
}

Tensor* Mesh::GetTensor (unsigned int index)
{
	return m_pTensors[index];
}

void Mesh::Init (unsigned int nVertices, unsigned int nFaces)
{
	Init ();

	InitVertices (nVertices);
	InitFaces (nFaces);

	m_nMaterials = 0;
	m_pMaterials = NULL;
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
		delete m_pFaces;
	}
	m_pFaces = NULL;
	m_nFaces = 0;
}

Mesh::~Mesh ()
{
	if (m_pVertices)	delete m_pVertices;
	if (m_pVertexNormals)	delete m_pVertexNormals;
	if (m_pVertexColors)	delete m_pVertexColors;
	DeleteFaces ();
	if (m_pMaterials) delete m_pMaterials;
	if (m_pOctree) delete m_pOctree;
}

void Mesh::Dump ()
{
	printf ("nVertices : %d\n", m_nVertices);
	printf ("pVertices : 0x%x\n", &m_pVertices);
	printf ("pVertexNormals : 0x%x\n", &m_pVertexNormals);
	printf ("pVertexColors : 0x%x\n", &m_pVertexColors);
	printf ("nFace : %d\n", m_nFaces);
	printf ("pFaces : 0x%x\n", &m_pFaces);
	printf ("pTextureCoordinates : 0x%x\n", &m_pTextureCoordinates);
	printf ("nMaterials : %d\n", m_nMaterials);
	printf ("pMaterials : 0x%x\n", &m_pMaterials);
	for (int i=0; i<m_nMaterials; i++)
		m_pMaterials[i]->Dump();
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
	if (!m_pVertices || (m_pVertices && nVertices != m_nVertices))
	{
		delete m_pVertices;
		m_pVertices = new float[3*nVertices];
	}
	m_nVertices = nVertices;
	memcpy (m_pVertices, pVertices, 3*nVertices*sizeof(float));

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
	if (!m_pVertices || i>=m_nVertices)
		return -1;
	
	m_pVertices[3*i+0] = x;
	m_pVertices[3*i+1] = y;
	m_pVertices[3*i+2] = z;
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
/*
	m_pFaces[i]->SetNVertices (4);
	m_pFaces[i]->m_pVertices[0] = a;
	m_pFaces[i]->m_pVertices[1] = b;
	m_pFaces[i]->m_pVertices[2] = c;
	m_pFaces[i]->m_pVertices[3] = d;

	return 0;
*/
}


int Mesh::computebbox (void)
{
	int i, j;
	if (m_nVertices == 0)
		return -1;

	for (j=0; j<3; j++)
	{
		bbox_min[j] = m_pVertices[j];
		bbox_max[j] = bbox_min[j];
	}
	for (int i=1; i<m_nVertices; i++)
	{
		for (j=0; j<3; j++)
		{
			if (m_pVertices[3*i+j] < bbox_min[j]) bbox_min[j] = m_pVertices[3*i+j];
			if (m_pVertices[3*i+j] > bbox_max[j]) bbox_max[j] = m_pVertices[3*i+j];
		}
	}
	
	return 0;
}

int Mesh::bbox (float min[3], float max[3])
{
	min[0] = bbox_min[0];
	min[1] = bbox_min[1];
	min[2] = bbox_min[2];
	max[0] = bbox_max[0];
	max[1] = bbox_max[1];
	max[2] = bbox_max[2];

	return 0;
}

float Mesh::bbox_diagonal_length (void)
{
	vec3 diagonal;
	vec3_init (diagonal, bbox_max[0]-bbox_min[0], bbox_max[1]-bbox_min[1], bbox_max[2]-bbox_min[2]);
	return vec3_length (diagonal);
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
		if (f == NULL)
			continue;
		if (f->GetNVertices() >= n)
			(verticesinfaces[n-1])++;
		else
			(verticesinfaces[f->GetNVertices()])++;
	}

	return 0;
}

//
//
//
void Mesh::ComputeNormals (void)
{
	if (!m_pVertexNormals)
		m_pVertexNormals = new float[3*m_nVertices];
	memset (m_pVertexNormals, 0, 3*m_nVertices*sizeof(float));

	//
	if (!m_pFaceNormals)
		m_pFaceNormals = new float[3*m_nFaces];
	memset (m_pFaceNormals, 0, 3*m_nFaces*sizeof(float));
	

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

int Mesh::Append (Mesh *m)
{
	if (!m)
		return 0;

	// vertices
	unsigned int res_nv = m_nVertices + m->m_nVertices;
	float *res_v = new float[3*res_nv];

	for (unsigned int i=0; i<3*m_nVertices; i++)
		res_v[i] = m_pVertices[i];

	for (unsigned int i=0; i<3*m->m_nVertices; i++)
		res_v[3*m_nVertices+i] = m->m_pVertices[i];

	delete m_pVertices;

	// faces
	unsigned int res_nf = m_nFaces + m->m_nFaces;
	Face **res_f = new Face*[res_nf];

	for (unsigned int i=0; i<m_nFaces; i++)
		res_f[i] = m_pFaces[i];

	for (unsigned int i=0; i<m->m_nFaces; i++)
	{
		res_f[m_nFaces+i] = new Face (*m->m_pFaces[i]);
		for (unsigned int j=0; j<res_f[m_nFaces+i]->m_nVertices; j++)
			res_f[m_nFaces+i]->m_pVertices[j] += m_nVertices;
		res_f[m_nFaces+i]->SetMaterialId(m_nMaterials + m->m_pFaces[i]->GetMaterialId());
	}

	// materials
	unsigned int res_nMaterials = m_nMaterials + m->m_nMaterials;
	for (unsigned int i=0; i<m->m_nMaterials; i++)
		Material_Add (m->m_pMaterials[i]);


	m_nVertices = res_nv;
	m_pVertices = res_v;

	m_nFaces = res_nf;
	m_pFaces = res_f;

	m_nMaterials = res_nMaterials;

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
			if (m_pVertexColors)
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
	if (!m_pVertexColors)
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
	pOctree->Build (m_pVertices, m_nVertices, 200, 20);
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
}

//
// from class Geometry
//
bool Mesh::GetIntersectionBboxWithRay (vec3 o, vec3 d)
{
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
		m_pOctree->BuildForTriangles (m_pVertices, m_nVertices,
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
	if (m_nMaterials == 0)
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

