#include "normals.h"

#include "cgmesh.h"

/**
*
* Compute the normales at the vertices.
*
* The following methods are available :
* - GOURAUD
* - THURMER
* - MAX
* - DESBRUN
*
*/
int Normals::EvalOnVertices (Mesh_half_edge *mesh, MethodId MethodId)
{
	int i;
	int nv = mesh->m_pMesh->m_nVertices;

	// initialization
	memset (mesh->m_pMesh->m_pVertexNormals.data(), 0., 3*nv*sizeof(float));

	switch (MethodId)
	{
	case GOURAUD:
		for (i=0; i<nv; i++)
		{
			if (!mesh->is_manifold (i))
				continue;

			vec3 n;
			vec3_init (n, 0.0, 0.0, 0.0);

			Citerator_half_edges_vertex he_ite (mesh->GetCheMesh(), i);
			int he;
			int a,b,c;
			float *v = mesh->m_pMesh->m_pVertices.data();
			for (he = he_ite.first (); he >= 0 && !he_ite.isLast (); he = he_ite.next ())
			{
				Che_edge &e = mesh->GetCheMesh()->edge(he);
				Face *f = mesh->m_pMesh->m_pFaces[e.m_face];
				a = f->GetVertex (0);
				b = f->GetVertex(1);
				c = f->GetVertex(2);
				
				vec3 v1, v2, v3;
				vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
				vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
				vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);

				// evaluate the normal for the current face
				vec3 ntmp;
				vec3_triangle_normal (ntmp, v1, v2, v3);
				//vec3_normalize (ntmp);

				// update the normal for the current vertex
				vec3_addition (n, n, ntmp);
			}
			
			vec3_normalize (n);
			mesh->m_pMesh->m_pVertexNormals[3*i]   = n[0];
			mesh->m_pMesh->m_pVertexNormals[3*i+1] = n[1];
			mesh->m_pMesh->m_pVertexNormals[3*i+2] = n[2];
		}
		break;
	case THURMER:
		for (i=0; i<nv; i++)
		{
			vec3 n;
			vec3_init (n, 0.0, 0.0, 0.0);

			Citerator_half_edges_vertex he_ite (mesh->GetCheMesh(), i);
			int he;
			int a,b,c;
			float *v = mesh->m_pMesh->m_pVertices.data();
			for (he = he_ite.first (); he >= 0 && !he_ite.isLast (); he = he_ite.next ())
			{
				Che_edge &e = mesh->GetCheMesh()->edge(he);
				Face *f = mesh->m_pMesh->m_pFaces[e.m_face];
				if (f->GetVertex (0) == i)
				{
					a = f->GetVertex (0);
					b = f->GetVertex (1);
					c = f->GetVertex (2);
				}
				else if (f->GetVertex (1) == i)
				{
					a = f->GetVertex (1);
					b = f->GetVertex (2);
					c = f->GetVertex (0);
				}
				else if (f->GetVertex (2) == i)
				{
					a = f->GetVertex (2);
					b = f->GetVertex (0);
					c = f->GetVertex (1);
				}
				else
					printf ("!!! state not supposed to be reached !!!\n");

				vec3 v1, v2, v3;
				vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
				vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
				vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);

				// evaluate the normal for the current face
				vec3 ntmp;
				vec3_triangle_normal (ntmp, v1, v2, v3);
				vec3_normalize (ntmp);

				// the weight associated to this direction is
				// the angle between (v1,v2) and (v1,v3)
				vec3 v1v2, v1v3;
				vec3_subtraction (v1v2, v2, v1);
				vec3_subtraction (v1v3, v3, v1);
				vec3_normalize (v1v2);
				vec3_normalize (v1v3);
				float weight = acos (vec3_dot_product (v1v2, v1v3));
				vec3_scale (ntmp, ntmp, weight);
				
				// update the normal for the current vertex
				vec3_addition (n, n, ntmp);


			}
			vec3_normalize (n);
			mesh->m_pMesh->m_pVertexNormals[3*i]   = n[0];
			mesh->m_pMesh->m_pVertexNormals[3*i+1] = n[1];
			mesh->m_pMesh->m_pVertexNormals[3*i+2] = n[2];
		}
		break;
	case MAX:
		// Nelson Max (1999): weight each incident face normal by
		// 1/(|e1|^2 * |e2|^2), where e1,e2 are the two triangle edges at the
		// vertex. Emphasises small/near triangles; good on irregular fans.
		for (i=0; i<nv; i++)
		{
			vec3 n;
			vec3_init (n, 0.0, 0.0, 0.0);

			Citerator_half_edges_vertex he_ite (mesh->GetCheMesh(), i);
			int he;
			int a,b,c;
			float *v = mesh->m_pMesh->m_pVertices.data();
			for (he = he_ite.first (); he >= 0 && !he_ite.isLast (); he = he_ite.next ())
			{
				Che_edge &e = mesh->GetCheMesh()->edge(he);
				Face *f = mesh->m_pMesh->m_pFaces[e.m_face];
				if (f->GetVertex (0) == i)      { a=f->GetVertex(0); b=f->GetVertex(1); c=f->GetVertex(2); }
				else if (f->GetVertex (1) == i) { a=f->GetVertex(1); b=f->GetVertex(2); c=f->GetVertex(0); }
				else if (f->GetVertex (2) == i) { a=f->GetVertex(2); b=f->GetVertex(0); c=f->GetVertex(1); }
				else continue;

				vec3 v1, v2, v3, e1, e2, cr;
				vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
				vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
				vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);
				vec3_subtraction (e1, v2, v1);
				vec3_subtraction (e2, v3, v1);
				vec3_cross_product (cr, e1, e2);           // face normal, |cr| = 2*area
				float denom = vec3_dot_product (e1, e1) * vec3_dot_product (e2, e2);
				if (denom > 1e-20f)
				{
					vec3_scale (cr, cr, 1.0f / denom);
					vec3_addition (n, n, cr);
				}
			}
			vec3_normalize (n);
			mesh->m_pMesh->m_pVertexNormals[3*i]   = n[0];
			mesh->m_pMesh->m_pVertexNormals[3*i+1] = n[1];
			mesh->m_pMesh->m_pVertexNormals[3*i+2] = n[2];
		}
		break;
	case DESBRUN:
		// Not implemented (no canonical per-vertex weighting); not exposed in
		// the UI. Fall back to Thurmer so it can never produce null normals.
		return EvalOnVertices (mesh, THURMER);
	default:
		printf ("type unknown to compute the normales\n");
	}

	return 0;
}

void Normals::invert_vertices_normales (Mesh_half_edge *mesh)
{
	for (unsigned int i=0; i<3*mesh->m_pMesh->m_nVertices; i++)
		mesh->m_pMesh->m_pVertexNormals[i] *= -1.;
}

/**
*
* Compute the normales (orientations) of the faces.
*
*/
int EvalOnFaces (Mesh_half_edge *mesh)
{
	if (!mesh || mesh->m_pMesh->m_pVertices.empty())
		return -1;

	int i, a, b, c;
	float *v = mesh->m_pMesh->m_pVertices.data();
	for (i=0; i<(int)mesh->m_pMesh->m_nFaces; i++)
	{
		Face *f = mesh->m_pMesh->m_pFaces[i];
		a = f->GetVertex(0);
		b = f->GetVertex(1);
		c = f->GetVertex(2);
		
		vec3 v1, v2, v3;
		vec3_init (v1, v[3*a], v[3*a+1], v[3*a+2]);
		vec3_init (v2, v[3*b], v[3*b+1], v[3*b+2]);
		vec3_init (v3, v[3*c], v[3*c+1], v[3*c+2]);

		vec3 n;
		vec3_triangle_normal (n, v1, v2, v3);
		vec3_normalize (n);

		mesh->m_pMesh->m_pFaceNormals[3*i]   = n[0];
		mesh->m_pMesh->m_pFaceNormals[3*i+1] = n[1];
		mesh->m_pMesh->m_pFaceNormals[3*i+2] = n[2];
	}

	return 0;
}
