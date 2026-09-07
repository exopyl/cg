#pragma once
// Ce que ce fichier utilise, et non le parapluie.
//
// Il incluait `cgmesh.h`, lequel l'inclut en retour : un CYCLE, que `#pragma
// once` rend silencieux mais qui impose a tout consommateur de ce fichier les 55
// inclusions du parapluie -- zlib, l'audio, l'ONNX comprises.
#include "mesh_half_edge.h"

class MeshAlgoSubdivisionKarbacher
{
public:
	MeshAlgoSubdivisionKarbacher () {};
	~MeshAlgoSubdivisionKarbacher () {};

	bool Apply (Mesh_half_edge *model);

private:
	void InitializePosition (Vector3d &par_pos, Vector3d &par_npos,
				 Vector3d par_v1, Vector3d par_v2, Vector3d par_v3,
				 Vector3d par_n1, Vector3d par_n2, Vector3d par_n3);
	void DeleteAngles (void);

	Mesh_half_edge *m_pModel;
};
