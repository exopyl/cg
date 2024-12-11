#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "surface_implicit_samples.h"

static float fTime = 0.0;
static vec3f sSourcePoint[3];

void update_time (float fNewTime)
{
        float fOffset;
        int iSourceNum;
	
        for(iSourceNum = 0; iSourceNum < 3; iSourceNum++)
        {
                sSourcePoint[iSourceNum].fX = .5;
                sSourcePoint[iSourceNum].fY = .5;
                sSourcePoint[iSourceNum].fZ = .5;
        }

        fTime = fNewTime;
        fOffset = sinf(fTime);
        sSourcePoint[0].fX *= fOffset;
        sSourcePoint[1].fY *= fOffset;
        sSourcePoint[2].fZ *= fOffset;
}

float fSample0(float fX, float fY, float fZ)
{
        return .5*sqrt(fX*fX + fY*fY + fZ*fZ);
}

float fSample1(float fX, float fY, float fZ)
{
	double fResult = 0.0;
        double fDx, fDy, fDz;
//	printf ("fSample1 : %f %f %f\n", sSourcePoint[0].fX, sSourcePoint[1].fY, sSourcePoint[2].fZ);
//	printf ("%x %x %x\n", &sSourcePoint[0].fX, &sSourcePoint[1].fY, &sSourcePoint[2].fZ);
	fDx = fX - sSourcePoint[0].fX;
        fDy = fY - sSourcePoint[0].fY;
        fDz = fZ - sSourcePoint[0].fZ;
        fResult += 0.5/(fDx*fDx + fDy*fDy + fDz*fDz);

        fDx = fX - sSourcePoint[1].fX;
        fDy = fY - sSourcePoint[1].fY;
        fDz = fZ - sSourcePoint[1].fZ;
        fResult += 1.0/(fDx*fDx + fDy*fDy + fDz*fDz);

        fDx = fX - sSourcePoint[2].fX;
        fDy = fY - sSourcePoint[2].fY;
        fDz = fZ - sSourcePoint[2].fZ;
        fResult += 1.5/(fDx*fDx + fDy*fDy + fDz*fDz);

        return fResult;
}

//fSample2 finds the distance of (fX, fY, fZ) from three moving lines
float fSample2(float fX, float fY, float fZ)
{
        double fResult = 0.0;
        double fDx, fDy, fDz;
        fDx = fX - sSourcePoint[0].fX;
        fDy = fY - sSourcePoint[0].fY;
        fResult += 0.5/(fDx*fDx + fDy*fDy);

        fDx = fX - sSourcePoint[1].fX;
        fDz = fZ - sSourcePoint[1].fZ;
        fResult += 0.75/(fDx*fDx + fDz*fDz);

        fDy = fY - sSourcePoint[2].fY;
        fDz = fZ - sSourcePoint[2].fZ;
        fResult += 1.0/(fDy*fDy + fDz*fDz);

        return fResult;
}

//fSample3 defines a height field by plugging the distance from the center into the sin and cos functions
float fSample3(float fX, float fY, float fZ)
{
        float fHeight = 20.0*(fTime + sqrt((0.5-fX)*(0.5-fX) + (0.5-fY)*(0.5-fY)));
        fHeight = 1.5 + 0.1*(sinf(fHeight) + cosf(fHeight));
        double fResult = (fHeight - fZ)*50.0;

        return fResult;
}

static vec3f sSourcePoints[10];
static float sWeightPoints[10];
static int sample4initialized = 0;
float fSample4(float fX, float fY, float fZ)
{
	if (!sample4initialized)
	{
		for (int i=0; i<10; i++)
		{
			sSourcePoints[i].fX = (rand() % 60 ) / 100. -.3;
			sSourcePoints[i].fY = (rand() % 120) / 100. -.6;
			sSourcePoints[i].fZ = 0.;
			sWeightPoints[i]    = (rand() % 10) /190.;
		}

		sample4initialized = 1;
	}

        double fResult = 0.0;
        double fDx, fDy, fDz;
	fResult += .00001/((fX-.3)*(fX-.3));
	fResult += .00001/((fX+.3)*(fX+.3));
	fResult += .00001/((fY-.6)*(fY-.6));
	fResult += .00001/((fY+.6)*(fY+.6));
	for (int i=0; i<10; i++)
	{
		fDx = fX - sSourcePoints[i].fX;
		fDy = fY - sSourcePoints[i].fY;
		fDz = fZ - sSourcePoints[i].fZ;
		fResult += sWeightPoints[i]/(fDx*fDx + fDy*fDy + fDz*fDz);
	}

        return fResult;
}

float fSample5(float fX, float fY, float fZ)
{
        double fResult = 0.;
        double fDx = 0., fDy = 0., fDz = 0.;
	for (int i=-3; i<2; i+=2)
		for (int j=-3; j<4; j+=2)
			for (int k=-3; k<4; k+=2)
			{
				fDx = fX - i*(.25);
				fDy = fY - j*(.25);
				fDz = fZ - k*(.25);
				fResult += 0.5/(fDx*fDx + fDy*fDy + fDz*fDz);
			}

        return fResult;
}

float fSample6(float fX, float fY, float fZ)
{
	int nsp = 8;
	float r = 1.;
	vec3f sp[8] = {
		{r, 0., 0.},
		{r*cos(45.*3.14159/180.), r*sin(45.*3.14159/180.), 0.},
		{0., r, 0.},
		{r*cos(135.*3.14159/180.), r*sin(135.*3.14159/180.), 0.},
		{-r, 0., 0.},
		{r*cos(225.*3.14159/180.), r*sin(225.*3.14159/180.), 0.},
		{0., -r, 0.},
		{r*cos(315.*3.14159/180.), r*sin(315.*3.14159/180.), 0.}
	};
	

        double fResult = 0.;
        double fDx = 0., fDy = 0., fDz = 0.;
	for (int i=0; i<nsp; i++)
	{
		fDx = fX - sp[i].fX;
		fDy = fY - sp[i].fY;
		fDz = fZ - sp[i].fZ;
		fResult += .3/sqrtf(fDx*fDx + fDy*fDy + fDz*fDz);
	}

        return fResult;
}

float fSample7(float fX, float fY, float fZ)
{
	return exp (fZ) * cos (fX) - cos(fY);
}
