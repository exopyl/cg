#include <stdio.h>
#include <math.h>

#include "light.h"

Light::Light()
{
	SetLightType (CG_LIGHT_TYPE_POINT);
	SetAmbientValue(0.5f, 0.5f, 0.5f, 1.0f);
	SetDiffuseValue (1.0f, 1.0f, 1.0f, 1.0f);
	SetSpecularValue (1.0f, 1.0f, 1.0f, 1.0f);
	//SetPosition (2.0f, 3.0f, 5.0f);
	//SetDirection (-2.0f, -3.0f, -5.0f);
	SetPosition (0.0f, 0.0f, 5.0f);
	SetDirection (0.0f, 0.0f, -1.0f);
	SetRange (180.0f);
	SetConstantAttenuation (1.0f);
	SetLinearAttenuation (0.0f);
	SetQuadraticAttenuation (0.0f);

	bEnable = false;
	bDisplay = false;
}

Light::~Light()
{
}

void Light::SetLightType(CG_LIGHT_TYPE eType)
{
	Type = eType;
}

void Light::SetDiffuseValue( float r, float g, float b, float a)
{
	Diffuse[0] = r;
	Diffuse[1] = g;
	Diffuse[2] = b;
	Diffuse[3] = a;
}

void Light::SetSpecularValue( float r, float g, float b, float a)
{
	Specular[0] = r;
	Specular[1] = g;
	Specular[2] = b;
	Specular[3] = a;
}

void Light::SetAmbientValue(float r, float g, float b, float a)
{
	Ambient[0] = r;
	Ambient[1] = g;
	Ambient[2] = b;
	Ambient[3] = a;
}

void Light::SetPosition(float x, float y, float z)
{
	Position[0] = x;
	Position[1] = y;
	Position[2] = z;
}

void Light::SetDirection(float x, float y, float z)
{
	float fInvLength = 1.f / sqrt(x*x + y*y + z*z);
	Direction[0] = x * fInvLength;
	Direction[1] = y * fInvLength;
	Direction[2] = z * fInvLength;
}

void Light::SetRange(float fValue)
{
	Range = fValue;
}

void Light::SetFalloff(float fValue)
{
	Falloff = fValue;
}

void Light::SetConstantAttenuation(float fValue)
{
	Attenuation0 = fValue;
}

void Light::SetLinearAttenuation(float fValue)
{
	Attenuation1 = fValue;
}

void Light::SetQuadraticAttenuation(float fValue)
{
	Attenuation2 = fValue;
}

void Light::SetInnerAngle(float fValue)
{
	Theta = fValue;
}

void Light::SetOuterAngle(float fValue)
{
	Phi = fValue;
}

void Light::SetEnable (bool status)
{
	bEnable = status;
}

bool Light::GetEnable (void)
{
	return bEnable;
}

void Light::SetDisplay (bool status)
{
	bDisplay = status;
}

bool Light::GetDisplay (void)
{
	return bDisplay;
}

void Light::Dump(void)
{
	switch (Type)
	{
	case 0:// POINT
		printf ("light point\n");
		break;
	case 1://SPOT:
		printf ("light spot\n");
		break;
	case 2://DIRECTIONAL:
		printf ("light directional\n");
		break;
	default:
		printf ("unknown type of light\n");
		break;
	}
	printf (" ambient : %f %f %f %f\n", Ambient[0], Ambient[1], Ambient[2], Ambient[3]);
	printf (" diffuse : %f %f  %f %f\n", Diffuse[0], Diffuse[1], Diffuse[2], Diffuse[3]);
	printf (" specular : %f %f %f %f\n", Specular[0], Specular[1], Specular[2], Specular[3]);
	printf (" position : %f %f %f\n", Position[0], Position[1], Position[2]);
}
