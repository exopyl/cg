#pragma once

class Curve
{
public:
	Curve () {};
	virtual ~Curve () {};

	// To be defined
	virtual void GetPosition (float t, float p[3]) const = 0;
	virtual void GetVelocity (float t, float v[3]) const = 0;
	virtual void GetAcceleration (float t, float a[3]) const = 0;
	virtual void GetJerk (float t, float j[3]) const = 0;
	virtual void GetSpeed (float t, float *s) const = 0;
	virtual void GetArclength (float t, float *s) const = 0;

	// Don't need to be defined
	void GetUnitTangentVector (float t, float T[3]);
	void GetUnitBinormalVector (float t, float B[3]);
	void GetUnitPrincipalNormalVector (float t, float N[3]);

	// To be defined
	virtual void GetCurvature (float t, float *kappa) const = 0;
	virtual void GetTorsion (float t, float *nu) const = 0;
	virtual void GetTangentialAcceleration (float t, float aT[3]) const = 0;
	virtual void GetNormalAcceleration (float t, float aT[3]) const = 0;

	void Export (char *filename);
};

//
//
//
class Curve01 : public Curve
{
public:
	Curve01 () : Curve() { m_m = 12; m_a = 1; m_r = 6.; };
	virtual ~Curve01 () {};
	
	virtual void GetPosition (float t, float p[3]) const;
	virtual void GetVelocity (float t, float v[3]) const;
	virtual void GetAcceleration (float t, float a[3]) const;
	virtual void GetJerk (float t, float j[3]) const;
	virtual void GetSpeed (float t, float *s) const;
	virtual void GetArclength (float t, float *s) const;

	//void GetUnitTangentVector (float t, float T[3]);
	//void GetUnitBinormalVector (float t, float B[3]);
	//void GetUnitPrincipalNormalVector (float t, float N[3]);

	virtual void GetCurvature (float t, float *kappa) const;
	virtual void GetTorsion (float t, float *nu) const;
	virtual void GetTangentialAcceleration (float t, float aT[3]) const;
	virtual void GetNormalAcceleration (float t, float aT[3]) const;

private:
	float m_r; // radius
	float m_m; // angular rate
	float m_a; // amplitude
};

//
// CurveHelical
//
class CurveHelical : public Curve
{
public:
	CurveHelical () : Curve() { m = 3.; n = 2.; };
	virtual ~CurveHelical() {};
	
	virtual void GetPosition (float t, float p[3]) const;
	virtual void GetVelocity (float t, float v[3]) const;
	virtual void GetAcceleration (float t, float a[3]) const;
	virtual void GetJerk (float t, float j[3]) const;
	virtual void GetSpeed (float t, float *s) const;
	virtual void GetArclength (float t, float *s) const;

	//void GetUnitTangentVector (float t, float T[3]);
	//void GetUnitBinormalVector (float t, float B[3]);
	//void GetUnitPrincipalNormalVector (float t, float N[3]);

	virtual void GetCurvature (float t, float *kappa) const;
	virtual void GetTorsion (float t, float *nu) const;
	virtual void GetTangentialAcceleration (float t, float aT[3]) const;
	virtual void GetNormalAcceleration (float t, float aT[3]) const;

private:
	float m; // angular rate
	float n; // height
};

//
// WindingLineOnTorus
// ref : http://www.maplesoft.com/applications/view.aspx?SID=4019&view=html
//
class CurveWindingLineOnTorus : public Curve
{
public:
	CurveWindingLineOnTorus () { p=4; q=1; m=2; n=10; };
	virtual ~CurveWindingLineOnTorus () {};

	virtual void GetPosition (float t, float p[3]) const;
	virtual void GetVelocity (float t, float v[3]) const;
	virtual void GetAcceleration (float t, float a[3]) const;
	virtual void GetJerk (float t, float j[3]) const;
	virtual void GetSpeed (float t, float *s) const;
	virtual void GetArclength (float t, float *s) const;
	//void GetUnitTangentVector (float t, float T[3]);
	//void GetUnitBinormalVector (float t, float B[3]);
	//void GetUnitPrincipalNormalVector (float t, float N[3]);
	virtual void GetCurvature (float t, float *kappa) const;
	virtual void GetTorsion (float t, float *nu) const;
	virtual void GetTangentialAcceleration (float t, float aT[3]) const;
	virtual void GetNormalAcceleration (float t, float aT[3]) const;

private:
	float p; // big radius
	float q; // small radius
	float m; // angular rate
	float n; // angular rate
};
