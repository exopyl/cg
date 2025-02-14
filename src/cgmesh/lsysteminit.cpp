#include "lsysteminit.h"

#include <stdio.h>

static void LoadFile (char *strFilename, LSystem *pLSystem)
{
	if (strFilename == NULL)
		return;

	FILE *pIn = fopen (strFilename, "r");
	if (pIn == NULL)
		return;

	char pBuffer[256];

	// Init
	fscanf (pIn, "%256s\n", &pBuffer);
	pLSystem->Init (pBuffer);

	// Angle
	float fAngle = 0.;
	fscanf (pIn, "%f\n", &fAngle);
	pLSystem->SetAngle (fAngle*3.14159/180.0);

	// Number of iterations
	unsigned int nIterations = 0;
	fscanf (pIn, "%d\n", &nIterations);

	// Rules
	do
	{
		fscanf (pIn, "%256s\n", &pBuffer);
		pBuffer[1]='\0';
		pLSystem->AddRule (pBuffer, pBuffer+2);
	} while (!feof (pIn));

	fclose (pIn);
}

//
void InitLSystems (std::map<int,LSystemData*>& mapLSystems)
{
	LSystem *t = NULL;
	LSystemData *pLSystemData = NULL;

	//t = new LSystem();
	//pLSystemData = new LSystemData;
	//LoadFile ("./test/data/lsystem_koch_curve.txt", t);
	//mapLSystems[LSYSTEM_KOCH_CURVE] = pLSystemData;
	//return;

	// courbe de Koch (LSYSTEM_KOCH_CURVE)
	t = new LSystem ();
	t->SetName ("Koch Curve");
	t->Init ("F+F+F+F");
	t->SetAngle (90*3.14159/180.0);
	t->AddRule ("F", "F+F-F-FF+F+F-F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = true;
	pLSystemData->nNberIterations = 4;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_KOCH_CURVE] = pLSystemData;
	return;
	/*
	// quadratic Koch island a (LSYSTEM_QUADRATIC_KOCH_ISLAND_A)
	t = new LSystem ();
	t->SetName ("Quadratic Koch island A");
	t->Init ("F+F+F+F");
	t->SetAngle (90*3.14159/180.0);
	t->AddRule ("F", "F+F-F-FFF+F+F-F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = true;
	pLSystemData->nNberIterations = 5;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_QUADRATIC_KOCH_ISLAND_A] = pLSystemData;

	// quadratic Koch island b (LSYSTEM_QUADRATIC_KOCH_ISLAND_B)
	t = new LSystem ();
	t->SetName ("Quadratic Koch island B");
	t->Init ("F+F+F+F");
	t->SetAngle (90*3.14159/180.0);
	t->AddRule ("F", "F-FF+FF+F+F-F-FF+F+F-F-FF-FF+F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = true;
	pLSystemData->nNberIterations = 4;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_QUADRATIC_KOCH_ISLAND_B] = pLSystemData;

	// square Sierpinski (LSYSTEM_SQUARE_SIERPINSKI)
	t = new LSystem ();
	t->SetName ("Square Sierpinski");
	t->Init ("F+XF+F+XF");
	t->SetAngle (90*3.14159/180.0);
	t->AddRule ("X", "XF-F+F-XF+F+XF-F+F-X");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = true;
	pLSystemData->nNberIterations = 7;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_SQUARE_SIERPINSKI] = pLSystemData;

	// Van Koch Snowflake (LSYSTEM_VAN_KOCH_SNOWFLAKE)
	t = new LSystem ();
	t->SetName ("Van Koch Snowflake");
	t->Init ("F++F++F");
	t->SetAngle (60.*3.14159/180.0);
	t->AddRule ("F", "F-F++F-F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = true;
	pLSystemData->nNberIterations = 7;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_VAN_KOCH_SNOWFLAKE] = pLSystemData;
*/



/*
	// Dragon curve (LSYSTEM_DRAGON_CURVE)
	t = new LSystem ();
	t->SetName ("Dragon curve");
	t->Init ("FX");
	t->SetAngle (90.*3.14159/180.0);
	t->AddRule ("X", "X+YF+");
	t->AddRule ("Y", "-FX-Y");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 17;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_DRAGON_CURVE] = pLSystemData;

	// Hexagonal Gosper (LSYSTEM_HEXAGONAL_GOSPER)
	t = new LSystem ();
	t->SetName ("Hexagonal Gosper");
	t->Init ("XF");
	t->SetAngle (60.*3.14159/180.0);
	t->AddRule ("X", "X+YF++YF-FX--FXFX-YF+");
	t->AddRule ("Y", "-FX+YFYF++YF+FX--FX-Y");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 6;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_DRAGON_CURVE] = pLSystemData;

	// Peano curve (LSYSTEM_PEANO_CURVE)
	t = new LSystem ();
	t->SetName ("Peano curve");
	t->Init ("X");
	t->SetAngle (90.*3.14159/180.0);
	t->AddRule ("X", "XFYFX+F+YFXFY-F-XFYFX");
	t->AddRule ("Y", "YFXFY-F-XFYFX+F+YFXFY");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 5;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_PEANO_CURVE] = pLSystemData;

	// Quadratic Gosper (LSYSTEM_QUADRATIC_GOSPER)
	t = new LSystem ();
	t->SetName ("Quadratic Gosper");
	t->Init ("-YF");
	t->SetAngle (90.*3.14159/180.0);
	t->AddRule ("X", "XFX-YF-YF+FX+FX-YF-YFFX+YF+FXFXYF-FX+YF+FXFX+YF-FXYF-YF-FX+FX+YFYF-");
	t->AddRule ("Y", "+FXFX-YF-YF+FX+FXYF+FX-YFYF-FX-YF+FXYFYF-FX-YFFX+FX+YF-YF-FX+FX+YFY");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 4;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_QUADRATIC_GOSPER] = pLSystemData;

	// Hilbert (LSYSTEM_HILBERT)
	t = new LSystem ();
	t->SetName ("Hilbert");
	t->Init ("X");
	t->SetAngle (90.*3.14159/180.0);
	t->AddRule ("X", "-YF+XFX+FY-");
	t->AddRule ("Y", "+XF-YFY-FX+");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 8;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_HILBERT] = pLSystemData;

	// Quadratic Snowflake (LSYSTEM_QUADRATIC_SNOWFLAKE)
	t = new LSystem ();
	t->SetName ("Quadratic Snowflake");
	t->Init ("F");
	t->SetAngle (90.*3.14159/180.0);
	t->AddRule ("F", "F+F-F-F+F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 7;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_QUADRATIC_SNOWFLAKE] = pLSystemData;

	// Triangle (LSYSTEM_SIERPINSKI_ARROWHEAD)
	t = new LSystem ();
	t->SetName ("Sierpinski Arrowhead");
	t->Init ("YF");
	t->SetAngle (60.*3.14159/180.0);
	t->AddRule ("X", "YF+XF+Y");
	t->AddRule ("Y", "XF-YF-X");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 11;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_SIERPINSKI_ARROWHEAD] = pLSystemData;
*/





/*
	// Triangle (LSYSTEM_TRIANGLE)
	t = new LSystem ();
	t->SetName ("Triangle");
	t->Init ("F+F+F");
	t->SetAngle (120.*3.14159/180.0);
	t->AddRule ("F", "F-F+F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 10;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_TRIANGLE] = pLSystemData;
*/




/*
	// Board (LSYSTEM_BOARD)
	t = new LSystem ();
	t->SetName ("Board");
	t->Init ("F+F+F+F");
	t->SetAngle (1.57079632679489661923);
	t->AddRule ("F", "FF+F+F+F+FF");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 5;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_BOARD] = pLSystemData;

	// Cross A (LSYSTEM_CROSS_A)
	t = new LSystem ();
	t->SetName ("Cross A");
	t->Init ("F+F+F+F");
	t->SetAngle (3.14159265358979323846/2.0);
	t->AddRule ("F", "F+FF++F+F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 6;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_CROSS_A] = pLSystemData;

	// Cross B (LSYSTEM_CROSS_B)
	t = new LSystem ();
	t->SetName ("Cross B");
	t->Init ("F+F+F+F");
	t->SetAngle (3.14159265358979323846/2.0);
	t->AddRule ("F", "F+F-F+F+F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 6;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_CROSS_B] = pLSystemData;

	// Rings (LSYSTEM_RINGS)
	t = new LSystem ();
	t->SetName ("Rings");
	t->Init ("F+F+F+F");
	t->SetAngle (3.14159265358979323846/2.0);
	t->AddRule ("F", "FF+F+F+F+F+F-F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 5;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_RINGS] = pLSystemData;
*/

	//
	// branching structures
	//

	// Leaf (LSYSTEM_LEAF)
	t = new LSystem ();
	t->SetName ("Leaf");
	t->Init ("a");
	t->SetAngle (45.*3.14159/180.0);
	t->SetLength (1.36);
	t->AddRule ("F", ">F<");
	t->AddRule ("a", "F[+x]Fb");
	t->AddRule ("b", "F[-y]Fa");
	t->AddRule ("x", "a");
	t->AddRule ("y", "b");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 19;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_LEAF] = pLSystemData;

	// Bush1 (LSYSTEM_BUSH1)
	t = new LSystem ();
	t->SetName ("Bush1");
	t->Init ("Y");
	t->SetAngle (25.7*3.14159/180.0);
	t->AddRule ("F", "X[-FFF][+FFF]FX");
	t->AddRule ("Y", "YFX[+Y][-Y]");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 6;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_BUSH1] = pLSystemData;
	
	// Bush2 (LSYSTEM_BUSH2)
	t = new LSystem ();
	t->SetName ("Bush2");
	t->Init ("F");
	t->SetAngle (22.5*3.14159/180.0);
	t->AddRule ("F", "FF+[+F-F-F]-[-F+F+F]");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 5;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_BUSH2] = pLSystemData;
	
	// Bush3 (LSYSTEM_BUSH3)
	t = new LSystem ();
	t->SetName ("Bush3");
	t->Init ("F");
	t->SetAngle (35*3.14159/180.0);
	t->AddRule ("F", "F[+FF][-FF]F[-F][+F]F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 5;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_BUSH3] = pLSystemData;

	// Bush4 (LSYSTEM_BUSH4)
	t = new LSystem ();
	t->SetName ("Bush4");
	t->Init ("VZFFF");
	t->SetAngle (20*3.14159/180.0);
	t->AddRule ("V", "[+++W][---W]YV");
	t->AddRule ("W", "+X[-W]Z");
	t->AddRule ("X", "-W[+X]Z");
	t->AddRule ("Y", "YZ");
	t->AddRule ("Z", "[-FFF][+FFF]F");
	pLSystemData = new LSystemData;
	pLSystemData->bClosed = false;
	pLSystemData->nNberIterations = 12;
	pLSystemData->pLSystem = t;
	mapLSystems[LSYSTEM_BUSH4] = pLSystemData;

	if (0)
	{
		t->Init ("F");
		t->SetAngle (25.7*3.14159/180.0);
		t->AddRule ("F", "F[+F]F[-F]F");
		for (int j=0; j<4; j++)
			t->Next ();
	}
	if (0)
	{
		t->Init ("F");
		t->SetAngle (20.0*3.14159/180.0);
		t->AddRule ("F", "F[+F]F[-F][F]");
		for (int j=0; j<4; j++)
			t->Next ();
	}
	if (0)
	{
		t->Init ("F");
		t->SetAngle (22.5*3.14159/180.0);
		t->AddRule ("F", "FF-[-F+F+F]+[+F-F-F]");
		for (int j=0; j<4; j++)
			t->Next ();
	}
	if (0)
	{
		t->Init ("X");
		t->SetAngle (20.0*3.14159/180.0);
		t->AddRule ("X", "F[+X]F[-X]+X");
		t->AddRule ("F", "FF");
		for (int j=0; j<4; j++)
			t->Next ();
	}
	if (0)
	{
		t->Init ("X");
		t->SetAngle (25.7*3.14159/180.0);
		t->AddRule ("X", "F[+X][-X]FX");
		t->AddRule ("F", "FF");
		for (int j=0; j<7; j++)
			t->Next ();
	}
	if (0)
	{
		t->Init ("X");
		t->SetAngle (22.5*3.14159/180.0);
		t->AddRule ("X", "F-[[X]+X]+F[+FX]-X");
		t->AddRule ("F", "FF");
		for (int j=0; j<3; j++)
			t->Next ();
	}

	//
	// 3D
	//
	if (0)
	{
		// LSYSTEM_3D
		t = new LSystem ();
		t->SetName ("Hilbert curve 3D");
		t->SetAngle (90.0*3.14159/180.0);
		t->Init ("A");
		t->AddRule ("A", "B-F+CFC+F-D&F^D-F+&&CFC+F+B//");
		t->AddRule ("B", "A&F^CFB^F^D^^-F-D^|F^B|FC^F^A//");
		t->AddRule ("C", "|D^|F^B-F+C^F^A&&FA&F^C+F+B^F^D//");
		t->AddRule ("D", "|CFB-F+B|FA&F^A&&FB-F+B|FC//");
		pLSystemData = new LSystemData;
		pLSystemData->bClosed = false;
		pLSystemData->nNberIterations = 5;
		pLSystemData->pLSystem = t;
		mapLSystems[LSYSTEM_HILBERT_CURVE_3D] = pLSystemData;
		for (int j=0; j<3; j++)
			t->Next ();

		t->ComputeGraphicalInterpretation3D ();
		t->Normalize ();
		//t->Scaling (2.0);
		//return t;
	}

	if (0)
	{
		// LSYSTEM_3D
		t = new LSystem ();
		t->SetName ("Plant1");
		t->SetAngle (25.*3.14159/180.0);
		t->Init ("F");
		t->AddRule ("F", "[+F+F+F+F][-F-F-F-F][^F^F^F^F][&F&F&F&F]");
		pLSystemData = new LSystemData;
		pLSystemData->bClosed = false;
		pLSystemData->nNberIterations = 5;
		pLSystemData->pLSystem = t;
		mapLSystems[LSYSTEM_PLANT1] = pLSystemData;
		for (int j=0; j<1; j++)
			t->Next ();

		t->ComputeGraphicalInterpretation3D ();
		t->Normalize ();
		//t->Scaling (2.0);
		//return t;
	}

	if (0)
	{
		// LSYSTEM_3D
		t = new LSystem ();
		t->SetName ("Plant2");
		t->SetAngle (15.*3.14159/180.0);
		t->Init ("X");
		t->AddRule ("X", "F&[+F+F+F+F][-F-F-F-F][^F^F^F^F][&F&F&F&F]FX");
		pLSystemData = new LSystemData;
		pLSystemData->bClosed = false;
		pLSystemData->nNberIterations = 5;
		pLSystemData->pLSystem = t;
		mapLSystems[LSYSTEM_PLANT2] = pLSystemData;
		for (int j=0; j<4; j++)
			t->Next ();

		t->ComputeGraphicalInterpretation3D ();
		t->Normalize ();
		//t->Scaling (2.0);
		//return t;
	}

	if (0)
	{
		// LSYSTEM_3D
		t = new LSystem ();
		t->SetName ("Plant3");
		t->SetAngle (15.*3.14159/180.0);
		t->Init ("Y");
		t->AddRule ("Y", "YFA[+Y][-Y][^Y][&Y]");
		t->AddRule ("A", "A[^FF][&FF][-FF][+FF]FA");
		pLSystemData = new LSystemData;
		pLSystemData->bClosed = false;
		pLSystemData->nNberIterations = 5;
		pLSystemData->pLSystem = t;
		mapLSystems[LSYSTEM_PLANT3] = pLSystemData;
		for (int j=0; j<3; j++)
			t->Next ();

		t->ComputeGraphicalInterpretation3D ();
		t->Normalize ();
		//t->Scaling (2.0);
		//return t;
	}

	if (0)
	{
		t = new LSystem ();
		t->Init ("A");
		t->SetAngle (22.5*3.14159/180.0);
		t->AddRule ("A", "[&FL!A]/////'[&FL!A]///////'[&FL!A]");
		t->AddRule ("F", "S/////F");
		t->AddRule ("S", "FL");
		pLSystemData = new LSystemData;
		pLSystemData->bClosed = false;
		pLSystemData->nNberIterations = 26;
		pLSystemData->pLSystem = t;
		//mapLSystems[LSYSTEM_3D] = pLSystemData;
		t->AddRule ("L", "['''^^{-f+f+f-|-f+f+f}]");
		for (int j=0; j<5; j++)
			t->Next ();

		t->ComputeGraphicalInterpretation3D ();
		//t->Normalize ();
		t->Scaling (5.0);
		//return t;
	}

/*
		for (int j=0; j<3; j++)
			t->Next ();

	t->ComputeGraphicalInterpretation2D ();
	t->Normalize ();
	t->Scaling (50.0);
	t->Dump ();
	//delete t;
	return t;
	*/
}
