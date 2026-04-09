#pragma once
#include <list>
using namespace std;

#include "../cgmath/cgmath.h"

enum NPRSegmentType {	NPR_SEGMENT_ANGLE,
			NPR_SEGMENT_BORDER,
			NPR_SEGMENT_SILHOUETTE
};

//
//
//
class NPRSegment
{
public:
	float fData;
	Vector3f e1, e2;
	int i1, i2;
};
typedef list<NPRSegment> ListNPRSegments;
typedef list<NPRSegment>::iterator ListNPRSegmentsIt;
