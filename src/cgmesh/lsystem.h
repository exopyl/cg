#ifndef __LSYSTEM_H__
#define __LSYSTEM_H__

#include "lsrule.h"

//
// Rule
//
class LSystem
{
public:
	LSystem ();
	~LSystem ();

	void SetName (char *strName);
	char* GetName (void);

	void SetAngle (float fAngle);
	void SetLength (float fLength);
	void AddRule (char *antecedent, char *image);
	void Init (char *basis);
	void Next();
	void ComputeGraphicalInterpretation2D (void);
	void ComputeGraphicalInterpretation3D (void);
	void Dump (void);


public:
	void Scaling (float scale);
	void UpdateBoundingBox (void);
	void Centerize (void);
	void Normalize (void);
	void Translate (float fX, float fY);
	void MirrorAroundX (float fHeight);
	void FittIn (float fWidth, float fHeight, float fMargin);


public:
	char* m_strName;

	// LSystem
	char *m_string; // 
	int m_iNumberRules;
	LSRule **m_oRules;

	// graphical representation
	int m_iNumberMaxPoints; // maximal number of points in the path
	int m_iNumberPoints; // number of points in the path
	float *m_walk; // path
	bool *m_bDrawable; // is the path drawable ?

	// to compute the graphical representation
	float m_fX, m_fY, m_fZ; // current position
	float *m_fStackPosition;

	float m_fCurrentAngle, m_fAngle;
	float m_fCurrentLength, m_fLength;
	int m_iStackIndex;
	float *m_fStackAngle;

	// additional information about graphical representation
	float XMinBBox;
	float XMaxBBox;
	float YMinBBox;
	float YMaxBBox;
	float ZMinBBox;
	float ZMaxBBox;

	int m_iDimension;
};

extern LSystem* InitLSystem (void);

#endif // __LSYSTEM_H__
