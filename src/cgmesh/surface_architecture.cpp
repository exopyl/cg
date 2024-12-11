#include <stdio.h>
#include <math.h>

#include "surface_architecture.h"
#include "../cgmath/cgmath.h"

Mesh* CreateBlock (float _width, float _height, float _depth, float bevel)
{
	Mesh *mesh = new Mesh (24, 26);

	float width = _width/2.;
	float height = _height/2.;
	float depth = _depth/2.;

	// vertices
	float vertices[3*24] = {width-bevel,    depth-bevel,   -height,
				width-bevel,   -(depth-bevel), -height,
				-(width-bevel), -(depth-bevel), -height,
				-(width-bevel),  depth-bevel,   -height,
				width-bevel,    depth-bevel,    height,
				-(width-bevel),  depth-bevel,    height,
				-(width-bevel), -(depth-bevel),  height,
				width-bevel,   -(depth-bevel),  height,
				width,          depth-bevel,   -(height-bevel),
				width,          depth-bevel,    height-bevel,
				width,         -(depth-bevel),  height-bevel,
				width,         -(depth-bevel), -(height-bevel),
				width-bevel,   -depth,         -(height-bevel),
				width-bevel,   -depth,           height-bevel,
				-(width-bevel), -depth,           height-bevel,
				-(width-bevel), -depth,         -(height-bevel),
				-width,         -(depth-bevel), -(height-bevel),
				-width,         -(depth-bevel),  height-bevel,
				-width,          depth-bevel,    height-bevel,
				-width,          depth-bevel,   -(height-bevel),
				width-bevel,    depth,          height-bevel,
				width-bevel,    depth,         -(height-bevel),
				-(width-bevel),  depth,         -(height-bevel),
				-(width-bevel),  depth,          height-bevel};
	mesh->SetVertices (24, vertices);

	// faces
	mesh->SetFace (0, 0, 1, 2, 3);
	mesh->SetFace (1, 6, 7, 4, 5);
	mesh->SetFace (2, 10, 11, 8, 9);
	mesh->SetFace (3, 14, 15, 12, 13);
	mesh->SetFace (4, 18, 19, 16, 17);
	mesh->SetFace (5, 22, 23, 20, 21);
	mesh->SetFace (6, 0, 8, 11, 1);
	mesh->SetFace (7, 15, 2, 1, 12);
	mesh->SetFace (8, 21, 0, 3, 22);
	mesh->SetFace (9, 19, 3, 2, 16);
	mesh->SetFace (10, 23, 5, 4, 20);
	mesh->SetFace (11, 5, 18, 17, 6);
	mesh->SetFace (12, 7, 10, 9, 4);
	mesh->SetFace (13, 6, 14, 13, 7);
	mesh->SetFace (14, 20, 9, 8, 21);
	mesh->SetFace (15, 10, 13, 12, 11);
	mesh->SetFace (16, 14, 17, 16, 15);
	mesh->SetFace (17, 18, 23, 22, 19);
	mesh->SetFace (18, 0, 21, 8);
	mesh->SetFace (19, 1, 11, 12);
	mesh->SetFace (20, 2, 15, 16);
	mesh->SetFace (21, 3, 19, 22);
	mesh->SetFace (22, 4, 9, 20);
	mesh->SetFace (23, 7, 13, 10);
	mesh->SetFace (24, 6, 17, 14);
	mesh->SetFace (25, 5, 23, 18);

	return mesh;
}

/*
//
// Arch (triangles)
//
Mesh* CreateArch (void)
{
	int n=50;
	printf ("-> %d %d\n", 16+2*(n-1), 22 + 4 + 4*(n-2) + 2 + 4);
	Mesh *mesh = new Mesh (16+2*(n-1), 22 + 4 + 4*(n-2) + 2 + 4);

	float door_width = 3.;
	float door_height = 6.;
	float column_thickness = 2.;
	float column_height = 2.;

	//
	// vertices
	//
	// bottom left
	mesh->SetVertex (0, -door_width-column_thickness, -column_thickness/2., 0.);
	mesh->SetVertex (1, -door_width-column_thickness,  column_thickness/2., 0.);
	mesh->SetVertex (2, -door_width,                   column_thickness/2., 0.);
	mesh->SetVertex (3, -door_width,                  -column_thickness/2., 0.);

	// bottom right
	mesh->SetVertex (4, door_width+column_thickness, -column_thickness/2., 0.);
	mesh->SetVertex (5, door_width+column_thickness,  column_thickness/2., 0.);
	mesh->SetVertex (6, door_width,                   column_thickness/2., 0.);
	mesh->SetVertex (7, door_width,                  -column_thickness/2., 0.);

	// top
	mesh->SetVertex (8,  -door_width-column_thickness, -column_thickness/2., door_height);
	mesh->SetVertex (9,  -door_width-column_thickness,  column_thickness/2., door_height);
	mesh->SetVertex (10,  door_width+column_thickness,  column_thickness/2., door_height);
	mesh->SetVertex (11,  door_width+column_thickness, -column_thickness/2., door_height);

	// middle
	mesh->SetVertex (12, -door_width, -column_thickness/2., column_height);
	mesh->SetVertex (13, -door_width,  column_thickness/2., column_height);
	mesh->SetVertex (14,  door_width,  column_thickness/2., column_height);
	mesh->SetVertex (15,  door_width, -column_thickness/2., column_height);

	//
	// faces
	//

	// bottom
	mesh->SetFace (0, 0, 1, 2);
	mesh->SetFace (1, 0, 2, 3);

	mesh->SetFace (2, 4, 6, 5);
	mesh->SetFace (3, 4, 7, 6);

	// up
	mesh->SetFace (4, 8, 10, 9);
	mesh->SetFace (5, 8, 11, 10);

	// side left
	mesh->SetFace (6, 0, 8, 1);
	mesh->SetFace (7, 1, 8, 9);

	// side right
	mesh->SetFace (8, 4, 5, 10);
	mesh->SetFace (9, 4, 10, 11);
	
	// front left
	mesh->SetFace (10, 12, 0, 3);
	mesh->SetFace (11, 12, 8, 0);

	// front right
	mesh->SetFace (12, 15, 7, 4);
	mesh->SetFace (13, 15, 4, 11);

	// back left
	mesh->SetFace (14, 13, 2, 1);
	mesh->SetFace (15, 13, 1, 9);

	// back right
	mesh->SetFace (16, 14, 5, 6);
	mesh->SetFace (17, 14, 10, 5);

	// side interior left
	mesh->SetFace (18, 3, 2, 12);
	mesh->SetFace (19, 2, 13, 12);

	// side interior left
	mesh->SetFace (20, 6, 15, 14);
	mesh->SetFace (21, 6, 7, 15);

	unsigned int vn = 16;
	unsigned int fn = 22;

	//
	// arch
	//
	// link extremities
	for (unsigned int i=1; i<n; i++)
	{
		mesh->SetVertex (vn,
				 door_width*cos(3.14159-i*3.14159/n),
				 -column_thickness/2.,
				 column_height + door_width*sin(3.14159-i*3.14159/n));
		mesh->SetVertex (vn+1,
				 door_width*cos(3.14159-i*3.14159/n),
				 column_thickness/2.,
				 column_height + door_width*sin(3.14159-i*3.14159/n));
		if (i==1)
		{
			// under
			mesh->SetFace (fn++, 12, 13, vn);
			mesh->SetFace (fn++, vn, 13, vn+1);
			
			// front
			mesh->SetFace (fn++, 8, 12, vn);

			// back
			mesh->SetFace (fn++, 13, 9, vn+1);
		}
		else
		{
			// under
			mesh->SetFace (fn++, vn-2, vn-1, vn);
			mesh->SetFace (fn++, vn, vn-1, vn+1);

			// front
			mesh->SetFace (fn++, vn-2, vn, (i-1<n/2)?8:11);
			if ((i-1<n/2) != (i-2<n/2))
				mesh->SetFace (fn++, 11, 8, vn-2);
				

			// back
			mesh->SetFace (fn++, vn+1, vn-1, (i-1<n/2)?9:10);
			if ((i-1<n/2) != (i-2<n/2))
				mesh->SetFace (fn++, 9, 10, vn-1);
		}
		if (i==n-1)
		{
			mesh->SetFace (fn++, vn, vn+1, 15);
			mesh->SetFace (fn++, 15, vn+1, 14);
			
			// front
			mesh->SetFace (fn++, 10, 14, vn+1);

			// back
			mesh->SetFace (fn++, 15, 11, vn);
		}
		vn+=2;
	}

	return mesh;
}
*/

Mesh* CreateArch (void)
{
	int n=40;
	Mesh *mesh = new Mesh (16+2*(n-1), 11 + 3 + 3*(n-2) + 2 + 3);

	float door_width = 3.;
	float door_height = 6.;
	float column_thickness = 2.;
	float column_height = 2.;

	unsigned int vn = 0;
	unsigned int fn = 0;

	//
	// vertices
	//
	// bottom left
	mesh->SetVertex (vn++, -door_width-column_thickness, -column_thickness/2., 0.);
	mesh->SetVertex (vn++, -door_width-column_thickness,  column_thickness/2., 0.);
	mesh->SetVertex (vn++, -door_width,                   column_thickness/2., 0.);
	mesh->SetVertex (vn++, -door_width,                  -column_thickness/2., 0.);

	// bottom right
	mesh->SetVertex (vn++, door_width+column_thickness, -column_thickness/2., 0.);
	mesh->SetVertex (vn++, door_width+column_thickness,  column_thickness/2., 0.);
	mesh->SetVertex (vn++, door_width,                   column_thickness/2., 0.);
	mesh->SetVertex (vn++, door_width,                  -column_thickness/2., 0.);

	// top
	mesh->SetVertex (vn++,  -door_width-column_thickness, -column_thickness/2., door_height);
	mesh->SetVertex (vn++,  -door_width-column_thickness,  column_thickness/2., door_height);
	mesh->SetVertex (vn++,  door_width+column_thickness,  column_thickness/2., door_height);
	mesh->SetVertex (vn++,  door_width+column_thickness, -column_thickness/2., door_height);

	// middle
	mesh->SetVertex (vn++, -door_width, -column_thickness/2., column_height);
	mesh->SetVertex (vn++, -door_width,  column_thickness/2., column_height);
	mesh->SetVertex (vn++,  door_width,  column_thickness/2., column_height);
	mesh->SetVertex (vn++,  door_width, -column_thickness/2., column_height);

	//
	// faces
	//

	// bottom
	mesh->SetFace (fn++, 0, 1, 2, 3);
	mesh->SetFace (fn++, 4, 7, 6, 5);

	// up
	mesh->SetFace (fn++, 8, 11, 10, 9);

	// side left
	mesh->SetFace (fn++, 0, 8, 9, 1);

	// side right
	mesh->SetFace (fn++, 4, 5, 10, 11);
	
	// front left
	mesh->SetFace (fn++, 12, 8, 0, 3);

	// front right
	mesh->SetFace (fn++, 15, 7, 4, 11);

	// back left
	mesh->SetFace (fn++, 13, 2, 1, 9);

	// back right
	mesh->SetFace (fn++, 14, 10, 5, 6);

	// side interior left
	mesh->SetFace (fn++, 2, 13, 12, 3);

	// side interior left
	mesh->SetFace (fn++, 6, 7, 15, 14);

	//
	// arch
	//
	// link extremities
	for (unsigned int i=1; i<n; i++)
	{
		mesh->SetVertex (vn,
				 door_width*cos(3.14159-i*3.14159/n),
				 -column_thickness/2.,
				 column_height + door_width*sin(3.14159-i*3.14159/n));
		mesh->SetVertex (vn+1,
				 door_width*cos(3.14159-i*3.14159/n),
				 column_thickness/2.,
				 column_height + door_width*sin(3.14159-i*3.14159/n));
		if (i==1)
		{
			// under
			mesh->SetFace (fn++, 12, 13, vn+1, vn);
			
			// front
			mesh->SetFace (fn++, 8, 12, vn);

			// back
			mesh->SetFace (fn++, 13, 9, vn+1);
		}
		else
		{
			// under
			mesh->SetFace (fn++, vn-2, vn-1, vn+1, vn);

			// front
			mesh->SetFace (fn++, vn-2, vn, (i-1<n/2)?8:11);
			if ((i-1<n/2) != (i-2<n/2))
				mesh->SetFace (fn++, 11, 8, vn-2);
				

			// back
			mesh->SetFace (fn++, vn+1, vn-1, (i-1<n/2)?9:10);
			if ((i-1<n/2) != (i-2<n/2))
				mesh->SetFace (fn++, 9, 10, vn-1);
		}
		if (i==n-1)
		{
			mesh->SetFace (fn++, vn, vn+1, 14, 15);
			
			// front
			mesh->SetFace (fn++, 10, 14, vn+1);

			// back
			mesh->SetFace (fn++, 15, 11, vn);
		}
		vn+=2;
	}

	return mesh;
}

Mesh* CreateArch2 (void)
{
	Mesh *mesh = new Mesh (360, 0);

	float fWidth = 3.;
	float fTop = 4.;
	float fRadius = 4.;
	
	vec2 vCenter;
	vec2_init (vCenter, -fWidth/4., -fTop/2.);

	unsigned int vn=0;
	for (unsigned int i=0; i<360; i++)
	  mesh->SetVertex (vn++, fRadius*cos((float)(i)), fRadius*sin((float)(i)), 0.);
	  

	return mesh;
}


int create_arc_circle (float center[2], float radius, float angles[2], unsigned int npts, float **p, float **tgt)
{
	float *_p = *p;
	float *_tgt = *tgt;
	
	for (int i=0; i<npts; i++)
	{
		float alpha = angles[0] + (angles[1]-angles[0]) * i / (npts - 1);
		_p[2*i]   = center[0]+radius*cos(alpha);
		_p[2*i+1] = center[1]+radius*sin(alpha);
	}

	return 0;
}

//
// a = width/2.
// b = height
// 
int create_arc_brise (float a, float b, unsigned int npts, float **p, float **tgt, float offset)
{
	float *_p = *p;
	float *_tgt = *tgt;
	unsigned int hnpts = npts/2;
	
	a += offset;
	b += offset;

	vec2 C;
	vec2_init (C, (a*a - b*b)/(2.*a), 0.);
	float r = a-C[0];
	float alpha_max = atan (-b/C[0]);
	//printf ("[create_arc_brise] %f %f / %f\n", C[0], C[1], r);

	float x, y;
	for (int i=0; i<hnpts; i++)
	{
		float alpha = alpha_max * i / (hnpts - 1);
		_p[2*i] = C[0]+r*cos(alpha);
		_p[2*i+1] = C[1]+r*sin(alpha);

		// symmetry
		_p[2*(npts-1-i)]   = -_p[2*i];
		_p[2*(npts-1-i)+1] = _p[2*i+1];
	}

	return 0;
}

// http://ens.math.univ-montp2.fr/SPIP/irem/archi/mathtxt/arcaccolade.php
int create_arc_accolade (float a, float b, float e, unsigned int npts, float **p, float **n)
{
	float *_p = *p;
	float *_n = *n;
	unsigned int hnpts = npts/2;

	float x, y;

	a = 5.; // Ox
	b = 3.; // Oy
	e = 0.3; // 0. (0) -> 1. (A)

	vec2 A, B, E, O1, O2;
	vec2_init (A, a, 0.);
	vec2_init (B, 0., b);
	vec2_init (E, a*e, b*(1.-e));
	vec2_init (O1, a*e, 0.5*(b*b-a*a)*(1-e)/b);
	vec2_init (O2, a*e, 0.5*(a*a-b*b)*e/b+b);
	
	vec2 O1A, O1E, O2E, O2B;
	vec2_subtraction (O1A, A, O1);
	vec2_subtraction (O1E, E, O1);
	vec2_subtraction (O2E, E, O2);
	vec2_subtraction (O2B, B, O2);
	float r1 = vec2_length (O1E);
	float r2 = vec2_length (O2E);

	float alpha;
	float alpha1 = -atan (O1[1]/a);
	for (int i=0; i<hnpts; i++)
	{
		alpha = alpha1 + (.5*3.14159-alpha1) * i / (hnpts - 1);
		x = O1[0]+r1*cos(alpha);
		y = O1[1]+r1*sin(alpha);
		_p[2*i]   = x;
		_p[2*i+1] = y;
	}

	float alpha2 = 3.14159 + atan ((O2[1]-b)/O2[0]);
	for (int i=0; i<hnpts; i++)
	{
		alpha = alpha2 + (.5*3.*3.14159-alpha2) * i / (hnpts - 1);
		x = O2[0]+r2*cos(alpha);
		y = O2[1]+r2*sin(alpha);
		_p[2*(hnpts+i)]   = x;
		_p[2*(hnpts+i)+1] = y;
	}

	return 0;
}

// http://ens.math.univ-montp2.fr/SPIP/irem/archi/mathtxt/arcanse.php
int create_arc_anse_de_panier (float a, float e, unsigned int npts, float **p, float **n)
{
	float *_p = *p;
	float *_n = *n;

	vec2 O0, O1, O2;

	vec2_init (O1,  a*e, 0.);
	vec2_init (O2, -a*e, 0.);
	vec2_init (O0, 0., -a*e*tan(3.14159/3.));
	float r = a*(1.-e);
	float rc = r+a*e*sqrt(1+tan(3.14159/3.)*tan(3.14159/3.));

	for (int i=0; i<npts; i++)
	{
		float alpha = 3.14159 * i / (npts - 1);
		if (i < npts/3.) // left portion
		{
			_p[2*i] = O1[0]+r*cos(alpha);
			_p[2*i+1] = O1[1]+r*sin(alpha);
		}
		else if (i > 2*npts/3.) // right portion
		{
			_p[2*i] = O2[0]+r*cos(alpha);
			_p[2*i+1] = O2[1]+r*sin(alpha);
		}
		else // central portion
		{
			_p[2*i] = O0[0]+rc*cos(alpha);
			_p[2*i+1] = O0[1]+rc*sin(alpha);
		}

		_n[2*i] = -sin(alpha);
		_n[2*i+1] = cos(alpha);
	}
/*	
	// right portion
	for (float alpha=0.; alpha<3.14159/3.; alpha+=0.01)
	{
		_p[2*vi] = O1[0]+r*cos(alpha);
		_p[2*vi+1] = O1[1]+r*sin(alpha);
		//printf ("v %f %f 0.\n", O1[0]+r*cos(alpha), O1[1]+r*sin(alpha));
	}
	// central portion
	for (float alpha=3.14159/3.; alpha<2.*3.14159/3.; alpha+=0.01)
	{
		_p[2*] = O0[0]+rc*cos(alpha);
		_p[2*] = O0[1]+rc*sin(alpha);
		//printf ("v %f %f 0.\n", O0[0]+rc*cos(alpha), O0[1]+rc*sin(alpha));x
	}
	// left portion
	for (float alpha=2.*3.14159/3.; alpha<3.14159; alpha+=0.01)
	{
		_p[2*] = O2[0]+r*cos(alpha);
		_p[2*] = O2[1]+r*sin(alpha);
		//printf ("v %f %f 0.\n", O2[0]+r*cos(alpha), O2[1]+r*sin(alpha));
	}
*/
	return 0;
}

// http://ens.math.univ-montp2.fr/SPIP/irem/archi/mathtxt/arcram.php
int create_arc_rampant (float a, float t, unsigned int npts, float **p, float **n)
{
	float *_p = *p;
	float *_n = *n;

	a = 1.;
	t = 0.6;

	vec2 O1, O2;
	vec2_init (O1,  0., 0.);
	vec2_init (O2,  0., t*a);
	
	int nh = npts/2;
	int vi = 0;
	
	// left portion : 3.14159 <= alpha < 3.14159/2
	for (vi=0; vi<nh; vi++)
	{
		float alpha = 3.14159 * (1. - (float)vi/(2*nh));
		_p[2*vi] = O1[0]+a*cos(alpha);
		_p[2*vi+1] = O1[1]+a*sin(alpha);
		
		_n[2*vi] = -sin(alpha);
		_n[2*vi+1] = cos(alpha);
	}
	
	// right portion : 3.14159/2 <= alpha <= 0
	for (vi=0; vi<npts-nh; vi++)
	{
		float alpha = 0.5 * 3.14159 * (1. - (float)vi/npts);
		_p[2*(nh+vi)] = O2[0]+a*(1.-t)*cos(alpha);
		_p[2*(nh+vi)+1] = O2[1]+a*(1.-t)*sin(alpha);
		
		_n[2*(nh+vi)] = -sin(alpha);
		_n[2*(nh+vi)+1] = cos(alpha);
	}
	
	return 0;
}

int extrude_moulure_along_curve (unsigned int npcurve, float *pcurve, float *tcurve,
				 unsigned int npmoulure, float *pmoulure,
				 float **points, unsigned int **faces)
{
	float *_points = *points;
	unsigned int *_faces = *faces;
	for (int i=0; i<npcurve; i++)
	{
		vec3 c;
		vec3_init (c, pcurve[2*i], 0., pcurve[2*i+1]);
		for (int j=0; j<npmoulure; j++)
		{
			float alpha = 2.*3.14159*j/npmoulure;
			vec3 m;
			vec3_init (m, pmoulure[2*i], pmoulure[2*i+1], 0.);
			
			// tangent
			vec3 t;
			vec3_init (t, tcurve[0], 0., tcurve[1]);
			vec3_normalize (t);
			
			// binormal (= T ^ N)
			vec3 b;
			vec3_init (b, 0., -1., 0.);

			// normal (= B ^ T)
			vec3 n;
			vec3_cross_product (n, b, t);
			vec3_normalize (n);
			
			float r = vec3_length (m);
			
			float x = c[0] + r*(-n[0]*cos(alpha) + b[0]*sin(alpha));
			float y = c[1] + r*(-n[1]*cos(alpha) + b[1]*sin(alpha));
			float z = c[2] + r*(-n[2]*cos(alpha) + b[2]*sin(alpha));

			_points[3*(npmoulure*i+j)]   = x;
			_points[3*(npmoulure*i+j)+1] = y;
			_points[3*(npmoulure*i+j)+2] = z;
			printf ("v %f %f %f\n", x, y, z);
		}
	}

	return 0;
}

ArcBrise::ArcBrise ()
{
	m_fAltitude = -1.;
	m_fWidth = -1.;
	m_fHeight = -1.;
	m_fAltitude2 = -1.;
	m_fWidth2 = -1.;
	m_fHeight2 = -1.;
}

ArcBrise::~ArcBrise ()
{
}

void ArcBrise::SetPrincipalArc (float altitude, float width, float height)
{
	m_fAltitude = altitude;
	m_fWidth = width;
	m_fHeight = height;
}

void ArcBrise::SetSecondArc (float altitude2, float width2, float height2)
{
	m_fAltitude2 = altitude2;
	m_fWidth2 = width2;
	m_fHeight2 = height2;
}


Rosace::Rosace ()
{
	m_nFoils = 3;
	
}

Rosace::~Rosace ()
{
}

Polygon2* Rosace::Generate (void)
{
	unsigned int resolution = 20;
	Polygon2 *pol = new Polygon2 ();
	float *p = pol->add_contour (0, m_nFoils*resolution, NULL);

	float alpha = 2.*M_PI/m_nFoils;
	float halpha = M_PI/m_nFoils;
	float radius = sin(halpha) / (1. + sin(halpha));
	vec2 center;
	vec2_init (center,
		   (1.-radius)*cos(halpha),
		   radius);

	for (int i=0; i<resolution; i++)
	{
		float a = -.5*M_PI + i*(M_PI+alpha)/resolution;
		float x = center[0] + radius*cos(a);
		float y = center[1] + radius*sin(a);
		p[2*i]   = x;
		p[2*i+1] = y;
	}

	for (int i=1; i<m_nFoils; i++)
	{
		float theta = i * alpha;
		mat2 rot;
		mat2_init (rot,
			   cos (theta), -sin (theta),
			   sin (theta),  cos (theta));
		for (int j=0; j<resolution; j++)
		{
			vec2 v;
			vec2_init (v, p[2*j], p[2*j+1]);
			mat2_transform (v, rot, v);
			p[2*resolution*i + 2*j]   = v[0];
			p[2*resolution*i + 2*j+1] = v[1];
		}
	}

	return pol;
}

//
// TODO :
// - treat case when the altitudes are not null
//
Polygon2* ArcBrise::Generate (void)
{
	unsigned int npts = 32;
	float *p = (float*)malloc(3*npts*sizeof(float));
	float *t = (float*)malloc(3*npts*sizeof(float));

	if (m_fWidth2 > m_fWidth/2.)
	{
		printf ("!!! m_fWidth2 > m_fWidth/2.");
		return NULL;
	}

	// principal arc
	create_arc_brise (m_fWidth/2., m_fHeight, npts, &p, &t, 0.);
	Polygon2 *pol = new Polygon2 ();
	pol->add_contour (0, 3*npts, NULL);
	for (int i=0; i<npts; i++)
		pol->set_point (0, i, p[2*i], m_fAltitude + p[2*i+1]);

	// second arc
	create_arc_brise (m_fWidth2/2., m_fHeight2, npts, &p, &t, 0.);
	for (int i=0; i<npts; i++)
	{
		pol->set_point (0, npts+i, p[2*(npts-1-i)]-m_fWidth/4., m_fAltitude2 + p[2*(npts-1-i)+1]);
		pol->set_point (0, 2*npts+i, p[2*(npts-1-i)]+m_fWidth/4., m_fAltitude2 + p[2*(npts-1-i)+1]);
	}

	//
	// rosace
	//
	// evaluate the locus of points equidistant from two circles => ellipse
	//
	// references :
	// http://www.mathokay.com/equidistant_locus.htm
	// http://www.geogebra.org/forum/viewtopic.php?f=2&t=23035
	//

	// principal circle
	float a, b;
	a = m_fWidth/2.;
	b = m_fHeight;
	vec2 c_principal;
	vec2_init (c_principal, (a*a - b*b)/(2.*a), m_fAltitude);
	float r_principal = a-c_principal[0];
	//printf ("principal : %f %f - %f\n", c_principal[0], c_principal[1], r_principal);

	// second circle
	vec2 c_second;
	a = m_fWidth2/2.;
	b = m_fHeight2;
	vec2_init (c_second, (a*a - b*b)/(2.*a), m_fAltitude2);
	float r_second = a-c_second[0];
	c_second[0] -= m_fWidth/4.;
	//printf ("second : %f %f - %f\n", c_second[0], c_second[1], r_second);
	
	// middle
	vec2 c_mid;
	vec2_addition (c_mid, c_principal, c_second);
	vec2_scalar (c_mid, c_mid, .5);
	//printf ("middle : %f %f\n", c_mid[0], c_mid[1]);

	// ellipse equation : x^2/a^2 + y^2/b^2 = 1 (centered on c_mid)
	a = (r_principal + r_second) / 2.;

	vec2 tmp;
	vec2_subtraction (tmp, c_principal, c_second);
	float l = vec2_length (tmp)/2.;
	b = sqrt (a*a - l*l);
	//printf ("ellipse : a = %f    b = %f\n", a, b);

	// and the translation is :
	float x_trans = c_mid[0];
	//printf ("x_trans = %f\n", -x_trans);

	// solve the equation with x = x_trans
	float rosace_y = b * sqrt (1. - x_trans*x_trans/(a*a));
	printf ("rosace_y = %f\n", rosace_y);

	// check the solution
	vec2_init (tmp, c_principal[0], c_principal[1]-rosace_y);
	float rosace_radius = r_principal - vec2_length (tmp);
	//printf ("rosace_radius = %f\n", rosace_radius);

	// alternate
	vec2_init (tmp, c_second[0], c_second[1]-rosace_y);
	rosace_radius = vec2_length (tmp) - r_second;
	//printf ("rosace_radius = %f\n", rosace_radius);

/*
	Img *pImg = new Img (1200, 1200);
	float s = 2.;

	pImg->draw_circle (600, 600,    (int)10/2., 0, 0, 0, 255);

	pImg->draw_circle (600+c_principal[0]/s, 600+c_principal[1]/s,    (int)10/2., 255, 0, 0, 255);
	pImg->draw_circle (600+c_principal[0]/s, 600+c_principal[1]/s,    (int)r_principal/s, 255, 0, 0, 255);

	pImg->draw_circle (600+c_second[0]/s, 600+c_second[1]/s,    (int)10/2., 0, 255, 0, 255);
	pImg->draw_circle (600+c_second[0]/s, 600+c_second[1]/s,    (int)r_second/s, 0, 255, 0, 255);

	pImg->draw_circle (600+c_mid[0]/s, 600+c_mid[1]/s,    (int)10/2., 0, 0, 255, 255);
	pImg->draw_ellipse (600+c_mid[0]/s, 600+c_mid[1]/s,    a/s, b/s, 0., 0., 255, 255);
	pImg->draw_circle (600, 600-rosace_y/s,    (int)10/2., 0, 0, 255, 255);

	pImg->save ("toto.png");
*/

	pol->add_contour (1, npts, NULL);
	float c[2] = {0., rosace_y};
	float angles[2] = {0., 2.*3.14159};
	create_arc_circle (c, rosace_radius, angles, npts, &p, &t);
	for (int i=0; i<npts; i++)
		pol->set_point (1, i, p[2*(npts-i-1)], p[2*(npts-i-1)+1]);

	pol->output ((char*)"polygon1.ppm");
	return pol;
	//pol->dump ();



//	return;
	float *_pVertices;
	unsigned int _nVertices;
	unsigned int *_pFaces;
	unsigned int _nFaces;
	pol->tesselate (&_pVertices, &_nVertices, &_pFaces, &_nFaces);
	FILE *f = fopen ("arc_offset.obj", "w");
	for (int i=0; i<_nVertices; i++)
		fprintf (f, "v %f %f %f\n", _pVertices[3*i], _pVertices[3*i+1], _pVertices[3*i+2]);
	for (int i=0; i<_nFaces; i++)
		fprintf (f, "f %d %d %d\n", 1+_pFaces[3*i], 1+_pFaces[3*i+1], 1+_pFaces[3*i+2]);
	fclose (f);
}

