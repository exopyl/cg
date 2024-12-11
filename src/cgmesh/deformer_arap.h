#ifndef __DEFORMER_ARAP_H__
#define __DEFORMER_ARAP_H__

//
// "As-Rigid-As-Possible Surface Modeling"
// Olga Sorkine, Marc Alexa, SGP 2007
// http://sites.fas.harvard.edu/~cs277/papers/sorkine_asrigid.pdf
//
// Adapted from
// https://code.google.com/p/3d-workspace/source/browse/trunk/MathLibrary/Deformer/ARAPDeformer.h
// https://code.google.com/p/3d-workspace/source/browse/trunk/MathLibrary/Deformer/ARAPDeformer.cpp
// &
// http://sourceforge.net/projects/meshtools/
// 

//#define USE_EIGEN
#ifdef USE_EIGEN


#include "mesh_half_edge.h"

#include <vector>
#include <map>

#include "../../extern/eigen-3.2.1/Core"
#include "../../extern/eigen-3.2.1/Sparse"
#include "../../extern/eigen-3.2.1/SVD"
#include "../../extern/eigen-3.2.1/Geometry"
//using namespace Eigen;

class DeformerARAP
{
public:
	DeformerARAP();
	~DeformerARAP();

	void SetMesh(Mesh_half_edge *pMesh);

	void SetBordersAsFixed();
    inline void SetFixed(int vi)
	{
		if (!m_pMesh)
			return;
        isFixed[vi] = true;
    }

	void PreProcess();

	void Deform(int nIterations=1);

private:
	Mesh_half_edge *m_pMesh;

	void ComputeCotangentWeights();
	double GetWij(int i, int j);
	void PreFactor();
	void SVDRotation();

private:
	std::vector<Eigen::Matrix3d> R;
	std::vector<Eigen::Vector3d> m_OrigMesh;
	std::vector<Eigen::VectorXd> xyz;
	std::vector<Eigen::VectorXd> b; // 
	std::vector<bool> isFixed;

	std::map< std::pair<int, int>, double > wij_weight;
	Eigen::SparseMatrix<double> Lt;
	Eigen::SimplicialLLT< Eigen::SparseMatrix<double> > solver;
	bool isSolverReady;
};

#endif// USE_EIGEN

#endif // __DEFORMER_ARAP_H__
