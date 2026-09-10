#include "material_pbr.h"

#include <cstdio>
#include <utility>

namespace {

const char* AlphaModeName (cgpbr::AlphaMode m)
{
	switch (m)
	{
	case cgpbr::AlphaMode::mask:  return "MASK";
	case cgpbr::AlphaMode::blend: return "BLEND";
	case cgpbr::AlphaMode::opaque:
	default:                      return "OPAQUE";
	}
}

const char* MapSlotName (cgpbr::MapSlot s)
{
	switch (s)
	{
	case cgpbr::MapSlot::base_color:         return "baseColor";
	case cgpbr::MapSlot::normal:             return "normal";
	case cgpbr::MapSlot::metallic_roughness: return "metallicRoughness";
	case cgpbr::MapSlot::occlusion:          return "occlusion";
	case cgpbr::MapSlot::emissive:           return "emissive";
	case cgpbr::MapSlot::count:
	default:                                 return "?";
	}
}

bool SlotInRange (cgpbr::MapSlot s)
{
	return static_cast<std::size_t> (s) < static_cast<std::size_t> (cgpbr::MapSlot::count);
}

} // namespace

MaterialType MaterialPbr::GetType (void) const
{
	return MATERIAL_PBR;
}

const cgpbr::TextureRef& MaterialPbr::GetMap (cgpbr::MapSlot s) const
{
	static const cgpbr::TextureRef empty {};
	if (!SlotInRange (s))
		return empty;
	return m_maps[static_cast<std::size_t> (s)];
}

void MaterialPbr::SetMap (cgpbr::MapSlot s, cgpbr::TextureRef ref)
{
	if (!SlotInRange (s))
		return;
	m_maps[static_cast<std::size_t> (s)] = std::move (ref);
}

bool MaterialPbr::HasMap (cgpbr::MapSlot s) const
{
	return GetMap (s).image != nullptr;
}

void MaterialPbr::Dump (void)
{
	printf ("MATERIAL_PBR : %s\n", m_name.c_str());
	printf ("   baseColor : %f %f %f %f\n",
	        m_factors.baseColor[0], m_factors.baseColor[1],
	        m_factors.baseColor[2], m_factors.baseColor[3]);
	printf ("   metallic : %f   roughness : %f\n", m_factors.metallic, m_factors.roughness);
	printf ("   emissive : %f %f %f\n",
	        m_factors.emissive[0], m_factors.emissive[1], m_factors.emissive[2]);
	printf ("   normalScale : %f   occlusionStrength : %f   alphaCutoff : %f\n",
	        m_factors.normalScale, m_factors.occlusionStrength, m_factors.alphaCutoff);
	printf ("   alphaMode : %s   doubleSided : %s\n",
	        AlphaModeName (m_alphaMode), m_doubleSided ? "true" : "false");

	for (std::size_t i = 0; i < static_cast<std::size_t> (cgpbr::MapSlot::count); ++i)
	{
		const cgpbr::MapSlot slot = static_cast<cgpbr::MapSlot> (i);
		if (!HasMap (slot))
			continue;
		const cgpbr::TextureRef& ref = GetMap (slot);
		printf ("   map %-18s : %s (%s, uv%u)\n", MapSlotName (slot),
		        ref.name.c_str(),
		        ref.colorSpace == cgpbr::ColorSpace::srgb ? "sRGB" : "lineaire",
		        (unsigned) ref.uvSet);
	}
}
