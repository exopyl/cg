#pragma once

#include <cstdint>
#include <string>

namespace Vecna::Scene {

/// Per-model metadata displayed in the UI info panel. Reconstructed from
/// Application.cpp (lines 897-1099) + ImGuiRenderer.cpp (renderInfoPanel).
///
/// `bboxMin`/`bboxMax` are populated from the loader. `loadTimeMs` /
/// `vertexCount` / `triangleCount` / `edgeCount` are populated by the
/// caller. `dimensions` and `center` are derived — call `computeDerived()`
/// after writing the bbox.
struct ModelInfo {
    std::string filename;
    double      loadTimeMs    = 0.0;
    uint32_t    vertexCount   = 0;
    uint32_t    triangleCount = 0;
    uint32_t    edgeCount     = 0;
    float       bboxMin[3]    = {0.0f, 0.0f, 0.0f};
    float       bboxMax[3]    = {0.0f, 0.0f, 0.0f};
    float       diagonal      = 0.0f;

    // Derived in computeDerived():
    float       dimensions[3] = {0.0f, 0.0f, 0.0f};  // bboxMax - bboxMin
    float       center[3]     = {0.0f, 0.0f, 0.0f};  // (bboxMin + bboxMax) / 2

    /// Recompute `dimensions` and `center` from `bboxMin`/`bboxMax`.
    void computeDerived();
};

inline void ModelInfo::computeDerived() {
    for (int i = 0; i < 3; ++i) {
        dimensions[i] = bboxMax[i] - bboxMin[i];
        center[i]     = 0.5f * (bboxMin[i] + bboxMax[i]);
    }
}

} // namespace Vecna::Scene
