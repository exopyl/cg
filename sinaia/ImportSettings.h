#pragma once

//
// Operations applied to a 3D model when it is imported. They only affect
// models loaded *after* the settings are changed; already-imported models
// are left untouched.
//
struct ImportSettings
{
    bool normalize     = false;  // center + scale the model to a unit bbox (off par défaut)
    bool triangulate   = false;  // split polygonal faces into triangles
    bool mergeVertices = false;  // weld coincident vertices
};
