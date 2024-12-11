#ifndef __COLOR_H__
#define __COLOR_H__

#include "../cgmath/common.h"

typedef struct RGBc  { unsigned char r, g, b; }    RGBc;
typedef struct RGBf  { float r, g, b; }            RGBf;
typedef struct RGBAc { unsigned char r, g, b, a; } RGBAc;
typedef struct RGBAf { float r, g, b, a; }         RGBAf;
typedef struct HSVf  { float h, s, v; }            HSVf;
typedef struct HSLf  { float h, s, l; }            HSLf;
// h = [0,360], s = [0,1], v = [0,1]
//		if s == 0, then h = -1 (undefined)

class Color
{
public:
	Color ();
	Color (unsigned char r, unsigned char g, unsigned char b);
	Color(const Color& color) { *this = color; };
	~Color ();

	void SetRGB (unsigned char r, unsigned char g, unsigned char b);
	void SetRGBA (unsigned char r, unsigned char g, unsigned char b, unsigned char a);

	inline unsigned char operator[](unsigned int i) { return (i<4)? c[i]:0; };
	inline unsigned char r() { return c[0]; };
	inline unsigned char g() { return c[1]; };
	inline unsigned char b() { return c[2]; };
	inline unsigned char a() { return c[3]; };

	//
	// Conversions as services
	// References : http://www.cs.rit.edu/~ncs/color/t_convert.html
	//
	static void Int2RGBf  (int c, RGBf &rgb);
	static void Int2RGBc  (int c, RGBc &rgb);
	static int  RGBc2Int  (unsigned char r, unsigned char g, unsigned char b);
	static int  RGBf2Int  (float r, float g, float b);
	static int  RGBf2Int  (RGBf rgb);
	static int  RGBc2Int  (RGBc rgb);
	static int  RGBAc2Int (RGBAc rgba);
	static void RGBftoHSV (RGBf rgb, HSVf hsv);
	static void HSVtoRGBf (HSVf hsv, RGBf rgb);
	static void RGBftoHSL (RGBf rgb, HSLf hsl);
	static void HSLtoRGBf (HSLf hsl, RGBf rgb);
	static void HSLf2HSVf (HSLf hsl, HSVf hsv);
	static void HSVf2HSLf (HSVf hsv, HSLf hsl);

private:
	unsigned char c[4];
};


// http://stackoverflow.com/questions/7706339/grayscale-to-red-green-blue-matlab-jet-color-scale
extern void color_jet(float index, float *_r, float *_g, float *_b);
extern void color_jet_int (float index, int *_r, int *_g, int *_b);

#endif // __COLOR_H__
