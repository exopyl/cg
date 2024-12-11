#include <stdio.h>

#include "ray.h"

void Ray::Dump (void)
{
	printf ("origin : %f %f %f\n", origin[0], origin[1], origin[2]);
	printf ("direction : %f %f %f\n", direction[0], direction[1], direction[2]);
}
