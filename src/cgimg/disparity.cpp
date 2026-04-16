#include "disparity.h"

DisparityEvaluator::DisparityEvaluator ()
{
	m_pLeft = nullptr;
	m_pRight = nullptr;
	m_pDisparity = nullptr;
}

DisparityEvaluator::~DisparityEvaluator ()
{
	m_pLeft = nullptr;
	m_pRight = nullptr;
	if (m_pDisparity)
		delete m_pDisparity;
}

int DisparityEvaluator::SetStereoPair (Img *pLeft, Img *pRight)
{
	m_pLeft = nullptr;
	m_pRight = nullptr;

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
		m_pDisparity = nullptr;
	}
	
	return Compute_Block ();
}

Img* DisparityEvaluator::GetDisparity (void)
{
	return m_pDisparity;
}

