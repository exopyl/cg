#include <stdio.h>
#include <string.h>

#include "image.h"

int Img::load (char const *filename, char const *path)
{
	m_iWidth = 0;
	m_iHeight = 0;
	m_pPixels = NULL;

	if (!filename)
		return -1;

	char filename_full[4096];
	if (path)
		sprintf (filename_full, "%s/%s", path, filename);
	else
		sprintf (filename_full, "%s", filename);

#ifdef PNG
	if (strcmp (filename+(strlen(filename)-4), ".png") == 0)
	     return import_png (filename_full);
#endif
#ifdef JPEGLIB
	if (strcmp (filename+(strlen(filename)-4), ".jpg") == 0)
	     return import_jpg (filename_full);
#endif
	if (strcmp (filename+(strlen(filename)-4), ".bmp") == 0)
	     return import_bmp (filename_full);
	if (strcmp (filename+(strlen(filename)-4), ".tga") == 0)
	     return import_tga (filename_full);
	if (strcmp (filename+(strlen(filename)-4), ".pbm") == 0 ||
	    strcmp (filename+(strlen(filename)-4), ".pgm") == 0 ||
	    strcmp (filename+(strlen(filename)-4), ".ppm") == 0	)
	     return import_pnm (filename_full);

	return -1;
}

int Img::save (char const *filename)
{
	if (!filename)
		return -1;

#ifdef PNG
	if (strcmp (filename+(strlen(filename)-4), ".png") == 0)
	     return export_png (filename);
#endif
#ifdef JPEGLIB
	if (strcmp (filename+(strlen(filename)-4), ".jpg") == 0)
	     return export_jpg (filename);
#endif
	if (strcmp (filename+(strlen(filename)-4), ".bmp") == 0)
	     return export_bmp (filename);
	if (strcmp (filename+(strlen(filename)-4), ".tga") == 0)
	     return export_tga (filename);
	if (strcmp (filename+(strlen(filename)-4), ".pbm") == 0 ||
	    strcmp (filename+(strlen(filename)-4), ".pgm") == 0 ||
	    strcmp (filename+(strlen(filename)-4), ".ppm") == 0	)
	     return export_pnm (filename);

	return -1;
}
