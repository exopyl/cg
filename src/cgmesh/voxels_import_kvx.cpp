#include "voxels_import_kvx.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// KVX = Ken Silverman's Build-engine / SLAB6 / Voxlap voxel model format.
// Layout: [numbytes][xsiz][ysiz][zsiz][xpiv][ypiv][zpiv]  (7 x int32)
//         [xoffset : (xsiz+1) x int32]
//         [xyoffset: xsiz x (ysiz+1) x uint16]
//         per (x,y) column: a run of "slabs" { ztop, zleng, cull, <zleng colour bytes> }
//         [256-colour palette : 768 bytes at end of file]
#define MAXXSIZ 256
#define MAXYSIZ 256
#define LIMZSIZ 255

Voxels* loadkvx (char *filename)
{
	FILE *fil = fopen (filename, "rb");
	if (!fil)
	{
		printf ("loadkvx: can't open %s\n", filename);
		return nullptr;
	}

	// header
	int32_t numbytes = 0, xsiz = 0, ysiz = 0, zsiz = 0;
	if (fread (&numbytes, 4, 1, fil) != 1 ||
	    fread (&xsiz,     4, 1, fil) != 1 ||
	    fread (&ysiz,     4, 1, fil) != 1 ||
	    fread (&zsiz,     4, 1, fil) != 1)
	{
		fclose (fil);
		return nullptr;
	}

	// validate the dimensions BEFORE allocating anything
	if (xsiz <= 0 || ysiz <= 0 || zsiz <= 0 ||
	    xsiz > MAXXSIZ || ysiz > MAXYSIZ || zsiz > LIMZSIZ)
	{
		printf ("loadkvx: invalid dimensions %dx%dx%d in %s\n", xsiz, ysiz, zsiz, filename);
		fclose (fil);
		return nullptr;
	}

	int32_t pivots[3];
	if (fread (pivots, 4, 3, fil) != 3) { fclose (fil); return nullptr; }

	// per-column offset tables
	int32_t xstart[MAXXSIZ + 1];
	unsigned short xyoffs[MAXXSIZ][MAXYSIZ + 1];
	if (fread (xstart, (size_t)(xsiz + 1) << 2, 1, fil) != 1) { fclose (fil); return nullptr; }
	for (int x = 0; x < xsiz; x++)
		if (fread (&xyoffs[x][0], (size_t)(ysiz + 1) << 1, 1, fil) != 1) { fclose (fil); return nullptr; }

	Voxels* pVoxels = new Voxels ((unsigned)xsiz, (unsigned)ysiz, (unsigned)zsiz);

	// decode the compressed slab runs, one column (x,y) at a time
	for (int x = 0; x < xsiz; x++)
		for (int y = 0; y < ysiz; y++)
		{
			long remaining = (long)xyoffs[x][y + 1] - (long)xyoffs[x][y];   // bytes for this column
			while (remaining > 0)
			{
				unsigned char slab[3];   // { ztop, zleng, cull } — read UNSIGNED
				if (fread (slab, 3, 1, fil) != 1) { remaining = 0; break; }
				const int z1 = slab[0];      // ztop
				const int k  = slab[1];      // zleng
				remaining -= k + 3;

				for (int j = 0; j < k; j++)
				{
					unsigned char c;
					if (fread (&c, 1, 1, fil) != 1) { remaining = 0; break; }
					const int z = z1 + j;
					if (z >= 0 && z < zsiz)   // defensive: never index outside the grid
					{
						pVoxels->activate ((unsigned)x, (unsigned)y, (unsigned)z);
						pVoxels->set_label ((unsigned)x, (unsigned)y, (unsigned)z, (unsigned int)c);
					}
				}
			}
		}

	// Palette: 256 RGB triplets in the trailing 768 bytes. Build-engine KVX
	// palettes are 6-bit VGA (values 0..63); scale to 8-bit when so.
	if (fseek (fil, -768, SEEK_END) == 0)
	{
		unsigned char pal[768];
		if (fread (pal, 768, 1, fil) == 1)
		{
			unsigned char maxv = 0;
			for (int i = 0; i < 768; i++) if (pal[i] > maxv) maxv = pal[i];
			const bool sixbit = (maxv <= 63);
			unsigned char rgb[768];
			for (int i = 0; i < 768; i++)
				rgb[i] = sixbit ? (unsigned char)((pal[i] * 255) / 63) : pal[i];
			pVoxels->set_palette (rgb, 256);
		}
	}

	fclose (fil);
	return pVoxels;
}
