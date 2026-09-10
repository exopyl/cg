#include "mesh_io_gltf_pbr.h"

#include <cstring>
#include <string>
#include <utility>

#include "../cgimg/image.h"
#include "material_pbr.h"

// TINYGLTF_NO_INCLUDE_JSON (pose par le CMakeLists de cgmesh) impose d'inclure
// nlohmann avant l'en-tete. L'IMPLEMENTATION de tinygltf vit dans
// tinygltf_impl.cpp ; la definir ici la dupliquerait.
#include <nlohmann/json.hpp>
#include <tinygltf/tiny_gltf.h>

namespace {

// Les pixels deja decodes d'une texture glTF -> une Img du depot.
//
// Rend nullptr quand rien d'exploitable n'est disponible : indice hors bornes,
// image non decodee (le lecteur n'avait pas de rappel d'image), ou disposition
// autre que RGBA 8 bits -- la seule que le tampon d'Img sache porter.
std::shared_ptr<Img> ImageFromGltfTexture (const tinygltf::Model& model, int textureIndex,
                                           std::string& name)
{
	if (textureIndex < 0 || textureIndex >= (int) model.textures.size())
		return nullptr;

	const tinygltf::Texture& texture = model.textures[textureIndex];
	if (texture.source < 0 || texture.source >= (int) model.images.size())
		return nullptr;

	const tinygltf::Image& image = model.images[texture.source];
	if (image.width <= 0 || image.height <= 0 || image.component != 4 || image.bits != 8)
		return nullptr;

	const size_t bytes = 4u * (size_t) image.width * (size_t) image.height;
	if (image.image.size() < bytes)
		return nullptr;

	std::shared_ptr<Img> out = std::make_shared<Img> ((unsigned int) image.width,
	                                                  (unsigned int) image.height, false);
	if (!out || !out->data())
		return nullptr;
	memcpy (out->data(), image.image.data(), bytes);

	// Nom INDICATIF (cf. TextureRef::name). Ni `image.name`, que la
	// specification n'oblige a rien, ni le repli sur l'indice, qui est LOCAL AU
	// MODELE, ne sont uniques : deux fichiers rendent tous deux
	// « gltf_image_0 ». Ce champ ne peut donc pas servir de cle de cache -- il
	// nomme, il n'identifie pas.
	name = image.name.empty() ? ("gltf_image_" + std::to_string (texture.source))
	                          : image.name;
	return out;
}

// Un *TextureInfo de glTF -> un emplacement de MaterialPbr. Ne pose rien quand
// l'image n'est pas exploitable : l'emplacement reste vide.
void AttachMap (MaterialPbr& dst, cgpbr::MapSlot slot, const tinygltf::Model& model,
                int textureIndex, int texCoord, cgpbr::ColorSpace space)
{
	std::string name;
	std::shared_ptr<Img> image = ImageFromGltfTexture (model, textureIndex, name);
	if (!image)
		return;

	cgpbr::TextureRef ref;
	ref.image      = std::move (image);
	ref.name       = std::move (name);
	ref.colorSpace = space;
	// Le coeur du format ne definit que TEXCOORD_0 et TEXCOORD_1 ; au-dela, on
	// rabat sur 0 plutot que de designer un attribut de sommet inexistant.
	ref.uvSet      = (texCoord == 1) ? (std::uint8_t) 1 : (std::uint8_t) 0;

	dst.SetMap (slot, std::move (ref));
}

cgpbr::AlphaMode AlphaModeFromGltf (const std::string& mode)
{
	if (mode == "MASK")
		return cgpbr::AlphaMode::mask;
	if (mode == "BLEND")
		return cgpbr::AlphaMode::blend;
	return cgpbr::AlphaMode::opaque;   // defaut du format
}

} // namespace

namespace cgpbr {

std::unique_ptr<MaterialPbr> materialFromGltf (const tinygltf::Model& model, int materialIndex)
{
	if (materialIndex < 0 || materialIndex >= (int) model.materials.size())
		return nullptr;

	const tinygltf::Material& src = model.materials[materialIndex];
	const tinygltf::PbrMetallicRoughness& pbr = src.pbrMetallicRoughness;

	std::unique_ptr<MaterialPbr> out = std::make_unique<MaterialPbr> ();
	out->SetName (src.name);

	// Les facteurs absents du fichier gardent le defaut du FORMAT, deja porte
	// par cgpbr::Factors : rien a ecrire pour eux. tinygltf les initialise aux
	// memes valeurs, l'affectation ci-dessous est donc sans effet dans ce cas.
	Factors& f = out->EditFactors();
	if (pbr.baseColorFactor.size() >= 4)
	{
		for (int i = 0; i < 4; ++i)
			f.baseColor[i] = (float) pbr.baseColorFactor[i];
	}
	f.metallic  = (float) pbr.metallicFactor;
	f.roughness = (float) pbr.roughnessFactor;

	if (src.emissiveFactor.size() >= 3)
	{
		for (int i = 0; i < 3; ++i)
			f.emissive[i] = (float) src.emissiveFactor[i];
	}

	f.normalScale       = (float) src.normalTexture.scale;
	f.occlusionStrength = (float) src.occlusionTexture.strength;
	f.alphaCutoff       = (float) src.alphaCutoff;

	out->SetAlphaMode (AlphaModeFromGltf (src.alphaMode));
	out->SetDoubleSided (src.doubleSided);

	// Espaces colorimetriques : les cartes porteuses de COULEUR sont en sRGB,
	// celles porteuses de DONNEES sont lineaires. Les confondre eclaire mal
	// sans qu'aucune erreur ne soit levee.
	AttachMap (*out, MapSlot::base_color, model,
	           pbr.baseColorTexture.index,         pbr.baseColorTexture.texCoord,
	           ColorSpace::srgb);
	AttachMap (*out, MapSlot::metallic_roughness, model,
	           pbr.metallicRoughnessTexture.index, pbr.metallicRoughnessTexture.texCoord,
	           ColorSpace::linear);
	AttachMap (*out, MapSlot::normal, model,
	           src.normalTexture.index,            src.normalTexture.texCoord,
	           ColorSpace::linear);
	AttachMap (*out, MapSlot::occlusion, model,
	           src.occlusionTexture.index,         src.occlusionTexture.texCoord,
	           ColorSpace::linear);
	AttachMap (*out, MapSlot::emissive, model,
	           src.emissiveTexture.index,          src.emissiveTexture.texCoord,
	           ColorSpace::srgb);

	return out;
}

} // namespace cgpbr
