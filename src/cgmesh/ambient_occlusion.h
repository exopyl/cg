#pragma once
#include "mesh_half_edge.h"

class Context;

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

	// Rend un tableau de nVertices flottants ALLOUE PAR MALLOC, a rendre par
	// free(). Nul si Init n'a pas ete appele, ou si l'annulation est survenue.
	//
	// ctx optionnel : le jeton est teste dans la boucle SUR LES SOMMETS, non
	// dans celle sur les passes -- c'est la premiere qui porte le travail, et le
	// cas nominal ne fait qu'une passe. Sur annulation, la fonction rend nullptr
	// apres avoir libere ses structures : jamais un tableau a moitie rempli.
	float* Evaluate (int nPasses = 1, const Context *ctx = nullptr);

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
