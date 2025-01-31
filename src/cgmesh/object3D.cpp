#include "object3D.h"
#include "mesh_io_3ds.h"

//
//
//
Object3D::Object3D ()
{
	m_listMeshes.clear ();
}

//
//
//
Object3D::~Object3D ()
{
	for (list<Mesh*>::iterator it = m_listMeshes.begin (); it != m_listMeshes.end (); it++)
	{
		Mesh *pMesh = (*it);
		if (pMesh)
			delete pMesh;
	}
	m_listMeshes.clear ();
}


unsigned int Object3D::GetNVertices() const
{
	unsigned int nVertices = 0;

	for (const auto& mesh : m_listMeshes)
		nVertices += mesh->GetNVertices();

	return nVertices;
}

unsigned int Object3D::GetNFaces() const
{
	unsigned int nFaces = 0;

	for (const auto& mesh : m_listMeshes)
		nFaces += mesh->GetNFaces();

	return nFaces;
}


size_t Object3D::GetNMeshes() const
{
	return m_listMeshes.size();
}

bool Object3D::IsTriangleMesh() const
{
	for (const auto& mesh : m_listMeshes)
		if (!mesh->IsTriangleMesh())
			return false;
	return true;
}

//
//
//
bool Object3D::import_file (char *filename)
{
	bool res = false;

	clean ();
	
  // check if the filename exists
  FILE *ptr = fopen (filename, "r");
  if (ptr == NULL)
    {
      return false;
    }
  else
  {
    fclose (ptr);
  }

  // determine the format
  int size = strlen(filename);

  // obj
  if (filename[size - 3] == 'o' && filename[size - 2] == 'b' && filename[size - 1] == 'j')
	  res = import_obj(filename);

  // stl
  if (filename[size - 3] == 's' && filename[size - 2] == 't' && filename[size - 1] == 'l')
	  res = import_obj(filename);

  // 3ds
  if (filename[size-3] == '3' && filename[size-2] == 'd' && filename[size-1] == 's')
    res = import_3ds (filename);

  return res;
}

//
//
//
bool Object3D::export_file (char *filename)
{
	bool res = false;

	// determine the format
	int size = strlen(filename);

	// obj
	if (filename[size - 3] == 'o' && filename[size - 2] == 'b' && filename[size - 1] == 'j')
		res = export_obj(filename);

	return res;

}

// import / export
void Object3D::clean (void)
{
}

bool Object3D::import_obj(char* filename)
{
	auto pMesh = new Mesh();
	pMesh->load(filename);

	m_listMeshes.push_back(pMesh);

	return true;
}

bool Object3D::export_obj(char* filename)
{
	if (!m_listMeshes.empty())
	{
		auto pMesh = m_listMeshes.front();
		pMesh->save(filename);
	}

	return true;
}

bool Object3D::import_stl(char* filename)
{
	auto pMesh = new Mesh();
	pMesh->load(filename);

	m_listMeshes.push_back(pMesh);

	return true;
}

bool Object3D::export_stl(char* filename)
{
	if (!m_listMeshes.empty())
	{
		auto pMesh = m_listMeshes.front();
		pMesh->save(filename);
	}

	return true;
}

bool Object3D::import_3ds(char* filename)
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

		m_listMeshes.push_back(pMesh);
	}

	Free3DSModel(p);

	return true;
}

bool Object3D::export_3ds(char* filename)
{
	return true;
}

