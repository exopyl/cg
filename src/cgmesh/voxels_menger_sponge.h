#pragma once
#include "voxels.h"

class MengerSponge : public Voxels
{
public:
	MengerSponge (unsigned int level);
	~MengerSponge ();
};
