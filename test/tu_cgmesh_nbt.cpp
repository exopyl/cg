#include <gtest/gtest.h>
#include <cstdio>
#include <cstdint>

#include "../src/cgmesh/cgmesh.h"            // pulls in nbt.h (guarded by CG_HAS_ZLIB)
#include "../src/cgmesh/voxels_import_nbt.h"  // loadnbt

// The NBT (Minecraft "Named Binary Tag") parser is a vendored BEER-WARE C
// library that had been dormant for years: its whole translation unit was
// gated behind an undefined ZLIB macro plus a #ifndef WIN32 guard, so it never
// compiled on any platform. It is now reactivated on top of the vendored
// extern/zlib-1.3.2 (see ENABLE_ZLIB / CG_HAS_ZLIB in CMake).
//
// This test exercises the full stack end to end:
//   * build an NBT tree in memory,
//   * serialise it with nbt_write() -> which gzip-compresses through zlib,
//   * read it back with nbt_parse() -> which gunzips through zlib,
//   * check the value round-trips,
//   * and confirm the on-disk file really is a gzip stream (magic 1f 8b),
//     which proves zlib is the one doing the (de)compression.
#ifdef CG_HAS_ZLIB
TEST(TEST_cgmesh_nbt, roundtrip_through_zlib)
{
    const char* path = "./nbt_roundtrip_test.tmp.nbt";
    remove(path); // clean any stale leftover

    // --- build: compound "root" { int "answer" = 42 } ---
    nbt_tag* root = nullptr;
    ASSERT_EQ(0, nbt_new_compound(&root, "root"));
    nbt_tag* answer = nullptr;
    ASSERT_EQ(0, nbt_new_int(&answer, "answer"));
    ASSERT_EQ(0, nbt_set_int(answer, 42));
    ASSERT_NE(nullptr, nbt_add_tag(answer, root)); // ownership moves into root

    // --- write (gzip via zlib) ---
    nbt_file* wf = nullptr;
    ASSERT_EQ(NBT_OK, nbt_init(&wf));
    wf->root = root;
    int written = nbt_write(wf, path);
    EXPECT_GT(written, 0) << "nbt_write should report a positive byte count";

    // --- the file must be a genuine gzip stream ---
    {
        FILE* f = fopen(path, "rb");
        ASSERT_NE(nullptr, f);
        unsigned char magic[2] = {0, 0};
        size_t n = fread(magic, 1, 2, f);
        fclose(f);
        ASSERT_EQ(2u, n);
        EXPECT_EQ(0x1f, magic[0]) << "not a gzip stream (byte 0)";
        EXPECT_EQ(0x8b, magic[1]) << "not a gzip stream (byte 1)";
    }

    // --- read back (gunzip via zlib) ---
    nbt_file* rf = nullptr;
    ASSERT_EQ(NBT_OK, nbt_init(&rf));
    ASSERT_EQ(NBT_OK, nbt_parse(rf, path));
    ASSERT_NE(nullptr, rf->root);
    EXPECT_EQ(TAG_COMPOUND, rf->root->type);

    nbt_compound* c = nbt_cast_compound(rf->root);
    ASSERT_NE(nullptr, c);
    nbt_tag* found = nbt_find_tag_by_name("answer", c);
    ASSERT_NE(nullptr, found) << "the 'answer' tag did not survive the round-trip";
    EXPECT_EQ(TAG_INT, found->type);
    int32_t* value = nbt_cast_int(found);
    ASSERT_NE(nullptr, value);
    EXPECT_EQ(42, *value);

    // --- cleanup ---
    nbt_free(wf); // frees the tree we built (wf->root == root)
    nbt_free(rf); // frees the parsed tree
    remove(path);
}

// Parse a real-world Minecraft "structure block" .nbt exported by a third party
// (test/data/nbt/ChickiSt26.nbt, 27x26x27, ~2000 blocks). This proves the
// reactivated parser + zlib handle production data end to end and exposes exactly
// the schema the sinaia importer needs:
//   root(compound) { size:[int,int,int], palette:[compound{Name,...}], blocks:[compound{pos:[int,int,int], state:int}] }
// The asset is optional, so the test SKIPS (does not fail) when absent.
TEST(TEST_cgmesh_nbt, parse_minecraft_structure)
{
    const char* path = "./test/data/nbt/ChickiSt26.nbt";
    { FILE* f = fopen(path, "rb"); if (!f) GTEST_SKIP() << "optional asset absent: " << path; fclose(f); }

    nbt_file* nf = nullptr;
    ASSERT_EQ(NBT_OK, nbt_init(&nf));
    ASSERT_EQ(NBT_OK, nbt_parse(nf, path));
    ASSERT_NE(nullptr, nf->root);
    ASSERT_EQ(TAG_COMPOUND, nf->root->type);
    nbt_compound* root = nbt_cast_compound(nf->root);
    ASSERT_NE(nullptr, root);

    // size : list of 3 ints; the product is 27*26*27 whatever the axis order.
    nbt_tag* sizeTag = nbt_find_tag_by_name("size", root);
    ASSERT_NE(nullptr, sizeTag);
    nbt_list* size = nbt_cast_list(sizeTag);
    ASSERT_NE(nullptr, size);
    ASSERT_EQ(3, size->length);
    long long vol = 1;
    for (int i = 0; i < 3; ++i)
    {
        int32_t* d = (int32_t*)size->content[i];
        ASSERT_NE(nullptr, d);
        EXPECT_GT(*d, 0);
        vol *= *d;
    }
    EXPECT_EQ(27LL * 26LL * 27LL, vol);

    // palette : non-empty list of block-state compounds.
    nbt_tag* palTag = nbt_find_tag_by_name("palette", root);
    ASSERT_NE(nullptr, palTag);
    nbt_list* pal = nbt_cast_list(palTag);
    ASSERT_NE(nullptr, pal);
    EXPECT_GT(pal->length, 0);

    // blocks : hundreds/thousands of {pos:[x,y,z], state:paletteIndex}.
    nbt_tag* blocksTag = nbt_find_tag_by_name("blocks", root);
    ASSERT_NE(nullptr, blocksTag);
    nbt_list* blocks = nbt_cast_list(blocksTag);
    ASSERT_NE(nullptr, blocks);
    EXPECT_GT(blocks->length, 500);

    // first block must expose a 3-int pos and a state indexing into the palette.
    nbt_compound* b0 = (nbt_compound*)blocks->content[0];
    ASSERT_NE(nullptr, b0);
    nbt_tag* posTag = nbt_find_tag_by_name("pos", b0);
    ASSERT_NE(nullptr, posTag);
    nbt_list* pos = nbt_cast_list(posTag);
    ASSERT_NE(nullptr, pos);
    EXPECT_EQ(3, pos->length);
    nbt_tag* stateTag = nbt_find_tag_by_name("state", b0);
    ASSERT_NE(nullptr, stateTag);
    int32_t* state = nbt_cast_int(stateTag);
    ASSERT_NE(nullptr, state);
    EXPECT_GE(*state, 0);
    EXPECT_LT(*state, pal->length);

    nbt_free(nf);
}

// End-to-end import through the exact path sinaia uses (VMeshes::load dispatches
// ".nbt" -> loadnbt -> Voxels -> ToMesh). Proves the structure .nbt becomes a
// coloured mesh. Optional asset -> SKIPS when absent.
TEST(TEST_cgmesh_nbt, import_structure_to_mesh)
{
    const char* path = "./test/data/nbt/ChickiSt26.nbt";
    { FILE* f = fopen(path, "rb"); if (!f) GTEST_SKIP() << "optional asset absent: " << path; fclose(f); }

    // Direct importer: geometry + palette.
    Voxels* vox = loadnbt(const_cast<char*>(path));
    ASSERT_NE(nullptr, vox);
    long long vol = (long long)vox->get_nx() * vox->get_ny() * vox->get_nz();
    EXPECT_EQ(27LL * 26LL * 27LL, vol);

    Mesh* m = vox->ToMesh();
    delete vox;
    ASSERT_NE(nullptr, m);
    EXPECT_GT(m->GetNVertices(), 0u);
    EXPECT_GT(m->GetNFaces(),    0u);
    // per-material colours applied (3 floats per vertex, not all identical)
    ASSERT_EQ(m->m_pVertexColors.size(), 3u * (size_t)m->GetNVertices());
    bool coloured = false;
    for (size_t i = 3; i < m->m_pVertexColors.size(); ++i)
        if (m->m_pVertexColors[i] != m->m_pVertexColors[i % 3]) { coloured = true; break; }
    EXPECT_TRUE(coloured) << "NBT palette colours were not applied to the mesh";
    delete m;
}
#endif // CG_HAS_ZLIB
