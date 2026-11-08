#include "parametric_catalog.h"

#include "parameterized_shapes.h"

namespace
{

template <class T>
std::unique_ptr<IParameterized> Make ()
{
	return std::unique_ptr<IParameterized> (new T ());
}

const char *const kCategory = "parametric";

} // namespace

const std::vector<ParametricShapeEntry> &ParametricShapes ()
{
	static const std::vector<ParametricShapeEntry> entries = {
		{ "shape.cube", "Cube", kCategory, &Make<ParameterizedCube> },
		{ "shape.sphere", "Sphere", kCategory, &Make<ParameterizedSphere> },
		{ "shape.cylinder", "Cylinder", kCategory, &Make<ParameterizedCylinder> },
		{ "shape.cone", "Cone", kCategory, &Make<ParameterizedCone> },
		{ "shape.capsule", "Capsule", kCategory, &Make<ParameterizedCapsule> },
		{ "shape.torus", "Torus", kCategory, &Make<ParameterizedTorus> },
		{ "shape.seashell", "Seashell", kCategory, &Make<ParameterizedSeashell> },
		{ "shape.seashell.von_seggern", "Seashell (von Seggern)", kCategory, &Make<ParameterizedSeashellVonSeggern> },
		{ "shape.klein_bottle", "Klein Bottle", kCategory, &Make<ParameterizedKleinBottle> },
		{ "shape.breather", "Breather", kCategory, &Make<ParameterizedBreather> },
		{ "shape.hyperbolic_paraboloid", "Hyperbolic Paraboloid", kCategory, &Make<ParameterizedHyperbolicParaboloid> },
		{ "shape.monkey_saddle", "Monkey Saddle", kCategory, &Make<ParameterizedMonkeySaddle> },
		{ "shape.blobs", "Blobs", kCategory, &Make<ParameterizedBlobs> },
		{ "shape.drop", "Drop", kCategory, &Make<ParameterizedDrop> },
		{ "shape.knot.torus", "Torus Knot", kCategory, &Make<ParameterizedTorusKnot> },
		{ "shape.knot.cinquefoil", "Cinquefoil Knot", kCategory, &Make<ParameterizedCinquefoilKnot> },
		{ "shape.knot.trefoil", "Trefoil Knot", kCategory, &Make<ParameterizedTrefoilKnot> },
		{ "shape.borromean_rings", "Borromean Rings", kCategory, &Make<ParameterizedBorromeanRings> },
		{ "shape.helicoid", "Helicoid", kCategory, &Make<ParameterizedHelicoid> },
		{ "shape.corkscrew", "Corkscrew", kCategory, &Make<ParameterizedCorkscrew> },
		{ "shape.mobius_strip", "Mobius Strip", kCategory, &Make<ParameterizedMobiusStrip> },
		{ "shape.radial_wave", "Radial Wave", kCategory, &Make<ParameterizedRadialWave> },
		{ "shape.guimard", "Guimard", kCategory, &Make<ParameterizedGuimard> },
		{ "shape.menger_sponge", "Menger Sponge", kCategory, &Make<ParameterizedMengerSponge> },
		{ "shape.lsystem", "L-system", kCategory, &Make<ParameterizedLSystem> }
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
