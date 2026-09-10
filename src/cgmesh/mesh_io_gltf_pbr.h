#pragma once

//
// Lecture d'un materiau glTF 2.0 vers MaterialPbr.
//
// tinygltf est CONFINE a l'unite d'implementation : cet en-tete n'expose qu'une
// declaration anticipee de tinygltf::Model (declare `class Model` dans
// tiny_gltf.h). Le faire autrement imposerait a tout consommateur les macros
// TINYGLTF_NO_* -- une classe compilee sans elles n'est pas la meme classe, et
// l'editeur de liens ne le signale pas.
//

#include <memory>

class MaterialPbr;

namespace tinygltf { class Model; }

namespace cgpbr {

// Un materiau glTF -> un MaterialPbr. Rend nullptr si l'indice est hors bornes.
//
// Les images sont partagees par shared_ptr<Img> CONSTRUIT ICI, a partir des
// pixels deja decodes du modele : l'appelant peut detruire le tinygltf::Model
// aussitot apres. Une image que le modele n'a pas decodee (aucun rappel
// d'image pose sur le lecteur, ou format refuse) laisse l'emplacement VIDE --
// une carte absente, pas une erreur.
//
// TEXCOORD_n avec n > 1 est rabattu sur 0 : le coeur du format n'en definit
// que deux.
//
// PERIMETRE : les FACTEURS et les CINQ CARTES du coeur de la specification 2.0,
// et rien d'autre. C'est un sous-ensemble du coeur, pas le coeur entier.
//
// Ce qui en est ABSENT bien qu'appartenant au coeur :
//
//   - le SAMPLER de chaque texture -- wrapS, wrapT, minFilter, magFilter.
//     `texture.sampler` n'est pas lu, seul `texture.source` l'est. Une carte
//     declaree CLAMP_TO_EDGE est donc restituee sans sa regle de repliement, et
//     le backend appliquera la sienne. Rien ne le signale.
//
// Ce qui en est absent parce que hors du coeur : KHR_texture_transform,
// KHR_materials_specular, KHR_materials_clearcoat et leurs semblables, IGNORES
// en silence par tinygltf comme par cette fonction -- un fichier qui en porte
// est lu, mais rendu sans eux.
std::unique_ptr<MaterialPbr> materialFromGltf (const tinygltf::Model& model, int materialIndex);

} // namespace cgpbr
