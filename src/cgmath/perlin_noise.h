#ifndef __PERLIN_NOISE_H__
#define __PERLIN_NOISE_H__

#ifdef __H__
extern double noise1(double arg);
extern float noise2(float vec[2]);
extern float noise3(float vec[3]);
#endif // __H__

const int B  = 0x100;
const int BM = 0xff;
const int N = 0x1000;
const int NP = 12;   /* 2^N */
const int NM  =0xfff;

class Cperlin_noise
{
public:
	Cperlin_noise ();
	~Cperlin_noise ();
  
	void init(void);

	double noise1 (double arg);
	float  noise2 (float vec[2]);
	float  noise3 (float vec[3]);

private:
	int   p[B+B+2];
	float g3[B+B+2][3];
	float g2[B+B+2][2];
	float g1[B+B+2];  
};

#endif // __PERLIN_NOISE_H__
