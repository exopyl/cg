#pragma once

#include "../cgimg/cgimg.h"

class DisparityEvaluator
{
public:
	DisparityEvaluator ();
	~DisparityEvaluator ();

	int SetStereoPair (Img *pLeft, Img *pRight);

	int Compute (void);

	Img *GetDisparity (void);

	

private:
	int Compute_Block (void);

	Img *m_pLeft, *m_pRight, *m_pDisparity;
};
