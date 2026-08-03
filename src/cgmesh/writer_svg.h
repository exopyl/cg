#pragma once

#include <list>

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

	void WriteStyleBegin (char* pStroke = nullptr, float fStrokeWidth = 1.0, char * pColorFill = nullptr);
	void WriteStyleEnd ();

	void WriteGroupBegin (char *strId = nullptr);
	void WriteGroupEnd (void);

	void WritePath (const std::list<point2D>& listPoints, PathStyle *pathStyle = nullptr);
	void WritePath (const std::list<std::list<point2D> >& listsPoints, PathStyle *pathStyle = nullptr);

	// ------------------------------------------------------------------------
	//  polygon / polyline : ferme ou ouvert, dit par l'ELEMENT
	// ------------------------------------------------------------------------
	// Un `<polygon>` est FERME par definition de la spec SVG -- le dernier point
	// rejoint le premier automatiquement. Un `<polyline>` est son pendant OUVERT.
	// La distinction est donc portee par le nom de l'element, sans attribut ni
	// convention a respecter.
	//
	// C'est ce qui manquait a WritePath : le `Z` de fermeture n'y est ecrit que si
	// l'appelant fournit un PathStyle avec bClosed, et le laisser a nullptr -- le
	// defaut -- produit un trace ouvert sans que rien ne le signale. Un lecteur
	// n'avait alors aucun moyen de savoir si un trace delimite une surface ou
	// n'est qu'une ligne, ce qui conduisait a le remplir d'office.
	//
	// Cote lecture, nanosvg reporte exactement cette distinction dans
	// NSVGpath::closed (nanosvg.h:2826-2833 : meme parseur, closeFlag 0 ou 1).
	//
	// Ne fermez PAS la liste en repetant le premier point pour un polygone : la
	// fermeture est implicite, un point repete ne ferait qu'un sommet en double.
	void WritePolygon  (const std::list<point2D>& listPoints);
	void WritePolyline (const std::list<point2D>& listPoints);

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
