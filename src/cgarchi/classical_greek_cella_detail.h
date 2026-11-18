#pragma once

// En-tete interne de l'architecture classique grecque, hors de l'API publique :
// faces de l'assise XVII interieure du naos, partagees par l'interieur
// (classical_greek_naos_interior) et le couronnement (classical_greek_cella_crown),
// pour que leurs faces communes soient calculees par les memes expressions en
// double, donc coplanaires au bit pres.

#include <cgarchi/classical_greek_cella.h>

namespace cgarchi
{
namespace classical
{
namespace detail
{

// Faces de l'assise XVII interieure d'epaisseur `thickness`, comptee depuis les
// nus du naos vers l'exterieur : X = +-side (murs longs), Y = front (mur de
// refend avant) et Y = back (mur de refend arriere).
struct InnerCourse
{
	double side = 0.0, front = 0.0, back = 0.0;
};

inline InnerCourse InnerCourseFaces (const CellaNaos &naos, double thickness)
{
	return {naos.inner + thickness, naos.front - thickness, naos.back + thickness};
}

// Vrai si les faces restent strictement dans l'epaisseur des murs longs et des
// murs de refend.
inline bool InnerCourseWithinWalls (const CellaNaos &naos, const InnerCourse &course)
{
	return course.side < naos.outer && course.front > naos.frontWall && course.back < naos.backWall;
}

} // namespace detail
} // namespace classical
} // namespace cgarchi
