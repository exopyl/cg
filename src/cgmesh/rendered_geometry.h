#ifndef __RENDERED_GEOMETRY_H__
#define __RENDERED_GEOMETRY_H__

#include "../cgmath/cgmath.h"
#include "material.h"

class RenderedPlane : public Plane
{
public:
	RenderedPlane ();
	~RenderedPlane ();

	void SetMaterial (Material *pMaterial) { m_pMaterial = pMaterial; };
	virtual void* GetMaterial (void) { return m_pMaterial; };

private:
	Material *m_pMaterial;
};

class RenderedSphere : public Sphere
{
public:
	RenderedSphere ();
	~RenderedSphere ();

	void SetMaterial (Material *pMaterial) { m_pMaterial = pMaterial; };
	virtual void* GetMaterial (void) { return m_pMaterial; };

private:
	Material *m_pMaterial;
};

class RenderedTorus : public Torus
{
public:
	RenderedTorus ();
	~RenderedTorus ();

	void SetMaterial (Material *pMaterial) { m_pMaterial = pMaterial; };
	virtual void* GetMaterial (void) { return m_pMaterial; };

private:
	Material *m_pMaterial;
};

class RenderedTriangle : public Triangle
{
public:
	RenderedTriangle ();
	~RenderedTriangle ();

	void SetMaterial (Material *pMaterial) { m_pMaterial = pMaterial; };
	virtual void* GetMaterial (void) { return m_pMaterial; };

private:
	Material *m_pMaterial;
};

#endif // __RENDERED_GEOMETRY_H__
