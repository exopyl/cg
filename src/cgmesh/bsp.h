#ifndef __BSP_H__
#define __BSP_H__

//
// ftp://ftp.sgi.com/other/bspfaq/faq/bspfaq.html
//

#include <list>

#include "../cgmath/cgmath.h"
#include "polygon3.h"

typedef enum {IN_BACK_OF = -1, COINCIDENT, IN_FRONT_OF, SPANNING} eClassification;

class BSP
{
public:
	BSP ();
	~BSP ();

	int Build (list<Polygon3*> polygons);
	const list<Polygon3*> GetPolygons (void) { return m_pPolygons; };
	BSP* GetBack  (void) { return m_pBack;  };
	BSP* GetFront (void) { return m_pFront; };

private:
	int Split_Polygon (Polygon3 *polygon, Polygon3 *front_piece, Polygon3 *back_piece);
	eClassification Classify_Polygon (Polygon3 *polygon);
	int Choose_Plane (list<Polygon3*> polygons);

	Plane *m_pPlane;
	list<Polygon3*> m_pPolygons;
	BSP *m_pFront;
	BSP *m_pBack;
};

#endif // __BSP_H__
