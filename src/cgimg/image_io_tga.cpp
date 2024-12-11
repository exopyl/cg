#include "image.h"

/*
int Img::import_tga (const char *filename)
{
	FILE *file;
	file = fopen(filename, "rb");
	if (!file)
		return -1;
	
	bool rle = false;
	bool truecolor = false;
	unsigned int CurrentPixel = 0;
	unsigned char ch_buf1, ch_buf2;
	unsigned char buf1[1000];
	
	unsigned char IDLength;
	unsigned char IDColorMapType;
	unsigned char IDImageType;
	
	fread (&IDLength, sizeof(unsigned char), 1, file);
	fread (&IDColorMapType, sizeof(unsigned char), 1, file);
	//ReadData(file, (char*)&IDLength, 1);
	//ReadData(file, (char*)&IDColorMapType, 1);
	
	if (IDColorMapType == 1)
	{
		fclose (file);
		return -1;
	}
	
	fread (&IDImageType, sizeof(unsigned char), 1, file);
	//ReadData(file, (char*)&IDImageType, 1); 
	
	int type = 255; // O -> grayscale / 1 -> truecolor rgb / 2 -> truecolor rgba
	switch (IDImageType)
	{
	case 2: // truecolor
		type = 1;
		break;
	case 3: // greyscale
		type = 0;
		break;
	case 10: // truecolor rle
		rle = true;
		type = 1;
		break;
	case 11: // grayscale rle
		rle = true;
		type = 0;
		break;
	default:
		return false;
	}

	fseek (file, 5, SEEK_CUR);
	//file.seekg(5, std::ios::cur);

	fseek (file, 4, SEEK_CUR);
	//file.seekg(4, std::ios::cur);
	fread (&m_iWidth, sizeof(unsigned int), 1, file);
	fread (&m_iHeight, sizeof(unsigned int), 1, file);
	//ReadData(file, (char*)&m_width, 2);
	//ReadData(file, (char*)&m_height, 2);

	unsigned char pixelDepth;
	fread (&pixelDepth, sizeof(unsigned char), 1, file);
	//ReadData(file, (char*)&m_pixelDepth, 1);

	if (! ((pixelDepth == 8) || (pixelDepth ==  24) ||
	       (pixelDepth == 16) || (pixelDepth == 32)))
		return -1;
	
	fread (&ch_buf1, sizeof(unsigned char), 1, file);
	//ReadData(file, (char*)&ch_buf1, 1); 
    
	ch_buf2 = 15; //00001111;

	// the depth of the alpha bitplane
	unsigned int alphaDepth;
	alphaDepth = ch_buf1 & ch_buf2;

	if (! ((alphaDepth == 0) || (alphaDepth == 8)))
		return -1;

	if (type == 1) // truecolor
	{
		type = 1;
		if (pixelDepth == 32)
			type = 2;
	}

	if (type == 255)
		return -1;

	fseek (file, IDLength, SEEK_CUR);
	//file.seekg(IDLength, std::ios::cur);

	resize_memory (m_iWidth, m_iHeight);
	//m_pixels = (byte*) malloc(m_width*m_height*(m_pixelDepth/8));

	if (!rle)
	{
		fread (&m_pPixels, sizeof(unsigned char), m_iWidth*m_iHeight*(pixelDepth/8), file);
		//ReadData(file, (char*)m_pixels, m_width*m_height*(m_pixelDepth/8));
	}
	else
	{
		while (CurrentPixel < m_iWidth*m_iHeight -1)
		{
			fread (&ch_buf1, sizeof(unsigned char), 1, file);
			//ReadData(file, (char*)&ch_buf1, 1);
			if ((ch_buf1 & 128) == 128) // this is an rle packet
			{
				ch_buf2 = (unsigned char)((ch_buf1 & 127) + 1); // how many pixels are encoded using this packet
				fread (&buf1, sizeof(unsigned char), pixelDepth/8, file);
				//ReadData(file, (char*)buf1, m_pixelDepth/8);
				for (unsigned int i=CurrentPixel; i<CurrentPixel+ch_buf2; i++)
					for (unsigned int j=0; j<pixelDepth/8; j++)
						m_pPixels[i*pixelDepth/8+j] = buf1[j];
				CurrentPixel += ch_buf2;
			}
			else // this is a raw packet
			{
				ch_buf2 = (unsigned char)((ch_buf1 & 127) + 1);
				fread (&buf1, sizeof(unsigned char), pixelDepth/8*ch_buf2, file);
				//ReadData(file, (char*)buf1, m_pixelDepth/8*ch_buf2);
				for (unsigned int i=CurrentPixel; i<CurrentPixel+ch_buf2; i++)
					for (unsigned int j=0; j<pixelDepth/8; j++)
						m_pPixels[i*pixelDepth/8+j] =  buf1[(i-CurrentPixel)*pixelDepth/8+j];
				CurrentPixel += ch_buf2;
			}
		}
	}

	// swap BGR(A) to RGB(A)
	unsigned char temp;
	if ((type == 1) || (type == 2))
		if ((pixelDepth == 24) || (pixelDepth == 32))
			for (unsigned int i= 0; i<m_iWidth*m_iHeight; i++)
			{
				temp = m_pPixels[i*pixelDepth/8];
				m_pPixels[i*pixelDepth/8] = m_pPixels[i*pixelDepth/8+2];
				m_pPixels[i*pixelDepth/8+2] = temp;
			}
	
	return 0;
}
*/

//
// References :
// http://paulbourke.net/dataformats/tga/
//

/*
 * recognized formats :
 * TGA_TYPE_MAPPED           nope
 * TGA_TYPE_TRUE_COLOR       24,32 bits ok
 * TGA_TYPE_GRAY             nope
 * TGA_TYPE_MAPPED_RLE       nope
 * TGA_TYPE_TRUE_COLOR_RLE   24,32 bits ok, only image_origin=0,2
 * TGA_TYPE_GRAY_RLE         nope
*/

/* The image types */
#define TGA_TYPE_EMPTY            0
#define TGA_TYPE_MAPPED           1
#define TGA_TYPE_TRUE_COLOR       2
#define TGA_TYPE_GRAY             3
#define TGA_TYPE_MAPPED_RLE       9
#define TGA_TYPE_TRUE_COLOR_RLE  10
#define TGA_TYPE_GRAY_RLE        11

int Img::import_tga (const char *filename)
{
	FILE *ptr;
	unsigned int w, h;
	unsigned int i,j;
	unsigned char *color_map;

	if ((ptr = fopen (filename, "rb")) == NULL)
	{
		printf ("unable to open %s\n", filename);
		return -1;
	}
  
	//
	unsigned char id_length;
	unsigned char color_map_type;
	unsigned char image_type;

	fread (&id_length, sizeof(unsigned char), 1, ptr);
	fread (&color_map_type, sizeof(unsigned char), 1, ptr);
	fread (&image_type, sizeof(unsigned char), 1, ptr);

	// color map specifications
	unsigned short color_map_origin;
	unsigned short color_map_length;
	unsigned char  color_map_entry_size;

	fread (&color_map_origin, sizeof(unsigned short), 1, ptr);
	fread (&color_map_length, sizeof(unsigned short), 1, ptr);
	fread (&color_map_entry_size, sizeof(unsigned char), 1, ptr);

	// image specification
	unsigned short x_origin;
	unsigned short y_origin;
	unsigned short width;
	unsigned short height;
	unsigned char  pixel_depth; // 8, 16, 24, 32
	unsigned char  image_descriptor;
	unsigned char  image_origin;
	unsigned char  alpha_channel_bits;
	unsigned char  *id;

	fread (&x_origin, sizeof(unsigned short), 1, ptr);
	fread (&y_origin, sizeof(unsigned short), 1, ptr);
	fread (&width, sizeof(unsigned short), 1, ptr);
	fread (&height, sizeof(unsigned short), 1, ptr);
	fread (&pixel_depth, sizeof(unsigned char), 1, ptr);
	fread (&image_descriptor, sizeof(unsigned char), 1, ptr);
	image_origin = (image_descriptor&0x30)>>4;
	alpha_channel_bits = (image_descriptor&0xf);

	// image id
	if (id_length != 0)
	{
		id = (unsigned char*)malloc((id_length+1)*sizeof(unsigned char));
		fread (id, sizeof(unsigned char), id_length, ptr);
	}
  
	// image data
	resize_memory (width, height);
	w = width;
	h = height;
	if (color_map_type)
	{
		switch (color_map_entry_size)
		{
		case 8:
			color_map = new unsigned char[color_map_length];
			//datas = new unsigned char[w*h];
			break;
		case 24:
			color_map = new unsigned char[3*color_map_length];
			//datas = new unsigned char[3*w*h];
			break;
		case 32:
			color_map = new unsigned char[4*color_map_length];
			//datas = new unsigned char[4*w*h];
			break;
		}
		//assert (color_map);
		//assert (datas);
    }
	else
    {
/*
		switch (pixel_depth)
		{
		case 8:
			datas = new unsigned char[w*h];
			break;
		case 24:
			datas = new unsigned char[3*w*h];
			break;
		case 32:
			datas = new unsigned char[4*w*h];
			break;
		}
		assert (datas);
*/
	}

	/******************/
	/* TGA_TYPE_EMPTY */
	/******************/
	if (image_type == TGA_TYPE_EMPTY)
	{
		printf ("No data included :(\n");
		fclose (ptr);
		return -1;
	}

	/*******************/
	/* TGA_TYPE_MAPPED */
	/*******************/
	if (image_type == TGA_TYPE_MAPPED)
	{
		// read the color map
		for (i=0; i<color_map_length; i++)
		{
			if (color_map_entry_size == 24)
			{
				fread (&color_map[3*i+2], sizeof(unsigned char), 1, ptr);
				fread (&color_map[3*i+1], sizeof(unsigned char), 1, ptr);
				fread (&color_map[3*i], sizeof(unsigned char), 1, ptr);
			}
		}
      
      // read the datas
      switch (image_origin)
	{
	case 0:
	  for (j=h-1; j>=0; j--)
	    for (i=0; i<w; i++)
	      {
			unsigned char index;
			fread (&index, sizeof(unsigned char), 1, ptr);
			if (color_map_entry_size == 24)
				  set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	case 1:
	  for (j=h-1; j>=0; j--)
	    for (i=w-1; i>=0; i--)
	      {
			unsigned char index;
			fread (&index, sizeof(unsigned char), 1, ptr);
			if (color_map_entry_size == 24)
			  set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	case 2:
	  for (j=0; j<h; j++)
	    for (i=0; i<w; i++)
	      {
		unsigned char index;
		fread (&index, sizeof(unsigned char), 1, ptr);
		if (color_map_entry_size == 24)
			  set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	case 3:
	  for (j=0; j<h; j++)
	    for (i=w-1; i>=0; i--)
	      {
		unsigned char index;
		fread (&index, sizeof(unsigned char), 1, ptr);
		if (color_map_entry_size == 24)
		  set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	default:
	  printf ("WARNING!!! bad \"image origin\" (%d)\n", image_origin);
	  fclose (ptr);
	  return -1;
	}
    }

  /***********************/
  /* TGA_TYPE_TRUE_COLOR */
  /***********************/
	unsigned char c[4];
  if (image_type == TGA_TYPE_TRUE_COLOR)
    {
      switch (image_origin)
	{
	case 0:
	  for (j=h-1; j>=0; j--)
	    for (i=0; i<w; i++)
	      {
			  if (pixel_depth == 24)
			  {
				  fread (c, sizeof(unsigned char), 3, ptr);
				  set_pixel (i, j, c[0], c[1], c[2], 255);
			  }
			  else if (pixel_depth == 32)
			  {
				  fread (c, sizeof(unsigned char), 4, ptr);
				  set_pixel (i, j, c[0], c[1], c[2], c[3]);
			  }
	      }

	  break;
	case 1:
	  for (j=h-1; j>=0; j--)
	    for (i=w-1; i>=0; i--)
	      {
		if (pixel_depth == 24)
		  {
			  unsigned char r, g, b;
		    fread (&b, sizeof(unsigned char), 1, ptr);
		    fread (&g, sizeof(unsigned char), 1, ptr);
		    fread (&r, sizeof(unsigned char), 1, ptr);
			  set_pixel (i, j, r, g, b, 255);
		  }
		if (pixel_depth == 32)
		  {
			  unsigned char r, g, b, a;
		    fread (&b, sizeof(unsigned char), 1, ptr);
		    fread (&g, sizeof(unsigned char), 1, ptr);
		    fread (&r, sizeof(unsigned char), 1, ptr);
		    fread (&a, sizeof(unsigned char), 1, ptr);
			  set_pixel (i, j, r, g, b, a);
		  }
	      }
	  break;
	case 2:
	  for (j=0; j<h; j++)
	    for (i=0; i<w; i++)
	      {
		if (pixel_depth == 24)
		  {
			  unsigned char r, g, b;
		    fread (&b, sizeof(unsigned char), 1, ptr);
		    fread (&g, sizeof(unsigned char), 1, ptr);
		    fread (&r, sizeof(unsigned char), 1, ptr);
			  set_pixel (i, j, r, g, b, 255);
		  }
		if (pixel_depth == 32)
		  {
			  unsigned char r, g, b, a;
		    fread (&b, sizeof(unsigned char), 1, ptr);
		    fread (&g, sizeof(unsigned char), 1, ptr);
		    fread (&r, sizeof(unsigned char), 1, ptr);
		    fread (&a, sizeof(unsigned char), 1, ptr);
			  set_pixel (i, j, r, g, b, a);
		  }
	      }
	  break;
	case 3:
	  for (j=0; j<h; j++)
	    for (i=w-1; i>=0; i--)
	      {
			if (pixel_depth == 24)
			  {
				unsigned char r, g, b;
				fread (&b, sizeof(unsigned char), 1, ptr);
				fread (&g, sizeof(unsigned char), 1, ptr);
				fread (&r, sizeof(unsigned char), 1, ptr);
				set_pixel (i, j, r, g, b, 255);
			  }
			if (pixel_depth == 32)
			  {
				unsigned char r, g, b, a;
				fread (&b, sizeof(unsigned char), 1, ptr);
				fread (&g, sizeof(unsigned char), 1, ptr);
				fread (&r, sizeof(unsigned char), 1, ptr);
				fread (&a, sizeof(unsigned char), 1, ptr);
				set_pixel (i, j, r, g, b, a);
			  }
	      }
	  break;
	default:
	  printf ("WARNING!!! bad \"image origin\" (%d)\n", image_origin);
	  fclose (ptr);
	  return -1;
	}
    }

  /*****************/
  /* TGA_TYPE_GRAY */
  /*****************/
  if (image_type == TGA_TYPE_GRAY)
    {
      printf ("TGA_TYPE_GRAY not yet implemented\n");
      fclose (ptr);
      return -1;
    }

  /***********************/
  /* TGA_TYPE_MAPPED_RLE */
  /***********************/
  if (image_type == TGA_TYPE_MAPPED_RLE)
    {
      printf ("TGA_TYPE_MAPPED_RLE not yet implemented\n");
      fclose (ptr);
      return -1;
    }
  
	/***************************/
	/* TGA_TYPE_TRUE_COLOR_RLE */
	/***************************/
	if (image_type == TGA_TYPE_TRUE_COLOR_RLE)
    {
      unsigned int pixels_read = 0;
      unsigned char repetition_block;
      unsigned char pixel_count;
      int i = 0;
      unsigned char r,g,b,a;

      switch (image_origin)
	  {
	case 0:
	  {
	    unsigned short current_line = height-1;
	    unsigned short pixels_on_line = 0;
	    while (pixels_read < w*h)
	      {
			// read a packet
			fread (&repetition_block, sizeof(unsigned char), 1, ptr);
			if ((repetition_block&0x80) == 0x80)
			{
				// run-length packet
				pixel_count = repetition_block - 0x80 + 1;
				fread (&b, sizeof(unsigned char), 1, ptr);
				fread (&g, sizeof(unsigned char), 1, ptr);
				fread (&r, sizeof(unsigned char), 1, ptr);
				a = 255;
				if (pixel_depth == 32)
					fread (&a, sizeof(unsigned char), 1, ptr);
				for (i=0; i<pixel_count; i++)
					set_pixel (pixels_on_line+i, current_line, r, g, b, a);
			}
			else
			{
				// non-run-length packet
				pixel_count = repetition_block + 1;
				for (i=0; i<pixel_count; i++)
				{
					fread (&b, sizeof(unsigned char), 1, ptr);
					fread (&g, sizeof(unsigned char), 1, ptr);
					fread (&r, sizeof(unsigned char), 1, ptr);
					a = 255;
					if (pixel_depth == 32)
						fread (&a, sizeof(unsigned char), 1, ptr);
					set_pixel (pixels_on_line+i, current_line, r, g, b, a);
				}
			}
			pixels_on_line += pixel_count;
			if (pixels_on_line >= width)
			  {
				current_line--;
				pixels_on_line = 0;
			  }
			pixels_read += pixel_count;
	      }
	    printf ("pixels read: %d\n", pixels_read);
	    break;
	  }
	case 1:
	  printf ("not yet implemented :(\n");
	  fclose (ptr);
	  return -1;
	  break;
	case 2:
	  while (pixels_read < w*h)
	    {
	      // read a packet
	      fread (&repetition_block, sizeof(unsigned char), 1, ptr);
	      if ((repetition_block&0x80) == 0x80)
		{
		  // run-length packet
		  pixel_count = repetition_block - 0x80 + 1;
		  fread (&b, sizeof(unsigned char), 1, ptr);
		  fread (&g, sizeof(unsigned char), 1, ptr);
		  fread (&r, sizeof(unsigned char), 1, ptr);
		  a = 255;
		  if (pixel_depth == 32)
		    fread (&a, sizeof(unsigned char), 1, ptr);
		  for (i=0; i<pixel_count; i++)
		    {
				m_pPixels[4*(pixels_read+i)+3] = a;
				m_pPixels[4*(pixels_read+i)+2] = b;
				m_pPixels[4*(pixels_read+i)+1] = g;
				m_pPixels[4*(pixels_read+i)+0] = r;
		    }
		}
	      else
		{
		  // non-run-length packet
		  pixel_count = repetition_block + 1;
		  for (i=0; i<pixel_count; i++)
		    {
				fread (&b, sizeof(unsigned char), 1, ptr);
				fread (&g, sizeof(unsigned char), 1, ptr);
				fread (&r, sizeof(unsigned char), 1, ptr);
				a = 255;
				if (pixel_depth == 32)
					fread (&a, sizeof(unsigned char), 1, ptr);
				
				m_pPixels[4*(pixels_read+i)+3] = a;
				m_pPixels[4*(pixels_read+i)+2] = b;
				m_pPixels[4*(pixels_read+i)+1] = g;
				m_pPixels[4*(pixels_read+i)+0] = r;
		    }
		}
	      pixels_read += pixel_count;
	    }
	  printf ("pixels read: %d\n", pixels_read);
	  break;
	case 3:
	  printf ("not yet implemented :(\n");
	  fclose (ptr);
	  return -1;
	  break;
	default:
	  printf ("WARNING!!! bad \"image origin\" (%d)\n", image_origin);
	  fclose (ptr);
	  return -1;
	}
    }
      
  /*********************/
  /* TGA_TYPE_GRAY_RLE */
  /*********************/
  if (image_type == TGA_TYPE_GRAY_RLE)
    {
      printf ("TGA_TYPE_GRAY_RLE not yet implemented\n");
      fclose (ptr);
      return -1;
    }

  /* On compte ce qui reste */
  i=0;
  while (!feof(ptr))
    {
      unsigned char c;
      fread (&c, sizeof(unsigned char), 1, ptr);
      i++;
    }
  printf ("reste: %d octets\n", i);
  
  fclose (ptr);
  
  return 0;
}

void Img::compute_colormap (unsigned char **_colormap, unsigned short *_colormap_length)
{
  unsigned int i,j;
  unsigned short colormap_length = 0;
  unsigned char *colormap = (unsigned char*)malloc(3*sizeof(unsigned char));
  colormap_length++;
  colormap[0] = m_pPixels[0];
  colormap[1] = m_pPixels[1];
  colormap[2] = m_pPixels[2];
  for (i=1; i<m_iWidth*m_iHeight; i++)
    {
      unsigned char r_walk = m_pPixels[4*i];
      unsigned char g_walk = m_pPixels[4*i+1];
      unsigned char b_walk = m_pPixels[4*i+2];
      for (j=0; j<colormap_length; j++)
	{
	  if (colormap[3*j]   == r_walk &&
	      colormap[3*j+1] == g_walk &&
	      colormap[3*j+2] == b_walk)
	    break;
	}
      if (j == colormap_length)
	{
	  colormap = (unsigned char*)realloc(colormap,3*(colormap_length+1)*sizeof(unsigned char));
	  colormap[3*colormap_length]   = r_walk;
	  colormap[3*colormap_length+1] = g_walk;
	  colormap[3*colormap_length+2] = b_walk;
	  colormap_length++;
	}
    }
  *_colormap_length = colormap_length;
  *_colormap = colormap;
}

int Img::export_tga (const char *filename)
{
	int mode = TGA_TYPE_TRUE_COLOR;

  FILE *ptr;
  unsigned int w,h;
  unsigned int i,j;

  ptr = fopen (filename, "wb");
  if (!ptr) return -1;
  w = m_iWidth;
  h = m_iHeight;

  unsigned short  x_origin, y_origin;
  unsigned char pixel_depth;
  unsigned char  image_descriptor;

  switch (mode)
    {
    case TGA_TYPE_TRUE_COLOR:
		{
      // header
      unsigned char *id = (unsigned char*)_strdup ("created by cgimg\0");
      size_t id_length = strlen ((char*)id)+1;
      fwrite (&id_length, sizeof(unsigned char), 1, ptr);
      
      unsigned char color_map_type = 0;
      fwrite (&color_map_type, sizeof(unsigned char), 1, ptr);
      
      unsigned char image_type = TGA_TYPE_TRUE_COLOR;
      fwrite (&image_type, sizeof(unsigned char), 1, ptr);
      
      // color map specifications
      unsigned short color_map_origin     = 0;
      unsigned short color_map_length     = 0;
      unsigned char  color_map_entry_size = 0;
      fwrite (&color_map_origin, sizeof(unsigned short), 1, ptr);
      fwrite (&color_map_length, sizeof(unsigned short), 1, ptr);
      fwrite (&color_map_entry_size, sizeof(unsigned char), 1, ptr);
      
      // image specification
      x_origin = y_origin = 0;
      fwrite (&x_origin, sizeof(unsigned short), 1, ptr);
      fwrite (&y_origin, sizeof(unsigned short), 1, ptr);
      fwrite (&m_iWidth, sizeof(unsigned short), 1, ptr);
      fwrite (&m_iHeight, sizeof(unsigned short), 1, ptr);
      pixel_depth = 24;   // RGB
      //pixel_depth = 32; // RGBA
      fwrite (&pixel_depth, sizeof(unsigned char), 1, ptr);
      image_descriptor = (0x02)<<4|(0x00);
      fwrite (&image_descriptor, sizeof(unsigned char), 1, ptr);
      
      if (id_length != 0)
	fwrite (id, sizeof(unsigned char), id_length, ptr);
      
      for (j=0; j<h; j++)
	for (i=0; i<w; i++)
	  {
	    if (pixel_depth == 24)
	      {
		fwrite (&m_pPixels[4*(j*w+i)+2], sizeof(unsigned char), 1, ptr);
		fwrite (&m_pPixels[4*(j*w+i)+1], sizeof(unsigned char), 1, ptr);
		fwrite (&m_pPixels[4*(j*w+i)], sizeof(unsigned char), 1, ptr);
	      }
	    if (pixel_depth == 32)
	      {
		fwrite (&m_pPixels[4*(j*w+i)+2], sizeof(unsigned char), 1, ptr);
		fwrite (&m_pPixels[4*(j*w+i)+1], sizeof(unsigned char), 1, ptr);
		fwrite (&m_pPixels[4*(j*w+i)], sizeof(unsigned char), 1, ptr);
		fwrite (&m_pPixels[4*(j*w+i)+3], sizeof(unsigned char), 1, ptr);
	      }
	  }
		}
	break;
    case TGA_TYPE_MAPPED:
		{
      // header
      unsigned char *id = (unsigned char*)_strdup ("created by cl\0");
      size_t id_length = strlen ((char*)id)+1;
      fwrite (&id_length, sizeof(unsigned char), 1, ptr);
      
      unsigned char color_map_type = 1;
      fwrite (&color_map_type, sizeof(unsigned char), 1, ptr);
      
      unsigned char image_type = TGA_TYPE_MAPPED;
      fwrite (&image_type, sizeof(unsigned char), 1, ptr);
      
      // color map specifications
      unsigned char *colormap=NULL;
      unsigned short colormap_length;
      compute_colormap (&colormap, &colormap_length);
      
      unsigned short color_map_origin     = 0;
      unsigned short color_map_length     = colormap_length;
      unsigned char color_map_entry_size = 24;
      fwrite (&color_map_origin, sizeof(unsigned short), 1, ptr);
      fwrite (&color_map_length, sizeof(unsigned short), 1, ptr);
      fwrite (&color_map_entry_size, sizeof(unsigned char), 1, ptr);
      
      // image specification
      x_origin = y_origin = 0;
      fwrite (&x_origin, sizeof(unsigned short), 1, ptr);
      fwrite (&y_origin, sizeof(unsigned short), 1, ptr);
      fwrite (&m_iWidth, sizeof(unsigned short), 1, ptr);
      fwrite (&m_iHeight, sizeof(unsigned short), 1, ptr);
      pixel_depth = 8; // index
      fwrite (&pixel_depth, sizeof(unsigned char), 1, ptr);
      image_descriptor = (0x02)<<4|(0x00);
      fwrite (&image_descriptor, sizeof(unsigned char), 1, ptr);
      
      if (id_length != 0)
	fwrite (id, sizeof(unsigned char), id_length, ptr);

      // write the color map
      for (i=0; i<colormap_length; i++)
	{
	  if (color_map_entry_size == 24)
	    {
	      fwrite (&colormap[3*i+2], sizeof(unsigned char), 1, ptr);
	      fwrite (&colormap[3*i+1], sizeof(unsigned char), 1, ptr);
	      fwrite (&colormap[3*i], sizeof(unsigned char), 1, ptr);
	    }
	}
      
      // write the datas
      for (j=0; j<h; j++)
	for (i=0; i<w; i++)
	  for (int index=0; index<colormap_length; index++)
	    {
	      if (m_pPixels[4*(j*w+i)]   == colormap[3*index] &&
		  m_pPixels[4*(j*w+i)+1] == colormap[3*index+1] &&
		  m_pPixels[4*(j*w+i)+2] == colormap[3*index+2])
		{
		  fwrite (&index, sizeof(unsigned char), 1, ptr);
		  break;
		}
	    }
		}
		break;
	default:
		break;
    }
  
  fclose (ptr);

  return 0;
}
