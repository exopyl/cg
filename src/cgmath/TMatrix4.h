#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ostream>
#include <limits>

#include "TVector4.h"
#include "StorageOrder.h"

/// 4x4 matrix template with configurable storage order.
///
/// Based on TMatrix4 from cgmath, modernized for C++20.
///
/// Logical indexing is always (row, col) via at(row, col):
/// - RowMajor:    m_data[row * 4 + col] — rows contiguous in memory
/// - ColumnMajor: m_data[col * 4 + row] — columns contiguous in memory (Vulkan-ready)
///
/// All mathematical conventions follow standard (GLM/GLU):
/// - SetLookAt: side/up/-forward as rows
/// - SetPerspective: Vulkan conventions (Y-flip, depth [0,1])
/// - Rotations: right-hand rule
///
/// With ColumnMajor (default), data() can be uploaded directly to Vulkan push constants.
template <class TValue, StorageOrder Order = StorageOrder::ColumnMajor>
class TMatrix4 {
public:

    // ========================================================================
    // Element access
    // ========================================================================

    /// Access element at logical (row, col), storage-order independent.
    TValue& at(int row, int col) {
        if constexpr (Order == StorageOrder::RowMajor)
            return m_data[row * 4 + col];
        else
            return m_data[col * 4 + row];
    }

    const TValue& at(int row, int col) const {
        if constexpr (Order == StorageOrder::RowMajor)
            return m_data[row * 4 + col];
        else
            return m_data[col * 4 + row];
    }

    // ========================================================================
    // Constructors
    // ========================================================================

    /// Default: identity matrix.
    TMatrix4() { SetIdentity(); }

    /// Construct from raw flat array (interpreted in current StorageOrder).
    explicit TMatrix4(const TValue* rawData) {
        std::memcpy(m_data, rawData, 16 * sizeof(TValue));
    }

    /// Construct from explicit values in logical (row, col) order.
    TMatrix4(
        TValue m00, TValue m01, TValue m02, TValue m03,
        TValue m10, TValue m11, TValue m12, TValue m13,
        TValue m20, TValue m21, TValue m22, TValue m23,
        TValue m30, TValue m31, TValue m32, TValue m33)
    {
        at(0,0)=m00; at(0,1)=m01; at(0,2)=m02; at(0,3)=m03;
        at(1,0)=m10; at(1,1)=m11; at(1,2)=m12; at(1,3)=m13;
        at(2,0)=m20; at(2,1)=m21; at(2,2)=m22; at(2,3)=m23;
        at(3,0)=m30; at(3,1)=m31; at(3,2)=m32; at(3,3)=m33;
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const TMatrix4& right) const {
        for (int i = 0; i < 16; ++i)
            if (m_data[i] != right.m_data[i])
                return false;
        return true;
    }

    bool operator!=(const TMatrix4& right) const { return !(*this == right); }

    TMatrix4& operator*=(const TMatrix4& right) {
        *this = *this * right;
        return *this;
    }

    TMatrix4 operator*(const TMatrix4& right) const {
        TMatrix4 res;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                TValue sum = TValue(0);
                for (int k = 0; k < 4; ++k)
                    sum += at(i, k) * right.at(k, j);
                res.at(i, j) = sum;
            }
        return res;
    }

    // ========================================================================
    // Set methods
    // ========================================================================

    void SetIdentity() {
        std::fill(m_data, m_data + 16, TValue(0));
        at(0,0) = at(1,1) = at(2,2) = at(3,3) = TValue(1);
    }

    /// Rotation around X axis (right-hand rule, radians).
    void SetRotateX(TValue angleRadians) {
        TValue c = std::cos(angleRadians);
        TValue s = std::sin(angleRadians);
        SetIdentity();
        at(1,1) =  c; at(1,2) = -s;
        at(2,1) =  s; at(2,2) =  c;
    }

    /// Rotation around Y axis (right-hand rule, radians).
    ///     cos   0   sin   0
    ///       0   1     0   0
    ///    -sin   0   cos   0
    ///       0   0     0   1
    void SetRotateY(TValue angleRadians) {
        TValue c = std::cos(angleRadians);
        TValue s = std::sin(angleRadians);
        SetIdentity();
        at(0,0) =  c; at(0,2) = s;
        at(2,0) = -s; at(2,2) = c;
    }

    /// Rotation around Z axis (right-hand rule, radians).
    void SetRotateZ(TValue angleRadians) {
        TValue c = std::cos(angleRadians);
        TValue s = std::sin(angleRadians);
        SetIdentity();
        at(0,0) =  c; at(0,1) = -s;
        at(1,0) =  s; at(1,1) =  c;
    }

    /// Standard lookAt view matrix (same convention as GLM/GLU).
    /// @param eye    Camera position (3 floats)
    /// @param center Target position (3 floats)
    /// @param up     World up vector (3 floats)
    void SetLookAt(const TValue* eye, const TValue* center, const TValue* up) {
        constexpr auto epsilon = std::numeric_limits<TValue>::epsilon();

        // forward = normalize(center - eye)
        TValue f[3] = {
            center[0] - eye[0],
            center[1] - eye[1],
            center[2] - eye[2]
        };
        TValue fLen = std::sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
        if (fLen < epsilon) { SetIdentity(); return; }
        f[0] /= fLen; f[1] /= fLen; f[2] /= fLen;

        // side = normalize(forward x up)
        TValue s[3] = {
            f[1]*up[2] - f[2]*up[1],
            f[2]*up[0] - f[0]*up[2],
            f[0]*up[1] - f[1]*up[0]
        };
        TValue sLen = std::sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
        if (sLen < epsilon) { SetIdentity(); return; }
        s[0] /= sLen; s[1] /= sLen; s[2] /= sLen;

        // recompute up = side x forward
        TValue u[3] = {
            s[1]*f[2] - s[2]*f[1],
            s[2]*f[0] - s[0]*f[2],
            s[0]*f[1] - s[1]*f[0]
        };

        // View matrix: side/up/-forward as rows
        at(0,0) =  s[0]; at(0,1) =  s[1]; at(0,2) =  s[2]; at(0,3) = -(s[0]*eye[0] + s[1]*eye[1] + s[2]*eye[2]);
        at(1,0) =  u[0]; at(1,1) =  u[1]; at(1,2) =  u[2]; at(1,3) = -(u[0]*eye[0] + u[1]*eye[1] + u[2]*eye[2]);
        at(2,0) = -f[0]; at(2,1) = -f[1]; at(2,2) = -f[2]; at(2,3) =  (f[0]*eye[0] + f[1]*eye[1] + f[2]*eye[2]);
        at(3,0) = TValue(0); at(3,1) = TValue(0); at(3,2) = TValue(0); at(3,3) = TValue(1);
    }

    /// Vulkan perspective projection (Y-flip, depth [0,1]).
    /// @param fovyRadians Vertical field of view in radians
    void SetPerspective(TValue fovyRadians, TValue aspect, TValue nearPlane, TValue farPlane) {

        constexpr auto epsilon = std::numeric_limits<TValue>::epsilon();
        TValue tanHalfFovy = std::tan(fovyRadians / TValue(2));

        std::fill(m_data, m_data + 16, TValue(0));

        if (std::abs(aspect) < epsilon || std::abs(tanHalfFovy) < epsilon) {
            SetIdentity();
            return;
        }

        at(0,0) = TValue(1) / (aspect * tanHalfFovy);
        at(1,1) = TValue(-1) / tanHalfFovy;  // Y-flip for Vulkan
        at(2,2) = farPlane / (nearPlane - farPlane);
        at(2,3) = (farPlane * nearPlane) / (nearPlane - farPlane);
        at(3,2) = TValue(-1);
    }

    /// OpenGL frustum projection (no Y-flip, depth [-1,1]).
    void SetFrustumGL(TValue left, TValue right, TValue bottom, TValue top,
                      TValue znear, TValue zfar) {
        TValue w = right - left;
        TValue h = top - bottom;
        TValue d = zfar - znear;

        std::fill(m_data, m_data + 16, TValue(0));
        at(0,0) = TValue(2) * znear / w;
        at(1,1) = TValue(2) * znear / h;
        at(0,2) = (right + left) / w;
        at(1,2) = (top + bottom) / h;
        at(2,2) = -(zfar + znear) / d;
        at(2,3) = TValue(-2) * zfar * znear / d;
        at(3,2) = TValue(-1);
    }

    /// OpenGL symmetric perspective (no Y-flip, depth [-1,1]).
    /// @param fovyDegrees Vertical field of view in degrees
    void SetPerspectiveGL(TValue fovyDegrees, TValue aspect, TValue znear, TValue zfar) {
        constexpr TValue PI = TValue(3.14159265358979323846);
        TValue ymax = znear * std::tan(fovyDegrees * PI / TValue(360));
        TValue xmax = ymax * aspect;
        SetFrustumGL(-xmax, xmax, -ymax, ymax, znear, zfar);
    }

    /// Orthographic projection (OpenGL conventions).
    void SetOrtho(TValue left, TValue right, TValue bottom, TValue top,
                  TValue nearPlane, TValue farPlane) {
        std::fill(m_data, m_data + 16, TValue(0));
        at(0,0) = TValue(2) / (right - left);
        at(1,1) = TValue(2) / (top - bottom);
        at(2,2) = TValue(-2) / (farPlane - nearPlane);
        at(0,3) = -(right + left) / (right - left);
        at(1,3) = -(top + bottom) / (top - bottom);
        at(2,3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        at(3,3) = TValue(1);
    }

    // ========================================================================
    // Operations
    // ========================================================================

    /// Transpose in place.
    void Transpose() {
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j)
                std::swap(at(i, j), at(j, i));
    }

    /// Return transposed copy.
    TMatrix4 GetTransposed() const {
        TMatrix4 result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.at(i, j) = at(j, i);
        return result;
    }

    TValue Determinant() const {
        return (at(0,0)*at(1,1) - at(1,0)*at(0,1)) * (at(2,2)*at(3,3) - at(3,2)*at(2,3))
             - (at(0,0)*at(2,1) - at(2,0)*at(0,1)) * (at(1,2)*at(3,3) - at(3,2)*at(1,3))
             + (at(0,0)*at(3,1) - at(3,0)*at(0,1)) * (at(1,2)*at(2,3) - at(2,2)*at(1,3))
             + (at(1,0)*at(2,1) - at(2,0)*at(1,1)) * (at(0,2)*at(3,3) - at(3,2)*at(0,3))
             - (at(1,0)*at(3,1) - at(3,0)*at(1,1)) * (at(0,2)*at(2,3) - at(2,2)*at(0,3))
             + (at(2,0)*at(3,1) - at(3,0)*at(2,1)) * (at(0,2)*at(1,3) - at(1,2)*at(0,3));
    }

    /// Compute inverse using Cramer's rule.
    /// @return false if matrix is singular (determinant == 0).
    bool GetInverse(TMatrix4& out) const {
        TValue d = Determinant();
        if (d == TValue(0))
            return false;

        d = TValue(1) / d;

        out.at(0,0) = d * (at(1,1)*(at(2,2)*at(3,3) - at(3,2)*at(2,3))
                         + at(2,1)*(at(3,2)*at(1,3) - at(1,2)*at(3,3))
                         + at(3,1)*(at(1,2)*at(2,3) - at(2,2)*at(1,3)));

        out.at(1,0) = d * (at(1,2)*(at(2,0)*at(3,3) - at(3,0)*at(2,3))
                         + at(2,2)*(at(3,0)*at(1,3) - at(1,0)*at(3,3))
                         + at(3,2)*(at(1,0)*at(2,3) - at(2,0)*at(1,3)));

        out.at(2,0) = d * (at(1,3)*(at(2,0)*at(3,1) - at(3,0)*at(2,1))
                         + at(2,3)*(at(3,0)*at(1,1) - at(1,0)*at(3,1))
                         + at(3,3)*(at(1,0)*at(2,1) - at(2,0)*at(1,1)));

        out.at(3,0) = d * (at(1,0)*(at(3,1)*at(2,2) - at(2,1)*at(3,2))
                         + at(2,0)*(at(1,1)*at(3,2) - at(3,1)*at(1,2))
                         + at(3,0)*(at(2,1)*at(1,2) - at(1,1)*at(2,2)));

        out.at(0,1) = d * (at(2,1)*(at(0,2)*at(3,3) - at(3,2)*at(0,3))
                         + at(3,1)*(at(2,2)*at(0,3) - at(0,2)*at(2,3))
                         + at(0,1)*(at(3,2)*at(2,3) - at(2,2)*at(3,3)));

        out.at(1,1) = d * (at(2,2)*(at(0,0)*at(3,3) - at(3,0)*at(0,3))
                         + at(3,2)*(at(2,0)*at(0,3) - at(0,0)*at(2,3))
                         + at(0,2)*(at(3,0)*at(2,3) - at(2,0)*at(3,3)));

        out.at(2,1) = d * (at(2,3)*(at(0,0)*at(3,1) - at(3,0)*at(0,1))
                         + at(3,3)*(at(2,0)*at(0,1) - at(0,0)*at(2,1))
                         + at(0,3)*(at(3,0)*at(2,1) - at(2,0)*at(3,1)));

        out.at(3,1) = d * (at(2,0)*(at(3,1)*at(0,2) - at(0,1)*at(3,2))
                         + at(3,0)*(at(0,1)*at(2,2) - at(2,1)*at(0,2))
                         + at(0,0)*(at(2,1)*at(3,2) - at(3,1)*at(2,2)));

        out.at(0,2) = d * (at(3,1)*(at(0,2)*at(1,3) - at(1,2)*at(0,3))
                         + at(0,1)*(at(1,2)*at(3,3) - at(3,2)*at(1,3))
                         + at(1,1)*(at(3,2)*at(0,3) - at(0,2)*at(3,3)));

        out.at(1,2) = d * (at(3,2)*(at(0,0)*at(1,3) - at(1,0)*at(0,3))
                         + at(0,2)*(at(1,0)*at(3,3) - at(3,0)*at(1,3))
                         + at(1,2)*(at(3,0)*at(0,3) - at(0,0)*at(3,3)));

        out.at(2,2) = d * (at(3,3)*(at(0,0)*at(1,1) - at(1,0)*at(0,1))
                         + at(0,3)*(at(1,0)*at(3,1) - at(3,0)*at(1,1))
                         + at(1,3)*(at(3,0)*at(0,1) - at(0,0)*at(3,1)));

        out.at(3,2) = d * (at(3,0)*(at(1,1)*at(0,2) - at(0,1)*at(1,2))
                         + at(0,0)*(at(3,1)*at(1,2) - at(1,1)*at(3,2))
                         + at(1,0)*(at(0,1)*at(3,2) - at(3,1)*at(0,2)));

        out.at(0,3) = d * (at(0,1)*(at(2,2)*at(1,3) - at(1,2)*at(2,3))
                         + at(1,1)*(at(0,2)*at(2,3) - at(2,2)*at(0,3))
                         + at(2,1)*(at(1,2)*at(0,3) - at(0,2)*at(1,3)));

        out.at(1,3) = d * (at(0,2)*(at(2,0)*at(1,3) - at(1,0)*at(2,3))
                         + at(1,2)*(at(0,0)*at(2,3) - at(2,0)*at(0,3))
                         + at(2,2)*(at(1,0)*at(0,3) - at(0,0)*at(1,3)));

        out.at(2,3) = d * (at(0,3)*(at(2,0)*at(1,1) - at(1,0)*at(2,1))
                         + at(1,3)*(at(0,0)*at(2,1) - at(2,0)*at(0,1))
                         + at(2,3)*(at(1,0)*at(0,1) - at(0,0)*at(1,1)));

        out.at(3,3) = d * (at(0,0)*(at(1,1)*at(2,2) - at(2,1)*at(1,2))
                         + at(1,0)*(at(2,1)*at(0,2) - at(0,1)*at(2,2))
                         + at(2,0)*(at(0,1)*at(1,2) - at(1,1)*at(0,2)));

        return true;
    }

    /// Invert in place. Returns false if singular.
    bool SetInverse() {
        TMatrix4 out;
        if (!GetInverse(out))
            return false;
        *this = out;
        return true;
    }

    // ========================================================================
    // Data access
    // ========================================================================

    /// Raw pointer to internal data (layout depends on StorageOrder).
    TValue* data() { return m_data; }
    const TValue* data() const { return m_data; }

    /// Alias for compatibility with original cgmath API.
    TValue* GetMatPtr() { return m_data; }
    const TValue* GetMatPtr() const { return m_data; }

    operator TValue*() { return m_data; }
    operator const TValue*() const { return m_data; }

    // ========================================================================
    // Conversion
    // ========================================================================

    /// Convert to a matrix with different storage order.
    /// The mathematical matrix is preserved; only the memory layout changes.
    template <StorageOrder OtherOrder>
    TMatrix4<TValue, OtherOrder> toOrder() const {
        TMatrix4<TValue, OtherOrder> result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.at(i, j) = at(i, j);
        return result;
    }

    // ========================================================================
    // IOstream
    // ========================================================================

    friend std::ostream& operator<<(std::ostream& out, const TMatrix4& m) {
        for (int i = 0; i < 4; ++i) {
            out << "( " << m.at(i,0) << " , " << m.at(i,1)
                << " , " << m.at(i,2) << " , " << m.at(i,3) << " )";
            if (i < 3) out << "\n";
        }
        return out;
    }

private:
    TValue m_data[16];
};

// Non-member matrix * vector operator. Computes logical product using at(row,col).
template <class TValue, StorageOrder Order>
TVector4<TValue> operator*(const TMatrix4<TValue, Order>& m, const TVector4<TValue>& v) {
    TVector4<TValue> res;
    res.x = m.at(0, 0) * v.x + m.at(0, 1) * v.y + m.at(0, 2) * v.z + m.at(0, 3) * v.w;
    res.y = m.at(1, 0) * v.x + m.at(1, 1) * v.y + m.at(1, 2) * v.z + m.at(1, 3) * v.w;
    res.z = m.at(2, 0) * v.x + m.at(2, 1) * v.y + m.at(2, 2) * v.z + m.at(2, 3) * v.w;
    res.w = m.at(3, 0) * v.x + m.at(3, 1) * v.y + m.at(3, 2) * v.z + m.at(3, 3) * v.w;
    return res;
}

// Type aliases (default: ColumnMajor for Vulkan)
using Matrix4f = TMatrix4<float>;
using Matrix4d = TMatrix4<double>;
using Matrix4i = TMatrix4<int>;

/// Défini quand cette API (at(), StorageOrder, data(), etc.) est utilisée.
/// Les tests peuvent faire #ifdef TMATRIX4_CGMATH_NEW_API pour adapter.
#define TMATRIX4_CGMATH_NEW_API 1
