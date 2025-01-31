#include <wx/textctrl.h>
#include <wx/dcclient.h>

#define _WINSOCKAPI_
#include "../src/cgre/gl_wrapper.h"
#include "../src/cgmath/cgmath.h"
#include "../src/cgmesh/bounding_box.h"
#include "../src/cgmesh/cgmesh.h"

#include "wxOpenGLCanvas.h"

BEGIN_EVENT_TABLE(MyGLCanvas, wxGLCanvas)
    EVT_SIZE(MyGLCanvas::OnSize)
    EVT_PAINT(MyGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(MyGLCanvas::OnEraseBackground)
    EVT_MOUSE_EVENTS(MyGLCanvas::OnMouse)
END_EVENT_TABLE()

//
//
//
MyGLCanvas::MyGLCanvas(wxWindow *parent, wxTextCtrl* pCtrlLog, int *args)
	:wxGLCanvas(parent)//, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, wxT("GLCanvas"), args)
//:wxGLCanvas(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, wxT("GLCanvas"), args)
//:wxGLCanvas(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, wxT("GLCanvas"))
{
	m_CtrlLog = pCtrlLog;
	m_context = new wxGLContext(this);
	//m_context->SetCurrent(static_cast<wxGLCanvas>(this));

	wglMakeCurrent(this->GetHDC(), m_context->GetGLRC());

	// 
	int version = gladLoaderLoadWGL(this->GetHDC());
	int versionGL = gladLoaderLoadGL();
	if (version == 0) {
		printf("Failed to initialize OpenGL context\n");
		return;
	}

	static int wx_gl_attribs[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 24, 0};
	//printf ("%d\n", ChooseGLVisual (wx_gl_attribs) == NULL);

	int argc = 1;
	char* argv[1] = { wxString((wxTheApp->argv)[0]).char_str() };
	//glutInit (&argc, argv);

	m_bInitialized = false;

	m_fBackgroundColor[0] = 1.;
	m_fBackgroundColor[1] = 1.;
	m_fBackgroundColor[2] = 1.;

	rendering_properties_init (prop);
	m_bBoundingBox = false;

	m_pTrackball = new Ctrackball ();
	m_pTrackball->set_zoom (-5.f);
	m_pTrackball->set_zoom_precision (2.f);

	m_pMesh = nullptr;

	m_fFovy = 45.f;
	m_fWindowWidth = 0.f;
	m_fWindowHeight = 0.f;
	m_fNear = 0.001f;
	m_fFar = 100.0f;
}

//
//
//
MyGLCanvas::~MyGLCanvas()
{
	delete m_pMesh;
	m_pMesh = nullptr;
	
	delete m_pObject;
	m_pObject = nullptr;

	delete m_context;
/*

	if (m_pNPRManager) delete m_pNPRManager;
	m_listId.clear ();
	m_listSegments.clear ();
*/
}

Mesh* MyGLCanvas::GetMesh(void)
{
	return m_pMesh;
};

void MyGLCanvas::SetMesh(Mesh *pMesh)
{
	delete m_pMesh;

	m_pMesh = pMesh;
	m_pMesh->centerize();
	m_pMesh->computebbox();
	float fLargestLength = m_pMesh->GetLargestLength();
	m_pMesh->scale(1.f / fLargestLength);
	m_pMesh->ComputeNormals();

	Refresh(false);
};


Object3D* MyGLCanvas::GetObject3D(void)
{
	return m_pObject;
};

void MyGLCanvas::SetObject3D(Object3D* pObject)
{
	delete m_pObject;

	m_pObject = pObject;


	BoundingBox bbox;
	for (const auto& mesh : m_pObject->GetMeshes())
	{
		mesh->computebbox();
		bbox.AddBoundingBox(mesh->bbox());

		mesh->ComputeNormals();
	}

	float center[3];
	bbox.GetCenter(center);
	float fLargestLength = bbox.GetLargestLength();
	for (const auto& mesh : m_pObject->GetMeshes())
	{
		mesh->translate(-center[0], -center[1], -center[2]);
		mesh->scale(1.f / fLargestLength);
		
		vector<unsigned int> nonManifoldEdges;
		vector<unsigned int> borders;
		mesh->GetTopologicIssues(nonManifoldEdges, borders);
		prop.nonManifoldEdges.insert(std::make_pair(mesh, nonManifoldEdges));
		prop.borders.insert(std::make_pair(mesh, borders));
	}



	Refresh(false);
}

//
//
//
void MyGLCanvas::LoadModel(const wxString& filename)
{
#ifdef LINUX
	locale_t loc;
	loc = newlocale(LC_NUMERIC, "C", NULL);
	uselocale(loc);
#endif // LINUX

	//cout << ((filename).mb_str(wxConvUTF8)) << endl;
	//printf ("%s\n", (char*) ((filename).mb_str(wxConvUTF8)).data());
	auto pObject = new Object3D();
	pObject->import_file((char*) ((filename).mb_str(wxConvUTF8)).data() );

	SetObject3D(pObject);

/*
	m_listId.clear ();
	std::list<Mesh_half_edge*> list = m_pObject->GetMeshes();
	for (std::list<Mesh_half_edge*>::iterator it = list.begin (); it != list.end (); it++)
	{
		m_pMesh = (*it);
		m_pMesh->center ();
		m_pMesh->compute_normales ();
		//m_pMesh->init_colors (239.0/255.0, 235.0/255.0, 160.0/255.0);
		m_pMesh->init_colors (1., 1., 1.);
		//m_pMesh->init_colors (.3f, 0.4f, 0.6f);

		m_nId = m_pRenderingEngine->GetRenderer3DManager()->addMesh (m_pMesh, GR3D_VERTEX_BUFFER);
		m_listId.push_back (m_nId);
	}
*/
}

void MyGLCanvas::SaveModel(const wxString& filename)
{
#ifdef LINUX
	locale_t loc;
	loc = newlocale(LC_NUMERIC, "C", NULL);
	uselocale(loc);
#endif // LINUX

	//cout << ((filename).mb_str(wxConvUTF8)) << endl;
	//printf ("%s\n", (char*) ((filename).mb_str(wxConvUTF8)).data());

	m_pObject->export_file((char*)((filename).mb_str(wxConvUTF8)).data());
}

//
//
//
void MyGLCanvas::DrawGL()
{

    // Initialize OpenGL
    if (!m_bInitialized)
    {
        InitGL();
        ResetProjectionMode();
        m_bInitialized = true;
    }

    // Clear
    //glClearColor( 0.3f, 0.4f, 0.6f, 1.0f );
    glClearColor( m_fBackgroundColor[0], m_fBackgroundColor[1], m_fBackgroundColor[2], 1.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // Transformations
    glLoadIdentity();

    //glScalef (0.01f, 0.01f, 0.01f);
	glEnable (GL_NORMALIZE);

	m_pTrackball->set_camera ();

	if (prop.light)
		glEnable (GL_LIGHTING);
	else
		glDisable (GL_LIGHTING);

	if (m_bSmooth)
		glShadeModel (GL_SMOOTH);
	else
		glShadeModel (GL_FLAT);

	repere_draw ();
	for (const auto& mesh : m_pObject->GetMeshes())
		mesh_draw (mesh, prop);
/*
	else
	{
		if (m_bWireframe)
			glutWireTeapot (0.5);
		else
			glutSolidTeapot (0.5);
	}
*/
/*
	if (m_pObject)
	{
		// draw model
		list<int>::iterator itId;
		for (itId = m_listId.begin (); itId != m_listId.end (); itId++)
		{
			m_nId = (*itId);
			m_pRenderingEngine->GetRenderer3DManager()->Draw (m_nId);
		}

		// test
		if(0)
		{
		float x, y, z;
		m_pTrackball->getCameraPosition (&x, &y, &z);
		Vector3f vPos (x, y, z);
		vPos.Normalize ();

		std::list<Mesh_half_edge*> list = m_pObject->GetMeshes();
		for (std::list<Mesh_half_edge*>::iterator it = list.begin (); it != list.end (); it++)
		{
			Mesh_half_edge *pMesh = (*it);

			float *pVertices = pMesh->get_vertices ();
			int nVertices = pMesh->get_n_vertices ();

			glBegin (GL_LINES);
			for (int i=0; i<nVertices; i++)
			{
				glVertex3f (vPos.x, vPos.y, vPos.z);
				glVertex3f (pVertices[3*i], pVertices[3*i+1], pVertices[3*i+2]);
			}
			glEnd ();
		}
		}


		// draw segments
		if (m_pNPRManager)
		{
			glPushAttrib (GL_ALL_ATTRIB_BITS);
			glEnable (GL_POLYGON_OFFSET_FILL); 
			glPolygonOffset (1., 2.);
			glEnable (GL_LINE_SMOOTH);
			
			//glEnable (GL_BLEND);
			//glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			//glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
			//glDisable (GL_LIGHTING);
			
			glDisableClientState (GL_COLOR_ARRAY);
			glLineWidth (2.f);
			glColor3f (0.f, 0.f, 0.f);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			glBegin (GL_LINES);

			m_listSegments = m_pNPRManager->GetSegments (NPR_SEGMENT_ANGLE);
			if (m_listSegments.size() != 0)
			{

				float *v = m_pMesh->get_vertices ();
				for (ListNPRSegmentsIt it=m_listSegments.begin(); it!=m_listSegments.end(); it++)
				{
					NPRSegment seg = (*it);
					if (seg.fData > m_fNPRAngleThreshold)
						break;
					Vector3 e1 = seg.e1;
					Vector3 e2 = seg.e2;
					glVertex3f (e1[0], e1[1], e1[2]);
					glVertex3f (e2[0], e2[1], e2[2]);
				}
			}

			m_listSegments = m_pNPRManager->GetSegments (NPR_SEGMENT_SILHOUETTE);
			if (m_listSegments.size() != 0)
			{
				glColor3f (0.f, 0.f, 0.f);

				float *v = m_pMesh->get_vertices ();
				for (ListNPRSegmentsIt it=m_listSegments.begin(); it!=m_listSegments.end(); it++)
				{
					NPRSegment seg = (*it);

					Vector3 e1 = seg.e1;
					Vector3 e2 = seg.e2;
					glVertex3f (e1[0], e1[1], e1[2]);
					glVertex3f (e2[0], e2[1], e2[2]);
				}

			}

			glEnd ();
			glPopAttrib ();
		}
	}
	else
	{
		if (m_bWireframe)
			glutWireTeapot (0.5);
		else
			glutSolidTeapot (0.5);
	}
*/

    glFlush();
    wxGLCanvas::SwapBuffers();
}

void MyGLCanvas::OnPaint(wxPaintEvent& WXUNUSED(event))
{
	wxGLCanvas::SetCurrent(*m_context);

	wxPaintDC dc(this);

#ifndef __WXMOTIF__
	//if (!GetContext()) return;
#endif
	wxGLCanvas::SetCurrent(*m_context);

	DrawGL();
}

void MyGLCanvas::OnSize(wxSizeEvent& event)
{
    // this is also necessary to update the context on some platforms
    //wxGLCanvas::OnSize(event);
    // Reset the OpenGL view aspect
    ResetProjectionMode();
}

void MyGLCanvas::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
    // Do nothing, to avoid flashing on MSW
}

//
//
//
void MyGLCanvas::OnMouse(wxMouseEvent& event)
{
	if (event.LeftDown())
	{
		m_pTrackball->mouse_press (LEFT_BUTTON, PRESSED, event.GetX(), event.GetY());
	}
	else if (event.LeftUp())
	{
		m_pTrackball->mouse_press (LEFT_BUTTON, RELEASED, event.GetX(), event.GetY());
	}
	else if (event.RightDown())
	{
		m_pTrackball->mouse_press (RIGHT_BUTTON, PRESSED, event.GetX(), event.GetY());
	}
	else if (event.RightUp())
	{
		m_pTrackball->mouse_press (RIGHT_BUTTON, RELEASED, event.GetX(), event.GetY());
	}
	else if (event.MiddleDown())
	{
		m_pTrackball->mouse_press (MIDDLE_BUTTON, PRESSED, event.GetX(), event.GetY());
	}
	else if (event.MiddleUp())
	{
		m_pTrackball->mouse_press (MIDDLE_BUTTON, RELEASED, event.GetX(), event.GetY());
	}
	else if (event.Dragging())
	{
		wxSize sz(GetClientSize());
		m_pTrackball->mouse_move (event.GetX(), event.GetY());
		
		// orientation has changed, redraw mesh
		Refresh(false);
	}
}

//
//
//
void MyGLCanvas::InitGL()
{
    static const GLfloat light0_pos[4]   = { -50.0f, 50.0f, 0.0f, 0.0f };

    // white light
    static const GLfloat light0_color[4] = { 0.6f, 0.6f, 0.6f, 1.0f };

    static const GLfloat light1_pos[4]   = {  50.0f, 50.0f, 0.0f, 0.0f };

    // cold blue light
    //static const GLfloat light1_color[4] = { 0.4f, 0.4f, 1.0f, 1.0f };
	static const GLfloat light1_color[4] = { 0.4f, 0.4f, 0.4f, 1.0f };

    // remove back faces
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // speedups
    glEnable(GL_DITHER);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);

/*
    // light
    glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light0_color);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1_color);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHTING);
*/

	float white[4] = {1.0, 1.0, 1.0, 1.0};
	float black[4] = {0.0, 0.0, 0.0, 0.0};
	// Lights
	glEnable (GL_LIGHTING);
	glEnable (GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
	glLightfv(GL_LIGHT0, GL_AMBIENT, black);


	GLfloat shininess[] = { 100.0F }; 
	glLightfv( GL_LIGHT0, GL_SPECULAR, white);
	glLightfv( GL_LIGHT0, GL_DIFFUSE, white);
	GLfloat position[] = { 0.0, 0.0, 50.0, 1.0 };
	glLightfv (GL_LIGHT0, GL_POSITION, position);


	glEnable (GL_COLOR_MATERIAL);
	glColorMaterial (GL_FRONT, GL_DIFFUSE);
	glColor3f (.2, .5, .8);

	GLfloat no_mat[] = {0.f, 0.f, 0.f, 1.f};
	GLfloat mat_diffuse[] = {0.1f, 0.5f, 0.8f, 1.f};
	GLfloat no_shininess[] = {20.f};
	glMaterialfv (GL_FRONT, GL_AMBIENT, no_mat);
	glMaterialfv (GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv (GL_FRONT, GL_SPECULAR, no_mat);
	glMaterialfv (GL_FRONT, GL_SHININESS, no_shininess);
	glMaterialfv (GL_FRONT, GL_EMISSION, no_mat);


    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
}

void MyGLCanvas::ResetProjectionMode()
{
    int w, h;
    GetClientSize(&w, &h);
	m_fWindowWidth = w;
	m_fWindowHeight = h;

#ifndef __WXMOTIF__
    //if ( GetContext() )
#endif
    {
        //SetCurrent(*m_context);
        glViewport(0, 0, (GLsizei) m_fWindowWidth, (GLsizei) m_fWindowHeight);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
		//Matrix4f mat;
		//mat.SetPerspective(m_fFovy, m_fWindowWidth / m_fWindowHeight, m_fNear, m_fFar);
		//glMultMatrixf((GLfloat*)mat.m_Mat);
        gluPerspective(m_fFovy, m_fWindowWidth/m_fWindowHeight, m_fNear, m_fFar);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

		m_pTrackball->set_dimensions (m_fWindowWidth, m_fWindowHeight);
    }
}

void MyGLCanvas::SetBackgroundColor (unsigned char r, unsigned char g, unsigned char b)
{
	m_fBackgroundColor[0] = r/255.;
	m_fBackgroundColor[1] = g/255.;
	m_fBackgroundColor[2] = b/255.;
}

void MyGLCanvas::GetBackgroundColor (unsigned char *r, unsigned char *g, unsigned char *b)
{
	*r = (unsigned char)(m_fBackgroundColor[0] * 255.);
	*g = (unsigned char)(m_fBackgroundColor[1] * 255.);
	*b = (unsigned char)(m_fBackgroundColor[2] * 255.);
}

void MyGLCanvas::ChangeFill (void)
{
	prop.display_fill = !prop.display_fill;
/*
	for (list<int>::iterator itId = m_listId.begin (); itId != m_listId.end (); itId++)
	{
		m_nId = (*itId);
		if (m_nId != -1)
			m_pRenderingEngine->GetRenderer3DManager()->SetFill (m_nId, m_bFill);
	}
*/
	Refresh(false);
}

bool MyGLCanvas::GetFill (void)
{
	return prop.display_fill;
}

void MyGLCanvas::ChangeWireframe (void)
{
	prop.display_wireframe = !prop.display_wireframe;
/*
	for (list<int>::iterator itId = m_listId.begin (); itId != m_listId.end (); itId++)
	{
		m_nId = (*itId);
		if (m_nId != -1)
			m_pRenderingEngine->GetRenderer3DManager()->SetWireframe (m_nId, m_bWireframe);
	}
*/
	Refresh(false);
}

bool MyGLCanvas::GetWireframe (void)
{
	return prop.display_wireframe;
}

void MyGLCanvas::ChangePoint (void)
{
	prop.display_points = !prop.display_points;
/*
	for (list<int>::iterator itId = m_listId.begin (); itId != m_listId.end (); itId++)
	{
		m_nId = (*itId);
		if (m_nId != -1)
			m_pRenderingEngine->GetRenderer3DManager()->SetPoint (m_nId, m_bPoint);
	}
*/
	Refresh(false);
}

bool MyGLCanvas::GetWarning (void)
{
	return prop.display_warning;
}


void MyGLCanvas::ChangeWarning(void)
{
	prop.display_warning = !prop.display_warning;
	Refresh(false);
}

bool MyGLCanvas::GetPoint(void)
{
	return prop.display_points;
}

void MyGLCanvas::ChangeBoundingBox (void)
{
	m_bBoundingBox = !m_bBoundingBox;
/*
	if (m_nId != -1)
		m_pRenderingEngine->GetRenderer3DManager()->SetBoundingBox (m_nId, m_bBoundingBox);
*/
	Refresh(false);
}

bool MyGLCanvas::GetBoundingBox (void)
{
	return m_bBoundingBox;
}

unsigned int MyGLCanvas::GetNNonManifoldEdges() const
{
	unsigned int nNonManifoldEdges = 0;
	for (auto& element : prop.nonManifoldEdges)
		nNonManifoldEdges += element.second.size() / 2;
	return nNonManifoldEdges;
}
unsigned int MyGLCanvas::GetNBorders() const
{
	unsigned int nBorders = 0;
	for (auto& element : prop.borders)
		nBorders += element.second.size() / 2;
	return nBorders;
}

//
// NPR
//
/*
void MyGLCanvas::NPRCompute (void)
{
	if (m_pNPRManager)
		delete m_pNPRManager;

	m_pNPRManager = new NPRManager ();
	if (m_pNPRManager == NULL)
		return;

	//m_pNPRManager->SetMesh (m_pMesh);
	m_pNPRManager->SetObject3D (m_pObject);

	float x, y, z;
	m_pTrackball->getCameraPosition (&x, &y, &z);
	Vector3f vCameraPosition (x, y, z);
	m_pNPRManager->SetCameraPosition (vCameraPosition);
	m_pNPRManager->ComputeSegments ();
 
	Refresh ();
}

#include <common/WriterSVG.h>
void MyGLCanvas::ExportNPR (void)
{
	GLfloat m[16];
	m_pTrackball->get_inverse_matrix (m);
	Matrix4f mat (m);

	float n = m_fNear;
	float f = m_fFar;
	float t = m_fNear * tan (m_fFovy/2.f);
	float b = -t;
	float r = t * m_fWindowWidth / m_fWindowHeight;
	float l = -r;
	Matrix4f matPerspective (	2.*n/(r-l), 0,			(r+l)/(r-l),	0.,
					0.,			2.*n/(t-b),	(t+b)/(t-b),	0.,
					0.,			0.,			-(f+n)/(f-n),	-2.*f*n/(f-n),
					0.,			0.,			-1.,			0.);
	
	if (m_listSegments.size() == 0)
		return;


	float *v = m_pMesh->get_vertices ();

	WriterSVG writerSVG;
	bool bOK = writerSVG.InitFile("d:\/\/toto.svg");
	if (!bOK)
	{
		printf ("berk\n");
	}
	writerSVG.WriteHeader();

	std::list<point2D> listPoints;
	for (ListNPRSegmentsIt it=m_listSegments.begin(); it!=m_listSegments.end(); it++)
	{
		listPoints.clear ();

		NPRSegment seg = (*it);
		int a = seg.i1;
		int b = seg.i2;

		Vector4f vec4A, vec4B;
		vec4A.Set (v[3*a], v[3*a+1], v[3*a+2], 1.f);
		vec4B.Set (v[3*b], v[3*b+1], v[3*b+2], 1.f);

		vec4A = mat * vec4A;
		vec4B = mat * vec4B;

		
		vec4A = matPerspective * vec4A;
		vec4B = matPerspective * vec4B;

		point2D ptA, ptB;
		float fScale = 300.;

		ptA.x = 100+fScale*vec4A.x;
		ptA.y = 400 - (100+fScale*vec4A.y);
		ptB.x = 100+fScale*vec4B.x;
		ptB.y = 400 - (100+fScale*vec4B.y);

		//ptA.x = 100+fScale*v[3*a];
		//ptA.y = 100+fScale*v[3*a+1];
		//ptB.x = 100+fScale*v[3*b];
		//ptB.y = 100+fScale*v[3*b+1];

		listPoints.push_back(ptA);
		listPoints.push_back(ptB);

		writerSVG.WritePolyline ("Id1", listPoints);

		listPoints.clear ();
	}

	writerSVG.WriteFooter ();

}
*/
