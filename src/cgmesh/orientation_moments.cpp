#include <math.h>

#include "orientation_moments.h"

/* CONSTANTS */
#define X 0
#define Y 1
#define Z 2

/* MACROS */
#define SQR(x) ((x)*(x))
#define CUBE(x) ((x)*(x)*(x))

Cmesh_orientation_moments::Cmesh_orientation_moments (Mesh_half_edge *mesh)
  : Cmesh_orientation(mesh)
{
  density = 1.0;  /* assume unit density */
}

void
Cmesh_orientation_moments::compute_orientation (void)
{
  int   nf      = model3d_half_edge->m_nFaces;
  float *v      = model3d_half_edge->m_pVertices;
  Face **f      = model3d_half_edge->m_pFaces;
  float *norm_f = model3d_half_edge->m_pFaceNormals;

  int A;   /* alpha */
  int B;   /* beta */
  int C;   /* gamma */
  double P1, Pa, Pb, Paa, Pab, Pbb, Paaa, Paab, Pabb, Pbbb;  /* projection integrals */
  double Fa, Fb, Fc, Faa, Fbb, Fcc, Faaa, Fbbb, Fccc, Faab, Fbbc, Fcca;  /* face integrals */
  double T0, T1[3], T2[3], TP[3];  /* volume integrals */

  double nx, ny, nz;
  int i,j;

  T0 = T1[X] = T1[Y] = T1[Z] = T2[X] = T2[Y] = T2[Z] = TP[X] = TP[Y] = TP[Z] = 0;

  for (i=0; i<nf; i++)
    {
      nx = fabs(norm_f[3*i]);
      ny = fabs(norm_f[3*i+1]);
      nz = fabs(norm_f[3*i+2]);
      if (nx > ny && nx > nz) C = X;
      else C = (ny > nz) ? Y : Z;
      A = (C + 1) % 3;
      B = (A + 1) % 3;
      
      /*** compute face integrals ***/
      double w;
      double k1, k2, k3, k4;
      
      /*** compute projection integrals ***/
      /* compute various integrations over projection of face */
      double a0, a1, da;
      double b0, b1, db;
      double a0_2, a0_3, a0_4, b0_2, b0_3, b0_4;
      double a1_2, a1_3, b1_2, b1_3;
      double C1, Ca, Caa, Caaa, Cb, Cbb, Cbbb;
      double Cab, Kab, Caab, Kaab, Cabb, Kabb;
      
      P1 = Pa = Pb = Paa = Pab = Pbb = Paaa = Paab = Pabb = Pbbb = 0.0;
      
      for (j=0; j<3; j++)
	{
		a0 = v[3*f[i]->GetVertex(j)+A];
		b0 = v[3*f[i]->GetVertex(j)+B];
		a1 = v[3*f[i]->GetVertex(((j+1)%3))+A];
		b1 = v[3*f[i]->GetVertex(((j+1)%3))+B];
	  da = a1 - a0;
	  db = b1 - b0;
	  a0_2 = a0 * a0; a0_3 = a0_2 * a0; a0_4 = a0_3 * a0;
	  b0_2 = b0 * b0; b0_3 = b0_2 * b0; b0_4 = b0_3 * b0;
	  a1_2 = a1 * a1; a1_3 = a1_2 * a1; 
	  b1_2 = b1 * b1; b1_3 = b1_2 * b1;
	  
	  C1 = a1 + a0;
	  Ca = a1*C1 + a0_2; Caa = a1*Ca + a0_3; Caaa = a1*Caa + a0_4;
	  Cb = b1*(b1 + b0) + b0_2; Cbb = b1*Cb + b0_3; Cbbb = b1*Cbb + b0_4;
	  Cab = 3*a1_2 + 2*a1*a0 + a0_2; Kab = a1_2 + 2*a1*a0 + 3*a0_2;
	  Caab = a0*Cab + 4*a1_3; Kaab = a1*Kab + 4*a0_3;
	  Cabb = 4*b1_3 + 3*b1_2*b0 + 2*b1*b0_2 + b0_3;
	  Kabb = b1_3 + 2*b1_2*b0 + 3*b1*b0_2 + 4*b0_3;
	  
	  P1 += db*C1;
	  Pa += db*Ca;
	  Paa += db*Caa;
	  Paaa += db*Caaa;
	  Pb += da*Cb;
	  Pbb += da*Cbb;
	  Pbbb += da*Cbbb;
	  Pab += db*(b1*Cab + b0*Kab);
	  Paab += db*(b1*Caab + b0*Kaab);
	  Pabb += da*(a1*Cabb + a0*Kabb);
	}
      
      P1 /= 2.0;
      Pa /= 6.0;
      Paa /= 12.0;
      Paaa /= 20.0;
      Pb /= -6.0;
      Pbb /= -12.0;
      Pbbb /= -20.0;
      Pab /= 24.0;
      Paab /= 60.0;
      Pabb /= -60.0;
      /*** end compute projection integrals ***/
      
      w = 
	      -norm_f[3*i] * v[3*f[i]->GetVertex(0)]
	      -norm_f[3*i+1] * v[3*f[i]->GetVertex(0)+1]
	      -norm_f[3*i+2] * v[3*f[i]->GetVertex(0)+2];
      
      k1 = 1 / norm_f[3*i+C]; k2 = k1 * k1; k3 = k2 * k1; k4 = k3 * k1;
      
      Fa = k1 * Pa;
      Fb = k1 * Pb;
      Fc = -k2 * (norm_f[3*i+A]*Pa + norm_f[3*i+B]*Pb + w*P1);
      
      Faa = k1 * Paa;
      Fbb = k1 * Pbb;
      Fcc = k3 * (SQR(norm_f[3*i+A])*Paa + 2*norm_f[3*i+A]*norm_f[3*i+B]*Pab + SQR(norm_f[3*i+B])*Pbb
		  + w*(2*(norm_f[3*i+A]*Pa + norm_f[3*i+B]*Pb) + w*P1));
      
      Faaa = k1 * Paaa;
      Fbbb = k1 * Pbbb;
      Fccc = -k4 * (CUBE(norm_f[3*i+A])*Paaa + 3*SQR(norm_f[3*i+A])*norm_f[3*i+B]*Paab 
		    + 3*norm_f[3*i+A]*SQR(norm_f[3*i+B])*Pabb + CUBE(norm_f[3*i+B])*Pbbb
		    + 3*w*(SQR(norm_f[3*i+A])*Paa + 2*norm_f[3*i+A]*norm_f[3*i+B]*Pab + SQR(norm_f[3*i+B])*Pbb)
		    + w*w*(3*(norm_f[3*i+A]*Pa + norm_f[3*i+B]*Pb) + w*P1));
      
      Faab = k1 * Paab;
      Fbbc = -k2 * (norm_f[3*i+A]*Pabb + norm_f[3*i+B]*Pbbb + w*Pbb);
      Fcca = k3 * (SQR(norm_f[3*i+A])*Paaa + 2*norm_f[3*i+A]*norm_f[3*i+B]*Paab + SQR(norm_f[3*i+B])*Pabb
		     + w*(2*(norm_f[3*i+A]*Paa + norm_f[3*i+B]*Pab) + w*Pa));
      /*** end of compute face integrals ***/
      
      T0 += norm_f[3*i] * ((A == X) ? Fa : ((B == X) ? Fb : Fc));
      
      T1[A] += norm_f[3*i+A] * Faa;
      T1[B] += norm_f[3*i+B] * Fbb;
      T1[C] += norm_f[3*i+C] * Fcc;
      T2[A] += norm_f[3*i+A] * Faaa;
      T2[B] += norm_f[3*i+B] * Fbbb;
      T2[C] += norm_f[3*i+C] * Fccc;
      TP[A] += norm_f[3*i+A] * Faab;
      TP[B] += norm_f[3*i+B] * Fbbc;
      TP[C] += norm_f[3*i+C] * Fcca;
    }

  T1[X] /= 2; T1[Y] /= 2; T1[Z] /= 2;
  T2[X] /= 3; T2[Y] /= 3; T2[Z] /= 3;
  TP[X] /= 2; TP[Y] /= 2; TP[Z] /= 2;


  /* compute orientation */
  double r[3];            /* center of mass */
  double J[3][3];         /* inertia tensor */
  
  /* compute mass */
  mass = density * T0;

  /* compute center of mass */
  r[X] = T1[X] / T0;
  r[Y] = T1[Y] / T0;
  r[Z] = T1[Z] / T0;

  center[0] = r[X];
  center[1] = r[Y];
  center[2] = r[Z];

  /* compute inertia tensor */
  J[X][X] = density * (T2[Y] + T2[Z]);
  J[Y][Y] = density * (T2[Z] + T2[X]);
  J[Z][Z] = density * (T2[X] + T2[Y]);
  J[X][Y] = J[Y][X] = - density * TP[X];
  J[Y][Z] = J[Z][Y] = - density * TP[Y];
  J[Z][X] = J[X][Z] = - density * TP[Z];

  /* translate inertia tensor to center of mass */
  J[X][X] -= mass * (r[Y]*r[Y] + r[Z]*r[Z]);
  J[Y][Y] -= mass * (r[Z]*r[Z] + r[X]*r[X]);
  J[Z][Z] -= mass * (r[X]*r[X] + r[Y]*r[Y]);
  J[X][Y] = J[Y][X] += mass * r[X] * r[Y];
  J[Y][Z] = J[Z][Y] += mass * r[Y] * r[Z];
  J[Z][X] = J[X][Z] += mass * r[Z] * r[X];

  Matrix3 m3(	J[X][X], J[X][Y], J[X][Z],
				J[Y][X], J[Y][Y], J[Y][Z],
				J[Z][X], J[Z][Y], J[Z][Z]	);
  Vector3 ev1, ev2, ev3, evalues;
  m3.SolveEigensystem (ev1, ev2, ev3, evalues);

  mrot[0] = ev3.x;
  mrot[1] = ev3.y;
  mrot[2] = ev3.z;
  if (mrot[0] < 0.0)
    {
      mrot[0] *= -1.0;
      mrot[1] *= -1.0;
      mrot[2] *= -1.0;
    }

  mrot[3] = ev2.x;
  mrot[4] = ev2.y;
  mrot[5] = ev2.z;
  if (mrot[4] < 0.0)
    {
      mrot[3] *= -1.0;
      mrot[4] *= -1.0;
      mrot[5] *= -1.0;
    }

  mrot[6] = ev1.x;
  mrot[7] = ev1.y;
  mrot[8] = ev1.z;
  if (mrot[8] < 0.0)
    {
      mrot[6] *= -1.0;
      mrot[7] *= -1.0;
      mrot[8] *= -1.0;
    }
}
