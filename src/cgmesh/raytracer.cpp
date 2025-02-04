#include <stdlib.h>
#include <stdio.h>

#include "../cgmesh/cgmesh.h"
#include "raytracer.h"

Raytracer::Raytracer ()
{
	m_pScene = new Scene ();
	m_pCamera = new Camera ();
	m_pLights[0] = new Light ();
	m_nLights = 1;

	m_time_GetColorWithRay = 0.;
	m_time_GetIntersectionWithRay = 0.;
}

Raytracer::~Raytracer ()
{
	for (unsigned int i=0; i<m_nLights; i++)
		delete m_pLights[i];
	if (m_pCamera)
		delete m_pCamera;
	if (m_pScene)
		delete m_pScene;
}

unsigned int Raytracer::AddLight (Light* pLight)
{
	m_pLights[m_nLights] = pLight;
	unsigned int idLight = m_nLights++;
	return idLight;
}

//
// Phong shading
// http://en.wikipedia.org/wiki/Phong_shading
//
int Raytracer::GetColorWithRay (vec3 vOrig, vec3 vDirection, float color[3])
{
	vec3 vIntersection, vN;
	Ticker t;
	color[0] = 0.;
	color[1] = 0.;
	color[2] = 0.;

	Geometry *pObject = GetScene()->GetIntersectionWithRay (vOrig, vDirection, vIntersection, vN);
	m_time_GetIntersectionWithRay += t.stop ();
	if (!pObject)
		return 0;

	// todo : check what type of material it is
	MaterialColorExt* pMaterial = (MaterialColorExt*)pObject->GetMaterial ();

	if (pMaterial == nullptr)
	{
		// default material
		pMaterial = new MaterialColorExt();
		pMaterial->SetAmbient(0.7, 0.1, 0.3, 1.f);
		pMaterial->SetDiffuse(0.7, 0.3, 0.2, 1.f);
		pMaterial->SetSpecular(0.5, 0.1, 0.1, 1.f);

		// TODO : fix memory leak...
	}

	//MaterialColorExt *pMaterial = new MaterialColorExt ();//pObject->m_pMaterial;
	//pMaterial->Init_From_Library (MaterialColorExt::EMERALD);
	float Ia, Id, Is;

	vec3_normalize (vN);

	for (unsigned int i=0; i<m_nLights; i++)
	{
		for (unsigned int j=0; j<RAYTRACER_SHADOWS_NUMBER_RAYS_PER_PIXEL; j++)
		{
			float vLight[3];
			vLight[0] = m_pLights[i]->Position[0];
			vLight[1] = m_pLights[i]->Position[1];
			vLight[2] = m_pLights[i]->Position[2];
			if (RAYTRACER_SHADOWS_NUMBER_RAYS_PER_PIXEL > 1)
			{
				vLight[0] += 0.5*RAYTRACER_SHADOWS_LIGHT_POSITION_OFFSET*((float)rand()/(RAND_MAX+1.0));
				vLight[1] += 0.5*RAYTRACER_SHADOWS_LIGHT_POSITION_OFFSET*((float)rand()/(RAND_MAX+1.0));
				vLight[2] += 0.5*RAYTRACER_SHADOWS_LIGHT_POSITION_OFFSET*((float)rand()/(RAND_MAX+1.0));
			}

			vec3 vL; // intersection -> light
			vL[0] = vLight[0] - vIntersection[0];
			vL[1] = vLight[1] - vIntersection[1];
			vL[2] = vLight[2] - vIntersection[2];
			float fL2 = vec3_dot_product (vL, vL);
			vec3_normalize (vL);
				
			vec3 vR;
			float fDotNL = vec3_dot_product (vN, vL);
			if (fDotNL < 0.)
				fDotNL = 0.;
				
			vR[0] = 2.*fDotNL*vN[0]-vL[0];
			vR[1] = 2.*fDotNL*vN[1]-vL[1];
			vR[2] = 2.*fDotNL*vN[2]-vL[2];
			vec3_normalize (vR);
				
			vec3 vV;
			vV[0] = -vDirection[0];
			vV[1] = -vDirection[1];
			vV[2] = -vDirection[2];
			vec3_normalize (vV);
				
			// todo : store the following parameters in the material
			float m_fDiffuseReflectionConstant = 0.5;
			float m_fAmbientReflectionConstant = 0.3;
			float m_fSpecularReflectionConstant = 0.5;
			float m_fShininess = 10.;
			//

			float fDotRV = vec3_dot_product (vR, vV);
			float fDotRValpha = (fDotRV < 0.)? 0. : pow ((float)(fDotRV), (float)(m_fShininess));
			Ia = m_fAmbientReflectionConstant;
			Id = m_fDiffuseReflectionConstant * fDotNL;
			Is = m_fSpecularReflectionConstant * fDotRValpha;
				
			float fIntensity = Ia + Id + Is;
				
			float vIntersectionOffset[3];
			float fOffset = 0.0001;
			vIntersectionOffset[0] = vIntersection[0] + fOffset*vL[0];
			vIntersectionOffset[1] = vIntersection[1] + fOffset*vL[1];
			vIntersectionOffset[2] = vIntersection[2] + fOffset*vL[2];

			color[0] += Ia*pMaterial->m_fAmbient[0] + Id*pMaterial->m_fDiffuse[0] + Is*pMaterial->m_fSpecular[0];
			color[1] += Ia*pMaterial->m_fAmbient[1] + Id*pMaterial->m_fDiffuse[1] + Is*pMaterial->m_fSpecular[1];
			color[2] += Ia*pMaterial->m_fAmbient[2] + Id*pMaterial->m_fDiffuse[2] + Is*pMaterial->m_fSpecular[2];
				
			float vIntersection2[3], vNormal2[3];
			//Geometry *pObject2 = GetScene()->GetIntersectionWithSegment (vIntersectionOffset, m_pLights[i]->m_vPosition,
			//							     vIntersection2, vNormal2);
			t.start ();
			Geometry *pObject2 = NULL;//GetScene()->GetIntersectionWithRay (vIntersectionOffset, vL, vIntersection2, vNormal2);
			m_time_GetIntersectionWithRay += t.stop ();
			if (pObject2)
			{
				vec3 vTmp;
				vTmp[0] = vIntersection2[0] - vIntersection[0];
				vTmp[1] = vIntersection2[1] - vIntersection[1];
				vTmp[2] = vIntersection2[2] - vIntersection[2];
				float l2 = vec3_dot_product (vTmp, vTmp);
				if (pObject2 && l2 > fL2)
				{
					//float fIntensity = dot_product (vNormal, vL);
					color[0] += /*Ia*pMaterial->m_fAmbient[0] + */Id*pMaterial->m_fDiffuse[0] + Is*pMaterial->m_fSpecular[0];
					color[1] += /*Ia*pMaterial->m_fAmbient[1] + */Id*pMaterial->m_fDiffuse[1] + Is*pMaterial->m_fSpecular[1];
					color[2] += /*Ia*pMaterial->m_fAmbient[2] + */Id*pMaterial->m_fDiffuse[2] + Is*pMaterial->m_fSpecular[2];
				}
			}
		}

		color[0] /= RAYTRACER_SHADOWS_NUMBER_RAYS_PER_PIXEL;
		color[1] /= RAYTRACER_SHADOWS_NUMBER_RAYS_PER_PIXEL;
		color[2] /= RAYTRACER_SHADOWS_NUMBER_RAYS_PER_PIXEL;

		if (color[0] > 1.) color[0] = 1.;
		if (color[1] > 1.) color[1] = 1.;
		if (color[2] > 1.) color[2] = 1.;
	}

	return 1;
}

Img* Raytracer::Trace (unsigned int iWidth, unsigned int iHeight)
{
	Ticker t;

	Img *pImg = new Img (iWidth, iHeight, false);

	//
	if (0) // orthographic
	{
		float width = 20.;
		float height = 20.;
		for (unsigned int j=0; j<iHeight; j++)
		{
			for (unsigned int i=0; i<iWidth; i++)
			{
				vec3 vD;
				vD[0] = 0.;
				vD[1] = 0.4;
				vD[2] = -0.1;
				vec3_normalize (vD);
				
				float vEye[3];
				vEye[0] = -width/2. + i*width/iWidth;
				vEye[1] = 0.;
				vEye[2] = height/2. - j*height/iHeight;
				
				// transform vD in local frame
				vEye[1] = -10.;
				vEye[2] += height/2.;

				// is there an interection between the ray and the geometry
				float vIntersection[3], vNormal[3], color[3];
				Ticker t;
				GetColorWithRay (vEye, vD, color);
				m_time_GetColorWithRay += t.stop ();
			}
		}
	}
	else // perspective
	{
		float width_aov = M_PI/8.;
		float height_aov = M_PI/8.;
		for (unsigned int j=0; j<iHeight; j++)
		{
			for (unsigned int i=0; i<iWidth; i++)
			{
				// spherical coordinates
				float fTheta = (M_PI + width_aov)/2. - i*width_aov/iWidth;
				float fPhi = height_aov/2. - j*height_aov/iHeight;
				
				// cartesian coordinates
				vec3 vD;
				vD[0] = cos(fTheta)*cos(fPhi);
				vD[1] = sin(fTheta)*cos(fPhi);
				vD[2] = sin(fPhi);
				vec3_normalize (vD);
				
				float vEye[3];
				vEye[0] = GetCamera()->m_vPosition[0];
				vEye[1] = GetCamera()->m_vPosition[1];
				vEye[2] = GetCamera()->m_vPosition[2];
				
				
				Ticker t;
				float color[3];
				if (!GetColorWithRay (vEye, vD, color))
				{
					// no intersection => set background
					color[0] = 1.;
					color[1] = 1.;
					color[2] = 1.;
				}
				m_time_GetColorWithRay += t.stop();
				pImg->set_pixel (i, j, (int)(color[0]*255.), (int)(color[1]*255.), (int)(color[2]*255.), 255);
			}
		}
	}

	printf ("Total elapsed time : %fs\n", t.stop ()/1000.);

	return pImg;
}

void Raytracer::Dump ()
{
	//printf ("nLights : %d\n", m_nLights);
	printf ("GetColorWithRay        : %f\n", m_time_GetColorWithRay);
	printf ("GetIntersectionWithRay : %f\n", m_time_GetIntersectionWithRay);
}

void Raytracer::export_statistics (char *filename)
{
	FILE *ptr = fopen (filename, "w");
	if (!ptr)
		return;

	fprintf (ptr, "<html>\n");

	fprintf (ptr, "<head><title></title></head>\n");

	fprintf (ptr, "<body>\n");

	fprintf (ptr, "<p>m_time_GetColorWithRay : %.3f sec\n", m_time_GetColorWithRay/1000);
	fprintf (ptr, "<p>m_time_GetIntersectionWithRay : %.3f sec\n", m_time_GetIntersectionWithRay/1000);


	fprintf (ptr, "<p>m_i_GetIntersectionBboxWithRay_count : %d\n", m_pScene->m_i_GetIntersectionBboxWithRay_count);
	fprintf (ptr, "<p>m_i_GetIntersectionWithRay_count : %d\n", m_pScene->m_i_GetIntersectionWithRay_count);


	fprintf (ptr, "</body>\n");
	fprintf (ptr, "</html>\n");

	fclose (ptr);
}


