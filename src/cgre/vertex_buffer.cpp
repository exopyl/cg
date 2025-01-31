#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>

#include  "vertex_buffer.h"

void VertexBuffer::Draw (bool bColor)
{
	if (!m_pMesh)
		return;

	if (m_pMesh->m_pVertexColors && bColor)
	{
		glEnableClientState (GL_COLOR_ARRAY);
		glColorPointer  (3, GL_FLOAT, 0, m_pMesh->m_pVertexColors);

#if 0
		if (Material *pMaterial = m_pMesh->GetMaterial ())
		{
			//glEnable (GL_COLOR_MATERIAL);
			//glColorMaterial (GL_FRONT, GL_DIFFUSE);			glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, pMaterial->m_fAmbient);
			glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, pMaterial->m_fDiffuse);
			glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, pMaterial->m_fSpecular);
			glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, pMaterial->m_fEmission);
			glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, pMaterial->m_fShininess);
		}
#endif
	}
	else
	{
		glDisableClientState (GL_COLOR_ARRAY);
	}
	if (m_pMesh->m_pVertexNormals)
	{
		glEnableClientState (GL_NORMAL_ARRAY);
		glNormalPointer (GL_FLOAT, 0, m_pMesh->m_pVertexNormals);
	}
	else
	{
		glDisableClientState (GL_NORMAL_ARRAY);
	}
	if (m_pMesh->m_pVertices)
	{
		glEnableClientState (GL_VERTEX_ARRAY);
		glVertexPointer (3, GL_FLOAT, 0, m_pMesh->m_pVertices);
	}
	
	// TODO : convert array of faces to array of indices
	//glDrawElements (GL_TRIANGLES, 3*m_pMesh->m_nFaces, GL_UNSIGNED_INT, m_pMesh->m_pFaces);
}
