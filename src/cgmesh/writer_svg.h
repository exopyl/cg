#pragma once

#include <list>
using namespace std;

typedef struct point2D
{
	void set (float _x, float _y) { x = _x; y = _y; };
	float x,y;
} point2D;



typedef struct PathStyle
{
	bool  bClosed;
	unsigned char  ucFillR, ucFillG, ucFillB;
	unsigned char  ucStrokeR, ucStrokeG, ucStrokeB;
	float fStrokeWidth;
} PathStyle;

//
//
//
class WriterSVG
{
public:
	WriterSVG(void);
	~WriterSVG(void);

	bool InitFile (char* pFilename);

	void WriteHeader (float fWidth, float fHeight);
	void WriteFooter (void);

	void WriteStyleBegin (char* pStroke = NULL, float fStrokeWidth = 1.0, char * pColorFill = NULL);
	void WriteStyleEnd ();

	void WriteGroupBegin (char *strId = NULL);
	void WriteGroupEnd (void);

	void WritePath (list<point2D> listPoints, PathStyle *pathStyle = NULL);
	void WritePath (list<list<point2D> > listsPoints, PathStyle *pathStyle = NULL);

	static inline void PathStyle_init (PathStyle *pathStyle)
		{
			pathStyle->bClosed = true;
			pathStyle->ucFillR = 255;
			pathStyle->ucFillG = 255;
			pathStyle->ucFillB = 255;
			pathStyle->ucStrokeR = 0;
			pathStyle->ucStrokeG = 0;
			pathStyle->ucStrokeB = 0;
			pathStyle->fStrokeWidth = 1.;
		}

private:
	FILE* m_pFile;
};


/*

*-> x
|
V

y
*/
