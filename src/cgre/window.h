#pragma once

#include "frame_buffer_object.h"
#include "examinator_trackball.h"

class Window
{
public:
	Window ();
	~Window ();

	//void setObject (char *filename);
	void select (bool status) { m_selected = status; };
	bool isSelected (void) { return m_selected; };
	void pre (void); // rendering to texture
	void display (void);

public:
	unsigned int m_id;
	Ctrackball *m_examinator;
private:
	bool m_selected;
	FrameBufferObject *m_fbo;
};
