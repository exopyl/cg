#include <stdio.h>
#include <math.h>
#include <limits.h>

#include "image_vectorization.h"
#include "../cgimg/cgimg.h"
#include "writer_svg.h"

CLitRasterToVector::CLitRasterToVector(void)
{
	m_palette = NULL;
}

CLitRasterToVector::~CLitRasterToVector(void)
{
	for(MapListPath::const_iterator itMapListPath = m_mapListPath.begin(); itMapListPath != m_mapListPath.end(); itMapListPath++)
	{
		ListPath* pListPath  = itMapListPath->second;

		for(ListPath::const_iterator it = pListPath->begin();it!=pListPath->end();it++)
		{
			TPath* pPath = *it;
			delete pPath;
		}
		delete pListPath;
	}

	if(m_palette)
		delete[] m_palette;
}

bool CLitRasterToVector::Vectorize(Img* pInput,
				   Color colorMask,
				   bool bUseMask,
				   Palette *pPalette)
{
	printf("\n########### Vectorize ############\n");

	// initialize the palette
	printf("InitializePalette :\n");
	if (!pPalette)
		pPalette = pInput->get_palette ();
	
	m_iPaletteSize = pPalette->NColors ();
	m_palette = new Color[m_iPaletteSize+1];
	for(int i=0;i<m_iPaletteSize;i++)
		m_palette[i] = Color (pPalette->m_pColors[i]);

	printf ("   use the following palette :\n");
	for(int i=0;i<m_iPaletteSize;i++)
		printf ("   %d / %d : %d %d %d\n", i, m_iPaletteSize, m_palette[i].r(), m_palette[i].g(), m_palette[i].b());
	

	Img sPalettizedNoBorder;
	sPalettizedNoBorder.palettize(pInput);

	// MAKE BORDER TO BE SURE TO HAVE CLOSED SURFACE
	Img sPalettized (sPalettizedNoBorder.width()+2, sPalettizedNoBorder.height()+2, true);
	for(int y=0;y<sPalettizedNoBorder.height();y++)
		for(int x=0;x<sPalettizedNoBorder.width();x++)
		{
			unsigned char r, g, b, a;
			sPalettizedNoBorder.get_pixel(x,y, &r, &g, &b, &a);
			sPalettized.set_pixel(x+1, y+1, r, g, b, a); // that makes a border with 0
		}

	int iBorderColor = m_iPaletteSize;
	for(int x=0;x<sPalettized.width();x++)
	{
		sPalettized.set_pixel_index(x, 0, iBorderColor);
		sPalettized.set_pixel_index(x, sPalettized.height()-1, iBorderColor);
	}
	for(int y=0;y<sPalettized.height();y++)
	{
		sPalettized.set_pixel_index(0, y, iBorderColor);
		sPalettized.set_pixel_index(sPalettized.width()-1, y ,iBorderColor);
	}

	if(bUseMask)
	{
		m_palette[m_iPaletteSize] = colorMask;
		for(int y=0;y<sPalettized.height();y++)
			for(int x=0;x<sPalettized.width();x++)
			{
				unsigned char r, g, b, a;
				sPalettized.get_pixel(x,y, &r, &g, &b, &a);
				if (r == colorMask.r() && g == colorMask.g() && b == colorMask.b() && a == colorMask.a())
					sPalettized.set_pixel_index(x, y, m_iPaletteSize);
			}
		printf ("   mask : %d %d %d\n", m_palette[m_iPaletteSize].r(), m_palette[m_iPaletteSize].g(), m_palette[m_iPaletteSize].b());
	}
	
	for(int y=0;y<sPalettized.height();y++)
	{
		for(int x=0;x<sPalettized.width();x++)
			printf ("%d ", sPalettized.get_pixel_index (x, y));
		printf ("\n");
	}
	
	
	// VECTORIZE
	bool bOK = true;
	printf("GeneratePath :\n");
	bOK = GeneratePath(sPalettized);
	if(!bOK)
		return false;

	// SMOOTH 
	printf("SmoothCoords :\n");
	SmoothCoords();

	// SIMPLE SIMPLIFY, we keep all corners -> needed to sort layers
	printf("RemoveUselessPoint :\n");
	RemoveUselessPoint();

	// SORT LAYER
	printf("CalculateLayerOrder :\n");
	CalculateLayerOrder(sPalettized);
	
	// SIMPLIFY
	printf("Simplify :\n");
	Simplify(.5);

	return bOK;
}

bool CLitRasterToVector::GeneratePath (const Img& sIndexed)
{
	// contour containing the indexcolor
	//-----------------------------------------------+
	//			              |......................|
	//			              |......................|
	//           0,0         1,0 ....... 2,0.........|
	//           xx,yy        #..........x+1,y.......|
	//           x,y          #......................|
	//	                      #......................|
	//-----------0,1#########1,1---------2,1---------|
	//........................|......................|
	//................x,y+1...|......................|
	//........................|......................|
	//...........0,2.........1,2........2,2..........|
	//........................|......................|
	//........................|......................|
	//-----------------------------------------------+


	// on tourne autour du noir dans le sens trigo
	static const int miniStripElement[16][17]=
	{		//switch(abcd)
		//case 0:
		//	##
		//	##
		{-1},
		//case 1:
		//	0#
		//	##
		{1,0, 1,1, 0,1 ,-1,-1},			
		//case 2:
		//	#0
		//	##
		{2,1, 1,1, 1,0,-1,-1},
		//case 3:
		//	00
		//	##
		{2,1, 1,1, 0,1 ,-1,-1}, // keep the center pt to not generate hole during the smoothing : 2 layers must share this point
		//case 4:
		//	##
		//	0#
		{0,1, 1,1, 1,2 ,-1,-1},				
		//case 5:
		//	0#
		//	0#
		{1,0,1,1,1,2,-1,-1},				
		//case 6:
		//	#0
		//	0#
		{ 0,1, 1,1, 1,0, -1, 2,1, 1,1, 1,2,-1,-1},				
		//case 7:
		//	00
		//	0#
		{2,1,1,1, 1,2,-1,-1},			
		//case 8:
		//	##
		//	#0
		{1,2, 1,1,  2,1,-1,-1},
		//case 9:
		//	0#
		//	#0
		{1,2,1,1, 0,1 ,-1,  1,0, 1,1, 2,1 ,-1,-1},
		//case 10:
		//	#0
		//	#0
		{1,2,1,1, 1,0,-1,-1},
		//case 11:
		//	00
		//	#0
		{1,2, 1,1, 0,1,-1,-1},
		//case 12:
		//	##
		//	00
		{0,1, 1,1, 2,1,-1,-1},
		//case 13:
		//	0#
		//	00
		{ 1,0, 1,1,2,1,-1,-1},
		//case 14:
		//	#0
		//	00
		{0,1, 1,1,  1,0,-1,-1},
		//case 15:
		//	00
		//	00
		{-1,-1}
	};

	MapPath* mapPathStartArray = new MapPath[m_iPaletteSize];
	MapPath* mapPathEndArray   = new MapPath[m_iPaletteSize];
	for(int iColorIndex=0; iColorIndex<m_iPaletteSize ; iColorIndex++) 
	{
		ListPath* pListPath = new ListPath;
		m_mapListPath[iColorIndex] = pListPath;
	}

	bool bOk = true;
	for(int y=0; y<sIndexed.height()-1; y++)
	{
		const unsigned char* pBuffer	      = (const unsigned char*)sIndexed.m_pPixels + y*sIndexed.width();
		const unsigned char* pBufferLigneDown = (const unsigned char*)sIndexed.m_pPixels + (y+1)*sIndexed.width();

		for(int x=0; x<sIndexed.width()-1; x++)
		{
			// a b
			// c d

			unsigned char va = *pBuffer++;
			unsigned char vb = *pBuffer;
			unsigned char vc = *pBufferLigneDown++;
			unsigned char vd = *pBufferLigneDown;		

			for(int iColorIndex=0; iColorIndex<m_iPaletteSize ; iColorIndex++) 
			{
				unsigned long dcba = ((vd!=iColorIndex)<<3) | ((vc!=iColorIndex)<<2) | ((vb!=iColorIndex)<<1) | (va!=iColorIndex);
				if(dcba == 0 || dcba==15)
					continue;
				
				MapPath& mapPathStart = mapPathStartArray[iColorIndex];
				MapPath& mapPathEnd   = mapPathEndArray[iColorIndex];

				ListPath* pListPath = m_mapListPath[iColorIndex];

				int xx = x<<1; // the output is double size!
				int yy = y<<1;		

				const int *pIndex = miniStripElement[dcba];
				while(*pIndex != -1)
				{
					// load elementary path
					TPath miniPath;
					while(*pIndex != -1)
					{
						TPoint pt;
						pt.x = xx + *pIndex++;
						pt.y = yy + *pIndex++;
						miniPath.push_back(pt);				
					}

					AddPath(*pListPath,miniPath,mapPathStart,mapPathEnd);

					// jump -1 for end of mini path
					pIndex++;
				}
			}
		}
	}

	if(bOk)
	{
		for(int iColorIndex = 0; iColorIndex < m_iPaletteSize ; iColorIndex++) 
		{
			MapPath& mapPathStart = mapPathStartArray[iColorIndex];
			MapPath& mapPathEnd   = mapPathEndArray[iColorIndex];
			ListPath* pListPath   = m_mapListPath[iColorIndex];

			// close path
			while(MergePath(*pListPath,mapPathStart,mapPathEnd)) {}

			// remove useless pair point -> it remove alse double point start/end, because start/end have always one even coord
			for(ListPath::const_iterator it = pListPath->begin();it!=pListPath->end();it++)
			{
				TPath* pPath = *it;
				for(TPath::iterator itpt = pPath->begin(); itpt!=pPath->end();)
				{			
					if((itpt->x&1) == 0 || (itpt->y&1) == 0)
						itpt = pPath->erase(itpt);			
					else
						itpt++;				
				}
			}

			if (1)
			{	
				int iNbPath = (*pListPath).size();
				printf ("iNbPath : %d (%d %d %d)\n", iNbPath,
					m_palette[iColorIndex].r(),
					m_palette[iColorIndex].g(),
					m_palette[iColorIndex].b());

				// draw paths
				Img sDebug(sIndexed.width()*5,sIndexed.height()*5);
				for(ListPath::iterator it = (*pListPath).begin();it!=(*pListPath).end();it++)
				{
					TPath* pPath = *it;
					unsigned char r = 255.*(rand())/RAND_MAX;
					unsigned char g = 255.*(rand())/RAND_MAX;
					unsigned char b = 255.*(rand())/RAND_MAX;
					
					TPath::const_iterator itPath = pPath->begin();
					Vector2f interPointS = GetInterpolatedCoord(*itPath);
					
					for(; itPath != pPath->end();itPath++)
					{
						Vector2f interPointE = GetInterpolatedCoord(*itPath);
						sDebug.draw_line((interPointS[0]+.5)*5,
								 (interPointS[1]+.5)*5,
								 (interPointE[0]+.5)*5,
								 (interPointE[1]+.5)*5,
								 r, g, b, 255);
						interPointS = interPointE;
					}
				}

				char filename[30];
				sprintf (filename, "toto%02d.bmp", iColorIndex);
				sDebug.save(filename);
			}

			// remove holes!
			//RemoveHoles(*pListPath,iColorIndex,sIndexed);

			// set color to the path
			for(ListPath::iterator it = pListPath->begin() ; it != pListPath->end() ; it++)
			{
				TPath* pPath = *it;
				m_pathColor[pPath] = iColorIndex;
			}
		}
	}
	delete[] mapPathStartArray; 
	delete[] mapPathEndArray;

	return bOk;
}

void CLitRasterToVector::WriteFilePolygonWithHole(/*bool bBottomTop,
						  bool bFill,
						  bool bBorderColor,
						  bool bDrawWhiteLayer,
						  const Color* pForceFillColor,*/
						  float fLineWidth/*,
						  const Color& borderColor,
						  bool bBorder*/) const
{
	WriterSVG *pWriter = new WriterSVG ();
	pWriter->InitFile ("output2.svg");
	pWriter->WriteHeader (500, 500);
	list<point2D> listPoints;

	list<list<Vector3f> > listSubPath;

	for(list<TPath*>::const_iterator itLayer  = m_layerOrder.begin() ;itLayer!=m_layerOrder.end(); itLayer++)
	{
		printf ("+ layer\n");
/*
		if(!bDrawWhiteLayer)
		{
			if(itLayer == m_layerOrder.begin()) // jump first path corresponding to paper rect
				continue;
			//			if(iColorIndex == 0)
			//			continue;
		}
*/
		TPath* pPath = *itLayer;

		// get color of the polygon
		int iColorIndex = 0;
		map<TPath*,int>::const_iterator itIndex = m_pathColor.find(pPath);
		if(itIndex != m_pathColor.end())
			iColorIndex = itIndex->second;
		else
			continue;

		// draw polygon
		PathStyle pathStyle;
		WriterSVG::PathStyle_init (&pathStyle);
		pathStyle.ucStrokeR = 0;
		pathStyle.ucStrokeG = 0;
		pathStyle.ucStrokeB = 0;
		pathStyle.fStrokeWidth = 2.;

		list<Vector3f> newlist;
		listSubPath.push_back(newlist);

		vector<Vector3f> lineStrip;
		for(TPath::const_iterator itPath = pPath->begin() ; itPath != pPath->end();itPath++)
		{
			Vector2f inter = GetFinalCoord(*itPath);
			point2D pt;
			pt.set (inter[0], inter[1]);
			listPoints.push_back (pt);
			//printf ("%f %f\n", inter[0], inter[1]);

			listSubPath.back().push_back(Vector3f(inter[0],inter[1],0.f));
			//lineStrip.push_back(Vector3f(inter[0],inter[1],0.f));
		}

		Color* pFillColor = &m_palette[iColorIndex];
		pathStyle.ucFillR = pFillColor->r();
		pathStyle.ucFillG = pFillColor->g();
		pathStyle.ucFillB = pFillColor->b();
		//printf ("%d %d %d\n", pathStyle.ucFillR, pathStyle.ucFillG, pathStyle.ucFillB);
		pWriter->WritePath (listPoints, &pathStyle);
		listPoints.clear ();
	}

	pWriter->WriteFooter ();

	delete pWriter;
		
	return;

	
//
// original
//
/*
	list<list<Vector3f> > listSubPath;

	for(list<TPath*>::const_iterator itLayer  = m_layerOrder.begin() ;itLayer!=m_layerOrder.end(); itLayer++)
	{
		if(!bDrawWhiteLayer)
		{
			if(itLayer == m_layerOrder.begin()) // jump first path corresponding to paper rect
				continue;
			//			if(iColorIndex == 0)
			//			continue;
		}

		TPath* pPath = *itLayer;

		// get color of the polygon
		int iColorIndex = 0;
		map<TPath*,int>::const_iterator itIndex = m_pathColor.find(pPath);
		if(itIndex != m_pathColor.end())
			iColorIndex = itIndex->second;
		else
			continue;

		// draw polygon

		list<Vector3f> newlist;
		listSubPath.push_back(newlist);

		vector<Vector3f> lineStrip;
		for(TPath::const_iterator itPath = pPath->begin() ; itPath != pPath->end();itPath++)
		{
			Vector2f inter = GetFinalCoord(*itPath);

			listSubPath.back().push_back(Vector3f(inter[0],inter[1],0.f));
			//lineStrip.push_back(Vector3f(inter[0],inter[1],0.f));
		}
	}

	const Color* pFillColor = pForceFillColor;
	if(!pFillColor)
		pFillColor = &m_palette[1];


	if(bBorderColor)
		pWriter->WritePolygonWithHoles(listSubPath,bFill,*pFillColor,bBorder,borderColor,fLineWidth,0);
	else
		pWriter->WritePolygonWithHoles(listSubPath,bFill,*pFillColor,bBorder,*pFillColor,fLineWidth,0);
*/
}

void CLitRasterToVector::WriteFilePolygonWithHole2 (float fLineWidth)
{
	WriterSVG *pWriter = new WriterSVG ();
	pWriter->InitFile ("output.svg");
	pWriter->WriteHeader (500, 500);
	list<point2D> listPoints;

	int icolor=0;
	for( MapListPath::const_iterator itMapListPath=m_mapListPath.begin();
		itMapListPath!=m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;
			// get color of the polygon
			int iColorIndex = 0;
			map<TPath*,int>::const_iterator itIndex = m_pathColor.find(pPath);
			if(itIndex != m_pathColor.end())
				iColorIndex = itIndex->second;
			else
				continue;

			// draw polygon
			PathStyle pathStyle;
			WriterSVG::PathStyle_init (&pathStyle);
			pathStyle.ucStrokeR = 0;
			pathStyle.ucStrokeG = 0;
			pathStyle.ucStrokeB = 0;
			pathStyle.fStrokeWidth = 2.;
			vector<Vector3f> lineStrip;
			for(TPath::const_iterator itPath = pPath->begin() ; itPath != pPath->end();itPath++)
			{
				Vector2f inter = GetFinalCoord(*itPath);

				//printf ("%f %f\n", inter[0], inter[1]);
				point2D pt;
				pt.set (inter[0], inter[1]);
				listPoints.push_back (pt);
				lineStrip.push_back(Vector3f(inter[0],inter[1],0.f));
			}


			//Color* pFillColor = m_palette[iColorIndex];
			pathStyle.ucFillR = m_palette[iColorIndex].r();
			pathStyle.ucFillG = m_palette[iColorIndex].g();
			pathStyle.ucFillB = m_palette[iColorIndex].b();
			pWriter->WritePath (listPoints, &pathStyle);
			listPoints.clear ();
		}
	}
	pWriter->WriteFooter ();

	delete pWriter;
}



void CLitRasterToVector::WriteFile(float fLineWidth) const
{
	WriterSVG *pWriter = new WriterSVG ();
	pWriter->InitFile ("output.svg");
	pWriter->WriteHeader (500, 500);
	list<point2D> listPoints;
	for(list<TPath*>::const_iterator itLayer  = m_layerOrder.begin() ;itLayer!=m_layerOrder.end(); itLayer++)
	{
		TPath* pPath = *itLayer;
		// get color of the polygon
		int iColorIndex = 0;
		map<TPath*,int>::const_iterator itIndex = m_pathColor.find(pPath);
		if(itIndex != m_pathColor.end())
			iColorIndex = itIndex->second;
		else
			continue;

		// draw polygon
		PathStyle pathStyle;
		WriterSVG::PathStyle_init (&pathStyle);
		pathStyle.ucStrokeR = 0;
		pathStyle.ucStrokeG = 0;
		pathStyle.ucStrokeB = 0;
		pathStyle.fStrokeWidth = 2.;
		vector<Vector3f> lineStrip;
		for(TPath::const_iterator itPath = pPath->begin() ; itPath != pPath->end();itPath++)
		{
			Vector2f inter = GetFinalCoord(*itPath);

			//printf ("%f %f\n", inter[0], inter[1]);
			point2D pt;
			pt.set (inter[0], inter[1]);
			listPoints.push_back (pt);
			lineStrip.push_back(Vector3f(inter[0],inter[1],0.f));
		}
		

		//Color* pFillColor = m_palette[iColorIndex];
		pathStyle.ucFillR = m_palette[iColorIndex].r();
		pathStyle.ucFillG = m_palette[iColorIndex].g();
		pathStyle.ucFillB = m_palette[iColorIndex].b();
		pWriter->WritePath (listPoints, &pathStyle);
		listPoints.clear ();
	}
	pWriter->WriteFooter ();

	delete pWriter;
		
	return;

	//
	// original
	//
	for(list<TPath*>::const_iterator itLayer  = m_layerOrder.begin() ;itLayer!=m_layerOrder.end(); itLayer++)
	{
		TPath* pPath = *itLayer;

		// get color of the polygon
		int iColorIndex = 0;
		map<TPath*,int>::const_iterator itIndex = m_pathColor.find(pPath);
		if(itIndex != m_pathColor.end())
			iColorIndex = itIndex->second;
		else
			continue;

		// draw polygon
		vector<Vector3f> lineStrip;
		for(TPath::const_iterator itPath = pPath->begin() ; itPath != pPath->end();itPath++)
		{
			Vector2f inter = GetFinalCoord(*itPath);

			lineStrip.push_back(Vector3f(inter[0],inter[1],0.f));
		}

		const Color* pFillColor = &m_palette[iColorIndex];
/*
		bool bBorderForLayer = bBorder;
		Color colWhite;
		colWhite.SetRGB(255,255,255);
		if(!bDrawWhiteLayer)
		{
			if(iColorIndex == 0)
			{				
				pFillColor = &colWhite;
				//bBorderForLayer = false;
			}
		}
*/
/*
		if(bBorderColor)
			pWriter->WritePolygon(lineStrip,bFill,*pFillColor,bBorderForLayer,borderColor,fLineWidth,0);
		else
			pWriter->WritePolygon(lineStrip,bFill,*pFillColor,bBorderForLayer,*pFillColor,fLineWidth,0);
*/
	}
}

void CLitRasterToVector::RemoveHoles( ListPath& listPath, int iColorIndex, const Img& sIndexed )
{
	for(ListPath::iterator it = listPath.begin();it!=listPath.end();)
	{
		TPath* pPath = *it;
		
		// check if the interior color is equal to the path color
		int iExteriorColor = 0;
		int iInteriorColor = 0;
		if(GetLimitColor(*pPath, sIndexed, iInteriorColor, iExteriorColor))
		{
			if (iInteriorColor != iColorIndex)
			{
				delete pPath;
				it = listPath.erase(it);
				continue;
			}	
		}
		it++;
	}
}

bool CLitRasterToVector::MergePath(ListPath& listPath,
				   MapPath& mapPathStart,
				   MapPath& mapPathEnd)
{
	bool bMerged = false;
	list<ListPath::iterator> pathToRemove;
	for(ListPath::iterator it = listPath.begin();it!=listPath.end();it++)
	{
		TPath& pathToAdd = **it;
		if(pathToAdd.size()==0)
			continue;

		unsigned long ulIndexStart	= pathToAdd.front().x	| (pathToAdd.front().y<<16);
		unsigned long ulIndexEnd		= pathToAdd.back().x	| (pathToAdd.back().y<<16);

		// searching path finishing by start to add
		bool bAdded = false;
		MapPath::iterator itfind = mapPathEnd.find(ulIndexStart);
		if(itfind != mapPathEnd.end())
		{
			TPath* pPath = itfind->second;
			if(pPath == &pathToAdd)
				continue;

			TPath::const_iterator itpath = pathToAdd.begin();		
			itpath++;																					// jump the point to not have double!!
			for(; itpath != pathToAdd.end();itpath++)
				pPath->push_back(*itpath);
			// update the map
			mapPathEnd.erase(itfind);

			// remove pathToAdd from the map
			itfind = mapPathStart.find(ulIndexStart);
			if(itfind == mapPathStart.end())
				return (false);
			else
				mapPathStart.erase(itfind);

			//	itfind = mapPathEnd.find(ulIndexEnd);
			//	if(itfind != mapPathEnd.end())
			//		ASSERT(false);
			mapPathEnd[ulIndexEnd] = pPath;


			bAdded = true;
		}
		else
		{
			itfind = mapPathStart.find(ulIndexEnd);
			if(itfind != mapPathStart.end())
			{
				TPath* pPath = itfind->second;
				if(pPath == &pathToAdd)
					continue;

				TPath::reverse_iterator itpath = pathToAdd.rbegin();
				itpath++;																					// jump the point to not have double!!
				for( ;itpath != pathToAdd.rend();itpath++)
					pPath->push_front(*itpath);
				// update the map
				mapPathStart.erase(itfind);

				// remove pathToAdd from the map
				itfind = mapPathEnd.find(ulIndexEnd);
				if(itfind == mapPathEnd.end())
					return (false);
				mapPathStart.erase(itfind);

				//		itfind = mapPathStart.find(ulIndexStart);
				//		if(itfind != mapPathStart.end())
				//			ASSERT(false);
				mapPathStart[ulIndexStart] = pPath;
				bAdded = true;
			}
			else
			{
			}
		}

		if(bAdded)
		{
			bMerged = true;
			pathToAdd.clear();
			pathToRemove.push_back(it);
		}
	}

	for(list<ListPath::iterator>::iterator it = pathToRemove.begin(); it != pathToRemove.end();it++)
	{
		ListPath::iterator itToRemove = * it;
		TPath *pPath = *itToRemove;
		delete pPath;
		listPath.erase(itToRemove);
	}

	return bMerged;
}
void CLitRasterToVector::AddPath (ListPath& listPath,
				  const TPath& pathToAdd,
				  MapPath& mapPathStart,
				  MapPath& mapPathEnd)
{
	if(pathToAdd.size()<2)
	{
		return;
	}

	unsigned long ulIndexStart	= pathToAdd.front().x | (pathToAdd.front().y<<16);
	unsigned long ulIndexEnd		= pathToAdd.back().x | (pathToAdd.back().y<<16);

	// searching path finishing by start to add
	MapPath::iterator itfind = mapPathEnd.find(ulIndexStart);
	if(itfind != mapPathEnd.end())
	{
		TPath* pPath = itfind->second;
		TPath::const_iterator itpath = pathToAdd.begin();		
		itpath++;																					// jump the point to not have double!!
		for(; itpath != pathToAdd.end();itpath++)
			pPath->push_back(*itpath);
		// update the map
		mapPathEnd.erase(itfind);
		itfind = mapPathEnd.find(ulIndexEnd);
		if(itfind != mapPathEnd.end())
			return;
		mapPathEnd[ulIndexEnd] = pPath;
		return;
	}
	else
	{
		itfind = mapPathStart.find(ulIndexEnd);
		if(itfind != mapPathStart.end())
		{
			TPath* pPath = itfind->second;

			TPath::const_reverse_iterator itpath = pathToAdd.rbegin();
			itpath++;																					// jump the point to not have double!!
			for( ;itpath != pathToAdd.rend();itpath++)
				pPath->push_front(*itpath);
			// update the map
			mapPathStart.erase(itfind);
			itfind = mapPathStart.find(ulIndexStart);
			if(itfind != mapPathStart.end())
				return;
			mapPathStart[ulIndexStart] = pPath;
			return;
		}
		else
		{
		}
	}

	// no found
	TPath* pNewPath = new TPath;
	for(TPath::const_iterator itpath = pathToAdd.begin(); itpath != pathToAdd.end();itpath++)
		pNewPath->push_back(*itpath);

	listPath.push_back(pNewPath);

	// update map
	itfind = mapPathEnd.find(ulIndexEnd);
	if(itfind != mapPathEnd.end())
		return;
	mapPathEnd[ulIndexEnd] = pNewPath;

	itfind = mapPathStart.find(ulIndexStart);
	if(itfind != mapPathStart.end())
		return;
	mapPathStart[ulIndexStart] = pNewPath;
}

const Vector2f& CLitRasterToVector::GetInterpolatedCoord (const TPoint& pt)
{
	unsigned long uiPointIndex = pt.x + (pt.y<<16);
	MapCoord::const_iterator itCoord = m_mapCoord.find(uiPointIndex);
	if(itCoord != m_mapCoord.end())
		return itCoord->second;
	else
	{
		Vector2f& vec = m_mapCoord[uiPointIndex];
		vec[0] = pt.x*.5f;
		vec[1] = pt.y*.5f;
		return vec;
	}
}

const Vector2f& CLitRasterToVector::GetFinalCoord (const TPoint& pt) const
{
	unsigned long uiPointIndex = pt.x + (pt.y<<16);
	MapCoord::const_iterator itCoord = m_mapCoord.find(uiPointIndex);
	if(itCoord != m_mapCoord.end())
		return itCoord->second;
	else
	{
		static Vector2f dede(0.f,0.f);
		return dede;
	}
}

void CLitRasterToVector::BuildAdjacencyMap(MapAdjacence& mapAdjacence)
{
	for( MapListPath::const_iterator itMapListPath=m_mapListPath.begin();
	     itMapListPath!=m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;

			TPath::const_iterator itPath = pPath->begin();
			if( itPath == pPath->end() )
				continue;
			unsigned long indexPrev = itPath->x | (itPath->y<<16);

			itPath++;
			for( ; itPath != pPath->end();itPath++)
			{
				unsigned long index = itPath->x | (itPath->y<<16);

				MapAdjacence::iterator itfind =  mapAdjacence.find(index);
				if(itfind == mapAdjacence.end())
					mapAdjacence[index].insert(indexPrev);
				else
					itfind->second.insert(indexPrev);

				itfind =  mapAdjacence.find(indexPrev);
				if(itfind == mapAdjacence.end())
					mapAdjacence[indexPrev].insert(index);
				else
					itfind->second.insert(index);

				indexPrev = index;
			}

			// link between end and start
			itPath = pPath->begin();
			{
				unsigned long index = itPath->x | (itPath->y<<16);

				MapAdjacence::iterator itfind =  mapAdjacence.find(index);
				if(itfind == mapAdjacence.end())
					mapAdjacence[index].insert(indexPrev);
				else
					itfind->second.insert(indexPrev);

				itfind =  mapAdjacence.find(indexPrev);
				if(itfind == mapAdjacence.end())
					mapAdjacence[indexPrev].insert(index);
				else
					itfind->second.insert(index);
			}
		}
	}
}

void CLitRasterToVector::SmoothCoords (void)
{
	// CALCULATE ADJACENCE
	MapAdjacence mapAdjacence;
	BuildAdjacencyMap(mapAdjacence);

	// COMPUTE COORDS  = fill m_mapCoord
	for(MapAdjacence::const_iterator itCoord = mapAdjacence.begin(); itCoord != mapAdjacence.end() ; itCoord++)
	{
		unsigned long uiPointIndex = itCoord->first;
		int x = uiPointIndex&0xffff;
		int y = uiPointIndex>>16;
		//m_mapCoord[uiPointIndex].Set( x*.5f +.5f , y*.5f -1.f); // with no inputWithBlackBorder
		m_mapCoord[uiPointIndex].Set( x*.5f +.5f -1, y*.5f +.5f); // with  inputWithBlackBorder
	}

	// TAUBIN FILTERING (because classic Laplacien filtered is too strong, and loose square, and round corner too much)
	//		http://mesh.brown.edu/taubin/pdfs/taubin-eg00star.pdf
	//		http://www.lems.brown.edu/~msj/cs292/assign4/volumeVis.html
	//		use = lambda,mu = .6307,-.6732
	for(int iPass = 0; iPass<10 ;iPass++)
	{
		for(int iPhase = 0; iPhase<2;iPhase++)
		{
			MapCoord newMapCoord;

			for(MapAdjacence::const_iterator itCoord = mapAdjacence.begin(); itCoord != mapAdjacence.end() ; itCoord++)
			{
				unsigned long uiPointIndex = itCoord->first;
				Vector2f coord = m_mapCoord[uiPointIndex];

				Vector2f gravity(0,0);

				for(set<unsigned long>::const_iterator it = itCoord->second.begin(); it != itCoord->second.end();it++)
				{
					unsigned long uiPointIndexA = *it;
					gravity += m_mapCoord[uiPointIndexA];
				}

				gravity *= 1.f/(itCoord->second.size());

				float coef;
				if(iPhase == 0)
					coef = .6307;	// lambda
				else
					coef = -.6732; // mu

				//coef = .5;
				newMapCoord[uiPointIndex] = coord + (gravity-coord) *coef;
			}

			m_mapCoord = newMapCoord;
		}
	}
}

bool CLitRasterToVector::GetLimitColor(const TPath& path, const Img& sIndexed, int& iInteriorColor, int& iExteriorColor)
{
	int i = 0;
	iInteriorColor = 0;
	iExteriorColor = 0;
	TPoint ptMin(INT_MAX,INT_MAX);

	for(TPath::const_iterator itPath = path.begin() ; itPath != path.end();itPath++)
	{
		const TPoint& pt = *itPath;
		if(pt.x<=ptMin.x && ((pt.x!=ptMin.x) || (pt.y<ptMin.y)))
			ptMin = pt;
	}
	
	if(ptMin.x == INT_MAX)
		return false; // the path can't be empty

	if(((ptMin.x & 1) == 1) && ((ptMin.y & 1) == 1))
	{
		printf ("%d %d\n", ptMin.x, ptMin.y);
		int xx = ptMin.x>>1;
		int yy = (ptMin.y>>1) +1;
		printf ("=> %d %d\n", xx, yy);
		unsigned char r, g, b, a;
		sIndexed.get_pixel(xx  ,yy, &r, &g, &b, &a);
		iExteriorColor = Color::RGBc2Int(r, g, b);
		sIndexed.get_pixel(xx+1,yy, &r, &g, &b, &a);
		iInteriorColor = Color::RGBc2Int(r, g, b);
	}
	else
		return (false); // occurs when touching border : to do!

	return true;
}

// interior : ptLimit+(1,0)
// exterior : ptLimit = exterior
// false if error!
bool CLitRasterToVector::GetLimitPixelCoord(const TPath& path,TPoint& ptLimit)
{
	int i = 0;
	TPoint ptMin(INT_MAX,INT_MAX);

	for(TPath::const_iterator itPath = path.begin() ; itPath != path.end();itPath++)
	{
		const TPoint& pt = *itPath;
		if(pt.x<=ptMin.x && ((pt.x!=ptMin.x) || (pt.y<ptMin.y)))
			ptMin = pt;
	}

	if(ptMin.x == INT_MAX)
		return false; // the path can't be empty

	if( ((ptMin.x & 1) == 1) && ((ptMin.y & 1) == 1))
	{
		ptLimit.x = ptMin.x>>1;
		ptLimit.y = (ptMin.y>>1) +1;
		return true;
	}
	else
		return false; // occurs when touching border : to do!

	return false;
}

void CLitRasterToVector::CalculateLayerOrder(const Img& sPalettized)
{
	printf("   CalculateLayerOrder_fill :\n");

	//
	// fill each closed contour with a unique index
	//
	typedef map<unsigned long,TPath*> MapIndexedPath;
	MapIndexedPath mapIndexedPath;
	list<unsigned long> listNewIndexes;

	Img sOneColorByPolygon (sPalettized); // copy the palettized image

	for (MapListPath::const_iterator itMapListPath = m_mapListPath.begin(); itMapListPath != m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;
			TPoint ptLimit;
			bool bLimitFound = GetLimitPixelCoord(*pPath,ptLimit);
			if(!bLimitFound)
				continue;
			printf ("ptLimit %d %d\n", ptLimit.x, ptLimit.y);

			unsigned char r = (unsigned char)(255.*rand()/RAND_MAX);
			unsigned char g = (unsigned char)(255.*rand()/RAND_MAX);
			unsigned char b = (unsigned char)(255.*rand()/RAND_MAX);
			unsigned long uiUniqueIndex = Color::RGBc2Int(r, g, b);
/*
			while (1)
			{
				unsigned long offset = 2;
				uiUniqueIndex = m_iPaletteSize+offset;
				offset++;
				if(uiUniqueIndex <= m_iPaletteSize) // dont fill with an already used color!!! (border/mask use m_iPaletteSize)
					continue;
				MapIndexedPath::iterator it = mapIndexedPath.find(uiUniqueIndex);
				if(it == mapIndexedPath.end())
					break;
			}
*/
			mapIndexedPath[uiUniqueIndex] = pPath;

			listNewIndexes.push_back(uiUniqueIndex);
			sOneColorByPolygon.flood_fill(ptLimit.x+1, ptLimit.y, r, g, b);
		}
	}
	sOneColorByPolygon.save("sOneColorByPolygon.bmp");

	// Generate relationship between path
	printf("   CalculateLayerOrder_Generate relationship :\n");
	MapOrder mmorders, mmordersInvert;	
	for( MapListPath::const_iterator itMapListPath = m_mapListPath.begin(); itMapListPath != m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;
			int iInteriorColor, iExteriorColor;
			bool bLimitFound = GetLimitColor(*pPath,sOneColorByPolygon,iInteriorColor,iExteriorColor);
			if(!bLimitFound)
				continue;
			mmorders.insert(MapOrder::value_type(iExteriorColor,iInteriorColor));
			mmordersInvert.insert(MapOrder::value_type(iInteriorColor,iExteriorColor));
		}
	}

	// Sort path
	printf("   CalculateLayerOrder_Sort path (size = %d): \n", mmorders.size());
	while(mmorders.size())
	{
		// search the layer above all 
		unsigned long uiLayerOnTop;
		bool bFound = false;

		for(list<unsigned long>::iterator itNewIndex = listNewIndexes.begin(); itNewIndex != listNewIndexes.end(); )
		{
			unsigned long iLayer = *itNewIndex;

			std::pair<MapOrder::iterator , MapOrder::iterator> it =	mmorders.equal_range(iLayer);
			if(it.first == it.second)
			{
				uiLayerOnTop = iLayer;
				bFound = true;

				itNewIndex = listNewIndexes.erase(itNewIndex);

				MapIndexedPath::iterator it = mapIndexedPath.find(uiLayerOnTop);
				if(it != mapIndexedPath.end())
					m_layerOrder.push_front(it->second);
				else
					return;

				// filter mmorders
				std::pair<MapOrder::iterator , MapOrder::iterator> itInv =	mmordersInvert.equal_range(uiLayerOnTop);
				for(MapOrder::iterator itMapInv = itInv.first; itMapInv!=itInv.second;itMapInv++)
				{
					// for each layer below uiLayerOnTop, remove relation with uiLayerOnTop
					unsigned long ulLayerBelow = itMapInv->second;
					std::pair<MapOrder::iterator , MapOrder::iterator> itBelow =	mmorders.equal_range(ulLayerBelow);
					for(MapOrder::iterator itMapBelow = itBelow.first; itMapBelow!=itBelow.second;)
					{
						MapOrder::iterator itNext = itMapBelow;
						itNext++;
						if(itMapBelow->second == uiLayerOnTop)
							mmorders.erase(itMapBelow);
						itMapBelow = itNext;
					}
				}
				mmordersInvert.erase(itInv.first,itInv.second);
			}
			else
				itNewIndex++;
		}

		if(!bFound)
		{
			// loop in condition !!!!!!!!!!!
			break;
		}
	}
}

void CLitRasterToVector::RemoveUselessPoint( void )
{
	set<unsigned long> usefullPoints;

	// get all corner points
	for( MapListPath::const_iterator itMapListPath = m_mapListPath.begin(); itMapListPath != m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;
			TPath::iterator itpt = pPath->begin();
			if(itpt == pPath->end())
				continue;
			unsigned long uiPointPrevIndex = itpt->x +  (itpt->y <<16);
			TPoint ptPrev = *itpt;
			itpt++;
			int dxPrev = 0;
			int dyPrev = 0;
			for( ; itpt!=pPath->end();itpt++)
			{
				int dx = itpt->x - ptPrev.x;
				int dy = itpt->y - ptPrev.y;

				if(dxPrev != dx ||	dyPrev != dy)
					usefullPoints.insert(uiPointPrevIndex);

				dxPrev = dx;
				dyPrev = dy;
				ptPrev = *itpt;
				uiPointPrevIndex = itpt->x +  (itpt->y <<16);
			}

			// last point
			itpt = pPath->begin();
			{
				int dx = itpt->x - ptPrev.x;
				int dy = itpt->y - ptPrev.y;
				if(dxPrev != dx || dyPrev != dy)
					usefullPoints.insert(uiPointPrevIndex);
			}
		}
	}


	// filter
	for( MapListPath::const_iterator itMapListPath = m_mapListPath.begin(); itMapListPath != m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;
			for( TPath::iterator itpt = pPath->begin(); itpt!=pPath->end();)
			{
				unsigned long uiPointIndex = itpt->x +  (itpt->y <<16);
				if(usefullPoints.find(uiPointIndex) != usefullPoints.end())
					itpt++;
				else
					itpt = pPath->erase(itpt);
			}
		}
	}
}

void CLitRasterToVector::Simplify( float fErr )
{
	MapAdjacence mapAdjacence;
	MapAdjacence mapLinkedPoints;
	set<unsigned long> removedPoints;

	BuildAdjacencyMap(mapAdjacence);

	for(;;)
	{
		bool bSimplified = false;
		for(MapAdjacence::iterator itPt = mapAdjacence.begin(); itPt != mapAdjacence.end() ; )
		{
			if(itPt->second.size() != 2)
			{
				itPt++;
				continue;
			}

			unsigned long uiPointIndex = itPt->first;
			const Vector2f& pt = m_mapCoord[uiPointIndex];		

			// this points is between AB :
			unsigned long uiPointIndexA = *itPt->second.begin();
			unsigned long uiPointIndexB = *itPt->second.rbegin();
			if(uiPointIndexA == uiPointIndexB)
			{
				itPt++;
				continue;
			}
			const Vector2f& ptA = m_mapCoord[uiPointIndexA];	
			const Vector2f& ptB = m_mapCoord[uiPointIndexB];		

			// evaluate chordale error if we remove this point
			Vector2f v = ptB-ptA;
			v.Normalize();
			Vector2f w = pt-ptA;
			float fChordalErr = fabs(v[0]*w[1] - v[1]*w[0]); 
			if(fChordalErr > fErr)
			{
				itPt++;
				continue;
			}

			// check that all points linked to P verify the new error
			bool bLinkedPointsOk = true;
			MapAdjacence::iterator itLinkedFind = mapLinkedPoints.find(uiPointIndex);
			if(itLinkedFind!=mapLinkedPoints.end())
			{
				const set<unsigned long>& linkedPoints = itLinkedFind->second;
				for(set<unsigned long>::const_iterator itPtLinked = linkedPoints.begin() ; itPtLinked != linkedPoints.end();itPtLinked++)
				{
					unsigned long uiLinkedPointIndex = *itPtLinked;
					const Vector2f& ptLinked = m_mapCoord[uiLinkedPointIndex];

					Vector2f v = ptB-ptA;
					v.Normalize();
					Vector2f w = ptLinked-ptA;
					float fChordalErr = fabs(v[0]*w[1] - v[1]*w[0]); 
					if(fChordalErr > fErr)
					{
						bLinkedPointsOk = false;
						break;
					}
				}
			}
			if(!bLinkedPointsOk)
			{
				itPt++;
				continue;
			}

			// set  uiPointIndex as remove for cleaning path after
			removedPoints.insert(uiPointIndex);

			// remove P from neightboor of A but add B
			MapAdjacence::iterator itPtA = mapAdjacence.find(uiPointIndexA);
			if(itPtA != mapAdjacence.end())
				itPtA->second.erase(uiPointIndex);
			else
				return;

			itPtA->second.insert(uiPointIndexB);

			// remove P from neightboor of B but add A
			MapAdjacence::iterator itPtB = mapAdjacence.find(uiPointIndexB);
			if(itPtB != mapAdjacence.end())
				itPtB->second.erase(uiPointIndex);
			else
				return;
			itPtB->second.insert(uiPointIndexA);

			// add P and all linked point of P to linked points list of A
			MapAdjacence::iterator itLinkedAFind = mapLinkedPoints.find(uiPointIndexA);
			if(itLinkedAFind == mapLinkedPoints.end())
			{
				mapLinkedPoints[uiPointIndexA].insert(uiPointIndex);
				itLinkedAFind = mapLinkedPoints.find(uiPointIndexA);
			}
			else
				itLinkedAFind->second.insert(uiPointIndex);			

			if(itLinkedFind != mapLinkedPoints.end())
			{
				set<unsigned long>& linkedPointsA = itLinkedAFind->second;
				const set<unsigned long>& linkedPoints = itLinkedFind->second;
				for(set<unsigned long>::const_iterator itPtLinked = linkedPoints.begin() ; itPtLinked != linkedPoints.end();itPtLinked++)
				{
					unsigned long uiLinkedPointIndex = *itPtLinked;
					linkedPointsA.insert(uiLinkedPointIndex);
				}
			}

			// add P to linked points list of B
			MapAdjacence::iterator itLinkedBFind = mapLinkedPoints.find(uiPointIndexB);
			if(itLinkedBFind == mapLinkedPoints.end())
			{
				mapLinkedPoints[uiPointIndexB].insert(uiPointIndex);
				itLinkedBFind = mapLinkedPoints.find(uiPointIndexB);
			}
			else
				itLinkedBFind->second.insert(uiPointIndex);

			if(itLinkedFind!=mapLinkedPoints.end())
			{
				set<unsigned long>& linkedPointsB = itLinkedBFind->second;
				set<unsigned long>& linkedPoints = itLinkedFind->second;
				for(set<unsigned long>::const_iterator itPtLinked = linkedPoints.begin() ; itPtLinked != linkedPoints.end();itPtLinked++)
				{
					unsigned long uiLinkedPointIndex = *itPtLinked;
					linkedPointsB.insert(uiLinkedPointIndex);
				}
				// free linked point of P
				linkedPoints.clear();
			}
			// remove from map and next
			MapAdjacence::iterator itPtNext = itPt;
			itPtNext++;
			mapAdjacence.erase(itPt);
			itPt = itPtNext;
			bSimplified = true;
		}
		if(!bSimplified)
			break;
	}

	//printf("nb points removed : %d\n",removedPoints.size());

	// filter
	for( MapListPath::const_iterator itMapListPath = m_mapListPath.begin(); itMapListPath != m_mapListPath.end(); itMapListPath++)
	{
		const ListPath& listPath  = *itMapListPath->second;
		for(ListPath::const_iterator it = listPath.begin();it!=listPath.end();it++)
		{
			TPath* pPath = *it;
			for( TPath::iterator itpt = pPath->begin(); itpt!=pPath->end();)
			{
				unsigned long uiPointIndex = itpt->x +  (itpt->y <<16);
				if(removedPoints.find(uiPointIndex) == removedPoints.end())
					itpt++;
				else
					itpt = pPath->erase(itpt);
			}
		}
	}
}
