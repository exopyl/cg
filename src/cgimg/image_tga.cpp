#include "image_tga.h"


TGAImg::TGAImg()
 { 
  pImage=pPalette=pData=nullptr;
  iWidth=iHeight=iBPP=bEnc=0;
  lImageSize=0;
  lDataSize=0;
 }


TGAImg::~TGAImg()
 {
  if(pImage)
   {
    delete [] pImage;
    pImage=nullptr;
   }

  if(pPalette)
   {
    delete [] pPalette;
    pPalette=nullptr;
   }

  if(pData)
   {
    delete [] pData;
    pData=nullptr;
   }
 }


int TGAImg::Load(char* szFilename)
 {
  using namespace std;
  ifstream fIn;
  unsigned long ulSize;
  int iRet;

  // Clear out any existing image and palette
   if(pImage)
    {
     delete [] pImage;
     pImage=nullptr;
    }

   if(pPalette)
    {
     delete [] pPalette;
     pPalette=nullptr;
    }

  // Open the specified file
  fIn.open(szFilename,ios::binary);

  // Le test d'ouverture etait commente. Sur un fichier absent, tellg() rend -1,
  // que la conversion en unsigned long transformait en taille colossale passee a
  // new[] ; et ReadHeader lisait ensuite pData[0..17] d'un tampon jamais rempli.
   if(!fIn.is_open())
    return IMG_ERR_NO_FILE;

  // Get file size
  fIn.seekg(0,ios_base::end);
  const std::streamoff sSize=fIn.tellg();
  fIn.seekg(0,ios_base::beg);

  // 18 octets : la taille de l'en-tete TGA, que ReadHeader lit sans condition.
   if(sSize<18)
    {
     fIn.close();
     return IMG_ERR_BAD_FORMAT;
    }
  ulSize=(unsigned long)sSize;

  // Allocate some space
  // Check and clear pDat, just in case
   if(pData)
    delete [] pData;

  pData=new unsigned char[ulSize];

   if(pData==nullptr)
    {
     fIn.close();
     return IMG_ERR_MEM_FAIL;
    }

  // Read the file into memory
  fIn.read((char*)pData,ulSize);

  fIn.close();

  lDataSize=ulSize;

  // Process the header
  iRet=ReadHeader();

   if(iRet!=IMG_OK)
    return iRet;

   switch(bEnc)
    {
     case 1: // Raw Indexed
      {
       // Check filesize against header values
        if((lImageSize+18+pData[0]+768)>ulSize)
         return IMG_ERR_BAD_FORMAT;

       // Double check image type field
        if(pData[1]!=1)
         return IMG_ERR_BAD_FORMAT;

       // Load image data
       iRet=LoadRawData();
        if(iRet!=IMG_OK)
         return iRet;

       // Load palette
       iRet=LoadTgaPalette();
        if(iRet!=IMG_OK)
         return iRet;

       break;
      }

     case 2: // Raw RGB
      {
       // Check filesize against header values
        if((lImageSize+18+pData[0])>ulSize)
         return IMG_ERR_BAD_FORMAT;

       // Double check image type field
        if(pData[1]!=0)
         return IMG_ERR_BAD_FORMAT;

       // Load image data
       iRet=LoadRawData();
        if(iRet!=IMG_OK)
         return iRet;

       BGRtoRGB(); // Convert to RGB
       break;
      }

     case 9: // RLE Indexed
      {
       // Double check image type field
        if(pData[1]!=1)
         return IMG_ERR_BAD_FORMAT;

       // Load image data
       iRet=LoadTgaRLEData();
        if(iRet!=IMG_OK)
         return iRet;

       // Load palette
       iRet=LoadTgaPalette();
        if(iRet!=IMG_OK)
         return iRet;

       break;
      }
 
     case 10: // RLE RGB
      {
       // Double check image type field
        if(pData[1]!=0)
         return IMG_ERR_BAD_FORMAT;

       // Load image data
       iRet=LoadTgaRLEData();
        if(iRet!=IMG_OK)
         return iRet;

       BGRtoRGB(); // Convert to RGB
       break;
      }

     default:
      return IMG_ERR_UNSUPPORTED;
    }

  // Check flip bit
   if((pData[17] & 0x20)==0) 
     FlipImg();

  // Release file memory
  delete [] pData;
  pData=nullptr;

  return IMG_OK;
 }


int TGAImg::ReadHeader() // Examine the header and populate our class attributes
 {
  short ColMapStart,ColMapLen;
  short x1,y1,x2,y2;

   if(pData==nullptr)
    return IMG_ERR_NO_FILE;

   if(pData[1]>1)    // 0 (RGB) and 1 (Indexed) are the only types we know about
    return IMG_ERR_UNSUPPORTED;

   bEnc=pData[2];     // Encoding flag  1 = Raw indexed image
                      //                2 = Raw RGB
                      //                3 = Raw greyscale
                      //                9 = RLE indexed
                      //               10 = RLE RGB
                      //               11 = RLE greyscale
                      //               32 & 33 Other compression, indexed

    if(bEnc>11)       // We don't want 32 or 33
     return IMG_ERR_UNSUPPORTED;


  // Get palette info
  memcpy(&ColMapStart,&pData[3],2);
  memcpy(&ColMapLen,&pData[5],2);

  // Reject indexed images if not a VGA palette (256 entries with 24 bits per entry)
   if(pData[1]==1) // Indexed
    {
     if(ColMapStart!=0 || ColMapLen!=256 || pData[7]!=24)
      return IMG_ERR_UNSUPPORTED;
    }

  // Get image window and produce width & height values
  memcpy(&x1,&pData[8],2);
  memcpy(&y1,&pData[10],2);
  memcpy(&x2,&pData[12],2);
  memcpy(&y2,&pData[14],2);

  iWidth=(x2-x1);
  iHeight=(y2-y1);

   if(iWidth<1 || iHeight<1)
    return IMG_ERR_BAD_FORMAT;

  // Bits per Pixel
  //
  // Cet octet vient du fichier et n'etait pas valide. Deux consequences, en aval
  // du seul `lImageSize` qui en decoule :
  //
  //   - une profondeur non multiple de 8 (ou nulle) donnait un iPixelSize et un
  //     lImageSize incoherents avec le contenu reel, et LoadRawData recopiait
  //     alors lImageSize octets depuis le fichier sans rapport avec sa taille ;
  //   - une profondeur arbitraire jusqu'a 255 faisait dimensionner le tampon a
  //     31 octets par pixel, tout en le parcourant pixel a pixel dans BGRtoRGB
  //     (cpp:S3519, image_tga.cpp:384).
  //
  // Seules les quatre profondeurs de la specification TGA sont acceptees ; 15 et
  // 16 bits ne sont de toute facon pas decodes ici, mais leur en-tete est licite.
  iBPP=pData[16];
   if(iBPP!=8 && iBPP!=15 && iBPP!=16 && iBPP!=24 && iBPP!=32)
    return IMG_ERR_UNSUPPORTED;

  // Check flip / interleave byte
   if(pData[17]>32) // Interleaved data
    return IMG_ERR_UNSUPPORTED;

  // Calculate image size
  //
  // Arithmetique en unsigned long, et non en int : iWidth et iHeight vont jusqu'a
  // 32767 chacun, donc le produit par la taille de pixel debordait un int 32 bits
  // pour les grandes images -- un debordement signe, dont le resultat negatif
  // devenait ensuite une taille d'allocation enorme.
  lImageSize=((unsigned long)iWidth * (unsigned long)iHeight * (unsigned long)(iBPP/8));

  return IMG_OK;
 }


int TGAImg::LoadRawData() // Load uncompressed image data
 {
  short iOffset;
 
   if(pImage) // Clear old data if present
    delete [] pImage;

  pImage=new unsigned char[lImageSize];

   if(pImage==nullptr)
    return IMG_ERR_MEM_FAIL;

  iOffset=pData[0]+18; // Add header to ident field size

   if(pData[1]==1) // Indexed images
    iOffset+=768;  // Add palette offset

   memcpy(pImage,&pData[iOffset],lImageSize);

  return IMG_OK;
 }


int TGAImg::LoadTgaRLEData() // Load RLE compressed image data
 {
  short iOffset,iPixelSize;
  unsigned char *pCur;
  unsigned long Index=0;
  unsigned char bLength,bLoop;

  // Calculate offset to image data
  iOffset=pData[0]+18;

  // Add palette offset for indexed images
   if(pData[1]==1)
    iOffset+=768; 

  // Get pixel size in bytes
  iPixelSize=iBPP/8;

  // L'offset lui-meme est dicte par le fichier (pData[0] = longueur du champ
  // identifiant) : il pouvait deja depasser la fin du tampon.
   if(iOffset<0 || (unsigned long)iOffset>=lDataSize)
    return IMG_ERR_BAD_FORMAT;

  // Set our pointer to the beginning of the image data
  pCur=&pData[iOffset];
  const unsigned char *const pEnd=pData+lDataSize;   // fin du fichier en memoire

  // Allocate space for the image data
   if(pImage!=nullptr)
    delete [] pImage;

  pImage=new unsigned char[lImageSize];

   if(pImage==nullptr)
    return IMG_ERR_MEM_FAIL;

  // Decode
  //
  // Le flux RLE se decrit lui-meme : chaque chunk annonce jusqu'a 128 pixels. Les
  // deux boucles internes avancaient `Index` et `pCur` SANS retester leurs bornes,
  // si bien qu'un fichier tronque ou un dernier chunk trop long ecrivait au-dela
  // de pImage (jusqu'a 128 pixels) et lisait au-dela de pData
  // (cpp:S3519, image_tga.cpp:384, via la conversion BGR qui parcourt ensuite le
  // tampon). Les deux extremites sont desormais bornees, et un flux incoherent
  // est rejete au lieu d'etre decode a moitie.
   while(Index<lImageSize)
    {
      if(pCur>=pEnd)
       return IMG_ERR_BAD_FORMAT;

      const bool bRunLength=((*pCur & 0x80)!=0);
      bLength=bRunLength ? (unsigned char)(*pCur-127) : (unsigned char)(*pCur+1);
      pCur++;            // Move to pixel data

      // Place-t-on encore bLength pixels dans l'image, et les lit-on dans le
      // fichier ? Un chunk RLE ne consomme qu'un pixel source, un chunk brut en
      // consomme bLength.
      const unsigned long lNeeded=(unsigned long)bLength*(unsigned long)iPixelSize;
      const unsigned long lSourceNeeded=bRunLength ? (unsigned long)iPixelSize : lNeeded;
       if(lNeeded>lImageSize-Index || lSourceNeeded>(unsigned long)(pEnd-pCur))
        return IMG_ERR_BAD_FORMAT;

      if(bRunLength) // Run length chunk (High bit = 1)
       {
        // Repeat the next pixel bLength times
         for(bLoop=0;bLoop!=bLength;++bLoop,Index+=iPixelSize)
          memcpy(&pImage[Index],pCur,iPixelSize);

        pCur+=iPixelSize; // Move to the next descriptor chunk
       }
      else // Raw chunk
       {
        // Write the next bLength pixels directly
         for(bLoop=0;bLoop!=bLength;++bLoop,Index+=iPixelSize,pCur+=iPixelSize)
          memcpy(&pImage[Index],pCur,iPixelSize);
       }
    }

  return IMG_OK;
 }


int TGAImg::LoadTgaPalette() // Load a 256 color palette
 {
  unsigned char bTemp;
  short iIndex,iPalPtr;
  
   // Delete old palette if present
   if(pPalette)
    {
     delete [] pPalette;
     pPalette=nullptr;
    }

  // La palette suit l'en-tete, mais rien ne garantissait que le fichier soit
  // assez long : la voie « RLE indexe » (bEnc == 9) n'a aucun controle de taille
  // dans Load, contrairement a la voie brute. Le memcpy lisait alors 768 octets
  // au-dela du tampon de fichier.
  const unsigned long lPaletteOffset=(unsigned long)pData[0]+18;
   if(lPaletteOffset+768>lDataSize)
    return IMG_ERR_BAD_FORMAT;

  // Create space for new palette
  pPalette=new unsigned char[768];

   if(pPalette==nullptr)
    return IMG_ERR_MEM_FAIL;

  // VGA palette is the 768 bytes following the header
  memcpy(pPalette,&pData[lPaletteOffset],768);

  // Palette entries are BGR ordered so we have to convert to RGB
   for(iIndex=0,iPalPtr=0;iIndex!=256;++iIndex,iPalPtr+=3)
    {
     bTemp=pPalette[iPalPtr];               // Get Blue value
     pPalette[iPalPtr]=pPalette[iPalPtr+2]; // Copy Red to Blue
     pPalette[iPalPtr+2]=bTemp;             // Replace Blue at the end
    }

  return IMG_OK;
 }


void TGAImg::BGRtoRGB() // Convert BGR to RGB (or back again)
 {
  unsigned long Index,nPixels;
  unsigned char *bCur;
  unsigned char bTemp;
  short iPixelSize;

  // Set ptr to start of image
  bCur=pImage;

  // Rien a convertir si le decodage n'a pas produit de tampon.
   if(bCur==nullptr)
    return;

  // Get pixel size in bytes
  iPixelSize=iBPP/8;

  // Calc number of pixels
  //
  // Deduit de la TAILLE DU TAMPON, et non du couple largeur x hauteur : c'est ce
  // tampon que la boucle parcourt, et rien ne garantissait que le produit des
  // dimensions corresponde a ce que LoadRawData / LoadTgaRLEData ont alloue.
  nPixels = (iPixelSize > 0) ? (lImageSize / (unsigned long)iPixelSize) : 0;

  // L'echange touche l'octet 0 et l'octet 2 de CHAQUE pixel : il n'a de sens que
  // si le pixel fait au moins 3 octets. En 8 bits (niveaux de gris) iPixelSize
  // vaut 1, donc `*(bCur+2)` lisait ET ecrivait deux octets au-dela du pixel
  // courant -- et au-dela du tampon sur le dernier (cpp:S3519, image_tga.cpp:375).
  // En 16 bits il empietait sur le pixel suivant. Rien a echanger dans ces deux
  // cas : les composantes ne sont pas rangees en BGR.
  if (iPixelSize < 3)
    return;

   for(Index=0;Index<nPixels;Index++)  // For each pixel
    {
     bTemp=*bCur;      // Get Blue value
     *bCur=*(bCur+2);  // Swap red value into first position
     *(bCur+2)=bTemp;  // Write back blue to last position

     bCur+=iPixelSize; // Jump to next pixel
    }

 }


void TGAImg::FlipImg() // Flips the image vertically (Why store images upside down?)
 {
  unsigned char bTemp;
  unsigned char *pLine1, *pLine2;
  int iLineLen,iIndex;
 
  iLineLen=iWidth*(iBPP/8);
  pLine1=pImage;
  pLine2=&pImage[iLineLen * (iHeight - 1)];

   for( ;pLine1<pLine2;pLine2-=(iLineLen*2))
    {
     for(iIndex=0;iIndex!=iLineLen;pLine1++,pLine2++,iIndex++)
      {
       bTemp=*pLine1;
       *pLine1=*pLine2;
       *pLine2=bTemp;       
      }
    } 

 }


int TGAImg::GetBPP() 
 {
  return iBPP;
 }


int TGAImg::GetWidth()
 {
  return iWidth;
 }


int TGAImg::GetHeight()
 {
  return iHeight;
 }


unsigned char* TGAImg::GetImg()
 {
  return pImage;
 }


unsigned char* TGAImg::GetPalette()
 {
  return pPalette;
 }
