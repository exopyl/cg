#pragma once
#include "voxels.h"

// Import a Minecraft "structure block" .nbt file (a gzip-compressed NBT tree
// holding size / palette / blocks) into a voxel grid. Each non-air block is
// activated at its position and labelled with its palette index; a per-material
// colour palette is derived from the block names.
//
// Returns a newly-allocated Voxels (caller owns) or nullptr when the file is not
// a structure NBT (e.g. a generic tag tree), when zlib/NBT support is disabled,
// or on any parse/bounds error. Never throws.
extern Voxels* loadnbt (char *filename);

// Map a Minecraft block name (with or without the "minecraft:" prefix) to an
// average RGB colour: a curated table for common blocks, with a stable hash
// fallback for unknown ones. Used by loadnbt() to colour the voxel palette;
// exposed so callers/tests can reproduce the same colours.
extern void nbtBlockColor (const char *name, unsigned char rgb[3]);
