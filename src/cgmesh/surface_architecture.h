#ifndef __SURFACE_ARCHITECTURE_H__
#define __SURFACE_ARCHITECTURE_H__

#include "mesh.h"
#include "polygon2.h"

extern Mesh* CreateArch (void);
extern Mesh* CreateArch2 (void);
extern Mesh* CreateBlock (float width = 1.61803399, float height = 1., float depth = 1., float bevel = 0.1);

extern int create_arc_brise (float a, float b, unsigned int npts, float **p, float **tgt, float offset);
extern int create_arc_accolade (float a, float b, float e, unsigned int npts, float **p, float **tgt);
extern int create_arc_anse_de_panier (float a, float e, unsigned int npts, float **p, float **tgt);
extern int create_arc_rampant (float a, float t, unsigned int npts, float **p, float **tgt);

extern int extrude_moulure_along_curve (unsigned int npcurve, float *pcurve, float *tcurve,
					unsigned int npmoulure, float *pmoulure,
					float **points, unsigned int **faces);

class Rosace
{
public:
	Rosace ();
	~Rosace ();
	
	inline void SetNFoils (unsigned int nFoils) { m_nFoils = nFoils; };
	Polygon2* Generate (void);

private:
	unsigned int m_nFoils;
	float m_fRadius;
};

class ArcBrise
{
public:
	ArcBrise ();
	~ArcBrise ();
	void SetPrincipalArc (float altitude, float width, float height);
	void SetSecondArc (float altitude2, float width2, float height2);
	Polygon2* Generate (void);
	
private:
	float m_fAltitude, m_fWidth, m_fHeight;
	float m_fAltitude2, m_fWidth2, m_fHeight2;
};

#endif // __SURFACE_ARCHITECTURE_H__
