#include "tesselator.h"

#ifdef WIN32
#include <windows.h>
#include <GL/gl.h>

Tesselator::Tesselator ()
{
}

Tesselator::~Tesselator ()
{
}

///////////////////////
//
// Callbacks for rendering
//
GLvoid CALLBACK RenderingVertexCallback(GLvoid *vertex)
{
	GLdouble *ptr;
	ptr = (GLdouble *) vertex;
	glVertex3dv((GLdouble *) ptr);
	glColor3dv((GLdouble *) ptr + 3);
}

GLvoid CALLBACK RenderingCombineCallback (GLdouble coords[3], GLdouble *vertex_data[4], GLfloat weight[4], GLdouble **dataOut)
{
	GLdouble *vertex;
	vertex = (GLdouble *) malloc(6 * sizeof(GLdouble));
	vertex[0] = coords[0];
	vertex[1] = coords[1];
	vertex[2] = coords[2];
	for (int i = 3; i < 6; i++)
	{
		vertex[i] = weight[0] * vertex_data[0][i] +
					weight[1] * vertex_data[1][i] +
					weight[2] * vertex_data[2][i] +
					weight[3] * vertex_data[3][i];
	}
	*dataOut = vertex;
}
//
///////////////////////

///////////////////////
//
// Callbacks for mesh
//
struct glutess_state {
	Mesh *mesh;
	unsigned int vc;
	unsigned int tri[3];
};


static GLvoid CALLBACK MeshEdgeFlagCallback(int unused)
{
	(void) unused;
}


static GLvoid CALLBACK MeshBeginCallback(int type, GLvoid *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	state->vc = 0;
	/*
	 * we only get triangles here because we set
	 * GLU_TESS_EDGE_FLAG callback to non-NULL
	 */
	(void) type;
}

static GLvoid CALLBACK MeshEndCallback(GLvoid *data)
{
	return;
}

static GLvoid CALLBACK MeshVertexCallback(GLvoid *vertex_data, GLvoid *user_data)
{
	struct glutess_state *state = (struct glutess_state *) user_data;
	Mesh *mesh = state->mesh;
	unsigned int vi = (unsigned int) vertex_data;

	state->tri[state->vc] = vi;

	if (state->vc == 2) {
		unsigned int fi;
		
		// generate face
		fi = mesh->m_nFaces++;

		Face *pFace = new Face ();
		pFace->SetTriangle (state->tri[0],
							state->tri[1],
							state->tri[2]);
		mesh->m_pFaces[fi] = pFace;

		state->vc = 0;
	} else
		state->vc++;
}

static GLvoid CALLBACK MeshCombineCallback (GLdouble coords[3], GLdouble *vertex_data[4], GLfloat weight[4], void **dataOut, void *user_data)
{
	printf ("MeshCombineCallback...\n");
	
	struct glutess_state *state = (struct glutess_state *) user_data;
	Mesh *mesh = state->mesh;
	unsigned int vi, v0, v1, v2, v3;

	// weight could be used later to interpolate vertex properties
	(void) weight;

	// generate vertex
	vi = mesh->m_nVertices++;
	mesh->m_pVertices[3*vi+0] = (float) coords[0];
	mesh->m_pVertices[3*vi+1] = (float) coords[1];
	mesh->m_pVertices[3*vi+2] = (float) coords[2];

	v0 = (unsigned int) vertex_data[0];
	v1 = (unsigned int) vertex_data[1];
	v2 = (unsigned int) vertex_data[2];
	v3 = (unsigned int) vertex_data[3];

	*dataOut = (void *) vi;
}

//
///////////////////////

int Tesselator::Set_Winding_Rule(GLenum winding_rule)
{
	// Set the winding rule
	gluTessProperty(tobj, GLU_TESS_WINDING_RULE, winding_rule);
	return(1);
}

void Tesselator::Display(void)
{
	tobj = gluNewTess(); 
	
		gluTessCallback(tobj, GLU_TESS_VERTEX, (GLvoid (__stdcall *)  ( )) &RenderingVertexCallback);
		gluTessCallback(tobj, GLU_TESS_BEGIN, (GLvoid (__stdcall *)  ( )) &glBegin);
		gluTessCallback(tobj, GLU_TESS_END, (GLvoid (__stdcall *)  ( )) &glEnd);
		gluTessCallback(tobj, GLU_TESS_COMBINE, (GLvoid (__stdcall *)  ( ))&RenderingCombineCallback);
	
		gluTessNormal(tobj, 0, 0, 1);

		gluTessBeginPolygon(tobj, NULL);
		std::list<std::list<vertex_s>>::iterator itContour;

		int i = 0;
		//printf ("Dump polygon : %d contours\n", m_listContours.size ());
		for (itContour = m_listContours.begin(); itContour != m_listContours.end (); itContour++)
		{
			std::list<vertex_s> contour = (*itContour);
			//printf ("-> New contour : %d vertices\n", contour.size ());
			
			gluTessBeginContour(tobj);
			std::list<vertex_s>::iterator itVertex;
			for (itVertex = contour.begin(); itVertex != contour.end (); itVertex++)
			{
				vertex_s v = (*itVertex);

				GLdouble *coords = (GLdouble*)malloc(3*sizeof(GLdouble));
				coords[0] = v.x;
				coords[1] = v.y;
				coords[2] = v.z;
				gluTessVertex(tobj, coords, coords);
				i++;
			}
			gluTessEndContour(tobj);
		}
		gluTessEndPolygon(tobj);
		gluDeleteTess(tobj);
}

Mesh* Tesselator::CreateMesh(void)
{
	Mesh *mesh = new Mesh ();
	mesh->Init (GetNumberVertices (), 100);
	mesh->m_nVertices = GetNumberVertices ();
	mesh->m_nFaces = 0;

	// copy the vertices
	{
		unsigned int index = 0;
		std::list<std::list<vertex_s>>::iterator itContour;
		for (itContour = m_listContours.begin(); itContour != m_listContours.end (); itContour++)
		{
			std::list<vertex_s> contour = (*itContour);
			std::list<vertex_s>::iterator itVertex;
			for (itVertex = contour.begin(); itVertex != contour.end (); itVertex++)
			{
				vertex_s v = (*itVertex);
				mesh->m_pVertices[3*index + 0] = v.x;
				mesh->m_pVertices[3*index + 1] = v.y;
				mesh->m_pVertices[3*index + 2] = v.z;
				index++;
			}
		}
	}

	struct glutess_state state;
	state.mesh = mesh;
	
	tobj = gluNewTess(); 
		gluTessCallback(tobj, GLU_TESS_BEGIN_DATA, (GLvoid (__stdcall *)  ( )) &MeshBeginCallback);
		gluTessCallback(tobj, GLU_TESS_VERTEX_DATA, (GLvoid (__stdcall *)  ( )) &MeshVertexCallback);
		gluTessCallback(tobj, GLU_TESS_END_DATA, (GLvoid (__stdcall *)  ( )) &MeshEndCallback);
		gluTessCallback(tobj, GLU_TESS_COMBINE_DATA, (GLvoid (__stdcall *)  ( ))&MeshCombineCallback);
		gluTessCallback(tobj, GLU_TESS_EDGE_FLAG_DATA, (GLvoid (__stdcall *)  ( ))&MeshEdgeFlagCallback);
	gluTessNormal(tobj, 0., 0., 1.);

		Set_Winding_Rule(GLU_TESS_WINDING_ODD); 

		gluTessBeginPolygon(tobj, &state);
		std::list<std::list<vertex_s>>::iterator itContour;

		int i = 0;
		//printf ("Dump polygon : %d contours\n", m_listContours.size ());
		for (itContour = m_listContours.begin(); itContour != m_listContours.end (); itContour++)
		{
			std::list<vertex_s> contour = (*itContour);
			//printf ("-> New contour : %d vertices\n", contour.size ());
			
			gluTessBeginContour(tobj);
			std::list<vertex_s>::iterator itVertex;
			for (itVertex = contour.begin(); itVertex != contour.end (); itVertex++)
			{
				vertex_s v = (*itVertex);

				GLdouble *coords = (GLdouble*)malloc(3*sizeof(GLdouble));
				coords[0] = v.x;
				coords[1] = v.y;
				coords[2] = v.z;
				gluTessVertex(tobj, coords, (void *) i);
				i++;
			}
			gluTessEndContour(tobj);
		}
		gluTessEndPolygon(tobj);
		gluDeleteTess(tobj);
	

	return state.mesh;
}

#endif // WIN32

