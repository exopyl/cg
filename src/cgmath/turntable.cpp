#include "turntable.h"

#include <cmath>

TMatrix4<float> RotationFromTurntable(double azimuth, double elevation)
{
    const double ca = std::cos(azimuth);
    const double sa = std::sin(azimuth);

    double ce, se;
    if (elevation >= kTurntableMaxElevation)
    {
        ce = 0.0;
        se = 1.0;
    }
    else if (elevation <= -kTurntableMaxElevation)
    {
        ce = 0.0;
        se = -1.0;
    }
    else
    {
        ce = std::cos(elevation);
        se = std::sin(elevation);
    }

    TMatrix4<float> r;   // identite : la ligne et la colonne 3 sont deja posees
    r.at(0, 0) = static_cast<float>(-sa);
    r.at(0, 1) = static_cast<float>(ca);
    r.at(0, 2) = 0.f;

    r.at(1, 0) = static_cast<float>(-se * ca);
    r.at(1, 1) = static_cast<float>(-se * sa);
    r.at(1, 2) = static_cast<float>(ce);

    r.at(2, 0) = static_cast<float>(ce * ca);
    r.at(2, 1) = static_cast<float>(ce * sa);
    r.at(2, 2) = static_cast<float>(se);
    return r;
}

TurntableAngles TurntableFromRotation(const TMatrix4<float>& rotation)
{
    const double r0x = rotation.at(0, 0);
    const double r0y = rotation.at(0, 1);
    const double r3x = rotation.at(2, 0);
    const double r3y = rotation.at(2, 1);
    const double r3z = rotation.at(2, 2);

    TurntableAngles angles;
    angles.elevation = std::atan2(r3z, std::hypot(r3x, r3y));

    // Hors du pole exact, la seule R sans roulis qui garde la direction de vue
    // r3 est celle de l'azimut de r3. Au pole exact, r3 n'a pas d'azimut ; la
    // ligne 0, horizontale, porte l'orientation de la vue en plan.
    if (r3x != 0.0 || r3y != 0.0)
        angles.azimuth = std::atan2(r3y, r3x);
    else
        angles.azimuth = std::atan2(-r0x, r0y);
    return angles;
}

double RollOfRotation(const TMatrix4<float>& rotation)
{
    const double r02 = rotation.at(0, 2);
    const double r12 = rotation.at(1, 2);

    // Vue exactement verticale : le roulis n'est pas defini. Sans ce test,
    // atan2(+0, -0) rendrait pi pour une vue de dessus.
    if (r02 == 0.0 && r12 == 0.0)
        return 0.0;
    return std::atan2(r02, r12);
}
