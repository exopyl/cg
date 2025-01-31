#include <math.h>
#include <string.h>

#include "bounding_box.h"

bool BoundingBox::IsEmpty() const
{
	return m_bEmpty;
}

void BoundingBox::AddPoint(float x, float y, float z)
{
	if (m_bEmpty)
	{
		m_bEmpty = false;

		m_min[0] = x;
		m_min[1] = y;
		m_min[2] = z;

		m_max[0] = x;
		m_max[1] = y;
		m_max[2] = z;
	}
	else
	{
		if (m_min[0] > x) m_min[0] = x;
		if (m_min[1] > y) m_min[1] = y;
		if (m_min[2] > z) m_min[2] = z;

		if (m_max[0] < x) m_max[0] = x;
		if (m_max[1] < y) m_max[1] = y;
		if (m_max[2] < z) m_max[2] = z;
	}
}

void BoundingBox::AddBoundingBox(const BoundingBox& bbox)
{
	if (m_bEmpty)
	{
		if (!bbox.IsEmpty())
			*this = bbox;
	}
	else if (!bbox.IsEmpty())
	{
		for (int i = 0; i < 3; i++)
		{
			if (m_min[i] > bbox.m_min[i]) m_min[i] = bbox.m_min[i];
			if (m_max[i] < bbox.m_max[i]) m_max[i] = bbox.m_max[i];
		}
	}
}

bool BoundingBox::GetMinMax(float min[3], float max[3]) const
{
	if (m_bEmpty)
		return false;

	memcpy(min, m_min, 3 * sizeof(float));
	memcpy(max, m_max, 3 * sizeof(float));

	return true;
}

bool BoundingBox::GetCenter(float center[3]) const
{
	if (m_bEmpty)
		return false;

	for (int i = 0; i < 3; i++)
		center[i] = .5f * (m_min[i] + m_max[i]);
}

float BoundingBox::GetDiagonalLength() const
{
	float length2 = 0.f;
	for (int i = 0; i < 3; i++)
		length2 += (m_max[i] - m_min[i]) * (m_max[i] - m_min[i]);
	return sqrt(length2);
}


float BoundingBox::GetLargestLength() const
{
	float fLargestLength = m_max[0] - m_min[0];
	for (int i = 1; i < 3; i++)
		if (fLargestLength < m_max[i] - m_min[i])
			fLargestLength = m_max[i] - m_min[i];
	return fLargestLength;
}

void BoundingBox::Translate(float x, float y, float z)
{
	if (!IsEmpty())
	{
		m_min[0] += x;
		m_max[0] += x;
		m_min[1] += y;
		m_max[1] += y;
		m_min[2] += z;
		m_max[2] += z;
	}
}

void BoundingBox::Scale(float x, float y, float z)
{
	if (!IsEmpty())
	{
		m_min[0] *= x;
		m_max[0] *= x;
		m_min[1] *= y;
		m_max[1] *= y;
		m_min[2] *= z;
		m_max[2] *= z;
	}
}
