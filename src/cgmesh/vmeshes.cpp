#include "vmeshes.h"

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

	for (auto& object : p->pObject)
	{
		auto pMesh = new Mesh();

		/*mat3 rot;
		mat3_init(rot,
			object.LocalCoordinateSystem[0][0], object.LocalCoordinateSystem[0][1], object.LocalCoordinateSystem[0][2],
			object.LocalCoordinateSystem[1][0], object.LocalCoordinateSystem[1][1], object.LocalCoordinateSystem[1][2],
			object.LocalCoordinateSystem[2][0], object.LocalCoordinateSystem[2][1], object.LocalCoordinateSystem[2][2]
		);
		mat3_transpose(rot);
		mat3_inverse(rot);*/

		Matrix3f rot2(
			object.LocalCoordinateSystem[0][0], object.LocalCoordinateSystem[0][1], object.LocalCoordinateSystem[0][2],
			object.LocalCoordinateSystem[1][0], object.LocalCoordinateSystem[1][1], object.LocalCoordinateSystem[1][2],
			object.LocalCoordinateSystem[2][0], object.LocalCoordinateSystem[2][1], object.LocalCoordinateSystem[2][2]
		);
		//rot2.Transpose();
		rot2.Inverse();

		unsigned int nVertices = object.numOfVerts;
		unsigned int nFaces = object.numOfFaces;
		pMesh->Init(nVertices, nFaces);

		for (unsigned int i = 0; i < nVertices; i++)
		{
			Vector3f pt(
				object.pVerts[i].fX,// - object.LocalCoordinateSystem[3][0],
				object.pVerts[i].fY,// - object.LocalCoordinateSystem[3][1],
				object.pVerts[i].fZ);// -object.LocalCoordinateSystem[3][2]);
			//rot2.TransformPoint(pt);
			pMesh->m_pVertices[3 * i] = pt.x;// +object.LocalCoordinateSystem[3][0];
			pMesh->m_pVertices[3 * i + 1] = pt.y;// +object.LocalCoordinateSystem[3][1];
			pMesh->m_pVertices[3 * i + 2] = pt.z;// +object.LocalCoordinateSystem[3][2];
		}

		for (unsigned int i = 0; i < nFaces; i++)
		{
			auto face = object.pFaces[i];

			pMesh->m_pFaces[i]->SetNVertices(3);
			for (unsigned int j = 0; j < 3; j++)
				pMesh->m_pFaces[i]->SetVertex(j, face.vertIndex[j]);
		}

		pMesh->m_name = std::string(object.strName);

		// Materials
		std::vector<int> materialMapping;
		for (int i = 0; i < p->numOfMaterials; i++)
		{
			auto& mat3ds = p->pMaterials[i];
			Material* pMaterial = nullptr;

			if (strlen(mat3ds.strFile) > 0)
			{
				pMaterial = new MaterialTexture(mat3ds.strFile);
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
				materialMapping.push_back(pMesh->Material_Add(pMaterial));
			}
			else
			{
				materialMapping.push_back(-1);
			}
		}

		// Assign materials to faces
		for (auto& matList : object.pFacesMaterialList)
		{
			if (matList.materialID >= 0 && matList.materialID < (int)materialMapping.size())
			{
				int meshMatId = materialMapping[matList.materialID];
				if (meshMatId != -1)
				{
					for (int i = 0; i < matList.numOfFaces; i++)
					{
						unsigned int faceIdx = matList.pFacesMaterialsList[i];
						if (faceIdx < nFaces)
						{
							pMesh->m_pFaces[faceIdx]->SetMaterialId(meshMatId);
						}
					}
				}
			}
		}

		m_Meshes.push_back(pMesh);
	}

	Free3DSModel(p);

	return true;
}

bool VMeshes::export_3ds(char* filename)
{
	return false;
}