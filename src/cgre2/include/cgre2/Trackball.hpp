#pragma once

namespace cgre2 {

/// Hemisphere-projection trackball that maps 2D mouse coordinates to a
/// 3D rotation. Reconstructed from Trackball.cpp; pure math, no GLFW or
/// Vulkan deps.
///
/// Usage:
///   1. `setDimensions(width, height)` whenever the viewport size changes.
///   2. On mouse button event: `onMousePress(button, pressed, x, y)`.
///   3. On mouse motion while pressed: `onMouseMove(x, y)`.
///   4. Sample the accumulated rotation via `getTransform()` — 16 floats,
///      column-major (compatible with `TMatrix4<ColumnMajor>`).
class Trackball {
public:
    Trackball();

    void setDimensions(int width, int height);

    /// `button` 0 = left mouse (GLFW_MOUSE_BUTTON_LEFT). Other buttons are
    /// ignored — trackball rotation is left-button only.
    void onMousePress(int button, bool pressed, int x, int y);

    /// Updates the accumulated transform via Rodrigues' formula when the
    /// trackball is currently rotating (left button held).
    void onMouseMove(int x, int y);

    /// 4×4 column-major rotation matrix. 16 contiguous floats — caller can
    /// `std::copy(getTransform(), getTransform()+16, ...)`.
    [[nodiscard]] const float* getTransform() const { return &m_transform[0][0]; }

private:
    void pointToVector(int x, int y, float v[3]) const;

    float m_transform[4][4]{};
    float m_lastPosition[3]{};
    bool  m_rotating = false;
    int   m_width    = 0;
    int   m_height   = 0;
};

} // namespace cgre2
