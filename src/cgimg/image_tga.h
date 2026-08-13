#pragma once

// TGA Loader - 16/11/04 Codehead

#include <iostream>
#include <fstream>
#include <memory.h>

#define IMG_OK              0x1
#define IMG_ERR_NO_FILE     0x2
#define IMG_ERR_MEM_FAIL    0x4
#define IMG_ERR_BAD_FORMAT  0x8
#define IMG_ERR_UNSUPPORTED 0x40

class TGAImg
 {
  public:
   TGAImg();
   ~TGAImg();
   int Load(char* szFilename);
   int GetBPP();
   int GetWidth();
   int GetHeight();
   unsigned char* GetImg();       // Return a pointer to image data
   unsigned char* GetPalette();   // Return a pointer to VGA palette

  private:
   short int iWidth,iHeight,iBPP;
   unsigned long lImageSize;
   // Taille du fichier charge dans pData. Les decodeurs en ont besoin pour ne pas
   // lire au-dela : le flux RLE se decrit lui-meme, donc un fichier tronque le
   // fait deborder sans que rien ne l'arrete.
   unsigned long lDataSize;
   char bEnc;
   unsigned char *pImage, *pPalette, *pData;

   // Internal workers
   int ReadHeader();
   int LoadRawData();
   int LoadTgaRLEData();
   int LoadTgaPalette();
   void BGRtoRGB();
   void FlipImg();
 };
