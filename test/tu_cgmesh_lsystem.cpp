#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"

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
			pLSystem->ComputeGraphicalInterpretation2D();

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

			if (++j < pLSystemData->nNberIterations)
			{
				pLSystem->Next();
			}
			else
				break;
		}
	}

}
