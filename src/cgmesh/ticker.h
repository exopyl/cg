#pragma once
#ifdef linux
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

class Ticker
{
public:
	Ticker ();
	~Ticker ();
	
	void evaluate_gettimeofday (void);

	void   start (void);
	double stop (void);

private:
	struct timeval s, e;
};

#endif // linux

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

class Ticker
{
public:
	Ticker ();
	//~Ticker ();

	void   start (void);
	double stop (void);
	
private:
	__int64 s, e, freq;
	DWORD m_StartTime, m_ElapsedTime, m_PreviousElapsedTime;
};

#endif // WIN32
