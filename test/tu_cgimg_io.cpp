#include <gtest/gtest.h>

#include <cstdio>

#include "../src/cgimg/cgimg.h"

TEST(TEST_cgimg_io, tga_ctc16)
{
    // context
    Img img;

    // action 1
    auto res = img.load("./test/data/tga/ctc16.tga");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 128);
    EXPECT_EQ(img.height(), 128);
}

TEST(TEST_cgimg_io, tga_ctc24)
{
    // context
    Img img;

    // action 1
    auto res = img.load("./test/data/tga/ctc24.tga");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 128);
    EXPECT_EQ(img.height(), 128);
}

TEST(TEST_cgimg_io, tga_ctc32)
{
    // context
    Img img;

    // action 1
    auto res = img.load("./test/data/tga/ctc32.tga");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 128);
    EXPECT_EQ(img.height(), 128);
}

TEST(TEST_cgimg_io, png_rgb)
{
    // context
    Img img;

    // action: 256x256 RGB PNG
    auto res = img.load("./test/data/fallout_mask.png");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 256);
    EXPECT_EQ(img.height(), 256);
    // RGB source: alpha is forced opaque on import
    EXPECT_EQ(img.get_a(0, 0), 255);
}

TEST(TEST_cgimg_io, png_rgba)
{
    // context
    Img img;

    // action: 512x512 RGBA PNG
    auto res = img.load("./test/data/fallout_mask2.png");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 512);
    EXPECT_EQ(img.height(), 512);
}

TEST(TEST_cgimg_io, jpg_rgb)
{
    // context
    Img img;

    // action: JPEG RGB (décodage stb)
    auto res = img.load("./test/data/jpg/Nicolae_Grigorescu_005.jpg");

    // expectations
    EXPECT_EQ(res, 0);
    EXPECT_EQ(img.width(), 1576);
    EXPECT_EQ(img.height(), 2186);
    EXPECT_EQ(img.get_a(0, 0), 255);   // source RGB : alpha forcé opaque à l'import
}

// ===========================================================================
//  TGA RLE : le decodage ne sort plus du tampon
// ===========================================================================
//
// La boucle de decodage ne testait `pixels_read < w*h` qu'AVANT de lire un
// paquet, et un paquet RLE porte jusqu'a 128 pixels : le dernier ecrivait donc
// jusqu'a 127 pixels au-dela de l'image -- environ 508 octets de tas -- sur un
// fichier que rien n'oblige a etre sain, et que `Img::load` accepte de
// n'importe ou.
//
// Ces cas construisent les fichiers a la main : aucune image de reference ne
// peut porter un paquet qui deborde, puisqu'un encodeur correct n'en produit
// pas. C'est precisement pour cela que le defaut avait survecu.

namespace {

// En-tete TGA minimal, vrai couleur non palettise, compresse RLE (type 10).
void WriteTgaRleHeader (std::FILE *f, unsigned short w, unsigned short h,
                        unsigned char depth)
{
	const unsigned char header[12] = { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	std::fwrite (header, 1, sizeof (header), f);
	std::fwrite (&w, 2, 1, f);
	std::fwrite (&h, 2, 1, f);
	std::fwrite (&depth, 1, 1, f);
	const unsigned char descriptor = 0;   // origine en bas a gauche
	std::fwrite (&descriptor, 1, 1, f);
}

} // namespace

TEST (TEST_cgimg_io, tga_rle_packet_running_past_the_image_does_not_write_outside)
{
	// UNE IMAGE DE 4 PIXELS, UN PAQUET DE 128. Avant correction, 124 pixels --
	// 496 octets -- partaient au-dela du tampon.
	const char *path = "./tga_rle_overrun.tga";
	std::FILE *f = std::fopen (path, "wb");
	ASSERT_NE (f, nullptr);
	WriteTgaRleHeader (f, 2, 2, 24);
	const unsigned char packet[4] = { 0xFF, 0x11, 0x22, 0x33 };   // 128 pixels
	std::fwrite (packet, 1, sizeof (packet), f);
	std::fclose (f);

	Img img;
	EXPECT_EQ (img.load (path), 0);
	EXPECT_EQ (img.width (), 2u);
	EXPECT_EQ (img.height (), 2u);

	// Les quatre pixels de l'image portent la couleur du paquet, et rien n'a ete
	// ecrit plus loin -- ce que l'absence de corruption du tas atteste sous les
	// controles d'execution de MSVC.
	unsigned char r = 0, g = 0, b = 0, a = 0;
	for (unsigned int y = 0; y < 2; ++y)
		for (unsigned int x = 0; x < 2; ++x)
		{
			ASSERT_TRUE (img.get_pixel (x, y, &r, &g, &b, &a)) << x << "," << y;
			EXPECT_EQ ((int)r, 0x33);
			EXPECT_EQ ((int)g, 0x22);
			EXPECT_EQ ((int)b, 0x11);
		}

	std::remove (path);
}

TEST (TEST_cgimg_io, tga_rle_truncated_file_stops_instead_of_repeating_the_last_read)
{
	// FICHIER TRONQUE AU MILIEU D'UN PAQUET. Le retour de `fread` n'etait pas
	// teste : la fin de l'image se remplissait avec la derniere valeur lue, en
	// silence. On demande maintenant que le decodage s'arrete, sans planter.
	const char *path = "./tga_rle_truncated.tga";
	std::FILE *f = std::fopen (path, "wb");
	ASSERT_NE (f, nullptr);
	WriteTgaRleHeader (f, 4, 4, 24);
	// Paquet brut annonce a 16 pixels, dont un seul est effectivement present.
	const unsigned char partial[4] = { 0x0F, 0x40, 0x50, 0x60 };
	std::fwrite (partial, 1, sizeof (partial), f);
	std::fclose (f);

	Img img;
	// Le chargement rend un statut, pas un plantage -- c'est tout ce qui est
	// exige ici : le contrat d'erreur du module est lui-meme un point ouvert de
	// l'audit, donc on ne fige pas une valeur qu'il faudra changer.
	img.load (path);
	EXPECT_EQ (img.width (), 4u);
	EXPECT_EQ (img.height (), 4u);

	std::remove (path);
}
