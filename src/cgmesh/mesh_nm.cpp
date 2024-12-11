#include <assert.h>

#include "mesh_nm.h"

Mesh_nm::Mesh_nm ():
	Mesh ()
{
}

Mesh_nm::~Mesh_nm ()
{
}

int Mesh_nm::load (char *filename)
{
	printf ("[mesh_nm::load]\n");
	if (strcmp (filename+(strlen(filename)-6), ".objnm") == 0)
		return import_objnm (filename);
	else
		return Mesh::load (filename);
}



/**
*
* import a mesh and the normal map containing in a file based on the obj format
*
*/
int Mesh_nm::import_objnm (char *filename)
{
  FILE *ptr;
  int vertex_walk, face_walk;
  char *buffer;
  char prefix[10];

  ptr = fopen (filename, "r");
  if (!ptr) return false;

  buffer = (char*)malloc(512*sizeof(char));
  if (!buffer) return false;

  // determine the number of vertices and the number of faces
  m_nVertices = 0;
  m_nFaces = 0;
  while (1)
  {
    buffer = fgets (buffer, 512, ptr);
    if (feof(ptr)) break;
    sscanf (buffer, "%s", prefix);
    if (!strcmp(prefix, "v")) m_nVertices++;
    if (!strcmp(prefix, "f")) m_nFaces++;
  }
  Init (m_nVertices, m_nFaces);

  m_pVertexNormals = (float*)malloc(3*m_nVertices*sizeof(float));
  assert (m_pVertexNormals);
  vt = (float*)malloc(2*m_nVertices*sizeof(float));
  assert (vt);

  tangent = (float*)malloc(3*m_nVertices*sizeof(float));
  assert (tangent);
  binormal = (float*)malloc(3*m_nVertices*sizeof(float));
  assert (binormal);
  tangentSpaceLight = (float*)malloc(3*m_nVertices*sizeof(float));
  assert (tangent);


  /* get the vertices and the faces */
  vertex_walk = 0;
  face_walk   = 0;
  rewind (ptr);
  buffer = (char*)malloc(512*sizeof(char));

  while (1)
  {
    buffer = fgets (buffer, 512, ptr);
    if (feof(ptr)) break;
    sscanf (buffer, "%s", prefix);
    if (!strcmp(prefix, "v"))
      {
        // position
        sscanf (buffer, "%s %f %f %f", prefix,
            &m_pVertices[3*vertex_walk],
            &m_pVertices[3*vertex_walk+1],
            &m_pVertices[3*vertex_walk+2]);
        //printf ("%f %f %f\n", v[3*vertex_walk], v[3*vertex_walk+1], v[3*vertex_walk+2]);

        // normale
        buffer = fgets (buffer, 512, ptr);
        sscanf (buffer, "%s %f %f %f", prefix,
            &m_pVertexNormals[3*vertex_walk],
            &m_pVertexNormals[3*vertex_walk+1],
            &m_pVertexNormals[3*vertex_walk+2]);
        //printf ("%f %f %f\n", vn[3*vertex_walk], vn[3*vertex_walk+1], vn[3*vertex_walk+2]);

        // tangent
        buffer = fgets (buffer, 512, ptr);
        sscanf (buffer, "%s %f %f %f", prefix,
            &tangent[3*vertex_walk],
            &tangent[3*vertex_walk+1],
            &tangent[3*vertex_walk+2]);
        //printf ("%f %f %f\n", tangent[3*vertex_walk], tangent[3*vertex_walk+1], tangent[3*vertex_walk+2]);

        // binormal
        buffer = fgets (buffer, 512, ptr);
        sscanf (buffer, "%s %f %f %f", prefix,
            &binormal[3*vertex_walk],
            &binormal[3*vertex_walk+1],
            &binormal[3*vertex_walk+2]);
        //printf ("%f %f %f\n", binormal[3*vertex_walk], binormal[3*vertex_walk+1], binormal[3*vertex_walk+2]);

        // texture
        buffer = fgets (buffer, 512, ptr);
        sscanf (buffer, "%s %f %f", prefix,
            &vt[2*vertex_walk],
            &vt[2*vertex_walk+1]);
        //printf ("%f %f\n", vt[2*vertex_walk], vt[2*vertex_walk+1]);

        vertex_walk++;
      }
   if (!strcmp(prefix, "f"))
      {
       char i1[64], i2[64], i3[64];
        sscanf (buffer, "%s %s %s %s", prefix, &i1, &i2, &i3);
	
	int a = atoi(strtok (i1, "/"))-1;
	int b = atoi(strtok (i2, "/"))-1;
	int c = atoi(strtok (i3, "/"))-1;
        //printf ("%d %d %d\n", a, b, c);
	m_pFaces[face_walk] = new Face ();
	m_pFaces[face_walk]->SetTriangle (a, b, c);

        face_walk++;
      }
  }

  fclose (ptr);
  free (buffer);

  return 0;
}
