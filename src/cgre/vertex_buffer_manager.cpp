#include "gl_wrapper.h"

#include "vertex_buffer_manager.h"

#include "diagnostics.h"
#include "gl_program.h"
#include "material_renderer.h"
#include "surface_program.h"
#include "../cgmesh/mesh_data_manager.h"

//
// VBO
//
// Server-side buffers (positions, normals, optional UVs, optional colors,
// triangle indices) driving glDrawElements via the fixed-function client-
// state API. Buffers are rebuilt on demand whenever the underlying Mesh's
// revision counter changes — keeps the visual in sync with in-place edits
// from the remote console (e.g. the `flip` command).
//
VBOManager::VBOManager()
{
	m_idCurrent = 0;
}

VBOManager::~VBOManager()
{
	for (auto& kv : m_mapVBO)
		releaseBuffers(kv.second);
	m_mapVBO.clear();
	m_idCurrent = 0;
}

void VBOManager::releaseBuffers(vboInfo& info)
{
	GLuint ids[] = { info.vboPositions, info.vboNormals,
	                 info.vboColors,    info.vboTexCoords,
	                 info.iboIndices };
	for (GLuint& id : ids)
	{
		if (id != 0)
		{
			glDeleteBuffers(1, &id);
			id = 0;
		}
	}
	info.vboPositions = 0;
	info.vboNormals   = 0;
	info.vboColors    = 0;
	info.vboTexCoords = 0;
	info.iboIndices   = 0;
}

void VBOManager::uploadMesh(Mesh* mesh, vboInfo& info, bool flat)
{
	info.pMesh = mesh;

	// Build the expanded per-polygon vertex layout: every face contributes
	// its own N corners, each carrying the face's polygon normal. Adjacent
	// faces do NOT share corners — this is what guarantees uniform shading
	// within each polygon and eliminates fan-diagonal kinks on non-planar
	// n-gons.
	const Mesh::PolygonRenderData rd = mesh->BuildPolygonRenderData(flat);

	info.hasNormals   = !rd.normals.empty();
	info.hasColors    = !rd.colors.empty();
	info.hasTexCoords = !rd.texCoords.empty();

	const size_t nRenderVerts = rd.positions.size() / 3;

	// Positions
	if (info.vboPositions == 0) glGenBuffers(1, &info.vboPositions);
	glBindBuffer(GL_ARRAY_BUFFER, info.vboPositions);
	glBufferData(GL_ARRAY_BUFFER, rd.positions.size() * sizeof(float),
	             rd.positions.data(), GL_STATIC_DRAW);

	if (info.hasNormals)
	{
		if (info.vboNormals == 0) glGenBuffers(1, &info.vboNormals);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboNormals);
		glBufferData(GL_ARRAY_BUFFER, rd.normals.size() * sizeof(float),
		             rd.normals.data(), GL_STATIC_DRAW);
	}
	else if (info.vboNormals != 0)
	{
		glDeleteBuffers(1, &info.vboNormals);
		info.vboNormals = 0;
	}

	if (info.hasColors)
	{
		if (info.vboColors == 0) glGenBuffers(1, &info.vboColors);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboColors);
		glBufferData(GL_ARRAY_BUFFER, rd.colors.size() * sizeof(float),
		             rd.colors.data(), GL_STATIC_DRAW);
	}
	else if (info.vboColors != 0)
	{
		glDeleteBuffers(1, &info.vboColors);
		info.vboColors = 0;
	}

	if (info.hasTexCoords)
	{
		if (info.vboTexCoords == 0) glGenBuffers(1, &info.vboTexCoords);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboTexCoords);
		glBufferData(GL_ARRAY_BUFFER, rd.texCoords.size() * sizeof(float),
		             rd.texCoords.data(), GL_STATIC_DRAW);
	}
	else if (info.vboTexCoords != 0)
	{
		glDeleteBuffers(1, &info.vboTexCoords);
		info.vboTexCoords = 0;
	}

	if (!rd.indices.empty())
	{
		if (info.iboIndices == 0) glGenBuffers(1, &info.iboIndices);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.iboIndices);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             rd.indices.size() * sizeof(unsigned int),
		             rd.indices.data(), GL_STATIC_DRAW);
		info.count = (int)rd.indices.size();
	}
	else
	{
		info.count = 0;
	}

	info.materialRanges = rd.materialRanges;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	info.revision = mesh->GetRevision();
	info.flat = flat;
	(void)nRenderVerts;

	// Un glBufferData a court de VRAM echoue en silence : sans ce controle, le
	// symptome est une geometrie qui disparait, pas un message.
	CGRE_CHECK_GL ("VBOManager::uploadMesh");
}

void VBOManager::removeMesh (int id)
{
	if (id < 0) return;

	const auto it = m_mapVBO.find ((unsigned int)id);
	if (it == m_mapVBO.end ())
		return;

	releaseBuffers (it->second);
	m_mapVBO.erase (it);

	CGRE_CHECK_GL ("VBOManager::removeMesh");
}

int VBOManager::addMesh (Mesh *mesh)
{
	if (!mesh) return -1;

	vboInfo info{};
	uploadMesh(mesh, info, false); // smooth by default; re-uploaded on demand
	if (info.iboIndices == 0)
		return -1;

	m_mapVBO[m_idCurrent++] = info;
	return m_idCurrent - 1;
}


void VBOManager::Draw (int id)
{
	auto it = m_mapVBO.find(id);
	if (it == m_mapVBO.end())
		return;
	vboInfo& info = it->second;

	// Pick up in-place mesh mutations (e.g. RemoteConsole `flip`), preserving
	// the current shading mode.
	if (info.pMesh && info.pMesh->GetRevision() != info.revision)
		uploadMesh(info.pMesh, info, info.flat);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, info.vboPositions);
	glVertexPointer(3, GL_FLOAT, 0, nullptr);

	if (info.hasNormals)
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboNormals);
		glNormalPointer(GL_FLOAT, 0, nullptr);
	}

	if (info.hasColors)
	{
		glEnableClientState(GL_COLOR_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboColors);
		glColorPointer(3, GL_FLOAT, 0, nullptr);
	}

	if (info.hasTexCoords)
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboTexCoords);
		glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.iboIndices);

	// Offset the surface so wireframe / vertex normals overlays don't z-fight
	// (parity with VertexArrayManager::Draw).
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 1.0f);

	glDrawElements(GL_TRIANGLES, info.count, GL_UNSIGNED_INT, nullptr);

	glDisable(GL_POLYGON_OFFSET_FILL);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if (info.hasTexCoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (info.hasColors)    glDisableClientState(GL_COLOR_ARRAY);
	if (info.hasNormals)   glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void VBOManager::DrawMaterialGroups (int id, const std::vector<int>& rendererIds, bool flat,
                                    bool useMeshMaterials, bool useVertexColors,
                                    bool lighting)
{
	auto it = m_mapVBO.find(id);
	if (it == m_mapVBO.end())
		return;
	vboInfo& info = it->second;

	// Re-upload if the geometry changed (revision) OR the shading mode changed
	// (flat vs smooth uses a different vertex/normal layout). Done first so the
	// material-less fallback below also sees the up-to-date buffers.
	if (info.pMesh && (info.pMesh->GetRevision() != info.revision || info.flat != flat))
		uploadMesh(info.pMesh, info, flat);

	// No per-material grouping available: fall back to a single draw (the
	// caller is expected to have activated the material already).
	if (info.materialRanges.empty())
	{
		Draw(id);
		return;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, info.vboPositions);
	glVertexPointer(3, GL_FLOAT, 0, nullptr);

	if (info.hasNormals)
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboNormals);
		glNormalPointer(GL_FLOAT, 0, nullptr);
	}

	if (info.hasColors)
	{
		glEnableClientState(GL_COLOR_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboColors);
		glColorPointer(3, GL_FLOAT, 0, nullptr);
	}

	if (info.hasTexCoords)
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboTexCoords);
		glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.iboIndices);

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups (etat client)");

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 1.0f);

	// Hors du mode « materiaux », un seul materiau pour tout le maillage : il est
	// lie UNE FOIS, avant la boucle, et les plages ne le changent plus.
	if (useMeshMaterials)
		MaterialRenderer::ActivateDefaultMaterial ();
	else
		MaterialRenderer::ActivateNeutralMaterial ();

	// SECOND CHEMIN DE DESSIN. Le programme n'est lie que pour la surface, et
	// delie juste apres la boucle : les surcouches, le repere et la grille
	// continuent en fixe-fonction, ce que le contexte de compatibilite autorise.
	// Si la construction du programme a echoue, `prog` est nul et l'on reste sur
	// le pipeline fixe -- degrade, jamais casse.
	const cgre::GlProgram* prog = cgre::IsSurfaceShaderEnabled () ? cgre::SurfaceProgram ()
	                                                             : nullptr;
	if (prog)
	{
		prog->Use ();
		// Unites d'echantillonnage, fixees une fois : ce sont celles auxquelles
		// MaterialRenderer::ActivateMaterial lie deja ses textures.
		prog->SetInt ("uAlbedo", 0);
		prog->SetInt ("uReflection", 1);
		// Le mode « couleurs par sommet » n'a de sens que si le maillage en porte.
		prog->SetInt ("uUseVertexColors", (useVertexColors && info.hasColors) ? 1 : 0);
		prog->SetInt ("uLighting", lighting ? 1 : 0);
		// Etat par defaut, valable pour le materiau neutre comme pour celui par
		// defaut : ni texture ni reflet. Chaque plage qui en a le corrige.
		prog->SetInt   ("uUseTexture", 0);
		prog->SetInt   ("uUseReflection", 0);
		prog->SetFloat ("uReflAmount", 0.f);
	}

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups (materiau de base)");

	for (const Mesh::MaterialRange& r : info.materialRanges)
	{
		if (useMeshMaterials &&
		    r.materialId != MATERIAL_NONE &&
		    r.materialId < rendererIds.size() &&
		    rendererIds[r.materialId] != -1)
		{
			MaterialRenderer::getInstance()->ActivateMaterial(rendererIds[r.materialId]);

			// Sous programme lie, glEnable(GL_TEXTURE_2D) ne decide plus rien :
			// c'est l'uniforme qui dit au fragment s'il doit echantillonner.
			if (prog)
			{
				const MaterialRenderer::MaterialGlInfo mi =
					MaterialRenderer::getInstance()->GetGlInfo (rendererIds[r.materialId]);
				prog->SetInt   ("uUseTexture", (mi.hasTexture && info.hasTexCoords) ? 1 : 0);
				prog->SetInt   ("uUseReflection", mi.hasReflection ? 1 : 0);
				prog->SetFloat ("uReflAmount", mi.reflAmount);
			}
		}
		glDrawElements(GL_TRIANGLES, r.count, GL_UNSIGNED_INT,
		               (const GLvoid*)(size_t)(r.offset * sizeof(unsigned int)));
	}

	// HORS DE LA BOUCLE, deliberement. glGetError peut serialiser le pipeline :
	// un controle par plage de materiau rendait l'application inutilisable sur un
	// maillage a nombreux materiaux des que `glcheck` etait allume. La file
	// d'erreurs etant globale, un controle apres la boucle attrape les memes
	// erreurs -- il dit seulement « dans la boucle » plutot que « a la plage 47 »,
	// ce qui suffit largement pour localiser.
	if (prog) cgre::GlProgram::Unuse ();

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups (boucle de dessin)");

	glDisable(GL_POLYGON_OFFSET_FILL);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if (info.hasTexCoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (info.hasColors)    glDisableClientState(GL_COLOR_ARRAY);
	if (info.hasNormals)   glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups");
}
