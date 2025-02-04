#include "camera.h"

Camera::Camera ()
{
}

Camera::~Camera ()
{
}

void Camera::SetPosition (float x, float y, float z)
{
	m_vPosition[0] = x;
	m_vPosition[1] = y;
	m_vPosition[2] = z;
}

void Camera::SetDirection (float x, float y, float z)
{
	m_vDirection[0] = x;
	m_vDirection[1] = y;
	m_vDirection[2] = z;
}

void Camera::SetUp (float x, float y, float z)
{
	m_vUp[0] = x;
	m_vUp[1] = y;
	m_vUp[2] = z;
}



