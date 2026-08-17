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
  // Tampon de ligne sur la pile : la taille est connue et bornee, et surtout le
  // `buffer = fgets (buffer, ...)` d'origine ecrasait le pointeur d'allocation
  // (fgets renvoie nullptr en fin de fichier) -- le bloc etait perdu, le free
  // final ne liberait rien, et le sscanf suivant lisait nullptr sur erreur d'E/S.
  char buffer[512] = {};
  // prefix etait un char[10] rempli par un "%s" NON borne : tout premier jeton de
  // plus de 9 caracteres debordait la pile. La largeur est desormais bornee.
  char prefix[64] = {};

  ptr = fopen (filename, "r");
  if (!ptr) return false;

  // determine the number of vertices and the number of faces
  unsigned int nv = 0, nf = 0;
  while (fgets (buffer, sizeof (buffer), ptr))
  {
    prefix[0] = '\0';   // une ligne vide laissait sinon le prefixe precedent
    if (sscanf (buffer, "%63s", prefix) != 1) continue;
    if (!strcmp(prefix, "v")) nv++;
    if (!strcmp(prefix, "f")) nf++;
  }
  Init (nv, nf);

  InitVertexNormals();
  vt = (float*)malloc(2*GetNVertices ()*sizeof(float));
  assert (vt);

  tangent = (float*)malloc(3*GetNVertices ()*sizeof(float));
  assert (tangent);
  binormal = (float*)malloc(3*GetNVertices ()*sizeof(float));
  assert (binormal);
  tangentSpaceLight = (float*)malloc(3*GetNVertices ()*sizeof(float));
  assert (tangentSpaceLight);   // testait `tangent` : copier-coller


  /* get the vertices and the faces */
  vertex_walk = 0;
  face_walk   = 0;
  rewind (ptr);

  while (fgets (buffer, sizeof (buffer), ptr))
  {
    prefix[0] = '\0';
    if (sscanf (buffer, "%63s", prefix) != 1) continue;
    // Les deux passes ne comptent pas les lignes de la meme facon (celle-ci
    // consomme 4 lignes de plus par sommet) : sans ces bornes, un fichier avec
    // des lignes vides faisait diverger les compteurs et ecrire hors de
    // GetVertices () / m_pFaces.
    if (!strcmp(prefix, "v") && vertex_walk < (int)GetNVertices ())
      {
        // position
        float p[3] = {};
        sscanf (buffer, "%63s %f %f %f", prefix, &p[0], &p[1], &p[2]);
        SetVertex ((unsigned int)vertex_walk, p[0], p[1], p[2]);

        // normale
        if (!fgets (buffer, sizeof (buffer), ptr)) break;   // fichier tronque
        float n[3] = {};
        sscanf (buffer, "%63s %f %f %f", prefix, &n[0], &n[1], &n[2]);
        SetVertexNormal ((unsigned int)vertex_walk, n[0], n[1], n[2]);

        // tangent
        if (!fgets (buffer, sizeof (buffer), ptr)) break;
        sscanf (buffer, "%63s %f %f %f", prefix,
            &tangent[3*vertex_walk],
            &tangent[3*vertex_walk+1],
            &tangent[3*vertex_walk+2]);
        //printf ("%f %f %f\n", tangent[3*vertex_walk], tangent[3*vertex_walk+1], tangent[3*vertex_walk+2]);

        // binormal
        if (!fgets (buffer, sizeof (buffer), ptr)) break;
        sscanf (buffer, "%63s %f %f %f", prefix,
            &binormal[3*vertex_walk],
            &binormal[3*vertex_walk+1],
            &binormal[3*vertex_walk+2]);
        //printf ("%f %f %f\n", binormal[3*vertex_walk], binormal[3*vertex_walk+1], binormal[3*vertex_walk+2]);

        // texture
        if (!fgets (buffer, sizeof (buffer), ptr)) break;
        sscanf (buffer, "%63s %f %f", prefix,
            &vt[2*vertex_walk],
            &vt[2*vertex_walk+1]);
        //printf ("%f %f\n", vt[2*vertex_walk], vt[2*vertex_walk+1]);

        vertex_walk++;
      }
   if (!strcmp(prefix, "f") && face_walk < (int)GetNFaces ())
      {
       char i1[64] = {}, i2[64] = {}, i3[64] = {};
        // `&i1` etait un char(*)[64] la ou "%s" attend un char* -- meme adresse,
        // mais type faux. Et les "%s" non bornes debordaient les tampons.
        if (sscanf (buffer, "%63s %63s %63s %63s", prefix, i1, i2, i3) != 4) continue;

	// strtok renvoie nullptr sur un jeton vide : atoi(nullptr) dereferencait
	const char *t1 = strtok (i1, "/");
	const char *t2 = strtok (i2, "/");
	const char *t3 = strtok (i3, "/");
	if (!t1 || !t2 || !t3) continue;
	int a = atoi(t1)-1;
	int b = atoi(t2)-1;
	int c = atoi(t3)-1;
        //printf ("%d %d %d\n", a, b, c);
	FaceAt (face_walk)->SetTriangle (a, b, c);

        face_walk++;
      }
  }

  fclose (ptr);

  return 0;
}
