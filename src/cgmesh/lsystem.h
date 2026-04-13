#pragma once
#include <string>
#include <vector>
#include "lsrule.h"

//
// Rule
//
class LSystem
{
public:
	LSystem ();
	~LSystem ();

	void SetName (const char *strName);
	const char* GetName (void);

	void SetAngle (float fAngle);
	void SetLength (float fLength);
	void AddRule (const char *antecedent, const char *image);
	void Init (const char *basis);
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
	std::string m_strName;

	// LSystem
	std::string m_string; // 
	std::vector<LSRule*> m_oRules;

	// graphical representation
	int m_iNumberMaxPoints; // maximal number of points in the path
	int m_iNumberPoints; // number of points in the path
	std::vector<float> m_walk; // path
	std::vector<bool> m_bDrawable; // is the path drawable ?

	// to compute the graphical representation
	float m_fX, m_fY, m_fZ; // current position
	std::vector<float> m_fStackPosition;

	float m_fCurrentAngle, m_fAngle;
	float m_fCurrentLength, m_fLength;
	int m_iStackIndex;
	std::vector<float> m_fStackAngle;

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
