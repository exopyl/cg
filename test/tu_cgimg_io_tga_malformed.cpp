#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "../src/cgimg/cgimg.h"

// ===========================================================================
//  TGA : en-têtes hostiles et fichiers tronqués
// ===========================================================================
//
// `ImgIO::import_tga`, atteint par `Img::load`, lit largeur et hauteur dans l'en-tête — 18 octets, dont rien ne
// garantit qu'ils décrivent une image possible — puis appelait
// `img.resize_memory(width, height)` SANS tester son retour, et reprenait
// ensuite `width`/`height` de l'en-tête plutôt que de l'image.
//
// Or `resize_memory` laisse, en cas d'échec, une image délibérément VIDE et
// cohérente : `m_pPixels` nul, dimensions à zéro (c'est écrit dans son corps,
// et elle documente qu'elle protège ainsi les appelants qui ignorent son
// retour). Le décodeur se fabriquait donc une image qui SE DÉCLARE de W × H sur
// un tampon inexistant, et les boucles RLE écrivaient à l'adresse nulle.
//
// ⚠ CE QUI N'EST PAS TESTÉ ICI, ET POURQUOI. Le déclencheur du pointeur nul est
// l'ÉCHEC de `resize_memory`. Sur une cible 64 bits, son garde anti-débordement
// ne peut pas se déclencher (largeur et hauteur sont des `unsigned short`, leur
// produit par 4 tient très au large dans un `size_t`) : il faut donc que
// `malloc` échoue. Or un en-tête annonçant 65535 × 65535 demande ~17 Go, et
// cette machine les a ACCORDÉS — mesuré : l'allocation réussit en 18 s via le
// fichier d'échange. Le défaut n'y est donc pas atteignable, et un test qui
// « passe » n'y prouverait rien.
//
// Là où il est atteignable : sur WebAssembly, où `size_t` fait 32 bits et où le
// garde de `resize_memory` se déclenche de lui-même sur ces dimensions, et sur
// toute machine qui refuse l'allocation. Le correctif reste donc justifié — il
// n'est simplement pas couvert par un test de non-régression, et il vaut mieux
// l'écrire que de laisser croire le contraire.
//
// ⚠ POURQUOI CE FICHIER EXISTE, alors que tu_cgimg_io.cpp porte déjà deux tests
// de non-régression RLE. Son helper `WriteTgaRleHeader` écrit `descriptor = 0`,
// donc une origine 0, donc la branche `case 0` — celle qui écrit par
// `set_pixel`, bornée par construction. Or les commentaires de ces deux tests
// décrivent le débordement de tas corrigé dans `case 2`, la branche qui indexe
// `m_pPixels` directement.
//
// Autrement dit : les tests écrits pour `case 2` n'ont jamais exercé `case 2`.
// Ils passent, mais pas pour la raison annoncée. Les tests ci-dessous bouclent
// sur les DEUX orientations (descripteur 0x00 et 0x20) et ferment ce trou.
//
// ===========================================================================

namespace {

// En-tête TGA de 18 octets. Les champs multi-octets sont en petit-boutiste,
// comme le fait la lecture par `fread` directe du fichier.
std::vector<unsigned char> tgaHeader (unsigned short w, unsigned short h,
                                      unsigned char imageType,
                                      unsigned char pixelDepth,
                                      unsigned char descriptor)
{
    std::vector<unsigned char> b (18, 0);
    b[2]  = imageType;                        // 10 = true color RLE
    b[12] = (unsigned char)(w & 0xFF);
    b[13] = (unsigned char)(w >> 8);
    b[14] = (unsigned char)(h & 0xFF);
    b[15] = (unsigned char)(h >> 8);
    b[16] = pixelDepth;
    b[17] = descriptor;                       // origine = (descriptor & 0x30) >> 4
    return b;
}

void writeFile (const std::string& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream ofs (path, std::ios::binary);
    ofs.write ((const char*)bytes.data (), (std::streamsize)bytes.size ());
}

const unsigned char kTrueColorRle = 10;

} // namespace

// Un fichier réduit à son seul en-tête : aucun paquet à décoder. Le décodeur
// doit s'arrêter sur le premier `fread` manqué au lieu de boucler sur des
// valeurs jamais initialisées.
//
// Les deux orientations sont exercées : `case 2` était déjà durci, `case 0` ne
// l'était pas — il ne testait aucun retour de `fread`, et remplissait la fin de
// l'image avec la dernière couleur lue, en silence.
TEST(TEST_cgimg_tga_malformed, a_header_without_pixel_data_terminates)
{
    for (unsigned char descriptor : { (unsigned char)0x00, (unsigned char)0x20 })
    {
        const std::string path = "./tu_tga_nodata.tga";
        writeFile (path, tgaHeader (16, 16, kTrueColorRle, 32, descriptor));

        Img img;
        const int rc = img.load (path.c_str ());

        // L'allocation de 16x16 reussit : l'import aboutit, sur une image dont
        // le contenu n'est pas defini mais dont les dimensions le sont.
        EXPECT_EQ(rc, 0) << "origine " << (int)((descriptor & 0x30) >> 4);
        EXPECT_EQ(img.width (), 16u);
        EXPECT_EQ(img.height (), 16u);

        std::remove (path.c_str ());
    }
}

// Un paquet RLE annonçant 128 pixels sur une image qui n'en contient que 4 :
// c'est le cas que la garde `room` de `case 2` couvre. Le fichier est complet
// et bien formé, seul le compte du paquet est mensonger.
TEST(TEST_cgimg_tga_malformed, a_run_longer_than_the_image_stays_in_bounds)
{
    for (unsigned char descriptor : { (unsigned char)0x00, (unsigned char)0x20 })
    {
        std::vector<unsigned char> bytes = tgaHeader (2, 2, kTrueColorRle, 32, descriptor);
        bytes.push_back (0x80 | 127);          // paquet RLE de 128 pixels
        bytes.push_back (0x11);                // b
        bytes.push_back (0x22);                // g
        bytes.push_back (0x33);                // r
        bytes.push_back (0xFF);                // a

        const std::string path = "./tu_tga_longrun.tga";
        writeFile (path, bytes);

        Img img;
        EXPECT_EQ(img.load (path.c_str ()), 0)
            << "origine " << (int)((descriptor & 0x30) >> 4);
        EXPECT_EQ(img.width (), 2u);
        EXPECT_EQ(img.height (), 2u);

        std::remove (path.c_str ());
    }
}
