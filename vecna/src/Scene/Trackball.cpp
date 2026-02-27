#include "Vecna/Scene/Trackball.hpp"

#include <cmath>
#include <cstring>

namespace Vecna::Scene {

static constexpr float PI = 3.14159265f;
static constexpr float PI_OVER_2 = PI / 2.0f;

Trackball::Trackball() {
    // Initialize transform to identity (column-major: m_transform[col][row])
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            m_transform[col][row] = (col == row) ? 1.0f : 0.0f;
}

void Trackball::setDimensions(int width, int height) {
    m_width = width;
    m_height = height;
}

void Trackball::pointToVector(int x, int y, float v[3]) const {
    // Project x, y onto a hemisphere centered within width, height
    // Ported from cgre Ctrackball::tbPointToVector
    v[0] = (2.0f * x - m_width) / static_cast<float>(m_width);
    v[1] = (m_height - 2.0f * y) / static_cast<float>(m_height);
    float d = std::sqrt(v[0] * v[0] + v[1] * v[1]);
    v[2] = std::cos(PI_OVER_2 * ((d < 1.0f) ? d : 1.0f));
    float a = 1.0f / std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] *= a;
    v[1] *= a;
    v[2] *= a;
}

void Trackball::onMousePress(int button, bool pressed, int x, int y) {
    // button 0 = left button (GLFW_MOUSE_BUTTON_LEFT)
    if (button != 0) {
        return;
    }

    if (pressed) {
        m_rotating = true;
        if (m_width > 0 && m_height > 0) {
            pointToVector(x, y, m_lastPosition);
        }
    } else {
        m_rotating = false;
    }
}

void Trackball::onMouseMove(int x, int y) {
    if (!m_rotating || m_width <= 0 || m_height <= 0) {
        return;
    }

    float currentPosition[3];
    pointToVector(x, y, currentPosition);

    // Calculate delta between current and last position on hemisphere
    float dx = currentPosition[0] - m_lastPosition[0];
    float dy = currentPosition[1] - m_lastPosition[1];
    float dz = currentPosition[2] - m_lastPosition[2];

    // Angle proportional to the length of mouse movement
    float angleDeg = 180.0f * std::sqrt(dx * dx + dy * dy + dz * dz);

    // Axis of rotation = cross product (last × current), same as cgre original
    float axis[3];
    axis[0] = m_lastPosition[1] * currentPosition[2] - m_lastPosition[2] * currentPosition[1];
    axis[1] = m_lastPosition[2] * currentPosition[0] - m_lastPosition[0] * currentPosition[2];
    axis[2] = m_lastPosition[0] * currentPosition[1] - m_lastPosition[1] * currentPosition[0];

    // Update last position for next move
    m_lastPosition[0] = currentPosition[0];
    m_lastPosition[1] = currentPosition[1];
    m_lastPosition[2] = currentPosition[2];

    // Build rotation matrix using Rodrigues' formula
    // R = I*cos(θ) + (1-cos(θ))*(axis⊗axis) + sin(θ)*[axis]×
    float angleRad = angleDeg * PI / 180.0f;
    float c = std::cos(angleRad);
    float s = std::sin(angleRad);

    // Normalize axis
    float axisLen = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (axisLen < 1e-8f) {
        return;  // No meaningful rotation
    }
    float ax = axis[0] / axisLen;
    float ay = axis[1] / axisLen;
    float az = axis[2] / axisLen;

    float oneMinusC = 1.0f - c;

    // Rodrigues rotation matrix (standard math convention):
    //   row 0: [c + ax²(1-c),      ax·ay(1-c) - az·s,  ax·az(1-c) + ay·s]
    //   row 1: [ay·ax(1-c) + az·s, c + ay²(1-c),        ay·az(1-c) - ax·s]
    //   row 2: [az·ax(1-c) - ay·s, az·ay(1-c) + ax·s,   c + az²(1-c)     ]
    //
    // Stored column-major: R[col][row] — matches TMatrix4<ColumnMajor> layout.
    float R[4][4];
    // Column 0
    R[0][0] = c + ax * ax * oneMinusC;
    R[0][1] = ay * ax * oneMinusC + az * s;
    R[0][2] = az * ax * oneMinusC - ay * s;
    R[0][3] = 0.0f;
    // Column 1
    R[1][0] = ax * ay * oneMinusC - az * s;
    R[1][1] = c + ay * ay * oneMinusC;
    R[1][2] = az * ay * oneMinusC + ax * s;
    R[1][3] = 0.0f;
    // Column 2
    R[2][0] = ax * az * oneMinusC + ay * s;
    R[2][1] = ay * az * oneMinusC - ax * s;
    R[2][2] = c + az * az * oneMinusC;
    R[2][3] = 0.0f;
    // Column 3
    R[3][0] = 0.0f;
    R[3][1] = 0.0f;
    R[3][2] = 0.0f;
    R[3][3] = 1.0f;

    // Accumulate: m_transform = R * m_transform (pre-multiply, column-major)
    // Pre-multiply applies R in screen/view space. With column-major storage
    // matching TMatrix4<ColumnMajor>, this gives intuitive trackball behavior.
    // Column-major multiply C=A*B: C[col][row] = Σ_k A[k][row] * B[col][k]
    // Here A=R, B=m_transform: result[col][row] = Σ_k R[k][row] * m_transform[col][k]
    float result[4][4];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            result[col][row] = 0.0f;
            for (int k = 0; k < 4; k++) {
                result[col][row] += R[k][row] * m_transform[col][k];
            }
        }
    }
    std::memcpy(m_transform, result, sizeof(m_transform));
}

} // namespace Vecna::Scene
