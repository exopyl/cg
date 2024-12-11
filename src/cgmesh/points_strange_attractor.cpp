#include <stdlib.h>
#include <stdio.h>

#include "points_strange_attractor.h"

void StrangeAttractor::set_origin (double xorig, double yorig, double zorig)
{
	x = xorig;// = 0.0;
	y = yorig;// = 0.0;
	z = zorig;// = 0.0;
}

/**
* Returns a parameter chosen randomly between -4.5 and 5.0
*/
double StrangeAttractor::random_parameter (void)
{
	double haz = (double)rand() / (double)RAND_MAX;
	return 9.5 * haz - 4.5;
}

char StrangeAttractor::random_signature (void)
{
	return 'a';
}

double StrangeAttractor::convert (char c)
{
	switch (c)
	{
/*	case '': return -4.5; break;
	case '': return -4.4; break;
	case '': return -4.3; break;
	case '': return -4.2; break;
	case '': return -4.1; break;
	case '': return -4.0; break;
	case '': return -3.9; break;
	case '': return -3.8; break;
	case '': return -3.7; break;
	case '': return -3.6; break;
	case '': return -3.5; break;
	case '': return -3.4; break;
	case '': return -3.3; break;
	case '': return -3.2; break;
	case '': return -3.1; break;
	case '': return -3.0; break;*/
	case '0': return -2.9; break;
	case '1': return -2.8; break;
	case '2': return -2.7; break;
	case '3': return -2.6; break;
	case '4': return -2.5; break;
	case '5': return -2.4; break;
	case '6': return -2.3; break;
	case '7': return -2.2; break;
	case '8': return -2.1; break;
	case '9': return -2.0; break;
/*	case '': return -1.9; break;
	case '': return -1.8; break;
	case '': return -1.7; break;
	case '': return -1.6; break;
	case '': return -1.5; break;
	case '': return -1.4; break;*/
	case '#': return -1.3; break;
	case 'A': return -1.2; break;
	case 'B': return -1.1; break;
	case 'C': return -1.0; break;
	case 'D': return -0.9; break;
	case 'E': return -0.8; break;
	case 'F': return -0.7; break;
	case 'G': return -0.6; break;
	case 'H': return -0.5; break;
	case 'I': return -0.4; break;
	case 'J': return -0.3; break;
	case 'K': return -0.2; break;
	case 'L': return -0.1; break;
	case 'M': return  0.0; break;
	case 'N': return  0.1; break;
	case 'O': return  0.2; break;
	case 'P': return  0.3; break;
	case 'Q': return  0.4; break;
	case 'R': return  0.5; break;
	case 'S': return  0.6; break;
	case 'T': return  0.7; break;
	case 'U': return  0.8; break;
	case 'V': return  0.9; break;
	case 'W': return  1.0; break;
	case 'X': return  1.1; break;
	case 'Y': return  1.2; break;
	case 'Z': return  1.3; break;
/*	case '': return  1.4; break;
	case '': return  1.5; break;
	case '': return  1.6; break;
	case '': return  1.7; break;
	case '': return  1.8; break;
	case '': return  1.9; break;*/
	case 'a': return  2.0; break;
	case 'b': return  2.1; break;
	case 'c': return  2.2; break;
	case 'd': return  2.3; break;
	case 'e': return  2.4; break;
	case 'f': return  2.5; break;
	case 'g': return  2.6; break;
	case 'h': return  2.7; break;
	case 'i': return  2.8; break;
	case 'j': return  2.9; break;
	case 'k': return  3.0; break;
	case 'l': return  3.1; break;
	case 'm': return  3.2; break;
	case 'n': return  3.3; break;
	case 'o': return  3.4; break;
	case 'p': return  3.5; break;
	case 'q': return  3.6; break;
	case 'r': return  3.7; break;
	case 's': return  3.8; break;
	case 't': return  3.9; break;
	case 'u': return  4.0; break;
	case 'v': return  4.1; break;
	case 'w': return  4.2; break;
	case 'x': return  4.3; break;
	case 'y': return  4.4; break;
	case 'z': return  4.5; break;
/*	case '': return  4.6; break;
	case '': return  4.7; break;
	case '': return  4.8; break;
	case '': return  4.9; break;
	case '': return  5.0; break;*/
	default: break;
	}
}
	
void StrangeAttractor::export_obj (char *filename, unsigned int npts)
{
	  FILE *ptr = fopen (filename, "w");
	  for (int i=0; i<npts; i++)
	  {
		  next ();
		  fprintf (ptr, "v %f %f %f\n", getX (), getY (), getZ ());
	  }
	  fclose (ptr);
}

void StrangeAttractor::export_asc (char *filename, unsigned int npts)
{
	  FILE *ptr = fopen (filename, "w");
	  for (int i=0; i<npts; i++)
	  {
		  next ();
		  fprintf (ptr, "%f %f %f %d %d %d %f %f %f\n",
			   getX (), getY (), getZ (),
			   (int)(255.*(4.+getX ())/8.), (int)(255.*(4.+getY ())/8.), (int)(255.*(8.+getZ ())/12.),
			   0., 0., 0.);
	  }
	  fclose (ptr);
}

