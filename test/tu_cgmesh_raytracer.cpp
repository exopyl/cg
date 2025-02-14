#include <gtest/gtest.h>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <sys/time.h>
#endif


#include "../src/cgmath/cgmath.h"
#include "../src/cgmesh/cgmesh.h"
#include "../src/cgmesh/raytracer.h"

void CreateScene (Scene *pScene, int id)
{
	switch (id)
	{
	case 0:
	{
		Plane *pPlane = new Plane ();
		/*
		pPlane->m_pMaterial->SetAmbient (0.1, 0.21, 0.7);
		pPlane->m_pMaterial->SetDiffuse (0.1, 0.3, 0.2);
		pPlane->m_pMaterial->SetSpecular (0.3, 0.8, 0.1);
		*/
		//pScene->AddObject (pPlane);

		Sphere *pSphere1 = new Sphere ();
		pSphere1->SetCenter (1., 0., 1.);
		pSphere1->SetRadius (1.);
		/*
		pSphere1->m_pMaterial->SetAmbient (0.7, 0.1, 0.3);
		pSphere1->m_pMaterial->SetDiffuse (0.7, 0.3, 0.2);
		pSphere1->m_pMaterial->SetSpecular (0.5, 0.1, 0.1);
		*/
		pScene->AddObject (pSphere1);
		
		Sphere *pSphere2 = new Sphere ();
		pSphere2->SetCenter (-1., 0., 0.);
		pSphere2->SetRadius (2.);
		pScene->AddObject (pSphere2);
	}
	break;
	case 1:
	{
		Plane *pPlane = new Plane ();
		/*
		pPlane->m_pMaterial->SetAmbient (0.1, 0.21, 0.7);
		pPlane->m_pMaterial->SetDiffuse (0.1, 0.3, 0.2);
		pPlane->m_pMaterial->SetSpecular (0.3, 0.8, 0.1);
		*/
		//pScene->AddObject (pPlane);

		Sphere *pSphere = new Sphere ();
		pSphere->SetCenter (0., 0., 3.);
		pSphere->SetRadius (1.5);
		pScene->AddObject (pSphere);

		Torus *pTorus = new Torus ();
		pTorus->R = 5.;
		pTorus->r = 1.;
		//pTorus->SetCenter (-3., -3., 0.);
		//pTorus->SetRadius (1.);
		pScene->AddObject (pTorus);
	}
	break;
	case 2:
	{
		float fScale = 4.;
		float r=0.3*fScale;
		float z=-r;
		unsigned int n=50;//300;
		for (unsigned int j=0; j<n; j++)
		{
			Sphere *pSphere = new Sphere ();
			float fAngle = j*180./(3.14159*1.5*n)-3.14159/3.;
			float rtmp = fScale*r*(sin(acos(z/r)));
			pSphere->SetCenter (-rtmp*cos(fAngle), rtmp*sin(fAngle), z*fScale);
			pSphere->SetRadius (0.13*fScale);
			pScene->AddObject (pSphere);
			z += 2.*r/n;
		}
	}
	break;
	case 3:
	{
		//Plane *pPlane = new Plane ();
		//pScene->AddObject (pPlane);

		Mesh *pMesh = new Mesh ();
		pMesh->load ("./test/data/BunnyLowPoly.stl");
		pMesh->FlipFaces();
		mat3 m;
		mat3_init_rotation_from_euler_angles (m, 0., 0., M_PI/2.);
		pMesh->transform (m);
		pMesh->centerize ();
		pMesh->scale (1./5.);
		pMesh->computebbox();
		pScene->AddObject (pMesh);
	}
	break;
	default:
		break;
	}
}

static std::shared_ptr< Raytracer > InitRaytracer(unsigned int idScene)
{
	auto pRaytracer = std::make_shared< Raytracer >();

	// camera
	Camera* pCamera = pRaytracer->GetCamera();
	pCamera->SetPosition(0., -30., 0.);
	pCamera->SetDirection(0., 1., 0.);
	pCamera->SetUp(0., 0., 1.);

	// lights
	Light* pLight1 = pRaytracer->GetLight(0);
	pLight1->SetPosition(-30., -30., 2.);
	//pRaytracer->AddLight (pLight1);

	Light* pLight2 = new Light();
	pLight2->SetPosition(30., 0., 0.);
	//pRaytracer->AddLight (pLight2);

	// scene
	Scene* pScene = pRaytracer->GetScene();
	CreateScene(pScene, idScene);

	return pRaytracer;
}

TEST(TEST_cgmath_raytracer, scene0)
{
	// Context
	auto pRaytracer = InitRaytracer(0);

	// Action
	Img* pImg = pRaytracer->Trace(128, 128);
	pImg->save("raytracer0.bmp");

	// Expectations
	EXPECT_EQ(pImg->width(), 128);
	EXPECT_EQ(pImg->height(), 128);

	// cleaning
	delete pImg;

	return;

	// export stats
	pRaytracer->Dump();
	pRaytracer->export_statistics("raytracer_stats_scene0.html");
}

TEST(TEST_cgmath_raytracer, scene1)
{
	// Context
	auto pRaytracer = InitRaytracer(1);

	// Action
	Img* pImg = pRaytracer->Trace(128, 128);
	pImg->save("raytracer1.bmp");

	// Expectations
	EXPECT_EQ(pImg->width(), 128);
	EXPECT_EQ(pImg->height(), 128);

	// cleaning
	delete pImg;

	return;

	// export stats
	pRaytracer->Dump();
	pRaytracer->export_statistics("raytracer_stats_scene0.html");
}

TEST(TEST_cgmath_raytracer, scene2)
{
	// Context
	auto pRaytracer = InitRaytracer(2);

	// Action
	Img* pImg = pRaytracer->Trace(128, 128);
	pImg->save("raytracer2.bmp");

	// Expectations
	EXPECT_EQ(pImg->width(), 128);
	EXPECT_EQ(pImg->height(), 128);

	// cleaning
	delete pImg;

	return;

	// export stats
	pRaytracer->Dump();
	pRaytracer->export_statistics("raytracer_stats_scene0.html");
}

TEST(TEST_cgmath_raytracer, scene3)
{
	// Context
	auto pRaytracer = InitRaytracer(3);

	// Action
	Img* pImg = pRaytracer->Trace(128, 128);
	pImg->save("raytracer3.bmp");

	// Expectations
	EXPECT_EQ(pImg->width(), 128);
	EXPECT_EQ(pImg->height(), 128);

	// cleaning
	delete pImg;

	return;

	// export stats
	pRaytracer->Dump();
	pRaytracer->export_statistics("raytracer_stats_scene0.html");
}
