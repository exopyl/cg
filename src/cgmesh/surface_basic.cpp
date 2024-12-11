//#define _BSD_SOURCE
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>

#include "surface_basic.h"

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

	float *pVertices = mesh->m_pVertices;
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

	float *pVertices = mesh->m_pVertices;
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

	float *pVertices = mesh->m_pVertices;
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
	unsigned int nVertices = sizeof(TeapotData_Vertex)/sizeof(3*sizeof(float));
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
	unsigned int vn = 10000;
	unsigned int fn = 2*((ThetaResolution-1)*(PhiResolution)+PhiResolution-1+1);
	Mesh *mesh = new Mesh (vn, fn);

	float Theta, Phi;
	float x, y, z;
	float a = 3;
	float b = 8;
	float c = 2;
	float Pi = 3.14159f;

	float stepTheta = 2*Pi/ThetaResolution; // step between two slices
	float stepPhi = 2*Pi/PhiResolution; // step in a slice

	//
	// vertices
	//
	unsigned int vi=0;
	for (Theta=0; Theta<Pi; Theta+=stepTheta)
	{
		for (Phi=0; Phi<2.*Pi; Phi+=stepPhi)
		{
			x = (a*(1.0f+sin(Theta)) + r(Theta)*cos(Phi)) * cos(Theta);
			y = (b+r(Theta)*cos(Phi)) * sin(Theta);
			z = r(Theta)*sin(Phi);
			mesh->SetVertex (vi++, x, y, z);
		}
	}

	for (Theta=Pi; Theta<2.*Pi; Theta+=stepTheta)
	{
		for (Phi=0; Phi<2.*Pi; Phi+=stepPhi)
		{
			x = a*(1+sin(Theta))*cos(Theta) - r(Theta) * cos(Phi);
			y = b*sin(Theta);
			z = r(Theta)*sin(Phi);
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

	printf ("%d %d\n", vi, fi);
	mesh->m_nVertices = vi;
	mesh->m_nFaces = fi;

	return mesh;
}

