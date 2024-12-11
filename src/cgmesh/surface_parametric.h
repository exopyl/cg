#ifndef __SURFACE_PARAMETRIC_H__
#define __SURFACE_PARAMETRIC_H__

#include "mesh.h"
#include "tensor.h"

typedef struct diff_s {
	vec3 position;
	float dx, dy, dxx, dxy, dyy;
	vec3 normal;
	vec3 first_fundamental_form;
	vec3 second_fundamental_form;
} diff_s;

class ParametricSurface : public Mesh
{
public:
	ParametricSurface () {};
	~ParametricSurface () {};

public:
	virtual int EvaluatePosition (float u, float v, diff_s *diff) = 0;
	int Generate (void);
	int EvaluateFundamentalForms (diff_s *diff, vec3 dfdu, vec3 dfdv, vec3 dfdudu, vec3 dfdudv, vec3 dfdvdv);
	Tensor* EvaluateTensor (diff_s diff);
	
	unsigned int iNU;
	unsigned int iNV;
	int bCloseU; // do we close the u border ?
	int bCloseV; // do we close the v border ?
	int bIndependentCloseU; // do we close independently the extremities u=0 and u=1 ?
	int bIndependentCloseV; // do we close independently the extremities v=0 and v=1 ?
	int bInverseCloseU;
	int bInverseCloseV;
	float fParams[4];
};

//
class ParametricSphere : public ParametricSurface
{
public:
	ParametricSphere (unsigned int nu=20, unsigned int nv=20);
	~ParametricSphere () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class EllipticHelicoid : public ParametricSurface
{
public:
	EllipticHelicoid (unsigned int nu=20, unsigned int nv=20, float a=1., float b=1., float c=0.2);
	~EllipticHelicoid () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class SeaShell : public ParametricSurface
{
public:
	SeaShell (unsigned int nu=20, unsigned int nv=20);
	~SeaShell () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class SeaShellVonSeggern : public ParametricSurface
{
public:
	SeaShellVonSeggern (unsigned int nu=20, unsigned int nv=20, float a=.2, float b=1., float c=.1, float n=2.);
	~SeaShellVonSeggern () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class CorkscrewSurface : public ParametricSurface
{
public:
	CorkscrewSurface (unsigned int nu=20, unsigned int nv=20, float a=1., float b=.5);
	~CorkscrewSurface () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class MobiusStrip : public ParametricSurface
{
public:
	MobiusStrip (unsigned int nu=20, unsigned int nv=20, float w=.1, float r=.5);
	~MobiusStrip () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class RadialWave : public ParametricSurface
{
public:
	RadialWave (unsigned int nu=20, unsigned int nv=20, float radius=10., float height=20., float frequency=.6);
	~RadialWave () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

//
class ParametricTorus : public ParametricSurface
{
public:
	ParametricTorus (unsigned int nu=20, unsigned int nv=20, float radius1=5., float radius2=2.);
	~ParametricTorus () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

/*
Mesh* mesh_generate_parametric_surface_trumpet (unsigned int nu, unsigned int nv,
						  float s, float t);
Mesh* mesh_generate_parametric_surface_cylinder (unsigned int nu, unsigned int nv,
						   float r, float h);
*/
class Breather : public ParametricSurface
{
public:
	Breather (unsigned int nu=20, unsigned int nv=20);
	~Breather () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

/*
// tubes
Mesh* mesh_generate_parametric_surface_helix (unsigned int nu, unsigned int nv);
*/

//
class BorromeanRing : public ParametricSurface
{
	friend class BorromeanRings;
public:
	BorromeanRing (unsigned int nu=20, unsigned int nv=20, float param1=2., float param2=1., float r=.2);
	~BorromeanRing () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class BorromeanRings : public Mesh
{
public:
	BorromeanRings (unsigned int nu=20, unsigned int nv=20);
	~BorromeanRings () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};
/*
Mesh* mesh_generate_parametric_surface_borromean_elliptical_rings (unsigned int nu, unsigned int nv);
*/

class TorusKnot : public ParametricSurface
{
public:
	TorusKnot (unsigned int nu=20, unsigned int nv=20, unsigned int a=3, unsigned int b=4);
	~TorusKnot () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class CinquefoilKnot : public ParametricSurface
{
public:
	CinquefoilKnot (unsigned int nu=20, unsigned int nv=20, unsigned int a=3);
	~CinquefoilKnot () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

//
class TrefoilKnot1 : public ParametricSurface
{
public:
	TrefoilKnot1 (unsigned int nu=20, unsigned int nv=20);
	~TrefoilKnot1 () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

//
class TrefoilKnot2 : public ParametricSurface
{
public:
	TrefoilKnot2 (unsigned int nu=20, unsigned int nv=20);
	~TrefoilKnot2 () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};



//
class HyperbolicParaboloid : public ParametricSurface
{
public:
	HyperbolicParaboloid (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~HyperbolicParaboloid () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class MonkeySaddle : public ParametricSurface
{
public:
	MonkeySaddle (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~MonkeySaddle () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class Blobs : public ParametricSurface
{
public:
	Blobs (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~Blobs () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class Drop : public ParametricSurface
{
public:
	Drop (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~Drop () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class Wave1 : public ParametricSurface
{
public:
	Wave1 (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~Wave1 () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class Wave2 : public ParametricSurface
{
public:
	Wave2 (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~Wave2 () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class Weight : public ParametricSurface
{
public:
	Weight (unsigned int nu=20, unsigned int nv=20, float xmin=-5., float xmax=5., float ymin=-5., float ymax=5.);
	~Weight () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

class Guimard : public ParametricSurface
{
public:
	Guimard (unsigned int nu=20, unsigned int nv=20, float a=2., float b=3., float c=1.);
	~Guimard () {};
	int EvaluatePosition (float u, float v, diff_s *diff);
};

#endif // __SURFACE_PARAMETRIC_H__
