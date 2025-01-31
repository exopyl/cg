#include <stdio.h>
#include <stdlib.h>

#include "gl_wrapper.h"

#include "mesh_renderer.h"
#include "material_renderer.h"

void rendering_properties_init (rendering_properties_s &prop)
{
	prop.light = 1;
	prop.display_points = 0;
	prop.display_vertex_normals = 0;
	prop.display_face_normals = 0;
	prop.display_wireframe = 0;
	prop.display_fill = 1;
	prop.normalized = 0;
	prop.pointsize = 1.;
	prop.linesize = 1.;
	prop.display_warning = 0;
}

float pointsize = 1.;






void mesh_draw (Mesh *mesh, rendering_properties_s &prop)
{
	if (!mesh)
		return;

	// normalize the model
	if (prop.normalized)
	{
		const BoundingBox& bbox = mesh->bbox();
		float length = bbox.GetLargestLength();
		float scale = 1.f/ length;

		glPushMatrix ();
		glScalef (scale, scale, scale);
		glEnable(GL_NORMALIZE);
	}

	// points
	if (prop.display_points)
	{
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);
		glColor3f (1., 0., 0.);
		glPointSize (prop.pointsize);
		glBegin (GL_POINTS);
		for (unsigned int i=0; i<mesh->m_nVertices; i++)
		{
			if (mesh->m_pVertexColors && !prop.light)
				glColor3f (mesh->m_pVertexColors[3*i],
					   mesh->m_pVertexColors[3*i+1],
					   mesh->m_pVertexColors[3*i+2]);
			glVertex3f (mesh->m_pVertices[3*i],
				    mesh->m_pVertices[3*i+1],
				    mesh->m_pVertices[3*i+2]);
		}
		glEnd ();
		glPopAttrib ();
	}

	// vertex normals
	if (prop.display_vertex_normals)
	{
		float fVertexNormalsScale = 0.2f;
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);
		glLineWidth (2.f);
		glColor3f (0., 0., 1.);
		glBegin (GL_LINES);
		for (unsigned int i=0; i<mesh->m_nVertices; i++)
		{
			glVertex3f (mesh->m_pVertices[3*i],
				    mesh->m_pVertices[3*i+1],
				    mesh->m_pVertices[3*i+2]);
			glVertex3f (mesh->m_pVertices[3*i] + fVertexNormalsScale*mesh->m_pVertexNormals[3*i],
				    mesh->m_pVertices[3*i+1] + fVertexNormalsScale*mesh->m_pVertexNormals[3*i+1],
				    mesh->m_pVertices[3*i+2] + fVertexNormalsScale*mesh->m_pVertexNormals[3*i+2]);
		}
		glEnd ();
		glPopAttrib ();
	}

	// polygons
	if (prop.display_fill)
	{
		for (unsigned int i=0; i<mesh->m_nFaces; i++)
		{
			Face *pFace = mesh->m_pFaces[i];
			if (pFace->GetMaterialId () != MATERIAL_NONE)
			{
				Material *mat = mesh->GetMaterial (pFace->GetMaterialId ());
				if (mat->GetType () == MATERIAL_COLOR)
				{
				}
				else if (mat->GetType () == MATERIAL_COLOR_ADV)
				{
					MaterialColorExt *matExt = dynamic_cast<MaterialColorExt*> (mat);
					float diffuse[4];
					matExt->GetDiffuse (diffuse);
					glColor3f (diffuse[0], diffuse[1], diffuse[2]);
				}
				else if (mat->GetType () == MATERIAL_TEXTURE)
				{
					glEnable(GL_TEXTURE_2D);
					//MaterialRenderer::getInstance()->ActivateMaterial (mat);
				}
			}
			if (pFace->m_nVertices == 3)
			{
				unsigned int a = pFace->m_pVertices[0];
				unsigned int b = pFace->m_pVertices[1];
				unsigned int c = pFace->m_pVertices[2];

				glBegin (GL_TRIANGLES);

				glNormal3f (mesh->m_pFaceNormals[3*i], mesh->m_pFaceNormals[3*i+1], mesh->m_pFaceNormals[3*i+2]);
				if (mesh->m_pVertexColors)
					glColor3f (mesh->m_pVertexColors[3*a], mesh->m_pVertexColors[3*a+1], mesh->m_pVertexColors[3*a+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]+1];
					glTexCoord2f(u, v);
				}
				glNormal3f (mesh->m_pVertexNormals[3*a], mesh->m_pVertexNormals[3*a+1], mesh->m_pVertexNormals[3*a+2]);
				glVertex3f (mesh->m_pVertices[3*a], mesh->m_pVertices[3*a+1], mesh->m_pVertices[3*a+2]);
				
				if (mesh->m_pVertexColors)
					glColor3f (mesh->m_pVertexColors[3*b], mesh->m_pVertexColors[3*b+1], mesh->m_pVertexColors[3*b+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]+1];
					glTexCoord2f(u, v);
				}
				glNormal3f (mesh->m_pVertexNormals[3*b], mesh->m_pVertexNormals[3*b+1], mesh->m_pVertexNormals[3*b+2]);
				glVertex3f (mesh->m_pVertices[3*b], mesh->m_pVertices[3*b+1], mesh->m_pVertices[3*b+2]);
				
				if (mesh->m_pVertexColors)
					glColor3f (mesh->m_pVertexColors[3*c], mesh->m_pVertexColors[3*c+1], mesh->m_pVertexColors[3*c+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]+1];
					glTexCoord2f(u, v);
				}

				glNormal3f (mesh->m_pVertexNormals[3*c], mesh->m_pVertexNormals[3*c+1], mesh->m_pVertexNormals[3*c+2]);
				glVertex3f (mesh->m_pVertices[3*c], mesh->m_pVertices[3*c+1], mesh->m_pVertices[3*c+2]);

				glEnd ();
			}
			else if (pFace->m_nVertices == 4)
			{
				unsigned int a = pFace->m_pVertices[0];
				unsigned int b = pFace->m_pVertices[1];
				unsigned int c = pFace->m_pVertices[2];
				unsigned int d = pFace->m_pVertices[3];
				glBegin (GL_QUADS);

				glNormal3f (mesh->m_pFaceNormals[3*i], mesh->m_pFaceNormals[3*i+1], mesh->m_pFaceNormals[3*i+2]);
				//glNormal3f (mesh->m_pVertexNormals[3*a], mesh->m_pVertexNormals[3*a+1], mesh->m_pVertexNormals[3*a+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*a], mesh->m_pVertices[3*a+1], mesh->m_pVertices[3*a+2]);
				
				//glNormal3f (mesh->m_pVertexNormals[3*b], mesh->m_pVertexNormals[3*b+1], mesh->m_pVertexNormals[3*b+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*b], mesh->m_pVertices[3*b+1], mesh->m_pVertices[3*b+2]);
				
				//glNormal3f (mesh->m_pVertexNormals[3*c], mesh->m_pVertexNormals[3*c+1], mesh->m_pVertexNormals[3*c+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*c], mesh->m_pVertices[3*c+1], mesh->m_pVertices[3*c+2]);
				
				//glNormal3f (mesh->m_pVertexNormals[3*d], mesh->m_pVertexNormals[3*d+1], mesh->m_pVertexNormals[3*d+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[3]];
					float v = 1.-mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[3]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*d], mesh->m_pVertices[3*d+1], mesh->m_pVertices[3*d+2]);
				glEnd ();
			}
			else
			{
				glBegin (GL_POLYGON);
				for (unsigned int j=0; j<pFace->m_nVertices; j++)
				{
					unsigned int a = pFace->m_pVertices[j];
					glVertex3f (mesh->m_pVertices[3*a], mesh->m_pVertices[3*a+1], mesh->m_pVertices[3*a+2]);
				}
				glEnd ();
			}
		}
	}

	// lines
	if (prop.display_wireframe)
	{
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);
		glColor3f (0., 0., 0.);

		glEnable (GL_POLYGON_OFFSET_LINE); 
		glPolygonOffset (1., 2.);
		glEnable (GL_LINE_SMOOTH);
		glLineWidth (.1f);
			
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBegin (GL_LINES);
		for (unsigned int i=0; i<mesh->m_nFaces; i++)
		{
			Face *pFace = mesh->m_pFaces[i];
			for (unsigned int j=0; j<pFace->m_nVertices; j++)
			{
				unsigned int a = pFace->m_pVertices[j];
				unsigned int b = pFace->m_pVertices[(j+1)%pFace->m_nVertices];
				glVertex3f (mesh->m_pVertices[3*a], mesh->m_pVertices[3*a+1], mesh->m_pVertices[3*a+2]);
				glVertex3f (mesh->m_pVertices[3*b], mesh->m_pVertices[3*b+1], mesh->m_pVertices[3*b+2]);
			}
		}
		glEnd ();
		glPopAttrib ();
	}

	// warning
	if (prop.display_warning)
	{
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_LIGHTING);

		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(1., 2.);
		glEnable(GL_LINE_SMOOTH);
		glLineWidth(.5f);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		// non manifold
		glColor3f(1., 0., 0.);
		glBegin(GL_LINES);
		for (auto& element : prop.nonManifoldEdges)
		{
			auto mesh = element.first;
			auto edges = element.second;
			for (auto index : edges)
				glVertex3f(mesh->m_pVertices[3 * index], mesh->m_pVertices[3 * index + 1], mesh->m_pVertices[3 * index + 2]);
		}
		glEnd();

		// borders
		glColor3f(1., 1., 0.);
		glBegin(GL_LINES);
		for (auto& element : prop.borders)
		{
			auto mesh = element.first;
			auto edges = element.second;
			for (auto index : edges)
				glVertex3f(mesh->m_pVertices[3 * index], mesh->m_pVertices[3 * index + 1], mesh->m_pVertices[3 * index + 2]);
		}
		glEnd();

		glPopAttrib();
	}

	if (prop.normalized)
	{
		glPopMatrix ();
		glDisable(GL_NORMALIZE);
	}
}



MeshRenderer *MeshRenderer::m_pInstance = new MeshRenderer;

MeshRenderer::MeshRenderer()
{
	m_nMeshes = 0;
	m_displayListManager = new DisplayListManager ();
	m_vertexArrayManager = new VertexArrayManager ();
	m_vboManager = new VBOManager();
	m_vertexBufferManager = new VertexBufferManager ();
}

MeshRenderer::~MeshRenderer()
{
}

int MeshRenderer::AddMesh (Mesh *pMesh, CG_rendering_method method)
{
	m_meshes[m_nMeshes].method = method;
	m_meshes[m_nMeshes].pMesh = pMesh;
	rendering_properties_init (m_meshes[m_nMeshes].properties);

	switch (method)
	{
	case CG_RENDERING_DEFAULT:
		break;
	case CG_RENDERING_DISPLAY_LIST:
		m_meshes[m_nMeshes].id = m_displayListManager->addMesh (pMesh);
		break;
	case CG_RENDERING_VERTEX_ARRAY:
		m_meshes[m_nMeshes].id = m_vertexArrayManager->addMesh (pMesh);
		break;
	case CG_RENDERING_VBO:
		m_meshes[m_nMeshes].id = m_vboManager->addMesh (pMesh);
		break;
	case CG_RENDERING_VERTEX_BUFFER:
		m_meshes[m_nMeshes].id = m_vertexBufferManager->addMesh (pMesh);
		break;
	default:
		break;
	}
	
	m_nMeshes++;
	return m_nMeshes-1;
}

void MeshRenderer::Draw (int id)
{
	switch (m_meshes[id].method)
	{
	case CG_RENDERING_DEFAULT:
		mesh_draw (m_meshes[id].pMesh, m_meshes[id].properties);
		break;
	case CG_RENDERING_DISPLAY_LIST:
		m_displayListManager->Draw (m_meshes[id].id);
		break;
	case CG_RENDERING_VERTEX_ARRAY:
		m_vertexArrayManager->Draw (m_meshes[id].id);
		break;
	case CG_RENDERING_VBO:
		m_vboManager->Draw (m_meshes[id].id);
		break;
	case CG_RENDERING_VERTEX_BUFFER:
		m_vertexBufferManager->Draw (m_meshes[id].id);
		break;
	default:
		break;
	}
}
