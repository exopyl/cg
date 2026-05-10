#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "vmeshes.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "mesh_io_rply.h"
#include "mesh_io_3ds.h"

VMeshes::VMeshes ()
{
}

VMeshes::~VMeshes ()
{
	clean ();
}

unsigned int VMeshes::GetNVertices() const
{
	unsigned int n = 0;
	for (auto pMesh : m_Meshes)
		n += pMesh->GetNVertices();
	return n;
}

unsigned int VMeshes::GetNFaces() const
{
	unsigned int n = 0;
	for (auto pMesh : m_Meshes)
		n += pMesh->GetNFaces();
	return n;
}

size_t VMeshes::GetNMeshes() const
{
	return m_Meshes.size();
}

bool VMeshes::IsTriangleMesh() const
{
	for (auto pMesh : m_Meshes)
		if (!pMesh->IsTriangleMesh())
			return false;
	return true;
}

bool VMeshes::save (char *filename)
{
	bool res = false;

	// determine the format
	int size = strlen(filename);
	if (size < 4) return false;

	// obj
	if (filename[size - 3] == 'o' && filename[size - 2] == 'b' && filename[size - 1] == 'j')
		res = export_obj(filename);
	// stl (ASCII by default)
	else if (filename[size - 3] == 's' && filename[size - 2] == 't' && filename[size - 1] == 'l')
		res = export_stl(filename);

	return res;
}

void VMeshes::clean (void)
{
	for (auto pMesh : m_Meshes)
		delete pMesh;
	m_Meshes.clear();
}

bool VMeshes::load(char* filename)
{
	bool res = false;

	std::string fileStr(filename);
	std::string ext = "";
	size_t dotPos = fileStr.find_last_of(".");
	if (dotPos != std::string::npos) {
		ext = fileStr.substr(dotPos + 1);
		for (auto& c : ext) c = tolower(c);
	}

	// 3ds
	if (ext == "3ds")
		res = import_3ds(filename);

	// gltf
	if (ext == "gltf")
		res = import_gltf(filename);

	// glb
	if (ext == "glb")
		res = import_gltf(filename);

	if (res)
		return res;

	Mesh* pMesh = new Mesh();
	if (pMesh->load(filename) == 0)
	{
		m_Meshes.push_back(pMesh);
		return true;
	}
	return false;
}

bool VMeshes::export_obj(char* filename)
{
	return false;
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
bool VMeshes::export_stl(char* filename)
{
	if (!filename) return false;
	FILE *fp = fopen(filename, "w");
	if (!fp) return false;

	std::string name = stemFromPath(filename);
	std::vector<Tri> tris = collectTriangles(m_Meshes);

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
bool VMeshes::export_stl_binary(char* filename)
{
	if (!filename) return false;
	FILE *fp = fopen(filename, "wb");
	if (!fp) return false;

	char header[80] = {0};
	{
		std::string name = stemFromPath(filename);
		std::strncpy(header, name.c_str(), sizeof(header));
	}
	fwrite(header, 1, 80, fp);

	std::vector<Tri> tris = collectTriangles(m_Meshes);
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

bool VMeshes::export_ply(char* filename)
{
	return false;
}

bool VMeshes::import_3ds(char* filename)
{
	t3DSModel* p = Load3DSFile(filename, nullptr);
	if (!p) return false;

	for (auto& object : p->pObject)
	{
		auto pMesh = new Mesh();

		Matrix3f rot2(
			object.LocalCoordinateSystem[0][0], object.LocalCoordinateSystem[0][1], object.LocalCoordinateSystem[0][2],
			object.LocalCoordinateSystem[1][0], object.LocalCoordinateSystem[1][1], object.LocalCoordinateSystem[1][2],
			object.LocalCoordinateSystem[2][0], object.LocalCoordinateSystem[2][1], object.LocalCoordinateSystem[2][2]
		);
		rot2.Inverse();

		unsigned int nVertices = object.numOfVerts;
		unsigned int nFaces = object.numOfFaces;
		pMesh->Init(nVertices, nFaces);

		for (unsigned int i = 0; i < nVertices; i++)
		{
			pMesh->m_pVertices[3 * i] = object.pVerts[i].fX;
			pMesh->m_pVertices[3 * i + 1] = object.pVerts[i].fY;
			pMesh->m_pVertices[3 * i + 2] = object.pVerts[i].fZ;
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
					pMaterial = new MaterialColor(mat3ds.sMaterial.Diffuse.r, mat3ds.sMaterial.Diffuse.g, mat3ds.sMaterial.Diffuse.b);
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

		m_Meshes.push_back(pMesh);
	}

	Free3DSModel(p);

	return true;
}

bool VMeshes::export_3ds(char* filename)
{
	return false;
}
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

bool DummyLoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
    std::string* warn, int req_width, int req_height,
    const unsigned char* bytes, int size, void* user_data)
{
    return true;
}

bool VMeshes::import_gltf(char* filename)
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
            if (primitive.mode != 4) continue; // TINYGLTF_MODE_TRIANGLES = 4

            auto pMesh = new Mesh();
            
            // Positions
            if (primitive.attributes.find("POSITION") == primitive.attributes.end()) continue;
            const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

            // Indices
            if (primitive.indices < 0) continue;
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

            pMesh->Init(posAccessor.count, indexAccessor.count / 3);

            // Map glTF material to Mesh material (base color only)
            if (primitive.material >= 0 && primitive.material < (int)model.materials.size()) {
                const auto& gltfMat = model.materials[primitive.material];
                auto pMatExt = new MaterialColorExt();
                pMatExt->SetName(gltfMat.name);
                const auto& baseColor = gltfMat.pbrMetallicRoughness.baseColorFactor;
                pMatExt->SetDiffuse(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
                pMatExt->SetAmbient(0.2f, 0.2f, 0.2f, 1.0f);
                pMatExt->SetSpecular(0.5f, 0.5f, 0.5f, 1.0f);
                pMatExt->SetShininess(32.0f);

                int matId = pMesh->Material_Add(pMatExt);
                pMesh->ApplyMaterial(matId); // Set default material for all faces
            }

            for (size_t i = 0; i < posAccessor.count; i++) {
                pMesh->SetVertex(i, positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
            }

            // Normals
            bool hasNormals = false;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];
                const float* normals = reinterpret_cast<const float*>(&normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);

                if (normAccessor.count == posAccessor.count) {
                    for (size_t i = 0; i < normAccessor.count; i++) {
                        pMesh->m_pVertexNormals[i * 3] = normals[i * 3];
                        pMesh->m_pVertexNormals[i * 3 + 1] = normals[i * 3 + 1];
                        pMesh->m_pVertexNormals[i * 3 + 2] = normals[i * 3 + 2];
                    }
                    hasNormals = true;
                }
            }

            if (indexAccessor.componentType == 5123) { // TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT = 5123
                const uint16_t* indices = reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                for (size_t i = 0; i < indexAccessor.count / 3; i++) {
                    pMesh->m_pFaces[i]->SetNVertices(3);
                    pMesh->m_pFaces[i]->SetVertex(0, indices[i * 3]);
                    pMesh->m_pFaces[i]->SetVertex(1, indices[i * 3 + 1]);
                    pMesh->m_pFaces[i]->SetVertex(2, indices[i * 3 + 2]);
                }
            } else if (indexAccessor.componentType == 5125) { // TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT = 5125
                const uint32_t* indices = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                for (size_t i = 0; i < indexAccessor.count / 3; i++) {
                    pMesh->m_pFaces[i]->SetNVertices(3);
                    pMesh->m_pFaces[i]->SetVertex(0, indices[i * 3]);
                    pMesh->m_pFaces[i]->SetVertex(1, indices[i * 3 + 1]);
                    pMesh->m_pFaces[i]->SetVertex(2, indices[i * 3 + 2]);
                }
            }

            pMesh->m_name = gltfMesh.name;
            if (!hasNormals)
                pMesh->ComputeNormals();
            m_Meshes.push_back(pMesh);
        }
    }

    return true;
}
