#include "../cgmesh/mesh.h" // Explicitly include Mesh definition early

#include <stdio.h>
#include <stdlib.h>

#include "gl_wrapper.h"

#include "mesh_renderer.h"
#include "material_renderer.h"
#include "../cgmesh/mesh_data_manager.h"

void rendering_properties_init (rendering_properties_s &prop)
{
	prop.light = 1;
	prop.smooth = 1;
	prop.display_points = 0;
	prop.display_vertex_normals = 0;
	prop.display_face_normals = 0;
	prop.display_wireframe = 0;
	prop.display_fill = 1;
	prop.display_repere = 1;
	prop.normalized = 0;
	prop.pointsize = 1.;
	prop.linesize = 1.;
	prop.display_warning = 0;
	prop.clipping_plane_active = 0;
	prop.clipping_plane_z = 0.0f;
}

float pointsize = 1.;






void mesh_draw (Mesh *mesh, rendering_properties_s &prop, const vector<int>& materialIds)
{
	if (!mesh)
		return;

	if (prop.clipping_plane_active)
	{
		GLdouble plane[] = { 0.0, 0.0, -1.0, (GLdouble)prop.clipping_plane_z };
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

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
			if (!mesh->m_pVertexColors.empty() && !prop.light)
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
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0, 1.0);

		int i_current_material = -1;

		for (unsigned int i=0; i<mesh->m_nFaces; i++)
		{
			Face *pFace = mesh->m_pFaces[i];
			
			// Material management
			int meshMatId = pFace->GetMaterialId();
			if (meshMatId != MATERIAL_NONE && meshMatId != i_current_material)
			{
				if (!materialIds.empty() && meshMatId < (int)materialIds.size())
				{
					int rendererId = materialIds[meshMatId];
					if (rendererId != -1)
					{
						MaterialRenderer::getInstance()->ActivateMaterial(rendererId);
						i_current_material = meshMatId;
					}
				}
				else {
					// fallback if no cache provided
					Material* mat = mesh->GetMaterial(meshMatId);
					if (mat) {
						int rendererId = MaterialRenderer::getInstance()->AddMaterial(mat);
						MaterialRenderer::getInstance()->ActivateMaterial(rendererId);
						i_current_material = meshMatId;
					}
				}
			}
			if (pFace->m_nVertices == 3)
			{
				unsigned int a = pFace->m_pVertices[0];
				unsigned int b = pFace->m_pVertices[1];
				unsigned int c = pFace->m_pVertices[2];

				glBegin (GL_TRIANGLES);

				// Face Normal (always used for FLAT shading or as base)
				glNormal3f (mesh->m_pFaceNormals[3*i], mesh->m_pFaceNormals[3*i+1], mesh->m_pFaceNormals[3*i+2]);

				// Vertex A
				if (!mesh->m_pVertexColors.empty() && !prop.light && i_current_material == -1)
					glColor3f (mesh->m_pVertexColors[3*a], mesh->m_pVertexColors[3*a+1], mesh->m_pVertexColors[3*a+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr && !mesh->m_pTextureCoordinates.empty())
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]+1];
					glTexCoord2f(u, v);
				}
				if (prop.smooth)
					glNormal3f (mesh->m_pVertexNormals[3*a], mesh->m_pVertexNormals[3*a+1], mesh->m_pVertexNormals[3*a+2]);
				glVertex3f (mesh->m_pVertices[3*a], mesh->m_pVertices[3*a+1], mesh->m_pVertices[3*a+2]);
				
				// Vertex B
				if (!mesh->m_pVertexColors.empty() && !prop.light && i_current_material == -1)
					glColor3f (mesh->m_pVertexColors[3*b], mesh->m_pVertexColors[3*b+1], mesh->m_pVertexColors[3*b+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr && !mesh->m_pTextureCoordinates.empty())
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]+1];
					glTexCoord2f(u, v);
				}
				if (prop.smooth)
					glNormal3f (mesh->m_pVertexNormals[3*b], mesh->m_pVertexNormals[3*b+1], mesh->m_pVertexNormals[3*b+2]);
				glVertex3f (mesh->m_pVertices[3*b], mesh->m_pVertices[3*b+1], mesh->m_pVertices[3*b+2]);
				
				// Vertex C
				if (!mesh->m_pVertexColors.empty() && !prop.light && i_current_material == -1)
					glColor3f (mesh->m_pVertexColors[3*c], mesh->m_pVertexColors[3*c+1], mesh->m_pVertexColors[3*c+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr && !mesh->m_pTextureCoordinates.empty())
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]+1];
					glTexCoord2f(u, v);
				}
				if (prop.smooth)
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
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[0]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*a], mesh->m_pVertices[3*a+1], mesh->m_pVertices[3*a+2]);
				
				//glNormal3f (mesh->m_pVertexNormals[3*b], mesh->m_pVertexNormals[3*b+1], mesh->m_pVertexNormals[3*b+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[1]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*b], mesh->m_pVertices[3*b+1], mesh->m_pVertices[3*b+2]);
				
				//glNormal3f (mesh->m_pVertexNormals[3*c], mesh->m_pVertexNormals[3*c+1], mesh->m_pVertexNormals[3*c+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[2]+1];
					glTexCoord2f(u, v);
				}
				glVertex3f (mesh->m_pVertices[3*c], mesh->m_pVertices[3*c+1], mesh->m_pVertices[3*c+2]);
				
				//glNormal3f (mesh->m_pVertexNormals[3*d], mesh->m_pVertexNormals[3*d+1], mesh->m_pVertexNormals[3*d+2]);
				if (pFace->m_bUseTextureCoordinates && pFace->m_pTextureCoordinatesIndices != nullptr)
				{
					glColor3f (1., 1., 1.);
					float u = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[3]];
					float v = mesh->m_pTextureCoordinates[2*pFace->m_pTextureCoordinatesIndices[3]+1];
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
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	// lines
	if (prop.display_wireframe)
	{
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);
		glColor3f (0., 0., 0.);

		glEnable (GL_LINE_SMOOTH);
		glLineWidth (prop.linesize);
			
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

		glEnable(GL_LINE_SMOOTH);
		glLineWidth(1.0f);

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

	if (prop.clipping_plane_active)
	{
		glDisable(GL_CLIP_PLANE0);
	}
}



MeshRenderer *MeshRenderer::m_pInstance = new MeshRenderer;

MeshRenderer::MeshRenderer()
{
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
	if (!pMesh)
		return -1;

	rendering_element_s el;
	el.method = method;
	el.pMesh = pMesh;
	el.id = -1;
	rendering_properties_init (el.properties);

	switch (method)
	{
	case CG_RENDERING_DEFAULT:
		break;
	case CG_RENDERING_DISPLAY_LIST:
		el.id = m_displayListManager->addMesh (pMesh);
		break;
	case CG_RENDERING_VERTEX_ARRAY:
		el.id = m_vertexArrayManager->addMesh (pMesh);
		break;
	case CG_RENDERING_VBO:
		el.id = m_vboManager->addMesh (pMesh);
		break;
	case CG_RENDERING_VERTEX_BUFFER:
		el.id = m_vertexBufferManager->addMesh (pMesh);
		break;
	default:
		break;
	}

	m_meshes.push_back(el);
	const int id = (int)m_meshes.size() - 1;
	m_meshToId[pMesh] = id;
	return id;
}

void MeshRenderer::RemoveMesh (Mesh *pMesh)
{
	auto it = m_meshToId.find(pMesh);
	if (it == m_meshToId.end())
		return;
	const int id = it->second;
	m_meshToId.erase(it);
	if (id >= 0 && id < (int)m_meshes.size())
	{
		// Mark slot vacant so Draw() skips it; we keep the slot to preserve
		// ids of other entries (other tabs / canvases) into m_meshes.
		m_meshes[id].pMesh = nullptr;
	}
}

void MeshRenderer::Draw (int id)
{
	if (id < 0 || id >= (int)m_meshes.size())
		return;

	rendering_element_s& el = m_meshes[id];
	if (!el.pMesh)
		return; // slot vacated by RemoveMesh

	switch (el.method)
	{
	case CG_RENDERING_DEFAULT:
		mesh_draw (el.pMesh, el.properties, GetMaterialRendererIds(id));
		break;
	case CG_RENDERING_VERTEX_ARRAY:
		if (el.properties.display_fill)
			m_vertexArrayManager->Draw (el.id);
		
		// Always call mesh_draw for extras (wireframe, points, warnings)
		{
			rendering_properties_s extras = el.properties;
			extras.display_fill = 0; // Surface already handled
			mesh_draw(el.pMesh, extras, GetMaterialRendererIds(id));
		}
		break;
	case CG_RENDERING_DISPLAY_LIST:
		m_displayListManager->Draw (el.id);
		break;
	case CG_RENDERING_VBO:
		m_vboManager->Draw (el.id);
		break;
	case CG_RENDERING_VERTEX_BUFFER:
		m_vertexBufferManager->Draw (el.id);
		break;
	default:
		break;
	}
}

int MeshRenderer::GetMeshId (Mesh *pMesh, CG_rendering_method method)
{
	auto it = m_meshToId.find(pMesh);
	if (it != m_meshToId.end())
		return it->second;
	
	return AddMesh(pMesh, method);
}

void MeshRenderer::SetProperties(int id, const rendering_properties_s& prop)
{
	if (id >= 0 && id < (int)m_meshes.size() && m_meshes[id].pMesh)
	{
		m_meshes[id].properties = prop;
	}
}

const vector<int>& MeshRenderer::GetMaterialRendererIds(int elementId)
{
	static const vector<int> empty;
	if (elementId < 0 || elementId >= (int)m_meshes.size())
		return empty;
	rendering_element_s& el = m_meshes[elementId];
	if (!el.pMesh)
		return empty;
	uint64_t currentRevision = el.pMesh->GetRevision();

	if (el.materialCache.revision == currentRevision)
		return el.materialCache.rendererIds;

	// Update cache
	el.materialCache.rendererIds.clear();
	el.materialCache.revision = currentRevision;

	for (unsigned int i = 0; i < el.pMesh->m_nMaterials; i++)
	{
		Material* mat = el.pMesh->GetMaterial(i);
		if (mat)
			el.materialCache.rendererIds.push_back(MaterialRenderer::getInstance()->AddMaterial(mat));
		else
			el.materialCache.rendererIds.push_back(-1);
	}

	return el.materialCache.rendererIds;
}
