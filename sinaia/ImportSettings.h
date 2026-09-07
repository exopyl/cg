#pragma once

//
// Operations applied to a 3D model when it is imported. They only affect
// models loaded *after* the settings are changed; already-imported models
// are left untouched.
//
struct ImportSettings
{
    // center + scale so the largest bbox dimension becomes
    // VMeshes::kNormalizedSize (100 mm = 10 cm = ten squares of the cutting mat).
    bool normalize     = false;  // off par défaut
    bool triangulate   = false;  // split polygonal faces into triangles
    bool mergeVertices = false;  // weld coincident vertices
};
