#include <stdio.h>
#include <string.h>
#include <filesystem>
#include <string>

#include "image.h"

int Img::load (char const *filename, char const *path)
{
	m_iWidth = 0;
	m_iHeight = 0;
	m_pPixels = nullptr;

	if (!filename)
		return -1;

	std::filesystem::path fullPath;
	if (path)
		fullPath = std::filesystem::path(path) / filename;
	else
		fullPath = std::filesystem::path(filename);

	std::string ext = fullPath.extension().string();

#ifdef PNG
	if (ext == ".png")
	     return import_png (fullPath.string().c_str());
#endif
#ifdef JPEGLIB
	if (ext == ".jpg")
	     return import_jpg (fullPath.string().c_str());
#endif
	if (ext == ".bmp")
	     return import_bmp (fullPath.string().c_str());
	if (ext == ".tga")
	     return import_tga (fullPath.string().c_str());
	if (ext == ".pbm" || ext == ".pgm" || ext == ".ppm")
	     return import_pnm (fullPath.string().c_str());

	return -1;
}

int Img::save (char const *filename)
{
	if (!filename)
		return -1;

	std::string ext = std::filesystem::path(filename).extension().string();

#ifdef PNG
	if (ext == ".png")
	     return export_png (filename);
#endif
#ifdef JPEGLIB
	if (ext == ".jpg")
	     return export_jpg (filename);
#endif
	if (ext == ".bmp")
	     return export_bmp (filename);
	if (ext == ".tga")
	     return export_tga (filename);
	if (ext == ".pbm" || ext == ".pgm" || ext == ".ppm")
	     return export_pnm (filename);

	return -1;
}
