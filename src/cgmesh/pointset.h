#ifndef __POINTSET_H__
#define __POINTSET_H__

/**
* PointSet
*/
class PointSet
{
public:
	PointSet (unsigned int nMaxPoints = 256);
	~PointSet ();

	int AddPoint (float x, float y, float z);

	void dump (void);
	void export_obj (char *filename);

	float* points (void) { return m_pPoints; };
	int nPoints (void) { return m_nPoints; };

protected:
	unsigned int m_nMaxPoints;
	unsigned int m_nPoints;
	float* m_pPoints;
	float* m_pNormals;
	float* m_pColors;
};

#endif // __POINTSET_H__

