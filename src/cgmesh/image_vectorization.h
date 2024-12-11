#ifndef __IMAGE_VECTORIZATION_H__
#define __IMAGE_VECTORIZATION_H__

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

class CLitRasterToVector
{
public:
	CLitRasterToVector(void);
	virtual ~CLitRasterToVector(void);
	bool Vectorize (Img* pInput,
			Color colorMask,
			bool bUseMask,
			Palette *pPalette = NULL); // if NULL, a palette is calculated

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

#endif // __IMAGE_VECTORIZATION_H__
