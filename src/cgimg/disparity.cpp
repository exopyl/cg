#include "disparity.h"

DisparityEvaluator::DisparityEvaluator ()
{
	m_pLeft = NULL;
	m_pRight = NULL;
	m_pDisparity = NULL;
}

DisparityEvaluator::~DisparityEvaluator ()
{
	m_pLeft = NULL;
	m_pRight = NULL;
	if (m_pDisparity)
		delete m_pDisparity;
}

int DisparityEvaluator::SetStereoPair (Img *pLeft, Img *pRight)
{
	m_pLeft = NULL;
	m_pRight = NULL;

	if (pLeft->width() != pRight->width() ||
	    pLeft->height() != pRight->height())
		return -1;

	m_pLeft = pLeft;
	m_pRight = pRight;

	return 0;
}

int DisparityEvaluator::Compute (void)
{
	if (!m_pLeft || !m_pRight)
		return -1;

	if (m_pDisparity)
	{
		delete m_pDisparity;
		m_pDisparity = NULL;
	}
	
	return Compute_Block ();
}

Img* DisparityEvaluator::GetDisparity (void)
{
	return m_pDisparity;
}

