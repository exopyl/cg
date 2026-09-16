#include "gl_wrapper.h"

#include "vertex_buffer_manager.h"

#include "diagnostics.h"
#include "gl_program.h"
#include "material_renderer.h"
#include "surface_program.h"
#include "../cgmesh/mesh_data_manager.h"
// Obtenus aujourd'hui par transitivite via material_renderer.h : les nommer ici
// rend ce fichier independant de ce que cet en-tete inclura demain.
#include "../cgmesh/material_pbr.h"

#include <cstddef>

// ---------------------------------------------------------------------------
//  Le pont entre cgpbr::MapSlot et cgre::PbrTextureUnit
// ---------------------------------------------------------------------------
// Les unites d'echantillonnage valent l'indice de l'emplacement DECALE DE DEUX,
// pour laisser au pipeline fixe les unites 0 et 1 (cf. surface_program.h). Les
// deux enumerations vivent dans des en-tetes differents et rien dans le langage
// ne les lie : sans ces assertions, une reorganisation de MapSlot ferait pointer
// chaque echantillonneur vers la mauvaise unite EN SILENCE.
//
// Le symptome serait un modele faux, pas un plantage -- une carte de rugosite
// lue comme emissive -- et AUCUN des quatre harnais ne l'attraperait : ils
// gardent le chemin non PBR, que ce decalage ne touche pas.
static constexpr int kPbrUnitOffset = 2;
static_assert (cgre::kPbrUnitBaseColor ==
               (int)cgpbr::MapSlot::base_color + kPbrUnitOffset, "unite PBR desaccordee");
static_assert (cgre::kPbrUnitNormal ==
               (int)cgpbr::MapSlot::normal + kPbrUnitOffset, "unite PBR desaccordee");
static_assert (cgre::kPbrUnitMetallicRoughness ==
               (int)cgpbr::MapSlot::metallic_roughness + kPbrUnitOffset, "unite PBR desaccordee");
static_assert (cgre::kPbrUnitOcclusion ==
               (int)cgpbr::MapSlot::occlusion + kPbrUnitOffset, "unite PBR desaccordee");
static_assert (cgre::kPbrUnitEmissive ==
               (int)cgpbr::MapSlot::emissive + kPbrUnitOffset, "unite PBR desaccordee");

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
	                 info.vboTangents,  info.iboIndices };
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
	info.vboTangents  = 0;
	info.iboIndices   = 0;
}

// ---------------------------------------------------------------------------
//  Faut-il televerser les tangentes de ce maillage ?
// ---------------------------------------------------------------------------
// DEUX CONDITIONS REUNIES, et la seconde n'est pas de la prudence : un attribut
// televerse et jamais echantillonne coute 16 octets par sommet de rendu, soit
// un tiers de plus qu'un sommet position + normale + UV, pour rien.
//
// La question se pose au MAILLAGE et non a la plage : le tampon de sommets est
// unique, et une seule plage porteuse d'une carte de normales suffit a le
// justifier.
//
// AreTangentsValid distingue des tangentes accordees a la geometrie courante de
// tangentes devenues perimees par une edition : sans elle, un maillage modifie
// rendrait un relief calcule sur une forme qui n'existe plus.
static bool MeshNeedsTangents (const Mesh* mesh)
{
	if (!mesh || !mesh->AreTangentsValid ())
		return false;

	for (unsigned int i = 0; i < mesh->GetNMaterials (); ++i)
	{
		const MaterialPbr* pbr = dynamic_cast<const MaterialPbr*> (mesh->GetMaterial (i));
		if (pbr && pbr->HasMap (cgpbr::MapSlot::normal))
			return true;
	}
	return false;
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
	// `rd.tangents` peut etre peuple sans qu'aucun materiau ne les lise : c'est
	// la demande qui decide, pas la disponibilite.
	info.hasTangents  = !rd.tangents.empty() && MeshNeedsTangents (mesh);

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

	if (info.hasTangents)
	{
		if (info.vboTangents == 0) glGenBuffers(1, &info.vboTangents);
		glBindBuffer(GL_ARRAY_BUFFER, info.vboTangents);
		glBufferData(GL_ARRAY_BUFFER, rd.tangents.size() * sizeof(float),
		             rd.tangents.data(), GL_STATIC_DRAW);
	}
	else if (info.vboTangents != 0)
	{
		// Un materiau qui perd sa carte de normales rend le tampon inutile :
		// meme regle que les trois autres tampons optionnels, il est LIBERE et non
		// conserve.
		glDeleteBuffers(1, &info.vboTangents);
		info.vboTangents = 0;
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

// ---------------------------------------------------------------------------
//  Liaison des cartes PBR, unites 2 a 6
// ---------------------------------------------------------------------------
// DEUX REGLES, et elles font qu'aucune unite n'est a defaire.
//
// 1. LES UNITES 0 ET 1 NE SONT PAS TOUCHEES : elles appartiennent au pipeline
//    fixe, qui y trouve l'albedo et la carte de reflexion, et les surcouches les
//    echantillonnent apres la boucle -- mesh_renderer ne desactive jamais
//    GL_TEXTURE_2D. Le chemin PBR commence a l'unite 2.
// 2. GL_TEXTURE_2D N'EST ACTIVE SUR AUCUNE UNITE. Sous programme lie,
//    l'activation fixe-fonction ne decide rien : c'est l'uniforme
//    d'echantillonneur qui designe l'unite. Une unite laissee LIEE mais
//    DESACTIVEE est inerte pour tout ce qui suit, alors qu'une unite laissee
//    ACTIVEE modulerait la couleur des surcouches.
//
// L'UNITE COURANTE EST RENDUE A 0 avant le retour : ActivateMaterial lie ses
// propres textures sans jamais appeler glActiveTexture, et prendrait donc
// l'unite laissee derriere nous.
//
// Une carte absente ne laisse rien lie : son uniforme de presence est faux, et
// le fragment ne l'echantillonne pas.
static void BindPbrMaps (const cgre::GlProgram* pbr,
                         const MaterialRenderer::MaterialPbrInfo& pbrInfo,
                         bool hasTexCoords, bool hasTangents)
{
	struct Binding { cgpbr::MapSlot slot; int unit; const char* sampler; const char* present; };
	static const Binding kBindings[] = {
		{ cgpbr::MapSlot::base_color,         cgre::kPbrUnitBaseColor,
		  "uBaseColorMap",         "uHasBaseColorMap"         },
		{ cgpbr::MapSlot::normal,             cgre::kPbrUnitNormal,
		  "uNormalMap",            "uHasNormalMap"            },
		{ cgpbr::MapSlot::metallic_roughness, cgre::kPbrUnitMetallicRoughness,
		  "uMetallicRoughnessMap", "uHasMetallicRoughnessMap" },
		{ cgpbr::MapSlot::emissive,           cgre::kPbrUnitEmissive,
		  "uEmissiveMap",          "uHasEmissiveMap"          },
	};

	for (const Binding& b : kBindings)
	{
		const std::size_t slot = static_cast<std::size_t> (b.slot);
		// Sans jeu d'UV televerse, aucune carte n'est echantillonnable : le
		// fragment lirait gl_TexCoord[0] non ecrit.
		//
		// La carte de NORMALES exige en plus une base tangente. Sans elle,
		// `aTangent` vaudrait l'attribut generique par defaut -- (0,0,0,1) --
		// et le repere serait degenere : la garde vaut donc aussi pour la base.
		bool present = hasTexCoords && pbrInfo.hasMap[slot];
		if (b.slot == cgpbr::MapSlot::normal && !hasTangents)
			present = false;
		pbr->SetInt (b.sampler, b.unit);
		pbr->SetInt (b.present, present ? 1 : 0);
		if (!present)
			continue;

		glActiveTexture (GL_TEXTURE0 + b.unit);
		glBindTexture (GL_TEXTURE_2D, pbrInfo.mapTex[slot]);
	}
	glActiveTexture (GL_TEXTURE0);

	const cgpbr::Factors& f = pbrInfo.factors;
	pbr->SetVec4  ("uBaseColorFactor", f.baseColor);
	pbr->SetVec3  ("uEmissiveFactor",  f.emissive);
	pbr->SetFloat ("uMetallicFactor",  f.metallic);
	pbr->SetFloat ("uRoughnessFactor", f.roughness);
	pbr->SetFloat ("uNormalScale",     f.normalScale);
}

void VBOManager::DrawMaterialGroups (int id, const std::vector<int>& rendererIds,
                                     const SurfaceDrawState& state)
{
	auto it = m_mapVBO.find(id);
	if (it == m_mapVBO.end())
		return;
	vboInfo& info = it->second;

	// Re-upload if the geometry changed (revision) OR the shading mode changed
	// (flat vs smooth uses a different vertex/normal layout). Done first so the
	// material-less fallback below also sees the up-to-date buffers.
	if (info.pMesh && (info.pMesh->GetRevision() != info.revision || info.flat != state.flat))
		uploadMesh(info.pMesh, info, state.flat);

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

	// LE TABLEAU DE COULEURS SUIT LE MODE D'OMBRAGE, PAS LA DONNEE.
	//
	// `info.hasColors` ne distingue rien : Mesh::InitVertices remplit
	// m_vertexColors d'un gris 0,5 pour TOUT maillage, et hasColors n'est que la
	// non-vacuite de ce tableau. Le lier sur ce seul critere revient a le lier
	// toujours, et fait prendre au pipeline fixe une couleur que le mode ne
	// demande pas -- le tableau PRIME sur glColor des que GL_COLOR_MATERIAL est
	// actif, et il est lu directement quand l'eclairage est eteint. Le programme
	// de surface, lui, n'honore le tableau que si uUseVertexColors : les deux
	// chemins divergeaient sur la meme donnee.
	//
	// Meme condition que l'uniforme pose plus bas : les deux chemins LIENT
	// desormais le tableau dans exactement les memes cas. Ils ne le LISENT pas
	// pour autant dans les memes cas -- le fixe l'ignore sous
	// glDisable(GL_COLOR_MATERIAL) et eclairage allume, ce qui est precisement
	// l'inertie par laquelle ce changement ne deplace aucun pixel sous eclairage.
	const bool useColorArray = (info.hasColors && state.useVertexColors);

	if (useColorArray)
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
	if (state.useMeshMaterials)
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
	// Programme COURANT, suivi explicitement : la boucle en change quand elle
	// rencontre un materiau PBR, et doit revenir au programme de surface ensuite.
	// Les uniformes sont un etat du PROGRAMME, pas du contexte : ceux poses avant
	// la boucle survivent a un aller-retour, il n'y a rien a reposer.
	const cgre::GlProgram* current = prog;
	// Le tableau de tangentes n'est active qu'a la premiere plage PBR qui le
	// reclame, et desactive une fois apres la boucle : un tableau d'attribut
	// generique laisse actif serait lu par tout dessin suivant.
	bool tangentArrayOn = false;
	// Resolue une fois : glGetAttribLocation est une recherche par chaine cote
	// pilote, a ne pas refaire par plage.
	const int tangentLoc = cgre::PbrTangentAttribLocation ();

	// LE TABLEAU GENERIQUE SE DESACTIVE AU BASCULEMENT DE PROGRAMME, pas
	// seulement en fin de boucle. En profil de compatibilite, la specification
	// n'exclut pas que les attributs generiques d'indice >= 1 soient ALIASES sur
	// les attributs conventionnels : laisse actif pendant que le programme de
	// surface dessine, le tampon de tangentes alimenterait gl_Normal ou
	// gl_MultiTexCoord0. Le materiel de mesure n'aliase pas -- ce qui ne
	// contraint que lui.
	auto disableTangentArray = [&]() {
		if (tangentArrayOn && tangentLoc >= 1)
		{
			glDisableVertexAttribArray ((GLuint)tangentLoc);
			tangentArrayOn = false;
		}
	};
	if (prog)
	{
		prog->Use ();
		// Unites d'echantillonnage, fixees une fois : ce sont celles auxquelles
		// MaterialRenderer::ActivateMaterial lie deja ses textures.
		prog->SetInt ("uAlbedo", 0);
		prog->SetInt ("uReflection", 1);
		// Meme condition que la liaison du tableau de couleurs ci-dessus : le
		// mode « couleurs par sommet » n'a de sens que si le maillage en porte.
		prog->SetInt ("uUseVertexColors", useColorArray ? 1 : 0);
		prog->SetInt ("uLighting", state.lighting ? 1 : 0);
		// Etat par defaut, valable pour le materiau neutre comme pour celui par
		// defaut : ni texture ni reflet. Chaque plage qui en a le corrige.
		prog->SetInt   ("uUseTexture", 0);
		prog->SetInt   ("uUseReflection", 0);
		prog->SetFloat ("uReflAmount", 0.f);
	}

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups (materiau de base)");

	for (const Mesh::MaterialRange& r : info.materialRanges)
	{
		const bool activated =
		    state.useMeshMaterials &&
		    r.materialId != MATERIAL_NONE &&
		    r.materialId < rendererIds.size() &&
		    rendererIds[r.materialId] != -1;

		if (activated)
		{
			MaterialRenderer::getInstance()->ActivateMaterial(rendererIds[r.materialId]);

			// CHEMIN PBR quand le materiau en est un ET que son programme se
			// construit. Le programme est demande ICI, au premier materiau PBR
			// rencontre : une session qui n'en croise aucun ne le compile jamais,
			// donc son rendu ne peut pas dependre de lui.
			const MaterialRenderer::MaterialPbrInfo pbrInfo =
				MaterialRenderer::getInstance()->GetPbrInfo (rendererIds[r.materialId]);
			const cgre::GlProgram* pbr =
				(prog && pbrInfo.isPbr) ? cgre::PbrProgram () : nullptr;

			if (pbr)
			{
				if (current != pbr) { pbr->Use (); current = pbr; }

				// TABLEAU DE TANGENTES, active a la PREMIERE plage PBR qui en a
				// besoin et desactive apres la boucle. La location est celle que
				// le compilateur a choisie, jamais 0 -- cf. PbrTangentAttribLocation.
				const bool useTangents =
					info.hasTangents && info.vboTangents != 0 && tangentLoc >= 1;
				if (useTangents && !tangentArrayOn)
				{
					glBindBuffer (GL_ARRAY_BUFFER, info.vboTangents);
					glVertexAttribPointer ((GLuint)tangentLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
					glEnableVertexAttribArray ((GLuint)tangentLoc);
					tangentArrayOn = true;
				}

				BindPbrMaps (pbr, pbrInfo, info.hasTexCoords, useTangents);
				pbr->SetInt ("uLighting", state.lighting ? 1 : 0);
				// Canal d'inspection : etat GLOBAL du moteur, pose comme les
				// autres uniformes a chaque plage parce qu'un uniforme n'est
				// pose que sur le programme lie. `off` hors mesure.
				pbr->SetInt ("uChannel", (int) cgre::GetPbrChannel ());

				// ENVIRONNEMENT. Pose ici et non dans le bloc d'uniformes qui
				// precede la boucle : ce bloc lie le programme DE SURFACE, alors
				// que le programme PBR n'est obtenu qu'ici, paresseusement, a la
				// premiere plage PBR rencontree. Un uniforme ne se pose que sur
				// le programme lie.
				const cgre::PbrEnvironment env = cgre::GetPbrEnvironment ();
				pbr->SetInt   ("uEnvEnabled", env.enabled ? 1 : 0);
				pbr->SetFloat ("uEnvSky",     env.sky);
				pbr->SetFloat ("uEnvGround",  env.ground);
			}
			else if (prog)
			{
				// Le programme PBR ne s'est pas construit, ou le materiau n'en
				// est pas un : la projection de Phong reste le chemin, et c'est
				// elle que ActivateMaterial vient d'installer.
				if (current != prog) { disableTangentArray (); prog->Use (); current = prog; }

				// Sous programme lie, glEnable(GL_TEXTURE_2D) ne decide plus
				// rien : c'est l'uniforme qui dit au fragment s'il doit
				// echantillonner.
				const MaterialRenderer::MaterialGlInfo mi =
					MaterialRenderer::getInstance()->GetGlInfo (rendererIds[r.materialId]);
				prog->SetInt   ("uUseTexture", (mi.hasTexture && info.hasTexCoords) ? 1 : 0);
				prog->SetInt   ("uUseReflection", mi.hasReflection ? 1 : 0);
				prog->SetFloat ("uReflAmount", mi.reflAmount);
			}
		}
		else if (state.useMeshMaterials)
		{
			// UNE PLAGE SANS MATERIAU EST UNE PLAGE A ETAT, elle aussi : sans ce
			// bloc elle garde la texture liee, les uniformes et les canaux de
			// materiau de la plage precedente. Les plages etant triees par
			// identifiant et MATERIAL_NONE valant (unsigned)-1, c'est la DERNIERE
			// du maillage -- elle heritait donc systematiquement du dernier
			// materiau dessine.
			//
			// L'ETAT EFFECTIF repose est celui d'avant la boucle. Trois etats
			// inertes ne le sont pas et n'ont pas a l'etre : la texture reste
			// liee sur l'unite 0 mais GL_TEXTURE_2D est desactive, le mode de
			// glColorMaterial subsiste mais la fonctionnalite aussi est
			// desactivee, et l'unite 1 garde sa grille GL_COMBINE alors qu'elle
			// n'echantillonne plus. Aucun de ces trois n'atteint un fragment.
			//
			// Hors du mode « materiaux » rien n'a perturbe l'etat, d'ou la garde.
			MaterialRenderer::ActivateDefaultMaterial ();
			if (prog)
			{
				if (current != prog) { disableTangentArray (); prog->Use (); current = prog; }
				prog->SetInt   ("uUseTexture", 0);
				prog->SetInt   ("uUseReflection", 0);
				prog->SetFloat ("uReflAmount", 0.f);
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
	disableTangentArray ();

	if (current) cgre::GlProgram::Unuse ();

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups (boucle de dessin)");

	glDisable(GL_POLYGON_OFFSET_FILL);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if (info.hasTexCoords) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (useColorArray)     glDisableClientState(GL_COLOR_ARRAY);
	if (info.hasNormals)   glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	CGRE_CHECK_GL ("VBOManager::DrawMaterialGroups");
}
