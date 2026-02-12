#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif // _POSIX_SOURCE
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif // _BSD_SOURCE

#include <math.h>
#include "../cgmath/cgmath.h"

#include "surface_parametric.h"


///////////////////////
//
// parametric surfaces
//
///////////////////////

// shared function to generate a parametric surface
int ParametricSurface::Generate(void)
{
	unsigned int nu = iNU;
	unsigned int nv = iNV;

	if (nu < 1 || nv < 1)
		return 0;
	
	unsigned int vn = nu*nv;
	unsigned int fn = (nu-1)*(nv-1)*2;
	if (bCloseU)
		fn += (nv-1)*2;
	if (bCloseV)
	{
		if (bIndependentCloseV)
		{
			fn += 2*(nu-2);
		}
		else
		{
			fn += (nu-1)*2;
		}
	}
	if (bCloseU && bCloseV && !bIndependentCloseU  && !bIndependentCloseV)
		fn += 2;
	
	Init (vn, fn);
	InitTensors ();

	// create vertices
	unsigned int index = 0;
	for (unsigned int v=0; v<nv; v++)
		for (unsigned int u=0; u<nu; u++)
		{
			float fU = u / (float) (nu-1+bCloseU*!bIndependentCloseU);
			float fV = v / (float) (nv-1+bCloseV*!bIndependentCloseV);

			diff_s diff;

			// position
			int retval = EvaluatePosition (fU, fV, &diff);
			m_pVertices[3*index]   = diff.position[0];
			m_pVertices[3*index+1] = diff.position[1];
			m_pVertices[3*index+2] = diff.position[2];

			// tensor
			m_pTensors[index] = EvaluateTensor (diff);

			index++;
		}

	// create faces
	unsigned int a, b, c;
	unsigned int fi = 0;
	Face *pFace = NULL;
	for (unsigned int v=0; v<nv-1; v++)
		for (unsigned int u=0; u<nu-1; u++)
		{
			// triangle 1
			pFace = new Face ();
			pFace->m_bUseTextureCoordinates = true;
			if (!pFace->m_pTextureCoordinates)
				pFace->m_pTextureCoordinates = new float[6];
			pFace->SetMaterialId (MATERIAL_NONE);

			pFace->m_pVertices[0] = v * nu + u; // ( u , v )
			pFace->m_pTextureCoordinates[0] = (float)(u)/(float)(nu-1+bCloseU);
			pFace->m_pTextureCoordinates[1] = 1.-(float)(v)/(float)(nv-1);
			
			pFace->m_pVertices[1] = (v+1) * nu + u; // ( u , v+1 )
			pFace->m_pTextureCoordinates[2] = (float)(u)/(float)(nu-1+bCloseU);
			pFace->m_pTextureCoordinates[3] = 1.-(float)(v+1)/(float)(nv-1);
			
			pFace->m_pVertices[2] = v * nu + u + 1; // ( u+1 , v )
			pFace->m_pTextureCoordinates[4] = (float)(u+1)/(float)(nu-1+bCloseU);
			pFace->m_pTextureCoordinates[5] = 1.-(float)(v)/(float)(nv-1);
			//f++;
			m_pFaces[fi++] = pFace;

			// triangle 2
			pFace = new Face ();
			pFace->m_bUseTextureCoordinates = true;
			if (!pFace->m_pTextureCoordinates)
				pFace->m_pTextureCoordinates = new float[6];
			pFace->SetMaterialId (MATERIAL_NONE);

			pFace->m_pVertices[0] = v * nu + u + 1; // ( u+1 , v )
			pFace->m_pTextureCoordinates[0] = (float)(u+1)/(float)(nu-1+bCloseU);
			pFace->m_pTextureCoordinates[1] = 1.-(float)(v)/(float)(nv-1);
			
			pFace->m_pVertices[1] = (v+1) * nu + u; // ( u , v+1 )
			pFace->m_pTextureCoordinates[2] = (float)(u)/(float)(nu-1+bCloseU);
			pFace->m_pTextureCoordinates[3] = 1.-(float)(v+1)/(float)(nv-1);
			
			pFace->m_pVertices[2] = (v+1) * nu + u + 1; // ( u+1 , v+1 ) 
			pFace->m_pTextureCoordinates[4] = (float)(u+1)/(float)(nu-1+bCloseU);
			pFace->m_pTextureCoordinates[5] = 1.-(float)(v+1)/(float)(nv-1);
			//f++;
			m_pFaces[fi++] = pFace;
		}

	if (bCloseU)
	{
		for (unsigned int v=0; v<nv-1; v++)
		{
			// triangle 1
			pFace = new Face ();
			pFace->m_bUseTextureCoordinates = true;
			if (!pFace->m_pTextureCoordinates)
				pFace->m_pTextureCoordinates = new float[6];
			pFace->SetMaterialId (MATERIAL_NONE);

			pFace->m_pVertices[0] = v * nu + nu-1; // ( nu-1 , v )
			pFace->m_pTextureCoordinates[0] = (float)(nu-1)/(float)(nu);
			pFace->m_pTextureCoordinates[1] = 1.-(float)(v)/(float)(nv-1);
			
			pFace->m_pVertices[1] = (v+1) * nu + nu-1; // ( nu-1 , v+1 )
			pFace->m_pTextureCoordinates[2] = (float)(nu-1)/(float)(nu);
			pFace->m_pTextureCoordinates[3] = 1.-(float)(v+1)/(float)(nv-1);
			
			pFace->m_pVertices[2] = v * nu; // ( 0 , v )
			pFace->m_pTextureCoordinates[4] = 1.;
			pFace->m_pTextureCoordinates[5] = 1.-(float)(v)/(float)(nv-1);
			//f++;
			m_pFaces[fi++] = pFace;

			// triangle 2
			pFace = new Face ();
			pFace->m_bUseTextureCoordinates = true;
			if (!pFace->m_pTextureCoordinates)
				pFace->m_pTextureCoordinates = new float[6];
			pFace->SetMaterialId (MATERIAL_NONE);

			pFace->m_pVertices[0] = v * nu; // ( 0 , v )
			pFace->m_pTextureCoordinates[0] = 1.;
			pFace->m_pTextureCoordinates[1] = 1.-(float)(v)/(float)(nv-1);
			
			pFace->m_pVertices[1] = (v+1) * nu + nu-1; // ( nu-1 , v+1 )
			pFace->m_pTextureCoordinates[2] = (float)(nu-1)/(float)(nu);
			pFace->m_pTextureCoordinates[3] = 1.-(float)(v+1)/(float)(nv-1);
			
			pFace->m_pVertices[2] = (v+1) * nu; // ( 0 , v+1 ) 
			pFace->m_pTextureCoordinates[4] = 1.;
			pFace->m_pTextureCoordinates[5] = 1.-(float)(v+1)/(float)(nv-1);
			//f++;
			m_pFaces[fi++] = pFace;
		}
	}
	if (bCloseV)
	{
		if (bIndependentCloseV)
		{
			// link with the vertex v=0;
			for (unsigned int u=1; u<nu-1; u++)
			{
				pFace = new Face ();
				pFace->m_bUseTextureCoordinates = true;
				pFace->SetMaterialId (MATERIAL_NONE);

				pFace->m_pVertices[0] = u;
				pFace->m_pTextureCoordinates[0] = 0;
				pFace->m_pTextureCoordinates[1] = 0;
				
				pFace->m_pVertices[1] = 0;
				pFace->m_pTextureCoordinates[2] = 0;
				pFace->m_pTextureCoordinates[3] = 1;
				
				pFace->m_pVertices[2] = (u+1)%nu;
				pFace->m_pTextureCoordinates[4] = 1;
				pFace->m_pTextureCoordinates[5] = 1;

				m_pFaces[fi++] = pFace;
			}

			// link with the vertex v=1;
			for (unsigned int u=1; u<nu-1; u++)
			{
				pFace = new Face ();
				//mesh_face_mi(mesh, f) = MATERIAL_NONE;
				//mesh->f[f].mat.u.texture = ((unsigned int)-1);
				//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
				pFace->m_pVertices[0] = (nv-1) * nu + u;
				//mesh_face_tex(mesh, f, 0)[0] = 0;
				//mesh_face_tex(mesh, f, 0)[1] = 0;
				
				pFace->m_pVertices[1] = (nv-1) * nu;
				//mesh_face_tex(mesh, f, 1)[0] = 0;
				//mesh_face_tex(mesh, f, 1)[1] = 1;
				
				pFace->m_pVertices[2] = (nv-1) * nu + (u+1)%nu;
				//mesh_face_tex(mesh, f, 2)[0] = 1;
				//mesh_face_tex(mesh, f, 2)[1] = 1;

				m_pFaces[fi++] = pFace;
			}
		}
		else
		{
			if (!bInverseCloseV)
				for (unsigned int u=0; u<nu-1; u++)
				{
					// triangle 1
					pFace = new Face ();
					//mesh_face_mi(mesh, f) = MATERIAL_NONE;
					//mesh->f[f].mat.u.texture = ((unsigned int)-1);
					//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
					pFace->m_pVertices[0] = (nv-1) * nu + u; // ( u , nv-1 )
					//mesh_face_tex(mesh, f, 0)[0] = 0;
					//mesh_face_tex(mesh, f, 0)[1] = 0;
					
					pFace->m_pVertices[1] = u; // ( u , 0 )
					//mesh_face_tex(mesh, f, 1)[0] = 1;
					//mesh_face_tex(mesh, f, 1)[1] = 1;
					
					pFace->m_pVertices[2] = (nv-1) * nu + u + 1; // ( u+1 , nv-1 )
					//mesh_face_tex(mesh, f, 2)[0] = 0;
					//mesh_face_tex(mesh, f, 2)[1] = 1;
					//f++;
					m_pFaces[fi++] = pFace;
			
					// triangle 2
					pFace = new Face ();
					//mesh_face_mi(mesh, f) = MATERIAL_NONE;
					//mesh->f[f].mat.u.texture = ((unsigned int)-1);
					//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
					pFace->m_pVertices[0] = (nv-1) * nu + u + 1; // ( u+1 , nv-1 )
					//mesh_face_tex(mesh, f, 0)[0] = 0;
					//mesh_face_tex(mesh, f, 0)[1] = 0;
					
					pFace->m_pVertices[1] = u; // ( u , 0 )
					//mesh_face_tex(mesh, f, 1)[0] = 1;
					//mesh_face_tex(mesh, f, 1)[1] = 1;
					
					pFace->m_pVertices[2] = (u + 1); // ( u+1 , 0 ) 
					//mesh_face_tex(mesh, f, 2)[0] = 0;
					//mesh_face_tex(mesh, f, 2)[1] = 1;
					//f++;
					m_pFaces[fi++] = pFace;
			}
			else
				for (unsigned int u=0; u<nu-1; u++)
				{
					// triangle 1
					pFace = new Face ();
					//mesh_face_mi(mesh, f) = MATERIAL_NONE;
					//mesh->f[f].mat.u.texture = ((unsigned int)-1);
					//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
					pFace->m_pVertices[0] = (nv-1) * nu + u; // ( u , nv-1 )
					//mesh_face_tex(mesh, f, 0)[0] = 0;
					//mesh_face_tex(mesh, f, 0)[1] = 0;
					
					pFace->m_pVertices[1] = (nu-1) - u; // ( u , 0 )
					//mesh_face_tex(mesh, f, 1)[0] = 1;
					//mesh_face_tex(mesh, f, 1)[1] = 1;
					
					pFace->m_pVertices[2] = (nv-1) * nu + u + 1; // ( u+1 , nv-1 )
					//mesh_face_tex(mesh, f, 2)[0] = 0;
					//mesh_face_tex(mesh, f, 2)[1] = 1;
					//f++;
					m_pFaces[fi++] = pFace;
					
					// triangle 2
					pFace = new Face ();
					//mesh_face_mi(mesh, f) = MATERIAL_NONE;
					//mesh->f[f].mat.u.texture = ((unsigned int)-1);
					//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
					pFace->m_pVertices[0] = (nv-1) * nu + u + 1; // ( u+1 , nv-1 )
					//mesh_face_tex(mesh, f, 0)[0] = 0;
					//mesh_face_tex(mesh, f, 0)[1] = 0;
					
					pFace->m_pVertices[1] = (nu-1) - u; // ( u , 0 )
					//mesh_face_tex(mesh, f, 1)[0] = 1;
					//mesh_face_tex(mesh, f, 1)[1] = 1;
					
					pFace->m_pVertices[2] = (nu-1) - (u + 1); // ( u+1 , 0 ) 
					//mesh_face_tex(mesh, f, 2)[0] = 0;
					//mesh_face_tex(mesh, f, 2)[1] = 1;
					//f++;
					m_pFaces[fi++] = pFace;
				}
		}
	}
	if (bCloseU && bCloseV && !bIndependentCloseU && !bIndependentCloseV)
	{
		// triangle 1
		pFace = new Face ();
		//mesh_face_mi(mesh, f) = MATERIAL_NONE;
		//mesh->f[f].mat.u.texture = ((unsigned int)-1);
		//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
		pFace->m_pVertices[0] = (nv-1) * nu + nu-1; // ( nu-1 , nv-1 )
		//mesh_face_tex(mesh, f, 0)[0] = 0;
		//mesh_face_tex(mesh, f, 0)[1] = 0;
		
		pFace->m_pVertices[1] = nu-1; // ( nu-1 , 0 )
		//mesh_face_tex(mesh, f, 1)[0] = 1;
		//mesh_face_tex(mesh, f, 1)[1] = 1;
		
		pFace->m_pVertices[2] = (nv-1) * nu; // ( 0 , nv-1 )
		//mesh_face_tex(mesh, f, 2)[0] = 0;
		//mesh_face_tex(mesh, f, 2)[1] = 1;
		//f++;
		m_pFaces[fi++] = pFace;
		
		// triangle 2
		pFace = new Face ();
		//mesh_face_mi(mesh, f) = MATERIAL_NONE;
		//mesh->f[f].mat.u.texture = ((unsigned int)-1);
		//mesh->f[f].flags |= FACE_FLAG_TEXCOORD;
		pFace->m_pVertices[0] = (nv-1) * nu; // ( 0 , nv-1 )
		//mesh_face_tex(mesh, f, 0)[0] = 0;
		//mesh_face_tex(mesh, f, 0)[1] = 0;
		
		pFace->m_pVertices[1] = nu-1; // ( nu-1 , 0 )
		//mesh_face_tex(mesh, f, 1)[0] = 1;
		//mesh_face_tex(mesh, f, 1)[1] = 1;
		
		pFace->m_pVertices[2] = 0; // (  , 0 ) 
		//mesh_face_tex(mesh, f, 2)[0] = 0;
		//mesh_face_tex(mesh, f, 2)[1] = 1;
		//f++;
		m_pFaces[fi++] = pFace;
	}

	return 1;
}

int ParametricSurface::EvaluateFundamentalForms (diff_s *diff, vec3 dfdu, vec3 dfdv, vec3 dfdudu, vec3 dfdudv, vec3 dfdvdv)
{
	// first fundamental form
	float e = vec3_dot_product (dfdu, dfdu);
	float f = vec3_dot_product (dfdu, dfdv);
	float g = vec3_dot_product (dfdv, dfdv);
	vec3_init (diff->first_fundamental_form, e, f, g);

	// normal
	vec3_cross_product (diff->normal, dfdu, dfdv);
	vec3_normalize (diff->normal);

	// second fundamental form
	float l = vec3_dot_product (dfdudu, diff->normal);
	float m = vec3_dot_product (dfdudv, diff->normal);
	float n = vec3_dot_product (dfdvdv, diff->normal);
	vec3_init (diff->second_fundamental_form, l, m, n);

	return 0;
}

Tensor* ParametricSurface::EvaluateTensor (diff_s diff)
{
	float _e, _f, _g, _a, _l, _m, _n;
	float temp;
	float kappa_mean, kappa_gaussian;
	float kappa1, kappa2;
/*
	_e = 1.+_dx*_dx;
	_f = _dx*_dy;
	_g = 1.+_dy*_dy;
	_a = 1./sqrt(_e*_g-_f*_f);
	_l = _a*_dxx;
	_m = _a*_dxy;
	_n = _a*_dyy;
*/
	_e = diff.first_fundamental_form[0];
	_f = diff.first_fundamental_form[1];
	_g = diff.first_fundamental_form[2];
	_l = diff.second_fundamental_form[0];
	_m = diff.second_fundamental_form[1];
	_n = diff.second_fundamental_form[2];

	if (_e*_g-_f*_f == 0.)
		return NULL;

	// mean curvature
	kappa_mean = 0.5 * (_e*_n-2*_f*_m+_g*_l) / (_e*_g-_f*_f);

	// gaussian curvature
	kappa_gaussian = (_l*_n-_m*_m) / (_e*_g-_f*_f);
	
	// principal curvatures
	temp = kappa_mean * kappa_mean - kappa_gaussian;
	if (std::fabs(temp) < 10.f * std::numeric_limits<float>::epsilon())
	{
		temp = 0.f;
	}
	if (temp < 0.0)
	{
		printf ("!!! Problem : kappa_mean * kappa_mean - kappa_gaussian = %f < 0\n", temp);
		temp = 0.0;
	}
	temp = sqrt (temp);
	kappa1 = kappa_mean + temp;
	kappa2 = kappa_mean - temp;

	// principal directions
	mat2 m;
	mat2_init (m,
		   _l*_g - _f*_m, _e*_m - _f*_l,
		   _g*_m - _f*_n, _e*_n - _f*_m);
	vec2 ev1, ev2, evalues;
	mat2_solve_eigensystem (m, ev1, ev2, evalues);

	/*
	 * evalues[0] should be equal to kappa1
	 * evalues[1] should be equal to kappa2
	 * printf ("%f - %f\n", kappa1 - evalues[0]/(_e*_g-_f*_f), kappa2 - evalues[1]/(_e*_g-_f*_f));
	 */
	
	// let's save the differential parameters
	Tensor *tensor = new Tensor ();
      
	// normale
	//Vector3d normale (-_dx, -_dy, 1.0);
	Vector3d normale (diff.normal[0], diff.normal[1], diff.normal[2]);
	normale.Normalize ();
	tensor->SetNormal (normale.x, normale.y, normale.z);
      
	// principal curvatures
	tensor->SetKappaMax (kappa1);
	tensor->SetKappaMin (kappa2);

	// principal directions
	Vector3d d1 (ev1[0], ev1[1], 0.);
	Vector3d d2 (ev2[0], ev2[1], 0.);
	d1.Normalize ();
	d2.Normalize ();

	// projection on the tangent plane
	float tmp;
	tmp = d1 * normale;
	Vector3d dd1 (d1.x - tmp*normale.x, d1.y - tmp*normale.y, d1.z - tmp*normale.z);
	dd1.Normalize ();
	tmp = d2 * normale;
	Vector3d dd2 (d2.x - tmp*normale.x, d2.y - tmp*normale.y, d2.z - tmp*normale.z);
	dd2.Normalize ();
	
	tensor->SetDirectionMax (dd1.x, dd1.y, dd1.z);
	tensor->SetDirectionMin (dd2.x, dd2.y, dd2.z);
	
	return tensor;
}

//
// compute the 3D coordinates of a point onto a parametric surface
// giving its parametric coordinates (u,v)
//
// input :
//  fU : index u of the point
//  fV : index v of the point
//
// output :
//  return value : 0 is ok, 1 if a problem has been encountered
//  (x,y,z) : 3D coordinates of the point onto the surface
//

int ParametricSphere::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2 * M_PI * fU;
	float fv = M_PI * fV;
	float sinu = sin (fu);
	float cosu = cos (fu);
	float sinv = sin (fv);
	float cosv = cos (fv);

	float fx = cosu * sinv;
	float fy = sinu * sinv;
	float fz = cosv;

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, - sinu * sinv, cosu * sinv, 0.);
	vec3_init (dfdv, cosu * cosv, sinu * cosv, - sinv);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, - cosu * sinv, - sinu * sinv, 0.);
	vec3_init (dfdudv, - sinu * cosv, cosu * cosv, 0.);
	vec3_init (dfdvdv, - cosu * sinv, - sinu * sinv, - cosv);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

ParametricSphere::ParametricSphere (unsigned int nu, unsigned int nv)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 1;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	Generate ();
}


//
// elliptic helicoid (ref : http://mathworld.wolfram.com/EllipticHelicoid.html)
//
int EllipticHelicoid::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float a = fParams[0];
	float b = fParams[1];
	float c = fParams[2];

	float fu = 2 * M_PI * fU;
	float fv = fV;
	float sinu = sin (fu);
	float cosu = cos (fu);

	float fx = a * fv * cosu;
	float fy = b * fv * sinu;
	float fz = c * fu;

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, - a * fv * sinu, b * fv * cosu, c);
	vec3_init (dfdv, a * cosu, b * sinu, 0.);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, - a * fv * cosu, - b * fv * sinu, 0.);
	vec3_init (dfdudv, - a * sinu, b * cosu, 0.);
	vec3_init (dfdvdv, 0., 0., 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

EllipticHelicoid::EllipticHelicoid (unsigned int nu, unsigned int nv, float a, float b, float c)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = a; // 1
	fParams[1] = b; // 1
	fParams[2] = c; // 0.2
	Generate ();
}

//
// seashell (ref : http://mathworld.wolfram.com/Seashell.html)
//
int SeaShell::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 6*M_PI * fU;
	float fv = 2*M_PI * fV;
	float tmp = cos (fv/2.0f) * cos (fv/2.0f);
	float fx = 2 * (1-exp (fu/(6*M_PI))) * cos (fu) * tmp;
	float fy = 2 * (-1+exp (fu/(6*M_PI))) * sin (fu) * tmp;
	float fz = 1 - exp (fu/(3*M_PI)) - sin(fv) + exp (fu/(6*M_PI)) * sin (fv);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 0., 0., 0.);
	vec3_init (dfdv, 0., 0., 0.);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

SeaShell::SeaShell (unsigned int nu, unsigned int nv)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	Generate ();
}

//
// seashell with parameterization by von Seggern (ref : http://mathworld.wolfram.com/Seashell.html)
//
int SeaShellVonSeggern::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float a = fParams[0];
	float b = fParams[1];
	float c = fParams[2];
	float n = fParams[3];

	float fu = 2*M_PI * fU;
	float fv = 2*M_PI * fV;

	float fx = ((1-fv/(2*M_PI)) * (1+cos(fu)) + c) * cos (n * fv);
	float fy = ((1-fv/(2*M_PI)) * (1+cos(fu)) + c) * sin (n * fv);
	float fz = (b * fv) / (2 * M_PI) + a * sin (fu) * (1 - fv / (2*M_PI));

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 0., 0., 0.);
	vec3_init (dfdv, 0., 0., 0.);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

SeaShellVonSeggern::SeaShellVonSeggern (unsigned int nu, unsigned int nv, float a, float b, float c, float n)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = a; // 0.2;
	fParams[1] = b; // 1
	fParams[2] = c; // 0.1;
	fParams[3] = n; // 2;
	Generate ();
}

//
// corkscrew surface (ref : http://mathworld.wolfram.com/CorkscrewSurface.html)
//
CorkscrewSurface::CorkscrewSurface (unsigned int nu, unsigned int nv, float a, float b)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;

	fParams[0] = a; // 1.0
	fParams[1] = b; // 0.5

	Generate ();
}

int CorkscrewSurface::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float a = fParams[0];
	float b = fParams[1];

	float fu = 2 * M_PI * fU;
	float fv = 2 * M_PI * fV;

	float fx = a * cos (fu) * cos (fv);
	float fy = a * sin (fu) * cos (fv);
	float fz = a * sin (fv) + b * fu;

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 0., 0., 0.);
	vec3_init (dfdv, 0., 0., 0.);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
// Mobius strip (ref : http://mathworld.wolfram.com/MoebiusStrip.html)
//
int MobiusStrip::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float w = fParams[0]; // 0.1
	float r = fParams[1]; // 0.5

	float fu = 2 * w * fU - w;
	float fv = 2 * M_PI * fV;

	float fx = (r + fu * cos (fv/2.)) * cos (fv);
	float fy = (r + fu * cos (fv/2.)) * sin (fv);
	float fz = fu * sin (fv/2.);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 0., 0., 0.);
	vec3_init (dfdv, 0., 0., 0.);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

MobiusStrip::MobiusStrip (unsigned int nu, unsigned int nv, float w, float r)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 1;
	fParams[0] = w; //
	fParams[1] = r; //
	Generate ();
}

//
// radial wave
//
int RadialWave::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float r = fParams[0]; // 10.0
	float h = fParams[1]; // 20.0
	float f = fParams[2]; // 0.6

	float fu = (fU-0.5)*2.0;
	float fv = (fV-0.5)*2.0;

	float fx = r * fu;
	float fy = r * fv;
	float fz = h * cos(f*sqrt((fu*fu)+(fv*fv)));

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 0., 0., 0.);
	vec3_init (dfdv, 0., 0., 0.);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

RadialWave::RadialWave (unsigned int nu, unsigned int nv, float radius, float height, float frequency)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = radius; //
	fParams[1] = height; //
	fParams[2] = frequency; //
	Generate ();
}

//
// torus
//
int ParametricTorus::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float r1 = fParams[0]; // 10.0
	float r2 = fParams[1]; // 20.0

	float fu = - 2 * M_PI * fU;
	float fv = 2 * M_PI * fV;

	float cosu = cos(fu);
	float sinu = sin(fu);
	float cosv = cos(fv);
	float sinv = sin(fv);

	float fx = (r1 + r2 * cosv) * cosu;
	float fy = (r1 + r2 * cosv) * sinu;
	float fz = r2 * sinv;

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, -(r1 + r2 * cosv) * sinu, (r1 + r2 * cosv) * cosu, 0.);
	vec3_init (dfdv, - r2 * sinv * cosu, - r2 * sinv * sinu, r2 * cosv);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, -(r1 + r2 * cosv) * cosu, - (r1 + r2 * cosv) * sinu, 0.);
	vec3_init (dfdudv, r2 * sinv * sinu, - r2 * sinv * cosu, 0.);
	vec3_init (dfdvdv, - r2 * cosv * cosu, - r2 * cosv * sinu, - r2 * sinv);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

ParametricTorus::ParametricTorus (unsigned int nu, unsigned int nv, float radius1, float radius2)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = radius1; // 5
	fParams[1] = radius2; // 2
	Generate ();
}

//
// treifol knot 1
//
int TrefoilKnot1::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = -4 * M_PI * (fU-0.5);
	float fv = 2 * M_PI * (fV-0.5);

	float fx = cos(fu) * cos(fv) + 3 * cos (fu) * (1.5 + sin(1.5*fu)/2.0);
	float fy = sin(fu) * cos(fv) + 3 * sin (fu) * (1.5 + sin(1.5*fu)/2.0);
	float fz = sin(fv) + 2*cos(1.5*fu);

	// position
	vec3_init (diff->position, fx, fy, fz);

	return 0;
}

TrefoilKnot1::TrefoilKnot1 (unsigned int nu, unsigned int nv)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;

	Generate ();
}

//
// treifol knot 2
//
int TrefoilKnot2::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

//[-3*pi:3*pi][-pi:pi]
	
	float fu = - 6 * M_PI * (fU-0.5);
	float fv = 2 * M_PI * (fV-0.5);

	float fx = cos(fu) * cos (fv) + 3 * cos (fu) * (1.5 + sin (fu*5/3)/2.0); // cos(u)*cos(v)+3*cos(u)*(1.5+sin(u*5/3)/2)
	float fy = sin(fu) * cos (fv) + 3 * sin (fu) * (1.5 + sin (fu*5/3)/2.0); // sin(u)*cos(v)+3*sin(u)*(1.5+sin(u*5/3)/2)
	float fz = sin (fv) + 2. * cos (fu*5/3); // sin(v)+2*cos(u*5/3)

	// position
	vec3_init (diff->position, fx, fy, fz);

	return 0;
}

TrefoilKnot2::TrefoilKnot2 (unsigned int nu, unsigned int nv)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;

	Generate ();
}
/*
//
// trumpet
//
int parametric_surface_get_trumpet (float fU, float fV,
				    float *x, float *y, float *z,
				    parametric_surface_data_s data)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float s = data.fParams[0]; // 0.4
	float t = data.fParams[1]; // 2

	float fu = 2 * M_PI * fU;
	float fv = fV;

	float denom = pow (1+fv, t);
	float fx = cos(fu) / denom;
	float fy = sin(fu) / denom;
	float fz = s*fv;

	*x = fx;
	*y = fy;
	*z = fz;

	return 0;
}

mesh_t *mesh_generate_parametric_surface_trumpet (unsigned int nu, unsigned int nv,
						  float s, float t)
{
	parametric_surface_data_s data;
	data.evaluate = &parametric_surface_get_trumpet;
	data.iNU = nu;
	data.iNV = nv;
	data.bCloseU = 1;
	data.bCloseV = 0;
	data.bIndependentCloseU = 0;
	data.bIndependentCloseV = 0;
	data.bInverseCloseU = 0;
	data.bInverseCloseV = 0;
	data.fParams[0] = s; // 5
	data.fParams[1] = t; // 2
	return mesh_generate_parametric_surface(data);
}

//
// cylinder
//
int parametric_surface_get_cylinder (float fU, float fV,
				    float *x, float *y, float *z,
				    parametric_surface_data_s data)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float radius = data.fParams[0];
	float height = data.fParams[1];

	float fu = 2 * M_PI * fU;
	float fv = fV;

	float fx = radius*cos(-fu);
	float fy = radius*sin(-fu);
	float fz = height*fv;

	*x = fx;
	*y = fy;
	*z = fz;

	return 0;
}

mesh_t *mesh_generate_parametric_surface_cylinder (unsigned int nu, unsigned int nv,
						   float r, float h)
{
	parametric_surface_data_s data;
	data.evaluate = &parametric_surface_get_cylinder;
	data.iNU = nu;
	data.iNV = nv;
	data.bCloseU = 1;
	data.bCloseV = 1;
	data.bIndependentCloseU = 0;
	data.bIndependentCloseV = 1;
	data.bInverseCloseU = 0;
	data.bInverseCloseV = 0;
	data.fParams[0] = r;
	data.fParams[1] = h;
	return mesh_generate_parametric_surface(data);
}






//
// tube (http://mathworld.wolfram.com/Tube.html)
//
int parametric_surface_get_helix (float fU, float fV,
				  float *x, float *y, float *z,
				  parametric_surface_data_s data)
{
	(void) data; // unused

	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 3 * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float r = 5;
	float elevation = 2;
	float rtube = 1;

	// point of the curve
	float cx = r*cos(fu);
	float cy = r*sin(fu);
	float cz = elevation*fu;

	// tangent
	fvec3 t;
	fvec3_init (t, -r*sin(fu), r*cos(fu), elevation);
	fvec3_normalize (t);

	// normal
	fvec3 n;
	fvec3_init (n, -r*cos(fu), -r*sin(fu), 0.);
	fvec3_normalize (n);

	// binormal (= T ^ N)
	fvec3 b;
	fvec3_cross_product (b, t, n);
	fvec3_normalize (b);


	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	*x = fx;
	*y = fy;
	*z = fz;

	return 0;
}

mesh_t *mesh_generate_parametric_surface_helix (unsigned int nu, unsigned int nv)
{
	parametric_surface_data_s data;
	data.evaluate = &parametric_surface_get_helix;
	data.iNU = nu;
	data.iNV = nv;
	data.bCloseU = 0;
	data.bCloseV = 1;
	data.bIndependentCloseU = 0;
	data.bIndependentCloseV = 0;
	data.bInverseCloseU = 0;
	data.bInverseCloseV = 0;
	return mesh_generate_parametric_surface(data);
}
*/

//
// borromean rings (http://local.wasp.uwa.edu.au/~pbourke/geometry/borromean/)
//
// (cos(u)       , sin(u) + r   , cos(3u) / 3)
// (cos(u) + 0.5 , sin(u) - r/2 , cos(3u) / 3)
// (cos(u) - 0.5 , sin(u) - r/2 , cos(3u) / 3)
//
int BorromeanRing::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2 * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float param1 = fParams[0];
	float param2 = fParams[1];
	float r = fParams[2]; // sqrt(3)/3

	float rtube = 0.2;

	// point of the curve
	float cx = cos(fu) + param1;
	float cy = sin(fu) + param2*r;
	float cz = cos(3.*fu) / 3.;

	// tangent
	vec3 t;
	vec3_init (t, -sin(fu), cos(fu), -sin(3.*fu));
	vec3_normalize (t);

	// normal
	vec3 n;
	vec3_init (n, -cos(fu), -sin(fu), -cos(3.*fu));
	vec3_normalize (n);

	// binormal (= T ^ N)
	vec3 b;
	vec3_cross_product (b, t, n);
	vec3_normalize (b);

	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	// position
	vec3_init (diff->position, fx, fy, fz);

	return 0;
}

BorromeanRing::BorromeanRing (unsigned int nu, unsigned int nv, float param1, float param2, float r)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	
	fParams[0] = param1;
	fParams[1] = param2;
	fParams[2] = r;

	Generate ();
}

BorromeanRings::BorromeanRings (unsigned int nu, unsigned int nv)
{
	BorromeanRing *r1 = new BorromeanRing (nu, nv, 0., 1., sqrt(3.)/3.);
	MaterialColor *r = new MaterialColor (255, 0, 0, 255);
	r1->ApplyMaterial (r1->Material_Add (r));

	BorromeanRing *r2 = new BorromeanRing (nu, nv, 0.5, -0.5, sqrt(3.)/3.);
	MaterialColor *g = new MaterialColor (0, 255, 0, 255);
	r2->ApplyMaterial (r2->Material_Add (g));

	BorromeanRing *r3 = new BorromeanRing (nu, nv, -0.5, -0.5, sqrt(3.)/3.);
	MaterialColor *b = new MaterialColor (0, 0, 255, 255);
	r3->ApplyMaterial (r3->Material_Add (b));

	Append (r1);
	Append (r2);
	Append (r3);
}

/*
//
// borromean elliptical rings (http://local.wasp.uwa.edu.au/~pbourke/geometry/borromean/)
//
// (     0     , r1 cos(u) , r2 sin(u) )
// ( r2 cos(u) ,     0     , r1 sin(u) )
// ( r1 cos(u) , r2 sin(u) ,     0     )
//
int parametric_surface_get_borromean_elliptical_rings1 (float fU, float fV,
							float *x, float *y, float *z,
							parametric_surface_data_s data)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2 * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float r1 = data.fParams[0];
	float r2 = data.fParams[1];

	float rtube = 0.2;

	// point of the curve
	float cx = 0.;
	float cy = r1 * cos(fu);
	float cz = r2 * sin(fu);

	// tangent
	fvec3 t;
	fvec3_init (t, 0., -r1*sin(fu), r2*cos(fu));
	fvec3_normalize (t);

	// normal
	fvec3 n;
	fvec3_init (n, 0., -r1*cos(fu), -r2*sin(fu));
	fvec3_normalize (n);

	// binormal (= T ^ N)
	fvec3 b;
	fvec3_cross_product (b, t, n);
	fvec3_normalize (b);


	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	*x = fx;
	*y = fy;
	*z = fz;

	return 0;
}

int parametric_surface_get_borromean_elliptical_rings2 (float fU, float fV,
							float *x, float *y, float *z,
							parametric_surface_data_s data)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2 * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float r1 = data.fParams[0];
	float r2 = data.fParams[1];

	float rtube = 0.2;

	// point of the curve
	float cx = r2 * cos(fu);
	float cy = 0.;
	float cz = r1 * sin(fu);

	// tangent
	fvec3 t;
	fvec3_init (t, -r2*sin(fu), 0., r1*cos(fu));
	fvec3_normalize (t);

	// normal
	fvec3 n;
	fvec3_init (n, -r2*cos(fu), 0., -r1*sin(fu));
	fvec3_normalize (n);

	// binormal (= T ^ N)
	fvec3 b;
	fvec3_cross_product (b, t, n);
	fvec3_normalize (b);


	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	*x = fx;
	*y = fy;
	*z = fz;

	return 0;
}

int parametric_surface_get_borromean_elliptical_rings3 (float fU, float fV,
							float *x, float *y, float *z,
							parametric_surface_data_s data)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2 * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float r1 = data.fParams[0];
	float r2 = data.fParams[1];

	float rtube = 0.2;

	// point of the curve
	float cx = r1 * cos(fu);
	float cy = r2 * sin(fu);
	float cz = 0.;

	// tangent
	fvec3 t;
	fvec3_init (t, -r1*sin(fu), r2*cos(fu), 0.);
	fvec3_normalize (t);

	// normal
	fvec3 n;
	fvec3_init (n, -r1*cos(fu), -r2*sin(fu), 0.);
	fvec3_normalize (n);

	// binormal (= T ^ N)
	fvec3 b;
	fvec3_cross_product (b, t, n);
	fvec3_normalize (b);


	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	*x = fx;
	*y = fy;
	*z = fz;

	return 0;
}

mesh_t *mesh_generate_parametric_surface_borromean_elliptical_rings (unsigned int nu, unsigned int nv)
{
	parametric_surface_data_s data;
	data.iNU = nu;
	data.iNV = nv;
	data.bCloseU = 1;
	data.bCloseV = 1;
	data.bIndependentCloseU = 0;
	data.bIndependentCloseV = 0;
	data.bInverseCloseU = 0;
	data.bInverseCloseV = 0;

	data.fParams[0] = 2.;
	data.fParams[1] = 1.;

	data.evaluate = &parametric_surface_get_borromean_elliptical_rings1;
	mesh_t *mesh1 = mesh_generate_parametric_surface(data);
	mesh_set_color (mesh1, color_float_rgba(1., 0., 0., 1.));

	data.evaluate = &parametric_surface_get_borromean_elliptical_rings2;
	mesh_t *mesh2 = mesh_generate_parametric_surface(data);
	mesh_set_color (mesh2, color_float_rgba(0., 1., 0., 1.));

	data.evaluate = &parametric_surface_get_borromean_elliptical_rings3;
	mesh_t *mesh3 = mesh_generate_parametric_surface(data);
	mesh_set_color (mesh3, color_float_rgba(0., 0., 1., 1.));

	mesh_append(mesh1, mesh2);
	mesh_delete (mesh2);

	mesh_append(mesh1, mesh3);
	mesh_delete (mesh3);

	return mesh1;
}
*/

//
// torus knot (http://vmm.math.uci.edu/3D-XplorMath/index.html)
// http://virtualmathmuseum.org/SpaceCurves/torus_knot/torus_knot.html
//
int TorusKnot::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2 * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float rtube = 0.8;

	float aa = 9.;
	float bb = 3.;
	float cc = 4.;

	float dd = fParams[0];
	float ee = fParams[1];

	// point of the curve
	float cx = (aa + bb*cos(dd * fu)) * cos(ee * fu);
	float cy = (aa + bb*cos(dd * fu)) * sin(ee * fu);
	float cz = cc*sin(dd * fu);

	// tangent
	vec3 t;
	vec3_init (t,
		   -(bb*dd*sin(dd*fu))*cos(ee*fu) - (aa+bb*cos(dd*fu))*ee*sin(ee*fu),
		   -(bb*dd*sin(dd*fu))*sin(ee*fu) + (aa+bb*cos(dd*fu))*ee*cos(ee*fu),
		   cc*dd*cos(dd*fu));
	vec3_normalize (t);

	// normal
	vec3 n;
	vec3_init (n,
		   -bb*dd*dd*cos(dd*fu)*cos(ee*fu) + (bb*dd*sin(dd*fu))*ee*sin(ee*fu) - (-bb*dd*sin(dd*fu)*ee*sin(ee*fu) + (aa+bb*cos(dd*fu))*ee*ee*cos(ee*fu)),
		   -bb*dd*dd*cos(dd*fu)*sin(ee*fu) - (bb*dd*sin(dd*fu))*ee*cos(ee*fu) + (-bb*dd*sin(dd*fu)*ee*cos(ee*fu) - (aa+bb*cos(dd*fu))*ee*ee*sin(ee*fu)),
		   -cc*dd*dd*sin(dd*fu));
	vec3_normalize (n);

	// binormal (= T ^ N)
	vec3 b;
	vec3_cross_product (b, t, n);
	vec3_normalize (b);

	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	// position
	vec3_init (diff->position, fx, fy, fz);

	return 0;
}

TorusKnot::TorusKnot (unsigned int nu, unsigned int nv, unsigned int a, unsigned int b)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;

	fParams[0] = a;
	fParams[1] = b;

	Generate ();
}

//
// Cinquefoil knot
//
// ref : http://local.wasp.uwa.edu.au/~pbourke/geometry/cinquefoil/
//
int CinquefoilKnot::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float k = fParams[0];

	float fu = (4*k+2) * M_PI * (fU);
	float fv = 2 * M_PI * (1.0 - fV); // tube

	float rtube = 0.1;

	// point of the curve
	float cx = cos(fu) * (2 - cos(2*fu/(2*k + 1)));
	float cy = sin(fu) * (2 - cos(2*fu/(2*k + 1)));
	float cz = -sin(2*fu/(2*k + 1));

	// tangent
	vec3 t;
	vec3_init (t,
		   -sin(fu)*(2-cos(2*fu/(2*k+1))) + cos(fu)*sin(2*fu/(2*k+1))*2/(2*k+1),
		   cos(fu)*(2-cos(2*fu/(2*k+1))) + sin(fu)*sin(2*fu/(2*k+1))*2/(2*k+1),
		   -cos(2*fu/(2*k+1))*2/(2*k+1) );
	vec3_normalize (t);

	// normal
	vec3 n;
	vec3_init (n,
		   -cos(fu)*(2-cos(2*fu/(2*k+1)))-sin(fu)*sin(2*fu/(2*k+1))*2/(2*k+1) - sin(fu)*sin(2*fu/(2*k+1))*2/(2*k+1)+cos(fu)*cos(2*fu/(2*k+1))*4/((2*k+1)*(2*k+1)),
		   -sin(fu)*(2-cos(2*fu/(2*k+1)))+cos(fu)*sin(2*fu/(2*k+1))*2/(2*k+1) + cos(fu)*sin(2*fu/(2*k+1))*2/(2*k+1)+sin(fu)*cos(2*fu/(2*k+1))*4/((2*k+1)*(2*k+1)),
		   -sin(2*fu/(2*k+1))*4/((2*k+1)*(2*k+1)) );
	vec3_normalize (n);
	
	// binormal (= T ^ N)
	vec3 b;
	vec3_cross_product (b, t, n);
	vec3_normalize (b);

	float fx = cx + rtube*(-n[0]*cos(fv) + b[0]*sin(fv));
	float fy = cy + rtube*(-n[1]*cos(fv) + b[1]*sin(fv));
	float fz = cz + rtube*(-n[2]*cos(fv) + b[2]*sin(fv));

	// position
	vec3_init (diff->position, fx, fy, fz);

	return 0;
}

CinquefoilKnot::CinquefoilKnot (unsigned int nu, unsigned int nv, unsigned int a)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;

	fParams[0] = a;

	Generate ();
}


//
// Breather (http://virtualmathmuseum.org/Surface/breather_p/breather_p.html)
//
Breather::Breather (unsigned int nu, unsigned int nv)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 1;
	bCloseV = 1;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	Generate ();
}

int Breather::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float fu = 2*14*(fU-0.5);
	float fv = 2*37.4*(fV-0.5);
	
	float aa = 2./5.;
	float wsqr = 1. - aa * aa;
        float w = sqrt(wsqr);
	float denom = aa * (w * cosh(aa * fu)* cosh(aa * fu) + aa * aa * sin(w * fv)* sin(w * fv));

	float fx = -fu/1.2 + (2 * wsqr * cosh(aa * fu) * sinh(aa * fu) / denom);
	float fy = 2 * w * cosh(aa * fu) * (-(w * cos(fv) * cos(w * fv)) - (sin(fv) * sin(w * fv))) / denom;
	float fz = 2 * w * cosh(aa * fu) * (-(w * sin(fv) * cos(w * fv)) + (cos(fv) * sin(w * fv))) / denom;

	// position
	vec3_init (diff->position, fx, fy, fz);

	return 0;
}

//
// HyperbolicParaboloid
// http://mathworld.wolfram.com/HyperbolicParaboloid.html
//
HyperbolicParaboloid::HyperbolicParaboloid (unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int HyperbolicParaboloid::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 0.2;

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude*(fx*fx - fy*fy);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., 2*amplitude*fx);
	vec3_init (dfdv, 0., 1., -2*amplitude*fy);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 2*amplitude);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., -2*amplitude);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
MonkeySaddle::MonkeySaddle(unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int MonkeySaddle::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 0.01;

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude*fx*(fx*fx - 3*fy*fy);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., 3*amplitude*fx*fx - 3*amplitude*fy*fy);
	vec3_init (dfdv, 0., 1., -6*amplitude*fx*fy);

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 6*amplitude*fx);
	vec3_init (dfdudv, 0., 0., -6*amplitude*fy);
	vec3_init (dfdvdv, 0., 0., -6*amplitude*fx);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
Blobs::Blobs(unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int Blobs::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 1.;

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude * sin (fy) * cos (fx);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., -amplitude*sin(fy)*sin(fx));
	vec3_init (dfdv, 0., 1., amplitude*cos(fy)*cos(fx));

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., -amplitude*sin(fy)*cos(fx));
	vec3_init (dfdudv, 0., 0., -amplitude*cos(fy)*sin(fx));
	vec3_init (dfdvdv, 0., 0., -amplitude*sin(fy)*cos(fx));

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
Drop::Drop(unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int Drop::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 1.;
	float w = 7.0; // frequency

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude * sin (w * sqrt (1 + fx*fx + fy*fy));

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., amplitude*w*fx*cos(w*sqrt(1+fx*fx+fy*fy)) / (sqrt(1+fx*fx+fy*fy)));
	vec3_init (dfdv, 0., 1., amplitude*w*fy*cos(w*sqrt(1+fx*fx+fy*fy)) / (sqrt(1+fx*fx+fy*fy)));

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., amplitude*w*(1+fx*fx+fy*fy-fx*fx*cos(w*sqrt(1+fx*fx+fy*fy))/((1+fx*fx+fy*fy)*sqrt(1+fx*fx+fy*fy))
						-			      
						amplitude*w*fx*fx*sin(w*sqrt(1+fx*fx+fy*fy))/(1+fx*fx+fy*fy)));
	vec3_init (dfdudv, 0., 0., -(amplitude*w*w*fx*fy*sin(w*sqrt(1+fx*fx+fy*fy))*sqrt(1+fx*fx+fy*fy)
				     +
				     amplitude*w*fx*fy*cos(w*sqrt(1+fx*fx+fy*fy)))
		   / (1+fx*fx+fy*fy));
	vec3_init (dfdvdv, 0., 0., amplitude*w*(1+fx*fx+fy*fy-fy*fy*cos(w*sqrt(1+fx*fx+fy*fy))/((1+fx*fx+fy*fy)*sqrt(1+fx*fx+fy*fy))
						-			      
						amplitude*w*fy*fy*sin(w*sqrt(1+fx*fx+fy*fy))/(1+fx*fx+fy*fy)));
	
	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
Wave1::Wave1(unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int Wave1::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 1.;

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude * sin (fy*3);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., 0.);
	vec3_init (dfdv, 0., 1., 3*amplitude*cos(3*fy));

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, 0., 0., 0.);
	vec3_init (dfdvdv, 0., 0., -9*amplitude*sin(3*fy));

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
Wave2::Wave2(unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int Wave2::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 1.;

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude*sin (2*(fx+fy));

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., 2*amplitude*cos(2*(fx+fy)));
	vec3_init (dfdv, 0., 1., 2*amplitude*cos(2*(fx+fy)));

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., -4*amplitude*sin(2*(fx+fy)));
	vec3_init (dfdudv, 0., 0., -4*amplitude*sin(2*(fx+fy)));
	vec3_init (dfdvdv, 0., 0., -4*amplitude*sin(2*(fx+fy)));

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
Weight::Weight(unsigned int nu, unsigned int nv, float xmin, float xmax, float ymin, float ymax)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = xmin;
	fParams[1] = xmax;
	fParams[2] = ymin;
	fParams[3] = ymax;
	Generate ();
}

int Weight::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float xmin = fParams[0];
	float xmax = fParams[1];
	float ymin = fParams[2];
	float ymax = fParams[3];
	float amplitude = 1.;

	float fx = fParams[0]+fU*(fParams[1]-fParams[0]);
	float fy = fParams[2]+fV*(fParams[3]-fParams[2]);
	float fz = amplitude * sqrt (1+fx*fx+fy*fy);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, 1., 0., amplitude*fx/sqrt(1+fx*fx+fy*fy));
	vec3_init (dfdv, 0., 1., amplitude*fy/sqrt(1+fx*fx+fy*fy));

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., amplitude*(1+fy*fy)/((1+fx*fx+fy*fy)*sqrt(1+fx*fx+fy*fy)));
	vec3_init (dfdudv, 0., 0., -amplitude*fx*fy/(1+fx*fx+fy*fy));
	vec3_init (dfdvdv, 0., 0., amplitude*(1+fx*fx)/((1+fx*fx+fy*fy)*sqrt(1+fx*fx+fy*fy)));

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}

//
//
//
Guimard::Guimard(unsigned int nu, unsigned int nv, float a, float b, float c)
{
	iNU = nu;
	iNV = nv;
	bCloseU = 0;
	bCloseV = 0;
	bIndependentCloseU = 0;
	bIndependentCloseV = 0;
	bInverseCloseU = 0;
	bInverseCloseV = 0;
	fParams[0] = a;
	fParams[1] = b;
	fParams[2] = c;
	Generate ();
}

int Guimard::EvaluatePosition (float fU, float fV, diff_s *diff)
{
	if (fU<0 || fU > 1)
		return 1;
	if (fV<0 || fV > 1)
		return 1;

	float a = fParams[0];
	float b = fParams[1];
	float c = fParams[2];

	fV *= 2. * M_PI;

	float fx = (a*(1.-fU) + b*fU) * cos(fV);
	float fy = b * fU * sin(fV);
	float fz = c * fU * sin(fV) * sin(fV);

	// position
	vec3_init (diff->position, fx, fy, fz);

	// partial derivates
	vec3 dfdu, dfdv;
	vec3_init (dfdu, (b-a) * cos(fV), b * sin(fV), c * sin(fV) * sin(fV));
	vec3_init (dfdv, -(a*(1.-fU) + b*fU) * sin(fV), -b * fU * cos(fV), 2. * c * fU * cos(fV) * sin(fV));

	vec3 dfdudu, dfdudv, dfdvdv;
	vec3_init (dfdudu, 0., 0., 0.);
	vec3_init (dfdudv, -(b-a) * sin(fV), b * cos(fV), 2. * c * cos(fV) * sin(fV));
	vec3_init (dfdvdv, -(a*(1.-fU) + b*fU) * cos(fV), b * fU * sin(fV), 0.);

	EvaluateFundamentalForms (diff, dfdu, dfdv, dfdudu, dfdudv, dfdvdv);

	return 0;
}
