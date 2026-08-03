#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mesh.h"
#include "mesh_io.h"
#include "../cgmath/cgmath.h"
#include "zip_manager.h"

// Taille des tampons de lecture ligne a ligne (etait dans mesh_io.cpp).
#define BUFFER_SIZE 4096

// ===========================================================================
//  Wavefront OBJ (+ MTL) -- import et export
// ===========================================================================
//
// Extrait de mesh_io.cpp, qui porte une douzaine de formats et depend de
// mesh_io_3ds.h et mesh_io_rply.h. L'OBJ, lui, ne depend que de Mesh : l'isoler
// permet de le compiler seul -- notamment pour la cible WebAssembly de maker, ou
// tirer tout mesh_io.cpp embarquerait 3DS et PLY pour ecrire un fichier texte.
//
// Aucun changement de comportement dans ce decoupage, a une exception pres :
// l'alignement du nom de materiau entre `usemtl` et `newmtl` (cf.
// objMaterialName ci-dessous).
//
// ===========================================================================

// Nom de materiau tel qu'ECRIT dans les fichiers, pour `usemtl` comme pour
// `newmtl`.
//
// Les deux DOIVENT sortir de la meme fonction. Ils etaient produits par deux
// expressions differentes -- `GetName()` pour le usemtl du .obj, "material_<index>"
// pour le newmtl du .mtl -- de sorte qu'aucun usemtl ne resolvait vers son newmtl
// des que le materiau portait un nom. Symptome trompeur : le fichier s'ouvre sans
// erreur et le modele sort SANS COULEUR, le lecteur ne trouvant simplement pas les
// materiaux nommes. La branche texture, elle, utilisait deja GetName().
//
// Deux garde-fous : un nom vide retombe sur l'index (un `newmtl` sans nom serait
// invalide), et les blancs deviennent des '_' car `usemtl` s'arrete au premier
// blanc -- « mon materiau » designerait le materiau « mon ».
static std::string objMaterialName (Material *mat, size_t index)
{
	std::string name = mat ? mat->GetName() : std::string();
	if (name.empty())
	{
		char buf[32];
		snprintf (buf, sizeof(buf), "material_%u", (unsigned int)index);
		return buf;
	}
	for (char &c : name)
		if (isspace ((unsigned char)c))
			c = '_';
	return name;
}

int MeshIO::import_mtl (Mesh& mesh, const char *filename, const char *path)
{
	FILE *ptr = nullptr;
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
	MaterialColorExt *mat = nullptr;
	MaterialTexture  *texMat = nullptr;   // dernier map_Kd vu, pour `refl`

	while (fgets(line, sizeof(line), ptr) != nullptr)
	{
		line_count++;

		// Sauter les lignes vides et les commentaires. `line[0] == 0` ne suffit pas :
		// fgets conserve le saut de ligne, donc une ligne vide vaut "\n" et non "".
		// Sans ce test, chaque ligne vide d'un MTL -- il y en a une entre deux
		// materiaux dans tout export Blender -- produisait une « MTL parse error ».
		{
			const char *p = line;
			while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
			if (*p == 0 || *p == '#')
				continue;
		}

		if (sscanf(line, " newmtl %s", name) == 1) // new material
		{
			mat = new MaterialColorExt ();
			mi = mesh.Material_Add(mat);
			mat->SetName (name);
			// Nouveau materiau : oublier la texture du precedent, sans quoi un
			// `refl` d'ici se collerait a la texture d'avant.
			texMat = nullptr;
		}
		else if (sscanf(line, " Kd %f %f %f", &r, &g, &b) == 3 && mat) mat->SetDiffuse (r, g, b, 1.);
		else if (sscanf(line, " Ka %f %f %f", &r, &g, &b) == 3 && mat) mat->SetAmbient (r, g, b, 1.);
		else if (sscanf(line, " Ks %f %f %f", &r, &g, &b) == 3 && mat) mat->SetSpecular (r, g, b, 1.);
		else if (sscanf(line, " Ke %f %f %f", &r, &g, &b) == 3 && mat) mat->SetEmission (r, g, b, 1.);
		else if (sscanf(line, " Tf %f %f %f", &r, &g, &b) == 3 && mat) {} // transmissive
		else if (sscanf(line, " d %f", &a) == 1) {} // ...
		else if (sscanf(line, " Tr %f", &a) == 1) {} // ...
		// OBJ Ns is a Phong specular exponent (0..1000). GL_SHININESS caps at
		// 128, and ActivateMaterial multiplies the stored value by 128, so we
		// store a 0..1 fraction. Without this the exponent stays at the default
		// 0, which makes pow(N.H, 0) == 1 everywhere: the full Ks specular is
		// then added to every lit fragment and any Ks>0 material saturates to
		// white ("all-white with grey shadow zones").
		else if (sscanf(line, " Ns %f Ni %f", &r, &g) == 2 && mat)
		{
			float e = r; if (e < 0.f) e = 0.f; if (e > 128.f) e = 128.f;
			mat->SetShininess (e / 128.f);
		}
		else if (sscanf(line, " Ns %f", &r) == 1 && mat)
		{
			float e = r; if (e < 0.f) e = 0.f; if (e > 128.f) e = 128.f;
			mat->SetShininess (e / 128.f);
		}
		else if (sscanf(line, " Ni %f", &r) == 1) {} // ...
		else if (sscanf(line, " illum %d", &dummy) == 1) {} // ...
		else if (sscanf(line, " map_Kd %s", name) == 1) { // diffuse texture
			MaterialTexture *tex = new MaterialTexture (name, path);
			if (mat)
				tex->SetName (mat->GetName());
			// La MAP EST la couleur diffuse : ambiant et diffus a BLANC, et non au
			// Kd/Ka du MTL. Meme raison que sur le chemin 3DS -- l'environnement de
			// texture est GL_MODULATE, donc reporter le Kd revient a le MULTIPLIER
			// au texel. Le materiau « BLACK » de Bar_chair_2.mtl porte
			// Kd = 0.018262 et texture le dessous en bois de l'assise : le texel
			// sortait a 1,8 % de son intensite, donc noir. `Ka 0.000000` retirait
			// en plus tout plancher ambiant.
			tex->SetAmbient (1.f, 1.f, 1.f, 1.f);
			tex->SetDiffuse (1.f, 1.f, 1.f, 1.f);

			if (mat)
			{
				tex->SetSpecular(mat->m_fSpecular[0], mat->m_fSpecular[1], mat->m_fSpecular[2], 1.f);
				// PAS de nouvelle division par 128 : import_mtl a DEJA converti le
				// Ns du MTL en fraction de 128 (`SetShininess(e / 128.f)`), et
				// ActivateMaterial remultiplie par 128 a l'affichage. Diviser ici
				// une seconde fois ramenait un Ns de 96 a un exposant de 0,75 :
				// pow(N.H, 0.75) est quasi constant, donc tout le speculaire
				// s'appliquait partout et les pieds chromes sortaient beaucoup plus
				// brillants que la reference.
				tex->SetShininess(mat->m_fShininess[0]);
			}
			// mat is owned by the Mesh's unique_ptr (mesh.Material_Add above);
			// mesh.SetMaterial(mi, tex) will reset the slot which destroys mat.
			mat = nullptr;
			mesh.SetMaterial (mi, tex);
			// Retenu pour un `refl` ulterieur : la carte de reflexion se declare
			// APRES map_Kd dans un MTL, quand `mat` a deja ete remplace.
			texMat = tex;
		}
		// Carte de REFLEXION. Elle suppose une texture diffuse deja declaree :
		// sans elle il n'y a pas de MaterialTexture pour la porter, et un
		// materiau purement reflechissant sort du modele de rendu actuel.
		else if (sscanf(line, " refl %s", name) == 1) {
			if (texMat && !texMat->SetReflectionMap (name, path))
				printf ("MTL line %d: carte de reflexion introuvable : %s\n", line_count, name);
		}
		// Maps reconnues mais NON exploitees : on les avale en silence.
		//
		// Les strcmp precedents comparaient la LIGNE ENTIERE a un prefixe comme
		// "map_Ka ", ce qui ne pouvait jamais correspondre : la ligne porte aussi le
		// nom du fichier et son saut de ligne. Toute directive de map non geree
		// tombait donc dans le `else` final et signalait une « MTL parse error » --
		// le map_Bump de Bar_chair_2.mtl en produisait deux a chaque chargement.
		// Un sscanf sur le mot-cle suivi d'un %s, comme les autres branches.
		else if (sscanf(line, " map_Ka %s",   name) == 1) {} // texture ambiante
		else if (sscanf(line, " map_Ks %s",   name) == 1) {} // texture speculaire
		else if (sscanf(line, " map_Ns %s",   name) == 1) {} // exposant speculaire
		else if (sscanf(line, " map_d %s",    name) == 1) {} // opacite
		else if (sscanf(line, " map_Bump %s", name) == 1) {} // carte de normales
		else if (sscanf(line, " map_bump %s", name) == 1) {} // idem, autre casse
		else if (sscanf(line, " bump %s",     name) == 1) {} // idem, forme courte
		else
			printf("MTL parse error line %d: '%s'", line_count, line);
	}

	fclose (ptr);

	return 0;
}

// Parse the next whitespace-delimited index token of an OBJ 'l' / 'p' element
// line, starting at *s. Reads the leading integer (the vertex reference; any
// "/vt/vn" suffix is ignored, as those elements only need positions), resolves
// OBJ's 1-based / negative indexing against the running vertex count, and
// range-checks it. Advances *s past the token.
// Returns false once the line holds no more tokens; otherwise returns true and
// sets `out` to the resolved 0-based index, or -1 if the token was malformed or
// out of range (caller skips those but keeps scanning the rest of the line).
static bool nextObjElementIndex (char *&s, int runningCount, unsigned int nVertices, int &out)
{
	while (*s && isspace ((unsigned char)*s)) s++;
	if (*s == '\0')
		return false;

	char *tok = s;
	while (*s && !isspace ((unsigned char)*s)) s++;

	out = -1;
	int idx = 0;
	if (sscanf (tok, "%d", &idx) == 1)
	{
		if (idx < 0)
			idx = runningCount + idx;   // -1 => most recently declared vertex
		else
			idx--;                      // 1-based -> 0-based
		if (idx >= 0 && (unsigned int) idx < nVertices)
			out = idx;
	}
	return true;
}

int MeshIO::import_obj (Mesh& mesh, const char *filename)
{
	if (filename == nullptr)
		return 0;

	char buffer[BUFFER_SIZE];
	char prefix[BUFFER_SIZE];

	FILE *file = fopen (filename, "r");
	if (file == nullptr)
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
		// sscanf("%s") leaves prefix UNTOUCHED on input-failure (blank lines,
		// trailing whitespace), and its return value is 0 (not -1) in that
		// case — so the `!= -1` guard alone lets us fall through to strcmp
		// with whatever stack/residual garbage was left in prefix. Initialize
		// it on every iteration so strcmp sees a defined empty string.
		prefix[0] = '\0';
		if (sscanf(buffer, "%s", prefix) != -1)
		{
			if (strcmp(prefix, "mtllib") == 0)
			{
				const char* q = buffer + 6; // past "mtllib"
				while (*q && isspace((unsigned char)*q)) q++;
				
				std::string mtlLine(q);
				mtlLine.erase(mtlLine.find_last_not_of(" \t\r\n") + 1);
				snprintf(mtlfile, BUFFER_SIZE, "%s", mtlLine.c_str());
			}
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
	mesh.Init (nPoints, nFaces);

	if (strlen (mtlfile) != 0)
	{
		auto dir = std::filesystem::path(filename).parent_path();
		MeshIO::import_mtl(mesh, mtlfile, dir.string().c_str());
	}

	if (nTexCoords)
	{
		mesh.m_nTextureCoordinates = nTexCoords;
		mesh.m_pTextureCoordinates.assign(2*nTexCoords, 0.0f);
	}
	int ipoint = 0, itexcoord = 0, iface = 0;
	int usemtl = -1;
	while (fgets (buffer, BUFFER_SIZE, file))
	{
		// Same defense as in the counting pass above: blank/whitespace lines
		// leave prefix unchanged and sscanf returns 0 (not -1), so without
		// the reset the subsequent strcmp branches could trigger on stale
		// data and start writing past the allocated vertex/face arrays.
		prefix[0] = '\0';
		if (sscanf(buffer, "%s", prefix) == -1)
			continue;
		if (strcmp (prefix, "usemtl") == 0)
		{
			// Safe: parse the material name without overflowing
			std::string_view mtl_name(buffer + 6); // past "usemtl"
			
			// Trim left
			while (!mtl_name.empty() && isspace(static_cast<unsigned char>(mtl_name.front())))
				mtl_name.remove_prefix(1);
			// Trim right
			while (!mtl_name.empty() && isspace(static_cast<unsigned char>(mtl_name.back())))
				mtl_name.remove_suffix(1);
				
			usemtl = mesh.GetMaterialId (std::string(mtl_name));
		}
		else if (strcmp (prefix, "v") == 0)
		{
			sscanf (buffer, "%s %f %f %f", prefix,
				&mesh.m_pVertices[3*ipoint],
				&mesh.m_pVertices[3*ipoint+1],
				&mesh.m_pVertices[3*ipoint+2]);
			ipoint++;
		}
		else if (strcmp (prefix, "vt") == 0)
		{
			float u = 0.f, v = 0.f;
			sscanf (buffer, "%s %f %f", prefix, &u, &v);
			mesh.m_pTextureCoordinates[2*itexcoord]   = u;
			// OBJ stores V with the origin at the bottom; OpenGL samples the
			// first uploaded row (top of the image) at V=0. Flip V.
			mesh.m_pTextureCoordinates[2*itexcoord+1] = 1.0f - v;
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
			Face *pFace = mesh.m_pFaces[iface];
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

				// OBJ negative indices count back from the most recently
				// declared element, i.e. relative to the number of vertices
				// seen SO FAR (ipoint), not the file total. For the common
				// "all v then all f" layout the two coincide, but using the
				// running counter is also correct for interleaved files.
				if (i0 < 0)
					i0 = ipoint + i0;
				else
					i0--;

				if (i0 < 0 || (unsigned int) i0 >= mesh.m_nVertices) {
					printf ("invalid vertex index %d (vn=%d)\n", i0, mesh.m_nVertices);
					continue;
				}

				pFace->SetVertex (fvn, i0);

				if (h1)
				{
					if (i1 < 0)
						i1 = itexcoord + i1;   // relative to UVs seen so far
					else
						i1--;
					// Clamp to a valid slot: the index is later used to read
					// mesh.m_pTextureCoordinates[2*i1] at render time, so an
					// out-of-range value (bad file / under-declared UVs) would
					// read out of bounds and crash.
					if (i1 < 0 || (unsigned int) i1 >= mesh.m_nTextureCoordinates)
						i1 = 0;
					pFace->m_bUseTextureCoordinates = true;
					// Allocate the per-face texcoord arrays ONCE: these
					// Activate* calls delete[] and reallocate, so calling them
					// per corner (as before) wiped the indices already written
					// for earlier corners, leaving uninitialised garbage that
					// the textured render path then read out of bounds.
					if (!pFace->m_pTextureCoordinatesIndices)
						pFace->ActivateTextureCoordinatesIndices();
					if (!pFace->m_pTextureCoordinates)
						pFace->ActivateTextureCoordinates();
					pFace->SetTexCoord (fvn, i1);
				}
				fvn++;
			}

			// material
			if (usemtl != -1)
				pFace->m_iMaterialId = usemtl;

			mesh.m_pFaces[iface] = pFace;
			iface++;
		}
		else if (strcmp (prefix, "l") == 0)
		{
			// Polyline: N vertex refs -> N-1 segments. Skip past the 'l'
			// token (tolerating leading whitespace) and consume refs.
			char *s = buffer;
			while (*s && isspace ((unsigned char)*s)) s++;
			while (*s && !isspace ((unsigned char)*s)) s++;

			int prev = -1, idx;
			while (nextObjElementIndex (s, ipoint, mesh.m_nVertices, idx))
			{
				if (idx < 0)
					continue;   // malformed / out-of-range ref, skip
				if (prev >= 0)
				{
					mesh.m_pLines.push_back ((unsigned int) prev);
					mesh.m_pLines.push_back ((unsigned int) idx);
				}
				prev = idx;
			}
		}
		else if (strcmp (prefix, "p") == 0)
		{
			// Point element: one or more vertex refs, one point each.
			char *s = buffer;
			while (*s && isspace ((unsigned char)*s)) s++;
			while (*s && !isspace ((unsigned char)*s)) s++;

			int idx;
			while (nextObjElementIndex (s, ipoint, mesh.m_nVertices, idx))
				if (idx >= 0)
					mesh.m_pPoints.push_back ((unsigned int) idx);
		}
	}
	fclose (file);

	// OBJ vertex normals (vn) are not imported, so compute them from the
	// geometry. Besides giving proper smooth shading, this guarantees
	// mesh.m_pFaceNormals / mesh.m_pVertexNormals are populated — the immediate-mode
	// render path reads mesh.m_pFaceNormals[] unconditionally and would otherwise
	// read out of bounds on a normal-less mesh.
	mesh.ComputeNormals ();

	return 0;
}

//
//
//
int MeshIO::export_obj (Mesh& mesh, const char *filename, bool emitObjectGroups)
{
	FILE *fp;
	unsigned int i;

	fp = fopen(filename,"w");
	if (fp == nullptr)
		return -1;

	// some comments
	fprintf (fp, "#\n");
	fprintf (fp, "# number of vertices : %d\n", mesh.m_nVertices);
	fprintf (fp, "# number of faces    : %d\n", mesh.m_nFaces);
	fprintf (fp, "#\n");
	fprintf (fp, "\n");

	// materials
	char *filematname = nullptr;
	if (!mesh.m_pMaterials.empty())
	{
		filematname = strdup (filename);
		sprintf (filematname+strlen (filematname)-3, "%s", "mtl");

		// `mtllib` doit designer le .mtl par son NOM SEUL : il est ecrit a cote du
		// .obj, et un chemin absolu ne resoudrait plus des que le couple est
		// deplace ou envoye ailleurs.
		//
		// Cherche les DEUX separateurs. Ne tester que '/' laissait passer tout
		// chemin Windows (`C:\...\model.obj`) : `strrchr` renvoyait nullptr et le
		// mtllib recevait le chemin ABSOLU de la machine qui avait exporte.
		char *slash     = strrchr (filematname, '/');
		char *backslash = strrchr (filematname, '\\');
		char *s = (slash > backslash) ? slash : backslash;
		fprintf (fp, "mtllib %s\n\n", (s != nullptr) ? &s[1] : filematname);
	}

	//
	// vertices
	//
	for (i = 0; i < mesh.m_nVertices; i++)
		fprintf (fp, "v %f %f %f\n",
			 mesh.m_pVertices[3*i], mesh.m_pVertices[3*i+1], mesh.m_pVertices[3*i+2]);
	if (0 && !mesh.m_pVertexNormals.empty())
	{
		for (i=0; i<mesh.m_nVertices; i++)
		{
			fprintf (fp, "vn %f %f %f\n",
				 mesh.m_pVertexNormals[3*i],
				 mesh.m_pVertexNormals[3*i+1],
				 mesh.m_pVertexNormals[3*i+2]);
		}
	}
	
	if (!mesh.m_pTextureCoordinates.empty())
	{
		for (i=0; i<mesh.m_nTextureCoordinates; i++)
			fprintf (fp, "vt %f %f\n", mesh.m_pTextureCoordinates[2*i], mesh.m_pTextureCoordinates[2*i+1]);
	}

	//
	// faces
	//
	unsigned int i_current_material = MATERIAL_NONE;
	for (i = 0; i <mesh.m_nFaces; i++)
	{
		if (!mesh.m_pFaces[i])
			continue;
		
		if (mesh.m_pFaces[i]->GetMaterialId () != MATERIAL_NONE &&
		    mesh.m_pFaces[i]->GetMaterialId () != i_current_material)
		{
			const unsigned int mid = (unsigned int)mesh.m_pFaces[i]->GetMaterialId ();
			const std::string mname =
				objMaterialName (mesh.m_pMaterials[mid].get(), mid);
			// `o` AVANT `usemtl` : c'est l'ordre attendu, la declaration d'objet
			// ouvrant le bloc auquel le materiau s'applique.
			if (emitObjectGroups)
				fprintf (fp, "o %s\n", mname.c_str());
			fprintf (fp, "usemtl %s\n", mname.c_str());
			i_current_material = mesh.m_pFaces[i]->GetMaterialId ();
		}

		if (0 && mesh.m_pFaces[i]->m_pTextureCoordinatesIndices)//mesh.m_pFaces[i]->m_bUseTextureCoordinates)
		{
			for (unsigned int j=0; j<mesh.m_pFaces[i]->m_nVertices; j++)
			{
				//fprintf (fp, "vt %f %f\n",
				//	 mesh.m_pTextureCoordinates[2*mesh.m_pFaces[i]->m_pTextureCoordinates[j]],
				//	 mesh.m_pTextureCoordinates[2*mesh.m_pFaces[i]->m_pTextureCoordinates[j]+1]);
				fprintf (fp, "vt %f %f\n",
					 mesh.m_pFaces[i]->m_pTextureCoordinates[2*j],
					 mesh.m_pFaces[i]->m_pTextureCoordinates[2*j+1]);
			}
		}

		fprintf (fp, "f ");
		for (unsigned int j=0; j<mesh.m_pFaces[i]->m_nVertices; j++)
		{
			// vertex
			fprintf (fp, "%d", 1+mesh.m_pFaces[i]->m_pVertices[j]);

			// texture coordinates
			if (mesh.m_pFaces[i]->m_bUseTextureCoordinates && mesh.m_pFaces[i]->m_pTextureCoordinatesIndices)
				fprintf (fp, "/%d", 1+mesh.m_pFaces[i]->m_pTextureCoordinatesIndices[j]);//mesh.m_pFaces[i]->m_nVertices);

			// normal
			if (0 && !mesh.m_pVertexNormals.empty())
			{
				if (!mesh.m_pFaces[i]->m_bUseTextureCoordinates)
					fprintf (fp, "/");
				fprintf (fp, "/%d\n", mesh.m_pFaces[i]->m_pVertices[j]);
			}
			
			fprintf (fp, " ");
		}
		fprintf (fp, "\n");
	}

	//
	// line segments ('l') and points ('p') — indices are 1-based in OBJ
	//
	for (i = 0; i < mesh.GetNLines(); i++)
		fprintf (fp, "l %u %u\n", 1 + mesh.m_pLines[2*i], 1 + mesh.m_pLines[2*i+1]);
	for (i = 0; i < mesh.GetNPoints(); i++)
		fprintf (fp, "p %u\n", 1 + mesh.m_pPoints[i]);

	fclose (fp);

	//
	// materials
	//
	if (!mesh.m_pMaterials.empty())
	{
		fp = fopen(filematname,"w");
		if (fp == nullptr)
			return -1;

		fprintf (fp, "\n");
		fprintf (fp, "# Wavefront material file\n");
		fprintf (fp, "\n");

		for (size_t i = 0; i < mesh.m_pMaterials.size(); ++i)
		{
			Material *pMaterial = mesh.m_pMaterials[i].get();
			if (!pMaterial)
				continue;
			switch (pMaterial->GetType ())
			{
			case MATERIAL_COLOR:
			{
				MaterialColor *pMaterialColor = dynamic_cast<MaterialColor*> (pMaterial);
				fprintf (fp, "newmtl %s\n", objMaterialName (pMaterial, i).c_str());
				fprintf (fp, "Ka 0.200000 0.200000 0.200000\n");
				fprintf (fp, "Kd %f %f %f\n",
					 pMaterialColor->GetFloatRed(), pMaterialColor->GetFloatGreen(), pMaterialColor->GetFloatBlue());
				// `Ks 1 1 1` avec `Ns 0` etait incoherent : un exposant nul rend le
				// terme speculaire CONSTANT sur toute la surface (pow(N.H,0)==1), donc
				// un voile blanc uniforme. Un MaterialColor ne porte qu'une couleur :
				// pas de speculaire, et illum 1 (diffus seul) pour le dire.
				fprintf (fp, "Ks 0.000000 0.000000 0.000000\n");
				fprintf (fp, "Ns 0.000000\n");
				// OPACITE. L'ancien `Tr 1.000000` disait l'inverse de l'intention :
				// dans la spec MTL, `Tr` est la TRANSPARENCE (1 = totalement
				// transparent), et c'est `d` qui vaut 1 pour opaque. Tout lecteur
				// conforme rendait donc le modele invisible -- sauf en incidence
				// rasante, ou l'accumulation des epaisseurs le fait reapparaitre.
				// cgmesh ne s'en apercevait pas : son propre import_mtl lit `d` et
				// `Tr` puis les jette.
				fprintf (fp, "d %f\n", pMaterialColor->GetFloatAlpha());
				fprintf (fp, "illum 1\n");
				fprintf (fp, "\n");
			}
			break;
			case MATERIAL_TEXTURE:
			{
				MaterialTexture *pMaterialTexture = dynamic_cast<MaterialTexture*> (pMaterial);
				// Meme fonction que le usemtl : un nom vide ou a espaces cassait
				// aussi cette branche, plus discretement.
				fprintf (fp, "newmtl %s\n", objMaterialName (pMaterial, i).c_str());
				fprintf (fp, "Ka 1.000000 1.000000 1.000000\n");
				// Kd BLANC, et surtout pas noir : sous illum 2, `map_Kd` MULTIPLIE la
				// couleur diffuse. L'ancien `Kd 0 0 0` renvoyait donc une texture
				// entierement noire, quelle que soit l'image.
				fprintf (fp, "Kd 1.000000 1.000000 1.000000\n");
				fprintf (fp, "Ks 0.000000 0.000000 0.000000\n");
				fprintf (fp, "Ns 0.000000\n");
				// Opacite, et non transparence -- cf. la branche couleur.
				fprintf (fp, "d 1.000000\n");
				fprintf (fp, "illum 2\n");
				fprintf (fp, "map_Kd %s\n", pMaterialTexture->GetFilename().c_str());
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

// ===========================================================================
//  OBJ + MTL dans une seule archive ZIP
// ===========================================================================
//
// export_obj ecrit par NOM DE FICHIER (fprintf sur un FILE*) sur ~180 lignes.
// Plutot que de le reecrire en generateur de chaine -- gros diff, risque de
// divergence entre deux ecrivains -- on le laisse produire ses deux fichiers dans
// un repertoire temporaire, puis on les relit et on les zippe. Un seul ecrivain,
// donc un seul comportement a maintenir et a tester.
//
// Sous Emscripten, temp_directory_path() est /tmp en MEMFS : le detour ne touche
// aucun disque.

namespace {

// Lit un fichier entier en octets. Chaine vide si absent.
std::string slurpFile (const std::string& path)
{
	FILE *fp = fopen (path.c_str(), "rb");
	if (!fp)
		return std::string();
	std::string out;
	char buf[8192];
	size_t n;
	while ((n = fread (buf, 1, sizeof(buf), fp)) > 0)
		out.append (buf, n);
	fclose (fp);
	return out;
}

// Radical d'un chemin : ni repertoire, ni extension. "a/b/foo.zip" -> "foo".
std::string stemOf (const std::string& path)
{
	size_t start = path.find_last_of ("/\\");
	start = (start == std::string::npos) ? 0 : start + 1;
	size_t dot = path.find_last_of ('.');
	if (dot == std::string::npos || dot < start)
		dot = path.size();
	return path.substr (start, dot - start);
}

} // namespace

std::string MeshIO::export_obj_zip_bytes (Mesh& mesh, const std::string& basename,
					  bool emitObjectGroups)
{
	// Un radical vide donnerait des entrees nommees ".obj" : on garde un defaut.
	const std::string stem = basename.empty() ? std::string("model") : basename;

	const std::string dir = std::filesystem::temp_directory_path().string();
	const std::string sep = (!dir.empty() && (dir.back() == '/' || dir.back() == '\\'))
				? std::string() : std::string("/");
	const std::string objPath = dir + sep + stem + ".obj";
	const std::string mtlPath = dir + sep + stem + ".mtl";

	if (export_obj (mesh, objPath.c_str(), emitObjectGroups) != 0)
		return std::string();

	std::vector<ZipManager::Entry> entries;
	const std::string obj = slurpFile (objPath);
	if (!obj.empty())
		entries.push_back ({ stem + ".obj", obj });
	// Absent quand le maillage ne porte aucun materiau : export_obj n'ecrit alors
	// pas de .mtl, et l'OBJ ne contient pas de ligne mtllib.
	const std::string mtl = slurpFile (mtlPath);
	if (!mtl.empty())
		entries.push_back ({ stem + ".mtl", mtl });

	std::remove (objPath.c_str());
	std::remove (mtlPath.c_str());

	if (entries.empty())
		return std::string();
	return ZipManager::BuildStored (entries);
}

int MeshIO::export_obj_zip (Mesh& mesh, const char *filename, bool emitObjectGroups)
{
	if (!filename)
		return -1;
	// Les entrees internes prennent le radical de l'archive : foo.zip contient
	// foo.obj et foo.mtl, donc `mtllib foo.mtl` resout apres extraction.
	const std::string bytes = export_obj_zip_bytes (mesh, stemOf (filename), emitObjectGroups);
	if (bytes.empty())
		return -1;

	FILE *fp = fopen (filename, "wb");
	if (!fp)
		return -1;
	const size_t written = fwrite (bytes.data(), 1, bytes.size(), fp);
	fclose (fp);
	return (written == bytes.size()) ? 0 : -1;
}
