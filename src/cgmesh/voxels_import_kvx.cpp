#include "voxels_import_kvx.h"

#include <stdio.h>
#include <stdlib.h>

#define MAXXSIZ 256 //Default values
#define MAXYSIZ 256
#define LIMZSIZ 255
#define BUFZSIZ 256
#define MAXVOXS 1048576

Voxels* loadkvx (char *filename)
{
	long numbytes, xsiz, ysiz, zsiz, xstart[MAXXSIZ+1];
	float xpiv, ypiv, zpiv;
	unsigned short xyoffs[MAXXSIZ][MAXYSIZ+1];
	char fipalette[768];
	long numvoxs; //sum of xlens and number of surface voxels
	unsigned short xlen[MAXXSIZ];
	char ylen[MAXXSIZ][MAXYSIZ];
	//voxtype voxdata[MAXVOXS];
	
	long i, j, k, x, y, z, z1, z2, fidatpos;
	char header[3];
	FILE *fil;

	if (!(fil = fopen(filename,"rb")))
	{
		printf ("can't open %s\n", filename);
		return NULL;
	}

	//Load KVX file data into memory
	fread(&numbytes,4,1,fil);
	printf ("%d\n", numbytes);
	fread(&xsiz,4,1,fil);
	fread(&ysiz,4,1,fil);
	fread(&zsiz,4,1,fil);
	printf ("%d %d %d\n", xsiz, ysiz, zsiz);
	Voxels *pVoxels = new Voxels (xsiz, ysiz, zsiz);
	if ((xsiz > MAXXSIZ) || (ysiz > MAXYSIZ) || (zsiz > LIMZSIZ))
	{
		fclose(fil);
		return 0;
	}
	fread(&i,4,1,fil); xpiv = ((float)i)/256.0;
	fread(&i,4,1,fil); ypiv = ((float)i)/256.0;
	fread(&i,4,1,fil); zpiv = ((float)i)/256.0;
	printf ("%f %f %f\n", xpiv, ypiv, zpiv);
	fread(xstart,(xsiz+1)<<2,1,fil);
	for(i=0;i<xsiz;i++)
		fread(&xyoffs[i][0],(ysiz+1)<<1,1,fil);

	unsigned char voxdata[40000];
	printf ("%d\n", numbytes-24-(xsiz+1)*4-xsiz*(ysiz+1)*2);

	for(x=0;x<xsiz;x++) //Set all surface voxels to 0
		for(y=0;y<ysiz;y++)
		{
			i = xyoffs[x][y+1] - xyoffs[x][y]; if (!i) continue;
			//j = (x*MAXYSIZ+y)*BUFZSIZ;
			//printf ("%d %d\n", i, j);
			while (i)
			{
				fread(header,3,1,fil);
				z1 = (long)header[0];
				k = (long)header[1];
				i -= k+3;
				z2 = z1+k;
				//printf ("%d %d => %d\n", z1, z2, k);
				for (unsigned int j=0; j<k; j++)
				{
					pVoxels->activate (x, y, z1+j);
					unsigned char c;
					fread (&c, 1, 1, fil);
					pVoxels->set_label (x, y, z1+j, (unsigned int)c);
				}

				//fseek(fil,z2-z1,SEEK_CUR);
				//setzrange0(j,z1,z2);
				//for(z=z1;z<z2;z++) vbit[(j+z)>>5] &= ~(1<<(j+z));
			}
		}

	fidatpos = ftell(fil);
	fseek(fil,-768,SEEK_END);
	fread(fipalette,768,1,fil);

	FILE *ptr = fopen ("toto_pixelart.mtl", "w");
	for (unsigned int i=0; i<256; i++)
		{
			fprintf (ptr, "newmtl material_%d\n", i);
			fprintf (ptr, "Ns 0.000000\n");
			fprintf (ptr, "Ka 0.000000 0.000000 0.000000\n");
			fprintf (ptr, "Kd %f %f %f\n", fipalette[3*i]/255., fipalette[3*i+1]/255., fipalette[3*i+2]/255.);
			fprintf (ptr, "Ks 0.000000 0.000000 0.000000\n");
			fprintf (ptr, "Ni 1.000000\n");
			fprintf (ptr, "d 1.000000\n");
			fprintf (ptr, "illum 2\n");
			fprintf (ptr, "\n");

		}
	fclose (ptr);

	return pVoxels;
}
