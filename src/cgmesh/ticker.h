#ifndef __TICKER_H__
#define __TICKER_H__

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

#endif // __TICKER_H__
