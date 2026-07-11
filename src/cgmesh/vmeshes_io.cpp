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
#include <string>
#include <vector>

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
	// stl (ASCII by default)
	else if (filename[size - 3] == 's' && filename[size - 2] == 't' && filename[size - 1] == 'l')
		res = export_stl(vm, filename);

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

	// Deep-copy a Mesh material (only the concrete types cgmesh instantiates).
	// A Mesh owns its materials (unique_ptr), so submeshes need their own copies.
	Material* objCloneMaterial(Material* m)
	{
		if (!m) return nullptr;
		Material* copy = nullptr;
		switch (m->GetType())
		{
		case MATERIAL_TEXTURE:   copy = new MaterialTexture(*static_cast<MaterialTexture*>(m)); break;
		case MATERIAL_COLOR_ADV: copy = new MaterialColorExt(*static_cast<MaterialColorExt*>(m)); break;
		case MATERIAL_COLOR:     copy = new MaterialColor(*static_cast<MaterialColor*>(m)); break;
		default:                 return nullptr;
		}
		// The material copy-ctors don't carry the base m_name over, so set it
		// explicitly to keep name-based lookups / export working per submesh.
		if (copy) copy->SetName(m->GetName());
		return copy;
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
	if ((unsigned int)faceObject.size() != flat->m_nFaces)
		nObjects = (nObjects <= 1) ? nObjects : 0;

	// 3a. Zero/one object -> keep the flattened mesh (fast path, no remap).
	if (nObjects <= 1)
	{
		if (nObjects == 1) flat->m_name = objNames[0];
		vm.AddMesh(flat);
		return true;
	}

	// 3b. One submesh per object.
	for (int obj = 0; obj < nObjects; obj++)
	{
		std::vector<unsigned int> faces;
		for (unsigned int fi = 0; fi < flat->m_nFaces; fi++)
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
			Face* f = flat->m_pFaces[fi];
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
			Face* f = flat->m_pFaces[fi];
			if (f->m_bUseTextureCoordinates && f->m_pTextureCoordinatesIndices)
			{
				anyUV = true;
				for (int c = 0; c < f->GetNVertices(); c++)
					localUV((int)f->m_pTextureCoordinatesIndices[c]);
			}
		}

		Mesh* sub = new Mesh();
		sub->Init((unsigned int)vmap.size(), (unsigned int)faces.size());
		sub->m_name = objNames[obj];

		for (auto& kv : vmap)
		{
			int g = kv.first, l = kv.second;
			sub->m_pVertices[3 * l]     = flat->m_pVertices[3 * g];
			sub->m_pVertices[3 * l + 1] = flat->m_pVertices[3 * g + 1];
			sub->m_pVertices[3 * l + 2] = flat->m_pVertices[3 * g + 2];
		}

		if (anyUV && !uvmap.empty())
		{
			unsigned int nUV = (unsigned int)uvmap.size();
			sub->m_nTextureCoordinates = nUV;
			sub->m_pTextureCoordinates.assign(2 * nUV, 0.0f);
			for (auto& kv : uvmap)
			{
				int g = kv.first, l = kv.second;
				if (2u * (unsigned int)g + 1u < flat->m_pTextureCoordinates.size())
				{
					sub->m_pTextureCoordinates[2 * l]     = flat->m_pTextureCoordinates[2 * g];
					sub->m_pTextureCoordinates[2 * l + 1] = flat->m_pTextureCoordinates[2 * g + 1];
				}
			}
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
			Face* src = flat->m_pFaces[faces[k]];
			Face* dst = sub->m_pFaces[k];
			int nv = src->GetNVertices();
			dst->SetNVertices((unsigned int)nv);
			for (int c = 0; c < nv; c++)
				dst->SetVertex((unsigned int)c, (unsigned int)localVert(src->GetVertex(c)));

			if (src->m_bUseTextureCoordinates && src->m_pTextureCoordinatesIndices)
			{
				dst->m_bUseTextureCoordinates = true;
				dst->ActivateTextureCoordinatesIndices();
				for (int c = 0; c < nv; c++)
					dst->SetTexCoord((unsigned int)c,
					                 (unsigned int)localUV((int)src->m_pTextureCoordinatesIndices[c]));
			}

			dst->SetMaterialId(localMat(src->m_iMaterialId));
		}

		for (auto& pl : objPolylines[obj])
			for (size_t i = 1; i < pl.size(); i++)
			{
				sub->m_pLines.push_back((unsigned int)localVert(pl[i - 1]));
				sub->m_pLines.push_back((unsigned int)localVert(pl[i]));
			}
		for (int g : objPoints[obj])
			sub->m_pPoints.push_back((unsigned int)localVert(g));

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
	// pre-computed normals. Non-triangle faces and out-of-range indices skipped.
	std::vector<Tri> collectTriangles(const std::vector<Mesh*> &meshes)
	{
		std::vector<Tri> tris;
		for (Mesh *m : meshes)
		{
			if (!m) continue;
			for (unsigned int i = 0; i < m->m_nFaces; ++i)
			{
				Face *f = m->m_pFaces[i];
				if (!f || f->GetNVertices() != 3) continue;
				int a = f->GetVertex(0), b = f->GetVertex(1), c = f->GetVertex(2);
				if (a < 0 || b < 0 || c < 0) continue;
				if ((unsigned)a >= m->m_nVertices || (unsigned)b >= m->m_nVertices || (unsigned)c >= m->m_nVertices) continue;

				Tri t;
				t.ax = m->m_pVertices[3*a];   t.ay = m->m_pVertices[3*a+1]; t.az = m->m_pVertices[3*a+2];
				t.bx = m->m_pVertices[3*b];   t.by = m->m_pVertices[3*b+1]; t.bz = m->m_pVertices[3*b+2];
				t.cx = m->m_pVertices[3*c];   t.cy = m->m_pVertices[3*c+1]; t.cz = m->m_pVertices[3*c+2];

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
				pMesh->m_pVertices[3 * i]     = w.x;
				pMesh->m_pVertices[3 * i + 1] = w.z;
				pMesh->m_pVertices[3 * i + 2] = -w.y;
			}
			else
			{
				pMesh->m_pVertices[3 * i]     = object.pVerts[i].fX;
				pMesh->m_pVertices[3 * i + 1] = object.pVerts[i].fY;
				pMesh->m_pVertices[3 * i + 2] = object.pVerts[i].fZ;
			}
		}

		// Load normals if present
		if (object.pNormals)
		{
			for (unsigned int i = 0; i < nVertices; i++)
			{
				pMesh->m_pVertexNormals[3 * i] = object.pNormals[i].fX;
				pMesh->m_pVertexNormals[3 * i + 1] = object.pNormals[i].fY;
				pMesh->m_pVertexNormals[3 * i + 2] = object.pNormals[i].fZ;
			}
		}

		for (unsigned int i = 0; i < nFaces; i++)
		{
			auto face = object.pFaces[i];

			pMesh->m_pFaces[i]->SetNVertices(3);
			for (unsigned int j = 0; j < 3; j++)
				pMesh->m_pFaces[i]->SetVertex(j, face.vertIndex[j]);
		}

		pMesh->m_name = std::string(object.strName);

		// Texture coordinates. 3DS stores one UV per vertex (parallel to the
		// position array), which is exactly the per-vertex layout the VBO /
		// polygon render path expects. Populate the mesh-level UV array, and
		// mirror the indices onto each face for the immediate-mode path.
		if (object.pTexVerts && object.numTexVertex > 0)
		{
			const unsigned int nUV = (unsigned int)object.numTexVertex;
			pMesh->m_pTextureCoordinates.assign(2 * nVertices, 0.0f);
			for (unsigned int i = 0; i < nVertices && i < nUV; i++)
			{
				pMesh->m_pTextureCoordinates[2 * i]     = object.pTexVerts[i].fU;
				// 3DS stores V with origin at the bottom; OpenGL samples the
				// first uploaded row at V=0 (top of the image). Flip V.
				pMesh->m_pTextureCoordinates[2 * i + 1] = 1.0f - object.pTexVerts[i].fV;
			}
			for (unsigned int i = 0; i < nFaces; i++)
			{
				Face* pFace = pMesh->m_pFaces[i];
				pFace->ActivateTextureCoordinatesIndices();
				pFace->m_bUseTextureCoordinates = true;
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
					// model's directory (PNG now supported via cgimg). The 3DS
					// diffuse/ambient/specular tint the texture under lighting
					// (GL_MODULATE) — e.g. a light rubber tread darkened by a
					// grey diffuse.
					auto pTex = new MaterialTexture(mat3ds.strFile,
					                                modelDir.empty() ? nullptr : modelDir.c_str());
					pTex->SetAmbient(mat3ds.sMaterial.Ambient.r / 255.f, mat3ds.sMaterial.Ambient.g / 255.f, mat3ds.sMaterial.Ambient.b / 255.f, 1.f);
					pTex->SetDiffuse(mat3ds.sMaterial.Diffuse.r / 255.f, mat3ds.sMaterial.Diffuse.g / 255.f, mat3ds.sMaterial.Diffuse.b / 255.f, 1.f);
					pTex->SetSpecular(mat3ds.sMaterial.Specular.r / 255.f, mat3ds.sMaterial.Specular.g / 255.f, mat3ds.sMaterial.Specular.b / 255.f, 1.f);
					pTex->SetShininess(mat3ds.sMaterial.Power / 100.f);
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
						pMesh->m_pFaces[faceIdx]->SetMaterialId(meshMatId);
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
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
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
template <typename T>
const T* GetAccessorDataPtr(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    if (accessor.bufferView < 0)
        return nullptr;

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    return reinterpret_cast<const T*>(&buffer.data[view.byteOffset + accessor.byteOffset]);
}

bool CopyFloatAccessorVec2(std::vector<float>& dst, const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    if (accessor.type != TINYGLTF_TYPE_VEC2 || accessor.bufferView < 0)
        return false;

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    const unsigned char* data = buffer.data.data() + view.byteOffset + accessor.byteOffset;
    const int stride = accessor.ByteStride(view);
    const int componentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
    if (componentSize <= 0 || stride < 2 * componentSize)
        return false;

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
            return 0.0f;
        }
    };

    dst.resize(accessor.count * 2);
    for (size_t i = 0; i < accessor.count; ++i)
    {
        const unsigned char* uv = data + i * stride;
        dst[2 * i] = convertComponent(uv);
        dst[2 * i + 1] = convertComponent(uv + componentSize);
    }
    return true;
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

    for (const auto& gltfMesh : model.meshes) {
        for (const auto& primitive : gltfMesh.primitives) {
			if (primitive.mode != 4)
			{
				continue; // TINYGLTF_MODE_TRIANGLES = 4
			}

            // Positions
			if (primitive.attributes.find("POSITION") == primitive.attributes.end())
			{
				continue;
			}

			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const float* positions = GetAccessorDataPtr<float>(model, posAccessor);
            if (!positions || posAccessor.type != TINYGLTF_TYPE_VEC3 || posAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
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
                indexAccessorPtr = &model.accessors[primitive.indices];
                if (indexAccessorPtr->bufferView < 0)
                {
                    continue;
                }
                indexViewPtr = &model.bufferViews[indexAccessorPtr->bufferView];
                indexBufferPtr = &model.buffers[indexViewPtr->buffer];
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

            // Map glTF material to Mesh material
            if (primitive.material >= 0 && primitive.material < (int)model.materials.size()) {
                const auto& gltfMat = model.materials[primitive.material];
                Material* pMaterial = nullptr;
                const auto& baseColor = gltfMat.pbrMetallicRoughness.baseColorFactor;
                const auto& baseColorTexture = gltfMat.pbrMetallicRoughness.baseColorTexture;

                if (baseColorTexture.index >= 0 && baseColorTexture.index < (int)model.textures.size())
                {
                    const tinygltf::Texture& gltfTexture = model.textures[baseColorTexture.index];
                    if (gltfTexture.source >= 0 && gltfTexture.source < (int)model.images.size())
                    {
                        const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
                        if (!gltfImage.image.empty() && gltfImage.width > 0 && gltfImage.height > 0)
                        {
                            pMaterial = new MaterialTexture(
                                gltfImage.name.empty() ? gltfMat.name : gltfImage.name,
                                static_cast<unsigned int>(gltfImage.width),
                                static_cast<unsigned int>(gltfImage.height),
                                gltfImage.image.data());
                            texCoordSet = baseColorTexture.texCoord;
                        }
                    }
                }

                if (!pMaterial)
                {
                    auto pMatExt = new MaterialColorExt();
                    pMatExt->SetName(gltfMat.name);
                    pMatExt->SetDiffuse(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
                    pMatExt->SetAmbient(0.2f, 0.2f, 0.2f, 1.0f);
                    pMatExt->SetSpecular(0.5f, 0.5f, 0.5f, 1.0f);
                    pMatExt->SetShininess(32.0f);
                    pMaterial = pMatExt;
                }

                pMaterial->SetName(gltfMat.name);
                int matId = pMesh->Material_Add(pMaterial);
                pMesh->ApplyMaterial(matId); // Set default material for all faces
            }

            for (size_t i = 0; i < posAccessor.count; i++) {
                pMesh->SetVertex(i, positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
            }

            // Normals
            bool hasNormals = false;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const float* normals = GetAccessorDataPtr<float>(model, normAccessor);

                if (normals &&
                    normAccessor.count == posAccessor.count &&
                    normAccessor.type == TINYGLTF_TYPE_VEC3 &&
                    normAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    for (size_t i = 0; i < normAccessor.count; i++) {
                        pMesh->m_pVertexNormals[i * 3] = normals[i * 3];
                        pMesh->m_pVertexNormals[i * 3 + 1] = normals[i * 3 + 1];
                        pMesh->m_pVertexNormals[i * 3 + 2] = normals[i * 3 + 2];
                    }
                    hasNormals = true;
                }
            }

            std::vector<float> textureCoordinates;
            const std::string texCoordAttribute = "TEXCOORD_" + std::to_string(texCoordSet);
            auto texCoordIt = primitive.attributes.find(texCoordAttribute);
            if (texCoordIt != primitive.attributes.end())
            {
                const tinygltf::Accessor& texAccessor = model.accessors[texCoordIt->second];
                if (CopyFloatAccessorVec2(textureCoordinates, model, texAccessor))
                {
                    pMesh->m_nTextureCoordinates = static_cast<unsigned int>(texAccessor.count);
                    pMesh->m_pTextureCoordinates = textureCoordinates;
                }
            }

            if (!hasIndices)
            {
                for (size_t i = 0; i < triangleCount; i++) {
                    pMesh->m_pFaces[i]->SetNVertices(3);
                    pMesh->m_pFaces[i]->SetVertex(0, static_cast<unsigned int>(i * 3));
                    pMesh->m_pFaces[i]->SetVertex(1, static_cast<unsigned int>(i * 3 + 1));
                    pMesh->m_pFaces[i]->SetVertex(2, static_cast<unsigned int>(i * 3 + 2));
                    if (!pMesh->m_pTextureCoordinates.empty())
                    {
                        pMesh->m_pFaces[i]->m_bUseTextureCoordinates = true;
                        pMesh->m_pFaces[i]->ActivateTextureCoordinatesIndices();
                        pMesh->m_pFaces[i]->SetTexCoord(0, static_cast<unsigned int>(i * 3));
                        pMesh->m_pFaces[i]->SetTexCoord(1, static_cast<unsigned int>(i * 3 + 1));
                        pMesh->m_pFaces[i]->SetTexCoord(2, static_cast<unsigned int>(i * 3 + 2));
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
                        pMesh->m_pFaces[i]->SetNVertices(3);
                        pMesh->m_pFaces[i]->SetVertex(0, indices[i * 3]);
                        pMesh->m_pFaces[i]->SetVertex(1, indices[i * 3 + 1]);
                        pMesh->m_pFaces[i]->SetVertex(2, indices[i * 3 + 2]);
                        if (!pMesh->m_pTextureCoordinates.empty())
                        {
                            pMesh->m_pFaces[i]->m_bUseTextureCoordinates = true;
                            pMesh->m_pFaces[i]->ActivateTextureCoordinatesIndices();
                            pMesh->m_pFaces[i]->SetTexCoord(0, indices[i * 3]);
                            pMesh->m_pFaces[i]->SetTexCoord(1, indices[i * 3 + 1]);
                            pMesh->m_pFaces[i]->SetTexCoord(2, indices[i * 3 + 2]);
                        }
                    }
                } else if (indexAccessor.componentType == 5125) { // TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT = 5125
                    const uint32_t* indices = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                    for (size_t i = 0; i < triangleCount; i++) {
                        pMesh->m_pFaces[i]->SetNVertices(3);
                        pMesh->m_pFaces[i]->SetVertex(0, indices[i * 3]);
                        pMesh->m_pFaces[i]->SetVertex(1, indices[i * 3 + 1]);
                        pMesh->m_pFaces[i]->SetVertex(2, indices[i * 3 + 2]);
                        if (!pMesh->m_pTextureCoordinates.empty())
                        {
                            pMesh->m_pFaces[i]->m_bUseTextureCoordinates = true;
                            pMesh->m_pFaces[i]->ActivateTextureCoordinatesIndices();
                            pMesh->m_pFaces[i]->SetTexCoord(0, indices[i * 3]);
                            pMesh->m_pFaces[i]->SetTexCoord(1, indices[i * 3 + 1]);
                            pMesh->m_pFaces[i]->SetTexCoord(2, indices[i * 3 + 2]);
                        }
                    }
                }
            }

            pMesh->m_name = gltfMesh.name;
            if (!hasNormals)
                pMesh->ComputeNormals();
            vm.AddMesh(pMesh);
        }
    }

    return true;
}
