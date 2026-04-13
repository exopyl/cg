#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

using namespace std;

TEST(TEST_cgmesh_lsystem, cube)
{
	// action
	std::map<int, LSystemData*> mapLSystem;
	InitLSystems(mapLSystem);

	for (std::map<int, LSystemData*>::const_iterator itLSystem = mapLSystem.begin(); itLSystem != mapLSystem.end(); itLSystem++)
	{
		LSystemData* pLSystemData = (*itLSystem).second;
		LSystem* pLSystem = pLSystemData->pLSystem;

		//LSystem *pLSystem = mapLSystem[LSYSTEM_SQUARE_SIERPINSKI];
		if (pLSystem == NULL)
			continue;

		//for (int j=0; j<pLSystemData->nNberIterations; j++,	pLSystem->Next ())
		int j = 0;
		while (1)
		{
			if (pLSystem->m_iDimension == 3)
				pLSystem->ComputeGraphicalInterpretation3D();
			else
				pLSystem->ComputeGraphicalInterpretation2D();

			if (pLSystem->m_iDimension == 2)
			{
				//float fWidth = 297.;
				//float fHeight = 210.;
				float fWidth = 250.;
				float fHeight = 250.;
				pLSystem->FittIn(fWidth, fHeight, 10);

				WriterSVG* pWriterSVG = new WriterSVG();

				char strFilename[255];
				sprintf(strFilename, (char*)"./data_generated_%s_%02d.svg", pLSystem->GetName(), j);

				pWriterSVG->InitFile(strFilename);
				pWriterSVG->WriteHeader(fWidth, fHeight);

				// frame
				list<point2D> frame;
				point2D pt1, pt2, pt3, pt4;
				pt1.set(0., 0.);
				pt2.set(fWidth, 0.);
				pt3.set(fWidth, fHeight);
				pt4.set(0., fHeight);
				frame.push_back(pt1);
				frame.push_back(pt2);
				frame.push_back(pt3);
				frame.push_back(pt4);
				frame.push_back(pt1);
				pWriterSVG->WriteStyleBegin((char*)"rgb(0,0,0)", 0.2);
				pWriterSVG->WritePath(frame);
				pWriterSVG->WriteStyleEnd();

				// LSystem
				if (pLSystemData->bClosed)
					pWriterSVG->WriteStyleBegin((char*)"rgb(255,0,0)", 1.f, (char*)"#a0a0a0");
				else
					pWriterSVG->WriteStyleBegin((char*)"rgb(255,0,0)", 1.f);
				pWriterSVG->WriteGroupBegin();
				list<point2D> listPoints;
				for (int i = 0; i < pLSystem->m_iNumberPoints; i++)
				{

					point2D ptStart, ptEnd;
					ptStart.x = pLSystem->m_walk[2 * i];
					ptStart.y = pLSystem->m_walk[2 * i + 1];
					ptEnd.x = pLSystem->m_walk[2 * (i + 1)];
					ptEnd.y = pLSystem->m_walk[2 * (i + 1) + 1];
					listPoints.push_back(ptStart);
					//listPoints.push_back (ptEnd);

					if (!pLSystem->m_bDrawable[i])
					{
						pWriterSVG->WritePath(listPoints);
						listPoints.clear();
						continue;
					}
				}
				pWriterSVG->WritePath(listPoints);

				pWriterSVG->WriteGroupEnd();
				pWriterSVG->WriteStyleEnd();
				pWriterSVG->WriteFooter();

				delete pWriterSVG;
			}

			if (++j < pLSystemData->nNberIterations)
			{
				pLSystem->Next();
			}
			else
				break;
		}
	}

}

struct RegressionData {
	size_t length;
	const char* prefix;
};

TEST(TEST_cgmesh_lsystem, regression_strings)
{
	std::map<int, LSystemData*> mapLSystem;
	InitLSystems(mapLSystem);

	std::map<std::string, RegressionData> referenceData;
	referenceData["Koch Curve"] = { 475, "F+F-F-FF+F+F-F+F+F-F-FF+F+F-F-F+F-F-FF+F+F-F-F+F-F-FF+F+F-FF+F-F-FF+F+F-F+F+F-F-FF+F+F-F+F+F-F-FF+F+" };
	referenceData["Quadratic Koch island A"] = { 567, "F+F-F-FFF+F+F-F+F+F-F-FFF+F+F-F-F+F-F-FFF+F+F-F-F+F-F-FFF+F+F-FF+F-F-FFF+F+F-FF+F-F-FFF+F+F-F+F+F-F-" };
	referenceData["Quadratic Koch island B"] = { 2211, "F-FF+FF+F+F-F-FF+F+F-F-FF-FF+F-F-FF+FF+F+F-F-FF+F+F-F-FF-FF+FF-FF+FF+F+F-F-FF+F+F-F-FF-FF+F+F-FF+FF+" };
	referenceData["Square Sierpinski"] = { 199, "F+XF-F+F-XF+F+XF-F+F-XF-F+F-XF-F+F-XF+F+XF-F+F-XF+F+XF-F+F-XF+F+XF-F+F-XF-F+F-XF-F+F-XF+F+XF-F+F-XF+" };
	referenceData["Van Koch Snowflake"] = { 112, "F-F++F-F-F-F++F-F++F-F++F-F-F-F++F-F++F-F++F-F-F-F++F-F++F-F++F-F-F-F++F-F++F-F++F-F-F-F++F-F++F-F++" };
	referenceData["Dragon curve"] = { 14, "FX+YF++-FX-YF+" };
	referenceData["Hexagonal Gosper"] = { 162, "X+YF++YF-FX--FXFX-YF++-FX+YFYF++YF+FX--FX-YF++-FX+YFYF++YF+FX--FX-YF-FX+YF++YF-FX--FXFX-YF+--FX+YF++" };
	referenceData["Peano curve"] = { 201, "XFYFX+F+YFXFY-F-XFYFXFYFXFY-F-XFYFX+F+YFXFYFXFYFX+F+YFXFY-F-XFYFX+F+YFXFY-F-XFYFX+F+YFXFYFXFYFX+F+YF" };
	referenceData["Quadratic Gosper"] = { 1719, "-+FXFX-YF-YF+FX+FX-YF-YFFX+YF+FXFXYF-FX+YF+FXFX+YF-FXYF-YF-FX+FX+YFYF-FXFX-YF-YF+FX+FX-YF-YFFX+YF+FX" };
	referenceData["Hilbert"] = { 51, "-+XF-YFY-FX+F+-YF+XFX+FY-F-YF+XFX+FY-+F+XF-YFY-FX+-" };
	referenceData["Quadratic Snowflake"] = { 49, "F+F-F-F+F+F+F-F-F+F-F+F-F-F+F-F+F-F-F+F+F+F-F-F+F" };
	referenceData["Sierpinski Arrowhead"] = { 26, "YF+XF+YF-XF-YF-XF-YF+XF+YF" };
	referenceData["Triangle"] = { 53, "F-F+F-F-F+F+F-F+F+F-F+F-F-F+F+F-F+F+F-F+F-F-F+F+F-F+F" };
	referenceData["Board"] = { 327, "FF+F+F+F+FFFF+F+F+F+FF+FF+F+F+F+FF+FF+F+F+F+FF+FF+F+F+F+FF+FF+F+F+F+FFFF+F+F+F+FF+FF+F+F+F+FFFF+F+F+" };
	referenceData["Cross A"] = { 199, "F+FF++F+F+F+FF++F+FF+FF++F+F++F+FF++F+F+F+FF++F+F+F+FF++F+F+F+FF++F+FF+FF++F+F++F+FF++F+F+F+FF++F+F+" };
	referenceData["Cross B"] = { 199, "F+F-F+F+F+F+F-F+F+F-F+F-F+F+F+F+F-F+F+F+F+F-F+F+F+F+F-F+F+F+F+F-F+F+F-F+F-F+F+F+F+F-F+F+F+F+F-F+F+F+" };
	referenceData["Rings"] = { 475, "FF+F+F+F+F+F-FFF+F+F+F+F+F-F+FF+F+F+F+F+F-F+FF+F+F+F+F+F-F+FF+F+F+F+F+F-F+FF+F+F+F+F+F-F+FF+F+F+F+F+" };
	referenceData["Leaf"] = { 17, ">F<[+a]>F<F[-y]Fa" };
	referenceData["Bush1"] = { 55, "YFX[+Y][-Y]X[-FFF][+FFF]FXX[+YFX[+Y][-Y]][-YFX[+Y][-Y]]" };
	referenceData["Bush2"] = { 172, "FF+[+F-F-F]-[-F+F+F]FF+[+F-F-F]-[-F+F+F]+[+FF+[+F-F-F]-[-F+F+F]-FF+[+F-F-F]-[-F+F+F]-FF+[+F-F-F]-[-F" };
	referenceData["Bush3"] = { 201, "F[+FF][-FF]F[-F][+F]F[+F[+FF][-FF]F[-F][+F]FF[+FF][-FF]F[-F][+F]F][-F[+FF][-FF]F[-F][+F]FF[+FF][-FF]" };
	referenceData["Bush4"] = { 56, "[++++X[-W]Z][---+X[-W]Z]YZ[+++W][---W]YV[-FFF][+FFF]FFFF" };
	referenceData["Hilbert curve 3D"] = { 271, "A&F^CFB^F^D^^-F-D^|F^B|FC^F^A//-F+|D^|F^B-F+C^F^A&&FA&F^C+F+B^F^D//F|D^|F^B-F+C^F^A&&FA&F^C+F+B^F^D/" };
	referenceData["Plant1"] = { 664, "[+[+F+F+F+F][-F-F-F-F][^F^F^F^F][&F&F&F&F]+[+F+F+F+F][-F-F-F-F][^F^F^F^F][&F&F&F&F]+[+F+F+F+F][-F-F-" };
	referenceData["Plant2"] = { 87, "F&[+F+F+F+F][-F-F-F-F][^F^F^F^F][&F&F&F&F]FF&[+F+F+F+F][-F-F-F-F][^F^F^F^F][&F&F&F&F]FX" };
	referenceData["Plant3"] = { 131, "YFA[+Y][-Y][^Y][&Y]FA[^FF][&FF][-FF][+FF]FA[+YFA[+Y][-Y][^Y][&Y]][-YFA[+Y][-Y][^Y][&Y]][^YFA[+Y][-Y]" };

	for (std::map<int, LSystemData*>::const_iterator itLSystem = mapLSystem.begin(); itLSystem != mapLSystem.end(); itLSystem++)
	{
		LSystemData* pLSystemData = (*itLSystem).second;
		LSystem* pLSystem = pLSystemData->pLSystem;

		if (pLSystem == NULL)
			continue;

		std::string name = pLSystem->GetName();
		if (referenceData.find(name) != referenceData.end())
		{
			if (name == "Koch Curve") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Quadratic Koch island A") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Quadratic Koch island B") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Square Sierpinski") pLSystem->Init((char*)"F+XF+F+XF");
			else if (name == "Van Koch Snowflake") pLSystem->Init((char*)"F++F++F");
			else if (name == "Dragon curve") pLSystem->Init((char*)"FX");
			else if (name == "Hexagonal Gosper") pLSystem->Init((char*)"XF");
			else if (name == "Peano curve") pLSystem->Init((char*)"X");
			else if (name == "Quadratic Gosper") pLSystem->Init((char*)"-YF");
			else if (name == "Hilbert") pLSystem->Init((char*)"X");
			else if (name == "Quadratic Snowflake") pLSystem->Init((char*)"F");
			else if (name == "Sierpinski Arrowhead") pLSystem->Init((char*)"YF");
			else if (name == "Triangle") pLSystem->Init((char*)"F+F+F");
			else if (name == "Board") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Cross A") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Cross B") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Rings") pLSystem->Init((char*)"F+F+F+F");
			else if (name == "Leaf") pLSystem->Init((char*)"a");
			else if (name == "Bush1") pLSystem->Init((char*)"Y");
			else if (name == "Bush2") pLSystem->Init((char*)"F");
			else if (name == "Bush3") pLSystem->Init((char*)"F");
			else if (name == "Bush4") pLSystem->Init((char*)"VZFFF");
			else if (name == "Hilbert curve 3D") pLSystem->Init((char*)"A");
			else if (name == "Plant1") pLSystem->Init((char*)"F");
			else if (name == "Plant2") pLSystem->Init((char*)"X");
			else if (name == "Plant3") pLSystem->Init((char*)"Y");

			pLSystem->Next();
			pLSystem->Next();

			std::string result = pLSystem->m_string;
			EXPECT_EQ(result.length(), referenceData[name].length) << "Regression failed (length) for L-System: " << name;
			EXPECT_EQ(result.substr(0, 100), std::string(referenceData[name].prefix)) << "Regression failed (prefix) for L-System: " << name;
		}
	}
}
