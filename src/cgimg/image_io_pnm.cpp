#include "image.h"
#include "image_io.h"


//
// pnm
//

// import pbm
int ImgIO::import_pbm (Img& img, FILE *ptr, unsigned int levels, int binary)
{
	if (binary)
		for (unsigned int j=0; j<img.m_iHeight; j++)
			for (unsigned int i=0; i<img.m_iWidth; i+=8)
			{
				char c;
				fread (&c, sizeof(char), 1, ptr);
			        unsigned char mask = 128;
				for (char k=0; k<8; k++)
				{
					if (i+k >= img.m_iWidth)
						break;
					if ((c&mask)==0)
						img.set_pixel (i+k,(/*height-1-*/j),
								    255, 255, 255, 255);
					else
						img.set_pixel (i+k,(/*height-1-*/j),
								    0, 0, 0, 255);
					mask /= 2;
				}
			}
	else
		for (unsigned int j=0; j<img.m_iHeight; j++)
			for (unsigned int i=0; i<img.m_iWidth; i++)
			{
				char c;
				fscanf (ptr, "%c", &c);
				if (c == '\n')
					i--;
				else
				{
					if (c=='1')
						img.set_pixel (i,(/*height-1-*/j),
								    0, 0, 0, 255);
					else
						img.set_pixel (i,(/*height-1-*/j),
								    255, 255, 255, 255);
				}
			}

	return 0;
}

int ImgIO::import_pgm (Img& img, FILE *ptr, unsigned int levels, int binary)
{
	if (binary)
		for (unsigned int j=0; j<img.m_iHeight; j++)
			for (unsigned int i=0; i<img.m_iWidth; i++)
			{
				char c;
				fread (&c, sizeof(char), 1, ptr);
				unsigned char gray = (unsigned char)(255.*(float)c/(float)levels);
				img.set_pixel (i, /*height-1-*/j,
						    gray, gray, gray, 255);
				//float gray = (float)c/(float)levels;
				//pixel[width*(height-1-j)+i] = color_float_rgba (gray, gray, gray, 1.);
			}
	else
		for (unsigned int j=0; j<img.m_iHeight; j++)
			for (unsigned int i=0; i<img.m_iWidth; i++)
			{
				unsigned int c;
				fscanf (ptr, "%d", &c);
				char gray = 255*(float)c/(float)levels;
				img.set_pixel (i, /*height-1-*/j, gray, gray, gray, 255);
				//float gray = (float)c/(float)levels;
				//pixel[width*(height-1-j)+i] = color_float_rgba (gray, gray, gray, 1.);
			}


	return 0;
}

int ImgIO::import_ppm (Img& img, FILE *ptr, unsigned int levels, int binary)
{
	if (binary)
	{
		for (unsigned int j=0; j<img.m_iHeight; j++)
			for (unsigned int i=0; i<img.m_iWidth; i++)
			{
				char c[3];
				fread (c, sizeof(char), 3, ptr);
				float r = (float)c[0]/(float)levels;
				float g = (float)c[1]/(float)levels;
				float b = (float)c[2]/(float)levels;
				img.set_pixel (i, /*height-1-*/j, 255*r, 255*g, 255*b, 255);
			}
	}
	else
	{
		for (unsigned int j=0; j<img.m_iHeight; j++)
			for (unsigned int i=0; i<img.m_iWidth; i++)
			{
				unsigned int c[3];
				fscanf (ptr, "%d %d %d", &c[0], &c[1], &c[2]);
				float r = (float)c[0]/(float)levels;
				float g = (float)c[1]/(float)levels;
				float b = (float)c[2]/(float)levels;
				img.set_pixel (i, /*height-1-*/j, 255*r, 255*g, 255*b, 255);
			}
	}

	return 0;
}

int ImgIO::import_pnm (Img& img, const char *filename)
{
	FILE *ptr = fopen (filename, "r");
	if (ptr == nullptr)
		return -1;

	char header[2];
	fscanf (ptr, "%c%c", &header[0], &header[1]);
	if (header[0] != 'P')
	{
		fclose (ptr);
		return -1;
	}

	char buffer[256];
	unsigned int width, height, levels;
	do {
		fgets (buffer, 256, ptr);
	} while (buffer[0] == '#' || buffer[0] == '\n');
	sscanf (buffer, "%d %d\n", &width, &height);
	fscanf (ptr, "%d\n", &levels);

	if (img.resize_memory (width, height) == -1)
		return -1;

	switch (header[1])
	{
	case '1':
		import_pbm (img, ptr, levels, 0);
		break;
	case '4':
		import_pbm (img, ptr, levels, 1);
		break;
	case '2':
		import_pgm (img, ptr, levels, 0);
		break;
	case '5':
		import_pgm (img, ptr, levels, 1);
		break;
	case '3':
		import_ppm (img, ptr,levels,  0);
		break;
	case '6':
		import_ppm (img, ptr, levels, 1);
		break;
	default:
		break;
	}

	fclose (ptr);

	return 0;
}

//
//
//

int ImgIO::export_ppm (Img& img, const char *filename, int binary)
{
	unsigned int w = img.m_iWidth;
	unsigned int h = img.m_iHeight;

	unsigned char c[4];
	if (binary)
	{
		FILE *ptr = fopen (filename, "w+b");
		if (ptr == nullptr)
			return -1;

		fprintf (ptr, "P6\n%d %d\n%d\n", w, h, 255);
		for (unsigned int j=0; j<h; j++)
			for (unsigned int i=0; i<w; i++)
			{
				img.get_pixel (i, j, &c[0], &c[1], &c[2], &c[3]);
				fwrite (c, sizeof(char), 3, ptr);
			}

		fclose (ptr);
	}
	else
	{
		FILE *ptr = fopen (filename, "w");
		if (ptr == nullptr)
			return -1;

		fprintf (ptr, "P3\n%d %d\n%d\n", w, h, 255);
		for (unsigned int j=0; j<h; j++)
			for (unsigned int i=0; i<w; i++)
			{
				img.get_pixel (i, j, &c[0], &c[1], &c[2], &c[3]);
				fprintf (ptr, "%d %d %d\n", c[0], c[1], c[2]);
			}
		fclose (ptr);
	}

	return 0;
}

int ImgIO::export_pnm (Img& img, const char *filename)
{
	return export_ppm (img, filename, 0);
}
