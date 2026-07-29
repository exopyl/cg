#pragma once
#include <stdlib.h>
#include <map>
#include <list>
#include <set>
#include <vector>
using namespace std;

#include "../cgmath/cgmath.h"
#include "../cgimg/cgimg.h"

class TPoint
{
public:
	TPoint(){}
	TPoint(const int &xx,const int &yy) { x = xx; y = yy; }
	bool operator==(const TPoint& pt) const { return (x == pt.x && y == pt.y); }
	int x;
	int y;
};
typedef map<unsigned int,Vector2f> MapCoord;

class TSegment
{
public:
	TSegment() {}
	TSegment(const TPoint& S,const TPoint& E) { s = S; e = E; }
	TPoint s, e;
};

typedef list<TPoint>	    TPath;
typedef list<TPath*>	    ListPath;
typedef map<int, ListPath*> MapListPath;
typedef multimap<int,int>   MapOrder;
typedef map<unsigned long,TPath*> MapPath;
typedef map<unsigned long,set<unsigned long> > MapAdjacence;

// One closed contour of a colour layer, expressed in the FINAL (smoothed +
// simplified) image coordinates -- i.e. exactly what WriteFile*() emits. The
// closing edge (last point -> first point) is implicit.
struct VectorContour
{
	vector<Vector2f> pts;
	// true when the contour bounds a HOLE of its own layer (a region of some
	// other colour enclosed by it) rather than the outer boundary of a blob.
	// Nesting is arbitrary: a same-colour island sitting inside a hole is
	// itself an outer boundary.
	bool isHole = false;
	// Signed shoelace area, in px^2 of the SOURCE image and in the IMAGE frame
	// (x right, y down) -- the frame `pts` is expressed in on return. Negative
	// for an outer boundary, positive for a hole: that sign IS what sets isHole,
	// and it is exposed so callers can see the criterion rather than recompute it.
	// Goes stale if the caller transforms `pts`.
	//
	// NOT a despeckling criterion: dropping small contours tears the surface,
	// because a speck straddling two colours has no matching hole contour in
	// either neighbour to close the gap. Despeckle the label image instead (see
	// ImageReliefOptions::despecklePasses).
	float area = 0.f;
};

// All the contours carrying one palette colour.
struct VectorLayer
{
	Color color;
	int   colorIndex = -1;
	vector<VectorContour> contours;
};

class CLitRasterToVector
{
public:
	CLitRasterToVector(void);
	virtual ~CLitRasterToVector(void);
	bool Vectorize (Img* pInput,
			Color colorMask,
			bool bUseMask,
			Palette *pPalette = nullptr,   // if nullptr, a palette is calculated
			float fSimplifyErr = .5f);     // contour simplification tolerance (px)

	void WriteFile (float fLineWidth) const;
	void WriteFilePolygonWithHole (/*bool bBottomTop,
				       bool bFill,
				       bool bBorderColor,
				       bool bDrawWhiteLayer,
				       const Color* pForceFillColor,*/
				       float fLineWidth/*,
				       const Color& borderColor,
				       bool bBorder*/) const;
	void WriteFilePolygonWithHole2 (float fLineWidth);

	// Introspection of the last Vectorize() result (for tests / callers).
	int GetNColorLayers (void) const { return (int)m_mapListPath.size(); }
	int GetNPaths (void) const
	{
		int n = 0;
		for (const auto& kv : m_mapListPath)
			if (kv.second) n += (int)kv.second->size();
		return n;
	}
	int GetNCoords (void) const { return (int)m_mapCoord.size(); }

	// Structured view of the last Vectorize() result: one entry per colour
	// layer that produced at least one usable contour, in palette-index order.
	// This is the accessor 3D consumers want (image_relief.h) -- it converts
	// the internal path/coord maps into plain point lists and classifies each
	// contour as outer boundary or hole. Empty before Vectorize() succeeds.
	vector<VectorLayer> GetLayers (void) const;

private:
	bool GeneratePath	( const Img& sIndexed); 
	void AddPath		( ListPath& listPath,const TPath& pathToAdd,MapPath& mapPathStart,MapPath& mapPathEnd);
	bool MergePath		( ListPath& listPath,MapPath& mapPathStart,MapPath& mapPathEnd);

	void SmoothCoords	( void );

	void RemoveHoles	( ListPath& listPath, int iColorIndex, const Img& sIndexed );
	bool GetLimitColor      ( const TPath& path,const Img& sIndexed,int& iInteriorColor,int& iExteriorColor);
	bool GetLimitPixelCoord ( const TPath& path,TPoint& ptLimit);

	const Vector2f& GetInterpolatedCoord( const TPoint& pt	);
	const Vector2f& GetFinalCoord				( const TPoint& pt	) const;

	void CalculateLayerOrder	( const Img& sPalettized);
	void RemoveUselessPoint		( void );
	void Simplify							( float fErr );
	void BuildAdjacencyMap(MapAdjacence& mapAdjacence);

	// result data
	MapListPath	m_mapListPath;	// one list path by color index
	Color*	    m_palette;
	int		    m_iPaletteSize;
	MapCoord	m_mapCoord;

	list<TPath*>	m_layerOrder;
	map<TPath*,int> m_pathColor;
};
