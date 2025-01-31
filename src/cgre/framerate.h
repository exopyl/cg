#pragma once

#ifdef WIN32
#include <windows.h>
#else
#include <time.h>
#endif // WIN32

class CFrameRate
{
public:
	CFrameRate (void);
	~CFrameRate () {};

public:
	int CalculateFrameRate();

private:
#ifdef WIN32
	int lastTick;
	int lastFrameRate;
	int frameRate;
#else
	time_t last_time;
	time_t current_time;
	int nFrames;
#endif
};
