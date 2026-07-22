//#define _BSD_SOURCE
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <array>

#include "surface_basic.h"
#include "chull.h"

//
//
//
Mesh* CreateTriangle (float x1, float y1, float z1,
		      float x2, float y2, float z2,
		      float x3, float y3, float z3)
{
	Mesh *mesh = new Mesh (3, 1);

	// create vertices
	float vertices[3*3] = {x1, y1, z1, x2, y2, z2, x3, y3, z3};
	mesh->SetVertices (3, vertices);

	// create faces
	unsigned int faces[3] = {0, 1, 2};
	mesh->SetFaces (1, 3, faces);

	return mesh;
}

//
//
//
Mesh* CreateQuad (void)
{
	Mesh *mesh = new Mesh ();

	// create vertices
	float vertices[4*3] = { -1., -1., 0.,
			         1., -1., 0.,
			         1.,  1., 0.,
			        -1.,  1., 0.  };
	mesh->SetVertices (4, vertices);

	// create faces
	unsigned int faces[4] = {0, 1, 2, 3};
	mesh->SetFaces (1, 4, faces);
	
	//float texture_coordinates[4*2] = {0, 0, 1, 0, 1, 1, 0, 1};
	//mesh->SetTextureCoordinates (1, 4, texture_coordinates);

	return mesh;
}

Mesh* CreateGrid(unsigned int nx, unsigned int ny)
{
	int nvertices = nx*ny;
	int nfaces = 2*(nx-1)*(ny-1);
	Mesh *mesh = new Mesh (nvertices, nfaces);

	// generate vertices
	for (int j=0; j<ny; j++)
		for (int i=0; i<nx; i++)
			mesh->SetVertex(nx*j+i, (float)i, (float)j, 0.f);

	// generates faces
	int fi=0;
	for (int j=0; j<ny-1; j++)
		for (int i=0; i<nx-1; i++)
		{
			mesh->SetFace(fi++, nx*j+i, nx*j+i+1, nx*(j+1)+i+1);
			mesh->SetFace(fi++, nx*j+i, nx*(j+1)+i+1, nx*(j+1)+i);
		}

		return mesh;
}

//
//
//
Mesh* CreateCube (bool bTri)
{
	Mesh *mesh = new Mesh ();

	float vertices[8*3] = { -1., -1., -1.,
		1., -1., -1.,
		1.,  1., -1.,
		-1.,  1., -1.,
		-1., -1.,  1.,
		1., -1.,  1.,
		1.,  1.,  1.,
		-1.,  1.,  1. };
	mesh->SetVertices (8, vertices);

	// create faces
	if (bTri)
	{
		unsigned int faces[12*3] = {0, 3, 2, 0, 2, 1,
			4, 5, 6, 4, 6, 7,
			0, 4, 7, 0, 7, 3,
			3, 7, 6, 3, 6, 2,
			2, 6, 5, 2, 5, 1,
			1, 5, 4, 1, 4, 0};
		mesh->SetFaces (12, 3, faces);
	}
	else
	{
		unsigned int faces[6*4] = {0, 3, 2, 1,
			4, 5, 6, 7,
			0, 4, 7, 3,
			3, 7, 6, 2,
			2, 6, 5, 1,
			1, 5, 4, 0};
		mesh->SetFaces (6, 4, faces);

		//float texture_coordinates[6*2] = {};
		//mesh->SetTextureCoordinates (6, 4, texture_coordinates);
	}

	return mesh;
}

//
//
//
Mesh* CreateTetrahedron (void)
{
	Mesh *mesh = new Mesh ();

	// create vertices
	float sqrt3 = 1.0f / sqrt(3.0f);
	float vertices[12] = {sqrt3, sqrt3, sqrt3,
			      -sqrt3, -sqrt3, sqrt3,
			      -sqrt3, sqrt3, -sqrt3,
			      sqrt3, -sqrt3, -sqrt3};
	mesh->SetVertices (4, vertices);

	// create faces
	unsigned int faces[12] = {0, 2, 1, 0, 1, 3, 2, 3, 1, 3, 2, 0};
	mesh->SetFaces (4, 3, faces);

	return mesh;
}

//
//
//
Mesh* CreateOctahedron (void)
{
	Mesh *mesh = new Mesh ();

	// create vertices
	float vertices[18] = {0.0, 0.0, -1.0,
			      1.0, 0.0, 0.0,
			      0.0, -1.0, 0.0,
			      -1.0, 0.0, 0.0,
			      0.0, 1.0, 0.0,
			      0.0, 0.0, 1.0}; 
	mesh->SetVertices (6, vertices);

	// create faces
	unsigned int faces[24] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4};
	mesh->SetFaces (8, 3, faces);

	return mesh;
}

Mesh* CreateDodecahedron (void)
{
	Mesh *mesh = new Mesh ();

	// create vertices
	float vertices[20*3] = {-0.57735,  -0.57735,  0.57735,
				0.934172,  0.356822,  0,
				0.934172,  -0.356822,  0,
				-0.934172,  0.356822,  0,
				-0.934172,  -0.356822,  0,
				0,  0.934172,  0.356822,
				0,  0.934172,  -0.356822,
				0.356822,  0,  -0.934172,
				-0.356822,  0,  -0.934172,
				0,  -0.934172,  -0.356822,
				0,  -0.934172,  0.356822,
				0.356822,  0,  0.934172,
				-0.356822,  0,  0.934172,
				0.57735,  0.57735,  -0.57735,
				0.57735,  0.57735,  0.57735,
				-0.57735,  0.57735,  -0.57735,
				-0.57735,  0.57735,  0.57735,
				0.57735,  -0.57735,  -0.57735,
				0.57735,  -0.57735,  0.57735,
				-0.57735,  -0.57735,  -0.57735 };
	mesh->SetVertices (20, vertices);

	// create faces
	unsigned int faces[12*5] = {  1, 14, 11, 18, 2,
				      1, 2, 17, 7, 13,
				      3, 15, 8, 19, 4,
				      3, 4, 0, 12, 16,
				      3, 16, 5, 6, 15,
				      1, 13, 6, 5, 14,
				      2, 18, 10, 9, 17,
				      4, 19, 9, 10, 0,
				      7, 17, 9, 19, 8,
				      6, 13, 7, 8, 15,
				      5, 16, 12, 11, 14,
				      10, 18, 11, 12, 0 };
	mesh->SetFaces (12, 5, faces);

	return mesh;
}

extern Mesh* CreateIcosahedron (void)
{
	Mesh *mesh = new Mesh ();

	// create vertices
	float t = (1.0f+sqrt(5.0f))/2.0f;
	float tau = t/sqrt(1.0f+t*t);
	float one = 1/sqrt(1.0f+t*t);

	float vertices[12*3] = {tau, one, 0.0,
			      -tau, one, 0.0,
			      -tau, -one, 0.0,
			      tau, -one, 0.0,
			      one, 0.0 ,  tau,
			      one, 0.0 , -tau,
			      -one, 0.0 , -tau,
			      -one, 0.0 , tau,
			      0.0 , tau, one,
			      0.0 , -tau, one,
			      0.0 , -tau, -one,
			      0.0 , tau, -one};
	mesh->SetVertices (12, vertices);

	// create faces
	unsigned int faces[20*3] = {4, 8, 7,
				    4, 7, 9,
				    5, 6, 11,
				    5, 10, 6,
				    0, 4, 3,
				    0, 3, 5,
				    2, 7, 1,
				    2, 1, 6,
				    8, 0, 11,
				    8, 11, 1,
				    9, 10, 3,
				    9, 2, 10,
				    8, 4, 0,
				    11, 0, 5,
				    4, 9, 3,
				    5, 3, 10,
				    7, 8, 1,
				    6, 1, 11,
				    7, 2, 9,
				    6, 10, 2};
	mesh->SetFaces (20, 3, faces);

	return mesh;
}

//
//
//
Mesh* CreateDisk (unsigned int nVertices)
{
	Mesh *mesh = new Mesh (nVertices, 1);

	float *pVertices = mesh->m_pVertices.data();
	for (unsigned int i=0; i<nVertices; i++)
	{
		float fAngle = i*2*M_PI/(nVertices);
		pVertices[3*i+0] = cos(fAngle);
		pVertices[3*i+1] = sin(fAngle);
		pVertices[3*i+2] = 0.;
	}

	Face *pFace = new Face ();
	pFace->SetNVertices (nVertices);
	for (unsigned int i=0; i<nVertices; i++)
		pFace->m_pVertices[i] = i;
	mesh->m_pFaces[0] = pFace;

	return mesh;
}

//
//
//
Mesh* CreateCone (float fHeight, float fRadius, unsigned int nVertices, bool bCap)
{
	unsigned int nv = nVertices+1;
	unsigned int nf = (bCap)? nVertices+1 : nVertices;
	Mesh *mesh = new Mesh (nv, nf);

	float *pVertices = mesh->m_pVertices.data();
	pVertices[0] = 0.;
	pVertices[1] = 0.;
	pVertices[2] = fHeight;
	for (unsigned int i=1; i<=nVertices; i++)
	{
		float fAngle = (i-1)*2*M_PI/(nVertices);
		pVertices[3*i+0] = fRadius * cos(fAngle);
		pVertices[3*i+1] = fRadius * sin(fAngle);
		pVertices[3*i+2] = 0.;
	}

	for (unsigned int i=0; i<nVertices; i++)
	{
		Face *pFace = new Face ();
		pFace->SetTriangle (0, i+1, (i==(nVertices-1))? 1 : i+2);
		mesh->m_pFaces[i] = pFace;
	}
	if (bCap)
	{
		Face *pFace = new Face ();
		pFace->SetNVertices (nVertices);
		for (unsigned int i=0; i<nVertices; i++)
			pFace->m_pVertices[i] = nVertices-i;
		mesh->m_pFaces[nVertices] = pFace;
	
	}

	return mesh;
}

//
//
//
Mesh* CreateCylinder (float fHeight, float fRadius, unsigned int nVertices, bool bCap, bool bTriangular)
{
	unsigned int nv = 2*nVertices;
	unsigned int nf = 0;
	if (!bTriangular)
		nf = (bCap)? nVertices+2 : nVertices;
	else
		nf = (bCap)? 2*nVertices+2*(nVertices-2) : 2*nVertices;
	Mesh *mesh = new Mesh (nv, nf);

	float *pVertices = mesh->m_pVertices.data();
	for (unsigned int i=0; i<nVertices; i++)
	{
		float fAngle = i*2*M_PI/(nVertices);
		pVertices[3*i+0] = fRadius * cos(fAngle);
		pVertices[3*i+1] = fRadius * sin(fAngle);
		pVertices[3*i+2] = 0.;

		pVertices[3*(nVertices+i)+0] = pVertices[3*i+0];
		pVertices[3*(nVertices+i)+1] = pVertices[3*i+1];
		pVertices[3*(nVertices+i)+2] = fHeight;
	}

	if (bTriangular)
	{
		unsigned int fi = 0;
		for (unsigned int i=0; i<nVertices; i++)
		{
			Face *pFace1 = new Face ();
			pFace1->SetTriangle (i, (i+1)%nVertices, nVertices+(i+1)%nVertices);
			mesh->m_pFaces[fi++] = pFace1;

			Face *pFace2 = new Face ();
			pFace2->SetTriangle (i, nVertices+(i+1)%nVertices, nVertices+i);
			mesh->m_pFaces[fi++] = pFace2;
		}
		if (bCap)
		{
			for (unsigned int i=1; i<nVertices-1; i++)
			{
				Face *pFaceTop = new Face ();
				pFaceTop->SetTriangle (nVertices, nVertices+i, nVertices+(i+1)%nVertices);
				mesh->m_pFaces[fi++] = pFaceTop;
			}
			for (unsigned int i=1; i<nVertices-1; i++)
			{
				Face *pFaceBottom = new Face ();
				pFaceBottom->SetTriangle (0, nVertices-i, nVertices-(i+1)%nVertices);
				mesh->m_pFaces[fi++] = pFaceBottom;
			}
		}
	}
	else
	{
		for (unsigned int i=0; i<nVertices; i++)
		{
			Face *pFace = new Face ();
			pFace->SetQuad (i, i+1, nVertices+i+1, nVertices+i);
			if (i==nVertices-1)
			{
				pFace->m_pVertices[1] = 0;
				pFace->m_pVertices[2] = nVertices;
			}
			mesh->m_pFaces[i] = pFace;
		}
		if (bCap)
		{
			Face *pFaceTop = new Face ();
			pFaceTop->SetNVertices (nVertices);
			Face *pFaceBottom = new Face ();
			pFaceBottom->SetNVertices (nVertices);
			for (unsigned int i=0; i<nVertices; i++)
			{
				pFaceTop->m_pVertices[i] = nVertices+i;
				pFaceBottom->m_pVertices[i] = nVertices-i-1;
			}
			mesh->m_pFaces[nVertices] = pFaceTop;
			mesh->m_pFaces[nVertices+1] = pFaceBottom;
		}
	}

	return mesh;
}

//
//
//
Mesh* CreateCapsule (unsigned int n, float height, float radius)
{
    float height_half = height/2.;
    unsigned int nhalf = n/2.;

	unsigned int nVertices = 2*n*nhalf+2;
	unsigned int nFaces = 2*(n-1)*2*nhalf + 2*n;
	Mesh *mesh = new Mesh (nVertices, nFaces);

	float x, y, z;
	unsigned int ivertex = 0;
	unsigned int iface = 0;

    //
    // vertices
    //

    // top
    for (unsigned int i=0; i<n; i++)
    {
        float angle = 2.*M_PI*i / n;
        x = radius*cos(angle);
        y = radius*sin(angle);
        z = height_half;
		mesh->SetVertex (ivertex++, x, y, z);
    }

    for (unsigned int j=1; j<nhalf; j++)
    {
        float hcapsule = cos ((M_PI/2.)*(1.-((float)j)/(nhalf)));
        float rcapsule = radius*sqrt (1. - hcapsule*hcapsule);
        hcapsule *= radius;
        for (unsigned int i=0; i<n; i++)
        {
            float angle = 2.*M_PI*i / n;
            x = rcapsule * cos(angle);
            y = rcapsule * sin(angle);
            z = height_half + hcapsule;
 			mesh->SetVertex (ivertex++, x, y, z);
        }
    }
    x = 0.;
    y = 0.;
    z = height_half + radius;
    mesh->SetVertex (ivertex++, x, y, z);
    
    // bottom
    for (unsigned int i=0; i<n; i++)
    {
        float angle = 2.*M_PI*i / n;
        x = radius*cos(angle);
        y = radius*sin(angle);
        z = -height_half;
		mesh->SetVertex (ivertex++, x, y, z);
    }

    for (unsigned int j=1; j<nhalf; j++)
    {
        float hcapsule = cos ((M_PI/2.)*(1.-((float)j)/(nhalf)));
        float rcapsule = radius*sqrt (1. - hcapsule*hcapsule);
        hcapsule *= radius;
        for (unsigned int i=0; i<n; i++)
        {
            float angle = 2.*M_PI*i / n;
            x = rcapsule * cos(angle);
            y = rcapsule * sin(angle);
            z = -height_half - hcapsule;
			mesh->SetVertex (ivertex++, x, y, z);
        }
    }
    x = 0.;
    y = 0.;
    z = -height_half - radius;
    mesh->SetVertex (ivertex++, x, y, z);

    //
    // faces
    //

    // top
    unsigned int voffsettop = 0;
    unsigned int a, b, c;
    for (unsigned int j=0; j<nhalf-1; j++)
        for (unsigned int i=0; i<n; i++)
        {
            a = voffsettop + j*n+i;
            b = voffsettop + j*n+(i+1)%n;
            c = voffsettop + (j+1)*n+(i+1)%n;
	    mesh->m_pFaces[iface] = new Face ();
	    mesh->m_pFaces[iface++]->SetTriangle (a, b, c);

            a = voffsettop + j*n+i;
            b = voffsettop + (j+1)*n+(i+1)%n;
            c = voffsettop + (j+1)*n+i;
            mesh->m_pFaces[iface] = new Face ();
	    mesh->m_pFaces[iface++]->SetTriangle (a, b, c);
        }

    for (unsigned int i=0; i<n; i++)
    {
        a = voffsettop + n*nhalf;
        b = voffsettop + n*(nhalf-1) + i;
        c = voffsettop + n*(nhalf-1) + (i+1)%n;
        mesh->m_pFaces[iface] = new Face ();
	mesh->m_pFaces[iface++]->SetTriangle (a, b, c);
    }

    // bottom
    voffsettop = n*nhalf+1;
    for (unsigned int j=0; j<nhalf-1; j++)
        for (unsigned int i=0; i<n; i++)
        {
            a = voffsettop + j*n+i;
            b = voffsettop + (j+1)*n+(i+1)%n;
            c = voffsettop + j*n+(i+1)%n;
            mesh->m_pFaces[iface] = new Face ();
	    mesh->m_pFaces[iface++]->SetTriangle (a, b, c);

            a = voffsettop + j*n+i;
            b = voffsettop + (j+1)*n+i;
            c = voffsettop + (j+1)*n+(i+1)%n;
            mesh->m_pFaces[iface] = new Face ();
	    mesh->m_pFaces[iface++]->SetTriangle (a, b, c);
        }

    for (unsigned int i=0; i<n; i++)
    {
        a = voffsettop + n*nhalf;
        b = voffsettop + n*(nhalf-1) + (i+1)%n;
        c = voffsettop + n*(nhalf-1) + i;
        mesh->m_pFaces[iface] = new Face ();
	mesh->m_pFaces[iface++]->SetTriangle (a, b, c);
    }

    // body
    for (unsigned int j=0; j<n; j++)
    {
        a = (j+1)%n;
        b = j;
        c = voffsettop+j;
        mesh->m_pFaces[iface] = new Face ();
	mesh->m_pFaces[iface++]->SetTriangle (a, b, c);

        a = (j+1)%n;
        b = voffsettop+j;
        c = voffsettop+(j+1)%n;
        mesh->m_pFaces[iface] = new Face ();
	mesh->m_pFaces[iface++]->SetTriangle (a, b, c);
    }

    return mesh;
}

#include "surface_teapot_data.h"
Mesh* CreateTeapot (void)
{
	unsigned int nVertices = sizeof(TeapotData_Vertex)/(3*sizeof(float));
	unsigned int nFaces = sizeof(TeapotData_Face)/(3*sizeof(int));

	Mesh *mesh = new Mesh (nVertices, nFaces);

	// create vertices
	for (unsigned int i=0; i<nVertices; i++)
	{
		mesh->m_pVertices[3*i+0] = TeapotData_Vertex[i][0];
		mesh->m_pVertices[3*i+1] = TeapotData_Vertex[i][1];
		mesh->m_pVertices[3*i+2] = TeapotData_Vertex[i][2];
	}

	// create faces
	for (unsigned int i=0; i<nFaces; i++)
	{
		Face *pFace = new Face ();
		pFace->SetTriangle (TeapotData_Face[3*i], TeapotData_Face[3*i+1], TeapotData_Face[3*i+2]);
		mesh->m_pFaces[i] = pFace;
	}

	return mesh;
}

//
//
//

/*
Theta \in [0,Pi] et Phi \in [0,2Pi]
x = (a*(1+sin(Theta)) + r(Theta)*cos(Phi)) * cos(Theta)
y = (b+r(Theta)*cos(Phi)) * sin(Theta)
z = r(Theta)*sin(Phi)

Theta \in [Pi,2Pi] et Phi \in [0,2Pi]
x = a*(1+sin(Theta))*cos(Theta) - r(Theta) * cos(Phi)
y = b*sin(Theta)
z = r(Theta)*sin(Phi)

  r(Theta) = c(1 - cos(Theta)/2)
  a = 3
  b = 8
  c = 2
*/

static float r (float alpha)
{
	return 2.0f*(1.0f-cos(alpha)*0.5f);
}

Mesh* CreateKleinBottle (int ThetaResolution, int PhiResolution)
{
	// Grille exacte ThetaResolution x PhiResolution. Auparavant des boucles a
	// compteur flottant (for(Theta=0; Theta<Pi; Theta+=step)) produisaient un
	// nombre de sommets != de cette grille (accumulation flottante) -> ecriture
	// hors-borne + decalage du stride des faces -> rupture de continuite.
	unsigned int vn = ThetaResolution * PhiResolution;
	unsigned int fn = 2*((ThetaResolution-1)*(PhiResolution)+PhiResolution-1+1);
	Mesh *mesh = new Mesh (vn, fn);

	float a = 3;
	float b = 8;
	float Pi = 3.14159f;

	float stepTheta = 2*Pi/ThetaResolution; // step between two slices
	float stepPhi = 2*Pi/PhiResolution; // step in a slice

	//
	// vertices (boucles ENTIERES -> exactement ThetaResolution*PhiResolution)
	//
	unsigned int vi=0;
	for (int j=0; j<ThetaResolution; j++)
	{
		float Theta = j*stepTheta;
		for (int i=0; i<PhiResolution; i++)
		{
			float Phi = i*stepPhi;
			float x, y, z;
			if (Theta < Pi)   // premiere moitie (tube)
			{
				x = (a*(1.0f+sin(Theta)) + r(Theta)*cos(Phi)) * cos(Theta);
				y = (b+r(Theta)*cos(Phi)) * sin(Theta);
				z = r(Theta)*sin(Phi);
			}
			else              // seconde moitie (anse) ; coincide avec la premiere en Theta=Pi
			{
				x = a*(1+sin(Theta))*cos(Theta) - r(Theta) * cos(Phi);
				y = b*sin(Theta);
				z = r(Theta)*sin(Phi);
			}
			mesh->SetVertex (vi++, x, y, z);
		}
	}

	//
	// faces
	//
	int nVerticesInSlice = PhiResolution;
	int nSlicesInTheta = ThetaResolution;
	int i, j, i1, i2, i3, i4;
	unsigned int fi=0;
	for (j=0; j<nSlicesInTheta-1; j++)
	{
		for (i=0; i<nVerticesInSlice; i++)
		{
			i1 = j*nVerticesInSlice+i;
			i2 = i1+1;
			i3 = (j+1)*nVerticesInSlice+i;
			i4 = i3+1;

			if ( !(nVerticesInSlice-1-i) ) // reach the border (i == (nVerticesInSlice-1))
			{
				i2 -= nVerticesInSlice;
				i4 -= nVerticesInSlice;
			}

			mesh->m_pFaces[fi] = new Face ();
			mesh->m_pFaces[fi++]->SetTriangle (i1, i2, i3);
			mesh->m_pFaces[fi] = new Face ();
			mesh->m_pFaces[fi++]->SetTriangle (i2, i4, i3);
		}
	}

	// last connection
	for (i=0; i<nVerticesInSlice-1; i++)
	{
		i1 = j*nVerticesInSlice+i;
		i2 = j*nVerticesInSlice+i+1;
		i3 = nVerticesInSlice/2-i;
		i4 = nVerticesInSlice/2-i-1;
		if (i3<0)
			i3 = nVerticesInSlice-(i-nVerticesInSlice/2);
		if (i4<0)
			i4 = nVerticesInSlice-(i+1-nVerticesInSlice/2);

		mesh->m_pFaces[fi] = new Face ();
		mesh->m_pFaces[fi++]->SetTriangle (i1, i2, i3);
		mesh->m_pFaces[fi] = new Face ();
		mesh->m_pFaces[fi++]->SetTriangle (i2, i4, i3);
	}
	i1 = i1+1;
	i2 = i2+1-nVerticesInSlice;
	i3 = (i3 == 0)? nVerticesInSlice-1 : i3-1;
	i4 = (i4 == 0)? nVerticesInSlice-1 : i4-1;
	mesh->m_pFaces[fi] = new Face ();
	mesh->m_pFaces[fi++]->SetTriangle (i1, i2, i3);
	mesh->m_pFaces[fi] = new Face ();
	mesh->m_pFaces[fi++]->SetTriangle (i2, i4, i3);

	mesh->m_nVertices = vi;
	mesh->m_nFaces = fi;

	return mesh;
}


// ===========================================================================
//  Tubes le long de polylignes (primitive generique reutilisable)
// ===========================================================================
//  Extrait de ParameterizedLSystem : "walk -> tube" n'a rien de specifique aux
//  L-systemes. Utilisable pour toute polyligne (courbe parametrique, ligne
//  extraite d'un mesh, chemin SVG...). Ne normalise pas les points.
//  Iso-fonctionnel : chaque segment est tube independamment (repere de section
//  propre) ; les jointures (alignement le long des chaines + coins/branches)
//  viendront enrichir cette fonction.

namespace {

// --- petit algebre vectorielle locale (V3 distinct de Vector3f : evite toute
// collision d'overload avec les free-functions eventuelles sur Vector3f) ------
struct V3 { float x, y, z; };
inline V3 v3(float x,float y,float z){ return {x,y,z}; }
inline V3 vsub(const V3&a,const V3&b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline V3 vadd(const V3&a,const V3&b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline V3 vmul(const V3&a,float s){ return {a.x*s,a.y*s,a.z*s}; }
inline float vdot(const V3&a,const V3&b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
inline V3 vcross(const V3&a,const V3&b){ return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
inline float vnorm(const V3&a){ return sqrtf(vdot(a,a)); }
inline V3 vnormalize(const V3&a){ float l=vnorm(a); return (l>1e-9f)? vmul(a,1.f/l): v3(1,0,0); }
inline V3 vrot(const V3&v,const V3&k,float ang){   // rotation de Rodrigues
	float c=cosf(ang), sn=sinf(ang);
	return vadd(vadd(vmul(v,c), vmul(vcross(k,v),sn)), vmul(k, vdot(k,v)*(1.f-c)));
}
inline V3 anyPerp(const V3&n){
	V3 a = (fabsf(n.z)<0.9f)? v3(0,0,1): v3(1,0,0);
	return vnormalize(vcross(a,n));
}
inline V3 transport(const V3&u,const V3&n0,const V3&n1){   // RMF discret (rotation minimale)
	V3 ax=vcross(n0,n1); float sn=vnorm(ax), c=vdot(n0,n1);
	V3 r = (sn<1e-7f)? u : vrot(u, vmul(ax,1.f/sn), atan2f(sn,c));
	r = vsub(r, vmul(n1, vdot(r,n1)));
	return vnormalize(r);
}
inline float signedAngle(const V3&a,const V3&b,const V3&n){
	return atan2f(vdot(vcross(a,b),n), vdot(a,b));
}

// Un anneau emis : centre, base d'index dans verts, K, type d'extremite
// (0 = interieur, 1 = debut de chaine ouverte, 2 = fin de chaine ouverte).
struct RingRec { V3 c; unsigned int base; int K; int endKind; };

struct Station { V3 c, n; bool corner; V3 din, dout; };

void addCorner(std::vector<Station>& st, const V3& c, const V3& din, const V3& dout, float miterLimit)
{
	V3 bis = vadd(din,dout); float bl=vnorm(bis);
	if (bl < 1e-5f) {                    // ~180 deg -> bevel : 2 anneaux perpendiculaires
		st.push_back({c, din, false, din, dout});
		st.push_back({c, dout,false, din, dout});
		return;
	}
	bis = vmul(bis,1.f/bl);
	float cosHalf = vdot(dout,bis); if (cosHalf<1e-4f) cosHalf=1e-4f;
	if (1.f/cosHalf <= miterLimit) st.push_back({c, bis, true, din, dout});     // miter
	else { st.push_back({c, din, false, din, dout});                            // bevel
	       st.push_back({c, dout,false, din, dout}); }
}

// L'ensemble de points s'etend-il vraiment en 3D ? (garde anti-degenerescence
// avant Chull3D : evite les cas colineaires/coplanaires et leur printf).
bool spans3D(const std::vector<float>& pts, int n, float r)
{
	auto P=[&](int i){ return v3(pts[3*i],pts[3*i+1],pts[3*i+2]); };
	V3 p0=P(0);
	int i1=-1; float best=0.f;
	for (int i=1;i<n;i++){ float d=vnorm(vsub(P(i),p0)); if(d>best){best=d;i1=i;} }
	if (i1<0 || best<1e-9f) return false;
	V3 e1=vnormalize(vsub(P(i1),p0));
	int i2=-1; best=0.f;
	for (int i=1;i<n;i++){ float m=vnorm(vcross(e1, vsub(P(i),p0))); if(m>best){best=m;i2=i;} }
	if (i2<0 || best<1e-9f) return false;
	V3 nrm=vnormalize(vcross(e1, vsub(P(i2),p0)));
	float thick=0.f;
	for (int i=0;i<n;i++){ float t=fabsf(vdot(vsub(P(i),p0),nrm)); if(t>thick)thick=t; }
	return thick > 0.1f*r;
}

// R1 (sweep anneau partage + RMF) + R2 (join miter/bevel) pour UNE polyligne.
// Emet sommets + bandes laterales, et ENREGISTRE chaque anneau dans `rings`
// (les capuchons et jointures sont decides ensuite par CreateTubes).
void sweepPolyline(const std::vector<Vector3f>& raw, float r, int K, float miterLimit,
                   std::vector<float>& verts, std::vector<unsigned int>& faces,
                   std::vector<RingRec>& rings)
{
	std::vector<V3> P;
	for (const auto& q : raw) {
		V3 p = v3(q.x,q.y,q.z);
		if (P.empty()) { P.push_back(p); continue; }
		V3 dd = vsub(p,P.back());
		if (vdot(dd,dd) >= 1e-12f) P.push_back(p);
	}
	bool closed = false;
	if ((int)P.size() >= 4) {
		V3 dd = vsub(P.front(),P.back());
		if (vdot(dd,dd) < 0.25f*r*r) { closed = true; P.pop_back(); }
	}
	const int M = (int)P.size();
	if (M < 2) return;

	const int nSeg = closed ? M : (M-1);
	std::vector<V3> d(nSeg);
	for (int i=0;i<nSeg;i++) d[i] = vnormalize(vsub(P[(i+1)%M], P[i]));

	std::vector<Station> st;
	if (!closed) {
		st.push_back({P[0], d[0], false, d[0], d[0]});
		for (int i=1;i<M-1;i++) addCorner(st, P[i], d[i-1], d[i], miterLimit);
		st.push_back({P[M-1], d[M-2], false, d[M-2], d[M-2]});
	} else {
		for (int i=0;i<M;i++) addCorner(st, P[i], d[(i-1+M)%M], d[i], miterLimit);
	}
	const int S = (int)st.size();
	if (S < 2) return;

	std::vector<V3> U(S), Vv(S);
	U[0]=anyPerp(st[0].n); Vv[0]=vcross(st[0].n,U[0]);
	for (int k=1;k<S;k++){ U[k]=transport(U[k-1], st[k-1].n, st[k].n); Vv[k]=vcross(st[k].n,U[k]); }
	if (closed) {
		V3 ucl = transport(U[S-1], st[S-1].n, st[0].n);
		float phi = signedAngle(ucl, U[0], st[0].n);
		for (int k=0;k<S;k++){ U[k]=vrot(U[k], st[k].n, -phi*(float)k/(float)S); Vv[k]=vcross(st[k].n,U[k]); }
	}

	std::vector<unsigned int> ringBase(S);
	for (int k=0;k<S;k++) {
		ringBase[k] = (unsigned int)(verts.size()/3);
		const Station& stt = st[k];
		bool doStretch=false; V3 mHat=v3(1,0,0); float stretch=1.f;
		if (stt.corner) {
			float cosHalf = vdot(stt.dout, stt.n); if (cosHalf<1e-4f) cosHalf=1e-4f;
			stretch = 1.f/cosHalf;
			V3 m = vsub(stt.dout, vmul(stt.n, cosHalf));
			float ml=vnorm(m); if (ml>1e-6f){ mHat=vmul(m,1.f/ml); doStretch=true; }
		}
		for (int j=0;j<K;j++) {
			float t=2.f*(float)M_PI*(float)j/(float)K, cs=cosf(t), sn=sinf(t);
			V3 o = vadd(vmul(U[k], r*cs), vmul(Vv[k], r*sn));
			if (doStretch) o = vadd(o, vmul(mHat, (stretch-1.f)*vdot(o,mHat)));
			V3 pp = vadd(stt.c, o);
			verts.push_back(pp.x); verts.push_back(pp.y); verts.push_back(pp.z);
		}
		int endKind = 0;
		if (!closed) { if (k==0) endKind=1; else if (k==S-1) endKind=2; }
		rings.push_back({stt.c, ringBase[k], K, endKind});
	}

	auto band=[&](unsigned int A, unsigned int B){
		for (int j=0;j<K;j++){ int j2=(j+1)%K;
			faces.push_back(A+j); faces.push_back(A+j2); faces.push_back(B+j2);
			faces.push_back(A+j); faces.push_back(B+j2); faces.push_back(B+j);
		}
	};
	const int lastBand = closed ? S : (S-1);
	for (int k=0;k<lastBand;k++) band(ringBase[k], ringBase[(k+1)%S]);
}

} // namespace

Mesh* CreateTubes (const std::vector<std::vector<Vector3f>>& polylines, float radius, int nSides, bool caps)
{
	if (nSides < 3) nSides = 3;
	const int   K = nSides;
	const float r = radius;
	const float miterLimit = 4.0f;

	std::vector<float>        verts;
	std::vector<unsigned int> faces;
	std::vector<RingRec>      rings;
	for (const auto& pl : polylines)
		sweepPolyline(pl, r, K, miterLimit, verts, faces, rings);

	// --- R3/R4 : traiter les extremites et les noeuds de branchement ---------
	// Cluster les anneaux par centre (points partages = noeuds implicites).
	// tol minuscule : on ne cluster que les points STRICTEMENT coincidents (les
	// vrais noeuds, coords bit-identiques), pas les points proches — sinon
	// sur-clustering sur fractales denses -> clusters geants -> hull qui boucle.
	const float tol = 1e-4f;
	std::map<std::array<int,3>, std::vector<int>> cells;
	for (int i=0;i<(int)rings.size();i++) {
		const V3& c = rings[i].c;
		std::array<int,3> key = { (int)floorf(c.x/tol), (int)floorf(c.y/tol), (int)floorf(c.z/tol) };
		cells[key].push_back(i);
	}

	// Budget : hull (R3) seulement pour les maillages moderes ; au-dela, repli
	// R4 (cap+overlap, rapide) pour rester interactif sur les grosses plantes.
	const bool useHull = ((int)rings.size() <= 8000);

	auto capRing=[&](const RingRec& rr){
		if (!caps) return;
		unsigned int b=rr.base; int Kk=rr.K;
		if (rr.endKind==1) for (int j=1;j<=Kk-2;j++){ faces.push_back(b); faces.push_back(b+j+1); faces.push_back(b+j); }
		else if (rr.endKind==2) for (int j=1;j<=Kk-2;j++){ faces.push_back(b); faces.push_back(b+j); faces.push_back(b+j+1); }
	};

	for (auto& kv : cells) {
		std::vector<int>& idxs = kv.second;
		if (idxs.size() == 1) { capRing(rings[idxs[0]]); continue; }

		// Noeud (>=2 anneaux) : R3 = hull des sommets des anneaux incidents.
		std::vector<float> pts;
		for (int ri : idxs) {
			const RingRec& rr = rings[ri];
			for (int j=0;j<rr.K;j++) {
				unsigned int vi = rr.base + (unsigned int)j;
				pts.push_back(verts[3*vi]); pts.push_back(verts[3*vi+1]); pts.push_back(verts[3*vi+2]);
			}
		}
		// dedup : O'Rourke boucle sur les points coincidents.
		std::vector<float> pd;
		const float deps2 = (1e-3f*r)*(1e-3f*r);
		for (int a=0;a<(int)pts.size();a+=3) {
			bool dup=false;
			for (int b=0;b<(int)pd.size();b+=3) {
				float dx=pts[a]-pd[b], dy=pts[a+1]-pd[b+1], dz=pts[a+2]-pd[b+2];
				if (dx*dx+dy*dy+dz*dz < deps2) { dup=true; break; }
			}
			if (!dup) { pd.push_back(pts[a]); pd.push_back(pts[a+1]); pd.push_back(pts[a+2]); }
		}
		int nPts = (int)(pd.size()/3);
		bool built = false;
		if (useHull && nPts >= 4 && spans3D(pd, nPts, r)) {
			// jitter minuscule DETERMINISTE : garantit une position generale
			// (O'Rourke boucle sinon sur les configs coplanaires/colineaires) ;
			// deterministe -> pas de scintillement du maillage entre regenerations.
			for (int a=0;a<(int)pd.size();a++) {
				unsigned h = (unsigned)((a+1)*2654435761u);
				pd[a] += (((float)((h>>8)&1023)/1023.f) - 0.5f) * (2e-3f*r);
			}
			Chull3D hull(pd.data(), nPts);
			hull.compute();
			float* hv=nullptr; int nhv=0; int* hf=nullptr; int nhf=0;
			if (hull.get_convex_hull(&hv,&nhv,&hf,&nhf) == 0 && nhv>=4 && nhf>=4) {
				unsigned int base = (unsigned int)(verts.size()/3);
				for (int i=0;i<nhv;i++){ verts.push_back(hv[3*i]); verts.push_back(hv[3*i+1]); verts.push_back(hv[3*i+2]); }
				for (int i=0;i<nhf;i++){ faces.push_back(base+(unsigned int)hf[3*i]); faces.push_back(base+(unsigned int)hf[3*i+1]); faces.push_back(base+(unsigned int)hf[3*i+2]); }
				built = true;
			}
			if (hv) free(hv);
			if (hf) free(hf);
		}
		if (!built) for (int ri : idxs) capRing(rings[ri]); // R4 : repli, ferme les extremites
	}

	Mesh* mesh = new Mesh();
	if (verts.empty()) {
		std::vector<float>        vtri = {0.f,0.f,0.f, 0.001f,0.f,0.f, 0.f,0.001f,0.f};
		std::vector<unsigned int> ftri = {0u,1u,2u};
		mesh->SetVertices(3, vtri.data());
		mesh->SetFaces(1, 3, ftri.data());
		return mesh;
	}
	mesh->SetVertices((unsigned int)(verts.size()/3), verts.data());
	mesh->SetFaces((unsigned int)(faces.size()/3), 3, faces.data());
	return mesh;
}

Mesh* CreateTube (const std::vector<Vector3f>& polyline, float radius, int nSides, bool caps)
{
	return CreateTubes(std::vector<std::vector<Vector3f>>{ polyline }, radius, nSides, caps);
}
