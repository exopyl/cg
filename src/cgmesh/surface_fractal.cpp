#include <math.h>
#include <string.h>

#include "mesh.h"


///////////////////////
//
// Fractals
//
///////////////////////

//
// ref : http://en.wikipedia.org/wiki/Menger_sponge
//
Mesh* CreateMengerSponge(unsigned int level)
{
	Mesh *mesh = NULL;

	if (level < 1 || level > 4)
		return NULL;

        /* create the voxelisation */
	char ***voxels = NULL;
	unsigned int i,j,k;
	float xmin, xmax, ymin, ymax, zmin, zmax;
	xmin = -10.0; xmax = 10.0; ymin = -10.0; ymax = 10.0; zmin = -10.0; zmax = 10.0;
	
	// deduce the size of the voxelization
	unsigned int nx = pow ((float)(3),(int)(level));
	unsigned int ny = pow ((float)(3),(int)(level));
	unsigned int nz = pow ((float)(3),(int)(level));
	
	// memory allocation
	voxels = (char***)malloc(nx*sizeof(char**));
	if (voxels == NULL)
		return NULL;

	for (i=0; i<nx; i++)
	{
		voxels[i] = (char**)malloc(ny*sizeof(char*));
		for (j=0; j<ny; j++)
		{
			voxels[i][j] = (char*)malloc(nz*sizeof(char));
			memset (voxels[i][j], 0, nz*sizeof(char));
		}
	}

	// compute the menger sponge in the voxelisation
	unsigned int lwalk, size;
	voxels[0][0][0] = 1;
	for (lwalk=0; lwalk<level; lwalk++)
	{
		size = pow ((float)(3),(int)(lwalk));
		for (i=0; i<size; i++)
			for (j=0; j<size; j++)
				for (k=0; k<size; k++)
				{
					// inferior part
					voxels[3*i][3*j][3*k]   = voxels[i][j][k];
					voxels[3*i+1][3*j][3*k] = voxels[i][j][k];
					voxels[3*i+2][3*j][3*k] = voxels[i][j][k];
					
					voxels[3*i][3*j+1][3*k]   = voxels[i][j][k];
					voxels[3*i+2][3*j+1][3*k] = voxels[i][j][k];
					
					voxels[3*i][3*j+2][3*k]   = voxels[i][j][k];
					voxels[3*i+1][3*j+2][3*k] = voxels[i][j][k];
					voxels[3*i+2][3*j+2][3*k] = voxels[i][j][k];
					
					// medium part
					voxels[3*i][3*j][3*k+1]   = voxels[i][j][k];
					voxels[3*i+2][3*j][3*k+1] = voxels[i][j][k];
					
					voxels[3*i][3*j+2][3*k+1]   = voxels[i][j][k];
					voxels[3*i+2][3*j+2][3*k+1] = voxels[i][j][k];
					
					// superior part
					voxels[3*i][3*j][3*k+2]   = voxels[i][j][k];
					voxels[3*i+1][3*j][3*k+2] = voxels[i][j][k];
					voxels[3*i+2][3*j][3*k+2] = voxels[i][j][k];
					
					voxels[3*i][3*j+1][3*k+2]   = voxels[i][j][k];
					voxels[3*i+2][3*j+1][3*k+2] = voxels[i][j][k];
					
					voxels[3*i][3*j+2][3*k+2]   = voxels[i][j][k];
					voxels[3*i+1][3*j+2][3*k+2] = voxels[i][j][k];
					voxels[3*i+2][3*j+2][3*k+2] = voxels[i][j][k];
				}
	}
	
	/* create the triangulation */
	unsigned int index, index1, index2, index3, index4;
	unsigned int nv = (nx+1)*(ny+1)*(nz+1);
	float *v = (float*)malloc(3*nv*sizeof(float));
	unsigned int nf = 3*nv;
	int *f = (int*)malloc(3*nf*sizeof(int));

	// vertices
	for (i=0; i<=nx; i++)
	{
		for (j=0; j<=ny; j++)
		{
			for (k=0; k<=nz; k++)
			{
				index = (nx+1)*(ny+1)*k + (ny+1)*j + i;
				v[3*index]   = xmin + i*(xmax-xmin)/nx;
				v[3*index+1] = ymin + j*(ymax-ymin)/ny;
				v[3*index+2] = zmin + k*(zmax-zmin)/nz;
			}
		}
	}

	
	// faces
	int fwalk = 0;
	for (i=0; i<nx; i++)
	{
		for (j=0; j<ny; j++)
		{
			for (k=0; k<nz; k++)
			{
				if (voxels[i][j][k])
				{
					if (i == 0 || !voxels[i-1][j][k])
					{
						index1 = (nx+1)*(ny+1)*k + (ny+1)*j + i;
						index2 = (nx+1)*(ny+1)*k + (ny+1)*(j+1) + i;
						index3 = (nx+1)*(ny+1)*(k+1) + (ny+1)*j + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (ny+1)*(j+1) + i;
						
						f[3*fwalk]   = index1;
						f[3*fwalk+1] = index3;
						f[3*fwalk+2] = index2;
						
						f[3*fwalk+3] = index2;
						f[3*fwalk+4] = index3;
						f[3*fwalk+5] = index4;
						
						fwalk += 2;
					}
					
					if (i==nx-1 || !voxels[i+1][j][k])
					{
						index1 = (nx+1)*(ny+1)*k + (ny+1)*j + i+1;
						index2 = (nx+1)*(ny+1)*k + (ny+1)*(j+1) + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (ny+1)*j + i+1;
						index4 = (nx+1)*(ny+1)*(k+1) + (ny+1)*(j+1) + i+1;
						
						f[3*fwalk]   = index1;
						f[3*fwalk+1] = index2;
						f[3*fwalk+2] = index3;
						
						f[3*fwalk+3] = index3;
						f[3*fwalk+4] = index2;
						f[3*fwalk+5] = index4;
						
						fwalk += 2;
					}
					
					if (j==0 || !voxels[i][j-1][k])
					{
						index1 = (nx+1)*(ny+1)*k + (ny+1)*j + i;
						index2 = (nx+1)*(ny+1)*k + (ny+1)*j + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (ny+1)*j + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (ny+1)*j + i+1;
						
						f[3*fwalk]   = index1;
						f[3*fwalk+1] = index2;
						f[3*fwalk+2] = index3;
						
						f[3*fwalk+3] = index3;
						f[3*fwalk+4] = index2;
						f[3*fwalk+5] = index4;
						
						fwalk += 2;
					}
					
					if (j==ny-1 || !voxels[i][j+1][k])
					{
						index1 = (nx+1)*(ny+1)*k + (ny+1)*(j+1) + i;
						index2 = (nx+1)*(ny+1)*k + (ny+1)*(j+1) + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (ny+1)*(j+1) + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (ny+1)*(j+1) + i+1;
						
						f[3*fwalk]   = index2;
						f[3*fwalk+1] = index1;
						f[3*fwalk+2] = index3;
						
						f[3*fwalk+3] = index2;
						f[3*fwalk+4] = index3;
						f[3*fwalk+5] = index4;
						
						fwalk += 2;
					}
					if (k==0 || !voxels[i][j][k-1])
					{
						index1 = (nx+1)*(ny+1)*k + (ny+1)*j + i;
						index2 = (nx+1)*(ny+1)*k + (ny+1)*j + i+1;
						index3 = (nx+1)*(ny+1)*k + (ny+1)*(j+1) + i;
						index4 = (nx+1)*(ny+1)*k + (ny+1)*(j+1) + i+1;
						
						f[3*fwalk]   = index1;
						f[3*fwalk+1] = index3;
						f[3*fwalk+2] = index2;
						
						f[3*fwalk+3] = index2;
						f[3*fwalk+4] = index3;
						f[3*fwalk+5] = index4;
						
						fwalk += 2;
					}
					
					if (k==nz-1 || !voxels[i][j][k+1])
					{
						index1 = (nx+1)*(ny+1)*(k+1) + (ny+1)*j + i;
						index2 = (nx+1)*(ny+1)*(k+1) + (ny+1)*j + i+1;
						index3 = (nx+1)*(ny+1)*(k+1) + (ny+1)*(j+1) + i;
						index4 = (nx+1)*(ny+1)*(k+1) + (ny+1)*(j+1) + i+1;
						
						f[3*fwalk]   = index1;
						f[3*fwalk+1] = index2;
						f[3*fwalk+2] = index3;
						
						f[3*fwalk+3] = index3;
						f[3*fwalk+4] = index2;
						f[3*fwalk+5] = index4;
						
						fwalk += 2;
					}
					
				}
			}
		}
	}

	nf = fwalk;

	// delete useless vertices
	char *v_used = (char*)malloc(nv*sizeof(char));
	v_used = (char*)memset((void*)v_used, 0, nv*sizeof(char));
	for (i=0; i<3*nf; i++) v_used[f[i]] = 1;
	
	int *new_indices = (int*)malloc(nv*sizeof(int));
	int iwalk = 0;
	for (i=0; i<nv; i++)
		if (v_used[i])
			new_indices[i] = iwalk++;
	int nv2 = iwalk;
	
	float *v2 = (float*)malloc(3*nv2*sizeof(float));
	iwalk = 0;
	for (i=0; i<nv; i++)
	{
		if (v_used[i])
		{
			v2[3*iwalk]   = v[3*i];
			v2[3*iwalk+1] = v[3*i+1];
			v2[3*iwalk+2] = v[3*i+2];
			iwalk++;
		}
	}
	
	int *f2 = (int*)malloc(3*nf*sizeof(int));
	for (i=0; i<3*nf; i++) f2[i] = new_indices[f[i]];
	
	free (v);
	v = v2;
	nv = nv2;
	free (f);
	f = f2;

	// create the mesh
	mesh = new Mesh (nv, nf);
	
	// create vertices
	mesh->SetVertices (nv, v);
	/*
	for (unsigned int i=0; i<nv; i++)
	{
		mesh->m_pVertices[3*i]   = v[3*i];
		mesh->m_pVertices[3*i+1] = v[3*i+1];
		mesh->m_pVertices[3*i+2] = v[3*i+2];
	}
	*/

	// create faces
	unsigned int face = 0;
	for (unsigned int i=0; i<nf; i++)
	{
		mesh->m_pFaces[i] = new Face ();
		mesh->m_pFaces[i]->SetTriangle (f[3*i], f[3*i+1], f[3*i+2]);
		/*
		mesh_face_vi(mesh, face, 0) = f[3*i];
		mesh_face_vi(mesh, face, 1) = f[3*i+1];
		mesh_face_vi(mesh, face, 2) = f[3*i+2];
		*/
	}

	/* cleaning */
	free (f2);
	free (v2);
	free (new_indices);
	free (v_used);
	for (i=0; i<nx; i++)
	{
		for (j=0; j<ny; j++)
		{
			if (voxels[i][j])
				free (voxels[i][j]);
		}
		if (voxels[i])
			free (voxels[i]);
	}
	free (voxels);

	return mesh;
}
