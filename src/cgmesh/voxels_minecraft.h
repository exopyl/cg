#ifndef __VOXELS_MINECRAFT_H__
#define __VOXELS_MINECRAFT_H__

#define BLOCK_STONE  1
#define BLOCK_GRASS  2
#define BLOCK_DIRT   3
#define BLOCK_COBBLESTONE 4
#define BLOCK_WOODEN_PLANK 5
#define BLOCK_WATER 8
#define BLOCK_STATIONARY_WATER 9
#define BLOCK_SAND 12
#define BLOCK_GRAVEL 13

static unsigned int block_to_texture_id[14*3] = // top / side / bottom
{
	0, 0, 0,
	1, 1, 1,
	0, 3, 2,
	2, 2, 2,
	16, 16, 16,
	4, 4, 4,
	0, 0, 0,
	0, 0, 0,
	205, 205, 205,
	205, 205, 205,
	0, 0, 0,
	0, 0, 0,
	18, 18, 18,
	19, 19, 19
};


#endif // __VOXELS_MINECRAFT_H__
