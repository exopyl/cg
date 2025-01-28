#include "image.h"

//
// References :
// http://www.fileformat.info/format/bmp/egff.htm
//

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>

/* AUX_RGBImageRec*
AUX_RGBImageRec *LoadBMP(char *Filename)
{
	if (!Filename) return NULL;
	FILE *File=fopen(Filename,"r");
	if (File)
	{
		fclose(File);
		return auxDIBImageLoad(Filename);
	}
	return NULL;
}
*/
int Img::import_bmp (const char *filename)
{
	FILE *filePtr;
	BITMAPFILEHEADER bitmapFileHeader;
	BITMAPINFOHEADER bitmapInfoHeader;
	
	filePtr = fopen(filename,"rb");
	if (filePtr == NULL)
		return -1;

	//read the bitmap file header
	fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);
	if (bitmapFileHeader.bfType != 0x4D42) //verify that this is a bmp file by check bitmap id
	{
		fclose(filePtr);
		return -1;
	}

	//read the bitmap info header
	fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER),1,filePtr);

	//move file point to the begining of bitmap data
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);
	
	unsigned int width = bitmapInfoHeader.biWidth;
	unsigned int height = bitmapInfoHeader.biHeight;
	int bits = bitmapInfoHeader.biBitCount;

	switch (bits)
	{
	case 1: // monochrome
		{
			resize_memory (width, height, false);
			int scanlineWidth = bitmapInfoHeader.biSizeImage/m_iHeight;
			unsigned char *bytes=(unsigned char*)malloc(scanlineWidth*sizeof(unsigned char));
			for(int y=m_iHeight-1;y>=0;y--)
			{
				fread(bytes,1,scanlineWidth,filePtr);
				int indexByte = 0;
				for(unsigned int x=0;x<m_iWidth;x+=8)
				{
					char byte = bytes[indexByte++];
					for(int x2=0;x2<8;++x2)
					{
						unsigned char level = ((byte>>(7-x2))&1)*255;
						set_pixel (x+x2,y,level,level,level,255);
					}
				}
			}
			free(bytes);
		}
		break;
	case 24: // RGB
		{
			resize_memory (width, height, false);
			int scanlineWidth = bitmapInfoHeader.biSizeImage/m_iHeight;
			unsigned char *bytes=(unsigned char*)malloc(scanlineWidth*sizeof(unsigned char));
			for(int y=m_iHeight-1;y>=0;y--)
			{
				fread(bytes,1,scanlineWidth,filePtr);
				for(unsigned int x=0;x<m_iWidth;x++)
				{
					for(int c=0;c<3;++c)
						m_pPixels[4*(y*m_iWidth + x)+2-c] = bytes[3*x+c];
					m_pPixels[4*(y*m_iWidth + x)+3] = 255;
				}
			}
			free(bytes);
		}
		break;
	default:
		{
			fclose (filePtr);
			return -1;
		}
		break;
	}

	fclose(filePtr);

	return 0;
}

int Img::export_bmp (const char *filename)
{
	FILE *ptrbuffer = fopen(filename, "wb");
	if (!ptrbuffer)
		return -1;

	BITMAPINFOHEADER bitmapInfoHeader; // bitmap info header
	bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfoHeader.biWidth = m_iWidth;
	bitmapInfoHeader.biHeight = m_iHeight;
	bitmapInfoHeader.biPlanes = 1;
	bitmapInfoHeader.biBitCount = 24;//8;
	bitmapInfoHeader.biCompression = BI_RGB;
	bitmapInfoHeader.biSizeImage = (((m_iWidth * 3) + 3) & ~3) * m_iHeight; // packed pixel formats
	bitmapInfoHeader.biXPelsPerMeter = 0;
	bitmapInfoHeader.biYPelsPerMeter = 0;
	bitmapInfoHeader.biClrUsed = 0;
	bitmapInfoHeader.biClrImportant = 0;
   
	BITMAPFILEHEADER bitmapFileHeader; // bitmap file header
	bitmapFileHeader.bfType = 0x4D42;//''MB''
	bitmapFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bitmapInfoHeader.biSizeImage;
	bitmapFileHeader.bfReserved1 = 0;
	bitmapFileHeader.bfReserved2 = 0;
	bitmapFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  
	fwrite(&bitmapFileHeader, 1, sizeof(BITMAPFILEHEADER), ptrbuffer); // write the bitmap info header
	fwrite(&bitmapInfoHeader, 1, sizeof(BITMAPINFOHEADER), ptrbuffer); // write the image data

	int scanlineWidth = bitmapInfoHeader.biSizeImage/m_iHeight;
	unsigned char *pData = (unsigned char*)malloc(scanlineWidth);
	memset(pData, 0, scanlineWidth);
	for (unsigned int j=0; j<m_iHeight; j++)
	{
		for (unsigned int i=0; i<m_iWidth; i++)
		{
			// ! the image is a "bottom-up" bitmap with the origin in the lower-left corner
			pData[3*i+2] = m_pPixels[4*((m_iHeight-1-j)*m_iWidth+i)];
			pData[3*i+1] = m_pPixels[4*((m_iHeight-1-j)*m_iWidth+i)+1];
			pData[3*i]   = m_pPixels[4*((m_iHeight-1-j)*m_iWidth+i)+2];
		}
		fwrite(pData, scanlineWidth, 1, ptrbuffer);
	}

	free (pData);
	fclose(ptrbuffer);

	return 0;
}

#else


typedef unsigned char  uchar;

typedef unsigned short uint16;
typedef signed short   int16;

typedef unsigned int   uint32;
typedef signed int     int32;

#define BI_RGB       0
#define BI_RLE4      1
#define BI_RLE8      2
#define BI_BITFIELDS 3

struct bmp_file_header_s
{
  uint32 size;
  uint32 reserved;
  uint32 offsettobits;
};

struct bmp_bitmap_header_s
{
  uint32 size;
  int32  width;
  int32  height;
  uint16 planes;
  uint16 bpp;
  uint32 compression;
  uint32 imagesize;
  int32  widthppm;
  int32  heightppm;
  uint32 colorused;
  uint32 colorimportant;
};

int Img::import_bmp (const char *filename)
{
	// check size of int
	if(sizeof(int)!=4)
		return -1;
		
	FILE* f=fopen(filename, "rb");
	if(!f)
		return -1;
	char header[54];
	fread(header,54,1,f);

	// "BM" in header
	if(header[0]!='B' || header[1]!='M') 
	{
		printf ("This file is not a bitmap, specifically it doesn't start 'BM'\n");
		fclose(f);
		return -1;
	}

	//it seems gimp sometimes makes its headers small, so we have to do this. hence all the fseeks
	int offset=*(unsigned int*)(header+10);
	
	int width=*(int*)(header+18);
	int height=*(int*)(header+22);
	//now the bitmap knows how big it is it can allocate its memory
	resize_memory (width, height);

	int bits=int(header[28]);
	printf ("bit : %d\n", bits);
	int x,y,c;
	char cols[256*4]; // color table
	switch(bits)
	{
	case 24:
		fseek(f,offset,SEEK_SET);
		//fread(m_pPixels,m_iWidth*m_iHeight*3,1,f);
		//for(y=0;y<m_iHeight;++y)
		for(y=m_iHeight-1;y>=0;y--)
			for(x=0;x<m_iWidth;x++)
			{
				char byte;
				for(int c=0;c<3;++c)
				{
					fread(&byte,1,1,f);
					m_pPixels[4*(y*m_iWidth + x)+2-c] = byte;
				}
				m_pPixels[4*(y*m_iWidth + x)+3] = 255;
			}
		break;

	case 8:

		/*fread(cols,256*4,1,f);							//read colortable
		fseek(f,offset,SEEK_SET);
		for(y=0;y<bmp.height;++y)						//(Notice 4bytes/col for some reason)
			for(x=0;x<bmp.width;++x)
			{
				BYTE byte;			
				fread(&byte,1,1,f);						//just read byte					
				for(int c=0;c<3;++c)
					bmp.pixel(x,y,c)=cols[byte*4+2-c];	//and look up in the table
			}
		*/
		//changed by shiben bhattacharjee, IIIT Hyderabad shiben@students.iiit.ac.in
		//since i needed 8bit greyscale data for my work

		fseek(f,offset,SEEK_SET);
		printf("Reading a grayscale image...\n");
		fread(m_pPixels, m_iWidth*m_iHeight,1,f);
		break;

	case 4:
/*
		fread(cols,16*4,1,f);
		fseek(f,offset,SEEK_SET);
		for(y=0;y<256;++y)
			for(x=0;x<256;x+=2)
			{
				char byte;
				fread(&byte,1,1,f);						//as above, but need to exract two
				for(c=0;c<3;++c)						//pixels from each byte
					bmp.pixel(x,y,c)=cols[byte/16*4+2-c];
				for(c=0;c<3;++c)
					bmp.pixel(x+1,y,c)=cols[byte%16*4+2-c];
			}
*/
		break;

	case 1:
		fread(cols,8,1,f);
		fseek(f,offset,SEEK_SET);
		for(y=0;y<m_iHeight;++y)
			for(x=0;x<m_iWidth;x+=8)
			{
				char byte;
				fread(&byte,1,1,f);
				//Every byte is eight pixels
				//so I'm shifting the byte to the relevant position, then masking out
				//all but the lowest bit in order to get the index into the colourtable.
				for(int x2=0;x2<8;++x2)
					for(int c=0;c<3;++c)
						m_pPixels[y*m_iWidth + x+x2 +c] = cols[((byte>>(7-x2))&1)*4+2-c];
				//bmp.pixel(x+x2,y,c)=cols[((byte>>(7-x2))&1)*4+2-c];
			}
		break;

	default:
		fclose(f);
		return -1;
	}

	if(ferror(f))
	{
		fclose(f);
		return -1;
	}
	
	fclose(f);

	return 0;
}

int Img::export_bmp (const char *filename)
{
  FILE *ptr;
  ptr = fopen (filename, "w");
  if (!ptr)
    return -1;

  uchar id[2] = {'B','M'};
  struct bmp_file_header_s fh;
  struct bmp_bitmap_header_s bh;

  fh.size           = sizeof(id) + sizeof(fh) + sizeof(bh) + sizeof(m_pPixels);
  fh.reserved       = 0;
  fh.offsettobits   = sizeof(id) + sizeof(fh) + sizeof(bh);
  
  bh.size           = sizeof(bh);
  bh.width          = width();
  bh.height         = height();
  bh.planes         = 1;
  bh.bpp            = 24;
  bh.compression    = BI_RGB;
  bh.imagesize      = 3*width()*height();
  bh.widthppm       = 0;
  bh.heightppm      = 0;
  bh.colorused      = 0;
  bh.colorimportant = 0;
  
  fwrite (&id, sizeof(unsigned char), 2, ptr);
  fwrite (&fh, sizeof(fh), 1, ptr);
  fwrite (&bh, sizeof(bh), 1, ptr);
  fwrite (m_pPixels, sizeof(unsigned char), 3*width()*height(), ptr); // todo : dismiss alpha
  fclose (ptr);  

  return 0;
}

#endif // WIN32
