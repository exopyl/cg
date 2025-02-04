#ifndef __CAMERA_H__
#define __CAMERA_H__

//#include "projection.h"

class Camera
{
public:
	Camera ();
	~Camera ();
	
	void SetPosition (float x, float y, float z);
	void SetDirection (float x, float y, float z);
	void SetUp (float x, float y, float z);

public:
	//Projection *m_pProjection;
	float m_vPosition[3];
	float m_vDirection[3];
	float m_vUp[3];
};

#endif // __CAMERA_H__

