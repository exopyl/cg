#include <stdlib.h>

#include "image.h"
#include "image_drawing.h"
#include "../cgmath/TVector2.h"

//
// drawing
//
int ImgDraw::horizontal_line (Img& img, unsigned int y,
			       unsigned int xstart, unsigned int xend,
			       unsigned char r, unsigned int g, unsigned char b, unsigned char a)
{
	if (xstart > xend)
		return horizontal_line (img, y, xend, xstart, r, g, b, a);
	for (unsigned int x=xstart; x<=xend; x++)
		img.set_pixel (x, y, r, g, b, a);
	return 0;
}

int ImgDraw::line (Img& img, unsigned int xstart, unsigned int ystart, unsigned int xend, unsigned int yend,
		    unsigned char r, unsigned int g, unsigned char b, unsigned char a)
{
 	int w = (int) img.m_iWidth;
	int h = (int) img.m_iHeight;

	int dy = yend - ystart;
        int dx = xend - xstart;
        int stepx, stepy;

	int x0 = xstart;
	int y0 = ystart;
	int x1 = xend;
	int y1 = yend;

        if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else
		stepy = 1;
        if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else
		stepx = 1;
        dy <<= 1;
        dx <<= 1;

	img.set_pixel (x0, y0, r, g, b, a);
        if (dx > dy) {
            int fraction = dy - (dx >> 1);
            while (x0 != x1) {
		    if (fraction >= 0) {
			    y0 += stepy;
			    fraction -= dx;
		    }
		    x0 += stepx;
		    fraction += dy;
		    img.set_pixel (x0, y0, r, g, b, a);
           }
        } else {
		int fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			img.set_pixel (x0, y0, r, g, b, a);
		}
        }
		return 0;
}

// References :
//  http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/bresenham.html
//  http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
//
int ImgDraw::circle (Img& img, unsigned int x0, unsigned int y0, unsigned int radius,
		      unsigned char r, unsigned int g, unsigned char b, unsigned char a)
{
	int x,y,d,dE,dSE;

	x=0;
	y=radius;
	d=1-radius;
	dE=3;
	dSE=-2*radius+5;

	img.set_pixel (x0+x, y0+y, r, g, b, a);
	img.set_pixel (x0+x, y0-y, r, g, b, a);
	img.set_pixel (x0+y, y0+x, r, g, b, a);
	img.set_pixel (x0+y, y0-x, r, g, b, a);
	img.set_pixel (x0-x, y0+y, r, g, b, a);
	img.set_pixel (x0-x, y0-y, r, g, b, a);
	img.set_pixel (x0-y, y0+x, r, g, b, a);
	img.set_pixel (x0-y, y0-x, r, g, b, a);

	while(y>x)
	{
		if(d<0)
		{
			d+=dE;
			dE+=2;
			dSE+=2;
			x++;
		}
		else
		{
			d+=dSE;
			dE+=2;
			dSE+=4;
			x++;
			y--;
		}
		img.set_pixel (x0+x, y0+y, r, g, b, a);
		img.set_pixel (x0+x, y0-y, r, g, b, a);
		img.set_pixel (x0+y, y0+x, r, g, b, a);
		img.set_pixel (x0+y, y0-x, r, g, b, a);
		img.set_pixel (x0-x, y0+y, r, g, b, a);
		img.set_pixel (x0-x, y0-y, r, g, b, a);
		img.set_pixel (x0-y, y0+x, r, g, b, a);
		img.set_pixel (x0-y, y0-x, r, g, b, a);
	}

	return 0;
}

//  http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/bresenham.html
//  http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
int ImgDraw::disk (Img& img, unsigned int x0, unsigned int y0, unsigned int radius,
		    unsigned char r, unsigned int g, unsigned char b, unsigned char a)
{
	int x,y,d,dE,dSE;

	x=0;
	y=radius;
	d=1-radius;
	dE=3;
	dSE=-2*radius+5;

	horizontal_line (img, y0-x, x0+y, x0-y, r, g, b, a);
	while(y>x)
	{
		if(d<0)
		{
			d+=dE;
			dE+=2;
			dSE+=2;
			x++;
		}
		else
		{
			d+=dSE;
			dE+=2;
			dSE+=4;
			x++;
			y--;
		}
		horizontal_line (img, y0+y, x0+x, x0-x, r, g, b, a);
		horizontal_line (img, y0-y, x0+x, x0-x, r, g, b, a);
	        horizontal_line (img, y0+x, x0+y, x0-y, r, g, b, a);
		horizontal_line (img, y0-x, x0+y, x0-y, r, g, b, a);
	}
	return 0;
}

//
// (x-x0)^2/a^2 + (y-y0)^2/b^2 = 1
//
int ImgDraw::ellipse (Img& img, unsigned int x0, unsigned int y0,
		       unsigned int radiusx, unsigned int radiusy,
		       unsigned char r, unsigned int g, unsigned char b, unsigned char a)
{
	int radiusx2 = radiusx*radiusx;
	for (int x=0; x<=(int)radiusx; x++)
	{
		int y = radiusy * sqrt (1. - (float)x*x/radiusx2);
		img.set_pixel (x+x0, y+y0, r, g, b, a);

		// symmetric values
		img.set_pixel (x+x0, -y+y0, r, g, b, a);
		img.set_pixel (-x+x0, y+y0, r, g, b, a);
		img.set_pixel (-x+x0, -y+y0, r, g, b, a);
	}

	return 0;
}

// smooth the transitions between black and white
int ImgDraw::smooth_transition (Img& img, int l)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));
	memcpy (pPixels, img.m_pPixels, 4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));

	int n=img.m_iHeight*img.m_iWidth;
	for (unsigned int j=0; j<img.m_iHeight; j++)
		for (unsigned int i=0; i<img.m_iWidth; i++)
		{
			if (img.m_pPixels[4*(j*img.m_iWidth+i)] == 255) // current pixel is white
			{
				float d=l+1;
				Vector2f p ((float)i, (float)j);
				for (int kj=-l; kj<l; kj++)
					for (int ki=-l; ki<l; ki++)
					{
						int index = (j+kj)*img.m_iWidth+i+ki;
						if (index>=0 && index<n) // in the image
						{
							if (img.m_pPixels[4*index] == 0) // pixel is black
							{
								// get the distance
								Vector2f t ((float)(i+ki), (float)(j+kj));
								float dt = p.getDistance (t);
								if (d > dt)
									d = dt;
							}
						}
					}
				if (d<=l)
				{
					//unsigned char g = (unsigned char)(255*sin(3.14159*0.5*d/(float)l));
					unsigned char g = 255-(unsigned char)(255*sqrt(cos(3.14159*0.5*d/(float)l)));
					//unsigned char g = 255*(unsigned char)(1.-(l-d))/l;
					for (int m=0; m<3; m++)
						pPixels[4*(j*img.m_iWidth+i)+m] = g;
					pPixels[4*(j*img.m_iWidth+i)+3] = 255;
				}
			}
		}
	free (img.m_pPixels);
	img.m_pPixels = pPixels;

	return 0;
}

