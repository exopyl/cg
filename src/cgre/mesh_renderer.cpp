#include "../cgmesh/mesh.h" // Explicitly include Mesh definition early

#include <stdio.h>
#include <stdlib.h>

#include <type_traits>

#include "gl_wrapper.h"

#include "mesh_renderer.h"
#include "material_renderer.h"
#include "diagnostics.h"
#include "../cgmesh/mesh_data_manager.h"

// VERROU. Cette structure est copiee par valeur a chaque maillage et a chaque
// image ; y remettre un conteneur a allocation dynamique ramenerait le cout
// quadratique qui figeait l'application. Le compilateur le refusera desormais.
static_assert (std::is_trivially_copyable<rendering_properties_s>::value,
               "rendering_properties_s doit rester trivialement copiable : elle est "
               "copiee par maillage et par image. Voir le commentaire de sa definition.");

void rendering_properties_init (rendering_properties_s &prop)
{
	prop.shading = CG_shading_mode::Materials;
	prop.light = 1;
	prop.smooth = 1;
	prop.display_points = 0;
	prop.display_vertex_normals = 0;
	prop.display_face_normals = 0;
	prop.display_wireframe = 0;
	prop.display_fill = 1;
	prop.display_repere = 1;
	prop.display_grid = 0;
	prop.display_cutting_mat = 0;
	prop.normalized = 0;
	prop.pointsize = 1.;
	prop.linesize = 1.;
	prop.line_color[0]  = 0.15f; prop.line_color[1]  = 0.45f; prop.line_color[2]  = 0.85f;
	prop.point_color[0] = 0.90f; prop.point_color[1] = 0.20f; prop.point_color[2] = 0.20f;
	prop.display_warning = 0;
	prop.clipping_plane_active = 0;
	prop.clipping_plane_z = 0.0f;
}

float pointsize = 1.;






void mesh_draw (Mesh *mesh, rendering_properties_s &prop, const vector<int>& materialIds,
                bool surfaceAlreadyDrawn)
{
	if (!mesh)
		return;

	// TOUT CE QUI EST CONSTANT POUR LE MAILLAGE, hisse hors des boucles.
	//
	// Ces accesseurs rendent des references (mesh.h), donc rien n'est copie ;
	// mais ils etaient appeles par SOMMET, et surtout leurs gardes DIVERGEAIENT
	// d'une branche a l'autre.
	//
	// C'est de cette divergence qu'est ne le defaut : la garde `.empty()` sur les
	// coordonnees de texture etait presente dans les TROIS copies du bloc
	// triangle et absente des QUATRE copies du bloc quad. Un maillage a faces
	// quadrangulaires dont les faces annoncent des UV sans que le vecteur en
	// porte faisait un operator[] sur un vecteur vide -- donc, cote MSVC, un
	// deref de nullptr des le premier quad.
	//
	// Les normales n'etaient gardees NI par .empty() ni par indice, alors que
	// mesh.h:594 previent que c'est a l'appelant d'appeler ComputeNormals().
	const std::vector<float>& vertices = mesh->GetVertices ();
	const std::vector<float>& vnormals = mesh->GetVertexNormals ();
	const std::vector<float>& fnormals = mesh->GetFaceNormals ();
	const std::vector<float>& uvs      = mesh->GetTextureCoordinates ();
	const std::vector<float>& vcolors  = mesh->GetVertexColors ();
	const bool hasVNormals = !vnormals.empty ();
	const bool hasFNormals = !fnormals.empty ();
	const bool hasVColors  = !vcolors.empty ();

	if (prop.clipping_plane_active)
	{
		GLdouble plane[] = { 0.0, 0.0, -1.0, (GLdouble)prop.clipping_plane_z };
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	// Renormalise normals on the GPU unconditionally: a safety net so lighting
	// stays correct whatever the vertex-normal magnitude or the modelview scale
	// (per-vertex normals are also unit-normalised at compute time now).
	glEnable(GL_NORMALIZE);

	// normalize the model
	if (prop.normalized)
	{
		const BoundingBox& bbox = mesh->bbox();
		float length = bbox.GetLargestLength();
		float scale = 1.f/ length;

		glPushMatrix ();
		glScalef (scale, scale, scale);
	}

	// points
	if (prop.display_points)
	{
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);
		glColor3f (1., 0., 0.);
		glPointSize (prop.pointsize);
		glBegin (GL_POINTS);
		for (unsigned int i=0; i<mesh->GetNVertices (); i++)
		{
			// Lighting is disabled for this pass (above), so per-vertex colours
			// from a coloured cloud (.ply/.pset/.pts) must be applied whenever
			// present — gating on !prop.light left coloured clouds all red.
			if (hasVColors)
				glColor3f (mesh->GetVertexColors ()[3*i],
					   mesh->GetVertexColors ()[3*i+1],
					   mesh->GetVertexColors ()[3*i+2]);
			glVertex3f (mesh->GetVertices ()[3*i],
				    mesh->GetVertices ()[3*i+1],
				    mesh->GetVertices ()[3*i+2]);
		}
		glEnd ();
		glPopAttrib ();
	}

	// vertex normals
	// hasVNormals : cet overlay lisait GetVertexNormals()[3*i] sans aucune garde,
	// alors qu'il s'affiche precisement quand on doute des normales -- donc,
	// potentiellement, quand ComputeNormals n'a pas ete appele.
	if (prop.display_vertex_normals && hasVNormals)
	{
		float fVertexNormalsScale = 0.2f;
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);
		glLineWidth (2.f);
		glColor3f (0., 0., 1.);
		glBegin (GL_LINES);
		for (unsigned int i=0; i<mesh->GetNVertices (); i++)
		{
			glVertex3f (mesh->GetVertices ()[3*i],
				    mesh->GetVertices ()[3*i+1],
				    mesh->GetVertices ()[3*i+2]);
			glVertex3f (mesh->GetVertices ()[3*i] + fVertexNormalsScale*mesh->GetVertexNormals ()[3*i],
				    mesh->GetVertices ()[3*i+1] + fVertexNormalsScale*mesh->GetVertexNormals ()[3*i+1],
				    mesh->GetVertices ()[3*i+2] + fVertexNormalsScale*mesh->GetVertexNormals ()[3*i+2]);
		}
		glEnd ();
		glPopAttrib ();
	}

	// polygons
	if (prop.display_fill && !surfaceAlreadyDrawn)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0, 1.0);

		int i_current_material = -1;

		// LE MODE D'OMBRAGE DECIDE, une fois pour tout le maillage.
		//
		// Hors du mode Materials, aucun materiau du maillage n'est active : la
		// couleur vient du materiau NEUTRE, ou des couleurs par sommet. Poser
		// l'etat ici plutot que par face evite de le reposer a chaque triangle,
		// et garde `i_current_material` a -1, ce dont la lecture des couleurs par
		// sommet plus bas se sert deja comme condition.
		const bool useMeshMaterials = (prop.shading == CG_shading_mode::Materials);
		const bool useVertexColors  = (prop.shading == CG_shading_mode::VertexColors)
		                              && !mesh->GetVertexColors ().empty ();
		if (useMeshMaterials)
		{
			// Le defaut est REPOSE a chaque maillage. Le laisser au reglage du
			// contexte, comme avant, le faisait ecraser par le premier materiau
			// active : la couleur d'un maillage sans materiau dependait alors de
			// ce qui avait ete dessine avant lui.
			MaterialRenderer::ActivateDefaultMaterial ();
		}
		else
		{
			MaterialRenderer::ActivateNeutralMaterial ();
			// Les couleurs par sommet passent par glColor : sans COLOR_MATERIAL
			// elles seraient ignorees sous eclairage, et le maillage sortirait
			// uniformement blanc.
			if (useVertexColors)
			{
				glColorMaterial (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
				glEnable (GL_COLOR_MATERIAL);
			}
		}

		for (unsigned int i=0; i<mesh->GetNFaces (); i++)
		{
			auto pFace = mesh->FaceAt (i);

			// Constant pour la FACE, plus reevalue par sommet : la meme condition
			// l'etait 3 fois (triangle) ou 4 fois (quad), et chaque pFace->... passe
			// par ConstFaceRef::IsValid() puis Mesh::FaceExists().
			const bool hasUVThisFace = !uvs.empty ()
			                           && pFace->UsesTextureCoordinates ()
			                           && pFace->HasTexCoordIndices ();

			// Material management
			int meshMatId = pFace->GetMaterialId();
			if (useMeshMaterials && meshMatId != MATERIAL_NONE && meshMatId != i_current_material)
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
			if (pFace->GetNVertices () == 3)
			{
				unsigned int a = pFace->GetVertex (0);
				unsigned int b = pFace->GetVertex (1);
				unsigned int c = pFace->GetVertex (2);

				glBegin (GL_TRIANGLES);

				// Face Normal (always used for FLAT shading or as base)
				if (hasFNormals) glNormal3f (fnormals[3*i], fnormals[3*i+1], fnormals[3*i+2]);

				// Vertex A
				if (useVertexColors || (hasVColors && !prop.light && i_current_material == -1))
					glColor3f (vcolors[3*a], vcolors[3*a+1], vcolors[3*a+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (0);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				if (prop.smooth)
					if (hasVNormals) glNormal3f (vnormals[3*a], vnormals[3*a+1], vnormals[3*a+2]);
				glVertex3f (mesh->GetVertices ()[3*a], mesh->GetVertices ()[3*a+1], mesh->GetVertices ()[3*a+2]);
				
				// Vertex B
				if (useVertexColors || (hasVColors && !prop.light && i_current_material == -1))
					glColor3f (vcolors[3*b], vcolors[3*b+1], vcolors[3*b+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (1);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				if (prop.smooth)
					if (hasVNormals) glNormal3f (vnormals[3*b], vnormals[3*b+1], vnormals[3*b+2]);
				glVertex3f (mesh->GetVertices ()[3*b], mesh->GetVertices ()[3*b+1], mesh->GetVertices ()[3*b+2]);
				
				// Vertex C
				if (useVertexColors || (hasVColors && !prop.light && i_current_material == -1))
					glColor3f (vcolors[3*c], vcolors[3*c+1], vcolors[3*c+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (2);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				if (prop.smooth)
					if (hasVNormals) glNormal3f (vnormals[3*c], vnormals[3*c+1], vnormals[3*c+2]);
				glVertex3f (mesh->GetVertices ()[3*c], mesh->GetVertices ()[3*c+1], mesh->GetVertices ()[3*c+2]);

				glEnd ();
			}
			else if (pFace->GetNVertices () == 4)
			{
				unsigned int a = pFace->GetVertex (0);
				unsigned int b = pFace->GetVertex (1);
				unsigned int c = pFace->GetVertex (2);
				unsigned int d = pFace->GetVertex (3);
				glBegin (GL_QUADS);

				if (hasFNormals) glNormal3f (fnormals[3*i], fnormals[3*i+1], fnormals[3*i+2]);
				//if (hasVNormals) glNormal3f (vnormals[3*a], vnormals[3*a+1], vnormals[3*a+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (0);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				glVertex3f (mesh->GetVertices ()[3*a], mesh->GetVertices ()[3*a+1], mesh->GetVertices ()[3*a+2]);
				
				//if (hasVNormals) glNormal3f (vnormals[3*b], vnormals[3*b+1], vnormals[3*b+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (1);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				glVertex3f (mesh->GetVertices ()[3*b], mesh->GetVertices ()[3*b+1], mesh->GetVertices ()[3*b+2]);
				
				//if (hasVNormals) glNormal3f (vnormals[3*c], vnormals[3*c+1], vnormals[3*c+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (2);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				glVertex3f (mesh->GetVertices ()[3*c], mesh->GetVertices ()[3*c+1], mesh->GetVertices ()[3*c+2]);
				
				//if (hasVNormals) glNormal3f (vnormals[3*d], vnormals[3*d+1], vnormals[3*d+2]);
				if (hasUVThisFace)
				{
					glColor3f (1., 1., 1.);
					const unsigned int ti = pFace->GetTexCoordIndex (3);
					// Indice issu du FICHIER : il n'etait borne nulle part.
					if (2*ti + 1 < uvs.size ()) glTexCoord2f (uvs[2*ti], uvs[2*ti+1]);
				}
				glVertex3f (mesh->GetVertices ()[3*d], mesh->GetVertices ()[3*d+1], mesh->GetVertices ()[3*d+2]);
				glEnd ();
			}
			else
			{
				glBegin (GL_POLYGON);
				for (unsigned int j=0; j<pFace->GetNVertices (); j++)
				{
					unsigned int a = pFace->GetVertex (j);
					glVertex3f (mesh->GetVertices ()[3*a], mesh->GetVertices ()[3*a+1], mesh->GetVertices ()[3*a+2]);
				}
				glEnd ();
			}
		}
		glDisable(GL_POLYGON_OFFSET_FILL);
		// COLOR_MATERIAL est REMIS comme il etait : il n'est active que pour les
		// couleurs par sommet, et le laisser ouvert ferait suivre le dernier
		// glColor a tout ce qui est dessine ensuite -- le fil de fer, les
		// reperes, le maillage suivant.
		glDisable (GL_COLOR_MATERIAL);
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
		for (unsigned int i=0; i<mesh->GetNFaces (); i++)
		{
			auto pFace = mesh->FaceAt (i);
			for (unsigned int j=0; j<pFace->GetNVertices (); j++)
			{
				unsigned int a = pFace->GetVertex (j);
				unsigned int b = pFace->GetVertex ((j+1)%pFace->GetNVertices ());
				glVertex3f (mesh->GetVertices ()[3*a], mesh->GetVertices ()[3*a+1], mesh->GetVertices ()[3*a+2]);
				glVertex3f (mesh->GetVertices ()[3*b], mesh->GetVertices ()[3*b+1], mesh->GetVertices ()[3*b+2]);
			}
		}
		glEnd ();
		glPopAttrib ();
	}

	// OBJ line ('l') and point ('p') elements are intrinsic mesh geometry
	// (segments de ligne / points isoles), not an optional overlay: they belong to the
	// mesh and are drawn with it whenever present. A lines/points file has no
	// faces, and the fast face managers (VBO / vertex array) don't know about
	// these primitives, so the immediate-mode path is the only place that can
	// emit them — hence they draw unconditionally here, alongside the surface.
	if (!mesh->GetLines ().empty() || !mesh->GetPoints ().empty())
	{
		glPushAttrib (GL_ALL_ATTRIB_BITS);
		glDisable (GL_LIGHTING);

		if (!mesh->GetLines ().empty())
		{
			glEnable (GL_LINE_SMOOTH);
			glLineWidth (prop.linesize > 0.f ? prop.linesize : 1.f);
			glColor3f (prop.line_color[0], prop.line_color[1], prop.line_color[2]);
			glBegin (GL_LINES);
			const size_t nIdx = mesh->GetLines ().size();
			for (size_t k = 0; k + 1 < nIdx; k += 2)
			{
				unsigned int a = mesh->GetLines ()[k];
				unsigned int b = mesh->GetLines ()[k+1];
				if (a >= mesh->GetNVertices () || b >= mesh->GetNVertices ())
					continue;
				glVertex3f (mesh->GetVertices ()[3*a], mesh->GetVertices ()[3*a+1], mesh->GetVertices ()[3*a+2]);
				glVertex3f (mesh->GetVertices ()[3*b], mesh->GetVertices ()[3*b+1], mesh->GetVertices ()[3*b+2]);
			}
			glEnd ();
		}

		if (!mesh->GetPoints ().empty())
		{
			glPointSize (prop.pointsize > 0.f ? prop.pointsize : 1.f);
			glColor3f (prop.point_color[0], prop.point_color[1], prop.point_color[2]);
			glBegin (GL_POINTS);
			for (unsigned int idx : mesh->GetPoints ())
			{
				if (idx >= mesh->GetNVertices ())
					continue;
				glVertex3f (mesh->GetVertices ()[3*idx], mesh->GetVertices ()[3*idx+1], mesh->GetVertices ()[3*idx+2]);
			}
			glEnd ();
		}

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

		// Lu PAR REFERENCE, et pour CE maillage seulement. L'ancienne version
		// parcourait les problemes de toute la scene -- donc les redessinait M
		// fois par image, une fois par maillage -- et copiait le vecteur de
		// chaque entree au passage (`auto edges = element.second`).
		const MeshDataManager::TopologicIssues& issues =
			MeshDataManager::GetInstance ().GetTopologicIssues (mesh);
		const std::vector<float>& vertices = mesh->GetVertices ();

		// non manifold
		glColor3f(1., 0., 0.);
		glBegin(GL_LINES);
		for (unsigned int index : issues.nonManifoldEdges)
			glVertex3f (vertices[3*index], vertices[3*index+1], vertices[3*index+2]);
		glEnd();

		// borders
		glColor3f(1., 1., 0.);
		glBegin(GL_LINES);
		for (unsigned int index : issues.borders)
			glVertex3f (vertices[3*index], vertices[3*index+1], vertices[3*index+2]);
		glEnd();

		glPopAttrib();
	}

	if (prop.normalized)
	{
		glPopMatrix ();
	}
	glDisable(GL_NORMALIZE);

	if (prop.clipping_plane_active)
	{
		glDisable(GL_CLIP_PLANE0);
	}

	// Les surcouches (fil de fer, points, normales, diagnostic) passent toutes
	// par ici, APRES DrawMaterialGroups. Sans ce point de controle, une erreur
	// nee dans cette fonction n'etait vidangee qu'au dessin SUIVANT et se voyait
	// donc attribuee a DrawMaterialGroups -- diagnostic trompeur.
	CGRE_CHECK_GL ("mesh_draw");
}



MeshRenderer *MeshRenderer::m_pInstance = new MeshRenderer;

MeshRenderer::MeshRenderer()
{

	m_vboManager = new VBOManager();
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
	case CG_RENDERING_VBO:
		el.id = m_vboManager->addMesh (pMesh);
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
	// LIBERER LES MATERIAUX DU MAILLAGE avant de lacher le pointeur.
	//
	// Le maillage est encore vivant a cet instant (les appelants -- fermeture
	// d'onglet, rechargement de modele -- appellent RemoveMesh AVANT de detruire
	// la scene), donc on peut encore enumerer ses materiaux. C'est le seul moment
	// ou c'est possible.
	//
	// Inconditionnel, sans comptage de references : Mesh::m_materials est un
	// vector<unique_ptr<Material>>, chaque maillage possede donc les siens et il
	// n'y a aucun partage entre maillages.
	for (unsigned int i = 0; i < pMesh->GetNMaterials (); i++)
	{
		if (Material *pMaterial = pMesh->GetMaterial (i))
			MaterialRenderer::getInstance ()->RemoveMaterial (pMaterial);
	}

	if (id >= 0 && id < (int)m_meshes.size())
	{
		// LIBERER LES TAMPONS GPU DU MAILLAGE. Sans cela, les cinq objets GL
		// (positions, normales, couleurs, UV, indices) restaient en VRAM pour la
		// duree du processus : sur un maillage de 2 M de triangles, de l'ordre de
		// 72 Mo abandonnes a chaque cycle recharger / fermer.
		if (m_meshes[id].method == CG_RENDERING_VBO)
			m_vboManager->removeMesh (m_meshes[id].id);
		m_meshes[id].id = -1;

		// Mark slot vacant so Draw() skips it; we keep the slot to preserve
		// ids of other entries (other tabs / canvases) into m_meshes.
		m_meshes[id].pMesh = nullptr;

		// Le cache d'identifiants de materiaux devient caduc en meme temps : les
		// emplacements qu'il designe viennent d'etre rendus et seront reattribues.
		// GetMaterialRendererIds rend deja un vecteur vide pour un pMesh nul, mais
		// autant ne pas conserver des indices perimes.
		m_meshes[id].materialCache.rendererIds.clear ();
		m_meshes[id].materialCache.revision = (uint64_t)-1;
	}
}

void MeshRenderer::Draw (int id)
{
	if (id < 0 || id >= (int)m_meshes.size())
		return;

	rendering_element_s& el = m_meshes[id];
	if (!el.pMesh)
		return; // slot vacated by RemoveMesh

	// For the fast paths (display list, vertex array, VBO), mesh_draw's per-face
	// material activation loop is bypassed. Activate the mesh's first material
	// here so the GL state is correct before the bulk draw call. This works
	// because cgmesh's ApplyMaterial paints all faces of a mesh with one
	// material id (true for our 3dm import path and most current importers).
	auto activateMeshMaterial = [&]() {
		// Hors du mode Materials, et aussi quand le maillage n'en porte AUCUN :
		// le neutre est lie explicitement. Sans cela, l'etat GL herite du
		// maillage precedent s'appliquait -- l'apparence d'un maillage sans
		// materiau dependait de l'ordre de la scene.
		if (el.properties.shading != CG_shading_mode::Materials)
		{
			MaterialRenderer::ActivateNeutralMaterial ();
			return;
		}
		const vector<int>& matIds = GetMaterialRendererIds(id);
		if (!matIds.empty() && matIds[0] != -1)
			MaterialRenderer::getInstance()->ActivateMaterial(matIds[0]);
		else
			MaterialRenderer::ActivateDefaultMaterial ();
	};

	// Enable the clipping plane up here so it covers every draw path
	// (VBO surface, vertex-array surface, mesh_draw overlays). Without this,
	// only the legacy mesh_draw branch saw the clip plane and the VBO
	// surface stayed uncut.
	if (el.properties.clipping_plane_active)
	{
		GLdouble plane[] = { 0.0, 0.0, -1.0, (GLdouble)el.properties.clipping_plane_z };
		glClipPlane(GL_CLIP_PLANE0, plane);
		glEnable(GL_CLIP_PLANE0);
	}

	// The fast paths (VBO / vertex array / display list / vertex buffer) bind a
	// single material for the whole mesh. A mesh carrying several materials
	// (e.g. a 3DS part whose faces mix a white bezel, a dark screen and a grey
	// body) must switch material per face, which only the immediate-mode
	// mesh_draw path does. Route such meshes through it.
	// The VBO path handles multiple materials itself (one draw call per
	// material run, see DrawMaterialGroups). The other fast paths bind a
	// single material for the whole mesh, so a multi-material mesh must go
	// through the immediate-mode mesh_draw which switches material per face.
	// ... et seulement quand les materiaux du maillage servent : en Neutre, il
	// n'y en a plus qu'un, donc les chemins rapides redeviennent utilisables.
	if (el.properties.shading == CG_shading_mode::Materials &&
	    el.pMesh->GetNMaterials() > 1 &&
	    el.method != CG_RENDERING_DEFAULT &&
	    el.method != CG_RENDERING_VBO)
	{
		mesh_draw(el.pMesh, el.properties, GetMaterialRendererIds(id));

		if (el.properties.clipping_plane_active)
			glDisable(GL_CLIP_PLANE0);
		return;
	}

	switch (el.method)
	{
	case CG_RENDERING_DEFAULT:
		mesh_draw (el.pMesh, el.properties, GetMaterialRendererIds(id));
		break;
	case CG_RENDERING_VBO:
		if (el.properties.display_fill)
		{
			// One draw call per material run (handles single- and
			// multi-material meshes); activates each material in turn.
			m_vboManager->DrawMaterialGroups (el.id, GetMaterialRendererIds(id), !el.properties.smooth,
			                                  el.properties.shading == CG_shading_mode::Materials,
			                                  el.properties.shading == CG_shading_mode::VertexColors);
		}

		// Overlays (wireframe, points, vertex normals, warnings) still go
		// through the legacy mesh_draw path. Un DRAPEAU, plus une copie de
		// `el.properties` : celle-ci dupliquait la structure entiere par maillage
		// et par image.
		mesh_draw(el.pMesh, el.properties, GetMaterialRendererIds(id), /*surfaceAlreadyDrawn*/ true);
		break;
	default:
		break;
	}

	if (el.properties.clipping_plane_active)
		glDisable(GL_CLIP_PLANE0);

	CGRE_CHECK_GL ("MeshRenderer::Draw");
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

	for (unsigned int i = 0; i < el.pMesh->GetNMaterials(); i++)
	{
		Material* mat = el.pMesh->GetMaterial(i);
		if (mat)
			el.materialCache.rendererIds.push_back(MaterialRenderer::getInstance()->AddMaterial(mat));
		else
			el.materialCache.rendererIds.push_back(-1);
	}

	return el.materialCache.rendererIds;
}
