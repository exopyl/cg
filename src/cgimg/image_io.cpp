#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <string>

#include "image.h"
#include "image_io.h"

// Public delegators — kept on Img so every caller of img.load()/img.save()
// stays unchanged; the real dispatch lives in ImgIO below.
int Img::load (char const *filename, char const *path) { return ImgIO::load (*this, filename, path); }
int Img::save (char const *filename)                   { return ImgIO::save (*this, filename); }

int ImgIO::load (Img& img, char const *filename, char const *path)
{
	// Libère l'éventuel contenu précédent (load sur un Img déjà chargé) avant de
	// repartir de zéro — sinon l'ancien buffer/palette fuit.
	if (img.m_pPixels)  free (img.m_pPixels);
	if (img.m_pPalette) { delete img.m_pPalette; img.m_pPalette = nullptr; }
	img.m_iWidth = 0;
	img.m_iHeight = 0;
	img.m_pPixels = nullptr;

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
	     return import_png (img, fullPath.string().c_str());
#endif
#ifdef JPG
	if (ext == ".jpg" || ext == ".jpeg")
	     return import_jpg (img, fullPath.string().c_str());
#endif
	if (ext == ".bmp")
	     return import_bmp (img, fullPath.string().c_str());
	if (ext == ".tga")
	     return import_tga (img, fullPath.string().c_str());
	if (ext == ".pbm" || ext == ".pgm" || ext == ".ppm")
	     return import_pnm (img, fullPath.string().c_str());

	return -1;
}

int ImgIO::save (Img& img, char const *filename)
{
	if (!filename)
		return -1;

	std::string ext = std::filesystem::path(filename).extension().string();

#ifdef PNG
	if (ext == ".png")
	     return export_png (img, filename);
#endif
	// Pas d'export JPEG : stb ne fournit que le décodeur (cf. export_png).
	if (ext == ".bmp")
	     return export_bmp (img, filename);
	if (ext == ".tga")
	     return export_tga (img, filename);
	if (ext == ".pbm" || ext == ".pgm" || ext == ".ppm")
	     return export_pnm (img, filename);

	return -1;
}
