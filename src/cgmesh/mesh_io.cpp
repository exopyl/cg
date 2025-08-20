#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <filesystem>

#include "mesh.h"
#include "mesh_io_3ds.h"
#include "../cgmath/cgmath.h"
#include "endianness.h"

int Mesh::load (const char *filename)
{
	int res = -1;
	if (!filename)
		return res;

	/*if (strcmp (filename+(strlen(filename)-4), ".out") == 0)
		LoadBundleOut (0);
		else*/
	if (strcmp (filename+(strlen(filename)-4), ".asc") == 0)
	  res = import_asc (filename);
	else if (strcmp (filename+(strlen(filename)-5), ".npts") == 0 ||
		 strcmp (filename+(strlen(filename)-5), ".pset") == 0)
		res = import_pset (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".obj") == 0)
		res = import_obj (filename);
	else if (strcmp(filename + (strlen(filename) - 4), ".stl") == 0)
		res = import_stl(filename);
	else if (strcmp (filename+(strlen(filename)-4), ".3ds") == 0)
		res = import_3ds (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".ifs") == 0)
		res = import_ifs (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".lwo") == 0)
		res = import_lwo (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".off") == 0)
		res = import_off (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".pgm") == 0)
		res = import_pgm (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".pts") == 0)
		res = import_pts (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".ply") == 0)
		res = import_ply (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".u3d") == 0)
		res = import_u3d (filename);

	// check coherency
	if (m_nTextureCoordinates == 0)
	{
		// desactivate texture coordinates if they are not provided
		for (unsigned int iFace = 0; iFace < m_nFaces; iFace++)
		{
			auto& face = m_pFaces[iFace];
			face->m_bUseTextureCoordinates = false;
		}
	}

	return res;
}

int Mesh::save (const char *filename)
{
	if (!filename)
		return -1;

	if (strcmp (filename+(strlen(filename)-4), ".asc") == 0)
	  return export_asc (filename);
	else if (strcmp (filename+(strlen(filename)-5), ".npts") == 0 ||
		 strcmp (filename+(strlen(filename)-5), ".pset") == 0)
	  return export_pset (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".obj") == 0)
	  return export_obj (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".dae") == 0)
	  return export_dae (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".cpp") == 0)
	  return export_cpp (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".gts") == 0)
	  return export_gts (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".off") == 0)
	  return export_off (filename);
	else if (strcmp (filename+(strlen(filename)-4), ".pts") == 0)
	  return export_pts (filename);
	else if (strcmp(filename + (strlen(filename) - 4), ".ply") == 0)
		return export_ply(filename);
	else if (strcmp(filename + (strlen(filename) - 4), ".stl") == 0)
		return export_stl(filename);

	return -1;
}

//
// OBJ
//
#define BUFFER_SIZE 4096

int Mesh::import_mtl (const char *filename, const char *path)
{
	FILE *ptr = NULL;
	char filename_full[BUFFER_SIZE];
	if (path)
	{
		sprintf (filename_full, "%s/%s", path, filename);
		ptr = fopen (filename_full, "r");
	}
	else
		ptr = fopen (filename, "r");
	if (!ptr)
		return -1;

	char line[BUFFER_SIZE];
	char prefix[BUFFER_SIZE];
	char name[BUFFER_SIZE];
	unsigned int line_count = 0;
	float r, g, b, a;
	unsigned int dummy;
	unsigned int mi = MATERIAL_NONE;
	MaterialColorExt *mat = NULL;

	while (fgets(line, sizeof(line), ptr) != NULL)
	{
		line_count++;

		if (line[0] == 0 || line[0] == '#') // skip empty lines and comments
			continue;

		if (sscanf(line, " newmtl %s", name) == 1) // new material
		{
			mat = new MaterialColorExt ();
			mi = Material_Add(mat);
			mat->m_pName = strdup(name);
		}
		else if (sscanf(line, " Kd %f %f %f", &r, &g, &b) == 3 && mat) mat->SetDiffuse (r, g, b, 1.);
		else if (sscanf(line, " Ka %f %f %f", &r, &g, &b) == 3 && mat) mat->SetAmbient (r, g, b, 1.);
		else if (sscanf(line, " Ks %f %f %f", &r, &g, &b) == 3 && mat) mat->SetSpecular (r, g, b, 1.);
		else if (sscanf(line, " Ke %f %f %f", &r, &g, &b) == 3 && mat) mat->SetEmission (r, g, b, 1.);
		else if (sscanf(line, " Tf %f %f %f", &r, &g, &b) == 3 && mat) {} // transmissive
		else if (sscanf(line, " d %f", &a) == 1) {} // ...
		else if (sscanf(line, " Tr %f", &a) == 1) {} // ...
		else if (sscanf(line, " Ns %f Ni %f", &r, &g) == 2) {} // ...
		else if (sscanf(line, " Ns %f", &r) == 1) {} // ...
		else if (sscanf(line, " Ni %f", &r) == 1) {} // ...
		else if (sscanf(line, " illum %d", &dummy) == 1) {} // ...
		else if (sscanf(line, " map_Kd %s", name) == 1) { // diffuse texture
			MaterialTexture *tex = new MaterialTexture (name, path);
			tex->SetName (mat->GetName());
			printf ("%d %d\n", tex->GetImage()->m_iWidth, tex->GetImage()->m_iHeight);
			delete mat;
			mat = NULL;
			SetMaterial (mi, tex);
		}
		else if (strcmp(line, "map_Ka ") == 0) {} // ambient texture
		else if (strcmp(line, "map_Ks ") == 0) {} // specular texture
		else if (strcmp(line, "map_bump ") == 0) {} // bumpmap
		else if (strcmp(line, "bump ") == 0) {} // bumpmap
		else
			printf("MTL parse error line %d: '%s'", line_count, line);
	}

	fclose (ptr);

	return 0;
}

int Mesh::import_obj (const char *filename)
{
	if (filename == NULL)
		return NULL;

	char buffer[BUFFER_SIZE];
	char prefix[BUFFER_SIZE];

	FILE *file = fopen (filename, "r");
	if (file == NULL)
	{
		printf ("Unable to open %s", filename);
		return -1;
	}
	
	unsigned int nPoints=0;
	unsigned int nTexCoords=0;
	unsigned int nFaces=0;
	char mtlfile[BUFFER_SIZE];
	mtlfile[0] = '\0';
	while (fgets (buffer, BUFFER_SIZE, file))
	{
		if (sscanf(buffer, "%s", prefix) != -1)
		{
			if (strcmp(prefix, "mtllib") == 0)
				sscanf(buffer, "%s %s", prefix, mtlfile);
			else if (strcmp(prefix, "v") == 0)
				nPoints++;
			else if (strcmp(prefix, "vt") == 0)
				nTexCoords++;
			else if (strcmp(prefix, "f") == 0)
				nFaces++;
		}
	}
	//printf ("%d %d %d\n", nPoints, nTexCoords, nFaces);
	rewind (file);
	Init (nPoints, nFaces);

	if (strlen (mtlfile) != 0)
	{
		auto dir = std::filesystem::path(filename).parent_path();
		import_mtl (mtlfile, dir.string().c_str());
	}

	if (nTexCoords)
	{
		m_nTextureCoordinates = nTexCoords;
		m_pTextureCoordinates = new float[2*nTexCoords];
	}
	int ipoint = 0, itexcoord = 0, iface = 0;
	int usemtl = -1;
	while (fgets (buffer, BUFFER_SIZE, file))
	{
		if (sscanf(buffer, "%s", prefix) == -1)
			continue;
		if (strcmp (prefix, "usemtl") == 0)
		{
			char mtl_name[BUFFER_SIZE];
			sscanf (buffer, "%s %s", prefix, mtl_name);
			usemtl = GetMaterialId (mtl_name);
		}
		else if (strcmp (prefix, "v") == 0)
		{
			sscanf (buffer, "%s %f %f %f", prefix,
				&m_pVertices[3*ipoint],
				&m_pVertices[3*ipoint+1],
				&m_pVertices[3*ipoint+2]);
			ipoint++;
		}
		else if (strcmp (prefix, "vt") == 0)
		{
			sscanf (buffer, "%s %f %f", prefix,
				&m_pTextureCoordinates[2*itexcoord],
				&m_pTextureCoordinates[2*itexcoord+1]);
			itexcoord++;
		}
		else if (strcmp (prefix, "f") == 0)
		{
			// get the number of indices
			unsigned int fvn = 0;
			char *s = buffer + sizeof("f ") - 1;
			while (*s != 0) {
				while (*s && isspace(*s))
					s++;
				while (*s && !isspace(*s))
					s++;
				while (*s && isspace(*s))
					s++;
				fvn++;
			}
			Face *pFace = m_pFaces[iface];
			if (!pFace)
				pFace = new Face ();

			if (fvn != 3)
				pFace->SetNVertices (fvn);
			
			// parse the indices
			fvn = 0;
			s = buffer + sizeof("f ") - 1;
			while (*s != 0) {
				int i0, i1, i2;
				int h0, h1, h2;
				
				h0 = h1 = h2 = 0;
				i0 = i1 = i2 = 0;

				if (sscanf(s, "%d/%d/%d", &i0, &i1, &i2) == 3)
					h0 = h1 = h2 = 1;
				else if (sscanf(s, "%d/%d", &i0, &i1) == 2)
					h0 = h1 = 1;
				else if (sscanf(s, "%d//%d", &i0, &i2) == 2)
					h0 = h2 = 1;
				else if (sscanf(s, "/%d/%d", &i1, &i2) == 2)
					h1 = h2 = 1;
				else if (sscanf(s, "%d", &i0) == 1)
					h0 = 1;
				else if (sscanf(s, "/%d", &i1) == 1)
					h1 = 1;
				else if (sscanf(s, "//%d", &i2) == 1)
					h2 = 1;
		
				while (*s && isspace(*s))
					s++;
				while (*s && !isspace(*s))
					s++;
				while (*s && isspace(*s))
					s++;

				if (!h0)
					continue;

				if (i0 < 0)
					i0 = m_nVertices + i0;
				else
					i0--;
				
				if (i0 < 0 || (unsigned int) i0 >= m_nVertices) {
					printf ("invalid vertex index %d (vn=%d)\n", i0, m_nVertices);
					continue;
				}

				pFace->SetVertex (fvn, i0);

				if (h1)
				{
					if (i1 < 0)
						i1 = m_nTextureCoordinates + i1;
					else
						i1--;
					pFace->m_bUseTextureCoordinates = true;
					pFace->ActivateTextureCoordinatesIndices();
					pFace->ActivateTextureCoordinates();
					pFace->SetTexCoord (fvn, i1);
				}
				fvn++;
			}

			// material
			if (usemtl != -1)
				pFace->m_iMaterialId = usemtl;

			m_pFaces[iface] = pFace;
			iface++;
		}
	}
	fclose (file);

	return 0;
}

//
//
//
int Mesh::export_obj (const char *filename)
{
	FILE *fp;
	unsigned int i;

	fp = fopen(filename,"w");
	if (fp == NULL)
		return -1;

	// some comments
	fprintf (fp, "#\n");
	fprintf (fp, "# number of vertices : %d\n", m_nVertices);
	fprintf (fp, "# number of faces    : %d\n", m_nFaces);
	fprintf (fp, "#\n");
	fprintf (fp, "\n");

	// materials
	char *filematname = NULL;
	if (m_nMaterials > 0)
	{
		filematname = strdup (filename);
		sprintf (filematname+strlen (filematname)-3, "%s", "mtl");

		char *s = strrchr(filematname, '/');
		fprintf (fp, "mtllib %s\n\n", (s != NULL)? &s[1]:filematname);
	}

	//
	// vertices
	//
	for (i = 0; i < m_nVertices; i++)
		fprintf (fp, "v %f %f %f\n",
			 m_pVertices[3*i], m_pVertices[3*i+1], m_pVertices[3*i+2]);
	if (0 && m_pVertexNormals)
	{
		for (i=0; i<m_nVertices; i++)
		{
			fprintf (fp, "vn %f %f %f\n",
				 m_pVertexNormals[3*i],
				 m_pVertexNormals[3*i+1],
				 m_pVertexNormals[3*i+2]);
		}
	}
	
	if (m_pTextureCoordinates)
	{
		for (i=0; i<m_nTextureCoordinates; i++)
			fprintf (fp, "vt %f %f\n", m_pTextureCoordinates[2*i], m_pTextureCoordinates[2*i+1]);
	}

	//
	// faces
	//
	unsigned int i_current_material = MATERIAL_NONE;
	for (i = 0; i <m_nFaces; i++)
	{
		if (!m_pFaces[i])
			continue;
		
		if (m_pFaces[i]->GetMaterialId () != MATERIAL_NONE &&
		    m_pFaces[i]->GetMaterialId () != i_current_material)
		{
			fprintf (fp, "usemtl %s\n", m_pMaterials[m_pFaces[i]->GetMaterialId ()]->GetName());
			i_current_material = m_pFaces[i]->GetMaterialId ();
		}

		if (0 && m_pFaces[i]->m_pTextureCoordinatesIndices)//m_pFaces[i]->m_bUseTextureCoordinates)
		{
			for (unsigned int j=0; j<m_pFaces[i]->m_nVertices; j++)
			{
				//fprintf (fp, "vt %f %f\n",
				//	 m_pTextureCoordinates[2*m_pFaces[i]->m_pTextureCoordinates[j]],
				//	 m_pTextureCoordinates[2*m_pFaces[i]->m_pTextureCoordinates[j]+1]);
				fprintf (fp, "vt %f %f\n",
					 m_pFaces[i]->m_pTextureCoordinates[2*j],
					 m_pFaces[i]->m_pTextureCoordinates[2*j+1]);
			}
		}

		fprintf (fp, "f ");
		for (unsigned int j=0; j<m_pFaces[i]->m_nVertices; j++)
		{
			// vertex
			fprintf (fp, "%d", 1+m_pFaces[i]->m_pVertices[j]);

			// texture coordinates
			if (m_pFaces[i]->m_bUseTextureCoordinates && m_pFaces[i]->m_pTextureCoordinatesIndices)
				fprintf (fp, "/%d", 1+m_pFaces[i]->m_pTextureCoordinatesIndices[j]);//m_pFaces[i]->m_nVertices);

			// normal
			if (0 && m_pVertexNormals)
			{
				if (!m_pFaces[i]->m_bUseTextureCoordinates)
					fprintf (fp, "/");
				fprintf (fp, "/%d\n", m_pFaces[i]->m_pVertices[j]);
			}
			
			fprintf (fp, " ");
		}
		fprintf (fp, "\n");
	}
	
	fclose (fp);

	//
	// materials
	//
	if (m_nMaterials > 0)
	{
		fp = fopen(filematname,"w");
		if (fp == NULL)
			return -1;
		
		fprintf (fp, "\n");
		fprintf (fp, "# Wavefront material file\n");
		fprintf (fp, "\n");

		for (unsigned int i=0; i<m_nMaterials; i++)
		{
			Material *pMaterial = m_pMaterials[i];
			if (!pMaterial)
				continue;
			switch (pMaterial->GetType ())
			{
			case MATERIAL_COLOR:
			{
				MaterialColor *pMaterialColor = dynamic_cast<MaterialColor*> (pMaterial);
				fprintf (fp, "newmtl material_%d\n", i);
				fprintf (fp, "Ka 0.200000 0.200000 0.200000\n");
				fprintf (fp, "Kd %f %f %f\n",
					 pMaterialColor->m_r/255., pMaterialColor->m_g/255., pMaterialColor->m_b/255.);
				fprintf (fp, "Ks 1.000000 1.000000 1.000000\n");
				fprintf (fp, "Tr 1.000000\n");
				fprintf (fp, "illum 2\n");
				fprintf (fp, "Ns 0.000000\n");
				fprintf (fp, "\n");
			}
			break;
			case MATERIAL_TEXTURE:
			{
				MaterialTexture *pMaterialTexture = dynamic_cast<MaterialTexture*> (pMaterial);
				fprintf (fp, "newmtl %s\n", pMaterialTexture->GetName ());
				fprintf (fp, "Ka 0.000000 0.000000 0.000000\n");
				fprintf (fp, "Kd 0.000000 0.000000 0.000000\n");
				fprintf (fp, "Ks 1.000000 1.000000 1.000000\n");
				fprintf (fp, "Tr 1.000000\n");
				fprintf (fp, "illum 2\n");
				fprintf (fp, "Ns 0.000000\n");
				fprintf (fp, "map_Kd %s\n", pMaterialTexture->GetFilename());
				fprintf (fp, "\n");
			}
			break;
			default:
				break;
			}
		}
		
		fclose (fp);

		free (filematname);
	}

	return 0;
}

//
// ASC
//
int Mesh::import_asc (const char *filename)
{
	if (filename == NULL)
		return -1;

	FILE *file = fopen (filename, "r");
	if (file == NULL)
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
	Init (nPoints, 0);
	InitVertexColors ();
	unsigned int i=0;
	while (!feof (file))
	{
		fscanf (file, "%f %f %f %d %d %d %f %f %f\n", &vx, &vy, &vz, &r, &g, &b, &nx, &ny, &nz);
		m_pVertices[3*i+0] = vx;
		m_pVertices[3*i+1] = vy;
		m_pVertices[3*i+2] = vz;
		m_pVertexColors[3*i+0]	= r/255.;
		m_pVertexColors[3*i+1]	= g/255.;
		m_pVertexColors[3*i+2]	= b/255.;
		m_pVertexNormals[3*i+0] = nx;
		m_pVertexNormals[3*i+1] = ny;
		m_pVertexNormals[3*i+2] = nz;
		i++;
	}
	fclose (file);
//	triangulate_regular_heightfield (640, 480);
	return 0;
}

int Mesh::export_asc (const char *filename)
{
	FILE *ptr = fopen (filename, "w");
	printf ("ASC : %d\n", m_nVertices);
	unsigned char r=0, g=0, b=0;
	if (m_pVertexColors)
	{
		for (unsigned int i=0; i<m_nVertices; i++)
		{
			r = (unsigned char)(255.*m_pVertexColors[3*i+0]);
			g = (unsigned char)(255.*m_pVertexColors[3*i+1]);
			b = (unsigned char)(255.*m_pVertexColors[3*i+2]);
			fprintf (ptr, "%f %f %f %d %d %d %f %f %f\n",
				 m_pVertices[3*i+0], m_pVertices[3*i+1], m_pVertices[3*i+2],
				 r, g, b,
				 m_pVertexNormals[3*i+0], m_pVertexNormals[3*i+1], m_pVertexNormals[3*i+2]
				);
		}
	}
	else
	{
		for (unsigned int i=0; i<m_nVertices; i++)
		{
			fprintf (ptr, "%f %f %f %d %d %d %f %f %f\n",
				 m_pVertices[3*i+0], m_pVertices[3*i+1], m_pVertices[3*i+2],
				 r, g, b,
				 m_pVertexNormals[3*i+0], m_pVertexNormals[3*i+1], m_pVertexNormals[3*i+2]
				);
		}
	}

	fclose (ptr);

	return 0;
}

//
// PSET
//
int Mesh::import_pset (const char *filename)
{
	if (filename == NULL)
		return -1;

	FILE *file = fopen (filename, "r");
	if (file == NULL)
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
	Init (nPoints, 0);
	unsigned int i=0;
	while (!feof (file))
	{
		fscanf (file, "%f %f %f %f %f %f\n",
			&m_pVertices[i],
			&m_pVertices[i+1],
			&m_pVertices[i+2],
			&m_pVertexNormals[i],
			&m_pVertexNormals[i+1],
			&m_pVertexNormals[i+2]);
		i+=3;
	}
	fclose (file);

	return 0;
}

int Mesh::export_pset (const char *filename)
{
	FILE *ptr = fopen (filename, "w");
	
	for (unsigned int i=0; i<m_nVertices; i++)
	{
		fprintf (ptr, "%f %f %f %f %f %f\n",
			 m_pVertices[3*i+0], m_pVertices[3*i+1], m_pVertices[3*i+2],
			 m_pVertexNormals[3*i+0], m_pVertexNormals[3*i+1], m_pVertexNormals[3*i+2]
			 );
	}

	fclose (ptr);

	return 0;
}



//
// DAE
//
int Mesh::export_dae (const char *filename)
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
	fprintf (ptr, "          <float_array id=\"GEO01-Position-array\" count=\"%d\">", 3*m_nVertices);
	for (int i=0; i<3*m_nVertices; i++)
		fprintf (ptr, "%f ", m_pVertices[i]);
	fprintf (ptr, "</float_array>\n");
	fprintf (ptr, "          <technique_common>\n\n");
	fprintf (ptr, "            <accessor source=\"#GEO01-Position-array\" count=\"%d\" stride=\"3\">\n", m_nVertices);
	fprintf (ptr, "              <param name=\"X\" type=\"float\"/>\n");
	fprintf (ptr, "              <param name=\"Y\" type=\"float\"/>\n");
	fprintf (ptr, "              <param name=\"Z\" type=\"float\"/>\n");
	fprintf (ptr, "            </accessor>\n");
	fprintf (ptr, "          </technique_common>\n");
	fprintf (ptr, "        </source>\n");
	fprintf (ptr, "        <source id=\"GEO01-UV\">\n");
	fprintf (ptr, "          <float_array id=\"GEO01-UV-array\" count=\"%d\">", 2*m_nTextureCoordinates);
	for (int i=0; i<2*m_nTextureCoordinates; i++)
		fprintf (ptr, "%f ", m_pTextureCoordinates[i]);
	fprintf (ptr, "</float_array>\n");
	fprintf (ptr, "          <technique_common>\n");
	fprintf (ptr, "            <accessor source=\"#GEO01-UV-array\" count=\"%d\" stride=\"2\">\n", m_nTextureCoordinates);
	fprintf (ptr, "              <param name=\"S\" type=\"float\"/>\n");
	fprintf (ptr, "              <param name=\"T\" type=\"float\"/>\n");
	fprintf (ptr, "            </accessor>\n");
	fprintf (ptr, "          </technique_common>\n");
	fprintf (ptr, "        </source>\n");
	fprintf (ptr, "        <vertices id=\"GEO01-Vertex\">\n");
	fprintf (ptr, "          <input semantic=\"POSITION\" source=\"#GEO01-Position\"/>\n");
	fprintf (ptr, "        </vertices>\n");
	fprintf (ptr, "        <triangles material=\"mat0\" count=\"%d\">\n", m_nFaces);
	fprintf (ptr, "          <input offset=\"0\" semantic=\"VERTEX\" source=\"#GEO01-Vertex\"/>\n");
	fprintf (ptr, "          <input offset=\"1\" semantic=\"TEXCOORD\" source=\"#GEO01-UV\"/>\n");
	fprintf (ptr, "          <p>");
	for (int i=0; i<m_nFaces; i++)
		for (int j=0; j<m_pFaces[i]->m_nVertices; j++)
			fprintf (ptr, "%d %d ", m_pFaces[i]->m_pVertices[j], m_pFaces[i]->m_pTextureCoordinatesIndices[j]);
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
int Mesh::export_cpp  (const char *filename)
{
	int i;
  FILE *ptr = fopen (filename, "w");
  if (!ptr)
    {
      printf ("unable to open %s\n", filename);
      return false;
    }
  char *modelname = strdup (filename);
  for (i=strlen (modelname); modelname[i]!='.' && i>0; i--);
  if (i!=0 && modelname[i-1]!='/') modelname[i] = '\0';
  for (i=strlen (modelname); modelname[i]!='/' && i>=0; i--);
  if (i!=-1) modelname = &modelname[i+1];
  if (strlen(modelname)==0)
    {
      printf ("unable to extract name of the file\n");
      return false;
    }

  fprintf (ptr, "/* model coming from %s */\n\n", filename);

  /* n_vertices & n_faces */
  int n_vertices = m_nVertices;
  int n_faces    = m_nFaces;
  fprintf (ptr, "static int %s_n_vertices = %d;\n", modelname, n_vertices);
  fprintf (ptr, "static int %s_n_faces = %d;\n\n", modelname, n_faces);
  
  /* vertices */
  float x, y, z;
  fprintf (ptr, "static float %s_vertices[] = {", modelname);
  x = m_pVertices[0];
  y = m_pVertices[1];
  z = m_pVertices[2];
  fprintf (ptr, "%f, %f, %f,\n", x, y, z);
  for (i=1; i<n_vertices-1; i++)
    {
  x = m_pVertices[3*i];
  y = m_pVertices[3*i+1];
  z = m_pVertices[3*i+2];
      fprintf (ptr, "\t\t%f, %f, %f,\n", x, y, z);
    }
  x = m_pVertices[3*i];
  y = m_pVertices[3*i+1];
  z = m_pVertices[3*i+2];
  fprintf (ptr, "\t\t%f, %f, %f};\n\n", x, y, z);

  /* faces */
  int a, b, c;
  fprintf (ptr, "static int %s_faces[] = {", modelname);
  a = m_pFaces[0]->GetVertex(0);
  b = m_pFaces[0]->GetVertex(1);
  c = m_pFaces[0]->GetVertex(2);
  fprintf (ptr, "%d, %d, %d,\n", a, b, c);
  for (i=1; i<n_faces-1; i++)
    {
  a = m_pFaces[i]->GetVertex(0);
  b = m_pFaces[i]->GetVertex(1);
  c = m_pFaces[i]->GetVertex(2);
      fprintf (ptr, "\t\t%d, %d, %d,\n", a, b , c);
    }
  a = m_pFaces[i]->GetVertex(0);
  b = m_pFaces[i]->GetVertex(1);
  c = m_pFaces[i]->GetVertex(2);
  fprintf (ptr, "\t\t%d, %d, %d};\n\n", a, b, c);

  /* vertices normales */
  fprintf (ptr, "static float %s_vertices_normales[] = {", modelname);
  float *vertices_normales = m_pVertexNormals;
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
int Mesh::export_gts (const char *filename)
{
  int i;
  FILE *ptr = fopen (filename, "w");
  
  fprintf (ptr, "%d %d %d\n", m_nVertices, 3*m_nFaces, m_nFaces);
  
  // vertices
  for (i=0; i<m_nVertices; i++)
    fprintf (ptr, "%f %f %f\n", m_pVertices[3*i], m_pVertices[3*i+1], m_pVertices[3*i+2]);

  // edges
  for (i=0; i<m_nFaces; i++)
    {
	    int a = m_pFaces[i]->GetVertex (0);
	    int b = m_pFaces[i]->GetVertex (1);
	    int c = m_pFaces[i]->GetVertex (2);
      fprintf (ptr, "%d %d\n", 1+a, 1+b);
      fprintf (ptr, "%d %d\n", 1+b, 1+c);
      fprintf (ptr, "%d %d\n", 1+c, 1+a);
    }

  // faces
  for (i=0; i<m_nFaces; i++)
    fprintf (ptr, "%d %d %d\n", 3*i+1, 3*i+2, 3*i+3);

  fclose (ptr);

  return 0;
}

//
// IFS
//
int Mesh::import_ifs (const char *filename)
{
	FILE *ptr = fopen (filename, "rb");
	if (ptr == NULL)
	{
		printf ("unable to open %s\n", filename);
		return false;
	}
	
	int length;
	char *buffer[256];
	
	// magic number "IFS"
	fread (&length, sizeof(unsigned int), 1, ptr);
	fread (buffer, sizeof(char), length, ptr);
	
	// version "1.0"
	float version;
	fread (&version, sizeof(float), 1, ptr);
	if (version != 1.0)
	{
		printf ("Bad Version: %f\n", version);
		return false;
	}
  
  // modelname
  fread (&length, sizeof(unsigned int), 1, ptr);
  fread (buffer, sizeof(char), length, ptr);
  
  // vertexheader "VERTICES"
  fread (&length, sizeof(unsigned int), 1, ptr);
  fread (buffer, sizeof(char), length, ptr);
  
  // vertices
  fread (&m_nVertices, sizeof(unsigned int), 1, ptr);
  InitVertices (m_nVertices);
  fread (m_pVertices, sizeof(float), 3*m_nVertices, ptr);
  
  // triangleheader "FACES"
  fread (&length, sizeof(unsigned int), 1, ptr);
  fread (buffer, sizeof(char), length, ptr);
  
  // faces
  fread (&m_nFaces, sizeof(unsigned int), 1, ptr);
  InitFaces (m_nFaces);
  int *f = (int*)malloc(3*m_nFaces*sizeof(int));
  fread (f, sizeof(float), 3*m_nFaces, ptr);
  for (int i=0; i<m_nFaces; i++)
  {
	  Face *pFace = new Face ();
	  pFace->SetTriangle (f[3*i], f[3*i+1], f[3*i+2]);
	  m_pFaces[i] = pFace;
  }
  
  fclose (ptr);

  return 0;
}

//
// import lwo
//
int Mesh::import_lwo (const char *filename)
{
  FILE *ptr = fopen (filename, "rb");
  if (!ptr)
    {
      printf ("unable to open %s\n", filename);
      return false;
    }

  char TagId[5];
  memset (TagId, 0, 5*sizeof(char));
  unsigned long TagSizeU4;
  unsigned short TagSizeU2;
  char TagName[256];


  // FORM
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("FORM", TagId))
  {
	  return false;
  }
  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  swap_endian_4 (&TagSizeU4);

  // LWO2
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("LWO2", TagId))
  {
	  return false;
  }

  // TAGS
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("TAGS", TagId))
  {
	  return false;
  }
  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  swap_endian_4 (&TagSizeU4);

  fread (TagName, TagSizeU4*sizeof(char), 1, ptr);

  // LAYR
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("LAYR", TagId))
  {
	  return false;
  }

  fread (&TagSizeU2, sizeof(unsigned short), 1, ptr);
  swap_endian_2 (&TagSizeU2);
  fread (&TagSizeU2, sizeof(unsigned short), 1, ptr);
  swap_endian_2 (&TagSizeU2);


  float pivot[3]={0.0, 0.0, 0.0};
  fread (pivot, sizeof(float), 3, ptr);

  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  char Name[256];
  memset (Name, 0, 256);
  int NameSize=0;
  do
  {
	  fread (&Name[NameSize], sizeof(char), 1, ptr);
  } while (Name[NameSize++]!='\0');
  printf ("%s\n", Name);

  // PNTS
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("PNTS", TagId))
  {
	  return false;
  }

  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  swap_endian_4 (&TagSizeU4);
  int nVertices = TagSizeU4 / 12;


	for (int i=0; i<nVertices; i++)
	{
	  float coord[3]={0.0, 0.0, 0.0};
	  fread (&coord, 3*sizeof(float), 1, ptr);
	  printf ("%f %f %f\n", coord[0], coord[1], coord[2]);
	}

  // BBOX
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("BBOX", TagId))
  {
	  return false;
  }
  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  swap_endian_4 (&TagSizeU4);

  float min[3], max[3];
  fread (&min, sizeof(float), 3, ptr);
  printf ("min : %f %f %f\n", min[0], min[1], min[2]);
  fread (&max, sizeof(float), 3, ptr);
  printf ("max : %f %f %f\n", max[0], max[1], max[2]);

  // POLS
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("POLS", TagId))
  {
	  return false;
  }
  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  swap_endian_4 (&TagSizeU4);

  // FACE
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("FACE", TagId))
  {
	  return false;
  }

  for (int i=0; i<12; i++)
  {
	  unsigned short NbrVertices;
	  unsigned short Index;
	  fread (&NbrVertices, sizeof(unsigned short), 1, ptr);
	  swap_endian_2 (&NbrVertices);
	  printf ("%d : ", NbrVertices);
	  for (int j=0; j<NbrVertices; j++)
	  {
		  fread (&Index, sizeof(unsigned short), 1, ptr);
		  swap_endian_2 (&Index);
		  printf ("%d ", Index);
	  }
	  printf ("\n");
  }

  // PTAG
  fread (TagId, sizeof(unsigned char), 4, ptr);
  if (strcmp ("PTAG", TagId))
  {
	  return false;
  }
  fread (&TagSizeU4, sizeof(unsigned long), 1, ptr);
  swap_endian_4 (&TagSizeU4);


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
int Mesh::import_off (const char *filename)
{
  char id[4];
  int i, nSegments;

  FILE* ptr = fopen (filename,"r");
  if (!ptr)
	  return -1;
  
  // Get header information
  fgets (id, 4, ptr);
  DASSERT (id[0]=='O' && id[1]=='F' && id[2]=='F');
  fscanf (ptr, "%d %d %d", &m_nVertices, &m_nFaces, &nSegments);
  
  // memory allocation
  Init (m_nVertices, m_nFaces);
 
  // Get the vertices
  for (i=0; i<m_nVertices; i++)
    fscanf (ptr, "%f %f %f", &m_pVertices[3*i], &m_pVertices[3*i+1], &m_pVertices[3*i+2]);

  // Get the faces
  for (i=0; i<m_nFaces; i++)
    {
      //float r_tmp, g_tmp, b_tmp;
      int n_vertices_in_face;

      fscanf (ptr, "%d", &n_vertices_in_face);
      if (n_vertices_in_face != 3)
	{
	  printf ("illegal file\n");
	  return false;
	}

      /*
      fscanf_s (ptr, "%d %d %d %f %f %f",
	      &f[3*i], &f[3*i+1], &f[3*i+2],
	      &r_tmp, &g_tmp, &b_tmp);
      */

      Face *pFace = new Face ();
      fscanf (ptr, "%d %d %d",
	      &pFace->m_pVertices[0], &pFace->m_pVertices[1], &pFace->m_pVertices[2]);

    }
  fclose (ptr);

  return 0;
}

/**
* export into off file
*/
int Mesh::export_off (const char *filename)
{
  FILE *ptr;
  int i;

  ptr = fopen (filename,"w");
  if (!ptr) return false;

  // header
  fprintf (ptr, "OFF\n");
  fprintf (ptr, "%ud %ud %d\n", m_nVertices, m_nFaces, 0);

  // vertices
  for (i=0; i<m_nVertices; i++)
    fprintf (ptr, "%f %f %f\n", m_pVertices[3*i], m_pVertices[3*i+1], m_pVertices[3*i+2]);

  // faces
  //for (i=0; i<n_faces; i++)
  //  fprintf (ptr, "3 %d %d %d 0.5 0.5 0.5\n",
  //	     f[3*i], f[3*i+1], f[3*i+2]);
  for (i=0; i<m_nFaces; i++)
  {
	  fprintf (ptr, "%d %d %d\n",
		   m_pFaces[i]->GetVertex (0), m_pFaces[i]->GetVertex (1), m_pFaces[i]->GetVertex (2));
  }

  fclose (ptr);

  return 0;
}

//
// PGM
//
// static function used by void import_pgm (char *filename)
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
int Mesh::import_pgm (const char *filename)
{
  int width, height, levels;
  //int level_walk;
  unsigned char *data = NULL;
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
  fscanf (ptr, "%c %c", &id[0], &id[1]);
  if (id[0]!='P' && id[1]!='2' && id[1]!='5')
    {
      printf ("\"%s\" is not a valid PGM file\n", filename);
      return false;
    }
  _pgm_skip_spaces (ptr);
  fscanf (ptr, "%d %d", &width, &height);
  _pgm_skip_spaces (ptr);
  fscanf (ptr, "%d", &levels);
  _pgm_skip_spaces (ptr);

  /* Create the data */
  data = (unsigned char*)malloc(width*height*sizeof(unsigned char));
  DASSERT (data);

  /* Get the data */
  if (id[1]=='2') /* ascii mode */
  {
    for (j=0; j<height; j++)
      for (i=0; i<width; i++)
	  {
		fscanf (ptr, "%d", &data[j*width+i]);
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
  for (i=0; i<width*height; i++)
    data[i] = 255*(data[i]-min)/(max-min);

  // translation to center
  float x_trans = (1)? -width/2.0 : 0;
  float y_trans = (1)? -height/2.0 : 0;

  // alloc memory
  m_nVertices = width*height;
  m_nFaces = 2*(width-1)*(height-1);
  Init (m_nVertices, m_nFaces);

  // fill the structure
  for (j=0; j<height; j++)
    for (i=0; i<width; i++)
	{
	  m_pVertices[3*(j*width+i)]   = (float)i+x_trans;
	  m_pVertices[3*(j*width+i)+1] = (float)(height-1-j)+y_trans;
	  m_pVertices[3*(j*width+i)+2] = (float)(255-data[j*width+i]/3.0)-150.0;
	}

  for (j=0; j<height-1; j++)
    for (i=0; i<width-1; i++)
	{
		Face *f1 = new Face ();
		f1->SetVertex (0, j*width+i);
		f1->SetVertex (1, (j+1)*width+i);
		f1->SetVertex (2, j*width+i+1);
		Face *f2 = new Face ();
		f2->SetVertex (0, (j+1)*width+i+1);
		f2->SetVertex (1, j*width+i+1);
		f2->SetVertex (2, (j+1)*width+i);
		m_pFaces[2*(j*(width-1)+i)]   = f1;
		m_pFaces[2*(j*(width-1)+i)+1] = f2;
	}

	return 0;
}

//
// PTS
//
int Mesh::import_pts (const char *filename)
{
	int i;
  FILE *ptr = fopen (filename, "r");
  if (!ptr)
    {
      printf ("unable to open %s\n", filename);
      return -1;
    }

  char *buffer = (char*)malloc(512*sizeof(char));
  if (buffer == NULL)
  {
	  return -1;
  }
  //
  m_nVertices = 0;
  while (1)
  {
    fgets (buffer, 512, ptr);
    if (feof(ptr)) break;
    m_nVertices++;
  }
  Init (m_nVertices, 0);
  m_pVertexColors = (float*)malloc(3*m_nVertices*sizeof(float));
  if (m_pVertexColors == NULL)
  {
	  free (buffer);
	  return false;
  }

  rewind (ptr);
  unsigned int vertex_walk=0;
  unsigned int r=0, g=0, b=0;
  while (1)
  {
    fgets (buffer, 512, ptr);
    if (feof(ptr)) break;

	sscanf (buffer, "%f %f %f %d %d %d", 
		&m_pVertices[3*vertex_walk],
		&m_pVertices[3*vertex_walk+1],
		&m_pVertices[3*vertex_walk+2],
		&r, &g, &b);
	m_pVertexColors[3*vertex_walk]   = (float)r/255.0;
	m_pVertexColors[3*vertex_walk+1] = (float)g/255.0;
	m_pVertexColors[3*vertex_walk+2] = (float)b/255.0;

	vertex_walk++;
  }
  fclose (ptr);

  free (buffer);

  return 0;
}

int Mesh::export_pts (const char *filename)
{
  FILE *ptr = fopen (filename, "w");
  if (!ptr)
	{
	  printf ("unable to open %s\n", filename);
	  return false;
	}

  if (m_pVertices != NULL && m_pVertexColors != NULL)
  {
	  for (int i=0; i<m_nVertices; i++)
	  {
		  fprintf (ptr, "%f %f %f %d %d %d\n",
			  m_pVertices[3*i], m_pVertices[3*i+1], m_pVertices[3*i+2],
			  (int)(m_pVertexColors[3*i]*255.0), (int)(m_pVertexColors[3*i+1]*255.0), (int)(m_pVertexColors[3*i+2]*255.0));
	  }
  }
  else
  {
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
    ply_get_argument_user_data(argument, &pMesh, NULL);
    Mesh *mesh = (Mesh*)pMesh;


   long length, value_index;
    ply_get_argument_property(argument, NULL, &length, &value_index);

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

int Mesh::import_ply (const char *filename)
{
	int i;
	long nvertices, ntriangles;
	p_ply ply = ply_open(filename, NULL, 0, NULL);
	if (!ply)
	{
		printf ("unable to open %s\n", filename);
		return -1;
	}
	if (!ply_read_header(ply))
		return -1;
	
	nvertices = ply_set_read_cb(ply, "vertex", "x", vertex_cb_coords, this, 0);
	ply_set_read_cb(ply, "vertex", "y", vertex_cb_coords, this, 1);
	ply_set_read_cb(ply, "vertex", "z", vertex_cb_coords, this, 2);
	
	int bNormals = ply_set_read_cb(ply, "vertex", "nx", vertex_cb_normals, this, 0);
	ply_set_read_cb(ply, "vertex", "ny", vertex_cb_normals, this, 1);
	ply_set_read_cb(ply, "vertex", "nz", vertex_cb_normals, this, 2);
	
	int bColors = ply_set_read_cb(ply, "vertex", "diffuse_red", vertex_cb_colors, this, 0);
	ply_set_read_cb(ply, "vertex", "diffuse_green", vertex_cb_colors, this, 1);
	ply_set_read_cb(ply, "vertex", "diffuse_blue", vertex_cb_colors, this, 2);
	
	ntriangles = ply_set_read_cb(ply, "face", "vertex_indices", face_cb, this, 0);
	
	Init (nvertices, ntriangles);
	if (bColors)
		InitVertexColors ();
	
	m_nVertices = (unsigned int)(-1);
	m_nFaces = (unsigned int)(-1);
	
	if (!ply_read(ply))
		return -1;
	
	m_nVertices = nvertices;
	m_nFaces = ntriangles;
	
	ply_close(ply);
	
	return 0;
}

int Mesh::export_ply (const char *filename)
{
    const char *value;
    p_ply oply = ply_create(filename, PLY_LITTLE_ENDIAN, NULL, 0, NULL);
    if (!oply)
	    return -1;

    ply_add_element(oply, "vertex", m_nVertices);
    ply_add_property(oply, "x", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    ply_add_property(oply, "y", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    ply_add_property(oply, "z", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    if (m_pVertexNormals)
    {
	    ply_add_property(oply, "nx", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
	    ply_add_property(oply, "ny", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
	    ply_add_property(oply, "nz", PLY_FLOAT, PLY_FLOAT, PLY_FLOAT);
    }

    // write output header
    if (!ply_write_header(oply))
	    return -1;

    for (unsigned int i=0; i<m_nVertices; i++)
    {
	    ply_write (oply, m_pVertices[3*i]);
	    ply_write (oply, m_pVertices[3*i+1]);
	    ply_write (oply, m_pVertices[3*i+2]);
	    if (m_pVertexNormals)
	    {
		    ply_write (oply, m_pVertexNormals[3*i]);
		    ply_write (oply, m_pVertexNormals[3*i+1]);
		    ply_write (oply, m_pVertexNormals[3*i+2]);
	    }
    }

    // clean
    if (!ply_close(oply))
	    return -1;
    return 0;
}


int Mesh::import_stl(const char* filename)
{
	FILE* ptr = nullptr;
	ptr = fopen(filename, "rb");
	if (ptr == nullptr)
		return 1;

	char header[80];
	fread(header, 80, sizeof(char), ptr);

	unsigned int nTriangles = 0;
	fread((char*)&nTriangles, 4, 1, ptr);

	Init(3 * nTriangles, nTriangles);

	float coords[12];
	char attributes[2];
	for (unsigned int iface = 0; iface < nTriangles; iface++)
	{
		fread((char*)coords, sizeof(float), 12, ptr);
		memcpy(m_pVertices + 9*iface, coords + 3, 9 * sizeof(float));

		Face* pFace = m_pFaces[iface];
		if (!pFace)
			pFace = new Face();
		pFace->SetNVertices(3);

		for (int j = 0; j < 3; j++)
			pFace->SetVertex(j, 3*iface+j);

		fread(attributes, sizeof(unsigned char), 2, ptr);
	}

	return 0;
}
/*
int Mesh::import_stl (char *filename)
{
	FILE *ptr = fopen (filename, "rb");
	if (!ptr)
		return -1;

	char header[81];
	fread (header, sizeof(unsigned char), 80, ptr);
	header[80] = '\0';
	//printf ("%s\n", header);

	unsigned int nfaces = 0;
	fread (&nfaces, sizeof(unsigned int), 1, ptr);
	printf ("%d faces\n", nfaces);

	float normal[3];
	float faceinfo[12];
	unsigned char attribute_byte_count[2];
	int n=0;
	FILE *out = fopen ("out.obj", "w");
	int nf = 1000000;
	while (!feof (ptr))
	{
		fread (faceinfo, sizeof(float), 12, ptr);
		fread (attribute_byte_count, sizeof(unsigned char), 2, ptr);
		if (n<nf)
		{
			fprintf (out, "v %f %f %f\n", faceinfo[3], faceinfo[4], faceinfo[5]);
			fprintf (out, "v %f %f %f\n", faceinfo[6], faceinfo[7], faceinfo[8]);
			fprintf (out, "v %f %f %f\n", faceinfo[9], faceinfo[10], faceinfo[11]);
		}

		n++;
		if (n == nfaces)
			break;

		printf ("%f %f %f\n", normal[0], normal[1], normal[2]);
		printf ("%f %f %f\n", v1[0], v1[1], v1[2]);
		printf ("%f %f %f\n", v2[0], v2[1], v2[2]);
		printf ("%f %f %f\n", v3[0], v3[1], v3[2]);

	};
	for (int i=0; i<nf; i++)
		fprintf (out, "f %d %d %d\n", 1+3*i, 1+3*i+1, 1+3*i+2);
	fclose (out);
	printf ("%d / %d\n", n, nfaces);

	fclose (ptr);

	return 0;
}
*/
int Mesh::export_stl (const char *filename)
{
	return -1;
}


int Mesh::import_3ds (const char *filename)
{
	t3DSModel *p = Load3DSFile(filename, NULL);
	Free3DSModel(p);
	return 0;
}

int Mesh::export_3ds (const char *filename)
{
	t3DSModel *p = Allocate3DSModel();
	Write3DSFile(p, filename, NULL);
	Free3DSModel(p);
	return 0;
}

