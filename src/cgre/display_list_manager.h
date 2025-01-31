#pragma once

#include "../cgmesh/cgmesh.h"

#include <GL/gl.h>

class DisplayListManager
{
public:
	DisplayListManager ()
	{
		m_idCurrent = 0;
	};
	~DisplayListManager () {};

	unsigned int addMesh (Mesh *mesh)
	{
		int id = glGenLists (1);
		glNewList (id, GL_COMPILE_AND_EXECUTE);

		//glPolygonMode (GL_FRONT_AND_BACK, GL_POINT);
		//glLineWidth (2.0);

		float *vertices = mesh->m_pVertices;
		float *verticesNormals = mesh->m_pVertexNormals;
		float *verticesColors = mesh->m_pVertexColors;
		int nFaces = mesh->m_nFaces;
		Face **faces = mesh->m_pFaces;
		int a, b, c;

		glBegin(GL_TRIANGLES);
		for (int i=0; i<nFaces; i++)
		{
			a = 3*faces[i]->m_pVertices[0];
			b = 3*faces[i]->m_pVertices[1];
			c = 3*faces[i]->m_pVertices[2];

			glNormal3f (verticesNormals[a], verticesNormals[a+1], verticesNormals[a+2]);
			if (verticesColors)
				glColor3f (verticesColors[a], verticesColors[a+1], verticesColors[a+2]);
			glVertex3f (vertices[a], vertices[a+1], vertices[a+2]);

			glNormal3f (verticesNormals[b], verticesNormals[b+1], verticesNormals[b+2]);
			if (verticesColors)
				glColor3f (verticesColors[b], verticesColors[b+1], verticesColors[b+2]);
			glVertex3f (vertices[b], vertices[b+1], vertices[b+2]);

			glNormal3f (verticesNormals[c], verticesNormals[c+1], verticesNormals[c+2]);
			if (verticesColors)
				glColor3f (verticesColors[c], verticesColors[c+1], verticesColors[c+2]);
			glVertex3f (vertices[c], vertices[c+1], vertices[c+2]);
		}
		glEnd ();

		glEndList ();

		m_displayLists[m_idCurrent++] = id;
		return m_idCurrent-1;
	}

	unsigned int addCurve (int nVertices, float *vertices)
	{
		int id = glGenLists (1);
		glNewList (id, GL_COMPILE_AND_EXECUTE);

		//glPolygonMode (GL_FRONT_AND_BACK, GL_POINT);
		glLineWidth (2.0);

		glDisable (GL_LIGHTING);
		glBegin(GL_LINE_STRIP);
		//glBegin(GL_LINE_LOOP);
		for (int i=0; i<nVertices; i++)
			glVertex3f (vertices[3*i], vertices[3*i+1], vertices[3*i+2]);
		glEnd ();

		glEndList ();

		m_displayLists[m_idCurrent++] = id;
		return m_idCurrent-1;
	};

#if 0
	// PointSet
	unsigned int addPointSet (PointSet* pointSet)
	{
		int id = glGenLists (1);
		glNewList (id, GL_COMPILE_AND_EXECUTE);

		//glPolygonMode (GL_FRONT_AND_BACK, GL_POINT);
		glPointSize (2.0);

		glDisable (GL_LIGHTING);
		glBegin(GL_POINTS);
		float *v = pointSet->points ();
		for (int i=0; i<pointSet->nPoints (); i++)
		{
			//glColor3f ((v[3*i]+1.0)/2.0, (v[3*i+1]+1.0)/2.0, (v[3*i+2]+1.0)/2.0);
			unsigned int x = (int)(512*(v[3*i]+1.0)/2.0);
			unsigned int y = (int)(512-512*(v[3*i+1]+1.0)/2.0);
			float r = rand ()/255.0;
			float g = rand ()/255.0;
			float b = rand ()/255.0;

			//if (1 || r*b*g != 1)
			if (y > 170 && y < 340)
			{
				glColor3f (r, g, b);
				glVertex3f (v[3*i], v[3*i+1], v[3*i+2]);
			}
		}
		glEnd ();

		glEndList ();

		m_displayLists[m_idCurrent++] = id;
		return m_idCurrent-1;
	};
#endif

	void Draw (int id)
	{
		glCallList (m_displayLists[id]);
	}

private:
	int m_idCurrent;
	int m_displayLists[8];
};
