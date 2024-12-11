#include "color.h"

Color::Color ()
{
	SetRGBA (255, 255, 255, 255);
}

Color::Color (unsigned char r, unsigned char g, unsigned char b)
{
	SetRGBA (r, g, b, 255);
}

Color::~Color ()
{
}

void Color::SetRGB (unsigned char r, unsigned char g, unsigned char b)
{
	SetRGBA (r, g, b, 255);
}

void Color::SetRGBA (unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	c[0] = r;
	c[1] = g;
	c[2] = b;
	c[3] = a;
}


//
// Conversions
//
void Color::Int2RGBf (int c, RGBf &rgb)
{
	rgb.r = ((c >> 16) & 255) / 255;
	rgb.g = ((c >> 8) & 255) / 255;
	rgb.b = (c & 255) / 255;
}

void Color::Int2RGBc (int c, RGBc &rgb)
{
	rgb.r = ((c >> 16) & 255);
	rgb.g = ((c >> 8) & 255);
	rgb.b = (c & 255);
}

int Color::RGBc2Int (unsigned char r, unsigned char g, unsigned char b)
{
	return ((int)(r) << 16) | ((int)(g) << 8) | (int)(b);
}

int Color::RGBf2Int (float r, float g, float b)
{
	return ((int)(r * 255) << 16) | ((int)(g * 255) << 8) | (int)(b * 255);
}

int Color::RGBc2Int (RGBc rgb)
{
	return RGBc2Int(rgb.r, rgb.g, rgb.b);
}

int Color::RGBAc2Int (RGBAc rgba)
{
	return RGBc2Int(rgba.r, rgba.g, rgba.b);
}

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//		if s == 0, then h = -1 (undefined)
void Color::RGBftoHSV (RGBf rgb, HSVf hsv)
{
	float r = rgb.r;
	float g = rgb.g;
	float b = rgb.b;
	float h, s, v;

	float min, max, delta;
	min = MIN3( r, g, b );
	max = MAX3( r, g, b );
	v = max;				// v
	delta = max - min;
	if( max != 0 )
		s = delta / max;		// s
	else {
		// r = g = b = 0		// s = 0, v is undefined
		s = 0;
		h = -1;
		return;
	}
	if( r == max )
		h = ( g - b ) / delta;		// between yellow & magenta
	else if( g == max )
		h = 2 + ( b - r ) / delta;	// between cyan & yellow
	else
		h = 4 + ( r - g ) / delta;	// between magenta & cyan
	h *= 60;				// degrees
	if( h < 0 )
		h += 360;

	hsv.h = h;
	hsv.s = s;
	hsv.v = v;
}

void Color::HSVtoRGBf (HSVf hsv, RGBf rgb)
{
	float h = hsv.h;
	float s = hsv.s;
	float v = hsv.v;
	float r, g, b;

	int i;
	float f, p, q, t;
	if( s == 0 ) {
		// achromatic (grey)
		r = g = b = v;
		return;
	}
	h /= 60;			// sector 0 to 5
	i = floor( h );
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );
	switch( i ) {
		case 0: r = v; g = t; b = p; break;
		case 1: r = q; g = v; b = p; break;
		case 2: r = p; g = v; b = t; break;
		case 3: r = p; g = q; b = v; break;
		case 4: r = t; g = p; b = v; break;
		case 5: r = v; g = p; b = q; break;
	}
	rgb.r = r;
	rgb.g = g;
	rgb.b = b;
}

//
void Color::RGBftoHSL (RGBf rgb, HSLf hsl)
{
        float max = MAX3( rgb.r, rgb.g, rgb.b );
        float min = MIN3( rgb.r, rgb.g, rgb.b );
        float add = max + min;
        float sub = max - min;
        
        float h;
	if (max == min) {
		h = 0;
        } else if (max == rgb.r) {
		h = (int)(60 * (rgb.g - rgb.b) / sub + 360) % 360;
        } else if (max == rgb.g) {
		h = 60 * (rgb.b - rgb.r) / sub + 120;
        } else if (max == rgb.b) {
            h = 60 * (rgb.r - rgb.g) / sub + 240;
        }
        
        float l = add / 2.;
        
	float s;
        if (max == min) {
		s = 0;
        } else if (l <= .5) {
		s = sub / add;
        } else {
		s = sub / (2 - add);
        }

	hsl.h = h;
	hsl.s = s;
	hsl.l = l;
}

//
void Color::HSLtoRGBf (HSLf hsl, RGBf rgb)
{
	float q;
	if (hsl.l < 1 / 2) {
		q = hsl.l * (1 + hsl.s);
        } else {
		q = hsl.l + hsl.s - (hsl.l * hsl.s);
        }
        
        float p = 2 * hsl.l - q;
	float hk = ((int)hsl.h % 360) / 360;
	float tr = hk + 1 / 3;
        float tg = hk;
        float tb = hk - 1 / 3;
        
        float tc[3] = {tr,tg,tb};
	for (int n=0; n<3; n++)
        {
            float t = tc[n];
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
	    if (t < 1 / 6) {
                tc[n] = p + ((q - p) * 6 * t);
            } else if (t < 1 / 2) {
                tc[n] = q;
            } else if (t < 2 / 3) {
                tc[n] = p + ((q - p) * 6 * (2 / 3 - t));
            } else {
                tc[n] = p;
            }
        }
        
	rgb.r = tc[0];
	rgb.g = tc[1];
	rgb.b = tc[2];
}

void Color::HSLf2HSVf(HSLf hsl, HSVf hsv)
{
	RGBf rgb = {0, 0, 0};
	HSLtoRGBf (hsl, rgb);
	RGBftoHSV (rgb, hsv);
}
    
void Color::HSVf2HSLf(HSVf hsv, HSLf hsl)
{
	RGBf rgb = {0, 0, 0};
	HSVtoRGBf (hsv, rgb);
	RGBftoHSL (rgb, hsl);
}





void color_jet(float index, float *_r, float *_g, float *_b)
{
	const float ri[5] = {0.f, 0.35f, 0.66f, 0.89f, 1.f};
	const float rv[5] = {0.f, 0.f, 1.f, 1.f, 0.5f};
	const float gi[6] = {0.f, 0.125f, 0.375f, 0.64f, 0.91f, 1.f};
	const float gv[6] = {0.f, 0.f, 1.f, 1.f, 0.f, 0.f};
	const float bi[5] = {0.f, 0.11f, 0.34f, 0.65f, 1.f};
	const float bv[5] = {0.5f, 1.f, 1.f, 0.f, 0.f};

	float r, g, b;
	int j;
	
	// red
	for (j=1; j<5; j++) if (ri[j] > index) break;
	r = ((rv[j]-rv[j-1])*index+rv[j-1]*ri[j]-rv[j]*ri[j-1])/(ri[j]-ri[j-1]);
	
	// green
	for (j=1; j<5; j++) if (gi[j] > index) break;
	g = ((gv[j]-gv[j-1])*index+gv[j-1]*gi[j]-gv[j]*gi[j-1])/(gi[j]-gi[j-1]);
	
	// blue
	for (j=1; j<5; j++) if (bi[j] > index) break;
	b = ((bv[j]-bv[j-1])*index+bv[j-1]*bi[j]-bv[j]*bi[j-1])/(bi[j]-bi[j-1]);

	*_r = r;
	*_g = g;
	*_b = b;
}

void color_jet_int (float index, int *_r, int *_g, int *_b)
{
	float fr, fg, fb;
	color_jet (index, &fr, &fg, &fb);

	*_r = (int)(255.*fr);
	*_g = (int)(255.*fg);
	*_b = (int)(255.*fb);
}
