#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : profil lateral biarc (etape 3)
// ===========================================================================
//
// Valeurs de reference calculees a la main (plan d'execution, §6.3 et §8 J2-a).
// Tolerances : rayons et centres a 1e-12 en relatif, hauteurs a 1e-12 * L.

namespace
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kTolHeight = 1e-7;

    struct Expected
    {
        double L, zA, zB, sC;              // entrees
        double rA, rB;                     // rayons
        double oAs, oAz, oBs, oBz;         // centres (s, z)
        double zC;
        bool   monotonic;
    };

    // Une ligne par cas du tableau de §8 J2-a. Pour k = 1 et k = 0,5 les centres
    // se deduisent des rayons : (r_A, z_A) et (L - r_B, z_B). Pour k = 1,
    // r_A = s_C (1 + 1) / 2 = 2 = s_C : le sommet du cercle 1 est au PRC.
    const std::vector<Expected> &referenceCases()
    {
        static const std::vector<Expected> cases = {
            {4, 0, 8, 2,          5, 5,             5, 0, -1, 8,               4,      true},
            {4, 0, 8, 1,          2.5, 7.5,         2.5, 0, -3.5, 8,           2,      true},
            {5, 0, 10, 15.0 / 7,  75.0 / 14, 50.0 / 7, 75.0 / 14, 0, -15.0 / 7, 10, 30.0 / 7, true},
            {4, 8, 0, 2,          5, 5,             5, 8, -1, 0,               4,      true},
            {4, 0, 4, 2,          2, 2,             2, 0, 2, 4,                2,      true},
            {4, 0, 2, 2,          1.25, 1.25,       1.25, 0, 2.75, 2,          1,      false},
        };
        return cases;
    }

    void expectRelative(double actual, double expected, const char *what)
    {
        EXPECT_NEAR(actual, expected, 1e-12 * std::fabs(expected)) << what;
    }

    OgeeProfile build(double L, double zA, double zB, double sC)
    {
        const ProfileResult r = BuildOgeeProfile(L, zA, zB, sC, kTolHeight);
        EXPECT_EQ(r.status, ProfileStatus::Ok);
        return r.profile;
    }

    double crossZ(const Vector2d &a, const Vector2d &b) { return a.x * b.y - a.y * b.x; }
}

// ---------------------------------------------------------------------------
//  Valeurs analytiques
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_profile, reference_radii_centres_and_prc)
{
    for (const Expected &c : referenceCases())
    {
        SCOPED_TRACE(::testing::Message() << "(L, zA, zB, sC) = (" << c.L << ", " << c.zA << ", " << c.zB << ", " << c.sC << ")");
        const OgeeProfile p = build(c.L, c.zA, c.zB, c.sC);
        EXPECT_EQ(p.length, c.L);
        EXPECT_EQ(p.z_start, c.zA);
        EXPECT_EQ(p.z_end, c.zB);
        EXPECT_EQ(p.s_prc, c.sC);
        expectRelative(p.r_start, c.rA, "r_start");
        expectRelative(p.r_end, c.rB, "r_end");
        EXPECT_NEAR(p.z_prc, c.zC, 1e-12 * c.L);
        const Vector2d oA = StartCenter(p), oB = EndCenter(p);
        expectRelative(oA.x, c.oAs, "O_A.s");
        expectRelative(oA.y, c.oAz, "O_A.z");
        expectRelative(oB.x, c.oBs, "O_B.s");
        expectRelative(oB.y, c.oBz, "O_B.z");
        EXPECT_EQ(p.monotonic, c.monotonic);
    }
}

TEST(TEST_cgarchi_gothic_micro_profile, reference_heights)
{
    const double L4 = 4e-12, L5 = 5e-12;   // 1e-12 * L
    {
        const OgeeProfile p = build(4, 0, 8, 2);
        EXPECT_NEAR(HeightAt(p, 1), 3, L4);
        EXPECT_NEAR(HeightAt(p, 3), 5, L4);
    }
    {
        const OgeeProfile p = build(5, 0, 10, 15.0 / 7);
        EXPECT_NEAR(HeightAt(p, 1), std::sqrt(68.0 / 7), L5);
        EXPECT_NEAR(HeightAt(p, 4), 10 - std::sqrt(93.0 / 7), L5);
    }
    {
        // Descendant : miroir de h(1) = 3 du cas montant.
        const OgeeProfile p = build(4, 8, 0, 2);
        EXPECT_NEAR(HeightAt(p, 1), 5, L4);
        EXPECT_NEAR(HeightAt(p, 3), 3, L4);
    }
}

// |O_A O_B| = r_A + r_B : les deux cercles sont tangents exterieurement au PRC.
TEST(TEST_cgarchi_gothic_micro_profile, centres_are_at_distance_of_the_radii_sum)
{
    const OgeeProfile p = build(4, 0, 8, 1);
    const Vector2d oA = StartCenter(p), oB = EndCenter(p);
    EXPECT_NEAR(std::hypot(oB.x - oA.x, oB.y - oA.y), 10.0, 1e-12 * 10.0);
}

// ---------------------------------------------------------------------------
//  Oracles independants de la construction
// ---------------------------------------------------------------------------

// Formule A.2 de l'article : C = (r_B O_A + r_A O_B) / (r_A + r_B), alignement
// de O_A, C, O_B, et chaque centre a distance de son rayon de C et de son bout.
TEST(TEST_cgarchi_gothic_micro_profile, article_formula_and_alignment)
{
    for (const Expected &c : referenceCases())
    {
        SCOPED_TRACE(::testing::Message() << "(L, zA, zB, sC) = (" << c.L << ", " << c.zA << ", " << c.zB << ", " << c.sC << ")");
        const OgeeProfile p = build(c.L, c.zA, c.zB, c.sC);
        const Vector2d oA = StartCenter(p), oB = EndCenter(p);
        const double rA = p.r_start, rB = p.r_end;
        const Vector2d C((rB * oA.x + rA * oB.x) / (rA + rB), (rB * oA.y + rA * oB.y) / (rA + rB));
        EXPECT_NEAR(C.x, p.s_prc, 1e-12 * c.L);
        EXPECT_NEAR(C.y, p.z_prc, 1e-12 * c.L);
        // Alignement : produit vectoriel de (C - O_A) et (O_B - C), rapporte aux normes.
        const Vector2d u(p.s_prc - oA.x, p.z_prc - oA.y), v(oB.x - p.s_prc, oB.y - p.z_prc);
        EXPECT_LE(std::fabs(crossZ(u, v)) / (std::hypot(u.x, u.y) * std::hypot(v.x, v.y)), 1e-12);
        EXPECT_NEAR(std::hypot(u.x, u.y), rA, 1e-12 * rA);
        EXPECT_NEAR(std::hypot(v.x, v.y), rB, 1e-12 * rB);
    }
}

// Bouts exacts, continuite au PRC, equations des deux cercles sur une grille.
TEST(TEST_cgarchi_gothic_micro_profile, ends_are_exact_and_the_profile_is_continuous)
{
    for (const Expected &c : referenceCases())
    {
        SCOPED_TRACE(::testing::Message() << "(L, zA, zB, sC) = (" << c.L << ", " << c.zA << ", " << c.zB << ", " << c.sC << ")");
        const OgeeProfile p = build(c.L, c.zA, c.zB, c.sC);
        EXPECT_EQ(HeightAt(p, 0.0), c.zA);
        EXPECT_EQ(HeightAt(p, c.L), c.zB);
        // Hors de [0, L] : rabattu sur les bouts.
        EXPECT_EQ(HeightAt(p, -1.0), c.zA);
        EXPECT_EQ(HeightAt(p, c.L + 1.0), c.zB);

        const double before = std::nextafter(c.sC, 0.0), after = std::nextafter(c.sC, c.L);
        EXPECT_NEAR(HeightAt(p, before), HeightAt(p, after), 1e-12);
        EXPECT_NEAR(HeightAt(p, c.sC), p.z_prc, 1e-12 * c.L);

        // Arc 1 sur [0, s_C], arc 2 sur [s_C, L] : centres saisis a la main.
        for (int i = 0; i <= 64; ++i)
        {
            const double s = c.L * i / 64.0;
            const double z = HeightAt(p, s);
            const bool first = s <= c.sC;
            const double cs = first ? c.oAs : c.oBs, cz = first ? c.oAz : c.oBz, r = first ? c.rA : c.rB;
            EXPECT_NEAR(std::hypot(s - cs, z - cz), r, 1e-12 * c.L) << "s = " << s;
        }
    }
}

// Pres d'un bout, s (2r - s) evite l'annulation de r^2 - (s - r)^2 : a s = 1e-12,
// la seconde forme perd environ 4 chiffres sur la hauteur.
TEST(TEST_cgarchi_gothic_micro_profile, height_near_the_ends_has_no_cancellation)
{
    const OgeeProfile p = build(4, 0, 8, 2);   // r_A = r_B = 5
    const double s = 1e-12;
    const double expected = std::sqrt(s * (10.0 - s));   // ~3,16e-6
    EXPECT_NEAR(HeightAt(p, s) - 0.0, expected, 1e-12 * expected);
    EXPECT_NEAR(8.0 - HeightAt(p, 4.0 - 0.5e-12), std::sqrt(0.5e-12 * (10.0 - 0.5e-12)), 1e-3 * std::sqrt(5e-12));
}

// ---------------------------------------------------------------------------
//  Tangentes et courbures
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_profile, tangents_are_vertical_at_the_ends_and_g1_at_the_prc)
{
    for (const Expected &c : referenceCases())
    {
        SCOPED_TRACE(::testing::Message() << "(L, zA, zB, sC) = (" << c.L << ", " << c.zA << ", " << c.zB << ", " << c.sC << ")");
        const OgeeProfile p = build(c.L, c.zA, c.zB, c.sC);
        const double sigma = c.zB > c.zA ? 1.0 : -1.0;
        const Vector2d t0 = SideTangentAt(p, 0.0), tL = SideTangentAt(p, c.L);
        EXPECT_NEAR(t0.x, 0.0, 1e-15);
        EXPECT_EQ(t0.y, sigma);
        EXPECT_NEAR(tL.x, 0.0, 1e-15);
        EXPECT_EQ(tL.y, sigma);

        // G1 : tangentes a gauche et a droite du PRC, et valeur analytique
        // (2k, sigma (k^2 - 1)) / (1 + k^2).
        const double k = std::fabs(c.zB - c.zA) / c.L;
        const Vector2d tl = SideTangentAt(p, c.sC);
        const Vector2d tr = SideTangentAt(p, std::nextafter(c.sC, c.L));
        EXPECT_LE(std::fabs(crossZ(tl, tr)), 1e-12);
        EXPECT_NEAR(tl.x, 2 * k / (1 + k * k), 1e-12);
        EXPECT_NEAR(tl.y, sigma * (k * k - 1) / (1 + k * k), 1e-12);

        // Unitaire, et perpendiculaire au rayon (s - O.s, z - O.z) sur la grille.
        for (int i = 0; i <= 32; ++i)
        {
            const double s = c.L * i / 32.0;
            const Vector2d t = SideTangentAt(p, s);
            EXPECT_NEAR(std::hypot(t.x, t.y), 1.0, 1e-15) << "s = " << s;
            const bool first = s <= c.sC;
            const double rs = s - (first ? c.oAs : c.oBs), rz = HeightAt(p, s) - (first ? c.oAz : c.oBz);
            EXPECT_NEAR(t.x * rs + t.y * rz, 0.0, 1e-12 * c.L) << "s = " << s;
            EXPECT_GE(t.x, 0.0) << "s = " << s;   // orientee vers les s croissants
        }

        // Courbures de signes opposes : sens de rotation de la tangente sur chaque arc.
        const double turn1 = crossZ(SideTangentAt(p, 0.25 * c.sC), SideTangentAt(p, 0.75 * c.sC));
        const double s2a = c.sC + 0.25 * (c.L - c.sC), s2b = c.sC + 0.75 * (c.L - c.sC);
        const double turn2 = crossZ(SideTangentAt(p, s2a), SideTangentAt(p, s2b));
        EXPECT_LT(turn1 * turn2, 0.0);
        EXPECT_LT(sigma * turn1, 0.0);   // montant : l'arc 1 tourne a droite
    }
}

TEST(TEST_cgarchi_gothic_micro_profile, unit_slope_has_a_horizontal_tangent_at_the_prc)
{
    const OgeeProfile p = build(4, 0, 4, 2);
    const Vector2d t = SideTangentAt(p, 2.0);
    EXPECT_NEAR(t.x, 1.0, 1e-15);
    EXPECT_NEAR(t.y, 0.0, 1e-15);
}

// ---------------------------------------------------------------------------
//  Monotonie : calculee sur les entrees, |z_end - z_start| >= length
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_profile, monotonic_switches_at_unit_slope)
{
    EXPECT_FALSE(build(4, 0, 4 * (1 - 1e-9), 2).monotonic);
    EXPECT_TRUE(build(4, 0, 4, 2).monotonic);
    EXPECT_FALSE(build(4, 4, 4e-9, 2).monotonic);
    EXPECT_TRUE(build(4, 4, 0, 2).monotonic);

    // k = 1 par les entrees. Les rayons de la forme (s^2 + dz^2) / (2 s) y donnent
    // r_end < L - s_C par arrondi : le drapeau ne doit pas en dependre.
    const OgeeProfile p = build(0.2, 0.2, 0.4, 0.1);
    EXPECT_TRUE(p.monotonic);
    EXPECT_GE(p.r_start, p.s_prc);
    EXPECT_GE(p.r_end, p.length - p.s_prc);

    // Non monotone : construit ; le sommet du cercle 1 (s = r_A) depasse z_C.
    const OgeeProfile q = build(4, 0, 2, 2);
    EXPECT_FALSE(q.monotonic);
    EXPECT_GT(HeightAt(q, 1.25), q.z_prc);
}

// ---------------------------------------------------------------------------
//  Cas refuses
// ---------------------------------------------------------------------------

TEST(TEST_cgarchi_gothic_micro_profile, invalid_inputs_are_reported)
{
    EXPECT_EQ(BuildOgeeProfile(4, 0, 0, 2, kTolHeight).status, ProfileStatus::Flat);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 0.5 * kTolHeight, 2, kTolHeight).status, ProfileStatus::Flat);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 2 * kTolHeight, 2, kTolHeight).status, ProfileStatus::Ok);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 8, 0, kTolHeight).status, ProfileStatus::PrcOutOfRange);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 8, 4, kTolHeight).status, ProfileStatus::PrcOutOfRange);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 8, -1, kTolHeight).status, ProfileStatus::PrcOutOfRange);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 8, 5, kTolHeight).status, ProfileStatus::PrcOutOfRange);
    EXPECT_EQ(BuildOgeeProfile(0, 0, 8, 2, kTolHeight).status, ProfileStatus::NonPositiveLength);
    EXPECT_EQ(BuildOgeeProfile(-4, 0, 8, 2, kTolHeight).status, ProfileStatus::NonPositiveLength);
    // Tolerance negative ou non finie : refusee, comme pour Intersect.
    EXPECT_EQ(BuildOgeeProfile(4, 0, 0, 2, -1.0).status, ProfileStatus::InvalidTolerance);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 8, 2, -1e-300).status, ProfileStatus::InvalidTolerance);
    EXPECT_EQ(BuildOgeeProfile(4, 0, 8, 2, 0.0).status, ProfileStatus::Ok);
    for (int i = 0; i < 5; ++i)
    {
        std::array<double, 5> in = {4, 0, 8, 2, kTolHeight};
        const ProfileStatus expected = i == 4 ? ProfileStatus::InvalidTolerance : ProfileStatus::NonFiniteInput;
        in[i] = kNaN;
        EXPECT_EQ(BuildOgeeProfile(in[0], in[1], in[2], in[3], in[4]).status, expected) << "entree " << i;
        in[i] = std::numeric_limits<double>::infinity();
        EXPECT_EQ(BuildOgeeProfile(in[0], in[1], in[2], in[3], in[4]).status, expected) << "entree " << i;
    }
    // Entrees finies dont la difference deborde.
    EXPECT_EQ(BuildOgeeProfile(4, -1e308, 1e308, 2, kTolHeight).status, ProfileStatus::NonFiniteInput);

    // Un profil refuse n'est pas rempli.
    const ProfileResult flat = BuildOgeeProfile(4, 3, 3, 2, kTolHeight);
    EXPECT_EQ(flat.profile.length, 0.0);
    EXPECT_EQ(flat.profile.r_start, 0.0);
}

// Une abscisse NaN ne donne pas une valeur finie.
TEST(TEST_cgarchi_gothic_micro_profile, nan_abscissa_gives_nan)
{
    const OgeeProfile p = build(4, 0, 8, 2);
    EXPECT_TRUE(std::isnan(HeightAt(p, kNaN)));
    const Vector2d t = SideTangentAt(p, kNaN);
    EXPECT_TRUE(std::isnan(t.x));
    EXPECT_TRUE(std::isnan(t.y));
}
