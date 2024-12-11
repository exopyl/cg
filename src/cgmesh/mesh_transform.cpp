#include "mesh.h"

//
// Transformations
//

void Mesh::centerize (void)
{
	float cx = 0., cy = 0., cz = 0.;
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		cx += m_pVertices[3*i];
		cy += m_pVertices[3*i+1];
		cz += m_pVertices[3*i+2];
	}
	cx /= m_nVertices;
	cy /= m_nVertices;
	cz /= m_nVertices;
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		m_pVertices[3*i+0] -= cx;
		m_pVertices[3*i+1] -= cy;
		m_pVertices[3*i+2] -= cz;
	}
}

void Mesh::scale (float s)
{
	for (unsigned int i=0; i<3*m_nVertices; i++)
		m_pVertices[i] *= s;
}

void Mesh::scale_xyz (float sx, float sy, float sz)
{
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		m_pVertices[3*i+0] *= sx;
		m_pVertices[3*i+1] *= sy;
		m_pVertices[3*i+2] *= sz;
	}
}

void Mesh::translate (float tx, float ty, float tz)
{
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		m_pVertices[3*i+0] += tx;
		m_pVertices[3*i+1] += ty;
		m_pVertices[3*i+2] += tz;
	}
}

void Mesh::transform (float mrot[9])
{
	float tmp[3];
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		tmp[0] = m_pVertices[3*i];
		tmp[1] = m_pVertices[3*i+1];
		tmp[2] = m_pVertices[3*i+2];
		m_pVertices[3*i]   = mrot[0]*tmp[0] + mrot[1]*tmp[1] + mrot[2]*tmp[2];
		m_pVertices[3*i+1] = mrot[3]*tmp[0] + mrot[4]*tmp[1] + mrot[5]*tmp[2];
		m_pVertices[3*i+2] = mrot[6]*tmp[0] + mrot[7]*tmp[1] + mrot[8]*tmp[2];
	}
}

void Mesh::transform (mat3 m)
{
	vec3 vtmp;
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		vec3_init (vtmp, m_pVertices[3*i], m_pVertices[3*i+1], m_pVertices[3*i+2]);
		mat3_transform (vtmp, m, vtmp);
		m_pVertices[3*i]   = vtmp[0];
		m_pVertices[3*i+1] = vtmp[1];
		m_pVertices[3*i+2] = vtmp[2];
	}
}

void Mesh::transform4 (mat4 m)
{
	vec4 vtmp;
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		vec4_init (vtmp, m_pVertices[3*i], m_pVertices[3*i+1], m_pVertices[3*i+2], 1.);
		mat4_transform (vtmp, m, vtmp);
		m_pVertices[3*i]   = vtmp[0];
		m_pVertices[3*i+1] = vtmp[1];
		m_pVertices[3*i+2] = vtmp[2];
	}
}

