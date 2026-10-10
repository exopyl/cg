#include "file_catalog.h"

namespace peles
{
namespace core
{

const std::vector<Format3D>& SupportedFormats ()
{
	static const std::vector<Format3D> formats = {
	    { "Wavefront OBJ", { "obj" }, FileKind::Obj },
	    { "STL", { "stl" }, FileKind::Stl },
	    { "3D Studio", { "3ds" }, FileKind::Generic },
#ifdef CG_HAS_OPENNURBS
	    { "Rhino 3DM", { "3dm" }, FileKind::Generic },
#endif
	    { "glTF", { "gltf", "glb" }, FileKind::Generic },
#ifdef CG_HAS_OCCT
	    { "STEP", { "step", "stp" }, FileKind::Generic },
	    { "IGES", { "iges", "igs" }, FileKind::Generic },
#endif
	    { "Point clouds", { "ply", "pset", "npts", "pts", "asc" }, FileKind::Obj },
	    { "OFF", { "off" }, FileKind::Generic },
	    { "KVX voxel", { "kvx" }, FileKind::Generic },
#ifdef CG_HAS_ZLIB
	    { "Minecraft NBT", { "nbt" }, FileKind::Generic },
#endif
#ifdef CG_HAS_LIB3MF
	    { "3MF", { "3mf" }, FileKind::Generic },
#endif
	};
	return formats;
}

const char* KindClass (FileKind kind)
{
	switch (kind)
	{
	case FileKind::Obj:
		return "kind-obj";
	case FileKind::Stl:
		return "kind-stl";
	case FileKind::Generic:
		break;
	}
	return "kind-generic";
}

std::string ToLowerAscii (std::string text)
{
	for (char& c : text)
	{
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
	}
	return text;
}

bool NameLess (const std::string& left, const std::string& right)
{
	const std::string lowerLeft  = ToLowerAscii (left);
	const std::string lowerRight = ToLowerAscii (right);
	if (lowerLeft != lowerRight)
		return lowerLeft < lowerRight;
	return left < right;
}

std::string KindClassForExtension (const std::string& extension)
{
	// Le point de tete est accepte : std::filesystem::path::extension() le
	// rend, et l'exiger retire ici obligerait chaque appelant a le couper --
	// donc a l'oublier une fois.
	std::string normalised = extension;
	if (!normalised.empty () && normalised.front () == '.')
		normalised.erase (normalised.begin ());
	normalised = ToLowerAscii (std::move (normalised));

	if (normalised.empty ())
		return std::string ();

	for (const Format3D& format : SupportedFormats ())
	{
		for (const std::string& candidate : format.extensions)
		{
			if (normalised == candidate)
				return KindClass (format.kind);
		}
	}
	return std::string ();
}

bool IsSupportedExtension (const std::string& extension)
{
	return !KindClassForExtension (extension).empty ();
}

} // namespace core
} // namespace peles
