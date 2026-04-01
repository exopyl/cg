#include "vmeshes.h"

#include <map>

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

	// obj
	if (filename[size - 3] == 'o' && filename[size - 2] == 'b' && filename[size - 1] == 'j')
		res = export_obj(filename);

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

	int size = strlen(filename);

	// obj
	if (filename[size - 3] == '3' && filename[size - 2] == 'd' && filename[size - 1] == 's')
		res = import_3ds(filename);

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

bool VMeshes::export_stl(char* filename)
{
	return false;
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