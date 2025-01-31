#include "framerate.h"

#ifdef WIN32

CFrameRate::CFrameRate (void)
{
	lastTick = GetTickCount();
}

int CFrameRate::CalculateFrameRate()
{
	if (GetTickCount() - lastTick >= 1000)
	{
		lastFrameRate = frameRate;
		frameRate = 0;
		lastTick = GetTickCount();
	}
	frameRate++;
	return lastFrameRate;
}

#else

CFrameRate::CFrameRate (void)
{
}

int CFrameRate::CalculateFrameRate()
{
	if (nFrames == 0)
		last_time = time (NULL);
	nFrames++;
	current_time = time (NULL);
	double diff = difftime (current_time, last_time);
	int fps = 0;
	if (diff >= 1)
	{
		char title[16];
		int fps = nFrames / diff;
	}
	return fps;
}

#endif
