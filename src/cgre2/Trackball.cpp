#include "cgre2/Trackball.hpp"

#include <cmath>
#include <cstring>

namespace cgre2 {

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
    // Map the cursor to a point on the virtual trackball (Bell/Shoemake): a
    // sphere near the centre, smoothly continued by a hyperbolic sheet toward
    // the edges. Both the value and its first derivative are continuous at the
    // sphere/hyperbola seam, so the rotation speed stays uniform across the
    // whole window and no region is a dead zone.
    //
    // Both axes are normalised by the *same* dimension, the smaller one, so the
    // unit disc is inscribed in the window and stays circular: a drag of N
    // pixels yields the same rotation whatever its direction. The domain
    // therefore exceeds ±1 along the long axis, which the hyperbolic sheet
    // handles.
    const float side = static_cast<float>(m_width < m_height ? m_width : m_height);
    v[0] = (2.0f * x - m_width) / side;
    v[1] = (m_height - 2.0f * y) / side;

    const float r = 1.0f;  // trackball radius, in the normalised plane
    const float d2 = v[0] * v[0] + v[1] * v[1];
    if (d2 <= r * r * 0.5f) {
        v[2] = std::sqrt(r * r - d2);  // inside the sphere: project onto it
    } else {
        v[2] = (r * r * 0.5f) / std::sqrt(d2);  // outside: hyperbolic sheet
    }

    const float a = 1.0f / std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
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

    // The rotation angle is the geometric angle between the two trackball
    // vectors. Both are unit vectors, so their chord c gives the angle as
    // 2*asin(c/2) radians.
    float halfChord = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
    if (halfChord > 1.0f) {
        halfChord = 1.0f;  // rounding can push the chord past 2
    }
    const float angleRad = 2.0f * std::asin(halfChord);

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

} // namespace cgre2
