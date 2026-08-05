#include <stdlib.h>

#include "image.h"
#include "image_filter.h"

#include <map>
#include <utility>
#include <vector>

namespace {

// Image de LABELS : une couleur RGB distincte -> un indice, attribue dans l'ordre
// de premiere rencontre en balayage raster. Les deux filtres non lineaires
// ci-dessous (mode, absorption) comparent et recopient des couleurs sans jamais
// les melanger : travailler sur des indices rend le vote O(1) par voisin, la ou un
// std::map par pixel coutait un log(n).
//
// L'alpha n'entre pas dans le label et n'est pas recopie : chaque pixel garde le
// sien (cf. applyLabels).
struct LabelImage
{
	int w = 0, h = 0;
	std::vector<int>          px;    // label par pixel
	std::vector<unsigned int> rgb;   // label -> RGB empaquete
	int  label (int x, int y) const { return px[(size_t)y * w + x]; }
	void set   (int x, int y, int l) { px[(size_t)y * w + x] = l; }
};

LabelImage toLabels (const Img& img)
{
	LabelImage L;
	L.w = (int)img.width ();
	L.h = (int)img.height ();
	L.px.assign ((size_t)L.w * L.h, 0);

	std::map<unsigned int, int> seen;
	for (int y = 0; y < L.h; ++y)
		for (int x = 0; x < L.w; ++x)
		{
			unsigned char r = 0, g = 0, b = 0, a = 0;
			img.get_pixel ((unsigned)x, (unsigned)y, &r, &g, &b, &a);
			const unsigned int key = ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
			std::map<unsigned int, int>::iterator it = seen.find (key);
			if (it == seen.end ())
			{
				it = seen.emplace (key, (int)L.rgb.size ()).first;
				L.rgb.push_back (key);
			}
			L.set (x, y, it->second);
		}
	return L;
}

void applyLabels (const LabelImage& L, Img& img)
{
	for (int y = 0; y < L.h; ++y)
		for (int x = 0; x < L.w; ++x)
		{
			const unsigned int c = L.rgb[(size_t)L.label (x, y)];
			unsigned char r = 0, g = 0, b = 0, a = 0;
			img.get_pixel ((unsigned)x, (unsigned)y, &r, &g, &b, &a);
			img.set_pixel ((unsigned)x, (unsigned)y,
			               (unsigned char)(c >> 16), (unsigned char)(c >> 8),
			               (unsigned char)c, a);
		}
}

} // namespace

int ImgFilter::sobel (Img& img)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));
	memset (pPixels, 0, 4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));
	char filterx[3][3] = { {-1, -2, -1}, {0, 0, 0},  {1, 2, 1} };
	char filtery[3][3] = { {-1, 0, 1},   {-2, 0, 2}, {-1, 0, 1} };
	unsigned char mrgb[3][3][3];
	int Gx[3], Gy[3];
	unsigned char a;

	// visit the image
	int i, j;
	int iWidth = img.m_iWidth;
	int iHeight = img.m_iHeight;
	for (j=0; j<iHeight; j++)
		for (i=0; i<iWidth; i++)
		{
			// visit the neighborough
			for (int k=-1; k<2; k++)
				for (int l=-1; l<2; l++)
				{
					int ii = i+k;
					int jj = j+l;
					if (ii < 0)
						ii = -ii;
					if (ii > iWidth-1)
						ii = 2 * iWidth - 1 - ii;
					if (jj < 0)
						jj = -jj;
					if (jj > iHeight-1)
						jj = 2 * iHeight - 1 - jj;

					img.get_pixel (ii, jj, &mrgb[0][k+1][l+1], &mrgb[1][k+1][l+1], &mrgb[2][k+1][l+1], &a);
				}
			
			// apply the filter
			memset (Gx, 0, 3*sizeof(int));
			memset (Gy, 0, 3*sizeof(int));
			for (int k=0; k<3; k++)
				for (int l=0; l<3; l++)
					for (int m=0; m<3; m++) // for each composant r g b
					{
						Gx[m] += filterx[k][l]*mrgb[m][k][l];
						Gy[m] += filtery[k][l]*mrgb[m][k][l];
					}
			int v;
			for (int m=0; m<3; m++)
			{
				v = fabs((float)Gx[m]) + fabs((float)Gy[m]);
				pPixels[4*(j*img.m_iWidth+i)+m]   = (v>255)? 255 : v;
			}
			pPixels[4*(j*img.m_iWidth+i)+3] = 255;
		}

	free (img.m_pPixels);
	img.m_pPixels = pPixels;

	return 0;
}

int ImgFilter::convolve (Img& img, float m[3][3], float divide, float decay)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));
	memset (pPixels, 0, 4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));

	if (divide == 0.)
	{
		for (int j=0; j<3; j++)
			for (int i=0; i<3; i++)
				divide += m[i][j];
	}
	if (divide == 0.)
		divide = 1.;
	int iWidth = img.m_iWidth;
	int iHeight = img.m_iHeight;
	for (int j=1; j<iHeight-1; j++)
		for (int i=1; i<iWidth-1; i++)
		{
			unsigned char r, g, b, a;
			float accum[4];
			memset (accum, 0, 4*sizeof(float));
			for (int jj=0; jj<3; jj++)
				for (int ii=0; ii<3; ii++)
				{
					img.get_pixel (i+ii-1, j+jj-1, &r, &g, &b, &a);
					accum[0] += m[ii][jj]*(float)r;
					accum[1] += m[ii][jj]*(float)g;
					accum[2] += m[ii][jj]*(float)b;
					accum[3] += m[ii][jj]*(float)a;
				}
			for (int k=0; k<4; k++)
			{
				accum[k] /= divide;
				if (accum[k] < 0)
					accum[k] = 0.;
				if (accum[k] > 255.)
					accum[k] = 255.;
				unsigned char level = (unsigned char)accum[k];
				pPixels[4*(j*img.m_iWidth+i)+k] = level;
			}
		}

	free (img.m_pPixels);
	img.m_pPixels = pPixels;

	return 0;
}

int ImgFilter::blur (Img& img)
{
	float m[3][3] = {{1., 1., 1.},
			 {1., 1., 1.},
			 {1., 1., 1.}};
	return convolve (img, m);
}

int ImgFilter::gaussian_blur (Img& img)
{
	// 3x3 Gaussian kernel (sum = 16). convolve() normalizes by the kernel sum when
	// divide == 0, so this convolves and rescales correctly (same path as blur()).
	// (Previously a no-op that returned success — the 5x5 kernel was only a comment.)
	float m[3][3] = {{1.f, 2.f, 1.f},
			 {2.f, 4.f, 2.f},
			 {1.f, 2.f, 1.f}};
	return convolve (img, m);
}

//
// "Bilateral Filtering for Gray and Color Images"
// http://users.soe.ucsc.edu/~manduchi/Papers/ICCV98.pdf
//
int ImgFilter::bilateral (Img& img)
{
	unsigned char *pPixels = (unsigned char*)malloc(4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));
	memset (pPixels, 0, 4*img.m_iWidth*img.m_iHeight*sizeof(unsigned char));

	int n = 6;
	float sigmaD = 5.;
	float sigmaR = 100.;

	float sigmaD2inv = 1. / (sigmaD*sigmaD);
	float sigmaR2inv = 1. / (sigmaR*sigmaR);
	unsigned char ro, go, bo, ao;
	unsigned char r, g, b, a;
	int i, ii, iii, j, jj, jjj;
	int iWidth = img.m_iWidth;
	int iHeight = img.m_iHeight;
	for (j=0; j<iHeight; j++)
		for (i=0; i<iWidth; i++)
		{
			img.get_pixel (i, j, &ro, &go, &bo, &ao);
			float accum[4] = {0., 0., 0., 0.};
			float norm = 0.;
			for (jj=-n/2; jj<=n/2; jj++)
				for (ii=-n/2; ii<=n/2; ii++)
				{
					iii = i + ii;
					jjj = j + jj;
					// Miroir de bord. `iii -= iii` donnait 0 (et non un miroir) :
					// tout le demi-voisinage hors cadre s'effondrait sur la colonne
					// 0, qui pesait alors jusqu'a 4 fois son poids -- visible sur le
					// lisere exterieur de l'image. -1-iii est le pendant symetrique
					// exact de la borne haute 2*W-1-iii : les deux replient autour du
					// bord en dupliquant le pixel de bord (-1 -> 0, W -> W-1).
					if (iii < 0)
						iii = -1 - iii;
					if (iii > iWidth-1)
						iii = 2 * iWidth - 1 - iii;
					if (jjj < 0)
						jjj = -1 - jjj;
					if (jjj > iHeight-1)
						jjj = 2 * iHeight - 1 - jjj;
						
						
					img.get_pixel (iii, jjj, &r, &g, &b, &a);

					// closeness function
					float c = exp (-0.5*(ii*ii+jj*jj)*sigmaD2inv);

					// similarity function
					float s = exp (-0.5*((r-ro)*(r-ro)+(g-go)*(g-go)+(b-bo)*(b-bo))*sigmaR2inv);
					
					//printf ("%f %f\n", c, s);
					norm += c*s;

					accum[0] += c*s*(float)r;
					accum[1] += c*s*(float)g;
					accum[2] += c*s*(float)b;
					accum[3] += c*s*(float)a;
				}
			//printf ("%f %f %f\n", accum[0], accum[1], accum[2]);
			for (int k=0; k<4; k++)
			{
				accum[k] /= norm;
				if (accum[k] < 0)
					accum[k] = 0.;
				if (accum[k] > 255.)
					accum[k] = 255.;
				unsigned char level = (unsigned char)accum[k];
				pPixels[4*(j*img.m_iWidth+i)+k] = level;
			}
		}

	free (img.m_pPixels);
	img.m_pPixels = pPixels;

	return 0;
}

//
// Filtre de MODE (vote majoritaire sur un voisinage carre).
//
// Chaque passe LIT un instantane (`src`) et ECRIT dans `L`, sinon le resultat
// dependrait de l'ordre de balayage : un pixel deja modifie voterait dans le
// voisinage de son successeur.
//
// A egalite, le pixel central est conserve -- la comparaison est STRICTE et part
// du compte du centre. Sans cette regle, une frontiere franche entre deux regions
// deriverait d'un pixel a chaque passe.
//
int ImgFilter::majority (Img& img, int radius, int passes)
{
	if (img.m_pPixels == nullptr || img.m_iWidth == 0 || img.m_iHeight == 0)
		return -1;
	if (radius < 1 || passes < 1)
		return 0;                      // rien a faire, pas une erreur

	LabelImage L = toLabels (img);
	const int nLabels = (int)L.rgb.size ();
	std::vector<int> votes ((size_t)nLabels, 0);
	// Un label touche par pixel visite au plus : (2r+1)^2 voisins.
	const int wnd = (2 * radius + 1) * (2 * radius + 1);
	std::vector<int> touched ((size_t)wnd, 0);

	for (int pass = 0; pass < passes; ++pass)
	{
		const std::vector<int> src = L.px;

		for (int y = 0; y < L.h; ++y)
			for (int x = 0; x < L.w; ++x)
			{
				const int centre = src[(size_t)y * L.w + x];
				int nTouched = 0;
				for (int dy = -radius; dy <= radius; ++dy)
					for (int dx = -radius; dx <= radius; ++dx)
					{
						const int xx = x + dx, yy = y + dy;
						if (xx < 0 || yy < 0 || xx >= L.w || yy >= L.h) continue;
						const int l = src[(size_t)yy * L.w + xx];
						if (votes[(size_t)l]++ == 0) touched[(size_t)nTouched++] = l;
					}

				int best = centre, bestVotes = votes[(size_t)centre];
				for (int i = 0; i < nTouched; ++i)
					if (votes[(size_t)touched[(size_t)i]] > bestVotes)
					{
						bestVotes = votes[(size_t)touched[(size_t)i]];
						best = touched[(size_t)i];
					}
				for (int i = 0; i < nTouched; ++i) votes[(size_t)touched[(size_t)i]] = 0;

				L.set (x, y, best);
			}
	}

	applyLabels (L, img);
	return 0;
}

//
// Absorption des composantes connexes trop petites.
//
// Chaque passe MESURE sur `L` intact et ECRIT dans une copie, comme
// filter_majority. Muter `L` au fil du balayage rendait la passe dependante de
// l'ordre : une region d'accueil rencontree APRES l'absorption d'un mouchetis
// voyait ses cellules fraichement absorbees exclues de son flood (leur `comp`
// etait deja marque), donc une taille sous-estimee -- et pouvait a son tour passer
// sous le seuil et partir dans une autre couleur.
//
// Une composante n'est repeinte que d'une couleur DEJA presente sur sa frontiere :
// l'etiquetage reste un pavage complet de l'image, sans trou. C'est ce qui permet
// aux appelants qui vectorisent ensuite les regions de garder une emprise exacte.
//
int ImgFilter::absorb_small_regions (Img& img, int minArea, int passes)
{
	if (img.m_pPixels == nullptr || img.m_iWidth == 0 || img.m_iHeight == 0)
		return -1;
	if (minArea <= 0 || passes < 1)
		return 0;                      // rien a faire, pas une erreur

	LabelImage L = toLabels (img);
	const int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };

	for (int pass = 0; pass < passes; ++pass)
	{
		std::vector<int> comp ((size_t)L.w * L.h, -1);
		std::vector<int> next = L.px;   // les fusions de CETTE passe vont ici
		bool merged = false;
		int nComp = 0;

		for (int y0 = 0; y0 < L.h; ++y0)
			for (int x0 = 0; x0 < L.w; ++x0)
			{
				if (comp[(size_t)y0 * L.w + x0] != -1) continue;
				const int mine = L.label (x0, y0);

				// flood fill de la composante, en comptant les labels de sa frontiere
				std::vector<std::pair<int,int>> cells;
				std::map<int, int> borderVotes;
				std::vector<std::pair<int,int>> stack { { x0, y0 } };
				comp[(size_t)y0 * L.w + x0] = nComp;
				while (!stack.empty ())
				{
					const std::pair<int,int> p = stack.back (); stack.pop_back ();
					cells.push_back (p);
					for (int k = 0; k < 4; ++k)
					{
						const int xx = p.first + dx4[k], yy = p.second + dy4[k];
						if (xx < 0 || yy < 0 || xx >= L.w || yy >= L.h) continue;
						const int other = L.label (xx, yy);
						if (other != mine) { borderVotes[other]++; continue; }
						if (comp[(size_t)yy * L.w + xx] != -1) continue;
						comp[(size_t)yy * L.w + xx] = nComp;
						stack.emplace_back (xx, yy);
					}
				}
				++nComp;

				if ((int)cells.size () >= minArea || borderVotes.empty ())
					continue;

				int winner = borderVotes.begin ()->first, best = -1;
				for (std::map<int,int>::const_iterator it = borderVotes.begin (); it != borderVotes.end (); ++it)
					if (it->second > best) { best = it->second; winner = it->first; }

				for (size_t c = 0; c < cells.size (); ++c)
					next[(size_t)cells[c].second * L.w + cells[c].first] = winner;
				merged = true;
			}

		if (!merged) break;
		L.px.swap (next);
	}

	applyLabels (L, img);
	return 0;
}

int ImgFilter::saturate (Img& img, float t)
{
	for (unsigned int j=0; j<img.m_iHeight; j++)
		for (unsigned int i=0; i<img.m_iWidth; i++)
		{
			unsigned char r, g, b, a, avg;

			img.get_pixel (i, j, &r, &g, &b, &a);
			avg = ( r + g + b ) / 3;

			r = CLAMP ((avg + t * (r - avg)), 0, 255);
			g = CLAMP ((avg + t * (g - avg)), 0, 255);
			b = CLAMP ((avg + t * (b - avg)), 0, 255);
		
			img.set_pixel (i, j, r, g, b, a);
		}
		return 0;
}

int ImgFilter::brightness (Img& img, float t)
{
	for (unsigned int j=0; j<img.m_iHeight; j++)
		for (unsigned int i=0; i<img.m_iWidth; i++)
		{
			unsigned char r, g, b, a;

			img.get_pixel (i, j, &r, &g, &b, &a);

			r = CLAMP (t * r, 0, 255);
			g = CLAMP (t * g, 0, 255);
			b = CLAMP (t * b, 0, 255);
		
			img.set_pixel (i, j, r, g, b, a);
		}
		return 0;
}

int ImgFilter::gamma (Img& img, float t)
{
	for (unsigned int j=0; j<img.m_iHeight; j++)
		for (unsigned int i=0; i<img.m_iWidth; i++)
		{
			unsigned char r, g, b, a;

			img.get_pixel (i, j, &r, &g, &b, &a);

			r = CLAMP (pow(r, t), 0, 255);
			g = CLAMP (pow(g, t), 0, 255);
			b = CLAMP (pow(b, t), 0, 255);
		
			img.set_pixel (i, j, r, g, b, a);
		}
		return 0;
}

int ImgFilter::sepia (Img& img)
{
	unsigned int w = img.m_iWidth;
	unsigned int h = img.m_iHeight;
	for (unsigned int j=0; j<h; j++)
		for (unsigned int i=0; i<w; i++)
		{
			unsigned char r, g, b, a;

			img.get_pixel (i, j, &r, &g, &b, &a);

			// Standard sepia matrix. (A second set_pixel used to overwrite this
			// with r+40/g+20/b-20 of the ORIGINAL pixel, negating the sepia.)
			img.set_pixel (i, j,
				   CLAMP ((r * 0.393) + (g * 0.769) + (b * 0.189), 0, 255),
				   CLAMP ((r * 0.349) + (g * 0.686) + (b * 0.168), 0, 255),
				   CLAMP ((r * 0.272) + (g * 0.534) + (b * 0.131), 0, 255),
				   a);
		}
		return 0;
}

