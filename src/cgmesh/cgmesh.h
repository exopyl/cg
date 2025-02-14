#ifndef __CGMESH_H__
#define __CGMESH_H__

// algebra
#include "../cgmath/cgmath.h"
#include "../cgimg/cgimg.h"

// mesh
#include "mesh.h"
#include "mesh_nm.h"
#include "object3D.h"

// polygon
#include "polygon2.h"

// point clouds
#include "points_strange_attractors.h"

// surfaces
#include "surface_basic.h"
#include "surface_parametric.h"
#include "surface_implicit.h"
#include "surface_implicit_tandem.h"
#include "surface_implicit_samples.h"
#include "surface_fractal.h"
#include "surface_architecture.h"

// tesselator
#include "tesselator.h"

// convex hull
#include "chull.h"

// DirectVisibilityOfPointSets
#include "DirectVisibilityOfPointSets.h"

// voxels
#include "voxels.h"
#include "voxels_menger_sponge.h"
#include "voxels_import_kvx.h"
#include "nbt.h"

// images
#include "../cgimg/cgimg.h"
#include "image_vectorization.h"

// audio
#include "audio.h"
#include "audio_convert.h"

// fractal
#include "lsysteminit.h"
#include "fractal.h"

// misc
#include "ticker.h"
#include "writer_svg.h"

// structures
#include "bintree.h"
#include "pair.h"

#include "half_edge.h"
#include "mesh_half_edge.h"
#include "octree.h"
#include "bsp.h"

#include "lights_manager.h"


#include "normals.h"
#include "DiffParamEvaluator.h"
#include "smoothing.h"
#include "subdivision.h"
#include "ambient_occlusion.h"
#include "descriptors.h"
#include "clipper.h"
#include "slicer.h"
#include "NPR.h"
#include "orientation.h"
#include "bundle.h"
#include "deformers.h"

// geodesic
#include "geodesic_wrapper.h"

#include "particles_system.h"
#include "particle_shape.h"

// vectorization
#include "extract_planes.h"
#include "set_lines.h"
#include "regions_vertices.h"
#include "regions_faces.h"

#include "deformer_arap.h"

#endif // __CGMESH_H__
