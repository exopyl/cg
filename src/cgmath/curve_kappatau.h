#ifndef __CURVE_KAPPATAU_H__
#define __CURVE_KAPPATAU_H__

//
// http://www.cs.sjsu.edu/faculty/rucker/kaptaudoc/ktpaper.htm
//
// example
//float kappa (float s) { return 8; }
//float tau (float s) { return sin(s)+3*sin(2*s); }
//
class CurveKappaTau
{
public:
	CurveKappaTau () {};
	~CurveKappaTau () {};

	void Export (char *filename);

	void set_kappa ( float (*f)(float) ) { m_kappa = f; };
	void set_tau ( float (*f)(float) ) { m_tau = f; };

private:
	float (*m_kappa)(float);
	float (*m_tau)(float);
};

#endif // __CURVE_KAPPATAU_H__
