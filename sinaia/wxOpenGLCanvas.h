#pragma once

#include "../src/cgre/gl_wrapper.h"
#include "wx/glcanvas.h"
#include <list>
#include "../src/cgre/cgre.h"


class Mesh;

//
// ref : http://wiki.wxwidgets.org/WxGLCanvas
//
class MyGLCanvas: public wxGLCanvas
{
public:
	wxGLContext*	m_context = nullptr;
	wxTextCtrl*		m_CtrlLog = nullptr; // TODO : replace with a lambda
	
	MyGLCanvas(wxWindow *parent, wxTextCtrl* pCtrlLog, int *args = 0);
	virtual ~MyGLCanvas();
	
	void LoadModel(const wxString& filename);
	void SaveModel(const wxString& filename);

	Mesh* GetMesh(void);
	void  SetMesh(Mesh *pMesh);

	Object3D* GetObject3D(void);
	void SetObject3D(Object3D* pObject);

	void SetBackgroundColor (unsigned char r, unsigned char g, unsigned char b);
	void GetBackgroundColor (unsigned char *r, unsigned char *g, unsigned char *b);

	void SetLighting (bool bLighting) { prop.light = bLighting; Refresh(false); };
	bool GetLighting (void) { return prop.light; };

	void SetSmooth (bool bSmooth) { m_bSmooth = bSmooth; Refresh(false); };
	bool GetSmooth (void) { return m_bSmooth; };

	void ChangeFill (void);
	bool GetFill (void);

	void ChangeWireframe (void);
	bool GetWireframe (void);

	void ChangePoint(void);
	bool GetPoint(void);

	void ChangeWarning(void);
	bool GetWarning(void);

	void ChangeBoundingBox (void);
	bool GetBoundingBox (void);

	unsigned int GetNNonManifoldEdges() const;
	unsigned int GetNBorders() const;

	void ResetProjectionMode();
	void DrawGL();

/*
	Mesh_half_edge* GetMesh (void) { return m_pMesh; };

	void NPRCompute (void);
	void SetSegments (ListNPRSegments &listSegments) { m_listSegments = listSegments; };
	void SetNPRAngleThreshold (float fValue) { m_fNPRAngleThreshold = fValue; };

	void ExportNPR (void);
*/
protected:
	void OnPaint(wxPaintEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnEraseBackground(wxEraseEvent& event);
	void OnMouse(wxMouseEvent& event);

private:
	void InitGL();

	bool m_bInitialized;

	// rendering modes
	rendering_properties_s prop;
	float m_fBackgroundColor[3];
	bool m_bSmooth;
	bool m_bBoundingBox;

	Ctrackball *m_pTrackball;

	//CRenderingEngine *m_pRenderingEngine;
	int m_nId;
	std::list<int> m_listId;

	Mesh *m_pMesh = nullptr;
	//Mesh_half_edge *m_pMesh = nullptr;
	Object3D *m_pObject = nullptr;


	// viewport
	float m_fFovy;
	float m_fWindowWidth, m_fWindowHeight;
	float m_fNear, m_fFar;

	// NPR
	//NPRManager *m_pNPRManager;

	//ListNPRSegments m_listSegments;
	float m_fNPRAngleThreshold;

    DECLARE_EVENT_TABLE()
};
