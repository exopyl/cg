#include <string.h>

#include "algebra_matrix2.h"
#include "common.h"

int mat2_init (mat2 m,
	       float m00, float m01,
	       float m10, float m11)
{
	m[0][0] = m00; m[0][1] = m01;
	m[1][0] = m10; m[1][1] = m11;
	return 0;
}

int mat2_transform (vec2 res, mat2 m, vec2 v)
{
	vec2 t;

	vec2_init (t,
		   m[0][0] * v[0] + m[0][1] * v[1],
		   m[1][0] * v[0] + m[1][1] * v[1]);

	memcpy(res, t, sizeof(vec2));
	return 0;
}

void mat2_copy (mat2 dst, mat2 src)
{
     dst[0][0] = src[0][0]; 
     dst[0][1] = src[0][1]; 
     dst[1][0] = src[1][0]; 
     dst[1][1] = src[1][1]; 
}

//
// Solve the eigensystem based on the matrix 2x2
//
// The vector evalues contains the 2 eigenvalues.
// The eigenvectors associated to the eigenvalues are stored in the
// vectors evector1 and evector2.
//
int mat2_solve_eigensystem (mat2 m, vec2 evector1, vec2 evector2, vec2 evalues)
{
	float a = m[0][0];
	float b = m[0][1];
	float c = m[1][0];
	float d = m[1][1];
	if (b == 0.0 && c == 0.0)
	{
		vec2_init (evalues, a, d);
		vec2_init (evector1, 1., 0.);
		vec2_init (evector2, 0., 1.);
		return 1;
	}
	float delta = (a+d)*(a+d) - 4*(a*d-b*c);
	if (delta < 0.0)
		return 0;

	vec2_init (evalues, ((a+d) + sqrt(delta)) / 2.0, ((a+d) - sqrt(delta)) / 2.0);

	if (b == 0.0)
	{
		vec2_init (evector1, 1.0, -c / (d - evalues[0]));
		vec2_init (evector2, 1.0, -c / (d - evalues[1]));
	}
	else
	{
		vec2_init (evector1, 1.0, (evalues[0] - a) / b);
		vec2_init (evector2, 1.0, (evalues[1] - a) / b);
	}

	vec2_normalize (evector1);
	vec2_normalize (evector2);

	return 1;
}

float mat2_determinant (mat2 m)
{
	return  m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

int fmat2_inverse (mat2 m)
{
	float fdet, finvdet;
	fdet = mat2_determinant(m);

	if (fdet == 0.f)	// Impossible to inverse the matrix
		return -1;

	finvdet = 1.f / fdet; // To avoid multiple division

	mat2 tmp;
	mat2_init (tmp,
		    finvdet*m[1][1], -finvdet*m[0][1],
		    -finvdet*m[1][0], finvdet*m[0][0]);
	
	mat2_copy (m, tmp);

	return 0;
}

void fmat2_dump (mat2 m)
{
	for (unsigned int i=0; i<2; i++)
	{
		for (unsigned int j=0; j<2; j++)
		     printf ("%.3f ", m[i][j]);
		printf ("\n");
	}
}

