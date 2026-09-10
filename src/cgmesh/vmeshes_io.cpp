#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "vmeshes_io.h"
#include "vmeshes.h"
#include "voxels.h"
#include "voxels_import_kvx.h"
#include "voxels_import_nbt.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "material_pbr.h"
#include "mesh_io_gltf_pbr.h"
#include "tangents.h"
#include "mesh_io_rply.h"
#include "mesh_io_3ds.h"

bool VMeshesIO::save(VMeshes& vm, const char *filename)
{
	bool res = false;

	// determine the format
	int size = strlen(filename);
	if (size < 4) return false;

	// obj
	if (filename[size - 3] == 'o' && filename[size - 2] == 'b' && filename[size - 1] == 'j')
		res = export_obj(vm, filename);
	// stl : BINAIRE.
	//
	// C'est le STL de fait -- compact (50 o/triangle contre ~250 en ASCII), lu
	// partout, et sans la perte de precision du formatage %g. import_stl detecte
	// automatiquement les deux variantes, donc l'aller-retour reste assure.
	//
	// export_stl (ASCII) reste implemente mais n'a plus d'appelant : il etait
	// jusqu'ici le seul export atteignable de VMeshesIO, alors que la variante
	// binaire -- ecrite, testee, meilleure -- ne l'etait pas. A exposer si un choix
	// de variante devient necessaire.
	else if (filename[size - 3] == 's' && filename[size - 2] == 't' && filename[size - 1] == 'l')
		res = export_stl_binary(vm, filename);

	return res;
}

bool VMeshesIO::load(VMeshes& vm, const char* filename)
{
	bool res = false;

	std::string fileStr(filename);
	std::string ext = "";
	size_t dotPos = fileStr.find_last_of(".");
	if (dotPos != std::string::npos) {
		ext = fileStr.substr(dotPos + 1);
		for (auto& c : ext) c = tolower(c);
	}

	// obj (split each object 'o'/'g' into its own Mesh)
	if (ext == "obj")
		res = import_obj(vm, filename);

	// 3ds
	if (ext == "3ds")
		res = import_3ds(vm, filename);

	// 3dm
	if (ext == "3dm")
		res = import_3dm(vm, filename);

	// 3mf (via lib3mf, gated on CG_HAS_LIB3MF)
	if (ext == "3mf")
		res = import_3mf(vm, filename);

	// gltf
	if (ext == "gltf")
		res = import_gltf(vm, filename);

	// glb
	if (ext == "glb")
		res = import_gltf(vm, filename);

	// step / stp (via OpenCASCADE, gated on CG_HAS_OCCT)
	if (ext == "step" || ext == "stp")
		res = import_step(vm, filename);

	// iges / igs (via OpenCASCADE, gated on CG_HAS_OCCT)
	if (ext == "iges" || ext == "igs")
		res = import_iges(vm, filename);

	// kvx (Ken Silverman voxel model): decode the voxel grid, then triangulate
	// its activated surface into a mesh (geometry only; palette not yet mapped).
	if (ext == "kvx")
	{
		Voxels* vox = loadkvx(const_cast<char*>(filename));
		if (vox)
		{
			Mesh* pMesh = vox->ToMesh();
			delete vox;
			if (pMesh)
			{
				vm.AddMesh(pMesh);
				res = true;
			}
		}
	}

	// nbt (Minecraft "structure block" voxel model): decode the block grid, then
	// triangulate its activated surface into a mesh (per-material vertex colours).
	if (ext == "nbt")
	{
		Voxels* vox = loadnbt(const_cast<char*>(filename));
		if (vox)
		{
			Mesh* pMesh = vox->ToMesh();
			delete vox;
			if (pMesh)
			{
				vm.AddMesh(pMesh);
				res = true;
			}
		}
	}

	if (res)
		return res;

	Mesh* pMesh = new Mesh();
	if (pMesh->load(filename) == 0)
	{
		vm.AddMesh(pMesh);
		return true;
	}
	delete pMesh;   // load failed: don't leak the throw-away mesh
	return false;
}

bool VMeshesIO::export_obj(VMeshes& vm, const char* filename)
{
	return false;
}

namespace
{
	// Read a text file into lines (for the light object-boundary pass).
	bool objReadLines(const char* filename, std::vector<std::string>& lines)
	{
		FILE* fp = fopen(filename, "r");
		if (!fp) return false;
		char buf[4096];
		while (fgets(buf, sizeof(buf), fp))
			lines.emplace_back(buf);
		fclose(fp);
		return true;
	}

	// First whitespace-delimited token of a line.
	std::string objFirstToken(const std::string& line)
	{
		size_t i = 0, n = line.size();
		while (i < n && isspace((unsigned char)line[i])) i++;
		size_t j = i;
		while (j < n && !isspace((unsigned char)line[j])) j++;
		return line.substr(i, j - i);
	}

	// Trimmed remainder after the first token (used as the object name).
	std::string objRestAfterToken(const std::string& line)
	{
		size_t i = 0, n = line.size();
		while (i < n && isspace((unsigned char)line[i])) i++;
		while (i < n && !isspace((unsigned char)line[i])) i++;   // skip token
		while (i < n && isspace((unsigned char)line[i])) i++;    // skip ws
		size_t end = line.size();
		while (end > i && isspace((unsigned char)line[end - 1])) end--;
		return line.substr(i, end - i);
	}

	// Deep-copy a Mesh material. A Mesh owns its materials, so submeshes need
	// their own copies.
	//
	// Material::clone() plutot qu'un switch sur GetType() : un switch rendait
	// nullptr pour toute sous-classe qu'il ne connaissait pas.
	Material* objCloneMaterial(Material* m)
	{
		return m ? m->clone().release() : nullptr;
	}

	// Parse the vertex refs of an OBJ 'l'/'p' element line into resolved 0-based
	// global vertex indices (any "/vt/vn" suffix ignored). runningVerts is the
	// number of 'v' declared so far, for OBJ negative (relative) indices.
	void objParseElementRefs(const std::string& line, int runningVerts, std::vector<int>& out)
	{
		const char* s = line.c_str();
		while (*s && !isspace((unsigned char)*s)) s++;   // skip the 'l'/'p' token
		while (*s)
		{
			while (*s && isspace((unsigned char)*s)) s++;
			if (!*s) break;
			int idx = 0;
			if (sscanf(s, "%d", &idx) == 1)
			{
				if (idx < 0) idx = runningVerts + idx; else idx--;
				if (idx >= 0 && idx < runningVerts)
					out.push_back(idx);
			}
			while (*s && !isspace((unsigned char)*s)) s++;
		}
	}
}

//
// Import an OBJ, splitting each object into its own Mesh. Strategy:
//   1. Parse the whole file into ONE flattened Mesh via the existing, tested
//      single-mesh path (Mesh::load -> MeshIO::import_obj): this resolves all
//      vertices, per-corner UVs, negative indices, mtllib/usemtl materials.
//   2. Light second pass over the file to tag each face (in file order) with
//      the object it belongs to ('o' delimits objects; 'g' is used only when
//      the file declares no 'o'), plus per-object line/point elements.
//   3. Rebuild one Mesh per object, re-indexing its vertices / UVs to a local
//      pool and cloning only the materials it uses.
// A file with 0 or 1 object keeps the flattened mesh as-is (no remap).
//
bool VMeshesIO::import_obj(VMeshes& vm, const char* filename)
{
	if (!filename) return false;

	// 1. Full parse into a single flattened Mesh.
	Mesh* flat = new Mesh();
	if (flat->load(filename) != 0)
	{
		delete flat;
		return false;
	}

	// 2. Light pass: object boundaries + per-object line/point elements.
	std::vector<std::string> lines;
	if (!objReadLines(filename, lines))
	{
		delete flat;
		return false;
	}

	bool hasO = false;
	for (const std::string& ln : lines)
		if (objFirstToken(ln) == "o") { hasO = true; break; }
	const std::string delim = hasO ? "o" : "g";

	std::vector<std::string> objNames;
	std::vector<int> faceObject;                            // face (file order) -> object
	std::vector<std::vector<std::vector<int>>> objPolylines; // [obj][polyline][refs]
	std::vector<std::vector<int>> objPoints;                // [obj][refs]
	int curObj = -1;
	int runningVerts = 0;

	auto ensureDefaultObject = [&]() {
		if (curObj < 0)
		{
			curObj = (int)objNames.size();
			objNames.emplace_back("default");
			objPolylines.emplace_back();
			objPoints.emplace_back();
		}
	};

	for (const std::string& ln : lines)
	{
		std::string tok = objFirstToken(ln);
		if (tok == delim)
		{
			curObj = (int)objNames.size();
			std::string name = objRestAfterToken(ln);
			objNames.emplace_back(name.empty() ? ("object_" + std::to_string(curObj)) : name);
			objPolylines.emplace_back();
			objPoints.emplace_back();
		}
		else if (tok == "v")
			runningVerts++;
		else if (tok == "f")
		{
			ensureDefaultObject();
			faceObject.push_back(curObj);
		}
		else if (tok == "l")
		{
			ensureDefaultObject();
			std::vector<int> refs;
			objParseElementRefs(ln, runningVerts, refs);
			if (refs.size() >= 2) objPolylines[curObj].push_back(refs);
		}
		else if (tok == "p")
		{
			ensureDefaultObject();
			std::vector<int> refs;
			objParseElementRefs(ln, runningVerts, refs);
			for (int r : refs) objPoints[curObj].push_back(r);
		}
	}

	int nObjects = (int)objNames.size();

	// Safety: the face count must match the flattened mesh. If parsing drifted,
	// fall back to the single-mesh behaviour rather than mis-assign faces.
	if ((unsigned int)faceObject.size() != flat->GetNFaces ())
		nObjects = (nObjects <= 1) ? nObjects : 0;

	// 3a. Zero/one object -> keep the flattened mesh (fast path, no remap).
	if (nObjects <= 1)
	{
		if (nObjects == 1) flat->SetName (objNames[0]);
		vm.AddMesh(flat);
		return true;
	}

	// 3b. One submesh per object.
	for (int obj = 0; obj < nObjects; obj++)
	{
		std::vector<unsigned int> faces;
		for (unsigned int fi = 0; fi < flat->GetNFaces (); fi++)
			if (faceObject[fi] == obj)
				faces.push_back(fi);

		if (faces.empty() && objPolylines[obj].empty() && objPoints[obj].empty())
			continue;   // object with no geometry -> no mesh

		// vertex remap (flat global index -> local, first-seen order)
		std::map<int, int> vmap;
		auto localVert = [&](int g) -> int {
			auto it = vmap.find(g);
			if (it != vmap.end()) return it->second;
			int local = (int)vmap.size();
			vmap[g] = local;
			return local;
		};
		for (unsigned int fi : faces)
		{
			auto f = flat->FaceAt (fi);
			for (int c = 0; c < f->GetNVertices(); c++)
				localVert(f->GetVertex(c));
		}
		for (auto& pl : objPolylines[obj]) for (int g : pl) localVert(g);
		for (int g : objPoints[obj]) localVert(g);

		// uv remap
		std::map<int, int> uvmap;
		auto localUV = [&](int g) -> int {
			auto it = uvmap.find(g);
			if (it != uvmap.end()) return it->second;
			int local = (int)uvmap.size();
			uvmap[g] = local;
			return local;
		};
		bool anyUV = false;
		for (unsigned int fi : faces)
		{
			auto f = flat->FaceAt (fi);
			if (f->UsesTextureCoordinates () && f->HasTexCoordIndices ())
			{
				anyUV = true;
				for (int c = 0; c < f->GetNVertices(); c++)
					localUV((int)f->GetTexCoordIndex (c));
			}
		}

		Mesh* sub = new Mesh();
		sub->Init((unsigned int)vmap.size(), (unsigned int)faces.size());
		sub->SetName (objNames[obj]);

		for (auto& kv : vmap)
		{
			int g = kv.first, l = kv.second;
			sub->SetVertexComponent (l, 0, flat->GetVertices ()[3 * g]);
			sub->SetVertexComponent (l, 1, flat->GetVertices ()[3 * g + 1]);
			sub->SetVertexComponent (l, 2, flat->GetVertices ()[3 * g + 2]);
		}

		if (anyUV && !uvmap.empty())
		{
			unsigned int nUV = (unsigned int)uvmap.size();
			std::vector<float> subUV (2 * (size_t)nUV, 0.0f);
			for (auto& kv : uvmap)
			{
				int g = kv.first, l = kv.second;
				if (2u * (unsigned int)g + 1u < flat->GetTextureCoordinates ().size())
				{
					subUV[2 * l]     = flat->GetTextureCoordinates ()[2 * g];
					subUV[2 * l + 1] = flat->GetTextureCoordinates ()[2 * g + 1];
				}
			}
			sub->SetTextureCoordinates (std::move (subUV), nUV);
		}

		// materials actually used by this object (cloned; only the used ones)
		std::map<int, int> matmap;
		auto localMat = [&](unsigned int gm) -> unsigned int {
			if (gm >= flat->GetNMaterials()) return MATERIAL_NONE;
			auto it = matmap.find((int)gm);
			if (it != matmap.end()) return (unsigned int)it->second;
			Material* copy = objCloneMaterial(flat->GetMaterial(gm));
			if (!copy) return MATERIAL_NONE;
			unsigned int id = sub->Material_Add(copy);
			matmap[(int)gm] = (int)id;
			return id;
		};

		for (unsigned int k = 0; k < (unsigned int)faces.size(); k++)
		{
			auto src = flat->FaceAt (faces[k]);
			auto dst = sub->FaceAt (k);
			int nv = src->GetNVertices();
			dst->SetNVertices((unsigned int)nv);
			for (int c = 0; c < nv; c++)
				dst->SetVertex((unsigned int)c, (unsigned int)localVert(src->GetVertex(c)));

			if (src->UsesTextureCoordinates () && src->HasTexCoordIndices ())
			{
				dst->SetUsesTextureCoordinates (true);
				dst->ActivateTextureCoordinatesIndices();
				for (int c = 0; c < nv; c++)
					dst->SetTexCoord((unsigned int)c,
					                 (unsigned int)localUV((int)src->GetTexCoordIndex (c)));
			}

			dst->SetMaterialId(localMat(src->GetMaterialId ()));
		}

		for (auto& pl : objPolylines[obj])
			for (size_t i = 1; i < pl.size(); i++)
			{
				sub->AddLine((unsigned int)localVert(pl[i - 1]), (unsigned int)localVert(pl[i]));
			}
		for (int g : objPoints[obj])
			sub->AddPoint((unsigned int)localVert(g));

		sub->ComputeNormals();
		vm.AddMesh(sub);
	}

	delete flat;
	return true;
}

//
// Helpers to derive the solid name from a path : strip directories and extension.
//
namespace
{
	std::string stemFromPath(const char *filename)
	{
		std::string s(filename ? filename : "");
		size_t slash = s.find_last_of("/\\");
		if (slash != std::string::npos) s = s.substr(slash + 1);
		size_t dot = s.find_last_of('.');
		if (dot != std::string::npos) s = s.substr(0, dot);
		if (s.empty()) s = "vmeshes";
		return s;
	}

	struct Tri
	{
		float ax, ay, az, bx, by, bz, cx, cy, cz;
		float nx, ny, nz;
	};

	// Collect every triangle of every Mesh in `meshes` into a flat list, with
	// pre-computed normals. Out-of-range indices are skipped.
	//
	// Passe par Mesh::GetTriangles(), qui TRIANGULE les faces a plus de trois
	// sommets (eventail pour les convexes, glutess pour les concaves).
	//
	// L'ancienne version parcourait m_pFaces et faisait `if (GetNVertices() != 3)
	// continue;` : toute face non triangulaire etait IGNOREE en silence, et le STL
	// sortait incomplet sans le moindre avertissement. Ce n'etait pas theorique --
	// CreateCube() a bTri=false par defaut, donc produit des quads, et c'est la
	// scene initiale de sinaia : l'enregistrer en STL donnait un fichier SANS
	// AUCUNE facette.
	std::vector<Tri> collectTriangles(const std::vector<Mesh*> &meshes)
	{
		std::vector<Tri> tris;
		for (Mesh *m : meshes)
		{
			if (!m) continue;
			const std::vector<unsigned int> idx = m->GetTriangles();
			for (size_t k = 0; k + 2 < idx.size(); k += 3)
			{
				const unsigned int a = idx[k], b = idx[k+1], c = idx[k+2];
				if (a >= m->GetNVertices () || b >= m->GetNVertices () || c >= m->GetNVertices ()) continue;

				Tri t;
				t.ax = m->GetVertices ()[3*a];   t.ay = m->GetVertices ()[3*a+1]; t.az = m->GetVertices ()[3*a+2];
				t.bx = m->GetVertices ()[3*b];   t.by = m->GetVertices ()[3*b+1]; t.bz = m->GetVertices ()[3*b+2];
				t.cx = m->GetVertices ()[3*c];   t.cy = m->GetVertices ()[3*c+1]; t.cz = m->GetVertices ()[3*c+2];

				float ux = t.bx - t.ax, uy = t.by - t.ay, uz = t.bz - t.az;
				float vx = t.cx - t.ax, vy = t.cy - t.ay, vz = t.cz - t.az;
				t.nx = uy*vz - uz*vy;
				t.ny = uz*vx - ux*vz;
				t.nz = ux*vy - uy*vx;
				float len = std::sqrt(t.nx*t.nx + t.ny*t.ny + t.nz*t.nz);
				if (len > 1e-12f) { t.nx /= len; t.ny /= len; t.nz /= len; }
				else { t.nx = 0.0f; t.ny = 0.0f; t.nz = 1.0f; }
				tris.push_back(t);
			}
		}
		return tris;
	}
}

//
// Export every Mesh's triangles as a single ASCII STL solid (concatenation).
//
bool VMeshesIO::export_stl(VMeshes& vm, const char* filename)
{
	if (!filename) return false;
	FILE *fp = fopen(filename, "w");
	if (!fp) return false;

	std::string name = stemFromPath(filename);
	std::vector<Tri> tris = collectTriangles(vm.GetMeshes());

	fprintf(fp, "solid %s\n", name.c_str());
	for (const Tri &t : tris)
	{
		fprintf(fp, "facet normal %g %g %g\n", t.nx, t.ny, t.nz);
		fprintf(fp, "  outer loop\n");
		fprintf(fp, "    vertex %g %g %g\n", t.ax, t.ay, t.az);
		fprintf(fp, "    vertex %g %g %g\n", t.bx, t.by, t.bz);
		fprintf(fp, "    vertex %g %g %g\n", t.cx, t.cy, t.cz);
		fprintf(fp, "  endloop\n");
		fprintf(fp, "endfacet\n");
	}
	fprintf(fp, "endsolid %s\n", name.c_str());
	fclose(fp);
	return true;
}

//
// Export every Mesh's triangles as a single binary STL solid.
//
bool VMeshesIO::export_stl_binary(VMeshes& vm, const char* filename)
{
	if (!filename) return false;
	FILE *fp = fopen(filename, "wb");
	if (!fp) return false;

	char header[80] = {0};
	{
		std::string name = stemFromPath(filename);
		// Binary fixed-width copy: clamp and memcpy. Remaining bytes stay
		// NUL from the zero-init above. The STL header is binary, so the
		// missing trailing NUL that static analyzers flag on strncpy is a
		// non-issue here — but memcpy makes the intent explicit.
		const size_t n = name.size() < sizeof(header) ? name.size() : sizeof(header);
		std::memcpy(header, name.data(), n);
	}
	fwrite(header, 1, 80, fp);

	std::vector<Tri> tris = collectTriangles(vm.GetMeshes());
	uint32_t nTri = (uint32_t)tris.size();
	fwrite(&nTri, sizeof(uint32_t), 1, fp);

	for (const Tri &t : tris)
	{
		float n[3] = { t.nx, t.ny, t.nz };
		float v[9] = { t.ax, t.ay, t.az, t.bx, t.by, t.bz, t.cx, t.cy, t.cz };
		uint16_t attr = 0;
		fwrite(n, sizeof(float), 3, fp);
		fwrite(v, sizeof(float), 9, fp);
		fwrite(&attr, sizeof(uint16_t), 1, fp);
	}
	fclose(fp);
	return true;
}

bool VMeshesIO::export_ply(VMeshes& vm, const char* filename)
{
	return false;
}

namespace {

// Helpers building 4x4 transforms in the column-vector convention (w = M * v),
// in native 3DS coordinates (Z-up). See VMeshesIO::import_3ds.
Matrix4f Kf_Translation(float tx, float ty, float tz)
{
	return Matrix4f(1,0,0,tx, 0,1,0,ty, 0,0,1,tz, 0,0,0,1);
}

Matrix4f Kf_Scale(float sx, float sy, float sz)
{
	return Matrix4f(sx,0,0,0, 0,sy,0,0, 0,0,sz,0, 0,0,0,1);
}

// Rodrigues axis-angle rotation. 3DS stores the rotation with the opposite
// sign to the right-hand rule, so callers pass the negated angle.
Matrix4f Kf_Rotation(float angle, const float axis[3])
{
	float x = axis[0], y = axis[1], z = axis[2];
	float len = std::sqrt(x*x + y*y + z*z);
	if (len < 1e-12f || std::fabs(angle) < 1e-9f)
		return Matrix4f(); // identity
	x /= len; y /= len; z /= len;
	float c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
	return Matrix4f(
		t*x*x + c,   t*x*y - s*z, t*x*z + s*y, 0,
		t*x*y + s*z, t*y*y + c,   t*y*z - s*x, 0,
		t*x*z - s*y, t*y*z + s*x, t*z*z + c,   0,
		0, 0, 0, 1);
}

} // namespace

bool VMeshesIO::import_3ds(VMeshes& vm, const char* filename)
{
	t3DSModel* p = Load3DSFile(filename, nullptr);
	if (!p) return false;

	// 3DS materials reference texture files by bare name; they live next to the
	// model. Remember the model's directory so MaterialTexture can find them.
	std::string modelDir;
	{
		std::string f(filename ? filename : "");
		size_t slash = f.find_last_of("/\\");
		if (slash != std::string::npos)
			modelDir = f.substr(0, slash);
	}

	// Build the world matrix of every keyframer node (frame-0 pose). Parents
	// always precede their children in the file, so a single forward pass
	// accumulates parent * local correctly.
	std::vector<Matrix4f> nodeWorld(p->pKfNodes.size());
	for (size_t i = 0; i < p->pKfNodes.size(); ++i)
	{
		const t3DSKfNode& n = p->pKfNodes[i];
		Matrix4f local = Kf_Translation(n.pos[0], n.pos[1], n.pos[2])
		               * (n.hasRot ? Kf_Rotation(-n.rotAngle, n.rotAxis) : Matrix4f())
		               * Kf_Scale(n.scale[0], n.scale[1], n.scale[2]);
		if (n.parent >= 0 && n.parent < (int)i)
			nodeWorld[i] = nodeWorld[n.parent] * local;
		else
			nodeWorld[i] = local;
	}

	for (auto& object : p->pObject)
	{
		auto pMesh = new Mesh();

		unsigned int nVertices = object.numOfVerts;
		unsigned int nFaces = object.numOfFaces;
		pMesh->Init(nVertices, nFaces);

		// Find the keyframer node driving this object (matched by name).
		int nodeIdx = -1;
		for (size_t i = 0; i < p->pKfNodes.size(); ++i)
			if (strcmp(p->pKfNodes[i].strName, object.strName) == 0)
			{
				nodeIdx = (int)i;
				break;
			}

		// 3DS stores mesh vertices in world space together with a per-object
		// mesh matrix (LocalCoordinateSystem) and keyframer node transform. The
		// part is placed by: v' = nodeWorld * T(-pivot) * inverse(meshMatrix) * v
		// (lib3ds convention). Both the vertices and the mesh matrix were Y/Z
		// swapped on read for the engine's Y-up frame; we undo that to compute
		// in native 3DS space, then swap the result back.
		//
		// Files without a keyframer node for this object keep the legacy
		// behaviour (vertices used as-is): meshMatrixOk stays false.
		Matrix4f display;       // identity by default
		bool useTransform = false;
		if (nodeIdx >= 0)
		{
			const auto& L = object.LocalCoordinateSystem; // stored row (a,b,c) -> raw (a,-c,b)
			Matrix4f meshMatrix(
				L[0][0], L[1][0], L[2][0], L[3][0],
				-L[0][2], -L[1][2], -L[2][2], -L[3][2],
				L[0][1], L[1][1], L[2][1], L[3][1],
				0, 0, 0, 1);
			Matrix4f meshInv;
			if (meshMatrix.GetInverse(meshInv))
			{
				const t3DSKfNode& n = p->pKfNodes[nodeIdx];
				display = nodeWorld[nodeIdx]
				        * Kf_Translation(-n.pivot[0], -n.pivot[1], -n.pivot[2])
				        * meshInv;
				useTransform = true;
			}
		}

		for (unsigned int i = 0; i < nVertices; i++)
		{
			if (useTransform)
			{
				// Undo the engine Y/Z swap to recover native 3DS coords.
				float ex = object.pVerts[i].fX;
				float ey = object.pVerts[i].fY;
				float ez = object.pVerts[i].fZ;
				TVector4<float> v(ex, -ez, ey, 1.0f);
				TVector4<float> w = display * v;
				// Re-apply the swap on the assembled world position.
				pMesh->SetVertexComponent (i, 0, w.x);
				pMesh->SetVertexComponent (i, 1, w.z);
				pMesh->SetVertexComponent (i, 2, -w.y);
			}
			else
			{
				pMesh->SetVertexComponent (i, 0, object.pVerts[i].fX);
				pMesh->SetVertexComponent (i, 1, object.pVerts[i].fY);
				pMesh->SetVertexComponent (i, 2, object.pVerts[i].fZ);
			}
		}

		// Load normals if present
		if (object.pNormals)
		{
			for (unsigned int i = 0; i < nVertices; i++)
			{
				pMesh->SetVertexNormal (i, object.pNormals[i].fX, object.pNormals[i].fY, object.pNormals[i].fZ);
			}
		}

		// INDICES VALIDES CONTRE LE NOMBRE DE SOMMETS.
		//
		// `vertIndex` vient du fichier 3DS sans aucun controle (un `unsigned short`
		// lu tel quel, cf. ReadVertexIndices_3DS) : rien ne garantit qu'il designe
		// un sommet de CET objet. Un indice hors bornes traverse ensuite tout le
		// module -- ComputeNormals ecrit dans m_vertexNormals[3*k] et incremente
		// nfaces[k] sans borne, donc une ecriture hors bornes a offset choisi par
		// le fichier.
		//
		// Le lecteur OBJ valide deja de cette maniere (mesh_io_obj.cpp) ; la faille
		// etait propre au chemin 3DS. Une face fautive est ECARTEE et signalee, et
		// non rabattue sur le sommet 0 : rabattre fabriquerait une face degeneree
		// silencieuse, que le diagnostic topologique attribuerait au maillage.
		//
		// DEUX PASSES, et non un filtrage en place : SetNFaces DETRUIT les faces
		// existantes pour en creer des neuves (mesh.cpp), donc le retaillage doit
		// preceder le remplissage.
		auto faceIsValid = [&](const t3DSFace& f) {
			for (unsigned int j = 0; j < 3; j++)
				if (f.vertIndex[j] < 0 || (unsigned int) f.vertIndex[j] >= nVertices)
					return false;
			return true;
		};

		unsigned int nValid = 0;
		for (unsigned int i = 0; i < nFaces; i++)
			if (faceIsValid (object.pFaces[i]))
				++nValid;

		if (nValid != nFaces)
		{
			fprintf (stderr, "import_3ds: %u face(s) sur %u ecartee(s), indice de "
			                 "sommet hors bornes (nv=%u)\n",
			         nFaces - nValid, nFaces, nVertices);
			pMesh->SetNFaces (nValid);
		}

		unsigned int nKept = 0;
		for (unsigned int i = 0; i < nFaces; i++)
		{
			const t3DSFace& face = object.pFaces[i];
			if (!faceIsValid (face))
				continue;

			pMesh->FaceAt (nKept)->SetNVertices(3);
			for (unsigned int j = 0; j < 3; j++)
				pMesh->FaceAt (nKept)->SetVertex(j, face.vertIndex[j]);
			++nKept;
		}

		pMesh->SetName (std::string(object.strName));

		// Texture coordinates. 3DS stores one UV per vertex (parallel to the
		// position array), which is exactly the per-vertex layout the VBO /
		// polygon render path expects. Populate the mesh-level UV array, and
		// mirror the indices onto each face for the immediate-mode path.
		if (object.pTexVerts && object.numTexVertex > 0)
		{
			const unsigned int nUV = (unsigned int)object.numTexVertex;
			// ⚠ Le COMPTE reste a zero sur ce chemin alors que le tableau est
			// renseigne. Epingle par tu_cgmesh_io ; cf. GetNTextureCoordinates()
			// dans mesh.h.
			std::vector<float> uv3ds (2 * (size_t)nVertices, 0.0f);
			for (unsigned int i = 0; i < nVertices && i < nUV; i++)
			{
				uv3ds[2 * i]     = object.pTexVerts[i].fU;
				// 3DS stores V with origin at the bottom; OpenGL samples the
				// first uploaded row at V=0 (top of the image). Flip V.
				uv3ds[2 * i + 1] = 1.0f - object.pTexVerts[i].fV;
			}
			pMesh->SetTextureCoordinates (std::move (uv3ds), pMesh->GetNTextureCoordinates ());
			for (unsigned int i = 0; i < nFaces; i++)
			{
				auto pFace = pMesh->FaceAt (i);
				pFace->ActivateTextureCoordinatesIndices();
				pFace->SetUsesTextureCoordinates (true);
				pFace->InitTexCoord(); // index = vertex index (UVs are per-vertex)
			}
		}

		// Materials: only import those used by this object
		std::map<int, int> materialMapping; // 3dsMatIdx -> meshMatIdx
		for (auto& matList : object.pFacesMaterialList)
		{
			int mat3dsIdx = matList.materialID;
			if (mat3dsIdx >= 0 && mat3dsIdx < p->numOfMaterials && materialMapping.find(mat3dsIdx) == materialMapping.end())
			{
				auto& mat3ds = p->pMaterials[mat3dsIdx];
				Material* pMaterial = nullptr;

				if (strlen(mat3ds.strFile) > 0)
				{
					// Textured material: load the referenced image from the
					// model's directory (PNG now supported via cgimg).
					auto pTex = new MaterialTexture(mat3ds.strFile,
					                                modelDir.empty() ? nullptr : modelDir.c_str());

					// La MAP EST la couleur diffuse : ambiant et diffus passent a
					// BLANC, et non au Kd/Ka du fichier.
					//
					// C'est la semantique du format -- dans 3ds Max une map
					// branchee sur l'emplacement diffus REMPLACE la couleur
					// diffuse, qui ne sert qu'a defaut de map. L'environnement de
					// texture etant GL_MODULATE, reporter le Kd revenait a le
					// MULTIPLIER au texel : sur Bar_chair_2.3ds, dont le materiau
					// « BLACKChair_C » porte Kd = 5/255 = 0.0196 et texture le
					// dessous en bois de l'assise, le texel sortait a 2 % de son
					// intensite, donc noir.
					//
					// Le 3DS n'a d'ailleurs aucun moyen d'exprimer « teinter une
					// map » : un MAT_TEXMAP ne porte que le nom du fichier et ses
					// tuilages. Un Kd quasi nul n'assombrit pas la texture, il
					// l'annule.
					pTex->SetAmbient (1.f, 1.f, 1.f, 1.f);
					pTex->SetDiffuse (1.f, 1.f, 1.f, 1.f);

					// Un exposant de Phong nul rend pow(N.H, 0) == 1 partout ou
					// N.H > 0 : le speculaire ENTIER s'ajoute a chaque fragment, et
					// il est module par le texel (le mode par defaut du pipeline
					// fixe est GL_SINGLE_COLOR, donc le speculaire est fondu dans
					// la couleur primaire AVANT le texturage). Aux angles rasants
					// N.H <= 0, OpenGL abandonne le terme : le rendu devient
					// tout-ou-rien, sature d'un cote et noir de l'autre.
					//
					// Un 3DS sans chunk MAT_SHININESS laisse Power a 0 (structure
					// zero-initialisee), ce qui veut dire « pas de reflet » et non
					// « reflet d'etendue infinie ». On coupe donc le speculaire.
					// Meme pathologie que le `Ks 1 1 1` avec `Ns 0` corrige cote
					// export MTL, sur l'autre chemin.
					if (mat3ds.sMaterial.Power > 0.f)
					{
						pTex->SetSpecular(mat3ds.sMaterial.Specular.r / 255.f, mat3ds.sMaterial.Specular.g / 255.f, mat3ds.sMaterial.Specular.b / 255.f, 1.f);
						pTex->SetShininess(mat3ds.sMaterial.Power / 100.f);
					}
					else
					{
						pTex->SetSpecular(0.f, 0.f, 0.f, 1.f);
						pTex->SetShininess(0.f);
					}

					// Carte de reflexion, si le materiau en declare une par
					// MAT_REFLMAP. L'echec de chargement est sans consequence :
					// SetReflectionMap laisse alors le materiau intact.
					if (mat3ds.strReflFile[0] != '\0')
						pTex->SetReflectionMap(mat3ds.strReflFile,
						                       modelDir.empty() ? nullptr : modelDir.c_str());
					pMaterial = pTex;
				}
				else
				{
					auto pMatExt = new MaterialColorExt();
					pMatExt->SetAmbient(mat3ds.sMaterial.Ambient.r / 255.f, mat3ds.sMaterial.Ambient.g / 255.f, mat3ds.sMaterial.Ambient.b / 255.f, mat3ds.sMaterial.Ambient.a / 255.f);
					pMatExt->SetDiffuse(mat3ds.sMaterial.Diffuse.r / 255.f, mat3ds.sMaterial.Diffuse.g / 255.f, mat3ds.sMaterial.Diffuse.b / 255.f, mat3ds.sMaterial.Diffuse.a / 255.f);
					pMatExt->SetSpecular(mat3ds.sMaterial.Specular.r / 255.f, mat3ds.sMaterial.Specular.g / 255.f, mat3ds.sMaterial.Specular.b / 255.f, mat3ds.sMaterial.Specular.a / 255.f);
					pMatExt->SetEmission(mat3ds.sMaterial.Emissive.r / 255.f, mat3ds.sMaterial.Emissive.g / 255.f, mat3ds.sMaterial.Emissive.b / 255.f, mat3ds.sMaterial.Emissive.a / 255.f);
					pMatExt->SetShininess(mat3ds.sMaterial.Power / 100.f);
					pMaterial = pMatExt;
				}

				if (pMaterial)
				{
					pMaterial->SetName(mat3ds.strName);
					int meshMatId = pMesh->Material_Add(pMaterial);
					materialMapping[mat3dsIdx] = meshMatId;
				}
			}
		}

		// Assign materials to faces
		for (auto& matList : object.pFacesMaterialList)
		{
			if (materialMapping.count(matList.materialID))
			{
				int meshMatId = materialMapping[matList.materialID];
				for (int i = 0; i < matList.numOfFaces; i++)
				{
					unsigned int faceIdx = matList.pFacesMaterialsList[i];
					if (faceIdx < nFaces)
						pMesh->FaceAt (faceIdx)->SetMaterialId(meshMatId);
				}
			}
		}

		if (!object.pNormals)
			pMesh->ComputeNormals();

		vm.AddMesh(pMesh);
	}

	Free3DSModel(p);

	return true;
}

bool VMeshesIO::export_3ds(VMeshes& vm, const char* filename)
{
	return false;
}
// tinygltf : SEULES les declarations ici. Son implementation vit dans
// tinygltf_impl.cpp, unite dediee compilee en natif ET en WebAssembly -- cette
// unite-ci ne l'est qu'en natif, et l'ecrivain GLB de maker n'aurait alors rien
// a lier. Les trois options TINYGLTF_NO_* viennent de CMakeLists.txt (PUBLIC) :
// elles changent la classe TinyGLTF elle-meme, donc toutes les unites doivent
// les voir. TINYGLTF_NO_INCLUDE_JSON impose l'inclusion de nlohmann/json.hpp
// ci-dessous, avant l'en-tete.
#define STB_IMAGE_IMPLEMENTATION

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstring>
#include <map>
#include <vector>

#include <nlohmann/json.hpp>
#include <stb/stb_image.h>
#include <tinygltf/tiny_gltf.h>

bool DummyLoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
    std::string* warn, int req_width, int req_height,
    const unsigned char* bytes, int size, void* user_data)
{
    (void)image_idx;
    (void)warn;
    (void)req_width;
    (void)req_height;
    (void)user_data;

    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char* decoded = stbi_load_from_memory(bytes, size, &width, &height, &components, STBI_rgb_alpha);
    if (!decoded)
    {
        if (err)
            *err += "Failed to decode glTF image with stb_image\n";
        return false;
    }

    image->width = width;
    image->height = height;
    image->component = 4;
    image->bits = 8;
    image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    image->image.assign(decoded, decoded + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    stbi_image_free(decoded);
    return true;
}

namespace
{
// Acces BORNE au tableau des accesseurs, rend nullptr hors bornes.
//
// Les indices d'accesseur viennent du fichier et de nulle part d'ailleurs :
// `primitive.attributes` est un map<string,int> que ParsePrimitive remplit par
// ParseStringIntegerProperty, `primitive.indices` un int lu de la meme facon,
// et NI L'UN NI L'AUTRE n'est confronte a model.accessors.size(). Indexer
// directement, c'est indexer par un entier arbitraire de l'entree.
const tinygltf::Accessor* AccessorAt(const tinygltf::Model& model, int index)
{
    if (index < 0 || index >= static_cast<int>(model.accessors.size()))
        return nullptr;
    return &model.accessors[index];
}

// Meme regle un cran plus bas : `bufferView.buffer` est lui aussi un entier du
// fichier que tinygltf ne borne pas.
const tinygltf::BufferView* BufferViewAt(const tinygltf::Model& model, int index)
{
    if (index < 0 || index >= static_cast<int>(model.bufferViews.size()))
        return nullptr;
    return &model.bufferViews[index];
}

const tinygltf::Buffer* BufferAt(const tinygltf::Model& model, int index)
{
    if (index < 0 || index >= static_cast<int>(model.buffers.size()))
        return nullptr;
    return &model.buffers[index];
}

// Copie d'un accessor VEC3 flottant, EN RESPECTANT byteStride.
//
// La foulee n'est pas une optimisation exotique : un GLB a tampon ENTRELACE
// range POSITION et NORMAL dans une meme vue, separes par la foulee declaree
// sur celle-ci. Une lecture contigue y prendrait la normale du sommet i pour la
// position du sommet i+1 -- geometrie fausse, aucune erreur, aucun
// avertissement.
//
// Accessor::ByteStride rend la taille d'un element quand la vue ne declare
// aucune foulee : le cas non entrelace passe donc par le meme chemin.
//
// Rend false -- donc primitive ignoree -- sur tout ce qui n'est pas lisible :
// mauvais type, indices hors bornes, foulee trop courte, ou tampon trop court
// pour le dernier element. Le dernier controle est indispensable : un accessor
// est une DECLARATION du fichier, que tinygltf ne confronte pas au tampon.
bool CopyFloatAccessorVec3(std::vector<float>& dst, const tinygltf::Model& model,
                           const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_VEC3 ||
        accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.bufferView < 0 ||
        accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        return false;

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size()))
        return false;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const int stride = accessor.ByteStride(view);
    const size_t element = 3 * sizeof(float);
    if (stride < static_cast<int>(element))
        return false;

    dst.clear();
    if (accessor.count == 0)
        return false;
    // `count` est un compte d'ELEMENTS, chacun d'au moins 2 octets : il ne peut
    // jamais depasser la taille du tampon exprimee en OCTETS. Ce plafond est
    // pose AVANT toute multiplication -- `span` comme `count * K` deborderaient
    // sinon en size_t, le premier en faisant PASSER le controle de portee, le
    // second en SOUS-ALLOUANT dst pendant que la boucle, elle, tourne `count`
    // fois. Le fichier est une entree non fiable et tinygltf ne borne pas
    // `count`.
    if (accessor.count > buffer.data.size())
        return false;

    // Les deux decalages sont des size_t du fichier : leur somme peut
    // BOUCLER. Bornee ici, elle rend `span` non bouclant a son tour, la
    // foulee etant plafonnee a 252 par ParseBufferView.
    const size_t base = view.byteOffset + accessor.byteOffset;
    if (base > buffer.data.size())
        return false;
    const size_t span = base + (accessor.count - 1) * static_cast<size_t>(stride) + element;
    if (span > buffer.data.size())
        return false;

    dst.resize(accessor.count * 3);
    for (size_t i = 0; i < accessor.count; ++i)
        memcpy(&dst[3 * i], buffer.data.data() + base + i * static_cast<size_t>(stride), element);
    return true;
}

// Copie d'un accessor VEC4 flottant, memes controles que la version VEC3.
//
// Restreinte au type FLOAT : glTF 2.0 de base n'autorise que lui pour TANGENT,
// BYTE et SHORT normalises relevant de KHR_mesh_quantization. Aucun exportateur
// courant ne les emet et une conversion non testee vaut moins qu'un refus
// franc -- refuser laisse la generation prendre le relais, convertir de travers
// donnerait une base tangente fausse sans aucun signal.
bool CopyFloatAccessorVec4(std::vector<float>& dst, const tinygltf::Model& model,
                           const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_VEC4 ||
        accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.bufferView < 0 ||
        accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        return false;

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size()))
        return false;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const int stride = accessor.ByteStride(view);
    const size_t element = 4 * sizeof(float);
    if (stride < static_cast<int>(element))
        return false;

    dst.clear();
    if (accessor.count == 0)
        return false;
    // Meme plafond que la version VEC3, et pour la meme raison : il ferme le
    // debordement de `span` et celui de `count * 4`.
    if (accessor.count > buffer.data.size())
        return false;

    // Les deux decalages sont des size_t du fichier : leur somme peut
    // BOUCLER. Bornee ici, elle rend `span` non bouclant a son tour, la
    // foulee etant plafonnee a 252 par ParseBufferView.
    const size_t base = view.byteOffset + accessor.byteOffset;
    if (base > buffer.data.size())
        return false;
    const size_t span = base + (accessor.count - 1) * static_cast<size_t>(stride) + element;
    if (span > buffer.data.size())
        return false;

    dst.resize(accessor.count * 4);
    for (size_t i = 0; i < accessor.count; ++i)
        memcpy(&dst[4 * i], buffer.data.data() + base + i * static_cast<size_t>(stride), element);
    return true;
}

// Copie d'un accessor VEC2, memes controles de bornes que les versions VEC3 et
// VEC4.
//
// Le type de composante est soumis a une LISTE BLANCHE de cinq entrees.
// glTF 2.0 de base n'autorise pour TEXCOORD_n que FLOAT, UNSIGNED_BYTE
// normalise et UNSIGNED_SHORT normalise ; BYTE et SHORT normalises relevent de
// KHR_mesh_quantization. Les cinq sont acceptes -- la tolerance sur les deux
// types signes ne coute rien et les exportateurs s'en servent pour compresser
// les UV -- mais ce qui n'est pas dans la liste est REFUSE.
//
// Le refus est la raison d'etre de la liste : ParseAccessor ne valide que
// BYTE <= componentType <= DOUBLE, donc un TEXCOORD_n en UNSIGNED_INT (5125) ou
// DOUBLE (5130) traverse tinygltf sans un mot. Converti par defaut, il
// produirait un tableau d'UV TOUTES NULLES declare valide, qui alimente le
// choix du jeu puis la generation de la base tangente : une parametrisation
// inventee, sans aucun signal.
bool CopyFloatAccessorVec2(std::vector<float>& dst, const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_VEC2 ||
        accessor.bufferView < 0 ||
        accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        return false;

    switch (accessor.componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        break;
    default:
        return false;
    }

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size()))
        return false;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];

    const int stride = accessor.ByteStride(view);
    const int componentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
    if (componentSize <= 0 || stride < 2 * componentSize)
        return false;

    dst.clear();
    if (accessor.count == 0)
        return false;
    // Meme plafond que les versions VEC3 et VEC4. Il tient ici aussi avec la
    // plus petite composante du format : `stride >= 2 * componentSize >= 2`,
    // donc `count` elements occupent au moins `2 * count` octets.
    if (accessor.count > buffer.data.size())
        return false;

    const size_t element = 2 * static_cast<size_t>(componentSize);
    // Les deux decalages sont des size_t du fichier : leur somme peut
    // BOUCLER. Bornee ici, elle rend `span` non bouclant a son tour, la
    // foulee etant plafonnee a 252 par ParseBufferView.
    const size_t base = view.byteOffset + accessor.byteOffset;
    if (base > buffer.data.size())
        return false;
    const size_t span = base + (accessor.count - 1) * static_cast<size_t>(stride) + element;
    if (span > buffer.data.size())
        return false;

    const unsigned char* data = buffer.data.data() + base;

    auto convertComponent = [&accessor](const unsigned char* componentData) -> float
    {
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return *reinterpret_cast<const float*>(componentData);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        {
            const float value = static_cast<float>(*reinterpret_cast<const uint8_t*>(componentData));
            return accessor.normalized ? value / 255.0f : value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            const float value = static_cast<float>(*reinterpret_cast<const uint16_t*>(componentData));
            return accessor.normalized ? value / 65535.0f : value;
        }
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        {
            const float value = static_cast<float>(*reinterpret_cast<const int8_t*>(componentData));
            return accessor.normalized ? (value < 0.0f ? value / 128.0f : value / 127.0f) : value;
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        {
            const float value = static_cast<float>(*reinterpret_cast<const int16_t*>(componentData));
            return accessor.normalized ? (value < 0.0f ? value / 32768.0f : value / 32767.0f) : value;
        }
        default:
            // Inatteignable : la liste blanche en tete de fonction a deja
            // refuse tout ce qui n'est pas l'un des cinq cas ci-dessus.
            return 0.0f;
        }
    };

    dst.resize(accessor.count * 2);
    for (size_t i = 0; i < accessor.count; ++i)
    {
        const unsigned char* uv = data + i * static_cast<size_t>(stride);
        dst[2 * i] = convertComponent(uv);
        dst[2 * i + 1] = convertComponent(uv + componentSize);
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Hierarchie de noeuds glTF -> repere du depot
// ---------------------------------------------------------------------------
// L'import lisait model.meshes en ignorant model.nodes, donc TOUTE
// transformation de scene. Deux consequences, silencieuses toutes les deux :
//
//   1. l'echelle et l'orientation portees par les noeuds etaient perdues. Le
//      Duck.glb de Khronos porte une echelle de 0,01 sur son noeud racine : il
//      arrivait 100 fois trop grand ;
//   2. la convention glTF elle-meme etait ignoree. glTF est Y-up et son unite de
//      distance est le METRE ; le depot travaille en Z-up et en MILLIMETRES.
//      L'export (mesh_io_gltf.cpp) porte cette conversion sur le noeud -- echelle
//      0,001 et -90 degres autour de X -- mais l'import n'en faisait pas
//      l'inverse, si bien qu'un aller-retour ne redonnait pas le modele de depart.
//
// On parcourt donc la scene, on compose les matrices de la racine jusqu'a chaque
// noeud portant un maillage, et l'on applique par-dessus la conversion inverse de
// celle de l'export :
//
//     depot = echelle(1000) . Rx(+90) . <chaine de noeuds> . sommet
//
// Rx(+90) envoie (x,y,z) sur (x,-z,y) : le +Y de glTF devient le +Z du depot.
//
// Un maillage reference par PLUSIEURS noeuds est instancie autant de fois, ce qui
// est la semantique du format -- c'est ainsi qu'un glTF exprime la repetition.

// Matrice 4x4 en COLONNES D'ABORD, comme glTF et OpenGL : m[4*c + r].
struct GltfMat4
{
    double m[16];
};

GltfMat4 Mat4Identity()
{
    GltfMat4 r = {};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
    return r;
}

// a . b, au sens ou le produit applique b PUIS a.
GltfMat4 Mat4Multiply(const GltfMat4& a, const GltfMat4& b)
{
    GltfMat4 r = {};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
        {
            double s = 0.0;
            for (int k = 0; k < 4; ++k)
                s += a.m[4 * k + row] * b.m[4 * c + k];
            r.m[4 * c + row] = s;
        }
    return r;
}

// Transformation LOCALE d'un noeud. La specification interdit de fournir a la
// fois `matrix` et un triplet TRS ; `matrix` l'emporte quand elle est presente.
GltfMat4 Mat4FromNode(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        GltfMat4 r;
        for (int i = 0; i < 16; ++i)
            r.m[i] = node.matrix[i];
        return r;
    }

    const double tx = (node.translation.size() == 3) ? node.translation[0] : 0.0;
    const double ty = (node.translation.size() == 3) ? node.translation[1] : 0.0;
    const double tz = (node.translation.size() == 3) ? node.translation[2] : 0.0;
    const double sx = (node.scale.size() == 3) ? node.scale[0] : 1.0;
    const double sy = (node.scale.size() == 3) ? node.scale[1] : 1.0;
    const double sz = (node.scale.size() == 3) ? node.scale[2] : 1.0;
    // Quaternion glTF : (x, y, z, w), dans cet ordre.
    const double qx = (node.rotation.size() == 4) ? node.rotation[0] : 0.0;
    const double qy = (node.rotation.size() == 4) ? node.rotation[1] : 0.0;
    const double qz = (node.rotation.size() == 4) ? node.rotation[2] : 0.0;
    const double qw = (node.rotation.size() == 4) ? node.rotation[3] : 1.0;

    // Bloc rotation du quaternion, puis mise a l'echelle de chaque COLONNE :
    // c'est l'ordre T . R . S impose par la specification.
    const double rot[9] = {
        1.0 - 2.0 * (qy * qy + qz * qz),  2.0 * (qx * qy + qz * qw),        2.0 * (qx * qz - qy * qw),
        2.0 * (qx * qy - qz * qw),        1.0 - 2.0 * (qx * qx + qz * qz),  2.0 * (qy * qz + qx * qw),
        2.0 * (qx * qz + qy * qw),        2.0 * (qy * qz - qx * qw),        1.0 - 2.0 * (qx * qx + qy * qy)
    };

    GltfMat4 r = Mat4Identity();
    const double s[3] = { sx, sy, sz };
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row)
            r.m[4 * c + row] = rot[3 * c + row] * s[c];
    r.m[12] = tx;
    r.m[13] = ty;
    r.m[14] = tz;
    return r;
}

// Conversion glTF -> depot : echelle(1000) . Rx(+90). Exactement l'inverse de ce
// que l'export ecrit sur son noeud (cf. mesh_io_gltf.cpp).
GltfMat4 Mat4GltfToRepo()
{
    // Rx(+90) : (x,y,z) -> (x,-z,y). Colonnes de la matrice = images des axes.
    GltfMat4 r = {};
    const double k = 1000.0;      // metres -> millimetres
    r.m[0]  = k;                  // X -> X
    r.m[6]  = k;                  // Y -> Z
    r.m[9]  = -k;                 // Z -> -Y
    r.m[15] = 1.0;
    return r;
}

void Mat4TransformPoint(const GltfMat4& t, const float in[3], float out[3])
{
    for (int row = 0; row < 3; ++row)
        out[row] = static_cast<float>(t.m[row]      * in[0] +
                                      t.m[4 + row]  * in[1] +
                                      t.m[8 + row]  * in[2] +
                                      t.m[12 + row]);
}

// Bloc 3x3 applique tel quel, destine aux TANGENTES.
//
// Une tangente n'est pas une normale : elle est PORTEE PAR la surface, pas
// perpendiculaire a elle. Elle se transforme donc par la matrice elle-meme et
// NON par son inverse transposee -- lui appliquer la matrice des normales la
// ferait sortir du plan tangent sous toute echelle non uniforme.
void Mat3TransformVector(const GltfMat4& t, const float in[3], float out[3])
{
    for (int row = 0; row < 3; ++row)
        out[row] = static_cast<float>(t.m[row]     * in[0] +
                                      t.m[4 + row] * in[1] +
                                      t.m[8 + row] * in[2]);
}

// Bloc 3x3 destine aux NORMALES : l'inverse TRANSPOSEE, et non la matrice
// elle-meme. Sous une echelle non uniforme, une normale transformee comme un
// point cesse d'etre perpendiculaire a sa surface et l'eclairage part de travers.
// Rendu en n[3*row + col], de sorte que sortie[row] = somme sur col.
//
// Bloc singulier (une echelle nulle sur un axe, ce qui aplatit le maillage) :
// on rend l'identite. Une normale non tournee reste exploitable, une normale
// nulle noircit la surface sans rien dire.
void Mat3InverseTranspose(const GltfMat4& t, double n[9])
{
    const double a[9] = {
        t.m[0], t.m[4], t.m[8],
        t.m[1], t.m[5], t.m[9],
        t.m[2], t.m[6], t.m[10]
    };  // a[3*row + col]

    const double cof[9] = {
        a[4] * a[8] - a[5] * a[7],   a[5] * a[6] - a[3] * a[8],   a[3] * a[7] - a[4] * a[6],
        a[2] * a[7] - a[1] * a[8],   a[0] * a[8] - a[2] * a[6],   a[1] * a[6] - a[0] * a[7],
        a[1] * a[5] - a[2] * a[4],   a[2] * a[3] - a[0] * a[5],   a[0] * a[4] - a[1] * a[3]
    };  // cof[3*row + col] = cofacteur de a(row, col)

    const double det = a[0] * cof[0] + a[1] * cof[1] + a[2] * cof[2];
    if (std::fabs(det) < 1e-20)
    {
        for (int i = 0; i < 9; ++i)
            n[i] = (i % 4 == 0) ? 1.0 : 0.0;
        return;
    }
    // (A^-1)^T [row][col] = cofacteur(row, col) / det.
    for (int i = 0; i < 9; ++i)
        n[i] = cof[i] / det;
}

double Mat3Determinant(const GltfMat4& t)
{
    return t.m[0] * (t.m[5] * t.m[10] - t.m[9] * t.m[6])
         - t.m[4] * (t.m[1] * t.m[10] - t.m[9] * t.m[2])
         + t.m[8] * (t.m[1] * t.m[6]  - t.m[5] * t.m[2]);
}

// Un maillage de la scene, avec sa transformation vers le repere du depot.
struct GltfInstance
{
    int      mesh;
    GltfMat4 xform;
};

void CollectGltfInstances(const tinygltf::Model& model, int nodeIndex,
                          const GltfMat4& parent, int depth,
                          std::vector<GltfInstance>& out)
{
    // La specification interdit les cycles dans la hierarchie ; un fichier casse
    // ou malveillant n'en tient pas compte, et cette recursion n'aurait alors
    // rien pour s'arreter. La borne de profondeur est ce garde-fou.
    if (depth > 64)
        return;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
        return;

    const tinygltf::Node& node = model.nodes[nodeIndex];
    const GltfMat4 world = Mat4Multiply(parent, Mat4FromNode(node));

    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size()))
        out.push_back(GltfInstance{ node.mesh, world });

    for (const int child : node.children)
        CollectGltfInstances(model, child, world, depth + 1, out);
}

// Les maillages a construire, dans l'ordre de la scene.
//
// La scene retenue est celle que le fichier designe (defaultScene), a defaut la
// premiere. Les noeuds hors scene sont ignores, ce qu'exige le format. Un fichier
// SANS scene ni noeud exploitable retombe sur model.meshes avec la seule
// conversion d'unite et d'axe : ce n'est pas conforme, mais c'est mieux que de
// ne rien afficher -- et cela reste le comportement d'avant, a la conversion pres.
std::vector<GltfInstance> GltfSceneInstances(const tinygltf::Model& model)
{
    const GltfMat4 toRepo = Mat4GltfToRepo();
    std::vector<GltfInstance> out;

    int sceneIndex = -1;
    if (model.defaultScene >= 0 && model.defaultScene < static_cast<int>(model.scenes.size()))
        sceneIndex = model.defaultScene;
    else if (!model.scenes.empty())
        sceneIndex = 0;

    if (sceneIndex >= 0)
        for (const int root : model.scenes[sceneIndex].nodes)
            CollectGltfInstances(model, root, toRepo, 0, out);

    if (out.empty())
        for (size_t i = 0; i < model.meshes.size(); ++i)
            out.push_back(GltfInstance{ static_cast<int>(i), toRepo });

    return out;
}
}

bool VMeshesIO::import_gltf(VMeshes& vm, const char* filename)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(DummyLoadImageData, nullptr);
    std::string err;
    std::string warn;

    int size = strlen(filename);
    bool ret = false;
    if (filename[size - 3] == 'g' && filename[size - 2] == 'l' && filename[size - 1] == 'b')
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    else
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!ret) {
        return false;
    }

    // Une entree par (noeud, maillage) de la scene, chacune avec sa matrice
    // composee -- conversion glTF -> depot comprise. Cf. GltfSceneInstances.
    const std::vector<GltfInstance> instances = GltfSceneInstances(model);

    for (const GltfInstance& instance : instances) {
        const tinygltf::Mesh& gltfMesh = model.meshes[instance.mesh];

        // Normales : inverse transposee du bloc lineaire. Determinant negatif =
        // transformation MIROIR, qui inverse le sens de parcours des triangles ;
        // les faces sont alors reordonnees plus bas, sans quoi le maillage sort
        // retourne (faces arriere devant, eclairage a l'envers).
        double normalMatrix[9];
        Mat3InverseTranspose(instance.xform, normalMatrix);
        const bool mirrored = (Mat3Determinant(instance.xform) < 0.0);

        for (const auto& primitive : gltfMesh.primitives) {
			if (primitive.mode != 4)
			{
				continue; // TINYGLTF_MODE_TRIANGLES = 4
			}

            // Positions
			auto posIt = primitive.attributes.find("POSITION");
			if (posIt == primitive.attributes.end())
			{
				continue;
			}

			const tinygltf::Accessor* posAccessorPtr = AccessorAt(model, posIt->second);
			if (!posAccessorPtr)
			{
				continue;
			}
			const tinygltf::Accessor& posAccessor = *posAccessorPtr;
            std::vector<float> positions;
            if (!CopyFloatAccessorVec3(positions, model, posAccessor))
            {
                continue;
            }

            bool hasIndices = primitive.indices >= 0;
            const tinygltf::Accessor* indexAccessorPtr = nullptr;
            const tinygltf::BufferView* indexViewPtr = nullptr;
            const tinygltf::Buffer* indexBufferPtr = nullptr;
            size_t triangleCount = 0;

            if (hasIndices)
            {
                indexAccessorPtr = AccessorAt(model, primitive.indices);
                if (!indexAccessorPtr)
                {
                    continue;
                }
                indexViewPtr = BufferViewAt(model, indexAccessorPtr->bufferView);
                if (!indexViewPtr)
                {
                    continue;
                }
                indexBufferPtr = BufferAt(model, indexViewPtr->buffer);
                if (!indexBufferPtr)
                {
                    continue;
                }
                // PORTEE DES INDICES DANS LE TAMPON. Les deux boucles
                // indexees d'ecriture des faces adressent le tampon par un
                // pointeur brut, sans repasser par un copieur : le controle
                // de portee que les copieurs portent pour les attributs doit
                // donc etre pose ici. `count` n'y est pas plus borne
                // qu'ailleurs -- tinygltf valide l'indice de l'accesseur et
                // celui de sa vue, jamais la place que `count` reclame.
                //
                // Plafond avant multiplication, meme raison que dans les
                // copieurs : un element d'indice occupe au moins un octet,
                // donc `count` ne peut depasser la taille du tampon en octets,
                // et ce plafond ferme le debordement de `count * taille`.
                {
                    const int indexComponentSize = tinygltf::GetComponentSizeInBytes(
                        static_cast<uint32_t>(indexAccessorPtr->componentType));
                    if (indexComponentSize <= 0)
                    {
                        continue;
                    }
                    if (indexAccessorPtr->count > indexBufferPtr->data.size())
                    {
                        continue;
                    }
                    const size_t indexBase = indexViewPtr->byteOffset + indexAccessorPtr->byteOffset;
                    if (indexBase > indexBufferPtr->data.size())
                    {
                        continue;
                    }
                    const size_t indexSpan = indexBase +
                        indexAccessorPtr->count * static_cast<size_t>(indexComponentSize);
                    if (indexSpan > indexBufferPtr->data.size())
                    {
                        continue;
                    }
                }
                triangleCount = indexAccessorPtr->count / 3;
            }
            else
            {
                if ((posAccessor.count % 3) != 0)
                {
                    continue;
                }
                triangleCount = posAccessor.count / 3;
            }

			auto pMesh = new Mesh();
            pMesh->Init(posAccessor.count, static_cast<unsigned int>(triangleCount));

            int texCoordSet = 0;
            // Materiau UNE FOIS POSSEDE PAR LE MAILLAGE. Sert a recrire les
            // uvSet quand les deux jeux sont permutes plus bas ; nul quand la
            // primitive n'a pas de materiau PBR.
            MaterialPbr* pbrOnMesh = nullptr;
            // Vrai quand le materiau porte une CARTE DE NORMALES : elle seule
            // rend la base tangente indispensable. Sans carte de normales, une
            // tangente ne sert a rien et la calculer serait du travail et de la
            // memoire pour un attribut que personne ne lit.
            //
            // Le JEU sur lequel la batir n'est pas lu ici : il est relu apres la
            // recriture des uvSet, seul moment ou l'attribut designe un jeu DU
            // MAILLAGE.
            bool needsTangentBasis = false;

            // MATERIAU : lu ENTIER, et porte tel quel par le maillage.
            //
            // cgpbr::materialFromGltf rend un MaterialPbr -- cinq cartes, sept
            // facteurs, mode d'alpha -- la ou ce bloc fabriquait un
            // MaterialColorExt dont le speculaire et la brillance etaient des
            // CONSTANTES : metallicFactor et roughnessFactor du fichier
            // n'atteignaient jamais le maillage.
            //
            // Aucune conversion colorimetrique ici : baseColorFactor est
            // LINEAIRE et MaterialPbr est le conteneur du format, donc il le
            // garde lineaire. La conversion vers le sRGB des materiaux du depot
            // appartient a la projection cgpbr::toPhong, seul endroit ou la
            // frontiere entre les deux modeles est franchie.
            if (primitive.material >= 0 && primitive.material < (int)model.materials.size()) {
                std::unique_ptr<MaterialPbr> pMaterial =
                    cgpbr::materialFromGltf(model, primitive.material);
                if (pMaterial)
                {
                    // Le jeu d'UV a lire est celui que designe la carte de
                    // couleur de base, ramene a 0 ou 1 par la lecture.
                    if (pMaterial->HasMap(cgpbr::MapSlot::base_color))
                        texCoordSet = pMaterial->GetMap(cgpbr::MapSlot::base_color).uvSet;

                    needsTangentBasis = pMaterial->HasMap(cgpbr::MapSlot::normal);

                    const unsigned int matId = pMesh->Material_Add(pMaterial.release());
                    pbrOnMesh = dynamic_cast<MaterialPbr*>(pMesh->GetMaterial(matId));
                    pMesh->ApplyMaterial(matId); // Set default material for all faces
                }
            }

            for (size_t i = 0; i < posAccessor.count; i++) {
                float p[3];
                Mat4TransformPoint(instance.xform, &positions[i * 3], p);
                pMesh->SetVertex(i, p[0], p[1], p[2]);
            }

            // Normals
            bool hasNormals = false;
            auto normIt = primitive.attributes.find("NORMAL");
            const tinygltf::Accessor* normAccessorPtr =
                (normIt != primitive.attributes.end()) ? AccessorAt(model, normIt->second) : nullptr;
            if (normAccessorPtr) {
                const tinygltf::Accessor& normAccessor = *normAccessorPtr;
                std::vector<float> normals;

                if (normAccessor.count == posAccessor.count &&
                    CopyFloatAccessorVec3(normals, model, normAccessor)) {
                    for (size_t i = 0; i < normAccessor.count; i++) {
                        const float* n = &normals[i * 3];
                        double t[3];
                        for (int row = 0; row < 3; ++row)
                            t[row] = normalMatrix[3 * row]     * n[0] +
                                     normalMatrix[3 * row + 1] * n[1] +
                                     normalMatrix[3 * row + 2] * n[2];
                        // L'echelle des noeuds (1000 rien que pour l'unite) passe
                        // dans la normale : sans renormalisation, GL_NORMALIZE la
                        // rattraperait au rendu, mais toute lecture directe des
                        // normales -- courbure, export -- verrait des vecteurs de
                        // longueur 1000.
                        const double len = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
                        const double inv = (len > 1e-20) ? 1.0 / len : 0.0;
                        pMesh->SetVertexNormal ((unsigned int)i,
                                                (float)(t[0] * inv),
                                                (float)(t[1] * inv),
                                                (float)(t[2] * inv));
                    }
                    hasNormals = true;
                }
            }

            // LES DEUX JEUX D'UV.
            //
            // Le jeu que designe la carte de couleur de base est monte au rang
            // 0 du maillage, l'autre au rang 1 -- et non chacun a son rang
            // d'origine. Deux raisons :
            //
            //  - le chemin de rendu a fonction fixe (cgre, VBOManager) ne
            //    connait qu'UN jeu, le 0. Monter TEXCOORD_1 au rang 1 quand
            //    c'est lui qui porte la couleur de base texturerait le modele
            //    avec la mauvaise parametrisation, en silence ;
            //  - c'est la permutation qui rend `uvSet` HONNETE : apres cette
            //    lecture, l'attribut designe un jeu DU MAILLAGE, pas un
            //    attribut du fichier. Les cartes sont donc recrites en
            //    consequence -- laisser une carte annoncer 1 quand sa
            //    parametrisation a ete montee au rang 0 serait exactement le
            //    mensonge que ce chantier supprime.
            //
            // LES DEUX jeux arrivent PARALLELES AUX SOMMETS : un accessor dont
            // le compte differe de celui des positions est refuse, pas tronque.
            // C'est un invariant de Mesh pour le jeu 1 ; pour le jeu 0, c'est
            // une consequence de la facon dont les faces sont ecrites plus bas,
            // qui adresse les UV par indice de sommet.
            {
                auto readUvSet = [&](int set, std::vector<float>& dst) -> bool
                {
                    auto it = primitive.attributes.find("TEXCOORD_" + std::to_string(set));
                    if (it == primitive.attributes.end())
                        return false;
                    const tinygltf::Accessor* acc = AccessorAt(model, it->second);
                    if (acc == nullptr)
                        return false;
                    return CopyFloatAccessorVec2(dst, model, *acc) && !dst.empty();
                };

                std::vector<float> primary, secondary;
                bool hasPrimary = readUvSet(texCoordSet, primary);
                if (!hasPrimary && texCoordSet != 0)
                {
                    // Le materiau designe un jeu que la primitive n'a pas :
                    // fichier non conforme (glTF exige des indices de TEXCOORD
                    // consecutifs a partir de 0). On retombe sur le jeu 0
                    // plutot que de laisser le maillage sans UV.
                    texCoordSet = 0;
                    hasPrimary = readUvSet(0, primary);
                }
                const bool hasSecondary = readUvSet(texCoordSet == 0 ? 1 : 0, secondary);

                // LES DEUX jeux sont soumis au meme contrat de taille. Les faces
                // adressent le jeu 0 par INDICE DE SOMMET (SetTexCoord ci-dessous
                // recopie l'indice de sommet) : un tableau plus court que les
                // positions serait indexe hors de ses bornes par tout
                // consommateur qui suit cette indirection.
                if (hasPrimary && primary.size() == 2 * posAccessor.count)
                    pMesh->SetTextureCoordinates (primary,
                                                  static_cast<unsigned int>(primary.size() / 2));
                if (hasSecondary && secondary.size() == 2 * posAccessor.count)
                    pMesh->SetTextureCoordinates1 (std::move(secondary));

                // RECRITURE DES uvSet, en deux temps.
                //
                //  1. permutation, quand les deux jeux ont ete echanges ;
                //  2. RABATTEMENT SUR LE SEUL JEU NON VIDE, dans les DEUX sens.
                //     Sans lui, une carte d'occlusion sur TEXCOORD_1 dans un
                //     fichier qui n'en porte pas -- ou dont l'accessor a ete
                //     refuse -- annoncerait un jeu que personne ne peut
                //     echantillonner. Le cas symetrique existe : le jeu 0 peut
                //     etre vide alors que le 1 est lu, si l'accessor de
                //     TEXCOORD_0 est refuse (type, foulee, compte) et pas
                //     l'autre. Ce que le rabattement epargne au consommateur,
                //     c'est le cas ou un jeu existe et ou uvSet designe
                //     l'AUTRE ; il ne le dispense pas de verifier que le jeu
                //     vise est non vide, le maillage pouvant n'en porter aucun.
                //
                //     Le rabattement ne pretend PAS que le jeu restant porte la
                //     bonne parametrisation -- il n'y en a plus qu'une. Il
                //     garantit seulement que uvSet designe un tableau qui
                //     existe. Les deux jeux vides laissent 0, valeur neutre.
                if (pbrOnMesh != nullptr)
                {
                    const bool swapped = (texCoordSet != 0);
                    const bool set0Exists = !pMesh->GetTextureCoordinates ().empty ();
                    const bool set1Exists = !pMesh->GetTextureCoordinates1 ().empty ();
                    for (int s = 0; s < static_cast<int>(cgpbr::MapSlot::count); ++s)
                    {
                        const cgpbr::MapSlot slot = static_cast<cgpbr::MapSlot>(s);
                        if (!pbrOnMesh->HasMap(slot))
                            continue;
                        cgpbr::TextureRef ref = pbrOnMesh->GetMap(slot);
                        if (swapped)
                            ref.uvSet = (ref.uvSet == 0) ? 1 : 0;
                        if (ref.uvSet == 1 && !set1Exists)
                            ref.uvSet = 0;
                        else if (ref.uvSet == 0 && !set0Exists && set1Exists)
                            ref.uvSet = 1;
                        pbrOnMesh->SetMap(slot, std::move(ref));
                    }
                }
            }

            // TANGENTES DU FICHIER. Elles priment sur toute generation : leur
            // auteur connait la parametrisation utilisee pour cuire la carte
            // de normales, ce qu'une reconstruction ne peut que retrouver
            // approximativement.
            //
            // LUES ICI, POSEES EN FIN D'IMPORT. SetVertexTangents estampille les
            // tangentes contre la revision COURANTE ; les poser avant l'ecriture
            // des faces les ferait declarer perimees par AreTangentsValid(), car
            // chaque SetNVertices / SetVertex / SetTexCoord incremente la
            // revision. Les tangentes de l'auteur sortiraient marquees plus
            // vieilles que la geometrie, et un consommateur qui suit le contrat
            // les regenererait -- exactement ce que leur primaute interdit.
            std::vector<float> fileTangents;
            {
                auto it = primitive.attributes.find("TANGENT");
                const tinygltf::Accessor* acc =
                    (it != primitive.attributes.end()) ? AccessorAt(model, it->second) : nullptr;
                // LA COPIE D'ABORD, LE CONTRAT DE TAILLE ENSUITE. Ce copieur
                // est le seul dont l'unique appel etait garde par une egalite
                // avec un AUTRE accessor : sa sureté memoire reposait alors sur
                // les controles de POSITION, pas sur les siens. C'est la
                // dependance croisee qui a produit B1. L'ordre inverse coute
                // une copie sur un fichier non conforme -- bornee par le
                // controle de portee du copieur, donc par la taille du tampon.
                if (acc != nullptr &&
                    CopyFloatAccessorVec4(fileTangents, model, *acc) &&
                    acc->count == posAccessor.count)
                {
                    for (size_t i = 0; i < acc->count; ++i)
                    {
                        float t[3];
                        Mat3TransformVector(instance.xform, &fileTangents[4 * i], t);
                        const double len = std::sqrt((double)t[0]*t[0] +
                                                     (double)t[1]*t[1] +
                                                     (double)t[2]*t[2]);
                        const double inv = (len > 1e-20) ? 1.0 / len : 0.0;
                        fileTangents[4 * i]     = (float)(t[0] * inv);
                        fileTangents[4 * i + 1] = (float)(t[1] * inv);
                        fileTangents[4 * i + 2] = (float)(t[2] * inv);
                        // Une transformation MIROIR inverse le sens de la
                        // base : la bitangente reconstruite par
                        // cross (N, T) * w pointerait du mauvais cote si w
                        // traversait inchange.
                        if (mirrored)
                            fileTangents[4 * i + 3] = -fileTangents[4 * i + 3];
                    }
                }
                else
                {
                    fileTangents.clear();
                }
            }

            if (!hasIndices)
            {
                for (size_t i = 0; i < triangleCount; i++) {
                    pMesh->FaceAt (i)->SetNVertices(3);
                    pMesh->FaceAt (i)->SetVertex(0, static_cast<unsigned int>(i * 3));
                    pMesh->FaceAt (i)->SetVertex(1, static_cast<unsigned int>(i * 3 + 1));
                    pMesh->FaceAt (i)->SetVertex(2, static_cast<unsigned int>(i * 3 + 2));
                    if (!pMesh->GetTextureCoordinates ().empty())
                    {
                        pMesh->FaceAt (i)->SetUsesTextureCoordinates (true);
                        pMesh->FaceAt (i)->ActivateTextureCoordinatesIndices();
                        pMesh->FaceAt (i)->SetTexCoord(0, static_cast<unsigned int>(i * 3));
                        pMesh->FaceAt (i)->SetTexCoord(1, static_cast<unsigned int>(i * 3 + 1));
                        pMesh->FaceAt (i)->SetTexCoord(2, static_cast<unsigned int>(i * 3 + 2));
                    }
                }
            }
            else
            {
                const tinygltf::Accessor& indexAccessor = *indexAccessorPtr;
                const tinygltf::BufferView& indexView = *indexViewPtr;
                const tinygltf::Buffer& indexBuffer = *indexBufferPtr;
                if (indexAccessor.componentType == 5123) { // TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT = 5123
                    const uint16_t* indices = reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                    for (size_t i = 0; i < triangleCount; i++) {
                        pMesh->FaceAt (i)->SetNVertices(3);
                        pMesh->FaceAt (i)->SetVertex(0, indices[i * 3]);
                        pMesh->FaceAt (i)->SetVertex(1, indices[i * 3 + 1]);
                        pMesh->FaceAt (i)->SetVertex(2, indices[i * 3 + 2]);
                        if (!pMesh->GetTextureCoordinates ().empty())
                        {
                            pMesh->FaceAt (i)->SetUsesTextureCoordinates (true);
                            pMesh->FaceAt (i)->ActivateTextureCoordinatesIndices();
                            pMesh->FaceAt (i)->SetTexCoord(0, indices[i * 3]);
                            pMesh->FaceAt (i)->SetTexCoord(1, indices[i * 3 + 1]);
                            pMesh->FaceAt (i)->SetTexCoord(2, indices[i * 3 + 2]);
                        }
                    }
                } else if (indexAccessor.componentType == 5125) { // TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT = 5125
                    const uint32_t* indices = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                    for (size_t i = 0; i < triangleCount; i++) {
                        pMesh->FaceAt (i)->SetNVertices(3);
                        pMesh->FaceAt (i)->SetVertex(0, indices[i * 3]);
                        pMesh->FaceAt (i)->SetVertex(1, indices[i * 3 + 1]);
                        pMesh->FaceAt (i)->SetVertex(2, indices[i * 3 + 2]);
                        if (!pMesh->GetTextureCoordinates ().empty())
                        {
                            pMesh->FaceAt (i)->SetUsesTextureCoordinates (true);
                            pMesh->FaceAt (i)->ActivateTextureCoordinatesIndices();
                            pMesh->FaceAt (i)->SetTexCoord(0, indices[i * 3]);
                            pMesh->FaceAt (i)->SetTexCoord(1, indices[i * 3 + 1]);
                            pMesh->FaceAt (i)->SetTexCoord(2, indices[i * 3 + 2]);
                        }
                    }
                }
            }

            // Transformation miroir : on rend aux triangles leur orientation en
            // echangeant deux sommets. Fait ICI, et non aux trois endroits ou les
            // faces sont ecrites (sans indices, indices 16 bits, indices 32 bits).
            if (mirrored)
            {
                for (unsigned int i = 0; i < pMesh->GetNFaces(); ++i)
                {
                    auto face = pMesh->FaceAt (i);
                    if (!face || face->GetNVertices() != 3)
                        continue;
                    const unsigned int v1 = face->GetVertex(1);
                    const unsigned int v2 = face->GetVertex(2);
                    face->SetVertex(1, v2);
                    face->SetVertex(2, v1);
                    if (face->UsesTextureCoordinates())
                    {
                        const int t1 = face->GetTexCoordIndex(1);
                        const int t2 = face->GetTexCoordIndex(2);
                        face->SetTexCoord(1, (unsigned int)t2);
                        face->SetTexCoord(2, (unsigned int)t1);
                    }
                }
            }

            pMesh->SetName (gltfMesh.name);
            if (!hasNormals)
                pMesh->ComputeNormals();

            // POSE DES TANGENTES DU FICHIER, au meme point du flot que la
            // generation : la geometrie est figee, donc l'estampille posee ici
            // vaut la revision courante et AreTangentsValid() rend vrai.
            const bool hasTangents = !fileTangents.empty();
            if (hasTangents)
                pMesh->SetVertexTangents(std::move(fileTangents));

            // GENERATION, seulement si le materiau a une carte de normales et
            // que le fichier n'a pas fourni de TANGENT. APRES ComputeNormals :
            // l'orthogonalisation lit les normales par sommet.
            //
            // LE JEU EST CELUI QUE LA CARTE DE NORMALES ECHANTILLONNE, relu
            // apres la recriture des uvSet -- glTF 2.0 attache le TANGENT aux
            // coordonnees de la normalTexture, et une base batie sur une autre
            // parametrisation est valide, unitaire, et fausse sans signal.
            if (!hasTangents && needsTangentBasis)
            {
                const unsigned int normalUvSet =
                    (pbrOnMesh != nullptr && pbrOnMesh->HasMap(cgpbr::MapSlot::normal))
                        ? pbrOnMesh->GetMap(cgpbr::MapSlot::normal).uvSet : 0u;
                // Le refus est SILENCIEUX cote generateTangents -- il rend un
                // booleen. Sans cette trace, un maillage a carte de normales et
                // sans UV repartirait sans base tangente et sans rien qui le
                // dise, ce qui est precisement le cas ou l'eclairage sera faux.
                if (!generateTangents(*pMesh, normalUvSet))
                    fprintf (stderr,
                             "import_gltf: '%s' porte une carte de normales mais "
                             "aucune coordonnee de texture dans le jeu %u ; "
                             "maillage sans base tangente.\n",
                             gltfMesh.name.c_str(), normalUvSet);
            }

            vm.AddMesh(pMesh);
        }
    }

    return true;
}
