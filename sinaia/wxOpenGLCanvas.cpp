#include <wx/textctrl.h>
#include <wx/dcclient.h>

#define _WINSOCKAPI_
#include "../src/cgre/gl_wrapper.h"
#include "../src/cgmath/cgmath.h"
#include "../src/cgmesh/bounding_box.h"
#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/mesh_data_manager.h"

#include "wxOpenGLCanvas.h"
#include "SinaiaFrame.h"
#include <wx/statusbr.h>
#include <wx/image.h>
#include <cstring>
#include <cmath>
#include <algorithm>

BEGIN_EVENT_TABLE(MyGLCanvas, wxGLCanvas)
    EVT_SIZE(MyGLCanvas::OnSize)
    EVT_PAINT(MyGLCanvas::OnPaint)
    EVT_ERASE_BACKGROUND(MyGLCanvas::OnEraseBackground)
    EVT_MOUSE_EVENTS(MyGLCanvas::OnMouse)
    EVT_KEY_DOWN(MyGLCanvas::OnKeyDown)
END_EVENT_TABLE()

const int* MyGLCanvas::GetDefaultAttributes()
{
	static const int attributes[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 24, WX_GL_SAMPLE_BUFFERS, 1, WX_GL_SAMPLES, 4, 0 };
	return attributes;
}

//
//
//
MyGLCanvas::MyGLCanvas(wxWindow *parent, wxTextCtrl* pCtrlLog, int *args)
	: wxGLCanvas(parent, wxID_ANY, args ? args : GetDefaultAttributes(), wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS)
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
	if (m_pVMeshes)
	{
		for (auto* mesh : m_pVMeshes->GetMeshes())
			if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);
	}

	delete m_pMesh;
	m_pMesh = nullptr;

	delete m_pVMeshes;
	m_pVMeshes = nullptr;

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


VMeshes* MyGLCanvas::GetVMeshes(void)
{
	return m_pVMeshes;
};

void MyGLCanvas::SetVMeshes(VMeshes* pObject, bool normalize)
{
	if (m_pVMeshes)
	{
		for (auto* mesh : m_pVMeshes->GetMeshes())
			if (mesh) MeshRenderer::getInstance()->RemoveMesh(mesh);
	}
	delete m_pVMeshes;

	m_pVMeshes = pObject;

    ApplyNormalization(normalize);
}

void MyGLCanvas::ApplyNormalization(bool normalize)
{
	if (!m_pVMeshes) return;

	for (const auto& mesh : m_pVMeshes->GetMeshes())
	{
		mesh->ComputeNormals();
	}

	if (normalize)
	{
		m_pVMeshes->Normalize(); // This method now re-centers and re-scales meshes and updates their individual bboxes
	}

	// Always compute the aggregate bounding box for all meshes AFTER potential normalization
	BoundingBox aggregateBbox;
	for (const auto& mesh : m_pVMeshes->GetMeshes())
	{
		mesh->computebbox(); // Ensure individual mesh bboxes are up-to-date
		aggregateBbox.AddBoundingBox(mesh->bbox());
	}

	// Add grid to bounding box to ensure it's not clipped
	aggregateBbox.AddPoint(2.f, 2.f, 1.f);
	aggregateBbox.AddPoint(-2.f, -2.f, -1.f);

	// Frame the camera on the resulting model.
	m_pTrackball->ResetTransformations(); // Reset camera rotation/translation
	FrameCamera(aggregateBbox);

	UpdateTopologicIssues();

	Refresh(false);
}

//
// Set the camera distance and clip planes. The camera orbits the WORLD ORIGIN
// (it is not recentred on the model), so each model is shown at its own
// coordinate position — a normalized model sits at the origin and appears
// centred, a non-normalized one appears offset by its native coordinates.
// We fit the sphere *centred on the origin* that contains the model, which
// just guarantees it stays visible and inside the clip planes.
//
void MyGLCanvas::FrameCamera(const BoundingBox& bbox)
{
	if (bbox.IsEmpty())
		return;

	float mn[3], mx[3];
	bbox.GetMinMax(mn, mx);
	const float ax = std::max(std::fabs(mn[0]), std::fabs(mx[0]));
	const float ay = std::max(std::fabs(mn[1]), std::fabs(mx[1]));
	const float az = std::max(std::fabs(mn[2]), std::fabs(mx[2]));
	const float radius = std::sqrt(ax * ax + ay * ay + az * az);
	if (radius <= 0.f)
		return;

	// Distance at which a sphere of `radius` (centred on the origin) fits the
	// vertical fov, + margin.
	const float halfFovy = (m_fFovy * 0.5f) * 3.14159265f / 180.f;
	const float sinHalf  = sinf(halfFovy);
	const float distance = (sinHalf > 1e-4f ? radius / sinHalf : radius * 3.f) * 1.2f;

	m_pTrackball->set_zoom(-distance);
	// Scale the zoom step to the model so right-drag zoom stays usable at any
	// scale (the historical feel was precision 2 at distance ~5).
	m_pTrackball->set_zoom_precision(std::max(0.5f, distance * 0.4f));
	// Let the trackball derive near/far from the live zoom each frame so the
	// clip planes follow the scene when zooming in/out.
	m_pTrackball->set_scene_radius(radius);
}

//
//
//
void MyGLCanvas::LoadModel(const wxString& filename, const ImportSettings& settings)
{
#ifdef LINUX
	locale_t loc;
	loc = newlocale(LC_NUMERIC, "C", nullptr);
	uselocale(loc);
#endif // LINUX

	//cout << ((filename).mb_str(wxConvUTF8)) << endl;
	//printf ("%s\n", (char*) ((filename).mb_str(wxConvUTF8)).data());
	auto meshes = new VMeshes();
	std::string loaded_filename = filename.ToUTF8().data();
	meshes->load(const_cast<char*>(loaded_filename.c_str()));

	wxString msg = wxString::Format(wxT("%zu meshes imported"), meshes->GetNMeshes());
	*m_CtrlLog << msg << _T("\n");

	// Echo the import options so the effect of each operation is traceable.
	*m_CtrlLog << wxString::Format(
		_T("Import options: Normalisation=%s, Triangulate=%s, Merge vertices=%s\n"),
		settings.normalize     ? _T("on") : _T("off"),
		settings.triangulate   ? _T("on") : _T("off"),
		settings.mergeVertices ? _T("on") : _T("off"));

	// Normalise (if enabled), compute normals, frame the camera and show the
	// model. Import operations then run on the resulting mesh — the same state
	// the Treatments menu operates on — so Merge vertices is effective at unit
	// scale rather than a no-op on raw (large) coordinates.
	SetVMeshes(meshes, settings.normalize);

	if (settings.triangulate || settings.mergeVertices)
	{
		for (auto& pMesh : meshes->GetMeshes())
		{
			if (settings.triangulate)
			{
				const auto nBefore = pMesh->GetNFaces();
				pMesh->Triangulate();
				*m_CtrlLog << wxString::Format(_T("  Triangulate: %u -> %u faces\n"),
				                               nBefore, pMesh->GetNFaces());
			}
			if (settings.mergeVertices)
			{
				const auto nBefore = pMesh->GetNVertices();
				pMesh->MergeVertices();
				*m_CtrlLog << wxString::Format(_T("  Merge vertices: %u -> %u vertices\n"),
				                               nBefore, pMesh->GetNVertices());
			}
			pMesh->ComputeNormals();
		}
		UpdateTopologicIssues();
		Refresh(false);
	}

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
	loc = newlocale(LC_NUMERIC, "C", nullptr);
	uselocale(loc);
#endif // LINUX

	//cout << ((filename).mb_str(wxConvUTF8)) << endl;
	//printf ("%s\n", (char*) ((filename).mb_str(wxConvUTF8)).data());

	m_pVMeshes->save((char*)((filename).mb_str(wxConvUTF8)).data());
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
	prop.smooth = m_bSmooth;

	if (prop.display_repere)
		repere_draw ();
	if (prop.display_grid)
		draw_grid();
	for (const auto& mesh : m_pVMeshes->GetMeshes())
	{
		// CG_RENDERING_VBO: server-side buffers (positions, normals, UVs,
		// colors, indices) bound once and re-drawn from VRAM. Avoids the
		// per-frame CPU->GPU push of CG_RENDERING_VERTEX_ARRAY. VBOManager
		// re-uploads automatically when Mesh::GetRevision() changes, so
		// RemoteConsole edits like `flip` stay visible.
		// The overlays (wireframe, vertex normals, points) are still drawn
		// by mesh_draw inside MeshRenderer::Draw.
		int id = MeshRenderer::getInstance()->GetMeshId(mesh, CG_RENDERING_VBO);

		// Update rendering properties from the canvas prop
		MeshRenderer::getInstance()->SetProperties(id, prop);

		MeshRenderer::getInstance()->Draw(id);
	}
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
	if (m_pVMeshes)
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

	ResetProjectionMode();
	DrawGL();

	// FPS overlay: count frames on natural paint events (mouse drag, resize,
	// programmatic Refresh, etc.) and push to the status bar every ~250 ms.
	// We deliberately do NOT call Refresh() from here to avoid a self-feeding
	// repaint loop that would starve modal dialogs and pump events forever.
	// If you need a steady measurement, hold the left mouse button and drag.
	MyFrame* frame = dynamic_cast<MyFrame*>(wxGetTopLevelParent(this));
	if (frame && frame->IsShowFps())
	{
		++m_fpsFrames;
		const auto now = std::chrono::steady_clock::now();
		const long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - m_fpsLastUpdate).count();
		if (elapsedMs >= 250)
		{
			const double fps = (m_fpsFrames * 1000.0) / static_cast<double>(elapsedMs);
			if (wxStatusBar* sb = frame->GetStatusBar())
				sb->SetStatusText(wxString::Format(wxT("FPS: %.0f"), fps), 1);
			m_fpsFrames = 0;
			m_fpsLastUpdate = now;
		}
	}
}

bool MyGLCanvas::SaveScreenshot(const wxString& path)
{
	if (!m_context) return false;
	wxGLCanvas::SetCurrent(*m_context);

	int w = 0, h = 0;
	GetClientSize(&w, &h);
	if (w <= 0 || h <= 0) return false;

	std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

	// glReadPixels returns rows bottom-up; PNG wants top-down — flip Y.
	std::vector<unsigned char> flipped(static_cast<size_t>(w) * h * 3);
	for (int y = 0; y < h; ++y)
	{
		std::memcpy(&flipped[static_cast<size_t>(h - 1 - y) * w * 3],
		            &pixels [static_cast<size_t>(y)         * w * 3],
		            static_cast<size_t>(w) * 3);
	}

	// wxImage with static_data=true keeps ownership in our vector; SaveFile
	// reads from it while the vector is still in scope. wxPNGHandler is
	// registered by wxInitAllImageHandlers() in SinaiaApp::OnInit.
	wxImage img(w, h, flipped.data(), true);
	return img.SaveFile(path, wxBITMAP_TYPE_PNG);
}

void MyGLCanvas::OnSize(wxSizeEvent& event)
{
    if (IsShownOnScreen()) {
        wxGLCanvas::SetCurrent(*m_context);
        ResetProjectionMode();
        Refresh(false);
    }
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
	if (event.ButtonDown())
		SetFocus();

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

void MyGLCanvas::OnKeyDown(wxKeyEvent& event)
{
	float step = 0.05f;
	switch (event.GetKeyCode())
	{
	case WXK_UP:
	case WXK_DOWN:
		if (!prop.clipping_plane_active)
		{
			if (m_CtrlLog) *m_CtrlLog << _T("Warning: Clipping plane is not active. Check the box in Rendering panel.\n");
			break;
		}
		if (event.GetKeyCode() == WXK_UP)
			prop.clipping_plane_z += step;
		else
			prop.clipping_plane_z -= step;

		if (m_CtrlLog) *m_CtrlLog << wxString::Format(wxT("Clipping Z: %.2f\n"), prop.clipping_plane_z);
		Refresh(false);
		Update();
		break;
	default:
		event.Skip();
		break;
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
    glEnable(GL_MULTISAMPLE);

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
	float black[4] = {0.0, 0.0, 0.0, 1.0};

	// Lighting. A single full-intensity headlight plus the default global
	// ambient (0.2) flattens the model: lit faces clip toward white and
	// recesses stay bright, washing out the texture relief. Instead use a
	// low global ambient, a slightly-under-full key light from the upper
	// front-left, and a dim fill from the opposite side. Both are directional
	// (w=0) so the shading is independent of this very large model's scale and
	// off-origin position.
	GLfloat globalAmbient[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
	glLightModelfv (GL_LIGHT_MODEL_AMBIENT, globalAmbient);

	// Two-sided lighting: light back faces with a flipped normal. Imported
	// meshes (e.g. OBJ from 3D Warehouse) often have inconsistent face winding,
	// which leaves some faces with inward-pointing normals; without this they
	// render near-black ("black reflections" that move with the view).
	glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	glEnable (GL_LIGHTING);

	// Key light
	GLfloat keyDiffuse[4]  = { 0.75f, 0.75f, 0.75f, 1.0f };
	GLfloat keyDir[4]      = { -0.4f, 0.6f, 1.0f, 0.0f };
	glEnable (GL_LIGHT0);
	glLightfv (GL_LIGHT0, GL_AMBIENT,  black);
	glLightfv (GL_LIGHT0, GL_DIFFUSE,  keyDiffuse);
	glLightfv (GL_LIGHT0, GL_SPECULAR, white);
	glLightfv (GL_LIGHT0, GL_POSITION, keyDir);

	// Fill light: dimmer, opposite side, no specular — lifts shadows just
	// enough to keep them readable without flattening the relief.
	GLfloat fillDiffuse[4] = { 0.28f, 0.28f, 0.30f, 1.0f };
	GLfloat fillDir[4]     = { 0.7f, 0.25f, 0.4f, 0.0f };
	glEnable (GL_LIGHT1);
	glLightfv (GL_LIGHT1, GL_AMBIENT,  black);
	glLightfv (GL_LIGHT1, GL_DIFFUSE,  fillDiffuse);
	glLightfv (GL_LIGHT1, GL_SPECULAR, black);
	glLightfv (GL_LIGHT1, GL_POSITION, fillDir);

	// Default material (overridden per material at draw time).
	glColor3f (.2, .5, .8);
	GLfloat no_mat[] = {0.f, 0.f, 0.f, 1.f};
	GLfloat mat_diffuse[] = {0.1f, 0.5f, 0.8f, 1.f};
	GLfloat no_shininess[] = {20.f};
	glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, no_mat);
	glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
	glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, no_mat);
	glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, no_shininess);
	glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, no_mat);

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

void MyGLCanvas::ChangeRepere(void)
{
	prop.display_repere = !prop.display_repere;
	Refresh(false);
}

bool MyGLCanvas::GetRepere(void)
{
	return prop.display_repere;
}

void MyGLCanvas::ChangeGrid(void)
{
	prop.display_grid = !prop.display_grid;
	Refresh(false);
}

bool MyGLCanvas::GetGrid(void)
{
	return prop.display_grid;
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

void MyGLCanvas::UpdateTopologicIssues()
{
	prop.nonManifoldEdges.clear();
	prop.borders.clear();
	if (m_pVMeshes)
	{
		for (const auto& mesh : m_pVMeshes->GetMeshes())
		{
            const auto& issues = MeshDataManager::GetInstance().GetTopologicIssues(mesh);
			prop.nonManifoldEdges.insert(std::make_pair(mesh, issues.nonManifoldEdges));
			prop.borders.insert(std::make_pair(mesh, issues.borders));
		}
	}
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
	if (m_pNPRManager == nullptr)
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
