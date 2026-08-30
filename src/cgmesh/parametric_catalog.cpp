#include "parametric_catalog.h"

#include "parameterized_shapes.h"

namespace
{

template <class T>
std::unique_ptr<IParameterized> Make ()
{
	return std::unique_ptr<IParameterized> (new T ());
}

} // namespace

const std::vector<ParametricShapeEntry> &ParametricShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = {
		{ "Cube", &Make<ParameterizedCube> },
		{ "Sphere", &Make<ParameterizedSphere> },
		{ "Cylinder", &Make<ParameterizedCylinder> },
		{ "Cone", &Make<ParameterizedCone> },
		{ "Capsule", &Make<ParameterizedCapsule> },
		{ "Torus", &Make<ParameterizedTorus> },
		{ "Seashell", &Make<ParameterizedSeashell> },
		{ "Seashell (von Seggern)", &Make<ParameterizedSeashellVonSeggern> },
		{ "Klein Bottle", &Make<ParameterizedKleinBottle> },
		{ "Breather", &Make<ParameterizedBreather> },
		{ "Hyperbolic Paraboloid", &Make<ParameterizedHyperbolicParaboloid> },
		{ "Monkey Saddle", &Make<ParameterizedMonkeySaddle> },
		{ "Blobs", &Make<ParameterizedBlobs> },
		{ "Drop", &Make<ParameterizedDrop> },
		{ "Torus Knot", &Make<ParameterizedTorusKnot> },
		{ "Cinquefoil Knot", &Make<ParameterizedCinquefoilKnot> },
		{ "Trefoil Knot", &Make<ParameterizedTrefoilKnot> },
		{ "Borromean Rings", &Make<ParameterizedBorromeanRings> },
		{ "Helicoid", &Make<ParameterizedHelicoid> },
		{ "Corkscrew", &Make<ParameterizedCorkscrew> },
		{ "Mobius Strip", &Make<ParameterizedMobiusStrip> },
		{ "Radial Wave", &Make<ParameterizedRadialWave> },
		{ "Guimard", &Make<ParameterizedGuimard> },
		{ "Menger Sponge", &Make<ParameterizedMengerSponge> },
		{ "L-system", &Make<ParameterizedLSystem> },
		{ "Gothic Window", &Make<ParameterizedGothicWindow> },
		{ "Gothic Block", &Make<ParameterizedGothicBlock> }
	};
	return entries;
}

const ParametricShapeEntry *FindParametricShape (const std::string &name)
{
	for (const ParametricShapeEntry &entry : ParametricShapes ())
		if (name == entry.name)
			return &entry;
	return nullptr;
}

std::unique_ptr<IParameterized> MakeParametricShape (const std::string &name)
{
	const ParametricShapeEntry *entry = FindParametricShape (name);
	return entry ? entry->make () : nullptr;
}
