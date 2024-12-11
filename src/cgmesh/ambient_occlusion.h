#ifndef __AMBIENT_OCCLUSION_H__
#define __AMBIENT_OCCLUSION_H__

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
			vec3_init (m_vPosition, 0., 0., 0.);
			vec3_init (m_vNormale, 0., 0., 0.);
			m_fArea = 0.;
			m_fOcclusion = 0.;
			m_fPrevOcclusion = 0.;
		}
		~Patch() {};

		vec3 m_vPosition;
		vec3 m_vNormale;
		float m_fArea;
		float m_fOcclusion;
		float m_fPrevOcclusion;
	};
	Patch *m_pPatches;

	float compute_occlusion (int index_receiver, int index_emitter);
	static float clampOcclusion (float);
};

#endif // __AMBIENT_OCCLUSION_H__
