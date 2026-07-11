#pragma once

#include "../cgimg/cgimg.h"

class DisparityEvaluator
{
public:
	DisparityEvaluator ();
	~DisparityEvaluator ();

	// Non-copiable : possède des Img* détruits dans le destructeur ; une copie
	// superficielle (règle de 3 manquante) provoquerait un double-delete.
	DisparityEvaluator (const DisparityEvaluator&)            = delete;
	DisparityEvaluator& operator= (const DisparityEvaluator&) = delete;

	int SetStereoPair (Img *pLeft, Img *pRight);

	int Compute (void);

	Img *GetDisparity (void);

	

private:
	int Compute_Block (void);

	Img *m_pLeft, *m_pRight, *m_pDisparity;
};
