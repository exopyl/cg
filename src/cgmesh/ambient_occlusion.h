#pragma once
#include "mesh_half_edge.h"

//
// References :
// http://loulou.developpez.com/tutoriels/3d/ambient-occlusion/
// http://www.csee.umbc.edu/~olano/635f05/wc1.pdf
//
class MeshAlgoAmbientOcclusion
{
	friend class CallbackTraverse;
public:
	MeshAlgoAmbientOcclusion ();
	~MeshAlgoAmbientOcclusion ();

	bool Init (Mesh *mesh);
	float* Evaluate (int nPasses = 1);

private:
	Mesh *m_pMesh;

	class Patch
	{
	public:
		Patch ()
		{
			m_vPosition.Set (0., 0., 0.);
			m_vNormale.Set (0., 0., 0.);
			m_fArea = 0.;
			m_fOcclusion = 0.;
			m_fPrevOcclusion = 0.;
		}
		~Patch() {};

		Vector3f m_vPosition;
		Vector3f m_vNormale;
		float m_fArea;
		float m_fOcclusion;
		float m_fPrevOcclusion;
	};
	Patch *m_pPatches;

	float compute_occlusion (int index_receiver, int index_emitter);
	static float clampOcclusion (float);
};
