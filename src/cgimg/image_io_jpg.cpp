#include "image.h"
#ifdef JPEGLIB

//
// Reference :
// http://download.blender.org/source/chest/blender_2.03_tree/jpeg/example.c
//
#include <stdio.h>
#include <setjmp.h>
extern "C" {
#ifdef WIN32
#include "../../extern/jpeglib/jpeglib.h"
#else
#include <jpeglib.h>
#endif
}

//
// export jpg
//
int Img::export_jpg (const char *filename)
{
	int image_width  = m_iWidth;
	int image_height = m_iHeight;	// Number of rows in image
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	
	FILE * outfile;
	JSAMPROW row_pointer[1];
	int row_stride;
	
	// Step 1: allocate and initialize JPEG compression object
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	// Step 2: specify data destination (eg, a file)
	if ((outfile = fopen(filename, "wb")) == NULL)
	{
		fprintf(stderr, "can't open %s\n", filename);
		return -1;
	}
	jpeg_stdio_dest(&cinfo, outfile);

	// Step 3: set parameters for compression
	cinfo.image_width = image_width; 	// image width and height, in pixels
	cinfo.image_height = image_height;
	cinfo.input_components = 3;		// # of color components per pixel
	cinfo.in_color_space = JCS_RGB; 	// colorspace of input image

	jpeg_set_defaults(&cinfo);

	//jpeg_set_quality(&cinfo, quality, TRUE); // limit to baseline-JPEG values

	// Step 4: Start compressor
	jpeg_start_compress(&cinfo, TRUE);

	// Step 5: while (scan lines remain to be written)
	//         jpeg_write_scanlines(...);

	row_stride = image_width * 3;	// JSAMPLEs per row in image buffer

	unsigned char *stride = (unsigned char*)malloc(row_stride*sizeof(unsigned char));
	unsigned int current_height = 0;
	while (cinfo.next_scanline < cinfo.image_height)
	{
		unsigned char r, g, b, a;
		for (unsigned int i=0; i < m_iWidth; i++)
		{
			get_pixel (i, current_height, &r, &g, &b, &a);
			stride[3*i]   = r;
			stride[3*i+1] = g;
			stride[3*i+2] = b;
		}

		row_pointer[0] = stride;
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);

		current_height++;
	}
	free (stride);

	// Step 6: Finish compression
	jpeg_finish_compress(&cinfo);

	fclose(outfile);
	
	// Step 7: release JPEG compression object
	jpeg_destroy_compress(&cinfo);
	
	return 0;
}



/******************** JPEG DECOMPRESSION SAMPLE INTERFACE *******************/

struct my_error_mgr {
	struct jpeg_error_mgr pub;	// "public" fields
	jmp_buf setjmp_buffer;	// for return to caller
};

typedef struct my_error_mgr * my_error_ptr;
METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr) cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

//
// import jpg
//
int Img::import_jpg (const char *filename)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	
	FILE * infile;
	JSAMPARRAY buffer;
	int row_stride;
	
	if ((infile = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return 0;
	}
	
	// Step 1: allocate and initialize JPEG decompression object
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}
	jpeg_create_decompress(&cinfo);
	
	// Step 2: specify data source
	jpeg_stdio_src(&cinfo, infile);
	
	// Step 3: read file parameters with jpeg_read_header()
	(void) jpeg_read_header(&cinfo, TRUE);
	
	// Step 4: set parameters for decompression
	// no need
	
	// Step 5: Start decompressor
	(void) jpeg_start_decompress(&cinfo);
	
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
	
	// Step 6: while (scan lines remain to be read)
	//           jpeg_read_scanlines(...);
	int height = 0;
	resize_memory (cinfo.output_width, cinfo.output_height);
	int current_height = 0;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);
		
		// treat the data
		//put_scanline_someplace(buffer[0], row_stride);
		char *row = (char*)buffer[0];
		unsigned char r, g, b;
		for (int i=0; i < row_stride; i+=3)
		{
			r = *row++;
			g = *row++;
			b = *row++;
			set_pixel (i/3, current_height, r, g, b, 255);
		}
		current_height++;
	}

	// Step 7: Finish decompression
	(void) jpeg_finish_decompress(&cinfo);
	
	// Step 8: Release JPEG decompression object
	jpeg_destroy_decompress(&cinfo);
	
	fclose(infile);

	return 0;
}
#endif // JPEGLIB
