#include "object3D.h"

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

//
//
//
bool Object3D::import_file (char *filename)
{
	bool res = false;

	clean ();
	
	int size = strlen (filename);

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

  // obj
  if (filename[size-3] == 'o' && filename[size-2] == 'b' && filename[size-1] == 'j')
    res = import_obj (filename);

  // 3ds
  //if (filename[size-3] == '3' && filename[size-2] == 'd' && filename[size-1] == 's')
  //  res = import_3ds (filename);

  return res;
}

//
//
//
bool Object3D::export_file (char *filename)
{
	return true;
}

// import / export
void Object3D::clean (void)
{
}

bool Object3D::import_obj (char *filename)
{
	return true;
}

bool Object3D::export_obj (char *filename)
{
	return true;
}

