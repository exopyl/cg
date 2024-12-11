#include "rendered_geometry.h"

RenderedPlane::RenderedPlane ()
	:Plane ()
{
	m_pMaterial = NULL;
}

RenderedSphere::RenderedSphere ()
	:Sphere ()
{
	m_pMaterial = NULL;
}

RenderedTorus::RenderedTorus ()
	:Torus ()
{
	m_pMaterial = NULL;
}

RenderedTriangle::RenderedTriangle ()
	:Triangle ()
{
	m_pMaterial = NULL;
}
