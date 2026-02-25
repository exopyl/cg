#pragma once

#include "TVector3.h"
#include "TMatrix4.h"

/// Camera template following the same pattern as TMatrix4.
///
/// Encapsulates view parameters (position, target, up) and projection
/// parameters (FOV, near/far planes). Delegates matrix construction to
/// TMatrix4::SetLookAt and TMatrix4::SetPerspective.
template <class TValue, StorageOrder Order = StorageOrder::ColumnMajor>
class TCamera {
public:

    // ========================================================================
    // Constructors
    // ========================================================================

    TCamera()
        : m_position(TValue(0), TValue(0), TValue(3))
        , m_target(TValue(0), TValue(0), TValue(0))
        , m_up(TValue(0), TValue(1), TValue(0))
        , m_fov(TValue(3.14159265358979323846 / 4.0))
        , m_nearPlane(TValue(0.1))
        , m_farPlane(TValue(100))
    {}

    // ========================================================================
    // Setters
    // ========================================================================

    void SetPosition(TValue x, TValue y, TValue z) { m_position.Set(x, y, z); }
    void SetPosition(const TVector3<TValue>& pos) { m_position = pos; }

    void SetTarget(TValue x, TValue y, TValue z) { m_target.Set(x, y, z); }
    void SetTarget(const TVector3<TValue>& target) { m_target = target; }

    void SetUp(TValue x, TValue y, TValue z) { m_up.Set(x, y, z); }

    void SetFov(TValue fovRadians) { m_fov = fovRadians; }

    void SetClippingPlanes(TValue nearPlane, TValue farPlane) {
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
    }

    // ========================================================================
    // Getters
    // ========================================================================

    const TVector3<TValue>& GetPosition() const { return m_position; }
    const TVector3<TValue>& GetTarget() const { return m_target; }
    const TVector3<TValue>& GetUp() const { return m_up; }
    TValue GetFov() const { return m_fov; }
    TValue GetNearPlane() const { return m_nearPlane; }
    TValue GetFarPlane() const { return m_farPlane; }

    // ========================================================================
    // Matrix generation
    // ========================================================================

    /// Build view matrix from current position/target/up.
    TMatrix4<TValue, Order> GetViewMatrix() const {
        TMatrix4<TValue, Order> view;
        view.SetLookAt(m_position, m_target, m_up);  // implicit operator const TValue*()
        return view;
    }

    /// Build perspective projection matrix (Vulkan conventions).
    TMatrix4<TValue, Order> GetProjectionMatrix(TValue aspectRatio) const {
        TMatrix4<TValue, Order> proj;
        proj.SetPerspective(m_fov, aspectRatio, m_nearPlane, m_farPlane);
        return proj;
    }

private:
    TVector3<TValue> m_position;
    TVector3<TValue> m_target;
    TVector3<TValue> m_up;
    TValue m_fov;
    TValue m_nearPlane;
    TValue m_farPlane;
};

// Type aliases
using Cameraf = TCamera<float>;
using Camerad = TCamera<double>;
