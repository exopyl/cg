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
	int nv = mesh->m_nVertices;

	// initialization
	memset (mesh->m_pVertexNormals, 0., 3*nv*sizeof(float));
	
	switch (MethodId)
	{
	case GOURAUD:
		for (i=0; i<nv; i++)
		{
			if (!mesh->is_manifold (i))
				continue;

			vec3 n;
			vec3_init (n, 0.0, 0.0, 0.0);
			
			Citerator_half_edges_vertex he_ite (mesh, i);
			Che_edge *he;
			int a,b,c;
			float *v = mesh->m_pVertices;
			for (he = he_ite.first (); he && !he_ite.isLast (); he = he_ite.next ())
			{
				Face *f = mesh->m_pFaces[he->m_face];
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
			mesh->m_pVertexNormals[3*i]   = n[0];
			mesh->m_pVertexNormals[3*i+1] = n[1];
			mesh->m_pVertexNormals[3*i+2] = n[2];
		}
		break;
	case THURMER:
		for (i=0; i<nv; i++)
		{
			vec3 n;
			vec3_init (n, 0.0, 0.0, 0.0);

			Citerator_half_edges_vertex he_ite (mesh, i);
			Che_edge *he;
			int a,b,c;
			float *v = mesh->m_pVertices;
			for (he = he_ite.first (); he && !he_ite.isLast (); he = he_ite.next ())
			{
				Face *f = mesh->m_pFaces[he->m_face];
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
			mesh->m_pVertexNormals[3*i]   = n[0];
			mesh->m_pVertexNormals[3*i+1] = n[1];
			mesh->m_pVertexNormals[3*i+2] = n[2];
		}
		break;
	case MAX:
		printf ("Not yet implemented\n");
		break;
	case DESBRUN:
		printf ("Not yet implemented\n");
		break;
	default:
		printf ("type unknown to compute the normales\n");
	}

	return 0;
}

void Normals::invert_vertices_normales (Mesh_half_edge *mesh)
{
	for (int i=0; i<3*mesh->m_nVertices; i++)
		mesh->m_pVertexNormals[i] *= -1.;
}

/**
*
* Compute the normales (orientations) of the faces.
*
*/
int EvalOnFaces (Mesh_half_edge *mesh)
{
	if (!mesh || !mesh->m_pVertices)
		return -1;

	int i, a, b, c;
	float *v = mesh->m_pVertices;
	for (i=0; i<mesh->m_nFaces; i++)
	{
		Face *f = mesh->m_pFaces[i];
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

		mesh->m_pFaceNormals[3*i]   = n[0];
		mesh->m_pFaceNormals[3*i+1] = n[1];
		mesh->m_pFaceNormals[3*i+2] = n[2];
	}

	return 0;
}
