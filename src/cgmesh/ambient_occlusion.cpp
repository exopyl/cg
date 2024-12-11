#include "ambient_occlusion.h"
#include "octree.h"

//
// Algo
//
MeshAlgoAmbientOcclusion::MeshAlgoAmbientOcclusion ()
{
	m_pMesh = NULL;
}

MeshAlgoAmbientOcclusion::~MeshAlgoAmbientOcclusion ()
{
}

bool MeshAlgoAmbientOcclusion::Init (Mesh *mesh)
{
	m_pMesh = mesh;
	return true;
}

float MeshAlgoAmbientOcclusion::clampOcclusion (float fOcclusion)
{
	//fOcclusion = exp(-fOcclusion);
	if (fOcclusion < 0.) fOcclusion = 0.;
	if (fOcclusion > 1.) fOcclusion = 1.;
	return fOcclusion;
}


//
// class to traverse the octree
//
class CallbackTraverse: public Octree::Callback
{
public:
	CallbackTraverse(int iCurrentPatch):m_iCurrentPatch(iCurrentPatch){};
	virtual bool operator()(Octree *pOctree, void *data)
	{
		MeshAlgoAmbientOcclusion *pAlgo = (MeshAlgoAmbientOcclusion*)data;
		MeshAlgoAmbientOcclusion::Patch &pPatch = pAlgo->m_pPatches[m_iCurrentPatch];

		float center[3], min[3], max[3];
		pOctree->GetMinMax (min, max);

		vec3 tmp;
		vec3 corners[8] = {
			{min[0], min[1], min[2]},
			{max[0], min[1], min[2]},
			{max[0], max[1], min[2]},
			{min[0], max[1], min[2]},
			{min[0], min[1], max[2]},
			{max[0], min[1], max[2]},
			{max[0], max[1], max[2]},
			{min[0], max[1], max[2]} };

		bool bCloseEnough = false;
		bool bNodeFacingRightHemisphere = false;

		// is the patch close enough with the octree node?
		if (pPatch.m_vPosition[0] > min[0] && pPatch.m_vPosition[0] < max[0] &&
			pPatch.m_vPosition[1] > min[1] && pPatch.m_vPosition[1] < max[1] &&
			pPatch.m_vPosition[2] > min[2] && pPatch.m_vPosition[2] < max[2]) // patch inside the octree node
			bCloseEnough = true;
		if (!bCloseEnough)
		{
			float fShortestDistance = 0.;
			for (int i=0; i<8; i++)
			{
				float fLength = vec3_distance (corners[i], pPatch.m_vPosition);
				if (i==0 || fLength < fShortestDistance)
					fShortestDistance = fLength;
			}
			if (fShortestDistance > m_fDistanceMax)
				return false;
		}

		// is the patch facing the octree node ?
		for (int i=0; i<8; i++)
		{
			vec3_subtraction (tmp, corners[i], pPatch.m_vPosition);
			if (!bNodeFacingRightHemisphere && vec3_dot_product (pPatch.m_vNormale, tmp) > 0.)
				bNodeFacingRightHemisphere = true;
		}
		if (!bNodeFacingRightHemisphere)
			return false;

		// treat the octree node
		if (!pOctree->IsLeaf ())
			return true; // let's visit the children

		// treat the patches contained in the leaf as potential occluders
		unsigned int nIndices = pOctree->GetNIndices();
		unsigned int *pIndices = pOctree->GetIndices();
		if (nIndices && pIndices)
		{
			for (int i=0; i<nIndices; i++)
				pAlgo->m_pPatches[m_iCurrentPatch].m_fOcclusion += pAlgo->compute_occlusion (m_iCurrentPatch,pIndices[i]);
		}

		return false;
	}

	int m_iCurrentPatch;
	float m_fDistanceMax;
	void SetMaxDistance(float fDistanceMax) { m_fDistanceMax = fDistanceMax; };
};

//
// compute occlusion on index_receiver due to index_occluder
//
float MeshAlgoAmbientOcclusion::compute_occlusion (int index_receiver, int index_occluder)
{
	if (index_receiver == index_occluder)
		return 0.;

	if (m_pPatches[index_occluder].m_fArea < 0.0000001) // for stability
		return 0.;

	// vector between the two points
	vec3 dir;
	vec3_subtraction (dir, m_pPatches[index_receiver].m_vPosition, m_pPatches[index_occluder].m_vPosition);

	// discard the patch if it is too far
	float dsquare = vec3_dot_product (dir, dir);
	if (dsquare > 100.*m_pPatches[index_occluder].m_fArea)
		return 0.;

	// form factor contribution
	vec3_normalize (dir);
	float CosR = -vec3_dot_product (dir, m_pPatches[index_receiver].m_vNormale);
	float CosE =  vec3_dot_product (dir, m_pPatches[index_occluder].m_vNormale);

	// discard the patch if the patches are not facing
	if (CosE < 0.)
		return 0.;

	float fOcclusion = CosE * CosR * m_pPatches[index_occluder].m_fArea * m_pPatches[index_occluder].m_fPrevOcclusion / (M_PI * dsquare + m_pPatches[index_occluder].m_fArea);
	return fOcclusion;
}

float* MeshAlgoAmbientOcclusion::Evaluate (int nPasses)
{
	if (!m_pMesh)
		return NULL;

	m_pMesh->ComputeNormals();
	unsigned int nVertices = m_pMesh->m_nVertices;
	float *pVertices = m_pMesh->m_pVertices;
	float *pVertexNormals =  m_pMesh->m_pVertexNormals;
	unsigned int nFaces = m_pMesh->m_nFaces;

	//
	// init the patches
	//
	m_pPatches = new Patch[nVertices];
	for (unsigned int i=0; i<nVertices; i++)
	{
		vec3_init (m_pPatches[i].m_vPosition, pVertices[3*i], pVertices[3*i+1], pVertices[3*i+2]);
		vec3_init (m_pPatches[i].m_vNormale, pVertexNormals[3*i], pVertexNormals[3*i+1], pVertexNormals[3*i+2]);
		m_pPatches[i].m_fArea = 0.;
		m_pPatches[i].m_fOcclusion = 0.;
		m_pPatches[i].m_fPrevOcclusion = 1.;
	}

	//
	// evaluate the occlusion contribution for each vertex
	//
	for (unsigned int i=0; i<nFaces; i++)
	{
		Face *pFace = m_pMesh->GetFace (i);
		if (!pFace)
			continue;

		float fArea = m_pMesh->GetFaceArea(i) / pFace->GetNVertices();
		for (unsigned int j=0; j<pFace->GetNVertices(); j++)
			m_pPatches[pFace->GetVertex(j)].m_fArea += fArea;
	}

	//
	// compute the occlusion for each vertex
	//

	// initialize an octree
	Octree *pOctree = new Octree ();
	pOctree->BuildWithIndices (m_pMesh->m_pVertices, m_pMesh->m_nVertices, 300 /* max elements per octree node */, 3 /* max depth */);
	float fMaxDistance = m_pMesh->bbox_diagonal_length()/10.;
	for (unsigned int iPass=0; iPass<nPasses; iPass++)
	{
		if (1) // traverse the octree (optimal)
		{
			for (unsigned int i=0; i<nVertices; i++)
			{
				m_pPatches[i].m_fOcclusion = 0.;
				if (m_pPatches[i].m_fArea < 0.0000001) // for stability
					continue;

				CallbackTraverse ct(i);
				ct.SetMaxDistance(fMaxDistance);
				pOctree->traverse (&ct, (void*)this);
				m_pPatches[i].m_fOcclusion = clampOcclusion(m_pPatches[i].m_fOcclusion);
			}
		}
		else // get nearest neighbours (less efficient)
		{
			for (unsigned int i=0; i<nVertices; i++)
			{
				vec3 tmp;
				m_pMesh->GetVertex (i, tmp);
				unsigned int *pClosestPoints = (unsigned int*)malloc(sizeof(unsigned int));
				unsigned int nClosestPoints = 0;
				//pOctree->GetClosestIndicesPoints (m_pMesh->m_pVertices, tmp, 10.*sqrt(m_pPatches[i].m_fArea), &pClosestPoints, &nClosestPoints);
				pOctree->GetClosestIndicesPoints (m_pMesh->m_pVertices, tmp, fMaxDistance, &pClosestPoints, &nClosestPoints);

				m_pPatches[i].m_fOcclusion = 0.;
				if (m_pPatches[i].m_fArea < 0.0000001) // for stability
					continue;

				//for (unsigned int j=0; j<nVertices; j++)
				for (unsigned int k=0; k<nClosestPoints; k++)
				{
					int j=pClosestPoints[k];
					m_pPatches[i].m_fOcclusion += compute_occlusion (i, j);
				}
				m_pPatches[i].m_fOcclusion = clampOcclusion(m_pPatches[i].m_fOcclusion);
			}
		}

		// store the previous occlusion
		for (unsigned int i=0; i<nVertices; i++)
			m_pPatches[i].m_fPrevOcclusion = m_pPatches[i].m_fOcclusion;
	}

	// format the output
	float *pOcclusions = (float*)malloc(nVertices*sizeof(float));
	for (unsigned int i=0; i<nVertices; i++)
		pOcclusions[i] = m_pPatches[i].m_fOcclusion;

	// cleaning
	delete pOctree;
	delete[] m_pPatches;
	m_pPatches = NULL;

	return pOcclusions;
}
