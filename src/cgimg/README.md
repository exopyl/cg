# cgimg

Bibliothèque statique d'images raster (`cg::cgimg`), C++17, au-dessus de `cgmath`.

## Responsabilité

Ce qui appartient au module :

- le conteneur `Img` (`image.h`) : pixels RGBA8 ou indexés sur une palette, géométrie, accès par pixel ;
- les traitements, regroupés par domaine en classes à méthodes statiques (`image.h:22-46`) : entrées-sorties
  (`ImgIO`), filtres, binarisation, dessin 2D, quantification de couleurs (Heckbert, Wu), transformée géodésique,
  histogramme, mires de test ;
- la disparité stéréo (`disparity.h`, `image_disparity_birchfield.h`), la couleur et la palette.

Formats (`image_io.h`) : BMP, TGA et PNM en lecture et en écriture ; PNG et JPEG en **lecture seule** (stb_image ne
fournit pas d'encodeur : `ImgIO::export_png` rend −1, `image_io_png.cpp:52-58`).

Ce qui n'y appartient pas : aucun maillage, aucun code gothique ni architectural. `cgimg` n'inclut que `cgmath`
(`image.h:5`, `color.h:3`) ; la seule mention d'un en-tête de `cgmesh` est commentée
(`image_quantization.cpp:12`).

## Dépendances

| Nature | Dépendance | Activation |
|---|---|---|
| interne | `cgmath`, lien `PUBLIC cg::cgmath` (`CMakeLists.txt:52`) | toujours |
| tierce | stb_image (`extern/stb`, répertoire d'inclusion PRIVATE, `CMakeLists.txt:11`) | options `CGIMG_WITH_PNG` et `CGIMG_WITH_JPG`, ON par défaut (`CMakeLists.txt:28-36`) |

Les deux options définissent des macros PRIVATE du même nom : elles ne fuient pas chez les consommateurs. Aucune
option `ENABLE_*` ne touche ce module.

## Usage minimal

Repris de `test/tu_cgimg_img.cpp`, test `pixel_accessors_report_success_by_return_value` (l. 24-31) :

```cpp
#include <cgimg/cgimg.h>

Img img (4, 3);                          // 4 x 3 pixels, RGBA8, sans palette
img.set_pixel (3, 2, 10, 20, 30, 40);    // rend true dans les limites
unsigned char r, g, b, a;
img.get_pixel (3, 2, &r, &g, &b, &a);    // r = 10, g = 20, b = 30, a = 40
```

Un accès hors limites est une faute de l'appelant : il déclenche un `assert` en Debug et rend `false` en Release.

## Tests

Fichiers `test/tu_cgimg_*.cpp`, suites `TEST_cgimg_*`, dans l'exécutable `TU`. Les images de test sont lues sous
`test/data`, copié dans le répertoire d'exécution par le build.

```powershell
ctest --test-dir build/<preset> -C Release -R "TEST_cgimg_"
```

`test/tu_cgimg_img.cpp` inclut `cgmesh.h` : ce fichier dépend de `cgmesh`, pas seulement de `cgimg`.
