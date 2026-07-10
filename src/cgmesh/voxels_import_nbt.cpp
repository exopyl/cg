#include "voxels_import_nbt.h"

#ifdef CG_HAS_ZLIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

#include "nbt.h"

// Minecraft "structure block" NBT schema (the .nbt produced by structure blocks
// and the data-pack /structure format):
//   root(compound) {
//     int    DataVersion
//     list   size    : 3 x TAG_INT   (sizeX, sizeY, sizeZ)
//     list   palette : compound { string Name = "minecraft:..."; compound Properties? }
//     list   blocks  : compound { list pos = 3 x TAG_INT; int state = palette index; ... }
//   }

namespace {

// Fetch a compound child of the expected TAG_LIST type (nullptr otherwise).
nbt_list* childList (nbt_compound* c, const char* name)
{
	if (!c) return nullptr;
	nbt_tag* t = nbt_find_tag_by_name (name, c);
	if (!t || t->type != TAG_LIST) return nullptr;
	return (nbt_list*)t->value;
}

// FNV-1a hash of the block name -> a stable, non-black mid-tone RGB. Fallback
// for blocks absent from the curated table below, so every material still gets a
// distinct, visible colour.
void nameToRGB (const char* name, unsigned char* rgb)
{
	unsigned int h = 2166136261u;
	for (const char* p = name ? name : ""; *p; ++p) { h ^= (unsigned char)*p; h *= 16777619u; }
	rgb[0] = (unsigned char)(64 + ((h        & 0xff) * 191) / 255);
	rgb[1] = (unsigned char)(64 + (((h >> 8)  & 0xff) * 191) / 255);
	rgb[2] = (unsigned char)(64 + (((h >> 16) & 0xff) * 191) / 255);
}

// Curated average RGB for common Minecraft blocks, so an imported build renders
// with recognisable colours (near-black armour, white/grey marble, metal/ore
// accents) rather than arbitrary hashes. Keys omit the "minecraft:" prefix.
struct BlockColor { const char* name; unsigned char r, g, b; };
const BlockColor kBlockColors[] = {
	// --- near-black (armour / cape) ---
	{ "black_concrete",                     8,  10,  15 },
	{ "black_wool",                        20,  21,  26 },
	{ "coal_block",                        16,  16,  16 },
	{ "blackstone",                        42,  37,  42 },
	{ "polished_blackstone",               49,  45,  50 },
	{ "polished_blackstone_bricks",        48,  44,  49 },
	{ "polished_blackstone_wall",          49,  45,  50 },
	{ "chiseled_polished_blackstone",      49,  45,  50 },
	{ "cracked_polished_blackstone_bricks",46,  42,  47 },
	{ "black_terracotta",                  37,  23,  16 },
	{ "netherite_block",                   68,  61,  61 },
	{ "basalt",                            73,  72,  78 },
	{ "polished_basalt",                   86,  85,  90 },
	{ "coal_ore",                          74,  71,  69 },
	{ "bedrock",                           85,  85,  85 },
	// --- greys ---
	{ "gray_concrete",                     54,  57,  61 },
	{ "gray_wool",                         62,  68,  71 },
	{ "cyan_terracotta",                   86,  91,  91 },
	{ "mossy_stone_bricks",               110, 118, 101 },
	{ "cobblestone",                      122, 120, 120 },
	{ "cracked_stone_bricks",             118, 117, 116 },
	{ "stone_bricks",                     122, 121, 122 },
	{ "chiseled_stone_bricks",            120, 119, 120 },
	{ "stone",                            127, 127, 127 },
	{ "andesite",                         132, 132, 133 },
	{ "polished_andesite",                132, 135, 132 },
	{ "light_gray_concrete",              125, 125, 115 },
	{ "light_gray_wool",                  142, 142, 134 },
	{ "smooth_stone",                     159, 159, 160 },
	{ "iron_block",                       216, 216, 216 },
	// --- whites / marble (quartz family) ---
	{ "diorite",                          207, 207, 208 },
	{ "polished_diorite",                 208, 209, 210 },
	{ "white_concrete",                   207, 213, 214 },
	{ "bone_block",                       229, 225, 205 },
	{ "quartz_block",                     235, 229, 222 },
	{ "quartz_bricks",                    233, 227, 220 },
	{ "quartz_pillar",                    235, 230, 222 },
	{ "chiseled_quartz_block",            232, 226, 218 },
	{ "smooth_quartz",                    236, 231, 224 },
	{ "white_wool",                       233, 236, 236 },
	{ "snow_block",                       241, 250, 250 },
	{ "end_rod",                          230, 224, 216 },
	{ "sandstone",                        219, 207, 160 },
	{ "ochre_froglight",                  226, 199, 130 },
	// --- metals / ores / gems ---
	{ "gold_block",                       246, 208,  61 },
	{ "diamond_block",                    110, 236, 233 },
	{ "emerald_block",                     42, 203,  87 },
	{ "redstone_block",                   175,  25,   9 },
	// --- foliage / misc ---
	{ "azalea_leaves",                     90, 120,  50 },
	{ "dark_oak_leaves",                   58,  74,  30 },
	{ "dead_brain_coral_block",           119, 113, 111 },
	{ "dead_bubble_coral_block",          119, 113, 111 },
	{ "dead_fire_coral_block",            119, 113, 111 },
	{ "dead_horn_coral_block",            119, 113, 111 },
	{ "dead_tube_coral_block",            119, 113, 111 },
	// --- dyed wool (16 colours; white/black/grey/light_grey already above) ---
	{ "orange_wool",                      240, 118,  19 },
	{ "magenta_wool",                     190,  69, 180 },
	{ "light_blue_wool",                   58, 175, 217 },
	{ "yellow_wool",                      248, 214,  49 },
	{ "lime_wool",                        112, 185,  25 },
	{ "pink_wool",                        237, 141, 172 },
	{ "cyan_wool",                         21, 137, 145 },
	{ "purple_wool",                      121,  42, 172 },
	{ "blue_wool",                         53,  57, 157 },
	{ "brown_wool",                       114,  71,  40 },
	{ "green_wool",                        84, 109,  27 },
	{ "red_wool",                         160,  39,  34 },
	// --- dyed concrete (vivid) ---
	{ "orange_concrete",                  224,  97,   0 },
	{ "magenta_concrete",                 169,  48, 159 },
	{ "light_blue_concrete",               35, 137, 198 },
	{ "yellow_concrete",                  240, 175,  21 },
	{ "lime_concrete",                     94, 168,  24 },
	{ "pink_concrete",                    213, 101, 142 },
	{ "cyan_concrete",                     21, 119, 136 },
	{ "purple_concrete",                  100,  32, 156 },
	{ "blue_concrete",                     44,  46, 143 },
	{ "brown_concrete",                    96,  59,  31 },
	{ "green_concrete",                    73,  91,  36 },
	{ "red_concrete",                     142,  32,  32 },
	// --- terracotta (muted/earthy; black/cyan already above) ---
	{ "terracotta",                       152,  94,  68 },
	{ "white_terracotta",                 209, 178, 161 },
	{ "orange_terracotta",                162,  84,  38 },
	{ "magenta_terracotta",               150,  88, 109 },
	{ "light_blue_terracotta",            113, 109, 138 },
	{ "yellow_terracotta",                186, 133,  35 },
	{ "lime_terracotta",                  103, 118,  53 },
	{ "pink_terracotta",                  162,  78,  79 },
	{ "gray_terracotta",                   58,  42,  36 },
	{ "light_gray_terracotta",            135, 107,  98 },
	{ "purple_terracotta",                118,  70,  86 },
	{ "blue_terracotta",                   74,  60,  91 },
	{ "brown_terracotta",                  77,  51,  36 },
	{ "green_terracotta",                  76,  83,  42 },
	{ "red_terracotta",                   143,  61,  47 },
	// --- misc building blocks ---
	{ "sponge",                           195, 192,  74 },
	{ "clay",                             162, 166, 180 },
	{ "birch_planks",                     192, 175, 121 },
	{ "dark_oak_planks",                   66,  43,  21 },
	{ "brown_mushroom_block",             149, 111,  84 },
	{ "smooth_red_sandstone",             181,  97,  30 },
	{ "lantern",                          231, 170,  88 },
	// --- wood planks ---
	{ "oak_planks",                       162, 130,  78 },
	{ "spruce_planks",                    114,  84,  48 },
	{ "jungle_planks",                    160, 115,  80 },
	{ "acacia_planks",                    168,  90,  50 },
	{ "mangrove_planks",                  117,  54,  48 },
	{ "cherry_planks",                    226, 181, 166 },
	{ "bamboo_planks",                    197, 167,  90 },
	{ "crimson_planks",                   108,  55,  79 },
	{ "warped_planks",                     43, 104,  99 },
	// --- logs / stems ---
	{ "oak_log",                          109,  85,  52 },
	{ "spruce_log",                        59,  43,  26 },
	{ "birch_log",                        215, 208, 198 },
	{ "jungle_log",                        85,  67,  32 },
	{ "acacia_log",                       104,  60,  32 },
	{ "dark_oak_log",                      60,  46,  27 },
	{ "mangrove_log",                      83,  49,  46 },
	{ "cherry_log",                        59,  34,  49 },
	{ "stripped_oak_log",                 176, 143,  86 },
	// --- leaves ---
	{ "oak_leaves",                        60,  89,  32 },
	{ "spruce_leaves",                     48,  74,  48 },
	{ "birch_leaves",                     108, 151,  74 },
	{ "jungle_leaves",                     55,  92,  15 },
	{ "acacia_leaves",                     84, 105,  26 },
	{ "mangrove_leaves",                   72, 116,  42 },
	{ "cherry_leaves",                    226, 161, 197 },
	{ "flowering_azalea_leaves",           96, 120,  55 },
	// --- natural terrain ---
	{ "grass_block",                       91, 143,  50 },
	{ "dirt",                             134,  96,  67 },
	{ "coarse_dirt",                      119,  85,  59 },
	{ "rooted_dirt",                      144, 104,  79 },
	{ "podzol",                            91,  62,  31 },
	{ "mycelium",                         111,  98, 101 },
	{ "dirt_path",                        148, 124,  68 },
	{ "moss_block",                        89, 109,  45 },
	{ "mud",                               60,  54,  57 },
	{ "packed_mud",                       148, 111,  80 },
	{ "mud_bricks",                       137, 105,  78 },
	{ "sand",                             219, 211, 160 },
	{ "red_sand",                         190, 102,  33 },
	{ "gravel",                           131, 127, 126 },
	{ "mossy_cobblestone",                111, 120, 101 },
	// --- more stones ---
	{ "granite",                          149, 103,  85 },
	{ "polished_granite",                 154, 106,  88 },
	{ "deepslate",                         77,  77,  80 },
	{ "cobbled_deepslate",                 77,  76,  81 },
	{ "polished_deepslate",                72,  72,  75 },
	{ "deepslate_bricks",                  70,  70,  73 },
	{ "deepslate_tiles",                   55,  56,  58 },
	{ "tuff",                             108, 109, 101 },
	{ "calcite",                          223, 224, 220 },
	{ "dripstone_block",                  134, 110,  96 },
	{ "bricks",                           150,  97,  83 },
	{ "smooth_sandstone",                 224, 214, 164 },
	{ "cut_sandstone",                    216, 207, 157 },
	{ "red_sandstone",                    186,  99,  29 },
	{ "obsidian",                          20,  18,  29 },
	{ "crying_obsidian",                   32,  15,  57 },
	// --- ice / snow ---
	{ "ice",                              145, 183, 247 },
	{ "packed_ice",                       141, 180, 249 },
	{ "blue_ice",                         116, 168, 251 },
	{ "snow",                             240, 244, 244 },
	// --- nether / end ---
	{ "netherrack",                        97,  38,  38 },
	{ "nether_bricks",                     44,  22,  26 },
	{ "red_nether_bricks",                 69,   8,  11 },
	{ "soul_sand",                         84,  64,  51 },
	{ "soul_soil",                         76,  58,  46 },
	{ "crimson_stem",                      92,  42,  61 },
	{ "warped_stem",                       58,  58,  72 },
	{ "crimson_nylium",                   130,  31,  31 },
	{ "warped_nylium",                     43, 109,  99 },
	{ "glowstone",                        132, 109,  72 },
	{ "shroomlight",                      240, 146,  70 },
	{ "end_stone",                        219, 222, 158 },
	{ "end_stone_bricks",                 218, 224, 158 },
	{ "purpur_block",                     170, 120, 170 },
	{ "purpur_pillar",                    172, 123, 172 },
	// --- copper (oxidation states) ---
	{ "copper_block",                     192, 109,  89 },
	{ "exposed_copper",                   161, 124, 103 },
	{ "weathered_copper",                 108, 153, 122 },
	{ "oxidized_copper",                   82, 161, 138 },
	{ "cut_copper",                       191, 107,  86 },
	// --- ocean / gems / farm ---
	{ "prismarine",                        99, 151, 138 },
	{ "prismarine_bricks",                 99, 171, 158 },
	{ "dark_prismarine",                   51,  91,  75 },
	{ "sea_lantern",                      172, 199, 190 },
	{ "amethyst_block",                   133,  97, 191 },
	{ "hay_block",                        165, 138,  15 },
	{ "honeycomb_block",                  229, 148,  29 },
	{ "honey_block",                      251, 181,  68 },
	{ "pumpkin",                          198, 118,  26 },
	{ "carved_pumpkin",                   197, 119,  27 },
	{ "melon",                            118, 142,  29 },
	{ "red_mushroom_block",               198,  44,  42 },
	{ "mushroom_stem",                    203, 196, 185 },
	// --- glass (rendered opaque; translucency not modelled) ---
	{ "glass",                            190, 220, 228 },
	{ "tinted_glass",                      44,  38,  46 },
	{ "white_stained_glass",              233, 236, 236 },
	{ "orange_stained_glass",             240, 118,  19 },
	{ "magenta_stained_glass",            190,  69, 180 },
	{ "light_blue_stained_glass",          58, 175, 217 },
	{ "yellow_stained_glass",             248, 214,  49 },
	{ "lime_stained_glass",               112, 185,  25 },
	{ "pink_stained_glass",               237, 141, 172 },
	{ "gray_stained_glass",                62,  68,  71 },
	{ "light_gray_stained_glass",         142, 142, 134 },
	{ "cyan_stained_glass",                21, 137, 145 },
	{ "purple_stained_glass",             121,  42, 172 },
	{ "blue_stained_glass",                53,  57, 157 },
	{ "brown_stained_glass",              114,  71,  40 },
	{ "green_stained_glass",               84, 109,  27 },
	{ "red_stained_glass",                160,  39,  34 },
	{ "black_stained_glass",               20,  21,  26 },
};

// Look up a curated colour by block name (accepts an optional "minecraft:"
// prefix). Returns true and fills rgb on a hit; false otherwise.
bool curatedColor (const char* name, unsigned char* rgb)
{
	if (!name) return false;
	const char* key = name;
	if (strncmp (key, "minecraft:", 10) == 0) key += 10;
	for (const BlockColor& bc : kBlockColors)
		if (strcmp (key, bc.name) == 0) { rgb[0] = bc.r; rgb[1] = bc.g; rgb[2] = bc.b; return true; }
	return false;
}

// "minecraft:air", "cave_air", "void_air" -> treated as empty space.
bool isAirName (const char* n)
{
	if (!n) return false;
	size_t L = strlen (n);
	return L >= 3 && strcmp (n + L - 3, "air") == 0;
}

} // namespace

// Public: curated colour first, stable hash fallback for unknown blocks.
void nbtBlockColor (const char* name, unsigned char rgb[3])
{
	if (!curatedColor (name, rgb))
		nameToRGB (name, rgb);
}

Voxels* loadnbt (char* filename)
{
	nbt_file* nf = nullptr;
	if (nbt_init (&nf) != NBT_OK)
		return nullptr;
	if (nbt_parse (nf, filename) != NBT_OK) { nbt_free (nf); return nullptr; }
	if (!nf->root || nf->root->type != TAG_COMPOUND) { nbt_free (nf); return nullptr; }

	nbt_compound* root = (nbt_compound*)nf->root->value;

	nbt_list* size   = childList (root, "size");
	nbt_list* pal    = childList (root, "palette");
	nbt_list* blocks = childList (root, "blocks");
	// Not a structure NBT (e.g. a generic tag tree) -> bail out cleanly.
	if (!size || size->length != 3 || !pal || !blocks) { nbt_free (nf); return nullptr; }

	const int sx = *(int32_t*)size->content[0];
	const int sy = *(int32_t*)size->content[1];
	const int sz = *(int32_t*)size->content[2];
	if (sx <= 0 || sy <= 0 || sz <= 0) { nbt_free (nf); return nullptr; }
	// Guard against a pathological / corrupt size before allocating the grid.
	if ((long long)sx * sy * sz > 200LL * 1000 * 1000) { nbt_free (nf); return nullptr; }

	Voxels* vox = new Voxels ((unsigned)sx, (unsigned)sy, (unsigned)sz);

	// Palette -> one RGB triplet per state index, plus an air mask.
	const int np = pal->length;
	std::vector<unsigned char> rgb ((size_t)(np > 0 ? np : 1) * 3, 200);
	std::vector<char>          air ((size_t)(np > 0 ? np : 1), 0);
	for (int i = 0; i < np; ++i)
	{
		nbt_compound* pe = (nbt_compound*)pal->content[i];
		const char* name = nullptr;
		if (pe)
		{
			nbt_tag* nt = nbt_find_tag_by_name ("Name", pe);
			if (nt && nt->type == TAG_STRING) name = (char*)nt->value;
		}
		nbtBlockColor (name, &rgb[(size_t)i * 3]);
		air[i] = isAirName (name) ? 1 : 0;
	}
	if (np > 0)
		vox->set_palette (rgb.data (), (unsigned)np);

	// Activate every non-air block at its position.
	for (int b = 0; b < blocks->length; ++b)
	{
		nbt_compound* blk = (nbt_compound*)blocks->content[b];
		if (!blk) continue;

		nbt_tag* posTag = nbt_find_tag_by_name ("pos",   blk);
		nbt_tag* stTag  = nbt_find_tag_by_name ("state", blk);
		if (!posTag || posTag->type != TAG_LIST || !stTag || stTag->type != TAG_INT) continue;

		nbt_list* pos = (nbt_list*)posTag->value;
		if (!pos || pos->length != 3) continue;

		const int x     = *(int32_t*)pos->content[0];
		const int y     = *(int32_t*)pos->content[1];
		const int z     = *(int32_t*)pos->content[2];
		const int state = *(int32_t*)stTag->value;

		if (state < 0 || state >= np) continue;
		if (air[state]) continue;
		if (x < 0 || x >= sx || y < 0 || y >= sy || z < 0 || z >= sz) continue; // defensive

		vox->activate  ((unsigned)x, (unsigned)y, (unsigned)z);
		vox->set_label ((unsigned)x, (unsigned)y, (unsigned)z, (unsigned)state);
	}

	nbt_free (nf);
	return vox;
}

#else  // !CG_HAS_ZLIB

// zlib/NBT disabled at build time: the importer degrades to "unsupported".
Voxels* loadnbt (char* /*filename*/) { return nullptr; }
void nbtBlockColor (const char* /*name*/, unsigned char rgb[3]) { rgb[0] = rgb[1] = rgb[2] = 200; }

#endif // CG_HAS_ZLIB
