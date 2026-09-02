#include <limits.h>
#include <stdlib.h>

#include "image.h"
#include "image_io.h"

// ===========================================================================
//  Decodage depuis un buffer en memoire (ImgIO::load_from_memory)
// ===========================================================================
//
// Unite SEPAREE de image_io_png.cpp et image_io_jpg.cpp, et elle porte donc une
// TROISIEME copie de l'implementation stb_image. Ce n'est pas un oubli :
//
//  - les deux autres compilent stb avec STB_IMAGE_STATIC, precisement pour que
//    leurs symboles restent internes et n'entrent pas en conflit avec la copie
//    embarquee dans cgmesh (vmeshes.cpp) quand les deux bibliotheques sont liees
//    ensemble. Des symboles statiques ne se partagent pas entre unites : aucune
//    des deux ne peut fournir le decodeur a celle-ci ;
//  - les deux sont de surcroit gardees par CGIMG_WITH_PNG / CGIMG_WITH_JPG,
//    alors que le decodage en memoire ne doit dependre d'aucune des deux --
//    il sert aussi le BMP, le TGA et le PNM, qui n'ont pas d'option.
//
// Consolider les trois en une unite unique est un point ouvert de l'audit de
// dette (debt_cgimg.md, axe extensibilite, « 2 copies de stb_image dans cgimg »).
// Il n'est deliberement PAS traite ici : la fusion toucherait le chemin de
// decodage JPEG dont dependent les pages « Image to puzzle » et « Blocs
// pixelises » de maker, et ce fichier n'a pas a en prendre le risque.
//
// Le cout est du TEMPS DE COMPILATION (stb_image fait ~7 800 lignes), pas de la
// taille de binaire : l'editeur de liens ne retient que les fonctions atteintes.
// ===========================================================================

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb/stb_image.h>

int ImgIO::load_from_memory (Img& img, const unsigned char *data, size_t size)
{
	// Meme remise a zero que ImgIO::load : un load_from_memory sur un Img deja
	// charge doit repartir de rien, sans quoi l'ancien tampon fuit.
	if (img.m_pPixels)  free (img.m_pPixels);
	if (img.m_pPalette) { delete img.m_pPalette; img.m_pPalette = nullptr; }
	img.m_iWidth  = 0;
	img.m_iHeight = 0;
	img.m_pPixels = nullptr;

	if (!data || size == 0)
		return -1;

	// stbi_load_from_memory prend un `int` : un buffer de plus de 2 Gio le ferait
	// deborder, et le test doit donc porter sur la valeur AVANT conversion.
	if (size > static_cast<size_t> (INT_MAX))
		return -1;

	int w = 0, h = 0, channels = 0;

	// 4 composantes forcees : chaque pixel revient en RGBA quel que soit l'espace
	// colorimetrique de la source, comme dans import_png / import_jpg. Le format
	// lui-meme est reconnu au contenu -- il n'y a pas d'extension ici.
	unsigned char *pixels = stbi_load_from_memory (data, static_cast<int> (size),
	                                               &w, &h, &channels, 4);
	if (!pixels || w <= 0 || h <= 0)
	{
		if (pixels) stbi_image_free (pixels);
		return -1;
	}

	// Retour teste, contrairement a ce que faisaient les importeurs avant
	// correction : sur echec d'allocation les dimensions sont posees mais le
	// tampon reste nul, et les set_pixel suivants ecriraient dedans.
	if (img.resize_memory (w, h) != 0)
	{
		stbi_image_free (pixels);
		return -1;
	}

	// Offsets en size_t et non en int : stb accepte jusqu'a 2^24 par cote, donc
	// `4 * (y * w + x)` deborderait un int 32 bits bien avant cette limite.
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
		{
			const unsigned char *p =
				pixels + 4 * ((size_t)y * (size_t)w + (size_t)x);
			img.set_pixel (x, y, p[0], p[1], p[2], p[3]);
		}

	stbi_image_free (pixels);
	return 0;
}
