#include "image.h"
#ifdef PNG

#ifndef WIN32
#include <png.h>

int Img::import_png (const char *filename)
{
	int x, y;
	
	int width, height;
	png_byte color_type;
	png_byte bit_depth;
	
	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep * row_pointers;

//

        png_byte header[8]; // 8 is the maximum size that can be checked

        // open file
        FILE *fp = fopen(filename, "rb");
        if (!fp)
	{
                printf ("[read_png_file] File %s could not be opened for reading\n", filename);
		return -1;
	}

	// test if it is a png file
        fread(header, 1, 8, fp);
        if (png_sig_cmp(header, 0, 8))
	{
                printf ("[read_png_file] File %s is not recognized as a PNG file\n", filename);
		return -1;
	}

        /* initialize stuff */
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr)
	{
                printf ("[read_png_file] png_create_read_struct failed\n");
		return -1;
	}

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
	{
                printf ("[read_png_file] png_create_info_struct failed\n");
		return -1;
	}

        if (setjmp(png_jmpbuf(png_ptr)))
	{
                printf ("[read_png_file] Error during init_io\n");
		return -1;
	}

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);

        png_read_info(png_ptr, info_ptr);

        width = png_get_image_width(png_ptr, info_ptr);
        height = png_get_image_height(png_ptr, info_ptr);
        color_type = png_get_color_type(png_ptr, info_ptr);
        bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        number_of_passes = png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        // read file
        if (setjmp(png_jmpbuf(png_ptr)))
	{
                printf ("[read_png_file] Error during read_image\n");
		return -1;
	}

        row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
        for (y=0; y<height; y++)
                row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));

        png_read_image(png_ptr, row_pointers);
	
	// convert to our structure
	resize_memory (width, height);
        for (y=0; y<height; y++)
	{
                png_byte* row = row_pointers[y];
                for (x=0; x<width; x++)
		{
			png_byte* ptr = NULL;
                        if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA)
			{
				ptr = &(row[x*4]);
				set_pixel (x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
			}
                       if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB)
			{
				ptr = &(row[x*3]);
				set_pixel (x, y, ptr[0], ptr[1], ptr[2], 255);

			}
                }
        }

        // cleanup heap allocation
        for (y=0; y<height; y++)
                free(row_pointers[y]);
        free(row_pointers);

        fclose(fp);

	return 0;
}

int Img::export_png (const char *filename)
{
	png_byte color_type = PNG_COLOR_TYPE_RGBA;
	png_byte bit_depth = 8;
	
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep * row_pointers = NULL;

        // create file
        FILE *fp = fopen(filename, "wb");
        if (!fp)
	{
		printf ("[write_png_file] File %s could not be opened for writing\n", filename);
		return -1;
	}


        // initialize stuff
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr)
	{
                printf ("[write_png_file] png_create_write_struct failed\n");
		return -1;
	}

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
	{
                printf ("[write_png_file] png_create_info_struct failed\n");
		return -1;
	}

        if (setjmp(png_jmpbuf(png_ptr)))
	{
                printf ("[write_png_file] Error during init_io\n");
		return -1;
	}
        png_init_io(png_ptr, fp);

        // write header
        if (setjmp(png_jmpbuf(png_ptr)))
	{
                printf ("[write_png_file] Error during writing header\n");
		return -1;
	}
        png_set_IHDR(png_ptr, info_ptr, m_iWidth, m_iHeight,
                     bit_depth, color_type, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png_ptr, info_ptr);

	// convert m_pPixels to row_pointers
        row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * m_iHeight);
        for (int y=0; y<m_iHeight; y++)
	{
                row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));
		png_byte *row = row_pointers[y];
		unsigned char r, g, b, a;
		for (int x=0; x<m_iWidth; x++)
		{
			get_pixel (x, y, &r, &g, &b, &a);
			*row++ = r;
			*row++ = g;
			*row++ = b;
			*row++ = a;
		}
	}

        // write bytes
        if (setjmp(png_jmpbuf(png_ptr)))
	{
                printf ("[write_png_file] Error during writing bytes\n");
		return -1;
	}
        png_write_image(png_ptr, row_pointers);


        // end write
        if (setjmp(png_jmpbuf(png_ptr)))
	{
		printf ("[write_png_file] Error during end of write\n");
		return -1;
	}
        png_write_end(png_ptr, NULL);

        // cleanup heap allocation
        for (int y=0; y<m_iHeight; y++)
                free(row_pointers[y]);
        free(row_pointers);

        fclose(fp);

	return 0;
}
#else
int Img::import_png (const char *filename)
{
	printf ("png module not loaded in win32\n");
	return -1;
}
int Img::export_png (const char *filename)
{
	printf ("png module not loaded in win32\n");
	return -1;
}
#endif // WIN32

#endif // PNG
