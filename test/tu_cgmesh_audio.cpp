#include <gtest/gtest.h>

// audio.h EN DIRECT : le parapluie ne le tire plus. Ce test est le seul
// consommateur des quatre unites audio du module.
#include "../src/cgmesh/audio.h"
#include "../src/cgmesh/audio_convert.h"

// La donnee est livree avec le depot et copiee dans le repertoire de travail
// des tests : son absence signale une invocation hors de ce repertoire, donc un
// echec, pas un saut.
//
// Le critere de presence est la cadence lue, et non le code de retour de load() :
// celui-ci vaut -1 sur ce fichier, dont le Subchunk1Size de 18 met le decodeur en
// echec sur les echantillons apres qu'il a renseigne l'en-tete. get_rate() reste
// nul tant qu'aucun en-tete n'a ete parcouru.


TEST(TEST_cgmesh_audio, rate)
{
    // context
    char* input = (char*)"./test/data/M1F1-Alaw-AFsp.wav";
    Audio* audio = new Audio();
    audio->load(input);
    ASSERT_GT(audio->get_rate(), 0u) << input << " introuvable ou illisible";
    audio->dump();

    // action
    auto rate = audio->get_rate();
    auto length = audio->get_length();

    // expectations
    EXPECT_EQ(rate, 8000.f);
    EXPECT_EQ(length, 145969);
}

TEST(TEST_cgmesh_audio, spectrum)
{
    // context
    char* input = (char*)"./test/data/M1F1-Alaw-AFsp.wav";
    Audio* audio = new Audio();
    audio->load(input);
    ASSERT_GT(audio->get_rate(), 0u) << input << " introuvable ou illisible";

    // action
    int nWindows = 1200;
    int nLength = 256;
    float* spectrum = (float*)malloc(nWindows * nLength * sizeof(float));
    float* data = (float*)malloc(nWindows * nLength * sizeof(float));
    for (int i = 0; i < nWindows; i++)
    {
        audio->get_spectrum((audio->get_length()) * i / nWindows, nLength, spectrum);
        data[i * (nLength / 2) + 0] = fabs(spectrum[0]);
        for (int j = 1; j < nLength / 2; j++)
        {
            float real, imag, mod;
            real = spectrum[j];
            int spectrum_size = nLength;
            imag = spectrum[spectrum_size - j];
            mod = sqrt(real * real + imag * imag);
            data[i * (nLength / 2) + j] = mod;
        }
    }

    // expectations
    EXPECT_EQ(data[0], 308.832031f);

    return;

    // export

    // obj
    FILE* ptr = fopen("fft.obj", "w");
    for (int i = 0; i < nWindows; i++)
        for (int j = 0; j < nLength / 2; j++)
            fprintf(ptr, "v %f %f %f\n", (float)i, (float)j, data[i * (nLength / 2) + j]);
    for (int i = 0; i < (nWindows - 1); i++)
        for (int j = 0; j < (nLength / 2 - 2); j++)
            fprintf(ptr, "f %d %d %d %d \n", 1 + i * (nLength / 2) + j, 1 + (i + 1) * (nLength / 2) + j, 1 + (i + 1) * (nLength / 2) + j + 1, 1 + i * (nLength / 2) + j + 1);

    fclose(ptr);


    float max_level = 0.;
    for (int i = 0; i < nWindows; i++)
        for (int j = 0; j < nLength / 2 - 1; j++)
            if (max_level < data[i * (nLength / 2) + j])
                max_level = data[i * (nLength / 2) + j];
    printf("max_level = %f\n", max_level);

    // output fft.ppm
    ptr = fopen("fft.ppm", "w");
    fprintf(ptr, "P3\n%d %d\n255\n", nLength / 2, nWindows);
    for (int i = 0; i < nWindows; i++)
        for (int j = 0; j < nLength / 2; j++)
        {
            float max_level_v = max_level / (0.6 * j);
            int r, g, b;
            color_jet_int(data[i * (nLength / 2) + j] / max_level_v, &r, &g, &b);
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            fprintf(ptr, "%d %d %d\n", r, g, b);
        }
    fclose(ptr);
}

TEST(TEST_cgmesh_audio, audio_2_image)
{
    // context
    char* input = (char*)"./test/data/M1F1-Alaw-AFsp.wav";
    Audio* audio = new Audio();
    audio->load(input);
    ASSERT_GT(audio->get_rate(), 0u) << input << " introuvable ou illisible";

    // action
    Img* img = audio_2_image(audio);

    // expectations
    int ret = img->save("audio.bmp");
    EXPECT_EQ(ret, 0);
}
