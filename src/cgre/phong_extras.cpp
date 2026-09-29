#include "phong_extras.h"

#include <cstddef>

namespace cgre
{

namespace
{
	// Une carte n'est retenue que si elle est la, si le maillage a des UV, et si
	// elle vise le jeu 0 -- le seul que VBOManager televerse. Une carte du jeu 1
	// echantillonnee au jeu 0 serait appliquee avec une autre parametrisation :
	// l'ignorer rend moins, mais ne rend pas faux.
	bool MapUsable (const MaterialRenderer::MaterialPbrInfo& info, cgpbr::MapSlot slot,
	                bool hasTexCoords)
	{
		const std::size_t i = static_cast<std::size_t> (slot);
		return hasTexCoords && info.hasMap[i] && info.mapTex[i] != 0 && info.uvSet[i] == 0;
	}

	float Clamp01 (float v)
	{
		return (v < 0.f) ? 0.f : ((v > 1.f) ? 1.f : v);
	}
}

PhongExtras MakePhongExtras (const MaterialRenderer::MaterialPbrInfo& info, bool hasTexCoords)
{
	PhongExtras x;
	if (!info.isPbr)
		return x;

	const cgpbr::Factors& f = info.factors;

	// EMISSION = facteur x carte (glTF). Un facteur nul annule la carte : on ne
	// l'echantillonne donc pas, ce qui evite un acces texture par fragment pour
	// rien -- le defaut du format est precisement un facteur nul.
	bool anyEmission = false;
	for (int i = 0; i < 3; ++i)
	{
		x.emissive[i] = (f.emissive[i] > 0.f) ? f.emissive[i] : 0.f;
		anyEmission   = anyEmission || x.emissive[i] > 0.f;
	}
	x.useEmissiveMap = anyEmission && MapUsable (info, cgpbr::MapSlot::emissive, hasTexCoords);

	// OCCLUSION : une force nulle rend la carte inerte, meme regle.
	x.occlusionStrength = Clamp01 (f.occlusionStrength);
	x.useOcclusionMap   = x.occlusionStrength > 0.f
	                      && MapUsable (info, cgpbr::MapSlot::occlusion, hasTexCoords);
	if (!x.useOcclusionMap)
		x.occlusionStrength = 0.f;

	// NORMALES : une echelle nulle aplatit la carte a la normale geometrique.
	x.normalScale  = f.normalScale;
	x.useNormalMap = f.normalScale != 0.f
	                 && MapUsable (info, cgpbr::MapSlot::normal, hasTexCoords);

	// DECOUPE : seul le mode `mask` la demande. `blend` n'est PAS exploite -- il
	// exigerait le melange ET un tri des surfaces par profondeur, que ce moteur
	// ne fait pas ; sans tri, l'ordre de la scene decide de ce qui se voit.
	if (info.alphaMode == cgpbr::AlphaMode::mask)
		x.alphaCutoff = (f.alphaCutoff > 0.f) ? f.alphaCutoff : 0.f;

	return x;
}

} // namespace cgre
