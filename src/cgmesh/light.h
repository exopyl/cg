#ifndef __LIGHT_H__
#define __LIGHT_H__

typedef enum _CG_LIGHT_TYPE {
    CG_LIGHT_TYPE_POINT          = 1,
    CG_LIGHT_TYPE_SPOT           = 2,
    CG_LIGHT_TYPE_DIRECTIONAL    = 3,
} CG_LIGHT_TYPE;

class Light
{
public:
    Light();
    ~Light();

public:
	// Setters
	void SetLightType( CG_LIGHT_TYPE eType );

	void SetDiffuseValue( float r, float g, float b, float a );
	void SetSpecularValue( float r, float g, float b, float a );
	void SetAmbientValue( float r, float g, float b, float a );

	void SetPosition(float x, float y, float z);
	void SetDirection(float x, float y, float z);

	void SetRange(float fValue);
	void SetFalloff(float fValue);
	void SetConstantAttenuation(float fValue);
	void SetLinearAttenuation(float fValue);
	void SetQuadraticAttenuation(float fValue);
	void SetInnerAngle(float fValue);
	void SetOuterAngle(float fValue);

	void SetEnable (bool status);
	bool GetEnable (void);
	void SetDisplay (bool status);
	bool GetDisplay (void);

	// Dump
	void Dump (void);
	void Display (void);

public:
	CG_LIGHT_TYPE Type;		// Type of light source
	float Diffuse[4];		// Diffuse color of light
	float Specular[4];		// Specular color of light
	float Ambient[4];		// Ambient color of light
	float Position[3];		// Position in world space
	float Direction[3];		// Direction in world space
	float Range;			// Cutoff range
	float Falloff;			// Falloff
	float Attenuation0;		// Constant attenuation
	float Attenuation1;		// Linear attenuation
	float Attenuation2;		// Quadratic attenuation
	float Theta;			// Inner angle of spotlight cone
	float Phi;				// Outer angle of spotlight cone

	bool bEnable;
	bool bDisplay;
};

#endif	// __LIGHT_H__
