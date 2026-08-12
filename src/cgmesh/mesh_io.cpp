#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mesh.h"
#include "mesh_io.h"
#include "mesh_io_3ds.h"
#include "../cgmath/cgmath.h"
#include "endianness.h"

int MeshIO::load (Mesh& mesh, const char *filename)
{
	int res = -1;
	if (!filename)
		return res;

	std::filesystem::path p(filename);
	std::string ext = p.extension().string();
	// Normalise to lower-case: extension() preserves case, so without this a file
	// named "toto.Stl" or "MODEL.STL" would match no branch and silently fail.
	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c) { return (char)std::tolower(c); });

	if (ext == ".asc")
		res = MeshIO::import_asc(mesh, filename);
	else if (ext == ".npts" || ext == ".pset")
		res = MeshIO::import_pset(mesh, filename);
	else if (ext == ".obj")
		res = MeshIO::import_obj(mesh, filename);
	else if (ext == ".stl")
		res = MeshIO::import_stl(mesh, filename);
	else if (ext == ".off")
		res = MeshIO::import_off(mesh, filename);
	else if (ext == ".pgm")
		res = MeshIO::import_pgm(mesh, filename);
	else if (ext == ".pts")
		res = MeshIO::import_pts(mesh, filename);
	else if (ext == ".ply")
		res = MeshIO::import_ply(mesh, filename);
	else if (ext == ".u3d")
		res = MeshIO::import_u3d(mesh, filename);

	// check coherency
	if (mesh.m_nTextureCoordinates == 0)
	{
		// desactivate texture coordinates if they are not provided
		for (unsigned int iFace = 0; iFace < mesh.m_nFaces; iFace++)
		{
			auto& face = mesh.m_pFaces[iFace];
			face->m_bUseTextureCoordinates = false;
		}
	}

	return res;
}

int MeshIO::save (Mesh& mesh, const char *filename)
{
	if (!filename)
		return -1;

	if (strcmp (filename+(strlen(filename)-4), ".asc") == 0)
	  return MeshIO::export_asc(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-5), ".npts") == 0 ||
		 strcmp (filename+(strlen(filename)-5), ".pset") == 0)
	  return MeshIO::export_pset(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-4), ".obj") == 0)
	  return MeshIO::export_obj(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-4), ".dae") == 0)
	  return MeshIO::export_dae(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-4), ".cpp") == 0)
	  return MeshIO::export_cpp(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-4), ".gts") == 0)
	  return MeshIO::export_gts(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-4), ".off") == 0)
	  return MeshIO::export_off(mesh, filename);
	else if (strcmp (filename+(strlen(filename)-4), ".pts") == 0)
	  return MeshIO::export_pts(mesh, filename);
	else if (strcmp(filename + (strlen(filename) - 4), ".ply") == 0)
		return MeshIO::export_ply(mesh, filename);
	else if (strcmp(filename + (strlen(filename) - 4), ".stl") == 0)
		return MeshIO::export_stl(mesh, filename);

	return -1;
}

// OBJ (+ MTL) : deplace dans mesh_io_obj.cpp, pour pouvoir etre compile sans
// mesh_io_3ds.h ni mesh_io_rply.h -- cf. la cible WebAssembly de maker.

#define BUFFER_SIZE 4096

//
// ASC
//

int MeshIO::import_asc (Mesh& mesh, const char *filename)
{
	if (filename == nullptr)
		return -1;

	FILE *file = fopen (filename, "r");
	if (file == nullptr)
	{
		printf ("[3DViewer] Unable to open %s", filename);
		return -1;
	}
	
	float vx, vy, vz, nx, ny, nz;
	int r, g, b;
	unsigned int nPoints=0;
	while (!feof (file))
	{
		fscanf (file, "%f %f %f %d %d %d %f %f %f\n", &vx, &vy, &vz, &r, &g, &b, &nx, &ny, &nz);
		nPoints++;
	}
	//nPoints--;
	rewind (file);
	mesh.Init (nPoints, 0);
	mesh.InitVertexColors ();
	unsigned int i=0;
	while (!feof (file))
	{
		fscanf (file, "%f %f %f %d %d %d %f %f %f\n", &vx, &vy, &vz, &r, &g, &b, &nx, &ny, &nz);
		mesh.m_pVertices[3*i+0] = vx;
		mesh.m_pVertices[3*i+1] = vy;
		mesh.m_pVertices[3*i+2] = vz;
		mesh.m_pVertexColors[3*i+0]	= r/255.;
		mesh.m_pVertexColors[3*i+1]	= g/255.;
		mesh.m_pVertexColors[3*i+2]	= b/255.;
		mesh.m_pVertexNormals[3*i+0] = nx;
		mesh.m_pVertexNormals[3*i+1] = ny;
		mesh.m_pVertexNormals[3*i+2] = nz;
		i++;
	}
	fclose (file);
//	triangulate_regular_heightfield (640, 480);
	return 0;
}

int MeshIO::export_asc (Mesh& mesh, const char *filename)
{
	FILE *ptr = fopen (filename, "w");
	printf ("ASC : %d\n", mesh.m_nVertices);
	unsigned char r=0, g=0, b=0;
	if (!mesh.m_pVertexColors.empty())
	{
		for (unsigned int i=0; i<mesh.m_nVertices; i++)
		{
			r = (unsigned char)(255.*mesh.m_pVertexColors[3*i+0]);
			g = (unsigned char)(255.*mesh.m_pVertexColors[3*i+1]);
			b = (unsigned char)(255.*mesh.m_pVertexColors[3*i+2]);
			fprintf (ptr, "%f %f %f %d %d %d %f %f %f\n",
				 mesh.m_pVertices[3*i+0], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2],
				 r, g, b,
				 mesh.m_pVertexNormals[3*i+0], mesh.m_pVertexNormals[3*i+1], mesh.m_pVertexNormals[3*i+2]
				);
		}
	}
	else
	{
		for (unsigned int i=0; i<mesh.m_nVertices; i++)
		{
			fprintf (ptr, "%f %f %f %d %d %d %f %f %f\n",
				 mesh.m_pVertices[3*i+0], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2],
				 r, g, b,
				 mesh.m_pVertexNormals[3*i+0], mesh.m_pVertexNormals[3*i+1], mesh.m_pVertexNormals[3*i+2]
				);
		}
	}

	fclose (ptr);

	return 0;
}

//
// PSET
//
int MeshIO::import_pset (Mesh& mesh, const char *filename)
{
	if (filename == nullptr)
		return -1;

	FILE *file = fopen (filename, "r");
	if (file == nullptr)
	{
		printf ("[3DViewer] Unable to open %s", filename);
		return -1;
	}
	
	float vx, vy, vz, nx, ny, nz;
	unsigned int nPoints=0;
	while (!feof (file))
	{
		fscanf (file, "%f %f %f %f %f %f\n", &vx, &vy, &vz, &nx, &ny, &nz);
		nPoints++;
	}
//	nPoints--;
	rewind (file);
	printf ("%d points\n", nPoints);
	mesh.Init (nPoints, 0);
	unsigned int i=0;
	while (!feof (file))
	{
		fscanf (file, "%f %f %f %f %f %f\n",
			&mesh.m_pVertices[i],
			&mesh.m_pVertices[i+1],
			&mesh.m_pVertices[i+2],
			&mesh.m_pVertexNormals[i],
			&mesh.m_pVertexNormals[i+1],
			&mesh.m_pVertexNormals[i+2]);
		i+=3;
	}
	fclose (file);

	return 0;
}

int MeshIO::export_pset (Mesh& mesh, const char *filename)
{
	FILE *ptr = fopen (filename, "w");
	
	for (unsigned int i=0; i<mesh.m_nVertices; i++)
	{
		fprintf (ptr, "%f %f %f %f %f %f\n",
			 mesh.m_pVertices[3*i+0], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2],
			 mesh.m_pVertexNormals[3*i+0], mesh.m_pVertexNormals[3*i+1], mesh.m_pVertexNormals[3*i+2]
			 );
	}

	fclose (ptr);

	return 0;
}



//
// DAE
//
int MeshIO::export_dae (Mesh& mesh, const char *filename)
{
	FILE *ptr = fopen (filename, "w");

	// header
	char *header = (char*)
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">\n"
		"  <asset>\n"
		"    <contributor>\n"
		"      <author>Author</author>\n"
		"      <authoring_tool>Authoring Tool</authoring_tool>\n"
		"      <comments>Comments</comments>\n"
		"    </contributor>\n"
		"    <created></created>\n"
		"    <modified></modified>\n"
		"    <unit meter=\"0.01\" name=\"centimeter\"/>\n"
		"    <up_axis>Z_UP</up_axis>\n"
		"  </asset>\n";
	fprintf (ptr, "%s", header);

	// effects
	fprintf (ptr, "   <library_effects>\n");
	fprintf (ptr, "      <effect id=\"mat0_fx\" name=\"mat0_fx\">\n");
	fprintf (ptr, "         <profile_COMMON>\n");
	fprintf (ptr, "           <newparam sid=\"test_jpg-surface\">\n");
	fprintf (ptr, "               <surface type=\"2D\">\n");
	fprintf (ptr, "                  <init_from>test_jpg-img</init_from>\n");
	fprintf (ptr, "               </surface>\n");
	fprintf (ptr, "            </newparam>\n");
	fprintf (ptr, "            <newparam sid=\"test_jpg-sampler\">\n");
	fprintf (ptr, "               <sampler2D>\n");
	fprintf (ptr, "                  <source>test_jpg-surface</source>\n");
	fprintf (ptr, "               </sampler2D>\n");
	fprintf (ptr, "            </newparam>\n");
	fprintf (ptr, "            <technique sid=\"COMMON\">\n");
	fprintf (ptr, "               <phong>\n");
	fprintf (ptr, "                  <emission><color>0.000000 0.000000 0.000000 1</color></emission>\n");
	fprintf (ptr, "                  <ambient><color>0.000000 0.000000 0.000000 1</color></ambient>\n");
	fprintf (ptr, "                  <diffuse><texture texture=\"test_jpg-sampler\" texcoord=\"CHANNEL1\"/></diffuse>\n");
	fprintf (ptr, "                  <specular><color>0.330000 0.330000 0.330000 1</color></specular>\n");
	fprintf (ptr, "                  <shininess><float>20.000000</float></shininess>\n");
	fprintf (ptr, "                  <reflectivity><float>0.100000</float></reflectivity>\n");
	fprintf (ptr, "                  <transparent><color>1 1 1 1</color></transparent>\n");
	fprintf (ptr, "                  <transparency><float>0.000000</float></transparency>\n");
	fprintf (ptr, "               </phong>\n");
	fprintf (ptr, "            </technique>\n");
	fprintf (ptr, "         </profile_COMMON>\n");
	fprintf (ptr, "      </effect>\n");
	fprintf (ptr, "   </library_effects>\n");

	// library images
	fprintf (ptr, "  <library_images>\n");
	fprintf (ptr, "   <image id=\"test_jpg-img\" name=\"test_jpg-img\">\n");
	fprintf (ptr, "      <init_from>./test.jpg</init_from>\n");
	fprintf (ptr, "    </image>\n");
	fprintf (ptr, "  </library_images>\n");

	// library materials
	fprintf (ptr, "   <library_materials>\n");
	fprintf (ptr, "      <material id=\"mat0\" name=\"mat0\">\n");
	fprintf (ptr, "         <instance_effect url=\"#mat0_fx\"/>\n");
	fprintf (ptr, "      </material>\n");
	fprintf (ptr, "   </library_materials>\n");

	// library geometries
	fprintf (ptr, "  <library_geometries>\n");
	fprintf (ptr, "    <geometry id=\"GEO01-mesh\" name=\"GEO01\">\n");
	fprintf (ptr, "      <mesh>\n");
	fprintf (ptr, "        <source id=\"GEO01-Position\">\n");
	fprintf (ptr, "          <float_array id=\"GEO01-Position-array\" count=\"%d\">", 3*mesh.m_nVertices);
	for (int i=0; i<3*mesh.m_nVertices; i++)
		fprintf (ptr, "%f ", mesh.m_pVertices[i]);
	fprintf (ptr, "</float_array>\n");
	fprintf (ptr, "          <technique_common>\n\n");
	fprintf (ptr, "            <accessor source=\"#GEO01-Position-array\" count=\"%d\" stride=\"3\">\n", mesh.m_nVertices);
	fprintf (ptr, "              <param name=\"X\" type=\"float\"/>\n");
	fprintf (ptr, "              <param name=\"Y\" type=\"float\"/>\n");
	fprintf (ptr, "              <param name=\"Z\" type=\"float\"/>\n");
	fprintf (ptr, "            </accessor>\n");
	fprintf (ptr, "          </technique_common>\n");
	fprintf (ptr, "        </source>\n");
	fprintf (ptr, "        <source id=\"GEO01-UV\">\n");
	fprintf (ptr, "          <float_array id=\"GEO01-UV-array\" count=\"%d\">", 2*mesh.m_nTextureCoordinates);
	for (int i=0; i<2*mesh.m_nTextureCoordinates; i++)
		fprintf (ptr, "%f ", mesh.m_pTextureCoordinates[i]);
	fprintf (ptr, "</float_array>\n");
	fprintf (ptr, "          <technique_common>\n");
	fprintf (ptr, "            <accessor source=\"#GEO01-UV-array\" count=\"%d\" stride=\"2\">\n", mesh.m_nTextureCoordinates);
	fprintf (ptr, "              <param name=\"S\" type=\"float\"/>\n");
	fprintf (ptr, "              <param name=\"T\" type=\"float\"/>\n");
	fprintf (ptr, "            </accessor>\n");
	fprintf (ptr, "          </technique_common>\n");
	fprintf (ptr, "        </source>\n");
	fprintf (ptr, "        <vertices id=\"GEO01-Vertex\">\n");
	fprintf (ptr, "          <input semantic=\"POSITION\" source=\"#GEO01-Position\"/>\n");
	fprintf (ptr, "        </vertices>\n");
	fprintf (ptr, "        <triangles material=\"mat0\" count=\"%d\">\n", mesh.m_nFaces);
	fprintf (ptr, "          <input offset=\"0\" semantic=\"VERTEX\" source=\"#GEO01-Vertex\"/>\n");
	fprintf (ptr, "          <input offset=\"1\" semantic=\"TEXCOORD\" source=\"#GEO01-UV\"/>\n");
	fprintf (ptr, "          <p>");
	for (int i=0; i<mesh.m_nFaces; i++)
		for (int j=0; j<mesh.m_pFaces[i]->m_nVertices; j++)
			fprintf (ptr, "%d %d ", mesh.m_pFaces[i]->m_pVertices[j], mesh.m_pFaces[i]->m_pTextureCoordinatesIndices[j]);
	fprintf (ptr, "</p>\n");
	fprintf (ptr, "        </triangles>\n");
	fprintf (ptr, "      </mesh>\n");
	fprintf (ptr, "    </geometry>\n");
	fprintf (ptr, "  </library_geometries>\n");

	// library visaul scenes
	fprintf (ptr, "  <library_visual_scenes>\n");
	fprintf (ptr, "    <visual_scene id=\"Scene\" name=\"Scene\">\n");
	fprintf (ptr, "     <node id=\"GEO01-node\" name=\"GEO01\" type=\"NODE\">\n");
	fprintf (ptr, "        <instance_geometry url=\"#GEO01-mesh\">\n");
	fprintf (ptr, "			<bind_material>\n");
	fprintf (ptr, "		                <technique_common>\n");
	fprintf (ptr, "                                 <instance_material symbol=\"mat0\" target=\"#mat0\">\n");
	fprintf (ptr, "                                          <bind_vertex_input input_semantic=\"TEXCOORD\" input_set=\"1\" semantic=\"CHANNEL1\"/>\n");
	fprintf (ptr, "                                 </instance_material>\n");
	fprintf (ptr, "                          </technique_common>\n");
	fprintf (ptr, "                 </bind_material>\n");
	fprintf (ptr, "        </instance_geometry>\n");
	fprintf (ptr, "      </node>\n");
	fprintf (ptr, "    </visual_scene>\n");
	fprintf (ptr, "  </library_visual_scenes>\n");
	fprintf (ptr, "  <scene>\n");
	fprintf (ptr, "    <instance_visual_scene url=\"#Scene\"/>\n");
	fprintf (ptr, "  </scene>\n");
	fprintf (ptr, "</COLLADA>\n");
	
	fclose (ptr);

	return 0;
}

//
// cpp
//
int MeshIO::export_cpp (Mesh& mesh, const char *filename)
{
	int i;
  // Cette fonction cumulait quatre defauts, corriges ensemble parce qu'ils
  // s'enchainent :
  //
  //   1. `return false` sur les chemins d'ERREUR. La convention du module est
  //      0 = succes / -1 = echec (cf. export_obj), et false vaut 0 : l'appelant
  //      qui testait le code de retour croyait l'export reussi alors que le
  //      fichier n'avait meme pas pu etre ouvert. C'est le plus grave des quatre,
  //      et Sonar ne le signalait pas.
  //   2. La seconde boucle de recherche evaluait `modelname[i]` AVANT de borner
  //      `i>=0` : a i==0 sans '/', elle decrementait puis lisait modelname[-1]
  //      (cpp:S3519, mesh_io.cpp:400).
  //   3. `modelname = &modelname[i+1]` perdait le pointeur de base du strdup,
  //      rendant tout free() impossible (cpp:S3584).
  //   4. Le retour ligne 405 sautait le fclose (cpp:S2095, mesh_io.cpp:404).
  //
  // std::filesystem::path fait le travail des deux boucles sans arithmetique de
  // pointeur, donc sans aucun de ces quatre pieges.
  FILE *ptr = fopen (filename, "w");
  if (!ptr)
    {
      fprintf (stderr, "export_cpp: unable to open %s\n", filename);
      return -1;
    }

  const std::string stem = std::filesystem::path(filename).stem().string();
  if (stem.empty())
    {
      fprintf (stderr, "export_cpp: unable to extract name of the file\n");
      fclose (ptr);
      return -1;
    }
  const char *modelname = stem.c_str();

  fprintf (ptr, "/* model coming from %s */\n\n", filename);

  /* n_vertices & n_faces */
  int n_vertices = mesh.m_nVertices;
  int n_faces    = mesh.m_nFaces;
  fprintf (ptr, "static int %s_n_vertices = %d;\n", modelname, n_vertices);
  fprintf (ptr, "static int %s_n_faces = %d;\n\n", modelname, n_faces);
  
  /* vertices */
  float x, y, z;
  fprintf (ptr, "static float %s_vertices[] = {", modelname);
  x = mesh.m_pVertices[0];
  y = mesh.m_pVertices[1];
  z = mesh.m_pVertices[2];
  fprintf (ptr, "%f, %f, %f,\n", x, y, z);
  for (i=1; i<n_vertices-1; i++)
    {
  x = mesh.m_pVertices[3*i];
  y = mesh.m_pVertices[3*i+1];
  z = mesh.m_pVertices[3*i+2];
      fprintf (ptr, "\t\t%f, %f, %f,\n", x, y, z);
    }
  x = mesh.m_pVertices[3*i];
  y = mesh.m_pVertices[3*i+1];
  z = mesh.m_pVertices[3*i+2];
  fprintf (ptr, "\t\t%f, %f, %f};\n\n", x, y, z);

  /* faces */
  int a, b, c;
  fprintf (ptr, "static int %s_faces[] = {", modelname);
  a = mesh.m_pFaces[0]->GetVertex(0);
  b = mesh.m_pFaces[0]->GetVertex(1);
  c = mesh.m_pFaces[0]->GetVertex(2);
  fprintf (ptr, "%d, %d, %d,\n", a, b, c);
  for (i=1; i<n_faces-1; i++)
    {
  a = mesh.m_pFaces[i]->GetVertex(0);
  b = mesh.m_pFaces[i]->GetVertex(1);
  c = mesh.m_pFaces[i]->GetVertex(2);
      fprintf (ptr, "\t\t%d, %d, %d,\n", a, b , c);
    }
  a = mesh.m_pFaces[i]->GetVertex(0);
  b = mesh.m_pFaces[i]->GetVertex(1);
  c = mesh.m_pFaces[i]->GetVertex(2);
  fprintf (ptr, "\t\t%d, %d, %d};\n\n", a, b, c);

  /* vertices normales */
  fprintf (ptr, "static float %s_vertices_normales[] = {", modelname);
  float *vertices_normales = mesh.m_pVertexNormals.data();
  fprintf (ptr, "%f, %f, %f,\n", vertices_normales[0], vertices_normales[1], vertices_normales[2]);
  for (i=1; i<n_vertices-1; i++)
    fprintf (ptr, "\t\t%f, %f, %f,\n",
	     vertices_normales[3*i], vertices_normales[3*i+1], vertices_normales[3*i+2]);
  fprintf (ptr, "\t\t%f, %f, %f};\n\n",
	   vertices_normales[3*i], vertices_normales[3*i+1], vertices_normales[3*i+2]);

  fclose (ptr);

  return 0;
}

//
// GTS
//
int MeshIO::export_gts (Mesh& mesh, const char *filename)
{
  int i;
  FILE *ptr = fopen (filename, "w");
  
  fprintf (ptr, "%d %d %d\n", mesh.m_nVertices, 3*mesh.m_nFaces, mesh.m_nFaces);
  
  // vertices
  for (i=0; i<mesh.m_nVertices; i++)
    fprintf (ptr, "%f %f %f\n", mesh.m_pVertices[3*i], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2]);

  // edges
  for (i=0; i<mesh.m_nFaces; i++)
    {
	    int a = mesh.m_pFaces[i]->GetVertex (0);
	    int b = mesh.m_pFaces[i]->GetVertex (1);
	    int c = mesh.m_pFaces[i]->GetVertex (2);
      fprintf (ptr, "%d %d\n", 1+a, 1+b);
      fprintf (ptr, "%d %d\n", 1+b, 1+c);
      fprintf (ptr, "%d %d\n", 1+c, 1+a);
    }

  // faces
  for (i=0; i<mesh.m_nFaces; i++)
    fprintf (ptr, "%d %d %d\n", 3*i+1, 3*i+2, 3*i+3);

  fclose (ptr);

  return 0;
}


//
// OFF
//
/**
*
* Read a mesh from a 'off' file.
* The format is as follows :
* header :
* OFF
* nv nf ne

* body :
* x1 y1 z1
* ...
* xm ym zm
* f1 f2 f3
* ...
* f3n f3n+1 f3n+2
*/
int MeshIO::import_off (Mesh& mesh, const char *filename)
{
  char id[4];
  int i, nSegments;

  FILE* ptr = fopen (filename,"r");
  if (!ptr)
	  return -1;
  
  // Get header information
  fgets (id, 4, ptr);
  DASSERT (id[0]=='O' && id[1]=='F' && id[2]=='F');
  fscanf (ptr, "%d %d %d", &mesh.m_nVertices, &mesh.m_nFaces, &nSegments);
  
  // memory allocation
  mesh.Init (mesh.m_nVertices, mesh.m_nFaces);
 
  // Get the vertices
  for (i=0; i<mesh.m_nVertices; i++)
    fscanf (ptr, "%f %f %f", &mesh.m_pVertices[3*i], &mesh.m_pVertices[3*i+1], &mesh.m_pVertices[3*i+2]);

  // Get the faces
  for (i=0; i<mesh.m_nFaces; i++)
    {
      //float r_tmp, g_tmp, b_tmp;
      int n_vertices_in_face;

      fscanf (ptr, "%d", &n_vertices_in_face);
      if (n_vertices_in_face != 3)
	{
	  printf ("illegal file\n");
	  fclose (ptr);
	  return -1;
	}

      // write into the face already allocated by mesh.Init() (do NOT leak a new one)
      Face *pFace = mesh.m_pFaces[i];
      fscanf (ptr, "%d %d %d",
	      &pFace->m_pVertices[0], &pFace->m_pVertices[1], &pFace->m_pVertices[2]);

    }
  fclose (ptr);

  return 0;
}

/**
* export into off file
*/
int MeshIO::export_off (Mesh& mesh, const char *filename)
{
  FILE *ptr;
  int i;

  ptr = fopen (filename,"w");
  if (!ptr) return false;

  // header
  fprintf (ptr, "OFF\n");
  fprintf (ptr, "%ud %ud %d\n", mesh.m_nVertices, mesh.m_nFaces, 0);

  // vertices
  for (i=0; i<mesh.m_nVertices; i++)
    fprintf (ptr, "%f %f %f\n", mesh.m_pVertices[3*i], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2]);

  // faces
  //for (i=0; i<n_faces; i++)
  //  fprintf (ptr, "3 %d %d %d 0.5 0.5 0.5\n",
  //	     f[3*i], f[3*i+1], f[3*i+2]);
  for (i=0; i<mesh.m_nFaces; i++)
  {
	  fprintf (ptr, "%d %d %d\n",
		   mesh.m_pFaces[i]->GetVertex (0), mesh.m_pFaces[i]->GetVertex (1), mesh.m_pFaces[i]->GetVertex (2));
  }

  fclose (ptr);

  return 0;
}

//
// PGM
//
// static function used by void MeshIO::import_pgm(mesh, char *filename)
static void
_pgm_skip_spaces(FILE *file)
{
  char c;
  
  while(1)
  {
    c = getc(file);
  
    if (isspace(c))
      continue;

    if (c == '#')
    {
      char buf[256];

      fgets(buf, 256, file);
      continue;
    }

    ungetc(c, file);

    break;
  }
}

/**
* Reads a mesh from a 'pgm' file. It creates a 3D surface as a height field.
*/
int MeshIO::import_pgm (Mesh& mesh, const char *filename)
{
  int width, height, levels;
  //int level_walk;
  unsigned char *data = nullptr;
  char id[2];
  FILE *ptr;
  int i,j;
  
  ptr = fopen (filename,"rb");
  if (!ptr)
    {
      printf ("couldn't open \"%s\"\n", filename);
      return -1;
    }

  /* Get header information */
  if (fscanf (ptr, "%c %c", &id[0], &id[1]) != 2)
    {
      printf ("Failed to read header from \"%s\"\n", filename);
      fclose (ptr);
      return -1;
    }
  if (id[0] != 'P' || (id[1] != '2' && id[1] != '5'))
    {
      printf ("\"%s\" is not a valid PGM file\n", filename);
      fclose (ptr);
      return -1;
    }

  _pgm_skip_spaces (ptr);
  if (fscanf (ptr, "%d %d", &width, &height) != 2 || width <= 0 || height <= 0)
    {
      printf ("invalid PGM dimensions in \"%s\"\n", filename);
      fclose (ptr);
      return -1;
    }
  _pgm_skip_spaces (ptr);
  fscanf (ptr, "%d", &levels);
  _pgm_skip_spaces (ptr);

  /* Create the data */
  data = (unsigned char*)malloc((size_t)width*height*sizeof(unsigned char));
  if (data == nullptr)
    {
      fclose (ptr);
      return -1;
    }

  /* Get the data */
  if (id[1]=='2') /* ascii mode */
  {
    for (j=0; j<height; j++)
      for (i=0; i<width; i++)
	  {
		fscanf (ptr, "%c", &data[j*width+i]);
	  }
  }
  if (id[1]=='5') /* raw mode */
  {
	fread (data, sizeof(unsigned char), height*width, ptr);
  }

  fclose (ptr);

  // equalization
  int min = 255;
  int max = 0;
  for (i=0; i<width*height; i++)
    {
      if (data[i] > max) max = data[i];
      if (data[i] < min) min = data[i];
    }    
  if (max > min)
    for (i=0; i<width*height; i++)
      data[i] = 255*(data[i]-min)/(max-min);

  // translation to center
  float x_trans = (1)? -width/2.0 : 0;
  float y_trans = (1)? -height/2.0 : 0;

  // alloc memory
  mesh.m_nVertices = width*height;
  mesh.m_nFaces = 2*(width-1)*(height-1);
  mesh.Init (mesh.m_nVertices, mesh.m_nFaces);

  // fill the structure
  for (j=0; j<height; j++)
    for (i=0; i<width; i++)
	{
	  mesh.m_pVertices[3*(j*width+i)]   = (float)i+x_trans;
	  mesh.m_pVertices[3*(j*width+i)+1] = (float)(height-1-j)+y_trans;
	  mesh.m_pVertices[3*(j*width+i)+2] = (float)(255-data[j*width+i]/3.0)-150.0;
	}

  for (j=0; j<height-1; j++)
    for (i=0; i<width-1; i++)
	{
		Face *f1 = mesh.m_pFaces[2*(j*(width-1)+i)];     // reuse faces allocated by mesh.Init (no leak)
		f1->SetVertex (0, j*width+i);
		f1->SetVertex (1, (j+1)*width+i);
		f1->SetVertex (2, j*width+i+1);
		Face *f2 = mesh.m_pFaces[2*(j*(width-1)+i)+1];
		f2->SetVertex (0, (j+1)*width+i+1);
		f2->SetVertex (1, j*width+i+1);
		f2->SetVertex (2, (j+1)*width+i);
	}

	free (data);
	return 0;
}

//
// PTS
//
int MeshIO::import_pts (Mesh& mesh, const char *filename)
{
	int i;
  FILE *ptr = fopen (filename, "r");
  if (!ptr)
    {
      printf ("unable to open %s\n", filename);
      return -1;
    }

  char *buffer = (char*)malloc(512*sizeof(char));
  if (buffer == nullptr)
  {
	  fclose(ptr);
	  return -1;
  }
  //
  mesh.m_nVertices = 0;
  while (1)
  {
    fgets (buffer, 512, ptr);
    if (feof(ptr)) break;
    mesh.m_nVertices++;
  }
  mesh.Init (mesh.m_nVertices, 0);
  mesh.m_pVertexColors.assign(3*mesh.m_nVertices, 0.0f);

  rewind (ptr);
  unsigned int vertex_walk=0;
  unsigned int r=0, g=0, b=0;
  while (1)
  {
    fgets (buffer, 512, ptr);
    if (feof(ptr)) break;

	sscanf (buffer, "%f %f %f %d %d %d", 
		&mesh.m_pVertices[3*vertex_walk],
		&mesh.m_pVertices[3*vertex_walk+1],
		&mesh.m_pVertices[3*vertex_walk+2],
		&r, &g, &b);
	mesh.m_pVertexColors[3*vertex_walk]   = (float)r/255.0;
	mesh.m_pVertexColors[3*vertex_walk+1] = (float)g/255.0;
	mesh.m_pVertexColors[3*vertex_walk+2] = (float)b/255.0;

	vertex_walk++;
  }
  fclose (ptr);

  free (buffer);

  return 0;
}

int MeshIO::export_pts (Mesh& mesh, const char *filename)
{
  FILE *ptr = fopen (filename, "w");
  if (!ptr)
	{
	  printf ("unable to open %s\n", filename);
	  return false;
	}

  if (!mesh.m_pVertices.empty() && !mesh.m_pVertexColors.empty())
  {
	  for (int i=0; i<mesh.m_nVertices; i++)
	  {
		  fprintf (ptr, "%f %f %f %d %d %d\n",
			  mesh.m_pVertices[3*i], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2],
			  (int)(mesh.m_pVertexColors[3*i]*255.0), (int)(mesh.m_pVertexColors[3*i+1]*255.0), (int)(mesh.m_pVertexColors[3*i+2]*255.0));
	  }
  }
  else
  {
	  fclose(ptr);
	  return false;
  }

  fclose (ptr);

  return true;
}

//
// PLY
//
#include "mesh_io_rply.h"
static int vertex_cb_coords(p_ply_argument argument) {
    long dim;
    void *pMesh;
    ply_get_argument_user_data(argument, &pMesh, &dim);
    Mesh *mesh = (Mesh*)pMesh;

    if (dim == 0) // new vertex
	    mesh->m_nVertices++;

    float coord = ply_get_argument_value(argument);
    mesh->m_pVertices[3*mesh->m_nVertices+dim] = coord;

    return 1;
}

static int vertex_cb_normals(p_ply_argument argument) {
    long dim;
    void *pMesh;
    ply_get_argument_user_data(argument, &pMesh, &dim);
    Mesh *mesh = (Mesh*)pMesh;
    float coord = ply_get_argument_value(argument);
    mesh->m_pVertexNormals[3*mesh->m_nVertices+dim] = coord;

    return 1;
}

static int vertex_cb_colors(p_ply_argument argument) {
    long dim;
    void *pMesh;
    ply_get_argument_user_data(argument, &pMesh, &dim);
    Mesh *mesh = (Mesh*)pMesh;
    float coord = ply_get_argument_value(argument)/255.;
    mesh->m_pVertexColors[3*mesh->m_nVertices+dim] = coord;

    return 1;
}

static int face_cb(p_ply_argument argument) {
    void *pMesh;
    ply_get_argument_user_data(argument, &pMesh, nullptr);
    Mesh *mesh = (Mesh*)pMesh;


   long length, value_index;
    ply_get_argument_property(argument, nullptr, &length, &value_index);

    if (value_index == -1) // new face
    {
	    mesh->m_nFaces++;
	    mesh->m_pFaces[mesh->m_nFaces] = new Face ();
	    mesh->m_pFaces[mesh->m_nFaces]->SetNVertices (ply_get_argument_value(argument));
    }
    else
    {
	    mesh->m_pFaces[mesh->m_nFaces]->m_pVertices[value_index] = ply_get_argument_value(argument);
    }
    return 1;
}

int MeshIO::import_ply (Mesh& mesh, const char *filename)
{
	int i;
	long nvertices, ntriangles;
	p_ply ply = ply_open(filename, nullptr, 0, nullptr);
	if (!ply)
	{
		printf ("unable to open %s\n", filename);
		return -1;
	}
	if (!ply_read_header(ply))
		return -1;
	
	nvertices = ply_set_read_cb(ply, "vertex", "x", vertex_cb_coords, &mesh, 0);
	ply_set_read_cb(ply, "vertex", "y", vertex_cb_coords, &mesh, 1);
	ply_set_read_cb(ply, "vertex", "z", vertex_cb_coords, &mesh, 2);
	
	int bNormals = ply_set_read_cb(ply, "vertex", "nx", vertex_cb_normals, &mesh, 0);
	ply_set_read_cb(ply, "vertex", "ny", vertex_cb_normals, &mesh, 1);
	ply_set_read_cb(ply, "vertex", "nz", vertex_cb_normals, &mesh, 2);
	
	int bColors = ply_set_read_cb(ply, "vertex", "red", vertex_cb_colors, &mesh, 0);
	ply_set_read_cb(ply, "vertex", "green", vertex_cb_colors, &mesh, 1);
	ply_set_read_cb(ply, "vertex", "blue", vertex_cb_colors, &mesh, 2);
	
	ntriangles = ply_set_read_cb(ply, "face", "vertex_indices", face_cb, &mesh, 0);
	
	mesh.Init (nvertices, ntriangles);
	if (bColors)
		mesh.InitVertexColors ();
	
	mesh.m_nVertices = (unsigned int)(-1);
	mesh.m_nFaces = (unsigned int)(-1);
	
	if (!ply_read(ply))
		return -1;
	
	mesh.m_nVertices = nvertices;
	mesh.m_nFaces = ntriangles;
	
	ply_close(ply);
	
	return 0;
}

int MeshIO::export_ply (Mesh& mesh, const char *filename)
{
    const char *value;
    p_ply oply = ply_create(filename, PLY_LITTLE_ENDIAN, nullptr, 0, nullptr);
    if (!oply)
	    return -1;

    ply_add_element(oply, "vertex", mesh.m_nVertices);
    ply_add_property(oply, "x", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    ply_add_property(oply, "y", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    ply_add_property(oply, "z", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    if (!mesh.m_pVertexNormals.empty())
    {
	    ply_add_property(oply, "nx", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
	    ply_add_property(oply, "ny", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
	    ply_add_property(oply, "nz", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    }

    // write output header
    if (!ply_write_header(oply))
	    return -1;

    for (unsigned int i=0; i<mesh.m_nVertices; i++)
    {
	    ply_write (oply, mesh.m_pVertices[3*i]);
	    ply_write (oply, mesh.m_pVertices[3*i+1]);
	    ply_write (oply, mesh.m_pVertices[3*i+2]);
	    if (!mesh.m_pVertexNormals.empty())
	    {
		    ply_write (oply, mesh.m_pVertexNormals[3*i]);
		    ply_write (oply, mesh.m_pVertexNormals[3*i+1]);
		    ply_write (oply, mesh.m_pVertexNormals[3*i+2]);
	    }
    }

    // clean
    if (!ply_close(oply))
	    return -1;
    return 0;
}


// ASCII STL import. Scans for "vertex x y z" records (3 per facet) and builds
// one independent triangle per facet (no vertex sharing, like binary STL).
int MeshIO::import_stl_ascii (Mesh& mesh, const char *filename)
{
	FILE* ptr = fopen(filename, "r");
	if (ptr == nullptr)
		return 1;

	std::vector<float> verts;   // xyz per vertex, 3 vertices per facet
	char token[256];
	while (fscanf(ptr, "%255s", token) == 1)
	{
		for (char* c = token; *c; ++c)
			*c = (char)tolower((unsigned char)*c);

		if (strcmp(token, "vertex") == 0)
		{
			float x, y, z;
			if (fscanf(ptr, "%f %f %f", &x, &y, &z) != 3)
				break;   // malformed record — keep the whole facets read so far
			verts.push_back(x);
			verts.push_back(y);
			verts.push_back(z);
		}
	}
	fclose(ptr);

	// Only complete triangles (9 floats) are usable.
	unsigned int nTriangles = (unsigned int)(verts.size() / 9);
	mesh.Init(3 * nTriangles, nTriangles);
	if (nTriangles == 0)
		return 0;

	memcpy(mesh.m_pVertices.data(), verts.data(),
	       (size_t)9 * nTriangles * sizeof(float));

	for (unsigned int iface = 0; iface < nTriangles; iface++)
	{
		Face* pFace = mesh.m_pFaces[iface];   // already allocated by Init()
		pFace->SetNVertices(3);
		for (int j = 0; j < 3; j++)
			pFace->SetVertex(j, 3*iface+j);
	}

	mesh.ComputeNormals();
	return 0;
}

int MeshIO::import_stl (Mesh& mesh, const char *filename)
{
	if (!filename)
		return 1;

	FILE* ptr = fopen(filename, "rb");
	if (ptr == nullptr)
		return 1;

	// A binary STL is exactly 84 + 50*nTriangles bytes (80-byte header,
	// 4-byte count, then 50 bytes per facet). Read the header/count, then
	// validate against the real file size: if they disagree the file is ASCII
	// (or corrupt). Trusting the count blindly on an ASCII file would allocate a
	// garbage, often enormous, triangle array and crash.
	fseek(ptr, 0, SEEK_END);
	long fileSize = ftell(ptr);
	fseek(ptr, 0, SEEK_SET);

	char header[80];
	unsigned int nTriangles = 0;
	bool binaryHeaderOk =
		fileSize >= 84 &&
		fread(header, sizeof(char), 80, ptr) == 80 &&
		fread(&nTriangles, 4, 1, ptr) == 1;

	long long expectedBinarySize = 84LL + 50LL * (unsigned long long)nTriangles;
	bool isBinary = binaryHeaderOk && expectedBinarySize == (long long)fileSize;

	if (!isBinary)
	{
		// Fall back to the ASCII parser (which re-opens the file).
		fclose(ptr);
		return import_stl_ascii(mesh, filename);
	}

	mesh.Init(3 * nTriangles, nTriangles);

	float coords[12];
	char attributes[2];
	for (unsigned int iface = 0; iface < nTriangles; iface++)
	{
		if (fread((char*)coords, sizeof(float), 12, ptr) != 12)
		{
			// Truncated file: bail out without computing normals (the faces past
			// this point are still default-constructed).
			fclose(ptr);
			return 1;
		}
		memcpy(mesh.m_pVertices.data() + 9*iface, coords + 3, 9 * sizeof(float));

		Face* pFace = mesh.m_pFaces[iface];   // already allocated by Init()
		pFace->SetNVertices(3);
		for (int j = 0; j < 3; j++)
			pFace->SetVertex(j, 3*iface+j);

		fread(attributes, sizeof(unsigned char), 2, ptr);   // 2-byte attribute count
	}

	fclose(ptr);
	mesh.ComputeNormals();
	return 0;
}

//
// STL ASCII export. Format :
//   solid <name>
//     facet normal nx ny nz
//       outer loop
//         vertex x y z
//         vertex x y z
//         vertex x y z
//       endloop
//     endfacet
//     ...
//   endsolid <name>
//
// The solid name is derived from the filename stem.
//
int MeshIO::export_stl (Mesh& mesh, const char *filename)
{
	if (!filename) return -1;

	// Derive a solid name from the filename stem (strip directories and extension).
	std::string solidName(filename);
	size_t slash = solidName.find_last_of("/\\");
	if (slash != std::string::npos) solidName = solidName.substr(slash + 1);
	size_t dot = solidName.find_last_of('.');
	if (dot != std::string::npos) solidName = solidName.substr(0, dot);
	if (solidName.empty()) solidName = "mesh";

	FILE *fp = fopen(filename, "w");
	if (!fp) return -1;

	fprintf(fp, "solid %s\n", solidName.c_str());

	for (unsigned int i = 0; i < mesh.m_nFaces; ++i)
	{
		Face *pFace = mesh.m_pFaces[i];
		if (!pFace || pFace->GetNVertices() != 3) continue;

		int a = pFace->GetVertex(0);
		int b = pFace->GetVertex(1);
		int c = pFace->GetVertex(2);
		if (a < 0 || b < 0 || c < 0) continue;
		if ((unsigned)a >= mesh.m_nVertices || (unsigned)b >= mesh.m_nVertices || (unsigned)c >= mesh.m_nVertices) continue;

		float ax = mesh.m_pVertices[3*a],   ay = mesh.m_pVertices[3*a+1], az = mesh.m_pVertices[3*a+2];
		float bx = mesh.m_pVertices[3*b],   by = mesh.m_pVertices[3*b+1], bz = mesh.m_pVertices[3*b+2];
		float cx = mesh.m_pVertices[3*c],   cy = mesh.m_pVertices[3*c+1], cz = mesh.m_pVertices[3*c+2];

		// Triangle normal = (b - a) x (c - a), normalized.
		float ux = bx - ax, uy = by - ay, uz = bz - az;
		float vx = cx - ax, vy = cy - ay, vz = cz - az;
		float nx = uy*vz - uz*vy;
		float ny = uz*vx - ux*vz;
		float nz = ux*vy - uy*vx;
		float len = sqrtf(nx*nx + ny*ny + nz*nz);
		if (len > 1e-12f) { nx /= len; ny /= len; nz /= len; }
		else { nx = 0.0f; ny = 0.0f; nz = 1.0f; }

		fprintf(fp, "facet normal %g %g %g\n", nx, ny, nz);
		fprintf(fp, "  outer loop\n");
		fprintf(fp, "    vertex %g %g %g\n", ax, ay, az);
		fprintf(fp, "    vertex %g %g %g\n", bx, by, bz);
		fprintf(fp, "    vertex %g %g %g\n", cx, cy, cz);
		fprintf(fp, "  endloop\n");
		fprintf(fp, "endfacet\n");
	}

	fprintf(fp, "endsolid %s\n", solidName.c_str());
	fclose(fp);
	return 0;
}

//
// STL binary export. Format :
//   80-byte header (text padded with NULs)
//   uint32_t  triangle_count       (little-endian)
//   For each triangle (50 bytes) :
//     float    normal[3]
//     float    vertex[3][3]
//     uint16_t attributes           (set to 0)
//
// Symmetric with Mesh::import_stl which reads binary STL.
//
int MeshIO::export_stl_binary (Mesh& mesh, const char *filename)
{
	if (!filename) return -1;

	FILE *fp = fopen(filename, "wb");
	if (!fp) return -1;

	// 80-byte header. Use the filename stem, NUL-padded.
	char header[80] = {0};
	{
		std::string stem(filename);
		size_t slash = stem.find_last_of("/\\");
		if (slash != std::string::npos) stem = stem.substr(slash + 1);
		size_t dot = stem.find_last_of('.');
		if (dot != std::string::npos) stem = stem.substr(0, dot);
		if (stem.empty()) stem = "mesh";
		// Binary fixed-width copy: clamp and memcpy. Remaining bytes stay
		// NUL from the zero-init above. The STL header is binary, so the
		// missing trailing NUL that static analyzers flag on strncpy is a
		// non-issue here — but memcpy makes the intent explicit.
		const size_t n = stem.size() < sizeof(header) ? stem.size() : sizeof(header);
		std::memcpy(header, stem.data(), n);
	}
	fwrite(header, 1, 80, fp);

	// Count valid triangles first (skip non-triangle faces / out-of-range indices).
	uint32_t nTri = 0;
	for (unsigned int i = 0; i < mesh.m_nFaces; ++i)
	{
		Face *pFace = mesh.m_pFaces[i];
		if (!pFace || pFace->GetNVertices() != 3) continue;
		int a = pFace->GetVertex(0), b = pFace->GetVertex(1), c = pFace->GetVertex(2);
		if (a < 0 || b < 0 || c < 0) continue;
		if ((unsigned)a >= mesh.m_nVertices || (unsigned)b >= mesh.m_nVertices || (unsigned)c >= mesh.m_nVertices) continue;
		++nTri;
	}
	fwrite(&nTri, sizeof(uint32_t), 1, fp);

	// Write each triangle.
	for (unsigned int i = 0; i < mesh.m_nFaces; ++i)
	{
		Face *pFace = mesh.m_pFaces[i];
		if (!pFace || pFace->GetNVertices() != 3) continue;
		int a = pFace->GetVertex(0), b = pFace->GetVertex(1), c = pFace->GetVertex(2);
		if (a < 0 || b < 0 || c < 0) continue;
		if ((unsigned)a >= mesh.m_nVertices || (unsigned)b >= mesh.m_nVertices || (unsigned)c >= mesh.m_nVertices) continue;

		float ax = mesh.m_pVertices[3*a],   ay = mesh.m_pVertices[3*a+1], az = mesh.m_pVertices[3*a+2];
		float bx = mesh.m_pVertices[3*b],   by = mesh.m_pVertices[3*b+1], bz = mesh.m_pVertices[3*b+2];
		float cx = mesh.m_pVertices[3*c],   cy = mesh.m_pVertices[3*c+1], cz = mesh.m_pVertices[3*c+2];

		float ux = bx - ax, uy = by - ay, uz = bz - az;
		float vx = cx - ax, vy = cy - ay, vz = cz - az;
		float nx = uy*vz - uz*vy;
		float ny = uz*vx - ux*vz;
		float nz = ux*vy - uy*vx;
		float len = sqrtf(nx*nx + ny*ny + nz*nz);
		if (len > 1e-12f) { nx /= len; ny /= len; nz /= len; }
		else { nx = 0.0f; ny = 0.0f; nz = 1.0f; }

		float n[3] = { nx, ny, nz };
		float v[9] = { ax, ay, az, bx, by, bz, cx, cy, cz };
		uint16_t attr = 0;
		fwrite(n,    sizeof(float), 3, fp);
		fwrite(v,    sizeof(float), 9, fp);
		fwrite(&attr, sizeof(uint16_t), 1, fp);
	}

	fclose(fp);
	return 0;
}


int MeshIO::export_3ds (Mesh& mesh, const char *filename)
{
	t3DSModel *p = Allocate3DSModel();
	Write3DSFile(p, filename, nullptr);
	Free3DSModel(p);
	return 0;
}

// ---------------------------------------------------------------------------
// Thin public delegators kept on Mesh: forward to the MeshIO statics above so
// the ~30+ existing callers of mesh.load()/save()/export_stl_binary() keep
// working unchanged.
// ---------------------------------------------------------------------------
int Mesh::load (const char *filename)              { return MeshIO::load (*this, filename); }
int Mesh::save (const char *filename)              { return MeshIO::save (*this, filename); }
int Mesh::export_stl_binary (const char *filename) { return MeshIO::export_stl_binary (*this, filename); }
