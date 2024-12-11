#include <stdio.h>
#include <math.h>

#include "fractal.h"


//
// mandelbrot
//
int inside_mandelbrot (int iteration_max, float x0, float y0, unsigned char c[3])
{
	float x = x0, y = y0;
	int iteration = 0;
	while ( x*x + y*y < 2*2  &&  iteration < iteration_max )
	{
		float xtemp = x*x - y*y + x0;
		y = 2*x*y + y0;
		x = xtemp;
		iteration++;
	}

	return iteration;
}

// domain : [-2.5 , 1] x [-1, 1]
int draw_mandelbrot (int iteration_max, float xmin, float xmax, float ymin, float ymax, float step)
{
	int width = (xmax-xmin)/step;
	int height = (ymax-ymin)/step;
	FILE *ptr = fopen ("mandelbrot.ppm", "w");
	fprintf (ptr, "P3\n%d %d\n255\n", width, height);
	unsigned char c[3];
	for (int j=0; j<height; j++)
		for (int i=0; i<width; i++)
		{
			int iteration = inside_mandelbrot (iteration_max, xmin+step*i, ymin+step*j, c);
			unsigned char level = (unsigned char)(255.*((float)iteration / (float)iteration_max));
			fprintf (ptr, "%d %d %d\n", level, level, level);
		}
	fclose (ptr);
	
	return 0;
}


//
// mandelbulb
//
int inside_mandelbulb (int max_iteration, float x0, float y0, float z0)
{
	float x = x0, y = y0, z = z0;
	int iteration = 0;
	
	int n=8;
	while ( x*x + y*y + z*z < 2  &&  iteration < max_iteration )
	{
		float pi=3.14159265;
		float r    = sqrt(x*x + y*y + z*z );
		float yang = atan2(sqrt(x*x + y*y) , z  );
		float zang = atan2(y , x);

		int n = 8;
		float r8 = r*r*r*r*r*r*r*r;
		float newx = (r8) * sin( yang*n ) * cos(zang*n);
		float newy = (r8) * sin( yang*n ) * sin(zang*n);
		float newz = (r8) * cos( yang*n );
		x = newx + x0;
		y = newy + y0;
		z = newz + z0;

		iteration++;
	}

	return iteration;
}

// http://www.skytopia.com/project/fractal/2mandelbulb.html
// http://www.iquilezles.org/www/articles/mandelbulb/mandelbulb.htm
// http://www.bugman123.com/Hypercomplex/index.html
// domain : [-1 , 1] x [-1 , 1] x [-1 , 1]
int draw_mandelbulb (float xmin, float xmax, float ymin, float ymax, float zmin, float zmax, float step)
{
	int iteration_max = 10;
	int dimx = (xmax-xmin)/step;
	int dimy = (ymax-ymin)/step;
	int dimz = (zmax-zmin)/step;
	//printf ("%d x %d x %d\n", dimx, dimy, dimz);
	FILE *ptr = fopen ("mandelbulb.obj", "w");
	printf ("var data = [\n");
	for (int k=0; k<dimz; k++)
	{
		printf ("[\n");
		for (int j=0; j<dimy; j++)
		{
			printf ("[\n");
			for (int i=0; i<dimx; i++)
			{
				int iteration = inside_mandelbulb (iteration_max, xmin+step*i, ymin+step*j, zmin+step*k);
				if (!(i==0 && j==0))
					printf (",");
				if (iteration != 0)
					fprintf (ptr, "v %f %f %f\n", (float)i, (float)j, (float)k);
			}
			printf ("]");
			if (j!=dimy-1)
				printf (",");
			printf("\n");
		}
		printf ("]");
		if (k!=dimz-1)
			printf (",");
		printf("\n");
	}
	printf ("];\n");
	fclose (ptr);
	
	return 0;
}
