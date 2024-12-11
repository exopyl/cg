#include <stdio.h>
#include <string.h>

#include "writer_svg.h"

//
//
//
WriterSVG::WriterSVG(void)
{
	m_pFile = NULL;
}

//
//
//
WriterSVG::~WriterSVG(void)
{
	if (m_pFile)
		fclose (m_pFile);
}

//
//
//
bool WriterSVG::InitFile (char* pFilename)
{
	if (m_pFile)
		fclose (m_pFile);
	m_pFile = NULL;

	m_pFile = fopen (pFilename, "w");
	return (m_pFile != NULL);
}

//
// Description:
//
// <?xml version="1.0" encoding="UTF-8" standalone="no"?>
// <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="297.000mm" height="210.000mm" viewBox="0.000000 0.000000 1052.362179 744.094470" stroke-linejoin="round" stroke-linecap="round" fill="none">
//
void WriterSVG::WriteHeader (float fWidth, float fHeight)
{
	if (!m_pFile)
		return;
	fprintf (m_pFile, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n");
	return;

	fprintf (m_pFile, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
	fprintf (m_pFile, "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"%fmm\" height=\"%fmm\" viewBox=\"0.000000 0.000000 %f %f\" stroke-linejoin=\"round\" stroke-linecap=\"round\" fill=\"none\">\n", fWidth, fHeight, fWidth, fHeight);
}

void WriterSVG::WriteFooter (void)
{
	if (!m_pFile)
		return;
	fprintf (m_pFile, "</svg>\n");
}

void WriterSVG::WriteStyleBegin (char* pStroke, float fStrokeWidth, char * pColorFill)
{
	if (!m_pFile)
		return;
	//fill:#a0a0a0;
	fprintf (m_pFile, "<g style=\"");
	
	if (pStroke)
		fprintf (m_pFile, "stroke:%s;", pStroke); // example : rgb(0,0,0)
	if (fStrokeWidth >= 0.)
		fprintf (m_pFile, "stroke-width:%f;", fStrokeWidth);
	if (pColorFill)
		fprintf (m_pFile, "fill:%s;", pColorFill); // example : #a0a0a0

	fprintf (m_pFile, "\">\n");
}

void WriterSVG::WriteStyleEnd ()
{
	if (!m_pFile)
		return;
	fprintf (m_pFile, "</g>\n");
}

void WriterSVG::WriteGroupBegin (char *strId)
{
	if (!m_pFile)
		return;

	if (strId == NULL)
		fprintf (m_pFile, "<g>\n");
	else
		fprintf (m_pFile, "<g id=\"%s\">\n", strId);
}

void WriterSVG::WriteGroupEnd (void)
{
	if (!m_pFile)
		return;

	fprintf (m_pFile, "</g>\n");
}

//
//
//
/*
<g id="Actor_7.0">
<g style="stroke:rgb(255,0,0);stroke-width:0.705">
<path d="M230.39,79.47 L230.39,79.44 "/>
<path d="M252.17,129.06 L252.20,129.08 "/>
<path d="M250.04,130.18 L251.36,123.57 L234.17,110.14 "/>
<path d="M272.78,118.57 L271.85,117.89 "/>
<path d="M274.79,117.51 L274.81,117.53 "/>
<path d="M251.36,123.57 L276.52,111.02 "/>
</g>
</g>
*/
void WriterSVG::WritePath (list<point2D> listPoints, PathStyle *pathStyle)
{
	if (!m_pFile)
		return;

	fprintf (m_pFile, "<path");
	if (pathStyle)
	{
		char hexcol[16];

		// fill
#ifdef WIN32
		_snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucFillR, pathStyle->ucFillG, pathStyle->ucFillB);
#else
		snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucFillR, pathStyle->ucFillG, pathStyle->ucFillB);
#endif
		fprintf (m_pFile, " fill=\"%s\"", hexcol);

		// stroke
#ifdef WIN32
		_snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucStrokeR, pathStyle->ucStrokeG, pathStyle->ucStrokeB);
#else
		snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucStrokeR, pathStyle->ucStrokeG, pathStyle->ucStrokeB);
#endif
		fprintf (m_pFile, " stroke=\"%s\"", hexcol);
	}
	list<point2D>::iterator it=listPoints.begin();
	point2D pt = (*it);
	fprintf (m_pFile, " d=\"M%f,%f ", pt.x, pt.y);
	for (it++; it!=listPoints.end(); it++)
	{
		pt = (*it);
		fprintf (m_pFile, "L%f,%f ", pt.x, pt.y);
	}
	if (pathStyle && pathStyle->bClosed)
		fprintf (m_pFile, "Z");
	fprintf (m_pFile, "\"/>\n");
}

void WriterSVG::WritePath (list<list<point2D> > listsPoints, PathStyle *pathStyle)
{
	if (!m_pFile)
		return;

	fprintf (m_pFile, "<path");
	if (pathStyle)
	{
		char hexcol[16];

		// fill
#ifdef WIN32
		_snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucFillR, pathStyle->ucFillG, pathStyle->ucFillB);
#else
		snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucFillR, pathStyle->ucFillG, pathStyle->ucFillB);
#endif
		fprintf (m_pFile, " fill=\"%s\"", hexcol);

		// stroke
#ifdef WIN32
		_snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucStrokeR, pathStyle->ucStrokeG, pathStyle->ucStrokeB);
#else
		snprintf(hexcol, sizeof (hexcol), "#%02x%02x%02x", pathStyle->ucStrokeR, pathStyle->ucStrokeG, pathStyle->ucStrokeB);
#endif
		fprintf (m_pFile, " stroke=\"%s\"", hexcol);
	}

	fprintf (m_pFile, " d=\"");
	list<list<point2D> >::iterator itList;
	for (itList = listsPoints.begin(); itList != listsPoints.end(); itList++)
	{
		list<point2D> listPoints = *itList;
		list<point2D>::iterator it=listPoints.begin();
		point2D pt = (*it);
		fprintf (m_pFile, "M%f,%f ", pt.x, pt.y);
		for (it++; it!=listPoints.end(); it++)
		{
			pt = (*it);
			fprintf (m_pFile, "L%f,%f ", pt.x, pt.y);
		}
		if (pathStyle && pathStyle->bClosed)
			fprintf (m_pFile, "Z ");
	}
	fprintf (m_pFile, "\"/>\n");
}
