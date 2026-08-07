#include "image.h"
#include "image_io.h"

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

int ImgIO::import_tga (Img& img, const char *filename)
{
	FILE *ptr;
	int w, h;
	int i,j;
	unsigned char *color_map = nullptr;

	if ((ptr = fopen (filename, "rb")) == nullptr)
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
	unsigned char  *id = nullptr;

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
		free (id);   // l'identifiant n'est pas exploité ensuite : libéré aussitôt
		id = nullptr;
	}

	// image data
	img.resize_memory (width, height);
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
				  img.set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	case 1:
	  for (j=h-1; j>=0; j--)
	    for (i=w-1; i>=0; i--)
	      {
			unsigned char index;
			fread (&index, sizeof(unsigned char), 1, ptr);
			if (color_map_entry_size == 24)
			  img.set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	case 2:
	  for (j=0; j<h; j++)
	    for (i=0; i<w; i++)
	      {
		unsigned char index;
		fread (&index, sizeof(unsigned char), 1, ptr);
		if (color_map_entry_size == 24)
			  img.set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	case 3:
	  for (j=0; j<h; j++)
	    for (i=w-1; i>=0; i--)
	      {
		unsigned char index;
		fread (&index, sizeof(unsigned char), 1, ptr);
		if (color_map_entry_size == 24)
		  img.set_pixel (i, j, color_map[3*index], color_map[3*index+1], color_map[3*index+2], 255);
	      }
	  break;
	default:
	  printf ("WARNING!!! bad \"image origin\" (%d)\n", image_origin);
	  fclose (ptr);
	  delete[] color_map;            // était fuité sur ce chemin d'erreur
	  return -1;
	}

      // Palette TGA : allouée sous color_map_type, utilisée uniquement par ce
      // bloc MAPPED. Libérée ici (chemin nominal) — était fuitée.
      delete[] color_map;
      color_map = nullptr;
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
				  // TGA stores BGR: swap to RGB (as cases 1/2/3 already do).
				  img.set_pixel (i, j, c[2], c[1], c[0], 255);
			  }
			  else if (pixel_depth == 32)
			  {
				  fread (c, sizeof(unsigned char), 4, ptr);
				  img.set_pixel (i, j, c[2], c[1], c[0], c[3]);
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
			  img.set_pixel (i, j, r, g, b, 255);
		  }
		if (pixel_depth == 32)
		  {
			  unsigned char r, g, b, a;
		    fread (&b, sizeof(unsigned char), 1, ptr);
		    fread (&g, sizeof(unsigned char), 1, ptr);
		    fread (&r, sizeof(unsigned char), 1, ptr);
		    fread (&a, sizeof(unsigned char), 1, ptr);
			  img.set_pixel (i, j, r, g, b, a);
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
			  img.set_pixel (i, j, r, g, b, 255);
		  }
		if (pixel_depth == 32)
		  {
			  unsigned char r, g, b, a;
		    fread (&b, sizeof(unsigned char), 1, ptr);
		    fread (&g, sizeof(unsigned char), 1, ptr);
		    fread (&r, sizeof(unsigned char), 1, ptr);
		    fread (&a, sizeof(unsigned char), 1, ptr);
			  img.set_pixel (i, j, r, g, b, a);
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
				img.set_pixel (i, j, r, g, b, 255);
			  }
			if (pixel_depth == 32)
			  {
				unsigned char r, g, b, a;
				fread (&b, sizeof(unsigned char), 1, ptr);
				fread (&g, sizeof(unsigned char), 1, ptr);
				fread (&r, sizeof(unsigned char), 1, ptr);
				fread (&a, sizeof(unsigned char), 1, ptr);
				img.set_pixel (i, j, r, g, b, a);
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
	    int current_line = height-1;
	    unsigned short pixels_on_line = 0;
	    while (pixels_read < w*h && current_line >= 0)
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
				for (i=0; i<pixel_count && current_line >= 0; i++)
				{
					img.set_pixel (pixels_on_line, current_line, r, g, b, a);
					pixels_on_line++;
					if (pixels_on_line >= width)
					{
						current_line--;
						pixels_on_line = 0;
					}
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
					if (current_line >= 0)
					{
						img.set_pixel (pixels_on_line, current_line, r, g, b, a);
						pixels_on_line++;
						if (pixels_on_line >= width)
						{
							current_line--;
							pixels_on_line = 0;
						}
					}
				}
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
				img.m_pPixels[4*(pixels_read+i)+3] = a;
				img.m_pPixels[4*(pixels_read+i)+2] = b;
				img.m_pPixels[4*(pixels_read+i)+1] = g;
				img.m_pPixels[4*(pixels_read+i)+0] = r;
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

				img.m_pPixels[4*(pixels_read+i)+3] = a;
				img.m_pPixels[4*(pixels_read+i)+2] = b;
				img.m_pPixels[4*(pixels_read+i)+1] = g;
				img.m_pPixels[4*(pixels_read+i)+0] = r;
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

void ImgIO::compute_colormap (Img& img, unsigned char **_colormap, unsigned short *_colormap_length)
{
  unsigned int i,j;
  unsigned short colormap_length = 0;
  unsigned char *colormap = (unsigned char*)malloc(3*sizeof(unsigned char));
  colormap_length++;
  colormap[0] = img.m_pPixels[0];
  colormap[1] = img.m_pPixels[1];
  colormap[2] = img.m_pPixels[2];
  for (i=1; i<img.m_iWidth*img.m_iHeight; i++)
    {
      unsigned char r_walk = img.m_pPixels[4*i];
      unsigned char g_walk = img.m_pPixels[4*i+1];
      unsigned char b_walk = img.m_pPixels[4*i+2];
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

int ImgIO::export_tga (Img& img, const char *filename)
{
	int mode = TGA_TYPE_TRUE_COLOR;

  FILE *ptr;
  unsigned int w,h;
  unsigned int i,j;

  ptr = fopen (filename, "wb");
  if (!ptr) return -1;
  w = img.m_iWidth;
  h = img.m_iHeight;

  unsigned short  x_origin, y_origin;
  unsigned char pixel_depth;
  unsigned char  image_descriptor;

  switch (mode)
    {
    case TGA_TYPE_TRUE_COLOR:
		{
      // header
      unsigned char *id = (unsigned char*)strdup ("created by cgimg\0");
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
      fwrite (&img.m_iWidth, sizeof(unsigned short), 1, ptr);
      fwrite (&img.m_iHeight, sizeof(unsigned short), 1, ptr);
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
		fwrite (&img.m_pPixels[4*(j*w+i)+2], sizeof(unsigned char), 1, ptr);
		fwrite (&img.m_pPixels[4*(j*w+i)+1], sizeof(unsigned char), 1, ptr);
		fwrite (&img.m_pPixels[4*(j*w+i)], sizeof(unsigned char), 1, ptr);
	      }
	    if (pixel_depth == 32)
	      {
		fwrite (&img.m_pPixels[4*(j*w+i)+2], sizeof(unsigned char), 1, ptr);
		fwrite (&img.m_pPixels[4*(j*w+i)+1], sizeof(unsigned char), 1, ptr);
		fwrite (&img.m_pPixels[4*(j*w+i)], sizeof(unsigned char), 1, ptr);
		fwrite (&img.m_pPixels[4*(j*w+i)+3], sizeof(unsigned char), 1, ptr);
	      }
	  }
		}
	break;
    case TGA_TYPE_MAPPED:
		{
      // header
      unsigned char *id = (unsigned char*)strdup ("created by cl\0");
      size_t id_length = strlen ((char*)id)+1;
      fwrite (&id_length, sizeof(unsigned char), 1, ptr);

      unsigned char color_map_type = 1;
      fwrite (&color_map_type, sizeof(unsigned char), 1, ptr);

      unsigned char image_type = TGA_TYPE_MAPPED;
      fwrite (&image_type, sizeof(unsigned char), 1, ptr);

      // color map specifications
      unsigned char *colormap=nullptr;
      unsigned short colormap_length;
      compute_colormap (img, &colormap, &colormap_length);

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
      fwrite (&img.m_iWidth, sizeof(unsigned short), 1, ptr);
      fwrite (&img.m_iHeight, sizeof(unsigned short), 1, ptr);
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
	      if (img.m_pPixels[4*(j*w+i)]   == colormap[3*index] &&
		  img.m_pPixels[4*(j*w+i)+1] == colormap[3*index+1] &&
		  img.m_pPixels[4*(j*w+i)+2] == colormap[3*index+2])
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
