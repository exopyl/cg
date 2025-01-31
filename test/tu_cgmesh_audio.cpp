#include <gtest/gtest.h>

#include "../src/cgmesh/cgmesh.h"


TEST(TEST_cgmesh_audio, audio_2_image)
{
    //return; // TOFIX
    // context
    char* input = (char*)"./test/data/M1F1-Alaw-AFsp.wav";
    Audio* audio = new Audio();
    audio->load(input);
    ASSERT_EQ(audio->get_length(), 145969);
    audio->dump();

    // action
    Img* img = audio_2_image(audio);

    // expectations
    int ret = img->save("audio.bmp");
    EXPECT_EQ(ret, 0);
}
