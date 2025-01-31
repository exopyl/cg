#pragma once

class Camera
{
public:
	Camera();

	void Set();

private:
	int m_window_width = 0;
	int m_window_height = 0;
	float fovyInDegrees = 45.f;
	float zNear = .001f;
	float zFar = 50.f;
};
