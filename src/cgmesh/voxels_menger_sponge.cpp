#include "voxels_menger_sponge.h"

#include <math.h>

MengerSponge::MengerSponge (unsigned int level)
{
	if (level < 1 || level > 6)
		return;

	// deduce the size of the voxelization
	unsigned int n = pow (3.,(int)level);
	init (n, n, n);

	// compute the menger sponge in the voxelisation
	unsigned int lwalk, size;
	m_pVoxels[0][0][0].m_bActivated = true;
	for (lwalk=0; lwalk<level; lwalk++)
	{
		size = pow (3.,(int)lwalk);
		for (unsigned int i=0; i<size; i++)
			for (unsigned int j=0; j<size; j++)
				for (unsigned int k=0; k<size; k++)
				{
					// inferior part
					m_pVoxels[3*i][3*j][3*k].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+1][3*j][3*k].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j][3*k].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					m_pVoxels[3*i][3*j+1][3*k].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j+1][3*k].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					m_pVoxels[3*i][3*j+2][3*k].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+1][3*j+2][3*k].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j+2][3*k].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					// medium part
					m_pVoxels[3*i][3*j][3*k+1].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j][3*k+1].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					m_pVoxels[3*i][3*j+2][3*k+1].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j+2][3*k+1].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					// superior part
					m_pVoxels[3*i][3*j][3*k+2].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+1][3*j][3*k+2].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j][3*k+2].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					m_pVoxels[3*i][3*j+1][3*k+2].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j+1][3*k+2].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					
					m_pVoxels[3*i][3*j+2][3*k+2].m_bActivated   = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+1][3*j+2][3*k+2].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
					m_pVoxels[3*i+2][3*j+2][3*k+2].m_bActivated = m_pVoxels[i][j][k].m_bActivated;
				}
	}
	
}

MengerSponge::~MengerSponge ()
{
}

