#ifndef __OCTREE_H__
#define __OCTREE_H__

#include "../cgmath/cgmath.h"

class Octree
{
	friend class Mesh;
public:
	Octree ();
	~Octree ();

	void GetCenter (float center[3]) const;
	void GetMinMax (float vecMin[3], float vecMax[3]) const;

	// Fill the octree with the points
	int Build (float *pPoints, int nPoints,
		   unsigned int maxPoints,
		   unsigned int maxDepth, unsigned int currentDepth = 0);


	// Same as former, but use an array of indices
	int BuildWithIndices (float *pPoints, int nPoints,
			      unsigned int maxPoints, unsigned int maxDepth,
			      unsigned int *pIndices = 0, int nIndices = 0, unsigned int currentDepth = 0);

	// Fill the octree with the triangles
	int BuildForTriangles (float *pPoints, int nPoints,
			      unsigned int maxTriangles, unsigned int maxDepth,
			      unsigned int *pTriangles, int nTriangles, unsigned int currentDepth = 0); // pTriangles contains indices

	Octree** GetChildren (void) { return m_pChildren; };
	int GetNLeaves (void);
	bool IsLeaf (void);
	int GetMaxDepth (void);

	// deal with coordinates
	unsigned int GetNPoints (void) { return m_nPoints; };
	float* GetPoints (void) { return m_pPoints; };
	int GetKNeighbours (vec3 pt, float distance);
	int GetSumNeighbours (vec3 pt, float distance, vec3 accum);
	int GetClosestPoints (vec3 pt, float distance, float **pNeighbours, unsigned int *nNeighbours);

	// deal with indices
	unsigned int GetNIndices (void) { return m_nIndices; };
	unsigned int* GetIndices (void) { return m_pIndices; };
	int GetClosestIndicesPoints (float *pVertices, vec3 pt, float distance, unsigned int **pNeighbours, unsigned int *nNeighbours);

	// deal with triangles
	unsigned int GetNTriangles (void) { return m_nTriangles; };
	unsigned int* GetTriangles (void) { return m_pTriangles; };

	void Dump (void);




	//
	// traverse
	//
	class Callback
	{
	public:
		virtual bool operator()(Octree *pOctree, void *data) = 0; // Return value: true = continue; false = abort
	};
	void traverse(Callback* callback, void *data)
	{
		if(callback)
			traverseRecursive(callback, this, data);
	}
	void traverseRecursive(Callback* callback, Octree *pOctree, void *data)
	{
		bool shouldContinue = callback->operator()(pOctree, data);
		if (!shouldContinue)
			return;
		for (int i=0; i<8; i++)
			if (pOctree->m_pChildren[i])
				traverseRecursive (callback, pOctree->m_pChildren[i], data);
	}    

private:
	void ComputeBounding (float *pPoints, int nPoints, float min[3], float max[3]);
	void ComputeBoundingWithIndices (float *pPoints, int nPoints, unsigned int *pIndices, int nIndices, float min[3], float max[3]);
	void ComputeBoundinForTriangles (float *pPoints, int nPoints, unsigned int *pTriangles, int nTriangles, float min[3], float max[3]);

	Octree *m_pFather;
	Octree *m_pChildren[8];
	
	// fill points
	unsigned int m_nPoints;
	float *m_pPoints;

	// fill points by using indices
	unsigned int m_nIndices;
	unsigned int *m_pIndices;

	// fill triangles
	unsigned int m_nTriangles;
	unsigned int *m_pTriangles;

	float m_vecMin[3];
	float m_vecMax[3];


};

/*
class OctreeIt
{
public:
	OctreeIt (const Octree *pOctree);
	~OctreeIt ();
	Octree* First ();
	Octree* Next ();
	bool Last ();
private:
	Octree *m_pOctree;
	Octree *m_pCurrentOctree;
};
*/

#endif // __OCTREE_H__
