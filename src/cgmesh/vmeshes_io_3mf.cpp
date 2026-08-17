// ============================================================================
//  3MF import (3MF Consortium / lib3mf)
// ============================================================================
//
// Point d'entree : VMeshesIO::import_3mf, dispatche par VMeshes::load sur
// l'extension ".3mf". Produit UN cgmesh::Mesh par build item du modele.
//
// ---------- Ce qui est couvert ---------------------------------------------
//
//   * objets maillés (<mesh>)          -> un Mesh chacun
//   * <build> / <item>                 -> placement dans la scene
//   * <components>                     -> resolus recursivement
//   * transformations                  -> composees et appliquees aux sommets
//
// ---------- Ce qui ne l'est PAS (volontaire, cf. docs/import_3mf_feasibility.md)
//
//   * matieres, couleurs, textures : le modele de proprietes 3MF est PAR
//     TRIANGLE alors que cgmesh porte des couleurs PAR SOMMET -- la conversion
//     demande une conception a part.
//   * extensions 3MF (beam lattice, slice, production).
//
// ---------- Notes d'implementation -----------------------------------------
//
// Layout : sLib3MFPosition est un float[3] et Mesh::m_pVertices un
// std::vector<float> a 3 flottants par sommet -- les deux layouts coincident,
// d'ou la copie en bloc plutot qu'un appel par sommet.
//
// Transformations : lib3mf donne un float[4][3] (3 colonnes de 4 lignes) ou les
// trois premieres lignes forment la partie lineaire et la quatrieme la
// translation. Elles sont appliquees ICI, sommet par sommet, plutot que via
// Mesh::transform : celui-ci ne prend qu'une matrice 3x3 et ne porte donc pas de
// translation. Cela evite d'elargir l'API de Mesh (mesh.h est inclus par 50
// fichiers) pour un besoin local a cet importeur.
//
// <cstdio> est utilise par le chemin actif ET par le stub CG_HAS_LIB3MF=Off --
// le garder hors du #ifdef.
// ============================================================================

#include <cstdio>

// ORDRE D'INCLUSION CRITIQUE -- ne pas reordonner.
//
// lib3mf_implicit.hpp tire <windows.h>, dont <rpcndr.h> qui declare son propre
// `byte` au namespace global. vmeshes.h, lui, fait `using namespace std;` :
// std::byte devient alors visible comme `byte`. Si vmeshes.h passe EN PREMIER,
// rpcndr.h echoue a se compiler lui-meme -- « error C2872: 'byte' : symbole
// ambigu » -- a l'interieur du SDK, sans rapport apparent avec ce fichier.
//
// D'ou lib3mf AVANT les en-tetes cgmesh : rpcndr.h est alors analyse tant que
// `std` n'a pas ete deverse dans le namespace global.
//
// Ce piege a ete masque un temps par _HAS_STD_BYTE=0, que le bloc PoissonRecon
// du CMakeLists ajoute a cgmesh : la faute n'apparaissait donc QUE dans les
// configurations sans ENABLE_POISSON (p. ex. celle de sulina). Ne pas compter
// dessus.
#ifdef CG_HAS_LIB3MF
#include "lib3mf_implicit.hpp"
#endif

#include "vmeshes.h"
#include "vmeshes_io.h"

#ifdef CG_HAS_LIB3MF

#include <string>
#include <vector>

#include "mesh.h"

namespace {

// Transformation affine 3MF : partie lineaire 3x3 + translation.
// L'identite est le neutre de la composition ci-dessous.
struct Xform
{
	float m[3][3] = { {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f} };
	float t[3]    = { 0.f, 0.f, 0.f };
};

// sLib3MFTransform::m_Fields est [4][3] : m_Fields[i][j] = ligne i, colonne j.
// Les lignes 0..2 portent la partie lineaire, la ligne 3 la translation.
Xform fromLib3mf (const Lib3MF::sTransform& s)
{
	Xform x;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			x.m[i][j] = s.m_Fields[i][j];
	for (int j = 0; j < 3; j++)
		x.t[j] = s.m_Fields[3][j];
	return x;
}

// Composition : applique `inner` PUIS `outer` (outer o inner).
// Un composant imbrique subit d'abord sa propre transformation, puis celle de
// son parent, et ainsi de suite jusqu'au build item.
Xform compose (const Xform& outer, const Xform& inner)
{
	Xform r;
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			float s = 0.f;
			for (int k = 0; k < 3; k++)
				s += inner.m[i][k] * outer.m[k][j];
			r.m[i][j] = s;
		}
	}
	for (int j = 0; j < 3; j++)
	{
		float s = outer.t[j];
		for (int k = 0; k < 3; k++)
			s += inner.t[k] * outer.m[k][j];
		r.t[j] = s;
	}
	return r;
}

// Applique la transformation a un point exprime en ligne : p' = p * M + t.
// C'est la convention 3MF (cf. specification core, section « 3D Matrix »).
void applyTo (const Xform& x, float& px, float& py, float& pz)
{
	const float a = px, b = py, c = pz;
	px = a * x.m[0][0] + b * x.m[1][0] + c * x.m[2][0] + x.t[0];
	py = a * x.m[0][1] + b * x.m[1][1] + c * x.m[2][1] + x.t[1];
	pz = a * x.m[0][2] + b * x.m[1][2] + c * x.m[2][2] + x.t[2];
}

// Convertit UN objet maille lib3mf en cgmesh::Mesh, transformation appliquee.
// Renvoie nullptr si l'objet est vide (0 sommet ou 0 triangle) : un Mesh sans
// geometrie n'apporte rien a la scene et perturbe les calculs de bbox.
Mesh* meshFromObject (Lib3MF::PMeshObject obj, const Xform& xform, const std::string& name)
{
	std::vector<Lib3MF::sPosition> verts;
	std::vector<Lib3MF::sTriangle> tris;
	obj->GetVertices (verts);
	obj->GetTriangleIndices (tris);

	if (verts.empty() || tris.empty())
		return nullptr;

	Mesh* pMesh = new Mesh ((unsigned int)verts.size(), (unsigned int)tris.size());
	pMesh->m_name = name;

	// Sommets : layout identique (3 floats), donc copie en bloc puis
	// transformation en place.
	for (size_t i = 0; i < verts.size(); i++)
	{
		float x = verts[i].m_Coordinates[0];
		float y = verts[i].m_Coordinates[1];
		float z = verts[i].m_Coordinates[2];
		applyTo (xform, x, y, z);
		pMesh->SetVertex ((unsigned int)i, x, y, z);
	}

	// Triangles. lib3mf ne produit que des triangles (le format n'a pas de
	// n-gone), d'ou le SetNVertices(3) systematique.
	const unsigned int nv = (unsigned int)verts.size();
	for (size_t f = 0; f < tris.size(); f++)
	{
		auto face = pMesh->FaceAt (f);
		face->SetNVertices (3);
		for (int k = 0; k < 3; k++)
		{
			const unsigned int vi = tris[f].m_Indices[k];
			// Garde-fou : un fichier malforme peut indexer hors du tableau de
			// sommets. lib3mf valide deja le modele, mais l'ecriture directe
			// dans m_pVertices en aval ne pardonnerait pas.
			face->SetVertex ((unsigned int)k, vi < nv ? vi : 0);
		}
	}

	return pMesh;
}

// Resout un objet (maille OU assemblage de composants) et empile les Mesh
// produits. `depth` borne la recursion : la spec 3MF interdit les cycles, mais
// un fichier hostile pourrait en contenir et lib3mf ne garantit pas de les
// rejeter tous.
void collect (Lib3MF::PModel model, Lib3MF::PObject obj, const Xform& xform,
              std::vector<Mesh*>& out, int depth)
{
	if (!obj || depth > 32)
		return;

	if (obj->IsMeshObject())
	{
		Lib3MF::PMeshObject mo = model->GetMeshObjectByID (obj->GetResourceID());
		if (Mesh* m = meshFromObject (mo, xform, obj->GetName()))
			out.push_back (m);
		return;
	}

	if (obj->IsComponentsObject())
	{
		Lib3MF::PComponentsObject co = model->GetComponentsObjectByID (obj->GetResourceID());
		const Lib3MF_uint32 n = co->GetComponentCount();
		for (Lib3MF_uint32 i = 0; i < n; i++)
		{
			Lib3MF::PComponent comp = co->GetComponent (i);
			Xform child = xform;
			if (comp->HasTransform())
				child = compose (xform, fromLib3mf (comp->GetTransform()));
			collect (model, comp->GetObjectResource(), child, out, depth + 1);
		}
	}
}

}  // namespace

bool VMeshesIO::import_3mf (VMeshes& vm, const char* filename)
{
	if (!filename)
		return false;

	std::vector<Mesh*> meshes;

	try
	{
		Lib3MF::PWrapper wrapper = Lib3MF::CWrapper::loadLibrary();
		Lib3MF::PModel   model   = wrapper->CreateModel();
		Lib3MF::PReader  reader  = model->QueryReader ("3mf");
		reader->ReadFromFile (std::string (filename));

		// Chemin nominal : parcourir le <build>, qui porte le placement voulu
		// par l'auteur du fichier.
		Lib3MF::PBuildItemIterator items = model->GetBuildItems();
		while (items->MoveNext())
		{
			Lib3MF::PBuildItem item = items->GetCurrent();
			Xform x;
			if (item->HasObjectTransform())
				x = fromLib3mf (item->GetObjectTransform());
			collect (model, item->GetObjectResource(), x, meshes, 0);
		}

		// Repli : certains fichiers (exports partiels, tests) declarent des
		// objets sans <build>. Plutot que de ne rien rendre, on importe les
		// objets maillés a leur position d'origine.
		if (meshes.empty())
		{
			Lib3MF::PMeshObjectIterator it = model->GetMeshObjects();
			while (it->MoveNext())
			{
				Lib3MF::PMeshObject mo = it->GetCurrentMeshObject();
				if (Mesh* m = meshFromObject (mo, Xform(), mo->GetName()))
					meshes.push_back (m);
			}
		}
	}
	catch (std::exception& e)
	{
		// lib3mf signale TOUTES ses erreurs par exception (fichier absent,
		// archive corrompue, XML invalide, bibliotheque partagee introuvable).
		// Ne pas la laisser traverser : VMeshes::load renvoie un booleen et ses
		// appelants (sinaia, sulina) n'ont aucun try/catch.
		std::fprintf (stderr, "VMeshesIO::import_3mf: %s\n", e.what());
		for (Mesh* m : meshes)
			delete m;
		return false;
	}

	if (meshes.empty())
	{
		std::fprintf (stderr, "VMeshesIO::import_3mf: no mesh object in %s\n", filename);
		return false;
	}

	for (Mesh* m : meshes)
		vm.AddMesh (m);

	return true;
}

#else  // !CG_HAS_LIB3MF

bool VMeshesIO::import_3mf (VMeshes& /*vm*/, const char* /*filename*/)
{
	std::fprintf (stderr,
		"VMeshesIO::import_3mf: lib3mf support not built in (CG_HAS_LIB3MF undefined)\n");
	return false;
}

#endif  // CG_HAS_LIB3MF
