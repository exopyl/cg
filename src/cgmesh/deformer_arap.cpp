#include "deformer_arap.h"

#ifdef USE_EIGEN

DeformerARAP::DeformerARAP()
{
	m_pMesh = NULL;
	isSolverReady = false;
}

DeformerARAP::~DeformerARAP()
{
	m_pMesh = NULL;
}

void DeformerARAP::SetMesh(Mesh_half_edge *pMesh)
{
	m_pMesh = pMesh;

	isFixed.clear();
	isFixed.resize(m_pMesh->m_nVertices, false);
}

void DeformerARAP::SetBordersAsFixed()
{
	for (int i=0; i<m_pMesh->m_nVertices; i++)
		if (m_pMesh->get_n_neighbours(i) == -1)
			SetFixed(i);
}

void DeformerARAP::ComputeCotangentWeights()
{
	int i, j, n;
	double wij = 0.;

	wij_weight.clear(); // clear the current map
	for (i=0; i<m_pMesh->m_nVertices; i++)
	{
		if (m_pMesh->get_n_neighbours(i) == -1) // topology not correct or vertex i is on the border
			continue;

		Che_edge *he = m_pMesh->get_edge_from_vertex(i);
		Che_edge *he_walk = he;
		do {
			j = he_walk->m_v_end;
			wij = 1.;//m_pMesh->cotangent_weight_formula(he_walk);
			wij_weight.insert(std::make_pair(std::make_pair(i,j), wij));

			he_walk = he_walk->m_he_next->m_he_next->m_pair;
		} while(he_walk != he);
	}
}

double DeformerARAP::GetWij(int i, int j)
{
	std::map< std::pair<int, int>, double >::iterator it = wij_weight.find(std::make_pair(i,j));
	assert (it != wij_weight.end());
	return it->second;
	return 1.;
}

void DeformerARAP::PreFactor()
{
	// Laplace-Beltrami operator : L matrix, n by n, weights
	Eigen::SparseMatrix<double> L(m_pMesh->m_nVertices, m_pMesh->m_nVertices);

	float v[3];
	for (int i=0; i<m_pMesh->m_nVertices; i++)
	{
		m_pMesh->GetVertex(i, v);
		m_OrigMesh[i] = Eigen::Vector3d(v[0],v[1],v[2]);

		double weight = 0.;

		if(!isFixed[i])
		{
			Che_edge *he = m_pMesh->get_edge_from_vertex(i);
			Che_edge *he_walk = he;
			do {
				int j = he_walk->m_v_end;
				double wij = GetWij(i,j);
				weight += wij;
				L.coeffRef(i,j) = -wij;

				he_walk = he_walk->m_he_next->m_he_next->m_pair;
			} while(he_walk != he);
		}
		else
			weight = 1.0;

		L.coeffRef(i,i) = weight;
	}
	
	Lt = L.transpose();
	solver.compute(Lt * L);
	isSolverReady = true;
}

void DeformerARAP::PreProcess()
{
	// reset data
	R.clear();
	R.resize(m_pMesh->m_nVertices, Eigen::Matrix3d::Identity());

	xyz.clear();
	xyz.resize(3, Eigen::VectorXd::Zero(m_pMesh->m_nVertices));

	b.clear();
	b.resize(3, Eigen::VectorXd::Zero(m_pMesh->m_nVertices));

	m_OrigMesh.clear();
	m_OrigMesh.resize(m_pMesh->m_nVertices, Eigen::Vector3d::Zero());

	// precompute cotangent weights (wij)
	ComputeCotangentWeights();

	// Pre-factor the system matrix of Equation (9)
	PreFactor();
}

void DeformerARAP::SVDRotation(void)
{
	Eigen::Matrix3d eye = Eigen::Matrix3d::Identity();

	for (int i=0; i<m_pMesh->m_nVertices; i++)
	{
		Che_edge *he = m_pMesh->get_edge_from_vertex(i);
		int valence = m_pMesh->get_n_neighbours(i);
		if (valence == -1)
			continue;

		int degree = 0;

		Eigen::MatrixXd P(3, valence), Q(3, valence);

		Che_edge *he_walk = he;
		do {
			// eij = pi - pj, including weights wij
			int j = he_walk->m_v_end;

			double wij = GetWij(i,j);
			P.col(degree) = (m_OrigMesh[i] - m_OrigMesh[j]) * wij;
			Q.col(degree++) = (Eigen::Vector3d(xyz[0][i], xyz[1][i], xyz[2][i]) - Eigen::Vector3d(xyz[0][j], xyz[1][j], xyz[2][j]));

			he_walk = he_walk->m_he_next->m_he_next->m_pair;
		} while (he != he_walk);

		// Compute the 3 by 3 covariance matrix:
		// actually S = (P * W * Q.t()); W is already considered in the previous step (P=P*W)
		Eigen::MatrixXd S = (P * Q.transpose());

		// Compute the singular value decomposition S = UDV.t
		Eigen::JacobiSVD<Eigen::MatrixXd> svd(S, Eigen::ComputeThinU | Eigen::ComputeThinV); // X = U * D * V.t()

		Eigen::MatrixXd V = svd.matrixV();
		Eigen::MatrixXd Ut = svd.matrixU().transpose();

		// V*U.t may be reflection (determinant = -1). in this case, we need to change the sign of 
		// column of U corresponding to the smallest singular value (3rd column)
		eye(2,2) = (V * Ut).determinant();

		R[i] = (V * eye * Ut); //Ri = (V * eye * U.t());
		assert(R[i].determinant()>0.);
	}
}

void DeformerARAP::Deform(int nIterations)
{
	if (!m_pMesh)
		return;

	if(!isSolverReady)
		PreProcess();

	// ARAP iteration
	for(int iter = 0; iter <= nIterations; iter++)
	{       
		// update vector b = wij/2 * (Ri+Rj) * (pi - pj), where pi and pj are coordinates of the original mesh
		// cf Equation (8)
		for (int i=0; i<m_pMesh->m_nVertices; i++)
		{
			vec3 v;
			m_pMesh->GetVertex(i, v);
			Eigen::Vector3d p (v[0], v[1], v[2]);

			if(!isFixed[i])
			{
				p = Eigen::Vector3d::Zero(); // Set to zero

				// visit the neighbors
				Che_edge *he = m_pMesh->get_edge_from_vertex(i);
				Che_edge *he_walk = he;
				do { 
					int j = he_walk->m_v_end;

					Eigen::Vector3d pij = m_OrigMesh[i] - m_OrigMesh[j];
					Eigen::Vector3d RijPijMat = ((R[i] + R[j]) * pij);

					double wij = GetWij(i,j);
					p += RijPijMat * (wij / 2.0);
					//p += ((R[i] + R[j]) * (m_OrigMesh[i] - m_OrigMesh[j])) * .5 * GetWij(i,j);

					he_walk = he_walk->m_he_next->m_he_next->m_pair;
				} while (he != he_walk);
			}

			// Set the right-hand side from Equation (8)
			for(int k = 0; k < 3; k++)
				b[k][i] = p[k];
		}

		// new positions p' are obtained by solving (9)
		for(int k = 0; k < 3; k++)
			xyz[k] = solver.solve(Lt * b[k]);

		// then further minimization is performed by re-computing local
		// rotations and using them to define a new right hand-side for the
		// linear system.
		// (if iter = 0, just means naive Laplacian Surface Editing (Ri is
		// identity matrix))
		if(iter > 0)
			SVDRotation();
	}

	// update vertex coordinates 
	for (int i=0; i<m_pMesh->m_nVertices; i++)
	{
		if (isFixed[i])
			continue;
		vec3 p;
		m_pMesh->GetVertex(i, p);
		printf ("%.3f %.3f %.3f -> %.3f %.3f %.3f\n", p[0], p[1], p[2], xyz[0][i], xyz[1][i], xyz[2][i]);
		m_pMesh->SetVertex(i, xyz[0][i], xyz[1][i], xyz[2][i]);
	}
}

#endif // USE_EIGEN