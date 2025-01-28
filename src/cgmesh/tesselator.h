// adapted from http://flipcode.com/archives/Polygon_Tessellation_In_OpenGL.shtml
#ifndef __TESSELATOR_H__
#define __TESSELATOR_H__

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/glu.h>

#include "cgmesh.h"

#include <list>
using namespace std;

// wrapper for GLU tesselator
class Tesselator
{
public:
	Tesselator ();
	~Tesselator ();

	enum TesselationOutput {
		RENDERING = 0,
		MESH			};


	//int DrawPolygon (GLdouble obj_data[][6], int num_vertices);
	void Display (void);
	Mesh* CreateMesh (void);


	int Init(TesselationOutput eTesselationOutput);
	int Set_Winding_Rule(GLenum winding_rule);

	// fill the member data
	typedef struct vertex_s
	{
		float x, y, z;
	} vertex_s;

	void NewPolygon ()
	{
		m_listCurrent.clear ();
	}
	void EndPolygon ()
	{
		CopyCurrentVertices ();
	}
	void NewContour ()
	{
		CopyCurrentVertices ();
	};
	void AddVertex (float x, float y, float z)
	{
		vertex_s v;
		v.x = x;
		v.y = y;
		v.z = z;
		m_listCurrent.push_back (v);
	}
	void DumpPolygon ()
	{
		std::list<std::list<vertex_s> >::iterator itContour;
		printf ("Dump polygon : %d contours\n", m_listContours.size ());
		for (itContour = m_listContours.begin(); itContour != m_listContours.end (); itContour++)
		{
			std::list<vertex_s> contour = (*itContour);
			printf ("-> New contour : %d vertices\n", contour.size ());

			std::list<vertex_s>::iterator itVertex;
			for (itVertex = contour.begin(); itVertex != contour.end (); itVertex++)
			{
				vertex_s v = (*itVertex);
				printf ("%f %f %f\n", v.x, v.y, v.z);
			}
		}
	}
	
private:
	GLUtesselator *tobj;

	unsigned int GetNumberVertices (void)
	{
			unsigned int nVertices = 0;
		std::list<std::list<vertex_s> >::iterator itContour;
		for (itContour = m_listContours.begin(); itContour != m_listContours.end (); itContour++)
		{
			std::list<vertex_s> contour = (*itContour);
			nVertices += contour.size ();
		}
		return nVertices;
	}

	std::list<std::list<vertex_s> > m_listContours;

	void CopyCurrentVertices (void)
	{
		if (!m_listCurrent.empty ())
		{
			std::list<vertex_s> listNewContour;
			for (std::list<vertex_s>::iterator itVertex = m_listCurrent.begin(); itVertex != m_listCurrent.end (); itVertex++)
			{
				listNewContour.push_back ((*itVertex));
			}
			m_listContours.push_back (listNewContour);
		}
		m_listCurrent.clear ();
	};
	std::list<vertex_s> m_listCurrent;
};

#endif // WIN32
#endif // __TESSELATOR_H__
