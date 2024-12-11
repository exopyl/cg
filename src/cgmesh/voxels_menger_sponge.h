#ifndef __VOXELS_MENGER_SPONGE_H__
#define __VOXELS_MENGER_SPONGE_H__

#include "voxels.h"

class MengerSponge : public Voxels
{
public:
	MengerSponge (unsigned int level);
	~MengerSponge ();
};

#endif // __VOXELS_MENGER_SPONGE_H__

