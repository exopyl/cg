#pragma once
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
	virtual ~ParametricSurface () {};

public:
	virtual bool EvaluatePosition (float u, float v, diff_s *diff) = 0;
	bool Generate (void);
	bool EvaluateFundamentalForms (diff_s *diff, vec3 dfdu, vec3 dfdv, vec3 dfdudu, vec3 dfdudv, vec3 dfdvdv);
	Tensor* EvaluateTensor (diff_s diff);
	
	unsigned int iNU;
	unsigned int iNV;
	bool bCloseU = false; // do we close the u border ?
	bool bCloseV = false; // do we close the v border ?
	bool bIndependentCloseU = false; // do we close independently the extremities u=0 and u=1 ?
	bool bIndependentCloseV = false; // do we close independently the extremities v=0 and v=1 ?
	bool bInverseCloseU = false;
	bool bInverseCloseV = false;
	float fParams[4];

private:
	void AddFace (unsigned int& fi,
		      unsigned int v1, unsigned int v2, unsigned int v3,
		      float u1, float v_1, float u2, float v_2, float u3, float v_3);
};

//
class ParametricSphere : public ParametricSurface
{
public:
	ParametricSphere (unsigned int nu=20, unsigned int nv=20);
	~ParametricSphere () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class EllipticHelicoid : public ParametricSurface
{
public:
	EllipticHelicoid (unsigned int nu=20, unsigned int nv=20, float a=1.0f, float b=1.0f, float c=0.2f);
	~EllipticHelicoid () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class SeaShell : public ParametricSurface
{
public:
	SeaShell (unsigned int nu=20, unsigned int nv=20);
	~SeaShell () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class SeaShellVonSeggern : public ParametricSurface
{
public:
	SeaShellVonSeggern (unsigned int nu=20, unsigned int nv=20, float a=0.2f, float b=1.0f, float c=0.1f, float n=2.0f);
	~SeaShellVonSeggern () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class CorkscrewSurface : public ParametricSurface
{
public:
	CorkscrewSurface (unsigned int nu=20, unsigned int nv=20, float a=1.0f, float b=0.5f);
	~CorkscrewSurface () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class MobiusStrip : public ParametricSurface
{
public:
	MobiusStrip (unsigned int nu=20, unsigned int nv=20, float w=0.1f, float r=0.5f);
	~MobiusStrip () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

// 
class RadialWave : public ParametricSurface
{
public:
	RadialWave (unsigned int nu=20, unsigned int nv=20, float radius=10.0f, float height=20.0f, float frequency=0.6f);
	~RadialWave () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

//
class ParametricTorus : public ParametricSurface
{
public:
	ParametricTorus (unsigned int nu=20, unsigned int nv=20, float radius1=5.0f, float radius2=2.0f);
	~ParametricTorus () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
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
	bool EvaluatePosition (float u, float v, diff_s *diff);
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
	BorromeanRing (unsigned int nu=20, unsigned int nv=20, float param1=2.0f, float param2=1.0f, float r=0.2f);
	~BorromeanRing () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class BorromeanRings : public Mesh
{
public:
	BorromeanRings (unsigned int nu=20, unsigned int nv=20);
	~BorromeanRings () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};
/*
Mesh* mesh_generate_parametric_surface_borromean_elliptical_rings (unsigned int nu, unsigned int nv);
*/

class TorusKnot : public ParametricSurface
{
public:
	TorusKnot (unsigned int nu=20, unsigned int nv=20, unsigned int a=3, unsigned int b=4);
	~TorusKnot () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class CinquefoilKnot : public ParametricSurface
{
public:
	CinquefoilKnot (unsigned int nu=20, unsigned int nv=20, unsigned int a=3);
	~CinquefoilKnot () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

//
class TrefoilKnot1 : public ParametricSurface
{
public:
	TrefoilKnot1 (unsigned int nu=20, unsigned int nv=20);
	~TrefoilKnot1 () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

//
class TrefoilKnot2 : public ParametricSurface
{
public:
	TrefoilKnot2 (unsigned int nu=20, unsigned int nv=20);
	~TrefoilKnot2 () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};



//
class HyperbolicParaboloid : public ParametricSurface
{
public:
	HyperbolicParaboloid (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~HyperbolicParaboloid () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class MonkeySaddle : public ParametricSurface
{
public:
	MonkeySaddle (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~MonkeySaddle () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class Blobs : public ParametricSurface
{
public:
	Blobs (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~Blobs () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class Drop : public ParametricSurface
{
public:
	Drop (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~Drop () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class Wave1 : public ParametricSurface
{
public:
	Wave1 (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~Wave1 () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class Wave2 : public ParametricSurface
{
public:
	Wave2 (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~Wave2 () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class Weight : public ParametricSurface
{
public:
	Weight (unsigned int nu=20, unsigned int nv=20, float xmin=-5.0f, float xmax=5.0f, float ymin=-5.0f, float ymax=5.0f);
	~Weight () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

class Guimard : public ParametricSurface
{
public:
	Guimard (unsigned int nu=20, unsigned int nv=20, float a=2.0f, float b=3.0f, float c=1.0f);
	~Guimard () {};
	bool EvaluatePosition (float u, float v, diff_s *diff);
};

