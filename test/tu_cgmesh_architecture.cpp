#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

// #include "../extern/tinyXML/tinyxml.h"

static void polygon2_tesselation_export (Polygon2 *pol, const char *filename)
{
	// tesselation
	unsigned int nVertices, nFaces;
	float *pVertices = NULL;
	unsigned int *pFaces = NULL;
	pol->tesselate (&pVertices, &nVertices, &pFaces, &nFaces);
	
	// export
	Mesh *m = new Mesh ();
	m->SetVertices (nVertices, pVertices);
	m->SetFaces (nFaces, 3, pFaces, NULL);
	m->save ((char*)filename);

	// cleaning
	free (pVertices);
	free (pFaces);
	delete m;
}

#ifdef USE_TINYXML
#include "../extern/tinyXML/tinyxml.h"

std::map<std::string, Polygon2*> library;
static Polygon2 *pFacade = NULL;

const unsigned int NUM_INDENTS_PER_SPACE=2;

const char * getIndent( unsigned int numIndents )
{
	static const char * pINDENT="                                      + ";
	static const unsigned int LENGTH=strlen( pINDENT );
	unsigned int n=numIndents*NUM_INDENTS_PER_SPACE;
	if ( n > LENGTH ) n = LENGTH;

	return &pINDENT[ LENGTH-n ];
}

// same as getIndent but no "+" at the end
const char * getIndentAlt( unsigned int numIndents )
{
	static const char * pINDENT="                                        ";
	static const unsigned int LENGTH=strlen( pINDENT );
	unsigned int n=numIndents*NUM_INDENTS_PER_SPACE;
	if ( n > LENGTH ) n = LENGTH;

	return &pINDENT[ LENGTH-n ];
}

int dump_attribs_to_stdout(TiXmlElement* pElement, unsigned int indent)
{
	if ( !pElement ) return 0;

	TiXmlAttribute* pAttrib=pElement->FirstAttribute();
	int i=0;
	int ival;
	double dval;
	const char* pIndent=getIndent(indent);
	printf("\n");
	while (pAttrib)
	{
		printf( "%s%s: value=[%s]", pIndent, pAttrib->Name(), pAttrib->Value());

		if (pAttrib->QueryIntValue(&ival)==TIXML_SUCCESS)    printf( " int=%d", ival);
		if (pAttrib->QueryDoubleValue(&dval)==TIXML_SUCCESS) printf( " d=%1.1f", dval);
		printf( "\n" );
		i++;
		pAttrib=pAttrib->Next();
	}
	return i;	
}

static const char* GetStringFromAttribute (TiXmlNode* pNode, char *attribName)
{
	TiXmlAttribute* pAttrib=pNode->ToElement()->FirstAttribute();
	while (pAttrib)
	{
		if (strcmp (pAttrib->Name(), attribName) == 0)
			return pAttrib->Value();
		pAttrib=pAttrib->Next();
	}
	return NULL;
}

static int GetIntegerFromAttribute (TiXmlNode* pNode, char *attribName)
{
	TiXmlAttribute* pAttrib=pNode->ToElement()->FirstAttribute();
	int ival;
	while (pAttrib)
	{
		if (strcmp (pAttrib->Name(), attribName) == 0)
		{
			if (pAttrib->QueryIntValue(&ival)==TIXML_SUCCESS)
				return ival;
		}
		pAttrib=pAttrib->Next();
	}
	return -1;
}

static float GetFloatFromAttribute (TiXmlNode* pNode, char *attribName)
{
	TiXmlAttribute* pAttrib=pNode->ToElement()->FirstAttribute();
	double dval;
	while (pAttrib)
	{
		if (strcmp (pAttrib->Name(), attribName) == 0)
		{
			if (pAttrib->QueryDoubleValue(&dval)==TIXML_SUCCESS)
				return (float)dval;
		}
		pAttrib=pAttrib->Next();
	}
	return 0.0f;
}

static Polygon2* GetDoor (TiXmlNode *pNode)
{
	Polygon2 *pol = new Polygon2 ();

	float width = GetFloatFromAttribute (pNode, (char*)"width"); 
	float height = GetFloatFromAttribute (pNode, (char*)"height"); 
	unsigned int nPoints = 4;
	float *pPoints = (float*)malloc(2*nPoints*sizeof(float));
	pPoints[0] = 0.; pPoints[1] = 0.;
	pPoints[2] = width; pPoints[3] = 0.;
	pPoints[4] = width; pPoints[5] = height;
	pPoints[6] = 0.; pPoints[7] = height;
	pol->add_contour (0, 4, pPoints);

	return pol;
}

static Polygon2* GetWindow (TiXmlNode *pNode)
{
	Polygon2 *pol = new Polygon2 ();

	float width = GetFloatFromAttribute (pNode, (char*)"width"); 
	float subheight = GetFloatFromAttribute (pNode, (char*)"subheight"); 
	float height = GetFloatFromAttribute (pNode, (char*)"height"); 
	unsigned int nPoints = 4;
	float *pPoints = (float*)malloc(2*nPoints*sizeof(float));
	pPoints[0] = 0.; pPoints[1] = subheight;
	pPoints[2] = width; pPoints[3] = subheight;
	pPoints[4] = width; pPoints[5] = subheight+height;
	pPoints[6] = 0.; pPoints[7] = subheight+height;
	pol->add_contour (0, 4, pPoints);

	return pol;
}

static Polygon2* GetFloor (TiXmlNode *pNode)
{
	Polygon2 *floor = new Polygon2 ();
	// int iContour=0;
	float currentWidth = 0.;
	for (TiXmlNode *pChild = pNode->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
	{
		if (strcmp (pChild->Value(), "space") == 0)
		{
			float width = GetFloatFromAttribute (pChild, (char*)"width"); 
			currentWidth += width;
		}
		if (strcmp (pChild->Value(), "window") == 0)
		{
			float width = GetFloatFromAttribute (pChild, (char*)"width"); 
			Polygon2 *window = GetWindow (pChild);
			window->translate (currentWidth, 0.);
			floor->add_polygon2d (window);
			// iContour++;
			currentWidth += width;
		}
		else if (strcmp (pChild->Value(), "door") == 0)
		{
			float width = GetFloatFromAttribute (pChild, (char*)"width"); 
			Polygon2 *door = GetDoor (pChild);
			door->translate (currentWidth, 0.);
			floor->add_polygon2d (door);
			// iContour++;
			currentWidth += width;
		}
	}
	return floor;
}

void ReadLibrary (TiXmlNode *pNode)
{
	for (TiXmlNode *pChild = pNode->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
	{
		if (strcmp (pChild->Value(), "floor") == 0)
		{
			printf ("found a floor in the library\n");
			const char* id = GetStringFromAttribute (pChild, (char*)"id");
			Polygon2 *floor = GetFloor (pChild);
			library.insert (std::pair<std::string,Polygon2*>(id,floor));
		}
		else if (strcmp (pChild->Value(), "window") == 0)
		{
			printf ("found a window in the library\n");
			const char* id = GetStringFromAttribute (pChild, (char*)"id");
			Polygon2 *window = GetWindow (pChild);
			library.insert (std::pair<std::string,Polygon2*>(id,window));
		}
		else if (strcmp (pChild->Value(), "door") == 0)
		{
			printf ("found a door in the library\n");
		}
	}
	printf ("%zu elements read\n", library.size());
}

void GenGeo ( TiXmlNode* pParent, unsigned int indent = 0 )
{
	if ( !pParent ) return;

	TiXmlNode *pChild;
	TiXmlText* pText;
	int t = pParent->Type();
	printf( "%s", getIndent(indent));
	int num;

	switch ( t )
	{
	case TiXmlNode::DOCUMENT:
		printf( "Document" );
		break;

	case TiXmlNode::ELEMENT:
		printf( "Element [%s]", pParent->Value() );
		num=dump_attribs_to_stdout(pParent->ToElement(), indent+1);
		switch(num)
		{
			case 0:  printf( " (No attributes)"); break;
			case 1:  printf( "%s1 attribute", getIndentAlt(indent)); break;
			default: printf( "%s%d attributes", getIndentAlt(indent), num); break;
		}
		if (strcmp (pParent->Value(), "library") == 0)
		{
			ReadLibrary (pParent);
		}
		else if (strcmp (pParent->Value(), "facade") == 0)
		{
			// parse the attributes
			float width = GetFloatFromAttribute (pParent, (char*)"width"); 
			float height = GetFloatFromAttribute (pParent, (char*)"height"); 
			printf ("width = %f\nheight = %f\n", width, height);

			pFacade = new Polygon2 ();
			float *pts = (float*)malloc(8*sizeof(float));
			pts[0] = 0.;
			pts[1] = 0.;

			pts[2] = width;
			pts[3] = 0.;

			pts[4] = width;
			pts[5] = height;

			pts[6] = 0.;
			pts[7] = height;
			pFacade->add_contour (0, 4, pts);
			free (pts);

			// int ifloor = 0;
			float FloorHeight = 3.;
			float currentHeight = 0.;
			for (TiXmlNode *pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
			{
				if (strcmp (pChild->Value(), "floor") == 0)
				{
					Polygon2 *floor = NULL;
					const char *id = GetStringFromAttribute (pChild, (char*)"id");
					if (id)
					{
						Polygon2 *floor_from_library = (Polygon2*)library[id];
						floor = new Polygon2 (*floor_from_library);
					}
					else
					{
						//float FloorHeight = GetFloatFromAttribute (pChild->ToElement(), (char*)"height");
						floor = GetFloor (pChild);
					}
					floor->translate (0., currentHeight);
					currentHeight += FloorHeight;
					floor->inverse_order ();
					pFacade->add_polygon2d (floor);
					//delete floor;
					// ifloor++;
				}
			}
		}
		return;
		break;

	case TiXmlNode::COMMENT:
		printf( "Comment: [%s]", pParent->Value());
		break;

	case TiXmlNode::UNKNOWN:
		printf( "Unknown" );
		break;

	case TiXmlNode::TEXT:
		pText = pParent->ToText();
		printf( "Text: [%s]", pText->Value() );
		break;

	case TiXmlNode::DECLARATION:
		printf( "Declaration" );
		break;
	default:
		break;
	}
	printf ("\n");

	for ( pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling()) 
	{
		GenGeo ( pChild, indent+1 );
	}
}

int main_xml (const char *filename)
{
	printf ("TinyXml v%d.%d.%d\n", TIXML_MAJOR_VERSION, TIXML_MINOR_VERSION, TIXML_PATCH_VERSION);

	TiXmlDocument doc (filename);
	bool bOk = doc.LoadFile ();
	if (!bOk)
	{
		printf ("could not load the xml file : %s\n", filename);
		return -1;
	}

	GenGeo (&doc);	

	return 0;
}
#endif

TEST(TEST_cgmesh_architecture, block)
{
	Mesh *block = CreateBlock ();
	EXPECT_NE(block, nullptr);
	EXPECT_GT(block->m_nVertices, 0);
	// block->save ((char*)"block.obj");
	delete block;
}

TEST(TEST_cgmesh_architecture, wall)
{
	float width = 1.61803399;
	float depth = 1.;
	float height = 1.;
	float bevel = 0.05;
	
	Mesh *wall = new Mesh ();
	for (unsigned int i=0; i<10; i++)
	{
		for (unsigned int j=0; j<5; j++)
		{
			Mesh *b = CreateBlock (width, height, depth, bevel);
			b->translate (j*width, 0., i*height);
			if (i%2)
				b->translate (width/2., 0., 0.);
			wall->Append (b);
		}
	}
	EXPECT_GT(wall->m_nVertices, 0);
	wall->save ((char*)"architecture_wall.obj");
	delete wall;
}

TEST(TEST_cgmesh_architecture, arch)
{
	Mesh *arch = CreateArch ();
	EXPECT_NE(arch, nullptr);
	EXPECT_GT(arch->m_nVertices, 0);
	arch->save ((char*)"architecture_arch.obj");
	delete arch;

	Mesh *arch2 = CreateArch2 ();
	EXPECT_NE(arch2, nullptr);
	EXPECT_GT(arch2->m_nVertices, 0);
	arch2->save ((char*)"architecture_arch2.obj");
	delete arch2;
}

TEST(TEST_cgmesh_architecture, rosace)
{
	Rosace *r = new Rosace ();
	r->SetNFoils (6);
	Polygon2 *pol_rosace = r->Generate ();
	EXPECT_NE(pol_rosace, nullptr);
	
	polygon2_tesselation_export (pol_rosace, "architecture_rosace.obj");

	delete pol_rosace;
	delete r;
}

TEST(TEST_cgmesh_architecture, arc_brise)
{
	ArcBrise *pArcBrise = new ArcBrise ();
	pArcBrise->SetPrincipalArc (0., 660., 660.);
	pArcBrise->SetSecondArc (0., 200., 200.);
	Polygon2 *pol = pArcBrise->Generate ();
	EXPECT_NE(pol, nullptr);
	delete pol;
	delete pArcBrise;
}

TEST(TEST_cgmesh_architecture, column)
{
	float vmin[3], vmax[3];

	Mesh *base = new Mesh ();
	// base->load ((char*)"./data/architecture/colonne/ordre_corinthien/base1.obj");
	base->computebbox ();
	const auto & bbox = base->bbox ();
	base->translate (-(bbox.GetMinX()+bbox.GetMaxX())/2., -(bbox.GetMinY()+bbox.GetMaxY())/2., -bbox.GetMinZ());
	float hbase = bbox.GetMaxZ() - bbox.GetMinZ();
	Mesh *colonne = new Mesh ();

	// colonne->load ((char*)"./data/architecture/colonne/colonne1.obj");
	colonne->computebbox ();
	const auto & bbox_colonne = colonne->bbox ();
	colonne->translate (-(bbox_colonne.GetMinX()+bbox_colonne.GetMaxX())/2., -(bbox_colonne.GetMinY()+bbox_colonne.GetMaxY())/2., -bbox_colonne.GetMinZ() + hbase);
	float hcolonne = bbox_colonne.GetMaxZ() - bbox_colonne.GetMinZ();

	Mesh *chapiteau = new Mesh ();
	// chapiteau->load ((char*)"./data/architecture/colonne/ordre_corinthien/chapiteau.obj");
	chapiteau->computebbox ();
	const auto & bbox_chapiteau = chapiteau->bbox ();
	chapiteau->translate (-(bbox_chapiteau.GetMinX()+bbox_chapiteau.GetMaxX())/2., -(bbox_chapiteau.GetMinY()+bbox_chapiteau.GetMaxY())/2., -bbox_chapiteau.GetMinZ() + hbase+hcolonne);

	Mesh *col = new Mesh ();
	col->Append (base);
	col->Append (colonne);
	col->Append (chapiteau);

	col->save ((char*)"architecture_corinthien.obj");

	delete col;
	delete base;
	delete colonne;
	delete chapiteau;
}

// Note: Test with XML parsing is commented out because tinyXML is not found in the project
#ifdef USE_TINYXML
TEST(TEST_cgmesh_architecture, xml_facade)
{
	// This test requires a valid XML file
	// const char *filename = "facade.xml";
	// main_xml (filename);
}
#endif
