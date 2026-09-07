// ===========================================================================
//  tinygltf : LA seule unite qui porte son implementation
// ===========================================================================
//
// tiny_gltf.h est une bibliotheque « header-only » a implementation gardee :
// exactement une unite de compilation doit definir TINYGLTF_IMPLEMENTATION.
// C'est celle-ci, et elle ne fait QUE cela.
//
// Elle est separee de ses consommateurs parce qu'ils n'appartiennent pas aux
// memes cibles : vmeshes_io.cpp, qui importe glTF/GLB, est hors du build
// WebAssembly (lecteur 3DS, stb_image, windows.h), alors que l'ecrivain GLB de
// mesh_io_gltf.cpp y est requis. Porter l'implementation dans l'un des deux la
// rendrait absente de l'autre cible : compilation qui passe, lien qui echoue.
//
// Les trois options de compilation de tinygltf (TINYGLTF_NO_STB_IMAGE,
// TINYGLTF_NO_STB_IMAGE_WRITE, TINYGLTF_NO_INCLUDE_JSON) sont posees par
// src/cgmesh/CMakeLists.txt, en PUBLIC : elles conditionnent l'initialiseur par
// defaut de membres de tinygltf::TinyGLTF, si bien qu'une unite qui inclurait
// l'en-tete sans elles verrait une AUTRE classe du meme nom -- violation de la
// regle de definition unique, silencieuse au lien.
//
// TINYGLTF_NO_INCLUDE_JSON impose d'inclure nlohmann/json.hpp AVANT l'en-tete.

#define TINYGLTF_IMPLEMENTATION

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>
