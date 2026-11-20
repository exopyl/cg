#pragma once

#include "TMatrix4.h"

/// Orientation de platine dans le monde Z-up, en radians.
///
/// azimut : direction horizontale de l'oeil vue depuis le pivot, depuis +X
/// vers +Y ; elevation : au-dessus du plan XY, dans [-pi/2, +pi/2].
///
/// La rotation monde -> oeil correspondante a pour LIGNES :
///
///     ligne 0 (droite ecran) = ( -sin az        ,  cos az        , 0      )
///     ligne 1 (haut ecran)   = ( -sin el.cos az , -sin el.sin az , cos el )
///     ligne 2 (vers l'oeil)  = (  cos el.cos az ,  cos el.sin az , sin el )
///
/// La ligne 0 est horizontale par construction : l'horizon est droit, et la
/// verticale du monde est vers le haut de l'image (cos el >= 0). Au pole
/// (el = +-pi/2), la ligne 0 reste definie par az, qui devient l'orientation
/// de la vue en plan : la parametrisation n'a pas de singularite.
struct TurntableAngles
{
    double azimuth   = 0.0;
    double elevation = 0.0;
};

/// pi/2 : borne de l'elevation, INCLUSE (vue en plan exacte atteignable).
constexpr double kTurntableMaxElevation = 1.5707963267948966;

/// R monde -> oeil, sans roulis. L'elevation est bornee a
/// [-kTurntableMaxElevation, +kTurntableMaxElevation] ; aux bornes, cos et sin
/// sont ecrits exacts (0 et +-1). Calcul en double, stockage en float.
///
/// Les valeurs exactes aux bornes sont necessaires : en simple precision,
/// cos(float(pi/2)) vaut -4,37e-8, et la vue en plan serait imperceptiblement
/// retournee (ligne 1, composante z, negative).
TMatrix4<float> RotationFromTurntable(double azimuth, double elevation);

/// Inverse de RotationFromTurntable sur une R sans roulis.
///
/// Sur une R avec roulis, rend la platine de MEME direction de vue r3 (ligne
/// 2) : l'azimut est celui de r3. Au pole exact, ou r3 n'a pas d'azimut,
/// l'azimut est lu sur la ligne 0, horizontale a cet endroit. R_tt(resultat)
/// differe donc de R par une rotation de l'image autour de son centre, d'angle
/// oppose au roulis (RollOfRotation), et nulle au pole exact.
///
/// Calcul en double sur les composantes float de R.
TurntableAngles TurntableFromRotation(const TMatrix4<float>& rotation);

/// Roulis de R : atan2(R[0][2], R[1][2]). 0 pour un horizon droit, pi pour
/// une vue retournee. Au pole exact, R[0][2] et R[1][2] sont nuls (zeros
/// signes compris) et le roulis, indefini, vaut 0.
double RollOfRotation(const TMatrix4<float>& rotation);
