#include <stdlib.h>

#include "ticker.h"


#ifdef linux

Ticker::Ticker ()
{
	gettimeofday (&s, NULL);
}

Ticker::~Ticker ()
{
}

void
Ticker::start (void)
{
	gettimeofday (&s, NULL);
}

double
Ticker::stop (void)
{
	gettimeofday (&e, NULL);
	return ((e.tv_sec - s.tv_sec)*1000000.0 + e.tv_usec - s.tv_usec)*0.000001;
}

void Ticker::evaluate_gettimeofday (void)
{
	if (1)
	{
		int count = 1 * 1000 * 1000 * 1;
		gettimeofday (&s, NULL);
		struct timeval tv_tmp;
		for (int i = 0; i < count; i++)
			gettimeofday(&tv_tmp, NULL);
		gettimeofday (&e, NULL);
		float diff = ((e.tv_sec - s.tv_sec)*1000000.0 + e.tv_usec - s.tv_usec)*0.000001;
		printf("%d calls in %f s = %f s/call\n", count, diff, (double)diff / (double)count);
	}
	else
	{
		struct timespec tv_start, tv_end;
		struct timeval tv_tmp;
		int count = 1 * 1000 * 1000 * 50;
		clockid_t clockid;
		int rv = clock_getcpuclockid(0, &clockid);
		if (rv)
		{
			perror("clock_getcpuclockid");
			return;
		}
		
		clock_gettime(clockid, &tv_start);
		for (int i = 0; i < count; i++)
			gettimeofday(&tv_tmp, NULL);
		clock_gettime(clockid, &tv_end);
		
		long long diff = (long long)(tv_end.tv_sec - tv_start.tv_sec)*(1*1000*1000*1000);
		diff += (tv_end.tv_nsec - tv_start.tv_nsec);
		
		printf("%d cycles in %lld ns = %f ns/cycle\n", count, diff, (double)diff / (double)count);
	}
}

#endif // linux


#ifdef WIN32
Ticker::Ticker ()
{
	QueryPerformanceFrequency ((LARGE_INTEGER*)&freq);
	QueryPerformanceCounter ((LARGE_INTEGER*)&s);
}

void
Ticker::start (void)
{
	QueryPerformanceCounter ((LARGE_INTEGER*)&s);
}

double
Ticker::stop (void)
{
	QueryPerformanceCounter ((LARGE_INTEGER*)&e);
	//return (double)(e.QuadPart - s.QuadPart)/freq;
	return (double)(1000*(e - s))/freq;
}

#endif // WIN32
