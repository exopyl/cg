#pragma once

class BoundingBox
{
public:
	BoundingBox() = default;

	bool IsEmpty() const;
	void AddPoint(float x, float y, float z);
	void AddBoundingBox(const BoundingBox& bbox);

	bool GetMinMax(float min[3], float max[3]) const;
	bool GetCenter(float center[3]) const;
	float GetDiagonalLength() const;
	float GetLargestLength() const;

	void Translate(float x, float y, float z);
	void Scale(float x, float y, float z);

private:
	bool m_bEmpty = true;
	float m_min[3];
	float m_max[3];
};
