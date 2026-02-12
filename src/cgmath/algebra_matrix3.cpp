#include "common.h"
#include "algebra_matrix3.h"
#include "algebra_quaternion.h"

int mat3_init (mat3 m,
	       float m00, float m01, float m02,
	       float m10, float m11, float m12,
	       float m20, float m21, float m22)
{
	m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
	m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
	m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
	return 0;
}

int mat3_init_array (mat3 m, float *array)
{
	m[0][0] = array[0]; m[0][1] = array[1]; m[0][2] = array[2];
	m[1][0] = array[3]; m[1][1] = array[4]; m[1][2] = array[5];
	m[2][0] = array[6]; m[2][1] = array[7]; m[2][2] = array[8];

	return 1;
}

int mat3_init_identity (mat3 m)
{
	for (unsigned int i=0; i<3; i++)
		for (unsigned int j=0; j<3; j++)
			m[i][j] = 0.;
	for (unsigned int i=0; i<3; i++)
		m[i][i] = 1.;
	return 0;
}

void mat3_transpose(mat3 m)
{
	SWAP(m[0][1], m[1][0]);
	SWAP(m[0][2], m[2][0]);
	SWAP(m[1][2], m[2][1]);
}

int mat3_init_rotation_from_vec3_to_vec3 (mat3 m, vec3 vDir1, vec3 vDir2)
{
	vec3_normalize (vDir1);
	vec3_normalize (vDir2);
	vec3 axis;
	vec3_cross_product (axis, vDir1, vDir2);
	if (fabs (vec3_length (axis)) < 1e-5)
	{
		mat3_init_identity (m);
		if (vec3_dot_product (vDir1, vDir2) == -1.0)
		{
			m[1][1] = -1.;
			m[2][2] = -1.;
		}
	}
	else
	{
		quaternion q;
		vec3_normalize (axis);
		float cosangle = vec3_dot_product (vDir1, vDir2);
		if (cosangle > 1.)
			cosangle = 1.;
		if (cosangle < -1.)
			cosangle = -1.;
		float angle = acos (cosangle);
		quaternion_init_axis_angle (q, axis, angle);
		quaternion_normalize (q);
		quaternion_convert_to_matrix3 (q, m);
	}
	return 0;
}

//
// Reference : http://www.flipcode.com/documents/matrfaq.html#Q36
//
int mat3_init_rotation_from_euler_angles (mat3 m, float rotx, float roty, float rotz)
{
    float A = cos(rotx);
    float B = sin(rotx);
    float C = cos(roty);
    float D = sin(roty);
    float E = cos(rotz);
    float F = sin(rotz);

    float AD = A * D;
    float BD = B * D;

    m[0][0] =  C * E;
    m[0][1] = -C * F;
    m[0][2] = -D;
    m[1][0] = -BD * E + A * F;
    m[1][1] =  BD * F + A * E;
    m[1][2] =  -B * C;
    m[2][0] =  AD * E + B * F;
    m[2][1] = -AD * F + B * E;
    m[2][2] =   A * C;

    return 0;
}					  

void mat3_transform(vec3 r, mat3 m, vec3 v)
{
	vec3 t;

	t[0] = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2];
	t[1] = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2];
	t[2] = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2];

	memcpy(r, t, sizeof(vec3));
}

void mat3_dump (mat3 m)
{
	for (unsigned int i=0; i<3; i++)
	{
		for (unsigned int j=0; j<3; j++)
		{
			printf ("%.3f ", m[i][j]);
		}
		printf ("\n");
	}
}

float mat3_determinant (mat3 m)
{
	return  m[0][0] * (m[1][1]*m[2][2] - m[2][1]*m[1][2]) -
		m[1][0] * (m[0][1]*m[2][2] - m[2][1]*m[0][2]) +
		m[2][0] * (m[0][1]*m[1][2] - m[1][1]*m[0][2]);
}

int mat3_inverse (mat3 m)
{
	float fdet, finvdet;

	fdet = mat3_determinant(m);
	
	if (fdet == 0.f)	//Impossible to inverse the matrix
		return -1;
	
	//To avoid multiple division
	finvdet = 1.f / fdet;

	mat3 tmp;
	tmp[0][0] = m[2][2]*m[1][1] - m[2][1]*m[1][2];
	tmp[0][1] = m[2][1]*m[0][2] - m[2][2]*m[0][1];
	tmp[0][2] = m[1][2]*m[0][1] - m[1][1]*m[0][2];
	tmp[1][0] = m[2][0]*m[1][2] - m[2][2]*m[1][0];
	tmp[1][1] = m[2][2]*m[0][0] - m[2][0]*m[0][2];
	tmp[1][2] = m[1][0]*m[0][2] - m[1][2]*m[0][0];
	tmp[2][0] = m[2][1]*m[1][0] - m[2][0]*m[1][1];
	tmp[2][1] = m[2][0]*m[0][1] - m[2][1]*m[0][0];
	tmp[2][2] = m[1][1]*m[0][0] - m[1][0]*m[0][1];
	
	for (unsigned int i=0; i<3; i++)
		for (unsigned int j=0; j<3; j++)
			m[i][j] = tmp[i][j] * finvdet;

	return 0;
}

void mat3_mul(mat3 r, mat3 a, mat3 b)
{
	int i, j;
	mat3 t;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			t[i][j] = a[i][0] * b[0][j] +
				  a[i][1] * b[1][j] +
				  a[i][2] * b[2][j];
		}
	memcpy(r, t, sizeof(mat3));
}

void mat3_scale(mat3 res, mat3 src, float s)
{
	int i,j;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
				res[i][j] = s * src[i][j];
}

int mat3_is_symmetric (mat3 m)
{
	if (m[0][1] == m[1][0] &&
	    m[0][2] == m[2][0] &&
	    m[1][2] == m[2][1] )
		return 1;
	else
		return 0;
}

//
//
//
static void mat3_tridiagonal (mat3 m, vec3 diag, vec3 subd)
{
  float fm00 = m[0][0];
  float fm01 = m[0][1];
  float fm02 = m[0][2];
  float fm11 = m[1][1];
  float fm12 = m[1][2];
  float fm22 = m[2][2];
  
  diag[0] = fm00;
  subd[2] = 0.0;
  if (fm02 != 0.0)
  {
      float length = sqrt (fm01*fm01 + fm02*fm02);
      float invlength = 1.0 / length;
      float q;
      fm01 *= invlength;
      fm02 *= invlength;
      q = 2.0*fm01*fm12 + fm02*(fm22-fm11);
      diag[1] = fm11+fm02*q;
      diag[2] = fm22-fm02*q;
      subd[0] = length;
      subd[1] = fm12-fm01*q;
      mat3_init ( m,
		   1.0, 0.0, 0.0,
		   0.0, fm01, fm02,
		   0.0, fm02, -fm01 );
  }
  else
  {
      diag[1] = fm11;
      diag[2] = fm22;
      subd[0] = fm01;
      subd[1] = fm12;
      mat3_init_identity (m);
  }
}

//
//
//
static int mat3_QLalgorithm (mat3 m, vec3 diag, vec3 subd)
{
	int imaxiter = 32;
	int i0, i1=0, i2, i3, i4;
	
	for (i0=0; i0<3; i0++)
	{
		float G, R;
		float sinus, cosinus, P;
		
		for (i2=i0; i2<=1; i2++)
		{
			float tmp = fabs (diag[i2]) + fabs(diag[i2+1]);
			if (fabs(subd[i2])+tmp == tmp)
				break;
		}
		if (i2 == i0)
			break;
		
		G = (diag[i0+1] - diag[i0]) / (2.0*subd[i0]);
		R = sqrt (G*G+1.0);
		if (G < 0.0)
			G = diag[i2] - diag[i0] + subd[i0] / (G - R);
		else
			G = diag[i2] - diag[i0] + subd[i0] / (G + R);
		
		sinus = 1.0;
		cosinus = 1.0;
		P = 0.0;
		
		for (i3=i2-1; i3>=i0; i3--)
		{
			float F = sinus*subd[i3];
			float B = cosinus*subd[i3];
			if (fabs(F) >= fabs(G))
			{
				cosinus = G/F;
				if (fabs(F) < 0.00001)
				     cosinus = -1.;
				R = sqrt(cosinus*cosinus + 1.0);
				subd[i3+1] = F*R;
				sinus = 1.0/R;
				cosinus *= sinus;
			}
			else
			{
				sinus = F/G;
				R = sqrt (sinus*sinus + 1.0);
				subd[i3+1]=G*R;
				cosinus = 1.0/R;
				sinus *= cosinus;
			}
			G = diag[i3+1] - P;
			R = (diag[i3] - G)*sinus + 2.0*B*cosinus;
			P = sinus*R;
			diag[i3+1] = G + P;
			G = cosinus*R - B;
			
			for (i4=0; i4<3; i4++)
			{
				F = m[i4][i3+1];
				m[i4][i3+1] = sinus*m[i4][i3] + cosinus*F;
				m[i4][i3] = cosinus*m[i4][i3] - sinus*F;
			}
		}
		diag[i0] -= P;
		subd[i0] = G;
		subd[i2] = 0.0;
	}
	if (i1 == imaxiter)
		return -1;
	
	return 0;
}

//
//
//
static void mat3_decreasingsort (mat3 m, vec3 diag)
{
  int i0, i1, i2;

  for (i0=0; i0<=1; i0++)
    {
      float max;
      i1 = i0;
      max = diag[i1];
      for (i2=i0+1; i2<3; i2++)
	{
	  if (diag[i2] > max)
	    {
	      i1 = i2;
	      max = diag[i1];
	    }
	}
      
      if (i1 != i0)
	{
	  diag[i1] = diag[i0];
	  diag[i0] = max;

	  for (i2=0; i2<3; i2++)
	    {
	      float tmp = m[i2][i0];
	      m[i2][i0] = m[i2][i1];
	      m[i2][i1] = tmp;
	    }
	}
    }
}

//
// Solve the eigensystem based on the matrix.
//
// The steps followed by the algorithm are :
// tridiagonal, QLalgorithm, decreasingsort.
//
// returns 0 if the eigensystem is solved, -1 if the eigensystem can't be solved
//
// The vector evalues contains the 3 eigenvalues.
// The eigenvectors associated to the eigenvalues are stored in the
// vectors evector1, evector2 and evector3.
//
int mat3_solve_eigensystem (mat3 m,
			     vec3 evalues,
			     vec3 evector1,
			     vec3 evector2,
			     vec3 evector3)
{
	if (mat3_determinant(m) == 0.)
	{
		return -1;
	}
	
	if (mat3_is_symmetric (m))
	{
		vec3 diag = {0.0, 0.0, 0.0};
		vec3 subd = {0.0, 0.0, 0.0};
		vec3_init (diag, 0., 0., 0.);
		vec3_init (subd, 0., 0., 0.);

		mat3_tridiagonal    (m, diag, subd);
		mat3_QLalgorithm    (m, diag, subd);
		mat3_decreasingsort (m, diag);
		
		vec3_init (evector1, m[0][0], m[1][0], m[2][0]);
		vec3_init (evector2, m[0][1], m[1][1], m[2][1]);
		vec3_init (evector3, m[0][2], m[1][2], m[2][2]);
		vec3_init (evalues, diag[0], diag[1], diag[2]);
		return 0;
	}
	else
	{
		return -1;
	}
	return -1;
}


//
// solve a linear system based on the matrix m and the right element right
// the result is given in res
//
// return :
// 0 if ko
// 1 if ok
//
int mat3_solve_linearsystem (mat3 m, vec3 right, vec3 res)
{
	float det = mat3_determinant(m);
	if (det == 0.0f)
		return 0;

	det = 1.0f / det;

	// Cramer's rule
	mat3 tmp;
	mat3_init (tmp,
		    right[0], m[0][1], m[0][2],
		    right[1], m[1][1], m[1][2],
		    right[2], m[2][1], m[2][2]);
	float resultx = mat3_determinant(tmp) * det;
	
	mat3_init (tmp,
		    m[0][0], right[0], m[0][2],
		    m[1][0], right[1], m[1][2],
		    m[2][0], right[2], m[2][2]);
	float resulty = mat3_determinant(tmp) * det;
	
	mat3_init (tmp,
		    m[0][0], m[0][1], right[0],
		    m[1][0], m[1][1], right[1],
		    m[2][0], m[2][1], right[2]);
	float resultz = mat3_determinant(tmp) * det;
	
	vec3_init (res, resultx, resulty, resultz);
	
	return 1;
}
