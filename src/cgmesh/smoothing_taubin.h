#ifndef __SMOOTHING_TAUBIN_H__
#define __SMOOTHING_TAUBIN_H__

#include "mesh_half_edge.h"

//
//
//
class MeshAlgoSmoothingTaubin
{
public:
	MeshAlgoSmoothingTaubin () {};
	~MeshAlgoSmoothingTaubin () {};

	bool Apply (Mesh_half_edge *model, float lambda = 0.7, float mu = -0.7527);

private:
	bool ApplyCoefficient (Mesh_half_edge *model, float coeff);
};

#endif // __SMOOTHING_TAUBIN_H__
